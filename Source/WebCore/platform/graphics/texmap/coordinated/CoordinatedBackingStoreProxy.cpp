/*
 * Copyright (C) 2024 Igalia S.L.
 * Copyright (C) 2010-2012 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "CoordinatedBackingStoreProxy.h"

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedGraphicsLayer.h"
#include "CoordinatedTileBuffer.h"
#include "FloatRect.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/SystemTracing.h>
#include <wtf/TZoneMallocInlines.h>

static constexpr int s_defaultTileDimension = 512;

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CoordinatedBackingStoreProxy);

static uint32_t generateTileID()
{
    static uint32_t id = 0;
    // We may get a zero ID due to wrap-around on overflow.
    if (++id)
        return id;
    return ++id;
}

CoordinatedBackingStoreProxy::Update::Update(Vector<uint32_t>&& tilesToCreate, Vector<TileUpdate>&& tilesToUpdate, Vector<uint32_t>&& tilesToRemove)
    : m_tilesToCreate(WTFMove(tilesToCreate))
    , m_tilesToUpdate(WTFMove(tilesToUpdate))
    , m_tilesToRemove(WTFMove(tilesToRemove))
{
}

CoordinatedBackingStoreProxy::Update::~Update() = default;

void CoordinatedBackingStoreProxy::Update::appendUpdate(RefPtr<Update>& other)
{
    if (!other)
        return;

    // Remove any creations or updates previously registered for tiles that are going to be removed now.
    for (const auto& tileID : other->m_tilesToRemove) {
        m_tilesToCreate.removeAll(tileID);
        m_tilesToUpdate.removeAllMatching([tileID](auto& update) {
            return update.tileID == tileID;
        });
    }

    m_tilesToCreate.appendVector(WTFMove(other->m_tilesToCreate));
    m_tilesToUpdate.appendVector(WTFMove(other->m_tilesToUpdate));
    m_tilesToRemove.appendVector(WTFMove(other->m_tilesToRemove));
}

std::unique_ptr<CoordinatedBackingStoreProxy> CoordinatedBackingStoreProxy::create(float contentsScale, std::optional<IntSize> tileSize)
{
    return makeUnique<CoordinatedBackingStoreProxy>(contentsScale, tileSize.value_or(IntSize { s_defaultTileDimension, s_defaultTileDimension }));
}

CoordinatedBackingStoreProxy::CoordinatedBackingStoreProxy(float contentsScale, const IntSize& tileSize)
    : m_contentsScale(contentsScale)
    , m_tileSize(tileSize)
{
}

bool CoordinatedBackingStoreProxy::setContentsScale(float contentsScale)
{
    if (m_contentsScale == contentsScale)
        return false;

    m_contentsScale = contentsScale;
    m_coverAreaMultiplier = 2;
    m_pendingTileCreation = false;
    m_contentsRect = { };
    m_visibleRect = { };
    m_coverRect = { };
    m_keepRect = { };

    return true;
}

RefPtr<CoordinatedBackingStoreProxy::Update> CoordinatedBackingStoreProxy::updateIfNeeded(const IntRect& unscaledVisibleRect, const IntRect& unscaledContentsRect, bool shouldCreateAndDestroyTiles, Vector<IntRect>&& dirtyRegion, CoordinatedGraphicsLayer& layer)
{
    invalidateRegion(dirtyRegion);

    Vector<uint32_t> tilesToCreate;
    Vector<TileUpdate> tilesToUpdate;
    Vector<uint32_t> tilesToRemove;

    auto createOrDestroyTiles = [&] {
        auto contentsRect = mapFromContents(unscaledContentsRect);
        auto visibleRect = mapFromContents(unscaledVisibleRect);
        float coverAreaMultiplier = MemoryPressureHandler::singleton().isUnderMemoryPressure() ? 1.0f : 2.0f;

        bool contentsRectChanged = m_contentsRect != contentsRect;
        bool geometryChanged = contentsRectChanged || m_visibleRect != visibleRect || m_coverAreaMultiplier != coverAreaMultiplier;
        if (!m_pendingTileCreation && !geometryChanged)
            return;

        if (geometryChanged) {
            m_contentsRect = contentsRect;
            m_visibleRect = visibleRect;
            m_coverAreaMultiplier = coverAreaMultiplier;
            if (m_contentsRect.isEmpty()) {
                m_coverRect = { };
                m_keepRect = { };
                if (m_tiles.isEmpty())
                    return;

                auto tiles = WTFMove(m_tiles);
                for (const auto& tile : tiles.values())
                    tilesToRemove.append(tile.id);

                return;
            }
        }

        /* We must compute cover and keep rects using the visibleRect, instead of the rect intersecting the visibleRect with m_rect,
         * because TBS can be used as a backing store of GraphicsLayer and the visible rect usually does not intersect with m_rect.
         * In the below case, the intersecting rect is an empty.
         *
         *  +----------------+
         *  |                |
         *  | m_contentsRect |
         *  |       +--------|---------------------+
         *  |       |  HERE  |  cover or keep      |
         *  +----------------+      rect           |
         *          |         +---------+          |
         *          |         | visible |          |
         *          |         |  rect   |          |
         *          |         +---------+          |
         *          |                              |
         *          |                              |
         *          +------------------------------+
         *
         * We must create or keep the tiles in the HERE region.
         */
        auto [coverRect, keepRect] = computeCoverAndKeepRect();
        m_coverRect = WTFMove(coverRect);
        m_keepRect = WTFMove(keepRect);

        // Drop tiles outside the new keepRect.
        m_tiles.removeIf([&](auto& iter) {
            if (!iter.value.rect.intersects(m_keepRect)) {
                tilesToRemove.append(iter.value.id);
                return true;
            }
            return false;
        });

        if (m_coverRect.isEmpty())
            return;

        // Resize tiles at the edge in case the contents size has changed.
        if (contentsRectChanged) {
            m_tiles.removeIf([&](auto& iter) {
                auto& tile = iter.value;
                auto expectedTileRect = tileRectForPosition(tile.position);
                if (expectedTileRect.isEmpty()) {
                    tilesToRemove.append(tile.id);
                    return true;
                }

                if (expectedTileRect != tile.rect)
                    tile.resize(expectedTileRect.size());

                return false;
            });
        }

        // Search for the tile position closest to the viewport center that does not yet contain a tile.
        // Which position is considered the closest depends on the tileDistance function.
        double shortestDistance = std::numeric_limits<double>::infinity();
        unsigned requiredTileCount = 0;
        IntPoint visibleCenterPosition = tilePositionForPoint(m_visibleRect.center());
        auto tileDistance = [&] (const IntPoint& tilePosition) -> double {
            if (m_visibleRect.intersects(tileRectForPosition(tilePosition)))
                return 0;
            return std::max(std::abs(visibleCenterPosition.y() - tilePosition.y()), std::abs(visibleCenterPosition.x() - tilePosition.x()));
        };

        // Cover areas (in tiles) with minimum distance from the visible rect. If the visible rect is
        // not covered already it will be covered first in one go, due to the distance being 0 for tiles
        // inside the visible rect.
        Vector<IntPoint> tilePositionsToCreate;
        auto topLeft = tilePositionForPoint(m_coverRect.minXMinYCorner());
        auto innerBottomRight = tilePositionForPoint(m_coverRect.maxXMaxYCorner() - IntSize(1, 1));
        for (int y = topLeft.y(); y <= innerBottomRight.y(); ++y) {
            for (int x = topLeft.x(); x <= innerBottomRight.x(); ++x) {
                IntPoint position(x, y);
                if (m_tiles.contains(position))
                    continue;

                requiredTileCount++;
                double distance = tileDistance(position);
                if (distance > shortestDistance)
                    continue;

                if (distance < shortestDistance) {
                    tilePositionsToCreate.clear();
                    shortestDistance = distance;
                }
                tilePositionsToCreate.append(WTFMove(position));
            }
        }

        if (requiredTileCount) {
            requiredTileCount -= tilePositionsToCreate.size();

            for (const auto& position : tilePositionsToCreate) {
                auto tile = Tile(generateTileID(), position, tileRectForPosition(position));
                tilesToCreate.append(tile.id);
                m_tiles.add(position, WTFMove(tile));
            }
        }

        // Re-call updateIfNeeded again to cover the visible area with the newest shortest distance.
        m_pendingTileCreation = requiredTileCount;
    };

    if (shouldCreateAndDestroyTiles)
        createOrDestroyTiles();

    // Update the dirty tiles.
    unsigned dirtyTilesCount = 0;
    for (const auto& tile : m_tiles.values()) {
        if (!tile.dirtyRect.isEmpty())
            dirtyTilesCount++;
    }

    WTFBeginSignpost(this, UpdateTiles, "dirty tiles: %u", dirtyTilesCount);

    unsigned dirtyTileIndex = 0;
    for (auto& tile : m_tiles.values()) {
        if (tile.dirtyRect.isEmpty())
            continue;

        WTFBeginSignpost(this, UpdateTile, "%u/%u, id: %d, rect: %ix%i+%i+%i, dirty: %ix%i+%i+%i", ++dirtyTileIndex, dirtyTilesCount, tile.id,
            tile.rect.x(), tile.rect.y(), tile.rect.width(), tile.rect.height(), tile.dirtyRect.x(), tile.dirtyRect.y(), tile.dirtyRect.width(), tile.dirtyRect.height());

        auto buffer = layer.paintTile(tile.dirtyRect);
        IntRect dirtyRect = WTFMove(tile.dirtyRect);
        dirtyRect.move(-tile.rect.x(), -tile.rect.y());
        tilesToUpdate.append({ tile.id, tile.rect, WTFMove(dirtyRect), WTFMove(buffer) });

        WTFEndSignpost(this, UpdateTileBackingStore);
    }

    WTFEndSignpost(this, UpdateTiles);

    return Update::create(WTFMove(tilesToCreate), WTFMove(tilesToUpdate), WTFMove(tilesToRemove));
}

