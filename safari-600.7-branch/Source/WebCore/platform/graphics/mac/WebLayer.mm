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

#include "config.h"
#import "WebLayer.h"

#import "GraphicsContext.h"
#import "GraphicsLayerCA.h"
#import "PlatformCALayer.h"
#import <QuartzCore/QuartzCore.h>

#if PLATFORM(IOS)
#import "WKGraphics.h"
#import "WAKWindow.h"
#import "WebCoreThread.h"
#endif

@interface CALayer(WebCoreCALayerPrivate)
- (void)reloadValueForKeyPath:(NSString *)keyPath;
@end

using namespace WebCore;

#if PLATFORM(IOS)
@interface WebLayer(Private)
- (void)drawScaledContentsInContext:(CGContextRef)context;
@end
#endif

@implementation WebLayer

- (void)drawInContext:(CGContextRef)context
{
    PlatformCALayer* layer = PlatformCALayer::platformCALayer(self);
    if (layer) {
        PlatformCALayer::RepaintRectList rectsToPaint = PlatformCALayer::collectRectsToPaint(context, layer);
        PlatformCALayer::drawLayerContents(context, layer, rectsToPaint);
    }
}

@end // implementation WebLayer

@implementation WebSimpleLayer

- (id<CAAction>)actionForKey:(NSString *)key
{
    // Fix for <rdar://problem/9015675>: Force the layer content to be updated when the tree is reparented.
    if ([key isEqualToString:@"onOrderIn"])
        [self reloadValueForKeyPath:@"contents"];

    return nil;
}

- (void)setNeedsDisplay
{
    PlatformCALayer* layer = PlatformCALayer::platformCALayer(self);
    if (layer && layer->owner() && layer->owner()->platformCALayerDrawsContent())
        [super setNeedsDisplay];
}

- (void)setNeedsDisplayInRect:(CGRect)dirtyRect
{
    PlatformCALayer* platformLayer = PlatformCALayer::platformCALayer(self);
    if (!platformLayer) {
        [super setNeedsDisplayInRect:dirtyRect];
        return;
    }

    if (PlatformCALayerClient* layerOwner = platformLayer->owner()) {
        if (layerOwner->platformCALayerDrawsContent()) {
            [super setNeedsDisplayInRect:dirtyRect];

            if (layerOwner->platformCALayerShowRepaintCounter(platformLayer)) {
                CGRect bounds = [self bounds];
                CGRect indicatorRect = CGRectMake(bounds.origin.x, bounds.origin.y, 52, 27);
                [super setNeedsDisplayInRect:indicatorRect];
            }
        }
    }
}

- (void)display
{
#if PLATFORM(IOS)
    if (pthread_main_np())
        WebThreadLock();
#endif
    [super display];
    PlatformCALayer* layer = PlatformCALayer::platformCALayer(self);
    if (layer && layer->owner())
        layer->owner()->platformCALayerLayerDidDisplay(layer);
}

- (void)drawInContext:(CGContextRef)context
{
#if PLATFORM(IOS)
    if (pthread_main_np())
        WebThreadLock();
#endif
    PlatformCALayer* layer = PlatformCALayer::platformCALayer(self);
    if (layer && layer->owner()) {
        GraphicsContext graphicsContext(context);
        graphicsContext.setIsCALayerContext(true);
        graphicsContext.setIsAcceleratedContext(layer->acceleratesDrawing());

        FloatRect clipBounds = CGContextGetClipBoundingBox(context);
        layer->owner()->platformCALayerPaintContents(layer, graphicsContext, clipBounds);
    }
}

@end // implementation WebSimpleLayer

// MARK: -

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

    return curDesc;
}

- (NSString*)extendedDescription
{
    return [self _descriptionWithPrefix:@""];
}

@end  // implementation WebLayer(ExtendedDescription)

#endif // NDEBUG
