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
#if ENABLE(3D_CANVAS)
#import "Canvas3DLayer.h"
#endif
#import "CString.h"
#import "FloatConversion.h"
#import "FloatRect.h"
#import "Image.h"
#import "PlatformString.h"
#import <QuartzCore/QuartzCore.h>
#import "RotateTransformOperation.h"
#import "ScaleTransformOperation.h"
#import "SystemTime.h"
#import "TranslateTransformOperation.h"
#import "WebLayer.h"
#import "WebTiledLayer.h"
#import <limits.h>
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
static NSString* getValueFunctionNameForTransformOperation(TransformOperation::OperationType transformType)
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

static String animationIdentifier(AnimatedPropertyID property, int index)
{
    String animationId = propertyIdToString(property);
    animationId.append("_");
    animationId.append(String::number(index));
    return animationId;
}

#if !HAVE_MODERN_QUARTZCORE
static TransformationMatrix flipTransform()
{
    TransformationMatrix flipper;
    flipper.flipY();
    return flipper;
}
#endif

static CAMediaTimingFunction* getCAMediaTimingFunction(const TimingFunction& timingFunction)
{
    switch (timingFunction.type()) {
        case LinearTimingFunction:
            return [CAMediaTimingFunction functionWithName:@"linear"];
        case CubicBezierTimingFunction:
            return [CAMediaTimingFunction functionWithControlPoints:static_cast<float>(timingFunction.x1()) :static_cast<float>(timingFunction.y1())
                        :static_cast<float>(timingFunction.x2()) :static_cast<float>(timingFunction.y2())];
    }
    return 0;
}

#ifndef NDEBUG
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
#endif

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

GraphicsLayer::CompositingCoordinatesOrientation GraphicsLayer::compositingCoordinatesOrientation()
{
    return CompositingCoordinatesBottomUp;
}

#ifndef NDEBUG
bool GraphicsLayer::showDebugBorders()
{
    static bool showDebugBorders = [[NSUserDefaults standardUserDefaults] boolForKey:@"WebCoreLayerBorders"];
    return showDebugBorders;
}

bool GraphicsLayer::showRepaintCounter()
{
    static bool showRepaintCounter = [[NSUserDefaults standardUserDefaults] boolForKey:@"WebCoreLayerRepaintCounter"];
    return showRepaintCounter;
}
#endif

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

PassOwnPtr<GraphicsLayer> GraphicsLayer::create(GraphicsLayerClient* client)
{
    return new GraphicsLayerCA(client);
}

GraphicsLayerCA::GraphicsLayerCA(GraphicsLayerClient* client)
: GraphicsLayer(client)
, m_contentsLayerPurpose(NoContentsLayer)
, m_contentsLayerHasBackgroundColor(false)
, m_uncommittedChanges(NoChange)
#if ENABLE(3D_CANVAS)
, m_platformGraphicsContext3D(NullPlatformGraphicsContext3D)
, m_platformTexture(NullPlatform3DObject)
#endif
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    m_layer.adoptNS([[WebLayer alloc] init]);
    [m_layer.get() setLayerOwner:this];

#if !HAVE_MODERN_QUARTZCORE
    setContentsOrientation(defaultContentsOrientation());
#endif

#ifndef NDEBUG
    updateDebugIndicators();
#endif

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
    
    // animationDidStart: can fire after this, so we need to clear out the layer on the delegate.
    [m_animationDelegate.get() setLayer:0];

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

void GraphicsLayerCA::addChild(GraphicsLayer* childLayer)
{
    GraphicsLayer::addChild(childLayer);
    noteLayerPropertyChanged(ChildrenChanged);
}

void GraphicsLayerCA::addChildAtIndex(GraphicsLayer* childLayer, int index)
{
    GraphicsLayer::addChildAtIndex(childLayer, index);
    noteLayerPropertyChanged(ChildrenChanged);
}

void GraphicsLayerCA::addChildBelow(GraphicsLayer* childLayer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildBelow(childLayer, sibling);
    noteLayerPropertyChanged(ChildrenChanged);
}

