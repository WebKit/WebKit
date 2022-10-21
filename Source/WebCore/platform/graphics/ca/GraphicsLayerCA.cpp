/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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

#include "Animation.h"
#include "DisplayListRecorderImpl.h"
#include "DisplayListReplayer.h"
#include "FloatConversion.h"
#include "FloatRect.h"
#include "GraphicsLayerContentsDisplayDelegate.h"
#include "GraphicsLayerFactory.h"
#include "Image.h"
#include "InMemoryDisplayList.h"
#include "Logging.h"
#include "Model.h"
#include "PlatformCAFilters.h"
#include "PlatformCALayer.h"
#include "PlatformScreen.h"
#include "Region.h"
#include "RotateTransformOperation.h"
#include "ScaleTransformOperation.h"
#include "TiledBacking.h"
#include "TransformState.h"
#include "TranslateTransformOperation.h"
#include <QuartzCore/CATransform3D.h>
#include <limits.h>
#include <pal/spi/cf/CFUtilitiesSPI.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/HexNumber.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/PointerComparison.h>
#include <wtf/SetForScope.h>
#include <wtf/SystemTracing.h>
#include <wtf/UUID.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/TextStream.h>

#if PLATFORM(IOS_FAMILY)
#include "SystemMemory.h"
#include "WebCoreThread.h"
#endif

#if PLATFORM(COCOA)
#include "PlatformCAAnimationCocoa.h"
#include "PlatformCALayerCocoa.h"
#endif

#if PLATFORM(WIN)
#include "PlatformCAAnimationWin.h"
#include "PlatformCALayerWin.h"
#endif

#if COMPILER(MSVC)
// See https://msdn.microsoft.com/en-us/library/1wea5zwe.aspx
#pragma warning(disable: 4701)
#endif

