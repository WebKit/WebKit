/*
 * Copyright (C) 2009-2017 Apple Inc. All rights reserved.
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

#include "Color.h"
#include "EventRegion.h"
#include "FilterOperations.h"
#include "FloatPoint.h"
#include "FloatPoint3D.h"
#include "FloatRoundedRect.h"
#include "FloatSize.h"
#include "GraphicsLayerClient.h"
#include "Path.h"
#include "PlatformLayer.h"
#include "Region.h"
#include "ScrollableArea.h"
#include "ScrollTypes.h"
#include "TimingFunction.h"
#include "TransformOperations.h"
#include "WindRule.h"
#include <wtf/EnumTraits.h>
#include <wtf/Function.h>
#include <wtf/TypeCasts.h>

#if ENABLE(CSS_COMPOSITING)
#include "GraphicsTypes.h"
#endif

namespace WTF {
class TextStream;
}

namespace WebCore {

class Animation;
class GraphicsContext;
class GraphicsLayerFactory;
class Image;
class TiledBacking;
class TimingFunction;
class TransformationMatrix;

namespace DisplayList {
typedef unsigned AsTextFlags;
}

// Base class for animation values (also used for transitions). Here to
// represent values for properties being animated via the GraphicsLayer,
// without pulling in style-related data from outside of the platform directory.
// FIXME: Should be moved to its own header file.
class AnimationValue {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~AnimationValue() = default;

    double keyTime() const { return m_keyTime; }
    const TimingFunction* timingFunction() const { return m_timingFunction.get(); }
    virtual std::unique_ptr<AnimationValue> clone() const = 0;

protected:
    AnimationValue(double keyTime, TimingFunction* timingFunction = nullptr)
        : m_keyTime(keyTime)
        , m_timingFunction(timingFunction)
    {
    }

    AnimationValue(const AnimationValue& other)
        : m_keyTime(other.m_keyTime)
        , m_timingFunction(other.m_timingFunction ? RefPtr<TimingFunction> { other.m_timingFunction->clone() } : nullptr)
    {
    }

    AnimationValue(AnimationValue&&) = default;

private:
    void operator=(const AnimationValue&) = delete;

    double m_keyTime;
    RefPtr<TimingFunction> m_timingFunction;
};

// Used to store one float value of an animation.
// FIXME: Should be moved to its own header file.
class FloatAnimationValue : public AnimationValue {
public:
    FloatAnimationValue(double keyTime, float value, TimingFunction* timingFunction = nullptr)
        : AnimationValue(keyTime, timingFunction)
        , m_value(value)
    {
    }

    std::unique_ptr<AnimationValue> clone() const override
    {
        return makeUnique<FloatAnimationValue>(*this);
    }

    float value() const { return m_value; }

private:
    float m_value;
};

// Used to store one transform value in a keyframe list.
// FIXME: Should be moved to its own header file.
class TransformAnimationValue : public AnimationValue {
public:
    TransformAnimationValue(double keyTime, const TransformOperations& value, TimingFunction* timingFunction = nullptr)
        : AnimationValue(keyTime, timingFunction)
        , m_value(value)
    {
    }

    std::unique_ptr<AnimationValue> clone() const override
    {
        return makeUnique<TransformAnimationValue>(*this);
    }

    TransformAnimationValue(const TransformAnimationValue& other)
        : AnimationValue(other)
    {
        m_value.operations().reserveInitialCapacity(other.m_value.operations().size());
        for (auto& operation : other.m_value.operations())
            m_value.operations().uncheckedAppend(operation->clone());
    }

    TransformAnimationValue(TransformAnimationValue&&) = default;

    const TransformOperations& value() const { return m_value; }

private:
    TransformOperations m_value;
};

// Used to store one filter value in a keyframe list.
// FIXME: Should be moved to its own header file.
class FilterAnimationValue : public AnimationValue {
public:
    FilterAnimationValue(double keyTime, const FilterOperations& value, TimingFunction* timingFunction = nullptr)
        : AnimationValue(keyTime, timingFunction)
        , m_value(value)
    {
    }

    std::unique_ptr<AnimationValue> clone() const override
    {
        return makeUnique<FilterAnimationValue>(*this);
    }

    FilterAnimationValue(const FilterAnimationValue& other)
        : AnimationValue(other)
    {
        m_value.operations().reserveInitialCapacity(other.m_value.operations().size());
        for (auto& operation : other.m_value.operations())
            m_value.operations().uncheckedAppend(operation->clone());
    }

    FilterAnimationValue(FilterAnimationValue&&) = default;

    const FilterOperations& value() const { return m_value; }

private:
    FilterOperations m_value;
};

// Used to store a series of values in a keyframe list.
// Values will all be of the same type, which can be inferred from the property.
// FIXME: Should be moved to its own header file.
class KeyframeValueList {
public:
    explicit KeyframeValueList(AnimatedPropertyID property)
        : m_property(property)
    {
    }

    KeyframeValueList(const KeyframeValueList& other)
        : m_property(other.property())
    {
        m_values.reserveInitialCapacity(other.m_values.size());
        for (auto& value : other.m_values)
            m_values.uncheckedAppend(value->clone());
    }

    KeyframeValueList(KeyframeValueList&&) = default;

    KeyframeValueList& operator=(const KeyframeValueList& other)
    {
        KeyframeValueList copy(other);
        swap(copy);
        return *this;
    }

    KeyframeValueList& operator=(KeyframeValueList&&) = default;

    void swap(KeyframeValueList& other)
    {
        std::swap(m_property, other.m_property);
        m_values.swap(other.m_values);
    }

    AnimatedPropertyID property() const { return m_property; }

    size_t size() const { return m_values.size(); }
    const AnimationValue& at(size_t i) const { return *m_values.at(i); }

    // Insert, sorted by keyTime.
    WEBCORE_EXPORT void insert(std::unique_ptr<const AnimationValue>);

protected:
    Vector<std::unique_ptr<const AnimationValue>> m_values;
    AnimatedPropertyID m_property;
};

// GraphicsLayer is an abstraction for a rendering surface with backing store,
// which may have associated transformation and animations.

class GraphicsLayer : public RefCounted<GraphicsLayer> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Type : uint8_t {
        Normal,
        PageTiledBacking,
        ScrollContainer,
        ScrolledContents,
        Shape
    };

    WEBCORE_EXPORT static Ref<GraphicsLayer> create(GraphicsLayerFactory*, GraphicsLayerClient&, Type = Type::Normal);
    
    WEBCORE_EXPORT virtual ~GraphicsLayer();
    
    // Unparent, clear the client, and clear the RefPtr.
    WEBCORE_EXPORT static void unparentAndClear(RefPtr<GraphicsLayer>&);
    // Clear the client, and clear the RefPtr, but leave parented.
    WEBCORE_EXPORT static void clear(RefPtr<GraphicsLayer>&);

    WEBCORE_EXPORT void clearClient();
    WEBCORE_EXPORT void setClient(GraphicsLayerClient&);

    Type type() const { return m_type; }

    virtual void initialize(Type) { }

    using PlatformLayerID = uint64_t;
    virtual PlatformLayerID primaryLayerID() const { return 0; }

    GraphicsLayerClient& client() const { return *m_client; }

    // Layer name. Only used to identify layers in debug output
    const String& name() const { return m_name; }
    virtual void setName(const String& name) { m_name = name; }
    virtual String debugName() const;

    GraphicsLayer* parent() const { return m_parent; };
    void setParent(GraphicsLayer*); // Internal use only.
    
    // Returns true if the layer has the given layer as an ancestor (excluding self).
    bool hasAncestor(GraphicsLayer*) const;
    
    const Vector<Ref<GraphicsLayer>>& children() const { return m_children; }
    Vector<Ref<GraphicsLayer>>& children() { return m_children; }

    // Returns true if the child list changed.
    WEBCORE_EXPORT virtual bool setChildren(Vector<Ref<GraphicsLayer>>&&);

    // Add child layers. If the child is already parented, it will be removed from its old parent.
    WEBCORE_EXPORT virtual void addChild(Ref<GraphicsLayer>&&);
    WEBCORE_EXPORT virtual void addChildAtIndex(Ref<GraphicsLayer>&&, int index);
    WEBCORE_EXPORT virtual void addChildAbove(Ref<GraphicsLayer>&&, GraphicsLayer* sibling);
    WEBCORE_EXPORT virtual void addChildBelow(Ref<GraphicsLayer>&&, GraphicsLayer* sibling);
    WEBCORE_EXPORT virtual bool replaceChild(GraphicsLayer* oldChild, Ref<GraphicsLayer>&& newChild);

    WEBCORE_EXPORT void removeAllChildren();
    WEBCORE_EXPORT virtual void removeFromParent();

    // The parent() of a maskLayer is set to the layer being masked.
    GraphicsLayer* maskLayer() const { return m_maskLayer.get(); }
    virtual void setMaskLayer(RefPtr<GraphicsLayer>&&);

    void setIsMaskLayer(bool isMask) { m_isMaskLayer = isMask; }
    bool isMaskLayer() const { return m_isMaskLayer; }
    
    // The given layer will replicate this layer and its children; the replica renders behind this layer.
    WEBCORE_EXPORT virtual void setReplicatedByLayer(RefPtr<GraphicsLayer>&&);
    // Whether this layer is being replicated by another layer.
    bool isReplicated() const { return m_replicaLayer; }
    // The layer that replicates this layer (if any).
    GraphicsLayer* replicaLayer() const { return m_replicaLayer.get(); }

    const FloatPoint& replicatedLayerPosition() const { return m_replicatedLayerPosition; }
    void setReplicatedLayerPosition(const FloatPoint& p) { m_replicatedLayerPosition = p; }

    enum ShouldSetNeedsDisplay {
        DontSetNeedsDisplay,
        SetNeedsDisplay
    };

    // Offset is origin of the renderer minus origin of the graphics layer.
    FloatSize offsetFromRenderer() const { return m_offsetFromRenderer; }
    void setOffsetFromRenderer(const FloatSize&, ShouldSetNeedsDisplay = SetNeedsDisplay);

    // Scroll offset of the content layer inside its scrolling parent layer.
    ScrollOffset scrollOffset() const { return m_scrollOffset; }
    void setScrollOffset(const ScrollOffset&, ShouldSetNeedsDisplay = SetNeedsDisplay);

#if ENABLE(SCROLLING_THREAD)
    ScrollingNodeID scrollingNodeID() const { return m_scrollingNodeID; }
    virtual void setScrollingNodeID(ScrollingNodeID nodeID) { m_scrollingNodeID = nodeID; }
#endif

    // The position of the layer (the location of its top-left corner in its parent)
    const FloatPoint& position() const { return m_position; }
    virtual void setPosition(const FloatPoint& p) { m_approximatePosition = WTF::nullopt; m_position = p; }

    // approximatePosition, if set, overrides position() and is used during coverage rect computation.
    FloatPoint approximatePosition() const { return m_approximatePosition ? m_approximatePosition.value() : m_position; }
    virtual void setApproximatePosition(const FloatPoint& p) { m_approximatePosition = p; }

    // For platforms that move underlying platform layers on a different thread for scrolling; just update the GraphicsLayer state.
    virtual void syncPosition(const FloatPoint& p) { m_position = p; }

    // Anchor point: (0, 0) is top left, (1, 1) is bottom right. The anchor point
    // affects the origin of the transforms.
    const FloatPoint3D& anchorPoint() const { return m_anchorPoint; }
    virtual void setAnchorPoint(const FloatPoint3D& p) { m_anchorPoint = p; }

    // The size of the layer.
    const FloatSize& size() const { return m_size; }
    WEBCORE_EXPORT virtual void setSize(const FloatSize&);

    // The boundOrigin affects the offset at which content is rendered, and sublayers are positioned.
    const FloatPoint& boundsOrigin() const { return m_boundsOrigin; }
    virtual void setBoundsOrigin(const FloatPoint& origin) { m_boundsOrigin = origin; }

    // For platforms that move underlying platform layers on a different thread for scrolling; just update the GraphicsLayer state.
    virtual void syncBoundsOrigin(const FloatPoint& origin) { m_boundsOrigin = origin; }

    const TransformationMatrix& transform() const;
    virtual void setTransform(const TransformationMatrix&);
    bool hasNonIdentityTransform() const { return m_transform && !m_transform->isIdentity(); }

    const TransformationMatrix& childrenTransform() const;
    virtual void setChildrenTransform(const TransformationMatrix&);
    bool hasNonIdentityChildrenTransform() const { return m_childrenTransform && !m_childrenTransform->isIdentity(); }

    bool preserves3D() const { return m_preserves3D; }
    virtual void setPreserves3D(bool b) { m_preserves3D = b; }
    
    bool masksToBounds() const { return m_masksToBounds; }
    virtual void setMasksToBounds(bool b) { m_masksToBounds = b; }
    
    bool drawsContent() const { return m_drawsContent; }
    virtual void setDrawsContent(bool b) { m_drawsContent = b; }

    bool contentsAreVisible() const { return m_contentsVisible; }
    virtual void setContentsVisible(bool b) { m_contentsVisible = b; }

    bool userInteractionEnabled() const { return m_userInteractionEnabled; }
    virtual void setUserInteractionEnabled(bool b) { m_userInteractionEnabled = b; }
    
    bool acceleratesDrawing() const { return m_acceleratesDrawing; }
    virtual void setAcceleratesDrawing(bool b) { m_acceleratesDrawing = b; }

    bool usesDisplayListDrawing() const { return m_usesDisplayListDrawing; }
    virtual void setUsesDisplayListDrawing(bool b) { m_usesDisplayListDrawing = b; }

    bool needsBackdrop() const { return !m_backdropFilters.isEmpty(); }

    // The color used to paint the layer background. Pass an invalid color to remove it.
    // Note that this covers the entire layer. Use setContentsToSolidColor() if the color should
    // only cover the contentsRect.
    const Color& backgroundColor() const { return m_backgroundColor; }
    WEBCORE_EXPORT virtual void setBackgroundColor(const Color&);

    // opaque means that we know the layer contents have no alpha
    bool contentsOpaque() const { return m_contentsOpaque; }
    virtual void setContentsOpaque(bool b) { m_contentsOpaque = b; }

    bool supportsSubpixelAntialiasedText() const { return m_supportsSubpixelAntialiasedText; }
    virtual void setSupportsSubpixelAntialiasedText(bool b) { m_supportsSubpixelAntialiasedText = b; }

    bool backfaceVisibility() const { return m_backfaceVisibility; }
    virtual void setBackfaceVisibility(bool b) { m_backfaceVisibility = b; }

    float opacity() const { return m_opacity; }
    virtual void setOpacity(float opacity) { m_opacity = opacity; }

    const FilterOperations& filters() const { return m_filters; }
    // Returns true if filter can be rendered by the compositor.
    virtual bool setFilters(const FilterOperations& filters) { m_filters = filters; return true; }

    const FilterOperations& backdropFilters() const { return m_backdropFilters; }
    virtual bool setBackdropFilters(const FilterOperations& filters) { m_backdropFilters = filters; return true; }

    virtual void setBackdropFiltersRect(const FloatRoundedRect& backdropFiltersRect) { m_backdropFiltersRect = backdropFiltersRect; }
    const FloatRoundedRect& backdropFiltersRect() const { return m_backdropFiltersRect; }

#if ENABLE(CSS_COMPOSITING)
    BlendMode blendMode() const { return m_blendMode; }
    virtual void setBlendMode(BlendMode blendMode) { m_blendMode = blendMode; }
#endif

    // Some GraphicsLayers paint only the foreground or the background content
    OptionSet<GraphicsLayerPaintingPhase> paintingPhase() const { return m_paintingPhase; }
    void setPaintingPhase(OptionSet<GraphicsLayerPaintingPhase>);

    enum ShouldClipToLayer {
        DoNotClipToLayer,
        ClipToLayer
    };

    virtual void setNeedsDisplay() = 0;
    // mark the given rect (in layer coords) as needing dispay. Never goes deep.
    virtual void setNeedsDisplayInRect(const FloatRect&, ShouldClipToLayer = ClipToLayer) = 0;

    virtual void setContentsNeedsDisplay() { };

    // The tile phase is relative to the GraphicsLayer bounds.
    virtual void setContentsTilePhase(const FloatSize& p) { m_contentsTilePhase = p; }
    FloatSize contentsTilePhase() const { return m_contentsTilePhase; }

    virtual void setContentsTileSize(const FloatSize& s) { m_contentsTileSize = s; }
    FloatSize contentsTileSize() const { return m_contentsTileSize; }
    bool hasContentsTiling() const { return !m_contentsTileSize.isEmpty(); }

    // Set that the position/size of the contents (image or video).
    FloatRect contentsRect() const { return m_contentsRect; }
    virtual void setContentsRect(const FloatRect& r) { m_contentsRect = r; }

    // Set a rounded rect that will be used to clip the layer contents.
    FloatRoundedRect contentsClippingRect() const { return m_contentsClippingRect; }
    virtual void setContentsClippingRect(const FloatRoundedRect& roundedRect) { m_contentsClippingRect = roundedRect; }
    
    // If true, contentsClippingRect is used to clip child GraphicsLayers.
    bool contentsRectClipsDescendants() const { return m_contentsRectClipsDescendants; }
    virtual void setContentsRectClipsDescendants(bool b) { m_contentsRectClipsDescendants = b; }

    // Set a rounded rect that is used to clip this layer and its descendants (implies setting masksToBounds).
    // Returns false if the platform can't support this rounded clip, and we should fall back to painting a mask.
    FloatRoundedRect maskToBoundsRect() const { return m_masksToBoundsRect; };
    virtual bool setMasksToBoundsRect(const FloatRoundedRect& roundedRect) { m_masksToBoundsRect = roundedRect; return false; }

    Path shapeLayerPath() const;
    virtual void setShapeLayerPath(const Path&);

    WindRule shapeLayerWindRule() const;
    virtual void setShapeLayerWindRule(WindRule);

    const EventRegion& eventRegion() const { return m_eventRegion; }
    virtual void setEventRegion(EventRegion&&);

    // Transitions are identified by a special animation name that cannot clash with a keyframe identifier.
    static String animationNameForTransition(AnimatedPropertyID);

    // Return true if the animation is handled by the compositing system. If this returns
    // false, the animation will be run by CSSAnimationController.
    // These methods handle both transitions and keyframe animations.
    virtual bool addAnimation(const KeyframeValueList&, const FloatSize& /*boxSize*/, const Animation*, const String& /*animationName*/, double /*timeOffset*/)  { return false; }
    virtual void pauseAnimation(const String& /*animationName*/, double /*timeOffset*/) { }
    virtual void removeAnimation(const String& /*animationName*/) { }

    WEBCORE_EXPORT virtual void suspendAnimations(MonotonicTime);
    WEBCORE_EXPORT virtual void resumeAnimations();

    virtual Vector<std::pair<String, double>> acceleratedAnimationsForTesting() const { return { }; }

    // Layer contents
    virtual void setContentsToImage(Image*) { }
    virtual bool shouldDirectlyCompositeImage(Image*) const { return true; }
