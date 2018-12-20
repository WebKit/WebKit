/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include <wtf/MonotonicTime.h>
#include <wtf/Optional.h>

namespace WebCore {

enum TileSizeMode {
    StandardTileSizeMode,
    GiantTileSizeMode
};

class FloatPoint;
class FloatRect;
class FloatSize;
class IntRect;
class IntSize;
class PlatformCALayer;

enum ScrollingModeIndication {
    SynchronousScrollingBecauseOfLackOfScrollingCoordinatorIndication,
    SynchronousScrollingBecauseOfStyleIndication,
    SynchronousScrollingBecauseOfEventHandlersIndication,
    AsyncScrollingIndication
};

struct VelocityData  {
    double horizontalVelocity;
    double verticalVelocity;
    double scaleChangeRate;
    MonotonicTime lastUpdateTime;
    
    VelocityData(double horizontal = 0, double vertical = 0, double scaleChange = 0, MonotonicTime updateTime = MonotonicTime())
        : horizontalVelocity(horizontal)
        , verticalVelocity(vertical)
        , scaleChangeRate(scaleChange)
        , lastUpdateTime(updateTime)
    {
    }
    
    bool velocityOrScaleIsChanging() const
    {
        return horizontalVelocity || verticalVelocity || scaleChangeRate;
    }
};

class TiledBacking {
public:
    virtual ~TiledBacking() = default;

    virtual void setVisibleRect(const FloatRect&) = 0;
    virtual FloatRect visibleRect() const = 0;

    // Only used to update the tile coverage map. 
    virtual void setLayoutViewportRect(Optional<FloatRect>) = 0;

    virtual void setCoverageRect(const FloatRect&) = 0;
    virtual FloatRect coverageRect() const = 0;
    virtual bool tilesWouldChangeForCoverageRect(const FloatRect&) const = 0;

    virtual void setTiledScrollingIndicatorPosition(const FloatPoint&) = 0;
    virtual void setTopContentInset(float) = 0;

    virtual void setVelocity(const VelocityData&) = 0;

    virtual void setTileSizeUpdateDelayDisabledForTesting(bool) = 0;
    
    enum {
        NotScrollable           = 0,
        HorizontallyScrollable  = 1 << 0,
        VerticallyScrollable    = 1 << 1
    };
    typedef unsigned Scrollability;
    virtual void setScrollability(Scrollability) = 0;

    virtual void prepopulateRect(const FloatRect&) = 0;

    virtual void setIsInWindow(bool) = 0;
    virtual bool isInWindow() const = 0;

    enum {
        CoverageForVisibleArea = 0,
        CoverageForVerticalScrolling = 1 << 0,
        CoverageForHorizontalScrolling = 1 << 1,
        CoverageForScrolling = CoverageForVerticalScrolling | CoverageForHorizontalScrolling
    };
    typedef unsigned TileCoverage;

    virtual void setTileCoverage(TileCoverage) = 0;
    virtual TileCoverage tileCoverage() const = 0;

    virtual void adjustTileCoverageRect(FloatRect& coverageRect, const FloatSize& newSize, const FloatRect& previousVisibleRect, const FloatRect& currentVisibleRect, float contentsScale) const = 0;

    virtual void willStartLiveResize() = 0;
    virtual void didEndLiveResize() = 0;

    virtual IntSize tileSize() const = 0;

    virtual void revalidateTiles() = 0;
    virtual void forceRepaint() = 0;

    virtual void setScrollingPerformanceLoggingEnabled(bool) = 0;
    virtual bool scrollingPerformanceLoggingEnabled() const = 0;
    
    virtual double retainedTileBackingStoreMemory() const = 0;

    virtual void setHasMargins(bool marginTop, bool marginBottom, bool marginLeft, bool marginRight) = 0;
    virtual void setMarginSize(int) = 0;
    virtual bool hasMargins() const = 0;
    virtual bool hasHorizontalMargins() const = 0;
    virtual bool hasVerticalMargins() const = 0;

    virtual int topMarginHeight() const = 0;
    virtual int bottomMarginHeight() const = 0;
    virtual int leftMarginWidth() const = 0;
    virtual int rightMarginWidth() const = 0;

    virtual void setZoomedOutContentsScale(float) = 0;
    virtual float zoomedOutContentsScale() const = 0;

    // Includes margins.
    virtual IntRect bounds() const = 0;
    virtual IntRect boundsWithoutMargin() const = 0;

    // Exposed for testing
    virtual IntRect tileCoverageRect() const = 0;
    virtual IntRect tileGridExtent() const = 0;
    virtual void setScrollingModeIndication(ScrollingModeIndication) = 0;

#if USE(CA)
    virtual PlatformCALayer* tiledScrollingIndicatorLayer() = 0;
#endif
};

} // namespace WebCore
