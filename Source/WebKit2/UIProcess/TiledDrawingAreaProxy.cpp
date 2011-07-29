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
#include "DrawingAreaMessages.h"
#include "DrawingAreaProxyMessages.h"
#include "MessageID.h"
#include "UpdateInfo.h"
#include "WebCoreArgumentCoders.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"

using namespace WebCore;

namespace WebKit {

static const int defaultTileWidth = 1024;
static const int defaultTileHeight = 1024;


// The TileSet's responsibility is to hold a group of tiles with the same contents scale together.
class TiledDrawingAreaTileSet {
public:
    typedef HashMap<TiledDrawingAreaTile::Coordinate, RefPtr<TiledDrawingAreaTile> > TileMap;

    TiledDrawingAreaTileSet(float contentsScale = 1.0f);

    WebCore::IntRect mapToContents(const WebCore::IntRect&) const;
    WebCore::IntRect mapFromContents(const WebCore::IntRect&) const;

    TileMap& tiles() { return m_tiles; }
    float contentsScale() const { return m_contentsScale; }

private:
    TileMap m_tiles;
    float m_contentsScale;
};

TiledDrawingAreaTileSet::TiledDrawingAreaTileSet(float contentsScale)
    : m_contentsScale(contentsScale)
{
}

IntRect TiledDrawingAreaTileSet::mapToContents(const IntRect& rect) const
{
    return enclosingIntRect(FloatRect(rect.x() / m_contentsScale,
                                      rect.y() / m_contentsScale,
                                      rect.width() / m_contentsScale,
                                      rect.height() / m_contentsScale));
}

IntRect TiledDrawingAreaTileSet::mapFromContents(const IntRect& rect) const
{
    return enclosingIntRect(FloatRect(rect.x() * m_contentsScale,
                                      rect.y() * m_contentsScale,
                                      rect.width() * m_contentsScale,
                                      rect.height() * m_contentsScale));
}


static IntPoint innerBottomRight(const IntRect& rect)
{
    // Actually, the rect does not contain rect.maxX(). Refer to IntRect::contain.
    return IntPoint(rect.maxX() - 1, rect.maxY() - 1);
}

PassOwnPtr<TiledDrawingAreaProxy> TiledDrawingAreaProxy::create(PlatformWebView* webView, WebPageProxy* webPageProxy)
{
    return adoptPtr(new TiledDrawingAreaProxy(webView, webPageProxy));
}

TiledDrawingAreaProxy::TiledDrawingAreaProxy(PlatformWebView* webView, WebPageProxy* webPageProxy)
    : DrawingAreaProxy(DrawingAreaTypeTiled, webPageProxy)
    , m_isWaitingForDidSetFrameNotification(false)
    , m_isVisible(true)
    , m_webView(webView)
    , m_currentTileSet(adoptPtr(new TiledDrawingAreaTileSet))
    , m_tileBufferUpdateTimer(RunLoop::main(), this, &TiledDrawingAreaProxy::tileBufferUpdateTimerFired)
    , m_tileCreationTimer(RunLoop::main(), this, &TiledDrawingAreaProxy::tileCreationTimerFired)
    , m_tileSize(defaultTileWidth, defaultTileHeight)
    , m_tileCreationDelay(0.01)
    , m_keepAreaMultiplier(2.5, 4.5)
    , m_coverAreaMultiplier(2, 3)
{
}

TiledDrawingAreaProxy::~TiledDrawingAreaProxy()
{
    // Disable updates on tiles to detach them from the proxy and cut the circular dependency.
    if (m_previousTileSet)
        disableTileSetUpdates(m_previousTileSet.get());
    disableTileSetUpdates(m_currentTileSet.get());
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
    page->process()->send(Messages::DrawingArea::SetSize(m_size), page->pageID());
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
        page->process()->send(Messages::DrawingArea::SuspendPainting(), page->pageID());
        return;
    }