#if PLATFORM(IOS_FAMILY)
    virtual PlatformLayer* contentsLayerForMedia() const { return 0; }
#endif

    enum class ContentsLayerPurpose : uint8_t {
        None = 0,
        Image,
        Media,
        Canvas,
        BackgroundColor,
        Plugin,
        EmbeddedView
    };

    enum class ContentsLayerEmbeddedViewType : uint8_t {
        None = 0,
        EditableImage,
    };

    using EmbeddedViewID = uint64_t;
    static EmbeddedViewID nextEmbeddedViewID();

    // Pass an invalid color to remove the contents layer.
    virtual void setContentsToSolidColor(const Color&) { }
    virtual void setContentsToEmbeddedView(GraphicsLayer::ContentsLayerEmbeddedViewType, EmbeddedViewID) { }
    virtual void setContentsToPlatformLayer(PlatformLayer*, ContentsLayerPurpose) { }
    virtual bool usesContentsLayer() const { return false; }

    // Callback from the underlying graphics system to draw layer contents.
    void paintGraphicsLayerContents(GraphicsContext&, const FloatRect& clip, GraphicsLayerPaintBehavior = GraphicsLayerPaintNormal);

    // For hosting this GraphicsLayer in a native layer hierarchy.
    virtual PlatformLayer* platformLayer() const { return 0; }

    enum class CompositingCoordinatesOrientation : uint8_t { TopDown, BottomUp };

    // Flippedness of the contents of this layer. Does not affect sublayer geometry.
    virtual void setContentsOrientation(CompositingCoordinatesOrientation orientation) { m_contentsOrientation = orientation; }
    CompositingCoordinatesOrientation contentsOrientation() const { return m_contentsOrientation; }

    void dumpLayer(WTF::TextStream&, LayerTreeAsTextBehavior = LayerTreeAsTextBehaviorNormal) const;

    virtual void setShowDebugBorder(bool show) { m_showDebugBorder = show; }
    bool isShowingDebugBorder() const { return m_showDebugBorder; }

    virtual void setShowRepaintCounter(bool show) { m_showRepaintCounter = show; }
    bool isShowingRepaintCounter() const { return m_showRepaintCounter; }

    // FIXME: this is really a paint count.
    int repaintCount() const { return m_repaintCount; }
    int incrementRepaintCount() { return ++m_repaintCount; }

    virtual void setDebugBackgroundColor(const Color&) { }
    virtual void setDebugBorder(const Color&, float /*borderWidth*/) { }

    enum class CustomAppearance : uint8_t {
        None,
        ScrollingOverhang,
        ScrollingShadow,
        LightBackdrop,
        DarkBackdrop
    };
    virtual void setCustomAppearance(CustomAppearance customAppearance) { m_customAppearance = customAppearance; }
    CustomAppearance customAppearance() const { return m_customAppearance; }

    // z-position is the z-equivalent of position(). It's only used for debugging purposes.
    virtual float zPosition() const { return m_zPosition; }
    WEBCORE_EXPORT virtual void setZPosition(float);

    WEBCORE_EXPORT virtual void distributeOpacity(float);
    WEBCORE_EXPORT virtual float accumulatedOpacity() const;

    virtual FloatSize pixelAlignmentOffset() const { return FloatSize(); }
    
    virtual void setAppliesPageScale(bool appliesScale = true) { m_appliesPageScale = appliesScale; }
    virtual bool appliesPageScale() const { return m_appliesPageScale; }

    float pageScaleFactor() const { return client().pageScaleFactor(); }
    float deviceScaleFactor() const { return client().deviceScaleFactor(); }
    
    // Whether this layer can throw away backing store to save memory. False for layers that can be revealed by async scrolling.
    virtual void setAllowsBackingStoreDetaching(bool) { }
    virtual bool allowsBackingStoreDetaching() const { return true; }

    virtual void deviceOrPageScaleFactorChanged() { }
    WEBCORE_EXPORT void noteDeviceOrPageScaleFactorChangedIncludingDescendants();

    void setIsInWindow(bool);

    // Some compositing systems may do internal batching to synchronize compositing updates
    // with updates drawn into the window. These methods flush internal batched state on this layer
    // and descendant layers, and this layer only.
    virtual void flushCompositingState(const FloatRect& /* clipRect */) { }
    virtual void flushCompositingStateForThisLayerOnly() { }

    // If the exposed rect of this layer changes, returns true if this or descendant layers need a flush,
    // for example to allocate new tiles.
    virtual bool visibleRectChangeRequiresFlush(const FloatRect& /* clipRect */) const { return false; }

    static FloatRect adjustCoverageRectForMovement(const FloatRect& coverageRect, const FloatRect& previousVisibleRect, const FloatRect& currentVisibleRect);

    // Return a string with a human readable form of the layer tree, If debug is true
    // pointers for the layers and timing data will be included in the returned string.
    WEBCORE_EXPORT String layerTreeAsText(LayerTreeAsTextBehavior = LayerTreeAsTextBehaviorNormal) const;

    // For testing.
    virtual String displayListAsText(DisplayList::AsTextFlags) const { return String(); }

    virtual void setIsTrackingDisplayListReplay(bool isTracking) { m_isTrackingDisplayListReplay = isTracking; }
    virtual bool isTrackingDisplayListReplay() const { return m_isTrackingDisplayListReplay; }
    virtual String replayDisplayListAsText(DisplayList::AsTextFlags) const { return String(); }

    // Return an estimate of the backing store memory cost (in bytes). May be incorrect for tiled layers.
    WEBCORE_EXPORT virtual double backingStoreMemoryEstimate() const;

    virtual bool backingStoreAttached() const { return true; }
    virtual bool backingStoreAttachedForTesting() const { return backingStoreAttached(); }

    virtual TiledBacking* tiledBacking() const { return 0; }

    void resetTrackedRepaints();
    void addRepaintRect(const FloatRect&);

    static bool supportsBackgroundColorContent();
    static bool supportsLayerType(Type);
    static bool supportsContentsTiling();
    static bool supportsSubpixelAntialiasedLayerText();

    void updateDebugIndicators();

    virtual bool isGraphicsLayerCA() const { return false; }
    virtual bool isGraphicsLayerCARemote() const { return false; }
    virtual bool isGraphicsLayerTextureMapper() const { return false; }
    virtual bool isCoordinatedGraphicsLayer() const { return false; }

    const Optional<FloatRect>& animationExtent() const { return m_animationExtent; }
    void setAnimationExtent(Optional<FloatRect> animationExtent) { m_animationExtent = animationExtent; }

    static void traverse(GraphicsLayer&, const WTF::Function<void (GraphicsLayer&)>&);

