//
//  WebControllerPolicyHandler.m
//  WebKit
//
//  Created by Christopher Blumenberg on Thu Jul 25 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <WebKit/WebControllerPolicyHandler.h>

@interface WebPolicyPrivate : NSObject
{
@public
    WebPolicyAction policyAction;
    NSString *path;
    NSURL *URL;
}
@end

@implementation WebPolicyPrivate

- (void)dealloc
{
    [path release];
}

@end

@implementation WebPolicy
- initWithPolicyAction: (WebPolicyAction)action URL:(NSURL *)URL andPath:(NSString *)path;
{
    [super init];
    _private = [[WebPolicyPrivate alloc] init];
    _private->policyAction = action;
    _private->path = [path retain];
    _private->URL = [URL retain];
    return self;
}

- (void)_setPolicyAction:(WebPolicyAction)policyAction
{
    _private->policyAction = policyAction;
}

- (WebPolicyAction)policyAction
{
    return _private->policyAction;
}

- (NSString *)path
{
    return _private->path;
}

- (NSURL *)URL
{
    return _private->URL;
}

- (void)_setPath:(NSString *)path
{
    [_private->path release];
    _private->path = [path retain];
}


- (void)dealloc
{
    [_private release];
    [super dealloc];
}

@end

@implementation WebURLPolicy

+ webPolicyWithURLAction: (WebURLAction)action
{
    return [[[WebPolicy alloc] initWithPolicyAction:action URL:nil andPath:nil] autorelease];
}

@end

@implementation WebFileURLPolicy

+ webPolicyWithFileAction: (WebFileAction)action
{
    return [[[WebPolicy alloc] initWithPolicyAction:action URL:nil andPath:nil] autorelease];
}

@end

@implementation WebContentPolicy

+ webPolicyWithContentAction: (WebContentAction)action andPath: (NSString *)thePath
{
    return [[[WebPolicy alloc] initWithPolicyAction:action URL:nil andPath:thePath] autorelease];
}


@end

@implementation WebClickPolicy

+ webPolicyWithClickAction: (WebClickAction)action URL:(NSURL *)URL andPath: (NSString *)thePath;
{
    return [[[WebPolicy alloc] initWithPolicyAction:action URL:URL andPath:thePath] autorelease];
}


@end