void GraphicsLayerCA::addChildAbove(GraphicsLayer* childLayer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildAbove(childLayer, sibling);
    noteLayerPropertyChanged(ChildrenChanged);
}

bool GraphicsLayerCA::replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild)
{
    if (GraphicsLayer::replaceChild(oldChild, newChild)) {
        noteLayerPropertyChanged(ChildrenChanged);
        return true;
    }
    return false;
}

void GraphicsLayerCA::removeFromParent()
{
    if (m_parent)
        static_cast<GraphicsLayerCA*>(m_parent)->noteLayerPropertyChanged(ChildrenChanged);
    GraphicsLayer::removeFromParent();
}

void GraphicsLayerCA::setMaskLayer(GraphicsLayer* layer)
{
    if (layer == m_maskLayer)
        return;

    GraphicsLayer::setMaskLayer(layer);
    noteLayerPropertyChanged(MaskLayerChanged);
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

static void moveAnimation(AnimatedPropertyID property, CALayer* fromLayer, CALayer* toLayer)
{
    for (int index = 0; ; ++index) {
        String animName = animationIdentifier(property, index);

        CAAnimation* anim = [fromLayer animationForKey:animName];
        if (!anim)
            break;

        [anim retain];
        [fromLayer removeAnimationForKey:animName];
        [toLayer addAnimation:anim forKey:animName];
        [anim release];
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

void GraphicsLayerCA::setContentsRect(const IntRect& rect)
{
    if (rect == m_contentsRect)
        return;

    GraphicsLayer::setContentsRect(rect);
    noteLayerPropertyChanged(ContentsRectChanged);
}

bool GraphicsLayerCA::addAnimation(const KeyframeValueList& valueList, const IntSize& boxSize, const Animation* anim, const String& keyframesName, double beginTime)
{
    if (forceSoftwareAnimation() || !anim || anim->isEmptyOrZeroDuration() || valueList.size() < 2)
        return false;

#if !HAVE_MODERN_QUARTZCORE
    // Older versions of QuartzCore do not handle opacity in transform layers properly, so we will
    // always do software animation in that case.
    if (valueList.property() == AnimatedPropertyOpacity)
        return false;
#endif

    bool createdAnimations = false;
    if (valueList.property() == AnimatedPropertyWebkitTransform)
        createdAnimations = createTransformAnimationsFromKeyframes(valueList, anim, keyframesName, beginTime, boxSize);
    else
        createdAnimations = createAnimationFromKeyframes(valueList, anim, keyframesName, beginTime);

    if (createdAnimations)
        noteLayerPropertyChanged(AnimationChanged);
        
    return createdAnimations;
}

void GraphicsLayerCA::removeAnimationsForProperty(AnimatedPropertyID property)
{
    if (m_transitionPropertiesToRemove.find(property) != m_transitionPropertiesToRemove.end())
        return;

    m_transitionPropertiesToRemove.add(property);
    noteLayerPropertyChanged(AnimationChanged);
}

void GraphicsLayerCA::removeAnimationsForKeyframes(const String& animationName)
{
    if (!animationIsRunning(animationName))
        return;

    m_keyframeAnimationsToProcess.add(animationName, Remove);
    noteLayerPropertyChanged(AnimationChanged);
}

void GraphicsLayerCA::pauseAnimation(const String& keyframesName)
{
    if (!animationIsRunning(keyframesName))
        return;

    AnimationsToProcessMap::iterator it = m_keyframeAnimationsToProcess.find(keyframesName);
    if (it != m_keyframeAnimationsToProcess.end()) {
        // If an animation is scheduled to be removed, don't change the remove to a pause.
        if (it->second != Remove)
            it->second = Pause;
    } else
        m_keyframeAnimationsToProcess.add(keyframesName, Pause);

    noteLayerPropertyChanged(AnimationChanged);
}

void GraphicsLayerCA::setContentsToImage(Image* image)
{
    if (image) {
        m_pendingContentsImage = image->nativeImageForCurrentFrame();
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
            noteLayerPropertyChanged(ChildrenChanged);
    } else {
        m_pendingContentsImage = 0;
        m_contentsLayerPurpose = NoContentsLayer;
        if (m_contentsLayer)
            noteLayerPropertyChanged(ChildrenChanged);
    }

    noteLayerPropertyChanged(ContentsImageChanged);
}

void GraphicsLayerCA::setContentsToVideo(PlatformLayer* videoLayer)
{
    if (videoLayer != m_contentsLayer.get())
        noteLayerPropertyChanged(ChildrenChanged);

    m_contentsLayer = videoLayer;
    noteLayerPropertyChanged(ContentsVideoChanged);

    m_contentsLayerPurpose = videoLayer ? ContentsLayerForVideo : NoContentsLayer;
}

void GraphicsLayerCA::setGeometryOrientation(CompositingCoordinatesOrientation orientation)
{
    if (orientation == m_geometryOrientation)
        return;

    GraphicsLayer::setGeometryOrientation(orientation);
    noteLayerPropertyChanged(GeometryOrientationChanged);

#if !HAVE_MODERN_QUARTZCORE
    // Geometry orientation is mapped onto children transform in older QuartzCores.
    switch (m_geometryOrientation) {
        case CompositingCoordinatesTopDown:
            setChildrenTransform(TransformationMatrix());
            break;
        
        case CompositingCoordinatesBottomUp:
            setChildrenTransform(flipTransform());
            break;
    }
#endif
}

void GraphicsLayerCA::syncCompositingState()
{
    recursiveCommitChanges();
}

void GraphicsLayerCA::recursiveCommitChanges()
{
    commitLayerChanges();

    if (m_maskLayer)
        static_cast<GraphicsLayerCA*>(m_maskLayer)->commitLayerChanges();

    const Vector<GraphicsLayer*>& childLayers = children();
    size_t numChildren = childLayers.size();
    for (size_t i = 0; i < numChildren; ++i) {
        GraphicsLayerCA* curChild = static_cast<GraphicsLayerCA*>(childLayers[i]);
        curChild->recursiveCommitChanges();
    }
}

void GraphicsLayerCA::commitLayerChanges()
{
    if (!m_uncommittedChanges)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    // Need to handle Preserves3DChanged first, because it affects which layers subsequent properties are applied to
    if (m_uncommittedChanges & Preserves3DChanged)
        updateLayerPreserves3D();

    if (m_uncommittedChanges & NameChanged) {
        if (m_transformLayer)
            [m_transformLayer.get() setName:("Transform layer " + name())];
        [m_layer.get() setName:name()];
    }

    if (m_uncommittedChanges & ContentsImageChanged) // Needs to happen before ChildrenChanged
        updateContentsImage();
        
    if (m_uncommittedChanges & ContentsVideoChanged) // Needs to happen before ChildrenChanged
        updateContentsVideo();
    
#if ENABLE(3D_CANVAS)
    if (m_uncommittedChanges & ContentsGraphicsContext3DChanged) // Needs to happen before ChildrenChanged
        updateContentsGraphicsContext3D();
#endif
    
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

    if (m_uncommittedChanges & GeometryOrientationChanged)
        updateGeometryOrientation();

    if (m_uncommittedChanges & MaskLayerChanged)
        updateMaskLayer();

    m_uncommittedChanges = NoChange;
    END_BLOCK_OBJC_EXCEPTIONS
}

void GraphicsLayerCA::updateSublayerList()
{
    NSMutableArray* newSublayers = nil;

    if (m_transformLayer) {
        // Add the primary layer first. Even if we have negative z-order children, the primary layer always comes behind.
        newSublayers = [[NSMutableArray alloc] initWithObjects:m_layer.get(), nil];
    } else if (m_contentsLayer) {
        // FIXME: add the contents layer in the correct order with negative z-order children.
        // This does not cause visible rendering issues because currently contents layers are only used
        // for replaced elements that don't have children.
        newSublayers = [[NSMutableArray alloc] initWithObjects:m_contentsLayer.get(), nil];
    }
    
    const Vector<GraphicsLayer*>& childLayers = children();
    size_t numChildren = childLayers.size();
    for (size_t i = 0; i < numChildren; ++i) {
        GraphicsLayerCA* curChild = static_cast<GraphicsLayerCA*>(childLayers[i]);
     
        CALayer* childLayer = curChild->layerForSuperlayer();
        if (!newSublayers)
            newSublayers = [[NSMutableArray alloc] initWithObjects:childLayer, nil];
        else
            [newSublayers addObject:childLayer];
    }

    [newSublayers makeObjectsPerformSelector:@selector(removeFromSuperlayer)];

    if (m_transformLayer) {
        [m_transformLayer.get() setSublayers:newSublayers];

        if (m_contentsLayer) {
            // If we have a transform layer, then the contents layer is parented in the 
            // primary layer (which is itself a child of the transform layer).
            [m_layer.get() setSublayers:nil];
            [m_layer.get() addSublayer:m_contentsLayer.get()];
        }
    } else {
        [m_layer.get() setSublayers:newSublayers];
    }

    [newSublayers release];
}

void GraphicsLayerCA::updateLayerPosition()
{
    // Position is offset on the layer by the layer anchor point.
    CGPoint posPoint = CGPointMake(m_position.x() + m_anchorPoint.x() * m_size.width(),
                                   m_position.y() + m_anchorPoint.y() * m_size.height());
    
    [primaryLayer() setPosition:posPoint];
}

void GraphicsLayerCA::updateLayerSize()
{
    CGRect rect = CGRectMake(0, 0, m_size.width(), m_size.height());
    if (m_transformLayer) {
        [m_transformLayer.get() setBounds:rect];
        // The anchor of the contents layer is always at 0.5, 0.5, so the position is center-relative.
        CGPoint centerPoint = CGPointMake(m_size.width() / 2.0f, m_size.height() / 2.0f);
        [m_layer.get() setPosition:centerPoint];
    }
    
    bool needTiledLayer = requiresTiledLayer(m_size);
    if (needTiledLayer != m_usingTiledLayer)
        swapFromOrToTiledLayer(needTiledLayer);
    
    [m_layer.get() setBounds:rect];
    
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
    updateLayerPosition();
}

void GraphicsLayerCA::updateTransform()
{
    CATransform3D transform;
    copyTransform(transform, m_transform);
    [primaryLayer() setTransform:transform];
}

void GraphicsLayerCA::updateChildrenTransform()
{
    CATransform3D transform;
    copyTransform(transform, m_childrenTransform);
    [primaryLayer() setSublayerTransform:transform];
}

void GraphicsLayerCA::updateMasksToBounds()
{
    [m_layer.get() setMasksToBounds:m_masksToBounds];
#ifndef NDEBUG
    updateDebugIndicators();
#endif
}

void GraphicsLayerCA::updateContentsOpaque()
{
    [m_layer.get() setOpaque:m_contentsOpaque];
}

void GraphicsLayerCA::updateBackfaceVisibility()
{
    [m_layer.get() setDoubleSided:m_backfaceVisibility];
}

void GraphicsLayerCA::updateLayerPreserves3D()
{
    Class transformLayerClass = NSClassFromString(@"CATransformLayer");
    if (!transformLayerClass)
        return;

    if (m_preserves3D && !m_transformLayer) {
        // Create the transform layer.
        m_transformLayer.adoptNS([[transformLayerClass alloc] init]);

        // Turn off default animations.
        [m_transformLayer.get() setStyle:[NSDictionary dictionaryWithObject:nullActionsDictionary() forKey:@"actions"]];

#ifndef NDEBUG
        [m_transformLayer.get() setName:[NSString stringWithFormat:@"Transform Layer CATransformLayer(%p) GraphicsLayer(%p)", m_transformLayer.get(), this]];
#endif
        // Copy the position from this layer.
        updateLayerPosition();
        updateLayerSize();
        updateAnchorPoint();
        updateTransform();
        updateChildrenTransform();
        
        CGPoint point = CGPointMake(m_size.width() / 2.0f, m_size.height() / 2.0f);
        [m_layer.get() setPosition:point];

        [m_layer.get() setAnchorPoint:CGPointMake(0.5f, 0.5f)];
        [m_layer.get() setTransform:CATransform3DIdentity];
        
        // Set the old layer to opacity of 1. Further down we will set the opacity on the transform layer.
        [m_layer.get() setOpacity:1];

        // Move this layer to be a child of the transform layer.
        [[m_layer.get() superlayer] replaceSublayer:m_layer.get() with:m_transformLayer.get()];
        [m_transformLayer.get() addSublayer:m_layer.get()];

        moveAnimation(AnimatedPropertyWebkitTransform, m_layer.get(), m_transformLayer.get());
        
        updateSublayerList();
    } else if (!m_preserves3D && m_transformLayer) {
        // Relace the transformLayer in the parent with this layer.
        [m_layer.get() removeFromSuperlayer];
        [[m_transformLayer.get() superlayer] replaceSublayer:m_transformLayer.get() with:m_layer.get()];

        moveAnimation(AnimatedPropertyWebkitTransform, m_transformLayer.get(), m_layer.get());

        // Release the transform layer.
        m_transformLayer = 0;

        updateLayerPosition();
        updateLayerSize();
        updateAnchorPoint();
        updateTransform();
        updateChildrenTransform();

        updateSublayerList();
    }

    updateOpacityOnLayer();
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

#ifndef NDEBUG
    updateDebugIndicators();
#endif
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
        
        updateContentsRect();
    } else {
        // No image.
        // m_contentsLayer will be removed via updateSublayerList.
        m_contentsLayer = 0;
    }
}

void GraphicsLayerCA::updateContentsVideo()
{
    // Video layer was set as m_contentsLayer, and will get parented in updateSublayerList().
    if (m_contentsLayer) {
        setupContentsLayer(m_contentsLayer.get());
        updateContentsRect();
    }
}

#if ENABLE(3D_CANVAS)
void GraphicsLayerCA::updateContentsGraphicsContext3D()
{
    // Canvas3D layer was set as m_contentsLayer, and will get parented in updateSublayerList().
    if (m_contentsLayer) {
        setupContentsLayer(m_contentsLayer.get());
        [m_contentsLayer.get() setNeedsDisplay];
        updateContentsRect();
    }
}
#endif
    
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
}

void GraphicsLayerCA::updateGeometryOrientation()
{
#if HAVE_MODERN_QUARTZCORE
    switch (geometryOrientation()) {
        case CompositingCoordinatesTopDown:
            [m_layer.get() setGeometryFlipped:NO];
            break;
        
        case CompositingCoordinatesBottomUp:
            [m_layer.get() setGeometryFlipped:YES];
            break;
    }
    // Geometry orientation is mapped onto children transform in older QuartzCores,
    // so is handled via setGeometryOrientation().
#endif
}

void GraphicsLayerCA::updateMaskLayer()
{
    CALayer* maskCALayer = m_maskLayer ? m_maskLayer->platformLayer() : 0;
    [m_layer.get() setMask:maskCALayer];
}

void GraphicsLayerCA::updateLayerAnimations()
{
    if (m_transitionPropertiesToRemove.size()) {
        HashSet<int>::const_iterator end = m_transitionPropertiesToRemove.end();
        for (HashSet<AnimatedProperty>::const_iterator it = m_transitionPropertiesToRemove.begin(); it != end; ++it) {
            AnimatedPropertyID currProperty = static_cast<AnimatedPropertyID>(*it);
            // Remove all animations with this property in the key.
            // We can't tell if this property is animating via a transition or animation here, but
            // that's OK because the style system never sends both transitions and animations for the same property.
            for (int index = 0; ; ++index) {
                if (!removeAnimationFromLayer(currProperty, index))
                    break;
            }
        }

        m_transitionPropertiesToRemove.clear();
    }

    if (m_keyframeAnimationsToProcess.size()) {
        AnimationsToProcessMap::const_iterator end = m_keyframeAnimationsToProcess.end();
        for (AnimationsToProcessMap::const_iterator it = m_keyframeAnimationsToProcess.begin(); it != end; ++it) {
            const String& currKeyframeName = it->first;
            KeyframeAnimationsMap::iterator animationIt = m_runningKeyframeAnimations.find(currKeyframeName);
            if (animationIt == m_runningKeyframeAnimations.end())
                continue;

            AnimationProcessingAction action = it->second;
            const Vector<AnimationPair>& animations = animationIt->second;
            for (size_t i = 0; i < animations.size(); ++i) {
                const AnimationPair& currPair = animations[i];
                switch (action) {
                    case Remove:
                        removeAnimationFromLayer(static_cast<AnimatedPropertyID>(currPair.first), currPair.second);
                        break;
                    case Pause:
                        pauseAnimationOnLayer(static_cast<AnimatedPropertyID>(currPair.first), currPair.second);
                        break;
                }
            }

            if (action == Remove)
                m_runningKeyframeAnimations.remove(currKeyframeName);
        }
    
        m_keyframeAnimationsToProcess.clear();
    }
    
    size_t numAnimations;
    if ((numAnimations = m_uncomittedAnimations.size())) {
        for (size_t i = 0; i < numAnimations; ++i) {
            const LayerAnimation& pendingAnimation = m_uncomittedAnimations[i];
            setAnimationOnLayer(pendingAnimation.m_animation.get(), pendingAnimation.m_property, pendingAnimation.m_index, pendingAnimation.m_beginTime);
            
            if (!pendingAnimation.m_keyframesName.isEmpty()) {
                // If this is a keyframe anim, we have to remember the association of keyframes name to property/index pairs,
                // so we can remove the animations later if needed.
                // For transitions, we can just generate animation names with property and index.
                KeyframeAnimationsMap::iterator it = m_runningKeyframeAnimations.find(pendingAnimation.m_keyframesName);
                if (it == m_runningKeyframeAnimations.end()) {
                    Vector<AnimationPair> firstPair;
                    firstPair.append(AnimationPair(pendingAnimation.m_property, pendingAnimation.m_index));
                    m_runningKeyframeAnimations.add(pendingAnimation.m_keyframesName, firstPair);
                } else {
                    Vector<AnimationPair>& animPairs = it->second;
                    animPairs.append(AnimationPair(pendingAnimation.m_property, pendingAnimation.m_index));
                }
            }
        }
        
        m_uncomittedAnimations.clear();
    }
}

void GraphicsLayerCA::setAnimationOnLayer(CAPropertyAnimation* caAnim, AnimatedPropertyID property, int index, double beginTime)
{
    PlatformLayer* layer = animatedLayer(property);

    if (beginTime) {
        NSTimeInterval time = [layer convertTime:currentTimeToMediaTime(beginTime) fromLayer:nil];
        [caAnim setBeginTime:time];
    }
    
    String animationName = animationIdentifier(property, index);
    
    [layer removeAnimationForKey:animationName];
    [layer addAnimation:caAnim forKey:animationName];
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

bool GraphicsLayerCA::removeAnimationFromLayer(AnimatedPropertyID property, int index)
{
    PlatformLayer* layer = animatedLayer(property);

    String animationName = animationIdentifier(property, index);

    if (![layer animationForKey:animationName])
        return false;
    
    [layer removeAnimationForKey:animationName];
    
    bug7311367Workaround(m_transformLayer.get(), m_transform);
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

void GraphicsLayerCA::pauseAnimationOnLayer(AnimatedPropertyID property, int index)
{
    PlatformLayer* layer = animatedLayer(property);

    String animationName = animationIdentifier(property, index);

    CAAnimation* caAnim = [layer animationForKey:animationName];
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

    double t = [layer convertTime:currentTimeToMediaTime(currentTime()) fromLayer:nil];
    [pausedAnim setSpeed:0];
    [pausedAnim setTimeOffset:t - [caAnim beginTime]];
    [layer addAnimation:pausedAnim forKey:animationName];  // This will replace the running animation.
}

#if ENABLE(3D_CANVAS)
void GraphicsLayerCA::setContentsToGraphicsContext3D(const GraphicsContext3D* graphicsContext3D)
{
    PlatformGraphicsContext3D context = graphicsContext3D->platformGraphicsContext3D();
    Platform3DObject texture = graphicsContext3D->platformTexture();
    
    if (context == m_platformGraphicsContext3D && texture == m_platformTexture)
        return;
        
    m_platformGraphicsContext3D = context;
    m_platformTexture = texture;
    
    noteLayerPropertyChanged(ChildrenChanged);
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    if (m_platformGraphicsContext3D != NullPlatformGraphicsContext3D && m_platformTexture != NullPlatform3DObject) {
        // create the inner 3d layer
        m_contentsLayer.adoptNS([[Canvas3DLayer alloc] initWithContext:static_cast<CGLContextObj>(m_platformGraphicsContext3D) texture:static_cast<GLuint>(m_platformTexture)]);
#ifndef NDEBUG
        [m_contentsLayer.get() setName:@"3D Layer"];
#endif        
    } else {
        // remove the inner layer
        m_contentsLayer = 0;
    }
    
    END_BLOCK_OBJC_EXCEPTIONS
    
    noteLayerPropertyChanged(ContentsGraphicsContext3DChanged);
    m_contentsLayerPurpose = m_contentsLayer ? ContentsLayerForGraphicsLayer3D : NoContentsLayer;
}
#endif
    
void GraphicsLayerCA::repaintLayerDirtyRects()
{
    if (!m_dirtyRects.size())
        return;

    for (size_t i = 0; i < m_dirtyRects.size(); ++i)
        [m_layer.get() setNeedsDisplayInRect:m_dirtyRects[i]];
    
    m_dirtyRects.clear();
}

bool GraphicsLayerCA::createAnimationFromKeyframes(const KeyframeValueList& valueList, const Animation* animation, const String& keyframesName, double beginTime)
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

    m_uncomittedAnimations.append(LayerAnimation(caAnimation, keyframesName, valueList.property(), animationIndex, beginTime));
    
    END_BLOCK_OBJC_EXCEPTIONS;

    return true;
}

bool GraphicsLayerCA::createTransformAnimationsFromKeyframes(const KeyframeValueList& valueList, const Animation* animation, const String& keyframesName, double beginTime, const IntSize& boxSize)
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

        // CA applies animations in reverse order (<rdar://problem/7095638>) so we need the last one we add (per property)
        // to be non-additive.
        bool additive = animationIndex < (numAnimations - 1);
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
    
        m_uncomittedAnimations.append(LayerAnimation(caAnimation, keyframesName, valueList.property(), animationIndex, beginTime));
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

    [propertyAnim setDuration:duration];
    [propertyAnim setRepeatCount:repeatCount];
    [propertyAnim setAutoreverses:anim->direction()];
    [propertyAnim setRemovedOnCompletion:NO];
    [propertyAnim setAdditive:additive];
    [propertyAnim setFillMode:@"extended"];

    [propertyAnim setDelegate:m_animationDelegate.get()];
}

CAMediaTimingFunction* GraphicsLayerCA::timingFunctionForAnimationValue(const AnimationValue* animValue, const Animation* anim)
{
    const TimingFunction* tf = 0;
    if (animValue->timingFunction())
        tf = animValue->timingFunction();
    else if (anim->isTimingFunctionSet())
        tf = &anim->timingFunction();

    return getCAMediaTimingFunction(tf ? *tf : TimingFunction());
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
    if (NSString* valueFunctionName = getValueFunctionNameForTransformOperation(transformOp))
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
    if (NSString* valueFunctionName = getValueFunctionNameForTransformOperation(transformOpType))
        [keyframeAnim setValueFunction:[CAValueFunction functionWithName:valueFunctionName]];
#endif
    return true;
}

void GraphicsLayerCA::suspendAnimations(double time)
{
    double t = currentTimeToMediaTime(time ? time : currentTime());
    [primaryLayer() setSpeed:0];
    [primaryLayer() setTimeOffset:t];
}

void GraphicsLayerCA::resumeAnimations()
{
    [primaryLayer() setSpeed:1];
    [primaryLayer() setTimeOffset:0];
}

WebLayer* GraphicsLayerCA::hostLayerForSublayers() const
{
    return m_transformLayer ? m_transformLayer.get() : m_layer.get();
}

WebLayer* GraphicsLayerCA::layerForSuperlayer() const
{
    if (m_transformLayer)
        return m_transformLayer.get();

    return m_layer.get();
}

CALayer* GraphicsLayerCA::animatedLayer(AnimatedPropertyID property) const
{
    return (property == AnimatedPropertyBackgroundColor) ? m_contentsLayer.get() : primaryLayer();
}

PlatformLayer* GraphicsLayerCA::platformLayer() const
{
    return primaryLayer();
}

#ifndef NDEBUG
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
#endif // NDEBUG

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

        if (GraphicsLayer::compositingCoordinatesOrientation() == GraphicsLayer::CompositingCoordinatesBottomUp)
            [tiledLayer setContentsGravity:@"bottomLeft"];
        else
            [tiledLayer setContentsGravity:@"topLeft"];

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
    [m_layer.get() setSublayers:[oldLayer.get() sublayers]];
    
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
    moveAnimation(AnimatedPropertyWebkitTransform, oldLayer.get(), m_layer.get());
    moveAnimation(AnimatedPropertyOpacity, oldLayer.get(), m_layer.get());
    moveAnimation(AnimatedPropertyBackgroundColor, oldLayer.get(), m_layer.get());
    
    // need to tell new layer to draw itself
    setNeedsDisplay();
    
#ifndef NDEBUG
    updateDebugIndicators();
#endif
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
#else
    ASSERT(contentsOrientation() == CompositingCoordinatesTopDown);
#endif
}

