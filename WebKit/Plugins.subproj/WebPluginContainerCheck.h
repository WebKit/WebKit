//
//  WebPluginContainerCheck.h
//  WebKit
//
//  Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@class NSURLRequest;
@class NSString;
@class WebPluginController;
@class WebPolicyDecisionListener;

@interface WebPluginContainerCheck : NSObject
{
    NSURLRequest *_request;
    NSString *_target;
    WebPluginController *_controller;
    id _resultObject;
    SEL _resultSelector;
    BOOL _done;
    WebPolicyDecisionListener *_listener;
}

+ (id)checkWithRequest:(NSURLRequest *)request target:(NSString *)target resultObject:(id)obj selector:(SEL)selector controller:(WebPluginController *)controller;

- (id)initWithRequest:(NSURLRequest *)request target:(NSString *)target resultObject:(id)obj selector:(SEL)selector controller:(WebPluginController *)controller;

- (void)start;

- (void)cancel;

@end
