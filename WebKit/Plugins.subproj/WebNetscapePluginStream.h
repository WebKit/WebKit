/*
        WebNetscapePluginStream.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WebKit/npapi.h>
#import <WebKit/WebBaseNetscapePluginStream.h>

@class WebNetscapePluginEmbeddedView;
@class WebResourceHandle;
@class WebResourceRequest;


@interface WebNetscapePluginStream : WebBaseNetscapePluginStream 
{
    WebNetscapePluginEmbeddedView *view;
    NSMutableData *resourceData;
}

- initWithRequest:(WebResourceRequest *)theRequest
    pluginPointer:(NPP)thePluginPointer
       notifyData:(void *)theNotifyData;

- (void)start;

- (void)stop;

@end
