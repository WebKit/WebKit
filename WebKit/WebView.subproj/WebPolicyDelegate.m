/*
        WebControllerPolicyDelegate.m
        Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebControllerPolicyDelegate.h>
#import <WebKit/WebControllerPolicyDelegatePrivate.h>

NSString *WebActionNavigationTypeKey = @"WebActionNavigationTypeKey";
NSString *WebActionElementKey = @"WebActionNavigationTypeKey";
NSString *WebActionButtonKey = @"WebActionButtonKey"; 
NSString *WebActionModifierFlagsKey = @"WebActionModifierFlagsKey";
NSString *WebActionOriginalURLKey = @"WebActionOriginalURLKey";


@interface WebPolicyDecisionListenerPrivate : NSObject
{
@public
    id target;
    SEL action;
}

-(id)initWithTarget:(id)target action:(SEL)action;

@end

@implementation WebPolicyDecisionListenerPrivate

-(id)initWithTarget:(id)t action:(SEL)a
{
    self = [super init];
    if (self != nil) {
	target = [t retain];
	action = a;
    }
    return self;
}

-(void)dealloc
{
    [target release];
    [super dealloc];
}

@end

@implementation WebPolicyDecisionListener

-(void)usePolicy:(WebPolicyAction)policy
{
    if (_private->target != nil) {
	[_private->target performSelector:_private->action withObject:(id)policy];
    }
}

@end

@implementation WebPolicyDecisionListener (WebPrivate)

-(id)_initWithTarget:(id)target action:(SEL)action
{
    self = [super init];
    if (self != nil) {
	_private = [[WebPolicyDecisionListenerPrivate alloc] initWithTarget:target action:action];
    }
    return self;
}

-(void)dealloc
{
    [_private release];
    [super dealloc];
}

-(void)_invalidate
{
    [self retain];
    [_private->target release];
    _private->target = nil;
    [self release];
}

@end
