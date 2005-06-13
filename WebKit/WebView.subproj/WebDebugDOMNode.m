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
    self = [super init];
    if (!self)
        return nil;

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
