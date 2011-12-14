/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TiledDrawingAreaProxy.h"

#if ENABLE(TILED_BACKING_STORE)
#include "DrawingAreaMessageKinds.h"
#include "DrawingAreaProxyMessageKinds.h"
#include "MessageID.h"
#include "UpdateChunk.h"
#include "WebCoreArgumentCoders.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"

using namespace WebCore;

namespace WebKit {

static const int defaultTileWidth = 1024;
static const int defaultTileHeight = 1024;

PassOwnPtr<TiledDrawingAreaProxy> TiledDrawingAreaProxy::create(PlatformWebView* webView, WebPageProxy* webPageProxy)
{
    return adoptPtr(new TiledDrawingAreaProxy(webView, webPageProxy));
}

TiledDrawingAreaProxy::TiledDrawingAreaProxy(PlatformWebView* webView, WebPageProxy* webPageProxy)
    : DrawingAreaProxy(DrawingAreaTypeTiled, webPageProxy)
    , m_isWaitingForDidSetFrameNotification(false)
    , m_isVisible(true)
    , m_webView(webView)
    , m_tileBufferUpdateTimer(RunLoop::main(), this, &TiledDrawingAreaProxy::tileBufferUpdateTimerFired)
    , m_tileCreationTimer(RunLoop::main(), this, &TiledDrawingAreaProxy::tileCreationTimerFired)
    , m_tileSize(defaultTileWidth, defaultTileHeight)
    , m_tileCreationDelay(0.01)
    , m_keepAreaMultiplier(2.5, 4.5)
    , m_coverAreaMultiplier(2, 3)
    , m_contentsScale(1)
{
}

TiledDrawingAreaProxy::~TiledDrawingAreaProxy()
{
}

void TiledDrawingAreaProxy::sizeDidChange()
{
    WebPageProxy* page = this->page();
    if (!page || !page->isValid())
        return;

    if (m_size.isEmpty())
        return;

    m_viewSize = m_size;
    m_lastSetViewSize = m_size;

    if (m_isWaitingForDidSetFrameNotification)
        return;
    m_isWaitingForDidSetFrameNotification = true;

    page->process()->responsivenessTimer()->start();
    page->process()->deprecatedSend(DrawingAreaLegacyMessage::SetSize, page->pageID(), CoreIPC::In(m_size));
}

void TiledDrawingAreaProxy::setPageIsVisible(bool isVisible)
{
    WebPageProxy* page = this->page();

    if (isVisible == m_isVisible)
        return;

    m_isVisible = isVisible;
    if (!page || !page->isValid())
        return;

    if (!m_isVisible) {
        // Tell the web process that it doesn't need to paint anything for now.
        page->process()->deprecatedSend(DrawingAreaLegacyMessage::SuspendPainting, page->pageID(), CoreIPC::In());
        return;
    }

    // The page is now visible.
    page->process()->deprecatedSend(DrawingAreaLegacyMessage::ResumePainting, page->pageID(), CoreIPC::In());

    // FIXME: We should request a full repaint here if needed.
}

void TiledDrawingAreaProxy::didSetSize(const IntSize& viewSize)
{
    ASSERT(m_isWaitingForDidSetFrameNotification);
    m_isWaitingForDidSetFrameNotification = false;

    if (viewSize != m_lastSetViewSize)
        setSize(m_lastSetViewSize, IntSize());

    WebPageProxy* page = this->page();
    page->process()->responsivenessTimer()->stop();
}

void TiledDrawingAreaProxy::didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    switch (messageID.get<DrawingAreaProxyLegacyMessage::Kind>()) {
    case DrawingAreaProxyLegacyMessage::TileUpdated: {
        int tileID;
        UpdateChunk updateChunk;
        float scale;
        unsigned pendingUpdateCount;
        if (!arguments->decode(CoreIPC::Out(tileID, updateChunk, scale, pendingUpdateCount)))
            return;

        TiledDrawingAreaTile* tile = m_tilesByID.get(tileID);
        ASSERT(!tile || tile->ID() == tileID);
        if (tile)
            tile->updateFromChunk(&updateChunk, scale);
        tileBufferUpdateComplete();
        break;
    }
    case DrawingAreaProxyLegacyMessage::DidSetSize: {
        IntSize size;
        if (!arguments->decode(CoreIPC::Out(size)))
            return;

        didSetSize(size);
        break;
    }
    case DrawingAreaProxyLegacyMessage::Invalidate: {
        IntRect rect;
        if (!arguments->decode(CoreIPC::Out(rect)))
            return;

        invalidate(rect);
        break;
    }
    case DrawingAreaProxyLegacyMessage::AllTileUpdatesProcessed: {
        tileBufferUpdateComplete();
        break;
    }
    case DrawingAreaProxyLegacyMessage::SnapshotTaken: {
        UpdateChunk chunk;
        if (!arguments->decode(CoreIPC::Out(chunk)))
            return;
        snapshotTaken(chunk);
        break;
    }
    default:
        ASSERT_NOT_REACHED();
    }
}

