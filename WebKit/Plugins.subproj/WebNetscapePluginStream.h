/*
        WebNetscapePluginStream.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WebKit/npapi.h>
#import <WebKit/WebBaseNetscapePluginStream.h>

@class NSURLRequest;
@class NSURLConnection;
@class WebNetscapePluginConnectionDelegate;


@interface WebNetscapePluginStream : WebBaseNetscapePluginStream 
{    
    WebNetscapePluginConnectionDelegate *_loader;
    NSURLRequest *request;
}

- (id)initWithRequest:(NSURLRequest *)theRequest
        pluginPointer:(NPP)thePluginPointer
           notifyData:(void *)theNotifyData
     sendNotification:(BOOL)sendNotification;
- (void)start;
- (void)stop;

@end
