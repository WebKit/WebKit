/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(COOKIE_STORE_API_BY_DEFAULT)

#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/Utilities.h"
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(WebKit, CookieStoreSetCookieForHigherLevelDomain)
{
    HTTPServer server({
        { "/foo"_s, { "foo"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 300, 300) configuration:configuration.get()]);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://subdomain.example.com/foo"]]];
    [navigationDelegate waitForDidFinishNavigation];

    NSError *error;
    [webView objectByCallingAsyncFunction:@"await cookieStore.set({ name: 'cookieName', value: 'cookieValue', domain: 'example.com' });" withArguments:nil error:&error];
    EXPECT_NULL(error);

    id result = [webView objectByCallingAsyncFunction:@"return (await cookieStore.get('cookieName')).value;" withArguments:nil error:&error];
    EXPECT_NULL(error);
    EXPECT_TRUE([result isKindOfClass:[NSString class]]);
    EXPECT_TRUE([result isEqualToString:@"cookieValue"]);
}

TEST(WebKit, CookieStoreSetCookieForPublicSuffixDomain)
{
    HTTPServer server({
        { "/foo"_s, { "foo"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 300, 300) configuration:configuration.get()]);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:navigationDelegate.get()];

    // Test for "com".
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/foo"]]];
    [navigationDelegate waitForDidFinishNavigation];

    NSError *error;
    [webView objectByCallingAsyncFunction:@"await cookieStore.set({ name: 'cookieName', value: 'cookieValue', domain: 'com' });" withArguments:nil error:&error];
    EXPECT_NOT_NULL(error);
    EXPECT_TRUE([[error description] containsString:@"The domain must be a registrable domain suffix of or be equal to the current host"]);
    error = NULL;

    // Test for "co.uk".
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.co.uk/foo"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView objectByCallingAsyncFunction:@"await cookieStore.set({ name: 'cookieName', value: 'cookieValue', domain: 'co.uk' });" withArguments:nil error:&error];
    EXPECT_NOT_NULL(error);
    EXPECT_TRUE([[error description] containsString:@"The domain must be a registrable domain suffix of or be equal to the current host"]);
}

} // namespace TestWebKitAPI

#endif // if ENABLE(COOKIE_STORE_API_BY_DEFAULT)
