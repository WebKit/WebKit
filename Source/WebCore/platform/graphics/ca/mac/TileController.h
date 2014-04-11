/*
 * Copyright (C) 2011-2014 Apple Inc. All rights reserved.
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

#ifndef TileController_h
#define TileController_h

#include "FloatRect.h"
#include "IntRect.h"
#include "PlatformCALayer.h"
#include "PlatformCALayerClient.h"
#include "TiledBacking.h"
#include "Timer.h"
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RetainPtr.h>

namespace WebCore {

class FloatRect;
class IntPoint;
class IntRect;
class TileCoverageMap;
class TileGrid;

typedef Vector<RetainPtr<PlatformLayer>> PlatformLayerList;

class TileController final : public TiledBacking {
    WTF_MAKE_NONCOPYABLE(TileController); WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<TileController> create(PlatformCALayer*);
    ~TileController();

    void tileCacheLayerBoundsChanged();

    void setNeedsDisplay();
    void setNeedsDisplayInRect(const IntRect&);

    void setContentsScale(float);
    float contentsScale() const { return m_contentsScale; }

    bool acceleratesDrawing() const { return m_acceleratesDrawing; }
    void setAcceleratesDrawing(bool);

    void setTilesOpaque(bool);
    bool tilesAreOpaque() const { return m_tilesAreOpaque; }

    PlatformCALayer& rootLayer() { return *m_tileCacheLayer; }
    const PlatformCALayer& rootLayer() const { return *m_tileCacheLayer; }

    void setTileDebugBorderWidth(float);
    void setTileDebugBorderColor(Color);

    virtual FloatRect visibleRect() const override { return m_visibleRect; }

    unsigned blankPixelCount() const;
    static unsigned blankPixelCountForTiles(const PlatformLayerList&, const FloatRect&, const IntPoint&);

#if PLATFORM(IOS)
    unsigned numberOfUnparentedTiles() const;
    void removeUnparentedTilesNow();
#endif

public:
    // Public for TileGrid and TileCoverageMap
    bool isInWindow() const { return m_isInWindow; }

    float deviceScaleFactor() const { return m_deviceScaleFactor; }

    Color tileDebugBorderColor() const { return m_tileDebugBorderColor; }
    float tileDebugBorderWidth() const { return m_tileDebugBorderWidth; }
    ScrollingModeIndication indicatorMode() const { return m_indicatorMode; }

    virtual IntSize tileSize() const override { return m_tileSize; }
    virtual IntRect bounds() const override;
    virtual bool hasMargins() const override;
    virtual int topMarginHeight() const override;
    virtual int bottomMarginHeight() const override;
    virtual int leftMarginWidth() const override;
    virtual int rightMarginWidth() const override;
    virtual TileCoverage tileCoverage() const override { return m_tileCoverage; }
    virtual bool unparentsOffscreenTiles() const override { return m_unparentsOffscreenTiles; }
    virtual bool scrollingPerformanceLoggingEnabled() const override { return m_scrollingPerformanceLoggingEnabled; }

    IntRect boundsWithoutMargin() const;

    FloatRect computeTileCoverageRect(const FloatRect& previousVisibleRect, const FloatRect& currentVisibleRect) const;

    IntRect boundsAtLastRevalidate() const { return m_boundsAtLastRevalidate; }
    IntRect boundsAtLastRevalidateWithoutMargin() const;
    FloatRect visibleRectAtLastRevalidate() const { return m_visibleRectAtLastRevalidate; }
    void didRevalidateTiles();

    bool shouldAggressivelyRetainTiles() const;
    bool shouldTemporarilyRetainTileCohorts() const;

    void updateTileCoverageMap();

    RefPtr<PlatformCALayer> createTileLayer(const IntRect&, TileGrid&);

    const TileGrid& tileGrid() const { return *m_tileGrid; }

    Vector<RefPtr<PlatformCALayer>> containerLayers();

private:
    TileController(PlatformCALayer*);

    TileGrid& tileGrid() { return *m_tileGrid; }

    // TiledBacking member functions.
    virtual void setVisibleRect(const FloatRect&) override;
    virtual bool tilesWouldChangeForVisibleRect(const FloatRect&) const override;
    virtual void setTiledScrollingIndicatorPosition(const FloatPoint&) override;
    virtual void prepopulateRect(const FloatRect&) override;
    virtual void setIsInWindow(bool) override;
    virtual void setTileCoverage(TileCoverage) override;
    virtual void revalidateTiles() override;
    virtual void forceRepaint() override;
    virtual IntRect tileGridExtent() const override;
    virtual void setScrollingPerformanceLoggingEnabled(bool flag) override { m_scrollingPerformanceLoggingEnabled = flag; }
    virtual void setUnparentsOffscreenTiles(bool flag) override { m_unparentsOffscreenTiles = flag; }
    virtual double retainedTileBackingStoreMemory() const override;
    virtual IntRect tileCoverageRect() const override;
    virtual PlatformCALayer* tiledScrollingIndicatorLayer() override;
    virtual void setScrollingModeIndication(ScrollingModeIndication) override;
    virtual void setTileMargins(int marginTop, int marginBottom, int marginLeft, int marginRight) override;
    virtual void setZoomedOutContentsScale(float) override;
    virtual float zoomedOutContentsScale() const override { return m_zoomedOutContentsScale; }

    void scheduleTileRevalidation(double interval);
    void tileRevalidationTimerFired(Timer<TileController>*);

    void setNeedsRevalidateTiles();

    PlatformCALayerClient* owningGraphicsLayer() const { return m_tileCacheLayer->owner(); }

    PlatformCALayer* m_tileCacheLayer;

    std::unique_ptr<TileCoverageMap> m_coverageMap;

    std::unique_ptr<TileGrid> m_tileGrid;

    IntSize m_tileSize;
    FloatRect m_visibleRect;
    FloatRect m_visibleRectAtLastRevalidate;
    IntRect m_boundsAtLastRevalidate;

    Timer<TileController> m_tileRevalidationTimer;

    float m_contentsScale;
    float m_zoomedOutContentsScale;
    float m_deviceScaleFactor;

    TileCoverage m_tileCoverage;

    // m_marginTop and m_marginBottom are the height in pixels of the top and bottom margin tiles. The width
    // of those tiles will be equivalent to the width of the other tiles in the grid. m_marginRight and
    // m_marginLeft are the width in pixels of the right and left margin tiles, respectively. The height of
    // those tiles will be equivalent to the height of the other tiles in the grid.
    int m_marginTop;
    int m_marginBottom;
    int m_marginLeft;
    int m_marginRight;

    bool m_isInWindow;
    bool m_scrollingPerformanceLoggingEnabled;
    bool m_unparentsOffscreenTiles;
    bool m_acceleratesDrawing;
    bool m_tilesAreOpaque;
    bool m_hasTilesWithTemporaryScaleFactor; // Used to make low-res tiles when zooming.

    Color m_tileDebugBorderColor;
    float m_tileDebugBorderWidth;
    ScrollingModeIndication m_indicatorMode;
};

} // namespace WebCore

#endif // TileController_h
