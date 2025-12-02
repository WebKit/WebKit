/*
 * Copyright (C) 2010-2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "GraphicsLayerCA.h"

#if USE(CA)

#include "DisplayList.h"
#include "DisplayListRecorderImpl.h"
#include "FloatConversion.h"
#include "FloatRect.h"
#include "GraphicsLayerAnimation.h"
#include "GraphicsLayerAsyncContentsDisplayDelegateCocoa.h"
#include "GraphicsLayerContentsDisplayDelegate.h"
#include "GraphicsLayerFactory.h"
#include "HTMLVideoElement.h"
#include "HostingContext.h"
#include "Image.h"
#include "Logging.h"
#include "Model.h"
#include "PlatformCAAnimationCocoa.h"
#include "PlatformCAFilters.h"
#include "PlatformCALayer.h"
#include "PlatformCALayerCocoa.h"
#include "PlatformLayer.h"
#include "PlatformScreen.h"
#include "Region.h"
#include "RotateTransformOperation.h"
#include "ScaleTransformOperation.h"
#include "Settings.h"
#include "TiledBacking.h"
#include "TransformOperationsSharedPrimitivesPrefix.h"
#include "TransformState.h"
#include "TranslateTransformOperation.h"
#include <QuartzCore/CATransform3D.h>
#include <limits.h>
#include <pal/spi/cf/CFUtilitiesSPI.h>
#include <ranges>
#include <wtf/CheckedArithmetic.h>
#include <wtf/HexNumber.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/PointerComparison.h>
#include <wtf/SetForScope.h>
#include <wtf/SystemTracing.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/UUID.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/TextStream.h>

#if PLATFORM(IOS_FAMILY)
#include "SystemMemory.h"
#include "WebCoreThread.h"
#endif

#if ENABLE(THREADED_ANIMATIONS)
#include "AcceleratedEffect.h"
#include "AcceleratedEffectStack.h"
#endif

#if ENABLE(MODEL_CONTEXT)
#include "ModelContext.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GraphicsLayerCA);

// The threshold width or height above which a tiled layer will be used. This should be
// large enough to avoid tiled layers for most GraphicsLayers, but less than the OpenGL
// texture size limit on all supported hardware.
#if PLATFORM(IOS_FAMILY)
static const int cMaxPixelDimension = 1280;
static const int cMaxPixelDimensionLowMemory = 1024;
static const int cMemoryLevelToUseSmallerPixelDimension = 35;
#else
static const int cMaxPixelDimension = 2048;
#endif

// Derived empirically: <rdar://problem/13401861>
static const unsigned cMaxLayerTreeDepth = 128;

// About 10 screens of an iPhone 6 Plus. <rdar://problem/44532782>
static const unsigned cMaxTotalBackdropFilterArea = 1242 * 2208 * 10;

// Don't let a single tiled layer use more than 156MB of memory. On a 3x display with RGB10A8 surfaces, this is about 12 tiles.
static const unsigned cMaxScaledTiledLayerMemorySize = 1024 * 1024 * 156;

// If we send a duration of 0 to CA, then it will use the default duration
// of 250ms. So send a very small value instead.
static const float cAnimationAlmostZeroDuration = 1e-3f;

static bool isTransformTypeTransformationMatrix(TransformOperation::Type transformType)
{
    switch (transformType) {
    case TransformOperation::Type::SkewX:
    case TransformOperation::Type::SkewY:
    case TransformOperation::Type::Skew:
    case TransformOperation::Type::Matrix:
    case TransformOperation::Type::Rotate3D:
    case TransformOperation::Type::Matrix3D:
    case TransformOperation::Type::Perspective:
    case TransformOperation::Type::Identity:
    case TransformOperation::Type::None:
        return true;
    default:
        return false;
    }
}

static bool isTransformTypeFloatPoint3D(TransformOperation::Type transformType)
{
    switch (transformType) {
    case TransformOperation::Type::Scale:
    case TransformOperation::Type::Scale3D:
    case TransformOperation::Type::Translate:
    case TransformOperation::Type::Translate3D:
        return true;
    default:
        return false;
    }
}

static bool isTransformTypeNumber(TransformOperation::Type transformType)
{
    return !isTransformTypeTransformationMatrix(transformType) && !isTransformTypeFloatPoint3D(transformType);
}

static void getTransformFunctionValue(const TransformOperation* transformOp, TransformOperation::Type transformType, float& value)
{
    switch (transformType) {
    case TransformOperation::Type::Rotate:
    case TransformOperation::Type::RotateX:
    case TransformOperation::Type::RotateY:
        value = transformOp ? narrowPrecisionToFloat(deg2rad(downcast<RotateTransformOperation>(*transformOp).angle())) : 0;
        break;
    case TransformOperation::Type::ScaleX:
        value = transformOp ? narrowPrecisionToFloat(downcast<ScaleTransformOperation>(*transformOp).x()) : 1;
        break;
    case TransformOperation::Type::ScaleY:
        value = transformOp ? narrowPrecisionToFloat(downcast<ScaleTransformOperation>(*transformOp).y()) : 1;
        break;
    case TransformOperation::Type::ScaleZ:
        value = transformOp ? narrowPrecisionToFloat(downcast<ScaleTransformOperation>(*transformOp).z()) : 1;
        break;
    case TransformOperation::Type::TranslateX:
        value = transformOp ? downcast<TranslateTransformOperation>(*transformOp).x() : 0;
        break;
    case TransformOperation::Type::TranslateY:
        value = transformOp ? downcast<TranslateTransformOperation>(*transformOp).y() : 0;
        break;
    case TransformOperation::Type::TranslateZ:
        value = transformOp ? downcast<TranslateTransformOperation>(*transformOp).z() : 0;
        break;
    default:
        break;
    }
}

static void getTransformFunctionValue(const TransformOperation* transformOp, TransformOperation::Type transformType, FloatPoint3D& value)
{
    switch (transformType) {
    case TransformOperation::Type::Scale:
    case TransformOperation::Type::Scale3D: {
        const auto* scaleTransformOp = downcast<ScaleTransformOperation>(transformOp);
        value.setX(scaleTransformOp ? narrowPrecisionToFloat(scaleTransformOp->x()) : 1);
        value.setY(scaleTransformOp ? narrowPrecisionToFloat(scaleTransformOp->y()) : 1);
        value.setZ(scaleTransformOp ? narrowPrecisionToFloat(scaleTransformOp->z()) : 1);
        break;
    }
    case TransformOperation::Type::Translate:
    case TransformOperation::Type::Translate3D: {
        const auto* translateTransformOp = downcast<TranslateTransformOperation>(transformOp);
        value.setX(translateTransformOp ? translateTransformOp->x() : 0);
        value.setY(translateTransformOp ? translateTransformOp->y() : 0);
        value.setZ(translateTransformOp ? translateTransformOp->z() : 0);
        break;
    }
    default:
        break;
    }
}

static void getTransformFunctionValue(const TransformOperation* transformOp, TransformOperation::Type transformType, TransformationMatrix& value)
{
    switch (transformType) {
    case TransformOperation::Type::SkewX:
    case TransformOperation::Type::SkewY:
    case TransformOperation::Type::Skew:
    case TransformOperation::Type::Matrix:
    case TransformOperation::Type::Rotate3D:
    case TransformOperation::Type::Matrix3D:
    case TransformOperation::Type::Perspective:
    case TransformOperation::Type::Identity:
    case TransformOperation::Type::None:
        if (transformOp)
            transformOp->applyUnrounded(value);
        else
            value.makeIdentity();
        break;
    default:
        break;
    }
}

static PlatformCAAnimation::ValueFunctionType getValueFunctionNameForTransformOperation(TransformOperation::Type transformType)
{
    // Use literal strings to avoid link-time dependency on those symbols.
    switch (transformType) {
    case TransformOperation::Type::RotateX:
        return PlatformCAAnimation::ValueFunctionType::RotateX;
    case TransformOperation::Type::RotateY:
        return PlatformCAAnimation::ValueFunctionType::RotateY;
    case TransformOperation::Type::Rotate:
        return PlatformCAAnimation::ValueFunctionType::RotateZ;
    case TransformOperation::Type::ScaleX:
        return PlatformCAAnimation::ValueFunctionType::ScaleX;
    case TransformOperation::Type::ScaleY:
        return PlatformCAAnimation::ValueFunctionType::ScaleY;
    case TransformOperation::Type::ScaleZ:
        return PlatformCAAnimation::ValueFunctionType::ScaleZ;
    case TransformOperation::Type::TranslateX:
        return PlatformCAAnimation::ValueFunctionType::TranslateX;
    case TransformOperation::Type::TranslateY:
        return PlatformCAAnimation::ValueFunctionType::TranslateY;
    case TransformOperation::Type::TranslateZ:
        return PlatformCAAnimation::ValueFunctionType::TranslateZ;
    case TransformOperation::Type::Scale:
    case TransformOperation::Type::Scale3D:
        return PlatformCAAnimation::ValueFunctionType::Scale;
    case TransformOperation::Type::Translate:
    case TransformOperation::Type::Translate3D:
        return PlatformCAAnimation::ValueFunctionType::Translate;
    default:
        return PlatformCAAnimation::ValueFunctionType::NoValueFunction;
    }
}

static bool animatedPropertyIsTransformOrRelated(AnimatedProperty property)
{
    return property == AnimatedProperty::Transform || property == AnimatedProperty::Translate || property == AnimatedProperty::Scale || property == AnimatedProperty::Rotate;
}

static bool animationHasStepsTimingFunction(const KeyframeValueList& valueList, const GraphicsLayerAnimation* anim)
{
    if (is<StepsTimingFunction>(anim->timingFunction()))
        return true;

    bool hasStepsDefaultTimingFunctionForKeyframes = is<StepsTimingFunction>(anim->defaultTimingFunctionForKeyframes());
    for (unsigned i = 0; i < valueList.size(); ++i) {
        if (RefPtr timingFunction = valueList.at(i).timingFunction()) {
            if (is<StepsTimingFunction>(*timingFunction))
                return true;
        } else if (hasStepsDefaultTimingFunctionForKeyframes)
            return true;
    }

    return false;
}

static inline bool supportsAcceleratedFilterAnimations()
{
    return true;
}

static PlatformCALayer::FilterType toPlatformCALayerFilterType(GraphicsLayer::ScalingFilter filter)
{
    switch (filter) {
    case GraphicsLayer::ScalingFilter::Linear:
        return PlatformCALayer::FilterType::Linear;
    case GraphicsLayer::ScalingFilter::Nearest:
        return PlatformCALayer::FilterType::Nearest;
    case GraphicsLayer::ScalingFilter::Trilinear:
        return PlatformCALayer::FilterType::Trilinear;
    }
    ASSERT_NOT_REACHED();
    return PlatformCALayer::FilterType::Linear;
}

bool GraphicsLayer::supportsLayerType(Type type)
{
    switch (type) {
    case Type::Normal:
    case Type::Structural:
    case Type::PageTiledBacking:
    case Type::ScrollContainer:
    case Type::ScrolledContents:
    case Type::TiledBacking:
        return true;
    case Type::Shape:
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

Ref<GraphicsLayer> GraphicsLayer::create(GraphicsLayerFactory* factory, GraphicsLayerClient& client, Type layerType)
{
    if (factory) {
        auto layer = factory->createGraphicsLayer(layerType, client);
        layer->initialize(layerType);
        return layer;
    }
    
    auto layer = adoptRef(*new GraphicsLayerCA(layerType, client));
    layer->initialize(layerType);
    return WTFMove(layer);
}

bool GraphicsLayerCA::filtersCanBeComposited(const FilterOperations& filters)
{
    return PlatformCALayerCocoa::filtersCanBeComposited(filters);
}

Ref<PlatformCALayer> GraphicsLayerCA::createPlatformCALayer(PlatformCALayer::LayerType layerType, PlatformCALayerClient* owner)
{
    auto result = PlatformCALayerCocoa::create(layerType, owner);

    if (result->canHaveBackingStore()) {
        auto contentsFormat = PlatformCALayer::contentsFormatForLayer(owner);
        result->setContentsFormat(contentsFormat);
    }

    return result;
}
    
Ref<PlatformCALayer> GraphicsLayerCA::createPlatformCALayer(PlatformLayer* platformLayer, PlatformCALayerClient* owner)
{
    return PlatformCALayerCocoa::create(platformLayer, owner);
}

#if ENABLE(MODEL_CONTEXT) && !ENABLE(GPU_PROCESS_MODEL)
Ref<PlatformCALayer> GraphicsLayerCA::createPlatformCALayer(Ref<ModelContext>, PlatformCALayerClient* owner)
{
    ASSERT_NOT_REACHED_WITH_MESSAGE("GraphicsLayerCARemote::createPlatformCALayer should always be called instead of this, but this symbol is needed to compile WebKitLegacy.");
    return GraphicsLayerCA::createPlatformCALayer(PlatformCALayer::LayerType::LayerTypeLayer, owner);
}
#endif

#if ENABLE(MODEL_ELEMENT)
Ref<PlatformCALayer> GraphicsLayerCA::createPlatformCALayer(Ref<WebCore::Model>, PlatformCALayerClient* owner)
{
    // By default, just make a plain layer; subclasses can override to provide a custom PlatformCALayer for Model.
    return GraphicsLayerCA::createPlatformCALayer(PlatformCALayer::LayerType::LayerTypeLayer, owner);
}
#endif

Ref<PlatformCALayer> GraphicsLayerCA::createPlatformCALayerHost(LayerHostingContextIdentifier, PlatformCALayerClient* owner)
{
    ASSERT_NOT_REACHED_WITH_MESSAGE("GraphicsLayerCARemote::createPlatformCALayerHost should always be called instead of this, but this symbol is needed to compile WebKitLegacy.");
    return GraphicsLayerCA::createPlatformCALayer(PlatformCALayer::LayerType::LayerTypeLayer, owner);
}

Ref<PlatformCALayer> GraphicsLayerCA::createPlatformVideoLayer(HTMLVideoElement&, PlatformCALayerClient* owner)
{
    // By default, just make a plain layer; subclasses can override to provide a custom PlatformCALayer for hosting context id.
    return GraphicsLayerCA::createPlatformCALayer(PlatformCALayer::LayerType::LayerTypeLayer, owner);
}

Ref<PlatformCAAnimation> GraphicsLayerCA::createPlatformCAAnimation(PlatformCAAnimation::AnimationType type, const String& keyPath)
{
    return PlatformCAAnimationCocoa::create(type, keyPath);
}

using LayerDisplayListHashMap = HashMap<const GraphicsLayerCA*, std::pair<FloatRect, Ref<const DisplayList::DisplayList>>>;

static LayerDisplayListHashMap& layerDisplayListMap()
{
    static NeverDestroyed<LayerDisplayListHashMap> sharedHashMap;
    return sharedHashMap;
}

GraphicsLayerCA::GraphicsLayerCA(Type layerType, GraphicsLayerClient& client)
    : GraphicsLayer(layerType, client)
    , m_needsFullRepaint(false)
    , m_allowsBackingStoreDetaching(true)
    , m_intersectsCoverageRect(false)
    , m_hasEverPainted(false)
    , m_hasDescendantsWithRunningTransformAnimations(false)
    , m_hasDescendantsWithUncommittedChanges(false)
{
}

void GraphicsLayerCA::initialize(Type layerType)
{
    PlatformCALayer::LayerType platformLayerType;
    switch (layerType) {
    case Type::Normal:
        platformLayerType = PlatformCALayer::LayerType::LayerTypeWebLayer;
        break;
    case Type::Structural:
        platformLayerType = PlatformCALayer::LayerType::LayerTypeTransformLayer;
        break;
    case Type::ScrolledContents:
        platformLayerType = PlatformCALayer::LayerType::LayerTypeWebLayer;
        break;
    case Type::PageTiledBacking:
        platformLayerType = PlatformCALayer::LayerType::LayerTypePageTiledBackingLayer;
        break;
    case Type::ScrollContainer:
        platformLayerType = PlatformCALayer::LayerType::LayerTypeScrollContainerLayer;
        break;
    case Type::Shape:
        platformLayerType = PlatformCALayer::LayerType::LayerTypeShapeLayer;
        break;
    case Type::TiledBacking:
        platformLayerType = PlatformCALayer::LayerType::LayerTypeTiledBackingLayer;
        break;
    }

    m_layer = createPlatformCALayer(platformLayerType, this);

    noteLayerPropertyChanged(ContentsScaleChanged);
    noteLayerPropertyChanged(CoverageRectChanged);
}

GraphicsLayerCA::~GraphicsLayerCA()
{
    if (isTrackingDisplayListReplay()) [[unlikely]]
        layerDisplayListMap().remove(this);

    // We release our references to the PlatformCALayers here, but do not actively unparent them,
    // since that will cause a commit and break our batched commit model. The layers will
    // get released when the rootmost modified GraphicsLayerCA rebuilds its child layers.

    // Clean up the layer.
    if (m_layer)
        m_layer->setOwner(nullptr);
    
    if (m_contentsLayer)
        m_contentsLayer->setOwner(nullptr);

    if (m_contentsClippingLayer)
        m_contentsClippingLayer->setOwner(nullptr);

    if (m_contentsShapeMaskLayer)
        m_contentsShapeMaskLayer->setOwner(nullptr);

    if (m_shapeMaskLayer)
        m_shapeMaskLayer->setOwner(nullptr);
    
    if (m_structuralLayer)
        m_structuralLayer->setOwner(nullptr);

    if (m_backdropLayer)
        m_backdropLayer->setOwner(nullptr);

    if (m_backdropClippingLayer)
        m_backdropClippingLayer->setOwner(nullptr);

    removeCloneLayers();

    if (m_parent)
        downcast<GraphicsLayerCA>(*m_parent).noteSublayersChanged();

    willBeDestroyed();
}

void GraphicsLayerCA::setName(const String& name)
{
    if (name == this->name())
        return;

    GraphicsLayer::setName(name);
    noteLayerPropertyChanged(NameChanged);
}

String GraphicsLayerCA::debugName() const
{
#if ENABLE(TREE_DEBUGGING)
    String caLayerDescription;
    if (m_layer->type() == PlatformCALayer::Type::Cocoa)
        caLayerDescription = makeString("CALayer(0x"_s, hex(reinterpret_cast<uintptr_t>(m_layer->platformLayer()), Lowercase), ") "_s);
    return makeString(caLayerDescription, "GraphicsLayer(0x"_s, hex(reinterpret_cast<uintptr_t>(this), Lowercase), ", "_s, primaryLayerID()->object(), ") "_s, name());
#else
    return name();
#endif
}

std::optional<PlatformLayerIdentifier> GraphicsLayerCA::primaryLayerID() const
{
    return primaryLayer()->layerID();
}

std::optional<PlatformLayerIdentifier> GraphicsLayerCA::layerIDIgnoringStructuralLayer() const
{
    return protectedLayer()->layerID();
}

PlatformLayer* GraphicsLayerCA::platformLayer() const
{
    return primaryLayer()->platformLayer();
}

bool GraphicsLayerCA::setChildren(Vector<Ref<GraphicsLayer>>&& children)
{
    bool childrenChanged = GraphicsLayer::setChildren(WTFMove(children));
    if (childrenChanged)
        noteSublayersChanged();
    
    return childrenChanged;
}

void GraphicsLayerCA::addChild(Ref<GraphicsLayer>&& childLayer)
{
    GraphicsLayer::addChild(WTFMove(childLayer));
    noteSublayersChanged();
}

void GraphicsLayerCA::addChildAtIndex(Ref<GraphicsLayer>&& childLayer, int index)
{
    GraphicsLayer::addChildAtIndex(WTFMove(childLayer), index);
    noteSublayersChanged();
}

void GraphicsLayerCA::addChildBelow(Ref<GraphicsLayer>&& childLayer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildBelow(WTFMove(childLayer), sibling);
    noteSublayersChanged();
}

void GraphicsLayerCA::addChildAbove(Ref<GraphicsLayer>&& childLayer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildAbove(WTFMove(childLayer), sibling);
    noteSublayersChanged();
}

bool GraphicsLayerCA::replaceChild(GraphicsLayer* oldChild, Ref<GraphicsLayer>&& newChild)
{
    if (GraphicsLayer::replaceChild(oldChild, WTFMove(newChild))) {
        noteSublayersChanged();
        return true;
    }
    return false;
}

void GraphicsLayerCA::willModifyChildren()
{
    noteSublayersChanged();
}

void GraphicsLayerCA::setMaskLayer(RefPtr<GraphicsLayer>&& layer)
{
    if (layer == m_maskLayer)
        return;

    GraphicsLayer::setMaskLayer(WTFMove(layer));
    noteLayerPropertyChanged(MaskLayerChanged);

    propagateLayerChangeToReplicas();
    
    if (m_replicatedLayer)
        downcast<GraphicsLayerCA>(*m_replicatedLayer).propagateLayerChangeToReplicas();
}

void GraphicsLayerCA::setReplicatedLayer(GraphicsLayer* layer)
{
    if (layer == m_replicatedLayer)
        return;

    GraphicsLayer::setReplicatedLayer(layer);
    noteLayerPropertyChanged(ReplicatedLayerChanged | ChildrenChanged);
}

void GraphicsLayerCA::setReplicatedByLayer(RefPtr<GraphicsLayer>&& layer)
{
    if (layer == m_replicaLayer)
        return;

    GraphicsLayer::setReplicatedByLayer(WTFMove(layer));
    noteSublayersChanged();
    noteLayerPropertyChanged(ReplicatedLayerChanged);
}

void GraphicsLayerCA::setPosition(const FloatPoint& point)
{
    if (point == m_position)
        return;

    GraphicsLayer::setPosition(point);
    noteLayerPropertyChanged(GeometryChanged);
}

void GraphicsLayerCA::syncPosition(const FloatPoint& point)
{
    if (point == m_position && !m_approximatePosition)
        return;

    GraphicsLayer::syncPosition(point);
    // Ensure future flushes will recompute the coverage rect and update tiling.
    noteLayerPropertyChanged(NeedsComputeVisibleAndCoverageRect, DontScheduleFlush);
}

void GraphicsLayerCA::setApproximatePosition(const FloatPoint& point)
{
    if (point == m_approximatePosition)
        return;

    GraphicsLayer::setApproximatePosition(point);
    // Ensure future flushes will recompute the coverage rect and update tiling.
    noteLayerPropertyChanged(NeedsComputeVisibleAndCoverageRect, DontScheduleFlush);
}

void GraphicsLayerCA::setAnchorPoint(const FloatPoint3D& point)
{
    if (point == m_anchorPoint)
        return;

    GraphicsLayer::setAnchorPoint(point);
    noteLayerPropertyChanged(GeometryChanged);
}

void GraphicsLayerCA::setSize(const FloatSize& size)
{
    if (size == m_size)
        return;

    GraphicsLayer::setSize(size);
    noteLayerPropertyChanged(GeometryChanged);
}

void GraphicsLayerCA::setBoundsOrigin(const FloatPoint& origin)
{
    if (origin == m_boundsOrigin)
        return;

    GraphicsLayer::setBoundsOrigin(origin);
    noteLayerPropertyChanged(GeometryChanged);
}

void GraphicsLayerCA::syncBoundsOrigin(const FloatPoint& origin)
{
    if (origin == m_boundsOrigin)
        return;

    GraphicsLayer::syncBoundsOrigin(origin);
    noteLayerPropertyChanged(NeedsComputeVisibleAndCoverageRect, DontScheduleFlush);
}

void GraphicsLayerCA::setTransform(const TransformationMatrix& t)
{
    if (t == transform())
        return;

    GraphicsLayer::setTransform(t);
    noteLayerPropertyChanged(TransformChanged);
}

void GraphicsLayerCA::setChildrenTransform(const TransformationMatrix& t)
{
    if (t == childrenTransform())
        return;

    GraphicsLayer::setChildrenTransform(t);
    noteLayerPropertyChanged(ChildrenTransformChanged);
}

void GraphicsLayerCA::moveOrCopyLayerAnimation(MoveOrCopy operation, const String& animationIdentifier, std::optional<Seconds> beginTime, PlatformCALayer *fromLayer, PlatformCALayer *toLayer)
{
    RefPtr<PlatformCAAnimation> anim = fromLayer->animationForKey(animationIdentifier);
    if (!anim)
        return;

    if (beginTime && beginTime->seconds() != anim->beginTime())
        anim->setBeginTime(beginTime->seconds());

    switch (operation) {
    case Move:
        fromLayer->removeAnimationForKey(animationIdentifier);
        toLayer->addAnimationForKey(animationIdentifier, *anim);
        break;

    case Copy:
        toLayer->addAnimationForKey(animationIdentifier, *anim);
        break;
    }
}

void GraphicsLayerCA::moveOrCopyAnimations(MoveOrCopy operation, PlatformCALayer *fromLayer, PlatformCALayer *toLayer)
{
#if ENABLE(THREADED_ANIMATIONS)
    if (RefPtr effectsStack = acceleratedEffectStack()) {
        auto isBackdropLayer = [&] {
            SUPPRESS_UNRETAINED_LOCAL if (auto* platformLayer = fromLayer->platformLayer())
                SUPPRESS_UNRETAINED_ARG return PlatformCALayerCocoa::layerTypeForPlatformLayer(platformLayer) == PlatformCALayerLayerType::LayerTypeBackdropLayer;
            return false;
        };
        auto& effects = isBackdropLayer() ? effectsStack->backdropLayerEffects() : effectsStack->primaryLayerEffects();
        if (operation == Move)
            fromLayer->clearAcceleratedEffectsAndBaseValues();
        toLayer->setAcceleratedEffectsAndBaseValues(effects, effectsStack->baseValues());
        return;
    }
#endif
    for (auto& animationGroup : m_animationGroups) {
        if ((animatedPropertyIsTransformOrRelated(animationGroup.m_property)
            || animationGroup.m_property == AnimatedProperty::Opacity
            || animationGroup.m_property == AnimatedProperty::BackgroundColor
            || animationGroup.m_property == AnimatedProperty::Filter))
            moveOrCopyLayerAnimation(operation, animationGroup.animationIdentifier(), animationGroup.computedBeginTime(), fromLayer, toLayer);
    }
}

void GraphicsLayerCA::setPreserves3D(bool preserves3D)
{
    if (preserves3D == m_preserves3D)
        return;

    GraphicsLayer::setPreserves3D(preserves3D);
    noteLayerPropertyChanged(Preserves3DChanged);
}

void GraphicsLayerCA::setMasksToBounds(bool masksToBounds)
{
    if (masksToBounds == m_masksToBounds)
        return;

    GraphicsLayer::setMasksToBounds(masksToBounds);
    noteLayerPropertyChanged(MasksToBoundsChanged | DebugIndicatorsChanged);
}

void GraphicsLayerCA::setDrawsContent(bool drawsContent)
{
    if (drawsContent == m_drawsContent)
        return;

    GraphicsLayer::setDrawsContent(drawsContent);
    noteLayerPropertyChanged(DrawsContentChanged | DebugIndicatorsChanged);
}

#if HAVE(SUPPORT_HDR_DISPLAY)
void GraphicsLayerCA::setDrawsHDRContent(bool drawsHDRContent)
{
    if (drawsHDRContent == m_drawsHDRContent)
        return;

    GraphicsLayer::setDrawsHDRContent(drawsHDRContent);
    noteLayerPropertyChanged(DrawsHDRContentChanged | DebugIndicatorsChanged);
}

void GraphicsLayerCA::setTonemappingEnabled(bool tonemappingEnabled)
{
    if (tonemappingEnabled == m_tonemappingEnabled)
        return;

    GraphicsLayer::setTonemappingEnabled(tonemappingEnabled);
    noteLayerPropertyChanged(TonemappingEnabledChanged);
}

void GraphicsLayerCA::setNeedsDisplayIfEDRHeadroomExceeds(float headroom)
{
    if (protectedLayer()->setNeedsDisplayIfEDRHeadroomExceeds(headroom)) {
        if (!!m_uncommittedChanges)
            client().notifyFlushRequired(this);
    }
}
#endif

void GraphicsLayerCA::setContentsVisible(bool contentsVisible)
{
    if (contentsVisible == m_contentsVisible)
        return;

    GraphicsLayer::setContentsVisible(contentsVisible);
    noteLayerPropertyChanged(ContentsVisibilityChanged);

    // Visibility affects whether the contentsLayer is parented.
    if (m_contentsLayer || m_contentsClippingLayer)
        noteSublayersChanged();
}

void GraphicsLayerCA::setUserInteractionEnabled(bool userInteractionEnabled)
{
    if (userInteractionEnabled == m_userInteractionEnabled)
        return;
    
    GraphicsLayer::setUserInteractionEnabled(userInteractionEnabled);
    noteLayerPropertyChanged(UserInteractionEnabledChanged);
}

void GraphicsLayerCA::setAcceleratesDrawing(bool acceleratesDrawing)
{
    if (acceleratesDrawing == m_acceleratesDrawing)
        return;

    GraphicsLayer::setAcceleratesDrawing(acceleratesDrawing);
    noteLayerPropertyChanged(AcceleratesDrawingChanged);
}

void GraphicsLayerCA::setUsesDisplayListDrawing(bool usesDisplayListDrawing)
{
    if (usesDisplayListDrawing == m_usesDisplayListDrawing)
        return;

    setNeedsDisplay();
    GraphicsLayer::setUsesDisplayListDrawing(usesDisplayListDrawing);
}

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
void GraphicsLayerCA::setIsSeparated(bool isSeparated)
{
    if (isSeparated == m_isSeparated)
        return;

    GraphicsLayer::setIsSeparated(isSeparated);
    noteLayerPropertyChanged(SeparatedChanged);
}

void GraphicsLayerCA::setIsSeparatedImage(bool isSeparatedImage)
{
    if (isSeparatedImage == m_isSeparatedImage)
        return;

    GraphicsLayer::setIsSeparatedImage(isSeparatedImage);
    // Impacts layer type not properties.
    noteLayerPropertyChanged(SeparatedChanged);
}

#if HAVE(CORE_ANIMATION_SEPARATED_PORTALS)
void GraphicsLayerCA::setIsSeparatedPortal(bool isSeparatedPortal)
{
    if (isSeparatedPortal == m_isSeparatedPortal)
        return;

    GraphicsLayer::setIsSeparatedPortal(isSeparatedPortal);
    noteLayerPropertyChanged(SeparatedPortalChanged);

}

void GraphicsLayerCA::setIsDescendentOfSeparatedPortal(bool isDescendentOfSeparatedPortal)
{
    if (isDescendentOfSeparatedPortal == m_isDescendentOfSeparatedPortal)
        return;

    GraphicsLayer::setIsDescendentOfSeparatedPortal(isDescendentOfSeparatedPortal);
    noteLayerPropertyChanged(DescendentOfSeparatedPortalChanged);
}
#endif
#endif

#if HAVE(CORE_MATERIAL)
void GraphicsLayerCA::setAppleVisualEffectData(AppleVisualEffectData effectData)
{
    if (effectData == m_appleVisualEffectData)
        return;

    bool backdropFiltersChanged = appleVisualEffectNeedsBackdrop(effectData.effect) != appleVisualEffectNeedsBackdrop(m_appleVisualEffectData.effect);

    GraphicsLayer::setAppleVisualEffectData(effectData);

    LayerChangeFlags changes = AppleVisualEffectChanged;
    if (backdropFiltersChanged)
        changes |= BackdropFiltersChanged;

    noteLayerPropertyChanged(changes);
}
#endif

void GraphicsLayerCA::setBackgroundColor(const Color& color)
{
    if (m_backgroundColor == color)
        return;

    GraphicsLayer::setBackgroundColor(color);
    noteLayerPropertyChanged(BackgroundColorChanged);
}

void GraphicsLayerCA::setContentsOpaque(bool opaque)
{
    if (m_contentsOpaque == opaque)
        return;

    GraphicsLayer::setContentsOpaque(opaque);
    noteLayerPropertyChanged(ContentsOpaqueChanged);
}

void GraphicsLayerCA::setBackfaceVisibility(bool visible)
{
    if (m_backfaceVisibility == visible)
        return;
    
    GraphicsLayer::setBackfaceVisibility(visible);
    noteLayerPropertyChanged(BackfaceVisibilityChanged);
}

void GraphicsLayerCA::setOpacity(float opacity)
{
    float clampedOpacity = std::max(0.0f, std::min(opacity, 1.0f));

    if (clampedOpacity == m_opacity)
        return;

    GraphicsLayer::setOpacity(clampedOpacity);
    noteLayerPropertyChanged(OpacityChanged);
}

bool GraphicsLayerCA::setFilters(const FilterOperations& filterOperations)
{
    bool canCompositeFilters = filtersCanBeComposited(filterOperations);

    if (m_filters == filterOperations)
        return canCompositeFilters;

    // Filters cause flattening, so we should never have filters on a layer with preserves3D().
    ASSERT(!filterOperations.size() || !preserves3D());

    if (canCompositeFilters) {
        GraphicsLayer::setFilters(filterOperations);
        noteLayerPropertyChanged(FiltersChanged);
    } else if (filters().size()) {
        // In this case filters are rendered in software, so we need to remove any 
        // previously attached hardware filters.
        clearFilters();
        noteLayerPropertyChanged(FiltersChanged);
    }
    return canCompositeFilters;
}

bool GraphicsLayerCA::setBackdropFilters(const FilterOperations& filterOperations)
{
    bool canCompositeFilters = filtersCanBeComposited(filterOperations);

    if (m_backdropFilters == filterOperations)
        return canCompositeFilters;

    // Filters cause flattening, so we should never have filters on a layer with preserves3D().
    ASSERT(!filterOperations.size() || !preserves3D());

    if (canCompositeFilters)
        GraphicsLayer::setBackdropFilters(filterOperations);
    else {
        // FIXME: This would clear the backdrop filters if we had a software implementation.
        clearBackdropFilters();
    }

    noteLayerPropertyChanged(BackdropFiltersChanged | DebugIndicatorsChanged);
    return canCompositeFilters;
}

void GraphicsLayerCA::setIsBackdropRoot(bool isBackdropRoot)
{
    GraphicsLayer::setIsBackdropRoot(isBackdropRoot);
    noteLayerPropertyChanged(BackdropRootChanged);
}

void GraphicsLayerCA::setBackdropFiltersRect(const FloatRoundedRect& backdropFiltersRect)
{
    if (backdropFiltersRect == m_backdropFiltersRect)
        return;

    GraphicsLayer::setBackdropFiltersRect(backdropFiltersRect);
    noteLayerPropertyChanged(BackdropFiltersRectChanged);
}

void GraphicsLayerCA::setBlendMode(BlendMode blendMode)
{
    if (GraphicsLayer::blendMode() == blendMode)
        return;

    GraphicsLayer::setBlendMode(blendMode);
    noteLayerPropertyChanged(BlendModeChanged);
}

bool GraphicsLayerCA::backingStoreAttached() const
{
    return protectedLayer()->backingStoreAttached();
}

bool GraphicsLayerCA::backingStoreAttachedForTesting() const
{
    Ref layer = *m_layer;
    return layer->backingStoreAttached() || layer->hasContents();
}

void GraphicsLayerCA::setNeedsDisplay()
{
    if (!drawsContent())
        return;

    m_needsFullRepaint = true;
    m_dirtyRects.clear();
    noteLayerPropertyChanged(DirtyRectsChanged);
    addRepaintRect(FloatRect(FloatPoint(), m_size));
}

void GraphicsLayerCA::setNeedsDisplayInRect(const FloatRect& r, ShouldClipToLayer shouldClip)
{
    if (!drawsContent())
        return;

    if (m_needsFullRepaint)
        return;

    FloatRect rect(r);
    if (shouldClip == ShouldClipToLayer::Clip) {
        FloatRect layerBounds(FloatPoint(), m_size);
        rect.intersect(layerBounds);
    }

    if (rect.isEmpty())
        return;

    addRepaintRect(rect);

    const size_t maxDirtyRects = 32;

    for (size_t i = 0; i < m_dirtyRects.size(); ++i) {
        if (m_dirtyRects[i].contains(rect))
            return;
    }

    if (m_dirtyRects.size() < maxDirtyRects)
        m_dirtyRects.append(rect);
    else
        m_dirtyRects[0].unite(rect);

    noteLayerPropertyChanged(DirtyRectsChanged);
}

void GraphicsLayerCA::setContentsNeedsDisplay()
{
    noteLayerPropertyChanged(ContentsNeedsDisplay);
}

void GraphicsLayerCA::setContentsRect(const FloatRect& rect)
{
    if (rect == m_contentsRect)
        return;

    GraphicsLayer::setContentsRect(rect);
    noteLayerPropertyChanged(ContentsRectsChanged);
}

void GraphicsLayerCA::setContentsClippingRect(const FloatRoundedRect& rect)
{
    if (rect == m_contentsClippingRect)
        return;

    GraphicsLayer::setContentsClippingRect(rect);
    noteLayerPropertyChanged(ContentsRectsChanged);
}

void GraphicsLayerCA::setContentsRectClipsDescendants(bool contentsRectClipsDescendants)
{
    if (contentsRectClipsDescendants == m_contentsRectClipsDescendants)
        return;

    GraphicsLayer::setContentsRectClipsDescendants(contentsRectClipsDescendants);
    noteLayerPropertyChanged(ChildrenChanged | ContentsRectsChanged);
}

void GraphicsLayerCA::setVideoGravity(MediaPlayerVideoGravity gravity)
{
    if (gravity == m_videoGravity)
        return;

    GraphicsLayer::setVideoGravity(gravity);
    noteLayerPropertyChanged(VideoGravityChanged);
}

void GraphicsLayerCA::setShapeLayerPath(const Path& path)
{
    // FIXME: need to check for path equality. No bool Path::operator==(const Path&)!.
    GraphicsLayer::setShapeLayerPath(path);
    noteLayerPropertyChanged(ShapeChanged);
}

void GraphicsLayerCA::setShapeLayerWindRule(WindRule windRule)
{
    if (windRule == m_shapeLayerWindRule)
        return;

    GraphicsLayer::setShapeLayerWindRule(windRule);
    noteLayerPropertyChanged(WindRuleChanged);
}

void GraphicsLayerCA::setEventRegion(EventRegion&& eventRegion)
{
    if (eventRegion == m_eventRegion)
        return;

    GraphicsLayer::setEventRegion(WTFMove(eventRegion));
    noteLayerPropertyChanged(EventRegionChanged, m_isCommittingChanges ? DontScheduleFlush : ScheduleFlush);
}

#if ENABLE(SCROLLING_THREAD)
void GraphicsLayerCA::setScrollingNodeID(std::optional<ScrollingNodeID> nodeID)
{
    if (nodeID == m_scrollingNodeID)
        return;

    GraphicsLayer::setScrollingNodeID(nodeID);
    noteLayerPropertyChanged(ScrollingNodeChanged);
}
#endif

bool GraphicsLayerCA::shouldRepaintOnSizeChange() const
{
    return drawsContent() && !tiledBacking();
}

bool GraphicsLayerCA::animationIsRunning(const String& animationName) const
{
    auto index = m_animations.findIf([&](LayerPropertyAnimation animation) {
        return animation.m_name == animationName;
    });
    return index != notFound && m_animations[index].m_playState == PlayState::Playing;
}

static bool timingFunctionIsCubicTimingFunctionWithYValueOutOfRange(const TimingFunction* timingFunction)
{
    auto* cubicBezierTimingFunction = dynamicDowncast<CubicBezierTimingFunction>(timingFunction);
    if (!cubicBezierTimingFunction)
        return false;

    auto yValueIsOutOfRange = [](double y) -> bool {
        return y < 0 || y > 1;
    };

    return yValueIsOutOfRange(cubicBezierTimingFunction->y1()) || yValueIsOutOfRange(cubicBezierTimingFunction->y2());
}

static bool keyframeValueListHasSingleIntervalWithLinearOrEquivalentTimingFunction(const KeyframeValueList& valueList)
{
    if (valueList.size() != 2)
        return false;

    RefPtr timingFunction = valueList.at(0).timingFunction();
    if (!timingFunction)
        return true;

    if (is<LinearTimingFunction>(*timingFunction)) {
        ASSERT(LinearTimingFunction::identity() == *timingFunction);
        return true;
    }

    RefPtr cubicBezierTimingFunction = dynamicDowncast<CubicBezierTimingFunction>(*timingFunction);
    return cubicBezierTimingFunction && cubicBezierTimingFunction->isLinear();
}

static bool animationCanBeAccelerated(const KeyframeValueList& valueList, const GraphicsLayerAnimation* anim)
{
    if (anim->playbackRate() != 1 || !anim->directionIsForwards())
        return false;

    if (!anim || anim->isZeroDuration() || valueList.size() < 2)
        return false;

    if (animationHasStepsTimingFunction(valueList, anim))
        return false;

    return true;
}

bool GraphicsLayerCA::addAnimation(const KeyframeValueList& valueList, const GraphicsLayerAnimation* anim, const String& animationName, double timeOffset)
{
    LOG_WITH_STREAM(Animations, stream << "GraphicsLayerCA " << this << " id " << primaryLayerID() << " addAnimation " << anim << " " << animationName << " duration " << anim->duration() << " (can be accelerated " << animationCanBeAccelerated(valueList, anim) << ")");

    ASSERT(!animationName.isEmpty());

    if (!animationCanBeAccelerated(valueList, anim))
        return false;

    bool keyframesShouldUseAnimationWideTimingFunction = false;
    // Core Animation clips values outside of the [0-1] range for animation-wide cubic timing functions.
    if (timingFunctionIsCubicTimingFunctionWithYValueOutOfRange(anim->timingFunction().get())) {
        if (!keyframeValueListHasSingleIntervalWithLinearOrEquivalentTimingFunction(valueList))
            return false;
        keyframesShouldUseAnimationWideTimingFunction = true;
    }

    bool createdAnimations = false;
    if (animatedPropertyIsTransformOrRelated(valueList.property()))
        createdAnimations = createTransformAnimationsFromKeyframes(valueList, anim, animationName, Seconds { timeOffset }, keyframesShouldUseAnimationWideTimingFunction);
    else if (valueList.property() == AnimatedProperty::Filter) {
        if (supportsAcceleratedFilterAnimations())
            createdAnimations = createFilterAnimationsFromKeyframes(valueList, anim, animationName, Seconds { timeOffset }, keyframesShouldUseAnimationWideTimingFunction);
    }
    else if (valueList.property() == AnimatedProperty::WebkitBackdropFilter) {
        if (supportsAcceleratedFilterAnimations())
            createdAnimations = createFilterAnimationsFromKeyframes(valueList, anim, animationName, Seconds { timeOffset }, keyframesShouldUseAnimationWideTimingFunction);
    }
    else
        createdAnimations = createAnimationFromKeyframes(valueList, anim, animationName, Seconds { timeOffset }, keyframesShouldUseAnimationWideTimingFunction);

    if (createdAnimations)
        noteLayerPropertyChanged(AnimationChanged | CoverageRectChanged);

    return createdAnimations;
}

void GraphicsLayerCA::pauseAnimation(const String& animationName, double timeOffset)
{
    LOG_WITH_STREAM(Animations, stream << "GraphicsLayerCA " << this << " id " << primaryLayerID() << " pauseAnimation " << animationName << " (is running " << animationIsRunning(animationName) << ")");

    for (auto& animation : m_animations) {
        // There may be several animations with the same name in the case of transform animations
        // animating multiple components as individual animations.
        if (animation.m_name == animationName && !animation.m_pendingRemoval) {
            animation.m_playState = PlayState::PausePending;
            animation.m_timeOffset = Seconds { timeOffset };

            noteLayerPropertyChanged(AnimationChanged);
        }
    }
}

void GraphicsLayerCA::removeAnimation(const String& animationName, std::optional<AnimatedProperty> property)
{
    LOG_WITH_STREAM(Animations, stream << "GraphicsLayerCA " << this << " id " << primaryLayerID() << " removeAnimation " << animationName << " (is running " << animationIsRunning(animationName) << ")");

    for (auto& animation : m_animations) {
        // There may be several animations with the same name in the case of transform animations
        // animating multiple components as individual animations.
        if (animation.m_name == animationName && !animation.m_pendingRemoval) {
            // If a specific property is provided, we must check we only remove the animations
            // for this specific property.
            if (property && animation.m_property != *property)
                continue;
            animation.m_pendingRemoval = true;
            noteLayerPropertyChanged(AnimationChanged | CoverageRectChanged);
        }
    }
}

void GraphicsLayerCA::transformRelatedPropertyDidChange()
{
    // If we are currently running a transform-related animation, a change in underlying
    // transform value means we must re-evaluate all transform-related animations to ensure
    // that the base value transform animations are current.
    if (isRunningTransformAnimation())
        noteLayerPropertyChanged(AnimationChanged | CoverageRectChanged);
}

void GraphicsLayerCA::platformCALayerAnimationStarted(const String& animationKey, MonotonicTime startTime)
{
    LOG_WITH_STREAM(Animations, stream << "GraphicsLayerCA " << this << " id " << primaryLayerID() << " platformCALayerAnimationStarted " << animationKey);

    auto index = m_animations.findIf([&](LayerPropertyAnimation animation) {
        return animation.animationIdentifier() == animationKey && !animation.m_beginTime;
    });

    if (index != notFound)
        m_animations[index].m_beginTime = startTime.secondsSinceEpoch();

    client().notifyAnimationStarted(this, animationKey, startTime);
}

void GraphicsLayerCA::platformCALayerAnimationEnded(const String& animationKey)
{
    LOG_WITH_STREAM(Animations, stream << "GraphicsLayerCA " << this << " id " << primaryLayerID() << " platformCALayerAnimationEnded " << animationKey);
    client().notifyAnimationEnded(this, animationKey);
}

void GraphicsLayerCA::setContentsToSolidColor(const Color& color)
{
    if (color == m_contentsSolidColor)
        return;

    m_contentsSolidColor = color;
    
    bool contentsLayerChanged = false;

    if (m_contentsSolidColor.isVisible()) {
        if (!m_contentsLayer || m_contentsLayerPurpose != ContentsLayerPurpose::BackgroundColor) {
            m_contentsLayerPurpose = ContentsLayerPurpose::BackgroundColor;
            Ref contentsLayer = createPlatformCALayer(PlatformCALayer::LayerType::LayerTypeLayer, this);
            m_contentsLayer = contentsLayer.copyRef();
#if ENABLE(TREE_DEBUGGING)
            contentsLayer->setName(makeString("contents color "_s, contentsLayer->layerID().object()));
#else
            contentsLayer->setName(MAKE_STATIC_STRING_IMPL("contents color"));
#endif
            contentsLayerChanged = true;
        }
    } else {
        contentsLayerChanged = std::exchange(m_contentsLayer, nullptr);
        m_contentsLayerPurpose = ContentsLayerPurpose::None;
    }
    m_contentsDisplayDelegate = nullptr;

    if (contentsLayerChanged)
        noteSublayersChanged();

    noteLayerPropertyChanged(ContentsColorLayerChanged);
}

void GraphicsLayerCA::setContentsToImage(Image* image)
{
    if (image) {
        auto newImage = image->currentNativeImage();
        if (!newImage)
            return;

        if (m_pendingContentsImage == newImage)
            return;

        m_pendingContentsImage = WTFMove(newImage);
        m_contentsLayerPurpose = ContentsLayerPurpose::Image;
        if (!m_contentsLayer)
            noteSublayersChanged();
    } else {
        m_pendingContentsImage = nullptr;
        m_contentsLayerPurpose = ContentsLayerPurpose::None;
        if (m_contentsLayer)
            noteSublayersChanged();
    }
    m_contentsDisplayDelegate = nullptr;
    m_pendingContentsImageBuffer = nullptr;

    noteLayerPropertyChanged(ContentsImageChanged);
}

void GraphicsLayerCA::setContentsToImageBuffer(ImageBuffer* image)
{
    if (image) {
        if (m_pendingContentsImageBuffer == image)
            return;

        m_pendingContentsImageBuffer = image;

        m_contentsLayerPurpose = ContentsLayerPurpose::Image;
        if (!m_contentsLayer)
            noteSublayersChanged();
    } else {
        m_pendingContentsImageBuffer = nullptr;
        m_contentsLayerPurpose = ContentsLayerPurpose::None;
        if (m_contentsLayer)
            noteSublayersChanged();
    }
    m_contentsDisplayDelegate = nullptr;
    m_pendingContentsImage = nullptr;

    noteLayerPropertyChanged(ContentsImageChanged);
}

#if ENABLE(MODEL_ELEMENT)
void GraphicsLayerCA::setContentsToModel(RefPtr<Model>&& model, ModelInteraction interactive)
{
    if (model == m_contentsModel)
        return;

    m_contentsModel = model;

    bool contentsLayerChanged = false;

    if (model) {
        Ref contentsLayer = createPlatformCALayer(*model, this);
        m_contentsLayer = contentsLayer.copyRef();
#if ENABLE(TREE_DEBUGGING)
        contentsLayer->setName(makeString("contents model "_s, contentsLayer->layerID().object()));
#else
        contentsLayer->setName(MAKE_STATIC_STRING_IMPL("contents model"));
#endif

        contentsLayer->setUserInteractionEnabled(interactive == ModelInteraction::Enabled);
        contentsLayer->setAnchorPoint({ });
        m_contentsLayerPurpose = ContentsLayerPurpose::Model;
        contentsLayerChanged = true;
    } else {
        contentsLayerChanged = std::exchange(m_contentsLayer, nullptr);
        m_contentsLayerPurpose = ContentsLayerPurpose::None;
    }
    m_contentsDisplayDelegate = nullptr;

    if (contentsLayerChanged)
        noteSublayersChanged();

    noteLayerPropertyChanged(ContentsRectsChanged | OpacityChanged);
}

std::optional<PlatformLayerIdentifier> GraphicsLayerCA::contentsLayerIDForModel() const
{
    return m_contentsLayerPurpose == ContentsLayerPurpose::Model ? std::optional { m_contentsLayer->layerID() } : std::nullopt;
}

#endif

void GraphicsLayerCA::setContentsToPlatformLayer(PlatformLayer* platformLayer, ContentsLayerPurpose purpose)
{
    if (m_contentsLayer && platformLayer == m_contentsLayer->platformLayer())
        return;

    // FIXME: The passed in layer might be a raw layer or an externally created
    // PlatformCALayer. To determine this we attempt to get the
    // PlatformCALayer pointer. If this returns a null pointer we assume it's
    // raw. This test might be invalid if the raw layer is, for instance, the
    // PlatformCALayer is using a user data pointer in the raw layer, and
    // the creator of the raw layer is using it for some other purpose.
    // For now we don't support such a case.
    if (platformLayer) {
        if (RefPtr platformCALayer = PlatformCALayer::platformCALayerForLayer(platformLayer))
            m_contentsLayer = WTFMove(platformCALayer);
        else
            m_contentsLayer = createPlatformCALayer(platformLayer, this);
        Ref { *m_contentsLayer }->setBackingStoreAttached(false);
    } else
        m_contentsLayer = nullptr;

    m_contentsLayerPurpose = platformLayer ? purpose : ContentsLayerPurpose::None;
    m_contentsDisplayDelegate = nullptr;
    noteSublayersChanged();
    noteLayerPropertyChanged(ContentsPlatformLayerChanged);
}

void GraphicsLayerCA::setContentsToPlatformLayerHost(LayerHostingContextIdentifier identifier)
{
    if (m_contentsLayer && m_contentsLayer->hostingContextIdentifier() == identifier)
        return;

    m_contentsLayer = createPlatformCALayerHost(identifier, this);
    m_contentsLayerPurpose = GraphicsLayer::ContentsLayerPurpose::Host;
    m_contentsDisplayDelegate = nullptr;
    noteSublayersChanged();
    noteLayerPropertyChanged(ContentsPlatformLayerChanged);
}

#if ENABLE(MODEL_CONTEXT) && !ENABLE(GPU_PROCESS_MODEL)
void GraphicsLayerCA::setContentsToModelContext(Ref<ModelContext> modelContext, ContentsLayerPurpose purpose)
{
    if (m_contentsLayer && m_contentsLayer->hostingContextIdentifier() == modelContext->modelContentsLayerHostingContextIdentifier())
        return;

    m_contentsLayer = createPlatformCALayer(modelContext, this);
    m_contentsLayerPurpose = purpose;
    m_contentsDisplayDelegate = nullptr;
    noteSublayersChanged();
    noteLayerPropertyChanged(ContentsPlatformLayerChanged);
}
#endif

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
void GraphicsLayerCA::removeModelContents()
{
    if (!m_contentsLayer)
        return;

    m_contentsLayer = nullptr;
    noteSublayersChanged();
    noteLayerPropertyChanged(ContentsPlatformLayerChanged);
}
#endif

void GraphicsLayerCA::setContentsToVideoElement(HTMLVideoElement& videoElement, ContentsLayerPurpose purpose)
{
#if HAVE(AVKIT)
    if (auto hostingContextID = videoElement.layerHostingContext().contextID) {
        if (m_contentsLayer && !m_contentsDisplayDelegate
            && m_layerHostingContextID == hostingContextID
            && m_contentsLayerPurpose == purpose) {
                return;
        }

        m_contentsLayer = createPlatformVideoLayer(videoElement, this);
        m_layerHostingContextID = hostingContextID;
        m_contentsLayerPurpose = purpose;
        m_contentsDisplayDelegate = nullptr;
        updateVideoGravity();
        noteSublayersChanged();
        noteLayerPropertyChanged(ContentsPlatformLayerChanged);
        return;
    }
#endif
    SUPPRESS_FORWARD_DECL_ARG setContentsToPlatformLayer(videoElement.platformLayer(), purpose);
}

void GraphicsLayerCA::setContentsDisplayDelegate(RefPtr<GraphicsLayerContentsDisplayDelegate>&& delegate, ContentsLayerPurpose purpose)
{
    if (m_contentsLayer && delegate == m_contentsDisplayDelegate)
        return;

    if (m_contentsLayer)
        m_contentsLayer->setOwner(nullptr);
    m_contentsLayer = nullptr;
    m_contentsDisplayDelegate = nullptr;
    m_contentsLayerPurpose = ContentsLayerPurpose::None;
    if (delegate) {
        Ref contentsLayer = createPlatformCALayer(PlatformCALayer::LayerType::LayerTypeContentsProvidedLayer, this);
        m_contentsLayer = contentsLayer.copyRef();
        m_contentsDisplayDelegate = delegate.copyRef();
        m_contentsLayerPurpose = purpose;
        // Currently delegated display is only useful when delegatee calls setContents, so set the
        // backing store settings accordingly.
        contentsLayer->setBackingStoreAttached(true);
        contentsLayer->setAcceleratesDrawing(true);
#if HAVE(SUPPORT_HDR_DISPLAY)
        contentsLayer->setTonemappingEnabled(true);
#endif
        delegate->prepareToDelegateDisplay(contentsLayer);
    }

    noteSublayersChanged();
    noteLayerPropertyChanged(ContentsPlatformLayerChanged);
}

PlatformLayerIdentifier GraphicsLayerCA::setContentsToAsyncDisplayDelegate(RefPtr<GraphicsLayerContentsDisplayDelegate> delegate, ContentsLayerPurpose purpose)
{
    setContentsDisplayDelegate(WTFMove(delegate), purpose);
    return m_contentsLayer->layerID();
}

#if PLATFORM(IOS_FAMILY)
PlatformLayer* GraphicsLayerCA::contentsLayerForMedia() const
{
    return m_contentsLayerPurpose == ContentsLayerPurpose::Media ? m_contentsLayer->platformLayer() : nullptr;
}
#endif

void GraphicsLayerCA::setContentsMinificationFilter(ScalingFilter filter)
{
    if (filter == m_contentsMinificationFilter)
        return;
    GraphicsLayer::setContentsMinificationFilter(filter);
    noteLayerPropertyChanged(ContentsScalingFiltersChanged);
}

void GraphicsLayerCA::setContentsMagnificationFilter(ScalingFilter filter)
{
    if (filter == m_contentsMagnificationFilter)
        return;
    GraphicsLayer::setContentsMagnificationFilter(filter);
    noteLayerPropertyChanged(ContentsScalingFiltersChanged);
}

void GraphicsLayerCA::layerDidDisplay(PlatformCALayer* layer)
{
    if (!m_layerClones)
        return;

    LayerMap* layerCloneMap = nullptr;

    if (layer == m_layer)
        layerCloneMap = &m_layerClones->primaryLayerClones;
    else if (layer == m_contentsLayer)
        layerCloneMap = &m_layerClones->contentsLayerClones;

    if (layerCloneMap) {
        for (auto& platformLayerClone : layerCloneMap->values())
            platformLayerClone->copyContentsFromLayer(layer);
    }
}

FloatPoint GraphicsLayerCA::computePositionRelativeToBase(float& pageScale) const
{
    pageScale = 1;

    bool didFindAnyLayerThatAppliesPageScale = false;
    FloatPoint offset;
    for (const GraphicsLayer* currLayer = this; currLayer; currLayer = currLayer->parent()) {
        if (currLayer->appliesPageScale()) {
            pageScale *= currLayer->pageScaleFactor();
            didFindAnyLayerThatAppliesPageScale = true;
        }

        offset += currLayer->position();
    }

    return didFindAnyLayerThatAppliesPageScale ? offset : FloatPoint { };
}

void GraphicsLayerCA::flushCompositingState(const FloatRect& visibleRect)
{
    TransformState state(TransformState::UnapplyInverseTransformDirection, FloatQuad(visibleRect), FloatQuad { visibleRect });

    CommitState commitState;
    commitState.ancestorHadChanges = visibleRect != m_previousCommittedVisibleRect;

    // There is no backdrop root above the root layer, and we can just assume the backing
    // will be opaque. RenderLayerBacking will force an explicit backdrop root outside
    // of any filters if needed.
    commitState.backdropRootIsOpaque = client().backdropRootIsOpaque(this);
    m_previousCommittedVisibleRect = visibleRect;

#if PLATFORM(IOS_FAMILY)
    // In WK1, UIKit may be changing layer bounds behind our back in overflow-scroll layers, so disable the optimization.
    // See the similar test in computeVisibleAndCoverageRect().
    if (m_layer->type() == PlatformCALayer::Type::Cocoa)
        commitState.ancestorHadChanges = true;
#endif

    recursiveCommitChanges(commitState, state);
}

void GraphicsLayerCA::flushCompositingStateForThisLayerOnly()
{
    float pageScaleFactor;
    bool layerTypeChanged = false;
    
    CommitState commitState;

    FloatPoint offset = computePositionRelativeToBase(pageScaleFactor);
    commitLayerTypeChangesBeforeSublayers(commitState, pageScaleFactor, layerTypeChanged);
    commitLayerChangesBeforeSublayers(commitState, pageScaleFactor, offset, layerTypeChanged);
    commitLayerChangesAfterSublayers(commitState);

    if (layerTypeChanged)
        client().didChangePlatformLayerForLayer(this);
}

static inline bool accumulatesTransform(const GraphicsLayerCA& layer)
{
    return !layer.masksToBounds() && (layer.preserves3D() || (layer.parent() && layer.parent()->preserves3D()));
}

bool GraphicsLayerCA::recursiveVisibleRectChangeRequiresFlush(const CommitState& commitState, const TransformState& state) const
{
    TransformState localState = state;
    CommitState childCommitState = commitState;

    // This may be called at times when layout has not been updated, so we want to avoid calling out to the client
    // for animating transforms.
    VisibleAndCoverageRects rects = computeVisibleAndCoverageRect(localState, accumulatesTransform(*this), 0);
    
    LOG_WITH_STREAM(Layers, stream << "GraphicsLayerCA " << this << " id " << primaryLayerID() << " recursiveVisibleRectChangeRequiresFlush: visible rect " << rects.visibleRect << " coverage rect " << rects.coverageRect);

    adjustCoverageRect(rects, m_visibleRect);

    auto bounds = FloatRect(m_boundsOrigin, size());

    bool intersectsCoverageRect = rects.coverageRect.intersects(bounds);
    if (intersectsCoverageRect != m_intersectsCoverageRect)
        return true;

    if (rects.coverageRect != m_coverageRect) {
        if (TiledBacking* tiledBacking = this->tiledBacking()) {
            if (tiledBacking->tilesWouldChangeForCoverageRect(rects.coverageRect))
                return true;
        }
    }

    if (m_maskLayer) {
        Ref maskLayerCA = downcast<GraphicsLayerCA>(*m_maskLayer);
        if (maskLayerCA->recursiveVisibleRectChangeRequiresFlush(childCommitState, localState))
            return true;
    }

    for (const auto& layer : children()) {
        Ref currentChild = downcast<GraphicsLayerCA>(layer.get());
        if (currentChild->recursiveVisibleRectChangeRequiresFlush(childCommitState, localState))
            return true;
    }

    if (RefPtr replicaLayer = m_replicaLayer) {
        if (downcast<GraphicsLayerCA>(*replicaLayer).recursiveVisibleRectChangeRequiresFlush(childCommitState, localState))
            return true;
    }
    
    return false;
}

bool GraphicsLayerCA::visibleRectChangeRequiresFlush(const FloatRect& clipRect) const
{
    TransformState state(TransformState::UnapplyInverseTransformDirection, FloatQuad(clipRect));
    CommitState commitState;
    return recursiveVisibleRectChangeRequiresFlush(commitState, state);
}

TiledBacking* GraphicsLayerCA::tiledBacking() const
{
    return protectedLayer()->tiledBacking();
}

TransformationMatrix GraphicsLayerCA::layerTransform(const FloatPoint& position, const TransformationMatrix* customTransform) const
{
    auto transform = TransformationMatrix(position.x(), position.y());

    auto currentTransform = [&]() -> const TransformationMatrix* {
        if (customTransform)
            return customTransform;

        if (m_transform)
            return m_transform.get();

        return nullptr;
    }();

    if (currentTransform)
        transform.multiply(transformByApplyingAnchorPoint(*currentTransform));

    if (RefPtr parentLayer = parent()) {
        if (parentLayer->hasNonIdentityChildrenTransform()) {
            auto boundsOrigin = parentLayer->boundsOrigin();

            auto parentAnchorPoint(parentLayer->anchorPoint());
            parentAnchorPoint.scale(parentLayer->size().width(), parentLayer->size().height(), 1);
            parentAnchorPoint += boundsOrigin;

            transform.translateRight3d(-parentAnchorPoint.x(), -parentAnchorPoint.y(), -parentAnchorPoint.z());
            transform = parentLayer->childrenTransform() * transform;
            transform.translateRight3d(parentAnchorPoint.x(), parentAnchorPoint.y(), parentAnchorPoint.z());
        }
    }

    return transform;
}

TransformationMatrix GraphicsLayerCA::transformByApplyingAnchorPoint(const TransformationMatrix& matrix) const
{
    if (matrix.isIdentity())
        return matrix;

    TransformationMatrix result;
    FloatPoint3D absoluteAnchorPoint(anchorPoint());
    absoluteAnchorPoint.scale(size().width(), size().height(), 1);
    result.translate3d(absoluteAnchorPoint.x(), absoluteAnchorPoint.y(), absoluteAnchorPoint.z());
    result.multiply(matrix);
    result.translate3d(-absoluteAnchorPoint.x(), -absoluteAnchorPoint.y(), -absoluteAnchorPoint.z());
    return result;
}

void GraphicsLayerCA::adjustContentsScaleLimitingFactor()
{
    if (type() == Type::PageTiledBacking || !m_layer->usesTiledBackingLayer())
        return;

    float contentsScaleLimitingFactor = 1;
    auto bounds = FloatRect { m_boundsOrigin, size() };
    auto tileCoverageRect = intersection(m_coverageRect, bounds);
    if (!tileCoverageRect.isEmpty()) {
        const unsigned bytesPerPixel = 4; // FIXME: Use backingStoreBytesPerPixel(), which needs to be plumbed out through TiledBacking.
        double scaleFactor = deviceScaleFactor() * pageScaleFactor();
        double memoryEstimate = tileCoverageRect.area() * scaleFactor * scaleFactor * bytesPerPixel;
        if (memoryEstimate > cMaxScaledTiledLayerMemorySize) {
            // sqrt because the memory computation is based on area, while contents scale is per-axis.
            contentsScaleLimitingFactor = std::sqrt(cMaxScaledTiledLayerMemorySize / memoryEstimate);

            const float minFactor = 0.05;
            const float maxFactor = 1;
            contentsScaleLimitingFactor = clampTo(contentsScaleLimitingFactor, minFactor, maxFactor);

            // Quantize the value to avoid too many repaints when animating.
            const float quanitzationFactor = 20;
            contentsScaleLimitingFactor = std::round(contentsScaleLimitingFactor * quanitzationFactor) / quanitzationFactor;
        }

        LOG_WITH_STREAM(Tiling, stream << "GraphicsLayerCA " << this << " id " << primaryLayerID() << " " << size() << " adjustContentsScaleLimitingFactor: for coverage area " << tileCoverageRect << " memory " << (memoryEstimate / (1024 * 1024)) << "MP computed contentsScaleLimitingFactor " << contentsScaleLimitingFactor);
    }

    setContentsScaleLimitingFactor(contentsScaleLimitingFactor);
}

void GraphicsLayerCA::setContentsScaleLimitingFactor(float factor)
{
    if (factor == m_contentsScaleLimitingFactor)
        return;
    
    m_contentsScaleLimitingFactor = factor;
    noteLayerPropertyChanged(ContentsScaleChanged);
}

GraphicsLayerCA::VisibleAndCoverageRects GraphicsLayerCA::computeVisibleAndCoverageRect(TransformState& state, bool preserves3D, ComputeVisibleRectFlags flags) const
{
    FloatPoint position = approximatePosition();
    client().customPositionForVisibleRectComputation(this, position);

    TransformationMatrix currentTransform;
    auto transform = [&]() {
        if ((flags & RespectAnimatingTransforms) && client().getCurrentTransform(this, currentTransform))
            return layerTransform(position, &currentTransform);

        return layerTransform(position);
    }();

    bool applyWasClamped;
    TransformState::TransformAccumulation accumulation = preserves3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform;
    state.applyTransform(transform, accumulation, &applyWasClamped);

    bool mapWasClamped;
    auto clipRectFromParent = state.mappedQuad(&mapWasClamped).boundingBox();

    FloatRect clipRectForSelf { { }, m_size };
    if (CheckedPtr backing = this->tiledBacking())
        clipRectForSelf = backing->adjustedTileClipRectForObscuredInsets(clipRectForSelf);

    if (!applyWasClamped && !mapWasClamped)
        clipRectForSelf.intersect(clipRectFromParent);

    if (masksToBounds()) {
        ASSERT(accumulation == TransformState::FlattenTransform);
        // Flatten, and replace the quad in the TransformState with one that is clipped to this layer's bounds.
        if (state.isMappingSecondaryQuad())
            state.reset(clipRectForSelf, clipRectForSelf);
        else
            state.reset(clipRectForSelf);
    }

    auto boundsOrigin = m_boundsOrigin;
#if PLATFORM(IOS_FAMILY)
    // In WK1, UIKit may be changing layer bounds behind our back in overflow-scroll layers, so use the layer's origin.
    if (m_layer->type() == PlatformCALayer::Type::Cocoa)
        boundsOrigin = m_layer->bounds().location();
#endif

    auto coverageRect = clipRectForSelf;
    auto quad = state.mappedSecondaryQuad(&mapWasClamped);
    if (quad && !mapWasClamped && !applyWasClamped)
        coverageRect = (*quad).boundingBox();

    if (!boundsOrigin.isZero()) {
        state.move(LayoutSize { toFloatSize(-boundsOrigin) }, accumulation);
        clipRectForSelf.moveBy(boundsOrigin);
        coverageRect.moveBy(boundsOrigin);
    }

    return { clipRectForSelf, coverageRect, currentTransform };
}

bool GraphicsLayerCA::adjustCoverageRect(VisibleAndCoverageRects& rects, const FloatRect& oldVisibleRect) const
{
    FloatRect coverageRect = rects.coverageRect;

    switch (type()) {
    case Type::PageTiledBacking:
        coverageRect = tiledBacking()->adjustTileCoverageRectForScrolling(coverageRect, size(), oldVisibleRect, rects.visibleRect, pageScaleFactor() * deviceScaleFactor());
        break;
    case Type::ScrolledContents:
        if (m_layer->usesTiledBackingLayer())
            coverageRect = tiledBacking()->adjustTileCoverageRectForScrolling(coverageRect, size(), oldVisibleRect, rects.visibleRect, pageScaleFactor() * deviceScaleFactor());
        else {
            // Even if we don't have tiled backing, we want to expand coverage so that contained layers get attached backing store.
            coverageRect = adjustCoverageRectForMovement(coverageRect, oldVisibleRect, rects.visibleRect);
        }
        break;
    case Type::Normal:
    case Type::TiledBacking:
        if (m_layer->usesTiledBackingLayer())
            coverageRect = tiledBacking()->adjustTileCoverageRect(coverageRect, oldVisibleRect, rects.visibleRect, size() != m_sizeAtLastCoverageRectUpdate);
        break;
    default:
        break;
    }
    
    if (rects.coverageRect == coverageRect)
        return false;

    LOG_WITH_STREAM(Layers, stream << "GraphicsLayerCA " << this << " id " << primaryLayerID() << " adjustCoverageRect: coverage rect adjusted from " << rects.coverageRect << " to  " << coverageRect);

    rects.coverageRect = coverageRect;
    return true;
}

void GraphicsLayerCA::setVisibleAndCoverageRects(const VisibleAndCoverageRects& rects)
{
    bool visibleRectChanged = rects.visibleRect != m_visibleRect;
    bool coverageRectChanged = rects.coverageRect != m_coverageRect;
    if (!visibleRectChanged && !coverageRectChanged && !animationExtent())
        return;

    auto bounds = FloatRect(m_boundsOrigin, size());
    if (auto extent = animationExtent()) {
        // Adjust the animation extent to match the current animation position.
        auto animatingTransformWithAnchorPoint = transformByApplyingAnchorPoint(rects.animatingTransform);
        bounds = valueOrDefault(animatingTransformWithAnchorPoint.inverse()).mapRect(*extent);
    }

    // FIXME: we need to take reflections into account when determining whether this layer intersects the coverage rect.
    bool intersectsCoverageRect = rects.coverageRect.intersects(bounds);

    LOG_WITH_STREAM(Layers, stream << "GraphicsLayerCA " << this << " id " << primaryLayerID() << " setVisibleAndCoverageRects: coverage rect  " << rects.coverageRect << " intersects bounds " << bounds << " " << intersectsCoverageRect);

    if (intersectsCoverageRect != m_intersectsCoverageRect) {
        addUncommittedChanges(CoverageRectChanged);
        m_intersectsCoverageRect = intersectsCoverageRect;
    }

    if (visibleRectChanged) {
        addUncommittedChanges(CoverageRectChanged);
        m_visibleRect = rects.visibleRect;
    }

    if (coverageRectChanged) {
        addUncommittedChanges(CoverageRectChanged);
        m_coverageRect = rects.coverageRect;
    }

    adjustContentsScaleLimitingFactor();
}

bool GraphicsLayerCA::needsCommit(const CommitState& commitState)
{
    if (renderingIsSuppressedIncludingDescendants())
        return false;

    if (commitState.ancestorHadChanges)
        return true;
    if (m_uncommittedChanges)
        return true;
    if (hasDescendantsWithUncommittedChanges())
        return true;
    // Accelerated transforms move the underlying layers without GraphicsLayers getting invalidated.
    if (isRunningTransformAnimation())
        return true;
    if (hasDescendantsWithRunningTransformAnimations())
        return true;

    return false;
}

void GraphicsLayerCA::recursiveCommitChanges(CommitState& commitState, const TransformState& state, float pageScaleFactor, const FloatPoint& positionRelativeToBase, bool affectedByPageScale)
{
    if (!needsCommit(commitState))
        return;

    TransformState localState = state;
    CommitState childCommitState = commitState;

    ++childCommitState.treeDepth;
    if (structuralLayerPurpose() != NoStructuralLayer)
        ++childCommitState.treeDepth;

    bool layerTypeChanged = false;
    commitLayerTypeChangesBeforeSublayers(childCommitState, pageScaleFactor, layerTypeChanged);

    bool affectedByTransformAnimation = commitState.ancestorHasTransformAnimation;

    bool accumulateTransform = accumulatesTransform(*this);
    VisibleAndCoverageRects rects = computeVisibleAndCoverageRect(localState, accumulateTransform);
    if (adjustCoverageRect(rects, m_visibleRect)) {
        if (state.isMappingSecondaryQuad())
            localState.setSecondaryQuadInMappedSpace(FloatQuad { rects.coverageRect });
    }
    setVisibleAndCoverageRects(rects);

    if (commitState.ancestorStartedOrEndedTransformAnimation)
        addUncommittedChanges(CoverageRectChanged);

#ifdef VISIBLE_TILE_WASH
    // Use having a transform as a key to making the tile wash layer. If every layer gets a wash,
    // they start to obscure useful information.
    if ((!m_transform.isIdentity() || tiledBacking()) && !m_visibleTileWashLayer) {
        constexpr auto washFillColor = Color::red.colorWithAlphaByte(50);
        constexpr auto washBorderColor = Color::red.colorWithAlphaByte(100);
        
        m_visibleTileWashLayer = createPlatformCALayer(PlatformCALayer::LayerTypeLayer, this);
        m_visibleTileWashLayer->setName(makeString("Visible Tile Wash Layer 0x"_s, hex(reinterpret_cast<uintptr_t>(m_visibleTileWashLayer->platformLayer()), Lowercase)));
        m_visibleTileWashLayer->setAnchorPoint(FloatPoint3D(0, 0, 0));
        m_visibleTileWashLayer->setBorderColor(washBorderColor);
        m_visibleTileWashLayer->setBorderWidth(8);
        m_visibleTileWashLayer->setBackgroundColor(washFillColor);
        noteSublayersChanged(DontScheduleFlush);
    }

    if (m_visibleTileWashLayer) {
        m_visibleTileWashLayer->setPosition(m_visibleRect.location());
        m_visibleTileWashLayer->setBounds(FloatRect(FloatPoint(), m_visibleRect.size()));
    }
#endif

    bool hadChanges = m_uncommittedChanges;

    // FIXME: This could be more fine-grained. Only some types of changes have impact on sublayers.
    if (!childCommitState.ancestorHadChanges)
        childCommitState.ancestorHadChanges = hadChanges;

    if (appliesPageScale()) {
        pageScaleFactor *= this->pageScaleFactor();
        affectedByPageScale = true;
    }

    // Accumulate an offset from the ancestral pixel-aligned layer.
    FloatPoint baseRelativePosition = positionRelativeToBase;
    if (affectedByPageScale)
        baseRelativePosition += m_position;

    bool wasRunningTransformAnimation = isRunningTransformAnimation();

    commitLayerChangesBeforeSublayers(childCommitState, pageScaleFactor, baseRelativePosition, layerTypeChanged);

    bool nowRunningTransformAnimation = wasRunningTransformAnimation;
    if (m_uncommittedChanges & AnimationChanged)
        nowRunningTransformAnimation = isRunningTransformAnimation();

    if (wasRunningTransformAnimation != nowRunningTransformAnimation)
        childCommitState.ancestorStartedOrEndedTransformAnimation = true;

    if (nowRunningTransformAnimation) {
        childCommitState.ancestorHasTransformAnimation = true;
        if (m_intersectsCoverageRect || !animationExtent())
            childCommitState.ancestorWithTransformAnimationIntersectsCoverageRect = true;
        affectedByTransformAnimation = true;
    }

    if (isBackdropRoot())
        childCommitState.backdropRootIsOpaque = backgroundColor().isOpaque() || client().backdropRootIsOpaque(this);

    if (RefPtr maskLayer = downcast<GraphicsLayerCA>(m_maskLayer.get())) {
        maskLayer->setVisibleAndCoverageRects(rects);
        maskLayer->commitLayerTypeChangesBeforeSublayers(childCommitState, pageScaleFactor, layerTypeChanged);
        maskLayer->commitLayerChangesBeforeSublayers(childCommitState, pageScaleFactor, baseRelativePosition, layerTypeChanged);
    }

    bool hasDescendantsWithRunningTransformAnimations = false;

    if (childCommitState.treeDepth <= cMaxLayerTreeDepth) {
        for (auto& layer : children()) {
            Ref currentChild = downcast<GraphicsLayerCA>(layer.get());
            currentChild->recursiveCommitChanges(childCommitState, localState, pageScaleFactor, baseRelativePosition, affectedByPageScale);

            if (currentChild->isRunningTransformAnimation() || currentChild->hasDescendantsWithRunningTransformAnimations())
                hasDescendantsWithRunningTransformAnimations = true;
        }
    }

    commitState.totalBackdropFilterArea = childCommitState.totalBackdropFilterArea;

    if (RefPtr replicaLayer = downcast<GraphicsLayerCA>(m_replicaLayer.get()))
        replicaLayer->recursiveCommitChanges(childCommitState, localState, pageScaleFactor, baseRelativePosition, affectedByPageScale);

    if (RefPtr maskLayer = downcast<GraphicsLayerCA>(m_maskLayer.get()))
        maskLayer->commitLayerChangesAfterSublayers(childCommitState);

    setHasDescendantsWithUncommittedChanges(false);
    setHasDescendantsWithRunningTransformAnimations(hasDescendantsWithRunningTransformAnimations);

    bool hadDirtyRects = m_uncommittedChanges & DirtyRectsChanged;
    commitLayerChangesAfterSublayers(childCommitState);

    if (affectedByTransformAnimation && m_layer->layerType() == PlatformCALayer::LayerType::LayerTypeTiledBackingLayer)
        client().notifySubsequentFlushRequired(this);

    if (layerTypeChanged)
        client().didChangePlatformLayerForLayer(this);

    if (usesDisplayListDrawing() && m_drawsContent && (!m_hasEverPainted || hadDirtyRects)) {
        TraceScope tracingScope(DisplayListRecordStart, DisplayListRecordEnd);
        m_displayList = nullptr;
        FloatRect initialClip(boundsOrigin(), size());
        DisplayList::RecorderImpl context(GraphicsContextState(), initialClip, AffineTransform());
        paintGraphicsLayerContents(context, FloatRect(FloatPoint(), size()));
        m_displayList = context.takeDisplayList();
    }
}

void GraphicsLayerCA::platformCALayerCustomSublayersChanged(PlatformCALayer*)
{
    noteLayerPropertyChanged(ChildrenChanged, m_isCommittingChanges ? DontScheduleFlush : ScheduleFlush);
}

bool GraphicsLayerCA::platformCALayerShowRepaintCounter(PlatformCALayer* platformLayer) const
{
    // The repaint counters are painted into the TileController tiles (which have no corresponding platform layer),
    // so we don't want to overpaint the repaint counter when called with the TileController's own layer.
    if (isPageTiledBackingLayer() && platformLayer)
        return false;

    return isShowingRepaintCounter();
}

void GraphicsLayerCA::platformCALayerPaintContents(PlatformCALayer*, GraphicsContext& context, const FloatRect& clip, OptionSet<GraphicsLayerPaintBehavior> layerPaintBehavior)
{
    m_hasEverPainted = true;
    if (m_displayList) {
        context.drawDisplayList(*m_displayList);
        
        if (isTrackingDisplayListReplay()) [[unlikely]] {
            // Original purpose of the code was to track playback time optimizations. However, there are no such things, and as such we
            // use the original.
            layerDisplayListMap().add(this, std::make_pair(clip, Ref { *m_displayList }));
        }
        return;
    }

    TraceScope tracingScope(PaintLayerStart, PaintLayerEnd);
    paintGraphicsLayerContents(context, clip, layerPaintBehavior);
}

void GraphicsLayerCA::platformCALayerSetNeedsToRevalidateTiles()
{
    noteLayerPropertyChanged(TilingAreaChanged, m_isCommittingChanges ? DontScheduleFlush : ScheduleFlush);
}

float GraphicsLayerCA::platformCALayerDeviceScaleFactor() const
{
    return deviceScaleFactor();
}

float GraphicsLayerCA::platformCALayerContentsScaleMultiplierForNewTiles(PlatformCALayer*) const
{
    return client().contentsScaleMultiplierForNewTiles(this);
}

bool GraphicsLayerCA::platformCALayerShouldAggressivelyRetainTiles(PlatformCALayer*) const
{
    return client().shouldAggressivelyRetainTiles(this);
}

bool GraphicsLayerCA::platformCALayerShouldTemporarilyRetainTileCohorts(PlatformCALayer*) const
{
    return client().shouldTemporarilyRetainTileCohorts(this);
}

bool GraphicsLayerCA::platformCALayerUseGiantTiles() const
{
    return client().useGiantTiles();
}

bool GraphicsLayerCA::platformCALayerCSSUnprefixedBackdropFilterEnabled() const
{
    return client().cssUnprefixedBackdropFilterEnabled();
}

void GraphicsLayerCA::platformCALayerLogFilledVisibleFreshTile(unsigned blankPixelCount)
{
    client().logFilledVisibleFreshTile(blankPixelCount);
}

bool GraphicsLayerCA::platformCALayerDelegatesDisplay(PlatformCALayer* layer) const
{
    return (m_contentsDisplayDelegate || m_contentsLayerPurpose == ContentsLayerPurpose::Image) && m_contentsLayer == layer;
}

void GraphicsLayerCA::platformCALayerLayerDisplay(PlatformCALayer* layer)
{
    ASSERT(m_contentsDisplayDelegate || m_contentsLayerPurpose == ContentsLayerPurpose::Image);
    ASSERT(layer == m_contentsLayer);
    if (RefPtr delegate = m_contentsDisplayDelegate)
        delegate->display(*layer);
}

bool GraphicsLayerCA::platformCALayerNeedsPlatformContext(const PlatformCALayer*) const
{
    return client().layerNeedsPlatformContext(*this);
}

void GraphicsLayerCA::commitLayerTypeChangesBeforeSublayers(CommitState&, float pageScaleFactor, bool& layerTypeChanged)
{
    SetForScope committingChangesChange(m_isCommittingChanges, true);

    bool needTiledLayer = requiresTiledLayer(pageScaleFactor);

    PlatformCALayer::LayerType currentLayerType = m_layer->layerType();
    PlatformCALayer::LayerType neededLayerType = currentLayerType;

    if (needTiledLayer)
        neededLayerType = PlatformCALayer::LayerType::LayerTypeTiledBackingLayer;
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    else if (m_isSeparatedImage)
        neededLayerType = PlatformCALayer::LayerType::LayerTypeSeparatedImageLayer;
    else if (currentLayerType == PlatformCALayer::LayerType::LayerTypeSeparatedImageLayer)
        neededLayerType = PlatformCALayer::LayerType::LayerTypeWebLayer;
#endif
    else if (currentLayerType == PlatformCALayer::LayerType::LayerTypeTiledBackingLayer)
        neededLayerType = PlatformCALayer::LayerType::LayerTypeWebLayer;

    if (neededLayerType != m_layer->layerType()) {
        changeLayerTypeTo(neededLayerType);
        layerTypeChanged = true;
    }
}

void GraphicsLayerCA::commitLayerChangesBeforeSublayers(CommitState& commitState, float pageScaleFactor, const FloatPoint& positionRelativeToBase, bool& layerChanged)
{
    SetForScope committingChangesChange(m_isCommittingChanges, true);

    if (!m_uncommittedChanges) {
        // Ensure that we cap layer depth in commitLayerChangesAfterSublayers().
        if (commitState.treeDepth > cMaxLayerTreeDepth)
            addUncommittedChanges(ChildrenChanged);
    }

    // Need to handle Preserves3DChanged first, because it affects which layers subsequent properties are applied to
    LayerChangeFlags structuralLayerUpdateReasons = Preserves3DChanged | ReplicatedLayerChanged | BackdropFiltersChanged;
#if HAVE(CORE_MATERIAL)
    structuralLayerUpdateReasons |= AppleVisualEffectChanged;
#endif
    if (m_uncommittedChanges & structuralLayerUpdateReasons) {
        if (updateStructuralLayer())
            layerChanged = true;
    }

    if (m_uncommittedChanges & GeometryChanged)
        updateGeometry(pageScaleFactor, positionRelativeToBase);

    if (m_uncommittedChanges & DrawsContentChanged)
        updateDrawsContent();

#if HAVE(SUPPORT_HDR_DISPLAY)
    if (m_uncommittedChanges & DrawsHDRContentChanged)
        updateDrawsHDRContent();

    if (m_uncommittedChanges & TonemappingEnabledChanged)
        updateTonemappingEnabled();
#endif

    if (m_uncommittedChanges & NameChanged)
        updateNames();

    if (m_uncommittedChanges & ContentsImageChanged) // Needs to happen before ChildrenChanged
        updateContentsImage();
        
    if (m_uncommittedChanges & ContentsPlatformLayerChanged) // Needs to happen before ChildrenChanged
        updateContentsPlatformLayer();

    if (m_uncommittedChanges & ContentsColorLayerChanged) // Needs to happen before ChildrenChanged
        updateContentsColorLayer();
    
    if (m_uncommittedChanges & BackgroundColorChanged)
        updateBackgroundColor();

    if (m_uncommittedChanges & TransformChanged)
        updateTransform();

    if (m_uncommittedChanges & ChildrenTransformChanged)
        updateChildrenTransform();
    
    if (m_uncommittedChanges & MasksToBoundsChanged)
        updateMasksToBounds();
    
    if (m_uncommittedChanges & ContentsVisibilityChanged)
        updateContentsVisibility();

    if (m_uncommittedChanges & UserInteractionEnabledChanged)
        updateUserInteractionEnabled();

    // Note that contentsScale can affect whether the layer can be opaque.
    if (m_uncommittedChanges & ContentsOpaqueChanged)
        updateContentsOpaque(pageScaleFactor);

    if (m_uncommittedChanges & BackfaceVisibilityChanged)
        updateBackfaceVisibility();

    if (m_uncommittedChanges & OpacityChanged)
        updateOpacityOnLayer();

    if (m_uncommittedChanges & FiltersChanged)
        updateFilters();

    // If there are backdrop filters, we need to always check the resource usage
    // because something up the tree may have changed its usage.
    if (m_uncommittedChanges & BackdropFiltersChanged || needsBackdrop())
        updateBackdropFilters(commitState);

#if HAVE(CORE_MATERIAL)
    if (m_uncommittedChanges & AppleVisualEffectChanged)
        updateAppleVisualEffectData();
#endif

    if (m_uncommittedChanges & BackdropRootChanged)
        updateBackdropRoot();

    if (m_uncommittedChanges & BackdropFiltersRectChanged)
        updateBackdropFiltersRect();

    if (m_uncommittedChanges & BlendModeChanged)
        updateBlendMode();

    if (m_uncommittedChanges & VideoGravityChanged)
        updateVideoGravity();

    if (m_uncommittedChanges & ShapeChanged)
        updateShape();

    if (m_uncommittedChanges & WindRuleChanged)
        updateWindRule();

    if (m_uncommittedChanges & AnimationChanged)
        updateAnimations();

    updateRootRelativeScale(); // Needs to happen before ContentsScaleChanged.

    // Updating the contents scale can cause parts of the layer to be invalidated,
    // so make sure to update the contents scale before updating the dirty rects.
    if (m_uncommittedChanges & ContentsScaleChanged)
        updateContentsScale(m_rootRelativeScaleFactor * pageScaleFactor);

    if (m_uncommittedChanges & CoverageRectChanged)
        updateCoverage(commitState);

    if (m_uncommittedChanges & AcceleratesDrawingChanged) // Needs to happen before TilingAreaChanged.
        updateAcceleratesDrawing();

    if (m_uncommittedChanges & TilingAreaChanged) // Needs to happen after CoverageRectChanged, ContentsScaleChanged
        updateTiles();

    if (m_uncommittedChanges & DirtyRectsChanged)
        repaintLayerDirtyRects();
    
    if (m_uncommittedChanges & ContentsRectsChanged) // Needs to happen before ChildrenChanged
        updateContentsRects();

    if (m_uncommittedChanges & EventRegionChanged)
        updateEventRegion();

#if ENABLE(SCROLLING_THREAD)
    if (m_uncommittedChanges & ScrollingNodeChanged)
        updateScrollingNode();
#endif

    if (m_uncommittedChanges & MaskLayerChanged) {
        updateMaskLayer();
        // If the mask layer becomes tiled it can set this flag again. Clear the flag so that
        // commitLayerChangesAfterSublayers doesn't update the mask again in the normal case.
        m_uncommittedChanges &= ~MaskLayerChanged;
    }

    if (m_uncommittedChanges & ContentsNeedsDisplay)
        updateContentsNeedsDisplay();

    if (m_uncommittedChanges & DebugIndicatorsChanged)
        updateDebugIndicators();

    if (m_uncommittedChanges & CustomAppearanceChanged)
        updateCustomAppearance();

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    if (m_uncommittedChanges & SeparatedChanged)
        updateIsSeparated();

#if HAVE(CORE_ANIMATION_SEPARATED_PORTALS)
    if (m_uncommittedChanges & SeparatedPortalChanged)
        updateIsSeparatedPortal();

    if (m_uncommittedChanges & DescendentOfSeparatedPortalChanged)
        updateIsDescendentOfSeparatedPortal();
#endif
#endif
    if (m_uncommittedChanges & ContentsScalingFiltersChanged)
        updateContentsScalingFilters();

    if (m_uncommittedChanges & ChildrenChanged) {
        updateSublayerList();
        // Sublayers may set this flag again, so clear it to avoid always updating sublayers in commitLayerChangesAfterSublayers().
        m_uncommittedChanges &= ~ChildrenChanged;
    }

    // Ensure that we cap layer depth in commitLayerChangesAfterSublayers().
    if (commitState.treeDepth > cMaxLayerTreeDepth)
        addUncommittedChanges(ChildrenChanged);
}

void GraphicsLayerCA::commitLayerChangesAfterSublayers(CommitState& commitState)
{
    if (!m_uncommittedChanges)
        return;

    SetForScope committingChangesChange(m_isCommittingChanges, true);

    if (m_uncommittedChanges & MaskLayerChanged)
        updateMaskLayer();

    if (m_uncommittedChanges & ChildrenChanged)
        updateSublayerList(commitState.treeDepth > cMaxLayerTreeDepth);

    if (m_uncommittedChanges & ReplicatedLayerChanged)
        updateReplicatedLayers();

    m_uncommittedChanges = NoChange;
}

void GraphicsLayerCA::updateNames()
{
    auto name = debugName();
    switch (structuralLayerPurpose()) {
    case StructuralLayerForPreserves3D:
        protectedStructuralLayer()->setName(makeString("preserve-3d: "_s, name));
        break;
    case StructuralLayerForReplicaFlattening:
        protectedStructuralLayer()->setName(makeString("replica flattening: "_s, name));
        break;
    case StructuralLayerForBackdrop:
        protectedStructuralLayer()->setName(makeString("backdrop hosting: "_s, name));
        break;
#if HAVE(MATERIAL_HOSTING)
    case StructuralLayerForMaterial:
        protectedStructuralLayer()->setName(makeString("material hosting: "_s, name));
        break;
#endif
    case NoStructuralLayer:
        break;
    }
    protectedLayer()->setName(name);
}

void GraphicsLayerCA::updateSublayerList(bool maxLayerDepthReached)
{
    RefPtr layer = m_layer;
    if (maxLayerDepthReached) {
        layer->setSublayers({ });
        return;
    }

    auto appendStructuralLayerChildren = [&](PlatformCALayerList& list) {
        if (m_backdropLayer)
            list.append(m_backdropLayer);

        if (m_replicaLayer)
            list.append(downcast<GraphicsLayerCA>(*m_replicaLayer).primaryLayer());
    
        list.append(layer);
    };

    auto appendContentsLayer = [&](PlatformCALayerList& list) {
        if (m_contentsVisible && m_contentsLayer)
            list.append(m_contentsLayer);
    };

    auto appendClippingLayers = [&](PlatformCALayerList& list) {
        if (m_contentsVisible && m_contentsClippingLayer)
            list.append(m_contentsClippingLayer);
    };

    auto appendCustomAndClippingLayers = [&](PlatformCALayerList& list) {
        if (auto* customSublayers = layer->customSublayers())
            list.appendVector(*customSublayers);

        if (m_contentsClippingLayer)
            appendClippingLayers(list);
        else
            appendContentsLayer(list);
    };

    auto appendLayersFromChildren = [&](PlatformCALayerList& list) {
        for (Ref child : children())
            list.append(downcast<GraphicsLayerCA>(child)->layerForSuperlayer());
    };

    auto appendDebugLayers = [&](PlatformCALayerList& list) {
#ifdef VISIBLE_TILE_WASH
        if (m_visibleTileWashLayer)
            list.append(m_visibleTileWashLayer);
#else
        UNUSED_PARAM(list);
#endif
    };

    auto buildChildLayerList = [&](PlatformCALayerList& list) {
        appendLayersFromChildren(list);
        appendDebugLayers(list);
    };

    PlatformCALayerList primaryLayerChildren;
    appendCustomAndClippingLayers(primaryLayerChildren);

    bool clippingLayerHostsChildren = m_contentsRectClipsDescendants && m_contentsClippingLayer;
    bool structuralLayerHostsChildren = !clippingLayerHostsChildren && m_structuralLayer && structuralLayerPurpose() != StructuralLayerPurpose::StructuralLayerForBackdrop;
    if (RefPtr contentsClippingLayer = m_contentsClippingLayer) {
        PlatformCALayerList clippingChildren;
        appendContentsLayer(clippingChildren);
        if (clippingLayerHostsChildren)
            buildChildLayerList(clippingChildren);
        contentsClippingLayer->setSublayers(clippingChildren);
    }

    if (RefPtr structuralLayer = m_structuralLayer) {
        PlatformCALayerList layerList;
        appendStructuralLayerChildren(layerList);

        if (structuralLayerHostsChildren)
            buildChildLayerList(layerList);

        structuralLayer->setSublayers(layerList);
    }

    if (!clippingLayerHostsChildren && !structuralLayerHostsChildren)
        buildChildLayerList(primaryLayerChildren);

    layer->setSublayers(primaryLayerChildren);
}

void GraphicsLayerCA::updateGeometry(float pageScaleFactor, const FloatPoint& positionRelativeToBase)
{
    FloatPoint scaledPosition = m_position;
    FloatPoint3D scaledAnchorPoint = m_anchorPoint;
    FloatSize scaledSize = m_size;
    FloatSize pixelAlignmentOffset;

    // FIXME: figure out if we really need to pixel align the graphics layer here.
    if (client().needsPixelAligment() && !WTF::isIntegral(pageScaleFactor) && m_drawsContent && !m_masksToBounds)
        computePixelAlignment(pageScaleFactor, positionRelativeToBase, scaledPosition, scaledAnchorPoint, pixelAlignmentOffset);

    // Update position.
    // Position is offset on the layer by the layer anchor point.
    FloatPoint3D adjustedPosition(scaledPosition.x() + scaledAnchorPoint.x() * scaledSize.width(), scaledPosition.y() + scaledAnchorPoint.y() * scaledSize.height(), scaledAnchorPoint.z());

    if (RefPtr structuralLayer = m_structuralLayer) {
        FloatPoint3D layerPosition(m_position.x() + m_anchorPoint.x() * m_size.width(), m_position.y() + m_anchorPoint.y() * m_size.height(), scaledAnchorPoint.z());
        FloatRect layerBounds(m_boundsOrigin, m_size);

        structuralLayer->setPosition(layerPosition);
        structuralLayer->setBounds(layerBounds);
        structuralLayer->setAnchorPoint(m_anchorPoint);

        if (m_layerClones) {
            for (auto& clone : m_layerClones->structuralLayerClones) {
                RefPtr cloneLayer = clone.value.get();
                FloatPoint3D clonePosition = layerPosition;

                if (m_replicaLayer && isReplicatedRootClone(clone.key)) {
                    // Maintain the special-case position for the root of a clone subtree,
                    // which we set up in replicatedLayerRoot().
                    clonePosition = positionForCloneRootLayer();
                }

                cloneLayer->setPosition(clonePosition);
                cloneLayer->setBounds(layerBounds);
                cloneLayer->setAnchorPoint(m_anchorPoint);
            }
        }

        // If we have a structural layer, we just use 0.5, 0.5 for the anchor point of the main layer.
        scaledAnchorPoint = FloatPoint(0.5f, 0.5f);
        adjustedPosition = FloatPoint(scaledAnchorPoint.x() * scaledSize.width() - pixelAlignmentOffset.width(), scaledAnchorPoint.y() * scaledSize.height() - pixelAlignmentOffset.height());
    }

    m_pixelAlignmentOffset = pixelAlignmentOffset;

    // Push the layer to device pixel boundary (setPosition()), but move the content back to its original position (setBounds())
    RefPtr layer = m_layer;
    layer->setPosition(adjustedPosition);
    FloatRect adjustedBounds = FloatRect(FloatPoint(m_boundsOrigin - pixelAlignmentOffset), m_size);
    layer->setBounds(adjustedBounds);
    layer->setAnchorPoint(scaledAnchorPoint);

    if (m_layerClones) {
        for (auto& clone : m_layerClones->primaryLayerClones) {
            RefPtr cloneLayer = clone.value.get();
            FloatPoint3D clonePosition = adjustedPosition;

            if (!m_structuralLayer && m_replicaLayer && isReplicatedRootClone(clone.key)) {
                // Maintain the special-case position for the root of a clone subtree,
                // which we set up in replicatedLayerRoot().
                clonePosition = positionForCloneRootLayer();
            }

            cloneLayer->setPosition(clonePosition);
            cloneLayer->setBounds(adjustedBounds);
            cloneLayer->setAnchorPoint(scaledAnchorPoint);
        }
    }
}

void GraphicsLayerCA::updateTransform()
{
    protectedPrimaryLayer()->setTransform(transform());

    if (LayerMap* layerCloneMap = primaryLayerClones()) {
        for (auto& clone : *layerCloneMap) {
            RefPtr currLayer = clone.value.get();
            if (m_replicaLayer && isReplicatedRootClone(clone.key)) {
                // Maintain the special-case transform for the root of a clone subtree,
                // which we set up in replicatedLayerRoot().
                currLayer->setTransform(TransformationMatrix());
            } else
                currLayer->setTransform(transform());
        }
    }
}

void GraphicsLayerCA::updateChildrenTransform()
{
    protectedPrimaryLayer()->setSublayerTransform(childrenTransform());

    if (LayerMap* layerCloneMap = primaryLayerClones()) {
        for (auto& layer : layerCloneMap->values())
            layer->setSublayerTransform(childrenTransform());
    }
}

void GraphicsLayerCA::updateMasksToBounds()
{
    protectedLayer()->setMasksToBounds(m_masksToBounds);

    if (m_layerClones) {
        for (auto& layer : m_layerClones->primaryLayerClones.values())
            layer->setMasksToBounds(m_masksToBounds);
    }
}

void GraphicsLayerCA::updateContentsVisibility()
{
    // Note that m_contentsVisible also affects whether m_contentsLayer is parented.
    RefPtr layer = m_layer;
    if (m_contentsVisible) {
        if (m_drawsContent)
            layer->setNeedsDisplay();

        if (RefPtr backdropLayer = m_backdropLayer)
            backdropLayer->setHidden(false);
    } else {
        layer->clearContents();

        if (m_layerClones) {
            for (auto& layer : m_layerClones->primaryLayerClones.values())
                layer->setContents(nullptr);
        }

        if (RefPtr backdropLayer = m_backdropLayer)
            backdropLayer->setHidden(true);
    }

    layer->setContentsHidden(!m_contentsVisible);
}

void GraphicsLayerCA::updateUserInteractionEnabled()
{
    protectedLayer()->setUserInteractionEnabled(m_userInteractionEnabled);
}

void GraphicsLayerCA::updateContentsOpaque(float pageScaleFactor)
{
    bool contentsOpaque = m_contentsOpaque;
    if (contentsOpaque) {
        float contentsScale = pageScaleFactor * deviceScaleFactor();
        if (!WTF::isIntegral(contentsScale) && !client().paintsOpaquelyAtNonIntegralScales(this))
            contentsOpaque = false;
    }
    
    protectedLayer()->setOpaque(contentsOpaque);

    if (m_layerClones) {
        for (auto& layer : m_layerClones->primaryLayerClones.values())
            layer->setOpaque(contentsOpaque);
    }
}

void GraphicsLayerCA::updateBackfaceVisibility()
{
    if (RefPtr structuralLayer = m_structuralLayer; structuralLayer && (structuralLayerPurpose() == StructuralLayerForReplicaFlattening || structuralLayerPurpose() == StructuralLayerForBackdrop)) {
        structuralLayer->setDoubleSided(m_backfaceVisibility);

        if (m_layerClones) {
            for (auto& layer : m_layerClones->structuralLayerClones.values())
                layer->setDoubleSided(m_backfaceVisibility);
        }
    }

    protectedLayer()->setDoubleSided(m_backfaceVisibility);

    if (m_layerClones) {
        for (auto& layer : m_layerClones->primaryLayerClones.values())
            layer->setDoubleSided(m_backfaceVisibility);
    }
}

void GraphicsLayerCA::updateFilters()
{
    protectedPrimaryLayer()->setFilters(m_filters);

    if (LayerMap* layerCloneMap = primaryLayerClones()) {
        for (auto& clone : *layerCloneMap) {
            if (m_replicaLayer && isReplicatedRootClone(clone.key))
                continue;

            Ref { *clone.value }->setFilters(m_filters);
        }
    }
}

void GraphicsLayerCA::updateBackdropFilters(CommitState& commitState)
{
    bool canHaveBackdropFilters = needsBackdrop();
    if (canHaveBackdropFilters) {
        canHaveBackdropFilters = false;
        IntRect backdropFilterRect = enclosingIntRect(m_backdropFiltersRect.rect());
        if (backdropFilterRect.width() > 0 && backdropFilterRect.height() > 0) {
            CheckedUint32 backdropFilterArea = CheckedUint32(backdropFilterRect.width()) * CheckedUint32(backdropFilterRect.height());
            if (!backdropFilterArea.hasOverflowed()) {
                CheckedUint32 newTotalBackdropFilterArea = CheckedUint32(commitState.totalBackdropFilterArea) + backdropFilterArea;
                if (!newTotalBackdropFilterArea.hasOverflowed() && newTotalBackdropFilterArea <= cMaxTotalBackdropFilterArea) {
                    commitState.totalBackdropFilterArea = newTotalBackdropFilterArea;
                    canHaveBackdropFilters = true;
                }
            }
        }
    }

    if (!canHaveBackdropFilters) {
        if (RefPtr backdropLayer = std::exchange(m_backdropLayer, nullptr)) {
            backdropLayer->removeFromSuperlayer();
            backdropLayer->setOwner(nullptr);
        }
        return;
    }

    // If nothing actually changed, no need to touch the layer properties.
    if (!(m_uncommittedChanges & BackdropFiltersChanged) && m_backdropLayer) {
        if (Ref { *m_backdropLayer }->backdropRootIsOpaque() == commitState.backdropRootIsOpaque)
            return;
    }

    auto expectedLayerType = PlatformCALayer::LayerType::LayerTypeBackdropLayer;
#if HAVE(CORE_MATERIAL)
    if (appleVisualEffectNeedsBackdrop(m_appleVisualEffectData.effect))
        expectedLayerType = PlatformCALayer::LayerType::LayerTypeMaterialLayer;
#endif

    bool makeLayer = !m_backdropLayer || (m_backdropLayer->layerType() != expectedLayerType);
    if (makeLayer) {
        Ref backdropLayer = createPlatformCALayer(expectedLayerType, this);
        m_backdropLayer = backdropLayer.copyRef();
        backdropLayer->setAnchorPoint(FloatPoint3D());
        backdropLayer->setMasksToBounds(true);
#if HAVE(CORE_MATERIAL)
        if (expectedLayerType == PlatformCALayer::LayerType::LayerTypeMaterialLayer)
            backdropLayer->setName(MAKE_STATIC_STRING_IMPL("material"));
        else
#endif
            backdropLayer->setName(MAKE_STATIC_STRING_IMPL("backdrop"));
    }

    Ref backdropLayer = *m_backdropLayer;
    backdropLayer->setHidden(!m_contentsVisible);
    backdropLayer->setBackdropRootIsOpaque(commitState.backdropRootIsOpaque);

    bool shouldSetFilters = true;
#if HAVE(CORE_MATERIAL)
    if (m_appleVisualEffectData.effect != AppleVisualEffect::None) {
        backdropLayer->setAppleVisualEffectData(m_appleVisualEffectData);
        shouldSetFilters = false;
    }
#endif

    if (shouldSetFilters)
        backdropLayer->setFilters(m_backdropFilters);

    if (m_layerClones) {
        for (auto& clone : m_layerClones->backdropLayerClones.values()) {
            clone->setHidden(!m_contentsVisible);
            clone->setBackdropRootIsOpaque(commitState.backdropRootIsOpaque);
            if (shouldSetFilters)
                clone->setFilters(m_backdropFilters);
        }
    }

    if (makeLayer) {
        updateBackdropFiltersRect();
        noteSublayersChanged(DontScheduleFlush);
    }
}

void GraphicsLayerCA::updateBackdropFiltersRect()
{
    RefPtr backdropLayer = m_backdropLayer;
    if (!backdropLayer)
        return;

    FloatRect contentBounds(0, 0, m_backdropFiltersRect.rect().width(), m_backdropFiltersRect.rect().height());
    backdropLayer->setBounds(contentBounds);
    backdropLayer->setPosition(m_backdropFiltersRect.rect().location());

    auto backdropRectRelativeToBackdropLayer = m_backdropFiltersRect;
    backdropRectRelativeToBackdropLayer.setLocation({ });
    updateClippingStrategy(*backdropLayer, m_backdropClippingLayer, backdropRectRelativeToBackdropLayer);

    if (m_layerClones) {
        for (auto& clone : m_layerClones->backdropLayerClones) {
            RefPtr backdropCloneLayer = clone.value.get();
            backdropCloneLayer->setBounds(contentBounds);
            backdropCloneLayer->setPosition(m_backdropFiltersRect.rect().location());

            CloneID cloneID = clone.key;
            RefPtr<PlatformCALayer> backdropClippingLayerClone = m_layerClones->backdropClippingLayerClones.get(cloneID);

            bool hadBackdropClippingLayer = backdropClippingLayerClone;
            updateClippingStrategy(*backdropCloneLayer, backdropClippingLayerClone, backdropRectRelativeToBackdropLayer);

            if (!backdropClippingLayerClone)
                m_layerClones->backdropClippingLayerClones.remove(cloneID);
            else if (backdropClippingLayerClone && !hadBackdropClippingLayer)
                m_layerClones->backdropClippingLayerClones.add(cloneID, backdropClippingLayerClone);
        }
    }
}

void GraphicsLayerCA::updateBackdropRoot()
{
    protectedLayer()->setIsBackdropRoot(isBackdropRoot());
}

void GraphicsLayerCA::updateBlendMode()
{
    protectedPrimaryLayer()->setBlendMode(m_blendMode);

    if (LayerMap* layerCloneMap = primaryLayerClones()) {
        for (auto& clone : *layerCloneMap) {
            if (m_replicaLayer && isReplicatedRootClone(clone.key))
                continue;
            Ref { *clone.value }->setBlendMode(m_blendMode);
        }
    }
}

void GraphicsLayerCA::updateVideoGravity()
{
    if (RefPtr contentsLayer = m_contentsLayer)
        contentsLayer->setVideoGravity(m_videoGravity);
}

void GraphicsLayerCA::updateShape()
{
    protectedLayer()->setShapePath(m_shapeLayerPath);

    if (LayerMap* layerCloneMap = primaryLayerClones()) {
        for (auto& layer : layerCloneMap->values())
            layer->setShapePath(m_shapeLayerPath);
    }
}

void GraphicsLayerCA::updateWindRule()
{
    protectedLayer()->setShapeWindRule(m_shapeLayerWindRule);
}

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
void GraphicsLayerCA::updateIsSeparated()
{
    m_layer->setIsSeparated(m_isSeparated);
}

#if HAVE(CORE_ANIMATION_SEPARATED_PORTALS)
void GraphicsLayerCA::updateIsSeparatedPortal()
{
    m_layer->setIsSeparatedPortal(m_isSeparatedPortal);
}

void GraphicsLayerCA::updateIsDescendentOfSeparatedPortal()
{
    m_layer->setIsDescendentOfSeparatedPortal(m_isDescendentOfSeparatedPortal);
}
#endif
#endif

#if HAVE(CORE_MATERIAL)
void GraphicsLayerCA::updateAppleVisualEffectData()
{
    if (RefPtr backdropLayer = m_backdropLayer; backdropLayer && (appleVisualEffectNeedsBackdrop(backdropLayer->appleVisualEffectData().effect) || appleVisualEffectNeedsBackdrop(m_appleVisualEffectData.effect)))
        backdropLayer->setAppleVisualEffectData(m_appleVisualEffectData);

    if (appleVisualEffectAppliesFilter(m_appleVisualEffectData.effect))
        protectedLayer()->setAppleVisualEffectData(m_appleVisualEffectData);
    else
        protectedLayer()->setAppleVisualEffectData({ });

#if HAVE(MATERIAL_HOSTING)
    if (RefPtr structuralLayer = m_structuralLayer; structuralLayer && appleVisualEffectIsHostedMaterial(m_appleVisualEffectData.effect))
        structuralLayer->setAppleVisualEffectData(m_appleVisualEffectData);
#endif
}
#endif

void GraphicsLayerCA::updateContentsScalingFilters()
{
    RefPtr contentsLayer = m_contentsLayer;
    if (!contentsLayer)
        return;
    contentsLayer->setMinificationFilter(toPlatformCALayerFilterType(m_contentsMinificationFilter));
    contentsLayer->setMagnificationFilter(toPlatformCALayerFilterType(m_contentsMagnificationFilter));
}

bool GraphicsLayerCA::updateStructuralLayer()
{
    return ensureStructuralLayer(structuralLayerPurpose());
}

bool GraphicsLayerCA::ensureStructuralLayer(StructuralLayerPurpose purpose)
{
    const LayerChangeFlags structuralLayerChangeFlags = NameChanged
        | GeometryChanged
        | TransformChanged
        | ChildrenTransformChanged
        | ChildrenChanged
        | BackfaceVisibilityChanged
        | FiltersChanged
        | BackdropFiltersChanged
        | BlendModeChanged
        | BackdropRootChanged
        | MaskLayerChanged
        | OpacityChanged;

    bool structuralLayerChanged = false;

    if (purpose == NoStructuralLayer) {
        if (RefPtr structuralLayer = m_structuralLayer) {
            // Replace the transformLayer in the parent with this layer.
            RefPtr layer = m_layer;
            layer->removeFromSuperlayer();
 
            // If m_layer doesn't have a parent, it means it's the root layer and
            // is likely hosted by something that is not expecting to be changed
            ASSERT(structuralLayer->superlayer());
            structuralLayer->protectedSuperlayer()->replaceSublayer(*structuralLayer, *layer);

            moveAnimations(structuralLayer.get(), layer.get());

            // Release the structural layer.
            m_structuralLayer = nullptr;
            structuralLayerChanged = true;

            addUncommittedChanges(structuralLayerChangeFlags);
        }
        return structuralLayerChanged;
    }

#if HAVE(MATERIAL_HOSTING)
    if (purpose == StructuralLayerForMaterial) {
        if (m_structuralLayer && m_structuralLayer->layerType() != PlatformCALayer::LayerType::LayerTypeMaterialHostingLayer)
            m_structuralLayer = nullptr;

        if (!m_structuralLayer) {
            m_structuralLayer = createPlatformCALayer(PlatformCALayer::LayerType::LayerTypeMaterialHostingLayer, this);
            structuralLayerChanged = true;
        }
    } else if (purpose == StructuralLayerForPreserves3D) {
#else
    if (purpose == StructuralLayerForPreserves3D) {
#endif
        if (m_structuralLayer && m_structuralLayer->layerType() != PlatformCALayer::LayerType::LayerTypeTransformLayer)
            m_structuralLayer = nullptr;
        
        if (!m_structuralLayer) {
            m_structuralLayer = createPlatformCALayer(PlatformCALayer::LayerType::LayerTypeTransformLayer, this);
            structuralLayerChanged = true;
        }
    } else {
        if (m_structuralLayer && m_structuralLayer->layerType() != PlatformCALayer::LayerType::LayerTypeLayer)
            m_structuralLayer = nullptr;

        if (!m_structuralLayer) {
            m_structuralLayer = createPlatformCALayer(PlatformCALayer::LayerType::LayerTypeLayer, this);
            structuralLayerChanged = true;
        }
    }
    
    if (!structuralLayerChanged)
        return false;
    
    addUncommittedChanges(structuralLayerChangeFlags);

    // We've changed the layer that our parent added to its sublayer list, so tell it to update
    // sublayers again in its commitLayerChangesAfterSublayers().
    downcast<GraphicsLayerCA>(*parent()).noteSublayersChanged(DontScheduleFlush);

    // Set properties of m_layer to their default values, since these are expressed on on the structural layer.
    FloatPoint point(m_size.width() / 2.0f, m_size.height() / 2.0f);
    FloatPoint3D anchorPoint(0.5f, 0.5f, 0);
    RefPtr layer = m_layer;
    layer->setPosition(point);
    layer->setAnchorPoint(anchorPoint);
    layer->setFilters(FilterOperations());
    layer->setTransform(TransformationMatrix());
    layer->setOpacity(1);
    layer->setBlendMode(BlendMode::Normal);
    if (m_layerClones) {
        for (auto& cloneLayer : m_layerClones->primaryLayerClones.values()) {
            cloneLayer->setPosition(point);
            cloneLayer->setAnchorPoint(anchorPoint);
            cloneLayer->setTransform(TransformationMatrix());
            cloneLayer->setOpacity(1);
        }
    }

    moveAnimations(layer.get(), m_structuralLayer.get());
    return true;
}

GraphicsLayerCA::StructuralLayerPurpose GraphicsLayerCA::structuralLayerPurpose() const
{
#if HAVE(MATERIAL_HOSTING)
    if (appleVisualEffectIsHostedMaterial(m_appleVisualEffectData.effect))
        return StructuralLayerForMaterial;
#endif

    if (preserves3D() && m_type != Type::Structural)
        return StructuralLayerForPreserves3D;
    
    if (isReplicated())
        return StructuralLayerForReplicaFlattening;

    if (needsBackdrop())
        return StructuralLayerForBackdrop;

    return NoStructuralLayer;
}

void GraphicsLayerCA::updateDrawsContent()
{
    RefPtr layer = m_layer;
    if (m_drawsContent) {
        layer->setNeedsDisplay();
        m_hasEverPainted = false;
    } else {
        layer->clearContents();
        if (m_layerClones) {
            for (auto& cloneLayer : m_layerClones->primaryLayerClones.values())
                cloneLayer->setContents(nullptr);
        }
    }
}

void GraphicsLayerCA::updateCoverage(const CommitState& commitState)
{
    // FIXME: Need to set coverage on clone layers too.
    if (TiledBacking* backing = tiledBacking()) {
        backing->setVisibleRect(m_visibleRect);
        backing->setCoverageRect(m_coverageRect);
    }

    RefPtr layer = m_layer;
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION) || HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    layer->setVisibleRect(m_visibleRect);
#endif

    bool requiresBacking = m_intersectsCoverageRect
        || !allowsBackingStoreDetaching()
        || commitState.ancestorWithTransformAnimationIntersectsCoverageRect // FIXME: Compute backing exactly for descendants of animating layers.
        || (isRunningTransformAnimation() && !animationExtent()); // Create backing if we don't know the animation extent.

#if !LOG_DISABLED
    if (requiresBacking) {
        auto reasonForBacking = [&] {
            if (m_intersectsCoverageRect)
                return "intersectsCoverageRect"_s;
            
            if (!allowsBackingStoreDetaching())
                return "backing detachment disallowed"_s;

            if (commitState.ancestorWithTransformAnimationIntersectsCoverageRect)
                return "ancestor with transform"_s;
            
            return "has transform animation with unknown extent"_s;
        };
        LOG_WITH_STREAM(Layers, stream << "GraphicsLayerCA "_s << this << " id "_s << primaryLayerID() << " setBackingStoreAttached: "_s << requiresBacking << " ("_s << reasonForBacking() << ')');
    } else
        LOG_WITH_STREAM(Layers, stream << "GraphicsLayerCA "_s << this << " id "_s << primaryLayerID() << " setBackingStoreAttached: "_s << requiresBacking);
#endif

    layer->setBackingStoreAttached(requiresBacking);
    if (m_layerClones) {
        for (auto& layer : m_layerClones->primaryLayerClones.values())
            layer->setBackingStoreAttached(requiresBacking);
    }

    m_sizeAtLastCoverageRectUpdate = m_size;
}

void GraphicsLayerCA::updateAcceleratesDrawing()
{
    protectedLayer()->setAcceleratesDrawing(m_acceleratesDrawing);
}

static void setLayerDebugBorder(PlatformCALayer& layer, Color borderColor, float borderWidth)
{
    layer.setBorderColor(borderColor);
    layer.setBorderWidth(borderColor.isValid() ? borderWidth : 0);
}

static const float contentsLayerBorderWidth = 4;
static Color contentsLayerDebugBorderColor(bool showingBorders)
{
    return showingBorders ? SRGBA<uint8_t> { 0, 0, 128, 180 } : Color { };
}

static const float cloneLayerBorderWidth = 2;
static Color cloneLayerDebugBorderColor(bool showingBorders)
{
    return showingBorders ? SRGBA<uint8_t> { 255, 122, 251 } : Color { };
}

void GraphicsLayerCA::updateDebugIndicators()
{
    Color borderColor;
    float width = 0;

    bool showDebugBorders = isShowingDebugBorder() || isShowingFrameProcessBorders();
    if (showDebugBorders)
        getDebugBorderInfo(borderColor, width);

    // Paint repaint counter.
    RefPtr layer = m_layer;
    layer->setNeedsDisplay();

    setLayerDebugBorder(*layer, borderColor, width);
    if (RefPtr contentsLayer = m_contentsLayer)
        setLayerDebugBorder(*contentsLayer, contentsLayerDebugBorderColor(showDebugBorders), contentsLayerBorderWidth);

    if (m_layerClones) {
        for (auto& layer : m_layerClones->primaryLayerClones.values())
            setLayerDebugBorder(*layer, borderColor, width);

        Color cloneLayerBorderColor = cloneLayerDebugBorderColor(showDebugBorders);
        for (auto& layer : m_layerClones->structuralLayerClones.values())
            setLayerDebugBorder(*layer, cloneLayerBorderColor, cloneLayerBorderWidth);

        Color contentsLayerBorderColor = contentsLayerDebugBorderColor(showDebugBorders);
        for (auto& layer : m_layerClones->contentsLayerClones.values())
            setLayerDebugBorder(*layer, contentsLayerBorderColor, contentsLayerBorderWidth);
    }
}

void GraphicsLayerCA::updateTiles()
{
    if (!m_layer->usesTiledBackingLayer())
        return;

    tiledBacking()->revalidateTiles();
}

void GraphicsLayerCA::updateBackgroundColor()
{
    protectedLayer()->setBackgroundColor(m_backgroundColor);
}

void GraphicsLayerCA::updateContentsImage()
{
    if (m_pendingContentsImage || m_pendingContentsImageBuffer) {
        if (!m_contentsLayer.get()) {
            Ref contentsLayer = createPlatformCALayer(PlatformCALayer::LayerType::LayerTypeLayer, this);
            m_contentsLayer = contentsLayer.copyRef();
#if ENABLE(TREE_DEBUGGING)
            contentsLayer->setName(makeString("contents image "_s, contentsLayer->layerID().object()));
#else
            contentsLayer->setName(MAKE_STATIC_STRING_IMPL("contents image"));
#endif
            setupContentsLayer(contentsLayer.ptr());
            // m_contentsLayer will be parented by updateSublayerList
        }

        // FIXME: maybe only do trilinear if the image is being scaled down,
        // but then what if the layer size changes?
        Ref contentsLayer = *m_contentsLayer;
        contentsLayer->setMinificationFilter(PlatformCALayer::FilterType::Trilinear);

        if (RefPtr pendingContentsImage = std::exchange(m_pendingContentsImage, nullptr))
            contentsLayer->setContents(pendingContentsImage->platformImage().get());
        else
            setLayerContentsToImageBuffer(contentsLayer.ptr(), m_pendingContentsImageBuffer.get());

        if (m_layerClones) {
            for (auto& layer : m_layerClones->contentsLayerClones.values()) {
                if (m_pendingContentsImageBuffer)
                    setLayerContentsToImageBuffer(layer.get(), m_pendingContentsImageBuffer.get());
                else
                    layer->setContents(contentsLayer->contents());
            }
        }

        m_pendingContentsImageBuffer = nullptr;

        updateContentsRects();
    } else {
        // No image.
        // m_contentsLayer will be removed via updateSublayerList.
        m_contentsLayer = nullptr;
    }
}

void GraphicsLayerCA::updateContentsPlatformLayer()
{
    RefPtr contentsLayer = m_contentsLayer;
    if (!contentsLayer)
        return;

    // Platform layer was set as m_contentsLayer, and will get parented in updateSublayerList().
    auto orientation = m_contentsDisplayDelegate ? Ref { *m_contentsDisplayDelegate }->orientation() : defaultContentsOrientation;
    setupContentsLayer(contentsLayer.get(), orientation);

    if (m_contentsLayerPurpose == ContentsLayerPurpose::Canvas)
        contentsLayer->setNeedsDisplay();

    updateContentsRects();
    updateContentsScalingFilters();
}

void GraphicsLayerCA::updateContentsColorLayer()
{
    RefPtr contentsLayer = m_contentsLayer;

    // Color layer was set as m_contentsLayer, and will get parented in updateSublayerList().
    if (!contentsLayer || m_contentsLayerPurpose != ContentsLayerPurpose::BackgroundColor)
        return;

    setupContentsLayer(contentsLayer.get());
    updateContentsRects();
    ASSERT(m_contentsSolidColor.isValid());
    contentsLayer->setBackgroundColor(m_contentsSolidColor);

    if (m_layerClones) {
        for (auto& layer : m_layerClones->contentsLayerClones.values())
            layer->setBackgroundColor(m_contentsSolidColor);
    }
}

// The clipping strategy depends on whether the rounded rect has equal corner radii.
// roundedRect is in the coordinate space of clippingLayer.
void GraphicsLayerCA::updateClippingStrategy(PlatformCALayer& clippingLayer, RefPtr<PlatformCALayer>& shapeMaskLayer, const FloatRoundedRect& roundedRect)
{
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    if (m_isSeparated && roundedRect.radii().hasEvenCorners() && clippingLayer.bounds() == roundedRect.rect()) {
        m_layer->setCornerRadius(roundedRect.radii().topLeft().width());
        return;
    }
    m_layer->setCornerRadius(0);
#endif

    if (roundedRect.radii().isUniformCornerRadius() && clippingLayer.bounds() == roundedRect.rect()) {
        clippingLayer.setMaskLayer(nullptr);
        if (shapeMaskLayer) {
            shapeMaskLayer->setOwner(nullptr);
            shapeMaskLayer = nullptr;
        }

        clippingLayer.setMasksToBounds(true);
        clippingLayer.setCornerRadius(roundedRect.radii().topLeft().width());
        return;
    }

    if (!shapeMaskLayer) {
        shapeMaskLayer = createPlatformCALayer(PlatformCALayer::LayerType::LayerTypeShapeLayer, this);
        shapeMaskLayer->setAnchorPoint({ });
        shapeMaskLayer->setName(MAKE_STATIC_STRING_IMPL("shape mask"));
    }

    // clippingLayer's boundsOrigin is roundedRect.rect().location(), and is non-zero to positioning descendant layers.
    // The mask layer needs an equivalent position.
    auto rectLocation = roundedRect.rect().location();
    shapeMaskLayer->setPosition({ rectLocation.x(), rectLocation.y(), 0.0f });

    auto shapeBounds = FloatRect { { }, roundedRect.rect().size() };
    shapeMaskLayer->setBounds(shapeBounds);
    
    auto localRoundedRect = roundedRect;
    localRoundedRect.setLocation({ });
    shapeMaskLayer->setShapeRoundedRect(localRoundedRect);

    clippingLayer.setCornerRadius(0);
    RefPtr maskLayer = shapeMaskLayer;
    clippingLayer.setMaskLayer(WTFMove(maskLayer));
}

void GraphicsLayerCA::updateContentsRects()
{
    if (!m_contentsLayer && !m_contentsRectClipsDescendants)
        return;

    auto contentBounds = FloatRect { { }, m_contentsRect.size() };
    
    bool gainedOrLostClippingLayer = false;
    if (m_contentsClippingRect.isRounded() || !m_contentsClippingRect.rect().contains(m_contentsRect)) {
        if (!m_contentsClippingLayer) {
            Ref contentsClippingLayer = createPlatformCALayer(PlatformCALayer::LayerType::LayerTypeLayer, this);
            m_contentsClippingLayer = contentsClippingLayer.copyRef();
            contentsClippingLayer->setAnchorPoint({ });
#if ENABLE(TREE_DEBUGGING)
            contentsClippingLayer->setName(makeString("contents clipping "_s, contentsClippingLayer->layerID().object()));
#else
            contentsClippingLayer->setName(MAKE_STATIC_STRING_IMPL("contents clipping"));
#endif
            gainedOrLostClippingLayer = true;
        }

        Ref contentsClippingLayer = *m_contentsClippingLayer;
        contentsClippingLayer->setPosition(m_contentsClippingRect.rect().location());
        contentsClippingLayer->setBounds(m_contentsClippingRect.rect());
        
        updateClippingStrategy(contentsClippingLayer, m_contentsShapeMaskLayer, m_contentsClippingRect);

        if (RefPtr contentsLayer = m_contentsLayer; contentsLayer && gainedOrLostClippingLayer) {
            contentsLayer->removeFromSuperlayer();
            contentsClippingLayer->appendSublayer(*contentsLayer);
        }
    } else {
        if (RefPtr contentsClippingLayer = m_contentsClippingLayer) {
            if (RefPtr contentsLayer = m_contentsLayer)
                contentsLayer->removeFromSuperlayer();

            contentsClippingLayer->removeFromSuperlayer();
            contentsClippingLayer->setOwner(nullptr);
            contentsClippingLayer->setMaskLayer(nullptr);
            m_contentsClippingLayer = nullptr;
            gainedOrLostClippingLayer = true;
        }

        if (m_contentsShapeMaskLayer) {
            m_contentsShapeMaskLayer->setOwner(nullptr);
            m_contentsShapeMaskLayer = nullptr;
        }
    }
    
    if (gainedOrLostClippingLayer)
        noteSublayersChanged(DontScheduleFlush);

    if (RefPtr contentsLayer = m_contentsLayer) {
        contentsLayer->setPosition(m_contentsRect.location());
        contentsLayer->setBounds(contentBounds);
    }

    if (m_layerClones) {
        for (auto& layer : m_layerClones->contentsLayerClones.values()) {
            layer->setPosition(m_contentsRect.location());
            layer->setBounds(contentBounds);
        }

        for (auto& clone : m_layerClones->contentsClippingLayerClones) {
            CloneID cloneID = clone.key;
            RefPtr<PlatformCALayer> shapeMaskLayerClone = m_layerClones->contentsShapeMaskLayerClones.get(cloneID);

            bool hadShapeMask = shapeMaskLayerClone;
            updateClippingStrategy(Ref { *clone.value }, shapeMaskLayerClone, m_contentsClippingRect);

            if (!shapeMaskLayerClone)
                m_layerClones->contentsShapeMaskLayerClones.remove(cloneID);
            else if (shapeMaskLayerClone && !hadShapeMask)
                m_layerClones->contentsShapeMaskLayerClones.add(cloneID, shapeMaskLayerClone);
        }
    }
}

void GraphicsLayerCA::updateEventRegion()
{
    m_layer->setEventRegion(eventRegion());
}

#if ENABLE(SCROLLING_THREAD)
void GraphicsLayerCA::updateScrollingNode()
{
    m_layer->setScrollingNodeID(scrollingNodeID());
}
#endif

void GraphicsLayerCA::updateMaskLayer()
{
    RefPtr<PlatformCALayer> maskCALayer = m_maskLayer ? downcast<GraphicsLayerCA>(*m_maskLayer).primaryLayer() : nullptr;
    
    LayerMap* layerCloneMap;
    if (RefPtr structuralLayer = m_structuralLayer; structuralLayer && structuralLayerPurpose() == StructuralLayerForBackdrop) {
        structuralLayer->setMaskLayer(WTFMove(maskCALayer));
        layerCloneMap = m_layerClones ? &m_layerClones->structuralLayerClones : nullptr;
    } else {
        protectedLayer()->setMaskLayer(WTFMove(maskCALayer));
        layerCloneMap = m_layerClones ? &m_layerClones->primaryLayerClones : nullptr;
    }

    LayerMap* maskLayerCloneMap = m_maskLayer ? downcast<GraphicsLayerCA>(*m_maskLayer).primaryLayerClones() : nullptr;
    if (layerCloneMap) {
        for (auto& clone : *layerCloneMap) {
            RefPtr<PlatformCALayer> maskClone = maskLayerCloneMap ? maskLayerCloneMap->get(clone.key) : nullptr;
            Ref { *clone.value }->setMaskLayer(WTFMove(maskClone));
        }
    }
}

void GraphicsLayerCA::updateReplicatedLayers()
{
    // Clone the descendants of the replicated layer, and parent under us.
    ReplicaState replicaState(ReplicaState::ReplicaBranch);

    RefPtr<PlatformCALayer>replicaRoot = replicatedLayerRoot(replicaState);
    if (!replicaRoot)
        return;

    if (RefPtr structuralLayer = m_structuralLayer)
        structuralLayer->insertSublayer(*replicaRoot, 0);
    else
        protectedLayer()->insertSublayer(*replicaRoot, 0);
}

#if HAVE(SUPPORT_HDR_DISPLAY)
void GraphicsLayerCA::updateDrawsHDRContent()
{
    auto contentsFormat = PlatformCALayer::contentsFormatForLayer(this);
    protectedLayer()->setContentsFormat(contentsFormat);
}

void GraphicsLayerCA::updateTonemappingEnabled()
{
    protectedLayer()->setTonemappingEnabled(m_tonemappingEnabled);
}
#endif

OptionSet<ContentsFormat> GraphicsLayerCA::screenContentsFormats() const
{
    return client().screenContentsFormats();
}

// For now, this assumes that layers only ever have one replica, so replicaIndices contains only 0 and 1.
GraphicsLayerCA::CloneID GraphicsLayerCA::ReplicaState::cloneID() const
{
    size_t depth = m_replicaBranches.size();

    const size_t bitsPerChar16 = sizeof(char16_t) * 8;
    size_t vectorSize = (depth + bitsPerChar16 - 1) / bitsPerChar16;
    
    Vector<char16_t> result(vectorSize, 0);

    // Create a string from the bit sequence which we can use to identify the clone.
    // Note that the string may contain embedded nulls, but that's OK.
    for (size_t i = 0; i < depth; ++i) {
        char16_t& currChar = result[i / bitsPerChar16];
        currChar = (currChar << 1) | m_replicaBranches[i];
    }
    
    return String::adopt(WTFMove(result));
}

RefPtr<PlatformCALayer> GraphicsLayerCA::replicatedLayerRoot(ReplicaState& replicaState)
{
    // Limit replica nesting, to avoid 2^N explosion of replica layers.
    if (!m_replicatedLayer || replicaState.replicaDepth() == ReplicaState::maxReplicaDepth)
        return nullptr;

    Ref replicatedLayer = downcast<GraphicsLayerCA>(*m_replicatedLayer);
    
    RefPtr<PlatformCALayer> clonedLayerRoot = replicatedLayer->fetchCloneLayers(this, replicaState, RootCloneLevel);
    FloatPoint cloneRootPosition = replicatedLayer->positionForCloneRootLayer();

    // Replica root has no offset or transform
    clonedLayerRoot->setPosition(cloneRootPosition);
    clonedLayerRoot->setTransform(TransformationMatrix());

    return clonedLayerRoot;
}

void GraphicsLayerCA::updateAnimations()
{
    // In order to guarantee that transform animations are applied in the expected order (translate, rotate, scale and transform),
    // we need to have them wrapped individually in an animation group because Core Animation sorts animations first by their begin
    // time, and then by the order in which they were added (for those with the same begin time). Since a rotate animation can have
    // an earlier begin time than a translate animation, we cannot rely on adding the animations in the correct order.
    //
    // Having an animation group wrapping each animation means that we can guarantee the order in which animations are applied by
    // ensuring they each have the same begin time. We set this begin time to be the smallest value possible, ensuring that base
    // transform animations are applied continuously. We'll then set the begin time of interpolating animations to be local to the
    // animation group, which means subtracting the group's begin time.

    // We use 1_s here because 0_s would have special meaning in Core Animation, meaning that the animation would have its begin
    // time set to the current time when it's committed.
    auto animationGroupBeginTime = 1_s;
    auto infiniteDuration = std::numeric_limits<double>::max();
    auto currentTime = Seconds(CACurrentMediaTime());

    auto addAnimationGroup = [&](AnimatedProperty property, const Vector<RefPtr<PlatformCAAnimation>>& animations) {
        auto caAnimationGroup = createPlatformCAAnimation(PlatformCAAnimation::AnimationType::Group, PlatformCAAnimation::makeGroupKeyPath());
        caAnimationGroup->setDuration(infiniteDuration);
        caAnimationGroup->setAnimations(animations);

        auto animationGroup = LayerPropertyAnimation(WTFMove(caAnimationGroup), makeString("group-"_s, WTF::UUID::createVersion4()), property, 0, 0_s);
        animationGroup.m_beginTime = animationGroupBeginTime;

        setAnimationOnLayer(animationGroup);
        m_animationGroups.append(WTFMove(animationGroup));
    };

    enum class Additive : bool { No, Yes };
    auto prepareAnimationForAddition = [&](LayerPropertyAnimation& animation, Additive additive = Additive::Yes) {
        auto caAnim = animation.m_animation;
        caAnim->setAdditive(additive == Additive::Yes);
        if (auto beginTime = animation.computedBeginTime())
            caAnim->setBeginTime(beginTime->seconds());

        if (animation.m_playState == PlayState::PausePending || animation.m_playState == PlayState::Paused) {
            caAnim->setSpeed(0);
            caAnim->setTimeOffset(animation.m_timeOffset.seconds());
            animation.m_playState = PlayState::Paused;
        } else
            animation.m_playState = PlayState::Playing;
    };

    auto addLeafAnimation = [&](LayerPropertyAnimation& animation) {
        if (!animation.m_beginTime)
            animation.m_beginTime = currentTime;
        prepareAnimationForAddition(animation, Additive::No);
        setAnimationOnLayer(animation);
    };

    enum class TransformationMatrixSource { UseIdentityMatrix, AskClient };
    auto makeBaseValueTransformAnimation = [&](AnimatedProperty property, TransformationMatrixSource matrixSource = TransformationMatrixSource::AskClient, Seconds beginTimeOfEarliestPropertyAnimation = 0_s) -> LayerPropertyAnimation* {
        // A base value transform animation can either be set to the identity matrix or to read the underlying
        // value from the GraphicsLayerClient. If we didn't explicitly ask for an identity matrix, we can skip
        // the addition of this base value transform animation since it will be a no-op.
        auto matrix = matrixSource == TransformationMatrixSource::UseIdentityMatrix ? TransformationMatrix() : client().transformMatrixForProperty(property);
        if (matrixSource == TransformationMatrixSource::AskClient && matrix.isIdentity())
            return nullptr;

        auto delay = beginTimeOfEarliestPropertyAnimation > currentTime ? beginTimeOfEarliestPropertyAnimation - currentTime : 0_s;

        // A base value transform animation needs to last forever and use the same value for its from and to values,
        // unless we're just filling until an animation for this property starts, in which case it must last for duration
        // of the delay until that animation.
        auto caAnimation = createPlatformCAAnimation(PlatformCAAnimation::AnimationType::Basic, PlatformCAAnimation::makeKeyPath(property));
        caAnimation->setDuration(delay ? delay.seconds() : infiniteDuration);
        caAnimation->setFromValue(matrix);
        caAnimation->setToValue(matrix);

        auto animation = LayerPropertyAnimation(WTFMove(caAnimation), makeString("base-transform-"_s, WTF::UUID::createVersion4()), property, 0, 0_s);
        if (delay)
            animation.m_beginTime = currentTime - animationGroupBeginTime;

        m_baseValueTransformAnimations.append(WTFMove(animation));
        return &m_baseValueTransformAnimations.last();
    };

    auto addBaseValueTransformAnimation = [&](AnimatedProperty property, TransformationMatrixSource matrixSource = TransformationMatrixSource::AskClient, Seconds beginTimeOfEarliestPropertyAnimation = 0_s) {
        // Additivity will depend on the source of the matrix, if it was explicitly provided as an identity matrix, it
        // is the initial base value transform animation and must override the current transform value for this layer.
        // Otherwise, it is meant to apply the underlying value for one specific transform-related property and be additive
        // to be combined with the other base value transform animations and interpolating animations.
        if (auto* animation = makeBaseValueTransformAnimation(property, matrixSource, beginTimeOfEarliestPropertyAnimation)) {
            prepareAnimationForAddition(*animation, matrixSource == TransformationMatrixSource::AskClient ? Additive::Yes : Additive::No);
            addAnimationGroup(animation->m_property, { animation->m_animation });
        }
    };

    // Now, remove all animation groups and leaf animations from the layer so that
    // we no longer have any layer animations.
    for (auto& animation : m_animations)
        removeCAAnimationFromLayer(animation);
    for (auto& animationGroup : m_animationGroups)
        removeCAAnimationFromLayer(animationGroup);

    // We can remove all previously-created base value transform animations and animation groups.
    m_baseValueTransformAnimations.clear();
    m_animationGroups.clear();

    // Now remove all the animations marked as pending removal.
    m_animations.removeAllMatching([&](LayerPropertyAnimation animation) {
        return animation.m_pendingRemoval;
    });

    // Now that our list of animations is current, we can separate animations by property so that
    // we can apply them in order. We only need to apply the last animation applied for a given
    // individual transform property, so we keep a reference to that. For animations targeting
    // the transform property itself, we keep them in order since they all need to apply and build
    // on top of each other. Finally, animations that are not transform-related can be applied
    // right away since their order relative to transform animations does not matter.
    LayerPropertyAnimation* translateAnimation = nullptr;
    LayerPropertyAnimation* scaleAnimation = nullptr;
    LayerPropertyAnimation* rotateAnimation = nullptr;
    Vector<LayerPropertyAnimation*> translateAnimations;
    Vector<LayerPropertyAnimation*> scaleAnimations;
    Vector<LayerPropertyAnimation*> rotateAnimations;
    Vector<LayerPropertyAnimation*> transformAnimations;

    for (auto& animation : m_animations) {
        switch (animation.m_property) {
        case AnimatedProperty::Translate:
            translateAnimation = &animation;
            break;
        case AnimatedProperty::Scale:
            scaleAnimation = &animation;
            break;
        case AnimatedProperty::Rotate:
            rotateAnimation = &animation;
            break;
        case AnimatedProperty::Transform:
            // In the case of animations targeting the "transform" CSS property, there may be several
            // animations created for a single KeyframeEffect, one for each transform component. In that
            // case the animation index starts at 0 and increases for each component. If we encounter an
            // index of 0 this means this animation establishes a new group of animation belonging to a
            // single KeyframeEffect. As such, since the top-most KeyframeEffect replaces the previous
            // ones, we can remove all the previously-added "transform" animations.
            if (!animation.m_index)
                transformAnimations.clear();
            transformAnimations.append(&animation);
            break;
        case AnimatedProperty::Opacity:
        case AnimatedProperty::BackgroundColor:
        case AnimatedProperty::Filter:
        case AnimatedProperty::WebkitBackdropFilter:
            addLeafAnimation(animation);
            break;
        case AnimatedProperty::Invalid:
            ASSERT_NOT_REACHED();
        }
    }

    if (translateAnimation)
        translateAnimations.append(translateAnimation);
    if (scaleAnimation)
        scaleAnimations.append(scaleAnimation);
    if (rotateAnimation)
        rotateAnimations.append(rotateAnimation);

    // Now we can apply the transform-related animations, taking care to add them in the right order
    // (translate/scale/rotate/transform) and generate non-interpolating base value transform animations
    // for each property that is not otherwise interpolated.
    if (!translateAnimations.isEmpty() || !scaleAnimations.isEmpty() || !rotateAnimations.isEmpty() || !transformAnimations.isEmpty()) {
        // Start with a base identity transform to override the transform applied to the layer and have a
        // sound base to add animations on top of with additivity enabled.
        addBaseValueTransformAnimation(AnimatedProperty::Transform, TransformationMatrixSource::UseIdentityMatrix);

        auto addAnimationsForProperty = [&](const Vector<LayerPropertyAnimation*>& animations, AnimatedProperty property) {
            if (animations.isEmpty()) {
                addBaseValueTransformAnimation(property);
                return;
            }

            LayerPropertyAnimation* earliestAnimation = nullptr;
            Vector<RefPtr<PlatformCAAnimation>> caAnimations;
            for (auto* animation : animations | std::views::reverse) {
                if (!animation->m_beginTime)
                    animation->m_beginTime = currentTime - animationGroupBeginTime;
                if (auto beginTime = animation->computedBeginTime()) {
                    if (!earliestAnimation || *earliestAnimation->computedBeginTime() > *beginTime)
                        earliestAnimation = animation;
                }
                prepareAnimationForAddition(*animation);
                caAnimations.append(animation->m_animation);
            }

            // If we have an animation with an explicit begin time that does not fill backwards and starts with a delay,
            // we must create a non-interpolating animation to set the current value for this transform-related property
            // until that animation begins.
            if (earliestAnimation) {
                auto fillMode = Ref { *earliestAnimation->m_animation }->fillMode();
                if (fillMode != PlatformCAAnimation::FillModeType::Backwards && fillMode != PlatformCAAnimation::FillModeType::Both) {
                    Seconds earliestBeginTime = *earliestAnimation->computedBeginTime() + animationGroupBeginTime;
                    if (earliestBeginTime > currentTime) {
                        if (auto* baseValueTransformAnimation = makeBaseValueTransformAnimation(property, TransformationMatrixSource::AskClient, earliestBeginTime)) {
                            prepareAnimationForAddition(*baseValueTransformAnimation);
                            caAnimations.append(baseValueTransformAnimation->m_animation);
                        }
                    }
                }
            }

            addAnimationGroup(property, caAnimations);
        };

        addAnimationsForProperty(transformAnimations, AnimatedProperty::Transform);
        addAnimationsForProperty(scaleAnimations, AnimatedProperty::Scale);
        addAnimationsForProperty(rotateAnimations, AnimatedProperty::Rotate);
        addAnimationsForProperty(translateAnimations, AnimatedProperty::Translate);
    }
}

bool GraphicsLayerCA::isRunningTransformAnimation() const
{
#if ENABLE(THREADED_ANIMATIONS)
    if (RefPtr effectStack = acceleratedEffectStack()) {
        return effectStack->primaryLayerEffects().findIf([](auto& effect) {
            return effect->animatesTransformRelatedProperty();
        }) != notFound;
    }
#endif

    return m_animations.findIf([&](LayerPropertyAnimation animation) {
        return animatedPropertyIsTransformOrRelated(animation.m_property) && (animation.m_playState == PlayState::Playing || animation.m_playState == PlayState::Paused);
    }) != notFound;
}

void GraphicsLayerCA::setAnimationOnLayer(LayerPropertyAnimation& animation)
{
    auto property = animation.m_property;
    RefPtr layer = animatedLayer(property);

    Ref caAnim = *animation.m_animation;

    if (auto beginTime = animation.computedBeginTime())
        caAnim->setBeginTime(beginTime->seconds());

    String animationID = animation.animationIdentifier();

    layer->removeAnimationForKey(animationID);
    layer->addAnimationForKey(animationID, caAnim);

    if (LayerMap* layerCloneMap = animatedLayerClones(property)) {
        for (auto& clone : *layerCloneMap) {
            // Skip immediate replicas, since they move with the original.
            if (m_replicaLayer && isReplicatedRootClone(clone.key))
                continue;

            Ref cloneValue = *clone.value;
            cloneValue->removeAnimationForKey(animationID);
            cloneValue->addAnimationForKey(animationID, caAnim);
        }
    }
}

// Workaround for <rdar://problem/7311367>
static void bug7311367Workaround(PlatformCALayer* transformLayer, const TransformationMatrix& transform)
{
    if (!transformLayer)
        return;

    TransformationMatrix caTransform = transform;
    caTransform.setM41(caTransform.m41() + 1);
    transformLayer->setTransform(caTransform);

    caTransform.setM41(caTransform.m41() - 1);
    transformLayer->setTransform(caTransform);
}

bool GraphicsLayerCA::removeCAAnimationFromLayer(LayerPropertyAnimation& animation)
{
    RefPtr layer = animatedLayer(animation.m_property);

    String animationID = animation.animationIdentifier();

    if (!layer->animationForKey(animationID))
        return false;
    
    layer->removeAnimationForKey(animationID);
    bug7311367Workaround(m_structuralLayer.get(), transform());

    if (LayerMap* layerCloneMap = animatedLayerClones(animation.m_property)) {
        for (auto& clone : *layerCloneMap) {
            // Skip immediate replicas, since they move with the original.
            if (m_replicaLayer && isReplicatedRootClone(clone.key))
                continue;

            Ref { *clone.value }->removeAnimationForKey(animationID);
        }
    }
    return true;
}

void GraphicsLayerCA::pauseCAAnimationOnLayer(LayerPropertyAnimation& animation)
{
    RefPtr layer = animatedLayer(animation.m_property);

    String animationID = animation.animationIdentifier();

    RefPtr<PlatformCAAnimation> curAnim = layer->animationForKey(animationID);
    if (!curAnim)
        return;

    // Animations on the layer are immutable, so we have to clone and modify.
    RefPtr<PlatformCAAnimation> newAnim = curAnim->copy();

    newAnim->setSpeed(0);
    newAnim->setTimeOffset(animation.m_timeOffset.seconds());

    layer->addAnimationForKey(animationID, *newAnim); // This will replace the running animation.

    // Pause the animations on the clones too.
    if (LayerMap* layerCloneMap = animatedLayerClones(animation.m_property)) {
        for (auto& clone : *layerCloneMap) {
            // Skip immediate replicas, since they move with the original.
            if (m_replicaLayer && isReplicatedRootClone(clone.key))
                continue;
            Ref { *clone.value }->addAnimationForKey(animationID, *newAnim);
        }
    }
}

void GraphicsLayerCA::repaintLayerDirtyRects()
{
    RefPtr layer = m_layer;
    if (m_needsFullRepaint) {
        ASSERT(!m_dirtyRects.size());
        layer->setNeedsDisplay();
        m_needsFullRepaint = false;
        return;
    }

    for (auto& dirtyRect : m_dirtyRects)
        layer->setNeedsDisplayInRect(dirtyRect);
    
    m_dirtyRects.clear();
}

void GraphicsLayerCA::updateContentsNeedsDisplay()
{
    if (RefPtr contentsLayer = m_contentsLayer)
        contentsLayer->setNeedsDisplay();
}

static bool isKeyframe(const KeyframeValueList& list)
{
    return list.size() > 1;
}

bool GraphicsLayerCA::createAnimationFromKeyframes(const KeyframeValueList& valueList, const GraphicsLayerAnimation* animation, const String& animationName, Seconds timeOffset, bool keyframesShouldUseAnimationWideTimingFunction)
{
    ASSERT(!animatedPropertyIsTransformOrRelated(valueList.property()) && (!supportsAcceleratedFilterAnimations() || valueList.property() != AnimatedProperty::Filter));

    bool valuesOK;
    
    bool additive = false;
    int animationIndex = 0;
    
    RefPtr<PlatformCAAnimation> caAnimation;

    if (isKeyframe(valueList)) {
        caAnimation = createKeyframeAnimation(animation, PlatformCAAnimation::makeKeyPath(valueList.property()), additive, keyframesShouldUseAnimationWideTimingFunction);
        valuesOK = setAnimationKeyframes(valueList, animation, caAnimation.get(), keyframesShouldUseAnimationWideTimingFunction);
    } else {
        if (animation->timingFunction()->isSpringTimingFunction())
            caAnimation = createSpringAnimation(animation, PlatformCAAnimation::makeKeyPath(valueList.property()), additive, keyframesShouldUseAnimationWideTimingFunction);
        else
            caAnimation = createBasicAnimation(animation, PlatformCAAnimation::makeKeyPath(valueList.property()), additive, keyframesShouldUseAnimationWideTimingFunction);
        valuesOK = setAnimationEndpoints(valueList, animation, caAnimation.get());
    }

    if (!valuesOK)
        return false;

    m_animations.append(LayerPropertyAnimation(caAnimation.releaseNonNull(), animationName, valueList.property(), animationIndex, timeOffset));

    return true;
}

bool GraphicsLayerCA::appendToUncommittedAnimations(const KeyframeValueList& valueList, TransformOperation::Type operationType, const GraphicsLayerAnimation* animation, const String& animationName, unsigned animationIndex, Seconds timeOffset, bool isMatrixAnimation, bool keyframesShouldUseAnimationWideTimingFunction)
{
    RefPtr<PlatformCAAnimation> caAnimation;
    bool validMatrices = true;
    if (isKeyframe(valueList)) {
        caAnimation = createKeyframeAnimation(animation, PlatformCAAnimation::makeKeyPath(valueList.property()), false, keyframesShouldUseAnimationWideTimingFunction);
        validMatrices = setTransformAnimationKeyframes(valueList, animation, caAnimation.get(), animationIndex, operationType, isMatrixAnimation, keyframesShouldUseAnimationWideTimingFunction);
    } else {
        if (animation->timingFunction()->isSpringTimingFunction())
            caAnimation = createSpringAnimation(animation, PlatformCAAnimation::makeKeyPath(valueList.property()), false, keyframesShouldUseAnimationWideTimingFunction);
        else
            caAnimation = createBasicAnimation(animation, PlatformCAAnimation::makeKeyPath(valueList.property()), false, keyframesShouldUseAnimationWideTimingFunction);
        validMatrices = setTransformAnimationEndpoints(valueList, animation, caAnimation.get(), animationIndex, operationType, isMatrixAnimation);
    }
    
    if (!validMatrices)
        return false;

    m_animations.append(LayerPropertyAnimation(caAnimation.releaseNonNull(), animationName, valueList.property(), animationIndex, timeOffset));
    return true;
}

static const TransformOperations& transformationAnimationValueAt(const KeyframeValueList& valueList, unsigned i)
{
    return static_cast<const TransformAnimationValue&>(valueList.at(i)).value();
}

static bool hasBig3DRotation(const KeyframeValueList& valueList, const TransformOperationsSharedPrimitivesPrefix<TransformOperation::Type>& prefix)
{
    // Hardware non-matrix animations are used for every function in the shared primitives prefix.
    // These kind of animations have issues with large rotation angles, so for every function that
    // will be represented as a hardware non-matrix animation, check that for each of those functions
    // the animation that's created for it will not have two consecutive keyframes that have a large
    // rotation angle between them.
    const auto& primitives = prefix.primitives();
    for (unsigned animationIndex = 0; animationIndex < primitives.size(); ++animationIndex) {
        auto type = primitives[animationIndex];
        if (type != TransformOperation::Type::Rotate3D)
            continue;
        for (size_t i = 1; i < valueList.size(); ++i) {
            // Since the shared primitive at this index is a rotation, both of these transform
            // functions should be RotateTransformOperations.
            RefPtr prevOperation = downcast<RotateTransformOperation>(transformationAnimationValueAt(valueList, i - 1).at(animationIndex));
            RefPtr operation = downcast<RotateTransformOperation>(transformationAnimationValueAt(valueList, i).at(animationIndex));
            auto angle = std::abs((prevOperation ? prevOperation->angle() : 0.0) - (operation ? operation->angle() : 0.0));
            if (angle > 180.0)
                return true;
        }
    }

    return false;
}

bool GraphicsLayerCA::createTransformAnimationsFromKeyframes(const KeyframeValueList& valueList, const GraphicsLayerAnimation* animation, const String& animationName, Seconds timeOffset, bool keyframesShouldUseAnimationWideTimingFunction)
{
    ASSERT(animatedPropertyIsTransformOrRelated(valueList.property()));

    // https://www.w3.org/TR/css-transforms-1/#interpolation-of-transforms
    // In the CSS Transform Level 1 and 2 Specification some transform functions can share a compatible transform
    // function primitive. For instance, the shared primitive of a translateX and translate3D operation is
    // TransformOperation::Type::Translate3D. When the transform function list of every keyframe in an animation
    // shares the same transform function primitive, we should interpolate between them without resorting
    // to matrix decomposition. The remaining parts of the transform function list should be interpolated
    // using matrix decomposition. The code below finds the shared primitives in this prefix.
    // FIXME: Currently, this only supports situations where every keyframe shares the same prefix of shared
    // transformation primitives, but the specification says direct interpolation should be determined by
    // the primitives shared between any two adjacent keyframes.
    TransformOperationsSharedPrimitivesPrefix<TransformOperation::Type> prefix;
    for (size_t i = 0; i < valueList.size(); ++i)
        prefix.update(transformationAnimationValueAt(valueList, i));

    // If this animation has a big rotation between two keyframes, fall back to software animation. CoreAnimation
    // will always take the shortest path between two rotations, which will result in incorrect animation when
    // the keyframes specify angles larger than one half rotation.
    if (hasBig3DRotation(valueList, prefix))
        return false;

    const auto& primitives = prefix.primitives();
    unsigned numberOfSharedPrimitives = valueList.size() > 1 ? primitives.size() : 0;

    removeAnimation(animationName, valueList.property());

    for (unsigned animationIndex = 0; animationIndex < numberOfSharedPrimitives; ++animationIndex) {
        if (!appendToUncommittedAnimations(valueList, primitives[animationIndex], animation, animationName, animationIndex, timeOffset, false /* isMatrixAnimation */, keyframesShouldUseAnimationWideTimingFunction))
            return false;
    }

    if (!prefix.hadIncompatibleTransformFunctions())
        return true;

    // If there were any incompatible transform functions, they will be appended to the animation list
    // as a single combined transformation matrix animation.
    return appendToUncommittedAnimations(valueList, TransformOperation::Type::Matrix3D, animation, animationName, primitives.size(), timeOffset, true /* isMatrixAnimation */, keyframesShouldUseAnimationWideTimingFunction);
}

