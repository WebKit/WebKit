/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
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

#import "config.h"
#import "PlatformCALayerCocoa.h"

#import "AnimationUtilities.h"
#import "GraphicsContext.h"
#import "GraphicsLayerCA.h"
#import "IOSurface.h"
#import "LengthFunctions.h"
#import "PlatformCAAnimationCocoa.h"
#import "PlatformCAFilters.h"
#import "ScrollbarThemeMac.h"
#import "TileController.h"
#import "TiledBacking.h"
#import "WebActionDisablingCALayerDelegate.h"
#import "WebCoreCALayerExtras.h"
#import "WebGLLayer.h"
#import "WebVideoContainerLayer.h"
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/SoftLinking.h>
#import "WebLayer.h"
#import "WebSystemBackdropLayer.h"
#import "WebTiledBackingLayer.h"
#import <AVFoundation/AVPlayer.h>
#import <AVFoundation/AVPlayerLayer.h>
#import <QuartzCore/QuartzCore.h>
#import <objc/runtime.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/VectorCocoa.h>

#if ENABLE(WEBGPU)
#import "WebGPULayer.h"
#endif

#if PLATFORM(IOS_FAMILY)
#import "FontAntialiasingStateSaver.h"
#import "WAKWindow.h"
#import "WKGraphics.h"
#import "WebCoreThread.h"
#else
#import "ThemeMac.h"
#endif

#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebCore {

using LayerToPlatformCALayerMap = HashMap<void*, PlatformCALayer*>;

static LayerToPlatformCALayerMap& layerToPlatformLayerMap()
{
    static NeverDestroyed<LayerToPlatformCALayerMap> layerMap;
    return layerMap;
}


RefPtr<PlatformCALayer> PlatformCALayer::platformCALayerForLayer(void* platformLayer)
{
    if (!platformLayer)
        return nullptr;

    static Lock lock;
    LockHolder lockHolder(lock);
    return layerToPlatformLayerMap().get(platformLayer);
}

Ref<PlatformCALayerCocoa> PlatformCALayerCocoa::create(LayerType layerType, PlatformCALayerClient* owner)
{
    return adoptRef(*new PlatformCALayerCocoa(layerType, owner));
}

Ref<PlatformCALayerCocoa> PlatformCALayerCocoa::create(void* platformLayer, PlatformCALayerClient* owner)
{
    return adoptRef(*new PlatformCALayerCocoa((__bridge CALayer *)platformLayer, owner));
}

static MonotonicTime mediaTimeToCurrentTime(CFTimeInterval t)
{
    return MonotonicTime::now() + Seconds(t - CACurrentMediaTime());
}

} // namespace WebCore

// Delegate for animationDidStart callback
@interface WebAnimationDelegate : NSObject {
    WebCore::PlatformCALayer* m_owner;
}

- (void)animationDidStart:(CAAnimation *)anim;
- (void)setOwner:(WebCore::PlatformCALayer*)owner;

@end

@implementation WebAnimationDelegate

- (void)animationDidStart:(CAAnimation *)animation
{
    using namespace WebCore;
#if PLATFORM(IOS_FAMILY)
    WebThreadLock();
#endif
    if (!m_owner)
        return;

    MonotonicTime startTime;
    if (hasExplicitBeginTime(animation)) {
        // We don't know what time CA used to commit the animation, so just use the current time
        // (even though this will be slightly off).
        startTime = mediaTimeToCurrentTime(CACurrentMediaTime());
    } else
        startTime = mediaTimeToCurrentTime([animation beginTime]);

    CALayer *layer = m_owner->platformLayer();

    String animationKey;
    for (NSString *key in [layer animationKeys]) {
        if ([layer animationForKey:key] == animation) {
            animationKey = key;
            break;
        }
    }

    if (!animationKey.isEmpty())
        m_owner->animationStarted(animationKey, startTime);
}

- (void)animationDidStop:(CAAnimation *)animation finished:(BOOL)finished
{
    using namespace WebCore;
#if PLATFORM(IOS_FAMILY)
    WebThreadLock();
#endif
    UNUSED_PARAM(finished);

    if (!m_owner)
        return;
    
    CALayer *layer = m_owner->platformLayer();

    String animationKey;
    for (NSString *key in [layer animationKeys]) {
        if ([layer animationForKey:key] == animation) {
            animationKey = key;
            break;
        }
    }

    if (!animationKey.isEmpty())
        m_owner->animationEnded(animationKey);
}

- (void)setOwner:(WebCore::PlatformCALayer*)owner
{
    m_owner = owner;
}

@end

