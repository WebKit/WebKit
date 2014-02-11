/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "TileGrid.h"

#if PLATFORM(IOS)

#include "MemoryPressureHandler.h"
#include "SystemMemory.h"
#include "TileGridTile.h"
#include "TileLayer.h"
#include "TileLayerPool.h"
#include "WAKWindow.h"
#include <CoreGraphics/CoreGraphicsPrivate.h>
#include <QuartzCore/QuartzCore.h>
#include <QuartzCore/QuartzCorePrivate.h>
#include <algorithm>
#include <functional>

namespace WebCore {

TileGrid::TileGrid(TileCache* tileCache, const IntSize& tileSize)
    : m_tileCache(tileCache)
    , m_tileHostLayer(adoptNS([[TileHostLayer alloc] initWithTileGrid:this]))
    , m_tileSize(tileSize)
    , m_scale(1)
    , m_validBounds(0, 0, std::numeric_limits<int>::max(), std::numeric_limits<int>::max()) 
{
}
    
TileGrid::~TileGrid()
{
    [m_tileHostLayer removeFromSuperlayer];
}

IntRect TileGrid::visibleRect() const
{
    IntRect visibleRect = enclosingIntRect(m_tileCache->visibleRectInLayer(m_tileHostLayer.get()));

    // When fast scrolling to the top, move the visible rect there immediately so we have tiles when the scrolling completes.
    if (m_tileCache->tilingMode() == TileCache::ScrollToTop)
        visibleRect.setY(0);

    return visibleRect;
}

void TileGrid::dropAllTiles()
{
    m_tiles.clear();
}

void TileGrid::dropTilesIntersectingRect(const IntRect& dropRect)
{
    dropTilesBetweenRects(dropRect, IntRect());
}

void TileGrid::dropTilesOutsideRect(const IntRect& keepRect)
{
    dropTilesBetweenRects(IntRect(0, 0, std::numeric_limits<int>::max(), std::numeric_limits<int>::max()), keepRect);
}

void TileGrid::dropTilesBetweenRects(const IntRect& dropRect, const IntRect& keepRect)
{
    Vector<TileIndex> toRemove;
    for (const auto& tile : m_tiles) {
        const TileIndex& index = tile.key;
        IntRect tileRect = tile.value->rect();
        if (tileRect.intersects(dropRect) && !tileRect.intersects(keepRect))
            toRemove.append(index);
    }
    unsigned removeCount = toRemove.size();
    for (unsigned n = 0; n < removeCount; ++n)
        m_tiles.remove(toRemove[n]);
}

unsigned TileGrid::tileByteSize() const
{
    IntSize tilePixelSize = m_tileSize;
    tilePixelSize.scale(m_tileCache->screenScale());
    return TileLayerPool::bytesBackingLayerWithPixelSize(tilePixelSize);
}

template <typename T>
static bool isFartherAway(const std::pair<double, T>& a, const std::pair<double, T>& b)
{
    return a.first > b.first;
}

bool TileGrid::dropDistantTiles(unsigned tilesNeeded, double shortestDistance)
{
    unsigned bytesPerTile = tileByteSize();
    unsigned bytesNeeded = tilesNeeded * bytesPerTile;
    unsigned bytesUsed = tileCount() * bytesPerTile;
    unsigned maximumBytes = m_tileCache->tileCapacityForGrid(this);

    int bytesToReclaim = int(bytesUsed) - (int(maximumBytes) - bytesNeeded);
    if (bytesToReclaim <= 0)
        return true;

    unsigned tilesToRemoveCount = bytesToReclaim / bytesPerTile;

    IntRect visibleRect = this->visibleRect();
    Vector<std::pair<double, TileIndex>> toRemove;
    for (const auto& tile : m_tiles) {
        const TileIndex& index = tile.key;
        const IntRect& tileRect = tile.value->rect();
        double distance = tileDistance2(visibleRect, tileRect);
        if (distance <= shortestDistance)
            continue;
        toRemove.append(std::make_pair(distance, index));
        std::push_heap(toRemove.begin(), toRemove.end(), std::ptr_fun(isFartherAway<TileIndex>));
        if (toRemove.size() > tilesToRemoveCount) {
            std::pop_heap(toRemove.begin(), toRemove.end(), std::ptr_fun(isFartherAway<TileIndex>));
            toRemove.removeLast();
        }
    }
    size_t removeCount = toRemove.size();
    for (size_t n = 0; n < removeCount; ++n)
        m_tiles.remove(toRemove[n].second);

    if (!shortestDistance)
        return true;

    return tileCount() * bytesPerTile + bytesNeeded <= maximumBytes;
}

void TileGrid::addTilesCoveringRect(const IntRect& rectToCover)
{
    // We never draw anything outside of our bounds.
    IntRect rect(rectToCover);
    rect.intersect(bounds());
    if (rect.isEmpty())
        return;

    TileIndex topLeftIndex = tileIndexForPoint(topLeft(rect));
    TileIndex bottomRightIndex = tileIndexForPoint(bottomRight(rect));
    for (int yIndex = topLeftIndex.y(); yIndex <= bottomRightIndex.y(); ++yIndex) {
        for (int xIndex = topLeftIndex.x(); xIndex <= bottomRightIndex.x(); ++xIndex) {
            TileIndex index(xIndex, yIndex);
            if (!tileForIndex(index))
                addTileForIndex(index);
        }
    }
}

void TileGrid::addTileForIndex(const TileIndex& index)
{
    m_tiles.set(index, TileGridTile::create(this, tileRectForIndex(index)));
}

CALayer* TileGrid::tileHostLayer() const
{
    return m_tileHostLayer.get();
}

IntRect TileGrid::bounds() const
{
    return IntRect(IntPoint(), IntSize([tileHostLayer() size]));
}

PassRefPtr<TileGridTile> TileGrid::tileForIndex(const TileIndex& index) const
{
    return m_tiles.get(index);
}

IntRect TileGrid::tileRectForIndex(const TileIndex& index) const
{
    IntRect rect(index.x() * m_tileSize.width() - (m_origin.x() ? m_tileSize.width() - m_origin.x() : 0),
                 index.y() * m_tileSize.height() - (m_origin.y() ? m_tileSize.height() - m_origin.y() : 0),
                 m_tileSize.width(),
                 m_tileSize.height());
    rect.intersect(bounds());
    return rect;
}

TileGrid::TileIndex TileGrid::tileIndexForPoint(const IntPoint& point) const
{
    ASSERT(m_origin.x() < m_tileSize.width());
    ASSERT(m_origin.y() < m_tileSize.height());
    int x = (point.x() + (m_origin.x() ? m_tileSize.width() - m_origin.x() : 0)) / m_tileSize.width();
    int y = (point.y() + (m_origin.y() ? m_tileSize.height() - m_origin.y() : 0)) / m_tileSize.height();
    return TileIndex(std::max(x, 0), std::max(y, 0));
}

void TileGrid::centerTileGridOrigin(const IntRect& visibleRect)
{
    if (visibleRect.isEmpty())
        return;

    unsigned minimumHorizontalTiles = 1 + (visibleRect.width() - 1) / m_tileSize.width();
    unsigned minimumVerticalTiles = 1 + (visibleRect.height() - 1) / m_tileSize.height();
    TileIndex currentTopLeftIndex = tileIndexForPoint(topLeft(visibleRect));
    TileIndex currentBottomRightIndex = tileIndexForPoint(bottomRight(visibleRect));
    unsigned currentHorizontalTiles = currentBottomRightIndex.x() - currentTopLeftIndex.x() + 1;
    unsigned currentVerticalTiles = currentBottomRightIndex.y() - currentTopLeftIndex.y() + 1;

    // If we have tiles already, only center if we would get benefits from both directions (as we need to throw out existing tiles).
    if (tileCount() && (currentHorizontalTiles == minimumHorizontalTiles || currentVerticalTiles == minimumVerticalTiles))
        return;

    IntPoint newOrigin(0, 0);
    IntSize size = bounds().size();
    if (size.width() > m_tileSize.width()) {
        newOrigin.setX((visibleRect.x() - (minimumHorizontalTiles * m_tileSize.width() - visibleRect.width()) / 2) % m_tileSize.width());
        if (newOrigin.x() < 0)
            newOrigin.setX(0);
    }
    if (size.height() > m_tileSize.height()) {
        newOrigin.setY((visibleRect.y() - (minimumVerticalTiles * m_tileSize.height() - visibleRect.height()) / 2) % m_tileSize.height());
        if (newOrigin.y() < 0)
            newOrigin.setY(0);
    }

    // Drop all existing tiles if the origin moved.
    if (newOrigin == m_origin)
        return;
    m_tiles.clear();
    m_origin = newOrigin;
}

PassRefPtr<TileGridTile> TileGrid::tileForPoint(const IntPoint& point) const
{
    return tileForIndex(tileIndexForPoint(point));
}

bool TileGrid::tilesCover(const IntRect& rect) const
{
    return tileForPoint(rect.location()) && tileForPoint(IntPoint(rect.maxX() - 1, rect.y())) &&
    tileForPoint(IntPoint(rect.x(), rect.maxY() - 1)) && tileForPoint(IntPoint(rect.maxX() - 1, rect.maxY() - 1));
}

void TileGrid::updateTileOpacity()
{
    TileMap::iterator end = m_tiles.end();
    for (TileMap::iterator it = m_tiles.begin(); it != end; ++it)
        [it->value->tileLayer() setOpaque:m_tileCache->tilesOpaque()];
}

void TileGrid::updateTileBorderVisibility()
{
    TileMap::iterator end = m_tiles.end();
    for (TileMap::iterator it = m_tiles.begin(); it != end; ++it)
        it->value->showBorder(m_tileCache->tileBordersVisible());
}

unsigned TileGrid::tileCount() const
{
    return m_tiles.size();
}

bool TileGrid::checkDoSingleTileLayout()
{
    IntSize size = bounds().size();
    if (size.width() > m_tileSize.width() || size.height() > m_tileSize.height())
        return false;

    if (m_origin != IntPoint(0, 0)) {
        m_tiles.clear();
        m_origin = IntPoint(0, 0);
    }

    dropInvalidTiles();

    if (size.isEmpty()) {
        ASSERT(!m_tiles.get(TileIndex(0, 0)));
        return true;
    }

    TileIndex originIndex(0, 0);
    if (!m_tiles.get(originIndex))
        m_tiles.set(originIndex, TileGridTile::create(this, tileRectForIndex(originIndex)));

    return true;
}

void TileGrid::updateHostLayerSize()
{
    CALayer* hostLayer = m_tileCache->hostLayer();
    CGRect tileHostBounds = [hostLayer convertRect:[hostLayer bounds] toLayer:tileHostLayer()];
    CGSize transformedSize;
    transformedSize.width = CGRound(tileHostBounds.size.width);
    transformedSize.height = CGRound(tileHostBounds.size.height);

    CGRect bounds = [tileHostLayer() bounds];
    if (CGSizeEqualToSize(bounds.size, transformedSize))
        return;
    bounds.size = transformedSize;
    [tileHostLayer() setBounds:bounds];
}

void TileGrid::dropInvalidTiles()
{
    IntRect bounds = this->bounds();
    IntRect dropBounds = intersection(m_validBounds, bounds);
    Vector<TileIndex> toRemove;
    for (const auto& tile : m_tiles) {
        const TileIndex& index = tile.key;
        const IntRect& tileRect = tile.value->rect();
        IntRect expectedTileRect = tileRectForIndex(index);
        if (expectedTileRect != tileRect || !dropBounds.contains(tileRect))
            toRemove.append(index);
    }
    unsigned removeCount = toRemove.size();
    for (unsigned n = 0; n < removeCount; ++n)
        m_tiles.remove(toRemove[n]);

    m_validBounds = bounds;
}

void TileGrid::invalidateTiles(const IntRect& dirtyRect)
{
    if (!hasTiles())
        return;

    IntRect bounds = this->bounds();
    if (intersection(bounds, m_validBounds) != m_validBounds) {
        // The bounds have got smaller. Everything outside will also be considered invalid and will be dropped by dropInvalidTiles().
        // Due to dirtyRect being limited to current bounds the tiles that are temporarily outside might miss invalidation 
        // completely othwerwise.
        m_validBounds = bounds;
    }

    Vector<TileIndex> invalidatedTiles;

    if (dirtyRect.width() > m_tileSize.width() * 4 || dirtyRect.height() > m_tileSize.height() * 4) {
        // For large invalidates, iterate over live tiles.
        TileMap::iterator end = m_tiles.end();
        for (TileMap::iterator it = m_tiles.begin(); it != end; ++it) {
            TileGridTile* tile = it->value.get();
            if (!tile->rect().intersects(dirtyRect))
               continue;
            tile->invalidateRect(dirtyRect);
            invalidatedTiles.append(it->key);
        }
    } else {
        TileIndex topLeftIndex = tileIndexForPoint(topLeft(dirtyRect));
        TileIndex bottomRightIndex = tileIndexForPoint(bottomRight(dirtyRect));
        for (int yIndex = topLeftIndex.y(); yIndex <= bottomRightIndex.y(); ++yIndex) {
            for (int xIndex = topLeftIndex.x(); xIndex <= bottomRightIndex.x(); ++xIndex) {
                TileIndex index(xIndex, yIndex);
                RefPtr<TileGridTile> tile = tileForIndex(index);
                if (!tile)
                    continue;
                if (!tile->rect().intersects(dirtyRect))
                    continue;
                tile->invalidateRect(dirtyRect);
                invalidatedTiles.append(index);
            }
        }
    }
    if (invalidatedTiles.isEmpty())
        return;
    // When using minimal coverage, drop speculative tiles instead of updating them.
    if (!shouldUseMinimalTileCoverage())
        return;
    if (m_tileCache->tilingMode() != TileCache::Minimal && m_tileCache->tilingMode() != TileCache::Normal)
        return;
    IntRect visibleRect = this->visibleRect();
    unsigned count = invalidatedTiles.size();
    for (unsigned i = 0; i < count; ++i) {
        RefPtr<TileGridTile> tile = tileForIndex(invalidatedTiles[i]);
        if (!tile->rect().intersects(visibleRect))
            m_tiles.remove(invalidatedTiles[i]);
    }
}

bool TileGrid::shouldUseMinimalTileCoverage() const
{
    return m_tileCache->tilingMode() == TileCache::Minimal
        || !m_tileCache->isSpeculativeTileCreationEnabled()
        || memoryPressureHandler().hasReceivedMemoryPressure();
}

IntRect TileGrid::adjustCoverRectForPageBounds(const IntRect& rect) const
{
    // Adjust the rect so that it stays within the bounds and keeps the pixel size.
    IntRect bounds = this->bounds();
    IntRect adjustedRect = rect;
    adjustedRect.move(rect.x() < bounds.x() ? bounds.x() - rect.x() : 0,
              rect.y() < bounds.y() ? bounds.y() - rect.y() : 0);
    adjustedRect.move(rect.maxX() > bounds.maxX() ? bounds.maxX() - rect.maxX() : 0,
              rect.maxY() > bounds.maxY() ? bounds.maxY() - rect.maxY() : 0);
    adjustedRect = intersection(bounds, adjustedRect);
    if (adjustedRect == rect || adjustedRect.isEmpty() || shouldUseMinimalTileCoverage())
        return adjustedRect;
    int pixels = adjustedRect.width() * adjustedRect.height();
    if (adjustedRect.width() != rect.width())
        adjustedRect.inflateY((pixels / adjustedRect.width() - adjustedRect.height()) / 2);
    else if (adjustedRect.height() != rect.height())
        adjustedRect.inflateX((pixels / adjustedRect.height() - adjustedRect.width()) / 2);
    return intersection(adjustedRect, bounds);
}

IntRect TileGrid::calculateCoverRect(const IntRect& visibleRect, bool& centerGrid)
{
    // Use minimum coverRect if we are under memory pressure.
    if (shouldUseMinimalTileCoverage()) {
        centerGrid = true;
        return visibleRect;
    }
    IntRect coverRect = visibleRect;
    centerGrid = false;
    coverRect.inflateX(visibleRect.width() / 2);
    coverRect.inflateY(visibleRect.height());
    return adjustCoverRectForPageBounds(coverRect);
}

double TileGrid::tileDistance2(const IntRect& visibleRect, const IntRect& tileRect) const
{
    // The "distance" calculated here is used to pick which tile to cache next. The idea is to create those
    // closest to the current viewport first so the user is more likely to see already rendered content we she
    // scrolls. The calculation is weighted to prefer vertical and downward direction.
    if (visibleRect.intersects(tileRect))
        return 0;
    IntPoint visibleCenter = visibleRect.location() + IntSize(visibleRect.width() / 2, visibleRect.height() / 2);
    IntPoint tileCenter = tileRect.location() + IntSize(tileRect.width() / 2, tileRect.height() / 2);
    
    double horizontalBias = 1.0;
    double leftwardBias = 1.0;
    double rightwardBias = 1.0;

    double verticalBias = 1.0;
    double upwardBias = 1.0;
    double downwardBias = 1.0;

    const double tilingBiasVeryLikely = 0.8;
    const double tilingBiasLikely = 0.9;

    switch (m_tileCache->tilingDirection()) {
    case TileCache::TilingDirectionUp:
        verticalBias = tilingBiasVeryLikely;
        upwardBias = tilingBiasLikely;
        break;
    case TileCache::TilingDirectionDown:
        verticalBias = tilingBiasVeryLikely;
        downwardBias = tilingBiasLikely;
        break;
    case TileCache::TilingDirectionLeft:
        horizontalBias = tilingBiasVeryLikely;
        leftwardBias = tilingBiasLikely;
        break;
    case TileCache::TilingDirectionRight:
        horizontalBias = tilingBiasVeryLikely;
        rightwardBias = tilingBiasLikely;
        break;
    }

    double xScale = horizontalBias * visibleRect.height() / visibleRect.width() * (tileCenter.x() >= visibleCenter.x() ? rightwardBias : leftwardBias);
    double yScale = verticalBias * visibleRect.width() / visibleRect.height() * (tileCenter.y() >= visibleCenter.y() ? downwardBias : upwardBias);

    double xDistance = xScale * (tileCenter.x() - visibleCenter.x());
    double yDistance = yScale * (tileCenter.y() - visibleCenter.y());

    double distance2 = xDistance * xDistance + yDistance * yDistance;
    return distance2;
}

void TileGrid::createTiles(TileCache::SynchronousTileCreationMode creationMode)
{
    IntRect visibleRect = this->visibleRect();
    if (visibleRect.isEmpty())
        return;

    // Drop tiles that are wrong size or outside the frame (because the frame has been resized).
    dropInvalidTiles();

    bool centerGrid;
    IntRect coverRect = calculateCoverRect(visibleRect, centerGrid);

    // If tile size is bigger than the view, centering minimizes the painting needed to cover the screen.
    // This is especially useful after zooming 
    centerGrid = centerGrid || !tileCount();
    if (centerGrid)
        centerTileGridOrigin(visibleRect);

    double shortestDistance = std::numeric_limits<double>::infinity();
    double coveredDistance = 0;
    Vector<TileGrid::TileIndex> tilesToCreate;
    unsigned pendingTileCount = 0;

    TileGrid::TileIndex topLeftIndex = tileIndexForPoint(topLeft(coverRect));
    TileGrid::TileIndex bottomRightIndex = tileIndexForPoint(bottomRight(coverRect));
    for (int yIndex = topLeftIndex.y(); yIndex <= bottomRightIndex.y(); ++yIndex) {
        for (int xIndex = topLeftIndex.x(); xIndex <= bottomRightIndex.x(); ++xIndex) {
            TileGrid::TileIndex index(xIndex, yIndex);
            // Currently visible tiles have distance of 0 and get all created in the same transaction.
            double distance = tileDistance2(visibleRect, tileRectForIndex(index));
            if (distance > coveredDistance)
                coveredDistance = distance;
            if (tileForIndex(index))
                continue;
            ++pendingTileCount;
            if (distance > shortestDistance)
                continue;
            if (distance < shortestDistance) {
                tilesToCreate.clear();
                shortestDistance = distance;
            }
            tilesToCreate.append(index);
        }
    }

    size_t tilesToCreateCount = tilesToCreate.size();

    // Tile creation timer will invoke this function again in CoverSpeculative mode.
    bool candidateTilesAreSpeculative = shortestDistance > 0;
    if (creationMode == TileCache::CoverVisibleOnly && candidateTilesAreSpeculative)
        tilesToCreateCount = 0;

    // Even if we don't create any tiles, we should still drop distant tiles
    // in case coverRect got smaller.
    double keepDistance = std::min(shortestDistance, coveredDistance);
    if (!dropDistantTiles(tilesToCreateCount, keepDistance))
        return;

    ASSERT(pendingTileCount >= tilesToCreateCount);
    if (!pendingTileCount)
        return;

    for (size_t n = 0; n < tilesToCreateCount; ++n)
        addTileForIndex(tilesToCreate[n]);

    bool didCreateTiles = !!tilesToCreateCount;
    bool createMoreTiles = pendingTileCount > tilesToCreateCount;
    m_tileCache->finishedCreatingTiles(didCreateTiles, createMoreTiles);
}

void TileGrid::dumpTiles()
{
    IntRect visibleRect = this->visibleRect();
    NSLog(@"transformed visibleRect = [%6d %6d %6d %6d]", visibleRect.x(), visibleRect.y(), visibleRect.width(), visibleRect.height());
    unsigned i = 0;
    TileMap::iterator end = m_tiles.end();
    for (TileMap::iterator it = m_tiles.begin(); it != end; ++it) {
        TileIndex& index = it->key;
        IntRect tileRect = it->value->rect();
        NSLog(@"#%-3d (%3d %3d) - [%6d %6d %6d %6d]%@", ++i, index.x(), index.y(), tileRect.x(), tileRect.y(), tileRect.width(), tileRect.height(), tileRect.intersects(visibleRect) ? @" *" : @"");
        NSLog(@"     %@", [it->value->tileLayer() contents]);
    }
}

} // namespace WebCore

#endif // PLATFORM(IOS)