bool GraphicsLayerCA::appendToUncommittedAnimations(const KeyframeValueList& valueList, const FilterOperation& operation, const GraphicsLayerAnimation* animation, const String& animationName, int animationIndex, Seconds timeOffset, bool keyframesShouldUseAnimationWideTimingFunction)
{
    auto filterOp = operation.type();
    if (!PlatformCAFilters::isAnimatedFilterProperty(filterOp))
        return true;

    bool valuesOK;
    RefPtr<PlatformCAAnimation> caAnimation;
    auto keyPath = PlatformCAAnimation::makeKeyPath(AnimatedProperty::Filter, filterOp, animationIndex);

    if (isKeyframe(valueList)) {
        caAnimation = createKeyframeAnimation(animation, keyPath, false, keyframesShouldUseAnimationWideTimingFunction);
        valuesOK = setFilterAnimationKeyframes(valueList, animation, caAnimation.get(), animationIndex, filterOp, keyframesShouldUseAnimationWideTimingFunction);
    } else {
        caAnimation = createBasicAnimation(animation, keyPath, false, keyframesShouldUseAnimationWideTimingFunction);
        valuesOK = setFilterAnimationEndpoints(valueList, animation, caAnimation.get(), animationIndex);
    }

    ASSERT_UNUSED(valuesOK, valuesOK);

    m_animations.append(LayerPropertyAnimation(caAnimation.releaseNonNull(), animationName, valueList.property(), animationIndex, timeOffset));

    return true;
}

