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

#if HAVE(WEBCONTENTRESTRICTIONS)

#import "HTTPServer.h"
#import "Helpers/cocoa/TestUIDelegate.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKErrorRef.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>

using namespace TestWebKitAPI;

TEST(ParentalControlsContentFilteringTests, BlockedURL)
{
    HTTPServer server({
        { "/blockedSite"_s, { "<p>This site should be blocked</p>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    NSData *shieldHTML = [@"<script>"
    "window.webkit.messageHandlers.testHandler.postMessage('SHIELD_IFRAME_READY', '*');"
    "</script>" dataUsingEncoding:NSUTF8StringEncoding];

    auto configuration = server.httpsProxyConfiguration();
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
    auto blockedURL = [NSURL URLWithString:@"https://example.com/blockedSite"];
    __block bool mockInstalled = false;
    [[webView configuration].websiteDataStore _installMockParentalControlsURLFilterForTestingWithBlockedURLs:@[blockedURL] replacementData:shieldHTML completionHandler:^{
        mockInstalled = true;
    }];
    Util::run(&mockInstalled);

    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate allowAnyTLSCertificate];
    __block bool didFail = false;

    [navigationDelegate setDidFailProvisionalNavigation:^(WKWebView *, WKNavigation *, NSError *error) {
        EXPECT_WK_STREQ(WebKitErrorDomain, error.domain);
        EXPECT_EQ(kWKErrorCodeFrameLoadBlockedByContentFilter, error.code);
        didFail = true;
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];

    __block bool iframeShieldDidLoad = false;
    [webView performAfterReceivingAnyMessage:^(NSString *message) {
        if ([message isEqualToString:@"SHIELD_IFRAME_READY"])
            iframeShieldDidLoad = true;
    }];

    [webView loadRequest:[NSURLRequest requestWithURL:blockedURL]];

    Util::run(&didFail);
    Util::run(&iframeShieldDidLoad);
}


TEST(ParentalControlsContentFilteringTests, BlockedURLAfterRedirect)
{
    HTTPServer server({
        { "/start"_s, { 302, {{ "Location"_s, "/final"_s }}, "redirecting..."_s } },
        { "/final"_s, { "Done."_s } }
    });

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    auto blockedURL = [NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/start", server.port()]];
    __block bool mockInstalled = false;
    [[webView configuration].websiteDataStore _installMockParentalControlsURLFilterForTestingWithBlockedURLs:@[blockedURL] completionHandler:^{
        mockInstalled = true;
    }];
    Util::run(&mockInstalled);

    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    __block bool didFail = false;
    [navigationDelegate setDidFailProvisionalNavigation:^(WKWebView *, WKNavigation *, NSError *error) {
        EXPECT_WK_STREQ(WebKitErrorDomain, error.domain);
        EXPECT_EQ(kWKErrorCodeFrameLoadBlockedByContentFilter, error.code);
        didFail = true;
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:server.request("/start"_s)];

    Util::run(&didFail);
}

TEST(ParentalControlsContentFilteringTests, BlockedURLAfterMultipleRedirections_BlockStart)
{
    HTTPServer server({
        { "/start"_s, { 302, {{ "Location"_s, "/middle"_s }}, "redirecting..."_s } },
        { "/middle"_s, { 302, {{ "Location"_s, "/final"_s }}, "redirecting..."_s } },
        { "/final"_s, { "Done."_s } }
    });

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    auto blockedURL = [NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/start", server.port()]];
    __block bool mockInstalled = false;
    [[webView configuration].websiteDataStore _installMockParentalControlsURLFilterForTestingWithBlockedURLs:@[blockedURL] completionHandler:^{
        mockInstalled = true;
    }];
    Util::run(&mockInstalled);

    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    __block bool didFail = false;
    [navigationDelegate setDidFailProvisionalNavigation:^(WKWebView *, WKNavigation *, NSError *error) {
        EXPECT_WK_STREQ(WebKitErrorDomain, error.domain);
        EXPECT_EQ(kWKErrorCodeFrameLoadBlockedByContentFilter, error.code);
        didFail = true;
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:server.request("/start"_s)];

    Util::run(&didFail);
}

TEST(ParentalControlsContentFilteringTests, BlockedURLAfterMultipleRedirections_BlockMiddle)
{
    HTTPServer server({
        { "/start"_s, { 302, {{ "Location"_s, "/middle"_s }}, "redirecting..."_s } },
        { "/middle"_s, { 302, {{ "Location"_s, "/final"_s }}, "redirecting..."_s } },
        { "/final"_s, { "Done."_s } }
    });

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    auto blockedURL = [NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/middle", server.port()]];
    __block bool mockInstalled = false;
    [[webView configuration].websiteDataStore _installMockParentalControlsURLFilterForTestingWithBlockedURLs:@[blockedURL] completionHandler:^{
        mockInstalled = true;
    }];
    Util::run(&mockInstalled);

    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    __block bool didFail = false;
    [navigationDelegate setDidFailProvisionalNavigation:^(WKWebView *, WKNavigation *, NSError *error) {
        EXPECT_WK_STREQ(WebKitErrorDomain, error.domain);
        EXPECT_EQ(kWKErrorCodeFrameLoadBlockedByContentFilter, error.code);
        didFail = true;
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:server.request("/start"_s)];

    Util::run(&didFail);
}

TEST(ParentalControlsContentFilteringTests, BlockedURLAfterMultipleRedirections_BlockFinal)
{
    HTTPServer server({
        { "/start"_s, { 302, {{ "Location"_s, "/middle"_s }}, "redirecting..."_s } },
        { "/middle"_s, { 302, {{ "Location"_s, "/final"_s }}, "redirecting..."_s } },
        { "/final"_s, { "Done."_s } }
    });

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    auto blockedURL = [NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/final", server.port()]];
    __block bool mockInstalled = false;
    [[webView configuration].websiteDataStore _installMockParentalControlsURLFilterForTestingWithBlockedURLs:@[blockedURL] completionHandler:^{
        mockInstalled = true;
    }];
    Util::run(&mockInstalled);

    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    __block bool didFail = false;
    [navigationDelegate setDidFailProvisionalNavigation:^(WKWebView *, WKNavigation *, NSError *error) {
        EXPECT_WK_STREQ(WebKitErrorDomain, error.domain);
        EXPECT_EQ(kWKErrorCodeFrameLoadBlockedByContentFilter, error.code);
        didFail = true;
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:server.request("/start"_s)];

    Util::run(&didFail);
}

TEST(ParentalControlsContentFilteringTests, BlockedIframe)
{
    auto mainframeHTML = "<script>onload = () => {"
    "var iframe = document.createElement('iframe');"
    "document.body.appendChild(iframe);"
    "iframe.src = 'http://localhost:' + window.location.port + '/blockedIframe'"
    "}</script>"_s;

    NSData *blockedShieldHTML =[@"<script>"
    "window.webkit.messageHandlers.testHandler.postMessage('SHIELD_IFRAME_READY', '*');"
    "</script>" dataUsingEncoding:NSUTF8StringEncoding];

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/blockedIframe"_s, { "<script>window.webkit.messageHandlers.testHandler.postMessage('BLOCKED_IFRAME_LOADED');</script>"_s } }
    });

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto blockedIframeURL = [NSURL URLWithString:[NSString stringWithFormat:@"http://localhost:%d/blockedIframe", server.port()]];
    __block bool mockInstalled = false;
    [[webView configuration].websiteDataStore _installMockParentalControlsURLFilterForTestingWithBlockedURLs:@[blockedIframeURL] replacementData:blockedShieldHTML completionHandler:^{
        mockInstalled = true;
    }];
    Util::run(&mockInstalled);

    __block bool iframeShieldDidLoad = false;
    __block bool blockedIframeLoaded = false;

    [webView performAfterReceivingAnyMessage:^(NSString *message) {
        if ([message isEqualToString:@"SHIELD_IFRAME_READY"])
            iframeShieldDidLoad = true;
        if ([message isEqualToString:@"BLOCKED_IFRAME_LOADED"])
            blockedIframeLoaded = true;
    }];

    [webView loadRequest:server.request("/mainframe"_s)];
    Util::run(&iframeShieldDidLoad);
    EXPECT_FALSE(blockedIframeLoaded);
}

TEST(ParentalControlsContentFilteringTests, OpenBlockedPopUp)
{
    auto mainframeHTML = "<script>"
    "var blockedUrl = 'https://example.com/blockedSite';"
    "window.open(blockedUrl);"
    "</script>"_s;

    NSData *popupReplacementHTML = [@"<script>"
    "window.webkit.messageHandlers.testHandler.postMessage('popup_html_loaded');"
    "</script>" dataUsingEncoding:NSUTF8StringEncoding];

    HTTPServer server({
        { "/mainframe"_s, { { { "Content-Type"_s, "text/html"_s } }, mainframeHTML } },
        { "/blockedSite"_s, { { { "Content-Type"_s, "text/html"_s } }, "<p>This site should be blocked</p>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto configuration = server.httpsProxyConfiguration();
    configuration.preferences.javaScriptCanOpenWindowsAutomatically = true;

    RetainPtr messageHandler = adoptNS([TestMessageHandler new]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);

    __block bool popupLoaded = false;
    [messageHandler addMessage:@"popup_html_loaded" withHandler:^{
        popupLoaded = true;
    }];

    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:navigationDelegate];

    __block RetainPtr<WKWebView> popupWindow;
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        popupWindow = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        [popupWindow.get() setNavigationDelegate:navigationDelegate];
        return popupWindow.get();
    };
    [webView setUIDelegate:uiDelegate];

    auto blockedURL = [NSURL URLWithString:@"https://example.com/blockedSite"];
    __block bool mockInstalled = false;
    [[webView configuration].websiteDataStore _installMockParentalControlsURLFilterForTestingWithBlockedURLs:@[blockedURL] replacementData:popupReplacementHTML completionHandler:^{
        mockInstalled = true;
    }];
    Util::run(&mockInstalled);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.org/mainframe"]]];
    Util::run(&popupLoaded);
}

TEST(ParentalControlsContentFilteringTests, BlockedNavigationFromLinkClick)
{
    auto mainframeHTML = "<html><a id='jsClickHere'></a></html>"
    "<script>"
    "jsClickHere.href = 'https://example.com/blockedSite';"
    "jsClickHere.click()"
    "</script>"_s;

    NSData *replacementHTML =[@"<script>"
    "window.webkit.messageHandlers.testHandler.postMessage('replacement_html_loaded', '*');"
    "</script>" dataUsingEncoding:NSUTF8StringEncoding];

    HTTPServer server({
        { "/mainframe"_s, { { { "Content-Type"_s, "text/html"_s } }, mainframeHTML } },
        { "/blockedSite"_s, { { { "Content-Type"_s, "text/html"_s } }, "<p>This site should be blocked</p>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto configuration = server.httpsProxyConfiguration();
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);

    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:navigationDelegate];

    auto blockedURL = [NSURL URLWithString:@"https://example.com/blockedSite"];
    __block bool mockInstalled = false;
    [[webView configuration].websiteDataStore _installMockParentalControlsURLFilterForTestingWithBlockedURLs:@[blockedURL] replacementData:replacementHTML completionHandler:^{
        mockInstalled = true;
    }];
    Util::run(&mockInstalled);

    __block bool shieldLoaded = false;
    [webView performAfterReceivingAnyMessage:^(NSString *message) {
        if ([message isEqualToString:@"replacement_html_loaded"])
            shieldLoaded = true;
    }];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.org/mainframe"]]];
    Util::run(&shieldLoaded);
}

#endif // HAVE(WEBCONTENTRESTRICTIONS)
