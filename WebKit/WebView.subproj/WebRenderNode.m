//
//  IFRenderNode.m
//  WebKit
//
//  Created by Darin Adler on Tue Jun 11 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "IFRenderNode.h"

#import <WebKit/IFWebCoreBridge.h>
#import <WebKit/IFWebView.h>
#import <WebKit/IFHTMLViewPrivate.h>

@interface WebKitRenderTreeCopier : NSObject <WebCoreRenderTreeCopier>
@end

@implementation IFRenderNode

- initWithName:(NSString *)n rect:(NSRect)r view:(NSView *)view children:(NSArray *)c
{
    NSMutableArray *collectChildren;
    
    [super init];

    collectChildren = [c mutableCopy];

    name = [n retain];
    rect = r;

    if ([view isKindOfClass:[NSScrollView class]]) {
        NSScrollView *scrollView = (NSScrollView *)view;
        view = [scrollView superview];
    }
    if ([view isKindOfClass:[IFWebView class]]) {
        IFWebView *webView = (IFWebView *)view;
        [collectChildren addObject:[[[IFRenderNode alloc] initWithWebView:webView] autorelease]];
    }
    
    children = [collectChildren copy];
    [collectChildren release];
    
    return self;
}

- initWithWebView:(IFWebView *)view
{
    WebKitRenderTreeCopier *copier;
    
    [self dealloc];

    if (![[view documentView] isMemberOfClass:[IFHTMLView class]]) {
        return nil;
    }
    
    copier = [[WebKitRenderTreeCopier alloc] init];
    IFHTMLView *htmlView = (IFHTMLView *)[view documentView];
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

- (NSObject *)nodeWithName:(NSString *)name rect:(NSRect)rect view:(NSView *)view children:(NSArray *)children
{
    return [[[IFRenderNode alloc] initWithName:name rect:rect view:view children:children] autorelease];
}

@end