bool GraphicsLayerCA::createFilterAnimationsFromKeyframes(const KeyframeValueList& valueList, const GraphicsLayerAnimation* animation, const String& animationName, Seconds timeOffset, bool keyframesShouldUseAnimationWideTimingFunction)
{
    ASSERT(valueList.property() == AnimatedProperty::Filter || valueList.property() == AnimatedProperty::WebkitBackdropFilter);

    int listIndex = validateFilterOperations(valueList);
    if (listIndex < 0)
        return false;

    const FilterOperations& operations = static_cast<const FilterAnimationValue&>(valueList.at(listIndex)).value();

    // FIXME: We can't currently hardware animate shadows.
    if (operations.hasFilterOfType<FilterOperation::Type::DropShadowWithStyleColor>())
        return false;

    // Make sure the platform layer didn't fallback to using software filter compositing instead.
    if (!filtersCanBeComposited(operations))
        return false;

    removeAnimation(animationName, valueList.property());

    int numberOfAnimations = operations.size();
    for (int animationIndex = 0; animationIndex < numberOfAnimations; ++animationIndex) {
        if (!appendToUncommittedAnimations(valueList, operations[animationIndex], animation, animationName, animationIndex, timeOffset, keyframesShouldUseAnimationWideTimingFunction))
            return false;
    }

    return true;
}