void TiledDrawingAreaProxy::requestTileUpdate(int tileID, const IntRect& dirtyRect)
{
    page()->process()->connection()->deprecatedSend(DrawingAreaLegacyMessage::RequestTileUpdate, page()->pageID(), CoreIPC::In(tileID, dirtyRect, contentsScale()));
}

void TiledDrawingAreaProxy::waitUntilUpdatesComplete()
{
    while (hasPendingUpdates()) {
        int tileID;
        UpdateChunk updateChunk;
        float scale;
        unsigned pendingUpdateCount;
        static const double tileUpdateTimeout = 10.0;
        OwnPtr<CoreIPC::ArgumentDecoder> arguments = page()->process()->connection()->deprecatedWaitFor(DrawingAreaProxyLegacyMessage::TileUpdated, page()->pageID(), tileUpdateTimeout);
        if (!arguments)
            break;
        if (!arguments->decode(CoreIPC::Out(tileID, updateChunk, scale, pendingUpdateCount)))
            break;
        TiledDrawingAreaTile* tile = m_tilesByID.get(tileID);
        ASSERT(!tile || tile->ID() == tileID);
        if (tile)
            tile->updateFromChunk(&updateChunk, scale);
    }
    tileBufferUpdateComplete();
}

PassRefPtr<TiledDrawingAreaTile> TiledDrawingAreaProxy::createTile(const TiledDrawingAreaTile::Coordinate& coordinate)
{
    RefPtr<TiledDrawingAreaTile> tile = TiledDrawingAreaTile::create(this, coordinate);
    setTile(coordinate, tile);
    return tile;
}

void TiledDrawingAreaProxy::setTileSize(const IntSize& size)
{
    if (m_tileSize == size)
        return;
    m_tileSize = size;
    removeAllTiles();
    startTileCreationTimer();
}

void TiledDrawingAreaProxy::setTileCreationDelay(double delay)
{
    m_tileCreationDelay = delay;
}

void TiledDrawingAreaProxy::setKeepAndCoverAreaMultipliers(const FloatSize& keepMultiplier, const FloatSize& coverMultiplier)
{
    m_keepAreaMultiplier = keepMultiplier;
    m_coverAreaMultiplier = coverMultiplier;
    startTileCreationTimer();
}

void TiledDrawingAreaProxy::takeSnapshot(const IntSize& size, const IntRect& contentsRect)
{
    WebPageProxy* page = this->page();
    page->process()->deprecatedSend(DrawingAreaLegacyMessage::TakeSnapshot, page->pageID(), CoreIPC::Out(size, contentsRect));
}

