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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#import "PlatformCALayerMac.h"

#import "AnimationUtilities.h"
#import "BlockExceptions.h"
#import "GraphicsContext.h"
#import "GraphicsLayerCA.h"
#import "LengthFunctions.h"
#import "PlatformCAFilters.h"
#import "PlatformCAFiltersMac.h"
#import "ScrollbarThemeMac.h"
#import "SoftLinking.h"
#import "TiledBacking.h"
#import "TileController.h"
#import "WebCoreCALayerExtras.h"
#import "WebLayer.h"
#import "WebTiledBackingLayer.h"
#import <objc/objc-auto.h>
#import <objc/runtime.h>
#import <AVFoundation/AVFoundation.h>
#import <QuartzCore/QuartzCore.h>
#import <wtf/CurrentTime.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS)
#import "WebCoreThread.h"
#import "WebTiledLayer.h"
#import <Foundation/NSGeometry.h>
#import <QuartzCore/CATiledLayerPrivate.h>
#endif

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_CLASS(AVFoundation, AVPlayerLayer)

using namespace WebCore;

PassRefPtr<PlatformCALayer> PlatformCALayerMac::create(LayerType layerType, PlatformCALayerClient* owner)
{
    return adoptRef(new PlatformCALayerMac(layerType, owner));
}

PassRefPtr<PlatformCALayer> PlatformCALayerMac::create(void* platformLayer, PlatformCALayerClient* owner)
{
    return adoptRef(new PlatformCALayerMac(static_cast<PlatformLayer*>(platformLayer), owner));
}

static NSString * const platformCALayerPointer = @"WKPlatformCALayer";
PlatformCALayer* PlatformCALayer::platformCALayer(void* platformLayer)
{
    if (!platformLayer)
        return 0;

    // Pointer to PlatformCALayer is kept in a key of the CALayer
    PlatformCALayer* platformCALayer = nil;
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    platformCALayer = static_cast<PlatformCALayer*>([[static_cast<CALayer*>(platformLayer) valueForKey:platformCALayerPointer] pointerValue]);
    END_BLOCK_OBJC_EXCEPTIONS
    return platformCALayer;
}

// This value must be the same as in PlatformCAAnimationMac.mm
static NSString * const WKNonZeroBeginTimeFlag = @"WKPlatformCAAnimationNonZeroBeginTimeFlag";

static double mediaTimeToCurrentTime(CFTimeInterval t)
{
    return monotonicallyIncreasingTime() + t - CACurrentMediaTime();
}

// Delegate for animationDidStart callback
@interface WebAnimationDelegate : NSObject {
    PlatformCALayer* m_owner;
}

- (void)animationDidStart:(CAAnimation *)anim;
- (void)setOwner:(PlatformCALayer*)owner;

@end

@implementation WebAnimationDelegate

- (void)animationDidStart:(CAAnimation *)animation
{
#if PLATFORM(IOS)
    WebThreadLock();
#endif
    // hasNonZeroBeginTime is stored in a key in the animation
    bool hasNonZeroBeginTime = [[animation valueForKey:WKNonZeroBeginTimeFlag] boolValue];
    CFTimeInterval startTime;

    if (hasNonZeroBeginTime) {
        // We don't know what time CA used to commit the animation, so just use the current time
        // (even though this will be slightly off).
        startTime = mediaTimeToCurrentTime(CACurrentMediaTime());
    } else
        startTime = mediaTimeToCurrentTime([animation beginTime]);

    if (m_owner)
        m_owner->animationStarted(startTime);
}

- (void)setOwner:(PlatformCALayer*)owner
{
    m_owner = owner;
}

@end

@interface CATiledLayer(GraphicsLayerCAPrivate)
- (void)displayInRect:(CGRect)r levelOfDetail:(int)lod options:(NSDictionary *)dict;
- (BOOL)canDrawConcurrently;
- (void)setCanDrawConcurrently:(BOOL)flag;
@end

@interface CALayer(Private)
- (void)setContentsChanged;
- (void)setAcceleratesDrawing:(BOOL)flag;
- (BOOL)acceleratesDrawing;
@end

void PlatformCALayerMac::setOwner(PlatformCALayerClient* owner)
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