Ref<PlatformCAAnimation> GraphicsLayerCA::createBasicAnimation(const GraphicsLayerAnimation* anim, const String& keyPath, bool additive, bool keyframesShouldUseAnimationWideTimingFunction)
{
    auto basicAnim = createPlatformCAAnimation(PlatformCAAnimation::AnimationType::Basic, keyPath);
    setupAnimation(basicAnim.ptr(), anim, additive, keyframesShouldUseAnimationWideTimingFunction);
    return basicAnim;
}

Ref<PlatformCAAnimation> GraphicsLayerCA::createKeyframeAnimation(const GraphicsLayerAnimation* anim, const String& keyPath, bool additive, bool keyframesShouldUseAnimationWideTimingFunction)
{
    auto keyframeAnim = createPlatformCAAnimation(PlatformCAAnimation::AnimationType::Keyframe, keyPath);
    setupAnimation(keyframeAnim.ptr(), anim, additive, keyframesShouldUseAnimationWideTimingFunction);
    return keyframeAnim;
}

Ref<PlatformCAAnimation> GraphicsLayerCA::createSpringAnimation(const GraphicsLayerAnimation* anim, const String& keyPath, bool additive, bool keyframesShouldUseAnimationWideTimingFunction)
{
    auto basicAnim = createPlatformCAAnimation(PlatformCAAnimation::AnimationType::Spring, keyPath);
    setupAnimation(basicAnim.ptr(), anim, additive, keyframesShouldUseAnimationWideTimingFunction);
    return basicAnim;
}

