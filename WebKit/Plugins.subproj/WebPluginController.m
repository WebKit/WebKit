//
//  WebPluginController.m
//  WebKit
//
//  Created by Chris Blumenberg on Wed Oct 23 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebController.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebPlugin.h>
#import <WebKit/WebPluginController.h>
#import <WebKit/WebWindowOperationsDelegate.h>
#import <WebKit/WebView.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebResourceRequest.h>

@implementation WebPluginController

- initWithWebFrame:(WebFrame *)theFrame
{
    [super init];
    
    // Not retained because the frame retains this plug-in controller.
    frame = theFrame;
    
    views = [[NSMutableArray array] retain];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(windowWillClose:)
                                                 name:NSWindowWillCloseNotification
                                               object:nil];
    
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    
    ASSERT([views count] == 0);
    [views release];
    [super dealloc];
}

- (void)addPluginView:(NSView <WebPlugin> *)view
{
    LOG(Plugins, "pluginInitialize: %s", [[view className] lossyCString]);
    
    [views addObject:view];
    [view pluginInitialize];
}

- (void)didAddPluginView:(NSView <WebPlugin> *)view
{
    LOG(Plugins, "pluginStart: %s", [[view className] lossyCString]);
    
    [view pluginStart];
}

- (void)startAllPlugins
{
    LOG(Plugins, "pluginStart");
    
    [views makeObjectsPerformSelector:@selector(pluginStart)];
}

- (void)stopAllPlugins
{
    LOG(Plugins, "pluginStop");
    
    [views makeObjectsPerformSelector:@selector(pluginStop)];
}

- (void)destroyAllPlugins
{
    LOG(Plugins, "pluginDestroy");
    
    [self stopAllPlugins];
    [views makeObjectsPerformSelector:@selector(removeFromSuperviewWithoutNeedingDisplay)];
    [views makeObjectsPerformSelector:@selector(pluginDestroy)];
    [views removeAllObjects];

    // after this point, do not try to do anything with the frame, even if we get some
    // late arriving messages from the plugin
    frame = nil;
}

- (void)windowWillClose:(NSNotification *)notification
{
    if([notification object] == [[frame webView] window]){
        [self destroyAllPlugins];
    }
}

- (void)showURL:(NSURL *)URL inFrame:(NSString *)target
{
    if ( !URL || !frame ){
        return;
    }

    WebFrame *otherFrame = [frame findOrCreateFramedNamed:target];

    [otherFrame loadRequest:[WebResourceRequest requestWithURL:URL]];
}

- (void)showStatus:(NSString *)message
{
    if(!message || !frame){
        return;
    }

    [[[frame controller] windowOperationsDelegate] setStatusText:message];
}

@end
