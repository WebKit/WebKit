/*	
    WebBaseResourceHandleDelegate.h
    Copyright (c) 2002, Apple Computer, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@class WebController;
@class WebDataSource;
@class WebError;
@class WebResource;
@class WebRequest;
@class WebResponse;

@protocol WebResourceDelegate;

@interface WebBaseResourceHandleDelegate : NSObject <WebResourceDelegate>
{
@protected
    WebDataSource *dataSource;
    WebResource *resource;
    WebRequest *request;
@private
    WebController *controller;
    WebResponse *response;
    id identifier;
    id resourceLoadDelegate;
    id downloadDelegate;
    NSURL *currentURL;
    BOOL reachedTerminalState;
    BOOL defersCallbacks;
}

- (BOOL)loadWithRequest:(WebRequest *)request;

// this method exists only to be subclassed, don't call it directly
- (void)startLoading:(WebRequest *)r;

- (void)setDataSource:(WebDataSource *)d;
- (WebDataSource *)dataSource;

- resourceLoadDelegate;
- downloadDelegate;

- (void)cancel;
- (void)cancelQuietly;
- (void)cancelWithError:(WebError *)error;

- (void)setDefersCallbacks:(BOOL)defers;
- (BOOL)defersCallbacks;

- (WebError *)cancelledError;

- (void)setIdentifier: ident;

@end
