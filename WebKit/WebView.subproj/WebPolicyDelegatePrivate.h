/*
        WebControllerPolicyDelegatePrivate.h
        Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

#import <WebKit/WebControllerPolicyDelegate.h>

@interface WebPolicy (WebPrivate)

- (void)_setPolicyAction:(WebPolicyAction)policyAction;
- (void)_setPath:(NSString *)path;

@end