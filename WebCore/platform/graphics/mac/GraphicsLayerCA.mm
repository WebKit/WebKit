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

static NSString* const WebAnimationCSSPropertyKey = @"GraphicsLayerCA_property";

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

static NSValue* getTransformFunctionValue(const GraphicsLayer::TransformValue& transformValue, size_t index, const IntSize& size, TransformOperation::OperationType transformType)
{
    TransformOperation* op = (index >= transformValue.value()->operations().size()) ? 0 : transformValue.value()->operations()[index].get();
    
    switch (transformType) {
        case TransformOperation::ROTATE:
        case TransformOperation::ROTATE_X:
        case TransformOperation::ROTATE_Y:
            return [NSNumber numberWithDouble:op ? deg2rad(static_cast<RotateTransformOperation*>(op)->angle()) : 0];
        case TransformOperation::SCALE_X:
            return [NSNumber numberWithDouble:op ? static_cast<ScaleTransformOperation*>(op)->x() : 0];
        case TransformOperation::SCALE_Y:
            return [NSNumber numberWithDouble:op ? static_cast<ScaleTransformOperation*>(op)->y() : 0];
        case TransformOperation::SCALE_Z:
            return [NSNumber numberWithDouble:op ? static_cast<ScaleTransformOperation*>(op)->z() : 0];
        case TransformOperation::TRANSLATE_X:
            return [NSNumber numberWithDouble:op ? static_cast<TranslateTransformOperation*>(op)->x(size) : 0];
        case TransformOperation::TRANSLATE_Y:
            return [NSNumber numberWithDouble:op ? static_cast<TranslateTransformOperation*>(op)->y(size) : 0];
        case TransformOperation::TRANSLATE_Z:
            return [NSNumber numberWithDouble:op ? static_cast<TranslateTransformOperation*>(op)->z(size) : 0];
        case TransformOperation::SCALE:
        case TransformOperation::TRANSLATE:
        case TransformOperation::SKEW_X:
        case TransformOperation::SKEW_Y:
        case TransformOperation::SKEW:
        case TransformOperation::MATRIX:
        case TransformOperation::SCALE_3D:
        case TransformOperation::TRANSLATE_3D:
        case TransformOperation::ROTATE_3D:
        case TransformOperation::MATRIX_3D:
        case TransformOperation::PERSPECTIVE:
        case TransformOperation::IDENTITY:
        case TransformOperation::NONE: {
            TransformationMatrix t;
            if (op)
                op->apply(t, size);
            CATransform3D cat;
            copyTransform(cat, t);
            return [NSValue valueWithCATransform3D:cat];
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
        default:
            return nil;
    }
}
#endif

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

static CALayer* getPresentationLayer(CALayer* layer)
{
    CALayer*  presLayer = [layer presentationLayer];
    if (!presLayer)
        presLayer = layer;

    return presLayer;
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
#ifndef NDEBUG
                             nullValue, @"zPosition",
#endif
                             nil];
    return actions;
}

GraphicsLayer* GraphicsLayer::createGraphicsLayer(GraphicsLayerClient* client)
{
    return new GraphicsLayerCA(client);
}

GraphicsLayerCA::GraphicsLayerCA(GraphicsLayerClient* client)
: GraphicsLayer(client)
, m_contentLayerForImageOrVideo(false)
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
    // Remove a inner layer if there is one.
    clearContents();

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    // Clean up the WK layer.
    if (m_layer) {
        WebLayer* layer = m_layer.get();
        [layer setLayerOwner:nil];
        [layer removeFromSuperlayer];
    }
    
    if (m_transformLayer)
        [m_transformLayer.get() removeFromSuperlayer];

    // animationDidStart: can fire after this, so we need to clear out the layer on the delegate.
    [m_animationDelegate.get() setLayer:0];

    END_BLOCK_OBJC_EXCEPTIONS
}

void GraphicsLayerCA::setName(const String& name)
{
    String longName = String::format("CALayer(%p) GraphicsLayer(%p) ", m_layer.get(), this) + name;
    GraphicsLayer::setName(longName);
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setName:name];
    END_BLOCK_OBJC_EXCEPTIONS
}

NativeLayer GraphicsLayerCA::nativeLayer() const
{
    return m_layer.get();
}

void GraphicsLayerCA::addChild(GraphicsLayer* childLayer)
{
    GraphicsLayer::addChild(childLayer);

    GraphicsLayerCA* childLayerCA = static_cast<GraphicsLayerCA*>(childLayer);
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [hostLayerForSublayers() addSublayer:childLayerCA->layerForSuperlayer()];
    END_BLOCK_OBJC_EXCEPTIONS
}

void GraphicsLayerCA::addChildAtIndex(GraphicsLayer* childLayer, int index)
{
    GraphicsLayer::addChildAtIndex(childLayer, index);

    GraphicsLayerCA* childLayerCA = static_cast<GraphicsLayerCA*>(childLayer);
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [hostLayerForSublayers() insertSublayer:childLayerCA->layerForSuperlayer() atIndex:index];
    END_BLOCK_OBJC_EXCEPTIONS
}

void GraphicsLayerCA::addChildBelow(GraphicsLayer* childLayer, GraphicsLayer* sibling)
{
    // FIXME: share code with base class
    ASSERT(childLayer != this);
    childLayer->removeFromParent();

    bool found = false;
    for (unsigned i = 0; i < m_children.size(); i++) {
        if (sibling == m_children[i]) {
            m_children.insert(i, childLayer);
            found = true;
            break;
        }
    }
    childLayer->setParent(this);

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    GraphicsLayerCA* childLayerCA = static_cast<GraphicsLayerCA*>(childLayer);
    GraphicsLayerCA* siblingLayerCA = static_cast<GraphicsLayerCA*>(sibling);
    if (found)
        [hostLayerForSublayers() insertSublayer:childLayerCA->layerForSuperlayer() below:siblingLayerCA->layerForSuperlayer()];
    else {
        m_children.append(childLayer);
        [hostLayerForSublayers() addSublayer:childLayerCA->layerForSuperlayer()];
    }

    END_BLOCK_OBJC_EXCEPTIONS
}