protected:
    WEBCORE_EXPORT explicit GraphicsLayer(Type, GraphicsLayerClient&);

    // Should be called from derived class destructors. Should call willBeDestroyed() on super.
    WEBCORE_EXPORT virtual void willBeDestroyed();
    bool beingDestroyed() const { return m_beingDestroyed; }

    // This method is used by platform GraphicsLayer classes to clear the filters
    // when compositing is not done in hardware. It is not virtual, so the caller
    // needs to notifiy the change to the platform layer as needed.
    void clearFilters() { m_filters.clear(); }
    void clearBackdropFilters() { m_backdropFilters.clear(); }

    // Given a KeyframeValueList containing filterOperations, return true if the operations are valid.
    static int validateFilterOperations(const KeyframeValueList&);

    // Given a list of TransformAnimationValues, see if all the operations for each keyframe match. If so
    // return the index of the KeyframeValueList entry that has that list of operations (it may not be
    // the first entry because some keyframes might have an empty transform and those match any list).
    // If the lists don't match return -1. On return, if hasBigRotation is true, functions contain 
    // rotations of >= 180 degrees
    static int validateTransformOperations(const KeyframeValueList&, bool& hasBigRotation);

    virtual bool shouldRepaintOnSizeChange() const { return drawsContent(); }

    virtual void setOpacityInternal(float) { }

    // The layer being replicated.
    GraphicsLayer* replicatedLayer() const { return m_replicatedLayer; }
    virtual void setReplicatedLayer(GraphicsLayer* layer) { m_replicatedLayer = layer; }

    void dumpProperties(WTF::TextStream&, LayerTreeAsTextBehavior) const;
    virtual void dumpAdditionalProperties(WTF::TextStream&, LayerTreeAsTextBehavior) const { }

    WEBCORE_EXPORT virtual void getDebugBorderInfo(Color&, float& width) const;

    GraphicsLayerClient* m_client; // Always non-null.
    String m_name;
    
    // Offset from the owning renderer
    FloatSize m_offsetFromRenderer;
    
    // Scroll offset of the content layer inside its scrolling parent layer.
    ScrollOffset m_scrollOffset;

    // Position is relative to the parent GraphicsLayer
    FloatPoint m_position;

    // If set, overrides m_position. Only used for coverage computation.
    Optional<FloatPoint> m_approximatePosition;

    FloatPoint3D m_anchorPoint { 0.5f, 0.5f, 0 };
    FloatSize m_size;
    FloatPoint m_boundsOrigin;

    std::unique_ptr<TransformationMatrix> m_transform;
    std::unique_ptr<TransformationMatrix> m_childrenTransform;

    Color m_backgroundColor;
    float m_opacity { 1 };
    float m_zPosition { 0 };
    
    FilterOperations m_filters;
    FilterOperations m_backdropFilters;
    
