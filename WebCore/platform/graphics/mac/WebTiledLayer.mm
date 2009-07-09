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

#import "WebTiledLayer.h"

#import "GraphicsContext.h"
#import "GraphicsLayer.h"
#import <wtf/UnusedParam.h>

using namespace WebCore;

@implementation WebTiledLayer

// Set a zero duration for the fade in of tiles
+ (CFTimeInterval)fadeDuration
{
    return 0;
}

// Make sure that tiles are drawn on the main thread
+ (BOOL)shouldDrawOnMainThread
{
    return YES;
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
#if defined(BUILDING_ON_LEOPARD)
        dirtyRect = CGRectApplyAffineTransform(dirtyRect, [self contentsTransform]);
#endif
        [super setNeedsDisplayInRect:dirtyRect];

#ifndef NDEBUG
        if (m_layerOwner->showRepaintCounter()) {
            CGRect bounds = [self bounds];
            CGRect indicatorRect = CGRectMake(bounds.origin.x, bounds.origin.y, 46, 25);
#if defined(BUILDING_ON_LEOPARD)
            indicatorRect = CGRectApplyAffineTransform(indicatorRect, [self contentsTransform]);
#endif
            [super setNeedsDisplayInRect:indicatorRect];
        }
#endif
    }
}

- (void)drawInContext:(CGContextRef)ctx
{
    [WebLayer drawContents:m_layerOwner ofLayer:self intoContext:ctx];
}

@end // implementation WebTiledLayer

#pragma mark -

@implementation WebTiledLayer(LayerMacAdditions)

- (void)setLayerOwner:(GraphicsLayer*)aLayer
{
    m_layerOwner = aLayer;
}

- (GraphicsLayer*)layerOwner
{
    return m_layerOwner;
}

@end

#endif // USE(ACCELERATED_COMPOSITING)
