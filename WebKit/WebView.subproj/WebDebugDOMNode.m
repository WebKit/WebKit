/*	
    WebDebugDOMNode.m
    Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import "WebDebugDOMNode.h"

#import <WebKit/WebBridge.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebNSViewExtras.h>

@interface WebKitDOMTreeCopier : NSObject <WebCoreDOMTreeCopier>
@end

@implementation WebDebugDOMNode

- initWithName:(NSString *)n value:(NSString *)v source:(NSString *)s children:(NSArray *)c
{
    [super init];

    children = [c copy];
    name = [n copy];
    value = [v copy];
    source = [s copy];
    
    return self;
}

- initWithWebFrameView:(WebFrameView *)view
{
    WebKitDOMTreeCopier *copier;
    
    [self release];

    if (![[view documentView] isMemberOfClass:[WebHTMLView class]]) {
        return nil;
    }
    
    copier = [[WebKitDOMTreeCopier alloc] init];
    WebHTMLView *htmlView = (WebHTMLView *)[view documentView];
    self = [[[htmlView _bridge] copyDOMTree:copier] retain];
    [copier release];
    
    return self;
}

- (void)dealloc
{
    [children release];
    [name release];
    [value release];
    [source release];
    
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

- (NSString *)value
{
    return value;
}

- (NSString *)source
{
    return source;
}

@end

@implementation WebKitDOMTreeCopier

- (NSObject *)nodeWithName:(NSString *)n value:(NSString *)v source:(NSString *)s children:(NSArray *)c
{
    return [[[WebDebugDOMNode alloc] initWithName:n value:v source:s children:c] autorelease];
}

@end