namespace WebCore {

void PlatformCALayerCocoa::setOwner(PlatformCALayerClient* owner)
{
    PlatformCALayer::setOwner(owner);
    
    // Change the delegate's owner if needed
    if (m_delegate)
        [static_cast<WebAnimationDelegate*>(m_delegate.get()) setOwner:this];        
}

static NSString *toCAFilterType(PlatformCALayer::FilterType type)
{
    switch (type) {
    case PlatformCALayer::Linear: return kCAFilterLinear;
    case PlatformCALayer::Nearest: return kCAFilterNearest;
    case PlatformCALayer::Trilinear: return kCAFilterTrilinear;
    default: return 0;
    }
}

PlatformCALayer::LayerType PlatformCALayerCocoa::layerTypeForPlatformLayer(PlatformLayer* layer)
{
    if (PAL::isAVFoundationFrameworkAvailable() && [layer isKindOfClass:PAL::getAVPlayerLayerClass()])
        return LayerTypeAVPlayerLayer;

    if ([layer isKindOfClass:WebVideoContainerLayer.class]
        && [(WebVideoContainerLayer*)layer playerLayer])
        return LayerTypeAVPlayerLayer;

    if ([layer isKindOfClass:[WebGLLayer class]])
        return LayerTypeContentsProvidedLayer;

#if ENABLE(WEBGPU)
    if ([layer isKindOfClass:[WebGPULayer class]])
        return LayerTypeContentsProvidedLayer;
#endif

    return LayerTypeCustom;
}

PlatformCALayerCocoa::PlatformCALayerCocoa(LayerType layerType, PlatformCALayerClient* owner)
    : PlatformCALayer(layerType, owner)
{
    Class layerClass = Nil;
    switch (layerType) {
    case LayerTypeLayer:
    case LayerTypeRootLayer:
        layerClass = [CALayer class];
        break;
    case LayerTypeScrollContainerLayer:
        // Scroll container layers only have special behavior with PlatformCALayerRemote.
        // fallthrough
    case LayerTypeEditableImageLayer:
    case LayerTypeWebLayer:
        layerClass = [WebLayer class];
        break;
    case LayerTypeSimpleLayer:
    case LayerTypeTiledBackingTileLayer:
        layerClass = [WebSimpleLayer class];
        break;
    case LayerTypeTransformLayer:
        layerClass = [CATransformLayer class];
        break;
#if ENABLE(FILTERS_LEVEL_2)
    case LayerTypeBackdropLayer:
        layerClass = [CABackdropLayer class];
        break;
    case LayerTypeLightSystemBackdropLayer:
        layerClass = [WebLightSystemBackdropLayer class];
        break;
    case LayerTypeDarkSystemBackdropLayer:
        layerClass = [WebDarkSystemBackdropLayer class];
        break;
#else
    case LayerTypeBackdropLayer:
    case LayerTypeLightSystemBackdropLayer:
    case LayerTypeDarkSystemBackdropLayer:
        ASSERT_NOT_REACHED();
        layerClass = [CALayer class];
        break;
#endif
    case LayerTypeTiledBackingLayer:
    case LayerTypePageTiledBackingLayer:
        layerClass = [WebTiledBackingLayer class];
        break;
    case LayerTypeAVPlayerLayer:
        if (PAL::isAVFoundationFrameworkAvailable())
            layerClass = PAL::getAVPlayerLayerClass();
        break;
    case LayerTypeContentsProvidedLayer:
        // We don't create PlatformCALayerCocoas wrapped around WebGLLayers or WebGPULayers.
        ASSERT_NOT_REACHED();
        break;
    case LayerTypeShapeLayer:
        layerClass = [CAShapeLayer class];
        // fillColor defaults to opaque black.
        break;
    case LayerTypeCustom:
        break;
    }

    if (layerClass)
        m_layer = adoptNS([(CALayer *)[layerClass alloc] init]);

#if ENABLE(FILTERS_LEVEL_2) && PLATFORM(MAC)
    if (layerType == LayerTypeBackdropLayer)
        [(CABackdropLayer*)m_layer.get() setWindowServerAware:NO];
#endif

    commonInit();
}

PlatformCALayerCocoa::PlatformCALayerCocoa(PlatformLayer* layer, PlatformCALayerClient* owner)
    : PlatformCALayer(layerTypeForPlatformLayer(layer), owner)
{
    m_layer = layer;
    commonInit();
}

void PlatformCALayerCocoa::commonInit()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    layerToPlatformLayerMap().add(m_layer.get(), this);
    
    // Clear all the implicit animations on the CALayer
    if (m_layerType == LayerTypeAVPlayerLayer || m_layerType == LayerTypeContentsProvidedLayer || m_layerType == LayerTypeScrollContainerLayer || m_layerType == LayerTypeCustom)
        [m_layer web_disableAllActions];
    else
        [m_layer setDelegate:[WebActionDisablingCALayerDelegate shared]];

    // So that the scrolling thread's performance logging code can find all the tiles, mark this as being a tile.
    if (m_layerType == LayerTypeTiledBackingTileLayer)
        [m_layer setValue:@YES forKey:@"isTile"];