namespace WebCore {

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

static bool isTransformTypeTransformationMatrix(TransformOperation::OperationType transformType)
{
    switch (transformType) {
    case TransformOperation::SKEW_X:
    case TransformOperation::SKEW_Y:
    case TransformOperation::SKEW:
    case TransformOperation::MATRIX:
    case TransformOperation::ROTATE_3D:
    case TransformOperation::MATRIX_3D:
    case TransformOperation::PERSPECTIVE:
    case TransformOperation::IDENTITY:
    case TransformOperation::NONE:
        return true;
    default:
        return false;
    }
}

static bool isTransformTypeFloatPoint3D(TransformOperation::OperationType transformType)
{
    switch (transformType) {
    case TransformOperation::SCALE:
    case TransformOperation::SCALE_3D:
    case TransformOperation::TRANSLATE:
    case TransformOperation::TRANSLATE_3D:
        return true;
    default:
        return false;
    }
}

static bool isTransformTypeNumber(TransformOperation::OperationType transformType)
{
    return !isTransformTypeTransformationMatrix(transformType) && !isTransformTypeFloatPoint3D(transformType);
}

static void getTransformFunctionValue(const TransformOperation* transformOp, TransformOperation::OperationType transformType, const FloatSize& size, float& value)
{
    switch (transformType) {
    case TransformOperation::ROTATE:
    case TransformOperation::ROTATE_X:
    case TransformOperation::ROTATE_Y:
        value = transformOp ? narrowPrecisionToFloat(deg2rad(downcast<RotateTransformOperation>(*transformOp).angle())) : 0;
        break;
    case TransformOperation::SCALE_X:
        value = transformOp ? narrowPrecisionToFloat(downcast<ScaleTransformOperation>(*transformOp).x()) : 1;
        break;
    case TransformOperation::SCALE_Y:
        value = transformOp ? narrowPrecisionToFloat(downcast<ScaleTransformOperation>(*transformOp).y()) : 1;
        break;
    case TransformOperation::SCALE_Z:
        value = transformOp ? narrowPrecisionToFloat(downcast<ScaleTransformOperation>(*transformOp).z()) : 1;
        break;
    case TransformOperation::TRANSLATE_X:
        value = transformOp ? downcast<TranslateTransformOperation>(*transformOp).xAsFloat(size) : 0;
        break;
    case TransformOperation::TRANSLATE_Y:
        value = transformOp ? downcast<TranslateTransformOperation>(*transformOp).yAsFloat(size) : 0;
        break;
    case TransformOperation::TRANSLATE_Z:
        value = transformOp ? downcast<TranslateTransformOperation>(*transformOp).zAsFloat() : 0;
        break;
    default:
        break;
    }
}

static void getTransformFunctionValue(const TransformOperation* transformOp, TransformOperation::OperationType transformType, const FloatSize& size, FloatPoint3D& value)
{
    switch (transformType) {
    case TransformOperation::SCALE:
    case TransformOperation::SCALE_3D: {
        const auto* scaleTransformOp = downcast<ScaleTransformOperation>(transformOp);
        value.setX(scaleTransformOp ? narrowPrecisionToFloat(scaleTransformOp->x()) : 1);
        value.setY(scaleTransformOp ? narrowPrecisionToFloat(scaleTransformOp->y()) : 1);
        value.setZ(scaleTransformOp ? narrowPrecisionToFloat(scaleTransformOp->z()) : 1);
        break;
    }
    case TransformOperation::TRANSLATE:
    case TransformOperation::TRANSLATE_3D: {
        const auto* translateTransformOp = downcast<TranslateTransformOperation>(transformOp);
        value.setX(translateTransformOp ? translateTransformOp->xAsFloat(size) : 0);
        value.setY(translateTransformOp ? translateTransformOp->yAsFloat(size) : 0);
        value.setZ(translateTransformOp ? translateTransformOp->zAsFloat() : 0);
        break;
    }
    default:
        break;
    }
}

static void getTransformFunctionValue(const TransformOperation* transformOp, TransformOperation::OperationType transformType, const FloatSize& size, TransformationMatrix& value)
{
    switch (transformType) {
    case TransformOperation::SKEW_X:
    case TransformOperation::SKEW_Y:
    case TransformOperation::SKEW:
    case TransformOperation::MATRIX:
    case TransformOperation::ROTATE_3D:
    case TransformOperation::MATRIX_3D:
    case TransformOperation::PERSPECTIVE:
    case TransformOperation::IDENTITY:
    case TransformOperation::NONE:
        if (transformOp)
            transformOp->apply(value, size);
        else
            value.makeIdentity();
        break;
    default:
        break;
    }
}

static PlatformCAAnimation::ValueFunctionType getValueFunctionNameForTransformOperation(TransformOperation::OperationType transformType)
{
    // Use literal strings to avoid link-time dependency on those symbols.
    switch (transformType) {
    case TransformOperation::ROTATE_X:
        return PlatformCAAnimation::RotateX;
    case TransformOperation::ROTATE_Y:
        return PlatformCAAnimation::RotateY;
    case TransformOperation::ROTATE:
        return PlatformCAAnimation::RotateZ;
    case TransformOperation::SCALE_X:
        return PlatformCAAnimation::ScaleX;
    case TransformOperation::SCALE_Y:
        return PlatformCAAnimation::ScaleY;
    case TransformOperation::SCALE_Z:
        return PlatformCAAnimation::ScaleZ;
    case TransformOperation::TRANSLATE_X:
        return PlatformCAAnimation::TranslateX;
    case TransformOperation::TRANSLATE_Y:
        return PlatformCAAnimation::TranslateY;
    case TransformOperation::TRANSLATE_Z:
        return PlatformCAAnimation::TranslateZ;
    case TransformOperation::SCALE:
    case TransformOperation::SCALE_3D:
        return PlatformCAAnimation::Scale;
    case TransformOperation::TRANSLATE:
    case TransformOperation::TRANSLATE_3D:
        return PlatformCAAnimation::Translate;
    default:
        return PlatformCAAnimation::NoValueFunction;
    }
}

static ASCIILiteral propertyIdToString(AnimatedPropertyID property)
{
    switch (property) {
    case AnimatedPropertyTranslate:
    case AnimatedPropertyScale:
    case AnimatedPropertyRotate:
    case AnimatedPropertyTransform:
        return "transform"_s;
    case AnimatedPropertyOpacity:
        return "opacity"_s;
    case AnimatedPropertyBackgroundColor:
        return "backgroundColor"_s;
    case AnimatedPropertyFilter:
        return "filters"_s;
#if ENABLE(FILTERS_LEVEL_2)
    case AnimatedPropertyWebkitBackdropFilter:
        return "backdropFilters"_s;
#endif
    case AnimatedPropertyInvalid:
        ASSERT_NOT_REACHED();
    }
    ASSERT_NOT_REACHED();
    return { };
}

static bool animationHasStepsTimingFunction(const KeyframeValueList& valueList, const Animation* anim)
{
    if (is<StepsTimingFunction>(anim->timingFunction()))
        return true;

    bool hasStepsDefaultTimingFunctionForKeyframes = is<StepsTimingFunction>(anim->defaultTimingFunctionForKeyframes());
    for (unsigned i = 0; i < valueList.size(); ++i) {
        if (const TimingFunction* timingFunction = valueList.at(i).timingFunction()) {
            if (is<StepsTimingFunction>(timingFunction))
                return true;
        } else if (hasStepsDefaultTimingFunctionForKeyframes)
            return true;
    }

    return false;
}

static inline bool supportsAcceleratedFilterAnimations()
{
#if PLATFORM(COCOA)
    return true;
#else
    return false;
#endif
}

static PlatformCALayer::FilterType toPlatformCALayerFilterType(GraphicsLayer::ScalingFilter filter)
{
    switch (filter) {
    case GraphicsLayer::ScalingFilter::Linear:
        return PlatformCALayer::Linear;
    case GraphicsLayer::ScalingFilter::Nearest:
        return PlatformCALayer::Nearest;
    case GraphicsLayer::ScalingFilter::Trilinear:
        return PlatformCALayer::Trilinear;
    }
    ASSERT_NOT_REACHED();
    return PlatformCALayer::Linear;
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
#if PLATFORM(COCOA)
        // FIXME: we can use shaper layers on Windows when PlatformCALayerCocoa::setShapePath() etc are implemented.
        return true;
#else
        return false;
#endif
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool GraphicsLayer::supportsSubpixelAntialiasedLayerText()
{
#if PLATFORM(MAC)
    return true;
#else
    return false;
#endif
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
#if PLATFORM(COCOA)
    return PlatformCALayerCocoa::filtersCanBeComposited(filters);
#elif PLATFORM(WIN)
    return PlatformCALayerWin::filtersCanBeComposited(filters);
#endif
}

Ref<PlatformCALayer> GraphicsLayerCA::createPlatformCALayer(PlatformCALayer::LayerType layerType, PlatformCALayerClient* owner)
{
#if PLATFORM(COCOA)
    auto result = PlatformCALayerCocoa::create(layerType, owner);
    
    if (result->canHaveBackingStore())
        result->setWantsDeepColorBackingStore(screenSupportsExtendedColor());
    
    return result;
#elif PLATFORM(WIN)
    return PlatformCALayerWin::create(layerType, owner);
#endif
}
    
Ref<PlatformCALayer> GraphicsLayerCA::createPlatformCALayer(PlatformLayer* platformLayer, PlatformCALayerClient* owner)
{
#if PLATFORM(COCOA)
    return PlatformCALayerCocoa::create(platformLayer, owner);
#elif PLATFORM(WIN)
    return PlatformCALayerWin::create(platformLayer, owner);
#endif
}

#if ENABLE(MODEL_ELEMENT)
Ref<PlatformCALayer> GraphicsLayerCA::createPlatformCALayer(Ref<WebCore::Model>, PlatformCALayerClient* owner)
{
    // By default, just make a plain layer; subclasses can override to provide a custom PlatformCALayer for Model.
    return GraphicsLayerCA::createPlatformCALayer(PlatformCALayer::LayerTypeLayer, owner);
}
#endif

Ref<PlatformCAAnimation> GraphicsLayerCA::createPlatformCAAnimation(PlatformCAAnimation::AnimationType type, const String& keyPath)
{
#if PLATFORM(COCOA)
    return PlatformCAAnimationCocoa::create(type, keyPath);
#elif PLATFORM(WIN)
    return PlatformCAAnimationWin::create(type, keyPath);
#endif
}

typedef HashMap<const GraphicsLayerCA*, std::pair<FloatRect, std::unique_ptr<DisplayList::InMemoryDisplayList>>> LayerDisplayListHashMap;

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
    if (UNLIKELY(isTrackingDisplayListReplay()))
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
    if (!m_layer->isPlatformCALayerRemote())
        caLayerDescription = makeString("CALayer(0x", hex(reinterpret_cast<uintptr_t>(m_layer->platformLayer()), Lowercase), ") ");
    return makeString(caLayerDescription, "GraphicsLayer(0x", hex(reinterpret_cast<uintptr_t>(this), Lowercase), ", ", primaryLayerID(), ") ", name());
#else
    return name();
#endif
}

GraphicsLayer::PlatformLayerID GraphicsLayerCA::primaryLayerID() const
{
    return primaryLayer()->layerID();
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

void GraphicsLayerCA::removeFromParent()
{
    if (m_parent)
        downcast<GraphicsLayerCA>(*m_parent).noteSublayersChanged();
    GraphicsLayer::removeFromParent();
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
    noteLayerPropertyChanged(ReplicatedLayerChanged);
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
    if (point == m_position)
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
    for (auto& animationGroup : m_animationGroups) {
        if ((animatedPropertyIsTransformOrRelated(animationGroup.m_property)
            || animationGroup.m_property == AnimatedPropertyOpacity
            || animationGroup.m_property == AnimatedPropertyBackgroundColor
            || animationGroup.m_property == AnimatedPropertyFilter))
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

void GraphicsLayerCA::setSupportsSubpixelAntialiasedText(bool supportsSubpixelAntialiasedText)
{
    if (m_supportsSubpixelAntialiasedText == supportsSubpixelAntialiasedText)
        return;

    GraphicsLayer::setSupportsSubpixelAntialiasedText(supportsSubpixelAntialiasedText);
    noteLayerPropertyChanged(SupportsSubpixelAntialiasedTextChanged);
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

void GraphicsLayerCA::setBackdropFiltersRect(const FloatRoundedRect& backdropFiltersRect)
{
    if (backdropFiltersRect == m_backdropFiltersRect)
        return;

    GraphicsLayer::setBackdropFiltersRect(backdropFiltersRect);
    noteLayerPropertyChanged(BackdropFiltersRectChanged);
}

#if ENABLE(CSS_COMPOSITING)
void GraphicsLayerCA::setBlendMode(BlendMode blendMode)
{
    if (GraphicsLayer::blendMode() == blendMode)
        return;

    GraphicsLayer::setBlendMode(blendMode);
    noteLayerPropertyChanged(BlendModeChanged);
}
#endif

bool GraphicsLayerCA::backingStoreAttached() const
{
    return m_layer->backingStoreAttached();
}

bool GraphicsLayerCA::backingStoreAttachedForTesting() const
{
    return m_layer->backingStoreAttached() || m_layer->hasContents();
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
    if (shouldClip == ClipToLayer) {
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
void GraphicsLayerCA::setScrollingNodeID(ScrollingNodeID nodeID)
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
    if (!is<CubicBezierTimingFunction>(timingFunction))
        return false;

    auto yValueIsOutOfRange = [](double y) -> bool {
        return y < 0 || y > 1;
    };

    auto& cubicBezierTimingFunction = downcast<CubicBezierTimingFunction>(*timingFunction);
    return yValueIsOutOfRange(cubicBezierTimingFunction.y1()) || yValueIsOutOfRange(cubicBezierTimingFunction.y2());
}

static bool keyframeValueListHasSingleIntervalWithLinearOrEquivalentTimingFunction(const KeyframeValueList& valueList)
{
    if (valueList.size() != 2)
        return false;

    auto* timingFunction = valueList.at(0).timingFunction();
    if (!timingFunction || is<LinearTimingFunction>(timingFunction))
        return true;

    return is<CubicBezierTimingFunction>(timingFunction) && downcast<CubicBezierTimingFunction>(*timingFunction).isLinear();
}

static bool animationCanBeAccelerated(const KeyframeValueList& valueList, const Animation* anim)
{
    if (anim->playbackRate() != 1 || !anim->directionIsForwards())
        return false;

    if (!anim || anim->isEmptyOrZeroDuration() || valueList.size() < 2)
        return false;

    if (animationHasStepsTimingFunction(valueList, anim))
        return false;

    return true;
}

bool GraphicsLayerCA::addAnimation(const KeyframeValueList& valueList, const FloatSize& boxSize, const Animation* anim, const String& animationName, double timeOffset)
{
    LOG_WITH_STREAM(Animations, stream << "GraphicsLayerCA " << this << " id " << primaryLayerID() << " addAnimation " << anim << " " << animationName << " duration " << anim->duration() << " (can be accelerated " << animationCanBeAccelerated(valueList, anim) << ")");

    ASSERT(!animationName.isEmpty());

    if (!animationCanBeAccelerated(valueList, anim))
        return false;

    bool keyframesShouldUseAnimationWideTimingFunction = false;
    // Core Animation clips values outside of the [0-1] range for animation-wide cubic timing functions.
    if (timingFunctionIsCubicTimingFunctionWithYValueOutOfRange(anim->timingFunction())) {
        if (!keyframeValueListHasSingleIntervalWithLinearOrEquivalentTimingFunction(valueList))
            return false;
        keyframesShouldUseAnimationWideTimingFunction = true;
    }

    bool createdAnimations = false;
    if (animatedPropertyIsTransformOrRelated(valueList.property()))
        createdAnimations = createTransformAnimationsFromKeyframes(valueList, anim, animationName, Seconds { timeOffset }, boxSize, keyframesShouldUseAnimationWideTimingFunction);
    else if (valueList.property() == AnimatedPropertyFilter) {
        if (supportsAcceleratedFilterAnimations())
            createdAnimations = createFilterAnimationsFromKeyframes(valueList, anim, animationName, Seconds { timeOffset }, keyframesShouldUseAnimationWideTimingFunction);
    }
#if ENABLE(FILTERS_LEVEL_2)
    else if (valueList.property() == AnimatedPropertyWebkitBackdropFilter) {
        if (supportsAcceleratedFilterAnimations())
            createdAnimations = createFilterAnimationsFromKeyframes(valueList, anim, animationName, Seconds { timeOffset }, keyframesShouldUseAnimationWideTimingFunction);
    }
#endif
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

void GraphicsLayerCA::removeAnimation(const String& animationName)
{
    LOG_WITH_STREAM(Animations, stream << "GraphicsLayerCA " << this << " id " << primaryLayerID() << " removeAnimation " << animationName << " (is running " << animationIsRunning(animationName) << ")");

    for (auto& animation : m_animations) {
        // There may be several animations with the same name in the case of transform animations
        // animating multiple components as individual animations.
        if (animation.m_name == animationName && !animation.m_pendingRemoval) {
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
            m_contentsLayer = createPlatformCALayer(PlatformCALayer::LayerTypeLayer, this);
#if ENABLE(TREE_DEBUGGING)
            m_contentsLayer->setName(makeString("contents color ", m_contentsLayer->layerID()));
#else
            m_contentsLayer->setName(MAKE_STATIC_STRING_IMPL("contents color"));
#endif
            contentsLayerChanged = true;
        }
    } else {
        contentsLayerChanged = m_contentsLayer;
        m_contentsLayerPurpose = ContentsLayerPurpose::None;
        m_contentsLayer = nullptr;
    }
    m_contentsDisplayDelegate = nullptr;

    if (contentsLayerChanged)
        noteSublayersChanged();

    noteLayerPropertyChanged(ContentsColorLayerChanged);
}

void GraphicsLayerCA::setContentsToImage(Image* image)
{
    if (image) {
        auto newImage = image->nativeImageForCurrentFrame();
        if (!newImage)
            return;

        // FIXME: probably don't need m_uncorrectedContentsImage at all now.
        if (m_uncorrectedContentsImage == newImage)
            return;
        
        m_uncorrectedContentsImage = WTFMove(newImage);
        m_pendingContentsImage = m_uncorrectedContentsImage;

        m_contentsLayerPurpose = ContentsLayerPurpose::Image;
        if (!m_contentsLayer)
            noteSublayersChanged();
    } else {
        m_uncorrectedContentsImage = nullptr;
        m_pendingContentsImage = nullptr;
        m_contentsLayerPurpose = ContentsLayerPurpose::None;
        if (m_contentsLayer)
            noteSublayersChanged();
    }
    m_contentsDisplayDelegate = nullptr;

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
        m_contentsLayer = createPlatformCALayer(*model, this);
#if ENABLE(TREE_DEBUGGING)
        m_contentsLayer->setName(makeString("contents model ", m_contentsLayer->layerID()));
#else
        m_contentsLayer->setName(MAKE_STATIC_STRING_IMPL("contents model"));
#endif

        m_contentsLayer->setUserInteractionEnabled(interactive == ModelInteraction::Enabled);
        m_contentsLayer->setAnchorPoint({ });
        m_contentsLayerPurpose = ContentsLayerPurpose::Model;
        contentsLayerChanged = true;
    } else {
        contentsLayerChanged = m_contentsLayer;
        m_contentsLayerPurpose = ContentsLayerPurpose::None;
        m_contentsLayer = nullptr;
    }
    m_contentsDisplayDelegate = nullptr;

    if (contentsLayerChanged)
        noteSublayersChanged();

    noteLayerPropertyChanged(ContentsRectsChanged | OpacityChanged);
}

GraphicsLayer::PlatformLayerID GraphicsLayerCA::contentsLayerIDForModel() const
{
    return m_contentsLayerPurpose == ContentsLayerPurpose::Model ? m_contentsLayer->layerID() : 0;
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
        auto platformCALayer = PlatformCALayer::platformCALayerForLayer(platformLayer);
        if (platformCALayer)
            m_contentsLayer = WTFMove(platformCALayer);
        else
            m_contentsLayer = createPlatformCALayer(platformLayer, this);
    } else
        m_contentsLayer = nullptr;

    m_contentsLayerPurpose = platformLayer ? purpose : ContentsLayerPurpose::None;
    m_contentsDisplayDelegate = nullptr;
    noteSublayersChanged();
    noteLayerPropertyChanged(ContentsPlatformLayerChanged);
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
        m_contentsLayer = createPlatformCALayer(PlatformCALayer::LayerTypeContentsProvidedLayer, this);
        m_contentsDisplayDelegate = WTFMove(delegate);
        m_contentsLayerPurpose = purpose;
        // Currently delegated display is only useful when delegatee calls setContents, so set the
        // backing store settings accordingly.
        m_contentsLayer->setBackingStoreAttached(true);
        m_contentsLayer->setAcceleratesDrawing(true);
        m_contentsDisplayDelegate->prepareToDelegateDisplay(*m_contentsLayer);
    }

    noteSublayersChanged();
    noteLayerPropertyChanged(ContentsPlatformLayerChanged);
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

    FloatPoint offset;
    for (const GraphicsLayer* currLayer = this; currLayer; currLayer = currLayer->parent()) {
        if (currLayer->appliesPageScale()) {
            pageScale = currLayer->pageScaleFactor();
            return offset;
        }

        offset += currLayer->position();
    }

    return FloatPoint();
}

void GraphicsLayerCA::flushCompositingState(const FloatRect& visibleRect)
{
    TransformState state(TransformState::UnapplyInverseTransformDirection, FloatQuad(visibleRect));
    state.setSecondaryQuad(FloatQuad { visibleRect });

    CommitState commitState;
    commitState.ancestorHadChanges = visibleRect != m_previousCommittedVisibleRect;
    m_previousCommittedVisibleRect = visibleRect;

#if PLATFORM(IOS_FAMILY)
    // In WK1, UIKit may be changing layer bounds behind our back in overflow-scroll layers, so disable the optimization.
    // See the similar test in computeVisibleAndCoverageRect().
    if (m_layer->isPlatformCALayerCocoa())
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
        auto& maskLayerCA = downcast<GraphicsLayerCA>(*m_maskLayer);
        if (maskLayerCA.recursiveVisibleRectChangeRequiresFlush(childCommitState, localState))
            return true;
    }

    for (const auto& layer : children()) {
        const auto& currentChild = downcast<GraphicsLayerCA>(layer.get());
        if (currentChild.recursiveVisibleRectChangeRequiresFlush(childCommitState, localState))
            return true;
    }

    if (m_replicaLayer)
        if (downcast<GraphicsLayerCA>(*m_replicaLayer).recursiveVisibleRectChangeRequiresFlush(childCommitState, localState))
            return true;
    
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
    return m_layer->tiledBacking();
}

TransformationMatrix GraphicsLayerCA::layerTransform(const FloatPoint& position, const TransformationMatrix* customTransform) const
{
    TransformationMatrix transform;
    transform.translate(position.x(), position.y());

    TransformationMatrix currentTransform;
    if (customTransform)
        currentTransform = *customTransform;
    else if (m_transform)
        currentTransform = *m_transform;

    transform.multiply(transformByApplyingAnchorPoint(currentTransform));

    if (GraphicsLayer* parentLayer = parent()) {
        if (parentLayer->hasNonIdentityChildrenTransform()) {
            FloatPoint boundsOrigin = parentLayer->boundsOrigin();

            FloatPoint3D parentAnchorPoint(parentLayer->anchorPoint());
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

    TransformationMatrix layerTransform;
    TransformationMatrix currentTransform;
    if ((flags & RespectAnimatingTransforms) && client().getCurrentTransform(this, currentTransform))
        layerTransform = this->layerTransform(position, &currentTransform);
    else
        layerTransform = this->layerTransform(position);

    bool applyWasClamped;
    TransformState::TransformAccumulation accumulation = preserves3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform;
    state.applyTransform(layerTransform, accumulation, &applyWasClamped);

    bool mapWasClamped;
    auto clipRectFromParent = state.mappedQuad(&mapWasClamped).boundingBox();

    auto clipRectForSelf = FloatRect { { }, m_size };
    if (!applyWasClamped && !mapWasClamped)
        clipRectForSelf.intersect(clipRectFromParent);

    if (masksToBounds()) {
        ASSERT(accumulation == TransformState::FlattenTransform);
        // Flatten, and replace the quad in the TransformState with one that is clipped to this layer's bounds.
        state.flatten();
        state.setQuad(clipRectForSelf);
        if (state.isMappingSecondaryQuad())
            state.setSecondaryQuad(FloatQuad { clipRectForSelf });
    }

    auto boundsOrigin = m_boundsOrigin;
#if PLATFORM(IOS_FAMILY)
    // In WK1, UIKit may be changing layer bounds behind our back in overflow-scroll layers, so use the layer's origin.
    if (m_layer->isPlatformCALayerCocoa())
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

    bool affectedByTransformAnimation = commitState.ancestorHasTransformAnimation;

    bool accumulateTransform = accumulatesTransform(*this);
    VisibleAndCoverageRects rects = computeVisibleAndCoverageRect(localState, accumulateTransform);
    if (adjustCoverageRect(rects, m_visibleRect)) {
        if (state.isMappingSecondaryQuad())
            localState.setLastPlanarSecondaryQuad(FloatQuad { rects.coverageRect });
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
        m_visibleTileWashLayer->setName(makeString("Visible Tile Wash Layer 0x", hex(reinterpret_cast<uintptr_t>(m_visibleTileWashLayer->platformLayer()), Lowercase)));
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
        pageScaleFactor = this->pageScaleFactor();
        affectedByPageScale = true;
    }

    // Accumulate an offset from the ancestral pixel-aligned layer.
    FloatPoint baseRelativePosition = positionRelativeToBase;
    if (affectedByPageScale)
        baseRelativePosition += m_position;

    bool wasRunningTransformAnimation = isRunningTransformAnimation();
    bool layerTypeChanged = false;
    
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
    
    if (GraphicsLayerCA* maskLayer = downcast<GraphicsLayerCA>(m_maskLayer.get())) {
        maskLayer->setVisibleAndCoverageRects(rects);
        maskLayer->commitLayerChangesBeforeSublayers(childCommitState, pageScaleFactor, baseRelativePosition, layerTypeChanged);
    }

    bool hasDescendantsWithRunningTransformAnimations = false;

    if (childCommitState.treeDepth <= cMaxLayerTreeDepth) {
        for (auto& layer : children()) {
            auto& currentChild = downcast<GraphicsLayerCA>(layer.get());
            currentChild.recursiveCommitChanges(childCommitState, localState, pageScaleFactor, baseRelativePosition, affectedByPageScale);

            if (currentChild.isRunningTransformAnimation() || currentChild.hasDescendantsWithRunningTransformAnimations())
                hasDescendantsWithRunningTransformAnimations = true;
        }
    }

    commitState.totalBackdropFilterArea = childCommitState.totalBackdropFilterArea;

    if (GraphicsLayerCA* replicaLayer = downcast<GraphicsLayerCA>(m_replicaLayer.get()))
        replicaLayer->recursiveCommitChanges(childCommitState, localState, pageScaleFactor, baseRelativePosition, affectedByPageScale);

    if (GraphicsLayerCA* maskLayer = downcast<GraphicsLayerCA>(m_maskLayer.get()))
        maskLayer->commitLayerChangesAfterSublayers(childCommitState);

    setHasDescendantsWithUncommittedChanges(false);
    setHasDescendantsWithRunningTransformAnimations(hasDescendantsWithRunningTransformAnimations);

    bool hadDirtyRects = m_uncommittedChanges & DirtyRectsChanged;
    commitLayerChangesAfterSublayers(childCommitState);

    if (affectedByTransformAnimation && m_layer->layerType() == PlatformCALayer::LayerTypeTiledBackingLayer)
        client().notifyFlushBeforeDisplayRefresh(this);

    if (layerTypeChanged)
        client().didChangePlatformLayerForLayer(this);

    if (usesDisplayListDrawing() && m_drawsContent && (!m_hasEverPainted || hadDirtyRects)) {
        TraceScope tracingScope(DisplayListRecordStart, DisplayListRecordEnd);

        m_displayList = makeUnique<DisplayList::InMemoryDisplayList>();
        
        FloatRect initialClip(boundsOrigin(), size());

        DisplayList::RecorderImpl context(*m_displayList, GraphicsContextState(), initialClip, AffineTransform());
        paintGraphicsLayerContents(context, FloatRect(FloatPoint(), size()));
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

void GraphicsLayerCA::platformCALayerPaintContents(PlatformCALayer*, GraphicsContext& context, const FloatRect& clip, GraphicsLayerPaintBehavior layerPaintBehavior)
{
    m_hasEverPainted = true;
    if (m_displayList) {
        DisplayList::Replayer replayer(context, *m_displayList);
        
        if (UNLIKELY(isTrackingDisplayListReplay())) {
            auto replayList = replayer.replay(clip, isTrackingDisplayListReplay()).trackedDisplayList;
            layerDisplayListMap().add(this, std::pair<FloatRect, std::unique_ptr<DisplayList::InMemoryDisplayList>>(clip, WTFMove(replayList)));
        } else
            replayer.replay(clip);

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

void GraphicsLayerCA::platformCALayerLogFilledVisibleFreshTile(unsigned blankPixelCount)
{
    client().logFilledVisibleFreshTile(blankPixelCount);
}

bool GraphicsLayerCA::platformCALayerDelegatesDisplay(PlatformCALayer* layer) const
{
    return m_contentsDisplayDelegate && m_contentsLayer == layer;
}

void GraphicsLayerCA::platformCALayerLayerDisplay(PlatformCALayer* layer)
{
    ASSERT(m_contentsDisplayDelegate);
    ASSERT(layer == m_contentsLayer);
    m_contentsDisplayDelegate->display(*layer);
}

void GraphicsLayerCA::commitLayerChangesBeforeSublayers(CommitState& commitState, float pageScaleFactor, const FloatPoint& positionRelativeToBase, bool& layerChanged)
{
    SetForScope committingChangesChange(m_isCommittingChanges, true);

    if (!m_uncommittedChanges) {
        // Ensure that we cap layer depth in commitLayerChangesAfterSublayers().
        if (commitState.treeDepth > cMaxLayerTreeDepth)
            addUncommittedChanges(ChildrenChanged);
    }

    bool needTiledLayer = requiresTiledLayer(pageScaleFactor);

    PlatformCALayer::LayerType currentLayerType = m_layer->layerType();
    PlatformCALayer::LayerType neededLayerType = currentLayerType;

    if (needTiledLayer)
        neededLayerType = PlatformCALayer::LayerTypeTiledBackingLayer;
    else if (currentLayerType == PlatformCALayer::LayerTypeTiledBackingLayer)
        neededLayerType = PlatformCALayer::LayerTypeWebLayer;

    if (neededLayerType != m_layer->layerType()) {
        changeLayerTypeTo(neededLayerType);
        layerChanged = true;
    }

    // Need to handle Preserves3DChanged first, because it affects which layers subsequent properties are applied to
    if (m_uncommittedChanges & (Preserves3DChanged | ReplicatedLayerChanged | BackdropFiltersChanged)) {
        if (updateStructuralLayer())
            layerChanged = true;
    }

    if (m_uncommittedChanges & GeometryChanged)
        updateGeometry(pageScaleFactor, positionRelativeToBase);

    if (m_uncommittedChanges & DrawsContentChanged)
        updateDrawsContent();

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

    if (m_uncommittedChanges & BackdropFiltersRectChanged)
        updateBackdropFiltersRect();

#if ENABLE(CSS_COMPOSITING)
    if (m_uncommittedChanges & BlendModeChanged)
        updateBlendMode();
#endif

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

    if (m_uncommittedChanges & SupportsSubpixelAntialiasedTextChanged)
        updateSupportsSubpixelAntialiasedText();

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
        m_structuralLayer->setName("preserve-3d: " + name);
        break;
    case StructuralLayerForReplicaFlattening:
        m_structuralLayer->setName("replica flattening: " + name);
        break;
    case StructuralLayerForBackdrop:
        m_structuralLayer->setName("backdrop hosting: " + name);
        break;
    case NoStructuralLayer:
        break;
    }
    m_layer->setName(name);
}

void GraphicsLayerCA::updateSublayerList(bool maxLayerDepthReached)
{
    if (maxLayerDepthReached) {
        m_layer->setSublayers({ });
        return;
    }

    auto appendStructuralLayerChildren = [&](PlatformCALayerList& list) {
        if (m_backdropLayer)
            list.append(m_backdropLayer);

        if (m_replicaLayer)
            list.append(downcast<GraphicsLayerCA>(*m_replicaLayer).primaryLayer());
    
        list.append(m_layer);
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
        if (auto* customSublayers = m_layer->customSublayers())
            list.appendVector(*customSublayers);

        if (m_contentsClippingLayer)
            appendClippingLayers(list);
        else
            appendContentsLayer(list);
    };

    auto appendLayersFromChildren = [&](PlatformCALayerList& list) {
        for (const auto& layer : children()) {
            const auto& currentChild = downcast<GraphicsLayerCA>(layer.get());
            PlatformCALayer* childLayer = currentChild.layerForSuperlayer();
            list.append(childLayer);
        }
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
    if (m_contentsClippingLayer) {
        PlatformCALayerList clippingChildren;
        if (clippingLayerHostsChildren)
            buildChildLayerList(clippingChildren);
        appendContentsLayer(clippingChildren);
        m_contentsClippingLayer->setSublayers(clippingChildren);
    }

    if (m_structuralLayer) {
        PlatformCALayerList layerList;
        appendStructuralLayerChildren(layerList);

        if (!clippingLayerHostsChildren)
            buildChildLayerList(layerList);

        m_structuralLayer->setSublayers(layerList);
    } else if (!clippingLayerHostsChildren)
        buildChildLayerList(primaryLayerChildren);

    m_layer->setSublayers(primaryLayerChildren);
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
    FloatPoint adjustedPosition(scaledPosition.x() + scaledAnchorPoint.x() * scaledSize.width(), scaledPosition.y() + scaledAnchorPoint.y() * scaledSize.height());

    if (m_structuralLayer) {
        FloatPoint layerPosition(m_position.x() + m_anchorPoint.x() * m_size.width(), m_position.y() + m_anchorPoint.y() * m_size.height());
        FloatRect layerBounds(m_boundsOrigin, m_size);

        m_structuralLayer->setPosition(layerPosition);
        m_structuralLayer->setBounds(layerBounds);
        m_structuralLayer->setAnchorPoint(m_anchorPoint);

        if (m_layerClones) {
            for (auto& clone : m_layerClones->structuralLayerClones) {
                PlatformCALayer* cloneLayer = clone.value.get();
                FloatPoint clonePosition = layerPosition;

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
    m_layer->setPosition(adjustedPosition);
    FloatRect adjustedBounds = FloatRect(FloatPoint(m_boundsOrigin - pixelAlignmentOffset), m_size);
    m_layer->setBounds(adjustedBounds);
    m_layer->setAnchorPoint(scaledAnchorPoint);

    if (m_layerClones) {
        for (auto& clone : m_layerClones->primaryLayerClones) {
            PlatformCALayer* cloneLayer = clone.value.get();
            FloatPoint clonePosition = adjustedPosition;

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
    primaryLayer()->setTransform(transform());

    if (LayerMap* layerCloneMap = primaryLayerClones()) {
        for (auto& clone : *layerCloneMap) {
            PlatformCALayer* currLayer = clone.value.get();
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
    primaryLayer()->setSublayerTransform(childrenTransform());

    if (LayerMap* layerCloneMap = primaryLayerClones()) {
        for (auto& layer : layerCloneMap->values())
            layer->setSublayerTransform(childrenTransform());
    }
}

void GraphicsLayerCA::updateMasksToBounds()
{
    m_layer->setMasksToBounds(m_masksToBounds);

    if (m_layerClones) {
        for (auto& layer : m_layerClones->primaryLayerClones.values())
            layer->setMasksToBounds(m_masksToBounds);
    }
}

void GraphicsLayerCA::updateContentsVisibility()
{
    // Note that m_contentsVisible also affects whether m_contentsLayer is parented.
    if (m_contentsVisible) {
        if (m_drawsContent)
            m_layer->setNeedsDisplay();

        if (m_backdropLayer)
            m_backdropLayer->setHidden(false);
    } else {
        m_layer->clearContents();

        if (m_layerClones) {
            for (auto& layer : m_layerClones->primaryLayerClones.values())
                layer->setContents(nullptr);
        }

        if (m_backdropLayer)
            m_backdropLayer->setHidden(true);
    }

    m_layer->setContentsHidden(!m_contentsVisible);
}

void GraphicsLayerCA::updateUserInteractionEnabled()
{
    m_layer->setUserInteractionEnabled(m_userInteractionEnabled);
}

void GraphicsLayerCA::updateContentsOpaque(float pageScaleFactor)
{
    bool contentsOpaque = m_contentsOpaque;
    if (contentsOpaque) {
        float contentsScale = pageScaleFactor * deviceScaleFactor();
        if (!WTF::isIntegral(contentsScale) && !client().paintsOpaquelyAtNonIntegralScales(this))
            contentsOpaque = false;
    }
    
    m_layer->setOpaque(contentsOpaque);

    if (m_layerClones) {
        for (auto& layer : m_layerClones->primaryLayerClones.values())
            layer->setOpaque(contentsOpaque);
    }
}

void GraphicsLayerCA::updateBackfaceVisibility()
{
    if (m_structuralLayer && structuralLayerPurpose() == StructuralLayerForReplicaFlattening) {
        m_structuralLayer->setDoubleSided(m_backfaceVisibility);

        if (m_layerClones) {
            for (auto& layer : m_layerClones->structuralLayerClones.values())
                layer->setDoubleSided(m_backfaceVisibility);
        }
    }

    m_layer->setDoubleSided(m_backfaceVisibility);

    if (m_layerClones) {
        for (auto& layer : m_layerClones->primaryLayerClones.values())
            layer->setDoubleSided(m_backfaceVisibility);
    }
}

void GraphicsLayerCA::updateFilters()
{
    m_layer->setFilters(m_filters);

    if (m_layerClones) {
        for (auto& clone : m_layerClones->primaryLayerClones) {
            if (m_replicaLayer && isReplicatedRootClone(clone.key))
                continue;

            clone.value->setFilters(m_filters);
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
        if (m_backdropLayer) {
            m_backdropLayer->removeFromSuperlayer();
            m_backdropLayer->setOwner(nullptr);
            m_backdropLayer = nullptr;
        }
        return;
    }

    // If nothing actually changed, no need to touch the layer properties.
    if (!(m_uncommittedChanges & BackdropFiltersChanged))
        return;

    bool madeLayer = !m_backdropLayer;
    if (!m_backdropLayer) {
        m_backdropLayer = createPlatformCALayer(PlatformCALayer::LayerTypeBackdropLayer, this);
        m_backdropLayer->setAnchorPoint(FloatPoint3D());
        m_backdropLayer->setMasksToBounds(true);
        m_backdropLayer->setName(MAKE_STATIC_STRING_IMPL("backdrop"));
    }

    m_backdropLayer->setHidden(!m_contentsVisible);
    m_backdropLayer->setFilters(m_backdropFilters);

    if (m_layerClones) {
        for (auto& clone : m_layerClones->backdropLayerClones) {
            PlatformCALayer* cloneLayer = clone.value.get();
            cloneLayer->setHidden(!m_contentsVisible);
            cloneLayer->setFilters(m_backdropFilters);
        }
    }

    if (madeLayer) {
        updateBackdropFiltersRect();
        noteSublayersChanged(DontScheduleFlush);
    }
}

void GraphicsLayerCA::updateBackdropFiltersRect()
{
    if (!m_backdropLayer)
        return;

    FloatRect contentBounds(0, 0, m_backdropFiltersRect.rect().width(), m_backdropFiltersRect.rect().height());
    m_backdropLayer->setBounds(contentBounds);
    m_backdropLayer->setPosition(m_backdropFiltersRect.rect().location());

    auto backdropRectRelativeToBackdropLayer = m_backdropFiltersRect;
    backdropRectRelativeToBackdropLayer.setLocation({ });
    updateClippingStrategy(*m_backdropLayer, m_backdropClippingLayer, backdropRectRelativeToBackdropLayer);

    if (m_layerClones) {
        for (auto& clone : m_layerClones->backdropLayerClones) {
            PlatformCALayer* backdropCloneLayer = clone.value.get();
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

#if ENABLE(CSS_COMPOSITING)
void GraphicsLayerCA::updateBlendMode()
{
    primaryLayer()->setBlendMode(m_blendMode);

    if (LayerMap* layerCloneMap = primaryLayerClones()) {
        for (auto& clone : *layerCloneMap) {
            if (m_replicaLayer && isReplicatedRootClone(clone.key))
                continue;
            clone.value->setBlendMode(m_blendMode);
        }
    }
}
#endif

void GraphicsLayerCA::updateShape()
{
    m_layer->setShapePath(m_shapeLayerPath);

    if (LayerMap* layerCloneMap = primaryLayerClones()) {
        for (auto& layer : layerCloneMap->values())
            layer->setShapePath(m_shapeLayerPath);
    }
}

void GraphicsLayerCA::updateWindRule()
{
    m_layer->setShapeWindRule(m_shapeLayerWindRule);
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

void GraphicsLayerCA::updateContentsScalingFilters()
{
    if (!m_contentsLayer)
        return;
    m_contentsLayer->setMinificationFilter(toPlatformCALayerFilterType(m_contentsMinificationFilter));
    m_contentsLayer->setMagnificationFilter(toPlatformCALayerFilterType(m_contentsMagnificationFilter));
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
        | MaskLayerChanged
        | OpacityChanged;

    bool structuralLayerChanged = false;

    if (purpose == NoStructuralLayer) {
        if (m_structuralLayer) {
            // Replace the transformLayer in the parent with this layer.
            m_layer->removeFromSuperlayer();
 
            // If m_layer doesn't have a parent, it means it's the root layer and
            // is likely hosted by something that is not expecting to be changed
            ASSERT(m_structuralLayer->superlayer());
            m_structuralLayer->superlayer()->replaceSublayer(*m_structuralLayer, *m_layer);

            moveAnimations(m_structuralLayer.get(), m_layer.get());

            // Release the structural layer.
            m_structuralLayer = nullptr;
            structuralLayerChanged = true;

            addUncommittedChanges(structuralLayerChangeFlags);
        }
        return structuralLayerChanged;
    }

    if (purpose == StructuralLayerForPreserves3D) {
        if (m_structuralLayer && m_structuralLayer->layerType() != PlatformCALayer::LayerTypeTransformLayer)
            m_structuralLayer = nullptr;
        
        if (!m_structuralLayer) {
            m_structuralLayer = createPlatformCALayer(PlatformCALayer::LayerTypeTransformLayer, this);
            structuralLayerChanged = true;
        }
    } else {
        if (m_structuralLayer && m_structuralLayer->layerType() != PlatformCALayer::LayerTypeLayer)
            m_structuralLayer = nullptr;

        if (!m_structuralLayer) {
            m_structuralLayer = createPlatformCALayer(PlatformCALayer::LayerTypeLayer, this);
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
    m_layer->setPosition(point);
    m_layer->setAnchorPoint(anchorPoint);
    m_layer->setTransform(TransformationMatrix());
    m_layer->setOpacity(1);
    if (m_layerClones) {
        for (auto& layer : m_layerClones->primaryLayerClones.values()) {
            layer->setPosition(point);
            layer->setAnchorPoint(anchorPoint);
            layer->setTransform(TransformationMatrix());
            layer->setOpacity(1);
        }
    }

    moveAnimations(m_layer.get(), m_structuralLayer.get());
    return true;
}

GraphicsLayerCA::StructuralLayerPurpose GraphicsLayerCA::structuralLayerPurpose() const
{
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
    if (m_drawsContent) {
        m_layer->setNeedsDisplay();
        m_hasEverPainted = false;
    } else {
        m_layer->clearContents();
        if (m_layerClones) {
            for (auto& layer : m_layerClones->primaryLayerClones.values())
                layer->setContents(nullptr);
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

    bool requiresBacking = m_intersectsCoverageRect
        || !allowsBackingStoreDetaching()
        || commitState.ancestorWithTransformAnimationIntersectsCoverageRect // FIXME: Compute backing exactly for descendants of animating layers.
        || (isRunningTransformAnimation() && !animationExtent()); // Create backing if we don't know the animation extent.

#if !LOG_DISABLED
    if (requiresBacking) {
        auto reasonForBacking = [&]() -> const char* {
            if (m_intersectsCoverageRect)
                return "intersectsCoverageRect";
            
            if (!allowsBackingStoreDetaching())
                return "backing detachment disallowed";

            if (commitState.ancestorWithTransformAnimationIntersectsCoverageRect)
                return "ancestor with transform";
            
            return "has transform animation with unknown extent";
        };
        LOG_WITH_STREAM(Layers, stream << "GraphicsLayerCA " << this << " id " << primaryLayerID() << " setBackingStoreAttached: " << requiresBacking << " (" << reasonForBacking() << ")");
    } else
        LOG_WITH_STREAM(Layers, stream << "GraphicsLayerCA " << this << " id " << primaryLayerID() << " setBackingStoreAttached: " << requiresBacking);
#endif

    m_layer->setBackingStoreAttached(requiresBacking);
    if (m_layerClones) {
        for (auto& layer : m_layerClones->primaryLayerClones.values())
            layer->setBackingStoreAttached(requiresBacking);
    }

    m_sizeAtLastCoverageRectUpdate = m_size;
}

void GraphicsLayerCA::updateAcceleratesDrawing()
{
    m_layer->setAcceleratesDrawing(m_acceleratesDrawing);
}

void GraphicsLayerCA::updateSupportsSubpixelAntialiasedText()
{
    m_layer->setSupportsSubpixelAntialiasedText(m_supportsSubpixelAntialiasedText);
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

    bool showDebugBorders = isShowingDebugBorder();
    if (showDebugBorders)
        getDebugBorderInfo(borderColor, width);

    // Paint repaint counter.
    m_layer->setNeedsDisplay();

    setLayerDebugBorder(*m_layer, borderColor, width);
    if (m_contentsLayer)
        setLayerDebugBorder(*m_contentsLayer, contentsLayerDebugBorderColor(showDebugBorders), contentsLayerBorderWidth);

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
    m_layer->setBackgroundColor(m_backgroundColor);
}

void GraphicsLayerCA::updateContentsImage()
{
    if (m_pendingContentsImage) {
        if (!m_contentsLayer.get()) {
            m_contentsLayer = createPlatformCALayer(PlatformCALayer::LayerTypeLayer, this);
#if ENABLE(TREE_DEBUGGING)
            m_contentsLayer->setName(makeString("contents image ", m_contentsLayer->layerID()));
#else
            m_contentsLayer->setName(MAKE_STATIC_STRING_IMPL("contents image"));
#endif
            setupContentsLayer(m_contentsLayer.get());
            // m_contentsLayer will be parented by updateSublayerList
        }

        // FIXME: maybe only do trilinear if the image is being scaled down,
        // but then what if the layer size changes?
        m_contentsLayer->setMinificationFilter(PlatformCALayer::Trilinear);
        m_contentsLayer->setContents(m_pendingContentsImage->platformImage().get());
        m_pendingContentsImage = nullptr;

        if (m_layerClones) {
            for (auto& layer : m_layerClones->contentsLayerClones.values())
                layer->setContents(m_contentsLayer->contents());
        }
        
        updateContentsRects();
    } else {
        // No image.
        // m_contentsLayer will be removed via updateSublayerList.
        m_contentsLayer = nullptr;
    }
}

void GraphicsLayerCA::updateContentsPlatformLayer()
{
    if (!m_contentsLayer)
        return;

    // Platform layer was set as m_contentsLayer, and will get parented in updateSublayerList().
    auto orientation = m_contentsDisplayDelegate ? m_contentsDisplayDelegate->orientation() : defaultContentsOrientation;
    setupContentsLayer(m_contentsLayer.get(), orientation);

    if (m_contentsLayerPurpose == ContentsLayerPurpose::Canvas)
        m_contentsLayer->setNeedsDisplay();

    updateContentsRects();
    updateContentsScalingFilters();
}

void GraphicsLayerCA::updateContentsColorLayer()
{
    // Color layer was set as m_contentsLayer, and will get parented in updateSublayerList().
    if (!m_contentsLayer || m_contentsLayerPurpose != ContentsLayerPurpose::BackgroundColor)
        return;

    setupContentsLayer(m_contentsLayer.get());
    updateContentsRects();
    ASSERT(m_contentsSolidColor.isValid());
    m_contentsLayer->setBackgroundColor(m_contentsSolidColor);

    if (m_layerClones) {
        for (auto& layer : m_layerClones->contentsLayerClones.values())
            layer->setBackgroundColor(m_contentsSolidColor);
    }
}

// The clipping strategy depends on whether the rounded rect has equal corner radii.
// roundedRect is in the coordinate space of clippingLayer.
void GraphicsLayerCA::updateClippingStrategy(PlatformCALayer& clippingLayer, RefPtr<PlatformCALayer>& shapeMaskLayer, const FloatRoundedRect& roundedRect)
{
    if (roundedRect.radii().isUniformCornerRadius() && clippingLayer.bounds() == roundedRect.rect()) {
        clippingLayer.setMask(nullptr);
        if (shapeMaskLayer) {
            shapeMaskLayer->setOwner(nullptr);
            shapeMaskLayer = nullptr;
        }

        clippingLayer.setMasksToBounds(true);
        clippingLayer.setCornerRadius(roundedRect.radii().topLeft().width());
        return;
    }

    if (!shapeMaskLayer) {
        shapeMaskLayer = createPlatformCALayer(PlatformCALayer::LayerTypeShapeLayer, this);
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
    clippingLayer.setMask(shapeMaskLayer.get());
}

void GraphicsLayerCA::updateContentsRects()
{
    if (!m_contentsLayer && !m_contentsRectClipsDescendants)
        return;

    auto contentBounds = FloatRect { { }, m_contentsRect.size() };
    
    bool gainedOrLostClippingLayer = false;
    if (m_contentsClippingRect.isRounded() || !m_contentsClippingRect.rect().contains(m_contentsRect)) {
        if (!m_contentsClippingLayer) {
            m_contentsClippingLayer = createPlatformCALayer(PlatformCALayer::LayerTypeLayer, this);
            m_contentsClippingLayer->setAnchorPoint({ });
#if ENABLE(TREE_DEBUGGING)
            m_contentsClippingLayer->setName(makeString("contents clipping ", m_contentsClippingLayer->layerID()));
#else
            m_contentsClippingLayer->setName(MAKE_STATIC_STRING_IMPL("contents clipping"));
#endif
            gainedOrLostClippingLayer = true;
        }

        m_contentsClippingLayer->setPosition(m_contentsClippingRect.rect().location());
        m_contentsClippingLayer->setBounds(m_contentsClippingRect.rect());
        
        updateClippingStrategy(*m_contentsClippingLayer, m_contentsShapeMaskLayer, m_contentsClippingRect);

        if (m_contentsLayer && gainedOrLostClippingLayer) {
            m_contentsLayer->removeFromSuperlayer();
            m_contentsClippingLayer->appendSublayer(*m_contentsLayer);
        }
    } else {
        if (m_contentsClippingLayer) {
            if (m_contentsLayer)
                m_contentsLayer->removeFromSuperlayer();

            m_contentsClippingLayer->removeFromSuperlayer();
            m_contentsClippingLayer->setOwner(nullptr);
            m_contentsClippingLayer->setMask(nullptr);
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

    if (m_contentsLayer) {
        m_contentsLayer->setPosition(m_contentsRect.location());
        m_contentsLayer->setBounds(contentBounds);
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
            updateClippingStrategy(*clone.value, shapeMaskLayerClone, m_contentsClippingRect);

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
    PlatformCALayer* maskCALayer = m_maskLayer ? downcast<GraphicsLayerCA>(*m_maskLayer).primaryLayer() : nullptr;
    
    LayerMap* layerCloneMap;
    if (m_structuralLayer && structuralLayerPurpose() == StructuralLayerForBackdrop) {
        m_structuralLayer->setMask(maskCALayer);
        layerCloneMap = m_layerClones ? &m_layerClones->structuralLayerClones : nullptr;
    } else {
        m_layer->setMask(maskCALayer);
        layerCloneMap = m_layerClones ? &m_layerClones->primaryLayerClones : nullptr;
    }

    LayerMap* maskLayerCloneMap = m_maskLayer ? downcast<GraphicsLayerCA>(*m_maskLayer).primaryLayerClones() : nullptr;
    if (layerCloneMap) {
        for (auto& clone : *layerCloneMap) {
            PlatformCALayer* maskClone = maskLayerCloneMap ? maskLayerCloneMap->get(clone.key) : nullptr;
            clone.value->setMask(maskClone);
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

    if (m_structuralLayer)
        m_structuralLayer->insertSublayer(*replicaRoot, 0);
    else
        m_layer->insertSublayer(*replicaRoot, 0);
}

// For now, this assumes that layers only ever have one replica, so replicaIndices contains only 0 and 1.
GraphicsLayerCA::CloneID GraphicsLayerCA::ReplicaState::cloneID() const
{
    size_t depth = m_replicaBranches.size();

    const size_t bitsPerUChar = sizeof(UChar) * 8;
    size_t vectorSize = (depth + bitsPerUChar - 1) / bitsPerUChar;
    
    Vector<UChar> result(vectorSize);
    result.fill(0);

    // Create a string from the bit sequence which we can use to identify the clone.
    // Note that the string may contain embedded nulls, but that's OK.
    for (size_t i = 0; i < depth; ++i) {
        UChar& currChar = result[i / bitsPerUChar];
        currChar = (currChar << 1) | m_replicaBranches[i];
    }
    
    return String::adopt(WTFMove(result));
}

RefPtr<PlatformCALayer> GraphicsLayerCA::replicatedLayerRoot(ReplicaState& replicaState)
{
    // Limit replica nesting, to avoid 2^N explosion of replica layers.
    if (!m_replicatedLayer || replicaState.replicaDepth() == ReplicaState::maxReplicaDepth)
        return nullptr;

    GraphicsLayerCA& replicatedLayer = downcast<GraphicsLayerCA>(*m_replicatedLayer);
    
    RefPtr<PlatformCALayer> clonedLayerRoot = replicatedLayer.fetchCloneLayers(this, replicaState, RootCloneLevel);
    FloatPoint cloneRootPosition = replicatedLayer.positionForCloneRootLayer();

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

    auto addAnimationGroup = [&](AnimatedPropertyID property, const Vector<RefPtr<PlatformCAAnimation>>& animations) {
        auto caAnimationGroup = createPlatformCAAnimation(PlatformCAAnimation::Group, emptyString());
        caAnimationGroup->setDuration(infiniteDuration);
        caAnimationGroup->setAnimations(animations);

        auto animationGroup = LayerPropertyAnimation(WTFMove(caAnimationGroup), makeString("group-"_s, UUID::createVersion4()), property, 0, 0, 0_s);
        animationGroup.m_beginTime = animationGroupBeginTime;

        setAnimationOnLayer(animationGroup);
        m_animationGroups.append(WTFMove(animationGroup));
    };

    enum class Additive { Yes, No };
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
    auto makeBaseValueTransformAnimation = [&](AnimatedPropertyID property, TransformationMatrixSource matrixSource = TransformationMatrixSource::AskClient, Seconds beginTimeOfEarliestPropertyAnimation = 0_s) -> LayerPropertyAnimation* {
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
        auto caAnimation = createPlatformCAAnimation(PlatformCAAnimation::Basic, propertyIdToString(property));
        caAnimation->setDuration(delay ? delay.seconds() : infiniteDuration);
        caAnimation->setFromValue(matrix);
        caAnimation->setToValue(matrix);

        auto animation = LayerPropertyAnimation(WTFMove(caAnimation), makeString("base-transform-"_s, UUID::createVersion4()), property, 0, 0, 0_s);
        if (delay)
            animation.m_beginTime = currentTime - animationGroupBeginTime;

        m_baseValueTransformAnimations.append(WTFMove(animation));
        return &m_baseValueTransformAnimations.last();
    };

    auto addBaseValueTransformAnimation = [&](AnimatedPropertyID property, TransformationMatrixSource matrixSource = TransformationMatrixSource::AskClient, Seconds beginTimeOfEarliestPropertyAnimation = 0_s) {
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
        case AnimatedPropertyTranslate:
            translateAnimation = &animation;
            break;
        case AnimatedPropertyScale:
            scaleAnimation = &animation;
            break;
        case AnimatedPropertyRotate:
            rotateAnimation = &animation;
            break;
        case AnimatedPropertyTransform:
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
        case AnimatedPropertyOpacity:
        case AnimatedPropertyBackgroundColor:
        case AnimatedPropertyFilter:
#if ENABLE(FILTERS_LEVEL_2)
        case AnimatedPropertyWebkitBackdropFilter:
#endif
            addLeafAnimation(animation);
            break;
        case AnimatedPropertyInvalid:
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
        addBaseValueTransformAnimation(AnimatedPropertyTransform, TransformationMatrixSource::UseIdentityMatrix);

        auto addAnimationsForProperty = [&](const Vector<LayerPropertyAnimation*>& animations, AnimatedPropertyID property) {
            if (animations.isEmpty()) {
                addBaseValueTransformAnimation(property);
                return;
            }

            LayerPropertyAnimation* earliestAnimation = nullptr;
            Vector<RefPtr<PlatformCAAnimation>> caAnimations;
            for (auto* animation : makeReversedRange(animations)) {
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
                auto fillMode = earliestAnimation->m_animation->fillMode();
                if (fillMode != PlatformCAAnimation::Backwards && fillMode != PlatformCAAnimation::Both) {
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

        addAnimationsForProperty(transformAnimations, AnimatedPropertyTransform);
        addAnimationsForProperty(scaleAnimations, AnimatedPropertyScale);
        addAnimationsForProperty(rotateAnimations, AnimatedPropertyRotate);
        addAnimationsForProperty(translateAnimations, AnimatedPropertyTranslate);
    }
}

bool GraphicsLayerCA::isRunningTransformAnimation() const
{
    return m_animations.findIf([&](LayerPropertyAnimation animation) {
        return animatedPropertyIsTransformOrRelated(animation.m_property) && (animation.m_playState == PlayState::Playing || animation.m_playState == PlayState::Paused);
    }) != notFound;
}

void GraphicsLayerCA::setAnimationOnLayer(LayerPropertyAnimation& animation)
{
    auto property = animation.m_property;
    PlatformCALayer* layer = animatedLayer(property);

    auto& caAnim = *animation.m_animation;

    if (auto beginTime = animation.computedBeginTime())
        caAnim.setBeginTime(beginTime->seconds());

    String animationID = animation.animationIdentifier();

    layer->removeAnimationForKey(animationID);
    layer->addAnimationForKey(animationID, caAnim);

    if (LayerMap* layerCloneMap = animatedLayerClones(property)) {
        for (auto& clone : *layerCloneMap) {
            // Skip immediate replicas, since they move with the original.
            if (m_replicaLayer && isReplicatedRootClone(clone.key))
                continue;

            clone.value->removeAnimationForKey(animationID);
            clone.value->addAnimationForKey(animationID, caAnim);
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
    PlatformCALayer* layer = animatedLayer(animation.m_property);

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

            clone.value->removeAnimationForKey(animationID);
        }
    }
    return true;
}

void GraphicsLayerCA::pauseCAAnimationOnLayer(LayerPropertyAnimation& animation)
{
    PlatformCALayer* layer = animatedLayer(animation.m_property);

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
            clone.value->addAnimationForKey(animationID, *newAnim);
        }
    }
}

void GraphicsLayerCA::repaintLayerDirtyRects()
{
    if (m_needsFullRepaint) {
        ASSERT(!m_dirtyRects.size());
        m_layer->setNeedsDisplay();
        m_needsFullRepaint = false;
        return;
    }

    for (auto& dirtyRect : m_dirtyRects)
        m_layer->setNeedsDisplayInRect(dirtyRect);
    
    m_dirtyRects.clear();
}

void GraphicsLayerCA::updateContentsNeedsDisplay()
{
    if (m_contentsLayer)
        m_contentsLayer->setNeedsDisplay();
}

static bool isKeyframe(const KeyframeValueList& list)
{
    return list.size() > 1;
}

bool GraphicsLayerCA::createAnimationFromKeyframes(const KeyframeValueList& valueList, const Animation* animation, const String& animationName, Seconds timeOffset, bool keyframesShouldUseAnimationWideTimingFunction)
{
    ASSERT(!animatedPropertyIsTransformOrRelated(valueList.property()) && (!supportsAcceleratedFilterAnimations() || valueList.property() != AnimatedPropertyFilter));

    bool valuesOK;
    
    bool additive = false;
    int animationIndex = 0;
    
    RefPtr<PlatformCAAnimation> caAnimation;

    if (isKeyframe(valueList)) {
        caAnimation = createKeyframeAnimation(animation, propertyIdToString(valueList.property()), additive, keyframesShouldUseAnimationWideTimingFunction);
        valuesOK = setAnimationKeyframes(valueList, animation, caAnimation.get(), keyframesShouldUseAnimationWideTimingFunction);
    } else {
        if (animation->timingFunction()->isSpringTimingFunction())
            caAnimation = createSpringAnimation(animation, propertyIdToString(valueList.property()), additive, keyframesShouldUseAnimationWideTimingFunction);
        else
            caAnimation = createBasicAnimation(animation, propertyIdToString(valueList.property()), additive, keyframesShouldUseAnimationWideTimingFunction);
        valuesOK = setAnimationEndpoints(valueList, animation, caAnimation.get());
    }

    if (!valuesOK)
        return false;

    m_animations.append(LayerPropertyAnimation(caAnimation.releaseNonNull(), animationName, valueList.property(), animationIndex, 0, timeOffset));

    return true;
}

bool GraphicsLayerCA::appendToUncommittedAnimations(const KeyframeValueList& valueList, TransformOperation::OperationType operationType, const Animation* animation, const String& animationName, const FloatSize& boxSize, unsigned animationIndex, Seconds timeOffset, bool isMatrixAnimation, bool keyframesShouldUseAnimationWideTimingFunction)
{
    RefPtr<PlatformCAAnimation> caAnimation;
    bool validMatrices = true;
    if (isKeyframe(valueList)) {
        caAnimation = createKeyframeAnimation(animation, propertyIdToString(valueList.property()), false, keyframesShouldUseAnimationWideTimingFunction);
        validMatrices = setTransformAnimationKeyframes(valueList, animation, caAnimation.get(), animationIndex, operationType, isMatrixAnimation, boxSize, keyframesShouldUseAnimationWideTimingFunction);
    } else {
        if (animation->timingFunction()->isSpringTimingFunction())
            caAnimation = createSpringAnimation(animation, propertyIdToString(valueList.property()), false, keyframesShouldUseAnimationWideTimingFunction);
        else
            caAnimation = createBasicAnimation(animation, propertyIdToString(valueList.property()), false, keyframesShouldUseAnimationWideTimingFunction);
        validMatrices = setTransformAnimationEndpoints(valueList, animation, caAnimation.get(), animationIndex, operationType, isMatrixAnimation, boxSize);
    }
    
    if (!validMatrices)
        return false;

    m_animations.append(LayerPropertyAnimation(caAnimation.releaseNonNull(), animationName, valueList.property(), animationIndex, 0, timeOffset));
    return true;
}

static const TransformOperations& transformationAnimationValueAt(const KeyframeValueList& valueList, unsigned i)
{
    return static_cast<const TransformAnimationValue&>(valueList.at(i)).value();
}

static bool hasBig3DRotation(const KeyframeValueList& valueList, const SharedPrimitivesPrefix& prefix)
{
    // Hardware non-matrix animations are used for every function in the shared primitives prefix.
    // These kind of animations have issues with large rotation angles, so for every function that
    // will be represented as a hardware non-matrix animation, check that for each of those functions
    // the animation that's created for it will not have two consecutive keyframes that have a large
    // rotation angle between them.
    const auto& primitives = prefix.primitives();
    for (unsigned animationIndex = 0; animationIndex < primitives.size(); ++animationIndex) {
        auto type = primitives[animationIndex];
        if (type != TransformOperation::ROTATE_3D)
            continue;
        for (size_t i = 1; i < valueList.size(); ++i) {
            // Since the shared primitive at this index is a rotation, both of these transform
            // functions should be RotateTransformOperations.
            auto prevOperation = downcast<RotateTransformOperation>(transformationAnimationValueAt(valueList, i - 1).at(animationIndex));
            auto operation = downcast<RotateTransformOperation>(transformationAnimationValueAt(valueList, i).at(animationIndex));
            auto angle = std::abs((prevOperation ? prevOperation->angle() : 0.0) - (operation ? operation->angle() : 0.0));
            if (angle > 180.0)
                return true;
        }
    }

    return false;
}

bool GraphicsLayerCA::createTransformAnimationsFromKeyframes(const KeyframeValueList& valueList, const Animation* animation, const String& animationName, Seconds timeOffset, const FloatSize& boxSize, bool keyframesShouldUseAnimationWideTimingFunction)
{
    ASSERT(animatedPropertyIsTransformOrRelated(valueList.property()));

    // https://www.w3.org/TR/css-transforms-1/#interpolation-of-transforms
    // In the CSS Transform Level 1 and 2 Specification some transform functions can share a compatible transform
    // function primitive. For instance, the shared primitive of a translateX and translate3D operation is
    // TransformOperation::TRANSLATE_3D. When the transform function list of every keyframe in an animation
    // shares the same transform function primitive, we should interpolate between them without resorting
    // to matrix decomposition. The remaining parts of the transform function list should be interpolated
    // using matrix decomposition. The code below finds the shared primitives in this prefix.
    // FIXME: Currently, this only supports situations where every keyframe shares the same prefix of shared
    // transformation primitives, but the specification says direct interpolation should be determined by
    // the primitives shared between any two adjacent keyframes.
    SharedPrimitivesPrefix prefix;
    for (size_t i = 0; i < valueList.size(); ++i)
        prefix.update(transformationAnimationValueAt(valueList, i));

    // If this animation has a big rotation between two keyframes, fall back to software animation. CoreAnimation
    // will always take the shortest path between two rotations, which will result in incorrect animation when
    // the keyframes specify angles larger than one half rotation.
    if (hasBig3DRotation(valueList, prefix))
        return false;

    const auto& primitives = prefix.primitives();
    unsigned numberOfSharedPrimitives = valueList.size() > 1 ? primitives.size() : 0;
    for (unsigned animationIndex = 0; animationIndex < numberOfSharedPrimitives; ++animationIndex) {
        if (!appendToUncommittedAnimations(valueList, primitives[animationIndex], animation, animationName, boxSize, animationIndex, timeOffset, false /* isMatrixAnimation */, keyframesShouldUseAnimationWideTimingFunction))
            return false;
    }

    if (!prefix.hadIncompatibleTransformFunctions())
        return true;

    // If there were any incompatible transform functions, they will be appended to the animation list
    // as a single combined transformation matrix animation.
    return appendToUncommittedAnimations(valueList, TransformOperation::MATRIX_3D, animation, animationName, boxSize, primitives.size(), timeOffset, true /* isMatrixAnimation */, keyframesShouldUseAnimationWideTimingFunction);
}

bool GraphicsLayerCA::appendToUncommittedAnimations(const KeyframeValueList& valueList, const FilterOperation* operation, const Animation* animation, const String& animationName, int animationIndex, Seconds timeOffset, bool keyframesShouldUseAnimationWideTimingFunction)
{
    FilterOperation::OperationType filterOp = operation->type();
    int numAnimatedProperties = PlatformCAFilters::numAnimatedFilterProperties(filterOp);
    
    // Each filter might need to animate multiple properties, each with their own keyPath. The keyPath is always of the form:
    //
    //      filter.filter_<animationIndex>.<filterPropertyName>
    //
    // PlatformCAAnimation tells us how many properties each filter has and we iterate that many times and create an animation
    // for each. This internalFilterPropertyIndex gets passed to PlatformCAAnimation so it can properly create the property animation
    // values.
    for (int internalFilterPropertyIndex = 0; internalFilterPropertyIndex < numAnimatedProperties; ++internalFilterPropertyIndex) {
        bool valuesOK;
        RefPtr<PlatformCAAnimation> caAnimation;
        auto keyPath = makeString("filters.filter_", animationIndex, '.', PlatformCAFilters::animatedFilterPropertyName(filterOp, internalFilterPropertyIndex));
        
        if (isKeyframe(valueList)) {
            caAnimation = createKeyframeAnimation(animation, keyPath, false, keyframesShouldUseAnimationWideTimingFunction);
            valuesOK = setFilterAnimationKeyframes(valueList, animation, caAnimation.get(), animationIndex, internalFilterPropertyIndex, filterOp, keyframesShouldUseAnimationWideTimingFunction);
        } else {
            caAnimation = createBasicAnimation(animation, keyPath, false, keyframesShouldUseAnimationWideTimingFunction);
            valuesOK = setFilterAnimationEndpoints(valueList, animation, caAnimation.get(), animationIndex, internalFilterPropertyIndex);
        }
        
        ASSERT_UNUSED(valuesOK, valuesOK);

        m_animations.append(LayerPropertyAnimation(caAnimation.releaseNonNull(), animationName, valueList.property(), animationIndex, internalFilterPropertyIndex, timeOffset));
    }

    return true;
}

bool GraphicsLayerCA::createFilterAnimationsFromKeyframes(const KeyframeValueList& valueList, const Animation* animation, const String& animationName, Seconds timeOffset, bool keyframesShouldUseAnimationWideTimingFunction)
{
#if ENABLE(FILTERS_LEVEL_2)
    ASSERT(valueList.property() == AnimatedPropertyFilter || valueList.property() == AnimatedPropertyWebkitBackdropFilter);
#else
    ASSERT(valueList.property() == AnimatedPropertyFilter);
#endif

    int listIndex = validateFilterOperations(valueList);
    if (listIndex < 0)
        return false;

    const FilterOperations& operations = static_cast<const FilterAnimationValue&>(valueList.at(listIndex)).value();
    // Make sure the platform layer didn't fallback to using software filter compositing instead.
    if (!filtersCanBeComposited(operations))
        return false;

    int numAnimations = operations.size();

    // FIXME: We can't currently hardware animate shadows.
    for (int i = 0; i < numAnimations; ++i) {
        if (operations.at(i)->type() == FilterOperation::DROP_SHADOW)
            return false;
    }

    for (int animationIndex = 0; animationIndex < numAnimations; ++animationIndex) {
        if (!appendToUncommittedAnimations(valueList, operations.operations().at(animationIndex).get(), animation, animationName, animationIndex, timeOffset, keyframesShouldUseAnimationWideTimingFunction))
            return false;
    }

    return true;
}

Ref<PlatformCAAnimation> GraphicsLayerCA::createBasicAnimation(const Animation* anim, const String& keyPath, bool additive, bool keyframesShouldUseAnimationWideTimingFunction)
{
    auto basicAnim = createPlatformCAAnimation(PlatformCAAnimation::Basic, keyPath);
    setupAnimation(basicAnim.ptr(), anim, additive, keyframesShouldUseAnimationWideTimingFunction);
    return basicAnim;
}

Ref<PlatformCAAnimation> GraphicsLayerCA::createKeyframeAnimation(const Animation* anim, const String& keyPath, bool additive, bool keyframesShouldUseAnimationWideTimingFunction)
{
    auto keyframeAnim = createPlatformCAAnimation(PlatformCAAnimation::Keyframe, keyPath);
    setupAnimation(keyframeAnim.ptr(), anim, additive, keyframesShouldUseAnimationWideTimingFunction);
    return keyframeAnim;
}

Ref<PlatformCAAnimation> GraphicsLayerCA::createSpringAnimation(const Animation* anim, const String& keyPath, bool additive, bool keyframesShouldUseAnimationWideTimingFunction)
{
    auto basicAnim = createPlatformCAAnimation(PlatformCAAnimation::Spring, keyPath);
    setupAnimation(basicAnim.ptr(), anim, additive, keyframesShouldUseAnimationWideTimingFunction);
    return basicAnim;
}

void GraphicsLayerCA::setupAnimation(PlatformCAAnimation* propertyAnim, const Animation* anim, bool additive, bool keyframesShouldUseAnimationWideTimingFunction)
{
    double duration = anim->duration();
    if (duration <= 0)
        duration = cAnimationAlmostZeroDuration;

    float repeatCount = anim->iterationCount();
    if (repeatCount == Animation::IterationCountInfinite)
        repeatCount = std::numeric_limits<float>::max();
    else if (anim->direction() == Animation::AnimationDirectionAlternate || anim->direction() == Animation::AnimationDirectionAlternateReverse)
        repeatCount /= 2;

    PlatformCAAnimation::FillModeType fillMode = PlatformCAAnimation::NoFillMode;
    switch (anim->fillMode()) {
    case AnimationFillMode::None:
        fillMode = PlatformCAAnimation::Forwards; // Use "forwards" rather than "removed" because the style system will remove the animation when it is finished. This avoids a flash.
        break;
    case AnimationFillMode::Backwards:
        fillMode = PlatformCAAnimation::Both; // Use "both" rather than "backwards" because the style system will remove the animation when it is finished. This avoids a flash.
        break;
    case AnimationFillMode::Forwards:
        fillMode = PlatformCAAnimation::Forwards;
        break;
    case AnimationFillMode::Both:
        fillMode = PlatformCAAnimation::Both;
        break;
    }

    propertyAnim->setDuration(duration);
    propertyAnim->setRepeatCount(repeatCount);
    propertyAnim->setAutoreverses(anim->direction() == Animation::AnimationDirectionAlternate || anim->direction() == Animation::AnimationDirectionAlternateReverse);
    propertyAnim->setRemovedOnCompletion(false);
    propertyAnim->setAdditive(additive);
    propertyAnim->setFillMode(fillMode);

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=215918
    // A CSS Transition is the only scenario where Animation::property() will have
    // its mode set to SingleProperty. In this case, we don't set the animation-wide
    // timing function to work around a Core Animation limitation.
    if (!keyframesShouldUseAnimationWideTimingFunction)
        propertyAnim->setTimingFunction(anim->timingFunction());
}

const TimingFunction& GraphicsLayerCA::timingFunctionForAnimationValue(const AnimationValue& animValue, const Animation& anim, bool keyframesShouldUseAnimationWideTimingFunction)
{
    if (keyframesShouldUseAnimationWideTimingFunction && anim.timingFunction()) {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=215918
        // A CSS Transition is the only scenario where Animation::property() will have
        // its mode set to SingleProperty. In this case, we chose not to set set the
        // animation-wide timing function, so we set it on the single keyframe interval
        // to work around a Core Animation limitation.
        return *anim.timingFunction();
    }
    if (animValue.timingFunction())
        return *animValue.timingFunction();
    if (anim.defaultTimingFunctionForKeyframes())
        return *anim.defaultTimingFunctionForKeyframes();
    return LinearTimingFunction::sharedLinearTimingFunction();
}

bool GraphicsLayerCA::setAnimationEndpoints(const KeyframeValueList& valueList, const Animation* animation, PlatformCAAnimation* basicAnim)
{
    bool forwards = animation->directionIsForwards();

    unsigned fromIndex = !forwards;
    unsigned toIndex = forwards;
    
    switch (valueList.property()) {
    case AnimatedPropertyOpacity: {
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

bool GraphicsLayerCA::setAnimationKeyframes(const KeyframeValueList& valueList, const Animation* animation, PlatformCAAnimation* keyframeAnim, bool keyframesShouldUseAnimationWideTimingFunction)
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
        case AnimatedPropertyOpacity: {
            const FloatAnimationValue& floatValue = static_cast<const FloatAnimationValue&>(curValue);
            values.append(floatValue.value());
            break;
        }
        default:
            ASSERT_NOT_REACHED(); // we don't animate color yet
            break;
        }

        if (i < (valueList.size() - 1))
            timingFunctions.append(timingFunctionForAnimationValue(forwards ? curValue : valueList.at(index - 1), *animation, keyframesShouldUseAnimationWideTimingFunction));
    }

    keyframeAnim->setKeyTimes(keyTimes);
    keyframeAnim->setValues(values);
    keyframeAnim->setTimingFunctions(timingFunctions, !forwards);

    return true;
}

bool GraphicsLayerCA::setTransformAnimationEndpoints(const KeyframeValueList& valueList, const Animation* animation, PlatformCAAnimation* basicAnim, int functionIndex, TransformOperation::OperationType transformOpType, bool isMatrixAnimation, const FloatSize& boxSize)
{
    ASSERT(valueList.size() == 2);

    bool forwards = animation->directionIsForwards();
    
    unsigned fromIndex = !forwards;
    unsigned toIndex = forwards;
    
    const auto& startValue = transformationAnimationValueAt(valueList, fromIndex);
    const auto& endValue = transformationAnimationValueAt(valueList, toIndex);

    if (isMatrixAnimation) {
        TransformationMatrix fromTransform, toTransform;
        startValue.apply(boxSize, fromTransform);
        endValue.apply(boxSize, toTransform);

        // If any matrix is singular, CA won't animate it correctly. So fall back to software animation
        if (!fromTransform.isInvertible() || !toTransform.isInvertible())
            return false;

        basicAnim->setFromValue(fromTransform);
        basicAnim->setToValue(toTransform);
    } else {
        if (isTransformTypeNumber(transformOpType)) {
            float fromValue;
            getTransformFunctionValue(startValue.at(functionIndex), transformOpType, boxSize, fromValue);
            basicAnim->setFromValue(fromValue);
            
            float toValue;
            getTransformFunctionValue(endValue.at(functionIndex), transformOpType, boxSize, toValue);
            basicAnim->setToValue(toValue);
        } else if (isTransformTypeFloatPoint3D(transformOpType)) {
            FloatPoint3D fromValue;
            getTransformFunctionValue(startValue.at(functionIndex), transformOpType, boxSize, fromValue);
            basicAnim->setFromValue(fromValue);
            
            FloatPoint3D toValue;
            getTransformFunctionValue(endValue.at(functionIndex), transformOpType, boxSize, toValue);
            basicAnim->setToValue(toValue);
        } else {
            TransformationMatrix fromValue;
            getTransformFunctionValue(startValue.at(functionIndex), transformOpType, boxSize, fromValue);
            basicAnim->setFromValue(fromValue);

            TransformationMatrix toValue;
            getTransformFunctionValue(endValue.at(functionIndex), transformOpType, boxSize, toValue);
            basicAnim->setToValue(toValue);
        }
    }

    auto valueFunction = getValueFunctionNameForTransformOperation(transformOpType);
    if (valueFunction != PlatformCAAnimation::NoValueFunction)
        basicAnim->setValueFunction(valueFunction);

    return true;
}

bool GraphicsLayerCA::setTransformAnimationKeyframes(const KeyframeValueList& valueList, const Animation* animation, PlatformCAAnimation* keyframeAnim, int functionIndex, TransformOperation::OperationType transformOpType, bool isMatrixAnimation, const FloatSize& boxSize, bool keyframesShouldUseAnimationWideTimingFunction)
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
            curValue.value().apply(functionIndex, boxSize, transform);

            // If any matrix is singular, CA won't animate it correctly. So fall back to software animation
            if (!transform.isInvertible())
                return false;

            transformationMatrixValues.append(transform);
        } else {
            const TransformOperation* transformOp = curValue.value().at(functionIndex);
            if (isTransformTypeNumber(transformOpType)) {
                float value;
                getTransformFunctionValue(transformOp, transformOpType, boxSize, value);
                floatValues.append(value);
            } else if (isTransformTypeFloatPoint3D(transformOpType)) {
                FloatPoint3D value;
                getTransformFunctionValue(transformOp, transformOpType, boxSize, value);
                floatPoint3DValues.append(value);
            } else {
                TransformationMatrix value;
                getTransformFunctionValue(transformOp, transformOpType, boxSize, value);
                transformationMatrixValues.append(value);
            }
        }

        if (i < (valueList.size() - 1))
            timingFunctions.append(timingFunctionForAnimationValue(forwards ? curValue : valueList.at(index - 1), *animation, keyframesShouldUseAnimationWideTimingFunction));
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
    if (valueFunction != PlatformCAAnimation::NoValueFunction)
        keyframeAnim->setValueFunction(valueFunction);

    return true;
}

bool GraphicsLayerCA::setFilterAnimationEndpoints(const KeyframeValueList& valueList, const Animation* animation, PlatformCAAnimation* basicAnim, int functionIndex, int internalFilterPropertyIndex)
{
    ASSERT(valueList.size() == 2);

    bool forwards = animation->directionIsForwards();

    unsigned fromIndex = !forwards;
    unsigned toIndex = forwards;

    const FilterAnimationValue& fromValue = static_cast<const FilterAnimationValue&>(valueList.at(fromIndex));
    const FilterAnimationValue& toValue = static_cast<const FilterAnimationValue&>(valueList.at(toIndex));

    const FilterOperation* fromOperation = fromValue.value().at(functionIndex);
    const FilterOperation* toOperation = toValue.value().at(functionIndex);

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

    basicAnim->setFromValue(fromOperation, internalFilterPropertyIndex);
    basicAnim->setToValue(toOperation, internalFilterPropertyIndex);

    return true;
}

bool GraphicsLayerCA::setFilterAnimationKeyframes(const KeyframeValueList& valueList, const Animation* animation, PlatformCAAnimation* keyframeAnim, int functionIndex, int internalFilterPropertyIndex, FilterOperation::OperationType filterOp, bool keyframesShouldUseAnimationWideTimingFunction)
{
    Vector<float> keyTimes;
    Vector<RefPtr<FilterOperation>> values;
    Vector<Ref<const TimingFunction>> timingFunctions;
    RefPtr<DefaultFilterOperation> defaultOperation;

    bool forwards = animation->directionIsForwards();

    for (unsigned i = 0; i < valueList.size(); ++i) {
        unsigned index = forwards ? i : (valueList.size() - i - 1);
        const FilterAnimationValue& curValue = static_cast<const FilterAnimationValue&>(valueList.at(index));
        keyTimes.append(forwards ? curValue.keyTime() : (1 - curValue.keyTime()));

        if (curValue.value().operations().size() > static_cast<size_t>(functionIndex))
            values.append(curValue.value().operations()[functionIndex]);
        else {
            if (!defaultOperation)
                defaultOperation = DefaultFilterOperation::create(filterOp);
            values.append(defaultOperation);
        }

        if (i < (valueList.size() - 1))
            timingFunctions.append(timingFunctionForAnimationValue(forwards ? curValue : valueList.at(index - 1), *animation, keyframesShouldUseAnimationWideTimingFunction));
    }
    
    keyframeAnim->setKeyTimes(keyTimes);
    keyframeAnim->setValues(values, internalFilterPropertyIndex);
    keyframeAnim->setTimingFunctions(timingFunctions, !forwards);

    return true;
}

void GraphicsLayerCA::suspendAnimations(MonotonicTime time)
{
    double t = PlatformCALayer::currentTimeToMediaTime(time ? time : MonotonicTime::now());
    primaryLayer()->setSpeed(0);
    primaryLayer()->setTimeOffset(t);

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
    primaryLayer()->setSpeed(1);
    primaryLayer()->setTimeOffset(0);

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

PlatformCALayer* GraphicsLayerCA::animatedLayer(AnimatedPropertyID property) const
{
    switch (property) {
    case AnimatedPropertyBackgroundColor:
        return m_contentsLayer.get();
#if ENABLE(FILTERS_LEVEL_2)
    case AnimatedPropertyWebkitBackdropFilter:
        // FIXME: Should be just m_backdropLayer.get(). Also, add an ASSERT(m_backdropLayer) here when https://bugs.webkit.org/show_bug.cgi?id=145322 is fixed.
        return m_backdropLayer ? m_backdropLayer.get() : primaryLayer();
#endif
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

GraphicsLayerCA::LayerMap* GraphicsLayerCA::animatedLayerClones(AnimatedPropertyID property) const
{
    if (!m_layerClones)
        return nullptr;

    return (property == AnimatedPropertyBackgroundColor) ? &m_layerClones->contentsLayerClones : primaryLayerClones();
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
        transform.decompose2(decomposeData);
        return std::max(std::abs(decomposeData.scaleX), std::abs(decomposeData.scaleY));
    };

    float rootRelativeScaleFactor = hasNonIdentityTransform() ? computeMaxScaleFromTransform(transform()) : 1;

    if (auto* parentLayer = parent(); parentLayer && parentLayer->hasNonIdentityChildrenTransform())
        rootRelativeScaleFactor = std::max(rootRelativeScaleFactor, computeMaxScaleFromTransform(parentLayer->childrenTransform()));

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

    if (contentsScale == m_layer->contentsScale())
        return;

    m_layer->setContentsScale(contentsScale);

    if (m_contentsLayer && m_contentsLayerPurpose == ContentsLayerPurpose::Media)
        m_contentsLayer->setContentsScale(contentsScale);

    if (tiledBacking()) {
        // Tiled backing repaints automatically on scale change.
        return;
    }

    if (drawsContent())
        m_layer->setNeedsDisplay();
}

void GraphicsLayerCA::updateCustomAppearance()
{
    m_layer->updateCustomAppearance(m_customAppearance);
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
        stream.dumpProperty("clip", it->value.first);
        stream << it->value.second->asText(flags);
        return stream.release();
        
    }

    return String();
}

void GraphicsLayerCA::setDebugBackgroundColor(const Color& color)
{    
    if (color.isValid())
        m_layer->setBackgroundColor(color);
    else
        m_layer->setBackgroundColor(Color::transparentBlack);
}

void GraphicsLayerCA::getDebugBorderInfo(Color& color, float& width) const
{
    if (isPageTiledBackingLayer()) {
        color = SRGBA<uint8_t> { 0, 0, 128, 128 }; // tile cache layer: dark blue
#if OS(WINDOWS)
        width = 1.0;
#else
        width = 0.5;
#endif
        return;
    }

    GraphicsLayer::getDebugBorderInfo(color, width);
}

const char* GraphicsLayerCA::purposeNameForInnerLayer(PlatformCALayer& layer) const
{
    if (&layer == m_structuralLayer.get())
        return "structural layer";
    if (&layer == m_contentsClippingLayer.get())
        return "contents clipping layer";
    if (&layer == m_shapeMaskLayer.get())
        return "shape mask layer";
    if (&layer == m_backdropClippingLayer.get())
        return "backdrop clipping layer";
    if (&layer == m_contentsLayer.get()) {
        switch (m_contentsLayerPurpose) {
        case ContentsLayerPurpose::None:
            return "contents layer (none)";
        case ContentsLayerPurpose::Image:
            return "contents layer (image)";
        case ContentsLayerPurpose::Media:
            return "contents layer (media)";
        case ContentsLayerPurpose::Canvas:
            return "contents layer (canvas)";
        case ContentsLayerPurpose::BackgroundColor:
            return "contents layer (background color)";
        case ContentsLayerPurpose::Plugin:
            return "contents layer (plugin)";
        case ContentsLayerPurpose::Model:
            return "contents layer (model)";
        }
    }
    if (&layer == m_contentsShapeMaskLayer.get())
        return "contents shape mask layer";
    if (&layer == m_backdropLayer.get())
        return "backdrop layer";
    return "platform layer";
}

void GraphicsLayerCA::dumpInnerLayer(TextStream& ts, PlatformCALayer* layer, OptionSet<PlatformLayerTreeAsTextFlags> flags) const
{
    if (!layer)
        return;

    ts << indent << "(" << purposeNameForInnerLayer(*layer) << "\n";

    {
        TextStream::IndentScope indentScope(ts);

        if (flags.contains(PlatformLayerTreeAsTextFlags::Debug))
            ts << indent << "(id " << layer->layerID() << ")\n";

        ts << indent << "(position " << layer->position().x() << " " << layer->position().y() << ")\n";
        ts << indent << "(bounds " << layer->bounds().width() << " " << layer->bounds().height() << ")\n";
        
        if (layer->opacity() != 1)
            ts << indent << "(opacity " << layer->opacity() << ")\n";

        if (layer->isHidden())
            ts << indent << "(hidden)\n";

        layer->dumpAdditionalProperties(ts, flags);

        if (!flags.contains(PlatformLayerTreeAsTextFlags::IgnoreChildren)) {
            auto sublayers = layer->sublayersForLogging();
            if (sublayers.size()) {
                ts << indent << "(children " << "\n";

                {
                    TextStream::IndentScope indentScope(ts);
                    for (auto& child : sublayers)
                        dumpInnerLayer(ts, child.get(), flags);
                }

                ts << indent << ")\n";
            }
        }
    }

    ts << indent << ")\n";
}

static TextStream& operator<<(TextStream& textStream, AnimatedPropertyID propertyID)
{
    switch (propertyID) {
    case AnimatedPropertyInvalid: textStream << "invalid"; break;
    case AnimatedPropertyTranslate: textStream << "translate"; break;
    case AnimatedPropertyScale: textStream << "scale"; break;
    case AnimatedPropertyRotate: textStream << "rotate"; break;
    case AnimatedPropertyTransform: textStream << "transform"; break;
    case AnimatedPropertyOpacity: textStream << "opacity"; break;
    case AnimatedPropertyBackgroundColor: textStream << "background-color"; break;
    case AnimatedPropertyFilter: textStream << "filter"; break;
#if ENABLE(FILTERS_LEVEL_2)
    case AnimatedPropertyWebkitBackdropFilter: textStream << "backdrop-filter"; break;
#endif
    }
    return textStream;
}

void GraphicsLayerCA::dumpAnimations(WTF::TextStream& textStream, const char* category, const Vector<LayerPropertyAnimation>& animations)
{
    auto dumpAnimation = [&textStream](const LayerPropertyAnimation& animation) {
        textStream << indent << "(" << animation.m_name;
        {
            TextStream::IndentScope indentScope(textStream);
            textStream.dumpProperty("CA animation", animation.m_animation.get());
            textStream.dumpProperty("property", animation.m_property);
            textStream.dumpProperty("index", animation.m_index);
            textStream.dumpProperty("subindex", animation.m_subIndex);
            textStream.dumpProperty("time offset", animation.m_timeOffset);
            textStream.dumpProperty("begin time", animation.m_beginTime);
            textStream.dumpProperty("play state", (unsigned)animation.m_playState);
            if (animation.m_pendingRemoval)
                textStream.dumpProperty("pending removal", animation.m_pendingRemoval);
            textStream << ")";
        }
    };
    
    if (animations.isEmpty())
        return;
    
    textStream << indent << "(" << category << "\n";
    {
        TextStream::IndentScope indentScope(textStream);
        for (const auto& animation : animations) {
            TextStream::IndentScope indentScope(textStream);
            dumpAnimation(animation);
        }

        textStream << ")\n";
    }
}

const char* GraphicsLayerCA::layerChangeAsString(LayerChange layerChange)
{
    switch (layerChange) {
    case LayerChange::NoChange: return ""; break;
    case LayerChange::NameChanged: return "NameChanged";
    case LayerChange::ChildrenChanged: return "ChildrenChanged";
    case LayerChange::GeometryChanged: return "GeometryChanged";
    case LayerChange::TransformChanged: return "TransformChanged";
    case LayerChange::ChildrenTransformChanged: return "ChildrenTransformChanged";
    case LayerChange::Preserves3DChanged: return "Preserves3DChanged";
    case LayerChange::MasksToBoundsChanged: return "MasksToBoundsChanged";
    case LayerChange::DrawsContentChanged: return "DrawsContentChanged";
    case LayerChange::BackgroundColorChanged: return "BackgroundColorChanged";
    case LayerChange::ContentsOpaqueChanged: return "ContentsOpaqueChanged";
    case LayerChange::BackfaceVisibilityChanged: return "BackfaceVisibilityChanged";
    case LayerChange::OpacityChanged: return "OpacityChanged";
    case LayerChange::AnimationChanged: return "AnimationChanged";
    case LayerChange::DirtyRectsChanged: return "DirtyRectsChanged";
    case LayerChange::ContentsImageChanged: return "ContentsImageChanged";
    case LayerChange::ContentsPlatformLayerChanged: return "ContentsPlatformLayerChanged";
    case LayerChange::ContentsColorLayerChanged: return "ContentsColorLayerChanged";
    case LayerChange::ContentsRectsChanged: return "ContentsRectsChanged";
    case LayerChange::MaskLayerChanged: return "MaskLayerChanged";
    case LayerChange::ReplicatedLayerChanged: return "ReplicatedLayerChanged";
    case LayerChange::ContentsNeedsDisplay: return "ContentsNeedsDisplay";
    case LayerChange::AcceleratesDrawingChanged: return "AcceleratesDrawingChanged";
    case LayerChange::SupportsSubpixelAntialiasedTextChanged: return "SupportsSubpixelAntialiasedTextChanged";
    case LayerChange::ContentsScaleChanged: return "ContentsScaleChanged";
    case LayerChange::ContentsVisibilityChanged: return "ContentsVisibilityChanged";
    case LayerChange::CoverageRectChanged: return "CoverageRectChanged";
    case LayerChange::FiltersChanged: return "FiltersChanged";
    case LayerChange::BackdropFiltersChanged: return "BackdropFiltersChanged";
    case LayerChange::BackdropFiltersRectChanged: return "BackdropFiltersRectChanged";
    case LayerChange::TilingAreaChanged: return "TilingAreaChanged";
    case LayerChange::DebugIndicatorsChanged: return "DebugIndicatorsChanged";
    case LayerChange::CustomAppearanceChanged: return "CustomAppearanceChanged";
    case LayerChange::BlendModeChanged: return "BlendModeChanged";
    case LayerChange::ShapeChanged: return "ShapeChanged";
    case LayerChange::WindRuleChanged: return "WindRuleChanged";
    case LayerChange::UserInteractionEnabledChanged: return "UserInteractionEnabledChanged";
    case LayerChange::NeedsComputeVisibleAndCoverageRect: return "NeedsComputeVisibleAndCoverageRect";
    case LayerChange::EventRegionChanged: return "EventRegionChanged";
#if ENABLE(SCROLLING_THREAD)
    case LayerChange::ScrollingNodeChanged: return "ScrollingNodeChanged";
#endif
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    case LayerChange::SeparatedChanged: return "SeparatedChanged";
#if HAVE(CORE_ANIMATION_SEPARATED_PORTALS)
    case LayerChange::SeparatedPortalChanged: return "SeparatedPortalChanged";
    case LayerChange::DescendentOfSeparatedPortalChanged: return "DescendentOfSeparatedPortalChanged";
#endif
#endif
    case LayerChange::ContentsScalingFiltersChanged: return "ContentsScalingFiltersChanged";
    }
    ASSERT_NOT_REACHED();
    return "";
}

void GraphicsLayerCA::dumpLayerChangeFlags(TextStream& textStream, LayerChangeFlags layerChangeFlags)
{
    textStream << '{';
    uint64_t bit = 1;
    bool first = true;
    while (layerChangeFlags) {
        if (layerChangeFlags & bit) {
            textStream << (first ? " " : ", ") << layerChangeAsString(static_cast<LayerChange>(bit));
            first = false;
        }
        layerChangeFlags &= ~bit;
        bit <<= 1;
    }
    textStream << " }";
}

void GraphicsLayerCA::dumpAdditionalProperties(TextStream& textStream, OptionSet<LayerTreeAsTextOptions> options) const
{
    if (options & LayerTreeAsTextOptions::IncludeVisibleRects) {
        textStream << indent << "(visible rect " << m_visibleRect.x() << ", " << m_visibleRect.y() << " " << m_visibleRect.width() << " x " << m_visibleRect.height() << ")\n";
        textStream << indent << "(coverage rect " << m_coverageRect.x() << ", " << m_coverageRect.y() << " " << m_coverageRect.width() << " x " << m_coverageRect.height() << ")\n";
        textStream << indent << "(intersects coverage rect " << m_intersectsCoverageRect << ")\n";
        textStream << indent << "(contentsScale " << m_layer->contentsScale() << ")\n";
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

    if ((options & LayerTreeAsTextOptions::IncludeDeepColor) && m_layer->wantsDeepColorBackingStore())
        textStream << indent << "(deep color 1)\n";

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

        dumpAnimations(textStream, "animations", m_animations);
        dumpAnimations(textStream, "base value animations", m_baseValueTransformAnimations);
        dumpAnimations(textStream, "animation groups", m_animationGroups);
    }
}

String GraphicsLayerCA::platformLayerTreeAsText(OptionSet<PlatformLayerTreeAsTextFlags> flags) const
{
    TextStream ts(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect);
    dumpInnerLayer(ts, primaryLayer(), flags);
    return ts.release();
}

void GraphicsLayerCA::setDebugBorder(const Color& color, float borderWidth)
{
    setLayerDebugBorder(*m_layer, color, borderWidth);
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

void GraphicsLayerCA::changeLayerTypeTo(PlatformCALayer::LayerType newLayerType)
{
    PlatformCALayer::LayerType oldLayerType = m_layer->layerType();
    if (newLayerType == oldLayerType)
        return;

    bool wasTiledLayer = oldLayerType == PlatformCALayer::LayerTypeTiledBackingLayer;
    bool isTiledLayer = newLayerType == PlatformCALayer::LayerTypeTiledBackingLayer;

    RefPtr<PlatformCALayer> oldLayer = m_layer;
    m_layer = createPlatformCALayer(newLayerType, this);

    m_layer->adoptSublayers(*oldLayer);

#ifdef VISIBLE_TILE_WASH
    if (m_visibleTileWashLayer)
        m_layer->appendSublayer(*m_visibleTileWashLayer;
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
        oldLayer->superlayer()->replaceSublayer(*oldLayer, *m_layer);
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
        | SupportsSubpixelAntialiasedTextChanged
        | FiltersChanged
        | BackdropFiltersChanged
        | MaskLayerChanged
        | OpacityChanged
        | EventRegionChanged
        | NameChanged
        | DebugIndicatorsChanged);
    
    if (isTiledLayer)
        addUncommittedChanges(CoverageRectChanged);

    adjustContentsScaleLimitingFactor();

    moveAnimations(oldLayer.get(), m_layer.get());
    
    // need to tell new layer to draw itself
    setNeedsDisplay();

    if (wasTiledLayer || isTiledLayer)
        client().tiledBackingUsageChanged(this, isTiledLayer);
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

RefPtr<PlatformCALayer> GraphicsLayerCA::findOrMakeClone(CloneID cloneID, PlatformCALayer *sourceLayer, LayerMap& clones, CloneLevel cloneLevel)
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
        resultLayer->setName(makeString("clone ", cloneID[0U], " of ", sourceLayer->layerID()));
#else
        resultLayer->setName("clone of " + m_name);
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

    if (m_maskLayer) {
        RefPtr<PlatformCALayer> maskClone = downcast<GraphicsLayerCA>(*m_maskLayer).fetchCloneLayers(replicaRoot, replicaState, IntermediateCloneLevel);
        primaryLayer->setMask(maskClone.get());
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
    
    if (m_replicaLayer && m_replicaLayer != replicaRoot) {
        // We have nested replicas. Ask the replica layer for a clone of its contents.
        replicaState.setBranchType(ReplicaState::ReplicaBranch);
        replicaLayer = downcast<GraphicsLayerCA>(*m_replicaLayer).fetchCloneLayers(replicaRoot, replicaState, RootCloneLevel);
        replicaState.setBranchType(ReplicaState::ChildBranch);
    }

    if (contentsClippingLayer && contentsLayer) {
        contentsClippingLayer->appendSublayer(*contentsLayer);
    }

    if (contentsShapeMaskLayer)
        contentsClippingLayer->setMask(contentsShapeMaskLayer.get());

    if (shapeMaskLayer)
        primaryLayer->setMask(shapeMaskLayer.get());

    if (replicaLayer || structuralLayer || contentsLayer || contentsClippingLayer || childLayers.size() > 0) {
        if (structuralLayer) {
            if (backdropLayer) {
                clonalSublayers.append(backdropLayer);
                backdropLayer->setMask(backdropClippingLayer.get());
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
            GraphicsLayerCA& childLayerCA = downcast<GraphicsLayerCA>(childLayer.get());
            if (auto platformLayer = childLayerCA.fetchCloneLayers(replicaRoot, replicaState, IntermediateCloneLevel))
                clonalSublayers.append(WTFMove(platformLayer));
        }

        replicaState.pop();

        for (size_t i = 0; i < clonalSublayers.size(); ++i)
            clonalSublayers[i]->removeFromSuperlayer();
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
    primaryLayer()->setOpacity(m_opacity);

    if (LayerMap* layerCloneMap = primaryLayerClones()) {
        for (auto& clone : *layerCloneMap) {
            if (m_replicaLayer && isReplicatedRootClone(clone.key))
                continue;

            clone.value->setOpacity(m_opacity);
        }
    }

#if ENABLE(MODEL_ELEMENT)
    if (m_contentsLayer && m_contentsLayerPurpose == ContentsLayerPurpose::Model)
        m_contentsLayer->setOpacity(m_opacity);
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

    return m_layer->backingStoreBytesPerPixel() * size().width() * m_layer->contentsScale() * size().height() * m_layer->contentsScale();
}

static String animatedPropertyIDAsString(AnimatedPropertyID property)
{
    switch (property) {
    case AnimatedPropertyTranslate:
    case AnimatedPropertyScale:
    case AnimatedPropertyRotate:
    case AnimatedPropertyTransform:
        return "transform"_s;
    case AnimatedPropertyOpacity:
        return "opacity"_s;
    case AnimatedPropertyBackgroundColor:
        return "background-color"_s;
    case AnimatedPropertyFilter:
        return "filter"_s;
#if ENABLE(FILTERS_LEVEL_2)
    case AnimatedPropertyWebkitBackdropFilter:
        return "backdrop-filter"_s;
#endif
    case AnimatedPropertyInvalid:
        return "invalid"_s;
    }
    ASSERT_NOT_REACHED();
    return ""_s;
}

Vector<std::pair<String, double>> GraphicsLayerCA::acceleratedAnimationsForTesting() const
{
    Vector<std::pair<String, double>> animations;

    for (auto& animation : m_animations) {
        if (animation.m_pendingRemoval)
            continue;
        if (auto caAnimation = animatedLayer(animation.m_property)->animationForKey(animation.animationIdentifier()))
            animations.append({ animatedPropertyIDAsString(animation.m_property), caAnimation->speed() });
        else
            animations.append({ animatedPropertyIDAsString(animation.m_property), (animation.m_playState == PlayState::Playing || animation.m_playState == PlayState::PlayPending) ? 1 : 0 });
    }

    return animations;
}

} // namespace WebCore

#endif
