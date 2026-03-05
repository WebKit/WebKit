/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "WebCoreTestSupport.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/MakeString.h>

#if PLATFORM(IOS_FAMILY)
#import <WebKit/WebUIKitSupport.h>
#import <WebKit/WebCoreThread.h>
#endif

@interface TLSObserver : NSObject
- (void)waitUntilNegotiatedLegacyTLSChanged;
@end

@implementation TLSObserver {
    bool _negotiatedLegacyTLSChanged;
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    EXPECT_WK_STREQ(keyPath, "_negotiatedLegacyTLS");
    _negotiatedLegacyTLSChanged = true;
}

- (void)waitUntilNegotiatedLegacyTLSChanged
{
    _negotiatedLegacyTLSChanged = false;
    while (!_negotiatedLegacyTLSChanged)
        TestWebKitAPI::Util::spinRunLoop();
}

@end

@interface TLSNavigationDelegate : NSObject <WKNavigationDelegate>
- (void)waitForDidFinishNavigation;
- (void)waitForDidFailProvisionalNavigation;
- (NSURL *)waitForDidNegotiateModernTLS;
- (bool)receivedShouldAllowDeprecatedTLS;
@property (nonatomic) bool shouldAllowDeprecatedTLS;
@end

@implementation TLSNavigationDelegate {
    bool _navigationFinished;
    bool _navigationFailed;
    bool _receivedShouldAllowDeprecatedTLS;
    RetainPtr<NSURL> _negotiatedModernTLS;
}

- (void)waitForDidFinishNavigation
{
    while (!_navigationFinished)
        TestWebKitAPI::Util::spinRunLoop();
}

- (void)waitForDidFailProvisionalNavigation
{
    while (!_navigationFailed)
        TestWebKitAPI::Util::spinRunLoop();
}

- (NSURL *)waitForDidNegotiateModernTLS
{
    while (!_negotiatedModernTLS)
        TestWebKitAPI::Util::spinRunLoop();
    return _negotiatedModernTLS.autorelease();
}

- (bool)receivedShouldAllowDeprecatedTLS
{
    return _receivedShouldAllowDeprecatedTLS;
}

- (void)webView:(WKWebView *)webView didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential * credential))completionHandler
{
    EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
    completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    _navigationFinished = true;
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    _navigationFailed = true;
}

- (void)webView:(WKWebView *)webView authenticationChallenge:(NSURLAuthenticationChallenge *)challenge shouldAllowDeprecatedTLS:(void (^)(BOOL))completionHandler
{
    _receivedShouldAllowDeprecatedTLS = true;
    completionHandler([self shouldAllowDeprecatedTLS]);
}

- (void)_webView:(WKWebView *)webView didNegotiateModernTLSForURL:(NSURL *)url
{
    _negotiatedModernTLS = url;
}

@end


namespace TestWebKitAPI {

const uint16_t tls1_1 = 0x0302;

TEST(TLSVersion, DefaultBehavior)
{
    HTTPServer server(HTTPServer::respondWithOK, HTTPServer::Protocol::HttpsWithLegacyTLS);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];
    [delegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    }];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]]];
#if ENABLE(TLS_1_2_DEFAULT_MINIMUM)
    [delegate waitForDidFailProvisionalNavigation];
#else
    [delegate waitForDidFinishNavigation];
#endif
}

RetainPtr<WKWebView> makeWebViewWith(WKWebsiteDataStore *store, RetainPtr<TestNavigationDelegate> delegate)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:store];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:delegate.get()];
    [delegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    }];
    return webView;
};

#if HAVE(TLS_VERSION_DURING_CHALLENGE)

TEST(TLSVersion, NetworkSession)
{
    HTTPServer server(HTTPServer::respondWithOK, HTTPServer::Protocol::HttpsWithLegacyTLS);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    {
        auto webView = makeWebViewWith([WKWebsiteDataStore defaultDataStore], delegate);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]]];
#if ENABLE(TLS_1_2_DEFAULT_MINIMUM)
        [delegate waitForDidFailProvisionalNavigation];