void GraphicsLayerCA::setupAnimation(PlatformCAAnimation* propertyAnim, const GraphicsLayerAnimation* anim, bool additive, bool keyframesShouldUseAnimationWideTimingFunction)
{
    double duration = anim->duration().value_or(0);
    if (duration <= 0)
        duration = cAnimationAlmostZeroDuration;

    float repeatCount = anim->iterationCount();
    if (repeatCount == GraphicsLayerAnimation::IterationCountInfinite)
        repeatCount = std::numeric_limits<float>::max();
    else if (anim->direction() == GraphicsLayerAnimation::Direction::Alternate || anim->direction() == GraphicsLayerAnimation::Direction::AlternateReverse)
        repeatCount /= 2;

    PlatformCAAnimation::FillModeType fillMode = PlatformCAAnimation::FillModeType::NoFillMode;
    switch (anim->fillMode()) {
    case GraphicsLayerAnimation::FillMode::None:
        fillMode = PlatformCAAnimation::FillModeType::Forwards; // Use "forwards" rather than "removed" because the style system will remove the animation when it is finished. This avoids a flash.
        break;
    case GraphicsLayerAnimation::FillMode::Backwards:
        fillMode = PlatformCAAnimation::FillModeType::Both; // Use "both" rather than "backwards" because the style system will remove the animation when it is finished. This avoids a flash.
        break;
    case GraphicsLayerAnimation::FillMode::Forwards:
        fillMode = PlatformCAAnimation::FillModeType::Forwards;
        break;
    case GraphicsLayerAnimation::FillMode::Both:
        fillMode = PlatformCAAnimation::FillModeType::Both;
        break;
    }

    propertyAnim->setDuration(duration);
    propertyAnim->setRepeatCount(repeatCount);
    propertyAnim->setAutoreverses(anim->direction() == GraphicsLayerAnimation::Direction::Alternate || anim->direction() == GraphicsLayerAnimation::Direction::AlternateReverse);
    propertyAnim->setRemovedOnCompletion(false);
    propertyAnim->setAdditive(additive);
    propertyAnim->setFillMode(fillMode);

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=215918
    // A CSS Transition is the only scenario where Animation::property() will have
    // its mode set to SingleProperty. In this case, we don't set the animation-wide
    // timing function to work around a Core Animation limitation.
    if (!keyframesShouldUseAnimationWideTimingFunction)
        propertyAnim->setTimingFunction(anim->timingFunction().get());
}