void GraphicsLayerCA::addChildAbove(GraphicsLayer* childLayer, GraphicsLayer* sibling)
{
    // FIXME: share code with base class
    ASSERT(childLayer != this);
    childLayer->removeFromParent();

    unsigned i;
    bool found = false;
    for (i = 0; i < m_children.size(); i++) {
        if (sibling == m_children[i]) {
            m_children.insert(i+1, childLayer);
            found = true;
            break;
        }
    }
    childLayer->setParent(this);

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    GraphicsLayerCA* childLayerCA = static_cast<GraphicsLayerCA*>(childLayer);
    GraphicsLayerCA* siblingLayerCA = static_cast<GraphicsLayerCA*>(sibling);
    if (found) {
        [hostLayerForSublayers() insertSublayer:childLayerCA->layerForSuperlayer() above:siblingLayerCA->layerForSuperlayer()];
    } else {
        m_children.append(childLayer);
        [hostLayerForSublayers() addSublayer:childLayerCA->layerForSuperlayer()];
    }

    END_BLOCK_OBJC_EXCEPTIONS
}

bool GraphicsLayerCA::replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild)
{
    // FIXME: share code with base class
    ASSERT(!newChild->parent());

    bool found = false;
    for (unsigned i = 0; i < m_children.size(); i++) {
        if (oldChild == m_children[i]) {
            m_children[i] = newChild;
            found = true;
            break;
        }
    }

    if (found) {
        oldChild->setParent(0);

        newChild->removeFromParent();
        newChild->setParent(this);

        BEGIN_BLOCK_OBJC_EXCEPTIONS
        GraphicsLayerCA* oldChildCA = static_cast<GraphicsLayerCA*>(oldChild);
        GraphicsLayerCA* newChildCA = static_cast<GraphicsLayerCA*>(newChild);
        [hostLayerForSublayers() replaceSublayer:oldChildCA->layerForSuperlayer() with:newChildCA->layerForSuperlayer()];
        END_BLOCK_OBJC_EXCEPTIONS
        return true;
    }
    return false;
}

void GraphicsLayerCA::removeFromParent()
{
    GraphicsLayer::removeFromParent();

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [layerForSuperlayer() removeFromSuperlayer];            
    END_BLOCK_OBJC_EXCEPTIONS
}

void GraphicsLayerCA::setPosition(const FloatPoint& point)
{
    // Don't short-circuit here, because position and anchor point are inter-dependent.
    GraphicsLayer::setPosition(point);

    // Position is offset on the layer by the layer anchor point.
    CGPoint posPoint = CGPointMake(m_position.x() + m_anchorPoint.x() * m_size.width(),
                                   m_position.y() + m_anchorPoint.y() * m_size.height());
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [primaryLayer() setPosition:posPoint];
    END_BLOCK_OBJC_EXCEPTIONS
}

void GraphicsLayerCA::setAnchorPoint(const FloatPoint3D& point)
{
    // Don't short-circuit here, because position and anchor point are inter-dependent.
    bool zChanged = (point.z() != m_anchorPoint.z());
    GraphicsLayer::setAnchorPoint(point);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    // set the value on the layer to the new transform.
    [primaryLayer() setAnchorPoint:FloatPoint(point.x(), point.y())];

    if (zChanged) {
#if HAVE_MODERN_QUARTZCORE
        [primaryLayer() setAnchorPointZ:m_anchorPoint.z()];
#endif
    }

    // Position depends on anchor point, so update it now.
    setPosition(m_position);
    END_BLOCK_OBJC_EXCEPTIONS
}

void GraphicsLayerCA::setSize(const FloatSize& size)
{
    GraphicsLayer::setSize(size);

    CGRect rect = CGRectMake(0.0f,
                             0.0f,
                             m_size.width(),
                             m_size.height());
    
    CGPoint centerPoint = CGPointMake(m_size.width() / 2.0f, m_size.height() / 2.0f);
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    if (m_transformLayer) {
        [m_transformLayer.get() setBounds:rect];
        // The anchor of the contents layer is always at 0.5, 0.5, so the position
        // is center-relative.
        [m_layer.get() setPosition:centerPoint];
    }
    
    bool needTiledLayer = requiresTiledLayer(m_size);
    if (needTiledLayer != m_usingTiledLayer)
        swapFromOrToTiledLayer(needTiledLayer);
    
    [m_layer.get() setBounds:rect];
    updateContentsTransform();

    // Note that we don't resize m_contentsLayer. It's up the caller to do that.

    END_BLOCK_OBJC_EXCEPTIONS

    // if we've changed the bounds, we need to recalculate the position
    // of the layer, taking anchor point into account
    setPosition(m_position);
}

void GraphicsLayerCA::setTransform(const TransformationMatrix& t)
{
    GraphicsLayer::setTransform(t);
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    CATransform3D transform;
    copyTransform(transform, t);
    [primaryLayer() setTransform:transform];
    END_BLOCK_OBJC_EXCEPTIONS
    
    // Remove any old transition entries for transform.
    removeAllAnimationsForProperty(AnimatedPropertyWebkitTransform);

    // Even if we don't have a transition in the list, the layer may still have one.
    // This happens when we are setting the final transform value after an animation or
    // transition has ended. In removeAnimation we toss the entry from the list but don't
    // remove it from the list. That way we aren't in danger of displaying a stale transform
    // in the time between removing the animation and setting the new unanimated value. We 
    // can't do this in removeAnimation because we don't know the new transform value there.
    String keyPath = propertyIdToString(AnimatedPropertyWebkitTransform);
    CALayer* layer = animatedLayer(AnimatedPropertyWebkitTransform);
    
    for (int i = 0; ; ++i) {
        String animName = keyPath + "_" + String::number(i);
        if (![layer animationForKey: animName])
            break;
        [layer removeAnimationForKey:animName];
    }
}

