/*
        WebControllerPolicyDelegate.m
        Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebControllerPolicyDelegate.h>

NSString *WebActionNavigationTypeKey = @"WebActionNavigationTypeKey";
NSString *WebActionElementKey = @"WebActionNavigationTypeKey";
NSString *WebActionButtonKey = @"WebActionButtonKey"; 
NSString *WebActionModifierFlagsKey = @"WebActionModifierFlagsKey";


@interface WebPolicyPrivate : NSObject
{
@public
    WebPolicyAction policyAction;
}
@end

@implementation WebPolicyPrivate

- (void)dealloc
{
    [super dealloc];
}

@end

@implementation WebPolicy

- initWithPolicyAction: (WebPolicyAction)action;
{
    [super init];
    _private = [[WebPolicyPrivate alloc] init];
    _private->policyAction = action;
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

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

@end

@implementation WebURLPolicy

+ webPolicyWithURLAction: (WebURLAction)action
{
    return [[[WebPolicy alloc] initWithPolicyAction:action] autorelease];
}

@end

@implementation WebFileURLPolicy

+ webPolicyWithFileAction: (WebFileAction)action
{
    return [[[WebPolicy alloc] initWithPolicyAction:action] autorelease];
}

@end

@implementation WebContentPolicy

+ webPolicyWithContentAction: (WebContentAction)action
{
    return [[[WebPolicy alloc] initWithPolicyAction:action] autorelease];
}


@end

@implementation WebClickPolicy

+ webPolicyWithClickAction: (WebClickAction)action
{
    return [[[WebPolicy alloc] initWithPolicyAction:action] autorelease];
}

@end
