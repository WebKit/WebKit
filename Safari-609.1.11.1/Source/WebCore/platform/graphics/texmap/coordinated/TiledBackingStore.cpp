/*
 Copyright (C) 2010-2012 Nokia Corporation and/or its subsidiary(-ies)
 
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

#if USE(COORDINATED_GRAPHICS)
#include "GraphicsContext.h"
#include "TiledBackingStoreClient.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/MemoryPressureHandler.h>

namespace WebCore {

static const int defaultTileDimension = 512;

static IntPoint innerBottomRight(const IntRect& rect)
{
    // Actually, the rect does not contain rect.maxX(). Refer to IntRect::contain.
    return IntPoint(rect.maxX() - 1, rect.maxY() - 1);
}

TiledBackingStore::TiledBackingStore(TiledBackingStoreClient& client, float contentsScale)
    : m_client(client)
    , m_tileSize(defaultTileDimension, defaultTileDimension)
    , m_coverAreaMultiplier(2.0f)
    , m_contentsScale(contentsScale)
    , m_pendingTileCreation(false)
{
}

TiledBackingStore::~TiledBackingStore() = default;

void TiledBackingStore::setTrajectoryVector(const FloatPoint& trajectoryVector)
{
    m_pendingTrajectoryVector = trajectoryVector;
    m_pendingTrajectoryVector.normalize();
}

void TiledBackingStore::createTilesIfNeeded(const IntRect& unscaledVisibleRect, const IntRect& contentsRect)
{
    IntRect scaledContentsRect = mapFromContents(contentsRect);
    IntRect visibleRect = mapFromContents(unscaledVisibleRect);
    float coverAreaMultiplier = MemoryPressureHandler::singleton().isUnderMemoryPressure() ? 1.0f : 2.0f;

    bool didChange = m_trajectoryVector != m_pendingTrajectoryVector || m_visibleRect != visibleRect || m_rect != scaledContentsRect || m_coverAreaMultiplier != coverAreaMultiplier;
    if (didChange || m_pendingTileCreation)
        createTiles(visibleRect, scaledContentsRect, coverAreaMultiplier);
}

void TiledBackingStore::invalidate(const IntRect& contentsDirtyRect)
{
    IntRect dirtyRect(mapFromContents(contentsDirtyRect));
    IntRect keepRectFitToTileSize = tileRectForCoordinate(tileCoordinateForPoint(m_keepRect.location()));
    keepRectFitToTileSize.unite(tileRectForCoordinate(tileCoordinateForPoint(innerBottomRight(m_keepRect))));

    // Only iterate on the part of the rect that we know we might have tiles.
    IntRect coveredDirtyRect = intersection(dirtyRect, keepRectFitToTileSize);
    Tile::Coordinate topLeft = tileCoordinateForPoint(coveredDirtyRect.location());
    Tile::Coordinate bottomRight = tileCoordinateForPoint(innerBottomRight(coveredDirtyRect));

    for (int yCoordinate = topLeft.y(); yCoordinate <= bottomRight.y(); ++yCoordinate) {
        for (int xCoordinate = topLeft.x(); xCoordinate <= bottomRight.x(); ++xCoordinate) {
            Tile* currentTile = m_tiles.get(Tile::Coordinate(xCoordinate, yCoordinate));
            if (!currentTile)
                continue;
            // Pass the full rect to each tile as coveredDirtyRect might not
            // contain them completely and we don't want partial tile redraws.
            currentTile->invalidate(dirtyRect);
        }
    }
}

Vector<std::reference_wrapper<Tile>> TiledBackingStore::dirtyTiles()
{
    Vector<std::reference_wrapper<Tile>> tiles;
    for (auto& tile : m_tiles.values()) {
        if (tile->isDirty())
            tiles.append(*tile);
    }

    return tiles;
}

double TiledBackingStore::tileDistance(const IntRect& viewport, const Tile::Coordinate& tileCoordinate) const
{
    if (viewport.intersects(tileRectForCoordinate(tileCoordinate)))
        return 0;

    IntPoint viewCenter = viewport.location() + IntSize(viewport.width() / 2, viewport.height() / 2);
    Tile::Coordinate centerCoordinate = tileCoordinateForPoint(viewCenter);

    return std::max(abs(centerCoordinate.y() - tileCoordinate.y()), abs(centerCoordinate.x() - tileCoordinate.x()));
}

// Returns a ratio between 0.0f and 1.0f of the surface covered by rendered tiles.
float TiledBackingStore::coverageRatio(const WebCore::IntRect& dirtyRect) const
{
    float rectArea = dirtyRect.width() * dirtyRect.height();
    float coverArea = 0.0f;

    Tile::Coordinate topLeft = tileCoordinateForPoint(dirtyRect.location());
    Tile::Coordinate bottomRight = tileCoordinateForPoint(innerBottomRight(dirtyRect));

    for (int yCoordinate = topLeft.y(); yCoordinate <= bottomRight.y(); ++yCoordinate) {
        for (int xCoordinate = topLeft.x(); xCoordinate <= bottomRight.x(); ++xCoordinate) {
            Tile::Coordinate currentCoordinate(xCoordinate, yCoordinate);
            Tile* currentTile = m_tiles.get(currentCoordinate);
            if (currentTile && currentTile->isReadyToPaint()) {
                IntRect coverRect = intersection(dirtyRect, currentTile->rect());
                coverArea += coverRect.width() * coverRect.height();
            }
        }
    }
    return coverArea / rectArea;
}

bool TiledBackingStore::visibleAreaIsCovered() const
{
    return coverageRatio(intersection(m_visibleRect, m_rect)) == 1.0f;
}

void TiledBackingStore::createTiles(const IntRect& visibleRect, const IntRect& scaledContentsRect, float coverAreaMultiplier)
{
    // Update our backing store geometry.
    m_rect = scaledContentsRect;
    m_trajectoryVector = m_pendingTrajectoryVector;
    m_visibleRect = visibleRect;
    m_coverAreaMultiplier = coverAreaMultiplier;

    if (m_rect.isEmpty()) {
        setCoverRect(IntRect());
        setKeepRect(IntRect());
        return;
    }

    /* We must compute cover and keep rects using the visibleRect, instead of the rect intersecting the visibleRect with m_rect,
     * because TBS can be used as a backing store of GraphicsLayer and the visible rect usually does not intersect with m_rect.
     * In the below case, the intersecting rect is an empty.
     *
     *  +---------------+
     *  |               |
     *  |   m_rect      |
     *  |       +-------|-----------------------+
     *  |       | HERE  |  cover or keep        |
     *  +---------------+      rect             |
     *          |         +---------+           |
     *          |         | visible |           |
     *          |         |  rect   |           |
     *          |         +---------+           |
     *          |                               |
     *          |                               |
     *          +-------------------------------+
     *
     * We must create or keep the tiles in the HERE region.
     */

    IntRect coverRect;
    IntRect keepRect;
    computeCoverAndKeepRect(m_visibleRect, coverRect, keepRect);

    setCoverRect(coverRect);
    setKeepRect(keepRect);

    if (coverRect.isEmpty())
        return;

    // Resize tiles at the edge in case the contents size has changed, but only do so
    // after having dropped tiles outside the keep rect.
    if (m_previousRect != m_rect) {
        m_previousRect = m_rect;
        resizeEdgeTiles();
    }

    // Search for the tile position closest to the viewport center that does not yet contain a tile.
    // Which position is considered the closest depends on the tileDistance function.
    double shortestDistance = std::numeric_limits<double>::infinity();
    Vector<Tile::Coordinate> tilesToCreate;
    unsigned requiredTileCount = 0;

    // Cover areas (in tiles) with minimum distance from the visible rect. If the visible rect is
    // not covered already it will be covered first in one go, due to the distance being 0 for tiles
    // inside the visible rect.
    Tile::Coordinate topLeft = tileCoordinateForPoint(coverRect.location());
    Tile::Coordinate bottomRight = tileCoordinateForPoint(innerBottomRight(coverRect));
    for (int yCoordinate = topLeft.y(); yCoordinate <= bottomRight.y(); ++yCoordinate) {
        for (int xCoordinate = topLeft.x(); xCoordinate <= bottomRight.x(); ++xCoordinate) {
            Tile::Coordinate currentCoordinate(xCoordinate, yCoordinate);
            if (m_tiles.contains(currentCoordinate))
                continue;
            ++requiredTileCount;
            double distance = tileDistance(m_visibleRect, currentCoordinate);
            if (distance > shortestDistance)
                continue;
            if (distance < shortestDistance) {
                tilesToCreate.clear();
                shortestDistance = distance;
            }
            tilesToCreate.append(currentCoordinate);
        }
    }

    // Now construct the tile(s) within the shortest distance.
    unsigned tilesToCreateCount = tilesToCreate.size();
    for (unsigned n = 0; n < tilesToCreateCount; ++n) {
        Tile::Coordinate coordinate = tilesToCreate[n];
        m_tiles.add(coordinate, makeUnique<Tile>(*this, coordinate));
    }
    requiredTileCount -= tilesToCreateCount;

    // Re-call createTiles on a timer to cover the visible area with the newest shortest distance.
    m_pendingTileCreation = requiredTileCount;
    if (m_pendingTileCreation)
        m_client.tiledBackingStoreHasPendingTileCreation();
}