void GraphicsLayerCA::setupContentsLayer(CALayer* contentsLayer)
{
    // Turn off implicit animations on the inner layer.
    [contentsLayer setStyle:[NSDictionary dictionaryWithObject:nullActionsDictionary() forKey:@"actions"]];

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

#ifndef NDEBUG
    if (showDebugBorders()) {
        setLayerBorderColor(contentsLayer, Color(0, 0, 128, 180));
        [contentsLayer setBorderWidth:1.0f];
    }
#endif
}

void GraphicsLayerCA::setOpacityInternal(float accumulatedOpacity)
{
    [(preserves3D() ? m_layer.get() : primaryLayer()) setOpacity:accumulatedOpacity];
}

void GraphicsLayerCA::updateOpacityOnLayer()
{
#if !HAVE_MODERN_QUARTZCORE
    // Distribute opacity either to our own layer or to our children. We pass in the 
    // contribution from our parent(s).
    distributeOpacity(parent() ? parent()->accumulatedOpacity() : 1);
#else
    [primaryLayer() setOpacity:m_opacity];
#endif
}

void GraphicsLayerCA::noteLayerPropertyChanged(LayerChangeFlags flags)
{
    if (!m_uncommittedChanges && m_client)
        m_client->notifySyncRequired(this);

    m_uncommittedChanges |= flags;
}

#if ENABLE(3D_CANVAS)
void GraphicsLayerCA::setGraphicsContext3DNeedsDisplay()
{
    if (m_contentsLayerPurpose == ContentsLayerForGraphicsLayer3D)
        [m_contentsLayer.get() setNeedsDisplay];
}
#endif
    
} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