void TiledDrawingAreaProxy::invalidate(const IntRect& contentsDirtyRect)
{
    IntRect dirtyRect(mapFromContents(contentsDirtyRect));

    TiledDrawingAreaTile::Coordinate topLeft = tileCoordinateForPoint(dirtyRect.location());
    TiledDrawingAreaTile::Coordinate bottomRight = tileCoordinateForPoint(IntPoint(dirtyRect.maxX(), dirtyRect.maxY()));

    IntRect coverRect = calculateCoverRect(m_previousVisibleRect);

    Vector<TiledDrawingAreaTile::Coordinate> tilesToRemove;

    for (unsigned yCoordinate = topLeft.y(); yCoordinate <= bottomRight.y(); ++yCoordinate) {
        for (unsigned xCoordinate = topLeft.x(); xCoordinate <= bottomRight.x(); ++xCoordinate) {
            RefPtr<TiledDrawingAreaTile> currentTile = tileAt(TiledDrawingAreaTile::Coordinate(xCoordinate, yCoordinate));
            if (!currentTile)
                continue;
            if (!currentTile->rect().intersects(dirtyRect))
                continue;
            // If a tile outside out current cover rect gets invalidated, just drop it instead of updating.
            if (!currentTile->rect().intersects(coverRect)) {
                tilesToRemove.append(currentTile->coordinate());
                continue;
            }
            currentTile->invalidate(dirtyRect);
        }
    }

    unsigned removeCount = tilesToRemove.size();
    for (unsigned n = 0; n < removeCount; ++n)
        removeTile(tilesToRemove[n]);

    startTileBufferUpdateTimer();
}

void TiledDrawingAreaProxy::updateTileBuffers()
{
    Vector<RefPtr<TiledDrawingAreaTile> > newDirtyTiles;
    TileMap::iterator end = m_tiles.end();
    for (TileMap::iterator it = m_tiles.begin(); it != end; ++it) {
        RefPtr<TiledDrawingAreaTile>& current = it->second;
        if (!current->isDirty())
            continue;
        newDirtyTiles.append(it->second);
    }

    if (newDirtyTiles.isEmpty())
        return;

    unsigned size = newDirtyTiles.size();
    for (unsigned n = 0; n < size; ++n)
        newDirtyTiles[n]->updateBackBuffer();
}

void TiledDrawingAreaProxy::tileBufferUpdateComplete()
{
    // Bail out if all tile back buffers have not been updated.
    Vector<TiledDrawingAreaTile*> tilesToFlip;
    TileMap::iterator end = m_tiles.end();
    for (TileMap::iterator it = m_tiles.begin(); it != end; ++it) {
        RefPtr<TiledDrawingAreaTile>& current = it->second;
        if (current->isReadyToPaint() && (current->isDirty() || current->hasBackBufferUpdatePending()))
            return;
        if (current->hasReadyBackBuffer())
            tilesToFlip.append(current.get());
    }
    // Everything done, move back buffers to front.
    Vector<IntRect> paintedArea;
    unsigned size = tilesToFlip.size();
    for (unsigned n = 0; n < size; ++n) {
        TiledDrawingAreaTile* tile = tilesToFlip[n];
        tile->swapBackBufferToFront();
        // FIXME: should not request system repaint for the full tile.
        paintedArea.append(mapToContents(tile->rect()));
    }
    if (size)
        updateWebView(paintedArea);

    m_tileCreationTimer.startOneShot(0);
}