PlatformCALayerMac::PlatformCALayerMac(LayerType layerType, PlatformCALayerClient* owner)
    : PlatformCALayer(layerType, owner)
    , m_customAppearance(GraphicsLayer::NoCustomAppearance)
{
    Class layerClass = Nil;
    switch (layerType) {
    case LayerTypeLayer:
    case LayerTypeRootLayer:
        layerClass = [CALayer class];
        break;
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
    case LayerTypeWebTiledLayer:
        ASSERT_NOT_REACHED();
        break;
    case LayerTypeTiledBackingLayer:
    case LayerTypePageTiledBackingLayer:
        layerClass = [WebTiledBackingLayer class];
        break;
    case LayerTypeAVPlayerLayer:
        layerClass = getAVPlayerLayerClass();
        break;
    case LayerTypeCustom:
        break;
    }

    if (layerClass)
        m_layer = adoptNS([[layerClass alloc] init]);
    
    commonInit();
}

PlatformCALayerMac::PlatformCALayerMac(PlatformLayer* layer, PlatformCALayerClient* owner)
    : PlatformCALayer([layer isKindOfClass:getAVPlayerLayerClass()] ? LayerTypeAVPlayerLayer : LayerTypeCustom, owner)
    , m_customAppearance(GraphicsLayer::NoCustomAppearance)
{
    m_layer = layer;
    commonInit();
}

void PlatformCALayerMac::commonInit()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    // Save a pointer to 'this' in the CALayer
    [m_layer.get() setValue:[NSValue valueWithPointer:this] forKey:platformCALayerPointer];
    
    // Clear all the implicit animations on the CALayer
    [m_layer web_disableAllActions];

    // So that the scrolling thread's performance logging code can find all the tiles, mark this as being a tile.
    if (m_layerType == LayerTypeTiledBackingTileLayer)
        [m_layer setValue:@YES forKey:@"isTile"];

    if (usesTiledBackingLayer()) {
        WebTiledBackingLayer* tiledBackingLayer = static_cast<WebTiledBackingLayer*>(m_layer.get());
        TileController* tileController = [tiledBackingLayer createTileController:this];

        m_customSublayers = adoptPtr(new PlatformCALayerList(1));
        PlatformCALayer* tileCacheTileContainerLayer = tileController->tileContainerLayer();
        (*m_customSublayers)[0] = tileCacheTileContainerLayer;
    }

    END_BLOCK_OBJC_EXCEPTIONS
}

PassRefPtr<PlatformCALayer> PlatformCALayerMac::clone(PlatformCALayerClient* owner) const
{
    LayerType type;
    switch (layerType()) {
    case LayerTypeTransformLayer:
        type = LayerTypeTransformLayer;
        break;
    case LayerTypeAVPlayerLayer:
        type = LayerTypeAVPlayerLayer;
        break;
    case LayerTypeLayer:
    default:
        type = LayerTypeLayer;
        break;
    };
    RefPtr<PlatformCALayer> newLayer = PlatformCALayerMac::create(type, owner);
    
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
#if ENABLE(CSS_FILTERS)
    newLayer->copyFiltersFrom(this);
#endif
    newLayer->updateCustomAppearance(customAppearance());

    if (type == LayerTypeAVPlayerLayer) {
        ASSERT([newLayer->platformLayer() isKindOfClass:getAVPlayerLayerClass()]);
        ASSERT([platformLayer() isKindOfClass:getAVPlayerLayerClass()]);

        AVPlayerLayer* destinationPlayerLayer = static_cast<AVPlayerLayer *>(newLayer->platformLayer());
        AVPlayerLayer* sourcePlayerLayer = static_cast<AVPlayerLayer *>(platformLayer());
        dispatch_async(dispatch_get_main_queue(), ^{
            [destinationPlayerLayer setPlayer:[sourcePlayerLayer player]];
        });
    }

    return newLayer;
}

PlatformCALayerMac::~PlatformCALayerMac()
{
    [m_layer.get() setValue:nil forKey:platformCALayerPointer];
    
    // Remove the owner pointer from the delegate in case there is a pending animationStarted event.
    [static_cast<WebAnimationDelegate*>(m_delegate.get()) setOwner:nil];

    if (usesTiledBackingLayer())
        [static_cast<WebTiledBackingLayer *>(m_layer.get()) invalidate];
}

