/*
 * Copyright (C) 2009, 2014 Apple Inc. All rights reserved.
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
#import "WebLayer.h"

#import "GraphicsContextCG.h"
#import "GraphicsLayerCA.h"
#import "PlatformCALayer.h"
#import <QuartzCore/QuartzCore.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/SetForScope.h>

#if PLATFORM(IOS_FAMILY)
#import "WKGraphics.h"
#import "WAKWindow.h"
#import "WebCoreThread.h"
#endif

#if PLATFORM(IOS_FAMILY)
@interface WebLayer(Private)
- (void)drawScaledContentsInContext:(CGContextRef)context;
@end
#endif

@implementation WebLayer

- (void)drawInContext:(CGContextRef)context
{
    auto layer = WebCore::PlatformCALayer::platformCALayerForLayer((__bridge void*)self);
    if (layer) {
        WebCore::GraphicsContextCG graphicsContext(context);
        WebCore::PlatformCALayer::RepaintRectList rectsToPaint = WebCore::PlatformCALayer::collectRectsToPaint(graphicsContext, layer.get());
        WebCore::PlatformCALayer::drawLayerContents(graphicsContext, layer.get(), rectsToPaint, self.isRenderingInContext ? WebCore::GraphicsLayerPaintSnapshotting : WebCore::GraphicsLayerPaintNormal);
    }
}

@end // implementation WebLayer

@implementation WebSimpleLayer

@synthesize isRenderingInContext = _isRenderingInContext;

- (void)renderInContext:(CGContextRef)context
{
    SetForScope<BOOL> change(_isRenderingInContext, YES);
    [super renderInContext:context];
}

- (id<CAAction>)actionForKey:(NSString *)key
{
    // Fix for <rdar://problem/9015675>: Force the layer content to be updated when the tree is reparented.
    if ([key isEqualToString:@"onOrderIn"])
        [self reloadValueForKeyPath:@"contents"];

    return nil;
}

- (void)setNeedsDisplay
{
    auto layer = WebCore::PlatformCALayer::platformCALayerForLayer((__bridge void*)self);
    if (layer && layer->owner() && layer->owner()->platformCALayerDrawsContent())
        [super setNeedsDisplay];
}

- (void)setNeedsDisplayInRect:(CGRect)dirtyRect
{
    auto platformLayer = WebCore::PlatformCALayer::platformCALayerForLayer((__bridge void*)self);
    if (!platformLayer) {
        [super setNeedsDisplayInRect:dirtyRect];
        return;
    }

    if (WebCore::PlatformCALayerClient* layerOwner = platformLayer->owner()) {
        if (layerOwner->platformCALayerDrawsContent()) {
            [super setNeedsDisplayInRect:dirtyRect];

            if (layerOwner->platformCALayerShowRepaintCounter(platformLayer.get())) {
                CGRect bounds = [self bounds];
                CGRect indicatorRect = CGRectMake(bounds.origin.x, bounds.origin.y, 52, 27);
                [super setNeedsDisplayInRect:indicatorRect];
            }
        }
    }
}

- (void)display
{
#if PLATFORM(IOS_FAMILY)
    if (pthread_main_np())
        WebThreadLock();
#endif
    ASSERT(isMainThread());
    [super display];
    auto layer = WebCore::PlatformCALayer::platformCALayerForLayer((__bridge void*)self);
    if (layer && layer->owner())
        layer->owner()->platformCALayerLayerDidDisplay(layer.get());
}

- (void)drawInContext:(CGContextRef)context
{
#if PLATFORM(IOS_FAMILY)
    if (pthread_main_np())
        WebThreadLock();
#endif
    ASSERT(isMainThread());
    auto layer = WebCore::PlatformCALayer::platformCALayerForLayer((__bridge void*)self);
    if (layer && layer->owner()) {
        WebCore::GraphicsContextCG graphicsContext(context);
        graphicsContext.setIsCALayerContext(true);
        graphicsContext.setIsAcceleratedContext(layer->acceleratesDrawing());

        WebCore::FloatRect clipBounds = CGContextGetClipBoundingBox(context);
        layer->owner()->platformCALayerPaintContents(layer.get(), graphicsContext, clipBounds, self.isRenderingInContext ? WebCore::GraphicsLayerPaintSnapshotting : WebCore::GraphicsLayerPaintNormal);
    }
}

@end // implementation WebSimpleLayer

#ifndef NDEBUG

@implementation CALayer(ExtendedDescription)

- (NSString*)_descriptionWithPrefix:(NSString*)inPrefix
{
    CGRect aBounds = [self bounds];
    CGPoint aPos = [self position];

    NSString* selfString = [NSString stringWithFormat:@"%@<%@ 0x%p> \"%@\" bounds(%.1f, %.1f, %.1f, %.1f) pos(%.1f, %.1f), sublayers=%lu masking=%d",
            inPrefix,
            [self class],
            self,
            [self name],
            aBounds.origin.x, aBounds.origin.y, aBounds.size.width, aBounds.size.height, 
            aPos.x, aPos.y,
            static_cast<unsigned long>([[self sublayers] count]),
            [self masksToBounds]];
    
    NSMutableString* curDesc = [NSMutableString stringWithString:selfString];
    
    if ([[self sublayers] count] > 0)
        [curDesc appendString:@"\n"];

    NSString* sublayerPrefix = [inPrefix stringByAppendingString:@"\t"];

    NSEnumerator* sublayersEnum = [[self sublayers] objectEnumerator];
    CALayer* curLayer;
    while ((curLayer = [sublayersEnum nextObject]))
        [curDesc appendString:[curLayer _descriptionWithPrefix:sublayerPrefix]];

    if ([[self sublayers] count] == 0)
        [curDesc appendString:@"\n"];

    if (CALayer *mask = [self mask]) {
        [curDesc appendString:@"mask: "];
        [curDesc appendString:[mask _descriptionWithPrefix:sublayerPrefix]];
    }

    return curDesc;
}

- (NSString*)extendedDescription
{
    return [self _descriptionWithPrefix:@""];
}

@end  // implementation WebLayer(ExtendedDescription)

#endif // NDEBUG