#if ENABLE(SCROLLING_THREAD)
    ScrollingNodeID m_scrollingNodeID { 0 };
#endif

#if ENABLE(CSS_COMPOSITING)
    BlendMode m_blendMode { BlendMode::Normal };
#endif

    const Type m_type;
    CustomAppearance m_customAppearance { CustomAppearance::None };
    OptionSet<GraphicsLayerPaintingPhase> m_paintingPhase { GraphicsLayerPaintingPhase::Foreground, GraphicsLayerPaintingPhase::Background };
    CompositingCoordinatesOrientation m_contentsOrientation { CompositingCoordinatesOrientation::TopDown }; // affects orientation of layer contents

    bool m_beingDestroyed : 1;
    bool m_contentsOpaque : 1;
    bool m_supportsSubpixelAntialiasedText : 1;
    bool m_preserves3D: 1;
    bool m_backfaceVisibility : 1;
    bool m_masksToBounds : 1;
    bool m_drawsContent : 1;
    bool m_contentsVisible : 1;
    bool m_contentsRectClipsDescendants : 1;
    bool m_acceleratesDrawing : 1;
    bool m_usesDisplayListDrawing : 1;
    bool m_appliesPageScale : 1; // Set for the layer which has the page scale applied to it.
    bool m_showDebugBorder : 1;
    bool m_showRepaintCounter : 1;
    bool m_isMaskLayer : 1;
    bool m_isTrackingDisplayListReplay : 1;
    bool m_userInteractionEnabled : 1;
    bool m_canDetachBackingStore : 1;

    int m_repaintCount { 0 };

    Vector<Ref<GraphicsLayer>> m_children;
    GraphicsLayer* m_parent { nullptr };

    RefPtr<GraphicsLayer> m_maskLayer { nullptr }; // Reference to mask layer.

    RefPtr<GraphicsLayer> m_replicaLayer { nullptr }; // A layer that replicates this layer. We only allow one, for now.
                                   // The replica is not parented; this is the primary reference to it.
    GraphicsLayer* m_replicatedLayer { nullptr }; // For a replica layer, a reference to the original layer.
    FloatPoint m_replicatedLayerPosition; // For a replica layer, the position of the replica.

    FloatRect m_contentsRect;
    FloatRoundedRect m_contentsClippingRect;
    FloatRoundedRect m_masksToBoundsRect;
    FloatSize m_contentsTilePhase;
    FloatSize m_contentsTileSize;
    FloatRoundedRect m_backdropFiltersRect;
    Optional<FloatRect> m_animationExtent;

    EventRegion m_eventRegion;
