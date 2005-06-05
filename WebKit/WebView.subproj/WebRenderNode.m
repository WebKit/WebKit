/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#import <WebKit/WebBridge.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebNSViewExtras.h>

@interface WebKitRenderTreeCopier : NSObject <WebCoreRenderTreeCopier>
@end

@implementation WebRenderNode

- initWithName:(NSString *)n position: (NSPoint)p rect:(NSRect)r view:(NSView *)view children:(NSArray *)c
{
    NSMutableArray *collectChildren;
    
    [super init];

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

- initWithWebFrameView:(WebFrameView *)view
{
    WebKitRenderTreeCopier *copier;
    
    [self release];

    if (![[view documentView] isMemberOfClass:[WebHTMLView class]]) {
        return nil;
    }
    
    copier = [[WebKitRenderTreeCopier alloc] init];
    WebHTMLView *htmlView = (WebHTMLView *)[view documentView];
    self = [[[htmlView _bridge] copyRenderTree:copier] retain];
    [copier release];
    
    return self;
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

@implementation WebKitRenderTreeCopier

- (NSObject *)nodeWithName:(NSString *)name position: (NSPoint)p rect:(NSRect)rect view:(NSView *)view children:(NSArray *)children
{
    return [[[WebRenderNode alloc] initWithName:name position: p rect:rect view:view children:children] autorelease];
}

@end