#else
        [delegate waitForDidFinishNavigation];
#endif
    }
    {
        auto webView = makeWebViewWith([WKWebsiteDataStore nonPersistentDataStore], delegate);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]]];
#if ENABLE(TLS_1_2_DEFAULT_MINIMUM)
        [delegate waitForDidFailProvisionalNavigation];
#else
        [delegate waitForDidFinishNavigation];
#endif
    }
    {
        auto configuration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
        [configuration setLegacyTLSEnabled:NO];
        auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:configuration.get()]);
        auto webView = makeWebViewWith(dataStore.get(), delegate);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]]];
        [delegate waitForDidFailProvisionalNavigation];
    }
    {
        auto configuration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
        [configuration setLegacyTLSEnabled:YES];
        auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:configuration.get()]);
        auto webView = makeWebViewWith(dataStore.get(), delegate);
        [webView loadRequest:server.request()];
        [delegate waitForDidFinishNavigation];
    }
}

TEST(TLSVersion, ShouldAllowDeprecatedTLS)
{
    HTTPServer server(HTTPServer::respondWithOK, HTTPServer::Protocol::HttpsWithLegacyTLS);
    {
        auto delegate = adoptNS([TLSNavigationDelegate new]);
        auto webView = adoptNS([WKWebView new]);
        [webView setNavigationDelegate:delegate.get()];
        [webView loadRequest:server.request()];
        [delegate waitForDidFailProvisionalNavigation];
#if ENABLE(TLS_1_2_DEFAULT_MINIMUM)
        EXPECT_FALSE([delegate receivedShouldAllowDeprecatedTLS]);
#else
        EXPECT_TRUE([delegate receivedShouldAllowDeprecatedTLS]);
#endif
    }
    {
        auto delegate = adoptNS([TLSNavigationDelegate new]);
        delegate.get().shouldAllowDeprecatedTLS = YES;
#if ENABLE(TLS_1_2_DEFAULT_MINIMUM)
        auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
        auto configuration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
        [configuration setLegacyTLSEnabled:YES];
        auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:configuration.get()]);
        auto webView = makeWebViewWith(dataStore.get(), navigationDelegate);
#else
        auto webView = adoptNS([WKWebView new]);
#endif
        [webView setNavigationDelegate:delegate.get()];
        [webView loadRequest:server.request()];
        [delegate waitForDidFinishNavigation];
        EXPECT_TRUE([delegate receivedShouldAllowDeprecatedTLS]);
    }
}

TEST(TLSVersion, Preconnect)
{
    bool connectionAttempted = false;
    HTTPServer server([&](const Connection&) {
        connectionAttempted = true;
    }, HTTPServer::Protocol::HttpsWithLegacyTLS);

    auto webView = adoptNS([WKWebView new]);
    [webView loadHTMLString:makeString("<head><link rel='preconnect' href='https://127.0.0.1:"_s, server.port(), "/'></link></head>"_s).createNSString().get() baseURL:nil];

    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    [delegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_TRUE(false);
        callback(NSURLSessionAuthChallengeUseCredential, nil);
    }];

    TestWebKitAPI::Util::run(&connectionAttempted);
}

#endif // HAVE(TLS_VERSION_DURING_CHALLENGE)

static std::pair<RetainPtr<WKWebView>, RetainPtr<TestNavigationDelegate>> webViewWithNavigationDelegate(RetainPtr<WKWebViewConfiguration> configuration = nullptr)
{
    auto delegate = adoptNS([TestNavigationDelegate new]);

#if ENABLE(TLS_1_2_DEFAULT_MINIMUM)
    RetainPtr<WKWebView> webView;
    if (!configuration) {
        auto configuration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
        [configuration setLegacyTLSEnabled:YES];
        auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:configuration.get()]);
        webView = makeWebViewWith(dataStore.get(), delegate);
    } else
        webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
#else
    auto webView = configuration ? adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]) : adoptNS([WKWebView new]);
#endif

    [webView setNavigationDelegate:delegate.get()];
    [delegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    }];
    return { webView, delegate };
}

