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

#import <WebKit/WebDefaultResourceLoadDelegate.h>

#import <Foundation/NSURLAuthenticationChallenge.h>
#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURLResponse.h>

#import <WebKit/WebDataSource.h>
#import <WebKit/WebPanelAuthenticationHandler.h>
#import <WebKit/WebView.h>


@implementation WebDefaultResourceLoadDelegate

static WebDefaultResourceLoadDelegate *sharedDelegate = nil;

// Return a object with vanilla implementations of the protocol's methods
// Note this feature relies on our default delegate being stateless
+ (WebDefaultResourceLoadDelegate *)sharedResourceLoadDelegate
{
    if (!sharedDelegate) {
        sharedDelegate = [[WebDefaultResourceLoadDelegate alloc] init];
    }
    return sharedDelegate;
}

- webView: (WebView *)wv identifierForInitialRequest: (NSURLRequest *)request fromDataSource: (WebDataSource *)dataSource
{
    return [[[NSObject alloc] init] autorelease];
}

-(NSURLRequest *)webView: (WebView *)wv resource:identifier willSendRequest: (NSURLRequest *)newRequest redirectResponse:(NSURLResponse *)redirectResponse fromDataSource:(WebDataSource *)dataSource
{
    return newRequest;
}

- (void)webView:(WebView *)wv resource:(id)identifier didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge fromDataSource:(WebDataSource *)dataSource
{
    NSWindow *window = [wv hostWindow] ? [wv hostWindow] : [wv window];
    [[WebPanelAuthenticationHandler sharedHandler] startAuthentication:challenge window:window];
}

- (void)webView:(WebView *)wv resource:(id)identifier didCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge fromDataSource:(WebDataSource *)dataSource
{
    [(WebPanelAuthenticationHandler *)[WebPanelAuthenticationHandler sharedHandler] cancelAuthentication:challenge];
}

-(void)webView: (WebView *)wv resource:identifier didReceiveResponse: (NSURLResponse *)response fromDataSource:(WebDataSource *)dataSource
{
}

-(void)webView: (WebView *)wv resource:identifier didReceiveContentLength: (unsigned)length fromDataSource:(WebDataSource *)dataSource
{
}

-(void)webView: (WebView *)wv resource:identifier didFinishLoadingFromDataSource:(WebDataSource *)dataSource
{
}

-(void)webView: (WebView *)wv resource:identifier didFailLoadingWithError:(NSError *)error fromDataSource:(WebDataSource *)dataSource
{
}

- (void)webView: (WebView *)wv plugInFailedWithError:(NSError *)error dataSource:(WebDataSource *)dataSource
{
}

@end


