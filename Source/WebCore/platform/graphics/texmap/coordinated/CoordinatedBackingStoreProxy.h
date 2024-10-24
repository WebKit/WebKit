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
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class CoordinatedBackingStoreProxyClient;
class CoordinatedGraphicsLayer;

class CoordinatedBackingStoreProxy {
    WTF_MAKE_TZONE_ALLOCATED(CoordinatedBackingStoreProxy);
    WTF_MAKE_NONCOPYABLE(CoordinatedBackingStoreProxy);
public:
    explicit CoordinatedBackingStoreProxy(CoordinatedBackingStoreProxyClient&, float contentsScale = 1.f);
    ~CoordinatedBackingStoreProxy();

    CoordinatedBackingStoreProxyClient& client() { return m_client; }

    bool setContentsScale(float);
    float contentsScale() const { return m_contentsScale; }

    const IntRect& coverRect() const { return m_coverRect; }

    enum class UpdateResult : uint8_t {
        BuffersChanged = 1 << 0,
        TilesPending =  1 << 1
    };
    OptionSet<UpdateResult> updateIfNeeded(const IntRect& unscaledVisibleRect, const IntRect& unscaledContentsRect, bool shouldCreateAndDestroyTiles, const Vector<IntRect, 1>&, CoordinatedGraphicsLayer&);

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

    void invalidateRegion(const Vector<IntRect, 1>&);
    void createOrDestroyTiles(const IntRect& visibleRect, const IntRect& scaledContentsRect, float coverAreaMultiplier);
    std::pair<IntRect, IntRect> computeCoverAndKeepRect() const;

    void adjustForContentsRect(IntRect&) const;

    IntRect mapToContents(const IntRect&) const;
    IntRect mapFromContents(const IntRect&) const;
    IntRect tileRectForPosition(const IntPoint&) const;
    IntPoint tilePositionForPoint(const IntPoint&) const;

    CoordinatedBackingStoreProxyClient& m_client;

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
