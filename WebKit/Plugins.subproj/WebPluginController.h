//
//  WebPluginController.h
//  WebKit
//
//  Created by Chris Blumenberg on Wed Oct 23 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@class WebHTMLView;
@class WebPluginPackage;
@class WebBridge;
@class WebView;

@interface WebPluginController : NSObject
{
    NSView *_documentView;
    NSMutableArray *_views;
    BOOL _started;
    NSMutableSet *_checksInProgress;
}

+ (NSView *)plugInViewWithArguments:(NSDictionary *)arguments fromPluginPackage:(WebPluginPackage *)plugin;
+ (BOOL)isPlugInView:(NSView *)view;

- (id)initWithDocumentView:(NSView *)view;

- (void)addPlugin:(NSView *)view;

- (void)startAllPlugins;
- (void)stopAllPlugins;
- (void)destroyAllPlugins;

- (WebBridge *)bridge;
- (WebView *)webView;

@end