void GraphicsLayerCA::setChildrenTransform(const TransformationMatrix& t)
{
    if (t == m_childrenTransform)
        return;

    GraphicsLayer::setChildrenTransform(t);

    CATransform3D transform;
    copyTransform(transform, t);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    // Set the value on the layer to the new transform.
    [primaryLayer() setSublayerTransform:transform];
    END_BLOCK_OBJC_EXCEPTIONS
}

static void moveAnimation(AnimatedPropertyID property, CALayer* fromLayer, CALayer* toLayer)
{
    String keyPath = GraphicsLayer::propertyIdToString(property);
    for (short index = 0; ; ++index) {
        String animName = keyPath + "_" + String::number(index);
        CAAnimation* anim = [fromLayer animationForKey:animName];
        if (!anim)
            break;

        [anim retain];
        [fromLayer removeAnimationForKey:animName];
        [toLayer addAnimation:anim forKey:animName];
        [anim release];
    }
}

static void moveSublayers(CALayer* fromLayer, CALayer* toLayer)
{
    NSArray* sublayersCopy = [[fromLayer sublayers] copy]; // Avoid mutation while enumerating, and keep the sublayers alive.
    NSEnumerator* childrenEnumerator = [sublayersCopy objectEnumerator];

    CALayer* layer;
    while ((layer = [childrenEnumerator nextObject]) != nil) {
        [layer removeFromSuperlayer];
        [toLayer addSublayer:layer];
    }
    [sublayersCopy release];
}

void GraphicsLayerCA::setPreserves3D(bool preserves3D)
{
    GraphicsLayer::setPreserves3D(preserves3D);

    CGPoint point = CGPointMake(m_size.width() / 2.0f, m_size.height() / 2.0f);
    CGPoint centerPoint = CGPointMake(0.5f, 0.5f);
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    Class transformLayerClass = NSClassFromString(@"CATransformLayer");
    if (preserves3D && !m_transformLayer && transformLayerClass) {
        // Create the transform layer.
        m_transformLayer.adoptNS([[transformLayerClass alloc] init]);

        // Turn off default animations.
        [m_transformLayer.get() setStyle:[NSDictionary dictionaryWithObject:nullActionsDictionary() forKey:@"actions"]];
        
#ifndef NDEBUG
        [m_transformLayer.get() setName:[NSString stringWithFormat:@"Transform Layer CATransformLayer(%p) GraphicsLayer(%p)", m_transformLayer.get(), this]];
#endif
        // Copy the position from this layer.
        [m_transformLayer.get() setBounds:[m_layer.get() bounds]];
        [m_transformLayer.get() setPosition:[m_layer.get() position]];
        [m_transformLayer.get() setAnchorPoint:[m_layer.get() anchorPoint]];
#if HAVE_MODERN_QUARTZCORE
        [m_transformLayer.get() setAnchorPointZ:[m_layer.get() anchorPointZ]];
#endif
        [m_transformLayer.get() setContentsRect:[m_layer.get() contentsRect]];
#ifndef NDEBUG
        [m_transformLayer.get() setZPosition:[m_layer.get() zPosition]];
#endif
        
        // The contents layer is positioned at (0,0) relative to the transformLayer.
        [m_layer.get() setPosition:point];
        [m_layer.get() setAnchorPoint:centerPoint];
#ifndef NDEBUG
        [m_layer.get() setZPosition:0.0f];
#endif
        
        // Transfer the transform over.
        [m_transformLayer.get() setTransform:[m_layer.get() transform]];
        [m_layer.get() setTransform:CATransform3DIdentity];
        
        // Transfer the opacity from the old layer to the transform layer.
        [m_transformLayer.get() setOpacity:m_opacity];
        [m_layer.get() setOpacity:1];

        // Move this layer to be a child of the transform layer.
        [[m_layer.get() superlayer] replaceSublayer:m_layer.get() with:m_transformLayer.get()];
        [m_transformLayer.get() addSublayer:m_layer.get()];

        moveAnimation(AnimatedPropertyWebkitTransform, m_layer.get(), m_transformLayer.get());
        moveSublayers(m_layer.get(), m_transformLayer.get());

    } else if (!preserves3D && m_transformLayer) {
        // Relace the transformLayer in the parent with this layer.
        [m_layer.get() removeFromSuperlayer];
        [[m_transformLayer.get() superlayer] replaceSublayer:m_transformLayer.get() with:m_layer.get()];

        moveAnimation(AnimatedPropertyWebkitTransform, m_transformLayer.get(), m_layer.get());
        moveSublayers(m_transformLayer.get(), m_layer.get());

        // Reset the layer position and transform.
        [m_layer.get() setPosition:[m_transformLayer.get() position]];
        [m_layer.get() setAnchorPoint:[m_transformLayer.get() anchorPoint]];
#if HAVE_MODERN_QUARTZCORE
        [m_layer.get() setAnchorPointZ:[m_transformLayer.get() anchorPointZ]];
#endif
        [m_layer.get() setContentsRect:[m_transformLayer.get() contentsRect]];
        [m_layer.get() setTransform:[m_transformLayer.get() transform]];
        [m_layer.get() setOpacity:[m_transformLayer.get() opacity]];
#ifndef NDEBUG
        [m_layer.get() setZPosition:[m_transformLayer.get() zPosition]];
#endif

        // Release the transform layer.
        m_transformLayer = 0;
    }

    END_BLOCK_OBJC_EXCEPTIONS
}

void GraphicsLayerCA::setMasksToBounds(bool masksToBounds)
{
    if (masksToBounds == m_masksToBounds)
        return;

    GraphicsLayer::setMasksToBounds(masksToBounds);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setMasksToBounds:masksToBounds];
    END_BLOCK_OBJC_EXCEPTIONS

