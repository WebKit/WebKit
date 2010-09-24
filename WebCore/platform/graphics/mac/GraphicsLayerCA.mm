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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"

#if USE(ACCELERATED_COMPOSITING)

#import "GraphicsLayerCA.h"

#import "Animation.h"
#import "BlockExceptions.h"
#import "FloatConversion.h"
#import "FloatRect.h"
#import "Image.h"
#import "PlatformString.h"
#import <QuartzCore/QuartzCore.h>
#import "RotateTransformOperation.h"
#import "ScaleTransformOperation.h"
#import "StringBuilder.h"
#import "SystemTime.h"
#import "TranslateTransformOperation.h"
#import "WebLayer.h"
#import "WebTiledLayer.h"
#import <limits.h>
#import <objc/objc-auto.h>
#import <wtf/CurrentTime.h>
#import <wtf/UnusedParam.h>
#import <wtf/RetainPtr.h>

using namespace std;

#define HAVE_MODERN_QUARTZCORE (!defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD))

namespace WebCore {

// The threshold width or height above which a tiled layer will be used. This should be
// large enough to avoid tiled layers for most GraphicsLayers, but less than the OpenGL
// texture size limit on all supported hardware.
static const int cMaxPixelDimension = 2000;

// The width and height of a single tile in a tiled layer. Should be large enough to
// avoid lots of small tiles (and therefore lots of drawing callbacks), but small enough
// to keep the overall tile cost low.
static const int cTiledLayerTileSize = 512;

// If we send a duration of 0 to CA, then it will use the default duration
// of 250ms. So send a very small value instead.
static const float cAnimationAlmostZeroDuration = 1e-3f;

// CACurrentMediaTime() is a time since boot. These methods convert between that and
// WebCore time, which is system time (UTC).
static CFTimeInterval currentTimeToMediaTime(double t)
{
    return CACurrentMediaTime() + t - WTF::currentTime();
}

static double mediaTimeToCurrentTime(CFTimeInterval t)
{
    return WTF::currentTime() + t - CACurrentMediaTime();
}

} // namespace WebCore

@interface CALayer(Private)
- (void)setContentsChanged;
@end

@interface WebAnimationDelegate : NSObject {
    WebCore::GraphicsLayerCA* m_graphicsLayer;
}

- (void)animationDidStart:(CAAnimation *)anim;
- (WebCore::GraphicsLayerCA*)graphicsLayer;
- (void)setLayer:(WebCore::GraphicsLayerCA*)graphicsLayer;

@end

@implementation WebAnimationDelegate

- (void)animationDidStart:(CAAnimation *)animation
{
    if (!m_graphicsLayer)
        return;

    double startTime = WebCore::mediaTimeToCurrentTime([animation beginTime]);
    m_graphicsLayer->client()->notifyAnimationStarted(m_graphicsLayer, startTime);
}

- (WebCore::GraphicsLayerCA*)graphicsLayer
{
    return m_graphicsLayer;
}

- (void)setLayer:(WebCore::GraphicsLayerCA*)graphicsLayer
{
    m_graphicsLayer = graphicsLayer;
}

@end

namespace WebCore {

static inline void copyTransform(CATransform3D& toT3D, const TransformationMatrix& t)
{
    toT3D.m11 = narrowPrecisionToFloat(t.m11());
    toT3D.m12 = narrowPrecisionToFloat(t.m12());
    toT3D.m13 = narrowPrecisionToFloat(t.m13());
    toT3D.m14 = narrowPrecisionToFloat(t.m14());
    toT3D.m21 = narrowPrecisionToFloat(t.m21());
    toT3D.m22 = narrowPrecisionToFloat(t.m22());
    toT3D.m23 = narrowPrecisionToFloat(t.m23());
    toT3D.m24 = narrowPrecisionToFloat(t.m24());
    toT3D.m31 = narrowPrecisionToFloat(t.m31());
    toT3D.m32 = narrowPrecisionToFloat(t.m32());
    toT3D.m33 = narrowPrecisionToFloat(t.m33());
    toT3D.m34 = narrowPrecisionToFloat(t.m34());
    toT3D.m41 = narrowPrecisionToFloat(t.m41());
    toT3D.m42 = narrowPrecisionToFloat(t.m42());
    toT3D.m43 = narrowPrecisionToFloat(t.m43());
    toT3D.m44 = narrowPrecisionToFloat(t.m44());
}

static NSValue* getTransformFunctionValue(const TransformOperation* transformOp, TransformOperation::OperationType transformType, const IntSize& size)
{
    switch (transformType) {
        case TransformOperation::ROTATE:
        case TransformOperation::ROTATE_X:
        case TransformOperation::ROTATE_Y:
            return [NSNumber numberWithDouble:transformOp ? deg2rad(static_cast<const RotateTransformOperation*>(transformOp)->angle()) : 0];
        case TransformOperation::SCALE_X:
            return [NSNumber numberWithDouble:transformOp ? static_cast<const ScaleTransformOperation*>(transformOp)->x() : 1];
        case TransformOperation::SCALE_Y:
            return [NSNumber numberWithDouble:transformOp ? static_cast<const ScaleTransformOperation*>(transformOp)->y() : 1];
        case TransformOperation::SCALE_Z:
            return [NSNumber numberWithDouble:transformOp ? static_cast<const ScaleTransformOperation*>(transformOp)->z() : 1];
        case TransformOperation::TRANSLATE_X:
            return [NSNumber numberWithDouble:transformOp ? static_cast<const TranslateTransformOperation*>(transformOp)->x(size) : 0];
        case TransformOperation::TRANSLATE_Y:
            return [NSNumber numberWithDouble:transformOp ? static_cast<const TranslateTransformOperation*>(transformOp)->y(size) : 0];
        case TransformOperation::TRANSLATE_Z:
            return [NSNumber numberWithDouble:transformOp ? static_cast<const TranslateTransformOperation*>(transformOp)->z(size) : 0];
        case TransformOperation::SCALE:
        case TransformOperation::SCALE_3D:
            return [NSArray arrayWithObjects:
                        [NSNumber numberWithDouble:transformOp ? static_cast<const ScaleTransformOperation*>(transformOp)->x() : 1],
                        [NSNumber numberWithDouble:transformOp ? static_cast<const ScaleTransformOperation*>(transformOp)->y() : 1],
                        [NSNumber numberWithDouble:transformOp ? static_cast<const ScaleTransformOperation*>(transformOp)->z() : 1],
                        nil];
        case TransformOperation::TRANSLATE:
        case TransformOperation::TRANSLATE_3D:
            return [NSArray arrayWithObjects:
                        [NSNumber numberWithDouble:transformOp ? static_cast<const TranslateTransformOperation*>(transformOp)->x(size) : 0],
                        [NSNumber numberWithDouble:transformOp ? static_cast<const TranslateTransformOperation*>(transformOp)->y(size) : 0],
                        [NSNumber numberWithDouble:transformOp ? static_cast<const TranslateTransformOperation*>(transformOp)->z(size) : 0],
                        nil];
        case TransformOperation::SKEW_X:
        case TransformOperation::SKEW_Y:
        case TransformOperation::SKEW:
        case TransformOperation::MATRIX:
        case TransformOperation::ROTATE_3D:
        case TransformOperation::MATRIX_3D:
        case TransformOperation::PERSPECTIVE:
        case TransformOperation::IDENTITY:
        case TransformOperation::NONE: {
            TransformationMatrix transform;
            if (transformOp)
                transformOp->apply(transform, size);
            CATransform3D caTransform;
            copyTransform(caTransform, transform);
            return [NSValue valueWithCATransform3D:caTransform];
        }
    }
    
    return 0;
}

#if HAVE_MODERN_QUARTZCORE
static NSString *getValueFunctionNameForTransformOperation(TransformOperation::OperationType transformType)
{
    // Use literal strings to avoid link-time dependency on those symbols.
    switch (transformType) {
        case TransformOperation::ROTATE_X:
            return @"rotateX"; // kCAValueFunctionRotateX;
        case TransformOperation::ROTATE_Y:
            return @"rotateY"; // kCAValueFunctionRotateY;
        case TransformOperation::ROTATE:
            return @"rotateZ"; // kCAValueFunctionRotateZ;
        case TransformOperation::SCALE_X:
            return @"scaleX"; // kCAValueFunctionScaleX;
        case TransformOperation::SCALE_Y:
            return @"scaleY"; // kCAValueFunctionScaleY;
        case TransformOperation::SCALE_Z:
            return @"scaleZ"; // kCAValueFunctionScaleZ;
        case TransformOperation::TRANSLATE_X:
            return @"translateX"; // kCAValueFunctionTranslateX;
        case TransformOperation::TRANSLATE_Y:
            return @"translateY"; // kCAValueFunctionTranslateY;
        case TransformOperation::TRANSLATE_Z:
            return @"translateZ"; // kCAValueFunctionTranslateZ;
        case TransformOperation::SCALE:
        case TransformOperation::SCALE_3D:
            return @"scale"; // kCAValueFunctionScale;
        case TransformOperation::TRANSLATE:
        case TransformOperation::TRANSLATE_3D:
            return @"translate"; // kCAValueFunctionTranslate;
        default:
            return nil;
    }
}
#endif

static String propertyIdToString(AnimatedPropertyID property)
{
    switch (property) {
        case AnimatedPropertyWebkitTransform:
            return "transform";
        case AnimatedPropertyOpacity:
            return "opacity";
        case AnimatedPropertyBackgroundColor:
            return "backgroundColor";
        case AnimatedPropertyInvalid:
            ASSERT_NOT_REACHED();
    }
    ASSERT_NOT_REACHED();
    return "";
}

static String animationIdentifier(const String& animationName, int index)
{
    StringBuilder builder;

    builder.append(animationName);
    builder.append("_");
    builder.append(String::number(index));
    return builder.toString();
}

static CAMediaTimingFunction* getCAMediaTimingFunction(const TimingFunction* timingFunction)
{
    // By this point, timing functions can only be linear or cubic, not steps.
    ASSERT(!timingFunction->isStepsTimingFunction());
    if (timingFunction->isCubicBezierTimingFunction()) {
        const CubicBezierTimingFunction* ctf = static_cast<const CubicBezierTimingFunction*>(timingFunction);
        return [CAMediaTimingFunction functionWithControlPoints:static_cast<float>(ctf->x1()) :static_cast<float>(ctf->y1())
                                                               :static_cast<float>(ctf->x2()) :static_cast<float>(ctf->y2())];
    } else
        return [CAMediaTimingFunction functionWithName:@"linear"];
}

static void setLayerBorderColor(PlatformLayer* layer, const Color& color)
{
    CGColorRef borderColor = createCGColor(color);
    [layer setBorderColor:borderColor];
    CGColorRelease(borderColor);
}

static void clearBorderColor(PlatformLayer* layer)
{
    [layer setBorderColor:nil];
}

static void setLayerBackgroundColor(PlatformLayer* layer, const Color& color)
{
    CGColorRef bgColor = createCGColor(color);
    [layer setBackgroundColor:bgColor];
    CGColorRelease(bgColor);
}

static void clearLayerBackgroundColor(PlatformLayer* layer)
{
    [layer setBackgroundColor:0];
}

static void safeSetSublayers(CALayer* layer, NSArray* sublayers)
{
    // Workaround for <rdar://problem/7390716>: -[CALayer setSublayers:] crashes if sublayers is an empty array, or nil, under GC.
    if (objc_collectingEnabled() && ![sublayers count]) {
        while ([[layer sublayers] count])
            [[[layer sublayers] objectAtIndex:0] removeFromSuperlayer];
        return;
    }
    
    [layer setSublayers:sublayers];
}

static bool caValueFunctionSupported()
{
    static bool sHaveValueFunction = [CAPropertyAnimation instancesRespondToSelector:@selector(setValueFunction:)];
    return sHaveValueFunction;
}

static bool forceSoftwareAnimation()
{
    static bool forceSoftwareAnimation = [[NSUserDefaults standardUserDefaults] boolForKey:@"WebCoreForceSoftwareAnimation"];
    return forceSoftwareAnimation;
}

static NSDictionary* nullActionsDictionary()
{
    NSNull* nullValue = [NSNull null];
    NSDictionary* actions = [NSDictionary dictionaryWithObjectsAndKeys:
                             nullValue, @"anchorPoint",
                             nullValue, @"bounds",
                             nullValue, @"contents",
                             nullValue, @"contentsRect",
                             nullValue, @"opacity",
                             nullValue, @"position",
                             nullValue, @"shadowColor",
                             nullValue, @"sublayerTransform",
                             nullValue, @"sublayers",
                             nullValue, @"transform",
                             nullValue, @"zPosition",
                             nil];
    return actions;
}

static bool animationHasStepsTimingFunction(const KeyframeValueList& valueList, const Animation* anim)
{
    if (anim->timingFunction()->isStepsTimingFunction())
        return true;
    
    for (unsigned i = 0; i < valueList.size(); ++i) {
        const TimingFunction* timingFunction = valueList.at(i)->timingFunction();
        if (timingFunction && timingFunction->isStepsTimingFunction())
            return true;
    }

    return false;
}

PassOwnPtr<GraphicsLayer> GraphicsLayer::create(GraphicsLayerClient* client)
{
    return new GraphicsLayerCA(client);
}

GraphicsLayerCA::GraphicsLayerCA(GraphicsLayerClient* client)
    : GraphicsLayer(client)
    , m_contentsLayerPurpose(NoContentsLayer)
    , m_contentsLayerHasBackgroundColor(false)
    , m_uncommittedChanges(NoChange)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    m_layer.adoptNS([[WebLayer alloc] init]);
    [m_layer.get() setLayerOwner:this];

#if !HAVE_MODERN_QUARTZCORE
    setContentsOrientation(defaultContentsOrientation());
#endif