void PlatformCALayerMac::animationStarted(CFTimeInterval beginTime)
{
    if (m_owner)
        m_owner->platformCALayerAnimationStarted(beginTime);
}

void PlatformCALayerMac::setNeedsDisplay(const FloatRect* dirtyRect)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if (dirtyRect)
        [m_layer.get() setNeedsDisplayInRect:*dirtyRect];
    else
        [m_layer.get() setNeedsDisplay];
    END_BLOCK_OBJC_EXCEPTIONS
}
    
void PlatformCALayerMac::setContentsChanged()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setContentsChanged];
    END_BLOCK_OBJC_EXCEPTIONS
}

PlatformCALayer* PlatformCALayerMac::superlayer() const
{
    return platformCALayer([m_layer.get() superlayer]);
}

void PlatformCALayerMac::removeFromSuperlayer()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() removeFromSuperlayer];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerMac::setSublayers(const PlatformCALayerList& list)
{
    // Short circuiting here avoids the allocation of the array below.
    if (list.size() == 0) {
        removeAllSublayers();
        return;
    }

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    NSMutableArray* sublayers = [[NSMutableArray alloc] init];
    for (size_t i = 0; i < list.size(); ++i)
        [sublayers addObject:list[i]->m_layer.get()];

    [m_layer.get() setSublayers:sublayers];
    [sublayers release];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerMac::removeAllSublayers()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setSublayers:nil];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerMac::appendSublayer(PlatformCALayer* layer)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    ASSERT(m_layer != layer->m_layer);
    [m_layer.get() addSublayer:layer->m_layer.get()];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerMac::insertSublayer(PlatformCALayer* layer, size_t index)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    ASSERT(m_layer != layer->m_layer);
    [m_layer.get() insertSublayer:layer->m_layer.get() atIndex:index];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerMac::replaceSublayer(PlatformCALayer* reference, PlatformCALayer* layer)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    ASSERT(m_layer != layer->m_layer);
    [m_layer.get() replaceSublayer:reference->m_layer.get() with:layer->m_layer.get()];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerMac::adoptSublayers(PlatformCALayer* source)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setSublayers:[source->m_layer.get() sublayers]];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerMac::addAnimationForKey(const String& key, PlatformCAAnimation* animation)
{
    // Add the delegate
    if (!m_delegate) {
        WebAnimationDelegate* webAnimationDelegate = [[WebAnimationDelegate alloc] init];
        m_delegate = adoptNS(webAnimationDelegate);
        [webAnimationDelegate setOwner:this];
    }
    
    CAPropertyAnimation* propertyAnimation = static_cast<CAPropertyAnimation*>(animation->platformAnimation());
    if (![propertyAnimation delegate])
        [propertyAnimation setDelegate:static_cast<id>(m_delegate.get())];
     
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() addAnimation:animation->m_animation.get() forKey:key];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerMac::removeAnimationForKey(const String& key)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() removeAnimationForKey:key];
    END_BLOCK_OBJC_EXCEPTIONS
}

PassRefPtr<PlatformCAAnimation> PlatformCALayerMac::animationForKey(const String& key)
{
    CAPropertyAnimation* propertyAnimation = static_cast<CAPropertyAnimation*>([m_layer.get() animationForKey:key]);
    if (!propertyAnimation)
        return 0;
    return PlatformCAAnimation::create(propertyAnimation);
}

void PlatformCALayerMac::setMask(PlatformCALayer* layer)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setMask:layer ? layer->platformLayer() : 0];
    END_BLOCK_OBJC_EXCEPTIONS
}

bool PlatformCALayerMac::isOpaque() const
{
    return [m_layer.get() isOpaque];
}

void PlatformCALayerMac::setOpaque(bool value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setOpaque:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

FloatRect PlatformCALayerMac::bounds() const
{
    return [m_layer.get() bounds];
}

void PlatformCALayerMac::setBounds(const FloatRect& value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setBounds:value];
    
    if (requiresCustomAppearanceUpdateOnBoundsChange())
        updateCustomAppearance(m_customAppearance);

    END_BLOCK_OBJC_EXCEPTIONS
}