bool TiledDrawingAreaProxy::paint(const IntRect& rect, PlatformDrawingContext context)
{
    if (m_isWaitingForDidSetFrameNotification) {
        WebPageProxy* page = this->page();
        if (!page->isValid())
            return false;

        if (page->process()->isLaunching())
            return false;
    }

    adjustVisibleRect();

    GraphicsContext gc(context);
    gc.save();

    // Assumes the backing store is painted with the scale transform applied.
    // Since tile content is already scaled, first revert the scaling from the painter.
    gc.scale(FloatSize(1 / m_contentsScale, 1 / m_contentsScale));

    IntRect dirtyRect = mapFromContents(rect);

    TiledDrawingAreaTile::Coordinate topLeft = tileCoordinateForPoint(dirtyRect.location());
    TiledDrawingAreaTile::Coordinate bottomRight = tileCoordinateForPoint(IntPoint(dirtyRect.maxX(), dirtyRect.maxY()));

    for (unsigned yCoordinate = topLeft.y(); yCoordinate <= bottomRight.y(); ++yCoordinate) {
        for (unsigned xCoordinate = topLeft.x(); xCoordinate <= bottomRight.x(); ++xCoordinate) {
            TiledDrawingAreaTile::Coordinate currentCoordinate(xCoordinate, yCoordinate);
            RefPtr<TiledDrawingAreaTile> currentTile = tileAt(currentCoordinate);
            if (currentTile && currentTile->isReadyToPaint())
                currentTile->paint(&gc, dirtyRect);
        }
    }

    gc.restore();
    return true;
}

void TiledDrawingAreaProxy::adjustVisibleRect()
{
    IntRect visibleRect = mapFromContents(webViewVisibleRect());
    if (m_previousVisibleRect == visibleRect)
        return;
    m_previousVisibleRect = visibleRect;

    startTileCreationTimer();
}

void TiledDrawingAreaProxy::setContentsScale(float scale)
{
    if (m_contentsScale == scale)
        return;
    m_contentsScale = scale;
    removeAllTiles();
    createTiles();
}

void TiledDrawingAreaProxy::removeAllTiles()
{
    Vector<RefPtr<TiledDrawingAreaTile> > tilesToRemove;
    copyValuesToVector(m_tiles, tilesToRemove);
    unsigned removeCount = tilesToRemove.size();
    for (unsigned n = 0; n < removeCount; ++n)
        removeTile(tilesToRemove[n]->coordinate());
}

double TiledDrawingAreaProxy::tileDistance(const IntRect& viewport, const TiledDrawingAreaTile::Coordinate& tileCoordinate)
{
    if (viewport.intersects(tileRectForCoordinate(tileCoordinate)))
        return 0;

    IntPoint viewCenter = viewport.location() + IntSize(viewport.width() / 2, viewport.height() / 2);
    TiledDrawingAreaTile::Coordinate centerCoordinate = tileCoordinateForPoint(viewCenter);

    // Manhattan distance, biased so that vertical distances are shorter.
    const double horizontalBias = 1.3;
    return abs(centerCoordinate.y() - tileCoordinate.y()) + horizontalBias * abs(centerCoordinate.x() - tileCoordinate.x());
}

IntRect TiledDrawingAreaProxy::calculateKeepRect(const IntRect& visibleRect) const
{
    IntRect result = visibleRect;
    // Inflates to both sides, so divide inflate delta by 2
    result.inflateX(visibleRect.width() * (m_keepAreaMultiplier.width() - 1) / 2);
    result.inflateY(visibleRect.height() * (m_keepAreaMultiplier.height() - 1) / 2);
    result.intersect(contentsRect());
    return result;
}

IntRect TiledDrawingAreaProxy::calculateCoverRect(const IntRect& visibleRect) const
{
    IntRect result = visibleRect;
    // Inflates to both sides, so divide inflate delta by 2
    result.inflateX(visibleRect.width() * (m_coverAreaMultiplier.width() - 1) / 2);
    result.inflateY(visibleRect.height() * (m_coverAreaMultiplier.height() - 1) / 2);
    result.intersect(contentsRect());
    return result;
}