#if USE(CA)
    WindRule m_shapeLayerWindRule { WindRule::NonZero };
    Path m_shapeLayerPath;
#endif
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const WebCore::GraphicsLayerPaintingPhase);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const Vector<GraphicsLayer::PlatformLayerID>&);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const GraphicsLayer::CustomAppearance&);

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_GRAPHICSLAYER(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(ToValueTypeName) \
    static bool isType(const WebCore::GraphicsLayer& layer) { return layer.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

#if ENABLE(TREE_DEBUGGING)
// Outside the WebCore namespace for ease of invocation from the debugger.
void showGraphicsLayerTree(const WebCore::GraphicsLayer* layer);
#endif

namespace WTF {

template<> struct EnumTraits<WebCore::GraphicsLayer::CustomAppearance> {
    using values = EnumValues<
        WebCore::GraphicsLayer::CustomAppearance,
        WebCore::GraphicsLayer::CustomAppearance::None,
        WebCore::GraphicsLayer::CustomAppearance::ScrollingOverhang,
        WebCore::GraphicsLayer::CustomAppearance::ScrollingShadow,
        WebCore::GraphicsLayer::CustomAppearance::LightBackdrop,
        WebCore::GraphicsLayer::CustomAppearance::DarkBackdrop
    >;
};

} // namespace WTF
