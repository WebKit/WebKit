/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

@interface WebLoader : NSObject
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
@interface NSObject (WebLoaderExtras)
- (void)connection:(NSURLConnection *)con didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived;
@end