void TiledDrawingAreaProxy::createTiles()
{
    IntRect visibleRect = mapFromContents(webViewVisibleRect());
    m_previousVisibleRect = visibleRect;

    if (visibleRect.isEmpty())
        return;

    // Resize tiles on edges in case the contents size has changed.
    bool didResizeTiles = resizeEdgeTiles();

    // Remove tiles outside out current maximum keep rect.
    dropTilesOutsideRect(calculateKeepRect(visibleRect));

    // Search for the tile position closest to the viewport center that does not yet contain a tile.
    // Which position is considered the closest depends on the tileDistance function.
    double shortestDistance = std::numeric_limits<double>::infinity();
    Vector<TiledDrawingAreaTile::Coordinate> tilesToCreate;
    unsigned requiredTileCount = 0;
    bool hasVisibleCheckers = false;
    TiledDrawingAreaTile::Coordinate topLeft = tileCoordinateForPoint(visibleRect.location());
    TiledDrawingAreaTile::Coordinate bottomRight = tileCoordinateForPoint(IntPoint(visibleRect.maxX(), visibleRect.maxY()));
    for (unsigned yCoordinate = topLeft.y(); yCoordinate <= bottomRight.y(); ++yCoordinate) {
        for (unsigned xCoordinate = topLeft.x(); xCoordinate <= bottomRight.x(); ++xCoordinate) {
            TiledDrawingAreaTile::Coordinate currentCoordinate(xCoordinate, yCoordinate);
            // Distance is 0 for all currently visible tiles.
            double distance = tileDistance(visibleRect, currentCoordinate);

            RefPtr<TiledDrawingAreaTile> tile = tileAt(currentCoordinate);
            if (!distance && (!tile || !tile->isReadyToPaint()))
                hasVisibleCheckers = true;
            if (tile)
                continue;

            ++requiredTileCount;

            if (distance > shortestDistance)
                continue;
            if (distance < shortestDistance) {
                tilesToCreate.clear();
                shortestDistance = distance;
            }
            tilesToCreate.append(currentCoordinate);
        }
    }

    if (hasVisibleCheckers && shortestDistance > 0)
        return;

    // Now construct the tile(s).
    unsigned tilesToCreateCount = tilesToCreate.size();
    for (unsigned n = 0; n < tilesToCreateCount; ++n)
        createTile(tilesToCreate[n]);

    requiredTileCount -= tilesToCreateCount;

    // Paint the content of the newly created tiles.
    if (tilesToCreateCount || didResizeTiles)
        updateTileBuffers();

    // Keep creating tiles until the whole coverRect is covered.
    if (requiredTileCount)
        m_tileCreationTimer.startOneShot(m_tileCreationDelay);
}

bool TiledDrawingAreaProxy::resizeEdgeTiles()
{
    IntRect contentsRect = this->contentsRect();
    bool wasResized = false;

    Vector<TiledDrawingAreaTile::Coordinate> tilesToRemove;
    TileMap::iterator end = m_tiles.end();
    for (TileMap::iterator it = m_tiles.begin(); it != end; ++it) {
        TiledDrawingAreaTile::Coordinate tileCoordinate = it->second->coordinate();
        IntRect tileRect = it->second->rect();
        IntRect expectedTileRect = tileRectForCoordinate(tileCoordinate);
        if (!contentsRect.contains(tileRect))
            tilesToRemove.append(tileCoordinate);
        else if (expectedTileRect != tileRect) {
            it->second->resize(expectedTileRect.size());
            wasResized = true;
        }
    }
    unsigned removeCount = tilesToRemove.size();
    for (unsigned n = 0; n < removeCount; ++n)
        removeTile(tilesToRemove[n]);
    return wasResized;
}

void TiledDrawingAreaProxy::dropTilesOutsideRect(const IntRect& keepRect)
{
    FloatRect keepRectF = keepRect;

    Vector<TiledDrawingAreaTile::Coordinate> toRemove;
    TileMap::iterator end = m_tiles.end();
    for (TileMap::iterator it = m_tiles.begin(); it != end; ++it) {
        TiledDrawingAreaTile::Coordinate coordinate = it->second->coordinate();
        FloatRect tileRect = it->second->rect();
        if (!tileRect.intersects(keepRectF))
            toRemove.append(coordinate);
    }
    unsigned removeCount = toRemove.size();
    for (unsigned n = 0; n < removeCount; ++n)
        removeTile(toRemove[n]);
}

