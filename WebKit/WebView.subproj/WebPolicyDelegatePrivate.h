/*	
        WebControllerPolicyDelegatePrivate.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/


#import <WebKit/WebControllerPolicyDelegate.h>

@interface WebPolicyDecisionListener (WebPrivate)

-(id)_initWithTarget:(id)target action:(SEL)action;

-(void)_invalidate;

@end
