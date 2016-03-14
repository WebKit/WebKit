/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "Animation.h"
#include "DisplayListRecorder.h"
#include "DisplayListReplayer.h"
#include "FloatConversion.h"
#include "FloatRect.h"
#include "GraphicsLayerFactory.h"
#include "Image.h"
#include "Logging.h"
#include "PlatformCAFilters.h"
#include "PlatformCALayer.h"
#include "RotateTransformOperation.h"
#include "ScaleTransformOperation.h"
#include "TextStream.h"
#include "TiledBacking.h"
#include "TransformState.h"
#include "TranslateTransformOperation.h"
#include <QuartzCore/CATransform3D.h>
#include <limits.h>
#include <wtf/CurrentTime.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SystemTracing.h>
#include <wtf/TemporaryChange.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(IOS)
#include "SystemMemory.h"
#include "WebCoreThread.h"
#endif

#if PLATFORM(COCOA)
#include "PlatformCAAnimationCocoa.h"
#include "PlatformCALayerCocoa.h"
#include "WebCoreSystemInterface.h"
#endif

#if PLATFORM(WIN)
#include "PlatformCAAnimationWin.h"
#include "PlatformCALayerWin.h"
#endif

namespace WebCore {

// The threshold width or height above which a tiled layer will be used. This should be
// large enough to avoid tiled layers for most GraphicsLayers, but less than the OpenGL
// texture size limit on all supported hardware.
#if PLATFORM(IOS)
static const int cMaxPixelDimension = 1280;
static const int cMaxPixelDimensionLowMemory = 1024;
static const int cMemoryLevelToUseSmallerPixelDimension = 35;
#else
static const int cMaxPixelDimension = 2048;
#endif

// Derived empirically: <rdar://problem/13401861>
static const int cMaxLayerTreeDepth = 250;

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
        value = transformOp ? narrowPrecisionToFloat(downcast<TranslateTransformOperation>(*transformOp).x(size)) : 0;
        break;
    case TransformOperation::TRANSLATE_Y:
        value = transformOp ? narrowPrecisionToFloat(downcast<TranslateTransformOperation>(*transformOp).y(size)) : 0;
        break;
    case TransformOperation::TRANSLATE_Z:
        value = transformOp ? narrowPrecisionToFloat(downcast<TranslateTransformOperation>(*transformOp).z(size)) : 0;
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
        value.setX(translateTransformOp ? narrowPrecisionToFloat(translateTransformOp->x(size)) : 0);
        value.setY(translateTransformOp ? narrowPrecisionToFloat(translateTransformOp->y(size)) : 0);
        value.setZ(translateTransformOp ? narrowPrecisionToFloat(translateTransformOp->z(size)) : 0);
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
    case AnimatedPropertyTransform:
        return ASCIILiteral("transform");
    case AnimatedPropertyOpacity:
        return ASCIILiteral("opacity");
    case AnimatedPropertyBackgroundColor:
        return ASCIILiteral("backgroundColor");
    case AnimatedPropertyFilter:
        return ASCIILiteral("filters");
#if ENABLE(FILTERS_LEVEL_2)
    case AnimatedPropertyWebkitBackdropFilter:
        return ASCIILiteral("backdropFilters");
#endif
    case AnimatedPropertyInvalid:
        ASSERT_NOT_REACHED();
    }
    ASSERT_NOT_REACHED();
    return ASCIILiteral("");
}

static String animationIdentifier(const String& animationName, AnimatedPropertyID property, int index, int subIndex)
{
    return animationName + '_' + String::number(property) + '_' + String::number(index) + '_' + String::number(subIndex);
}

