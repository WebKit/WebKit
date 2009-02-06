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
#import "GraphicsLayer.h"
#import <QuartzCore/QuartzCore.h>
#import "WebCoreTextRenderer.h"
#import <wtf/UnusedParam.h>

using namespace WebCore;

@implementation WebLayer

+ (void)drawContents:(WebCore::GraphicsLayer*)layerContents ofLayer:(CALayer*)layer intoContext:(CGContextRef)context
{
    UNUSED_PARAM(layer);
    CGContextSaveGState(context);
    
    if (layerContents && layerContents->client()) {
        [NSGraphicsContext saveGraphicsState];

        // Set up an NSGraphicsContext for the context, so that parts of AppKit that rely on
        // the current NSGraphicsContext (e.g. NSCell drawing) get the right one.
        NSGraphicsContext* layerContext = [NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:YES];
        [NSGraphicsContext setCurrentContext:layerContext];

        GraphicsContext graphicsContext(context);

        // It's important to get the clip from the context, because it may be significantly
        // smaller than the layer bounds (e.g. tiled layers)
        CGRect clipBounds = CGContextGetClipBoundingBox(context);
        IntRect clip(enclosingIntRect(clipBounds));
        layerContents->paintGraphicsLayerContents(graphicsContext, clip);

        [NSGraphicsContext restoreGraphicsState];
    }
#ifndef NDEBUG
    else {
        ASSERT_NOT_REACHED();

        // FIXME: ideally we'd avoid calling -setNeedsDisplay on a layer that is a plain color,
        // so CA never makes backing store for it (which is what -setNeedsDisplay will do above).
        CGContextSetRGBFillColor(context, 0.0f, 1.0f, 0.0f, 1.0f);
        CGRect aBounds = [layer bounds];
        CGContextFillRect(context, aBounds);
    }
#endif

    CGContextRestoreGState(context);

#ifndef NDEBUG
    if (layerContents && layerContents->showRepaintCounter()) {
        bool isTiledLayer = [layer isKindOfClass:[CATiledLayer class]];

        char text[16]; // that's a lot of repaints
        snprintf(text, sizeof(text), "%d", layerContents->incrementRepaintCount());

        CGAffineTransform a = CGContextGetCTM(context);
        
        CGContextSaveGState(context);
        if (isTiledLayer)
            CGContextSetRGBFillColor(context, 0.0f, 1.0f, 0.0f, 0.8f);
        else
            CGContextSetRGBFillColor(context, 1.0f, 0.0f, 0.0f, 0.8f);
        
        CGRect aBounds = [layer bounds];

        aBounds.size.width = 10 + 12 * strlen(text);
        aBounds.size.height = 25;
        CGContextFillRect(context, aBounds);
        
        CGContextSetRGBFillColor(context, 0.0f, 0.0f, 0.0f, 1.0f);

        CGContextSetTextMatrix(context, CGAffineTransformMakeScale(1.0f, -1.0f));
        CGContextSelectFont(context, "Helvetica", 25, kCGEncodingMacRoman);
        CGContextShowTextAtPoint(context, aBounds.origin.x + 3.0f, aBounds.origin.y + 20.0f, text, strlen(text));
        
        CGContextRestoreGState(context);        
    }
#endif
}

// Disable default animations
- (id<CAAction>)actionForKey:(NSString *)key
{
    UNUSED_PARAM(key);
    return nil;
}

// Implement this so presentationLayer can get our custom attributes
- (id)initWithLayer:(id)layer
{
    if ((self = [super initWithLayer:layer]))
        m_layerOwner = [(WebLayer*)layer layerOwner];

    return self;
}

- (void)setNeedsDisplay
{
    if (m_layerOwner && m_layerOwner->client() && m_layerOwner->drawsContent())
        [super setNeedsDisplay];
}

- (void)setNeedsDisplayInRect:(CGRect)dirtyRect
{
    if (m_layerOwner && m_layerOwner->client() && m_layerOwner->drawsContent()) {
        [super setNeedsDisplayInRect:dirtyRect];

#ifndef NDEBUG
        if (m_layerOwner->showRepaintCounter()) {
            CGRect bounds = [self bounds];
            [super setNeedsDisplayInRect:CGRectMake(bounds.origin.x, bounds.origin.y, 46, 25)];
        }
#endif
    }
}

- (void)drawInContext:(CGContextRef)context
{
    [WebLayer drawContents:m_layerOwner ofLayer:self intoContext:context];
}

@end // implementation WebLayer

#pragma mark -

@implementation WebLayer(WebLayerAdditions)

- (void)setLayerOwner:(GraphicsLayer*)aLayer
{
    m_layerOwner = aLayer;
}

- (GraphicsLayer*)layerOwner
{
    return m_layerOwner;
}

@end

#pragma mark -

#ifndef NDEBUG

@implementation CALayer(ExtendedDescription)

- (NSString*)_descriptionWithPrefix:(NSString*)inPrefix
{
    CGRect aBounds = [self bounds];
    CGPoint aPos = [self position];
    CATransform3D t = [self transform];

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
