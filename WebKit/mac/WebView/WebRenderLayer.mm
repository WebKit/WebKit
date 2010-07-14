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

#import "WebRenderLayer.h"

#import "WebFrameInternal.h"
#import <WebCore/Frame.h>
#import <WebCore/FrameLoaderClient.h>
#import <WebCore/PlatformString.h>
#import <WebCore/RenderLayer.h>
#import <WebCore/RenderView.h>
#import <WebCore/RenderView.h>

using namespace WebCore;


@interface WebRenderLayer(Private)

- (id)initWithRenderLayer:(RenderLayer *)layer;
- (void)buildDescendantLayers:(RenderLayer*)rootLayer;

@end

@implementation WebRenderLayer

+ (NSString *)nameForLayer:(RenderLayer*)layer;
{
    RenderObject* renderer = layer->renderer();
    NSString *name = [NSString stringWithUTF8String:renderer->renderName()];

    if (Node* node = renderer->node()) {
        if (node->isElementNode())
            name = [name stringByAppendingFormat:@" %@", (NSString *)static_cast<Element*>(node)->tagName()];

        if (node->hasID())
            name = [name stringByAppendingFormat:@" ‘%@’", (NSString *)static_cast<Element*>(node)->getIdAttribute()];
    }

    if (layer->isReflection())
        name = [name stringByAppendingString:@" (reflection)"];

    return name;
}

- (id)initWithRenderLayer:(RenderLayer*)layer
{
    if ((self = [super init])) {
        name = [[WebRenderLayer nameForLayer:layer] retain];
        bounds = layer->absoluteBoundingBox();
        composited = layer->isComposited();
    }

    return self;
}

// Only called on the root.
- (id)initWithWebFrame:(WebFrame *)webFrame
{
    self = [super init];
    
    Frame* frame = core(webFrame);
    if (!frame->loader()->client()->hasHTMLView()) {
        [self release];
        return nil;
    }
    
    RenderObject* renderer = frame->contentRenderer();
    if (!renderer) {
        [self release];
        return nil;
    }

    if (renderer->hasLayer()) {
        RenderLayer* layer = toRenderBoxModelObject(renderer)->layer();

        name = [[WebRenderLayer nameForLayer:layer] retain];
        bounds = layer->absoluteBoundingBox();
        composited = layer->isComposited();
    
        [self buildDescendantLayers:layer];
    }
    
    return self;
}

- (void)dealloc
{
    [children release];
    [name release];
    [super dealloc];
}

- (void)buildDescendantLayers:(RenderLayer*)layer
{
    NSMutableArray *childWebLayers = [[NSMutableArray alloc] init];

    // Build children in back to front order.
    
    if (Vector<RenderLayer*>* negZOrderList = layer->negZOrderList()) {
        size_t listSize = negZOrderList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = negZOrderList->at(i);

            WebRenderLayer* newLayer = [[WebRenderLayer alloc] initWithRenderLayer:curLayer];
            [newLayer buildDescendantLayers:curLayer];

            [childWebLayers addObject:newLayer];
            [newLayer release];
        }
    }

    if (Vector<RenderLayer*>* normalFlowList = layer->normalFlowList()) {
        size_t listSize = normalFlowList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = normalFlowList->at(i);

            WebRenderLayer* newLayer = [[WebRenderLayer alloc] initWithRenderLayer:curLayer];
            [newLayer buildDescendantLayers:curLayer];

            [childWebLayers addObject:newLayer];
            [newLayer release];
        }
    }

    if (Vector<RenderLayer*>* posZOrderList = layer->posZOrderList()) {
        size_t listSize = posZOrderList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = posZOrderList->at(i);

            WebRenderLayer* newLayer = [[WebRenderLayer alloc] initWithRenderLayer:curLayer];
            [newLayer buildDescendantLayers:curLayer];

            [childWebLayers addObject:newLayer];
            [newLayer release];
        }
    }

    children = childWebLayers;
}

- (NSArray *)children
{
    return children;
}

- (NSString *)name
{
    return name;
}

- (NSString *)positionString
{
    return [NSString stringWithFormat:@"(%.0f, %.0f)", bounds.origin.x, bounds.origin.y];
}

- (NSString *)widthString
{
    return [NSString stringWithFormat:@"%.0f", bounds.size.width];
}

- (NSString *)heightString
{
    return [NSString stringWithFormat:@"%.0f", bounds.size.height];
}

- (BOOL)composited
{
    return composited;
}

@end