    updateDebugIndicators();

    m_animationDelegate.adoptNS([[WebAnimationDelegate alloc] init]);
    [m_animationDelegate.get() setLayer:this];
    
    END_BLOCK_OBJC_EXCEPTIONS
}

GraphicsLayerCA::~GraphicsLayerCA()
{
    // We release our references to the CALayers here, but do not actively unparent them,
    // since that will cause a commit and break our batched commit model. The layers will
    // get released when the rootmost modified GraphicsLayerCA rebuilds its child layers.
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    // Clean up the WK layer.
    if (m_layer) {
        WebLayer* layer = m_layer.get();
        [layer setLayerOwner:nil];
    }
    
    if (m_contentsLayer) {
        if ([m_contentsLayer.get() respondsToSelector:@selector(setLayerOwner:)])
            [(id)m_contentsLayer.get() setLayerOwner:nil];
    }
    
    // animationDidStart: can fire after this, so we need to clear out the layer on the delegate.
    [m_animationDelegate.get() setLayer:0];

    // Release the clone layers inside the exception-handling block.
    removeCloneLayers();
    
    END_BLOCK_OBJC_EXCEPTIONS
}

void GraphicsLayerCA::setName(const String& name)
{
    String longName = String::format("CALayer(%p) GraphicsLayer(%p) ", m_layer.get(), this) + name;
    GraphicsLayer::setName(longName);
    noteLayerPropertyChanged(NameChanged);
}

NativeLayer GraphicsLayerCA::nativeLayer() const
{
    return m_layer.get();
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
        static_cast<GraphicsLayerCA*>(m_parent)->noteSublayersChanged();
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
        static_cast<GraphicsLayerCA*>(m_replicatedLayer)->propagateLayerChangeToReplicas();
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
    noteLayerPropertyChanged(PositionChanged);
}

void GraphicsLayerCA::setAnchorPoint(const FloatPoint3D& point)
{
    if (point == m_anchorPoint)
        return;

    GraphicsLayer::setAnchorPoint(point);
    noteLayerPropertyChanged(AnchorPointChanged);
}