const TimingFunction& GraphicsLayerCA::timingFunctionForAnimationValue(const AnimationValue& animValue, const GraphicsLayerAnimation& anim, bool keyframesShouldUseAnimationWideTimingFunction)
{
    if (keyframesShouldUseAnimationWideTimingFunction && anim.timingFunction()) {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=215918
        // A CSS Transition is the only scenario where Animation::property() will have
        // its mode set to SingleProperty. In this case, we chose not to set set the
        // animation-wide timing function, so we set it on the single keyframe interval
        // to work around a Core Animation limitation.
        return *anim.timingFunction().unsafeGet();
    }
    if (animValue.timingFunction())
        return *animValue.timingFunction();
    if (anim.defaultTimingFunctionForKeyframes())
        return *anim.defaultTimingFunctionForKeyframes().unsafeGet();
    return LinearTimingFunction::identity();
}

bool GraphicsLayerCA::setAnimationEndpoints(const KeyframeValueList& valueList, const GraphicsLayerAnimation* animation, PlatformCAAnimation* basicAnim)
{
    bool forwards = animation->directionIsForwards();

    unsigned fromIndex = !forwards;
    unsigned toIndex = forwards;
    
    switch (valueList.property()) {
    case AnimatedProperty::Opacity: {
        basicAnim->setFromValue(static_cast<const FloatAnimationValue&>(valueList.at(fromIndex)).value());
        basicAnim->setToValue(static_cast<const FloatAnimationValue&>(valueList.at(toIndex)).value());
        break;
    }
    default:
        ASSERT_NOT_REACHED(); // we don't animate color yet
        break;
    }

    return true;
}

bool GraphicsLayerCA::setAnimationKeyframes(const KeyframeValueList& valueList, const GraphicsLayerAnimation* animation, PlatformCAAnimation* keyframeAnim, bool keyframesShouldUseAnimationWideTimingFunction)
{
    Vector<float> keyTimes;
    Vector<float> values;
    Vector<Ref<const TimingFunction>> timingFunctions;

    bool forwards = animation->directionIsForwards();
    
    for (unsigned i = 0; i < valueList.size(); ++i) {
        unsigned index = forwards ? i : (valueList.size() - i - 1);
        const AnimationValue& curValue = valueList.at(index);
        keyTimes.append(forwards ? curValue.keyTime() : (1 - curValue.keyTime()));

        switch (valueList.property()) {
        case AnimatedProperty::Opacity: {
            const FloatAnimationValue& floatValue = static_cast<const FloatAnimationValue&>(curValue);
            values.append(floatValue.value());
            break;
        }
        default:
            ASSERT_NOT_REACHED(); // we don't animate color yet
            break;
        }

        if (i < (valueList.size() - 1))
            timingFunctions.append(Ref { timingFunctionForAnimationValue(forwards ? curValue : valueList.at(index - 1), *animation, keyframesShouldUseAnimationWideTimingFunction) });
    }

    keyframeAnim->setKeyTimes(keyTimes);
    keyframeAnim->setValues(values);
    keyframeAnim->setTimingFunctions(timingFunctions, !forwards);

    return true;
}

