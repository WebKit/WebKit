/*	
    WebBaseResourceHandleDelegate.h
    Copyright (c) 2002, Apple Computer, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

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
@private
    WebResourceRequest *request;
    WebResourceResponse *response;
    id identifier;
    id <WebResourceLoadDelegate>resourceLoadDelegate;
    id <WebResourceLoadDelegate>downloadDelegate;
    NSURL *currentURL;
    BOOL isDownload;
    BOOL reachedTerminalState;
    BOOL defersCallbacks;
}

- (BOOL)loadWithRequest:(WebResourceRequest *)request;

- (void)setDataSource:(WebDataSource *)d;
- (WebDataSource *)dataSource;

- (id <WebResourceLoadDelegate>)resourceLoadDelegate;
- (id <WebResourceLoadDelegate>)downloadDelegate;
- (void)setIsDownload:(BOOL)f;

- (void)cancel;
- (void)cancelQuietly;

- (void)setDefersCallbacks:(BOOL)defers;

- (WebError *)cancelledError;

- (void)notifyDelegatesOfInterruptionByPolicyChange;

@end
