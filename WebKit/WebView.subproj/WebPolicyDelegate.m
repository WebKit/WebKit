/*
        WebPolicyDelegate.m
        Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebPolicyDelegatePrivate.h>

NSString *WebActionNavigationTypeKey = @"WebActionNavigationTypeKey";
NSString *WebActionElementKey = @"WebActionElementKey";
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


-(void)_usePolicy:(WebPolicyAction)policy
{
    if (_private->target != nil) {
	[_private->target performSelector:_private->action withObject:(id)policy];
    }
}

-(void)_invalidate
{
    [self retain];
    [_private->target release];
    _private->target = nil;
    [self release];
}

// WebPolicyDecisionListener implementation

-(void)use
{
    [self _usePolicy:WebPolicyUse];
}

-(void)ignore
{
    [self _usePolicy:WebPolicyIgnore];
}

-(void)download
{
    [self _usePolicy:WebPolicyDownload];
}

// WebFormSubmissionListener implementation

-(void)continue
{
    [self _usePolicy:WebPolicyUse];
}

@end