TEST(TLSVersion, NegotiatedLegacyTLS)
{
    HTTPServer server({
        { "/"_s, { "hello"_s } }
    }, HTTPServer::Protocol::HttpsWithLegacyTLS);

    auto [webView, delegate] = webViewWithNavigationDelegate();
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [webView loadRequest:request];

    auto observer = adoptNS([TLSObserver new]);
    [webView addObserver:observer.get() forKeyPath:@"_negotiatedLegacyTLS" options:NSKeyValueObservingOptionNew context:nil];
    
    EXPECT_FALSE([webView _negotiatedLegacyTLS]);
    [observer waitUntilNegotiatedLegacyTLSChanged];
    EXPECT_TRUE([webView _negotiatedLegacyTLS]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"]]];
    [observer waitUntilNegotiatedLegacyTLSChanged];
    EXPECT_FALSE([webView _negotiatedLegacyTLS]);

    [webView loadRequest:request];
    [observer waitUntilNegotiatedLegacyTLSChanged];
    EXPECT_TRUE([webView _negotiatedLegacyTLS]);

    [webView removeObserver:observer.get() forKeyPath:@"_negotiatedLegacyTLS"];
}

TEST(TLSVersion, NavigateBack)
{
    HTTPServer legacyTLSServer({
        { "/"_s, { "hello"_s } }
    }, HTTPServer::Protocol::HttpsWithLegacyTLS);

    HTTPServer modernTLSServer({
        { "/"_s, { "hello"_s } }
    }, HTTPServer::Protocol::Https);
    
    auto [webView, delegate] = webViewWithNavigationDelegate();
    auto observer = adoptNS([TLSObserver new]);
    [webView addObserver:observer.get() forKeyPath:@"_negotiatedLegacyTLS" options:NSKeyValueObservingOptionNew context:nil];

    [webView loadRequest:legacyTLSServer.request()];
    EXPECT_FALSE([webView _negotiatedLegacyTLS]);
    [delegate waitForDidFinishNavigation];
    EXPECT_TRUE([webView _negotiatedLegacyTLS]);

    [webView loadRequest:modernTLSServer.request()];
    [delegate waitForDidFinishNavigation];
    EXPECT_FALSE([webView _negotiatedLegacyTLS]);

    [webView goBack];
    [observer waitUntilNegotiatedLegacyTLSChanged];
    EXPECT_TRUE([webView _negotiatedLegacyTLS]);

    [webView removeObserver:observer.get() forKeyPath:@"_negotiatedLegacyTLS"];
}

TEST(TLSVersion, BackForwardNegotiatedLegacyTLS)
{
    HTTPServer secureServer({
        { "/"_s, { "hello"_s }}
    }, HTTPServer::Protocol::Https);
    HTTPServer insecureServer({
        { "/"_s, { "hello"_s } }
    }, HTTPServer::Protocol::HttpsWithLegacyTLS);
    HTTPServer mixedContentServer({
        { "/"_s, { {{ "Content-Type"_s, "text/html"_s }}, makeString("<img src='https://127.0.0.1:"_s, insecureServer.port(), "/'></img>"_s) } },
    }, HTTPServer::Protocol::Https);

    auto [webView, delegate] = webViewWithNavigationDelegate();
    EXPECT_FALSE([webView _negotiatedLegacyTLS]);

    [webView loadRequest:secureServer.request()];
    [delegate waitForDidFinishNavigation];
    EXPECT_FALSE([webView _negotiatedLegacyTLS]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://localhost:%d/", mixedContentServer.port()]]]];
    [delegate waitForDidFinishNavigation];
    while (![webView _negotiatedLegacyTLS])
        TestWebKitAPI::Util::spinRunLoop();
    EXPECT_TRUE([webView _negotiatedLegacyTLS]);

    [webView goBack];
    [delegate waitForDidFinishNavigation];
    EXPECT_FALSE([webView _negotiatedLegacyTLS]);

    [webView goForward];
    [delegate waitForDidFinishNavigation];
    while (![webView _negotiatedLegacyTLS])
        TestWebKitAPI::Util::spinRunLoop();
    EXPECT_TRUE([webView _negotiatedLegacyTLS]);
}

TEST(TLSVersion, Subresource)
{
    HTTPServer legacyTLSServer({
        { "/"_s, { "hello"_s } }
    }, HTTPServer::Protocol::HttpsWithLegacyTLS);

    HTTPServer modernTLSServer({
        { "/"_s, { makeString("<script>fetch('https://127.0.0.1:"_s, legacyTLSServer.port(), "/',{mode:'no-cors'})</script>"_s) } },
        { "/pageWithoutSubresource"_s, { "hello"_s }}
    }, HTTPServer::Protocol::Https);
    
    auto [webView, delegate] = webViewWithNavigationDelegate();
    auto observer = adoptNS([TLSObserver new]);
    [webView addObserver:observer.get() forKeyPath:@"_negotiatedLegacyTLS" options:NSKeyValueObservingOptionNew context:nil];

    EXPECT_FALSE([webView _negotiatedLegacyTLS]);
    [webView loadRequest:modernTLSServer.request()];
    while (![webView _negotiatedLegacyTLS])
        [observer waitUntilNegotiatedLegacyTLSChanged];
    
    EXPECT_TRUE([webView _negotiatedLegacyTLS]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/pageWithoutSubresource", modernTLSServer.port()]]]];
    [delegate waitForDidFinishNavigation];
    EXPECT_FALSE([webView _negotiatedLegacyTLS]);

    [webView goBack];
    [delegate waitForDidFinishNavigation];
    EXPECT_TRUE([webView _negotiatedLegacyTLS]);

    [webView removeObserver:observer.get() forKeyPath:@"_negotiatedLegacyTLS"];
}

TEST(TLSVersion, DidNegotiateModernTLS)
{
    HTTPServer server({
        { "/"_s, { "hello"_s }}
    }, HTTPServer::Protocol::Https);

    auto delegate = adoptNS([TLSNavigationDelegate new]);
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    auto dataStoreConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    [dataStoreConfiguration setFastServerTrustEvaluationEnabled:YES];
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]).get()];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:server.request()];
    NSURL *url = [delegate waitForDidNegotiateModernTLS];
    EXPECT_WK_STREQ(url.host, "127.0.0.1");
    EXPECT_EQ(url.port.unsignedShortValue, server.port());
}