void TiledBackingStore::adjustForContentsRect(IntRect& rect) const
{
    IntRect bounds = m_rect;
    IntSize candidateSize = rect.size();

    rect.intersect(bounds);

    if (rect.size() == candidateSize)
        return;

    /*
     * In the following case, there is no intersection of the contents rect and the cover rect.
     * Thus the latter should not be inflated.
     *
     *  +---------------+
     *  |   m_rect      |
     *  +---------------+
     *
     *          +-------------------------------+
     *          |          cover rect           |
     *          |         +---------+           |
     *          |         | visible |           |
     *          |         |  rect   |           |
     *          |         +---------+           |
     *          +-------------------------------+
     */
    if (rect.isEmpty())
        return;

    // Try to create a cover rect of the same size as the candidate, but within content bounds.
    int pixelsCovered = 0;
    if (!WTF::safeMultiply(candidateSize.width(), candidateSize.height(), pixelsCovered))
        pixelsCovered = std::numeric_limits<int>::max();

    if (rect.width() < candidateSize.width())
        rect.inflateY(((pixelsCovered / rect.width()) - rect.height()) / 2);
    if (rect.height() < candidateSize.height())
        rect.inflateX(((pixelsCovered / rect.height()) - rect.width()) / 2);

    rect.intersect(bounds);
}