    if (usesTiledBackingLayer()) {
        WebTiledBackingLayer* tiledBackingLayer = static_cast<WebTiledBackingLayer*>(m_layer.get());
        TileController* tileController = [tiledBackingLayer createTileController:this];

        m_customSublayers = makeUnique<PlatformCALayerList>(tileController->containerLayers());
    }

    END_BLOCK_OBJC_EXCEPTIONS
}

Ref<PlatformCALayer> PlatformCALayerCocoa::clone(PlatformCALayerClient* owner) const
{
    LayerType type;
    switch (layerType()) {
    case LayerTypeTransformLayer:
        type = LayerTypeTransformLayer;
        break;
    case LayerTypeAVPlayerLayer:
        type = LayerTypeAVPlayerLayer;
        break;
    case LayerTypeShapeLayer:
        type = LayerTypeShapeLayer;
        break;
    case LayerTypeBackdropLayer:
        type = LayerTypeBackdropLayer;
        break;
    case LayerTypeLayer:
    default:
        type = LayerTypeLayer;
        break;
    };
    auto newLayer = PlatformCALayerCocoa::create(type, owner);
    
    newLayer->setPosition(position());
    newLayer->setBounds(bounds());
    newLayer->setAnchorPoint(anchorPoint());
    newLayer->setTransform(transform());
    newLayer->setSublayerTransform(sublayerTransform());
    newLayer->setContents(contents());
    newLayer->setMasksToBounds(masksToBounds());
    newLayer->setDoubleSided(isDoubleSided());
    newLayer->setOpaque(isOpaque());
    newLayer->setBackgroundColor(backgroundColor());
    newLayer->setContentsScale(contentsScale());
    newLayer->setCornerRadius(cornerRadius());
    newLayer->copyFiltersFrom(*this);
    newLayer->updateCustomAppearance(customAppearance());

    if (type == LayerTypeAVPlayerLayer) {
        ASSERT(PAL::isAVFoundationFrameworkAvailable() && [newLayer->platformLayer() isKindOfClass:PAL::getAVPlayerLayerClass()]);

        AVPlayerLayer *destinationPlayerLayer = newLayer->avPlayerLayer();
        AVPlayerLayer *sourcePlayerLayer = avPlayerLayer();
        ASSERT(sourcePlayerLayer);

        dispatch_async(dispatch_get_main_queue(), ^{
            [destinationPlayerLayer setPlayer:[sourcePlayerLayer player]];
        });
    }
    
    if (type == LayerTypeShapeLayer)
        newLayer->setShapeRoundedRect(shapeRoundedRect());

    return newLayer;
}

PlatformCALayerCocoa::~PlatformCALayerCocoa()
{
    layerToPlatformLayerMap().remove(m_layer.get());
    
    // Remove the owner pointer from the delegate in case there is a pending animationStarted event.
    [static_cast<WebAnimationDelegate*>(m_delegate.get()) setOwner:nil];

    if (usesTiledBackingLayer())
        [static_cast<WebTiledBackingLayer *>(m_layer.get()) invalidate];
}

void PlatformCALayerCocoa::animationStarted(const String& animationKey, MonotonicTime beginTime)
{
    if (m_owner)
        m_owner->platformCALayerAnimationStarted(animationKey, beginTime);
}

void PlatformCALayerCocoa::animationEnded(const String& animationKey)
{
    if (m_owner)
        m_owner->platformCALayerAnimationEnded(animationKey);
}

void PlatformCALayerCocoa::setNeedsDisplay()
{
    if (!m_backingStoreAttached)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setNeedsDisplay];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerCocoa::setNeedsDisplayInRect(const FloatRect& dirtyRect)
{
    if (!m_backingStoreAttached)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setNeedsDisplayInRect:dirtyRect];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerCocoa::copyContentsFromLayer(PlatformCALayer* layer)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    CALayer* caLayer = layer->m_layer.get();
    if ([m_layer contents] != [caLayer contents])
        [m_layer setContents:[caLayer contents]];
    else
        [m_layer reloadValueForKeyPath:@"contents"];
    END_BLOCK_OBJC_EXCEPTIONS
}

PlatformCALayer* PlatformCALayerCocoa::superlayer() const
{
    return platformCALayerForLayer((__bridge void*)[m_layer superlayer]).get();
}