PassRefPtr<TiledDrawingAreaTile> TiledDrawingAreaProxy::tileAt(const TiledDrawingAreaTile::Coordinate& coordinate) const
{
    return m_tiles.get(coordinate);
}

void TiledDrawingAreaProxy::setTile(const TiledDrawingAreaTile::Coordinate& coordinate, RefPtr<TiledDrawingAreaTile> tile)
{
    m_tiles.set(coordinate, tile);
    m_tilesByID.set(tile->ID(), tile.get());
}

void TiledDrawingAreaProxy::removeTile(const TiledDrawingAreaTile::Coordinate& coordinate)
{
    RefPtr<TiledDrawingAreaTile> tile = m_tiles.take(coordinate);

    m_tilesByID.remove(tile->ID());

    if (!tile->hasBackBufferUpdatePending())
        return;
    WebPageProxy* page = this->page();
    page->process()->deprecatedSend(DrawingAreaLegacyMessage::CancelTileUpdate, page->pageID(), CoreIPC::In(tile->ID()));
}

IntRect TiledDrawingAreaProxy::mapToContents(const IntRect& rect) const
{
    return enclosingIntRect(FloatRect(rect.x() / m_contentsScale,
                                      rect.y() / m_contentsScale,
                                      rect.width() / m_contentsScale,
                                      rect.height() / m_contentsScale));
}

IntRect TiledDrawingAreaProxy::mapFromContents(const IntRect& rect) const
{
    return enclosingIntRect(FloatRect(rect.x() * m_contentsScale,
                                      rect.y() * m_contentsScale,
                                      rect.width() * m_contentsScale,
                                      rect.height() * m_contentsScale));
}

IntRect TiledDrawingAreaProxy::contentsRect() const
{
    return mapFromContents(IntRect(IntPoint(0, 0), m_viewSize));
}

IntRect TiledDrawingAreaProxy::tileRectForCoordinate(const TiledDrawingAreaTile::Coordinate& coordinate) const
{
    IntRect rect(coordinate.x() * m_tileSize.width(),
                 coordinate.y() * m_tileSize.height(),
                 m_tileSize.width(),
                 m_tileSize.height());

    rect.intersect(contentsRect());
    return rect;
}

TiledDrawingAreaTile::Coordinate TiledDrawingAreaProxy::tileCoordinateForPoint(const IntPoint& point) const
{
    int x = point.x() / m_tileSize.width();
    int y = point.y() / m_tileSize.height();
    return TiledDrawingAreaTile::Coordinate(std::max(x, 0), std::max(y, 0));
}


void TiledDrawingAreaProxy::startTileBufferUpdateTimer()
{
    if (m_tileBufferUpdateTimer.isActive())
        return;
    m_tileBufferUpdateTimer.startOneShot(0);
}

void TiledDrawingAreaProxy::tileBufferUpdateTimerFired()
{
    updateTileBuffers();
}

void TiledDrawingAreaProxy::startTileCreationTimer()
{
    if (m_tileCreationTimer.isActive())
        return;
    m_tileCreationTimer.startOneShot(0);
}

void TiledDrawingAreaProxy::tileCreationTimerFired()
{
    createTiles();
}

bool TiledDrawingAreaProxy::hasPendingUpdates() const
{
    TileMap::const_iterator end = m_tiles.end();
    for (TileMap::const_iterator it = m_tiles.begin(); it != end; ++it) {
        const RefPtr<TiledDrawingAreaTile>& current = it->second;
        if (current->hasBackBufferUpdatePending())
            return true;
    }
    return false;
}

} // namespace WebKit

#endif