    // The page is now visible.
    page->process()->send(Messages::DrawingArea::ResumePainting(), page->pageID());

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

void TiledDrawingAreaProxy::tileUpdated(int tileID, const UpdateInfo& updateInfo, float scale, unsigned pendingUpdateCount)
{
    TiledDrawingAreaTile* tile = m_tilesByID.get(tileID);
    ASSERT(!tile || tile->ID() == tileID);
    if (tile)
        tile->incorporateUpdate(updateInfo, scale);
    tileBufferUpdateComplete();
}

void TiledDrawingAreaProxy::allTileUpdatesProcessed()
{
    tileBufferUpdateComplete();
}

void TiledDrawingAreaProxy::registerTile(int tileID, TiledDrawingAreaTile* tile)
{
    m_tilesByID.set(tileID, tile);
}

void TiledDrawingAreaProxy::unregisterTile(int tileID)
{
    m_tilesByID.remove(tileID);
}

void TiledDrawingAreaProxy::requestTileUpdate(int tileID, const IntRect& dirtyRect)
{
    page()->process()->connection()->send(Messages::DrawingArea::RequestTileUpdate(tileID, dirtyRect, m_currentTileSet->contentsScale()), page()->pageID());
}

void TiledDrawingAreaProxy::cancelTileUpdate(int tileID)
{
    if (!page()->process()->isValid())
        return;
    page()->process()->send(Messages::DrawingArea::CancelTileUpdate(tileID), page()->pageID());
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

void TiledDrawingAreaProxy::invalidate(const IntRect& contentsDirtyRect)
{
    IntRect dirtyRect(m_currentTileSet->mapFromContents(contentsDirtyRect));

    TiledDrawingAreaTile::Coordinate topLeft = tileCoordinateForPoint(dirtyRect.location());
    TiledDrawingAreaTile::Coordinate bottomRight = tileCoordinateForPoint(innerBottomRight(dirtyRect));

    IntRect coverRect = calculateCoverRect(visibleRect());

    Vector<TiledDrawingAreaTile::Coordinate> tilesToRemove;

    for (unsigned yCoordinate = topLeft.y(); yCoordinate <= bottomRight.y(); ++yCoordinate) {
        for (unsigned xCoordinate = topLeft.x(); xCoordinate <= bottomRight.x(); ++xCoordinate) {
            RefPtr<TiledDrawingAreaTile> currentTile = m_currentTileSet->tiles().get(TiledDrawingAreaTile::Coordinate(xCoordinate, yCoordinate));
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
        m_currentTileSet->tiles().remove(tilesToRemove[n]);

    startTileBufferUpdateTimer();
}

void TiledDrawingAreaProxy::updateTileBuffers()
{
    Vector<RefPtr<TiledDrawingAreaTile> > newDirtyTiles;
    TiledDrawingAreaTileSet::TileMap::iterator end = m_currentTileSet->tiles().end();
    for (TiledDrawingAreaTileSet::TileMap::iterator it = m_currentTileSet->tiles().begin(); it != end; ++it) {
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
    TiledDrawingAreaTileSet::TileMap::iterator end = m_currentTileSet->tiles().end();
    for (TiledDrawingAreaTileSet::TileMap::iterator it = m_currentTileSet->tiles().begin(); it != end; ++it) {
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
        paintedArea.append(m_currentTileSet->mapToContents(tile->rect()));
    }

    if (m_previousTileSet && (coverageRatio(m_currentTileSet.get(), visibleRect()) >= 1.0f || !hasPendingUpdates())) {
        paintedArea.clear();
        paintedArea.append(m_visibleContentRect);
        m_previousTileSet.clear();
    }

    if (!paintedArea.isEmpty())
        updateWebView(paintedArea);

    m_tileCreationTimer.startOneShot(0);
}

void TiledDrawingAreaProxy::paint(TiledDrawingAreaTileSet* tileSet, const IntRect& rect, WebCore::GraphicsContext& gc)
{
    IntRect dirtyRect = tileSet->mapFromContents(rect);

    TiledDrawingAreaTile::Coordinate topLeft = tileCoordinateForPoint(dirtyRect.location());
    TiledDrawingAreaTile::Coordinate bottomRight = tileCoordinateForPoint(innerBottomRight(dirtyRect));

    for (unsigned yCoordinate = topLeft.y(); yCoordinate <= bottomRight.y(); ++yCoordinate) {
        for (unsigned xCoordinate = topLeft.x(); xCoordinate <= bottomRight.x(); ++xCoordinate) {
            TiledDrawingAreaTile::Coordinate currentCoordinate(xCoordinate, yCoordinate);
            RefPtr<TiledDrawingAreaTile> currentTile = tileSet->tiles().get(currentCoordinate);
            if (currentTile && currentTile->isReadyToPaint())
                currentTile->paint(&gc, dirtyRect);
        }
    }
}

bool TiledDrawingAreaProxy::paint(const WebCore::IntRect& rect, PlatformDrawingContext context)
{
    if (m_isWaitingForDidSetFrameNotification) {
        WebPageProxy* page = this->page();
        if (!page->isValid())
            return false;

        if (page->process()->isLaunching())
            return false;
    }

    GraphicsContext gc(context);
    gc.save();

    // Assumes the backing store is painted with the scale transform applied.
    // Since tile content is already scaled, first revert the scaling from the painter.
    gc.scale(FloatSize(1 / m_currentTileSet->contentsScale(), 1 / m_currentTileSet->contentsScale()));
    paint(m_currentTileSet.get(), rect, gc);

    // Paint the previous scale tiles, if any, over the currently updating tiles.
    if (m_previousTileSet) {
        // Re-apply the reverted current tiles scaling and then
        // revert the previous tiles scaling to get matched contents scale.
        float currentToPreviousRevertedScale = m_currentTileSet->contentsScale() / m_previousTileSet->contentsScale();
        gc.scale(FloatSize(currentToPreviousRevertedScale, currentToPreviousRevertedScale));
        paint(m_previousTileSet.get(), rect, gc);
    }

    gc.restore();
    return true;
}

void TiledDrawingAreaProxy::setVisibleContentRect(const WebCore::IntRect& visibleContentRect)
{
    if (m_visibleContentRect != visibleContentRect) {
        m_visibleContentRect = visibleContentRect;
        startTileCreationTimer();
    }
}

float TiledDrawingAreaProxy::coverageRatio(TiledDrawingAreaTileSet* tileSet, const WebCore::IntRect& rect)
{
    IntRect dirtyRect = tileSet->mapFromContents(rect);
    float rectArea = dirtyRect.width() * dirtyRect.height();
    float coverArea = 0.0f;

    TiledDrawingAreaTile::Coordinate topLeft = tileCoordinateForPoint(dirtyRect.location());
    TiledDrawingAreaTile::Coordinate bottomRight = tileCoordinateForPoint(innerBottomRight(dirtyRect));

    for (unsigned yCoordinate = topLeft.y(); yCoordinate <= bottomRight.y(); ++yCoordinate) {
        for (unsigned xCoordinate = topLeft.x(); xCoordinate <= bottomRight.x(); ++xCoordinate) {
            TiledDrawingAreaTile::Coordinate currentCoordinate(xCoordinate, yCoordinate);
            RefPtr<TiledDrawingAreaTile> currentTile = tileSet->tiles().get(TiledDrawingAreaTile::Coordinate(xCoordinate, yCoordinate));
            if (currentTile && currentTile->isReadyToPaint()) {
                IntRect coverRect = intersection(dirtyRect, currentTile->rect());
                coverArea += coverRect.width() * coverRect.height();
            }
        }
    }
    return coverArea / rectArea;
}

void TiledDrawingAreaProxy::setContentsScale(float scale)
{
    if (m_currentTileSet->contentsScale() == scale)
        return;

    // Keep the current tiles in m_previousTileSet while the new scale tiles are being updated.
    // If m_currentTileSet still hasn't more paintable content for the current viewport rect
    // than m_previousTileSet, then keep the old m_previousTileSet. This happens when
    // setContentsScale is called twice while m_currentTileSet still contains few or no rendered tiles.
    if (!m_previousTileSet || coverageRatio(m_previousTileSet.get(), visibleRect()) < coverageRatio(m_currentTileSet.get(), visibleRect())) {
        m_previousTileSet = m_currentTileSet.release();
        disableTileSetUpdates(m_previousTileSet.get());
    }
    m_currentTileSet = adoptPtr(new TiledDrawingAreaTileSet(scale));
    createTiles();
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
    IntRect visibleRect = this->visibleRect();
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
    TiledDrawingAreaTile::Coordinate bottomRight = tileCoordinateForPoint(innerBottomRight(visibleRect));
    for (unsigned yCoordinate = topLeft.y(); yCoordinate <= bottomRight.y(); ++yCoordinate) {
        for (unsigned xCoordinate = topLeft.x(); xCoordinate <= bottomRight.x(); ++xCoordinate) {
            TiledDrawingAreaTile::Coordinate currentCoordinate(xCoordinate, yCoordinate);
            // Distance is 0 for all currently visible tiles.
            double distance = tileDistance(visibleRect, currentCoordinate);

            RefPtr<TiledDrawingAreaTile> tile = m_currentTileSet->tiles().get(currentCoordinate);
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
    for (unsigned n = 0; n < tilesToCreateCount; ++n) {
        TiledDrawingAreaTile::Coordinate coordinate = tilesToCreate[n];
        RefPtr<TiledDrawingAreaTile> tile = TiledDrawingAreaTile::create(this, coordinate);
        m_currentTileSet->tiles().set(coordinate, tile);
    }

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
    TiledDrawingAreaTileSet::TileMap::iterator end = m_currentTileSet->tiles().end();
    for (TiledDrawingAreaTileSet::TileMap::iterator it = m_currentTileSet->tiles().begin(); it != end; ++it) {
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
        m_currentTileSet->tiles().remove(tilesToRemove[n]);
    return wasResized;
}

void TiledDrawingAreaProxy::dropTilesOutsideRect(const IntRect& keepRect)
{
    FloatRect keepRectF = keepRect;

    Vector<TiledDrawingAreaTile::Coordinate> toRemove;
    TiledDrawingAreaTileSet::TileMap::iterator end = m_currentTileSet->tiles().end();
    for (TiledDrawingAreaTileSet::TileMap::iterator it = m_currentTileSet->tiles().begin(); it != end; ++it) {
        TiledDrawingAreaTile::Coordinate coordinate = it->second->coordinate();
        FloatRect tileRect = it->second->rect();
        if (!tileRect.intersects(keepRectF))
            toRemove.append(coordinate);
    }
    unsigned removeCount = toRemove.size();
    for (unsigned n = 0; n < removeCount; ++n)
        m_currentTileSet->tiles().remove(toRemove[n]);
}

void TiledDrawingAreaProxy::disableTileSetUpdates(TiledDrawingAreaTileSet* tileSet)
{
    TiledDrawingAreaTileSet::TileMap::const_iterator end = tileSet->tiles().end();
    for (TiledDrawingAreaTileSet::TileMap::const_iterator it = tileSet->tiles().begin(); it != end; ++it)
        it->second->disableUpdates();
}

void TiledDrawingAreaProxy::removeAllTiles()
{
    m_currentTileSet = adoptPtr(new TiledDrawingAreaTileSet(m_currentTileSet->contentsScale()));
}


IntRect TiledDrawingAreaProxy::contentsRect() const
{
    return m_currentTileSet->mapFromContents(IntRect(IntPoint(0, 0), m_viewSize));
}

IntRect TiledDrawingAreaProxy::visibleRect() const
{
    return m_currentTileSet->mapFromContents(m_visibleContentRect);
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
    TiledDrawingAreaTileSet::TileMap::const_iterator end = m_currentTileSet->tiles().end();
    for (TiledDrawingAreaTileSet::TileMap::const_iterator it = m_currentTileSet->tiles().begin(); it != end; ++it) {
        const RefPtr<TiledDrawingAreaTile>& current = it->second;
        if (current->hasBackBufferUpdatePending())
            return true;
    }
    return false;
}

} // namespace WebKit

#endif
