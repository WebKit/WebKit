/*	
    WebBaseResourceHandleDelegate.h
    Copyright (c) 2002, Apple Computer, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@class WebDataSource;
@class WebResourceHandle;
@class WebResourceRequest;
@class WebResourceResponse;

@protocol WebResourceHandleDelegate;
@protocol WebResourceLoadDelegate;

@interface WebBaseResourceHandleDelegate : NSObject <WebResourceHandleDelegate>
{
    WebResourceHandle *handle;
    WebDataSource *dataSource;
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

- (void)loadWithRequest:(WebResourceRequest *)request;

- (void)setDataSource:(WebDataSource *)d;
- (WebDataSource *)dataSource;

- (id <WebResourceLoadDelegate>)resourceLoadDelegate;
- (id <WebResourceLoadDelegate>)downloadDelegate;
- (void)setIsDownload:(BOOL)f;
- (void)cancel;

- (void)setDefersCallbacks:(BOOL)defers;

@end
