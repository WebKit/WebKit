/*	
    WebBaseResourceHandleDelegate.h
    Copyright (c) 2002, Apple Computer, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@class WebView;
@class WebDataSource;
@class WebError;
@class WebResource;
@class NSURLRequest;
@class NSURLResponse;

@protocol WebResourceDelegate;

@interface WebBaseResourceHandleDelegate : NSObject <WebResourceDelegate>
{
@protected
    WebDataSource *dataSource;
    WebResource *resource;
    NSURLRequest *request;
@private
    WebView *controller;
    NSURLResponse *response;
    id identifier;
    id resourceLoadDelegate;
    id downloadDelegate;
    NSURL *currentURL;
    BOOL reachedTerminalState;
    BOOL defersCallbacks;
}

- (BOOL)loadWithRequest:(NSURLRequest *)request;

// this method exists only to be subclassed, don't call it directly
- (void)startLoading:(NSURLRequest *)r;

- (void)setDataSource:(WebDataSource *)d;
- (WebDataSource *)dataSource;

- resourceLoadDelegate;
- downloadDelegate;

- (void)cancel;
- (void)cancelWithError:(WebError *)error;

- (void)setDefersCallbacks:(BOOL)defers;
- (BOOL)defersCallbacks;

- (WebError *)cancelledError;

- (void)setIdentifier: ident;

@end