#ifndef NDEBUG
    updateDebugIndicators();
#endif
}

void GraphicsLayerCA::setDrawsContent(bool drawsContent)
{
    if (drawsContent != m_drawsContent) {
        GraphicsLayer::setDrawsContent(drawsContent);

        bool needTiledLayer = requiresTiledLayer(m_size);
        if (needTiledLayer != m_usingTiledLayer)
            swapFromOrToTiledLayer(needTiledLayer);

        BEGIN_BLOCK_OBJC_EXCEPTIONS
        if (m_drawsContent)
            [m_layer.get() setNeedsDisplay];
        else
            [m_layer.get() setContents:nil];
        
#ifndef NDEBUG
        updateDebugIndicators();
#endif
        END_BLOCK_OBJC_EXCEPTIONS
    }
}

void GraphicsLayerCA::setBackgroundColor(const Color& color, const Animation* transition, double beginTime)
{
    GraphicsLayer::setBackgroundColor(color, transition, beginTime);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    
    if (!m_contentsLayer.get()) {
        WebLayer* colorLayer = [WebLayer layer];
#ifndef NDEBUG
        [colorLayer setName:@"Color Layer"];
#endif
        setContentsLayer(colorLayer);
    }
    
    if (transition && !transition->isEmptyOrZeroDuration()) {
        CALayer* presLayer = [m_contentsLayer.get() presentationLayer];
        // If we don't have a presentationLayer, just use the CALayer
        if (!presLayer)
            presLayer = m_contentsLayer.get();

        // Get the current value of the background color from the layer
        CGColorRef fromBackgroundColor = [presLayer backgroundColor];

        CGColorRef bgColor = createCGColor(color);
        setBasicAnimation(AnimatedPropertyBackgroundColor, TransformOperation::NONE, 0, fromBackgroundColor, bgColor, true, transition, beginTime);
        CGColorRelease(bgColor);
    } else {
        removeAllAnimationsForProperty(AnimatedPropertyBackgroundColor);    
        setLayerBackgroundColor(m_contentsLayer.get(), m_backgroundColor);
    }

    END_BLOCK_OBJC_EXCEPTIONS
}

void GraphicsLayerCA::clearBackgroundColor()
{
    if (!m_contentLayerForImageOrVideo)
        setContentsLayer(0);
    else
        clearLayerBackgroundColor(m_contentsLayer.get());
}

void GraphicsLayerCA::setContentsOpaque(bool opaque)
{
    GraphicsLayer::setContentsOpaque(opaque);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setOpaque:m_contentsOpaque];
    END_BLOCK_OBJC_EXCEPTIONS
}

void GraphicsLayerCA::setBackfaceVisibility(bool visible)
{
    if (m_backfaceVisibility == visible)
        return;
    
    GraphicsLayer::setBackfaceVisibility(visible);
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setDoubleSided:visible];
    END_BLOCK_OBJC_EXCEPTIONS
}

bool GraphicsLayerCA::setOpacity(float opacity, const Animation* transition, double beginTime)
{
    if (forceSoftwareAnimation())
        return false;
        
    float clampedOpacity = max(0.0f, min(opacity, 1.0f));
    
    bool opacitiesDiffer = (m_opacity != clampedOpacity);
    
    GraphicsLayer::setOpacity(clampedOpacity, transition, beginTime);
    
    int animIndex = findAnimationEntry(AnimatedPropertyOpacity, 0);
        
    // If we don't have a transition just set the opacity
    if (!transition || transition->isEmptyOrZeroDuration()) {
        // Three cases:
        //  1) no existing animation or transition: just set opacity
        //  2) existing transition: clear it and set opacity
        //  3) existing animation: just return
        //
        if (animIndex < 0 || m_animations[animIndex].isTransition()) {
            BEGIN_BLOCK_OBJC_EXCEPTIONS
            [primaryLayer() setOpacity:opacity];
            if (animIndex >= 0) {
                removeAllAnimationsForProperty(AnimatedPropertyOpacity);
                animIndex = -1;
            } else {
                String keyPath = propertyIdToString(AnimatedPropertyOpacity);
                
                // FIXME: using hardcoded '0' here. We should be clearing all animations. For now there is only 1.
                String animName = keyPath + "_0";
                [animatedLayer(AnimatedPropertyOpacity) removeAnimationForKey:animName];
            }
            END_BLOCK_OBJC_EXCEPTIONS
        } else {
            // We have an animation, so don't set the opacity directly.
            return false;
        }
    } else {
        // At this point, we know we have a transition. But if it is the same and
        // the opacity value has not changed, don't do anything.
        if (!opacitiesDiffer &&
                ((animIndex == -1) ||    
                 (m_animations[animIndex].isTransition() && *(m_animations[animIndex].animation()) == *transition))) {
            return false;
        }
    }
    
    // If an animation is running, ignore this transition, but still save the value.
    if (animIndex >= 0 && !m_animations[animIndex].isTransition())
        return false;

    bool didAnimate = false;
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    
    NSNumber* fromOpacityValue = nil;
    NSNumber* toOpacityValue = [NSNumber numberWithFloat:opacity];
    
    if (transition && !transition->isEmptyOrZeroDuration()) {
        CALayer* presLayer = getPresentationLayer(primaryLayer());
        float fromOpacity = [presLayer opacity];
        fromOpacityValue = [NSNumber numberWithFloat:fromOpacity];
        setBasicAnimation(AnimatedPropertyOpacity, TransformOperation::NONE, 0, fromOpacityValue, toOpacityValue, true, transition, beginTime);
        didAnimate = true;
    }

    END_BLOCK_OBJC_EXCEPTIONS
    
    return didAnimate;
}