static bool animationHasStepsTimingFunction(const KeyframeValueList& valueList, const Animation* anim)
{
    if (anim->timingFunction()->isStepsTimingFunction())
        return true;
    
    for (unsigned i = 0; i < valueList.size(); ++i) {
        if (const TimingFunction* timingFunction = valueList.at(i).timingFunction()) {
            if (timingFunction->isStepsTimingFunction())
                return true;
        }
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

bool GraphicsLayer::supportsLayerType(Type type)
{
    switch (type) {
    case Type::Normal:
    case Type::PageTiledBacking:
    case Type::Scrolling:
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

bool GraphicsLayer::supportsBackgroundColorContent()
{
    return true;
}

static bool isSmoothedLayerTextEnabled = true;

bool GraphicsLayer::supportsSmoothedLayerText()
{
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200
    // FIXME: This is currently a major regression on PLT (rdar://problem/25113391).
    // return isSmoothedLayerTextEnabled;
    return false;
#endif
    return false;
}

void GraphicsLayer::setSmoothedLayerTextEnabled(bool flag)
{
    isSmoothedLayerTextEnabled = flag;
}

bool GraphicsLayer::smoothedLayerTextEnabled()
{
    return isSmoothedLayerTextEnabled;
}

std::unique_ptr<GraphicsLayer> GraphicsLayer::create(GraphicsLayerFactory* factory, GraphicsLayerClient& client, Type layerType)
{
    std::unique_ptr<GraphicsLayer> graphicsLayer;
    if (!factory)
        graphicsLayer = std::make_unique<GraphicsLayerCA>(layerType, client);
    else
        graphicsLayer = factory->createGraphicsLayer(layerType, client);

    graphicsLayer->initialize(layerType);

    return graphicsLayer;
}

bool GraphicsLayerCA::filtersCanBeComposited(const FilterOperations& filters)
{
#if PLATFORM(COCOA)
    return PlatformCALayerCocoa::filtersCanBeComposited(filters);
#elif PLATFORM(WIN)
    return PlatformCALayerWin::filtersCanBeComposited(filters);
#endif
}

PassRefPtr<PlatformCALayer> GraphicsLayerCA::createPlatformCALayer(PlatformCALayer::LayerType layerType, PlatformCALayerClient* owner)
{
#if PLATFORM(COCOA)
    return PlatformCALayerCocoa::create(layerType, owner);
#elif PLATFORM(WIN)
    return PlatformCALayerWin::create(layerType, owner);
#endif
}
    
PassRefPtr<PlatformCALayer> GraphicsLayerCA::createPlatformCALayer(PlatformLayer* platformLayer, PlatformCALayerClient* owner)
{
#if PLATFORM(COCOA)
    return PlatformCALayerCocoa::create(platformLayer, owner);
#elif PLATFORM(WIN)
    return PlatformCALayerWin::create(platformLayer, owner);
#endif
}

PassRefPtr<PlatformCAAnimation> GraphicsLayerCA::createPlatformCAAnimation(PlatformCAAnimation::AnimationType type, const String& keyPath)
{
#if PLATFORM(COCOA)
    return PlatformCAAnimationCocoa::create(type, keyPath);
#elif PLATFORM(WIN)
    return PlatformCAAnimationWin::create(type, keyPath);
#endif
}

typedef HashMap<const GraphicsLayerCA*, std::pair<FloatRect, std::unique_ptr<DisplayList::DisplayList>>> LayerDisplayListHashMap;

static LayerDisplayListHashMap& layerDisplayListMap()
{
    static NeverDestroyed<LayerDisplayListHashMap> sharedHashMap;
    return sharedHashMap;
}

GraphicsLayerCA::GraphicsLayerCA(Type layerType, GraphicsLayerClient& client)
    : GraphicsLayer(layerType, client)
    , m_needsFullRepaint(false)
    , m_usingBackdropLayerType(false)
    , m_isViewportConstrained(false)
    , m_intersectsCoverageRect(false)
    , m_hasEverPainted(false)
{
}

void GraphicsLayerCA::initialize(Type layerType)
{
    PlatformCALayer::LayerType platformLayerType;
    switch (layerType) {
    case Type::Normal:
        platformLayerType = PlatformCALayer::LayerType::LayerTypeWebLayer;
        break;
    case Type::PageTiledBacking:
        platformLayerType = PlatformCALayer::LayerType::LayerTypePageTiledBackingLayer;
        break;
    case Type::Scrolling:
        platformLayerType = PlatformCALayer::LayerType::LayerTypeScrollingLayer;
        break;
    case Type::Shape:
        platformLayerType = PlatformCALayer::LayerType::LayerTypeShapeLayer;
        break;
    }
    m_layer = createPlatformCALayer(platformLayerType, this);
    noteLayerPropertyChanged(ContentsScaleChanged);
}

GraphicsLayerCA::~GraphicsLayerCA()
{
    if (UNLIKELY(isTrackingDisplayListReplay()))
        layerDisplayListMap().remove(this);

    // Do cleanup while we can still safely call methods on the derived class.
    willBeDestroyed();
}

void GraphicsLayerCA::willBeDestroyed()
{
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

    removeCloneLayers();

    GraphicsLayer::willBeDestroyed();
}

void GraphicsLayerCA::setName(const String& name)
{
    String caLayerDescription;

    if (!m_layer->isPlatformCALayerRemote())
        caLayerDescription = String::format("CALayer(%p) ", m_layer->platformLayer());

    String longName = caLayerDescription + String::format("GraphicsLayer(%p, %llu) ", this, primaryLayerID()) + name;
    GraphicsLayer::setName(longName);
    noteLayerPropertyChanged(NameChanged);
}

GraphicsLayer::PlatformLayerID GraphicsLayerCA::primaryLayerID() const
{
    return primaryLayer()->layerID();
}

PlatformLayer* GraphicsLayerCA::platformLayer() const
{
    return primaryLayer()->platformLayer();
}

bool GraphicsLayerCA::setChildren(const Vector<GraphicsLayer*>& children)
{
    bool childrenChanged = GraphicsLayer::setChildren(children);
    if (childrenChanged)
        noteSublayersChanged();
    
    return childrenChanged;
}

void GraphicsLayerCA::addChild(GraphicsLayer* childLayer)
{
    GraphicsLayer::addChild(childLayer);
    noteSublayersChanged();
}

void GraphicsLayerCA::addChildAtIndex(GraphicsLayer* childLayer, int index)
{
    GraphicsLayer::addChildAtIndex(childLayer, index);
    noteSublayersChanged();
}

void GraphicsLayerCA::addChildBelow(GraphicsLayer* childLayer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildBelow(childLayer, sibling);
    noteSublayersChanged();
}

void GraphicsLayerCA::addChildAbove(GraphicsLayer* childLayer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildAbove(childLayer, sibling);
    noteSublayersChanged();
}

bool GraphicsLayerCA::replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild)
{
    if (GraphicsLayer::replaceChild(oldChild, newChild)) {
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

void GraphicsLayerCA::setMaskLayer(GraphicsLayer* layer)
{
    if (layer == m_maskLayer)
        return;

    GraphicsLayer::setMaskLayer(layer);
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

void GraphicsLayerCA::setReplicatedByLayer(GraphicsLayer* layer)
{
    if (layer == m_replicaLayer)
        return;

    GraphicsLayer::setReplicatedByLayer(layer);
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

void GraphicsLayerCA::setTransform(const TransformationMatrix& t)
{
    if (t == m_transform)
        return;

    GraphicsLayer::setTransform(t);
    noteLayerPropertyChanged(TransformChanged);
}

void GraphicsLayerCA::setChildrenTransform(const TransformationMatrix& t)
{
    if (t == m_childrenTransform)
        return;

    GraphicsLayer::setChildrenTransform(t);
    noteLayerPropertyChanged(ChildrenTransformChanged);
}

void GraphicsLayerCA::moveOrCopyLayerAnimation(MoveOrCopy operation, const String& animationIdentifier, PlatformCALayer *fromLayer, PlatformCALayer *toLayer)
{
    RefPtr<PlatformCAAnimation> anim = fromLayer->animationForKey(animationIdentifier);
    if (!anim)
        return;

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
    // Look for running animations affecting this property.
    AnimationsMap::const_iterator end = m_runningAnimations.end();
    for (AnimationsMap::const_iterator it = m_runningAnimations.begin(); it != end; ++it) {
        const Vector<LayerPropertyAnimation>& propertyAnimations = it->value;
        size_t numAnimations = propertyAnimations.size();
        for (size_t i = 0; i < numAnimations; ++i) {
            const LayerPropertyAnimation& currAnimation = propertyAnimations[i];

            if (currAnimation.m_property == AnimatedPropertyTransform
                || currAnimation.m_property == AnimatedPropertyOpacity
                || currAnimation.m_property == AnimatedPropertyBackgroundColor
                || currAnimation.m_property == AnimatedPropertyFilter)
                moveOrCopyLayerAnimation(operation, animationIdentifier(currAnimation.m_name, currAnimation.m_property, currAnimation.m_index, currAnimation.m_subIndex), fromLayer, toLayer);
        }
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
    if (m_contentsLayer)
        noteSublayersChanged();
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

void GraphicsLayerCA::setSupportsSmoothedFonts(bool supportsSmoothedFonts)
{
    if (m_supportsSmoothedFonts == supportsSmoothedFonts)
        return;

    GraphicsLayer::setSupportsSmoothedFonts(supportsSmoothedFonts);
    noteLayerPropertyChanged(ContentsFormatChanged);
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
    noteLayerPropertyChanged(BackdropFiltersChanged);
    return canCompositeFilters;
}

void GraphicsLayerCA::setBackdropFiltersRect(const FloatRect& backdropFiltersRect)
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

    addRepaintRect(rect);
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

bool GraphicsLayerCA::setMasksToBoundsRect(const FloatRoundedRect& roundedRect)
{
    if (roundedRect == m_masksToBoundsRect)
        return true;

    GraphicsLayer::setMasksToBoundsRect(roundedRect);
    noteLayerPropertyChanged(MasksToBoundsRectChanged);
    return true;
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

bool GraphicsLayerCA::shouldRepaintOnSizeChange() const
{
    return drawsContent() && !tiledBacking();
}

bool GraphicsLayerCA::animationCanBeAccelerated(const KeyframeValueList& valueList, const Animation* anim) const
{
    if (!anim || anim->isEmptyOrZeroDuration() || valueList.size() < 2)
        return false;

    if (animationHasStepsTimingFunction(valueList, anim))
        return false;

#if ENABLE(CSS_ANIMATIONS_LEVEL_2)
    // If there is a trigger that depends on the scroll position, we cannot accelerate the animation.
    if (anim->trigger()->isScrollAnimationTrigger()) {
        ScrollAnimationTrigger& scrollTrigger = downcast<ScrollAnimationTrigger>(*anim->trigger().get());
        if (scrollTrigger.hasEndValue())
            return false;
    }
#endif

    return true;
}

bool GraphicsLayerCA::addAnimation(const KeyframeValueList& valueList, const FloatSize& boxSize, const Animation* anim, const String& animationName, double timeOffset)
{
    ASSERT(!animationName.isEmpty());

    if (!animationCanBeAccelerated(valueList, anim))
        return false;

    bool createdAnimations = false;
    if (valueList.property() == AnimatedPropertyTransform)
        createdAnimations = createTransformAnimationsFromKeyframes(valueList, anim, animationName, timeOffset, boxSize);
    else if (valueList.property() == AnimatedPropertyFilter) {
        if (supportsAcceleratedFilterAnimations())
            createdAnimations = createFilterAnimationsFromKeyframes(valueList, anim, animationName, timeOffset);
    }
#if ENABLE(FILTERS_LEVEL_2)
    else if (valueList.property() == AnimatedPropertyWebkitBackdropFilter) {
        if (supportsAcceleratedFilterAnimations())
            createdAnimations = createFilterAnimationsFromKeyframes(valueList, anim, animationName, timeOffset);
    }
#endif
    else
        createdAnimations = createAnimationFromKeyframes(valueList, anim, animationName, timeOffset);

    if (createdAnimations)
        noteLayerPropertyChanged(AnimationChanged);

    return createdAnimations;
}

void GraphicsLayerCA::pauseAnimation(const String& animationName, double timeOffset)
{
    if (!animationIsRunning(animationName))
        return;

    AnimationsToProcessMap::iterator it = m_animationsToProcess.find(animationName);
    if (it != m_animationsToProcess.end()) {
        AnimationProcessingAction& processingInfo = it->value;
        // If an animation is scheduled to be removed, don't change the remove to a pause.
        if (processingInfo.action != Remove)
            processingInfo.action = Pause;
    } else
        m_animationsToProcess.add(animationName, AnimationProcessingAction(Pause, timeOffset));

    noteLayerPropertyChanged(AnimationChanged);
}

void GraphicsLayerCA::removeAnimation(const String& animationName)
{
    if (!animationIsRunning(animationName))
        return;

    m_animationsToProcess.add(animationName, AnimationProcessingAction(Remove));
    noteLayerPropertyChanged(AnimationChanged);
}

void GraphicsLayerCA::platformCALayerAnimationStarted(const String& animationKey, CFTimeInterval startTime)
{
    client().notifyAnimationStarted(this, animationKey, startTime);
}

void GraphicsLayerCA::platformCALayerAnimationEnded(const String& animationKey)
{
    client().notifyAnimationEnded(this, animationKey);
}

void GraphicsLayerCA::setContentsToSolidColor(const Color& color)
{
    if (color == m_contentsSolidColor)
        return;

    m_contentsSolidColor = color;
    
    bool contentsLayerChanged = false;

    if (m_contentsSolidColor.isValid() && m_contentsSolidColor.alpha()) {
        if (!m_contentsLayer || m_contentsLayerPurpose != ContentsLayerForBackgroundColor) {
            m_contentsLayerPurpose = ContentsLayerForBackgroundColor;
            m_contentsLayer = createPlatformCALayer(PlatformCALayer::LayerTypeLayer, this);
#ifndef NDEBUG
            m_contentsLayer->setName(String::format("Background Color Layer %llu", m_contentsLayer->layerID()));
#endif
            contentsLayerChanged = true;
        }
    } else {
        contentsLayerChanged = m_contentsLayer;
        m_contentsLayerPurpose = NoContentsLayer;
        m_contentsLayer = nullptr;
    }

    if (contentsLayerChanged)
        noteSublayersChanged();

    noteLayerPropertyChanged(ContentsColorLayerChanged);
}

void GraphicsLayerCA::setContentsToImage(Image* image)
{
    if (image) {
        CGImageRef newImage = image->nativeImageForCurrentFrame();
        if (!newImage)
            return;

        // FIXME: probably don't need m_uncorrectedContentsImage at all now.
        if (m_uncorrectedContentsImage && m_uncorrectedContentsImage.get() == newImage)
            return;
        
        m_uncorrectedContentsImage = newImage;
        m_pendingContentsImage = newImage;

        m_contentsLayerPurpose = ContentsLayerForImage;
        if (!m_contentsLayer)
            noteSublayersChanged();
    } else {
        m_uncorrectedContentsImage = nullptr;
        m_pendingContentsImage = nullptr;
        m_contentsLayerPurpose = NoContentsLayer;
        if (m_contentsLayer)
            noteSublayersChanged();
    }

    noteLayerPropertyChanged(ContentsImageChanged);
}

void GraphicsLayerCA::setContentsToPlatformLayer(PlatformLayer* platformLayer, ContentsLayerPurpose purpose)
{
    if (m_contentsLayer && platformLayer == m_contentsLayer->platformLayer())
        return;

    if (m_contentsClippingLayer && m_contentsLayer)
        m_contentsLayer->removeFromSuperlayer();

    // FIXME: The passed in layer might be a raw layer or an externally created
    // PlatformCALayer. To determine this we attempt to get the
    // PlatformCALayer pointer. If this returns a null pointer we assume it's
    // raw. This test might be invalid if the raw layer is, for instance, the
    // PlatformCALayer is using a user data pointer in the raw layer, and
    // the creator of the raw layer is using it for some other purpose.
    // For now we don't support such a case.
    PlatformCALayer* platformCALayer = PlatformCALayer::platformCALayer(platformLayer);
    m_contentsLayer = platformLayer ? (platformCALayer ? platformCALayer : createPlatformCALayer(platformLayer, this)) : nullptr;
    m_contentsLayerPurpose = platformLayer ? purpose : NoContentsLayer;

    if (m_contentsClippingLayer && m_contentsLayer)
        m_contentsClippingLayer->appendSublayer(*m_contentsLayer);

    noteSublayersChanged();
    noteLayerPropertyChanged(ContentsPlatformLayerChanged);
}

#if PLATFORM(IOS)
PlatformLayer* GraphicsLayerCA::contentsLayerForMedia() const
{
    return m_contentsLayerPurpose == ContentsLayerForMedia ? m_contentsLayer->platformLayer() : nullptr;
}
#endif

void GraphicsLayerCA::layerDidDisplay(PlatformCALayer* layer)
{
    LayerMap* layerCloneMap;

    if (layer == m_layer)
        layerCloneMap = m_layerClones.get();
    else if (layer == m_contentsLayer)
        layerCloneMap = m_contentsLayerClones.get();
    else
        return;

    if (layerCloneMap) {
        LayerMap::const_iterator end = layerCloneMap->end();
        for (LayerMap::const_iterator it = layerCloneMap->begin(); it != end; ++it) {
            PlatformCALayer* currClone = it->value.get();
            if (!currClone)
                continue;

            currClone->copyContentsFromLayer(layer);
        }
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

void GraphicsLayerCA::flushCompositingState(const FloatRect& clipRect, bool viewportIsStable)
{
    TransformState state(TransformState::UnapplyInverseTransformDirection, FloatQuad(clipRect));
    FloatQuad coverageQuad(clipRect);
    state.setSecondaryQuad(&coverageQuad);
    recursiveCommitChanges(CommitState(viewportIsStable), state);
}

void GraphicsLayerCA::flushCompositingStateForThisLayerOnly(bool viewportIsStable)
{
    float pageScaleFactor;
    bool hadChanges = m_uncommittedChanges;
    
    CommitState commitState(viewportIsStable);

    FloatPoint offset = computePositionRelativeToBase(pageScaleFactor);
    commitLayerChangesBeforeSublayers(commitState, pageScaleFactor, offset);
    commitLayerChangesAfterSublayers(commitState);

    if (hadChanges)
        client().didCommitChangesForLayer(this);
}

static inline bool accumulatesTransform(const GraphicsLayerCA& layer)
{
    return layer.preserves3D() || (layer.parent() && layer.parent()->preserves3D());
}

bool GraphicsLayerCA::recursiveVisibleRectChangeRequiresFlush(const TransformState& state) const
{
    TransformState localState = state;
    
    // This may be called at times when layout has not been updated, so we want to avoid calling out to the client
    // for animating transforms.
    VisibleAndCoverageRects rects = computeVisibleAndCoverageRect(localState, accumulatesTransform(*this), 0);
    adjustCoverageRect(rects, m_visibleRect);

    if (rects.coverageRect != m_coverageRect) {
        if (TiledBacking* tiledBacking = this->tiledBacking()) {
            if (tiledBacking->tilesWouldChangeForCoverageRect(rects.coverageRect))
                return true;
        }
    }

    if (m_maskLayer) {
        GraphicsLayerCA& maskLayerCA = downcast<GraphicsLayerCA>(*m_maskLayer);
        if (maskLayerCA.recursiveVisibleRectChangeRequiresFlush(localState))
            return true;
    }

    const Vector<GraphicsLayer*>& childLayers = children();
    size_t numChildren = childLayers.size();
    
    for (size_t i = 0; i < numChildren; ++i) {
        GraphicsLayerCA& currentChild = downcast<GraphicsLayerCA>(*childLayers[i]);
        if (currentChild.recursiveVisibleRectChangeRequiresFlush(localState))
            return true;
    }

    if (m_replicaLayer)
        if (downcast<GraphicsLayerCA>(*m_replicaLayer).recursiveVisibleRectChangeRequiresFlush(localState))
            return true;
    
    return false;
}

bool GraphicsLayerCA::visibleRectChangeRequiresFlush(const FloatRect& clipRect) const
{
    TransformState state(TransformState::UnapplyInverseTransformDirection, FloatQuad(clipRect));
    return recursiveVisibleRectChangeRequiresFlush(state);
}

TiledBacking* GraphicsLayerCA::tiledBacking() const
{
    return m_layer->tiledBacking();
}

TransformationMatrix GraphicsLayerCA::layerTransform(const FloatPoint& position, const TransformationMatrix* customTransform) const
{
    TransformationMatrix transform;
    transform.translate(position.x(), position.y());

    TransformationMatrix currentTransform = customTransform ? *customTransform : m_transform;
    
    if (!currentTransform.isIdentity()) {
        FloatPoint3D absoluteAnchorPoint(anchorPoint());
        absoluteAnchorPoint.scale(size().width(), size().height(), 1);
        transform.translate3d(absoluteAnchorPoint.x(), absoluteAnchorPoint.y(), absoluteAnchorPoint.z());
        transform.multiply(currentTransform);
        transform.translate3d(-absoluteAnchorPoint.x(), -absoluteAnchorPoint.y(), -absoluteAnchorPoint.z());
    }

    if (GraphicsLayer* parentLayer = parent()) {
        if (!parentLayer->childrenTransform().isIdentity()) {
            FloatPoint3D parentAnchorPoint(parentLayer->anchorPoint());
            parentAnchorPoint.scale(parentLayer->size().width(), parentLayer->size().height(), 1);

            transform.translateRight3d(-parentAnchorPoint.x(), -parentAnchorPoint.y(), -parentAnchorPoint.z());
            transform = parentLayer->childrenTransform() * transform;
            transform.translateRight3d(parentAnchorPoint.x(), parentAnchorPoint.y(), parentAnchorPoint.z());
        }
    }
    
    return transform;
}

GraphicsLayerCA::VisibleAndCoverageRects GraphicsLayerCA::computeVisibleAndCoverageRect(TransformState& state, bool preserves3D, ComputeVisibleRectFlags flags) const
{
    FloatPoint position = m_position;
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
    FloatRect clipRectForChildren = state.mappedQuad(&mapWasClamped).boundingBox();
    FloatPoint boundsOrigin = m_boundsOrigin;
#if PLATFORM(IOS)
    // In WK1, UIKit may be changing layer bounds behind our back in overflow-scroll layers, so use the layer's origin.
    if (m_layer->isPlatformCALayerCocoa())
        boundsOrigin = m_layer->bounds().location();
#endif
    clipRectForChildren.move(boundsOrigin.x(), boundsOrigin.y());
    
    FloatRect clipRectForSelf(boundsOrigin, m_size);
    if (!applyWasClamped && !mapWasClamped)
        clipRectForSelf.intersect(clipRectForChildren);
    
    if (masksToBounds()) {
        ASSERT(accumulation == TransformState::FlattenTransform);
        // Flatten, and replace the quad in the TransformState with one that is clipped to this layer's bounds.
        state.flatten();
        state.setQuad(clipRectForSelf);
        if (state.isMappingSecondaryQuad()) {
            FloatQuad secondaryQuad(clipRectForSelf);
            state.setSecondaryQuad(&secondaryQuad);
        }
    }

    FloatRect coverageRect = clipRectForSelf;
    Optional<FloatQuad> quad = state.mappedSecondaryQuad(&mapWasClamped);
    if (quad && !mapWasClamped && !applyWasClamped)
        coverageRect = (*quad).boundingBox();

    return VisibleAndCoverageRects(clipRectForSelf, coverageRect);
}

bool GraphicsLayerCA::adjustCoverageRect(VisibleAndCoverageRects& rects, const FloatRect& oldVisibleRect) const
{
    FloatRect coverageRect = rects.coverageRect;

    // FIXME: TileController's computeTileCoverageRect() code should move here, and we should unify these different
    // ways of computing coverage.
    switch (type()) {
    case Type::PageTiledBacking:
        tiledBacking()->adjustTileCoverageRect(coverageRect, size(), oldVisibleRect, rects.visibleRect, pageScaleFactor() * deviceScaleFactor());
        break;
    case Type::Normal:
        if (m_layer->layerType() == PlatformCALayer::LayerTypeTiledBackingLayer)
            coverageRect.unite(adjustTiledLayerVisibleRect(tiledBacking(), oldVisibleRect, rects.visibleRect, m_sizeAtLastCoverageRectUpdate, m_size));
        break;
    default:
        break;
    }
    
    if (rects.coverageRect == coverageRect)
        return false;

    rects.coverageRect = coverageRect;
    return true;
}

void GraphicsLayerCA::setVisibleAndCoverageRects(const VisibleAndCoverageRects& rects, bool isViewportConstrained, bool viewportIsStable)
{
    bool visibleRectChanged = rects.visibleRect != m_visibleRect;
    bool coverageRectChanged = rects.coverageRect != m_coverageRect;
    if (!visibleRectChanged && !coverageRectChanged)
        return;

    if (isViewportConstrained && !viewportIsStable)
        return;

    // FIXME: we need to take reflections into account when determining whether this layer intersects the coverage rect.
    bool intersectsCoverageRect = isViewportConstrained || rects.coverageRect.intersects(FloatRect(m_boundsOrigin, size()));
    if (intersectsCoverageRect != m_intersectsCoverageRect) {
        m_uncommittedChanges |= CoverageRectChanged;
        m_intersectsCoverageRect = intersectsCoverageRect;

        if (GraphicsLayerCA* maskLayer = downcast<GraphicsLayerCA>(m_maskLayer)) {
            maskLayer->m_uncommittedChanges |= CoverageRectChanged;
            maskLayer->m_intersectsCoverageRect = intersectsCoverageRect;
        }
    }

    if (visibleRectChanged) {
        m_uncommittedChanges |= CoverageRectChanged;
        m_visibleRect = rects.visibleRect;

        if (GraphicsLayerCA* maskLayer = downcast<GraphicsLayerCA>(m_maskLayer)) {
            // FIXME: this assumes that the mask layer has the same geometry as this layer (which is currently always true).
            maskLayer->m_uncommittedChanges |= CoverageRectChanged;
            maskLayer->m_visibleRect = rects.visibleRect;
        }
    }

    if (coverageRectChanged) {
        m_uncommittedChanges |= CoverageRectChanged;
        m_coverageRect = rects.coverageRect;

        if (GraphicsLayerCA* maskLayer = downcast<GraphicsLayerCA>(m_maskLayer)) {
            maskLayer->m_uncommittedChanges |= CoverageRectChanged;
            maskLayer->m_coverageRect = rects.coverageRect;
        }
    }
}

// rootRelativeTransformForScaling is a transform from the root, but for layers with transform animations, it cherry-picked the state of the
// animation that contributes maximally to the scale (on every layer with animations down the hierarchy).
void GraphicsLayerCA::recursiveCommitChanges(const CommitState& commitState, const TransformState& state, float pageScaleFactor, const FloatPoint& positionRelativeToBase, bool affectedByPageScale)
{
    TransformState localState = state;
    CommitState childCommitState = commitState;
    bool affectedByTransformAnimation = commitState.ancestorHasTransformAnimation;

    bool accumulateTransform = accumulatesTransform(*this);
    VisibleAndCoverageRects rects = computeVisibleAndCoverageRect(localState, accumulateTransform);
    if (adjustCoverageRect(rects, m_visibleRect)) {
        if (state.isMappingSecondaryQuad()) {
            FloatQuad secondaryQuad(rects.coverageRect);
            localState.setLastPlanarSecondaryQuad(&secondaryQuad);
        }
    }
    setVisibleAndCoverageRects(rects, m_isViewportConstrained || commitState.ancestorIsViewportConstrained, commitState.viewportIsStable);

#ifdef VISIBLE_TILE_WASH
    // Use having a transform as a key to making the tile wash layer. If every layer gets a wash,
    // they start to obscure useful information.
    if ((!m_transform.isIdentity() || m_usingTiledBacking) && !m_visibleTileWashLayer) {
        static Color washFillColor(255, 0, 0, 50);
        static Color washBorderColor(255, 0, 0, 100);
        
        m_visibleTileWashLayer = createPlatformCALayer(PlatformCALayer::LayerTypeLayer, this);
        String name = String::format("Visible Tile Wash Layer %p", m_visibleTileWashLayer->platformLayer());
        m_visibleTileWashLayer->setName(name);
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
    
    if (appliesPageScale()) {
        pageScaleFactor = this->pageScaleFactor();
        affectedByPageScale = true;
    }

    // Accumulate an offset from the ancestral pixel-aligned layer.
    FloatPoint baseRelativePosition = positionRelativeToBase;
    if (affectedByPageScale)
        baseRelativePosition += m_position;
    
    commitLayerChangesBeforeSublayers(childCommitState, pageScaleFactor, baseRelativePosition);

    if (isRunningTransformAnimation()) {
        childCommitState.ancestorHasTransformAnimation = true;
        affectedByTransformAnimation = true;
    }
    
    childCommitState.ancestorIsViewportConstrained |= m_isViewportConstrained;

    if (GraphicsLayerCA* maskLayer = downcast<GraphicsLayerCA>(m_maskLayer))
        maskLayer->commitLayerChangesBeforeSublayers(childCommitState, pageScaleFactor, baseRelativePosition);

    const Vector<GraphicsLayer*>& childLayers = children();
    size_t numChildren = childLayers.size();
    
    for (size_t i = 0; i < numChildren; ++i) {
        GraphicsLayerCA& currentChild = downcast<GraphicsLayerCA>(*childLayers[i]);
        currentChild.recursiveCommitChanges(childCommitState, localState, pageScaleFactor, baseRelativePosition, affectedByPageScale);
    }

    if (GraphicsLayerCA* replicaLayer = downcast<GraphicsLayerCA>(m_replicaLayer))
        replicaLayer->recursiveCommitChanges(childCommitState, localState, pageScaleFactor, baseRelativePosition, affectedByPageScale);

    if (GraphicsLayerCA* maskLayer = downcast<GraphicsLayerCA>(m_maskLayer))
        maskLayer->commitLayerChangesAfterSublayers(childCommitState);

    bool hadDirtyRects = m_uncommittedChanges & DirtyRectsChanged;
    commitLayerChangesAfterSublayers(childCommitState);

    if (affectedByTransformAnimation && m_layer->layerType() == PlatformCALayer::LayerTypeTiledBackingLayer)
        client().notifyFlushBeforeDisplayRefresh(this);

    if (hadChanges)
        client().didCommitChangesForLayer(this);

    if (usesDisplayListDrawing() && m_drawsContent && (!m_hasEverPainted || hadDirtyRects)) {
#ifdef LOG_RECORDING_TIME
        double startTime = currentTime();
#endif
        m_displayList = std::make_unique<DisplayList::DisplayList>();
        
        FloatRect initialClip(boundsOrigin(), size());

        GraphicsContext context;
        DisplayList::Recorder recorder(context, *m_displayList, initialClip, AffineTransform());
        paintGraphicsLayerContents(context, FloatRect(FloatPoint(), size()));

#ifdef LOG_RECORDING_TIME
        double duration = currentTime() - startTime;
        WTFLogAlways("Recording took %.5fms", duration * 1000.0);
#endif
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

void GraphicsLayerCA::platformCALayerPaintContents(PlatformCALayer*, GraphicsContext& context, const FloatRect& clip)
{
    m_hasEverPainted = true;
    if (m_displayList) {
        DisplayList::Replayer replayer(context, *m_displayList);
        
        if (UNLIKELY(isTrackingDisplayListReplay())) {
            auto replayList = replayer.replay(clip, isTrackingDisplayListReplay());
            layerDisplayListMap().add(this, std::pair<FloatRect, std::unique_ptr<DisplayList::DisplayList>>(clip, WTFMove(replayList)));
        } else
            replayer.replay(clip);

        return;
    }

    TraceScope tracingScope(PaintLayerStart, PaintLayerEnd);
    paintGraphicsLayerContents(context, clip);
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

static PlatformCALayer::LayerType layerTypeForCustomBackdropAppearance(GraphicsLayer::CustomAppearance appearance)
{
    return appearance == GraphicsLayer::LightBackdropAppearance ? PlatformCALayer::LayerTypeLightSystemBackdropLayer : PlatformCALayer::LayerTypeDarkSystemBackdropLayer;
}

static bool isCustomBackdropLayerType(PlatformCALayer::LayerType layerType)
{
    return layerType == PlatformCALayer::LayerTypeLightSystemBackdropLayer || layerType == PlatformCALayer::LayerTypeDarkSystemBackdropLayer;
}

void GraphicsLayerCA::commitLayerChangesBeforeSublayers(CommitState& commitState, float pageScaleFactor, const FloatPoint& positionRelativeToBase)
{
    TemporaryChange<bool> committingChangesChange(m_isCommittingChanges, true);

    ++commitState.treeDepth;
    if (m_structuralLayer)
        ++commitState.treeDepth;

    if (!m_uncommittedChanges) {
        // Ensure that we cap layer depth in commitLayerChangesAfterSublayers().
        if (commitState.treeDepth > cMaxLayerTreeDepth)
            m_uncommittedChanges |= ChildrenChanged;
    }

    bool needTiledLayer = requiresTiledLayer(pageScaleFactor);
    bool needBackdropLayerType = (customAppearance() == LightBackdropAppearance || customAppearance() == DarkBackdropAppearance);
    PlatformCALayer::LayerType neededLayerType = m_layer->layerType();

    if (needBackdropLayerType)
        neededLayerType = layerTypeForCustomBackdropAppearance(customAppearance());
    else if (needTiledLayer)
        neededLayerType = PlatformCALayer::LayerTypeTiledBackingLayer;
    else if (isCustomBackdropLayerType(m_layer->layerType()) || m_usingTiledBacking)
        neededLayerType = PlatformCALayer::LayerTypeWebLayer;

    if (neededLayerType != m_layer->layerType())
        changeLayerTypeTo(neededLayerType);

    // Need to handle Preserves3DChanged first, because it affects which layers subsequent properties are applied to
    if (m_uncommittedChanges & (Preserves3DChanged | ReplicatedLayerChanged | BackdropFiltersChanged))
        updateStructuralLayer();

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

    // Note that contentsScale can affect whether the layer can be opaque.
    if (m_uncommittedChanges & ContentsOpaqueChanged)
        updateContentsOpaque(pageScaleFactor);

    if (m_uncommittedChanges & ContentsFormatChanged)
        updateContentsFormat();

    if (m_uncommittedChanges & BackfaceVisibilityChanged)
        updateBackfaceVisibility();

    if (m_uncommittedChanges & OpacityChanged)
        updateOpacityOnLayer();

    if (m_uncommittedChanges & FiltersChanged)
        updateFilters();

    if (m_uncommittedChanges & BackdropFiltersChanged)
        updateBackdropFilters();

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

    // Updating the contents scale can cause parts of the layer to be invalidated,
    // so make sure to update the contents scale before updating the dirty rects.
    if (m_uncommittedChanges & ContentsScaleChanged)
        updateContentsScale(pageScaleFactor);

    if (m_uncommittedChanges & CoverageRectChanged)
        updateCoverage();

    if (m_uncommittedChanges & TilingAreaChanged) // Needs to happen after CoverageRectChanged, ContentsScaleChanged
        updateTiles();

    if (m_uncommittedChanges & DirtyRectsChanged)
        repaintLayerDirtyRects();
    
    if (m_uncommittedChanges & ContentsRectsChanged) // Needs to happen before ChildrenChanged
        updateContentsRects();

    if (m_uncommittedChanges & MasksToBoundsRectChanged) // Needs to happen before ChildrenChanged
        updateMasksToBoundsRect();

    if (m_uncommittedChanges & MaskLayerChanged) {
        updateMaskLayer();
        // If the mask layer becomes tiled it can set this flag again. Clear the flag so that
        // commitLayerChangesAfterSublayers doesn't update the mask again in the normal case.
        m_uncommittedChanges &= ~MaskLayerChanged;
    }

    if (m_uncommittedChanges & ContentsNeedsDisplay)
        updateContentsNeedsDisplay();
    
    if (m_uncommittedChanges & AcceleratesDrawingChanged)
        updateAcceleratesDrawing();

    if (m_uncommittedChanges & DebugIndicatorsChanged)
        updateDebugBorder();

    if (m_uncommittedChanges & CustomAppearanceChanged)
        updateCustomAppearance();

    if (m_uncommittedChanges & ChildrenChanged) {
        updateSublayerList();
        // Sublayers may set this flag again, so clear it to avoid always updating sublayers in commitLayerChangesAfterSublayers().
        m_uncommittedChanges &= ~ChildrenChanged;
    }

    // Ensure that we cap layer depth in commitLayerChangesAfterSublayers().
    if (commitState.treeDepth > cMaxLayerTreeDepth)
        m_uncommittedChanges |= ChildrenChanged;
}

void GraphicsLayerCA::commitLayerChangesAfterSublayers(CommitState& commitState)
{
    if (!m_uncommittedChanges)
        return;

    TemporaryChange<bool> committingChangesChange(m_isCommittingChanges, true);

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
    switch (structuralLayerPurpose()) {
    case StructuralLayerForPreserves3D:
        m_structuralLayer->setName("Transform layer " + name());
        break;
    case StructuralLayerForReplicaFlattening:
        m_structuralLayer->setName("Replica flattening layer " + name());
        break;
    case StructuralLayerForBackdrop:
        m_structuralLayer->setName("Backdrop hosting layer " + name());
        break;
    case NoStructuralLayer:
        break;
    }
    m_layer->setName(name());
}

void GraphicsLayerCA::updateSublayerList(bool maxLayerDepthReached)
{
    if (maxLayerDepthReached) {
        m_layer->setSublayers(PlatformCALayerList());
        return;
    }
    
    const PlatformCALayerList* customSublayers = m_layer->customSublayers();

    PlatformCALayerList structuralLayerChildren;
    PlatformCALayerList primaryLayerChildren;

    PlatformCALayerList& childListForSublayers = m_structuralLayer ? structuralLayerChildren : primaryLayerChildren;

    if (customSublayers)
        primaryLayerChildren.appendVector(*customSublayers);

    if (m_structuralLayer) {
        if (m_backdropLayer)
            structuralLayerChildren.append(m_backdropLayer);

        if (m_replicaLayer)
            structuralLayerChildren.append(downcast<GraphicsLayerCA>(*m_replicaLayer).primaryLayer());
    
        structuralLayerChildren.append(m_layer);
    }

    if (m_contentsLayer && m_contentsVisible) {
        // FIXME: add the contents layer in the correct order with negative z-order children.
        // This does not cause visible rendering issues because currently contents layers are only used
        // for replaced elements that don't have children.
        primaryLayerChildren.append(m_contentsClippingLayer ? m_contentsClippingLayer : m_contentsLayer);
    }
    
    const Vector<GraphicsLayer*>& childLayers = children();
    size_t numChildren = childLayers.size();
    for (size_t i = 0; i < numChildren; ++i) {
        GraphicsLayerCA& currentChild = downcast<GraphicsLayerCA>(*childLayers[i]);
        PlatformCALayer* childLayer = currentChild.layerForSuperlayer();
        childListForSublayers.append(childLayer);
    }

#ifdef VISIBLE_TILE_WASH
    if (m_visibleTileWashLayer)
        childListForSublayers.append(m_visibleTileWashLayer);
#endif

    if (m_structuralLayer)
        m_structuralLayer->setSublayers(structuralLayerChildren);
    
    m_layer->setSublayers(primaryLayerChildren);
}

void GraphicsLayerCA::updateGeometry(float pageScaleFactor, const FloatPoint& positionRelativeToBase)
{
    FloatPoint scaledPosition = m_position;
    FloatPoint3D scaledAnchorPoint = m_anchorPoint;
    FloatSize scaledSize = m_size;
    FloatSize pixelAlignmentOffset;

    // FIXME: figure out if we really need to pixel align the graphics layer here.
    if (m_client.needsPixelAligment() && !WTF::isIntegral(pageScaleFactor) && m_drawsContent && !m_masksToBounds)
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

        if (LayerMap* layerCloneMap = m_structuralLayerClones.get()) {
            for (auto& clone : *layerCloneMap) {
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

    if (LayerMap* layerCloneMap = m_layerClones.get()) {
        for (auto& clone : *layerCloneMap) {
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
    primaryLayer()->setTransform(m_transform);

    if (LayerMap* layerCloneMap = primaryLayerClones()) {
        for (auto& clone : *layerCloneMap) {
            PlatformCALayer* currLayer = clone.value.get();
            if (m_replicaLayer && isReplicatedRootClone(clone.key)) {
                // Maintain the special-case transform for the root of a clone subtree,
                // which we set up in replicatedLayerRoot().
                currLayer->setTransform(TransformationMatrix());
            } else
                currLayer->setTransform(m_transform);
        }
    }
}

void GraphicsLayerCA::updateChildrenTransform()
{
    primaryLayer()->setSublayerTransform(m_childrenTransform);

    if (LayerMap* layerCloneMap = primaryLayerClones()) {
        for (auto& layer : layerCloneMap->values())
            layer->setSublayerTransform(m_childrenTransform);
    }
}

void GraphicsLayerCA::updateMasksToBounds()
{
    m_layer->setMasksToBounds(m_masksToBounds);

    if (LayerMap* layerCloneMap = m_layerClones.get()) {
        for (auto& layer : layerCloneMap->values())
            layer->setMasksToBounds(m_masksToBounds);
    }
}

void GraphicsLayerCA::updateContentsVisibility()
{
    // Note that m_contentsVisible also affects whether m_contentsLayer is parented.
    if (m_contentsVisible) {
        if (m_drawsContent)
            m_layer->setNeedsDisplay();
    } else {
        m_layer->setContents(nullptr);

        if (LayerMap* layerCloneMap = m_layerClones.get()) {
            for (auto& layer : layerCloneMap->values())
                layer->setContents(nullptr);
        }
    }
}

void GraphicsLayerCA::updateContentsOpaque(float pageScaleFactor)
{
    bool contentsOpaque = m_contentsOpaque;
    if (contentsOpaque) {
        float contentsScale = pageScaleFactor * deviceScaleFactor();
        if (!WTF::isIntegral(contentsScale) && !m_client.paintsOpaquelyAtNonIntegralScales(this))
            contentsOpaque = false;
    }
    
    m_layer->setOpaque(contentsOpaque);

    if (LayerMap* layerCloneMap = m_layerClones.get()) {
        for (auto& layer : layerCloneMap->values())
            layer->setOpaque(contentsOpaque);
    }
}

void GraphicsLayerCA::updateContentsFormat()
{
    PlatformCALayer::ContentsFormatFlags formatFlags = 0;
    if (supportsSmoothedFonts())
        formatFlags |= PlatformCALayer::SmoothedFonts;

    m_layer->setContentsFormat(formatFlags);

    if (LayerMap* layerCloneMap = m_layerClones.get()) {
        for (auto& layer : layerCloneMap->values())
            layer->setContentsFormat(formatFlags);
    }
}

void GraphicsLayerCA::updateBackfaceVisibility()
{
    if (m_structuralLayer && structuralLayerPurpose() == StructuralLayerForReplicaFlattening) {
        m_structuralLayer->setDoubleSided(m_backfaceVisibility);

        if (LayerMap* layerCloneMap = m_structuralLayerClones.get()) {
            for (auto& layer : layerCloneMap->values())
                layer->setDoubleSided(m_backfaceVisibility);
        }
    }

    m_layer->setDoubleSided(m_backfaceVisibility);

    if (LayerMap* layerCloneMap = m_layerClones.get()) {
        for (auto& layer : layerCloneMap->values())
            layer->setDoubleSided(m_backfaceVisibility);
    }
}

void GraphicsLayerCA::updateFilters()
{
    m_layer->setFilters(m_filters);

    if (LayerMap* layerCloneMap = m_layerClones.get()) {
        for (auto& clone : *layerCloneMap) {
            if (m_replicaLayer && isReplicatedRootClone(clone.key))
                continue;

            clone.value->setFilters(m_filters);
        }
    }
}

void GraphicsLayerCA::updateBackdropFilters()
{
    if (m_backdropFilters.isEmpty()) {
        if (m_backdropLayer) {
            m_backdropLayer->removeFromSuperlayer();
            m_backdropLayer->setOwner(nullptr);
            m_backdropLayer = nullptr;
        }
        return;
    }

    if (!m_backdropLayer) {
        m_backdropLayer = createPlatformCALayer(PlatformCALayer::LayerTypeBackdropLayer, this);
        m_backdropLayer->setAnchorPoint(FloatPoint3D());
        m_backdropLayer->setMasksToBounds(true);
    }
    m_backdropLayer->setFilters(m_backdropFilters);
}

void GraphicsLayerCA::updateBackdropFiltersRect()
{
    if (!m_backdropLayer)
        return;
    FloatRect contentBounds(0, 0, m_backdropFiltersRect.width(), m_backdropFiltersRect.height());
    m_backdropLayer->setBounds(contentBounds);
    m_backdropLayer->setPosition(m_backdropFiltersRect.location());
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
}

void GraphicsLayerCA::updateWindRule()
{
    m_layer->setShapeWindRule(m_shapeLayerWindRule);
}

void GraphicsLayerCA::updateStructuralLayer()
{
    ensureStructuralLayer(structuralLayerPurpose());
}

void GraphicsLayerCA::ensureStructuralLayer(StructuralLayerPurpose purpose)
{
    const LayerChangeFlags structuralLayerChangeFlags = NameChanged
        | GeometryChanged
        | TransformChanged
        | ChildrenTransformChanged
        | ChildrenChanged
        | BackfaceVisibilityChanged
        | FiltersChanged
        | BackdropFiltersChanged
        | OpacityChanged;

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

            m_uncommittedChanges |= structuralLayerChangeFlags;
        }
        return;
    }

    bool structuralLayerChanged = false;
    
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
        return;
    
    m_uncommittedChanges |= structuralLayerChangeFlags;

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
        LayerMap::const_iterator end = m_layerClones->end();
        for (LayerMap::const_iterator it = m_layerClones->begin(); it != end; ++it) {
            PlatformCALayer* currLayer = it->value.get();
            currLayer->setPosition(point);
            currLayer->setAnchorPoint(anchorPoint);
            currLayer->setTransform(TransformationMatrix());
            currLayer->setOpacity(1);
        }
    }

    moveAnimations(m_layer.get(), m_structuralLayer.get());
}

GraphicsLayerCA::StructuralLayerPurpose GraphicsLayerCA::structuralLayerPurpose() const
{
    if (preserves3D())
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
        m_layer->setContents(0);
        if (m_layerClones) {
            LayerMap::const_iterator end = m_layerClones->end();
            for (LayerMap::const_iterator it = m_layerClones->begin(); it != end; ++it)
                it->value->setContents(0);
        }
    }
}

void GraphicsLayerCA::updateCoverage()
{
    // FIXME: Need to set coverage on clone layers too.
    if (TiledBacking* backing = tiledBacking()) {
        backing->setVisibleRect(m_visibleRect);
        backing->setCoverageRect(m_coverageRect);
    }

    m_layer->setBackingStoreAttached(m_intersectsCoverageRect);
    if (m_layerClones) {
        LayerMap::const_iterator end = m_layerClones->end();
        for (LayerMap::const_iterator it = m_layerClones->begin(); it != end; ++it)
            it->value->setBackingStoreAttached(m_intersectsCoverageRect);
    }

    m_sizeAtLastCoverageRectUpdate = m_size;
}

void GraphicsLayerCA::updateAcceleratesDrawing()
{
    m_layer->setAcceleratesDrawing(m_acceleratesDrawing);
}

static void setLayerDebugBorder(PlatformCALayer& layer, Color borderColor, float borderWidth)
{
    layer.setBorderColor(borderColor);
    layer.setBorderWidth(borderColor.isValid() ? borderWidth : 0);
}

static const float contentsLayerBorderWidth = 4;
static Color contentsLayerDebugBorderColor(bool showingBorders)
{
    return showingBorders ? Color(0, 0, 128, 180) : Color();
}

static const float cloneLayerBorderWidth = 2;
static Color cloneLayerDebugBorderColor(bool showingBorders)
{
    return showingBorders ? Color(255, 122, 251) : Color();
}

void GraphicsLayerCA::updateDebugBorder()
{
    Color borderColor;
    float width = 0;

    bool showDebugBorders = isShowingDebugBorder();
    if (showDebugBorders)
        getDebugBorderInfo(borderColor, width);

    setLayerDebugBorder(*m_layer, borderColor, width);
    if (m_contentsLayer)
        setLayerDebugBorder(*m_contentsLayer, contentsLayerDebugBorderColor(showDebugBorders), contentsLayerBorderWidth);

    if (m_layerClones) {
        for (auto& clone : m_layerClones->values())
            setLayerDebugBorder(*clone, borderColor, width);
    }

    if (m_structuralLayerClones) {
        Color cloneLayerBorderColor = cloneLayerDebugBorderColor(showDebugBorders);
        for (auto& clone : m_structuralLayerClones->values())
            setLayerDebugBorder(*clone, cloneLayerBorderColor, cloneLayerBorderWidth);
    }

    if (m_contentsLayerClones) {
        Color contentsLayerBorderColor = contentsLayerDebugBorderColor(showDebugBorders);
        for (auto& contentsLayerClone : m_contentsLayerClones->values())
            setLayerDebugBorder(*contentsLayerClone, contentsLayerBorderColor, contentsLayerBorderWidth);
    }
}

FloatRect GraphicsLayerCA::adjustTiledLayerVisibleRect(TiledBacking* tiledBacking, const FloatRect& oldVisibleRect, const FloatRect& newVisibleRect, const FloatSize& oldSize, const FloatSize& newSize)
{
    // If the old visible rect is empty, we have no information about how the visible area is changing
    // (maybe the layer was just created), so don't attempt to expand. Also don't attempt to expand
    // if the size changed or the rects don't overlap.
    if (oldVisibleRect.isEmpty() || newSize != oldSize || !newVisibleRect.intersects(oldVisibleRect))
        return newVisibleRect;

    const float paddingMultiplier = 2;

    float leftEdgeDelta = paddingMultiplier * (newVisibleRect.x() - oldVisibleRect.x());
    float rightEdgeDelta = paddingMultiplier * (newVisibleRect.maxX() - oldVisibleRect.maxX());

    float topEdgeDelta = paddingMultiplier * (newVisibleRect.y() - oldVisibleRect.y());
    float bottomEdgeDelta = paddingMultiplier * (newVisibleRect.maxY() - oldVisibleRect.maxY());
    
    FloatRect existingTileBackingRect = tiledBacking->visibleRect();
    FloatRect expandedRect = newVisibleRect;

    // More exposed on left side.
    if (leftEdgeDelta < 0) {
        float newLeft = expandedRect.x() + leftEdgeDelta;
        // Pad to the left, but don't reduce padding that's already in the backing store (since we're still exposing to the left).
        if (newLeft < existingTileBackingRect.x())
            expandedRect.shiftXEdgeTo(newLeft);
        else
            expandedRect.shiftXEdgeTo(existingTileBackingRect.x());
    }

    // More exposed on right.
    if (rightEdgeDelta > 0) {
        float newRight = expandedRect.maxX() + rightEdgeDelta;
        // Pad to the right, but don't reduce padding that's already in the backing store (since we're still exposing to the right).
        if (newRight > existingTileBackingRect.maxX())
            expandedRect.setWidth(newRight - expandedRect.x());
        else
            expandedRect.setWidth(existingTileBackingRect.maxX() - expandedRect.x());
    }

    // More exposed at top.
    if (topEdgeDelta < 0) {
        float newTop = expandedRect.y() + topEdgeDelta;
        if (newTop < existingTileBackingRect.y())
            expandedRect.shiftYEdgeTo(newTop);
        else
            expandedRect.shiftYEdgeTo(existingTileBackingRect.y());
    }

    // More exposed on bottom.
    if (bottomEdgeDelta > 0) {
        float newBottom = expandedRect.maxY() + bottomEdgeDelta;
        if (newBottom > existingTileBackingRect.maxY())
            expandedRect.setHeight(newBottom - expandedRect.y());
        else
            expandedRect.setHeight(existingTileBackingRect.maxY() - expandedRect.y());
    }
    
    expandedRect.intersect(tiledBacking->boundsWithoutMargin());
    return expandedRect;
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
#ifndef NDEBUG
            m_contentsLayer->setName(String::format("Image Layer %llu", m_contentsLayer->layerID()));
#endif
            setupContentsLayer(m_contentsLayer.get());
            // m_contentsLayer will be parented by updateSublayerList
        }

        // FIXME: maybe only do trilinear if the image is being scaled down,
        // but then what if the layer size changes?
        m_contentsLayer->setMinificationFilter(PlatformCALayer::Trilinear);
        m_contentsLayer->setContents(m_pendingContentsImage.get());
        m_pendingContentsImage = 0;

        if (m_contentsLayerClones) {
            LayerMap::const_iterator end = m_contentsLayerClones->end();
            for (LayerMap::const_iterator it = m_contentsLayerClones->begin(); it != end; ++it)
                it->value->setContents(m_contentsLayer->contents());
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
    setupContentsLayer(m_contentsLayer.get());

    if (m_contentsLayerPurpose == ContentsLayerForCanvas)
        m_contentsLayer->setNeedsDisplay();

    updateContentsRects();
}

void GraphicsLayerCA::updateContentsColorLayer()
{
    // Color layer was set as m_contentsLayer, and will get parented in updateSublayerList().
    if (!m_contentsLayer || m_contentsLayerPurpose != ContentsLayerForBackgroundColor)
        return;

    setupContentsLayer(m_contentsLayer.get());
    updateContentsRects();
    ASSERT(m_contentsSolidColor.isValid());
    m_contentsLayer->setBackgroundColor(m_contentsSolidColor);

    if (m_contentsLayerClones) {
        LayerMap::const_iterator end = m_contentsLayerClones->end();
        for (LayerMap::const_iterator it = m_contentsLayerClones->begin(); it != end; ++it)
            it->value->setBackgroundColor(m_contentsSolidColor);
    }
}

// The clipping strategy depends on whether the rounded rect has equal corner radii.
void GraphicsLayerCA::updateClippingStrategy(PlatformCALayer& clippingLayer, RefPtr<PlatformCALayer>& shapeMaskLayer, const FloatRoundedRect& roundedRect)
{
    if (roundedRect.radii().isUniformCornerRadius()) {
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
        shapeMaskLayer->setAnchorPoint(FloatPoint3D());
    }
    
    shapeMaskLayer->setPosition(FloatPoint());
    shapeMaskLayer->setBounds(clippingLayer.bounds());

    clippingLayer.setCornerRadius(0);
    clippingLayer.setMask(shapeMaskLayer.get());
    
    FloatRoundedRect offsetRoundedRect(clippingLayer.bounds(), roundedRect.radii());
    shapeMaskLayer->setShapeRoundedRect(offsetRoundedRect);
}

void GraphicsLayerCA::updateContentsRects()
{
    if (!m_contentsLayer)
        return;

    FloatPoint contentOrigin;
    const FloatRect contentBounds(0, 0, m_contentsRect.width(), m_contentsRect.height());

    FloatPoint clippingOrigin(m_contentsClippingRect.rect().location());
    FloatRect clippingBounds(FloatPoint(), m_contentsClippingRect.rect().size());
    
    bool gainedOrLostClippingLayer = false;
    if (m_contentsClippingRect.isRounded() || !m_contentsClippingRect.rect().contains(m_contentsRect)) {
        if (!m_contentsClippingLayer) {
            m_contentsClippingLayer = createPlatformCALayer(PlatformCALayer::LayerTypeLayer, this);
            m_contentsClippingLayer->setAnchorPoint(FloatPoint());
#ifndef NDEBUG
            m_contentsClippingLayer->setName(String::format("Contents Clipping Layer %llu", m_contentsClippingLayer->layerID()));
#endif
            gainedOrLostClippingLayer = true;
        }

        m_contentsClippingLayer->setPosition(clippingOrigin);
        m_contentsClippingLayer->setBounds(clippingBounds);

        updateClippingStrategy(*m_contentsClippingLayer, m_contentsShapeMaskLayer, m_contentsClippingRect);

        if (gainedOrLostClippingLayer) {
            m_contentsLayer->removeFromSuperlayer();
            m_contentsClippingLayer->appendSublayer(*m_contentsLayer);
        }
    
        contentOrigin = FloatPoint(m_contentsRect.location() - m_contentsClippingRect.rect().location());
    } else {
        if (m_contentsClippingLayer) {
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

        contentOrigin = m_contentsRect.location();
    }
    
    if (gainedOrLostClippingLayer)
        noteSublayersChanged(DontScheduleFlush);

    m_contentsLayer->setPosition(contentOrigin);
    m_contentsLayer->setBounds(contentBounds);

    if (m_contentsLayerClones) {
        for (auto& layer : m_contentsLayerClones->values()) {
            layer->setPosition(contentOrigin);
            layer->setBounds(contentBounds);
        }
    }

    if (m_contentsClippingLayerClones) {
        if (!m_contentsShapeMaskLayerClones && m_contentsShapeMaskLayer)
            m_contentsShapeMaskLayerClones = std::make_unique<LayerMap>();

        for (auto& clone : *m_contentsClippingLayerClones) {
            CloneID cloneID = clone.key;
            RefPtr<PlatformCALayer> shapeMaskLayerClone;
            if (m_contentsShapeMaskLayerClones)
                shapeMaskLayerClone = m_contentsShapeMaskLayerClones->get(cloneID);

            bool hadShapeMask = shapeMaskLayerClone;
            updateClippingStrategy(*clone.value, shapeMaskLayerClone, m_contentsClippingRect);

            if (!shapeMaskLayerClone && m_contentsShapeMaskLayerClones)
                m_contentsShapeMaskLayerClones->remove(cloneID);
            else if (shapeMaskLayerClone && !hadShapeMask)
                m_contentsShapeMaskLayerClones->add(cloneID, shapeMaskLayerClone);
        }
    }
}

void GraphicsLayerCA::updateMasksToBoundsRect()
{
    updateClippingStrategy(*m_layer, m_shapeMaskLayer, m_masksToBoundsRect);

    if (m_layerClones) {
        for (auto& clone : *m_layerClones) {
            CloneID cloneID = clone.key;
            RefPtr<PlatformCALayer> shapeMaskLayerClone;
            if (m_shapeMaskLayerClones)
                shapeMaskLayerClone = m_shapeMaskLayerClones->get(cloneID);

            bool hadShapeMask = shapeMaskLayerClone;
            updateClippingStrategy(*clone.value, shapeMaskLayerClone, m_masksToBoundsRect);

            if (!shapeMaskLayerClone && m_shapeMaskLayerClones)
                m_shapeMaskLayerClones->remove(cloneID);
            else if (shapeMaskLayerClone && !hadShapeMask)
                m_shapeMaskLayerClones->add(cloneID, shapeMaskLayerClone);
        }
    }
}

void GraphicsLayerCA::updateMaskLayer()
{
    PlatformCALayer* maskCALayer = m_maskLayer ? downcast<GraphicsLayerCA>(*m_maskLayer).primaryLayer() : nullptr;
    m_layer->setMask(maskCALayer);

    LayerMap* maskLayerCloneMap = m_maskLayer ? downcast<GraphicsLayerCA>(*m_maskLayer).primaryLayerClones() : nullptr;
    
    if (LayerMap* layerCloneMap = m_layerClones.get()) {
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
    
    return String::adopt(result);
}

PassRefPtr<PlatformCALayer> GraphicsLayerCA::replicatedLayerRoot(ReplicaState& replicaState)
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
    if (m_animationsToProcess.size()) {
        AnimationsToProcessMap::const_iterator end = m_animationsToProcess.end();
        for (AnimationsToProcessMap::const_iterator it = m_animationsToProcess.begin(); it != end; ++it) {
            const String& currAnimationName = it->key;
            AnimationsMap::iterator animationIt = m_runningAnimations.find(currAnimationName);
            if (animationIt == m_runningAnimations.end())
                continue;

            const AnimationProcessingAction& processingInfo = it->value;
            const Vector<LayerPropertyAnimation>& animations = animationIt->value;
            for (size_t i = 0; i < animations.size(); ++i) {
                const LayerPropertyAnimation& currAnimation = animations[i];
                switch (processingInfo.action) {
                case Remove:
                    removeCAAnimationFromLayer(currAnimation.m_property, currAnimationName, currAnimation.m_index, currAnimation.m_subIndex);
                    break;
                case Pause:
                    pauseCAAnimationOnLayer(currAnimation.m_property, currAnimationName, currAnimation.m_index, currAnimation.m_subIndex, processingInfo.timeOffset);
                    break;
                }
            }

            if (processingInfo.action == Remove)
                m_runningAnimations.remove(currAnimationName);
        }
    
        m_animationsToProcess.clear();
    }
    
    size_t numAnimations;
    if ((numAnimations = m_uncomittedAnimations.size())) {
        for (size_t i = 0; i < numAnimations; ++i) {
            const LayerPropertyAnimation& pendingAnimation = m_uncomittedAnimations[i];
            setAnimationOnLayer(*pendingAnimation.m_animation, pendingAnimation.m_property, pendingAnimation.m_name, pendingAnimation.m_index, pendingAnimation.m_subIndex, pendingAnimation.m_timeOffset);
            
            AnimationsMap::iterator it = m_runningAnimations.find(pendingAnimation.m_name);
            if (it == m_runningAnimations.end()) {
                Vector<LayerPropertyAnimation> animations;
                animations.append(pendingAnimation);
                m_runningAnimations.add(pendingAnimation.m_name, animations);
            } else {
                Vector<LayerPropertyAnimation>& animations = it->value;
                animations.append(pendingAnimation);
            }
        }
        m_uncomittedAnimations.clear();
    }
}

bool GraphicsLayerCA::isRunningTransformAnimation() const
{
    AnimationsMap::const_iterator end = m_runningAnimations.end();
    for (AnimationsMap::const_iterator it = m_runningAnimations.begin(); it != end; ++it) {
        const Vector<LayerPropertyAnimation>& propertyAnimations = it->value;
        size_t numAnimations = propertyAnimations.size();
        for (size_t i = 0; i < numAnimations; ++i) {
            const LayerPropertyAnimation& currAnimation = propertyAnimations[i];
            if (currAnimation.m_property == AnimatedPropertyTransform)
                return true;
        }
    }
    return false;
}

void GraphicsLayerCA::setAnimationOnLayer(PlatformCAAnimation& caAnim, AnimatedPropertyID property, const String& animationName, int index, int subIndex, double timeOffset)
{
    PlatformCALayer* layer = animatedLayer(property);

    if (timeOffset)
        caAnim.setBeginTime(CACurrentMediaTime() - timeOffset);

    String animationID = animationIdentifier(animationName, property, index, subIndex);

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

bool GraphicsLayerCA::removeCAAnimationFromLayer(AnimatedPropertyID property, const String& animationName, int index, int subIndex)
{
    PlatformCALayer* layer = animatedLayer(property);

    String animationID = animationIdentifier(animationName, property, index, subIndex);

    if (!layer->animationForKey(animationID))
        return false;
    
    layer->removeAnimationForKey(animationID);
    bug7311367Workaround(m_structuralLayer.get(), m_transform);

    if (LayerMap* layerCloneMap = animatedLayerClones(property)) {
        for (auto& clone : *layerCloneMap) {
            // Skip immediate replicas, since they move with the original.
            if (m_replicaLayer && isReplicatedRootClone(clone.key))
                continue;

            clone.value->removeAnimationForKey(animationID);
        }
    }
    return true;
}

void GraphicsLayerCA::pauseCAAnimationOnLayer(AnimatedPropertyID property, const String& animationName, int index, int subIndex, double timeOffset)
{
    PlatformCALayer* layer = animatedLayer(property);

    String animationID = animationIdentifier(animationName, property, index, subIndex);

    RefPtr<PlatformCAAnimation> curAnim = layer->animationForKey(animationID);
    if (!curAnim)
        return;

    // Animations on the layer are immutable, so we have to clone and modify.
    RefPtr<PlatformCAAnimation> newAnim = curAnim->copy();

    newAnim->setSpeed(0);
    newAnim->setTimeOffset(timeOffset);
    
    layer->addAnimationForKey(animationID, *newAnim); // This will replace the running animation.

    // Pause the animations on the clones too.
    if (LayerMap* layerCloneMap = animatedLayerClones(property)) {
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

    if (!m_dirtyRects.size())
        return;

    for (size_t i = 0; i < m_dirtyRects.size(); ++i)
        m_layer->setNeedsDisplayInRect(m_dirtyRects[i]);
    
    m_dirtyRects.clear();
}

void GraphicsLayerCA::updateContentsNeedsDisplay()
{
    if (m_contentsLayer)
        m_contentsLayer->setNeedsDisplay();
}

bool GraphicsLayerCA::createAnimationFromKeyframes(const KeyframeValueList& valueList, const Animation* animation, const String& animationName, double timeOffset)
{
    ASSERT(valueList.property() != AnimatedPropertyTransform && (!supportsAcceleratedFilterAnimations() || valueList.property() != AnimatedPropertyFilter));

    bool isKeyframe = valueList.size() > 2;
    bool valuesOK;
    
    bool additive = false;
    int animationIndex = 0;
    
    RefPtr<PlatformCAAnimation> caAnimation;
    
    if (isKeyframe) {
        caAnimation = createKeyframeAnimation(animation, propertyIdToString(valueList.property()), additive);
        valuesOK = setAnimationKeyframes(valueList, animation, caAnimation.get());
    } else {
        caAnimation = createBasicAnimation(animation, propertyIdToString(valueList.property()), additive);
        valuesOK = setAnimationEndpoints(valueList, animation, caAnimation.get());
    }
    
    if (!valuesOK)
        return false;

    m_uncomittedAnimations.append(LayerPropertyAnimation(caAnimation, animationName, valueList.property(), animationIndex, 0, timeOffset));

    return true;
}

bool GraphicsLayerCA::appendToUncommittedAnimations(const KeyframeValueList& valueList, const TransformOperations* operations, const Animation* animation, const String& animationName, const FloatSize& boxSize, int animationIndex, double timeOffset, bool isMatrixAnimation)
{
    TransformOperation::OperationType transformOp = isMatrixAnimation ? TransformOperation::MATRIX_3D : operations->operations().at(animationIndex)->type();
    bool additive = animationIndex > 0;
    bool isKeyframe = valueList.size() > 2;

    RefPtr<PlatformCAAnimation> caAnimation;
    bool validMatrices = true;
    if (isKeyframe) {
        caAnimation = createKeyframeAnimation(animation, propertyIdToString(valueList.property()), additive);
        validMatrices = setTransformAnimationKeyframes(valueList, animation, caAnimation.get(), animationIndex, transformOp, isMatrixAnimation, boxSize);
    } else {
        caAnimation = createBasicAnimation(animation, propertyIdToString(valueList.property()), additive);
        validMatrices = setTransformAnimationEndpoints(valueList, animation, caAnimation.get(), animationIndex, transformOp, isMatrixAnimation, boxSize);
    }
    
    if (!validMatrices)
        return false;

    m_uncomittedAnimations.append(LayerPropertyAnimation(caAnimation, animationName, valueList.property(), animationIndex, 0, timeOffset));
    return true;
}

bool GraphicsLayerCA::createTransformAnimationsFromKeyframes(const KeyframeValueList& valueList, const Animation* animation, const String& animationName, double timeOffset, const FloatSize& boxSize)
{
    ASSERT(valueList.property() == AnimatedPropertyTransform);

    bool hasBigRotation;
    int listIndex = validateTransformOperations(valueList, hasBigRotation);
    const TransformOperations* operations = (listIndex >= 0) ? &static_cast<const TransformAnimationValue&>(valueList.at(listIndex)).value() : 0;

    bool validMatrices = true;

    // If function lists don't match we do a matrix animation, otherwise we do a component hardware animation.
    bool isMatrixAnimation = listIndex < 0;
    int numAnimations = isMatrixAnimation ? 1 : operations->size();

#if PLATFORM(IOS)
    bool reverseAnimationList = false;
#else
    bool reverseAnimationList = true;
#if !PLATFORM(WIN)
        // Old versions of Core Animation apply animations in reverse order (<rdar://problem/7095638>) so we need to flip the list.
        // to be non-additive. For binary compatibility, the current version of Core Animation preserves this behavior for applications linked
        // on or before Snow Leopard.
        // FIXME: This fix has not been added to QuartzCore on Windows yet (<rdar://problem/9112233>) so we expect the
        // reversed animation behavior
        static bool executableWasLinkedOnOrBeforeSnowLeopard = wkExecutableWasLinkedOnOrBeforeSnowLeopard();
        if (!executableWasLinkedOnOrBeforeSnowLeopard)
            reverseAnimationList = false;
#endif
#endif // PLATFORM(IOS)
    if (reverseAnimationList) {
        for (int animationIndex = numAnimations - 1; animationIndex >= 0; --animationIndex) {
            if (!appendToUncommittedAnimations(valueList, operations, animation, animationName, boxSize, animationIndex, timeOffset, isMatrixAnimation)) {
                validMatrices = false;
                break;
            }
        }
    } else {
        for (int animationIndex = 0; animationIndex < numAnimations; ++animationIndex) {
            if (!appendToUncommittedAnimations(valueList, operations, animation, animationName, boxSize, animationIndex, timeOffset, isMatrixAnimation)) {
                validMatrices = false;
                break;
            }
        }
    }

    return validMatrices;
}

bool GraphicsLayerCA::appendToUncommittedAnimations(const KeyframeValueList& valueList, const FilterOperation* operation, const Animation* animation, const String& animationName, int animationIndex, double timeOffset)
{
    bool isKeyframe = valueList.size() > 2;
    
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
        String keyPath = String::format("filters.filter_%d.%s", animationIndex, PlatformCAFilters::animatedFilterPropertyName(filterOp, internalFilterPropertyIndex));
        
        if (isKeyframe) {
            caAnimation = createKeyframeAnimation(animation, keyPath, false);
            valuesOK = setFilterAnimationKeyframes(valueList, animation, caAnimation.get(), animationIndex, internalFilterPropertyIndex, filterOp);
        } else {
            caAnimation = createBasicAnimation(animation, keyPath, false);
            valuesOK = setFilterAnimationEndpoints(valueList, animation, caAnimation.get(), animationIndex, internalFilterPropertyIndex);
        }
        
        ASSERT(valuesOK);

        m_uncomittedAnimations.append(LayerPropertyAnimation(caAnimation, animationName, valueList.property(), animationIndex, internalFilterPropertyIndex, timeOffset));
    }

    return true;
}

bool GraphicsLayerCA::createFilterAnimationsFromKeyframes(const KeyframeValueList& valueList, const Animation* animation, const String& animationName, double timeOffset)
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
        if (!appendToUncommittedAnimations(valueList, operations.operations().at(animationIndex).get(), animation, animationName, animationIndex, timeOffset))
            return false;
    }

    return true;
}

PassRefPtr<PlatformCAAnimation> GraphicsLayerCA::createBasicAnimation(const Animation* anim, const String& keyPath, bool additive)
{
    RefPtr<PlatformCAAnimation> basicAnim = createPlatformCAAnimation(PlatformCAAnimation::Basic, keyPath);
    setupAnimation(basicAnim.get(), anim, additive);
    return basicAnim;
}

PassRefPtr<PlatformCAAnimation>GraphicsLayerCA::createKeyframeAnimation(const Animation* anim, const String& keyPath, bool additive)
{
    RefPtr<PlatformCAAnimation> keyframeAnim = createPlatformCAAnimation(PlatformCAAnimation::Keyframe, keyPath);
    setupAnimation(keyframeAnim.get(), anim, additive);
    return keyframeAnim;
}

void GraphicsLayerCA::setupAnimation(PlatformCAAnimation* propertyAnim, const Animation* anim, bool additive)
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
    case AnimationFillModeNone:
        fillMode = PlatformCAAnimation::Forwards; // Use "forwards" rather than "removed" because the style system will remove the animation when it is finished. This avoids a flash.
        break;
    case AnimationFillModeBackwards:
        fillMode = PlatformCAAnimation::Both; // Use "both" rather than "backwards" because the style system will remove the animation when it is finished. This avoids a flash.
        break;
    case AnimationFillModeForwards:
        fillMode = PlatformCAAnimation::Forwards;
        break;
    case AnimationFillModeBoth:
        fillMode = PlatformCAAnimation::Both;
        break;
    }

    propertyAnim->setDuration(duration);
    propertyAnim->setRepeatCount(repeatCount);
    propertyAnim->setAutoreverses(anim->direction() == Animation::AnimationDirectionAlternate || anim->direction() == Animation::AnimationDirectionAlternateReverse);
    propertyAnim->setRemovedOnCompletion(false);
    propertyAnim->setAdditive(additive);
    propertyAnim->setFillMode(fillMode);
}

const TimingFunction* GraphicsLayerCA::timingFunctionForAnimationValue(const AnimationValue& animValue, const Animation& anim)
{
    if (animValue.timingFunction())
        return animValue.timingFunction();
    if (anim.isTimingFunctionSet())
        return anim.timingFunction().get();
    
    return CubicBezierTimingFunction::defaultTimingFunction();
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

    // This codepath is used for 2-keyframe animations, so we still need to look in the start
    // for a timing function. Even in the reversing animation case, the first keyframe provides the timing function.
    const TimingFunction* timingFunction = timingFunctionForAnimationValue(valueList.at(0), *animation);
    if (timingFunction)
        basicAnim->setTimingFunction(timingFunction, !forwards);

    return true;
}

bool GraphicsLayerCA::setAnimationKeyframes(const KeyframeValueList& valueList, const Animation* animation, PlatformCAAnimation* keyframeAnim)
{
    Vector<float> keyTimes;
    Vector<float> values;
    Vector<const TimingFunction*> timingFunctions;

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
            timingFunctions.append(timingFunctionForAnimationValue(forwards ? curValue : valueList.at(index - 1), *animation));
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
    
    const TransformAnimationValue& startValue = static_cast<const TransformAnimationValue&>(valueList.at(fromIndex));
    const TransformAnimationValue& endValue = static_cast<const TransformAnimationValue&>(valueList.at(toIndex));

    if (isMatrixAnimation) {
        TransformationMatrix fromTransform, toTransform;
        startValue.value().apply(boxSize, fromTransform);
        endValue.value().apply(boxSize, toTransform);

        // If any matrix is singular, CA won't animate it correctly. So fall back to software animation
        if (!fromTransform.isInvertible() || !toTransform.isInvertible())
            return false;

        basicAnim->setFromValue(fromTransform);
        basicAnim->setToValue(toTransform);
    } else {
        if (isTransformTypeNumber(transformOpType)) {
            float fromValue;
            getTransformFunctionValue(startValue.value().at(functionIndex), transformOpType, boxSize, fromValue);
            basicAnim->setFromValue(fromValue);
            
            float toValue;
            getTransformFunctionValue(endValue.value().at(functionIndex), transformOpType, boxSize, toValue);
            basicAnim->setToValue(toValue);
        } else if (isTransformTypeFloatPoint3D(transformOpType)) {
            FloatPoint3D fromValue;
            getTransformFunctionValue(startValue.value().at(functionIndex), transformOpType, boxSize, fromValue);
            basicAnim->setFromValue(fromValue);
            
            FloatPoint3D toValue;
            getTransformFunctionValue(endValue.value().at(functionIndex), transformOpType, boxSize, toValue);
            basicAnim->setToValue(toValue);
        } else {
            TransformationMatrix fromValue;
            getTransformFunctionValue(startValue.value().at(functionIndex), transformOpType, boxSize, fromValue);
            basicAnim->setFromValue(fromValue);

            TransformationMatrix toValue;
            getTransformFunctionValue(endValue.value().at(functionIndex), transformOpType, boxSize, toValue);
            basicAnim->setToValue(toValue);
        }
    }

    // This codepath is used for 2-keyframe animations, so we still need to look in the start
    // for a timing function. Even in the reversing animation case, the first keyframe provides the timing function.
    const TimingFunction* timingFunction = timingFunctionForAnimationValue(valueList.at(0), *animation);
    basicAnim->setTimingFunction(timingFunction, !forwards);

    PlatformCAAnimation::ValueFunctionType valueFunction = getValueFunctionNameForTransformOperation(transformOpType);
    if (valueFunction != PlatformCAAnimation::NoValueFunction)
        basicAnim->setValueFunction(valueFunction);

    return true;
}

bool GraphicsLayerCA::setTransformAnimationKeyframes(const KeyframeValueList& valueList, const Animation* animation, PlatformCAAnimation* keyframeAnim, int functionIndex, TransformOperation::OperationType transformOpType, bool isMatrixAnimation, const FloatSize& boxSize)
{
    Vector<float> keyTimes;
    Vector<float> floatValues;
    Vector<FloatPoint3D> floatPoint3DValues;
    Vector<TransformationMatrix> transformationMatrixValues;
    Vector<const TimingFunction*> timingFunctions;

    bool forwards = animation->directionIsForwards();

    for (unsigned i = 0; i < valueList.size(); ++i) {
        unsigned index = forwards ? i : (valueList.size() - i - 1);
        const TransformAnimationValue& curValue = static_cast<const TransformAnimationValue&>(valueList.at(index));
        keyTimes.append(forwards ? curValue.keyTime() : (1 - curValue.keyTime()));

        TransformationMatrix transform;

        if (isMatrixAnimation) {
            curValue.value().apply(boxSize, transform);

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

            curValue.value().apply(boxSize, transform);
        }

        if (i < (valueList.size() - 1))
            timingFunctions.append(timingFunctionForAnimationValue(forwards ? curValue : valueList.at(index - 1), *animation));
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

    // This codepath is used for 2-keyframe animations, so we still need to look in the start
    // for a timing function. Even in the reversing animation case, the first keyframe provides the timing function.
    basicAnim->setTimingFunction(timingFunctionForAnimationValue(valueList.at(0), *animation), !forwards);

    return true;
}

bool GraphicsLayerCA::setFilterAnimationKeyframes(const KeyframeValueList& valueList, const Animation* animation, PlatformCAAnimation* keyframeAnim, int functionIndex, int internalFilterPropertyIndex, FilterOperation::OperationType filterOp)
{
    Vector<float> keyTimes;
    Vector<RefPtr<FilterOperation>> values;
    Vector<const TimingFunction*> timingFunctions;
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
            timingFunctions.append(timingFunctionForAnimationValue(forwards ? curValue : valueList.at(index - 1), *animation));
    }
    
    keyframeAnim->setKeyTimes(keyTimes);
    keyframeAnim->setValues(values, internalFilterPropertyIndex);
    keyframeAnim->setTimingFunctions(timingFunctions, !forwards);

    return true;
}

void GraphicsLayerCA::suspendAnimations(double time)
{
    double t = PlatformCALayer::currentTimeToMediaTime(time ? time : monotonicallyIncreasingTime());
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
    return m_structuralLayer.get() ? m_structuralLayer.get() : m_layer.get(); 
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

GraphicsLayerCA::LayerMap* GraphicsLayerCA::animatedLayerClones(AnimatedPropertyID property) const
{
    return (property == AnimatedPropertyBackgroundColor) ? m_contentsLayerClones.get() : primaryLayerClones();
}

void GraphicsLayerCA::updateContentsScale(float pageScaleFactor)
{
    float contentsScale = pageScaleFactor * deviceScaleFactor();

    if (isPageTiledBackingLayer() && tiledBacking()) {
        float zoomedOutScale = m_client.zoomedOutPageScaleFactor() * deviceScaleFactor();
        tiledBacking()->setZoomedOutContentsScale(zoomedOutScale);
    }

    if (contentsScale == m_layer->contentsScale())
        return;

    m_layer->setContentsScale(contentsScale);

    if (m_contentsLayer && m_contentsLayerPurpose == ContentsLayerForMedia)
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

String GraphicsLayerCA::displayListAsText(DisplayList::AsTextFlags flags) const
{
    if (!m_displayList)
        return String();

    return m_displayList->asText(flags);
}

void GraphicsLayerCA::setIsTrackingDisplayListReplay(bool isTracking)
{
    if (isTracking == m_isTrackingDisplayListReplay)
        return;

    m_isTrackingDisplayListReplay = isTracking;
    if (!m_isTrackingDisplayListReplay)
        layerDisplayListMap().remove(this);
}

String GraphicsLayerCA::replayDisplayListAsText(DisplayList::AsTextFlags flags) const
{
    auto it = layerDisplayListMap().find(this);
    if (it != layerDisplayListMap().end()) {
        TextStream stream;
        
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
        m_layer->setBackgroundColor(Color::transparent);
}

void GraphicsLayerCA::getDebugBorderInfo(Color& color, float& width) const
{
    if (isPageTiledBackingLayer()) {
        color = Color(0, 0, 128, 128); // tile cache layer: dark blue
#if OS(WINDOWS)
        width = 1.0;
#else
        width = 0.5;
#endif
        return;
    }

    GraphicsLayer::getDebugBorderInfo(color, width);
}

static void dumpInnerLayer(TextStream& textStream, String label, PlatformCALayer* layer, int indent, LayerTreeAsTextBehavior behavior)
{
    if (!layer)
        return;

    writeIndent(textStream, indent + 1);
    textStream << "(" << label << " ";
    if (behavior & LayerTreeAsTextDebug)
        textStream << "id=" << layer->layerID() << " ";
    textStream << layer->position().x() << ", " << layer->position().y()
        << " " << layer->bounds().width() << " x " << layer->bounds().height() << ")\n";
}

void GraphicsLayerCA::dumpAdditionalProperties(TextStream& textStream, int indent, LayerTreeAsTextBehavior behavior) const
{
    if (behavior & LayerTreeAsTextIncludeVisibleRects) {
        writeIndent(textStream, indent + 1);
        textStream << "(visible rect " << m_visibleRect.x() << ", " << m_visibleRect.y() << " " << m_visibleRect.width() << " x " << m_visibleRect.height() << ")\n";

        writeIndent(textStream, indent + 1);
        textStream << "(coverage rect " << m_coverageRect.x() << ", " << m_coverageRect.y() << " " << m_coverageRect.width() << " x " << m_coverageRect.height() << ")\n";

        writeIndent(textStream, indent + 1);
        textStream << "(intersects coverage rect " << m_intersectsCoverageRect << ")\n";

        writeIndent(textStream, indent + 1);
        textStream << "(contentsScale " << m_layer->contentsScale() << ")\n";
    }

    if (tiledBacking() && (behavior & LayerTreeAsTextIncludeTileCaches)) {
        if (behavior & LayerTreeAsTextDebug) {
            writeIndent(textStream, indent + 1);
            textStream << "(tiled backing " << tiledBacking() << ")\n";
        }

        IntRect tileCoverageRect = tiledBacking()->tileCoverageRect();
        writeIndent(textStream, indent + 1);
        textStream << "(tile cache coverage " << tileCoverageRect.x() << ", " << tileCoverageRect.y() << " " << tileCoverageRect.width() << " x " << tileCoverageRect.height() << ")\n";

        IntSize tileSize = tiledBacking()->tileSize();
        writeIndent(textStream, indent + 1);
        textStream << "(tile size " << tileSize.width() << " x " << tileSize.height() << ")\n";
        
        IntRect gridExtent = tiledBacking()->tileGridExtent();
        writeIndent(textStream, indent + 1);
        textStream << "(top left tile " << gridExtent.x() << ", " << gridExtent.y() << " tiles grid " << gridExtent.width() << " x " << gridExtent.height() << ")\n";
    }
    
    if (behavior & LayerTreeAsTextIncludeContentLayers) {
        dumpInnerLayer(textStream, "structural layer", m_structuralLayer.get(), indent, behavior);
        dumpInnerLayer(textStream, "contents clipping layer", m_contentsClippingLayer.get(), indent, behavior);
        dumpInnerLayer(textStream, "shape mask layer", m_shapeMaskLayer.get(), indent, behavior);
        dumpInnerLayer(textStream, "contents layer", m_contentsLayer.get(), indent, behavior);
        dumpInnerLayer(textStream, "contents shape mask layer", m_contentsShapeMaskLayer.get(), indent, behavior);
        dumpInnerLayer(textStream, "backdrop layer", m_backdropLayer.get(), indent, behavior);
    }

    if (behavior & LayerTreeAsTextDebug) {
        writeIndent(textStream, indent + 1);
        textStream << "(acceleratetes drawing " << m_acceleratesDrawing << ")\n";
        writeIndent(textStream, indent + 1);
        textStream << "(uses display-list drawing " << m_usesDisplayListDrawing << ")\n";
    }
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
    if (!m_drawsContent || isPageTiledBackingLayer())
        return false;

    // FIXME: catch zero-size height or width here (or earlier)?
#if PLATFORM(IOS)
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

    RefPtr<PlatformCALayer> oldLayer = m_layer;

    m_layer = createPlatformCALayer(newLayerType, this);

    m_usingTiledBacking = newLayerType == PlatformCALayer::LayerTypeTiledBackingLayer;
    m_usingBackdropLayerType = isCustomBackdropLayerType(newLayerType);

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

    m_uncommittedChanges |= ChildrenChanged
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
        | MaskLayerChanged
        | OpacityChanged
        | DebugIndicatorsChanged;
    
    if (m_usingTiledBacking)
        m_uncommittedChanges |= CoverageRectChanged;

#ifndef NDEBUG
    String name = String::format("%sCALayer(%p) GraphicsLayer(%p, %llu) ", (newLayerType == PlatformCALayer::LayerTypeWebTiledLayer) ? "Tiled " : "", m_layer->platformLayer(), this, primaryLayerID()) + m_name;
    m_layer->setName(name);
#endif

    moveAnimations(oldLayer.get(), m_layer.get());
    
    // need to tell new layer to draw itself
    setNeedsDisplay();

    if (oldLayerType == PlatformCALayer::LayerTypeTiledBackingLayer || newLayerType == PlatformCALayer::LayerTypeTiledBackingLayer)
        client().tiledBackingUsageChanged(this, m_usingTiledBacking);
}

GraphicsLayer::CompositingCoordinatesOrientation GraphicsLayerCA::defaultContentsOrientation() const
{
    return CompositingCoordinatesTopDown;
}

void GraphicsLayerCA::setupContentsLayer(PlatformCALayer* contentsLayer)
{
    // Turn off implicit animations on the inner layer.
#if !PLATFORM(IOS)
    contentsLayer->setMasksToBounds(true);
#endif

    if (defaultContentsOrientation() == CompositingCoordinatesBottomUp) {
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

PassRefPtr<PlatformCALayer> GraphicsLayerCA::findOrMakeClone(CloneID cloneID, PlatformCALayer *sourceLayer, LayerMap* clones, CloneLevel cloneLevel)
{
    if (!sourceLayer)
        return nullptr;

    RefPtr<PlatformCALayer> resultLayer;

    // Add with a dummy value to get an iterator for the insertion position, and a boolean that tells
    // us whether there's an item there. This technique avoids two hash lookups.
    RefPtr<PlatformCALayer> dummy;
    LayerMap::AddResult addResult = clones->add(cloneID, dummy);
    if (!addResult.isNewEntry) {
        // Value was not added, so it exists already.
        resultLayer = addResult.iterator->value.get();
    } else {
        resultLayer = cloneLayer(sourceLayer, cloneLevel);
#ifndef NDEBUG
        resultLayer->setName(String::format("Clone %d of layer %llu", cloneID[0U], sourceLayer->layerID()));
#endif
        addResult.iterator->value = resultLayer;
    }

    return resultLayer;
}   

void GraphicsLayerCA::ensureCloneLayers(CloneID cloneID, RefPtr<PlatformCALayer>& primaryLayer, RefPtr<PlatformCALayer>& structuralLayer,
    RefPtr<PlatformCALayer>& contentsLayer, RefPtr<PlatformCALayer>& contentsClippingLayer, RefPtr<PlatformCALayer>& contentsShapeMaskLayer, RefPtr<PlatformCALayer>& shapeMaskLayer, CloneLevel cloneLevel)
{
    structuralLayer = nullptr;
    contentsLayer = nullptr;

    if (!m_layerClones)
        m_layerClones = std::make_unique<LayerMap>();

    if (!m_structuralLayerClones && m_structuralLayer)
        m_structuralLayerClones = std::make_unique<LayerMap>();

    if (!m_contentsLayerClones && m_contentsLayer)
        m_contentsLayerClones = std::make_unique<LayerMap>();

    if (!m_contentsClippingLayerClones && m_contentsClippingLayer)
        m_contentsClippingLayerClones = std::make_unique<LayerMap>();

    if (!m_contentsShapeMaskLayerClones && m_contentsShapeMaskLayer)
        m_contentsShapeMaskLayerClones = std::make_unique<LayerMap>();

    if (!m_shapeMaskLayerClones && m_shapeMaskLayer)
        m_shapeMaskLayerClones = std::make_unique<LayerMap>();

    primaryLayer = findOrMakeClone(cloneID, m_layer.get(), m_layerClones.get(), cloneLevel);
    structuralLayer = findOrMakeClone(cloneID, m_structuralLayer.get(), m_structuralLayerClones.get(), cloneLevel);
    contentsLayer = findOrMakeClone(cloneID, m_contentsLayer.get(), m_contentsLayerClones.get(), cloneLevel);
    contentsClippingLayer = findOrMakeClone(cloneID, m_contentsClippingLayer.get(), m_contentsClippingLayerClones.get(), cloneLevel);
    contentsShapeMaskLayer = findOrMakeClone(cloneID, m_contentsShapeMaskLayer.get(), m_contentsShapeMaskLayerClones.get(), cloneLevel);
    shapeMaskLayer = findOrMakeClone(cloneID, m_shapeMaskLayer.get(), m_shapeMaskLayerClones.get(), cloneLevel);
}

void GraphicsLayerCA::clearClones(std::unique_ptr<LayerMap>& layerMap)
{
    if (!layerMap)
        return;

    for (auto& layer : layerMap->values())
        layer->setOwner(nullptr);
    
    layerMap = nullptr;
}

void GraphicsLayerCA::removeCloneLayers()
{
    clearClones(m_layerClones);
    clearClones(m_structuralLayerClones);
    clearClones(m_contentsLayerClones);
    clearClones(m_contentsClippingLayerClones);
    clearClones(m_contentsShapeMaskLayerClones);
    clearClones(m_shapeMaskLayerClones);
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

PassRefPtr<PlatformCALayer> GraphicsLayerCA::fetchCloneLayers(GraphicsLayer* replicaRoot, ReplicaState& replicaState, CloneLevel cloneLevel)
{
    RefPtr<PlatformCALayer> primaryLayer;
    RefPtr<PlatformCALayer> structuralLayer;
    RefPtr<PlatformCALayer> contentsLayer;
    RefPtr<PlatformCALayer> contentsClippingLayer;
    RefPtr<PlatformCALayer> contentsShapeMaskLayer;
    RefPtr<PlatformCALayer> shapeMaskLayer;
    ensureCloneLayers(replicaState.cloneID(), primaryLayer, structuralLayer, contentsLayer, contentsClippingLayer, contentsShapeMaskLayer, shapeMaskLayer, cloneLevel);

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

    const Vector<GraphicsLayer*>& childLayers = children();
    Vector<RefPtr<PlatformCALayer>> clonalSublayers;

    RefPtr<PlatformCALayer> replicaLayer;
    
    if (m_replicaLayer && m_replicaLayer != replicaRoot) {
        // We have nested replicas. Ask the replica layer for a clone of its contents.
        replicaState.setBranchType(ReplicaState::ReplicaBranch);
        replicaLayer = downcast<GraphicsLayerCA>(*m_replicaLayer).fetchCloneLayers(replicaRoot, replicaState, RootCloneLevel);
        replicaState.setBranchType(ReplicaState::ChildBranch);
    }

    if (contentsClippingLayer) {
        ASSERT(contentsLayer);
        contentsClippingLayer->appendSublayer(*contentsLayer);
    }

    if (contentsShapeMaskLayer)
        contentsClippingLayer->setMask(contentsShapeMaskLayer.get());

    if (shapeMaskLayer)
        primaryLayer->setMask(shapeMaskLayer.get());
    
    if (replicaLayer || structuralLayer || contentsLayer || contentsClippingLayer || childLayers.size() > 0) {
        if (structuralLayer) {
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

        for (auto* childLayer : childLayers) {
            GraphicsLayerCA& childLayerCA = downcast<GraphicsLayerCA>(*childLayer);
            if (RefPtr<PlatformCALayer> platformLayer = childLayerCA.fetchCloneLayers(replicaRoot, replicaState, IntermediateCloneLevel))
                clonalSublayers.append(platformLayer.release());
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

PassRefPtr<PlatformCALayer> GraphicsLayerCA::cloneLayer(PlatformCALayer *layer, CloneLevel cloneLevel)
{
    RefPtr<PlatformCALayer> newLayer = layer->clone(this);

    if (cloneLevel == IntermediateCloneLevel) {
        newLayer->setOpacity(layer->opacity());
        copyAnimations(layer, newLayer.get());
    }

    setLayerDebugBorder(*newLayer, cloneLayerDebugBorderColor(isShowingDebugBorder()), cloneLayerBorderWidth);

    return newLayer;
}

void GraphicsLayerCA::setOpacityInternal(float accumulatedOpacity)
{
    LayerMap* layerCloneMap = nullptr;
    
    if (preserves3D()) {
        m_layer->setOpacity(accumulatedOpacity);
        layerCloneMap = m_layerClones.get();
    } else {
        primaryLayer()->setOpacity(accumulatedOpacity);
        layerCloneMap = primaryLayerClones();
    }

    if (layerCloneMap) {
        for (auto& clone : *layerCloneMap) {
            if (m_replicaLayer && isReplicatedRootClone(clone.key))
                continue;
            clone.value->setOpacity(m_opacity);
        }
    }
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
}

void GraphicsLayerCA::setIsViewportConstrained(bool isViewportConstrained)
{
    if (isViewportConstrained == m_isViewportConstrained)
        return;

    m_isViewportConstrained = isViewportConstrained;
    noteLayerPropertyChanged(CoverageRectChanged);
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

bool GraphicsLayerCA::canThrottleLayerFlush() const
{
    // Tile layers are currently plain CA layers, attached directly by TileController. They require immediate flush as they may contain garbage.
    return !(m_uncommittedChanges & TilesAdded);
}

void GraphicsLayerCA::noteLayerPropertyChanged(LayerChangeFlags flags, ScheduleFlushOrNot scheduleFlush)
{
    bool hadUncommittedChanges = !!m_uncommittedChanges;
    bool oldCanThrottleLayerFlush = canThrottleLayerFlush();

    m_uncommittedChanges |= flags;

    if (scheduleFlush == ScheduleFlush) {
        bool needsFlush = !hadUncommittedChanges || oldCanThrottleLayerFlush != canThrottleLayerFlush();
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

    if (!m_layer->backingContributesToMemoryEstimate())
        return 0;

    return m_layer->backingStoreBytesPerPixel() * size().width() * m_layer->contentsScale() * size().height() * m_layer->contentsScale();
}

} // namespace WebCore
