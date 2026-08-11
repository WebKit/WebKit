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

#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestCocoa.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import <WebKit/WKHistoryDelegatePrivate.h>
#import <WebKit/WKNavigationData.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WebKit.h>
#import <wtf/text/MakeString.h>

@interface HistoryDelegate : NSObject <WKHistoryDelegatePrivate> {
    @public
    RetainPtr<WKNavigationData> lastNavigationData;
    RetainPtr<NSString> lastUpdatedTitle;
    RetainPtr<NSURL> lastRedirectSource;
    RetainPtr<NSURL> lastRedirectDestination;
}
@end

@implementation HistoryDelegate

- (void)_webView:(WKWebView *)webView didNavigateWithNavigationData:(WKNavigationData *)navigationData
{
    lastNavigationData = navigationData;
}

- (void)_webView:(WKWebView *)webView didUpdateHistoryTitle:(NSString *)title forURL:(NSURL *)URL
{
    lastUpdatedTitle = title;
}

- (void)_webView:(WKWebView *)webView didPerformServerRedirectFromURL:(NSURL *)sourceURL toURL:(NSURL *)destinationURL
{
    lastRedirectSource = sourceURL;
    lastRedirectDestination = destinationURL;
}

@end

TEST(HistoryDelegate, NonpersistentDataStoreDoesNotSendHistoryEvents)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { "<head><title>Page Title</title></head>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    RetainPtr websiteDataStore = adoptNS([[WKWebsiteDataStore nonPersistentDataStore] retain]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:websiteDataStore.get()];

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr historyDelegate = adoptNS([[HistoryDelegate alloc] init]);
    [webView _setHistoryDelegate:historyDelegate.get()];

    auto mainURL = makeString("http://localhost:"_s, server.port(), "/"_s);
    RetainPtr request = adoptNS([[NSURLRequest alloc] initWithURL:adoptNS([[NSURL alloc] initWithString:mainURL.createNSString().get()]).get()]);
    [webView loadRequest:request.get()];
    [webView _test_waitForDidFinishNavigation];
    TestWebKitAPI::Util::spinRunLoop();

    EXPECT_NULL(historyDelegate->lastNavigationData.get());
}

TEST(HistoryDelegate, NonpersistentDataStoreSendsHistoryEventsWhenAllowingPrivacySensitiveOperations)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { "<head><title>Page Title</title></head>"_s } },
        { "/server_redirect_source"_s, { 301, { { "Location"_s, "server_redirect_destination"_s } } } },
        { "/server_redirect_destination"_s, { "<head><title>Target Page</title></head>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    RetainPtr websiteDataStore = adoptNS([[WKWebsiteDataStore nonPersistentDataStore] retain]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:websiteDataStore.get()];

    [configuration preferences]._allowPrivacySensitiveOperationsInNonPersistentDataStores = YES;

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr historyDelegate = adoptNS([[HistoryDelegate alloc] init]);
    [webView _setHistoryDelegate:historyDelegate.get()];

    auto mainURL = makeString("http://localhost:"_s, server.port(), "/"_s);
    RetainPtr url = adoptNS([[NSURL alloc] initWithString:mainURL.createNSString().get()]);
    RetainPtr request = adoptNS([[NSURLRequest alloc] initWithURL:url.get()]);
    [webView loadRequest:request.get()];

    TestWebKitAPI::Util::waitFor([&] {
        return historyDelegate->lastNavigationData && historyDelegate->lastUpdatedTitle;
    });

    EXPECT_NS_EQUAL([historyDelegate->lastNavigationData originalRequest], request.get());
    EXPECT_NS_EQUAL([historyDelegate->lastNavigationData originalRequest].URL, url.get());
    EXPECT_NS_EQUAL(historyDelegate->lastUpdatedTitle.get(), @"Page Title");

    auto sourceURL = makeString("http://localhost:"_s, server.port(), "/server_redirect_source"_s);
    auto destinationURL = makeString("http://localhost:"_s, server.port(), "/server_redirect_destination"_s);
    url = adoptNS([[NSURL alloc] initWithString:sourceURL.createNSString().get()]);
    request = adoptNS([[NSURLRequest alloc] initWithURL:url.get()]);
    [webView loadRequest:request.get()];
    [webView _test_waitForDidFinishNavigation];
    TestWebKitAPI::Util::spinRunLoop();

    TestWebKitAPI::Util::waitFor([&] {
        return historyDelegate->lastRedirectSource && historyDelegate->lastRedirectDestination;
    });

    EXPECT_WK_STREQ([historyDelegate->lastRedirectSource absoluteString], sourceURL);
    EXPECT_WK_STREQ([historyDelegate->lastRedirectDestination absoluteString], destinationURL);
}

TEST(HistoryDelegate, PersistentDataStoreSendsHistoryEvents)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { "<head><title>Page Title</title></head>"_s } },
        { "/server_redirect_source"_s, { 301, { { "Location"_s, "server_redirect_destination"_s } } } },
        { "/server_redirect_destination"_s, { "<head><title>Target Page</title></head>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    RetainPtr websiteDataStore = adoptNS([[WKWebsiteDataStore defaultDataStore] retain]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:websiteDataStore.get()];

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr historyDelegate = adoptNS([[HistoryDelegate alloc] init]);
    [webView _setHistoryDelegate:historyDelegate.get()];

    auto mainURL = makeString("http://localhost:"_s, server.port(), "/"_s);
    RetainPtr url = adoptNS([[NSURL alloc] initWithString:mainURL.createNSString().get()]);
    RetainPtr request = adoptNS([[NSURLRequest alloc] initWithURL:url.get()]);
    [webView loadRequest:request.get()];

    TestWebKitAPI::Util::waitFor([&] {
        return historyDelegate->lastNavigationData && historyDelegate->lastUpdatedTitle;
    });

    EXPECT_NS_EQUAL([historyDelegate->lastNavigationData originalRequest], request.get());
    EXPECT_NS_EQUAL([historyDelegate->lastNavigationData originalRequest].URL, url.get());
    EXPECT_NS_EQUAL(historyDelegate->lastUpdatedTitle.get(), @"Page Title");

    auto sourceURL = makeString("http://localhost:"_s, server.port(), "/server_redirect_source"_s);
    auto destinationURL = makeString("http://localhost:"_s, server.port(), "/server_redirect_destination"_s);
    url = adoptNS([[NSURL alloc] initWithString:sourceURL.createNSString().get()]);
    request = adoptNS([[NSURLRequest alloc] initWithURL:url.get()]);
    [webView loadRequest:request.get()];
    [webView _test_waitForDidFinishNavigation];
    TestWebKitAPI::Util::spinRunLoop();

    TestWebKitAPI::Util::waitFor([&] {
        return historyDelegate->lastRedirectSource && historyDelegate->lastRedirectDestination;
    });

    EXPECT_WK_STREQ([historyDelegate->lastRedirectSource absoluteString], sourceURL);
    EXPECT_WK_STREQ([historyDelegate->lastRedirectDestination absoluteString], destinationURL);
}

