/*
 Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.
 
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "TiledBackingStore.h"

#if ENABLE(TILED_BACKING_STORE)

#include "GraphicsContext.h"
#include "TiledBackingStoreClient.h"

namespace WebCore {
    
static const int defaultTileWidth = 512;
static const int defaultTileHeight = 512;

TiledBackingStore::TiledBackingStore(TiledBackingStoreClient* client)
    : m_client(client)
    , m_tileBufferUpdateTimer(new TileTimer(this, &TiledBackingStore::tileBufferUpdateTimerFired))
    , m_tileCreationTimer(new TileTimer(this, &TiledBackingStore::tileCreationTimerFired))
    , m_tileSize(defaultTileWidth, defaultTileHeight)
    , m_contentsScale(1.f)
    , m_pendingScale(0)
    , m_contentsFrozen(false)
{
}

TiledBackingStore::~TiledBackingStore()
{
    delete m_tileBufferUpdateTimer;
    delete m_tileCreationTimer;
}

void TiledBackingStore::invalidate(const IntRect& contentsDirtyRect)
{
    IntRect dirtyRect(mapFromContents(contentsDirtyRect));
    
    Tile::Coordinate topLeft = tileCoordinateForPoint(dirtyRect.topLeft());
    Tile::Coordinate bottomRight = tileCoordinateForPoint(dirtyRect.bottomRight());
    
    for (unsigned yCoordinate = topLeft.y(); yCoordinate <= bottomRight.y(); ++yCoordinate) {
        for (unsigned xCoordinate = topLeft.x(); xCoordinate <= bottomRight.x(); ++xCoordinate) {
            RefPtr<Tile> currentTile = tileAt(Tile::Coordinate(xCoordinate, yCoordinate));
            if (!currentTile)
                continue;
            currentTile->invalidate(dirtyRect);
        }
    }

    startTileBufferUpdateTimer();
}

void TiledBackingStore::updateTileBuffers()
{
    if (m_contentsFrozen)
        return;
    
    Vector<IntRect> paintedArea;
    Vector<RefPtr<Tile> > dirtyTiles;
    TileMap::iterator end = m_tiles.end();
    for (TileMap::iterator it = m_tiles.begin(); it != end; ++it) {
        if (!it->second->isDirty())
            continue;
        dirtyTiles.append(it->second);
        // FIXME: should not request system repaint for the full tile.
        paintedArea.append(mapToContents(it->second->rect()));
    }
    
    if (dirtyTiles.isEmpty())
        return;
    
    m_client->tiledBackingStorePaintBegin();

    // FIXME: In single threaded case, tile back buffers could be updated asynchronously 
    // one by one and then swapped to front in one go. This would minimize the time spent
    // blocking on tile updates.
    unsigned size = dirtyTiles.size();
    for (unsigned n = 0; n < size; ++n)
        dirtyTiles[n]->updateBackBuffer();

    for (unsigned n = 0; n < size; ++n)
        dirtyTiles[n]->swapBackBufferToFront();

    m_client->tiledBackingStorePaintEnd(paintedArea);
}

void TiledBackingStore::paint(GraphicsContext* context, const IntRect& rect)
{
    context->save();
    
    // Assumes the backing store is painted with the scale transform applied.
    // Since tile content is already scaled, first revert the scaling from the painter.
    context->scale(FloatSize(1.f / m_contentsScale, 1.f / m_contentsScale));
    
    IntRect dirtyRect = mapFromContents(rect);
    
    Tile::Coordinate topLeft = tileCoordinateForPoint(dirtyRect.topLeft());
    Tile::Coordinate bottomRight = tileCoordinateForPoint(dirtyRect.bottomRight());

    for (unsigned yCoordinate = topLeft.y(); yCoordinate <= bottomRight.y(); ++yCoordinate) {
        for (unsigned xCoordinate = topLeft.x(); xCoordinate <= bottomRight.x(); ++xCoordinate) {
            Tile::Coordinate currentCoordinate(xCoordinate, yCoordinate);
            RefPtr<Tile> currentTile = tileAt(currentCoordinate);
            if (currentTile && currentTile->isReadyToPaint())
                currentTile->paint(context, dirtyRect);
            else {
                FloatRect tileRect = tileRectForCoordinate(currentCoordinate);
                FloatRect target = intersection(tileRect, FloatRect(rect));
                Tile::paintCheckerPattern(context, target);
            }
        }
    }
    context->restore();
}

void TiledBackingStore::viewportChanged(const IntRect& contentsViewport)
{
    IntRect viewport = mapFromContents(contentsViewport);
    if (m_viewport == viewport)
        return;

    m_viewport = viewport;

    startTileCreationTimer();
}

void TiledBackingStore::setContentsScale(float scale)
{
    if (m_pendingScale == m_contentsScale) {
        m_pendingScale = 0;
        return;
    }
    m_pendingScale = scale;
    if (m_contentsFrozen)
        return;
    commitScaleChange();
}
    
void TiledBackingStore::commitScaleChange()
{
    m_contentsScale = m_pendingScale;
    m_pendingScale = 0;
    m_tiles.clear();
    createTiles();
}

double TiledBackingStore::tileDistance(const IntRect& viewport, const Tile::Coordinate& tileCoordinate)
{
    if (viewport.intersects(tileRectForCoordinate(tileCoordinate)))
        return 0;
    
    IntPoint viewCenter = viewport.location() + IntSize(viewport.width() / 2, viewport.height() / 2);
    Tile::Coordinate centerCoordinate = tileCoordinateForPoint(viewCenter);
    
    // Manhattan distance, biased so that vertical distances are shorter.
    const double horizontalBias = 1.3;
    return abs(centerCoordinate.y() - tileCoordinate.y()) + horizontalBias * abs(centerCoordinate.x() - tileCoordinate.x());
}

void TiledBackingStore::createTiles()
{
    if (m_contentsFrozen)
        return;

    if (m_viewport.isEmpty())
        return;

    // Remove tiles that extend outside the current contents rect.
    dropOverhangingTiles();

    // FIXME: Make configurable/adapt to memory.
    IntRect keepRect = m_viewport;
    keepRect.inflateX(m_viewport.width());
    keepRect.inflateY(3 * m_viewport.height());
    keepRect.intersect(contentsRect());
    
    dropTilesOutsideRect(keepRect);
    
    IntRect coverRect = m_viewport;
    coverRect.inflateX(m_viewport.width() / 2);
    coverRect.inflateY(2 * m_viewport.height());
    coverRect.intersect(contentsRect());
    
    // Search for the tile position closest to the viewport center that does not yet contain a tile. 
    // Which position is considered the closest depends on the tileDistance function.
    double shortestDistance = std::numeric_limits<double>::infinity();
    Vector<Tile::Coordinate> tilesToCreate;
    unsigned requiredTileCount = 0;
    Tile::Coordinate topLeft = tileCoordinateForPoint(coverRect.topLeft());
    Tile::Coordinate bottomRight = tileCoordinateForPoint(coverRect.bottomRight());
    for (unsigned yCoordinate = topLeft.y(); yCoordinate <= bottomRight.y(); ++yCoordinate) {
        for (unsigned xCoordinate = topLeft.x(); xCoordinate <= bottomRight.x(); ++xCoordinate) {
            Tile::Coordinate currentCoordinate(xCoordinate, yCoordinate);
            if (tileAt(currentCoordinate))
                continue;
            ++requiredTileCount;
            // Distance is 0 for all currently visible tiles.
            double distance = tileDistance(m_viewport, currentCoordinate);
            if (distance > shortestDistance)
                continue;
            if (distance < shortestDistance) {
                tilesToCreate.clear();
                shortestDistance = distance;
            }
            tilesToCreate.append(currentCoordinate);
        }
    }
    
    // Now construct the tile(s)
    unsigned tilesToCreateCount = tilesToCreate.size();
    for (unsigned n = 0; n < tilesToCreateCount; ++n) {
        Tile::Coordinate coordinate = tilesToCreate[n];
        setTile(coordinate, Tile::create(this, coordinate));
    }
    requiredTileCount -= tilesToCreateCount;
    
    // Paint the content of the newly created tiles
    if (tilesToCreateCount)
        updateTileBuffers();

    // Keep creating tiles until the whole coverRect is covered.
    if (requiredTileCount)
        m_tileCreationTimer->startOneShot(0);
}

void TiledBackingStore::dropOverhangingTiles()
{    
    IntRect contentsRect = this->contentsRect();

    Vector<Tile::Coordinate> tilesToRemove;
    TileMap::iterator end = m_tiles.end();
    for (TileMap::iterator it = m_tiles.begin(); it != end; ++it) {
        Tile::Coordinate tileCoordinate = it->second->coordinate();
        IntRect tileRect = it->second->rect();
        IntRect expectedTileRect = tileRectForCoordinate(tileCoordinate);
        if (expectedTileRect != tileRect || !contentsRect.contains(tileRect))
            tilesToRemove.append(tileCoordinate);
    }
    unsigned removeCount = tilesToRemove.size();
    for (unsigned n = 0; n < removeCount; ++n)
        removeTile(tilesToRemove[n]);
}

void TiledBackingStore::dropTilesOutsideRect(const IntRect& keepRect)
{
    FloatRect keepRectF = keepRect;

    Vector<Tile::Coordinate> toRemove;
    TileMap::iterator end = m_tiles.end();
    for (TileMap::iterator it = m_tiles.begin(); it != end; ++it) {
        Tile::Coordinate coordinate = it->second->coordinate();
        FloatRect tileRect = it->second->rect();
        if (!tileRect.intersects(keepRectF))
            toRemove.append(coordinate);
    }
    unsigned removeCount = toRemove.size();
    for (unsigned n = 0; n < removeCount; ++n)
        removeTile(toRemove[n]);
}

PassRefPtr<Tile> TiledBackingStore::tileAt(const Tile::Coordinate& coordinate) const
{
    return m_tiles.get(coordinate);
}

void TiledBackingStore::setTile(const Tile::Coordinate& coordinate, PassRefPtr<Tile> tile)
{
    m_tiles.set(coordinate, tile);
}

void TiledBackingStore::removeTile(const Tile::Coordinate& coordinate)
{
    m_tiles.remove(coordinate);
}

IntRect TiledBackingStore::mapToContents(const IntRect& rect) const
{
    return enclosingIntRect(FloatRect(rect.x() / m_contentsScale,
        rect.y() / m_contentsScale,
        rect.width() / m_contentsScale,
        rect.height() / m_contentsScale));
}

IntRect TiledBackingStore::mapFromContents(const IntRect& rect) const
{
    return enclosingIntRect(FloatRect(rect.x() * m_contentsScale,
        rect.y() * m_contentsScale,
        rect.width() * m_contentsScale,
        rect.height() * m_contentsScale));
}

IntRect TiledBackingStore::contentsRect() const
{
    return mapFromContents(m_client->tiledBackingStoreContentsRect());
}

IntRect TiledBackingStore::tileRectForCoordinate(const Tile::Coordinate& coordinate) const
{
    IntRect rect(coordinate.x() * m_tileSize.width(),
        coordinate.y() * m_tileSize.height(),
        m_tileSize.width(),
        m_tileSize.height());

    rect.intersect(contentsRect());
    return rect;
}
    
Tile::Coordinate TiledBackingStore::tileCoordinateForPoint(const IntPoint& point) const
{
    int x = point.x() / m_tileSize.width();
    int y = point.y() / m_tileSize.height();
    return Tile::Coordinate(std::max(x, 0), std::max(y, 0));
}


void TiledBackingStore::startTileBufferUpdateTimer()
{
    if (m_tileBufferUpdateTimer->isActive() || m_contentsFrozen)
        return;
    m_tileBufferUpdateTimer->startOneShot(0);
}

void TiledBackingStore::tileBufferUpdateTimerFired(TileTimer*)
{
    updateTileBuffers();
}

void TiledBackingStore::startTileCreationTimer()
{
    if (m_tileCreationTimer->isActive() || m_contentsFrozen)
        return;
    m_tileCreationTimer->startOneShot(0);
}

void TiledBackingStore::tileCreationTimerFired(TileTimer*)
{
    createTiles();
}

void TiledBackingStore::setContentsFrozen(bool freeze)
{
    if (m_contentsFrozen == freeze)
        return;

    m_contentsFrozen = freeze;

    // Restart the timers. There might be pending invalidations that
    // were not painted or created because tiles are not created or
    // painted when in frozen state.
    if (m_contentsFrozen)
        return;
    if (m_pendingScale)
        commitScaleChange();
    else {
        startTileCreationTimer();
        startTileBufferUpdateTimer();
    }
}

}

#endif
