//
//  WebPluginController.m
//  WebKit
//
//  Created by Chris Blumenberg on Wed Oct 23 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebPluginController.h>

#import <WebKit/WebController.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebPlugin.h>
#import <WebKit/WebPluginContainer.h>
#import <WebKit/WebView.h>
#import <WebKit/WebWindowOperationsDelegate.h>

#import <WebFoundation/WebResourceRequest.h>

@implementation WebPluginController

- initWithDataSource:(WebDataSource *)dataSource
{
    [super init];
    _dataSource = dataSource;
    _views = [[NSMutableArray alloc] init];
    return self;
}

- (void)startAllPlugins
{
    if (_started) {
        return;
    }
    
    LOG(Plugins, "starting all plugins");

    [_views makeObjectsPerformSelector:@selector(pluginStart)];
    _started = YES;
}

- (void)stopAllPlugins
{
    if (!_started) {
        return;
    }
    
    LOG(Plugins, "stopping all plugins");

    [_views makeObjectsPerformSelector:@selector(pluginStop)];
    _started = NO;
}

- (void)addPlugin:(NSView <WebPlugin> *)view
{
    if (!_dataSource) {
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

- (void)dataSourceWillBeDeallocated
{
    LOG(Plugins, "destroying all plug-ins");
    
    [self stopAllPlugins];
    [_views makeObjectsPerformSelector:@selector(removeFromSuperviewWithoutNeedingDisplay)];
    [_views makeObjectsPerformSelector:@selector(pluginDestroy)];
    [_views release];
    _views = nil;

    _dataSource = nil;
}

- (void)showURL:(NSURL *)URL inFrame:(NSString *)target
{
    if (!URL) {
        ERROR("nil URL passed");
        return;
    }
    if (!_dataSource) {
        ERROR("could not load URL %@ because plug-in has already been destroyed", URL);
        return;
    }
    WebFrame *frame = [_dataSource webFrame];
    if (!frame) {
        ERROR("could not load URL %@ because plug-in has already been stopped", URL);
        return;
    }
    WebResourceRequest *request = [WebResourceRequest requestWithURL:URL];
    if (!request) {
        ERROR("could not load URL %@", URL);
        return;
    }
    [[frame findOrCreateFramedNamed:target] loadRequest:request];
}

- (void)showStatus:(NSString *)message
{
    if (!message) {
        message = @"";
    }
    if (!_dataSource) {
        ERROR("could not show status message (%@) because plug-in has already been destroyed", message);
        return;
    }
    [[[_dataSource controller] windowOperationsDelegate] setStatusText:message];
}

@end
