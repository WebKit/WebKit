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

#ifndef TileGrid_h
#define TileGrid_h

#include "IntPointHash.h"
#include "IntRect.h"
#include "PlatformCALayerClient.h"
#include "Timer.h"
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/Ref.h>

namespace WebCore {

class GraphicsContext;
class PlatformCALayer;
class TileController;

class TileGrid : public PlatformCALayerClient {
    WTF_MAKE_NONCOPYABLE(TileGrid); WTF_MAKE_FAST_ALLOCATED;
public:
    TileGrid(TileController&);
    ~TileGrid();

    PlatformCALayer& containerLayer() { return m_containerLayer.get(); }

    void setScale(float);
    float scale() const { return m_scale; }

    void setNeedsDisplay();
    void setNeedsDisplayInRect(const IntRect&);
    void dropTilesInRect(const IntRect&);

    void updateTilerLayerProperties();

    bool prepopulateRect(const FloatRect&);

    enum TileValidationPolicyFlag {
        PruneSecondaryTiles = 1 << 0,
        UnparentAllTiles = 1 << 1
    };
    void revalidateTiles(unsigned validationPolicyFlags);
    bool tilesWouldChangeForVisibleRect(const FloatRect& newVisibleRect, const FloatRect& oldVisibleRect) const;

    IntRect tileCoverageRect() const;
    IntRect extent() const;

    double retainedTileBackingStoreMemory() const;
    unsigned blankPixelCount() const;

    void drawTileMapContents(CGContextRef, CGRect layerBounds) const;

#if PLATFORM(IOS)
    unsigned numberOfUnparentedTiles() const { return m_cohortList.size(); }
    void removeUnparentedTilesNow();
#endif

    typedef IntPoint TileIndex;
    typedef unsigned TileCohort;
    static const TileCohort VisibleTileCohort = UINT_MAX;

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
    void setTileNeedsDisplayInRect(const TileIndex&, TileInfo&, const IntRect& repaintRectInTileCoords, const IntRect& coverageRectInTileCoords);

    IntRect rectForTileIndex(const TileIndex&) const;
    void adjustRectAtTileIndexForMargin(const TileIndex&, IntRect&) const;
    void getTileIndexRangeForRect(const IntRect&, TileIndex& topLeft, TileIndex& bottomRight) const;

    enum class CoverageType { PrimaryTiles, SecondaryTiles };
    IntRect ensureTilesForRect(const FloatRect&, CoverageType);

    void removeAllSecondaryTiles();
    void removeTilesInCohort(TileCohort);

    void scheduleCohortRemoval();
    void cohortRemovalTimerFired(Timer<TileGrid>*);
    TileCohort nextTileCohort() const;
    void startedNewCohort(TileCohort);
    TileCohort newestTileCohort() const;
    TileCohort oldestTileCohort() const;

    void removeTiles(Vector<TileGrid::TileIndex>& toRemove);

    // PlatformCALayerClient
    virtual void platformCALayerPaintContents(PlatformCALayer*, GraphicsContext&, const FloatRect&) override;
    virtual bool platformCALayerShowDebugBorders() const override;
    virtual bool platformCALayerShowRepaintCounter(PlatformCALayer*) const override;
    virtual int platformCALayerIncrementRepaintCount(PlatformCALayer*) override;
    virtual bool platformCALayerContentsOpaque() const override;
    virtual bool platformCALayerDrawsContent() const override { return true; }
    virtual float platformCALayerDeviceScaleFactor() const override;

    TileController& m_controller;
    Ref<PlatformCALayer> m_containerLayer;

    typedef HashMap<TileIndex, TileInfo> TileMap;
    TileMap m_tiles;

    IntRect m_primaryTileCoverageRect;
    Vector<FloatRect> m_secondaryTileCoverageRects;

    float m_scale;

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

    Timer<TileGrid> m_cohortRemovalTimer;

    typedef HashMap<PlatformCALayer*, int> RepaintCountMap;
    RepaintCountMap m_tileRepaintCounts;
};

}
#endif
