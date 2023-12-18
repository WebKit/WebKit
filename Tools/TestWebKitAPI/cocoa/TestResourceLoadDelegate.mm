/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "TestResourceLoadDelegate.h"

#import <WebKit/_WKResourceLoadInfo.h>

@implementation TestResourceLoadDelegate

- (void)webView:(WKWebView *)webView resourceLoad:(_WKResourceLoadInfo *)resourceLoad didSendRequest:(NSURLRequest *)request
{
    if (_didSendRequest)
        _didSendRequest(webView, resourceLoad, request);
}

- (void)webView:(WKWebView *)webView resourceLoad:(_WKResourceLoadInfo *)resourceLoad didPerformHTTPRedirection:(NSURLResponse *)response newRequest:(NSURLRequest *)request
{
    if (_didPerformHTTPRedirection)
        _didPerformHTTPRedirection(webView, resourceLoad, response, request);
}

- (void)webView:(WKWebView *)webView resourceLoad:(_WKResourceLoadInfo *)resourceLoad didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (_didReceiveChallenge)
        _didReceiveChallenge(webView, resourceLoad, challenge);
}

- (void)webView:(WKWebView *)webView resourceLoad:(_WKResourceLoadInfo *)resourceLoad didReceiveResponse:(NSURLResponse *)response
{
    if (_didReceiveResponse)
        _didReceiveResponse(webView, resourceLoad, response);
}

- (void)webView:(WKWebView *)webView resourceLoad:(_WKResourceLoadInfo *)resourceLoad didCompleteWithError:(NSError *)error response:(NSURLResponse *)response
{
    if (_didCompleteWithError)
        _didCompleteWithError(webView, resourceLoad, error, response);
}

@end
