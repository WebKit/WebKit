/*
        WebDefaultPolicyDelegatePrivate.h
	Copyright 2002, Apple Computer, Inc.
 */

#import <Foundation/Foundation.h>

#import "WebDefaultPolicyDelegate.h"

@interface WebDefaultPolicyDelegate (WebPrivate)
+ (WebDefaultPolicyDelegate *)_sharedWebPolicyDelegate;
@end

