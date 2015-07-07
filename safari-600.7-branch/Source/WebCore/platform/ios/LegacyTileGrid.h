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

#ifndef LegacyTileGrid_h
#define LegacyTileGrid_h

#if PLATFORM(IOS)

#include "IntPoint.h"
#include "IntPointHash.h"
#include "IntRect.h"
#include "IntSize.h"
#include "LegacyTileCache.h"
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RetainPtr.h>

#define LOG_TILING 0

@class CALayer;

namespace WebCore {

class LegacyTileGridTile;

class LegacyTileGrid {
    WTF_MAKE_NONCOPYABLE(LegacyTileGrid);
public:
    typedef IntPoint TileIndex;

    static PassOwnPtr<LegacyTileGrid> create(LegacyTileCache* cache, const IntSize& tileSize) { return adoptPtr(new LegacyTileGrid(cache, tileSize)); }

    ~LegacyTileGrid();

    LegacyTileCache* tileCache() const { return m_tileCache; }

    CALayer *tileHostLayer() const;
    IntRect bounds() const;
    unsigned tileCount() const;

    float scale() const { return m_scale; }
    void setScale(float scale) { m_scale = scale; }

    IntRect visibleRect() const;

    void createTiles(LegacyTileCache::SynchronousTileCreationMode);

    void dropAllTiles();
    void dropInvalidTiles();
    void dropTilesOutsideRect(const IntRect&);
    void dropTilesIntersectingRect(const IntRect&);
    // Drops tiles that intersect dropRect but do not intersect keepRect.
    void dropTilesBetweenRects(const IntRect& dropRect, const IntRect& keepRect);
    bool dropDistantTiles(unsigned tilesNeeded, double shortestDistance);

    void addTilesCoveringRect(const IntRect&);

    bool tilesCover(const IntRect&) const;
    void centerTileGridOrigin(const IntRect& visibleRect);
    void invalidateTiles(const IntRect& dirtyRect);

    void updateTileOpacity();
    void updateTileBorderVisibility();
    void updateHostLayerSize();
    bool checkDoSingleTileLayout();

    bool hasTiles() const { return !m_tiles.isEmpty(); }

    IntRect calculateCoverRect(const IntRect& visibleRect, bool& centerGrid);

    // Logging
    void dumpTiles();

private:
    double tileDistance2(const IntRect& visibleRect, const IntRect& tileRect) const;
    unsigned tileByteSize() const;

    void addTileForIndex(const TileIndex&);

    PassRefPtr<LegacyTileGridTile> tileForIndex(const TileIndex&) const;
    IntRect tileRectForIndex(const TileIndex&) const;
    PassRefPtr<LegacyTileGridTile> tileForPoint(const IntPoint&) const;
    TileIndex tileIndexForPoint(const IntPoint&) const;

    IntRect adjustCoverRectForPageBounds(const IntRect&) const;
    bool shouldUseMinimalTileCoverage() const;

private:        
    LegacyTileGrid(LegacyTileCache*, const IntSize&);

    LegacyTileCache* m_tileCache;
    RetainPtr<CALayer> m_tileHostLayer;

    IntPoint m_origin;
    IntSize m_tileSize;

    float m_scale;

    typedef HashMap<TileIndex, RefPtr<LegacyTileGridTile>> TileMap;
    TileMap m_tiles;

    IntRect m_validBounds;
};

static inline IntPoint topLeft(const IntRect& rect)
{
    return rect.location();
}

static inline IntPoint bottomRight(const IntRect& rect)
{
    return IntPoint(rect.maxX() - 1, rect.maxY() - 1);
}

} // namespace WebCore

#endif // PLATFORM(IOS)
#endif // TileGrid_h
