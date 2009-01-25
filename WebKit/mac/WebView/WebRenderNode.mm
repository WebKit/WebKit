/*
 * Copyright (C) 2005, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebRenderNode.h"

#import "WebFrameInternal.h"
#import "WebFrameView.h"
#import "WebHTMLView.h"
#import <WebCore/Frame.h>
#import <WebCore/RenderText.h>
#import <WebCore/RenderWidget.h>
#import <WebCore/RenderView.h>
#import <WebCore/Widget.h>

using namespace WebCore;

@implementation WebRenderNode

- (id)initWithName:(NSString *)n position:(NSPoint)p rect:(NSRect)r view:(NSView *)view children:(NSArray *)c
{
    NSMutableArray *collectChildren;
    
    self = [super init];
    if (!self)
        return nil;

    collectChildren = [c mutableCopy];

    name = [n retain];
    rect = r;
    absolutePosition = p;

    if ([view isKindOfClass:[NSScrollView class]]) {
        NSScrollView *scrollView = (NSScrollView *)view;
        view = [scrollView superview];
    }
    if ([view isKindOfClass:[WebFrameView class]]) {
        WebFrameView *webFrameView = (WebFrameView *)view;
        WebRenderNode *node = [[WebRenderNode alloc] initWithWebFrameView:webFrameView];
        [collectChildren addObject:node];
        [node release];
    }
    
    children = [collectChildren copy];
    [collectChildren release];
    
    return self;
}

static WebRenderNode *copyRenderNode(RenderObject* node)
{
    NSMutableArray *children = [[NSMutableArray alloc] init];
    for (RenderObject* child = node->firstChild(); child; child = child->nextSibling()) {
        WebRenderNode *childCopy = copyRenderNode(child);
        [children addObject:childCopy];
        [childCopy release];
    }

    NSString *name = [[NSString alloc] initWithUTF8String:node->renderName()];
    
    RenderWidget* renderWidget = node->isWidget() ? static_cast<RenderWidget*>(node) : 0;
    Widget* widget = renderWidget ? renderWidget->widget() : 0;
    NSView *view = widget ? widget->platformWidget() : nil;

    // FIXME: broken with transforms
    FloatPoint absPos = node->localToAbsolute(FloatPoint());
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    if (node->isBox()) {
        RenderBox* box = toRenderBox(node);
        x = box->x();
        y = box->y();
        width = box->width();
        height = box->height();
    } else if (node->isText()) {
        // FIXME: Preserve old behavior even though it's strange.
        RenderText* text = toRenderText(node);
        x = text->firstRunX();
        y = text->firstRunY();
        IntRect box = text->linesBoundingBox();
        width = box.width();
        height = box.height();
    }
    
    WebRenderNode *result = [[WebRenderNode alloc] initWithName:name
        position:absPos rect:NSMakeRect(x, y, width, height)
        view:view children:children];

    [name release];
    [children release];

    return result;
}

- (id)initWithWebFrameView:(WebFrameView *)view
{
    [self release];

    if (![[view documentView] isMemberOfClass:[WebHTMLView class]])
        return nil;

    RenderObject* renderer = core([view webFrame])->contentRenderer();
    if (!renderer)
        return nil;

    return copyRenderNode(renderer);
}

- (void)dealloc
{
    [children release];
    [name release];
    [super dealloc];
}

- (NSArray *)children
{
    return children;
}

- (NSString *)name
{
    return name;
}

- (NSString *)absolutePositionString
{
    return [NSString stringWithFormat:@"(%.0f, %.0f)", absolutePosition.x, absolutePosition.y];
}

- (NSString *)positionString
{
    return [NSString stringWithFormat:@"(%.0f, %.0f)", rect.origin.x, rect.origin.y];
}

- (NSString *)widthString
{
    return [NSString stringWithFormat:@"%.0f", rect.size.width];
}

- (NSString *)heightString
{
    return [NSString stringWithFormat:@"%.0f", rect.size.height];
}

@end
