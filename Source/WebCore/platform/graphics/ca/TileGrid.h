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

#pragma once

#include "IntPointHash.h"
#include "IntRect.h"
#include "PlatformCALayerClient.h"
#include "Timer.h"
#include <wtf/Deque.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashMap.h>
#include <wtf/Ref.h>

#if USE(CG)
typedef struct CGContext *CGContextRef;
#endif

namespace WebCore {

class GraphicsContext;
class PlatformCALayer;
class TileController;

class TileGrid : public PlatformCALayerClient {
    WTF_MAKE_NONCOPYABLE(TileGrid);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit TileGrid(TileController&);
    ~TileGrid();

#if USE(CA)
    PlatformCALayer& containerLayer() { return m_containerLayer; }
#endif

    void setIsZoomedOutTileGrid(bool);

    void setScale(float);
    float scale() const { return m_scale; }

    void setNeedsDisplay();
    void setNeedsDisplayInRect(const IntRect&);
    void dropTilesInRect(const IntRect&);

    void updateTileLayerProperties();

    bool prepopulateRect(const FloatRect&);

    enum ValidationPolicyFlag : uint8_t {
        PruneSecondaryTiles = 1 << 0,
        UnparentAllTiles    = 1 << 1
    };
    void revalidateTiles(OptionSet<ValidationPolicyFlag> = { });

    bool tilesWouldChangeForCoverageRect(const FloatRect&) const;

    IntRect tileCoverageRect() const;
    IntRect extent() const;
    
    IntSize tileSize() const { return m_tileSize; }

    double retainedTileBackingStoreMemory() const;
    unsigned blankPixelCount() const;

#if USE(CG)
    void drawTileMapContents(CGContextRef, CGRect layerBounds) const;
#endif

#if PLATFORM(IOS_FAMILY)
    unsigned numberOfUnparentedTiles() const { return m_cohortList.size(); }
    void removeUnparentedTilesNow();
#endif

    using TileIndex = IntPoint;

    using TileCohort = unsigned;
    static constexpr TileCohort visibleTileCohort = std::numeric_limits<TileCohort>::max();

    struct TileInfo {
        RefPtr<PlatformCALayer> layer;
        TileCohort cohort { visibleTileCohort };
        bool hasStaleContent { false };
    };

private:
    void setTileNeedsDisplayInRect(const TileIndex&, TileInfo&, const IntRect& repaintRectInTileCoords, const IntRect& coverageRectInTileCoords);

    IntRect rectForTileIndex(const TileIndex&) const;
    bool getTileIndexRangeForRect(const IntRect&, TileIndex& topLeft, TileIndex& bottomRight) const;

    enum class CoverageType { PrimaryTiles, SecondaryTiles };
    IntRect ensureTilesForRect(const FloatRect&, CoverageType);

    struct TileCohortInfo {
        TileCohort cohort;
        MonotonicTime creationTime;
        TileCohortInfo(TileCohort inCohort, MonotonicTime inTime)
            : cohort(inCohort)
            , creationTime(inTime)
        { }

        Seconds timeUntilExpiration();
    };

    void removeAllTiles();
    void removeAllSecondaryTiles();
    void removeTilesInCohort(TileCohort);

    void scheduleCohortRemoval();
    void cohortRemovalTimerFired();
    TileCohort nextTileCohort() const;
    void startedNewCohort(TileCohort);
    TileCohort newestTileCohort() const;
    TileCohort oldestTileCohort() const;

    void removeTiles(Vector<TileGrid::TileIndex>& toRemove);

    // PlatformCALayerClient
    void platformCALayerPaintContents(PlatformCALayer*, GraphicsContext&, const FloatRect&, GraphicsLayerPaintBehavior) override;
    bool platformCALayerShowDebugBorders() const override;
    bool platformCALayerShowRepaintCounter(PlatformCALayer*) const override;
    int platformCALayerRepaintCount(PlatformCALayer*) const override;
    int platformCALayerIncrementRepaintCount(PlatformCALayer*) override;
    bool platformCALayerContentsOpaque() const override;
    bool platformCALayerDrawsContent() const override { return true; }
    float platformCALayerDeviceScaleFactor() const override;
    bool isUsingDisplayListDrawing(PlatformCALayer*) const override;

    TileController& m_controller;
#if USE(CA)
    Ref<PlatformCALayer> m_containerLayer;
#endif

    HashMap<TileIndex, TileInfo> m_tiles;

    IntRect m_primaryTileCoverageRect;
    Vector<FloatRect> m_secondaryTileCoverageRects;

    Deque<TileCohortInfo> m_cohortList;

    Timer m_cohortRemovalTimer;

    HashCountedSet<PlatformCALayer*> m_tileRepaintCounts;
    
    IntSize m_tileSize;

    float m_scale { 1 };
};

}
