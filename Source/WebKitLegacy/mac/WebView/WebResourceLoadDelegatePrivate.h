/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#import <TargetConditionals.h>

@class WebView;
@class WebDataSource;

@interface NSObject (WebResourceLoadDelegatePrivate)

- (void)webView:(WebView *)webView didLoadResourceFromMemoryCache:(NSURLRequest *)request response:(NSURLResponse *)response length:(NSInteger)length fromDataSource:(WebDataSource *)dataSource;
- (BOOL)webView:(WebView *)webView resource:(id)identifier shouldUseCredentialStorageForDataSource:(WebDataSource *)dataSource;

/*!
 @method webView:resource:canAuthenticateAgainstProtectionSpace:forDataSource:
 @abstract Inspect an NSURLProtectionSpace before an authentication attempt is made. Only used on Snow Leopard or newer.
 @param protectionSpace an NSURLProtectionSpace that will be used to generate an authentication challenge
 @result Return YES if the resource load delegate is prepared to respond to an authentication challenge generated with protectionSpace, NO otherwise
 */
- (BOOL)webView:(WebView *)sender resource:(id)identifier canAuthenticateAgainstProtectionSpace:(NSURLProtectionSpace *)protectionSpace forDataSource:(WebDataSource *)dataSource WEBKIT_AVAILABLE_MAC(10_6);

/*!
 @method webView:shouldPaintBrokenImageForURL:(NSURL*)imageURL
 @abstract This message is sent when an image cannot be decoded or displayed.
 @param imageURL The url of the broken image.
 @result return YES if WebKit should paint the default broken image.
 */
- (BOOL)webView:(WebView*)sender shouldPaintBrokenImageForURL:(NSURL*)imageURL;

#if TARGET_OS_IPHONE
- (id)webThreadWebView:(WebView *)sender identifierForInitialRequest:(NSURLRequest *)request fromDataSource:(WebDataSource *)dataSource;
- (NSURLRequest *)webThreadWebView:(WebView *)sender resource:(id)identifier willSendRequest:(NSURLRequest *)request redirectResponse:(NSURLResponse *)redirectResponse fromDataSource:(WebDataSource *)dataSource;
- (void)webThreadWebView:(WebView *)sender resource:(id)identifier didReceiveContentLength:(NSInteger)length fromDataSource:(WebDataSource *)dataSource;
- (void)webThreadWebView:(WebView *)sender resource:(id)identifier didReceiveResponse:(NSURLResponse *)response fromDataSource:(WebDataSource *)dataSource;
- (void)webThreadWebView:(WebView *)webView didLoadResourceFromMemoryCache:(NSURLRequest *)request response:(NSURLResponse *)response length:(NSInteger)length fromDataSource:(WebDataSource *)dataSource;
- (void)webThreadWebView:(WebView *)sender resource:(id)identifier didFinishLoadingFromDataSource:(WebDataSource *)dataSource;
- (void)webThreadWebView:(WebView *)sender resource:(id)identifier didFailLoadingWithError:(NSError *)error fromDataSource:(WebDataSource *)dataSource;
- (NSCachedURLResponse *)webThreadWebView:(WebView *)sender resource:(id)identifier willCacheResponse:(NSCachedURLResponse *)response fromDataSource:(WebDataSource *)dataSource;

/*!
 @method webView:connectionPropertiesForResource:
 @abstract Provide a CFStream level properties dictionary to be used by a connection
 @result return an NSDictionary with the desired stream properties or nil
 */
- (NSDictionary *)webView:(WebView *)sender connectionPropertiesForResource:(id)identifier dataSource:(WebDataSource *)dataSource;
#endif

@end
