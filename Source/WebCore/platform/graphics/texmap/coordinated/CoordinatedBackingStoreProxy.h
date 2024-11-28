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

namespace WebCore {
class CoordinatedGraphicsLayer;
class CoordinatedTileBuffer;

class CoordinatedBackingStoreProxy final : public ThreadSafeRefCounted<CoordinatedBackingStoreProxy> {
    WTF_MAKE_TZONE_ALLOCATED(CoordinatedBackingStoreProxy);
public:
    static Ref<CoordinatedBackingStoreProxy> create(float contentsScale, std::optional<IntSize> tileSize = std::nullopt);
    ~CoordinatedBackingStoreProxy();

    bool setContentsScale(float);
    const IntRect& coverRect() const { return m_coverRect; }

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

        float scale() const { return m_scale; }
        const Vector<uint32_t>& tilesToCreate() const { return m_tilesToCreate; }
        const Vector<TileUpdate>& tilesToUpdate() const { return m_tilesToUpdate; }
        const Vector<uint32_t>& tilesToRemove() const { return m_tilesToRemove; }

        void appendUpdate(float, Vector<uint32_t>&&, Vector<TileUpdate>&&, Vector<uint32_t>&&);

    private:
        float m_scale { 1 };
        Vector<uint32_t> m_tilesToCreate;
        Vector<TileUpdate> m_tilesToUpdate;
        Vector<uint32_t> m_tilesToRemove;
    };

    enum class UpdateResult : uint8_t {
        BuffersChanged = 1 << 0,
        TilesPending = 1 << 1,
        TilesChanged = 1 << 2
    };
    OptionSet<UpdateResult> updateIfNeeded(const IntRect& unscaledVisibleRect, const IntRect& unscaledContentsRect, bool shouldCreateAndDestroyTiles, const Vector<IntRect, 1>&, CoordinatedGraphicsLayer&);
    Update takePendingUpdate();

private:
    struct Tile {
        Tile() = default;
        Tile(uint32_t id, const IntPoint& position, IntRect&& tileRect)
            : id(id)
            , position(position)
            , rect(WTFMove(tileRect))
            , dirtyRect(rect)
        {
        }
        Tile(const Tile&) = delete;
        Tile& operator=(const Tile&) = delete;
        Tile(Tile&&) = default;
        Tile& operator=(Tile&&) = default;

        void resize(const IntSize& size)
        {
            rect.setSize(size);
            dirtyRect = rect;
        }

        void addDirtyRect(const IntRect& dirty)
        {
            auto tileDirtyRect = intersection(dirty, rect);
            dirtyRect.unite(tileDirtyRect);
        }

        bool isDirty() const
        {
            return !dirtyRect.isEmpty();
        }

        void markClean()
        {
            dirtyRect = { };
        }

        uint32_t id { 0 };
        IntPoint position;
        IntRect rect;
        IntRect dirtyRect;
    };

    CoordinatedBackingStoreProxy(float contentsScale, const IntSize& tileSize);

    void invalidateRegion(const Vector<IntRect, 1>&);
    void createOrDestroyTiles(const IntRect& visibleRect, const IntRect& scaledContentsRect, float coverAreaMultiplier, Vector<uint32_t>& tilesToCreate, Vector<uint32_t>& tilesToRemove);
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
    UncheckedKeyHashMap<IntPoint, Tile> m_tiles;
    struct {
        Lock lock;
        Update pending WTF_GUARDED_BY_LOCK(lock);
    } m_update;
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