void GraphicsLayerCA::setNeedsDisplay()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    
    if (drawsContent())
        [m_layer.get() setNeedsDisplay];

    END_BLOCK_OBJC_EXCEPTIONS
}

void GraphicsLayerCA::setNeedsDisplayInRect(const FloatRect& rect)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    
    if (drawsContent())
        [m_layer.get() setNeedsDisplayInRect:rect];

    END_BLOCK_OBJC_EXCEPTIONS
}


bool GraphicsLayerCA::animateTransform(const TransformValueList& valueList, const IntSize& size, const Animation* anim, double beginTime, bool isTransition)
{
    if (forceSoftwareAnimation() || !anim || anim->isEmptyOrZeroDuration() || valueList.size() < 2)
        return false;
        
    TransformValueList::FunctionList functionList;
    bool isValid, hasBigRotation;
    valueList.makeFunctionList(functionList, isValid, hasBigRotation);

    // We need to fall back to software animation if we don't have setValueFunction:, and
    // we would need to animate each incoming transform function separately. This is the
    // case if we have a rotation >= 180 or we have more than one transform function.
    if ((hasBigRotation || functionList.size() > 1) && !caValueFunctionSupported())
        return false;
    
    bool success = true;
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    
    // Rules for animation:
    //
    //  1)  If functionList is empty or we don't have a big rotation, we do a matrix animation. We could
    //      use component animation for lists without a big rotation, but there is no need to, and this
    //      is more efficient.
    //
    //  2)  Otherwise we do a component hardware animation.
    bool isMatrixAnimation = !isValid || !hasBigRotation;
        
    // Set transform to identity since we are animating components and we need the base
    // to be the identity transform.
    TransformationMatrix t;
    CATransform3D toT3D;
    copyTransform(toT3D, t);
    [primaryLayer() setTransform:toT3D];
        
    bool isKeyframe = valueList.size() > 2;
        
    // Iterate through the transform functions, sending an animation for each one.
    for (int functionIndex = 0; ; ++functionIndex) {
        if (functionIndex >= static_cast<int>(functionList.size()) && !isMatrixAnimation)
            break;
            
        TransformOperation::OperationType opType = isMatrixAnimation ? TransformOperation::MATRIX_3D : functionList[functionIndex];

        if (isKeyframe) {
            RetainPtr<NSMutableArray> timesArray(AdoptNS, [[NSMutableArray alloc] init]);
            RetainPtr<NSMutableArray> valArray(AdoptNS, [[NSMutableArray alloc] init]);
            RetainPtr<NSMutableArray> tfArray(AdoptNS, [[NSMutableArray alloc] init]);
            
            // Iterate through the keyframes, building arrays for the animation.
            for (Vector<TransformValue>::const_iterator it = valueList.values().begin(); it != valueList.values().end(); ++it) {
                const TransformValue& curValue = (*it);
                
                // fill in the key time and timing function
                [timesArray.get() addObject:[NSNumber numberWithFloat:curValue.key()]];
                
                const TimingFunction* tf = 0;
                if (curValue.timingFunction())
                    tf = curValue.timingFunction();
                else if (anim->isTimingFunctionSet())
                    tf = &anim->timingFunction();
                
                CAMediaTimingFunction* timingFunction = getCAMediaTimingFunction(tf ? *tf : TimingFunction(LinearTimingFunction));
                [tfArray.get() addObject:timingFunction];
                
                // fill in the function
                if (isMatrixAnimation) {
                    TransformationMatrix t;
                    curValue.value()->apply(size, t);
                    
                    // If any matrix is singular, CA won't animate it correctly. So fall back to software animation
                    if (!t.isInvertible()) {
                        success = false;
                        break;
                    }
                    
                    CATransform3D cat;
                    copyTransform(cat, t);
                    [valArray.get() addObject:[NSValue valueWithCATransform3D:cat]];
                } else
                    [valArray.get() addObject:getTransformFunctionValue(curValue, functionIndex, size, opType)];
            }
            
            if (success) {
                // We toss the last tfArray value because it has to be one shorter than the others.
                [tfArray.get() removeLastObject];
            
                setKeyframeAnimation(AnimatedPropertyWebkitTransform, opType, functionIndex, timesArray.get(), valArray.get(), tfArray.get(), isTransition, anim, beginTime);
            }
        } else {
            // Is a transition
            id fromValue = 0;
            id toValue = 0;
            
            if (isMatrixAnimation) {
                TransformationMatrix fromt, tot;
                valueList.at(0).value()->apply(size, fromt);
                valueList.at(1).value()->apply(size, tot);

                // If any matrix is singular, CA won't animate it correctly. So fall back to software animation
                if (!fromt.isInvertible() || !tot.isInvertible())
                    success = false;
                else {
                    CATransform3D cat;
                    copyTransform(cat, fromt);
                    fromValue = [NSValue valueWithCATransform3D:cat];
                    copyTransform(cat, tot);
                    toValue = [NSValue valueWithCATransform3D:cat];
                }
            } else {
                fromValue = getTransformFunctionValue(valueList.at(0), functionIndex, size, opType);
                toValue = getTransformFunctionValue(valueList.at(1), functionIndex, size, opType);
            }
            
            if (success)
                setBasicAnimation(AnimatedPropertyWebkitTransform, opType, functionIndex, fromValue, toValue, isTransition, anim, beginTime);
        }
        
        if (isMatrixAnimation)
            break;
    }

    END_BLOCK_OBJC_EXCEPTIONS
    return success;
}