void GraphicsLayerCA::setSize(const FloatSize& size)
{
    if (size == m_size)
        return;

    GraphicsLayer::setSize(size);
    noteLayerPropertyChanged(SizeChanged);
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

void GraphicsLayerCA::moveOrCopyLayerAnimation(MoveOrCopy operation, const String& animationIdentifier, CALayer *fromLayer, CALayer *toLayer)
{
    NSString *animationID = animationIdentifier;
    CAAnimation *anim = [fromLayer animationForKey:animationID];
    if (!anim)
        return;

    switch (operation) {
        case Move:
            [anim retain];
            [fromLayer removeAnimationForKey:animationID];
            [toLayer addAnimation:anim forKey:animationID];
            [anim release];
            break;

        case Copy:
            [toLayer addAnimation:anim forKey:animationID];
            break;
    }
}

void GraphicsLayerCA::moveOrCopyAnimationsForProperty(MoveOrCopy operation, AnimatedPropertyID property, CALayer *fromLayer, CALayer *toLayer)
{
    // Look for running animations affecting this property.
    AnimationsMap::const_iterator end = m_runningAnimations.end();
    for (AnimationsMap::const_iterator it = m_runningAnimations.begin(); it != end; ++it) {
        const Vector<PropertyAnimationPair>& propertyAnimations = it->second;
        size_t numAnimations = propertyAnimations.size();
        for (size_t i = 0; i < numAnimations; ++i) {
            const PropertyAnimationPair& currPair = propertyAnimations[i];
            if (currPair.first == property)
                moveOrCopyLayerAnimation(operation, animationIdentifier(it->first, currPair.second), fromLayer, toLayer);
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
    noteLayerPropertyChanged(MasksToBoundsChanged);
}

void GraphicsLayerCA::setDrawsContent(bool drawsContent)
{
    if (drawsContent == m_drawsContent)
        return;

    GraphicsLayer::setDrawsContent(drawsContent);
    noteLayerPropertyChanged(DrawsContentChanged);
}

void GraphicsLayerCA::setBackgroundColor(const Color& color)
{
    if (m_backgroundColorSet && m_backgroundColor == color)
        return;

    GraphicsLayer::setBackgroundColor(color);

    m_contentsLayerHasBackgroundColor = true;
    noteLayerPropertyChanged(BackgroundColorChanged);
}

void GraphicsLayerCA::clearBackgroundColor()
{
    if (!m_backgroundColorSet)
        return;

    GraphicsLayer::clearBackgroundColor();
    m_contentsLayerHasBackgroundColor = false;
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
    float clampedOpacity = max(0.0f, min(opacity, 1.0f));

    if (clampedOpacity == m_opacity)
        return;

    GraphicsLayer::setOpacity(clampedOpacity);
    noteLayerPropertyChanged(OpacityChanged);
}

void GraphicsLayerCA::setNeedsDisplay()
{
    FloatRect hugeRect(-numeric_limits<float>::max() / 2, -numeric_limits<float>::max() / 2,
                       numeric_limits<float>::max(), numeric_limits<float>::max());

    setNeedsDisplayInRect(hugeRect);
}

void GraphicsLayerCA::setNeedsDisplayInRect(const FloatRect& rect)
{
    if (!drawsContent())
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
}

void GraphicsLayerCA::setContentsNeedsDisplay()
{
    noteLayerPropertyChanged(ContentsNeedsDisplay);
}

void GraphicsLayerCA::setContentsRect(const IntRect& rect)
{
    if (rect == m_contentsRect)
        return;

    GraphicsLayer::setContentsRect(rect);
    noteLayerPropertyChanged(ContentsRectChanged);
}

bool GraphicsLayerCA::addAnimation(const KeyframeValueList& valueList, const IntSize& boxSize, const Animation* anim, const String& animationName, double timeOffset)
{
    ASSERT(!animationName.isEmpty());

    if (forceSoftwareAnimation() || !anim || anim->isEmptyOrZeroDuration() || valueList.size() < 2)
        return false;

#if !HAVE_MODERN_QUARTZCORE
    // Older versions of QuartzCore do not handle opacity in transform layers properly, so we will
    // always do software animation in that case.
    if (valueList.property() == AnimatedPropertyOpacity)
        return false;
#endif

    // CoreAnimation does not handle the steps() timing function. Fall back
    // to software animation in that case.
    if (animationHasStepsTimingFunction(valueList, anim))
        return false;

    bool createdAnimations = false;
    if (valueList.property() == AnimatedPropertyWebkitTransform)
        createdAnimations = createTransformAnimationsFromKeyframes(valueList, anim, animationName, timeOffset, boxSize);
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
        AnimationProcessingAction& processingInfo = it->second;
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

void GraphicsLayerCA::setContentsToImage(Image* image)
{
    if (image) {
        CGImageRef newImage = image->nativeImageForCurrentFrame();
        if (!newImage)
            return;

        // Check to see if the image changed; we have to do this because the call to
        // CGImageCreateCopyWithColorSpace() below can create a new image every time.
        if (m_uncorrectedContentsImage && m_uncorrectedContentsImage.get() == newImage)
            return;
        
        m_uncorrectedContentsImage = newImage;
        m_pendingContentsImage = newImage;
        CGColorSpaceRef colorSpace = CGImageGetColorSpace(m_pendingContentsImage.get());

        static CGColorSpaceRef deviceRGB = CGColorSpaceCreateDeviceRGB();
        if (colorSpace && CFEqual(colorSpace, deviceRGB)) {
            // CoreGraphics renders images tagged with DeviceRGB using the color space of the main display. When we hand such
            // images to CA we need to tag them similarly so CA rendering matches CG rendering.
            static CGColorSpaceRef genericRGB = CGDisplayCopyColorSpace(kCGDirectMainDisplay);
            m_pendingContentsImage.adoptCF(CGImageCreateCopyWithColorSpace(m_pendingContentsImage.get(), genericRGB));
        }
        m_contentsLayerPurpose = ContentsLayerForImage;
        if (!m_contentsLayer)
            noteSublayersChanged();
    } else {
        m_uncorrectedContentsImage = 0;
        m_pendingContentsImage = 0;
        m_contentsLayerPurpose = NoContentsLayer;
        if (m_contentsLayer)
            noteSublayersChanged();
    }

    noteLayerPropertyChanged(ContentsImageChanged);
}

void GraphicsLayerCA::setContentsToMedia(PlatformLayer* mediaLayer)
{
    if (mediaLayer == m_contentsLayer)
        return;

    m_contentsLayer = mediaLayer;
    m_contentsLayerPurpose = mediaLayer ? ContentsLayerForMedia : NoContentsLayer;

    noteSublayersChanged();
    noteLayerPropertyChanged(ContentsMediaLayerChanged);
}

void GraphicsLayerCA::didDisplay(PlatformLayer* layer)
{
    CALayer* sourceLayer;
    LayerMap* layerCloneMap;

    if (layer == m_layer) {
        sourceLayer = m_layer.get();
        layerCloneMap = m_layerClones.get();
    } else if (layer == m_contentsLayer) {
        sourceLayer = m_contentsLayer.get();
        layerCloneMap = m_contentsLayerClones.get();
    } else
        return;

    if (layerCloneMap) {
        LayerMap::const_iterator end = layerCloneMap->end();
        for (LayerMap::const_iterator it = layerCloneMap->begin(); it != end; ++it) {
            CALayer *currClone = it->second.get();
            if (!currClone)
                continue;

            if ([currClone contents] != [sourceLayer contents])
                [currClone setContents:[sourceLayer contents]];
            else
                [currClone setContentsChanged];
        }
    }
}

void GraphicsLayerCA::syncCompositingState()
{
    recursiveCommitChanges();
}

void GraphicsLayerCA::syncCompositingStateForThisLayerOnly()
{
    commitLayerChangesBeforeSublayers();
    commitLayerChangesAfterSublayers();
}

void GraphicsLayerCA::recursiveCommitChanges()
{
    commitLayerChangesBeforeSublayers();

    if (m_maskLayer)
        static_cast<GraphicsLayerCA*>(m_maskLayer)->commitLayerChangesBeforeSublayers();

    const Vector<GraphicsLayer*>& childLayers = children();
    size_t numChildren = childLayers.size();
    for (size_t i = 0; i < numChildren; ++i) {
        GraphicsLayerCA* curChild = static_cast<GraphicsLayerCA*>(childLayers[i]);
        curChild->recursiveCommitChanges();
    }

    if (m_replicaLayer)
        static_cast<GraphicsLayerCA*>(m_replicaLayer)->recursiveCommitChanges();

    if (m_maskLayer)
        static_cast<GraphicsLayerCA*>(m_maskLayer)->commitLayerChangesAfterSublayers();

    commitLayerChangesAfterSublayers();
}

void GraphicsLayerCA::commitLayerChangesBeforeSublayers()
{
    if (!m_uncommittedChanges)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    // Need to handle Preserves3DChanged first, because it affects which layers subsequent properties are applied to
    if (m_uncommittedChanges & (Preserves3DChanged | ReplicatedLayerChanged))
        updateStructuralLayer();

    if (m_uncommittedChanges & NameChanged)
        updateLayerNames();

    if (m_uncommittedChanges & ContentsImageChanged) // Needs to happen before ChildrenChanged
        updateContentsImage();
        
    if (m_uncommittedChanges & ContentsMediaLayerChanged) // Needs to happen before ChildrenChanged
        updateContentsMediaLayer();
    
    if (m_uncommittedChanges & ContentsCanvasLayerChanged) // Needs to happen before ChildrenChanged
        updateContentsCanvasLayer();
    
    if (m_uncommittedChanges & BackgroundColorChanged)  // Needs to happen before ChildrenChanged, and after updating image or video
        updateLayerBackgroundColor();

    if (m_uncommittedChanges & ChildrenChanged)
        updateSublayerList();

    if (m_uncommittedChanges & PositionChanged)
        updateLayerPosition();
    
    if (m_uncommittedChanges & AnchorPointChanged)
        updateAnchorPoint();
    
    if (m_uncommittedChanges & SizeChanged)
        updateLayerSize();

    if (m_uncommittedChanges & TransformChanged)
        updateTransform();

    if (m_uncommittedChanges & ChildrenTransformChanged)
        updateChildrenTransform();
    
    if (m_uncommittedChanges & MasksToBoundsChanged)
        updateMasksToBounds();
    
    if (m_uncommittedChanges & DrawsContentChanged)
        updateLayerDrawsContent();

    if (m_uncommittedChanges & ContentsOpaqueChanged)
        updateContentsOpaque();

    if (m_uncommittedChanges & BackfaceVisibilityChanged)
        updateBackfaceVisibility();

    if (m_uncommittedChanges & OpacityChanged)
        updateOpacityOnLayer();
    
    if (m_uncommittedChanges & AnimationChanged)
        updateLayerAnimations();
    
    if (m_uncommittedChanges & DirtyRectsChanged)
        repaintLayerDirtyRects();
    
    if (m_uncommittedChanges & ContentsRectChanged)
        updateContentsRect();

    if (m_uncommittedChanges & MaskLayerChanged)
        updateMaskLayer();

    if (m_uncommittedChanges & ContentsNeedsDisplay)
        updateContentsNeedsDisplay();

    END_BLOCK_OBJC_EXCEPTIONS
}

void GraphicsLayerCA::commitLayerChangesAfterSublayers()
{
    if (!m_uncommittedChanges)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    if (m_uncommittedChanges & ReplicatedLayerChanged)
        updateReplicatedLayers();

    m_uncommittedChanges = NoChange;
    END_BLOCK_OBJC_EXCEPTIONS
}

void GraphicsLayerCA::updateLayerNames()
{
    switch (structuralLayerPurpose()) {
        case StructuralLayerForPreserves3D:
            [m_structuralLayer.get() setName:("Transform layer " + name())];
            break;
        case StructuralLayerForReplicaFlattening:
            [m_structuralLayer.get() setName:("Replica flattening layer " + name())];
            break;
        case NoStructuralLayer:
            break;
    }
    [m_layer.get() setName:name()];
}

void GraphicsLayerCA::updateSublayerList()
{
    NSMutableArray* newSublayers = nil;

    const Vector<GraphicsLayer*>& childLayers = children();

    if (m_structuralLayer || m_contentsLayer || childLayers.size() > 0) {
        newSublayers = [[NSMutableArray alloc] init];

        if (m_structuralLayer) {
            // Add the replica layer first.
            if (m_replicaLayer)
                [newSublayers addObject:static_cast<GraphicsLayerCA*>(m_replicaLayer)->primaryLayer()];
            // Add the primary layer. Even if we have negative z-order children, the primary layer always comes behind.
            [newSublayers addObject:m_layer.get()];
        } else if (m_contentsLayer) {
            // FIXME: add the contents layer in the correct order with negative z-order children.
            // This does not cause visible rendering issues because currently contents layers are only used
            // for replaced elements that don't have children.
            [newSublayers addObject:m_contentsLayer.get()];
        }
        
        size_t numChildren = childLayers.size();
        for (size_t i = 0; i < numChildren; ++i) {
            GraphicsLayerCA* curChild = static_cast<GraphicsLayerCA*>(childLayers[i]);
            CALayer *childLayer = curChild->layerForSuperlayer();
            [newSublayers addObject:childLayer];
        }

        [newSublayers makeObjectsPerformSelector:@selector(removeFromSuperlayer)];
    }

    if (m_structuralLayer) {
        safeSetSublayers(m_structuralLayer.get(), newSublayers);

        if (m_contentsLayer) {
            // If we have a transform layer, then the contents layer is parented in the 
            // primary layer (which is itself a child of the transform layer).
            safeSetSublayers(m_layer.get(), nil);
            [m_layer.get() addSublayer:m_contentsLayer.get()];
        }
    } else
        safeSetSublayers(m_layer.get(), newSublayers);

    [newSublayers release];
}

void GraphicsLayerCA::updateLayerPosition()
{
    FloatSize usedSize = m_usingTiledLayer ? constrainedSize() : m_size;

    // Position is offset on the layer by the layer anchor point.
    CGPoint posPoint = CGPointMake(m_position.x() + m_anchorPoint.x() * usedSize.width(),
                                   m_position.y() + m_anchorPoint.y() * usedSize.height());
    
    [primaryLayer() setPosition:posPoint];

    if (LayerMap* layerCloneMap = primaryLayerClones()) {
        LayerMap::const_iterator end = layerCloneMap->end();
        for (LayerMap::const_iterator it = layerCloneMap->begin(); it != end; ++it) {
            CGPoint clonePosition = posPoint;
            if (m_replicaLayer && isReplicatedRootClone(it->first)) {
                // Maintain the special-case position for the root of a clone subtree,
                // which we set up in replicatedLayerRoot().
                clonePosition = positionForCloneRootLayer();
            }
            CALayer *currLayer = it->second.get();
            [currLayer setPosition:clonePosition];
        }
    }
}

void GraphicsLayerCA::updateLayerSize()
{
    CGRect rect = CGRectMake(0, 0, m_size.width(), m_size.height());
    if (m_structuralLayer) {
        [m_structuralLayer.get() setBounds:rect];
        
        if (LayerMap* layerCloneMap = m_structuralLayerClones.get()) {
            LayerMap::const_iterator end = layerCloneMap->end();
            for (LayerMap::const_iterator it = layerCloneMap->begin(); it != end; ++it)
                [it->second.get() setBounds:rect];
        }

        // The anchor of the contents layer is always at 0.5, 0.5, so the position is center-relative.
        CGPoint centerPoint = CGPointMake(m_size.width() / 2.0f, m_size.height() / 2.0f);
        [m_layer.get() setPosition:centerPoint];

        if (LayerMap* layerCloneMap = m_layerClones.get()) {
            LayerMap::const_iterator end = layerCloneMap->end();
            for (LayerMap::const_iterator it = layerCloneMap->begin(); it != end; ++it)
                [it->second.get() setPosition:centerPoint];
        }
    }
    
    bool needTiledLayer = requiresTiledLayer(m_size);
    if (needTiledLayer != m_usingTiledLayer)
        swapFromOrToTiledLayer(needTiledLayer);
    
    if (m_usingTiledLayer) {
        FloatSize sizeToUse = constrainedSize();
        rect = CGRectMake(0, 0, sizeToUse.width(), sizeToUse.height());
    }
    
    [m_layer.get() setBounds:rect];
    if (LayerMap* layerCloneMap = m_layerClones.get()) {
        LayerMap::const_iterator end = layerCloneMap->end();
        for (LayerMap::const_iterator it = layerCloneMap->begin(); it != end; ++it)
            [it->second.get() setBounds:rect];
    }
    
    // Contents transform may depend on height.
    updateContentsTransform();

    // Note that we don't resize m_contentsLayer. It's up the caller to do that.

    // if we've changed the bounds, we need to recalculate the position
    // of the layer, taking anchor point into account.
    updateLayerPosition();
}

void GraphicsLayerCA::updateAnchorPoint()
{
    [primaryLayer() setAnchorPoint:FloatPoint(m_anchorPoint.x(), m_anchorPoint.y())];
#if HAVE_MODERN_QUARTZCORE
    [primaryLayer() setAnchorPointZ:m_anchorPoint.z()];
#endif

    if (LayerMap* layerCloneMap = primaryLayerClones()) {
        LayerMap::const_iterator end = layerCloneMap->end();
        for (LayerMap::const_iterator it = layerCloneMap->begin(); it != end; ++it) {   
            CALayer *currLayer = it->second.get();
            [currLayer setAnchorPoint:FloatPoint(m_anchorPoint.x(), m_anchorPoint.y())];
#if HAVE_MODERN_QUARTZCORE
            [currLayer setAnchorPointZ:m_anchorPoint.z()];
#endif
        }
    }

    updateLayerPosition();
}

void GraphicsLayerCA::updateTransform()
{
    CATransform3D transform;
    copyTransform(transform, m_transform);
    [primaryLayer() setTransform:transform];

    if (LayerMap* layerCloneMap = primaryLayerClones()) {
        LayerMap::const_iterator end = layerCloneMap->end();
        for (LayerMap::const_iterator it = layerCloneMap->begin(); it != end; ++it) {
            CALayer *currLayer = it->second.get();
            if (m_replicaLayer && isReplicatedRootClone(it->first)) {
                // Maintain the special-case transform for the root of a clone subtree,
                // which we set up in replicatedLayerRoot().
                [currLayer setTransform:CATransform3DIdentity];
            } else
                [currLayer setTransform:transform];
        }
    }
}

void GraphicsLayerCA::updateChildrenTransform()
{
    CATransform3D transform;
    copyTransform(transform, m_childrenTransform);
    [primaryLayer() setSublayerTransform:transform];

    if (LayerMap* layerCloneMap = primaryLayerClones()) {
        LayerMap::const_iterator end = layerCloneMap->end();
        for (LayerMap::const_iterator it = layerCloneMap->begin(); it != end; ++it) {
            CALayer *currLayer = it->second.get();
            [currLayer setSublayerTransform:transform];
        }
    }
}

void GraphicsLayerCA::updateMasksToBounds()
{
    [m_layer.get() setMasksToBounds:m_masksToBounds];

    if (LayerMap* layerCloneMap = m_layerClones.get()) {
        LayerMap::const_iterator end = layerCloneMap->end();
        for (LayerMap::const_iterator it = layerCloneMap->begin(); it != end; ++it) {
            CALayer *currLayer = it->second.get();
            [currLayer setMasksToBounds:m_masksToBounds];
        }
    }

    updateDebugIndicators();
}

void GraphicsLayerCA::updateContentsOpaque()
{
    [m_layer.get() setOpaque:m_contentsOpaque];

    if (LayerMap* layerCloneMap = m_layerClones.get()) {
        LayerMap::const_iterator end = layerCloneMap->end();
        for (LayerMap::const_iterator it = layerCloneMap->begin(); it != end; ++it) {
            CALayer *currLayer = it->second.get();
            [currLayer setOpaque:m_contentsOpaque];
        }
    }
}

void GraphicsLayerCA::updateBackfaceVisibility()
{
    if (m_structuralLayer && structuralLayerPurpose() == StructuralLayerForReplicaFlattening) {
        [m_structuralLayer.get() setDoubleSided:m_backfaceVisibility];

        if (LayerMap* layerCloneMap = m_structuralLayerClones.get()) {
            LayerMap::const_iterator end = layerCloneMap->end();
            for (LayerMap::const_iterator it = layerCloneMap->begin(); it != end; ++it) {
                CALayer *currLayer = it->second.get();
                [currLayer setDoubleSided:m_backfaceVisibility];
            }
        }
    }

    [m_layer.get() setDoubleSided:m_backfaceVisibility];

    if (LayerMap* layerCloneMap = m_layerClones.get()) {
        LayerMap::const_iterator end = layerCloneMap->end();
        for (LayerMap::const_iterator it = layerCloneMap->begin(); it != end; ++it) {
            CALayer *currLayer = it->second.get();
            [currLayer setDoubleSided:m_backfaceVisibility];
        }
    }
}

void GraphicsLayerCA::updateStructuralLayer()
{
    ensureStructuralLayer(structuralLayerPurpose());
}

void GraphicsLayerCA::ensureStructuralLayer(StructuralLayerPurpose purpose)
{
    if (purpose == NoStructuralLayer) {
        if (m_structuralLayer) {
            // Replace the transformLayer in the parent with this layer.
            [m_layer.get() removeFromSuperlayer];
            [[m_structuralLayer.get() superlayer] replaceSublayer:m_structuralLayer.get() with:m_layer.get()];

            moveOrCopyAnimationsForProperty(Move, AnimatedPropertyWebkitTransform, m_structuralLayer.get(), m_layer.get());
            moveOrCopyAnimationsForProperty(Move, AnimatedPropertyOpacity, m_structuralLayer.get(), m_layer.get());

            // Release the structural layer.
            m_structuralLayer = 0;

            // Update the properties of m_layer now that we no longer have a structural layer.
            updateLayerPosition();
            updateLayerSize();
            updateAnchorPoint();
            updateTransform();
            updateChildrenTransform();

            updateSublayerList();
            updateOpacityOnLayer();
        }
        return;
    }
    
    bool structuralLayerChanged = false;
    
    if (purpose == StructuralLayerForPreserves3D) {
        Class transformLayerClass = NSClassFromString(@"CATransformLayer");
        if (!transformLayerClass)
            return;

        if (m_structuralLayer && ![m_structuralLayer.get() isKindOfClass:transformLayerClass])
            m_structuralLayer = 0;
        
        if (!m_structuralLayer) {
            m_structuralLayer.adoptNS([[transformLayerClass alloc] init]);
            structuralLayerChanged = true;
        }
    } else {
        if (m_structuralLayer && ![m_structuralLayer.get() isMemberOfClass:[CALayer self]])
            m_structuralLayer = 0;

        if (!m_structuralLayer) {
            m_structuralLayer.adoptNS([[CALayer alloc] init]);
            structuralLayerChanged = true;
        }
    }
    
    if (!structuralLayerChanged)
        return;
    
    // Turn off default animations.
    [m_structuralLayer.get() setStyle:[NSDictionary dictionaryWithObject:nullActionsDictionary() forKey:@"actions"]];

    updateLayerNames();

    // Update the properties of the structural layer.
    updateLayerPosition();
    updateLayerSize();
    updateAnchorPoint();
    updateTransform();
    updateChildrenTransform();
    updateBackfaceVisibility();
    
    // Set properties of m_layer to their default values, since these are expressed on on the structural layer.
    CGPoint point = CGPointMake(m_size.width() / 2.0f, m_size.height() / 2.0f);
    [m_layer.get() setPosition:point];
    [m_layer.get() setAnchorPoint:CGPointMake(0.5f, 0.5f)];
    [m_layer.get() setTransform:CATransform3DIdentity];
    [m_layer.get() setOpacity:1];

    // Move this layer to be a child of the transform layer.
    [[m_layer.get() superlayer] replaceSublayer:m_layer.get() with:m_structuralLayer.get()];
    [m_structuralLayer.get() addSublayer:m_layer.get()];

    moveOrCopyAnimationsForProperty(Move, AnimatedPropertyWebkitTransform, m_layer.get(), m_structuralLayer.get());
    moveOrCopyAnimationsForProperty(Move, AnimatedPropertyOpacity, m_layer.get(), m_structuralLayer.get());
    
    updateSublayerList();
    updateOpacityOnLayer();
}

GraphicsLayerCA::StructuralLayerPurpose GraphicsLayerCA::structuralLayerPurpose() const
{
    if (preserves3D())
        return StructuralLayerForPreserves3D;
    
    if (isReplicated())
        return StructuralLayerForReplicaFlattening;
    
    return NoStructuralLayer;
}

void GraphicsLayerCA::updateLayerDrawsContent()
{
    bool needTiledLayer = requiresTiledLayer(m_size);
    if (needTiledLayer != m_usingTiledLayer)
        swapFromOrToTiledLayer(needTiledLayer);

    if (m_drawsContent)
        [m_layer.get() setNeedsDisplay];
    else
        [m_layer.get() setContents:nil];

    updateDebugIndicators();
}

void GraphicsLayerCA::updateLayerBackgroundColor()
{
    if (!m_contentsLayer)
        return;

    // We never create the contents layer just for background color yet.
    if (m_backgroundColorSet)
        setLayerBackgroundColor(m_contentsLayer.get(), m_backgroundColor);
    else
        clearLayerBackgroundColor(m_contentsLayer.get());
}

void GraphicsLayerCA::updateContentsImage()
{
    if (m_pendingContentsImage) {
        if (!m_contentsLayer.get()) {
            WebLayer* imageLayer = [WebLayer layer];
#ifndef NDEBUG
            [imageLayer setName:@"Image Layer"];
#endif
            setupContentsLayer(imageLayer);
            m_contentsLayer.adoptNS([imageLayer retain]);
            // m_contentsLayer will be parented by updateSublayerList
        }

        // FIXME: maybe only do trilinear if the image is being scaled down,
        // but then what if the layer size changes?
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
        [m_contentsLayer.get() setMinificationFilter:kCAFilterTrilinear];
#endif
        [m_contentsLayer.get() setContents:(id)m_pendingContentsImage.get()];
        m_pendingContentsImage = 0;

        if (m_contentsLayerClones) {
            LayerMap::const_iterator end = m_contentsLayerClones->end();
            for (LayerMap::const_iterator it = m_contentsLayerClones->begin(); it != end; ++it)
                [it->second.get() setContents:[m_contentsLayer.get() contents]];
        }
        
        updateContentsRect();
    } else {
        // No image.
        // m_contentsLayer will be removed via updateSublayerList.
        m_contentsLayer = 0;
    }
}

void GraphicsLayerCA::updateContentsMediaLayer()
{
    // Video layer was set as m_contentsLayer, and will get parented in updateSublayerList().
    if (m_contentsLayer) {
        setupContentsLayer(m_contentsLayer.get());
        updateContentsRect();
    }
}

void GraphicsLayerCA::updateContentsCanvasLayer()
{
    // CanvasLayer was set as m_contentsLayer, and will get parented in updateSublayerList().
    if (m_contentsLayer) {
        setupContentsLayer(m_contentsLayer.get());
        [m_contentsLayer.get() setNeedsDisplay];
        updateContentsRect();
    }
}

void GraphicsLayerCA::updateContentsRect()
{
    if (!m_contentsLayer)
        return;

    CGPoint point = CGPointMake(m_contentsRect.x(),
                                m_contentsRect.y());
    CGRect rect = CGRectMake(0.0f,
                             0.0f,
                             m_contentsRect.width(),
                             m_contentsRect.height());

    [m_contentsLayer.get() setPosition:point];
    [m_contentsLayer.get() setBounds:rect];

    if (m_contentsLayerClones) {
        LayerMap::const_iterator end = m_contentsLayerClones->end();
        for (LayerMap::const_iterator it = m_contentsLayerClones->begin(); it != end; ++it) {
            CALayer *currLayer = it->second.get();
            [currLayer setPosition:point];
            [currLayer setBounds:rect];
        }
    }
}

void GraphicsLayerCA::updateMaskLayer()
{
    CALayer *maskCALayer = m_maskLayer ? m_maskLayer->platformLayer() : 0;
    [m_layer.get() setMask:maskCALayer];

    LayerMap* maskLayerCloneMap = m_maskLayer ? static_cast<GraphicsLayerCA*>(m_maskLayer)->primaryLayerClones() : 0;
    
    if (LayerMap* layerCloneMap = m_layerClones.get()) {
        LayerMap::const_iterator end = layerCloneMap->end();
        for (LayerMap::const_iterator it = layerCloneMap->begin(); it != end; ++it) {
            CALayer *currLayer = it->second.get();
            
            CALayer *maskClone = maskLayerCloneMap ? maskLayerCloneMap->get(it->first).get() : 0;
            [currLayer setMask:maskClone];
        }
    }
}

void GraphicsLayerCA::updateReplicatedLayers()
{
    // Clone the descendants of the replicated layer, and parent under us.
    ReplicaState replicaState(ReplicaState::ReplicaBranch);

    CALayer *replicaRoot = replicatedLayerRoot(replicaState);
    if (!replicaRoot)
        return;

    if (m_structuralLayer)
        [m_structuralLayer.get() insertSublayer:replicaRoot atIndex:0];
    else
        [m_layer.get() insertSublayer:replicaRoot atIndex:0];
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

CALayer *GraphicsLayerCA::replicatedLayerRoot(ReplicaState& replicaState)
{
    // Limit replica nesting, to avoid 2^N explosion of replica layers.
    if (!m_replicatedLayer || replicaState.replicaDepth() == ReplicaState::maxReplicaDepth)
        return nil;

    GraphicsLayerCA* replicatedLayer = static_cast<GraphicsLayerCA*>(m_replicatedLayer);
    
    CALayer *clonedLayerRoot = replicatedLayer->fetchCloneLayers(this, replicaState, RootCloneLevel);
    FloatPoint cloneRootPosition = replicatedLayer->positionForCloneRootLayer();

    // Replica root has no offset or transform
    [clonedLayerRoot setPosition:cloneRootPosition];
    [clonedLayerRoot setTransform:CATransform3DIdentity];

    return clonedLayerRoot;
}

void GraphicsLayerCA::updateLayerAnimations()
{
    if (m_animationsToProcess.size()) {
        AnimationsToProcessMap::const_iterator end = m_animationsToProcess.end();
        for (AnimationsToProcessMap::const_iterator it = m_animationsToProcess.begin(); it != end; ++it) {
            const String& currAnimationName = it->first;
            AnimationsMap::iterator animationIt = m_runningAnimations.find(currAnimationName);
            if (animationIt == m_runningAnimations.end())
                continue;

            const AnimationProcessingAction& processingInfo = it->second;
            const Vector<PropertyAnimationPair>& animations = animationIt->second;
            for (size_t i = 0; i < animations.size(); ++i) {
                const PropertyAnimationPair& currPair = animations[i];
                switch (processingInfo.action) {
                    case Remove:
                        removeCAAnimationFromLayer(static_cast<AnimatedPropertyID>(currPair.first), currAnimationName, currPair.second);
                        break;
                    case Pause:
                        pauseCAAnimationOnLayer(static_cast<AnimatedPropertyID>(currPair.first), currAnimationName, currPair.second, processingInfo.timeOffset);
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
            setCAAnimationOnLayer(pendingAnimation.m_animation.get(), pendingAnimation.m_property, pendingAnimation.m_name, pendingAnimation.m_index, pendingAnimation.m_timeOffset);
            
            AnimationsMap::iterator it = m_runningAnimations.find(pendingAnimation.m_name);
            if (it == m_runningAnimations.end()) {
                Vector<PropertyAnimationPair> firstPair;
                firstPair.append(PropertyAnimationPair(pendingAnimation.m_property, pendingAnimation.m_index));
                m_runningAnimations.add(pendingAnimation.m_name, firstPair);
            } else {
                Vector<PropertyAnimationPair>& animPairs = it->second;
                animPairs.append(PropertyAnimationPair(pendingAnimation.m_property, pendingAnimation.m_index));
            }
        }
        
        m_uncomittedAnimations.clear();
    }
}

void GraphicsLayerCA::setCAAnimationOnLayer(CAPropertyAnimation* caAnim, AnimatedPropertyID property, const String& animationName, int index, double timeOffset)
{
    PlatformLayer* layer = animatedLayer(property);

    [caAnim setTimeOffset:timeOffset];
    
    String animationID = animationIdentifier(animationName, index);
    
    [layer removeAnimationForKey:animationID];
    [layer addAnimation:caAnim forKey:animationID];

    if (LayerMap* layerCloneMap = animatedLayerClones(property)) {
        LayerMap::const_iterator end = layerCloneMap->end();
        for (LayerMap::const_iterator it = layerCloneMap->begin(); it != end; ++it) {
            // Skip immediate replicas, since they move with the original.
            if (m_replicaLayer && isReplicatedRootClone(it->first))
                continue;
            CALayer *currLayer = it->second.get();
            [currLayer removeAnimationForKey:animationID];
            [currLayer addAnimation:caAnim forKey:animationID];
        }
    }
}

// Workaround for <rdar://problem/7311367>
static void bug7311367Workaround(CALayer* transformLayer, const TransformationMatrix& transform)
{
    if (!transformLayer)
        return;

    CATransform3D caTransform;
    copyTransform(caTransform, transform);
    caTransform.m41 += 1;
    [transformLayer setTransform:caTransform];

    caTransform.m41 -= 1;
    [transformLayer setTransform:caTransform];
}

bool GraphicsLayerCA::removeCAAnimationFromLayer(AnimatedPropertyID property, const String& animationName, int index)
{
    PlatformLayer* layer = animatedLayer(property);

    String animationID = animationIdentifier(animationName, index);

    if (![layer animationForKey:animationID])
        return false;
    
    [layer removeAnimationForKey:animationID];
    bug7311367Workaround(m_structuralLayer.get(), m_transform);

    if (LayerMap* layerCloneMap = animatedLayerClones(property)) {
        LayerMap::const_iterator end = layerCloneMap->end();
        for (LayerMap::const_iterator it = layerCloneMap->begin(); it != end; ++it) {
            // Skip immediate replicas, since they move with the original.
            if (m_replicaLayer && isReplicatedRootClone(it->first))
                continue;

            CALayer *currLayer = it->second.get();
            [currLayer removeAnimationForKey:animationID];
        }
    }
    return true;
}

static void copyAnimationProperties(CAPropertyAnimation* from, CAPropertyAnimation* to)
{
    [to setBeginTime:[from beginTime]];
    [to setDuration:[from duration]];
    [to setRepeatCount:[from repeatCount]];
    [to setAutoreverses:[from autoreverses]];
    [to setFillMode:[from fillMode]];
    [to setRemovedOnCompletion:[from isRemovedOnCompletion]];
    [to setAdditive:[from isAdditive]];
    [to setTimingFunction:[from timingFunction]];

#if HAVE_MODERN_QUARTZCORE
    [to setValueFunction:[from valueFunction]];
#endif
}

void GraphicsLayerCA::pauseCAAnimationOnLayer(AnimatedPropertyID property, const String& animationName, int index, double timeOffset)
{
    PlatformLayer* layer = animatedLayer(property);

    String animationID = animationIdentifier(animationName, index);

    CAAnimation *caAnim = [layer animationForKey:animationID];
    if (!caAnim)
        return;

    // Animations on the layer are immutable, so we have to clone and modify.
    CAPropertyAnimation* pausedAnim = nil;
    if ([caAnim isKindOfClass:[CAKeyframeAnimation class]]) {
        CAKeyframeAnimation* existingKeyframeAnim = static_cast<CAKeyframeAnimation*>(caAnim);
        CAKeyframeAnimation* newAnim = [CAKeyframeAnimation animationWithKeyPath:[existingKeyframeAnim keyPath]];
        copyAnimationProperties(existingKeyframeAnim, newAnim);
        [newAnim setValues:[existingKeyframeAnim values]];
        [newAnim setKeyTimes:[existingKeyframeAnim keyTimes]];
        [newAnim setTimingFunctions:[existingKeyframeAnim timingFunctions]];
        pausedAnim = newAnim;
    } else if ([caAnim isKindOfClass:[CABasicAnimation class]]) {
        CABasicAnimation* existingPropertyAnim = static_cast<CABasicAnimation*>(caAnim);
        CABasicAnimation* newAnim = [CABasicAnimation animationWithKeyPath:[existingPropertyAnim keyPath]];
        copyAnimationProperties(existingPropertyAnim, newAnim);
        [newAnim setFromValue:[existingPropertyAnim fromValue]];
        [newAnim setToValue:[existingPropertyAnim toValue]];
        pausedAnim = newAnim;
    }

    // pausedAnim has the beginTime of caAnim already.
    [pausedAnim setSpeed:0];
    [pausedAnim setTimeOffset:timeOffset];
    
    [layer addAnimation:pausedAnim forKey:animationID];  // This will replace the running animation.

    // Pause the animations on the clones too.
    if (LayerMap* layerCloneMap = animatedLayerClones(property)) {
        LayerMap::const_iterator end = layerCloneMap->end();
        for (LayerMap::const_iterator it = layerCloneMap->begin(); it != end; ++it) {
            // Skip immediate replicas, since they move with the original.
            if (m_replicaLayer && isReplicatedRootClone(it->first))
                continue;
            CALayer *currLayer = it->second.get();
            [currLayer addAnimation:pausedAnim forKey:animationID];
        }
    }
}

void GraphicsLayerCA::setContentsToCanvas(PlatformLayer* canvasLayer)
{
    if (canvasLayer == m_contentsLayer)
        return;
        
    m_contentsLayer = canvasLayer;
    if (m_contentsLayer && [m_contentsLayer.get() respondsToSelector:@selector(setLayerOwner:)])
        [(id)m_contentsLayer.get() setLayerOwner:this];
    
    m_contentsLayerPurpose = canvasLayer ? ContentsLayerForCanvas : NoContentsLayer;

    noteSublayersChanged();
    noteLayerPropertyChanged(ContentsCanvasLayerChanged);
}
    
void GraphicsLayerCA::repaintLayerDirtyRects()
{
    if (!m_dirtyRects.size())
        return;

    for (size_t i = 0; i < m_dirtyRects.size(); ++i)
        [m_layer.get() setNeedsDisplayInRect:m_dirtyRects[i]];
    
    m_dirtyRects.clear();
}

void GraphicsLayerCA::updateContentsNeedsDisplay()
{
    if (m_contentsLayer)
        [m_contentsLayer.get() setNeedsDisplay];
}

bool GraphicsLayerCA::createAnimationFromKeyframes(const KeyframeValueList& valueList, const Animation* animation, const String& animationName, double timeOffset)
{
    ASSERT(valueList.property() != AnimatedPropertyWebkitTransform);

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    bool isKeyframe = valueList.size() > 2;
    bool valuesOK;
    
    bool additive = false;
    int animationIndex = 0;
    
    CAPropertyAnimation* caAnimation;
    if (isKeyframe) {
        CAKeyframeAnimation* keyframeAnim = createKeyframeAnimation(animation, valueList.property(), additive);
        valuesOK = setAnimationKeyframes(valueList, animation, keyframeAnim);
        caAnimation = keyframeAnim;
    } else {
        CABasicAnimation* basicAnim = createBasicAnimation(animation, valueList.property(), additive);
        valuesOK = setAnimationEndpoints(valueList, animation, basicAnim);
        caAnimation = basicAnim;
    }
    
    if (!valuesOK)
        return false;

    m_uncomittedAnimations.append(LayerPropertyAnimation(caAnimation, animationName, valueList.property(), animationIndex, timeOffset));
    
    END_BLOCK_OBJC_EXCEPTIONS;

    return true;
}

bool GraphicsLayerCA::createTransformAnimationsFromKeyframes(const KeyframeValueList& valueList, const Animation* animation, const String& animationName, double timeOffset, const IntSize& boxSize)
{
    ASSERT(valueList.property() == AnimatedPropertyWebkitTransform);

    TransformOperationList functionList;
    bool listsMatch, hasBigRotation;
    fetchTransformOperationList(valueList, functionList, listsMatch, hasBigRotation);

    // We need to fall back to software animation if we don't have setValueFunction:, and
    // we would need to animate each incoming transform function separately. This is the
    // case if we have a rotation >= 180 or we have more than one transform function.
    if ((hasBigRotation || functionList.size() > 1) && !caValueFunctionSupported())
        return false;

    bool validMatrices = true;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    // If functionLists don't match we do a matrix animation, otherwise we do a component hardware animation.
    // Also, we can't do component animation unless we have valueFunction, so we need to do matrix animation
    // if that's not true as well.
    bool isMatrixAnimation = !listsMatch || !caValueFunctionSupported();
    
    size_t numAnimations = isMatrixAnimation ? 1 : functionList.size();
    bool isKeyframe = valueList.size() > 2;
    
    // Iterate through the transform functions, sending an animation for each one.
    for (size_t animationIndex = 0; animationIndex < numAnimations; ++animationIndex) {
        TransformOperation::OperationType transformOp = isMatrixAnimation ? TransformOperation::MATRIX_3D : functionList[animationIndex];
        CAPropertyAnimation* caAnimation;

#if defined(BUILDING_ON_LEOPARD) || defined(BUILDING_ON_SNOW_LEOPARD)
        // CA applies animations in reverse order (<rdar://problem/7095638>) so we need the last one we add (per property)
        // to be non-additive.
        bool additive = animationIndex < (numAnimations - 1);
#else
        bool additive = animationIndex > 0;
#endif
        if (isKeyframe) {
            CAKeyframeAnimation* keyframeAnim = createKeyframeAnimation(animation, valueList.property(), additive);
            validMatrices = setTransformAnimationKeyframes(valueList, animation, keyframeAnim, animationIndex, transformOp, isMatrixAnimation, boxSize);
            caAnimation = keyframeAnim;
        } else {
            CABasicAnimation* basicAnim = createBasicAnimation(animation, valueList.property(), additive);
            validMatrices = setTransformAnimationEndpoints(valueList, animation, basicAnim, animationIndex, transformOp, isMatrixAnimation, boxSize);
            caAnimation = basicAnim;
        }
        
        if (!validMatrices)
            break;
    
        m_uncomittedAnimations.append(LayerPropertyAnimation(caAnimation, animationName, valueList.property(), animationIndex, timeOffset));
    }

    END_BLOCK_OBJC_EXCEPTIONS;

    return validMatrices;
}

CABasicAnimation* GraphicsLayerCA::createBasicAnimation(const Animation* anim, AnimatedPropertyID property, bool additive)
{
    CABasicAnimation* basicAnim = [CABasicAnimation animationWithKeyPath:propertyIdToString(property)];
    setupAnimation(basicAnim, anim, additive);
    return basicAnim;
}

CAKeyframeAnimation* GraphicsLayerCA::createKeyframeAnimation(const Animation* anim, AnimatedPropertyID property, bool additive)
{
    CAKeyframeAnimation* keyframeAnim = [CAKeyframeAnimation animationWithKeyPath:propertyIdToString(property)];
    setupAnimation(keyframeAnim, anim, additive);
    return keyframeAnim;
}

void GraphicsLayerCA::setupAnimation(CAPropertyAnimation* propertyAnim, const Animation* anim, bool additive)
{
    double duration = anim->duration();
    if (duration <= 0)
        duration = cAnimationAlmostZeroDuration;

    float repeatCount = anim->iterationCount();
    if (repeatCount == Animation::IterationCountInfinite)
        repeatCount = FLT_MAX;
    else if (anim->direction() == Animation::AnimationDirectionAlternate)
        repeatCount /= 2;

    NSString *fillMode = 0;
    switch (anim->fillMode()) {
    case AnimationFillModeNone:
        fillMode = kCAFillModeForwards; // Use "forwards" rather than "removed" because the style system will remove the animation when it is finished. This avoids a flash.
        break;
    case AnimationFillModeBackwards:
        fillMode = kCAFillModeBoth; // Use "both" rather than "backwards" because the style system will remove the animation when it is finished. This avoids a flash.
        break;
    case AnimationFillModeForwards:
       fillMode = kCAFillModeForwards;
       break;
    case AnimationFillModeBoth:
       fillMode = kCAFillModeBoth;
       break;
    }

    [propertyAnim setDuration:duration];
    [propertyAnim setRepeatCount:repeatCount];
    [propertyAnim setAutoreverses:anim->direction()];
    [propertyAnim setRemovedOnCompletion:NO];
    [propertyAnim setAdditive:additive];
    [propertyAnim setFillMode:fillMode];

    [propertyAnim setDelegate:m_animationDelegate.get()];
}

CAMediaTimingFunction* GraphicsLayerCA::timingFunctionForAnimationValue(const AnimationValue* animValue, const Animation* anim)
{
    const TimingFunction* tf = 0;
    if (animValue->timingFunction())
        tf = animValue->timingFunction();
    else if (anim->isTimingFunctionSet())
        tf = anim->timingFunction().get();

    return getCAMediaTimingFunction(tf ? tf : CubicBezierTimingFunction::create().get());
}

bool GraphicsLayerCA::setAnimationEndpoints(const KeyframeValueList& valueList, const Animation* anim, CABasicAnimation* basicAnim)
{
    id fromValue = nil;
    id toValue = nil;

    switch (valueList.property()) {
        case AnimatedPropertyOpacity: {
            const FloatAnimationValue* startVal = static_cast<const FloatAnimationValue*>(valueList.at(0));
            const FloatAnimationValue* endVal = static_cast<const FloatAnimationValue*>(valueList.at(1));
            fromValue = [NSNumber numberWithFloat:startVal->value()];
            toValue = [NSNumber numberWithFloat:endVal->value()];
            break;
        }
        default:
            ASSERT_NOT_REACHED();     // we don't animate color yet
            break;
    }

    // This codepath is used for 2-keyframe animations, so we still need to look in the start
    // for a timing function.
    CAMediaTimingFunction* timingFunction = timingFunctionForAnimationValue(valueList.at(0), anim);
    [basicAnim setTimingFunction:timingFunction];

    [basicAnim setFromValue:fromValue];
    [basicAnim setToValue:toValue];

    return true;
}

bool GraphicsLayerCA::setAnimationKeyframes(const KeyframeValueList& valueList, const Animation* anim, CAKeyframeAnimation* keyframeAnim)
{
    RetainPtr<NSMutableArray> keyTimes(AdoptNS, [[NSMutableArray alloc] init]);
    RetainPtr<NSMutableArray> values(AdoptNS, [[NSMutableArray alloc] init]);
    RetainPtr<NSMutableArray> timingFunctions(AdoptNS, [[NSMutableArray alloc] init]);

    for (unsigned i = 0; i < valueList.size(); ++i) {
        const AnimationValue* curValue = valueList.at(i);
        [keyTimes.get() addObject:[NSNumber numberWithFloat:curValue->keyTime()]];

        switch (valueList.property()) {
            case AnimatedPropertyOpacity: {
                const FloatAnimationValue* floatValue = static_cast<const FloatAnimationValue*>(curValue);
                [values.get() addObject:[NSNumber numberWithFloat:floatValue->value()]];
                break;
            }
            default:
                ASSERT_NOT_REACHED();     // we don't animate color yet
                break;
        }

        CAMediaTimingFunction* timingFunction = timingFunctionForAnimationValue(curValue, anim);
        [timingFunctions.get() addObject:timingFunction];
    }
    
    // We toss the last tfArray value because it has to one shorter than the others.
    [timingFunctions.get() removeLastObject];

    [keyframeAnim setKeyTimes:keyTimes.get()];
    [keyframeAnim setValues:values.get()];
    [keyframeAnim setTimingFunctions:timingFunctions.get()];
    
    return true;
}

bool GraphicsLayerCA::setTransformAnimationEndpoints(const KeyframeValueList& valueList, const Animation* anim, CABasicAnimation* basicAnim, int functionIndex, TransformOperation::OperationType transformOp, bool isMatrixAnimation, const IntSize& boxSize)
{
    id fromValue;
    id toValue;
    
    ASSERT(valueList.size() == 2);
    const TransformAnimationValue* startValue = static_cast<const TransformAnimationValue*>(valueList.at(0));
    const TransformAnimationValue* endValue = static_cast<const TransformAnimationValue*>(valueList.at(1));
    
    if (isMatrixAnimation) {
        TransformationMatrix fromTransform, toTransform;
        startValue->value()->apply(boxSize, fromTransform);
        endValue->value()->apply(boxSize, toTransform);

        // If any matrix is singular, CA won't animate it correctly. So fall back to software animation
        if (!fromTransform.isInvertible() || !toTransform.isInvertible())
            return false;

        CATransform3D caTransform;
        copyTransform(caTransform, fromTransform);
        fromValue = [NSValue valueWithCATransform3D:caTransform];

        copyTransform(caTransform, toTransform);
        toValue = [NSValue valueWithCATransform3D:caTransform];
    } else {
        fromValue = getTransformFunctionValue(startValue->value()->at(functionIndex), transformOp, boxSize);
        toValue = getTransformFunctionValue(endValue->value()->at(functionIndex), transformOp, boxSize);
    }

    // This codepath is used for 2-keyframe animations, so we still need to look in the start
    // for a timing function.
    CAMediaTimingFunction* timingFunction = timingFunctionForAnimationValue(valueList.at(0), anim);
    [basicAnim setTimingFunction:timingFunction];

    [basicAnim setFromValue:fromValue];
    [basicAnim setToValue:toValue];

#if HAVE_MODERN_QUARTZCORE
    if (NSString *valueFunctionName = getValueFunctionNameForTransformOperation(transformOp))
        [basicAnim setValueFunction:[CAValueFunction functionWithName:valueFunctionName]];
#endif

    return true;
}

bool GraphicsLayerCA::setTransformAnimationKeyframes(const KeyframeValueList& valueList, const Animation* animation, CAKeyframeAnimation* keyframeAnim, int functionIndex, TransformOperation::OperationType transformOpType, bool isMatrixAnimation, const IntSize& boxSize)
{
    RetainPtr<NSMutableArray> keyTimes(AdoptNS, [[NSMutableArray alloc] init]);
    RetainPtr<NSMutableArray> values(AdoptNS, [[NSMutableArray alloc] init]);
    RetainPtr<NSMutableArray> timingFunctions(AdoptNS, [[NSMutableArray alloc] init]);

    for (unsigned i = 0; i < valueList.size(); ++i) {
        const TransformAnimationValue* curValue = static_cast<const TransformAnimationValue*>(valueList.at(i));
        [keyTimes.get() addObject:[NSNumber numberWithFloat:curValue->keyTime()]];

        if (isMatrixAnimation) {
            TransformationMatrix transform;
            curValue->value()->apply(boxSize, transform);

            // If any matrix is singular, CA won't animate it correctly. So fall back to software animation
            if (!transform.isInvertible())
                return false;

            CATransform3D caTransform;
            copyTransform(caTransform, transform);
            [values.get() addObject:[NSValue valueWithCATransform3D:caTransform]];
        } else {
            const TransformOperation* transformOp = curValue->value()->at(functionIndex);
            [values.get() addObject:getTransformFunctionValue(transformOp, transformOpType, boxSize)];
        }

        CAMediaTimingFunction* timingFunction = timingFunctionForAnimationValue(curValue, animation);
        [timingFunctions.get() addObject:timingFunction];
    }
    
    // We toss the last tfArray value because it has to one shorter than the others.
    [timingFunctions.get() removeLastObject];

    [keyframeAnim setKeyTimes:keyTimes.get()];
    [keyframeAnim setValues:values.get()];
    [keyframeAnim setTimingFunctions:timingFunctions.get()];

#if HAVE_MODERN_QUARTZCORE
    if (NSString *valueFunctionName = getValueFunctionNameForTransformOperation(transformOpType))
        [keyframeAnim setValueFunction:[CAValueFunction functionWithName:valueFunctionName]];
#endif
    return true;
}

void GraphicsLayerCA::suspendAnimations(double time)
{
    double t = currentTimeToMediaTime(time ? time : currentTime());
    [primaryLayer() setSpeed:0];
    [primaryLayer() setTimeOffset:t];

    // Suspend the animations on the clones too.
    if (LayerMap* layerCloneMap = primaryLayerClones()) {
        LayerMap::const_iterator end = layerCloneMap->end();
        for (LayerMap::const_iterator it = layerCloneMap->begin(); it != end; ++it) {
            CALayer *currLayer = it->second.get();
            [currLayer setSpeed:0];
            [currLayer setTimeOffset:t];
        }
    }
}

void GraphicsLayerCA::resumeAnimations()
{
    [primaryLayer() setSpeed:1];
    [primaryLayer() setTimeOffset:0];

    // Resume the animations on the clones too.
    if (LayerMap* layerCloneMap = primaryLayerClones()) {
        LayerMap::const_iterator end = layerCloneMap->end();
        for (LayerMap::const_iterator it = layerCloneMap->begin(); it != end; ++it) {
            CALayer *currLayer = it->second.get();
            [currLayer setSpeed:1];
            [currLayer setTimeOffset:0];
        }
    }
}

CALayer* GraphicsLayerCA::hostLayerForSublayers() const
{
    return m_structuralLayer.get() ? m_structuralLayer.get() : m_layer.get(); 
}

CALayer* GraphicsLayerCA::layerForSuperlayer() const
{
    return m_structuralLayer ? m_structuralLayer.get() : m_layer.get();
}

CALayer* GraphicsLayerCA::animatedLayer(AnimatedPropertyID property) const
{
    return (property == AnimatedPropertyBackgroundColor) ? m_contentsLayer.get() : primaryLayer();
}

GraphicsLayerCA::LayerMap* GraphicsLayerCA::animatedLayerClones(AnimatedPropertyID property) const
{
    return (property == AnimatedPropertyBackgroundColor) ? m_contentsLayerClones.get() : primaryLayerClones();
}

PlatformLayer* GraphicsLayerCA::platformLayer() const
{
    return primaryLayer();
}

void GraphicsLayerCA::setDebugBackgroundColor(const Color& color)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    
    if (color.isValid())
        setLayerBackgroundColor(m_layer.get(), color);
    else
        clearLayerBackgroundColor(m_layer.get());
    
    END_BLOCK_OBJC_EXCEPTIONS
}

void GraphicsLayerCA::setDebugBorder(const Color& color, float borderWidth)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    
    if (color.isValid()) {
        setLayerBorderColor(m_layer.get(), color);
        [m_layer.get() setBorderWidth:borderWidth];
    } else {
        clearBorderColor(m_layer.get());
        [m_layer.get() setBorderWidth:0];
    }
    
    END_BLOCK_OBJC_EXCEPTIONS
}

FloatSize GraphicsLayerCA::constrainedSize() const
{
    float tileColumns = ceilf(m_size.width() / cTiledLayerTileSize);
    float tileRows = ceilf(m_size.height() / cTiledLayerTileSize);
    double numTiles = tileColumns * tileRows;
    
    FloatSize constrainedSize = m_size;
    const unsigned cMaxTileCount = 512;
    while (numTiles > cMaxTileCount) {
        // Constrain the wider dimension.
        if (constrainedSize.width() >= constrainedSize.height()) {
            tileColumns = max(floorf(cMaxTileCount / tileRows), 1.0f);
            constrainedSize.setWidth(tileColumns * cTiledLayerTileSize);
        } else {
            tileRows = max(floorf(cMaxTileCount / tileColumns), 1.0f);
            constrainedSize.setHeight(tileRows * cTiledLayerTileSize);
        }
        numTiles = tileColumns * tileRows;
    }
    
    return constrainedSize;
}

bool GraphicsLayerCA::requiresTiledLayer(const FloatSize& size) const
{
    if (!m_drawsContent)
        return false;

    // FIXME: catch zero-size height or width here (or earlier)?
    return size.width() > cMaxPixelDimension || size.height() > cMaxPixelDimension;
}

void GraphicsLayerCA::swapFromOrToTiledLayer(bool useTiledLayer)
{
    if (useTiledLayer == m_usingTiledLayer)
        return;

    CGSize tileSize = CGSizeMake(cTiledLayerTileSize, cTiledLayerTileSize);

    RetainPtr<CALayer> oldLayer = m_layer.get();
    
    Class layerClass = useTiledLayer ? [WebTiledLayer self] : [WebLayer self];
    m_layer.adoptNS([[layerClass alloc] init]);

    m_usingTiledLayer = useTiledLayer;

    if (useTiledLayer) {
        WebTiledLayer* tiledLayer = (WebTiledLayer*)m_layer.get();
        [tiledLayer setTileSize:tileSize];
        [tiledLayer setLevelsOfDetail:1];
        [tiledLayer setLevelsOfDetailBias:0];
        [tiledLayer setContentsGravity:@"bottomLeft"];

#if !HAVE_MODERN_QUARTZCORE
        // Tiled layer has issues with flipped coordinates.
        setContentsOrientation(CompositingCoordinatesTopDown);
#endif
    } else {
#if !HAVE_MODERN_QUARTZCORE
        setContentsOrientation(defaultContentsOrientation());
#endif
    }
    
    [m_layer.get() setLayerOwner:this];
    safeSetSublayers(m_layer.get(), [oldLayer.get() sublayers]);
    
    [[oldLayer.get() superlayer] replaceSublayer:oldLayer.get() with:m_layer.get()];

    updateContentsTransform();

    updateLayerPosition();
    updateLayerSize();
    updateAnchorPoint();
    updateTransform();
    updateChildrenTransform();
    updateMasksToBounds();
    updateContentsOpaque();
    updateBackfaceVisibility();
    updateLayerBackgroundColor();
    
    updateOpacityOnLayer();
    
#ifndef NDEBUG
    String name = String::format("CALayer(%p) GraphicsLayer(%p) ", m_layer.get(), this) + m_name;
    [m_layer.get() setName:name];
#endif

    // move over animations
    moveOrCopyAnimationsForProperty(Move, AnimatedPropertyWebkitTransform, oldLayer.get(), m_layer.get());
    moveOrCopyAnimationsForProperty(Move, AnimatedPropertyOpacity, oldLayer.get(), m_layer.get());
    moveOrCopyAnimationsForProperty(Move, AnimatedPropertyBackgroundColor, oldLayer.get(), m_layer.get());
    
    // need to tell new layer to draw itself
    setNeedsDisplay();
    
    updateDebugIndicators();
}

GraphicsLayer::CompositingCoordinatesOrientation GraphicsLayerCA::defaultContentsOrientation() const
{
#if !HAVE_MODERN_QUARTZCORE
    // Older QuartzCore does not support -geometryFlipped, so we manually flip the root
    // layer geometry, and then flip the contents of each layer back so that the CTM for CG
    // is unflipped, allowing it to do the correct font auto-hinting.
    return CompositingCoordinatesBottomUp;
#else
    return CompositingCoordinatesTopDown;
#endif
}

void GraphicsLayerCA::updateContentsTransform()
{
#if !HAVE_MODERN_QUARTZCORE
    if (contentsOrientation() == CompositingCoordinatesBottomUp) {
        CGAffineTransform contentsTransform = CGAffineTransformMakeScale(1, -1);
        contentsTransform = CGAffineTransformTranslate(contentsTransform, 0, -[m_layer.get() bounds].size.height);
        [m_layer.get() setContentsTransform:contentsTransform];
    }
#endif
}

void GraphicsLayerCA::setupContentsLayer(CALayer* contentsLayer)
{
    // Turn off implicit animations on the inner layer.
    [contentsLayer setStyle:[NSDictionary dictionaryWithObject:nullActionsDictionary() forKey:@"actions"]];
    [contentsLayer setMasksToBounds:YES];

    if (defaultContentsOrientation() == CompositingCoordinatesBottomUp) {
        CATransform3D flipper = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, -1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f};
        [contentsLayer setTransform:flipper];
        [contentsLayer setAnchorPoint:CGPointMake(0.0f, 1.0f)];
    } else
        [contentsLayer setAnchorPoint:CGPointZero];

    if (showDebugBorders()) {
        setLayerBorderColor(contentsLayer, Color(0, 0, 128, 180));
        [contentsLayer setBorderWidth:1.0f];
    }
}

CALayer *GraphicsLayerCA::findOrMakeClone(CloneID cloneID, CALayer *sourceLayer, LayerMap* clones, CloneLevel cloneLevel)
{
    if (!sourceLayer)
        return 0;

    CALayer *resultLayer;

    // Add with a dummy value to get an iterator for the insertion position, and a boolean that tells
    // us whether there's an item there. This technique avoids two hash lookups.
    RetainPtr<CALayer> dummy;
    pair<LayerMap::iterator, bool> addResult = clones->add(cloneID, dummy);
    if (!addResult.second) {
        // Value was not added, so it exists already.
        resultLayer = addResult.first->second.get();
    } else {
        resultLayer = cloneLayer(sourceLayer, cloneLevel);
#ifndef NDEBUG
        [resultLayer setName:[NSString stringWithFormat:@"Clone %d of layer %@", cloneID[0U], sourceLayer]];
#endif
        addResult.first->second = resultLayer;
    }

    return resultLayer;
}   

void GraphicsLayerCA::ensureCloneLayers(CloneID cloneID, CALayer *& primaryLayer, CALayer *& structuralLayer, CALayer *& contentsLayer, CloneLevel cloneLevel)
{
    structuralLayer = nil;
    contentsLayer = nil;

    if (!m_layerClones)
        m_layerClones = new LayerMap;

    if (!m_structuralLayerClones && m_structuralLayer)
        m_structuralLayerClones = new LayerMap;

    if (!m_contentsLayerClones && m_contentsLayer)
        m_contentsLayerClones = new LayerMap;

    primaryLayer = findOrMakeClone(cloneID, m_layer.get(), m_layerClones.get(), cloneLevel);
    structuralLayer = findOrMakeClone(cloneID, m_structuralLayer.get(), m_structuralLayerClones.get(), cloneLevel);
    contentsLayer = findOrMakeClone(cloneID, m_contentsLayer.get(), m_contentsLayerClones.get(), cloneLevel);
}

void GraphicsLayerCA::removeCloneLayers()
{
    m_layerClones = 0;
    m_structuralLayerClones = 0;
    m_contentsLayerClones = 0;
}

FloatPoint GraphicsLayerCA::positionForCloneRootLayer() const
{
    // This can get called during a sync when we've just removed the m_replicaLayer.
    if (!m_replicaLayer)
        return FloatPoint();

    FloatPoint replicaPosition = m_replicaLayer->replicatedLayerPosition();
    return FloatPoint(replicaPosition.x() + m_anchorPoint.x() * m_size.width(),
                      replicaPosition.y() + m_anchorPoint.y() * m_size.height());
}

void GraphicsLayerCA::propagateLayerChangeToReplicas()
{
    for (GraphicsLayer* currLayer = this; currLayer; currLayer = currLayer->parent()) {
        GraphicsLayerCA* currLayerCA = static_cast<GraphicsLayerCA*>(currLayer);
        if (!currLayerCA->hasCloneLayers())
            break;

        if (currLayerCA->replicaLayer())
            static_cast<GraphicsLayerCA*>(currLayerCA->replicaLayer())->noteLayerPropertyChanged(ReplicatedLayerChanged);
    }
}

CALayer *GraphicsLayerCA::fetchCloneLayers(GraphicsLayer* replicaRoot, ReplicaState& replicaState, CloneLevel cloneLevel)
{
    CALayer *primaryLayer;
    CALayer *structuralLayer;
    CALayer *contentsLayer;
    ensureCloneLayers(replicaState.cloneID(), primaryLayer, structuralLayer, contentsLayer, cloneLevel);

    if (m_maskLayer) {
        CALayer *maskClone = static_cast<GraphicsLayerCA*>(m_maskLayer)->fetchCloneLayers(replicaRoot, replicaState, IntermediateCloneLevel);
        [primaryLayer setMask:maskClone];
    }

    if (m_replicatedLayer) {
        // We are a replica being asked for clones of our layers.
        CALayer *replicaRoot = replicatedLayerRoot(replicaState);
        if (!replicaRoot)
            return nil;

        if (structuralLayer) {
            [structuralLayer insertSublayer:replicaRoot atIndex:0];
            return structuralLayer;
        }
        
        [primaryLayer insertSublayer:replicaRoot atIndex:0];
        return primaryLayer;
    }

    const Vector<GraphicsLayer*>& childLayers = children();
    NSMutableArray* clonalSublayers = nil;

    CALayer *replicaLayer = nil;
    if (m_replicaLayer && m_replicaLayer != replicaRoot) {
        // We have nested replicas. Ask the replica layer for a clone of its contents.
        replicaState.setBranchType(ReplicaState::ReplicaBranch);
        replicaLayer = static_cast<GraphicsLayerCA*>(m_replicaLayer)->fetchCloneLayers(replicaRoot, replicaState, RootCloneLevel);
        replicaState.setBranchType(ReplicaState::ChildBranch);
    }
    
    if (replicaLayer || structuralLayer || contentsLayer || childLayers.size() > 0) {
        clonalSublayers = [[NSMutableArray alloc] init];

        if (structuralLayer) {
            // Replicas render behind the actual layer content.
            if (replicaLayer)
                [clonalSublayers addObject:replicaLayer];
                
            // Add the primary layer next. Even if we have negative z-order children, the primary layer always comes behind.
            [clonalSublayers addObject:primaryLayer];
        } else if (contentsLayer) {
            // FIXME: add the contents layer in the correct order with negative z-order children.
            // This does not cause visible rendering issues because currently contents layers are only used
            // for replaced elements that don't have children.
            [clonalSublayers addObject:contentsLayer];
        }
        
        replicaState.push(ReplicaState::ChildBranch);

        size_t numChildren = childLayers.size();
        for (size_t i = 0; i < numChildren; ++i) {
            GraphicsLayerCA* curChild = static_cast<GraphicsLayerCA*>(childLayers[i]);

            CALayer *childLayer = curChild->fetchCloneLayers(replicaRoot, replicaState, IntermediateCloneLevel);
            if (childLayer)
                [clonalSublayers addObject:childLayer];
        }

        replicaState.pop();

        [clonalSublayers makeObjectsPerformSelector:@selector(removeFromSuperlayer)];
    }
    
    CALayer *result;
    if (structuralLayer) {
        [structuralLayer setSublayers:clonalSublayers];

        if (contentsLayer) {
            // If we have a transform layer, then the contents layer is parented in the 
            // primary layer (which is itself a child of the transform layer).
            [primaryLayer setSublayers:nil];
            [primaryLayer addSublayer:contentsLayer];
        }

        result = structuralLayer;
    } else {
        [primaryLayer setSublayers:clonalSublayers];
        result = primaryLayer;
    }

    [clonalSublayers release];
    return result;
}

CALayer *GraphicsLayerCA::cloneLayer(CALayer *layer, CloneLevel cloneLevel)
{
    static Class transformLayerClass = NSClassFromString(@"CATransformLayer");
    CALayer *newLayer = nil;
    if ([layer isKindOfClass:transformLayerClass])
        newLayer = [transformLayerClass layer];
    else
        newLayer = [CALayer layer];

    [newLayer setStyle:[NSDictionary dictionaryWithObject:nullActionsDictionary() forKey:@"actions"]];
    
    [newLayer setPosition:[layer position]];
    [newLayer setBounds:[layer bounds]];
    [newLayer setAnchorPoint:[layer anchorPoint]];
#if HAVE_MODERN_QUARTZCORE
    [newLayer setAnchorPointZ:[layer anchorPointZ]];
#endif
    [newLayer setTransform:[layer transform]];
    [newLayer setSublayerTransform:[layer sublayerTransform]];
    [newLayer setContents:[layer contents]];
    [newLayer setMasksToBounds:[layer masksToBounds]];
    [newLayer setDoubleSided:[layer isDoubleSided]];
    [newLayer setOpaque:[layer isOpaque]];
    [newLayer setBackgroundColor:[layer backgroundColor]];

    if (cloneLevel == IntermediateCloneLevel) {
        [newLayer setOpacity:[layer opacity]];
        moveOrCopyAnimationsForProperty(Copy, AnimatedPropertyWebkitTransform, layer, newLayer);
        moveOrCopyAnimationsForProperty(Copy, AnimatedPropertyOpacity, layer, newLayer);
    }
    
    if (showDebugBorders()) {
        setLayerBorderColor(newLayer, Color(255, 122, 251));
        [newLayer setBorderWidth:2];
    }
    
    return newLayer;
}

void GraphicsLayerCA::setOpacityInternal(float accumulatedOpacity)
{
    LayerMap* layerCloneMap = 0;
    
    if (preserves3D()) {
        [m_layer.get() setOpacity:accumulatedOpacity];
        layerCloneMap = m_layerClones.get();
    } else {
        [primaryLayer() setOpacity:accumulatedOpacity];
        layerCloneMap = primaryLayerClones();
    }

    if (layerCloneMap) {
        LayerMap::const_iterator end = layerCloneMap->end();
        for (LayerMap::const_iterator it = layerCloneMap->begin(); it != end; ++it) {
            if (m_replicaLayer && isReplicatedRootClone(it->first))
                continue;
            CALayer *currLayer = it->second.get();
            [currLayer setOpacity:m_opacity];
        }
    }
}

void GraphicsLayerCA::updateOpacityOnLayer()
{
#if !HAVE_MODERN_QUARTZCORE
    // Distribute opacity either to our own layer or to our children. We pass in the 
    // contribution from our parent(s).
    distributeOpacity(parent() ? parent()->accumulatedOpacity() : 1);
#else
    [primaryLayer() setOpacity:m_opacity];

    if (LayerMap* layerCloneMap = primaryLayerClones()) {
        LayerMap::const_iterator end = layerCloneMap->end();
        for (LayerMap::const_iterator it = layerCloneMap->begin(); it != end; ++it) {
            if (m_replicaLayer && isReplicatedRootClone(it->first))
                continue;

            CALayer *currLayer = it->second.get();
            [currLayer setOpacity:m_opacity];
        }
        
    }
#endif
}

void GraphicsLayerCA::noteSublayersChanged()
{
    noteLayerPropertyChanged(ChildrenChanged);
    propagateLayerChangeToReplicas();
}

void GraphicsLayerCA::noteLayerPropertyChanged(LayerChangeFlags flags)
{
    if (!m_uncommittedChanges && m_client)
        m_client->notifySyncRequired(this);

    m_uncommittedChanges |= flags;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
