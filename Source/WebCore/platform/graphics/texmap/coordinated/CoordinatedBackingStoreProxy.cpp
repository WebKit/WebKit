/*
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
#include <wtf/CheckedArithmetic.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/SystemTracing.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CoordinatedBackingStoreProxy);

static constexpr int s_defaultTileDimension = 512;

static uint32_t generateTileID()
{
    static uint32_t id = 0;
    // We may get a zero ID due to wrap-around on overflow.
    if (++id)
        return id;
    return ++id;
}

CoordinatedBackingStoreProxy::Update::~Update() = default;

void CoordinatedBackingStoreProxy::Update::appendUpdate(float scale, Vector<uint32_t>&& tilesToCreate, Vector<TileUpdate>&& tilesToUpdate, Vector<uint32_t>&& tilesToRemove)
{
    m_scale = scale;

    // Remove any creations or updates previously registered for tiles that are going to be removed now.
    if (!m_tilesToCreate.isEmpty() || !m_tilesToUpdate.isEmpty()) {
        for (const auto& tileID : tilesToRemove) {
            m_tilesToCreate.removeAll(tileID);
            m_tilesToUpdate.removeAllMatching([tileID](auto& update) {
                return update.tileID == tileID;
            });
        }
    }

    if (m_tilesToCreate.isEmpty())
        m_tilesToCreate = WTFMove(tilesToCreate);
    else
        m_tilesToCreate.appendVector(WTFMove(tilesToCreate));

    if (m_tilesToUpdate.isEmpty())
        m_tilesToUpdate = WTFMove(tilesToUpdate);
    else
        m_tilesToUpdate.appendVector(WTFMove(tilesToUpdate));

    if (m_tilesToRemove.isEmpty())
        m_tilesToRemove = WTFMove(tilesToRemove);
    else
        m_tilesToRemove.appendVector(WTFMove(tilesToRemove));
}

Ref<CoordinatedBackingStoreProxy> CoordinatedBackingStoreProxy::create(float contentsScale, std::optional<IntSize> tileSize)
{
    return adoptRef(*new CoordinatedBackingStoreProxy(contentsScale, tileSize.value_or(IntSize { s_defaultTileDimension, s_defaultTileDimension })));
}

CoordinatedBackingStoreProxy::CoordinatedBackingStoreProxy(float contentsScale, const IntSize& tileSize)
    : m_contentsScale(contentsScale)
    , m_tileSize(tileSize)
{
}

CoordinatedBackingStoreProxy::~CoordinatedBackingStoreProxy() = default;

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

OptionSet<CoordinatedBackingStoreProxy::UpdateResult> CoordinatedBackingStoreProxy::updateIfNeeded(const IntRect& unscaledVisibleRect, const IntRect& unscaledContentsRect, bool shouldCreateAndDestroyTiles, const Vector<IntRect, 1>& dirtyRegion, CoordinatedGraphicsLayer& layer)
{
    invalidateRegion(dirtyRegion);

    Vector<uint32_t> tilesToCreate;
    Vector<uint32_t> tilesToRemove;
    if (shouldCreateAndDestroyTiles) {
        IntRect contentsRect = mapFromContents(unscaledContentsRect);
        IntRect visibleRect = mapFromContents(unscaledVisibleRect);
        float coverAreaMultiplier = MemoryPressureHandler::singleton().isUnderMemoryPressure() ? 1.0f : 2.0f;
        createOrDestroyTiles(visibleRect, contentsRect, coverAreaMultiplier, tilesToCreate, tilesToRemove);
    }

    OptionSet<UpdateResult> result;
    if (m_pendingTileCreation)
        result.add(UpdateResult::TilesPending);

    // Update the dirty tiles.
    unsigned dirtyTilesCount = 0;
    for (const auto& tile : m_tiles.values()) {
        if (tile.isDirty())
            dirtyTilesCount++;
    }

    WTFBeginSignpost(this, UpdateTiles, "dirty tiles: %u", dirtyTilesCount);

    Vector<Update::TileUpdate> tilesToUpdate;
    unsigned dirtyTileIndex = 0;
    for (auto& tile : m_tiles.values()) {
        if (!tile.isDirty())
            continue;

        WTFBeginSignpost(this, UpdateTile, "%u/%u, id: %d, rect: %ix%i+%i+%i, dirty: %ix%i+%i+%i", ++dirtyTileIndex, dirtyTilesCount, tile.id,
            tile.rect.x(), tile.rect.y(), tile.rect.width(), tile.rect.height(), tile.dirtyRect.x(), tile.dirtyRect.y(), tile.dirtyRect.width(), tile.dirtyRect.height());

        auto buffer = layer.paintTile(tile.dirtyRect);
        IntRect updateRect(tile.dirtyRect);
        updateRect.move(-tile.rect.x(), -tile.rect.y());
        tilesToUpdate.append({ tile.id, tile.rect, WTFMove(updateRect), WTFMove(buffer) });
        tile.markClean();
        result.add(UpdateResult::BuffersChanged);

        WTFEndSignpost(this, UpdateTile);
    }

    WTFEndSignpost(this, UpdateTiles);

    if (tilesToCreate.isEmpty() && tilesToUpdate.isEmpty() && tilesToRemove.isEmpty())
        return result;

    result.add(UpdateResult::TilesChanged);
    {
        Locker locker { m_update.lock };
        m_update.pending.appendUpdate(m_contentsScale, WTFMove(tilesToCreate), WTFMove(tilesToUpdate), WTFMove(tilesToRemove));
    }
    return result;
}

void CoordinatedBackingStoreProxy::invalidateRegion(const Vector<IntRect, 1>& dirtyRegion)
{
    for (const auto& contentsDirtyRect : dirtyRegion) {
        IntRect dirtyRect(mapFromContents(contentsDirtyRect));
        IntRect keepRectFitToTileSize = tileRectForPosition(tilePositionForPoint(m_keepRect.minXMinYCorner()));
        keepRectFitToTileSize.unite(tileRectForPosition(tilePositionForPoint(m_keepRect.maxXMaxYCorner() - IntSize(1, 1))));

        // Only iterate on the part of the rect that we know we might have tiles.
        IntRect coveredDirtyRect = intersection(dirtyRect, keepRectFitToTileSize);
        forEachTilePositionInRect(coveredDirtyRect, [&](IntPoint&& position) {
            auto it = m_tiles.find(position);
            if (it == m_tiles.end())
                return;

            // Pass the full rect to each tile as coveredDirtyRect might not
            // contain them completely and we don't want partial tile redraws.
            it->value.addDirtyRect(dirtyRect);
        });
    }
}

void CoordinatedBackingStoreProxy::createOrDestroyTiles(const IntRect& visibleRect, const IntRect& contentsRect, float coverAreaMultiplier, Vector<uint32_t>& tilesToCreate, Vector<uint32_t>& tilesToRemove)
{
    bool contentsRectChanged = m_contentsRect != contentsRect;
    bool geometryChanged = contentsRectChanged || m_visibleRect != visibleRect || m_coverAreaMultiplier != coverAreaMultiplier;
    if (!geometryChanged && !m_pendingTileCreation)
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

            for (const auto& tile : m_tiles.values())
                tilesToRemove.append(tile.id);
            m_tiles.clear();
            return;
        }
    }

    /* We must compute cover and keep rects using the visibleRect, instead of the rect intersecting the visibleRect with m_contentsRect,
     * because TBS can be used as a backing store of GraphicsLayer and the visible rect usually does not intersect with m_contentsRect.
     * In the below case, the intersecting rect is an empty.
     *
     *  +----------------+
     *  |                |
     *  | m_contentsRect |
     *  |       +--------|----------------------+
     *  |       | HERE   |  cover or keep       |
     *  +----------------+      rect            |
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

    auto [coverRect, keepRect] = computeCoverAndKeepRect();
    m_coverRect = WTFMove(coverRect);
    m_keepRect = WTFMove(keepRect);

    // Drop tiles outside the new keepRect.
    m_tiles.removeIf([&](auto& iter) {
        auto& tile = iter.value;
        if (!tile.rect.intersects(m_keepRect)) {
            tilesToRemove.append(tile.id);
            return true;
        }
        return false;
    });

    if (m_coverRect.isEmpty())
        return;

    // Resize tiles at the edge in case the contents size has changed, but only do so
    // after having dropped tiles outside the keep rect.
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
    unsigned requiredTileCount = 0;
    forEachTilePositionInRect(m_coverRect, [&](IntPoint&& position) {
        if (m_tiles.contains(position))
            return;

        ++requiredTileCount;
        double distance = tileDistance(position);
        if (distance > shortestDistance)
            return;

        if (distance < shortestDistance) {
            tilesToCreate.clear();
            shortestDistance = distance;
        }
        tilePositionsToCreate.append(position);
    });

    if (requiredTileCount) {
        requiredTileCount -= tilePositionsToCreate.size();

        for (const auto& position : tilePositionsToCreate) {
            auto tile = Tile(generateTileID(), position, tileRectForPosition(position));
            tilesToCreate.append(tile.id);
            m_tiles.add(position, WTFMove(tile));
        }
    }

    // Re-call createTiles on a timer to cover the visible area with the newest shortest distance.
    m_pendingTileCreation = requiredTileCount;
}

void CoordinatedBackingStoreProxy::adjustForContentsRect(IntRect& rect) const
{
    IntRect bounds = m_contentsRect;
    IntSize candidateSize = rect.size();

    rect.intersect(bounds);

    if (rect.size() == candidateSize)
        return;

    /*
     * In the following case, there is no intersection of the contents rect and the cover rect.
     * Thus the latter should not be inflated.
     *
     *  +----------------+
     *  | m_contentsRect |
     *  +----------------+
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

    adjustForContentsRect(coverRect);

    // The keep rect is an inflated version of the cover rect, inflated in tile dimensions.
    keepRect.unite(coverRect);
    keepRect.inflateX(m_tileSize.width() / 2);
    keepRect.inflateY(m_tileSize.height() / 2);
    keepRect.intersect(m_contentsRect);

    ASSERT(coverRect.isEmpty() || keepRect.contains(coverRect));
    return { WTFMove(coverRect), WTFMove(keepRect) };
}

CoordinatedBackingStoreProxy::Update CoordinatedBackingStoreProxy::takePendingUpdate()
{
    Locker locker { m_update.lock };
    return WTFMove(m_update.pending);
}

IntRect CoordinatedBackingStoreProxy::mapToContents(const IntRect& rect) const
{
    return enclosingIntRect(FloatRect(rect.x() / m_contentsScale,
        rect.y() / m_contentsScale,
        rect.width() / m_contentsScale,
        rect.height() / m_contentsScale));
}

IntRect CoordinatedBackingStoreProxy::mapFromContents(const IntRect& rect) const
{
    return enclosingIntRect(FloatRect(rect.x() * m_contentsScale,
        rect.y() * m_contentsScale,
        rect.width() * m_contentsScale,
        rect.height() * m_contentsScale));
}

IntRect CoordinatedBackingStoreProxy::tileRectForPosition(const IntPoint& position) const
{
    IntRect rect(position.x() * m_tileSize.width(),
        position.y() * m_tileSize.height(),
        m_tileSize.width(),
        m_tileSize.height());

    rect.intersect(m_contentsRect);
    return rect;
}

IntPoint CoordinatedBackingStoreProxy::tilePositionForPoint(const IntPoint& point) const
{
    int x = point.x() / m_tileSize.width();
    int y = point.y() / m_tileSize.height();
    return IntPoint(std::max(x, 0), std::max(y, 0));
}

void CoordinatedBackingStoreProxy::forEachTilePositionInRect(const IntRect& rect, Function<void(IntPoint&&)>&& callback)
{
    auto topLeft = tilePositionForPoint(rect.minXMinYCorner());
    auto innerBottomRight = tilePositionForPoint(rect.maxXMaxYCorner() - IntSize(1, 1));
    for (int y = topLeft.y(); y <= innerBottomRight.y(); ++y) {
        for (int x = topLeft.x(); x <= innerBottomRight.x(); ++x)
            callback(IntPoint(x, y));
    }
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