#if HAVE(TLS_VERSION_DURING_CHALLENGE)

TEST(TLSVersion, LegacySubresources)
{
    HTTPServer legacyServer({
        { "/frame"_s, { "shouldn't load with fastServerTrustEvaluationEnabled"_s }}
    }, HTTPServer::Protocol::HttpsWithLegacyTLS);

    HTTPServer modernServer({
        { "/"_s, { makeString("<iframe src='https://127.0.0.1:"_s, legacyServer.port(), "/frame'/>"_s) }}
    }, HTTPServer::Protocol::Https);

    auto dataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [dataStoreConfiguration setFastServerTrustEvaluationEnabled:YES];
    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [webViewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]).get()];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);

    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    }];
    [webView setNavigationDelegate:delegate.get()];

    [webView loadRequest:modernServer.request()];
    [delegate waitForDidFinishNavigation];

    EXPECT_EQ(legacyServer.totalRequests(), 0u);
    EXPECT_EQ(modernServer.totalRequests(), 1u);

    auto defaultWebView = adoptNS([WKWebView new]);
    [defaultWebView setNavigationDelegate:delegate.get()];
    [defaultWebView loadRequest:modernServer.request()];
    [delegate waitForDidFinishNavigation];
#if ENABLE(TLS_1_2_DEFAULT_MINIMUM)
    EXPECT_EQ(legacyServer.totalRequests(), 0u);
#else
    EXPECT_EQ(legacyServer.totalRequests(), 1u);
#endif
    EXPECT_EQ(modernServer.totalRequests(), 2u);
}

#endif // HAVE(TLS_VERSION_DURING_CHALLENGE)

}
