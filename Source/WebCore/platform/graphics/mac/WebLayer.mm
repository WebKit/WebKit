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

#if !PLATFORM(IOS)
#import "ThemeMac.h"
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

namespace WebCore {

RepaintRectList collectRectsToPaint(CGContextRef context, PlatformCALayer* platformCALayer)
{
    __block double totalRectArea = 0;
    __block unsigned rectCount = 0;
    __block RepaintRectList dirtyRects;

    platformCALayer->enumerateRectsBeingDrawn(context, ^(CGRect rect) {
        if (++rectCount > webLayerMaxRectsToPaint)
            return;

        totalRectArea += rect.size.width * rect.size.height;
        dirtyRects.append(rect);
    });

    FloatRect clipBounds = CGContextGetClipBoundingBox(context);
    double clipArea = clipBounds.width() * clipBounds.height();

    if (rectCount >= webLayerMaxRectsToPaint || totalRectArea >= clipArea * webLayerWastedSpaceThreshold) {
        dirtyRects.clear();
        dirtyRects.append(clipBounds);
    }

    return dirtyRects;
}

void drawLayerContents(CGContextRef context, WebCore::PlatformCALayer* platformCALayer, RepaintRectList& dirtyRects)
{
    WebCore::PlatformCALayerClient* layerContents = platformCALayer->owner();
    if (!layerContents)
        return;

#if PLATFORM(IOS)
    WKSetCurrentGraphicsContext(context);
#endif

    CGContextSaveGState(context);

    // We never use CompositingCoordinatesBottomUp on Mac.
    ASSERT(layerContents->platformCALayerContentsOrientation() == GraphicsLayer::CompositingCoordinatesTopDown);

#if PLATFORM(IOS)
    WKFontAntialiasingStateSaver fontAntialiasingState(context, [platformCALayer->platformLayer() isOpaque]);
    fontAntialiasingState.setup([WAKWindow hasLandscapeOrientation]);
#else
    [NSGraphicsContext saveGraphicsState];

    // Set up an NSGraphicsContext for the context, so that parts of AppKit that rely on
    // the current NSGraphicsContext (e.g. NSCell drawing) get the right one.
    NSGraphicsContext* layerContext = [NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:YES];
    [NSGraphicsContext setCurrentContext:layerContext];
#endif

    GraphicsContext graphicsContext(context);
    graphicsContext.setIsCALayerContext(true);
    graphicsContext.setIsAcceleratedContext(platformCALayer->acceleratesDrawing());

    if (!layerContents->platformCALayerContentsOpaque()) {
        // Turn off font smoothing to improve the appearance of text rendered onto a transparent background.
        graphicsContext.setShouldSmoothFonts(false);
    }

#if !PLATFORM(IOS)
    // It's important to get the clip from the context, because it may be significantly
    // smaller than the layer bounds (e.g. tiled layers)
    FloatRect clipBounds = CGContextGetClipBoundingBox(context);

    FloatRect focusRingClipRect = clipBounds;
#if __MAC_OS_X_VERSION_MIN_REQUIRED < 1090
    // Set the focus ring clip rect which needs to be in base coordinates.
    AffineTransform transform = CGContextGetCTM(context);
    focusRingClipRect = transform.mapRect(clipBounds);
#endif
    ThemeMac::setFocusRingClipRect(focusRingClipRect);
#endif // !PLATFORM(IOS)

    for (auto rect : dirtyRects) {
        GraphicsContextStateSaver stateSaver(graphicsContext);
        graphicsContext.clip(rect);

        layerContents->platformCALayerPaintContents(platformCALayer, graphicsContext, enclosingIntRect(rect));
    }

#if PLATFORM(IOS)
    fontAntialiasingState.restore();
#else
    ThemeMac::setFocusRingClipRect(FloatRect());

    [NSGraphicsContext restoreGraphicsState];
#endif

    // Re-fetch the layer owner, since <rdar://problem/9125151> indicates that it might have been destroyed during painting.
    layerContents = platformCALayer->owner();
    ASSERT(layerContents);

    CGContextRestoreGState(context);

    // Always update the repaint count so that it's accurate even if the count itself is not shown. This will be useful
    // for the Web Inspector feeding this information through the LayerTreeAgent.
    int repaintCount = layerContents->platformCALayerIncrementRepaintCount(platformCALayer);

    if (!platformCALayer->usesTiledBackingLayer() && layerContents && layerContents->platformCALayerShowRepaintCounter(platformCALayer))
        drawRepaintIndicator(context, platformCALayer, repaintCount, nullptr);
}

void drawRepaintIndicator(CGContextRef context, PlatformCALayer* platformCALayer, int repaintCount, CGColorRef customBackgroundColor)
{
    char text[16]; // that's a lot of repaints
    snprintf(text, sizeof(text), "%d", repaintCount);

    CGRect indicatorBox = platformCALayer->bounds();
    indicatorBox.size.width = 12 + 10 * strlen(text);
    indicatorBox.size.height = 27;
    CGContextSaveGState(context);

    CGContextSetAlpha(context, 0.5f);
    CGContextBeginTransparencyLayerWithRect(context, indicatorBox, 0);

    if (customBackgroundColor)
        CGContextSetFillColorWithColor(context, customBackgroundColor);
    else
        CGContextSetRGBFillColor(context, 0, 0.5f, 0.25f, 1);

    CGContextFillRect(context, indicatorBox);

    if (platformCALayer->acceleratesDrawing())
        CGContextSetRGBFillColor(context, 1, 0, 0, 1);
    else
        CGContextSetRGBFillColor(context, 1, 1, 1, 1);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    CGContextSetTextMatrix(context, CGAffineTransformMakeScale(1, -1));
    CGContextSelectFont(context, "Helvetica", 22, kCGEncodingMacRoman);
    CGContextShowTextAtPoint(context, indicatorBox.origin.x + 5, indicatorBox.origin.y + 22, text, strlen(text));
#pragma clang diagnostic pop

    CGContextEndTransparencyLayer(context);
    CGContextRestoreGState(context);
}

}

@implementation WebLayer

- (void)drawInContext:(CGContextRef)context
{
    PlatformCALayer* layer = PlatformCALayer::platformCALayer(self);
    if (layer) {
        RepaintRectList rectsToPaint = collectRectsToPaint(context, layer);
        drawLayerContents(context, layer, rectsToPaint);
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
        layer->owner()->platformCALayerLayerDidDisplay(self);
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
        layer->owner()->platformCALayerPaintContents(layer, graphicsContext, enclosingIntRect(clipBounds));
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