void PlatformCALayerCocoa::removeFromSuperlayer()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer removeFromSuperlayer];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerCocoa::setSublayers(const PlatformCALayerList& list)
{
    // Short circuiting here avoids the allocation of the array below.
    if (!list.size()) {
        removeAllSublayers();
        return;
    }

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setSublayers:createNSArray(list, [] (auto& layer) {
        return layer->m_layer;
    }).get()];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerCocoa::removeAllSublayers()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setSublayers:nil];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerCocoa::appendSublayer(PlatformCALayer& layer)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    ASSERT(m_layer != layer.m_layer);
    [m_layer addSublayer:layer.m_layer.get()];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerCocoa::insertSublayer(PlatformCALayer& layer, size_t index)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    ASSERT(m_layer != layer.m_layer);
    [m_layer insertSublayer:layer.m_layer.get() atIndex:index];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerCocoa::replaceSublayer(PlatformCALayer& reference, PlatformCALayer& layer)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    ASSERT(m_layer != layer.m_layer);
    [m_layer replaceSublayer:reference.m_layer.get() with:layer.m_layer.get()];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerCocoa::adoptSublayers(PlatformCALayer& source)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setSublayers:[source.m_layer.get() sublayers]];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerCocoa::addAnimationForKey(const String& key, PlatformCAAnimation& animation)
{
    // Add the delegate
    if (!m_delegate) {
        WebAnimationDelegate* webAnimationDelegate = [[WebAnimationDelegate alloc] init];
        m_delegate = adoptNS(webAnimationDelegate);
        [webAnimationDelegate setOwner:this];
    }
    
    CAPropertyAnimation* propertyAnimation = static_cast<CAPropertyAnimation*>(downcast<PlatformCAAnimationCocoa>(animation).platformAnimation());
    if (![propertyAnimation delegate])
        [propertyAnimation setDelegate:static_cast<id>(m_delegate.get())];

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer addAnimation:propertyAnimation forKey:key];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerCocoa::removeAnimationForKey(const String& key)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer removeAnimationForKey:key];
    END_BLOCK_OBJC_EXCEPTIONS
}

RefPtr<PlatformCAAnimation> PlatformCALayerCocoa::animationForKey(const String& key)
{
    CAPropertyAnimation* propertyAnimation = static_cast<CAPropertyAnimation*>([m_layer animationForKey:key]);
    if (!propertyAnimation)
        return nullptr;
    return PlatformCAAnimationCocoa::create(propertyAnimation);
}

void PlatformCALayerCocoa::setMask(PlatformCALayer* layer)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setMask:layer ? layer->platformLayer() : nil];
    END_BLOCK_OBJC_EXCEPTIONS
}

bool PlatformCALayerCocoa::isOpaque() const
{
    return [m_layer isOpaque];
}

void PlatformCALayerCocoa::setOpaque(bool value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setOpaque:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

FloatRect PlatformCALayerCocoa::bounds() const
{
    return [m_layer bounds];
}

void PlatformCALayerCocoa::setBounds(const FloatRect& value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setBounds:value];
    
    if (requiresCustomAppearanceUpdateOnBoundsChange())
        updateCustomAppearance(m_customAppearance);

    END_BLOCK_OBJC_EXCEPTIONS
}

FloatPoint3D PlatformCALayerCocoa::position() const
{
    CGPoint point = [m_layer position];
    return FloatPoint3D(point.x, point.y, [m_layer zPosition]);
}

void PlatformCALayerCocoa::setPosition(const FloatPoint3D& value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setPosition:CGPointMake(value.x(), value.y())];
    [m_layer setZPosition:value.z()];
    END_BLOCK_OBJC_EXCEPTIONS
}

FloatPoint3D PlatformCALayerCocoa::anchorPoint() const
{
    CGPoint point = [m_layer anchorPoint];
    float z = 0;
    z = [m_layer anchorPointZ];
    return FloatPoint3D(point.x, point.y, z);
}

void PlatformCALayerCocoa::setAnchorPoint(const FloatPoint3D& value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setAnchorPoint:CGPointMake(value.x(), value.y())];
    [m_layer setAnchorPointZ:value.z()];
    END_BLOCK_OBJC_EXCEPTIONS
}

TransformationMatrix PlatformCALayerCocoa::transform() const
{
    return [m_layer transform];
}