bool GraphicsLayerCA::setTransformAnimationEndpoints(const KeyframeValueList& valueList, const GraphicsLayerAnimation* animation, PlatformCAAnimation* basicAnim, int functionIndex, TransformOperation::Type transformOpType, bool isMatrixAnimation)
{
    ASSERT(valueList.size() == 2);

    bool forwards = animation->directionIsForwards();
    
    unsigned fromIndex = !forwards;
    unsigned toIndex = forwards;
    
    const auto& startValue = transformationAnimationValueAt(valueList, fromIndex);
    const auto& endValue = transformationAnimationValueAt(valueList, toIndex);

    if (isMatrixAnimation) {
        TransformationMatrix fromTransform, toTransform;
        startValue.apply(fromTransform);
        endValue.apply(toTransform);

        // If any matrix is singular, CA won't animate it correctly. So fall back to software animation
        if (!fromTransform.isInvertible() || !toTransform.isInvertible())
            return false;

        basicAnim->setFromValue(fromTransform);
        basicAnim->setToValue(toTransform);
    } else {
        if (isTransformTypeNumber(transformOpType)) {
            float fromValue;
            getTransformFunctionValue(RefPtr { startValue.at(functionIndex) }.get(), transformOpType, fromValue);
            basicAnim->setFromValue(fromValue);
            
            float toValue;
            getTransformFunctionValue(RefPtr { endValue.at(functionIndex) }.get(), transformOpType, toValue);
            basicAnim->setToValue(toValue);
        } else if (isTransformTypeFloatPoint3D(transformOpType)) {
            FloatPoint3D fromValue;
            getTransformFunctionValue(RefPtr { startValue.at(functionIndex) }.get(), transformOpType, fromValue);
            basicAnim->setFromValue(fromValue);
            
            FloatPoint3D toValue;
            getTransformFunctionValue(RefPtr { endValue.at(functionIndex) }.get(), transformOpType, toValue);
            basicAnim->setToValue(toValue);
        } else {
            TransformationMatrix fromValue;
            getTransformFunctionValue(RefPtr { startValue.at(functionIndex) }.get(), transformOpType, fromValue);
            basicAnim->setFromValue(fromValue);

            TransformationMatrix toValue;
            getTransformFunctionValue(RefPtr { endValue.at(functionIndex) }.get(), transformOpType, toValue);
            basicAnim->setToValue(toValue);
        }
    }

    auto valueFunction = getValueFunctionNameForTransformOperation(transformOpType);
    if (valueFunction != PlatformCAAnimation::ValueFunctionType::NoValueFunction)
        basicAnim->setValueFunction(valueFunction);

    return true;
}

bool GraphicsLayerCA::setTransformAnimationKeyframes(const KeyframeValueList& valueList, const GraphicsLayerAnimation* animation, PlatformCAAnimation* keyframeAnim, int functionIndex, TransformOperation::Type transformOpType, bool isMatrixAnimation, bool keyframesShouldUseAnimationWideTimingFunction)
{
    Vector<float> keyTimes;
    Vector<float> floatValues;
    Vector<FloatPoint3D> floatPoint3DValues;
    Vector<TransformationMatrix> transformationMatrixValues;
    Vector<Ref<const TimingFunction>> timingFunctions;

    bool forwards = animation->directionIsForwards();

    for (unsigned i = 0; i < valueList.size(); ++i) {
        unsigned index = forwards ? i : (valueList.size() - i - 1);
        const TransformAnimationValue& curValue = static_cast<const TransformAnimationValue&>(valueList.at(index));
        keyTimes.append(forwards ? curValue.keyTime() : (1 - curValue.keyTime()));

        if (isMatrixAnimation) {
            TransformationMatrix transform;
            curValue.value().apply(transform, functionIndex);

            // If any matrix is singular, CA won't animate it correctly. So fall back to software animation
            if (!transform.isInvertible())
                return false;

            transformationMatrixValues.append(transform);
        } else {
            RefPtr transformOp = curValue.value().at(functionIndex);
            if (isTransformTypeNumber(transformOpType)) {
                float value;
                getTransformFunctionValue(transformOp.get(), transformOpType, value);
                floatValues.append(value);
            } else if (isTransformTypeFloatPoint3D(transformOpType)) {
                FloatPoint3D value;
                getTransformFunctionValue(transformOp.get(), transformOpType, value);
                floatPoint3DValues.append(value);
            } else {
                TransformationMatrix value;
                getTransformFunctionValue(transformOp.get(), transformOpType, value);
                transformationMatrixValues.append(value);
            }
        }

        if (i < (valueList.size() - 1))
            timingFunctions.append(Ref { timingFunctionForAnimationValue(forwards ? curValue : valueList.at(index - 1), *animation, keyframesShouldUseAnimationWideTimingFunction) });
    }
    
    keyframeAnim->setKeyTimes(keyTimes);
    
    if (isTransformTypeNumber(transformOpType))
        keyframeAnim->setValues(floatValues);
    else if (isTransformTypeFloatPoint3D(transformOpType))
        keyframeAnim->setValues(floatPoint3DValues);
    else
        keyframeAnim->setValues(transformationMatrixValues);
        
    keyframeAnim->setTimingFunctions(timingFunctions, !forwards);

    PlatformCAAnimation::ValueFunctionType valueFunction = getValueFunctionNameForTransformOperation(transformOpType);
    if (valueFunction != PlatformCAAnimation::ValueFunctionType::NoValueFunction)
        keyframeAnim->setValueFunction(valueFunction);

    return true;
}

bool GraphicsLayerCA::setFilterAnimationEndpoints(const KeyframeValueList& valueList, const GraphicsLayerAnimation* animation, PlatformCAAnimation* basicAnim, int functionIndex)
{
    ASSERT(valueList.size() == 2);

    bool forwards = animation->directionIsForwards();

    unsigned fromIndex = !forwards;
    unsigned toIndex = forwards;

    const FilterAnimationValue& fromValue = static_cast<const FilterAnimationValue&>(valueList.at(fromIndex));
    const FilterAnimationValue& toValue = static_cast<const FilterAnimationValue&>(valueList.at(toIndex));

    RefPtr fromOperation = fromValue.value().at(functionIndex);
    RefPtr toOperation = toValue.value().at(functionIndex);

    RefPtr<DefaultFilterOperation> defaultFromOperation;
    RefPtr<DefaultFilterOperation> defaultToOperation;

    ASSERT(fromOperation || toOperation);

    if (!fromOperation) {
        defaultFromOperation = DefaultFilterOperation::create(toOperation->type());
        fromOperation = defaultFromOperation.get();
    }

    if (!toOperation) {
        defaultToOperation = DefaultFilterOperation::create(fromOperation->type());
        toOperation = defaultToOperation.get();
    }

    basicAnim->setFromValue(*fromOperation);
    basicAnim->setToValue(*toOperation);

    return true;
}

bool GraphicsLayerCA::setFilterAnimationKeyframes(const KeyframeValueList& valueList, const GraphicsLayerAnimation* animation, PlatformCAAnimation* keyframeAnim, int functionIndex, FilterOperation::Type filterOp, bool keyframesShouldUseAnimationWideTimingFunction)
{
    Vector<float> keyTimes;
    Vector<Ref<FilterOperation>> values;
    Vector<Ref<const TimingFunction>> timingFunctions;
    RefPtr<DefaultFilterOperation> defaultOperation;

    bool forwards = animation->directionIsForwards();

    for (unsigned i = 0; i < valueList.size(); ++i) {
        unsigned index = forwards ? i : (valueList.size() - i - 1);
        const FilterAnimationValue& curValue = static_cast<const FilterAnimationValue&>(valueList.at(index));
        keyTimes.append(forwards ? curValue.keyTime() : (1 - curValue.keyTime()));

        if (curValue.value().size() > static_cast<size_t>(functionIndex))
            values.append(curValue.value()[functionIndex].copyRef());
        else {
            if (!defaultOperation)
                defaultOperation = DefaultFilterOperation::create(filterOp);
            values.append(*defaultOperation);
        }

        if (i < (valueList.size() - 1))
            timingFunctions.append(Ref { timingFunctionForAnimationValue(forwards ? curValue : valueList.at(index - 1), *animation, keyframesShouldUseAnimationWideTimingFunction) });
    }
    
    keyframeAnim->setKeyTimes(keyTimes);
    keyframeAnim->setValues(values);
    keyframeAnim->setTimingFunctions(timingFunctions, !forwards);

    return true;
}

void GraphicsLayerCA::suspendAnimations(MonotonicTime time)
{
    double t = PlatformCALayer::currentTimeToMediaTime(time ? time : MonotonicTime::now());
    RefPtr primaryLayer = this->primaryLayer();
    primaryLayer->setSpeed(0);
    primaryLayer->setTimeOffset(t);

    // Suspend the animations on the clones too.
    if (LayerMap* layerCloneMap = primaryLayerClones()) {
        for (auto& layer : layerCloneMap->values()) {
            layer->setSpeed(0);
            layer->setTimeOffset(t);
        }
    }
}

void GraphicsLayerCA::resumeAnimations()
{
    RefPtr primaryLayer = this->primaryLayer();
    primaryLayer->setSpeed(1);
    primaryLayer->setTimeOffset(0);

    // Resume the animations on the clones too.
    if (LayerMap* layerCloneMap = primaryLayerClones()) {
        for (auto& layer : layerCloneMap->values()) {
            layer->setSpeed(1);
            layer->setTimeOffset(0);
        }
    }
}

PlatformCALayer* GraphicsLayerCA::hostLayerForSublayers() const
{
    if (contentsRectClipsDescendants() && m_contentsClippingLayer)
        return m_contentsClippingLayer.get();

    if (m_structuralLayer)
        return m_structuralLayer.get();

    return m_layer.get();
}

PlatformCALayer* GraphicsLayerCA::layerForSuperlayer() const
{
    return m_structuralLayer ? m_structuralLayer.get() : m_layer.get();
}

PlatformCALayer* GraphicsLayerCA::animatedLayer(AnimatedProperty property) const
{
    switch (property) {
    case AnimatedProperty::BackgroundColor:
        return m_contentsLayer.get();
    case AnimatedProperty::WebkitBackdropFilter:
        // FIXME: Should be just m_backdropLayer.get(). Also, add an ASSERT(m_backdropLayer) here when https://bugs.webkit.org/show_bug.cgi?id=145322 is fixed.
        return m_backdropLayer ? m_backdropLayer.get() : primaryLayer();
    default:
        return primaryLayer();
    }
}

GraphicsLayerCA::LayerMap* GraphicsLayerCA::primaryLayerClones() const
{
    if (!m_layerClones)
        return nullptr;

    return m_structuralLayer ? &m_layerClones->structuralLayerClones : &m_layerClones->primaryLayerClones;
}

GraphicsLayerCA::LayerMap* GraphicsLayerCA::animatedLayerClones(AnimatedProperty property) const
{
    if (!m_layerClones)
        return nullptr;

    return (property == AnimatedProperty::BackgroundColor) ? &m_layerClones->contentsLayerClones : primaryLayerClones();
}

void GraphicsLayerCA::updateRootRelativeScale()
{
    // For CSS animations we could figure out the max scale level during the animation and only figure out the max content scale once.
    // For JS driven animation, we need to be more clever to keep the peformance as before. Ideas:
    // - only update scale factor when the change is 'significant' (to be defined, (orig - new)/orig > delta?)
    // - never update the scale factor when it gets smaller (unless we're under memory pressure) (or only periodcally)
    // - ...
    // --> For now we disable this logic alltogether, but allow to turn it on selectively (for LBSE)
    if (!m_shouldUpdateRootRelativeScaleFactor)
        return;

    auto computeMaxScaleFromTransform = [](const TransformationMatrix& transform) -> float {
        if (transform.isIdentityOrTranslation())
            return 1;
        TransformationMatrix::Decomposed2Type decomposeData;
        if (!transform.decompose2(decomposeData))
            return 1;
        return std::max(std::abs(decomposeData.scaleX), std::abs(decomposeData.scaleY));
    };

    float rootRelativeScaleFactor = hasNonIdentityTransform() ? computeMaxScaleFromTransform(transform()) : 1;
    if (RefPtr parent = m_parent) {
        if (parent->hasNonIdentityChildrenTransform())
            rootRelativeScaleFactor *= computeMaxScaleFromTransform(parent->childrenTransform());
        rootRelativeScaleFactor *= downcast<GraphicsLayerCA>(*parent).rootRelativeScaleFactor();
    }

    if (rootRelativeScaleFactor != m_rootRelativeScaleFactor) {
        m_rootRelativeScaleFactor = rootRelativeScaleFactor;
        m_uncommittedChanges |= ContentsScaleChanged;
    }
}

void GraphicsLayerCA::updateContentsScale(float pageScaleFactor)
{
    float contentsScale = pageScaleFactor * deviceScaleFactor() * m_contentsScaleLimitingFactor;

    if (isPageTiledBackingLayer() && tiledBacking()) {
        float zoomedOutScale = client().zoomedOutPageScaleFactor() * deviceScaleFactor();
        tiledBacking()->setZoomedOutContentsScale(zoomedOutScale);
    }

    if (auto customScale = client().customContentsScale(*this))
        contentsScale = *customScale;

    RefPtr layer = m_layer;
    if (contentsScale == layer->contentsScale())
        return;

    layer->setContentsScale(contentsScale);

    if (RefPtr contentsLayer = m_contentsLayer; contentsLayer && m_contentsLayerPurpose == ContentsLayerPurpose::Media)
        contentsLayer->setContentsScale(contentsScale);

    if (tiledBacking()) {
        // Tiled backing repaints automatically on scale change.
        return;
    }

    if (drawsContent())
        layer->setNeedsDisplay();
}

void GraphicsLayerCA::updateCustomAppearance()
{
    protectedLayer()->updateCustomAppearance(m_customAppearance);
}

void GraphicsLayerCA::setShowDebugBorder(bool showBorder)
{
    if (showBorder == m_showDebugBorder)
        return;

    GraphicsLayer::setShowDebugBorder(showBorder);
    noteLayerPropertyChanged(DebugIndicatorsChanged);
}

void GraphicsLayerCA::setShowRepaintCounter(bool showCounter)
{
    if (showCounter == m_showRepaintCounter)
        return;

    GraphicsLayer::setShowRepaintCounter(showCounter);
    noteLayerPropertyChanged(DebugIndicatorsChanged);
}

void GraphicsLayerCA::setShowFrameProcessBorders(bool showBorders)
{
    if (showBorders == m_showFrameProcessBorders)
        return;

    GraphicsLayer::setShowFrameProcessBorders(showBorders);
    noteLayerPropertyChanged(DebugIndicatorsChanged);
}

String GraphicsLayerCA::displayListAsText(OptionSet<DisplayList::AsTextFlag> flags) const
{
    if (!m_displayList)
        return String();

    return m_displayList->asText(flags);
}

void GraphicsLayerCA::setAllowsBackingStoreDetaching(bool allowDetaching)
{
    if (allowDetaching == m_allowsBackingStoreDetaching)
        return;

    m_allowsBackingStoreDetaching = allowDetaching;
    noteLayerPropertyChanged(CoverageRectChanged);
}

void GraphicsLayerCA::setIsTrackingDisplayListReplay(bool isTracking)
{
    if (isTracking == m_isTrackingDisplayListReplay)
        return;

    m_isTrackingDisplayListReplay = isTracking;
    if (!m_isTrackingDisplayListReplay)
        layerDisplayListMap().remove(this);
}

String GraphicsLayerCA::replayDisplayListAsText(OptionSet<DisplayList::AsTextFlag> flags) const
{
    auto it = layerDisplayListMap().find(this);
    if (it != layerDisplayListMap().end()) {
        TextStream stream(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect);
        
        TextStream::GroupScope scope(stream);
        stream.dumpProperty("clip"_s, it->value.first);
        stream << it->value.second->asText(flags);
        return stream.release();
        
    }

    return String();
}

void GraphicsLayerCA::setDebugBackgroundColor(const Color& color)
{    
    if (color.isValid())
        protectedLayer()->setBackgroundColor(color);
    else
        protectedLayer()->setBackgroundColor(Color::transparentBlack);
}

Color GraphicsLayerCA::pageTiledBackingBorderColor() const
{
    return SRGBA<uint8_t> { 0, 0, 128, 128 }; // tile cache layer: dark blue
}

void GraphicsLayerCA::getDebugBorderInfo(Color& color, float& width) const
{
    if (isPageTiledBackingLayer()) {
        color = pageTiledBackingBorderColor();
        width = 0.5;
        return;
    }

    GraphicsLayer::getDebugBorderInfo(color, width);
}

ASCIILiteral GraphicsLayerCA::purposeNameForInnerLayer(PlatformCALayer& layer) const
{
    if (&layer == m_structuralLayer.get())
        return "structural layer"_s;
    if (&layer == m_contentsClippingLayer.get())
        return "contents clipping layer"_s;
    if (&layer == m_shapeMaskLayer.get())
        return "shape mask layer"_s;
    if (&layer == m_backdropClippingLayer.get())
        return "backdrop clipping layer"_s;
    if (&layer == m_contentsLayer.get()) {
        switch (m_contentsLayerPurpose) {
        case ContentsLayerPurpose::None:
            return "contents layer (none)"_s;
        case ContentsLayerPurpose::Image:
            return "contents layer (image)"_s;
        case ContentsLayerPurpose::Media:
            return "contents layer (media)"_s;
        case ContentsLayerPurpose::Canvas:
            return "contents layer (canvas)"_s;
        case ContentsLayerPurpose::BackgroundColor:
            return "contents layer (background color)"_s;
        case ContentsLayerPurpose::Plugin:
            return "contents layer (plugin)"_s;
        case ContentsLayerPurpose::Model:
            return "contents layer (model)"_s;
        case ContentsLayerPurpose::HostedModel:
            return "contents layer (hosted model)"_s;
        case ContentsLayerPurpose::Host:
            return "contents layer (host)"_s;
        }
    }
    if (&layer == m_contentsShapeMaskLayer.get())
        return "contents shape mask layer"_s;
    if (&layer == m_backdropLayer.get()) {
#if HAVE(CORE_MATERIAL)
        if (protectedBackdropLayer()->appleVisualEffectData().effect != AppleVisualEffect::None)
            return "backdrop layer (material)"_s;
#endif
        return "backdrop layer"_s;
    }
    return "platform layer"_s;
}

void GraphicsLayerCA::dumpInnerLayer(TextStream& ts, PlatformCALayer* layer, OptionSet<PlatformLayerTreeAsTextFlags> flags) const
{
    if (!layer)
        return;

    ts << indent << '(' << purposeNameForInnerLayer(*layer) << '\n';

    {
        TextStream::IndentScope indentScope(ts);

        if (flags.contains(PlatformLayerTreeAsTextFlags::Debug))
            ts << indent << "(id "_s << layer->layerID() << ")\n"_s;

        ts << indent << "(position "_s << layer->position().x() << ' ' << layer->position().y() << ")\n"_s;
        ts << indent << "(bounds "_s << layer->bounds().width() << ' ' << layer->bounds().height() << ")\n"_s;
        
        if (layer->opacity() != 1)
            ts << indent << "(opacity "_s << layer->opacity() << ")\n"_s;

        if (layer->isHidden())
            ts << indent << "(hidden)\n"_s;

        layer->dumpAdditionalProperties(ts, flags);

        if (!flags.contains(PlatformLayerTreeAsTextFlags::IgnoreChildren)) {
            auto sublayers = layer->sublayersForLogging();
            if (sublayers.size()) {
                ts << indent << "(children "_s << '\n';

                {
                    TextStream::IndentScope indentScope(ts);
                    for (auto& child : sublayers)
                        dumpInnerLayer(ts, child.get(), flags);
                }

                ts << indent << ")\n"_s;
            }
        }
    }

    ts << indent << ")\n"_s;
}

static TextStream& operator<<(TextStream& textStream, AnimatedProperty propertyID)
{
    switch (propertyID) {
    case AnimatedProperty::Invalid: textStream << "invalid"; break;
    case AnimatedProperty::Translate: textStream << "translate"; break;
    case AnimatedProperty::Scale: textStream << "scale"; break;
    case AnimatedProperty::Rotate: textStream << "rotate"; break;
    case AnimatedProperty::Transform: textStream << "transform"; break;
    case AnimatedProperty::Opacity: textStream << "opacity"; break;
    case AnimatedProperty::BackgroundColor: textStream << "background-color"; break;
    case AnimatedProperty::Filter: textStream << "filter"; break;
    case AnimatedProperty::WebkitBackdropFilter: textStream << "backdrop-filter"; break;
    }
    return textStream;
}

void GraphicsLayerCA::dumpAnimations(WTF::TextStream& textStream, ASCIILiteral category, const Vector<LayerPropertyAnimation>& animations)
{
    auto dumpAnimation = [&textStream](const LayerPropertyAnimation& animation) {
        textStream << indent << '(' << animation.m_name;
        {
            TextStream::IndentScope indentScope(textStream);
            textStream.dumpProperty("CA animation"_s, animation.m_animation.get());
            textStream.dumpProperty("property"_s, animation.m_property);
            textStream.dumpProperty("index"_s, animation.m_index);
            textStream.dumpProperty("time offset"_s, animation.m_timeOffset);
            textStream.dumpProperty("begin time"_s, animation.m_beginTime);
            textStream.dumpProperty("play state"_s, (unsigned)animation.m_playState);
            if (animation.m_pendingRemoval)
                textStream.dumpProperty("pending removal"_s, animation.m_pendingRemoval);
            textStream << ')';
        }
    };
    
    if (animations.isEmpty())
        return;
    
    textStream << indent << '(' << category << '\n';
    {
        TextStream::IndentScope indentScope(textStream);
        for (const auto& animation : animations) {
            TextStream::IndentScope indentScope(textStream);
            dumpAnimation(animation);
        }

        textStream << ")\n"_s;
    }
}

ASCIILiteral GraphicsLayerCA::layerChangeAsString(LayerChange layerChange)
{
    switch (layerChange) {
    case LayerChange::NoChange: return ""_s; break;
    case LayerChange::NameChanged: return "NameChanged"_s;
    case LayerChange::ChildrenChanged: return "ChildrenChanged"_s;
    case LayerChange::GeometryChanged: return "GeometryChanged"_s;
    case LayerChange::TransformChanged: return "TransformChanged"_s;
    case LayerChange::ChildrenTransformChanged: return "ChildrenTransformChanged"_s;
    case LayerChange::Preserves3DChanged: return "Preserves3DChanged"_s;
    case LayerChange::MasksToBoundsChanged: return "MasksToBoundsChanged"_s;
    case LayerChange::DrawsContentChanged: return "DrawsContentChanged"_s;
    case LayerChange::BackgroundColorChanged: return "BackgroundColorChanged"_s;
    case LayerChange::ContentsOpaqueChanged: return "ContentsOpaqueChanged"_s;
    case LayerChange::BackfaceVisibilityChanged: return "BackfaceVisibilityChanged"_s;
    case LayerChange::OpacityChanged: return "OpacityChanged"_s;
    case LayerChange::AnimationChanged: return "AnimationChanged"_s;
    case LayerChange::DirtyRectsChanged: return "DirtyRectsChanged"_s;
    case LayerChange::ContentsImageChanged: return "ContentsImageChanged"_s;
    case LayerChange::ContentsPlatformLayerChanged: return "ContentsPlatformLayerChanged"_s;
    case LayerChange::ContentsColorLayerChanged: return "ContentsColorLayerChanged"_s;
    case LayerChange::ContentsRectsChanged: return "ContentsRectsChanged"_s;
    case LayerChange::MaskLayerChanged: return "MaskLayerChanged"_s;
    case LayerChange::ReplicatedLayerChanged: return "ReplicatedLayerChanged"_s;
    case LayerChange::ContentsNeedsDisplay: return "ContentsNeedsDisplay"_s;
    case LayerChange::AcceleratesDrawingChanged: return "AcceleratesDrawingChanged"_s;
    case LayerChange::ContentsScaleChanged: return "ContentsScaleChanged"_s;
    case LayerChange::ContentsVisibilityChanged: return "ContentsVisibilityChanged"_s;
    case LayerChange::CoverageRectChanged: return "CoverageRectChanged"_s;
    case LayerChange::FiltersChanged: return "FiltersChanged"_s;
    case LayerChange::BackdropFiltersChanged: return "BackdropFiltersChanged"_s;
    case LayerChange::BackdropFiltersRectChanged: return "BackdropFiltersRectChanged"_s;
    case LayerChange::TilingAreaChanged: return "TilingAreaChanged"_s;
    case LayerChange::DebugIndicatorsChanged: return "DebugIndicatorsChanged"_s;
    case LayerChange::CustomAppearanceChanged: return "CustomAppearanceChanged"_s;
    case LayerChange::BlendModeChanged: return "BlendModeChanged"_s;
    case LayerChange::ShapeChanged: return "ShapeChanged"_s;
    case LayerChange::WindRuleChanged: return "WindRuleChanged"_s;
    case LayerChange::UserInteractionEnabledChanged: return "UserInteractionEnabledChanged"_s;
    case LayerChange::NeedsComputeVisibleAndCoverageRect: return "NeedsComputeVisibleAndCoverageRect"_s;
    case LayerChange::EventRegionChanged: return "EventRegionChanged"_s;
#if ENABLE(SCROLLING_THREAD)
    case LayerChange::ScrollingNodeChanged: return "ScrollingNodeChanged"_s;
#endif
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    case LayerChange::SeparatedChanged: return "SeparatedChanged"_s;
#if HAVE(CORE_ANIMATION_SEPARATED_PORTALS)
    case LayerChange::SeparatedPortalChanged: return "SeparatedPortalChanged"_s;
    case LayerChange::DescendentOfSeparatedPortalChanged: return "DescendentOfSeparatedPortalChanged"_s;
#endif
#endif
    case LayerChange::ContentsScalingFiltersChanged: return "ContentsScalingFiltersChanged"_s;
    case LayerChange::VideoGravityChanged: return "VideoGravityChanged"_s;
    case LayerChange::BackdropRootChanged: return "BackdropRootChanged"_s;
#if HAVE(CORE_MATERIAL)
    case LayerChange::AppleVisualEffectChanged: return "AppleVisualEffectChanged"_s;
#endif
#if HAVE(SUPPORT_HDR_DISPLAY)
    case LayerChange::DrawsHDRContentChanged: return "DrawsHDRContentChanged"_s;
    case LayerChange::TonemappingEnabledChanged: return "TonemappingEnabledChanged"_s;
#endif
    }
    ASSERT_NOT_REACHED();
    return ""_s;
}

void GraphicsLayerCA::dumpLayerChangeFlags(TextStream& textStream, LayerChangeFlags layerChangeFlags)
{
    textStream << '{';
    uint64_t bit = 1;
    bool first = true;
    while (layerChangeFlags) {
        if (layerChangeFlags & bit) {
            textStream << (first ? " "_s : ", "_s) << layerChangeAsString(static_cast<LayerChange>(bit));
            first = false;
        }
        layerChangeFlags &= ~bit;
        bit <<= 1;
    }
    textStream << " }"_s;
}

