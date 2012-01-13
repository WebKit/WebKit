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

#ifndef TileCache_h
#define TileCache_h

#include "IntSize.h"
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RetainPtr.h>

#ifdef __OBJC__
@class CALayer;
@class WebTileCacheLayer;
@class WebTileLayer;
#else
class CALayer;
class WebTileCacheLayer;
class WebTileLayer;
#endif

namespace WebCore {

class IntPoint;
class IntRect;

class TileCache {
    WTF_MAKE_NONCOPYABLE(TileCache);

public:
    static PassOwnPtr<TileCache> create(WebTileCacheLayer*, const IntSize& tileSize);

    void tileCacheLayerBoundsChanged();

    void setNeedsDisplay();
    void setNeedsDisplayInRect(const IntRect&);
    void drawLayer(WebTileLayer*, CGContextRef);

    CALayer *tileContainerLayer() const { return m_tileContainerLayer.get(); }

private:
    TileCache(WebTileCacheLayer*, const IntSize& tileSize);

    IntRect bounds() const;
    void getTileRangeForRect(const IntRect&, IntPoint& topLeft, IntPoint& bottomRight);

    IntSize numTilesForGridSize(const IntSize&) const;
    void resizeTileGrid(const IntSize& numTiles);

    WebTileLayer* tileLayerAtPosition(const IntPoint&) const;
    RetainPtr<WebTileLayer> createTileLayer();

    WebTileCacheLayer* m_tileCacheLayer;
    const IntSize m_tileSize;

    RetainPtr<CALayer> m_tileContainerLayer;

    // Number of tiles in each dimension.
    IntSize m_numTilesInGrid;
};

} // namespace WebCore

#endif // TileCache_h
