/*
        WebControllerPolicyDelegate.m
        Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebControllerPolicyDelegate.h>

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
    [URL release];
    [super dealloc];
}

@end

@implementation WebPolicy

- initWithPolicyAction: (WebPolicyAction)action URL:(NSURL *)URL andPath:(NSString *)path;
{
    [super init];
    _private = [[WebPolicyPrivate alloc] init];
    _private->policyAction = action;
    _private->path = [path copy];
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
    NSString *copy = [path copy];
    [_private->path release];
    _private->path = copy;
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
