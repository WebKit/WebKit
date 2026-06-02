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

#pragma once

#if USE(COORDINATED_GRAPHICS)
#include "FloatPoint.h"
#include "IntPoint.h"
#include "IntPointHash.h"
#include "IntRect.h"
#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>

#include <sstream>

namespace WebCore {
class CoordinatedPlatformLayer;
class CoordinatedTileBuffer;
class Damage;
class GraphicsLayer;

static const uint32_t s_minimunUnionDirtyArea = 128 * 128;

class CoordinatedBackingStoreProxy final : public ThreadSafeRefCounted<CoordinatedBackingStoreProxy> {
    WTF_MAKE_TZONE_ALLOCATED(CoordinatedBackingStoreProxy);
public:
    static Ref<CoordinatedBackingStoreProxy> create();
    ~CoordinatedBackingStoreProxy() = default;

    static constexpr int s_defaultCPUTileSize = 256;

    const IntRect& coverRect() const LIFETIME_BOUND { return m_coverRect; }

    class Update {
        WTF_MAKE_NONCOPYABLE(Update);
    public:
        Update() = default;
        Update(Update&&) = default;
        Update& operator=(Update&&) = default;
        ~Update();

        struct TileUpdate {
            uint32_t tileID { 0 };
            IntRect tileRect;
            IntRect dirtyRect;
            Ref<CoordinatedTileBuffer> buffer;
        };

        const Vector<uint32_t>& tilesToCreate() const LIFETIME_BOUND { return m_tilesToCreate; }
        const Vector<TileUpdate>& tilesToUpdate() const LIFETIME_BOUND { return m_tilesToUpdate; }
        const Vector<uint32_t>& tilesToRemove() const LIFETIME_BOUND { return m_tilesToRemove; }

        void appendUpdate(Vector<uint32_t>&&, Vector<TileUpdate>&&, Vector<uint32_t>&&);
        void waitUntilPaintingComplete();

        bool isEmpty() const { return m_tilesToCreate.isEmpty() && m_tilesToUpdate.isEmpty() && m_tilesToRemove.isEmpty(); }

    private:
        Vector<uint32_t> m_tilesToCreate;
        Vector<TileUpdate> m_tilesToUpdate;
        Vector<uint32_t> m_tilesToRemove;
    };

    enum class UpdateResult : uint8_t {
        BuffersChanged = 1 << 0,
        TilesPending = 1 << 1,
        TilesChanged = 1 << 2
    };
    OptionSet<UpdateResult> updateIfNeeded(const IntRect& unscaledVisibleRect, const IntRect& unscaledContentsRect, float contentsScale, bool shouldCreateAndDestroyTiles, const Vector<IntRect, 1>&, Damage&, CoordinatedPlatformLayer&);
    Update takePendingUpdate();

    void waitUntilPaintingComplete();

private:
    struct Tile {
        Tile() = default;
        Tile(uint32_t id, const IntPoint& position, IntRect&& tileRect)
            : id(id)
            , position(position)
            , rect(WTF::move(tileRect))
            , dirtyRects({ rect })
            , minimumDirtyAreaUnionRatio(0.7f)
        {
            char* mimimumCoverageRatioString = getenv("TILE_DIRTY_AREA_UNION_COVERAGE_RATIO");
            if (mimimumCoverageRatioString) {
                float ratio;
                std::stringstream ss;

                ss << mimimumCoverageRatioString;
                if (ss >> ratio) {
                    minimumDirtyAreaUnionRatio = ratio;
                }
            }
        }
        Tile(const Tile&) = delete;
        Tile& operator=(const Tile&) = delete;
        Tile(Tile&&) = default;
        Tile& operator=(Tile&&) = default;

        void resize(const IntSize& size)
        {
            rect.setSize(size);
            dirtyRects = { rect };
        }

        void addDirtyRect(const IntRect& dirty)
        {
            auto tileDirtyRect = intersection(dirty, rect);
            ASSERT(!tileDirtyRect.isEmpty());

            // Do quick best effort to find dirty rect which is closest
            // to incoming rect to avoid unification of huge areas

            int uniteIndex = -1;
            if (minimumDirtyAreaUnionRatio > 0) {
                float highestCoverageAreaRatio = 0;
                int highestCoverageAreaRatioIndex = -1;
                for (size_t idx = 0; idx < dirtyRects.size(); ++idx) {
                    float unitedArea = unionRect(tileDirtyRect, dirtyRects[idx]).area();
                    if (unitedArea <= s_minimunUnionDirtyArea) {
                        uniteIndex = idx;
                        break;
                    } else {
                        float dirtyRectArea1 = tileDirtyRect.area();
                        float dirtyRectArea2 = dirtyRects[idx].area();
                        float interectionArea = intersection(tileDirtyRect, dirtyRects[idx]).area();
                        float coverageAreaRatio = (dirtyRectArea1 + dirtyRectArea2 - interectionArea) / unitedArea;
                        if (coverageAreaRatio >= highestCoverageAreaRatio) {
                            highestCoverageAreaRatio = coverageAreaRatio;
                            highestCoverageAreaRatioIndex = idx;
                        }
                    }
                }

                if (highestCoverageAreaRatio > minimumDirtyAreaUnionRatio) {
                    uniteIndex = highestCoverageAreaRatioIndex;
                }
            } else if (dirtyRects.size() > 0) {
                uniteIndex = 0;
            }

            if (uniteIndex != -1) {
                dirtyRects[uniteIndex].unite(tileDirtyRect);
            } else {
                dirtyRects.append(tileDirtyRect);
            }
        }

        bool isDirty() const
        {
            return !dirtyRects.isEmpty();
        }

        void markClean()
        {
            dirtyRects.clear();
        }

        uint32_t id { 0 };
        IntPoint position;
        IntRect rect;
        Vector<IntRect> dirtyRects;
        float minimumDirtyAreaUnionRatio;
    };

    CoordinatedBackingStoreProxy() = default;

    void invalidateRegion(const Vector<IntRect, 1>&);
    void createOrDestroyTiles(const IntRect& unscaledVisibleRect, const IntRect& unscaledContentsRect, const IntSize& unscaledViewportSize, float contentsScale, int maxTextureSize, Damage&, Vector<uint32_t>& tilesToCreate, Vector<uint32_t>& tilesToRemove);
    IntSize computeTileSize(const IntSize& viewportSize, int maxTextureSize) const;
    std::pair<IntRect, IntRect> computeCoverAndKeepRect() const;
    void adjustForContentsRect(IntRect&) const;

    IntRect mapToContents(const IntRect&) const;
    IntRect mapFromContents(const IntRect&) const;
    IntRect tileRectForPosition(const IntPoint&) const;
    IntPoint tilePositionForPoint(const IntPoint&) const;
    void forEachTilePositionInRect(const IntRect&, Function<void(IntPoint&&)>&&);

    float m_contentsScale { 1 };
    IntSize m_tileSize;
    float m_coverAreaMultiplier { 2 };
    bool m_pendingTileCreation { false };
    IntRect m_contentsRect;
    IntRect m_visibleRect;
    IntRect m_coverRect;
    IntRect m_keepRect;
    HashMap<IntPoint, Tile> m_tiles;
    struct {
        Lock lock;
        Update pending WTF_GUARDED_BY_LOCK(lock);
    } m_update;
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