FloatPoint3D PlatformCALayerMac::position() const
{
    CGPoint point = [m_layer.get() position];
    return FloatPoint3D(point.x, point.y, [m_layer.get() zPosition]);
}

void PlatformCALayerMac::setPosition(const FloatPoint3D& value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setPosition:CGPointMake(value.x(), value.y())];
    [m_layer.get() setZPosition:value.z()];
    END_BLOCK_OBJC_EXCEPTIONS
}

FloatPoint3D PlatformCALayerMac::anchorPoint() const
{
    CGPoint point = [m_layer.get() anchorPoint];
    float z = 0;
    z = [m_layer.get() anchorPointZ];
    return FloatPoint3D(point.x, point.y, z);
}

void PlatformCALayerMac::setAnchorPoint(const FloatPoint3D& value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setAnchorPoint:CGPointMake(value.x(), value.y())];
    [m_layer.get() setAnchorPointZ:value.z()];
    END_BLOCK_OBJC_EXCEPTIONS
}

TransformationMatrix PlatformCALayerMac::transform() const
{
    return [m_layer.get() transform];
}

void PlatformCALayerMac::setTransform(const TransformationMatrix& value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setTransform:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

TransformationMatrix PlatformCALayerMac::sublayerTransform() const
{
    return [m_layer.get() sublayerTransform];
}

void PlatformCALayerMac::setSublayerTransform(const TransformationMatrix& value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setSublayerTransform:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerMac::setHidden(bool value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setHidden:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerMac::setGeometryFlipped(bool value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setGeometryFlipped:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

bool PlatformCALayerMac::isDoubleSided() const
{
    return [m_layer.get() isDoubleSided];
}

void PlatformCALayerMac::setDoubleSided(bool value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setDoubleSided:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

bool PlatformCALayerMac::masksToBounds() const
{
    return [m_layer.get() masksToBounds];
}

void PlatformCALayerMac::setMasksToBounds(bool value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setMasksToBounds:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

bool PlatformCALayerMac::acceleratesDrawing() const
{
    return [m_layer.get() acceleratesDrawing];
}

void PlatformCALayerMac::setAcceleratesDrawing(bool acceleratesDrawing)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setAcceleratesDrawing:acceleratesDrawing];
    END_BLOCK_OBJC_EXCEPTIONS
}

CFTypeRef PlatformCALayerMac::contents() const
{
    return [m_layer.get() contents];
}

void PlatformCALayerMac::setContents(CFTypeRef value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setContents:static_cast<id>(const_cast<void*>(value))];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerMac::setContentsRect(const FloatRect& value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setContentsRect:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerMac::setMinificationFilter(FilterType value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setMinificationFilter:toCAFilterType(value)];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerMac::setMagnificationFilter(FilterType value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setMagnificationFilter:toCAFilterType(value)];
    END_BLOCK_OBJC_EXCEPTIONS
}

Color PlatformCALayerMac::backgroundColor() const
{
    return [m_layer.get() backgroundColor];
}

void PlatformCALayerMac::setBackgroundColor(const Color& value)
{
    CGFloat components[4];
    value.getRGBA(components[0], components[1], components[2], components[3]);

    RetainPtr<CGColorSpaceRef> colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());
    RetainPtr<CGColorRef> color = adoptCF(CGColorCreate(colorSpace.get(), components));

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setBackgroundColor:color.get()];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerMac::setBorderWidth(float value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setBorderWidth:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerMac::setBorderColor(const Color& value)
{
    CGFloat components[4];
    value.getRGBA(components[0], components[1], components[2], components[3]);

    RetainPtr<CGColorSpaceRef> colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());
    RetainPtr<CGColorRef> color = adoptCF(CGColorCreate(colorSpace.get(), components));

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setBorderColor:color.get()];
    END_BLOCK_OBJC_EXCEPTIONS
}

float PlatformCALayerMac::opacity() const
{
    return [m_layer.get() opacity];
}

void PlatformCALayerMac::setOpacity(float value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setOpacity:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

#if ENABLE(CSS_FILTERS)
void PlatformCALayerMac::setFilters(const FilterOperations& filters)
{
    PlatformCAFilters::setFiltersOnLayer(this->platformLayer(), filters);
}

void PlatformCALayerMac::copyFiltersFrom(const PlatformCALayer* sourceLayer)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setFilters:[sourceLayer->platformLayer() filters]];
    END_BLOCK_OBJC_EXCEPTIONS
}

bool PlatformCALayerMac::filtersCanBeComposited(const FilterOperations& filters)
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
#endif

#if ENABLE(CSS_COMPOSITING)
void PlatformCALayer::setBlendMode(BlendMode blendMode)
{
    PlatformCAFilters::setBlendingFiltersOnLayer(this, blendMode);
}
#endif

void PlatformCALayerMac::setName(const String& value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setName:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerMac::setSpeed(float value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setSpeed:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerMac::setTimeOffset(CFTimeInterval value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setTimeOffset:value];
    END_BLOCK_OBJC_EXCEPTIONS
}

float PlatformCALayerMac::contentsScale() const
{
    return [m_layer.get() contentsScale];
}

void PlatformCALayerMac::setContentsScale(float value)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setContentsScale:value];
#if PLATFORM(IOS)
    [m_layer.get() setRasterizationScale:value];

    if (m_layerType == LayerTypeWebTiledLayer) {
        // This will invalidate all the tiles so we won't end up with stale tiles with the wrong scale in the wrong place,
        // see <rdar://problem/9434765> for more information.
        static NSDictionary *optionsDictionary = [[NSDictionary alloc] initWithObjectsAndKeys:[NSNumber numberWithBool:YES], kCATiledLayerRemoveImmediately, nil];
        [(CATiledLayer *)m_layer.get() setNeedsDisplayInRect:[m_layer.get() bounds] levelOfDetail:0 options:optionsDictionary];
    }
#endif
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformCALayerMac::setEdgeAntialiasingMask(unsigned mask)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_layer.get() setEdgeAntialiasingMask:mask];
    END_BLOCK_OBJC_EXCEPTIONS
}

