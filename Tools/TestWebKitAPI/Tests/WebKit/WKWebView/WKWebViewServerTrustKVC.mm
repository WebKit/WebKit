/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/HTTPServer/TestCertificates.h"
#import "Helpers/cocoa/SiteIsolationTestUtilities.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestUIDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <wtf/RetainPtr.h>

@interface TrustObserver : NSObject
- (void)waitUntilServerTrustChanged;
@end

@implementation TrustObserver {
    bool _observedServerTrust;
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    EXPECT_WK_STREQ(keyPath, "serverTrust");
    _observedServerTrust = true;
}

- (void)waitUntilServerTrustChanged
{
    _observedServerTrust = false;
    while (!_observedServerTrust)
        TestWebKitAPI::Util::spinRunLoop();
}

@end

TEST(WKWebView, ServerTrustKVC)
{
    using namespace TestWebKitAPI;
    HTTPServer server({ { "/"_s, { "hi"_s } } }, HTTPServer::Protocol::Https);
    HTTPServer plaintextServer({ { "/"_s, { "hi"_s } } });
    RetainPtr webView = adoptNS([WKWebView new]);
    RetainPtr delegate = adoptNS([TestNavigationDelegate new]);
    webView.get().navigationDelegate = delegate.get();
    [delegate allowAnyTLSCertificate];
    EXPECT_NULL([webView valueForKey:@"serverTrust"]);

    RetainPtr observer = adoptNS([TrustObserver new]);
    [webView addObserver:observer.get() forKeyPath:@"serverTrust" options:NSKeyValueObservingOptionNew context:nil];
    [webView loadRequest:server.request()];
    [observer waitUntilServerTrustChanged];
    EXPECT_NOT_NULL([webView serverTrust]);

    [webView loadRequest:plaintextServer.request()];
    [observer waitUntilServerTrustChanged];
    EXPECT_NULL([webView serverTrust]);

    [webView goBack];
    [observer waitUntilServerTrustChanged];
    EXPECT_NOT_NULL([webView serverTrust]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://localhost:%d/", server.port()]]]];
    [observer waitUntilServerTrustChanged];
    EXPECT_NOT_NULL([webView serverTrust]);

    [webView goBack];
    [observer waitUntilServerTrustChanged];
    EXPECT_NOT_NULL([webView serverTrust]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
    [observer waitUntilServerTrustChanged];
    EXPECT_NULL([webView serverTrust]);
}

TEST(WKWebView, ServerTrustKVCWithCOOP)
{
    using namespace TestWebKitAPI;
    HTTPServer server({
        { "/path1"_s, { "hi"_s } },
        { "/path2"_s, { { { "Cross-Origin-Opener-Policy"_s, "same-origin"_s } }, "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:server.httpsProxyConfiguration()]);
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    webView.get().UIDelegate = uiDelegate.get();
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();

    RetainPtr observer = adoptNS([TrustObserver new]);
    __block RetainPtr<WKWebView> opened;
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *, WKWindowFeatures *) {
        opened = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        opened.get().navigationDelegate = navigationDelegate.get();
        [opened addObserver:observer.get() forKeyPath:@"serverTrust" options:NSKeyValueObservingOptionNew context:nil];
        return opened.get();
    };
    [webView loadURL:[NSURL URLWithString:@"https://webkit.org/path1"]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView evaluateJavaScript:@"window.open('https://webkit.org/path2')" completionHandler:nil];
    [observer waitUntilServerTrustChanged];

    [webView loadHTMLString:@"<script>alert('loaded')</script>" baseURL:[NSURL URLWithString:@"https://webkit.org/path1"]];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "loaded");
    EXPECT_NULL(webView.get().serverTrust);
}

TEST(WKWebView, ServerTrustForSubframeLoadedFromMemoryCache)
{
    using namespace TestWebKitAPI;

    // The second iframe has the same URL as the first, so its main resource is served from the
    // WebContent process's memory cache without a network process request. The network process
    // therefore never sends a certificate for the second frame, but the UI process still needs
    // to report one because the cached response had one.
    HTTPServer server({
        { "/"_s, { "<script>"
            "let loadedCount = 0;"
            "addEventListener('message', () => {"
            "    if (++loadedCount == 1) {"
            "        let iframe = document.createElement('iframe');"
            "        iframe.src = '/frame';"
            "        document.body.appendChild(iframe);"
            "    } else"
            "        alert('both frames loaded');"
            "});"
            "</script>"
            "<iframe src='/frame'></iframe>"_s } },
        { "/frame"_s, { { { "Cache-Control"_s, "max-age=3600"_s } }, "<script>addEventListener('load', () => parent.postMessage('loaded', '*'))</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:server.httpsProxyConfiguration()]);
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    webView.get().UIDelegate = uiDelegate.get();
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadURL:[NSURL URLWithString:@"https://webkit.org/"]];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "both frames loaded");

    __block bool done { false };
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        EXPECT_EQ(mainFrame.childFrames.count, 2u);
        for (_WKFrameTreeNode *child in mainFrame.childFrames) {
            EXPECT_WK_STREQ(child.info.request.URL.absoluteString, "https://webkit.org/frame");
            EXPECT_NOT_NULL(child.info._serverTrust);
            EXPECT_TRUE([TestCertificates verifyCertificateAndPublicKeyOfTrust:child.info._serverTrust]);
        }
        done = true;
    }];
    Util::run(&done);
}

TEST(WKWebView, ServerTrustForCrossSiteSubframeWithSiteIsolation)
{
    using namespace TestWebKitAPI;

    HTTPServer server({
        { "/"_s, { "<script>"
            "addEventListener('message', () => alert('subframe loaded'));"
            "</script>"
            "<iframe src='https://example.com/frame'></iframe>"_s } },
        { "/frame"_s, { "<script>addEventListener('load', () => parent.postMessage('loaded', '*'))</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectZero);
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/"]]];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "subframe loaded");

    __block bool done { false };
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        EXPECT_EQ(mainFrame.childFrames.count, 1u);
        for (_WKFrameTreeNode *child in mainFrame.childFrames) {
            EXPECT_WK_STREQ(child.info.request.URL.absoluteString, "https://example.com/frame");
            EXPECT_NOT_NULL(child.info._serverTrust);
            EXPECT_TRUE([TestCertificates verifyCertificateAndPublicKeyOfTrust:child.info._serverTrust]);
        }
        done = true;
    }];
    Util::run(&done);
}