void TiledBackingStore::computeCoverAndKeepRect(const IntRect& visibleRect, IntRect& coverRect, IntRect& keepRect) const
{
    coverRect = visibleRect;
    keepRect = visibleRect;

    // If we cover more that the actual viewport we can be smart about which tiles we choose to render.
    if (m_coverAreaMultiplier > 1) {
        // The initial cover area covers equally in each direction, according to the coverAreaMultiplier.
        coverRect.inflateX(visibleRect.width() * (m_coverAreaMultiplier - 1) / 2);
        coverRect.inflateY(visibleRect.height() * (m_coverAreaMultiplier - 1) / 2);
        keepRect = coverRect;

        if (m_trajectoryVector != FloatPoint::zero()) {
            // A null trajectory vector (no motion) means that tiles for the coverArea will be created.
            // A non-null trajectory vector will shrink the covered rect to visibleRect plus its expansion from its
            // center toward the cover area edges in the direction of the given vector.

            // E.g. if visibleRect == (10,10)5x5 and coverAreaMultiplier == 3.0:
            // a (0,0) trajectory vector will create tiles intersecting (5,5)15x15,
            // a (1,0) trajectory vector will create tiles intersecting (10,10)10x5,
            // and a (1,1) trajectory vector will create tiles intersecting (10,10)10x10.

            // Multiply the vector by the distance to the edge of the cover area.
            float trajectoryVectorMultiplier = (m_coverAreaMultiplier - 1) / 2;

            // Unite the visible rect with a "ghost" of the visible rect moved in the direction of the trajectory vector.
            coverRect = visibleRect;
            coverRect.move(coverRect.width() * m_trajectoryVector.x() * trajectoryVectorMultiplier, coverRect.height() * m_trajectoryVector.y() * trajectoryVectorMultiplier);

            coverRect.unite(visibleRect);
        }
        ASSERT(keepRect.contains(coverRect));
    }

    adjustForContentsRect(coverRect);

    // The keep rect is an inflated version of the cover rect, inflated in tile dimensions.
    keepRect.unite(coverRect);
    keepRect.inflateX(m_tileSize.width() / 2);
    keepRect.inflateY(m_tileSize.height() / 2);
    keepRect.intersect(m_rect);

    ASSERT(coverRect.isEmpty() || keepRect.contains(coverRect));
}

void TiledBackingStore::resizeEdgeTiles()
{
    Vector<Tile::Coordinate> tilesToRemove;
    for (auto& tile : m_tiles.values()) {
        Tile::Coordinate tileCoordinate = tile->coordinate();
        IntRect tileRect = tile->rect();
        IntRect expectedTileRect = tileRectForCoordinate(tileCoordinate);
        if (expectedTileRect.isEmpty())
            tilesToRemove.append(tileCoordinate);
        else if (expectedTileRect != tileRect)
            tile->resize(expectedTileRect.size());
    }

    for (auto& coordinateToRemove : tilesToRemove)
        m_tiles.remove(coordinateToRemove);
}

void TiledBackingStore::setKeepRect(const IntRect& keepRect)
{
    // Drop tiles outside the new keepRect.

    FloatRect keepRectF = keepRect;

    Vector<Tile::Coordinate> toRemove;
    for (auto& tile : m_tiles.values()) {
        Tile::Coordinate coordinate = tile->coordinate();
        FloatRect tileRect = tile->rect();
        if (!tileRect.intersects(keepRectF))
            toRemove.append(coordinate);
    }

    for (auto& coordinateToRemove : toRemove)
        m_tiles.remove(coordinateToRemove);

    m_keepRect = keepRect;
}

void TiledBackingStore::removeAllNonVisibleTiles(const IntRect& unscaledVisibleRect, const IntRect& contentsRect)
{
    IntRect boundedVisibleRect = mapFromContents(intersection(unscaledVisibleRect, contentsRect));
    setKeepRect(boundedVisibleRect);
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

IntRect TiledBackingStore::tileRectForCoordinate(const Tile::Coordinate& coordinate) const
{
    IntRect rect(coordinate.x() * m_tileSize.width(),
        coordinate.y() * m_tileSize.height(),
        m_tileSize.width(),
        m_tileSize.height());

    rect.intersect(m_rect);
    return rect;
}

Tile::Coordinate TiledBackingStore::tileCoordinateForPoint(const IntPoint& point) const
{
    int x = point.x() / m_tileSize.width();
    int y = point.y() / m_tileSize.height();
    return Tile::Coordinate(std::max(x, 0), std::max(y, 0));
}

}

#endif
