/*
        WebNetscapePluginStream.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WebKit/npapi.h>
#import <WebKit/WebBaseNetscapePluginStream.h>

@class WebBaseNetscapePluginView;
@class WebResourceHandle;
@class WebResourceRequest;
@protocol WebResourceHandleDelegate;

@interface WebNetscapePluginStream : WebBaseNetscapePluginStream <WebResourceHandleDelegate>
{
    WebBaseNetscapePluginView *view;
    
    WebResourceRequest *request;
    WebResourceHandle *resource;
    
    NSMutableData *resourceData;

    NSURL *currentURL;
}

- initWithRequest:(WebResourceRequest *)theRequest
    pluginPointer:(NPP)thePluginPointer
       notifyData:(void *)theNotifyData;

- (void)start;

- (void)stop;

@end