std::pair<IntRect, IntRect> CoordinatedBackingStoreProxy::computeCoverAndKeepRect() const
{
    IntRect coverRect = m_visibleRect;
    IntRect keepRect = m_visibleRect;

    // If we cover more that the actual viewport we can be smart about which tiles we choose to render.
    if (m_coverAreaMultiplier > 1) {
        // The initial cover area covers equally in each direction, according to the coverAreaMultiplier.
        coverRect.inflateX(m_visibleRect.width() * (m_coverAreaMultiplier - 1) / 2);
        coverRect.inflateY(m_visibleRect.height() * (m_coverAreaMultiplier - 1) / 2);
        keepRect = coverRect;
        ASSERT(keepRect.contains(coverRect));
    }

    // Adjust the cover rect for contents rect.
    IntSize candidateSize = coverRect.size();
    coverRect.intersect(m_contentsRect);
    if (!coverRect.isEmpty() && coverRect.size() != candidateSize) {
        // Try to create a cover rect of the same size as the candidate, but within content bounds.
        int pixelsCovered = 0;
        if (!WTF::safeMultiply(candidateSize.width(), candidateSize.height(), pixelsCovered))
            pixelsCovered = std::numeric_limits<int>::max();

        if (coverRect.width() < candidateSize.width())
            coverRect.inflateY(((pixelsCovered / coverRect.width()) - coverRect.height()) / 2);
        if (coverRect.height() < candidateSize.height())
            coverRect.inflateX(((pixelsCovered / coverRect.height()) - coverRect.width()) / 2);

        coverRect.intersect(m_contentsRect);
    }

    // The keep rect is an inflated version of the cover rect, inflated in tile dimensions.
    keepRect.unite(coverRect);
    keepRect.inflateX(m_tileSize.width() / 2);
    keepRect.inflateY(m_tileSize.height() / 2);
    keepRect.intersect(m_contentsRect);

    ASSERT(coverRect.isEmpty() || keepRect.contains(coverRect));
    return { WTFMove(coverRect), WTFMove(keepRect) };
}