void GraphicsLayerCA::dumpAdditionalProperties(TextStream& textStream, OptionSet<LayerTreeAsTextOptions> options) const
{
    RefPtr layer = m_layer;
    if (options & LayerTreeAsTextOptions::IncludeVisibleRects) {
        textStream << indent << "(visible rect " << m_visibleRect.x() << ", " << m_visibleRect.y() << " " << m_visibleRect.width() << " x " << m_visibleRect.height() << ")\n";
        textStream << indent << "(coverage rect " << m_coverageRect.x() << ", " << m_coverageRect.y() << " " << m_coverageRect.width() << " x " << m_coverageRect.height() << ")\n";
        textStream << indent << "(intersects coverage rect " << m_intersectsCoverageRect << ")\n";
        textStream << indent << "(contentsScale " << layer->contentsScale() << ")\n";
        if (m_contentsScaleLimitingFactor != 1)
            textStream << indent << "(contentsScale limiting factor " << m_contentsScaleLimitingFactor << ")\n";
    }

    if (tiledBacking() && (options & LayerTreeAsTextOptions::IncludeTileCaches)) {
        if (options & LayerTreeAsTextOptions::Debug)
            textStream << indent << "(tiled backing " << tiledBacking() << ")\n";

        IntRect tileCoverageRect = tiledBacking()->tileCoverageRect();
        textStream << indent << "(tile cache coverage " << tileCoverageRect.x() << ", " << tileCoverageRect.y() << " " << tileCoverageRect.width() << " x " << tileCoverageRect.height() << ")\n";

        IntSize tileSize = tiledBacking()->tileSize();
        textStream << indent << "(tile size " << tileSize.width() << " x " << tileSize.height() << ")\n";
        
        IntRect gridExtent = tiledBacking()->tileGridExtent();
        textStream << indent << "(top left tile " << gridExtent.x() << ", " << gridExtent.y() << " tiles grid " << gridExtent.width() << " x " << gridExtent.height() << ")\n";

        textStream << indent << "(in window " << tiledBacking()->isInWindow() << ")\n";
    }

    if (options & LayerTreeAsTextOptions::IncludeDeviceScale)
        textStream << indent << "(device scale " << deviceScaleFactor() << ")\n";

    if ((options & LayerTreeAsTextOptions::IncludeExtendedColor) && layer->contentsFormat() != ContentsFormat::RGBA8)
        textStream << indent << "(contentsFormat " << layer->contentsFormat() << ")\n";

    if (options & LayerTreeAsTextOptions::IncludeContentLayers) {
        OptionSet<PlatformLayerTreeAsTextFlags> platformFlags = { PlatformLayerTreeAsTextFlags::IgnoreChildren };
        if (options & LayerTreeAsTextOptions::Debug)
            platformFlags.add(PlatformLayerTreeAsTextFlags::Debug);
        dumpInnerLayer(textStream, m_structuralLayer.get(), platformFlags);
        dumpInnerLayer(textStream, m_contentsClippingLayer.get(), platformFlags);
        dumpInnerLayer(textStream, m_shapeMaskLayer.get(), platformFlags);
        dumpInnerLayer(textStream, m_backdropClippingLayer.get(), platformFlags);
        dumpInnerLayer(textStream, m_contentsLayer.get(), platformFlags);
        dumpInnerLayer(textStream, m_contentsShapeMaskLayer.get(), platformFlags);
        dumpInnerLayer(textStream, m_backdropLayer.get(), platformFlags);
    }

    if (options & LayerTreeAsTextOptions::Debug) {
        if (m_usesDisplayListDrawing)
            textStream << indent << "(uses display-list drawing " << m_usesDisplayListDrawing << ")\n";

        if (m_uncommittedChanges) {
            textStream << indent << "(uncommitted changes ";
            dumpLayerChangeFlags(textStream, m_uncommittedChanges);
            textStream << ")\n";
        }

        dumpAnimations(textStream, "animations"_s, m_animations);
        dumpAnimations(textStream, "base value animations"_s, m_baseValueTransformAnimations);
        dumpAnimations(textStream, "animation groups"_s, m_animationGroups);
    }
}

String GraphicsLayerCA::platformLayerTreeAsText(OptionSet<PlatformLayerTreeAsTextFlags> flags) const
{
    TextStream ts(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect);
    dumpInnerLayer(ts, protectedPrimaryLayer().get(), flags);
    return ts.release();
}

void GraphicsLayerCA::setDebugBorder(const Color& color, float borderWidth)
{
    setLayerDebugBorder(*protectedLayer(), color, borderWidth);
}

void GraphicsLayerCA::setCustomAppearance(CustomAppearance customAppearance)
{
    if (customAppearance == m_customAppearance)
        return;

    GraphicsLayer::setCustomAppearance(customAppearance);
    noteLayerPropertyChanged(CustomAppearanceChanged);
}

bool GraphicsLayerCA::requiresTiledLayer(float pageScaleFactor) const
{
    if (isTiledBackingLayer())
        return true;

    if (!m_drawsContent || isPageTiledBackingLayer() || !allowsTiling())
        return false;

    // FIXME: catch zero-size height or width here (or earlier)?
#if PLATFORM(IOS_FAMILY)
    int maxPixelDimension = systemMemoryLevel() < cMemoryLevelToUseSmallerPixelDimension ? cMaxPixelDimensionLowMemory : cMaxPixelDimension;
    return m_size.width() * pageScaleFactor > maxPixelDimension || m_size.height() * pageScaleFactor > maxPixelDimension;
#else
    return m_size.width() * pageScaleFactor > cMaxPixelDimension || m_size.height() * pageScaleFactor > cMaxPixelDimension;
#endif
}

void GraphicsLayerCA::setTileCoverage(TileCoverage coverage)
{
    m_tileCoverage = coverage;
    GraphicsLayer::setTileCoverage(coverage);
}

void GraphicsLayerCA::changeLayerTypeTo(PlatformCALayer::LayerType newLayerType)
{
    PlatformCALayer::LayerType oldLayerType = m_layer->layerType();
    if (newLayerType == oldLayerType)
        return;

    bool wasTiledLayer = oldLayerType == PlatformCALayer::LayerType::LayerTypeTiledBackingLayer;
    bool isTiledLayer = newLayerType == PlatformCALayer::LayerType::LayerTypeTiledBackingLayer;

    RefPtr<PlatformCALayer> oldLayer = m_layer;
    Ref newLayer = createPlatformCALayer(newLayerType, this);
    m_layer = newLayer.copyRef();

    if (auto* backing = tiledBacking())
        backing->setTileCoverage(m_tileCoverage);

    newLayer->adoptSublayers(*oldLayer);

#ifdef VISIBLE_TILE_WASH
    if (m_visibleTileWashLayer)
        newLayer->appendSublayer(*m_visibleTileWashLayer;
#endif

    if (isMaskLayer()) {
        // A mask layer's superlayer is the layer that it masks. Set the MaskLayerChanged dirty bit
        // so that the parent will fix up the platform layers in commitLayerChangesAfterSublayers().
        if (GraphicsLayer* parentLayer = parent())
            downcast<GraphicsLayerCA>(*parentLayer).noteLayerPropertyChanged(MaskLayerChanged);
    } else if (oldLayer->superlayer()) {
        // Skip this step if we don't have a superlayer. This is probably a benign
        // case that happens while restructuring the layer tree, and also occurs with
        // WebKit2 page overlays, which can become tiled but are out-of-tree.
        oldLayer->protectedSuperlayer()->replaceSublayer(*oldLayer, newLayer);
    }

    addUncommittedChanges(ChildrenChanged
        | GeometryChanged
        | TransformChanged
        | ChildrenTransformChanged
        | MasksToBoundsChanged
        | ContentsOpaqueChanged
        | BackfaceVisibilityChanged
        | BackgroundColorChanged
        | ContentsScaleChanged
        | AcceleratesDrawingChanged
        | FiltersChanged
        | BackdropFiltersChanged
        | BackdropRootChanged
        | BlendModeChanged
        | MaskLayerChanged
        | OpacityChanged
        | EventRegionChanged
        | NameChanged
        | DebugIndicatorsChanged
#if HAVE(CORE_MATERIAL)
        | AppleVisualEffectChanged
#endif
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
        | ContentsRectsChanged
        | SeparatedChanged
#endif
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION) || HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
        | CoverageRectChanged);
#else
        );

        if (isTiledLayer)
            addUncommittedChanges(CoverageRectChanged);
#endif

    adjustContentsScaleLimitingFactor();

    moveAnimations(oldLayer.get(), newLayer.ptr());
    
    // need to tell new layer to draw itself
    setNeedsDisplay();

    if (wasTiledLayer || isTiledLayer)
        client().tiledBackingUsageChanged(this, isTiledLayer);

    oldLayer->setOwner(nullptr);
}

void GraphicsLayerCA::setupContentsLayer(PlatformCALayer* contentsLayer, CompositingCoordinatesOrientation orientation)
{
    // Turn off implicit animations on the inner layer.
#if !PLATFORM(IOS_FAMILY)
    contentsLayer->setMasksToBounds(true);
#endif
    if (orientation == CompositingCoordinatesOrientation::BottomUp) {
        TransformationMatrix flipper(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, -1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);
        contentsLayer->setTransform(flipper);
        contentsLayer->setAnchorPoint(FloatPoint3D(0, 1, 0));
    } else
        contentsLayer->setAnchorPoint(FloatPoint3D());

    setLayerDebugBorder(*contentsLayer, contentsLayerDebugBorderColor(isShowingDebugBorder()), contentsLayerBorderWidth);
}

RefPtr<PlatformCALayer> GraphicsLayerCA::findOrMakeClone(const CloneID& cloneID, PlatformCALayer *sourceLayer, LayerMap& clones, CloneLevel cloneLevel)
{
    if (!sourceLayer)
        return nullptr;

    RefPtr<PlatformCALayer> resultLayer;

    // Add with a dummy value to get an iterator for the insertion position, and a boolean that tells
    // us whether there's an item there. This technique avoids two hash lookups.
    RefPtr<PlatformCALayer> dummy;
    LayerMap::AddResult addResult = clones.add(cloneID, dummy);
    if (!addResult.isNewEntry) {
        // Value was not added, so it exists already.
        resultLayer = addResult.iterator->value.get();
    } else {
        resultLayer = cloneLayer(sourceLayer, cloneLevel);
#if ENABLE(TREE_DEBUGGING)
        resultLayer->setName(makeString("clone "_s, hex(cloneID[0U]), " of "_s, sourceLayer->layerID().object()));
#else
        resultLayer->setName(makeString("clone of "_s, m_name));
#endif
        addResult.iterator->value = resultLayer;
    }

    return resultLayer;
}

void GraphicsLayerCA::ensureCloneLayers(CloneID cloneID, RefPtr<PlatformCALayer>& primaryLayer, RefPtr<PlatformCALayer>& structuralLayer,
    RefPtr<PlatformCALayer>& contentsLayer, RefPtr<PlatformCALayer>& contentsClippingLayer, RefPtr<PlatformCALayer>& contentsShapeMaskLayer, RefPtr<PlatformCALayer>& shapeMaskLayer, RefPtr<PlatformCALayer>& backdropLayer, RefPtr<PlatformCALayer>& backdropClippingLayer, CloneLevel cloneLevel)
{
    structuralLayer = nullptr;
    contentsLayer = nullptr;

    if (!m_layerClones)
        m_layerClones = makeUnique<LayerClones>();

    primaryLayer = findOrMakeClone(cloneID, m_layer.get(), m_layerClones->primaryLayerClones, cloneLevel);
    structuralLayer = findOrMakeClone(cloneID, m_structuralLayer.get(), m_layerClones->structuralLayerClones, cloneLevel);
    contentsLayer = findOrMakeClone(cloneID, m_contentsLayer.get(), m_layerClones->contentsLayerClones, cloneLevel);
    contentsClippingLayer = findOrMakeClone(cloneID, m_contentsClippingLayer.get(), m_layerClones->contentsClippingLayerClones, cloneLevel);
    contentsShapeMaskLayer = findOrMakeClone(cloneID, m_contentsShapeMaskLayer.get(), m_layerClones->contentsShapeMaskLayerClones, cloneLevel);
    shapeMaskLayer = findOrMakeClone(cloneID, m_shapeMaskLayer.get(), m_layerClones->shapeMaskLayerClones, cloneLevel);
    backdropLayer = findOrMakeClone(cloneID, m_backdropLayer.get(), m_layerClones->backdropLayerClones, cloneLevel);
    backdropClippingLayer = findOrMakeClone(cloneID, m_backdropClippingLayer.get(), m_layerClones->backdropClippingLayerClones, cloneLevel);
}

void GraphicsLayerCA::clearClones(LayerMap& layerMap)
{
    for (auto& layer : layerMap.values())
        layer->setOwner(nullptr);
}

void GraphicsLayerCA::removeCloneLayers()
{
    if (!m_layerClones)
        return;

    clearClones(m_layerClones->primaryLayerClones);
    clearClones(m_layerClones->structuralLayerClones);
    clearClones(m_layerClones->contentsLayerClones);
    clearClones(m_layerClones->contentsClippingLayerClones);
    clearClones(m_layerClones->contentsShapeMaskLayerClones);
    clearClones(m_layerClones->shapeMaskLayerClones);
    clearClones(m_layerClones->backdropLayerClones);
    clearClones(m_layerClones->backdropClippingLayerClones);
    
    m_layerClones = nullptr;
}

FloatPoint GraphicsLayerCA::positionForCloneRootLayer() const
{
    // This can get called during a flush when we've just removed the m_replicaLayer.
    if (!m_replicaLayer)
        return FloatPoint();

    FloatPoint replicaPosition = m_replicaLayer->replicatedLayerPosition();
    return FloatPoint(replicaPosition.x() + m_anchorPoint.x() * m_size.width(),
                      replicaPosition.y() + m_anchorPoint.y() * m_size.height());
}

void GraphicsLayerCA::propagateLayerChangeToReplicas(ScheduleFlushOrNot scheduleFlush)
{
    for (GraphicsLayer* currentLayer = this; currentLayer; currentLayer = currentLayer->parent()) {
        GraphicsLayerCA& currentLayerCA = downcast<GraphicsLayerCA>(*currentLayer);
        if (!currentLayerCA.hasCloneLayers())
            break;

        if (currentLayerCA.replicaLayer())
            downcast<GraphicsLayerCA>(*currentLayerCA.replicaLayer()).noteLayerPropertyChanged(ReplicatedLayerChanged, scheduleFlush);
    }
}

RefPtr<PlatformCALayer> GraphicsLayerCA::fetchCloneLayers(GraphicsLayer* replicaRoot, ReplicaState& replicaState, CloneLevel cloneLevel)
{
    RefPtr<PlatformCALayer> primaryLayer;
    RefPtr<PlatformCALayer> structuralLayer;
    RefPtr<PlatformCALayer> contentsLayer;
    RefPtr<PlatformCALayer> contentsClippingLayer;
    RefPtr<PlatformCALayer> contentsShapeMaskLayer;
    RefPtr<PlatformCALayer> shapeMaskLayer;
    RefPtr<PlatformCALayer> backdropLayer;
    RefPtr<PlatformCALayer> backdropClippingLayer;
    ensureCloneLayers(replicaState.cloneID(), primaryLayer, structuralLayer, contentsLayer, contentsClippingLayer, contentsShapeMaskLayer, shapeMaskLayer, backdropLayer, backdropClippingLayer, cloneLevel);

    if (RefPtr maskLayer = m_maskLayer) {
        RefPtr<PlatformCALayer> maskClone = downcast<GraphicsLayerCA>(*maskLayer).fetchCloneLayers(replicaRoot, replicaState, IntermediateCloneLevel);
        primaryLayer->setMaskLayer(WTFMove(maskClone));
    }

    if (m_replicatedLayer) {
        // We are a replica being asked for clones of our layers.
        RefPtr<PlatformCALayer> replicaRoot = replicatedLayerRoot(replicaState);
        if (!replicaRoot)
            return nullptr;

        if (structuralLayer) {
            structuralLayer->insertSublayer(*replicaRoot, 0);
            return structuralLayer;
        }
        
        primaryLayer->insertSublayer(*replicaRoot, 0);
        return primaryLayer;
    }

    auto& childLayers = children();
    Vector<RefPtr<PlatformCALayer>> clonalSublayers;

    RefPtr<PlatformCALayer> replicaLayer;
    
    if (RefPtr protectedReplicaLayer = m_replicaLayer; protectedReplicaLayer && protectedReplicaLayer != replicaRoot) {
        // We have nested replicas. Ask the replica layer for a clone of its contents.
        replicaState.setBranchType(ReplicaState::ReplicaBranch);
        replicaLayer = downcast<GraphicsLayerCA>(*protectedReplicaLayer).fetchCloneLayers(replicaRoot, replicaState, RootCloneLevel);
        replicaState.setBranchType(ReplicaState::ChildBranch);
    }

    if (contentsClippingLayer && contentsLayer)
        contentsClippingLayer->appendSublayer(*contentsLayer);

    if (contentsShapeMaskLayer)
        contentsClippingLayer->setMaskLayer(WTFMove(contentsShapeMaskLayer));

    if (shapeMaskLayer)
        primaryLayer->setMaskLayer(WTFMove(shapeMaskLayer));

    if (replicaLayer || structuralLayer || contentsLayer || contentsClippingLayer || childLayers.size() > 0) {
        if (structuralLayer) {
            if (backdropLayer) {
                clonalSublayers.append(backdropLayer);
                backdropLayer->setMaskLayer(WTFMove(backdropClippingLayer));
            }
            
            // Replicas render behind the actual layer content.
            if (replicaLayer)
                clonalSublayers.append(replicaLayer);
                
            // Add the primary layer next. Even if we have negative z-order children, the primary layer always comes behind.
            clonalSublayers.append(primaryLayer);
        } else if (contentsClippingLayer) {
            // FIXME: add the contents layer in the correct order with negative z-order children.
            // This does not cause visible rendering issues because currently contents layers are only used
            // for replaced elements that don't have children.
            clonalSublayers.append(contentsClippingLayer);
        } else if (contentsLayer) {
            // FIXME: add the contents layer in the correct order with negative z-order children.
            // This does not cause visible rendering issues because currently contents layers are only used
            // for replaced elements that don't have children.
            clonalSublayers.append(contentsLayer);
        }
        
        replicaState.push(ReplicaState::ChildBranch);

        for (auto& childLayer : childLayers) {
            Ref childLayerCA = downcast<GraphicsLayerCA>(childLayer.get());
            if (auto platformLayer = childLayerCA->fetchCloneLayers(replicaRoot, replicaState, IntermediateCloneLevel))
                clonalSublayers.append(WTFMove(platformLayer));
        }

        replicaState.pop();

        for (RefPtr clonalSublayer : clonalSublayers)
            clonalSublayer->removeFromSuperlayer();
    }
    
    RefPtr<PlatformCALayer> result;
    if (structuralLayer) {
        structuralLayer->setSublayers(clonalSublayers);

        if (contentsClippingLayer || contentsLayer) {
            // If we have a transform layer, then the contents layer is parented in the 
            // primary layer (which is itself a child of the transform layer).
            primaryLayer->removeAllSublayers();
            primaryLayer->appendSublayer(contentsClippingLayer ? *contentsClippingLayer : *contentsLayer);
        }

        result = structuralLayer;
    } else {
        primaryLayer->setSublayers(clonalSublayers);
        result = primaryLayer;
    }

    return result;
}

Ref<PlatformCALayer> GraphicsLayerCA::cloneLayer(PlatformCALayer *layer, CloneLevel cloneLevel)
{
    auto newLayer = layer->clone(this);

    if (cloneLevel == IntermediateCloneLevel) {
        newLayer->setOpacity(layer->opacity());
        copyAnimations(layer, newLayer.ptr());
    }

    setLayerDebugBorder(newLayer, cloneLayerDebugBorderColor(isShowingDebugBorder()), cloneLayerBorderWidth);

    return newLayer;
}

void GraphicsLayerCA::updateOpacityOnLayer()
{
    protectedPrimaryLayer()->setOpacity(m_opacity);

    if (LayerMap* layerCloneMap = primaryLayerClones()) {
        for (auto& clone : *layerCloneMap) {
            if (m_replicaLayer && isReplicatedRootClone(clone.key))
                continue;

            Ref { *clone.value }->setOpacity(m_opacity);
        }
    }

#if ENABLE(MODEL_ELEMENT)
    if (RefPtr contentsLayer = m_contentsLayer; contentsLayer && m_contentsLayerPurpose == ContentsLayerPurpose::Model)
        contentsLayer->setOpacity(m_opacity);
#endif
}

void GraphicsLayerCA::deviceOrPageScaleFactorChanged()
{
    noteChangesForScaleSensitiveProperties();
}

void GraphicsLayerCA::noteChangesForScaleSensitiveProperties()
{
    noteLayerPropertyChanged(GeometryChanged | ContentsScaleChanged | ContentsOpaqueChanged);
}

void GraphicsLayerCA::computePixelAlignment(float pageScale, const FloatPoint& positionRelativeToBase,
    FloatPoint& position, FloatPoint3D& anchorPoint, FloatSize& alignmentOffset) const
{
    FloatRect baseRelativeBounds(positionRelativeToBase, m_size);
    FloatRect scaledBounds = baseRelativeBounds;
    float contentsScale = pageScale * deviceScaleFactor();
    // Scale by the page scale factor to compute the screen-relative bounds.
    scaledBounds.scale(contentsScale);
    // Round to integer boundaries.
    FloatRect alignedBounds = encloseRectToDevicePixels(LayoutRect(scaledBounds), deviceScaleFactor());
    
    // Convert back to layer coordinates.
    alignedBounds.scale(1 / contentsScale);

    alignmentOffset = baseRelativeBounds.location() - alignedBounds.location();
    position = m_position - alignmentOffset;

    // Now we have to compute a new anchor point which compensates for rounding.
    float anchorPointX = m_anchorPoint.x();
    float anchorPointY = m_anchorPoint.y();
    
    if (alignedBounds.width())
        anchorPointX = (baseRelativeBounds.width() * anchorPointX + alignmentOffset.width()) / alignedBounds.width();
    
    if (alignedBounds.height())
        anchorPointY = (baseRelativeBounds.height() * anchorPointY + alignmentOffset.height()) / alignedBounds.height();
     
    anchorPoint = FloatPoint3D(anchorPointX, anchorPointY, m_anchorPoint.z() * contentsScale);
}

void GraphicsLayerCA::noteSublayersChanged(ScheduleFlushOrNot scheduleFlush)
{
    noteLayerPropertyChanged(ChildrenChanged, scheduleFlush);
    propagateLayerChangeToReplicas(scheduleFlush);
}

void GraphicsLayerCA::addUncommittedChanges(LayerChangeFlags flags)
{
    m_uncommittedChanges |= flags;

    if (m_isCommittingChanges)
        return;

    for (auto* ancestor = parent(); ancestor; ancestor = ancestor->parent()) {
        auto& ancestorCA = static_cast<GraphicsLayerCA&>(*ancestor);
        ASSERT(!ancestorCA.m_isCommittingChanges);
        if (ancestorCA.hasDescendantsWithUncommittedChanges())
            return;
        ancestorCA.setHasDescendantsWithUncommittedChanges(true);
    }
}

void GraphicsLayerCA::setHasDescendantsWithUncommittedChanges(bool value)
{
    m_hasDescendantsWithUncommittedChanges = value;
}

void GraphicsLayerCA::noteLayerPropertyChanged(LayerChangeFlags flags, ScheduleFlushOrNot scheduleFlush)
{
    if (beingDestroyed())
        return;

    bool hadUncommittedChanges = !!m_uncommittedChanges;

    addUncommittedChanges(flags);

    if (scheduleFlush == ScheduleFlush) {
        bool needsFlush = !hadUncommittedChanges;
        if (needsFlush)
            client().notifyFlushRequired(this);
    }
}

double GraphicsLayerCA::backingStoreMemoryEstimate() const
{
    if (!drawsContent())
        return 0;

    // contentsLayer is given to us, so we don't really know anything about its contents.
    // FIXME: ignores layer clones.
    
    if (TiledBacking* tiledBacking = this->tiledBacking())
        return tiledBacking->retainedTileBackingStoreMemory();

    if (!backingStoreAttached())
        return 0;

    Ref layer = *m_layer;
    return layer->backingStoreBytesPerPixel() * size().width() * layer->contentsScale() * size().height() * layer->contentsScale();
}

Vector<GraphicsLayer::AcceleratedAnimationForTesting> GraphicsLayerCA::acceleratedAnimationsForTesting() const
{
    Vector<GraphicsLayer::AcceleratedAnimationForTesting> animations;

#if ENABLE(THREADED_ANIMATIONS)
    auto addAcceleratedEffect = [&](const AcceleratedEffect& effect) {
        for (auto property : effect.animatedProperties())
            animations.append({ acceleratedEffectPropertyIDAsString(property), effect.playbackRate(), true });
    };

    if (RefPtr effectsStack = acceleratedEffectStack()) {
        for (auto& effect : effectsStack->primaryLayerEffects())
            addAcceleratedEffect(effect.get());
        for (auto& effect : effectsStack->backdropLayerEffects())
            addAcceleratedEffect(effect.get());
    }
#endif

    for (auto& animation : m_animations) {
        if (animation.m_pendingRemoval)
            continue;
        if (auto caAnimation = protectedAnimatedLayer(animation.m_property)->animationForKey(animation.animationIdentifier()))
            animations.append({ animatedPropertyIDAsString(animation.m_property), caAnimation->speed(), false });
        else
            animations.append({ animatedPropertyIDAsString(animation.m_property), (animation.m_playState == PlayState::Playing || animation.m_playState == PlayState::PlayPending) ? 1.0 : 0.0, false });
    }

    return animations;
}

RefPtr<GraphicsLayerAsyncContentsDisplayDelegate> GraphicsLayerCA::createAsyncContentsDisplayDelegate(GraphicsLayerAsyncContentsDisplayDelegate* existing)
{
    if (existing && existing->isGraphicsLayerAsyncContentsDisplayDelegateCocoa()) {
        static_cast<GraphicsLayerAsyncContentsDisplayDelegateCocoa*>(existing)->updateGraphicsLayerCA(*this);
        return existing;
    }
    return adoptRef(new GraphicsLayerAsyncContentsDisplayDelegateCocoa(*this));
}

#if ENABLE(THREADED_ANIMATIONS)
void GraphicsLayerCA::setAcceleratedEffectsAndBaseValues(AcceleratedEffects&& effects, AcceleratedEffectValues&& baseValues)
{
    auto hadEffectStack = !!acceleratedEffectStack();

    GraphicsLayer::setAcceleratedEffectsAndBaseValues(WTFMove(effects), WTFMove(baseValues));

    // Nothing to do if we didn't have an accelerated stack and we still don't.
    if (!hadEffectStack && !acceleratedEffectStack())
        return;

    RefPtr layer = primaryLayer();
    ASSERT(layer);

    auto hasEffectsTargetingPrimaryLayer = false;
    auto hasEffectsTargetingBackdropLayer = false;

    if (RefPtr effectsStack = acceleratedEffectStack()) {
        auto& primaryLayerEffects = effectsStack->primaryLayerEffects();
        hasEffectsTargetingPrimaryLayer = !primaryLayerEffects.isEmpty();
        layer->setAcceleratedEffectsAndBaseValues(primaryLayerEffects, effectsStack->baseValues());

        auto& backdropLayerEffects = effectsStack->backdropLayerEffects();
        hasEffectsTargetingBackdropLayer = !backdropLayerEffects.isEmpty();
        if (RefPtr backdropLayer = m_backdropLayer)
            backdropLayer->setAcceleratedEffectsAndBaseValues(backdropLayerEffects, effectsStack->baseValues());
    }

    if (!hasEffectsTargetingPrimaryLayer)
        layer->clearAcceleratedEffectsAndBaseValues();
    if (!hasEffectsTargetingBackdropLayer) {
        if (RefPtr backdropLayer = m_backdropLayer)
            backdropLayer->clearAcceleratedEffectsAndBaseValues();
    }

    // After clearing animations, ensure that any property that could have
    // been animated is reset to match the current non-animated values.
    if (!hasEffectsTargetingPrimaryLayer && !hasEffectsTargetingBackdropLayer)
        noteLayerPropertyChanged(TransformChanged | FiltersChanged | OpacityChanged | BackdropFiltersChanged | DebugIndicatorsChanged);

    noteLayerPropertyChanged(AnimationChanged | CoverageRectChanged);
}
#endif

void GraphicsLayerCA::purgeFrontBufferForTesting()
{
    if (RefPtr layer = primaryLayer())
        layer->purgeFrontBufferForTesting();
}

void GraphicsLayerCA::purgeBackBufferForTesting()
{
    if (RefPtr layer = primaryLayer())
        layer->purgeBackBufferForTesting();
}

void GraphicsLayerCA::markFrontBufferVolatileForTesting()
{
    if (RefPtr layer = primaryLayer())
        layer->markFrontBufferVolatileForTesting();
}

} // namespace WebCore

#endif
