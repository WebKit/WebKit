/*	
    WebBaseResourceHandleDelegate.h
    Copyright (c) 2002, Apple Computer, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WebKit/WebViewPrivate.h>

@class NSError;
@class NSURLAuthenticationChallenge;
@class NSURLConnection;
@class NSURLConnectionAuthenticationChallenge;
@class NSURLCredential;
@class NSURLRequest;
@class NSURLResponse;
@class WebDataSource;
@class WebView;

@interface WebBaseResourceHandleDelegate : NSObject
{
@protected
    WebDataSource *dataSource;
    NSURLConnection *connection;
    NSURLRequest *request;
@private
    WebView *webView;
    NSURLResponse *response;
    id identifier;
    id resourceLoadDelegate;
    id downloadDelegate;
    NSURLAuthenticationChallenge *currentConnectionChallenge;
    NSURLAuthenticationChallenge *currentWebChallenge;
    BOOL reachedTerminalState;
    BOOL defersCallbacks;
    WebResourceDelegateImplementationCache implementations;
}

- (BOOL)loadWithRequest:(NSURLRequest *)request;

- (void)setDataSource:(WebDataSource *)d;
- (WebDataSource *)dataSource;

- (id)resourceLoadDelegate;
- (id)downloadDelegate;

- (void)cancel;
- (void)cancelWithError:(NSError *)error;

- (void)setDefersCallbacks:(BOOL)defers;
- (BOOL)defersCallbacks;

- (NSError *)cancelledError;

- (void)setIdentifier:(id)ident;

- (void)releaseResources;
- (NSURLResponse *)response;

@end
