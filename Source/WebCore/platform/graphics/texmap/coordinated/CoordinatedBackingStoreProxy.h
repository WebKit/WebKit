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

#pragma once

#if USE(COORDINATED_GRAPHICS)
#include "IntPointHash.h"
#include "IntRect.h"
#include <optional>
#include <wtf/Function.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {
class CoordinatedGraphicsLayer;
class CoordinatedTileBuffer;

class CoordinatedBackingStoreProxy {
    WTF_MAKE_TZONE_ALLOCATED(CoordinatedBackingStoreProxy);
    WTF_MAKE_NONCOPYABLE(CoordinatedBackingStoreProxy);
public:
    static std::unique_ptr<CoordinatedBackingStoreProxy> create(float contentsScale, std::optional<IntSize> tileSize = std::nullopt);
    CoordinatedBackingStoreProxy(float contentsScale, const IntSize& tileSize);
    ~CoordinatedBackingStoreProxy() = default;

    bool setContentsScale(float);
    const IntRect& coverRect() const { return m_coverRect; }
    bool hasPendingTileCreation() const { return m_pendingTileCreation; }

    struct TileUpdate {
        uint32_t tileID { 0 };
        IntRect tileRect;
        IntRect dirtyRect;
        Ref<CoordinatedTileBuffer> buffer;
    };

    class Update final : public ThreadSafeRefCounted<Update> {
    public:
        static RefPtr<Update> create(Vector<uint32_t>&& tilesToCreate, Vector<TileUpdate>&& tilesToUpdate, Vector<uint32_t>&& tilesToRemove)
        {
            if (tilesToCreate.isEmpty() && tilesToUpdate.isEmpty() && tilesToRemove.isEmpty())
                return nullptr;
            return adoptRef(new Update(WTFMove(tilesToCreate), WTFMove(tilesToUpdate), WTFMove(tilesToRemove)));
        }
        ~Update();

        const Vector<uint32_t> tilesToCreate() const { return m_tilesToCreate; }
        const Vector<TileUpdate>& tilesToUpdate() const { return m_tilesToUpdate; }
        const Vector<uint32_t> tilesToRemove() const { return m_tilesToRemove; }

        void appendUpdate(RefPtr<Update>&);

    private:
        Update(Vector<uint32_t>&&, Vector<TileUpdate>&&, Vector<uint32_t>&&);

        Vector<uint32_t> m_tilesToCreate;
        Vector<TileUpdate> m_tilesToUpdate;
        Vector<uint32_t> m_tilesToRemove;
    };

    RefPtr<Update> updateIfNeeded(const IntRect& unscaledVisibleRect, const IntRect& unscaledContentsRect, bool shouldCreateAndDestroyTiles, Vector<IntRect>&& dirtyRegion, CoordinatedGraphicsLayer&);

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

        uint32_t id { 0 };
        IntPoint position;
        IntRect rect;
        IntRect dirtyRect;
    };

    std::pair<IntRect, IntRect> computeCoverAndKeepRect() const;
    void invalidateRegion(const Vector<IntRect>&);

    IntRect mapToContents(const IntRect&) const;
    IntRect mapFromContents(const IntRect&) const;
    IntRect tileRectForPosition(const IntPoint&) const;
    IntPoint tilePositionForPoint(const IntPoint&) const;

    float m_contentsScale { 1 };
    IntSize m_tileSize;
    float m_coverAreaMultiplier { 2 };
    bool m_pendingTileCreation { false };
    IntRect m_contentsRect;
    IntRect m_visibleRect;
    IntRect m_coverRect;
    IntRect m_keepRect;
    UncheckedKeyHashMap<IntPoint, Tile> m_tiles;
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