bool PlatformCALayerMac::requiresCustomAppearanceUpdateOnBoundsChange() const
{
    return m_customAppearance == GraphicsLayer::ScrollingShadow;
}

void PlatformCALayerMac::updateCustomAppearance(GraphicsLayer::CustomAppearance appearance)
{
    m_customAppearance = appearance;

#if ENABLE(RUBBER_BANDING)
    switch (appearance) {
    case GraphicsLayer::NoCustomAppearance:
        ScrollbarThemeMac::removeOverhangAreaBackground(platformLayer());
        ScrollbarThemeMac::removeOverhangAreaShadow(platformLayer());
        break;
    case GraphicsLayer::ScrollingOverhang:
        ScrollbarThemeMac::setUpOverhangAreaBackground(platformLayer());
        break;
    case GraphicsLayer::ScrollingShadow:
        ScrollbarThemeMac::setUpOverhangAreaShadow(platformLayer());
        break;
    }
#endif
}

TiledBacking* PlatformCALayerMac::tiledBacking()
{
    if (!usesTiledBackingLayer())
        return 0;

    WebTiledBackingLayer *tiledBackingLayer = static_cast<WebTiledBackingLayer *>(m_layer.get());
    return [tiledBackingLayer tiledBacking];
}

#if PLATFORM(IOS)
bool PlatformCALayer::isWebLayer()
{
    BOOL result = NO;
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    result = [m_layer.get() isKindOfClass:[WebLayer self]];
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

void PlatformCALayer::setTileSize(const IntSize& tileSize)
{
    if (m_layerType != LayerTypeWebTiledLayer)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [static_cast<WebTiledLayer*>(m_layer.get()) setTileSize:tileSize];
    END_BLOCK_OBJC_EXCEPTIONS
}
#endif // PLATFORM(IOS)

PassRefPtr<PlatformCALayer> PlatformCALayerMac::createCompatibleLayer(PlatformCALayer::LayerType layerType, PlatformCALayerClient* client) const
{
    return PlatformCALayerMac::create(layerType, client);
}

void PlatformCALayerMac::enumerateRectsBeingDrawn(CGContextRef context, void (^block)(CGRect))
{
    wkCALayerEnumerateRectsBeingDrawnWithBlock(m_layer.get(), context, block);
}

#endif // USE(ACCELERATED_COMPOSITING)
