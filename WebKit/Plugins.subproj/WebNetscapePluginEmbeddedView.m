//
//  WebNetscapePluginEmbeddedView.m
//  WebKit
//
//  Created by Administrator on Mon Sep 30 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <WebKit/WebBaseNetscapePluginViewPrivate.h>
#import <WebKit/WebController.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebNetscapePluginEmbeddedView.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPlugin.h>
#import <WebKit/WebView.h>

#import <WebFoundation/WebFoundation.h>

@implementation WebNetscapePluginEmbeddedView

- (id)initWithFrame:(NSRect)frame
             plugin:(WebNetscapePlugin *)thePlugin
                URL:(NSURL *)theURL
            baseURL:(NSURL *)theBaseURL
               mime:(NSString *)mimeType
          arguments:(NSDictionary *)arguments
{
    [super initWithFrame:frame];

    URL = [theURL retain];
    
    [self setMIMEType:mimeType];
    [self setBaseURL:theBaseURL];
    [self setArguments:arguments];
    [self setMode:NP_EMBED];
    
    // load the plug-in if it is not already loaded
    if (![thePlugin load]){
        return nil;
    }

    [self setPlugin:thePlugin];

    return self;
}

- (void)dealloc
{
    [URL release];
    [super dealloc];
}

- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];
    
    if ([self window]){
        [self start];
    }
}

- (void)start
{
    [super start];

    if(URL){
        WebResourceRequest *request = [WebResourceRequest requestWithURL:URL];
        [self loadRequest:request inTarget:nil withNotifyData:nil];
    }
}

- (WebDataSource *)dataSource
{
    WebView *webView = (WebView *)[self _web_superviewOfClass:[WebView class]];
    WebFrame *webFrame = [[webView controller] frameForView:webView];

    return [webFrame dataSource];
}

@end
