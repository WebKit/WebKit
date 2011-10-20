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

#if USE(TILED_BACKING_STORE)

#include "GraphicsContext.h"
#include "TiledBackingStoreClient.h"

namespace WebCore {
    
static const int defaultTileWidth = 512;
static const int defaultTileHeight = 512;

static IntPoint innerBottomRight(const IntRect& rect)
{
    // Actually, the rect does not contain rect.maxX(). Refer to IntRect::contain.
    return IntPoint(rect.maxX() - 1, rect.maxY() - 1);
}

TiledBackingStore::TiledBackingStore(TiledBackingStoreClient* client, PassOwnPtr<TiledBackingStoreBackend> backend)
    : m_client(client)
    , m_backend(backend)
    , m_tileBufferUpdateTimer(new TileTimer(this, &TiledBackingStore::tileBufferUpdateTimerFired))
    , m_tileCreationTimer(new TileTimer(this, &TiledBackingStore::tileCreationTimerFired))
    , m_tileSize(defaultTileWidth, defaultTileHeight)
    , m_tileCreationDelay(0.01)
    , m_keepAreaMultiplier(3.5f)
    , m_coverAreaMultiplier(2.5f)
    , m_contentsScale(1.f)
    , m_pendingScale(0)
    , m_contentsFrozen(false)
{
    ASSERT(m_coverAreaMultiplier <= m_keepAreaMultiplier);
}

TiledBackingStore::~TiledBackingStore()
{
    delete m_tileBufferUpdateTimer;
    delete m_tileCreationTimer;
}
    
void TiledBackingStore::setTileSize(const IntSize& size)
{
    m_tileSize = size;
    m_tiles.clear();
    startTileCreationTimer();
}

void TiledBackingStore::setTileCreationDelay(double delay)
{
    m_tileCreationDelay = delay;
}

void TiledBackingStore::setKeepAndCoverAreaMultipliers(float keepMultiplier, float coverMultiplier)
{
    ASSERT(coverMultiplier <= keepMultiplier);
    m_keepAreaMultiplier = keepMultiplier;
    m_coverAreaMultiplier = coverMultiplier;
    startTileCreationTimer();
}

void TiledBackingStore::setVisibleRectTrajectoryVector(const FloatPoint& vector)
{
    if (m_visibleRectTrajectoryVector == vector)
        return;

    m_visibleRectTrajectoryVector = vector;
    startTileCreationTimer();
}

void TiledBackingStore::invalidate(const IntRect& contentsDirtyRect)
{
    IntRect dirtyRect(mapFromContents(contentsDirtyRect));
    
    Tile::Coordinate topLeft = tileCoordinateForPoint(dirtyRect.location());
    Tile::Coordinate bottomRight = tileCoordinateForPoint(innerBottomRight(dirtyRect));
    
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
    if (!m_client->tiledBackingStoreUpdatesAllowed() || m_contentsFrozen)
        return;
    
    m_client->tiledBackingStorePaintBegin();

    Vector<IntRect> paintedArea;
    Vector<RefPtr<Tile> > dirtyTiles;
    TileMap::iterator end = m_tiles.end();
    for (TileMap::iterator it = m_tiles.begin(); it != end; ++it) {
        if (!it->second->isDirty())
            continue;
        dirtyTiles.append(it->second);
    }
    
    if (dirtyTiles.isEmpty()) {
        m_client->tiledBackingStorePaintEnd(paintedArea);
        return;
    }

    // FIXME: In single threaded case, tile back buffers could be updated asynchronously 
    // one by one and then swapped to front in one go. This would minimize the time spent
    // blocking on tile updates.
    unsigned size = dirtyTiles.size();
    for (unsigned n = 0; n < size; ++n) {
        Vector<IntRect> paintedRects = dirtyTiles[n]->updateBackBuffer();
        paintedArea.append(paintedRects);
        dirtyTiles[n]->swapBackBufferToFront();
    }

    m_client->tiledBackingStorePaintEnd(paintedArea);
}

void TiledBackingStore::paint(GraphicsContext* context, const IntRect& rect)
{
    context->save();
    
    // Assumes the backing store is painted with the scale transform applied.
    // Since tile content is already scaled, first revert the scaling from the painter.
    context->scale(FloatSize(1.f / m_contentsScale, 1.f / m_contentsScale));
    
    IntRect dirtyRect = mapFromContents(rect);
    
    Tile::Coordinate topLeft = tileCoordinateForPoint(dirtyRect.location());
    Tile::Coordinate bottomRight = tileCoordinateForPoint(innerBottomRight(dirtyRect));

    for (unsigned yCoordinate = topLeft.y(); yCoordinate <= bottomRight.y(); ++yCoordinate) {
        for (unsigned xCoordinate = topLeft.x(); xCoordinate <= bottomRight.x(); ++xCoordinate) {
            Tile::Coordinate currentCoordinate(xCoordinate, yCoordinate);
            RefPtr<Tile> currentTile = tileAt(currentCoordinate);
            if (currentTile && currentTile->isReadyToPaint())
                currentTile->paint(context, dirtyRect);
            else {
                IntRect tileRect = tileRectForCoordinate(currentCoordinate);
                IntRect target = intersection(tileRect, dirtyRect);
                if (target.isEmpty())
                    continue;
                m_backend->paintCheckerPattern(context, FloatRect(target));
            }
        }
    }
    context->restore();
}

void TiledBackingStore::adjustVisibleRect()
{
    IntRect visibleRect = visibleContentsRect();
    if (m_previousVisibleRect == visibleRect)
        return;
    m_previousVisibleRect = visibleRect;

    startTileCreationTimer();
}

IntRect TiledBackingStore::visibleContentsRect()
{
    return mapFromContents(intersection(m_client->tiledBackingStoreVisibleRect(), m_client->tiledBackingStoreContentsRect()));
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

double TiledBackingStore::tileDistance(const IntRect& viewport, const Tile::Coordinate& tileCoordinate) const
{
    if (viewport.intersects(tileRectForCoordinate(tileCoordinate)))
        return 0;
    
    IntPoint viewCenter = viewport.location() + IntSize(viewport.width() / 2, viewport.height() / 2);
    Tile::Coordinate centerCoordinate = tileCoordinateForPoint(viewCenter);
    
    // Manhattan distance, biased so that vertical distances are shorter.
    const double horizontalBias = 1.3;
    return abs(centerCoordinate.y() - tileCoordinate.y()) + horizontalBias * abs(centerCoordinate.x() - tileCoordinate.x());
}

// Returns a ratio between 0.0f and 1.0f of the surface of contentsRect covered by rendered tiles.
float TiledBackingStore::coverageRatio(const WebCore::IntRect& contentsRect)
{
    IntRect dirtyRect = mapFromContents(contentsRect);
    float rectArea = dirtyRect.width() * dirtyRect.height();
    float coverArea = 0.0f;

    Tile::Coordinate topLeft = tileCoordinateForPoint(dirtyRect.location());
    Tile::Coordinate bottomRight = tileCoordinateForPoint(innerBottomRight(dirtyRect));

    for (unsigned yCoordinate = topLeft.y(); yCoordinate <= bottomRight.y(); ++yCoordinate) {
        for (unsigned xCoordinate = topLeft.x(); xCoordinate <= bottomRight.x(); ++xCoordinate) {
            Tile::Coordinate currentCoordinate(xCoordinate, yCoordinate);
            RefPtr<Tile> currentTile = tileAt(Tile::Coordinate(xCoordinate, yCoordinate));
            if (currentTile && currentTile->isReadyToPaint()) {
                IntRect coverRect = intersection(dirtyRect, currentTile->rect());
                coverArea += coverRect.width() * coverRect.height();
            }
        }
    }
    return coverArea / rectArea;
}

void TiledBackingStore::createTiles()
{
    if (m_contentsFrozen)
        return;
    
    IntRect visibleRect = visibleContentsRect();
    m_previousVisibleRect = visibleRect;

    if (visibleRect.isEmpty())
        return;

    // Resize tiles on edges in case the contents size has changed.
    bool didResizeTiles = resizeEdgeTiles();

    IntRect keepRect = computeKeepRect(visibleRect);
    
    dropTilesOutsideRect(keepRect);
    
    IntRect coverRect = computeCoverRect(visibleRect);
    ASSERT(keepRect.contains(coverRect));
    
    // Search for the tile position closest to the viewport center that does not yet contain a tile. 
    // Which position is considered the closest depends on the tileDistance function.
    double shortestDistance = std::numeric_limits<double>::infinity();
    Vector<Tile::Coordinate> tilesToCreate;
    unsigned requiredTileCount = 0;
    Tile::Coordinate topLeft = tileCoordinateForPoint(coverRect.location());
    Tile::Coordinate bottomRight = tileCoordinateForPoint(innerBottomRight(coverRect));
    for (unsigned yCoordinate = topLeft.y(); yCoordinate <= bottomRight.y(); ++yCoordinate) {
        for (unsigned xCoordinate = topLeft.x(); xCoordinate <= bottomRight.x(); ++xCoordinate) {
            Tile::Coordinate currentCoordinate(xCoordinate, yCoordinate);
            if (tileAt(currentCoordinate))
                continue;
            ++requiredTileCount;
            // Distance is 0 for all currently visible tiles.
            double distance = tileDistance(visibleRect, currentCoordinate);
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
        setTile(coordinate, m_backend->createTile(this, coordinate));
    }
    requiredTileCount -= tilesToCreateCount;
    
    // Paint the content of the newly created tiles
    if (tilesToCreateCount || didResizeTiles)
        updateTileBuffers();

    // Keep creating tiles until the whole coverRect is covered.
    if (requiredTileCount)
        m_tileCreationTimer->startOneShot(m_tileCreationDelay);
}

IntRect TiledBackingStore::computeKeepRect(const IntRect& visibleRect) const
{
    IntRect result = visibleRect;
    // Inflates to both sides, so divide the inflate delta by 2.
    result.inflateX(visibleRect.width() * (m_keepAreaMultiplier - 1) / 2);
    result.inflateY(visibleRect.height() * (m_keepAreaMultiplier - 1) / 2);
    result.intersect(contentsRect());

    return result;
}

// A null trajectory vector means that tiles intersecting all the coverArea (i.e. visibleRect * coverMultiplier) will be created.
// A non-null trajectory vector will shrink the intersection rect to visibleRect plus its expansion from its
// center toward the cover area edges in the direction of the given vector.
// E.g. if visibleRect == (10,10)5x5 and coverMultiplier == 3.0:
// a (0,0) trajectory vector will create tiles intersecting (5,5)15x15,
// a (1,0) trajectory vector will create tiles intersecting (10,10)10x5,
// and a (1,1) trajectory vector will create tiles intersecting (10,10)10x10.
IntRect TiledBackingStore::computeCoverRect(const IntRect& visibleRect) const
{
    IntRect result = visibleRect;
    float trajectoryVectorNorm = sqrt(pow(m_visibleRectTrajectoryVector.x(), 2) + pow(m_visibleRectTrajectoryVector.y(), 2));
    if (trajectoryVectorNorm > 0) {
        // Multiply the vector by the distance to the edge of the cover area.
        float trajectoryVectorMultiplier = (m_coverAreaMultiplier - 1) / 2;
        // Unite the visible rect with a "ghost" of the visible rect moved in the direction of the trajectory vector.
        result.move(result.width() * m_visibleRectTrajectoryVector.x() / trajectoryVectorNorm * trajectoryVectorMultiplier,
                    result.height() * m_visibleRectTrajectoryVector.y() / trajectoryVectorNorm * trajectoryVectorMultiplier);
        result.unite(visibleRect);
    } else {
        result.inflateX(visibleRect.width() * (m_coverAreaMultiplier - 1) / 2);
        result.inflateY(visibleRect.height() * (m_coverAreaMultiplier - 1) / 2);
    }
    result.intersect(contentsRect());

    return result;
}

bool TiledBackingStore::resizeEdgeTiles()
{
    bool wasResized = false;

    Vector<Tile::Coordinate> tilesToRemove;
    TileMap::iterator end = m_tiles.end();
    for (TileMap::iterator it = m_tiles.begin(); it != end; ++it) {
        Tile::Coordinate tileCoordinate = it->second->coordinate();
        IntRect tileRect = it->second->rect();
        IntRect expectedTileRect = tileRectForCoordinate(tileCoordinate);
        if (expectedTileRect.isEmpty())
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
    if (m_tileBufferUpdateTimer->isActive() || !m_client->tiledBackingStoreUpdatesAllowed() || m_contentsFrozen)
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
