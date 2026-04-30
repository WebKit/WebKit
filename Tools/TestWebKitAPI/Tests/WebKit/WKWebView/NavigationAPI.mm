/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(NavigationAPI, PushStateUpdatesCurrentEntryKeyWithoutAllowPrivacySensitiveOperationsInNonPersistentDataStores)
{
    HTTPServer server({
        { "/example"_s, { "example"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    // This will also set a non-persistent data store on the configuration.
    RetainPtr configuration = server.httpsProxyConfiguration();
    EXPECT_FALSE([configuration.get().websiteDataStore isPersistent]);
    [configuration preferences]._allowPrivacySensitiveOperationsInNonPersistentDataStores = NO;

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 300, 300) configuration:configuration.get()]);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    NSString *keyBefore = [webView stringByEvaluatingJavaScript:@"navigation.currentEntry.key"];
    [webView stringByEvaluatingJavaScript:@"history.pushState(null, '', '/foo')"];
    NSString *keyAfter = [webView stringByEvaluatingJavaScript:@"navigation.currentEntry.key"];

    EXPECT_FALSE([keyBefore isEqualToString:keyAfter]);
}

TEST(NavigationAPI, ReplaceStateUpdatesCurrentEntryIDWithoutAllowPrivacySensitiveOperationsInNonPersistentDataStores)
{
    HTTPServer server({
        { "/example"_s, { "example"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    // This will also set a non-persistent data store on the configuration.
    RetainPtr configuration = server.httpsProxyConfiguration();
    EXPECT_FALSE([configuration.get().websiteDataStore isPersistent]);
    [configuration preferences]._allowPrivacySensitiveOperationsInNonPersistentDataStores = NO;

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 300, 300) configuration:configuration.get()]);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    NSString *idBefore = [webView stringByEvaluatingJavaScript:@"navigation.currentEntry.id"];
    [webView stringByEvaluatingJavaScript:@"history.replaceState(null, '', '/foo')"];
    NSString *idAfter = [webView stringByEvaluatingJavaScript:@"navigation.currentEntry.id"];

    EXPECT_FALSE([idBefore isEqualToString:idAfter]);
}

TEST(NavigationAPI, ReplaceStateAfterBackPreservesKey)
{
    HTTPServer server({
        { "/example"_s, { "example"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 300, 300) configuration:configuration.get()]);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView stringByEvaluatingJavaScript:@"history.pushState({}, '', '/page-a')"];
    NSString *keyPageA = [webView stringByEvaluatingJavaScript:@"navigation.currentEntry.key"];

    [webView stringByEvaluatingJavaScript:@"history.pushState({}, '', '/page-b')"];
    NSString *keyPageB = [webView stringByEvaluatingJavaScript:@"navigation.currentEntry.key"];
    EXPECT_FALSE([keyPageA isEqualToString:keyPageB]);

    [webView _evaluateJavaScriptWithoutUserGesture:@"history.back()" completionHandler:nil];
    [navigationDelegate waitForDidSameDocumentNavigation];

    NSString *keyAfterBack = [webView stringByEvaluatingJavaScript:@"navigation.currentEntry.key"];
    EXPECT_TRUE([keyPageA isEqualToString:keyAfterBack]);

    [webView stringByEvaluatingJavaScript:@"history.replaceState({ updated: true }, '')"];
    NSString *keyAfterReplace = [webView stringByEvaluatingJavaScript:@"navigation.currentEntry.key"];
    EXPECT_TRUE([keyPageA isEqualToString:keyAfterReplace]);
}

TEST(NavigationAPI, InterceptFailsForDifferentSubdomain)
{
    HTTPServer server({
        { "/page1"_s, { "page1"_s } },
        { "/page2"_s, { "page2"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 300, 300) configuration:configuration.get()]);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://page1.example.com/page1"]]];
    [navigationDelegate waitForDidFinishNavigation];

    NSError *error = nil;
    id result = [webView objectByCallingAsyncFunction:@"return await new Promise((resolve) => {"
        "    navigation.addEventListener('navigate', (event) => {"
        "       try {"
        "           event.intercept({ handler: () => {} });"
        "           resolve('intercept succeeded');"
        "       } catch (e) {"
        "           resolve('intercept failed: ' + e.name);"
        "       }"
        "    });"
        "    location.href = 'https://page2.example.com/page2';"
        "});" withArguments:nil error:&error];

    EXPECT_NULL(error);
    EXPECT_TRUE([result isKindOfClass:[NSString class]]);
    EXPECT_TRUE([result hasPrefix:@"intercept failed"]);
}

TEST(NavigationAPI, InterceptFailsForDifferentUsernameAndPassword)
{
    HTTPServer server({
        { "/page1"_s, { "page1"_s } },
        { "/page2"_s, { "page2"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 300, 300) configuration:configuration.get()]);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://username1:password1@example.com/page1"]]];
    [navigationDelegate waitForDidFinishNavigation];

    NSError *error = nil;
    id result = [webView objectByCallingAsyncFunction:@"return await new Promise((resolve) => {"
        "    navigation.addEventListener('navigate', (event) => {"
        "       try {"
        "           event.intercept({ handler: () => {} });"
        "           resolve('intercept succeeded');"
        "       } catch (e) {"
        "           resolve('intercept failed: ' + e.name);"
        "       }"
        "    });"
        "    location.href = 'https://username2:password2@example.com/page2';"
        "});" withArguments:nil error:&error];

    EXPECT_NULL(error);
    EXPECT_TRUE([result isKindOfClass:[NSString class]]);
    EXPECT_TRUE([result hasPrefix:@"intercept failed"]);
}

} // namespace TestWebKitAPI
