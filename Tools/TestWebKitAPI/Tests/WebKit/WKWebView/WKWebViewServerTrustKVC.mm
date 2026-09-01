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
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestUIDelegate.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/MakeString.h>

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

@interface QualifiedServerTrustObserver : NSObject
- (void)waitUntilQualifiedServerTrustChanged;
@end

@implementation QualifiedServerTrustObserver {
    bool _observedQualifiedServerTrust;
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    EXPECT_WK_STREQ(keyPath, "qualifiedServerTrust");
    _observedQualifiedServerTrust = true;
}

- (void)waitUntilQualifiedServerTrustChanged
{
    _observedQualifiedServerTrust = false;
    while (!_observedQualifiedServerTrust)
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

TEST(WKWebView, ServerTrustAfterTwoProcessSwaps)
{
    using namespace TestWebKitAPI;
    HTTPServer server({
        { "/source"_s, { "hi"_s } },
        { "/destination"_s, { { { "Cross-Origin-Opener-Policy"_s, "same-origin"_s } }, "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:server.httpsProxyConfiguration()]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadURL:[NSURL URLWithString:@"https://webkit.org/source"]];
    [navigationDelegate waitForDidFinishNavigation];
    verifyCertificateAndPublicKey([webView serverTrust]);

    __block pid_t firstProvisionalProcessIdentifier = 0;
    navigationDelegate.get().didStartProvisionalNavigation = ^(WKWebView *view, WKNavigation *) {
        if (!firstProvisionalProcessIdentifier)
            firstProvisionalProcessIdentifier = view._provisionalWebProcessIdentifier;
    };
    [webView loadURL:[NSURL URLWithString:@"https://example.com/destination"]];
    [navigationDelegate waitForDidFinishNavigation];
    navigationDelegate.get().didStartProvisionalNavigation = nil;

    EXPECT_NE(firstProvisionalProcessIdentifier, 0);
    EXPECT_NE(firstProvisionalProcessIdentifier, [webView _webProcessIdentifier]);

    EXPECT_NOT_NULL([webView serverTrust]);
    verifyCertificateAndPublicKey([webView serverTrust]);
}

TEST(WKWebView, QualifiedServerTrustKVC)
{
    using namespace TestWebKitAPI;
    constexpr auto qualifiedServerTrustBody = "This is where the 2QWAC bytes will go."_s;

    HTTPServer server({ { "/2qwac"_s, { qualifiedServerTrustBody } } }, HTTPServer::Protocol::HttpsProxy);
    auto linkHeader = [&] (ASCIILiteral rel) {
        return makeString("<https://webkit.org/2qwac>; rel=\""_s, rel, "\""_s);
    };
    server.addResponse("/no-link"_s, { "hi"_s });
    server.addResponse("/binding-link"_s, { { { "Link"_s, linkHeader("tls-certificate-binding preload"_s) } }, "hi"_s });
    server.addResponse("/other-rel"_s, { { { "Link"_s, linkHeader("author"_s) } }, "hi"_s });
    server.addResponse("/many-links"_s, { { { "Link"_s, makeString("<https://webkit.org/author>; rel=\"author\", "_s, linkHeader("tls-certificate-binding"_s)) } }, "hi"_s });
    server.addResponse("/cross-origin-link"_s, { { { "Link"_s, "<https://example.com/2qwac>; rel=\"tls-certificate-binding\""_s } }, "hi"_s });

    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setQualifiedServerTrustDebugEnabledForTesting:YES];
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    RetainPtr viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:viewConfiguration.get()]);
    RetainPtr delegate = adoptNS([TestNavigationDelegate new]);
    webView.get().navigationDelegate = delegate.get();
    [delegate allowAnyTLSCertificate];
    EXPECT_NULL([webView valueForKey:@"qualifiedServerTrust"]);

    RetainPtr observer = adoptNS([QualifiedServerTrustObserver new]);
    [webView addObserver:observer.get() forKeyPath:@"qualifiedServerTrust" options:NSKeyValueObservingOptionNew context:nil];

    [webView loadURL:[NSURL URLWithString:@"https://webkit.org/binding-link"]];
    [observer waitUntilQualifiedServerTrustChanged];
    verifyCertificateAndPublicKey([webView qualifiedServerTrust]);

    // Committing a response without a tls-certificate-binding link clears the 2QWAC.
    [webView loadURL:[NSURL URLWithString:@"https://webkit.org/no-link"]];
    [observer waitUntilQualifiedServerTrustChanged];
    EXPECT_NULL([webView qualifiedServerTrust]);

    // The tls-certificate-binding link is found even when it isn't the only link.
    [webView loadURL:[NSURL URLWithString:@"https://webkit.org/many-links"]];
    [observer waitUntilQualifiedServerTrustChanged];
    verifyCertificateAndPublicKey([webView qualifiedServerTrust]);

    // A link with a different rel is not a 2QWAC.
    [webView loadURL:[NSURL URLWithString:@"https://webkit.org/other-rel"]];
    [observer waitUntilQualifiedServerTrustChanged];
    EXPECT_NULL([webView qualifiedServerTrust]);

    // A cross-origin tls-certificate-binding link is ignored. The 2QWAC is already
    // null here, so there is nothing to observe and we wait for the navigation instead.
    [webView loadURL:[NSURL URLWithString:@"https://webkit.org/cross-origin-link"]];
    [delegate waitForDidFinishNavigation];
    Util::runFor(0.1_s);
    EXPECT_NULL([webView qualifiedServerTrust]);

    // Load a valid 2QWAC again to verify the checks above didn't just break the fetch entirely.
    [webView loadURL:[NSURL URLWithString:@"https://webkit.org/binding-link"]];
    [observer waitUntilQualifiedServerTrustChanged];
    verifyCertificateAndPublicKey([webView qualifiedServerTrust]);

    [webView removeObserver:observer.get() forKeyPath:@"qualifiedServerTrust"];
}
