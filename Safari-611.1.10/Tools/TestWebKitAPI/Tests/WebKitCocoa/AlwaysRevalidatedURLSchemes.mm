/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import <WebKit/WKBrowsingContextController.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/RetainPtr.h>

static NSString * const customScheme = @"custom";
static size_t loadsStarted;

@interface AlwaysRevalidatedURLSchemeProtocol : NSURLProtocol
@end

@implementation AlwaysRevalidatedURLSchemeProtocol

+ (BOOL)canInitWithRequest:(NSURLRequest *)request
{
    return [request.URL.scheme isEqualToString:customScheme];
}

+ (NSURLRequest *)canonicalRequestForRequest:(NSURLRequest *)request
{
    return request;
}

+ (BOOL)requestIsCacheEquivalent:(NSURLRequest *)a toRequest:(NSURLRequest *)b
{
    return NO;
}

- (void)startLoading
{
    ++loadsStarted;

    auto data = adoptNS([[NSData alloc] initWithBase64EncodedString:@"iVBORw0KGgoAAAANSUhEUgAAAAoAAAAKCAYAAACNMs+9AAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAAFZJREFUeF59z4EJADEIQ1F36k7u5E7ZKXeUQPACJ3wK7UNokVxVk9kHnQH7bY9hbDyDhNXgjpRLqFlo4M2GgfyJHhjq8V4agfrgPQX3JtJQGbofmCHgA/nAKks+JAjFAAAAAElFTkSuQmCC" options:0]);
    auto response = adoptNS([[NSURLResponse alloc] initWithURL:self.request.URL MIMEType:@"image/png" expectedContentLength:[data length] textEncodingName:nil]);
    [self.client URLProtocol:self didReceiveResponse:response.get() cacheStoragePolicy:NSURLCacheStorageNotAllowed];
    [self.client URLProtocol:self didLoadData:data.get()];
    [self.client URLProtocolDidFinishLoading:self];
}

- (void)stopLoading
{
}

@end

TEST(WebKit, AlwaysRevalidatedURLSchemes)
{
    @autoreleasepool {
        [NSURLProtocol registerClass:[AlwaysRevalidatedURLSchemeProtocol class]];
        [WKBrowsingContextController registerSchemeForCustomProtocol:customScheme];

        auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
        [processPoolConfiguration setAlwaysRevalidatedURLSchemes:@[customScheme]];

        auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
        auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [webViewConfiguration setProcessPool:processPool.get()];

        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:webViewConfiguration.get()]);

        NSString *htmlString = [NSString stringWithFormat:@"<!DOCTYPE html><body><img src='%@://image'>", customScheme];
        [webView loadHTMLString:htmlString baseURL:nil];
        [webView _test_waitForDidFinishNavigation];
        EXPECT_EQ(1UL, loadsStarted);

        [webView loadHTMLString:htmlString baseURL:nil];
        [webView _test_waitForDidFinishNavigation];
        EXPECT_EQ(2UL, loadsStarted);

        [WKBrowsingContextController unregisterSchemeForCustomProtocol:customScheme];
        [NSURLProtocol unregisterClass:[AlwaysRevalidatedURLSchemeProtocol class]];
    }
}
