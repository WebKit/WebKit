/*
 *  WebControllerPolicyHandlerPrivate.h
 *  WebKit
 *
 *  Created by Chris Blumenberg on Thu Jul 25 2002.
 *  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 */

#import <Cocoa/Cocoa.h>

#import <WebKit/WebControllerPolicyHandler.h>

@interface WebPolicy (WebPrivate)

- (void)_setPolicyAction:(WebPolicyAction)policyAction;
- (void)_setPath:(NSString *)path;

@end