void PlatformCALayerCocoa::setTransform(const TransformationMatrix& value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setTransform:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

TransformationMatrix PlatformCALayerCocoa::sublayerTransform() const
{
    return [m_layer sublayerTransform];
}

void PlatformCALayerCocoa::setSublayerTransform(const TransformationMatrix& value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setSublayerTransform:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

bool PlatformCALayerCocoa::isHidden() const
{
    return [m_layer isHidden];
}

void PlatformCALayerCocoa::setHidden(bool value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setHidden:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

bool PlatformCALayerCocoa::contentsHidden() const
{
    return false;
}

void PlatformCALayerCocoa::setContentsHidden(bool)
{
}

bool PlatformCALayerCocoa::userInteractionEnabled() const
{
    return true;
}

void PlatformCALayerCocoa::setUserInteractionEnabled(bool)
{
}

void PlatformCALayerCocoa::setBackingStoreAttached(bool attached)
{
    if (attached == m_backingStoreAttached)
        return;

    m_backingStoreAttached = attached;

    if (attached)
        setNeedsDisplay();
    else
        setContents(nullptr);
}

bool PlatformCALayerCocoa::backingStoreAttached() const
{
    return m_backingStoreAttached;
}

bool PlatformCALayerCocoa::geometryFlipped() const
{
    return [m_layer isGeometryFlipped];
}

void PlatformCALayerCocoa::setGeometryFlipped(bool value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setGeometryFlipped:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

bool PlatformCALayerCocoa::isDoubleSided() const
{
    return [m_layer isDoubleSided];
}

void PlatformCALayerCocoa::setDoubleSided(bool value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setDoubleSided:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

bool PlatformCALayerCocoa::masksToBounds() const
{
    return [m_layer masksToBounds];
}

void PlatformCALayerCocoa::setMasksToBounds(bool value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setMasksToBounds:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

bool PlatformCALayerCocoa::acceleratesDrawing() const
{
    return [m_layer drawsAsynchronously];
}

void PlatformCALayerCocoa::setAcceleratesDrawing(bool acceleratesDrawing)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setDrawsAsynchronously:acceleratesDrawing];
    END_BLOCK_OBJC_EXCEPTIONS
}

bool PlatformCALayerCocoa::wantsDeepColorBackingStore() const
{
    return m_wantsDeepColorBackingStore;
}

void PlatformCALayerCocoa::setWantsDeepColorBackingStore(bool wantsDeepColorBackingStore)
{
    if (wantsDeepColorBackingStore == m_wantsDeepColorBackingStore)
        return;

    m_wantsDeepColorBackingStore = wantsDeepColorBackingStore;

    if (usesTiledBackingLayer()) {
        [static_cast<WebTiledBackingLayer *>(m_layer.get()) setWantsDeepColorBackingStore:m_wantsDeepColorBackingStore];
        return;
    }

    updateContentsFormat();
}

bool PlatformCALayerCocoa::supportsSubpixelAntialiasedText() const
{
    return m_supportsSubpixelAntialiasedText;
}

void PlatformCALayerCocoa::setSupportsSubpixelAntialiasedText(bool supportsSubpixelAntialiasedText)
{
    if (supportsSubpixelAntialiasedText == m_supportsSubpixelAntialiasedText)
        return;
    
    m_supportsSubpixelAntialiasedText = supportsSubpixelAntialiasedText;

    if (usesTiledBackingLayer()) {
        [static_cast<WebTiledBackingLayer *>(m_layer.get()) setSupportsSubpixelAntialiasedText:m_supportsSubpixelAntialiasedText];
        return;
    }

    updateContentsFormat();
}

bool PlatformCALayerCocoa::hasContents() const
{
    return [m_layer contents];
}

CFTypeRef PlatformCALayerCocoa::contents() const
{
    return (__bridge CFTypeRef)[m_layer contents];
}

void PlatformCALayerCocoa::setContents(CFTypeRef value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setContents:(__bridge id)value];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerCocoa::setContentsRect(const FloatRect& value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setContentsRect:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerCocoa::setMinificationFilter(FilterType value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setMinificationFilter:toCAFilterType(value)];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerCocoa::setMagnificationFilter(FilterType value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setMagnificationFilter:toCAFilterType(value)];
    END_BLOCK_OBJC_EXCEPTIONS
}

Color PlatformCALayerCocoa::backgroundColor() const
{
    return [m_layer backgroundColor];
}

void PlatformCALayerCocoa::setBackgroundColor(const Color& value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setBackgroundColor:cachedCGColor(value)];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerCocoa::setBorderWidth(float value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setBorderWidth:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerCocoa::setBorderColor(const Color& value)
{
    if (value.isValid()) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        [m_layer setBorderColor:cachedCGColor(value)];
        END_BLOCK_OBJC_EXCEPTIONS
    } else {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        [m_layer setBorderColor:nil];
        END_BLOCK_OBJC_EXCEPTIONS
    }
}

float PlatformCALayerCocoa::opacity() const
{
    return [m_layer opacity];
}

void PlatformCALayerCocoa::setOpacity(float value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setOpacity:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerCocoa::setFilters(const FilterOperations& filters)
{
    PlatformCAFilters::setFiltersOnLayer(platformLayer(), filters);
}

void PlatformCALayerCocoa::copyFiltersFrom(const PlatformCALayer& sourceLayer)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setFilters:[sourceLayer.platformLayer() filters]];
    END_BLOCK_OBJC_EXCEPTIONS
}

bool PlatformCALayerCocoa::filtersCanBeComposited(const FilterOperations& filters)
{
    // Return false if there are no filters to avoid needless work
    if (!filters.size())
        return false;
    
    for (unsigned i = 0; i < filters.size(); ++i) {
        const FilterOperation* filterOperation = filters.at(i);
        switch (filterOperation->type()) {
        case FilterOperation::REFERENCE:
            return false;
        case FilterOperation::DROP_SHADOW:
            // FIXME: For now we can only handle drop-shadow is if it's last in the list
            if (i < (filters.size() - 1))
                return false;
            break;
        default:
            break;
        }
    }

    return true;
}

#if ENABLE(CSS_COMPOSITING)
void PlatformCALayerCocoa::setBlendMode(BlendMode blendMode)
{
    PlatformCAFilters::setBlendingFiltersOnLayer(platformLayer(), blendMode);
}
#endif

void PlatformCALayerCocoa::setName(const String& value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setName:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerCocoa::setSpeed(float value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setSpeed:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerCocoa::setTimeOffset(CFTimeInterval value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setTimeOffset:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

float PlatformCALayerCocoa::contentsScale() const
{
    return [m_layer contentsScale];
}

void PlatformCALayerCocoa::setContentsScale(float value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setContentsScale:value];
#if PLATFORM(IOS_FAMILY)
    [m_layer setRasterizationScale:value];
#endif
    END_BLOCK_OBJC_EXCEPTIONS
}

float PlatformCALayerCocoa::cornerRadius() const
{
    return [m_layer cornerRadius];
}

void PlatformCALayerCocoa::setCornerRadius(float value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setCornerRadius:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerCocoa::setEdgeAntialiasingMask(unsigned mask)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer setEdgeAntialiasingMask:mask];
    END_BLOCK_OBJC_EXCEPTIONS
}

FloatRoundedRect PlatformCALayerCocoa::shapeRoundedRect() const
{
    ASSERT(m_layerType == LayerTypeShapeLayer);
    if (m_shapeRoundedRect)
        return *m_shapeRoundedRect;

    return FloatRoundedRect();
}

void PlatformCALayerCocoa::setShapeRoundedRect(const FloatRoundedRect& roundedRect)
{
    ASSERT(m_layerType == LayerTypeShapeLayer);
    m_shapeRoundedRect = makeUnique<FloatRoundedRect>(roundedRect);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    Path shapePath;
    shapePath.addRoundedRect(roundedRect);
    [(CAShapeLayer *)m_layer setPath:shapePath.platformPath()];
    END_BLOCK_OBJC_EXCEPTIONS
}

WindRule PlatformCALayerCocoa::shapeWindRule() const
{
    ASSERT(m_layerType == LayerTypeShapeLayer);

    NSString *fillRule = [(CAShapeLayer *)m_layer fillRule];
    if ([fillRule isEqualToString:@"even-odd"])
        return WindRule::EvenOdd;

    return WindRule::NonZero;
}

void PlatformCALayerCocoa::setShapeWindRule(WindRule windRule)
{
    ASSERT(m_layerType == LayerTypeShapeLayer);

    switch (windRule) {
    case WindRule::NonZero:
        [(CAShapeLayer *)m_layer setFillRule:@"non-zero"];
        break;
    case WindRule::EvenOdd:
        [(CAShapeLayer *)m_layer setFillRule:@"even-odd"];
        break;
    }
}

Path PlatformCALayerCocoa::shapePath() const
{
    ASSERT(m_layerType == LayerTypeShapeLayer);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    return { adoptCF(CGPathCreateMutableCopy([(CAShapeLayer *)m_layer path])) };
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerCocoa::setShapePath(const Path& path)
{
    ASSERT(m_layerType == LayerTypeShapeLayer);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [(CAShapeLayer *)m_layer setPath:path.platformPath()];
    END_BLOCK_OBJC_EXCEPTIONS
}

bool PlatformCALayerCocoa::requiresCustomAppearanceUpdateOnBoundsChange() const
{
    return m_customAppearance == GraphicsLayer::CustomAppearance::ScrollingShadow;
}

void PlatformCALayerCocoa::updateCustomAppearance(GraphicsLayer::CustomAppearance appearance)
{
    if (m_customAppearance == appearance)
        return;

    m_customAppearance = appearance;

#if ENABLE(RUBBER_BANDING)
    switch (appearance) {
    case GraphicsLayer::CustomAppearance::None:
    case GraphicsLayer::CustomAppearance::LightBackdrop:
    case GraphicsLayer::CustomAppearance::DarkBackdrop:
        ScrollbarThemeMac::removeOverhangAreaBackground(platformLayer());
        ScrollbarThemeMac::removeOverhangAreaShadow(platformLayer());
        break;
    case GraphicsLayer::CustomAppearance::ScrollingOverhang:
        ScrollbarThemeMac::setUpOverhangAreaBackground(platformLayer());
        break;
    case GraphicsLayer::CustomAppearance::ScrollingShadow:
        ScrollbarThemeMac::setUpOverhangAreaShadow(platformLayer());
        break;
    }
#endif
}

void PlatformCALayerCocoa::setEventRegion(const EventRegion& eventRegion)
{
    m_eventRegion = eventRegion;
}

GraphicsLayer::EmbeddedViewID PlatformCALayerCocoa::embeddedViewID() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

static NSString *layerContentsFormat(bool acceleratesDrawing, bool wantsDeepColor, bool supportsSubpixelAntialiasedFonts)
{
#if HAVE(IOSURFACE_RGB10)
    if (wantsDeepColor)
        return kCAContentsFormatRGBA10XR;
#else
    UNUSED_PARAM(wantsDeepColor);
#endif

#if PLATFORM(MAC)
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (supportsSubpixelAntialiasedFonts && acceleratesDrawing)
        return kCAContentsFormatRGBA8ColorRGBA8LinearGlyphMask;
    ALLOW_DEPRECATED_DECLARATIONS_END
#else
    UNUSED_PARAM(supportsSubpixelAntialiasedFonts);
    UNUSED_PARAM(acceleratesDrawing);
#endif

    return nil;
}

void PlatformCALayerCocoa::updateContentsFormat()
{
    if (m_layerType == LayerTypeWebLayer || m_layerType == LayerTypeTiledBackingTileLayer) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        if (NSString *formatString = layerContentsFormat(acceleratesDrawing(), wantsDeepColorBackingStore(), supportsSubpixelAntialiasedText()))
            [m_layer setContentsFormat:formatString];
        END_BLOCK_OBJC_EXCEPTIONS
    }
}

TiledBacking* PlatformCALayerCocoa::tiledBacking()
{
    if (!usesTiledBackingLayer())
        return nullptr;

    WebTiledBackingLayer *tiledBackingLayer = static_cast<WebTiledBackingLayer *>(m_layer.get());
    return [tiledBackingLayer tiledBacking];
}

#if PLATFORM(IOS_FAMILY)
bool PlatformCALayer::isWebLayer()
{
    BOOL result = NO;
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    result = [m_layer isKindOfClass:[WebLayer class]];
    END_BLOCK_OBJC_EXCEPTIONS
    return result;
}

void PlatformCALayer::setBoundsOnMainThread(CGRect bounds)
{
    CALayer *layer = m_layer.get();
    dispatch_async(dispatch_get_main_queue(), ^{
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        [layer setBounds:bounds];
        END_BLOCK_OBJC_EXCEPTIONS
    });
}

void PlatformCALayer::setPositionOnMainThread(CGPoint position)
{
    CALayer *layer = m_layer.get();
    dispatch_async(dispatch_get_main_queue(), ^{
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        [layer setPosition:position];
        END_BLOCK_OBJC_EXCEPTIONS
    });
}

void PlatformCALayer::setAnchorPointOnMainThread(FloatPoint3D value)
{
    CALayer *layer = m_layer.get();
    dispatch_async(dispatch_get_main_queue(), ^{
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        [layer setAnchorPoint:CGPointMake(value.x(), value.y())];
        [layer setAnchorPointZ:value.z()];
        END_BLOCK_OBJC_EXCEPTIONS
    });
}
#endif // PLATFORM(IOS_FAMILY)

PlatformCALayer::RepaintRectList PlatformCALayer::collectRectsToPaint(GraphicsContext& context, PlatformCALayer* platformCALayer)
{
    __block double totalRectArea = 0;
    __block unsigned rectCount = 0;
    __block RepaintRectList dirtyRects;
    
    platformCALayer->enumerateRectsBeingDrawn(context, ^(FloatRect rect) {
        if (++rectCount > webLayerMaxRectsToPaint)
            return;
        
        totalRectArea += rect.area();
        dirtyRects.append(rect);
    });
    
    FloatRect clipBounds = context.clipBounds();
    double clipArea = clipBounds.width() * clipBounds.height();
    
    if (rectCount >= webLayerMaxRectsToPaint || totalRectArea >= clipArea * webLayerWastedSpaceThreshold) {
        dirtyRects.clear();
        dirtyRects.append(clipBounds);
    }
    
    return dirtyRects;
}

void PlatformCALayer::drawLayerContents(GraphicsContext& graphicsContext, WebCore::PlatformCALayer* platformCALayer, RepaintRectList& dirtyRects, GraphicsLayerPaintBehavior layerPaintBehavior)
{
    WebCore::PlatformCALayerClient* layerContents = platformCALayer->owner();
    if (!layerContents)
        return;

    if (!layerContents->platformCALayerRepaintCount(platformCALayer))
        layerPaintBehavior |= GraphicsLayerPaintFirstTilePaint;

    GraphicsContextStateSaver saver(graphicsContext);

    // We never use CompositingCoordinatesOrientation::BottomUp on Mac.
    ASSERT(layerContents->platformCALayerContentsOrientation() == GraphicsLayer::CompositingCoordinatesOrientation::TopDown);

#if PLATFORM(IOS_FAMILY)
    WTF::Optional<FontAntialiasingStateSaver> fontAntialiasingState;
#endif
    if (graphicsContext.hasPlatformContext()) {
        CGContextRef context = graphicsContext.platformContext();
#if PLATFORM(IOS_FAMILY)
        WKSetCurrentGraphicsContext(context);

        fontAntialiasingState = FontAntialiasingStateSaver { context, !![platformCALayer->platformLayer() isOpaque] };
        fontAntialiasingState->setup([WAKWindow hasLandscapeOrientation]);
#else
        [NSGraphicsContext saveGraphicsState];

        // Set up an NSGraphicsContext for the context, so that parts of AppKit that rely on
        // the current NSGraphicsContext (e.g. NSCell drawing) get the right one.
        NSGraphicsContext* layerContext = [NSGraphicsContext graphicsContextWithCGContext:context flipped:YES];
        [NSGraphicsContext setCurrentContext:layerContext];
#endif
    }
    
    {
        graphicsContext.setIsCALayerContext(true);
        graphicsContext.setIsAcceleratedContext(platformCALayer->acceleratesDrawing());

        if (!layerContents->platformCALayerContentsOpaque() && !platformCALayer->supportsSubpixelAntialiasedText() && FontCascade::isSubpixelAntialiasingAvailable()) {
            // Turn off font smoothing to improve the appearance of text rendered onto a transparent background.
            graphicsContext.setShouldSmoothFonts(false);
        }

#if PLATFORM(MAC)
        // It's important to get the clip from the context, because it may be significantly
        // smaller than the layer bounds (e.g. tiled layers)
        ThemeMac::setFocusRingClipRect(graphicsContext.clipBounds());
#endif

        for (const auto& rect : dirtyRects) {
            GraphicsContextStateSaver stateSaver(graphicsContext);
            graphicsContext.clip(rect);

            layerContents->platformCALayerPaintContents(platformCALayer, graphicsContext, rect, layerPaintBehavior);
        }

#if PLATFORM(IOS_FAMILY)
        if (fontAntialiasingState)
            fontAntialiasingState->restore();
#else
        ThemeMac::setFocusRingClipRect(FloatRect());

        [NSGraphicsContext restoreGraphicsState];
#endif
    }

    saver.restore();

    // Re-fetch the layer owner, since <rdar://problem/9125151> indicates that it might have been destroyed during painting.
    layerContents = platformCALayer->owner();
    ASSERT(layerContents);
    
    // Always update the repaint count so that it's accurate even if the count itself is not shown. This will be useful
    // for the Web Inspector feeding this information through the LayerTreeAgent.
    int repaintCount = layerContents->platformCALayerIncrementRepaintCount(platformCALayer);

    if (!platformCALayer->usesTiledBackingLayer() && layerContents && layerContents->platformCALayerShowRepaintCounter(platformCALayer))
        drawRepaintIndicator(graphicsContext, platformCALayer, repaintCount);
}

CGRect PlatformCALayer::frameForLayer(const PlatformLayer* tileLayer)
{
    return [tileLayer frame];
}

Ref<PlatformCALayer> PlatformCALayerCocoa::createCompatibleLayer(PlatformCALayer::LayerType layerType, PlatformCALayerClient* client) const
{
    return PlatformCALayerCocoa::create(layerType, client);
}

void PlatformCALayerCocoa::enumerateRectsBeingDrawn(GraphicsContext& context, void (^block)(FloatRect))
{
    CGSRegionObj region = (CGSRegionObj)[m_layer regionBeingDrawn];
    if (!region) {
        block(context.clipBounds());
        return;
    }

    CGAffineTransform inverseTransform = CGAffineTransformInvert(context.getCTM());
    CGSRegionEnumeratorObj enumerator = CGSRegionEnumerator(region);
    const CGRect* nextRect;
    while ((nextRect = CGSNextRect(enumerator))) {
        CGRect rectToDraw = CGRectApplyAffineTransform(*nextRect, inverseTransform);
        block(rectToDraw);
    }
    
    CGSReleaseRegionEnumerator(enumerator);
}

unsigned PlatformCALayerCocoa::backingStoreBytesPerPixel() const
{
#if PLATFORM(IOS_FAMILY)
    if (wantsDeepColorBackingStore())
        return isOpaque() ? 4 : 5;
#endif

#if PLATFORM(MAC)
    if (!isOpaque() && supportsSubpixelAntialiasedText())
        return 8;
#endif
    return 4;
}

AVPlayerLayer *PlatformCALayerCocoa::avPlayerLayer() const
{
    if (!PAL::isAVFoundationFrameworkAvailable())
        return nil;

    if (layerType() != LayerTypeAVPlayerLayer)
        return nil;

    if ([platformLayer() isKindOfClass:PAL::getAVPlayerLayerClass()])
        return static_cast<AVPlayerLayer *>(platformLayer());

    if ([platformLayer() isKindOfClass:WebVideoContainerLayer.class])
        return static_cast<WebVideoContainerLayer *>(platformLayer()).playerLayer;

    ASSERT_NOT_REACHED();
    return nil;
}

} // namespace WebCore
