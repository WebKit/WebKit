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

#include "IntPointHash.h"
#include "IntRect.h"
#include "TiledBacking.h"
#include "Timer.h"
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS CALayer;
OBJC_CLASS WebTileCacheLayer;
OBJC_CLASS WebTileLayer;

namespace WebCore {

class FloatRect;
class IntPoint;
class IntRect;

class TileCache : public TiledBacking {
    WTF_MAKE_NONCOPYABLE(TileCache);

public:
    static PassOwnPtr<TileCache> create(WebTileCacheLayer*);
    ~TileCache();

    void tileCacheLayerBoundsChanged();

    void setNeedsDisplay();
    void setNeedsDisplayInRect(const IntRect&);
    void drawLayer(WebTileLayer *, CGContextRef);

    void setScale(CGFloat);

    bool acceleratesDrawing() const { return m_acceleratesDrawing; }
    void setAcceleratesDrawing(bool);

    CALayer *tileContainerLayer() const { return m_tileContainerLayer.get(); }

    void setTileDebugBorderWidth(float);
    void setTileDebugBorderColor(CGColorRef);

private:
    TileCache(WebTileCacheLayer*);

    // TiledBacking member functions.
    virtual void visibleRectChanged(const IntRect&) OVERRIDE;
    virtual void setIsInWindow(bool) OVERRIDE;
    virtual void setTileCoverage(TileCoverage) OVERRIDE;
    virtual TileCoverage tileCoverage() const OVERRIDE { return m_tileCoverage; }

    IntRect bounds() const;

    typedef IntPoint TileIndex;
    IntRect rectForTileIndex(const TileIndex&) const;
    void getTileIndexRangeForRect(const IntRect&, TileIndex& topLeft, TileIndex& bottomRight);

    IntRect tileCoverageRect() const;
    IntSize tileSizeForCoverageRect(const IntRect&) const;

    void scheduleTileRevalidation(double interval);
    void tileRevalidationTimerFired(Timer<TileCache>*);
    void revalidateTiles();

    WebTileLayer* tileLayerAtIndex(const TileIndex&) const;
    RetainPtr<WebTileLayer> createTileLayer(const IntRect&);

    bool shouldShowRepaintCounters() const;
    void drawRepaintCounter(WebTileLayer *, CGContextRef);

    WebTileCacheLayer* m_tileCacheLayer;
    RetainPtr<CALayer> m_tileContainerLayer;
    IntSize m_tileSize;
    IntRect m_visibleRect;

    typedef HashMap<TileIndex, RetainPtr<WebTileLayer> > TileMap;
    TileMap m_tiles;
    Timer<TileCache> m_tileRevalidationTimer;
    IntRect m_tileCoverageRect;

    CGFloat m_scale;
    CGFloat m_deviceScaleFactor;

    bool m_isInWindow;
    TileCoverage m_tileCoverage;
    bool m_acceleratesDrawing;

    RetainPtr<CGColorRef> m_tileDebugBorderColor;
    float m_tileDebugBorderWidth;
};

} // namespace WebCore

#endif // TileCache_h
