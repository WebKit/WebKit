/*	
    WebBaseResourceHandleDelegate.h
    Copyright (c) 2002, Apple Computer, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@class WebController;
@class WebDataSource;
@class WebError;
@class WebResourceHandle;
@class WebResourceRequest;
@class WebResourceResponse;

@protocol WebResourceHandleDelegate;
@protocol WebResourceLoadDelegate;

@interface WebBaseResourceHandleDelegate : NSObject <WebResourceHandleDelegate>
{
@protected
    WebDataSource *dataSource;
    WebResourceHandle *handle;
    WebResourceRequest *request;
@private
    WebController *controller;
    WebResourceResponse *response;
    id identifier;
    id <WebResourceLoadDelegate>resourceLoadDelegate;
    id <WebResourceLoadDelegate>downloadDelegate;
    NSURL *currentURL;
    BOOL reachedTerminalState;
    BOOL defersCallbacks;
}

- (BOOL)loadWithRequest:(WebResourceRequest *)request;

// this method exists only to be subclassed, don't call it directly
- (void)startLoading:(WebResourceRequest *)r;

- (void)setDataSource:(WebDataSource *)d;
- (WebDataSource *)dataSource;

- (id <WebResourceLoadDelegate>)resourceLoadDelegate;
- (id <WebResourceLoadDelegate>)downloadDelegate;
- (BOOL)isDownload;

- (void)cancel;
- (void)cancelQuietly;
- (void)cancelWithError:(WebError *)error;

- (void)setDefersCallbacks:(BOOL)defers;
- (BOOL)defersCallbacks;

- (WebError *)cancelledError;

- (void)notifyDelegatesOfInterruptionByPolicyChange;

@end