bool GraphicsLayerCA::animateFloat(AnimatedPropertyID property, const FloatValueList& valueList, const Animation* animation, double beginTime)
{
    if (forceSoftwareAnimation() || valueList.size() < 2)
        return false;
        
    // if there is already is an animation for this property and it hasn't changed, ignore it.
    int i = findAnimationEntry(property, 0);
    if (i >= 0 && *m_animations[i].animation() == *animation) {
        m_animations[i].setIsCurrent();
        return false;
    }

    if (valueList.size() == 2) {
        float fromVal = valueList.at(0).value();
        float toVal   = valueList.at(1).value();
        if (isnan(toVal) && isnan(fromVal))
            return false;
    
        // initialize the property to 0
        [animatedLayer(property) setValue:0 forKeyPath:propertyIdToString(property)];
        setBasicAnimation(property, TransformOperation::NONE, 0, isnan(fromVal) ? nil : [NSNumber numberWithFloat:fromVal], isnan(toVal) ? nil : [NSNumber numberWithFloat:toVal], false, animation, beginTime);
        return true;
    }

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    
    RetainPtr<NSMutableArray> timesArray(AdoptNS, [[NSMutableArray alloc] init]);
    RetainPtr<NSMutableArray> valArray(AdoptNS, [[NSMutableArray alloc] init]);
    RetainPtr<NSMutableArray> tfArray(AdoptNS, [[NSMutableArray alloc] init]);

    for (unsigned i = 0; i < valueList.values().size(); ++i) {
        const FloatValue& curValue = valueList.values()[i];
        [timesArray.get() addObject:[NSNumber numberWithFloat:curValue.key()]];
        [valArray.get() addObject:[NSNumber numberWithFloat:curValue.value()]];

        const TimingFunction* tf = 0;
        if (curValue.timingFunction())
            tf = curValue.timingFunction();
        else if (animation->isTimingFunctionSet())
            tf = &animation->timingFunction();

        CAMediaTimingFunction* timingFunction = getCAMediaTimingFunction(tf ? *tf : TimingFunction());
        [tfArray.get() addObject:timingFunction];
    }
    
    // We toss the last tfArray value because it has to one shorter than the others.
    [tfArray.get() removeLastObject];
    
    // Initialize the property to 0.
    [animatedLayer(property) setValue:0 forKeyPath:propertyIdToString(property)];
    // Then set the animation.
    setKeyframeAnimation(property, TransformOperation::NONE, 0, timesArray.get(), valArray.get(), tfArray.get(), false, animation, beginTime);

    END_BLOCK_OBJC_EXCEPTIONS
    return true;
}

void GraphicsLayerCA::setContentsToImage(Image* image)
{
    if (image) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        {
            if (!m_contentsLayer.get()) {
                WebLayer* imageLayer = [WebLayer layer];
#ifndef NDEBUG
                [imageLayer setName:@"Image Layer"];
#endif
                setContentsLayer(imageLayer);
            }

            // FIXME: maybe only do trilinear if the image is being scaled down,
            // but then what if the layer size changes?
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
            [m_contentsLayer.get() setMinificationFilter:kCAFilterTrilinear];
#endif
            RetainPtr<CGImageRef> theImage(image->nativeImageForCurrentFrame());
            CGColorSpaceRef colorSpace = CGImageGetColorSpace(theImage.get());

            static CGColorSpaceRef deviceRGB = CGColorSpaceCreateDeviceRGB();
            if (CFEqual(colorSpace, deviceRGB)) {
                // CoreGraphics renders images tagged with DeviceRGB using GenericRGB. When we hand such
                // images to CA we need to tag them similarly so CA rendering matches CG rendering.
                static CGColorSpaceRef genericRGB = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
                theImage.adoptCF(CGImageCreateCopyWithColorSpace(theImage.get(), genericRGB));
            }

            [m_contentsLayer.get() setContents:(id)theImage.get()];
        }
        END_BLOCK_OBJC_EXCEPTIONS
    } else
        setContentsLayer(0);

    m_contentLayerForImageOrVideo = (image != 0);
}

void GraphicsLayerCA::setContentsToVideo(PlatformLayer* videoLayer)
{
    setContentsLayer(videoLayer);
    m_contentLayerForImageOrVideo = (videoLayer != 0);
}

void GraphicsLayerCA::clearContents()
{
    if (m_contentLayerForImageOrVideo) {
        setContentsLayer(0);
        m_contentLayerForImageOrVideo = false;
    }
}

void GraphicsLayerCA::updateContentsRect()
{
    if (m_client && m_contentsLayer) {
        IntRect contentRect = m_client->contentsBox(this);
        
        CGPoint point = CGPointMake(contentRect.x(),
                                    contentRect.y());
        CGRect rect = CGRectMake(0.0f,
                                 0.0f,
                                 contentRect.width(),
                                 contentRect.height());

        BEGIN_BLOCK_OBJC_EXCEPTIONS
        [m_contentsLayer.get() setPosition:point];
        [m_contentsLayer.get() setBounds:rect];
        END_BLOCK_OBJC_EXCEPTIONS
    }
}

void GraphicsLayerCA::setGeometryOrientation(CompositingCoordinatesOrientation orientation)
{
    switch (orientation) {
    case CompositingCoordinatesTopDown:
#if HAVE_MODERN_QUARTZCORE
        [m_layer.get() setGeometryFlipped:NO];
#else
        setChildrenTransform(TransformationMatrix());
#endif
        break;
        
    case CompositingCoordinatesBottomUp:
#if HAVE_MODERN_QUARTZCORE
        [m_layer.get() setGeometryFlipped:YES];
#else
        setChildrenTransform(flipTransform());
#endif
        break;
    }
}

GraphicsLayerCA::CompositingCoordinatesOrientation GraphicsLayerCA::geometryOrientation() const
{
    // CoreAnimation defaults to bottom-up
    bool layerFlipped;
#if HAVE_MODERN_QUARTZCORE
    layerFlipped = [m_layer.get() isGeometryFlipped];
#else
    layerFlipped = childrenTransform().m22() == -1;
#endif
    return layerFlipped ? CompositingCoordinatesBottomUp : CompositingCoordinatesTopDown;
}

