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

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#import "WebLayer.h"

#import "GraphicsContext.h"
#import "GraphicsLayerCA.h"
#import "PlatformCALayer.h"
#import <objc/objc-runtime.h>
#import <QuartzCore/QuartzCore.h>
#import <wtf/UnusedParam.h>

@interface CALayer(WebCoreCALayerPrivate)
- (void)reloadValueForKeyPath:(NSString *)keyPath;
@end

using namespace WebCore;

@implementation WebLayer

void drawLayerContents(CGContextRef context, CALayer *layer, WebCore::PlatformCALayer* platformLayer)
{
    WebCore::PlatformCALayerClient* layerContents = platformLayer->owner();
    if (!layerContents)
        return;

    CGContextSaveGState(context);

    CGRect layerBounds = [layer bounds];
    if (layerContents->platformCALayerContentsOrientation() == WebCore::GraphicsLayer::CompositingCoordinatesBottomUp) {
        CGContextScaleCTM(context, 1, -1);
        CGContextTranslateCTM(context, 0, -layerBounds.size.height);
    }

    [NSGraphicsContext saveGraphicsState];

    // Set up an NSGraphicsContext for the context, so that parts of AppKit that rely on
    // the current NSGraphicsContext (e.g. NSCell drawing) get the right one.
    NSGraphicsContext* layerContext = [NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:YES];
    [NSGraphicsContext setCurrentContext:layerContext];

    GraphicsContext graphicsContext(context);
    graphicsContext.setIsCALayerContext(true);
    graphicsContext.setIsAcceleratedContext(platformLayer->acceleratesDrawing());

    if (!layerContents->platformCALayerContentsOpaque()) {
        // Turn off font smoothing to improve the appearance of text rendered onto a transparent background.
        graphicsContext.setShouldSmoothFonts(false);
    }
    
    // It's important to get the clip from the context, because it may be significantly
    // smaller than the layer bounds (e.g. tiled layers)
    CGRect clipBounds = CGContextGetClipBoundingBox(context);
    IntRect clip(enclosingIntRect(clipBounds));
    layerContents->platformCALayerPaintContents(graphicsContext, clip);

    [NSGraphicsContext restoreGraphicsState];

    // Re-fetch the layer owner, since <rdar://problem/9125151> indicates that it might have been destroyed during painting.
    layerContents = platformLayer->owner();
    ASSERT(layerContents);
    if (layerContents && layerContents->platformCALayerShowRepaintCounter()) {
        bool isTiledLayer = [layer isKindOfClass:[CATiledLayer class]];

        char text[16]; // that's a lot of repaints
        snprintf(text, sizeof(text), "%d", layerContents->platformCALayerIncrementRepaintCount());

        CGRect indicatorBox = layerBounds;
        indicatorBox.size.width = 12 + 10 * strlen(text);
        indicatorBox.size.height = 27;
        CGContextSaveGState(context);
        
        CGContextSetAlpha(context, 0.5f);
        CGContextBeginTransparencyLayerWithRect(context, indicatorBox, 0);

        if (isTiledLayer)
            CGContextSetRGBFillColor(context, 1, 0.5f, 0, 1);
        else
            CGContextSetRGBFillColor(context, 0, 0.5f, 0.25f, 1);
        
        CGContextFillRect(context, indicatorBox);
        
        if (platformLayer->acceleratesDrawing())
            CGContextSetRGBFillColor(context, 1, 0, 0, 1);
        else
            CGContextSetRGBFillColor(context, 1, 1, 1, 1);

        CGContextSetTextMatrix(context, CGAffineTransformMakeScale(1, -1));
        CGContextSelectFont(context, "Helvetica", 22, kCGEncodingMacRoman);
        CGContextShowTextAtPoint(context, indicatorBox.origin.x + 5, indicatorBox.origin.y + 22, text, strlen(text));
        
        CGContextEndTransparencyLayer(context);
        CGContextRestoreGState(context);        
    }

    CGContextRestoreGState(context);
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
            if (layerOwner->platformCALayerContentsOrientation() == WebCore::GraphicsLayer::CompositingCoordinatesBottomUp)
                dirtyRect.origin.y = [self bounds].size.height - dirtyRect.origin.y - dirtyRect.size.height;

            [super setNeedsDisplayInRect:dirtyRect];

            if (layerOwner->platformCALayerShowRepaintCounter()) {
                CGRect bounds = [self bounds];
                CGRect indicatorRect = CGRectMake(bounds.origin.x, bounds.origin.y, 52, 27);
                if (layerOwner->platformCALayerContentsOrientation() == WebCore::GraphicsLayer::CompositingCoordinatesBottomUp)
                    indicatorRect.origin.y = [self bounds].size.height - indicatorRect.origin.y - indicatorRect.size.height;

                [super setNeedsDisplayInRect:indicatorRect];
            }
        }
    }
}

- (void)display
{
    [super display];
    PlatformCALayer* layer = PlatformCALayer::platformCALayer(self);
    if (layer && layer->owner())
        layer->owner()->platformCALayerLayerDidDisplay(self);
}

- (void)drawInContext:(CGContextRef)context
{
    PlatformCALayer* layer = PlatformCALayer::platformCALayer(self);
    if (layer)
        drawLayerContents(context, self, layer);
}

@end // implementation WebLayer

// MARK: -

#ifndef NDEBUG

@implementation CALayer(ExtendedDescription)

- (NSString*)_descriptionWithPrefix:(NSString*)inPrefix
{
    CGRect aBounds = [self bounds];
    CGPoint aPos = [self position];

    NSString* selfString = [NSString stringWithFormat:@"%@<%@ 0x%08x> \"%@\" bounds(%.1f, %.1f, %.1f, %.1f) pos(%.1f, %.1f), sublayers=%d masking=%d",
            inPrefix,
            [self class],
            self,
            [self name],
            aBounds.origin.x, aBounds.origin.y, aBounds.size.width, aBounds.size.height, 
            aPos.x, aPos.y,
            [[self sublayers] count],
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

#endif // USE(ACCELERATED_COMPOSITING)
