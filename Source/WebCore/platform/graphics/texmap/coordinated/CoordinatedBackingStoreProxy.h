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
#include "CoordinatedBackingStoreProxyTile.h"
#include "FloatPoint.h"
#include "IntPoint.h"
#include "IntRect.h"
#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class GraphicsContext;
class CoordinatedBackingStoreProxyClient;

class CoordinatedBackingStoreProxy {
    WTF_MAKE_TZONE_ALLOCATED(CoordinatedBackingStoreProxy);
    WTF_MAKE_NONCOPYABLE(CoordinatedBackingStoreProxy);
public:
    explicit CoordinatedBackingStoreProxy(CoordinatedBackingStoreProxyClient&, float contentsScale = 1.f);
    ~CoordinatedBackingStoreProxy();

    CoordinatedBackingStoreProxyClient& client() { return m_client; }

    void createTilesIfNeeded(const IntRect& unscaledVisibleRect, const IntRect& contentsRect);

    float contentsScale() const { return m_contentsScale; }

    Vector<std::reference_wrapper<CoordinatedBackingStoreProxyTile>> dirtyTiles();

    void invalidate(const IntRect& dirtyRect);

    const IntRect& coverRect() const { return m_coverRect; }

private:
    void createTiles(const IntRect& visibleRect, const IntRect& scaledContentsRect, float coverAreaMultiplier);
    void computeCoverAndKeepRect(const IntRect& visibleRect, IntRect& coverRect, IntRect& keepRect) const;

    void resizeEdgeTiles();
    void setCoverRect(const IntRect& rect) { m_coverRect = rect; }
    void setKeepRect(const IntRect&);

    void adjustForContentsRect(IntRect&) const;
    double tileDistance(const IntRect& viewport, const IntPoint&) const;

    IntRect mapToContents(const IntRect&) const;
    IntRect mapFromContents(const IntRect&) const;
    IntRect tileRectForPosition(const IntPoint&) const;
    IntPoint tilePositionForPoint(const IntPoint&) const;

    CoordinatedBackingStoreProxyClient& m_client;

    UncheckedKeyHashMap<IntPoint, std::unique_ptr<CoordinatedBackingStoreProxyTile>> m_tiles;

    float m_contentsScale { 1 };
    IntSize m_tileSize;
    float m_coverAreaMultiplier { 2 };
    bool m_pendingTileCreation { false };
    IntRect m_contentsRect;
    IntRect m_visibleRect;
    IntRect m_coverRect;
    IntRect m_keepRect;
    IntRect m_previousContentsRect;

    friend class CoordinatedBackingStoreProxyTile;
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