void GraphicsLayerCA::setBasicAnimation(AnimatedPropertyID property, TransformOperation::OperationType operationType, short index, void* fromVal, void* toVal, bool isTransition, const Animation* transition, double beginTime)
{
    ASSERT(fromVal || toVal);

    WebLayer* layer = animatedLayer(property);
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    // add an entry for this animation
    addAnimationEntry(property, index, isTransition, transition);

    String keyPath = propertyIdToString(property);
    String animName = keyPath + "_" + String::number(index);

    CABasicAnimation* basicAnim = [CABasicAnimation animationWithKeyPath:keyPath];
    
    double duration = transition->duration();
    if (duration <= 0)
        duration = cAnimationAlmostZeroDuration;
        
    float repeatCount = transition->iterationCount();
    if (repeatCount == Animation::IterationCountInfinite)
        repeatCount = FLT_MAX;
    else if (transition->direction() == Animation::AnimationDirectionAlternate)
        repeatCount /= 2;
        
    [basicAnim setDuration:duration];
    [basicAnim setRepeatCount:repeatCount];
    [basicAnim setAutoreverses:transition->direction()];
    [basicAnim setRemovedOnCompletion:NO];

    // Note that currently transform is the only property which has animations
    // with an index > 0.
    [basicAnim setAdditive:property == AnimatedPropertyWebkitTransform];
    [basicAnim setFillMode:@"extended"];
#if HAVE_MODERN_QUARTZCORE
    if (NSString* valueFunctionName = getValueFunctionNameForTransformOperation(operationType))
        [basicAnim setValueFunction:[CAValueFunction functionWithName:valueFunctionName]];
#else
    UNUSED_PARAM(operationType);
#endif
    
    // Set the delegate (and property value).
    int prop = isTransition ? property : AnimatedPropertyInvalid;
    [basicAnim setValue:[NSNumber numberWithInt:prop] forKey:WebAnimationCSSPropertyKey];
    [basicAnim setDelegate:m_animationDelegate.get()];

    NSTimeInterval bt = beginTime ? [layer convertTime:currentTimeToMediaTime(beginTime) fromLayer:nil] : 0;
    [basicAnim setBeginTime:bt];
    
    if (fromVal)
        [basicAnim setFromValue:reinterpret_cast<id>(fromVal)];
    if (toVal)
        [basicAnim setToValue:reinterpret_cast<id>(toVal)];

    const TimingFunction* tf = 0;
    if (transition->isTimingFunctionSet())
        tf = &transition->timingFunction();

    CAMediaTimingFunction* timingFunction = getCAMediaTimingFunction(tf ? *tf : TimingFunction());
    [basicAnim setTimingFunction:timingFunction];

    // Send over the animation.
    [layer removeAnimationForKey:animName];
    [layer addAnimation:basicAnim forKey:animName];
    
    END_BLOCK_OBJC_EXCEPTIONS
}

void GraphicsLayerCA::setKeyframeAnimation(AnimatedPropertyID property, TransformOperation::OperationType operationType, short index, void* keys, void* values, void* timingFunctions, 
                                    bool isTransition, const Animation* anim, double beginTime)
{
    PlatformLayer* layer = animatedLayer(property);

    // Add an entry for this animation (which may change beginTime).
    addAnimationEntry(property, index, isTransition, anim);

    String keyPath = propertyIdToString(property);
    String animName = keyPath + "_" + String::number(index);

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    CAKeyframeAnimation* keyframeAnim = [CAKeyframeAnimation animationWithKeyPath:keyPath];

    double duration = anim->duration();
    if (duration <= 0)
        duration = cAnimationAlmostZeroDuration;

    float repeatCount = anim->iterationCount();
    if (repeatCount == Animation::IterationCountInfinite)
        repeatCount = FLT_MAX;
    else if (anim->direction() == Animation::AnimationDirectionAlternate)
        repeatCount /= 2;

    [keyframeAnim setDuration:duration];
    [keyframeAnim setRepeatCount:repeatCount];
    [keyframeAnim setAutoreverses:anim->direction()];
    [keyframeAnim setRemovedOnCompletion:NO];

    // The first animation is non-additive, all the rest are additive.
    // Note that currently transform is the only property which has animations
    // with an index > 0.
    [keyframeAnim setAdditive:(property == AnimatedPropertyWebkitTransform) ? YES : NO];
    [keyframeAnim setFillMode:@"extended"];
#if HAVE_MODERN_QUARTZCORE
    if (NSString* valueFunctionName = getValueFunctionNameForTransformOperation(operationType))
        [keyframeAnim setValueFunction:[CAValueFunction functionWithName:valueFunctionName]];
#else
    UNUSED_PARAM(operationType);
#endif

    [keyframeAnim setKeyTimes:reinterpret_cast<id>(keys)];
    [keyframeAnim setValues:reinterpret_cast<id>(values)];
    
    // Set the delegate (and property value).
    int prop = isTransition ? property : AnimatedPropertyInvalid;
    [keyframeAnim setValue:[NSNumber numberWithInt: prop] forKey:WebAnimationCSSPropertyKey];
    [keyframeAnim setDelegate:m_animationDelegate.get()];

    NSTimeInterval bt = beginTime ? [layer convertTime:currentTimeToMediaTime(beginTime) fromLayer:nil] : 0;
    [keyframeAnim setBeginTime:bt];
    
    // Set the timing functions, if any.
    if (timingFunctions != nil)
        [keyframeAnim setTimingFunctions:(id)timingFunctions];
        
    // Send over the animation.
    [layer removeAnimationForKey:animName];
    [layer addAnimation:keyframeAnim forKey:animName];
        
    END_BLOCK_OBJC_EXCEPTIONS
}

void GraphicsLayerCA::suspendAnimations()
{
    double t = currentTimeToMediaTime(currentTime());
    [primaryLayer() setSpeed:0];
    [primaryLayer() setTimeOffset:t];
}

