//
//  IFDOMNode.m
//  WebKit
//
//  Created by Darin Adler on Tue Jun 11 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "IFDOMNode.h"

#import <WebKit/IFWebView.h>
#import <WebKit/IFWebCoreBridge.h>
#import <WebKit/IFHTMLViewPrivate.h>

@interface WebKitDOMTreeCopier : NSObject <WebCoreDOMTreeCopier>
@end

@implementation IFDOMNode

- initWithName:(NSString *)n value:(NSString *)v source:(NSString *)s children:(NSArray *)c
{
    [super init];

    children = [c copy];
    name = [n copy];
    value = [v copy];
    source = [s copy];
    
    return self;
}

- initWithWebView:(IFWebView *)view
{
    WebKitDOMTreeCopier *copier;
    
    [self dealloc];

    if (![[view documentView] isMemberOfClass:[IFHTMLView class]]) {
        return nil;
    }
    
    copier = [[WebKitDOMTreeCopier alloc] init];
    self = [[[(IFHTMLView *)[view documentView] _bridge] copyDOMTree:copier] retain];
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
    return [[[IFDOMNode alloc] initWithName:n value:v source:s children:c] autorelease];
}

@end