void CoordinatedBackingStoreProxy::invalidateRegion(const Vector<IntRect>& dirtyRegion)
{
    auto invalidate = [&](const IntRect& contentsDirtyRect) {
        IntRect dirtyRect(mapFromContents(contentsDirtyRect));
        IntRect keepRectFitToTileSize = tileRectForPosition(tilePositionForPoint(m_keepRect.minXMinYCorner()));
        keepRectFitToTileSize.unite(tileRectForPosition(tilePositionForPoint(m_keepRect.maxXMaxYCorner() - IntSize(1, 1))));

        // Only iterate on the part of the rect that we know we might have tiles.
        IntRect coveredDirtyRect = intersection(dirtyRect, keepRectFitToTileSize);
        auto topLeft = tilePositionForPoint(coveredDirtyRect.minXMinYCorner());
        auto innerBottomRight = tilePositionForPoint(coveredDirtyRect.maxXMaxYCorner() - IntSize(1, 1));

        for (int y = topLeft.y(); y <= innerBottomRight.y(); ++y) {
            for (int x = topLeft.x(); x <= innerBottomRight.x(); ++x) {
                auto it = m_tiles.find(IntPoint(x, y));
                if (it == m_tiles.end())
                    continue;

                // Pass the full rect to each tile as coveredDirtyRect might not contain them completely and we don't want partial tile redraws.
                it->value.addDirtyRect(dirtyRect);
            }
        }
    };

    for (const auto& rect : dirtyRegion)
        invalidate(rect);
}

IntRect CoordinatedBackingStoreProxy::mapToContents(const IntRect& rect) const
{
    FloatRect scaledRect(rect);
    scaledRect.scale(1 / m_contentsScale);
    return enclosingIntRect(scaledRect);
}

IntRect CoordinatedBackingStoreProxy::mapFromContents(const IntRect& rect) const
{
    FloatRect scaledRect(rect);
    scaledRect.scale(m_contentsScale);
    return enclosingIntRect(scaledRect);
}

IntRect CoordinatedBackingStoreProxy::tileRectForPosition(const IntPoint& position) const
{
    IntRect tileRect({ position.x() * m_tileSize.width(), position.y() * m_tileSize.height() }, m_tileSize);
    tileRect.intersect(m_contentsRect);
    return tileRect;
}

IntPoint CoordinatedBackingStoreProxy::tilePositionForPoint(const IntPoint& point) const
{
    int x = point.x() / m_tileSize.width();
    int y = point.y() / m_tileSize.height();
    return { std::max(x, 0), std::max(y, 0) };
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