void GraphicsLayerCA::resumeAnimations()
{
    [primaryLayer() setSpeed:1];
    [primaryLayer() setTimeOffset:0];
}

void GraphicsLayerCA::removeAnimation(int index, bool reset)
{
    ASSERT(index >= 0);

    AnimatedPropertyID property = m_animations[index].property();
    
    // Set the value of the property and remove the animation.
    String keyPath = propertyIdToString(property);
    String animName = keyPath + "_" + String::number(m_animations[index].index());
    CALayer* layer = animatedLayer(property);

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    // If we are not resetting, it means we are pausing. So we need to get the current presentation
    // value into the property before we remove the animation.
    if (!reset) {
        // Put the current value into the property.
        CALayer* presLayer = [layer presentationLayer];
        if (presLayer)
            [layer setValue:[presLayer valueForKeyPath:keyPath] forKeyPath:keyPath];

        // Make sure the saved values accurately reflect the value in the layer.
        id val = [layer valueForKeyPath:keyPath];
        switch (property) {
            case AnimatedPropertyWebkitTransform:
                // FIXME: needs comment explaining why the m_transform is not obtained from the layer
                break;
            case AnimatedPropertyBackgroundColor:
                m_backgroundColor = Color(reinterpret_cast<CGColorRef>(val));
                break;
            case AnimatedPropertyOpacity:
                m_opacity = [val floatValue];
                break;
            case AnimatedPropertyInvalid:
                ASSERT_NOT_REACHED();
                break;
        }
    }
    
    // If we have reached the end of an animation, we don't want to actually remove the 
    // animation from the CALayer. At some point we will be setting the property to its
    // unanimated value and at that point we will remove the animation. That will avoid
    // any flashing between the time the animation is removed and the property is set.
    if (!reset || m_animations[index].isTransition())
        [layer removeAnimationForKey:animName];

    END_BLOCK_OBJC_EXCEPTIONS

    // Remove the animation entry.
    m_animations.remove(index);
}

PlatformLayer* GraphicsLayerCA::hostLayerForSublayers() const
{
    return m_transformLayer ? m_transformLayer.get() : m_layer.get();
}

PlatformLayer* GraphicsLayerCA::layerForSuperlayer() const
{
    if (m_transformLayer)
        return m_transformLayer.get();

    return m_layer.get();
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

void GraphicsLayerCA::setZPosition(float position)
{
    GraphicsLayer::setZPosition(position);
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [primaryLayer() setZPosition:position];
    END_BLOCK_OBJC_EXCEPTIONS
}
#endif

bool GraphicsLayerCA::requiresTiledLayer(const FloatSize& size) const
{
    if (!m_drawsContent)
        return false;

    // FIXME: catch zero-size height or width here (or earlier)?
    return size.width() > cMaxPixelDimension || size.height() > cMaxPixelDimension;
}

void GraphicsLayerCA::swapFromOrToTiledLayer(bool userTiledLayer)
{
    if (userTiledLayer == m_usingTiledLayer)
        return;

    CGSize tileSize = CGSizeMake(cTiledLayerTileSize, cTiledLayerTileSize);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    
    RetainPtr<CALayer> oldLayer = m_layer.get();
    
    Class layerClass = userTiledLayer ? [WebTiledLayer self] : [WebLayer self];
    m_layer.adoptNS([[layerClass alloc] init]);

    if (userTiledLayer) {
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
    
    [m_layer.get() setBounds:[oldLayer.get() bounds]];
    [m_layer.get() setPosition:[oldLayer.get() position]];
    [m_layer.get() setAnchorPoint:[oldLayer.get() anchorPoint]];
    [m_layer.get() setOpaque:[oldLayer.get() isOpaque]];
    [m_layer.get() setOpacity:[oldLayer.get() opacity]];
    [m_layer.get() setTransform:[oldLayer.get() transform]];
    [m_layer.get() setSublayerTransform:[oldLayer.get() sublayerTransform]];
    [m_layer.get() setDoubleSided:[oldLayer.get() isDoubleSided]];
#ifndef NDEBUG
    [m_layer.get() setZPosition:[oldLayer.get() zPosition]];
#endif
    updateContentsTransform();
    
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
    
    END_BLOCK_OBJC_EXCEPTIONS
    
    m_usingTiledLayer = userTiledLayer;
    
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

void GraphicsLayerCA::setContentsLayer(WebLayer* contentsLayer)
{
    if (contentsLayer == m_contentsLayer)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    if (m_contentsLayer) {
        [m_contentsLayer.get() removeFromSuperlayer];
        m_contentsLayer = 0;
    }
    
    if (contentsLayer) {
        // Turn off implicit animations on the inner layer.
        [contentsLayer setStyle:[NSDictionary dictionaryWithObject:nullActionsDictionary() forKey:@"actions"]];
        m_contentsLayer.adoptNS([contentsLayer retain]);

        if (defaultContentsOrientation() == CompositingCoordinatesBottomUp) {
            CATransform3D flipper = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, -1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f};
            [m_contentsLayer.get() setTransform:flipper];
            [m_contentsLayer.get() setAnchorPoint:CGPointMake(0.0f, 1.0f)];
        } else
            [m_contentsLayer.get() setAnchorPoint:CGPointZero];

        // Insert the content layer first. Video elements require this, because they have
        // shadow content that must display in front of the video.
        [m_layer.get() insertSublayer:m_contentsLayer.get() atIndex:0];

        updateContentsRect();

#ifndef NDEBUG
        if (showDebugBorders()) {
            setLayerBorderColor(m_contentsLayer.get(), Color(0, 0, 128, 180));
            [m_contentsLayer.get() setBorderWidth:1.0f];
        }
#endif
    }
#ifndef NDEBUG
    updateDebugIndicators();
#endif

    END_BLOCK_OBJC_EXCEPTIONS
}

} // namespace WebCore


#endif // USE(ACCELERATED_COMPOSITING)
