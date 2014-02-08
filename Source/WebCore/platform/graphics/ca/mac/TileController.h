/*
 * Copyright (C) 2011, 2012, 2013 Apple Inc. All rights reserved.
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
#include "IntPointHash.h"
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

typedef Vector<RetainPtr<PlatformLayer>> PlatformLayerList;

class TileController : public TiledBacking, public PlatformCALayerClient {
    WTF_MAKE_NONCOPYABLE(TileController);

public:
    static PassOwnPtr<TileController> create(PlatformCALayer*);
    ~TileController();

    void tileCacheLayerBoundsChanged();

    void setNeedsDisplay();
    void setNeedsDisplayInRect(const IntRect&);

    void setScale(float);
    float scale() const { return m_scale; }

    bool acceleratesDrawing() const { return m_acceleratesDrawing; }
    void setAcceleratesDrawing(bool);

    void setTilesOpaque(bool);
    bool tilesAreOpaque() const { return m_tilesAreOpaque; }

    PlatformCALayer *tileContainerLayer() const { return m_tileContainerLayer.get(); }

    void setTileDebugBorderWidth(float);
    void setTileDebugBorderColor(Color);

    virtual FloatRect visibleRect() const override { return m_visibleRect; }

    unsigned blankPixelCount() const;
    static unsigned blankPixelCountForTiles(const PlatformLayerList&, const FloatRect&, const IntPoint&);

#if PLATFORM(IOS)
    unsigned numberOfUnparentedTiles() const { return m_cohortList.size(); }
    void removeUnparentedTilesNow();
#endif

public:
    // Only public for inline methods in the implementation file.
    typedef IntPoint TileIndex;
    typedef unsigned TileCohort;
    static const TileCohort VisibleTileCohort = UINT_MAX;
    typedef HashMap<PlatformCALayer*, int> RepaintCountMap;

    struct TileInfo {
        RefPtr<PlatformCALayer> layer;
        TileCohort cohort; // VisibleTileCohort is visible.
        bool hasStaleContent;
        
        TileInfo()
            : cohort(VisibleTileCohort)
            , hasStaleContent(false)
        { }
    };

private:
    TileController(PlatformCALayer*);

    // TiledBacking member functions.
    virtual void setVisibleRect(const FloatRect&) override;
    virtual bool tilesWouldChangeForVisibleRect(const FloatRect&) const override;
    virtual void setExposedRect(const FloatRect&) override;
    virtual void prepopulateRect(const FloatRect&) override;
    virtual void setIsInWindow(bool) override;
    virtual void setTileCoverage(TileCoverage) override;
    virtual TileCoverage tileCoverage() const override { return m_tileCoverage; }
    virtual void revalidateTiles() override;
    virtual void forceRepaint() override;
    virtual IntSize tileSize() const override { return m_tileSize; }
    virtual IntRect tileGridExtent() const override;
    virtual void setScrollingPerformanceLoggingEnabled(bool flag) override { m_scrollingPerformanceLoggingEnabled = flag; }
    virtual bool scrollingPerformanceLoggingEnabled() const override { return m_scrollingPerformanceLoggingEnabled; }
    virtual void setAggressivelyRetainsTiles(bool flag) override { m_aggressivelyRetainsTiles = flag; }
    virtual bool aggressivelyRetainsTiles() const override { return m_aggressivelyRetainsTiles; }
    virtual void setUnparentsOffscreenTiles(bool flag) override { m_unparentsOffscreenTiles = flag; }
    virtual bool unparentsOffscreenTiles() const override { return m_unparentsOffscreenTiles; }
    virtual double retainedTileBackingStoreMemory() const override;
    virtual IntRect tileCoverageRect() const override;
    virtual PlatformCALayer* tiledScrollingIndicatorLayer() override;
    virtual void setScrollingModeIndication(ScrollingModeIndication) override;
    virtual void setTileMargins(int marginTop, int marginBottom, int marginLeft, int marginRight) override;
    virtual bool hasMargins() const override;
    virtual int topMarginHeight() const override;
    virtual int bottomMarginHeight() const override;
    virtual int leftMarginWidth() const override;
    virtual int rightMarginWidth() const override;

    // PlatformCALayerClient
    virtual void platformCALayerLayoutSublayersOfLayer(PlatformCALayer*) override { }
    virtual bool platformCALayerRespondsToLayoutChanges() const override { return false; }
    virtual void platformCALayerAnimationStarted(CFTimeInterval) override { }
    virtual GraphicsLayer::CompositingCoordinatesOrientation platformCALayerContentsOrientation() const override { return GraphicsLayer::CompositingCoordinatesTopDown; }
    virtual void platformCALayerPaintContents(PlatformCALayer*, GraphicsContext&, const IntRect&) override;
    virtual bool platformCALayerShowDebugBorders() const override;
    virtual bool platformCALayerShowRepaintCounter(PlatformCALayer*) const override;
    virtual int platformCALayerIncrementRepaintCount(PlatformCALayer*) override;

    virtual bool platformCALayerContentsOpaque() const override { return m_tilesAreOpaque; }
    virtual bool platformCALayerDrawsContent() const override { return true; }
    virtual void platformCALayerLayerDidDisplay(PlatformLayer*) override { }

    virtual void platformCALayerSetNeedsToRevalidateTiles() override { }
    virtual float platformCALayerDeviceScaleFactor() const override;

    virtual IntRect bounds() const override;
    IntRect boundsWithoutMargin() const;
    IntRect boundsAtLastRevalidateWithoutMargin() const;

    IntRect rectForTileIndex(const TileIndex&) const;
    void adjustRectAtTileIndexForMargin(const TileIndex&, IntRect&) const;
    void getTileIndexRangeForRect(const IntRect&, TileIndex& topLeft, TileIndex& bottomRight) const;

    FloatRect computeTileCoverageRect(const FloatRect& previousVisibleRect, const FloatRect& currentVisibleRect) const;

    void scheduleTileRevalidation(double interval);
    void tileRevalidationTimerFired(Timer<TileController>*);

    void scheduleCohortRemoval();
    void cohortRemovalTimerFired(Timer<TileController>*);
    
    typedef unsigned TileValidationPolicyFlags;

    void setNeedsRevalidateTiles();
    void revalidateTiles(TileValidationPolicyFlags foregroundValidationPolicy, TileValidationPolicyFlags backgroundValidationPolicy);
    enum class CoverageType { PrimaryTiles, SecondaryTiles };

    // Returns the bounds of the covered tiles.
    IntRect ensureTilesForRect(const FloatRect&, CoverageType);
    void updateTileCoverageMap();

    void removeAllTiles();
    void removeAllSecondaryTiles();
    void removeTilesInCohort(TileCohort);

    TileCohort nextTileCohort() const;
    void startedNewCohort(TileCohort);
    
    TileCohort newestTileCohort() const;
    TileCohort oldestTileCohort() const;

    void setTileNeedsDisplayInRect(const TileIndex&, TileInfo&, const IntRect& repaintRectInTileCoords, const IntRect& coverageRectInTileCoords);

    RefPtr<PlatformCALayer> createTileLayer(const IntRect&);

    void drawTileMapContents(CGContextRef, CGRect);

    PlatformCALayerClient* owningGraphicsLayer() const { return m_tileCacheLayer->owner(); }

    FloatRect scaledExposedRect() const;

    PlatformCALayer* m_tileCacheLayer;
    RefPtr<PlatformCALayer> m_tileContainerLayer;
    RefPtr<PlatformCALayer> m_tiledScrollingIndicatorLayer; // Used for coverage visualization.
    RefPtr<PlatformCALayer> m_visibleRectIndicatorLayer;

    IntSize m_tileSize;
    FloatRect m_visibleRect;
    FloatRect m_visibleRectAtLastRevalidate;
    FloatRect m_exposedRect; // The exposed area of containing platform views.
    IntRect m_boundsAtLastRevalidate;
    
    Vector<FloatRect> m_secondaryTileCoverageRects;

    typedef HashMap<TileIndex, TileInfo> TileMap;
    TileMap m_tiles;
    Timer<TileController> m_tileRevalidationTimer;
    Timer<TileController> m_cohortRemovalTimer;

    RepaintCountMap m_tileRepaintCounts;

    struct TileCohortInfo {
        TileCohort cohort;
        double creationTime; // in monotonicallyIncreasingTime().
        TileCohortInfo(TileCohort inCohort, double inTime)
            : cohort(inCohort)
            , creationTime(inTime)
        { }
    };
    typedef Deque<TileCohortInfo> TileCohortList;
    TileCohortList m_cohortList;
    
    IntRect m_primaryTileCoverageRect; // In tile coords.

    float m_scale;
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
    bool m_aggressivelyRetainsTiles;
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
