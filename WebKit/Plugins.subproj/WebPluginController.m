//
//  WebPluginController.m
//  WebKit
//
//  Created by Chris Blumenberg on Wed Oct 23 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebPluginController.h>

#import <WebKit/WebFrame.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebHTMLView.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebPlugin.h>
#import <WebKit/WebPluginContainer.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebUIDelegate.h>

#import <Foundation/NSURLRequest.h>

@implementation WebPluginController

- initWithHTMLView:(WebHTMLView *)HTMLView
{
    [super init];
    _HTMLView = HTMLView;
    _views = [[NSMutableArray alloc] init];
    return self;
}

- (void)startAllPlugins
{
    if (_started) {
        return;
    }
    
    if ([_views count] > 0) {
        LOG(Plugins, "starting WebKit plugins : %@", [_views description]);
    }
    
    [_views makeObjectsPerformSelector:@selector(pluginStart)];
    _started = YES;
}

- (void)stopAllPlugins
{
    if (!_started) {
        return;
    }

    if ([_views count] > 0) {
        LOG(Plugins, "stopping WebKit plugins: %@", [_views description]);
    }
    
    [_views makeObjectsPerformSelector:@selector(pluginStop)];
    _started = NO;
}

- (void)addPlugin:(NSView <WebPlugin> *)view
{
    if (!_HTMLView) {
        ERROR("can't add a plug-in to a defunct WebPluginController");
        return;
    }
    
    if (![_views containsObject:view]) {
        [_views addObject:view];

        LOG(Plugins, "initializing plug-in %@", view);
        [view pluginInitialize];

        if (_started) {
            LOG(Plugins, "starting plug-in %@", view);
            [view pluginStart];
        }
    }
}

- (void)destroyAllPlugins
{    
    [self stopAllPlugins];

    if ([_views count] > 0) {
        LOG(Plugins, "destroying WebKit plugins: %@", [_views description]);
    }
    
    [_views makeObjectsPerformSelector:@selector(removeFromSuperviewWithoutNeedingDisplay)];
    [_views makeObjectsPerformSelector:@selector(pluginDestroy)];
    [_views release];
    _views = nil;

    _HTMLView = nil;
}

- (void)showURL:(NSURL *)URL inFrame:(NSString *)target
{
    if (!URL) {
        ERROR("nil URL passed");
        return;
    }
    if (!_HTMLView) {
        ERROR("could not load URL %@ because plug-in has already been destroyed", URL);
        return;
    }
    WebFrame *frame = [_HTMLView _frame];
    if (!frame) {
        ERROR("could not load URL %@ because plug-in has already been stopped", URL);
        return;
    }
    NSURLRequest *request = [NSURLRequest requestWithURL:URL];
    if (!request) {
        ERROR("could not load URL %@", URL);
        return;
    }
    if (!target) {
        target = @"_top";
    }
    [frame _loadRequest:request inFrameNamed:target];
}

- (void)showStatus:(NSString *)message
{
    if (!message) {
        message = @"";
    }
    if (!_HTMLView) {
        ERROR("could not show status message (%@) because plug-in has already been destroyed", message);
        return;
    }
    WebView *v = [_HTMLView _webView];
    [[v _UIDelegateForwarder] webView:v setStatusText:message];
}

@end
