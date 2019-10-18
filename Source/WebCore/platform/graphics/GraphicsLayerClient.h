/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "TiledBacking.h"
#include <wtf/Forward.h>
#include <wtf/OptionSet.h>

namespace WebCore {

class FloatPoint;
class FloatRect;
class GraphicsContext;
class GraphicsLayer;
class IntPoint;
class IntRect;
class TransformationMatrix;

enum class GraphicsLayerPaintingPhase {
    Background            = 1 << 0,
    Foreground            = 1 << 1,
    Mask                  = 1 << 2,
    ClipPath              = 1 << 3,
    OverflowContents      = 1 << 4,
    CompositedScroll      = 1 << 5,
    ChildClippingMask     = 1 << 6,
};

enum AnimatedPropertyID {
    AnimatedPropertyInvalid,
    AnimatedPropertyTransform,
    AnimatedPropertyOpacity,
    AnimatedPropertyBackgroundColor,
    AnimatedPropertyFilter,
#if ENABLE(FILTERS_LEVEL_2)
    AnimatedPropertyWebkitBackdropFilter,
#endif
};

enum LayerTreeAsTextBehaviorFlags {
    LayerTreeAsTextBehaviorNormal               = 0,
    LayerTreeAsTextDebug                        = 1 << 0, // Dump extra debugging info like layer addresses.
    LayerTreeAsTextIncludeVisibleRects          = 1 << 1,
    LayerTreeAsTextIncludeTileCaches            = 1 << 2,
    LayerTreeAsTextIncludeRepaintRects          = 1 << 3,
    LayerTreeAsTextIncludePaintingPhases        = 1 << 4,
    LayerTreeAsTextIncludeContentLayers         = 1 << 5,
    LayerTreeAsTextIncludePageOverlayLayers     = 1 << 6,
    LayerTreeAsTextIncludeAcceleratesDrawing    = 1 << 7,
    LayerTreeAsTextIncludeClipping              = 1 << 8,
    LayerTreeAsTextIncludeBackingStoreAttached  = 1 << 9,
    LayerTreeAsTextIncludeRootLayerProperties   = 1 << 10,
    LayerTreeAsTextIncludeEventRegion           = 1 << 11,
    LayerTreeAsTextIncludeDeepColor             = 1 << 12,
    LayerTreeAsTextShowAll                      = 0xFFFF
};
typedef unsigned LayerTreeAsTextBehavior;

enum GraphicsLayerPaintFlags {
    GraphicsLayerPaintNormal                    = 0,
    GraphicsLayerPaintSnapshotting              = 1 << 0,
    GraphicsLayerPaintFirstTilePaint            = 1 << 1,
};
typedef unsigned GraphicsLayerPaintBehavior;
    
class GraphicsLayerClient {
public:
    virtual ~GraphicsLayerClient() = default;

    virtual void tiledBackingUsageChanged(const GraphicsLayer*, bool /*usingTiledBacking*/) { }
    
    // Callback for when hardware-accelerated animation started.
    virtual void notifyAnimationStarted(const GraphicsLayer*, const String& /*animationKey*/, MonotonicTime /*time*/) { }
    virtual void notifyAnimationEnded(const GraphicsLayer*, const String& /*animationKey*/) { }

    // Notification that a layer property changed that requires a subsequent call to flushCompositingState()
    // to appear on the screen.
    virtual void notifyFlushRequired(const GraphicsLayer*) { }
    
    // Notification that this layer requires a flush before the next display refresh.
    virtual void notifyFlushBeforeDisplayRefresh(const GraphicsLayer*) { }

    virtual void paintContents(const GraphicsLayer*, GraphicsContext&, const FloatRect& /* inClip */, GraphicsLayerPaintBehavior) { }
    virtual void didChangePlatformLayerForLayer(const GraphicsLayer*) { }

    // Provides current transform (taking transform-origin and animations into account). Input matrix has been
    // initialized to identity already. Returns false if the layer has no transform.
    virtual bool getCurrentTransform(const GraphicsLayer*, TransformationMatrix&) const { return false; }

    // Allows the client to modify a layer position used during the visibleRect calculation, for example to ignore
    // scroll overhang.
    virtual void customPositionForVisibleRectComputation(const GraphicsLayer*, FloatPoint&) const { }

    // Multiplier for backing store size, related to high DPI.
    virtual float deviceScaleFactor() const { return 1; }
    // Page scale factor.
    virtual float pageScaleFactor() const { return 1; }
    virtual float zoomedOutPageScaleFactor() const { return 0; }

    virtual float contentsScaleMultiplierForNewTiles(const GraphicsLayer*) const { return 1; }
    virtual bool paintsOpaquelyAtNonIntegralScales(const GraphicsLayer*) const { return false; }

    virtual bool isTrackingRepaints() const { return false; }

    virtual bool shouldSkipLayerInDump(const GraphicsLayer*, LayerTreeAsTextBehavior) const { return false; }
    virtual bool shouldDumpPropertyForLayer(const GraphicsLayer*, const char*, LayerTreeAsTextBehavior) const { return true; }

    virtual bool shouldAggressivelyRetainTiles(const GraphicsLayer*) const { return false; }
    virtual bool shouldTemporarilyRetainTileCohorts(const GraphicsLayer*) const { return true; }

    virtual bool useGiantTiles() const { return false; }

    virtual bool needsPixelAligment() const { return false; }

    virtual bool needsIOSDumpRenderTreeMainFrameRenderViewLayerIsAlwaysOpaqueHack(const GraphicsLayer&) const { return false; }

    virtual void logFilledVisibleFreshTile(unsigned) { };

#ifndef NDEBUG
    // RenderLayerBacking overrides this to verify that it is not
    // currently painting contents. An ASSERT fails, if it is.
    // This is executed in GraphicsLayer construction and destruction
    // to verify that we don't create or destroy GraphicsLayers
    // while painting.
    virtual void verifyNotPainting() { }
#endif
};

} // namespace WebCore

