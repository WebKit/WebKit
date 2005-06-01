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
@class WebResource;
@class WebView;

@interface WebBaseResourceHandleDelegate : NSObject
{
@protected
    WebDataSource *dataSource;
    NSURLConnection *connection;
    NSURLRequest *request;
    BOOL reachedTerminalState;
@private
    WebView *webView;
    NSURLResponse *response;
    id identifier;
    id resourceLoadDelegate;
    id downloadDelegate;
    NSURLAuthenticationChallenge *currentConnectionChallenge;
    NSURLAuthenticationChallenge *currentWebChallenge;
    BOOL cancelledFlag;
    BOOL defersCallbacks;
    BOOL waitingToDeliverResource;
    BOOL deliveredResource;
    WebResourceDelegateImplementationCache implementations;
    NSURL *originalURL;
    NSMutableData *resourceData;
    WebResource *resource;
#ifndef NDEBUG
    BOOL isInitializingConnection;
#endif
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

- (void)addData:(NSData *)data;
- (NSData *)resourceData;

// Connection-less callbacks allow us to send callbacks using data attained from a WebResource instead of an NSURLConnection.
- (NSURLRequest *)willSendRequest:(NSURLRequest *)newRequest redirectResponse:(NSURLResponse *)redirectResponse;
- (void)didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge;
- (void)didCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge;
- (void)didReceiveResponse:(NSURLResponse *)r;
- (void)didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived;
- (void)willStopBufferingData:(NSData *)data;
- (void)didFinishLoading;
- (void)didFailWithError:(NSError *)error;
- (NSCachedURLResponse *)willCacheResponse:(NSCachedURLResponse *)cachedResponse;

// Used to work around the fact that you don't get any more NSURLConnection callbacks until you return from the first one.
+ (BOOL)inConnectionCallback;

@end

// Note: This interface can be removed once this method is declared
// in Foundation (probably will be in Foundation-485).
@interface NSObject (WebBaseResourceHandleDelegateExtras)
- (void)connection:(NSURLConnection *)con didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived;
@end
