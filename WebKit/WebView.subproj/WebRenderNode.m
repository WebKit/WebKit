//
//  WebRenderNode.m
//  WebKit
//
//  Created by Darin Adler on Tue Jun 11 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

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
