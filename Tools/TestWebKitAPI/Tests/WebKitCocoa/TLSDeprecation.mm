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

#if HAVE(SSL)

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "TCPServer.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "WebCoreTestSupport.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/StringConcatenateNumbers.h>

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
- (NSURLAuthenticationChallenge *)waitForDidNegotiateModernTLS;
- (bool)receivedShouldAllowDeprecatedTLS;
@property (nonatomic) bool shouldAllowDeprecatedTLS;
@end

@implementation TLSNavigationDelegate {
    bool _navigationFinished;
    bool _navigationFailed;
    bool _receivedShouldAllowDeprecatedTLS;
    RetainPtr<NSURLAuthenticationChallenge> _negotiatedModernTLS;
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

- (NSURLAuthenticationChallenge *)waitForDidNegotiateModernTLS
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

- (void)_webView:(WKWebView *)webView didNegotiateModernTLS:(NSURLAuthenticationChallenge *)challenge
{
    _negotiatedModernTLS = challenge;
}

@end


namespace TestWebKitAPI {

const uint16_t tls1_1 = 0x0302;

#if HAVE(TLS_PROTOCOL_VERSION_T)

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
    [delegate waitForDidFinishNavigation];
}

#endif // HAVE(TLS_PROTOCOL_VERSION_T)

#if HAVE(TLS_VERSION_DURING_CHALLENGE)

RetainPtr<WKWebView> makeWebViewWith(WKWebsiteDataStore *store, RetainPtr<TestNavigationDelegate> delegate)
{
    WKWebViewConfiguration *configuration = [[[WKWebViewConfiguration alloc] init] autorelease];
    configuration.websiteDataStore = store;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [webView setNavigationDelegate:delegate.get()];
    [delegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    }];
    return webView;
};

TEST(TLSVersion, NetworkSession)
{
    HTTPServer server(HTTPServer::respondWithOK, HTTPServer::Protocol::HttpsWithLegacyTLS);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    {
        auto webView = makeWebViewWith([WKWebsiteDataStore defaultDataStore], delegate);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]]];
        [delegate waitForDidFinishNavigation];
    }
    {
        auto webView = makeWebViewWith([WKWebsiteDataStore nonPersistentDataStore], delegate);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]]];
        [delegate waitForDidFinishNavigation];
    }
    {
        auto configuration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
        [configuration setLegacyTLSEnabled:NO];
        auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:configuration.get()]);
        auto webView = makeWebViewWith(dataStore.get(), delegate);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]]];
        [delegate waitForDidFailProvisionalNavigation];
    }
}

TEST(TLSVersion, NetworkSessionNSUserDefaults)
{
    NSString *defaultsKey = @"WebKitEnableLegacyTLS";
    [[NSUserDefaults standardUserDefaults] setBool:NO forKey:defaultsKey];

    HTTPServer server(HTTPServer::respondWithOK, HTTPServer::Protocol::HttpsWithLegacyTLS);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    {
        auto webView = makeWebViewWith([WKWebsiteDataStore defaultDataStore], delegate);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]]];
        [delegate waitForDidFailProvisionalNavigation];
    }
    {
        auto webView = makeWebViewWith([WKWebsiteDataStore nonPersistentDataStore], delegate);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]]];
        [delegate waitForDidFailProvisionalNavigation];
    }
    [[NSUserDefaults standardUserDefaults] removeObjectForKey:defaultsKey];
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
        EXPECT_TRUE([delegate receivedShouldAllowDeprecatedTLS]);
    }
    {
        auto delegate = adoptNS([TLSNavigationDelegate new]);
        delegate.get().shouldAllowDeprecatedTLS = YES;
        auto webView = adoptNS([WKWebView new]);
        [webView setNavigationDelegate:delegate.get()];
        [webView loadRequest:server.request()];
        [delegate waitForDidFinishNavigation];
        EXPECT_TRUE([delegate receivedShouldAllowDeprecatedTLS]);
    }
}

TEST(TLSVersion, Preconnect)
{
    bool connectionAttempted = false;
    TCPServer server(TCPServer::Protocol::HTTPS, [&](SSL *ssl) {
        EXPECT_FALSE(ssl);
        connectionAttempted = true;
    }, tls1_1);

    auto webView = adoptNS([WKWebView new]);
    [webView loadHTMLString:makeString("<head><link rel='preconnect' href='https://127.0.0.1:", server.port(), "/'></link></head>") baseURL:nil];

    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    [delegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_TRUE(false);
        callback(NSURLSessionAuthChallengeUseCredential, nil);
    }];

    TestWebKitAPI::Util::run(&connectionAttempted);
}

#endif // HAVE(TLS_VERSION_DURING_CHALLENGE)

#if HAVE(TLS_PROTOCOL_VERSION_T)

static std::pair<RetainPtr<WKWebView>, RetainPtr<TestNavigationDelegate>> webViewWithNavigationDelegate()
{
    auto delegate = adoptNS([TestNavigationDelegate new]);
    auto webView = adoptNS([WKWebView new]);
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
        { "/", { "hello" } }
    }, HTTPServer::Protocol::HttpsWithLegacyTLS);

    auto [webView, delegate] = webViewWithNavigationDelegate();
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [webView loadRequest:request];

    auto observer = adoptNS([TLSObserver new]);
    [webView addObserver:observer.get() forKeyPath:@"_negotiatedLegacyTLS" options:NSKeyValueObservingOptionNew context:nil];
    
    EXPECT_FALSE([webView _negotiatedLegacyTLS]);
    [observer waitUntilNegotiatedLegacyTLSChanged];
    EXPECT_TRUE([webView _negotiatedLegacyTLS]);

    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
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
        { "/", { "hello" } }
    }, HTTPServer::Protocol::HttpsWithLegacyTLS);

    HTTPServer modernTLSServer({
        { "/", { "hello" } }
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
        { "/", { "hello" }}
    }, HTTPServer::Protocol::Https);
    HTTPServer insecureServer({
        { "/", { "hello" } }
    }, HTTPServer::Protocol::HttpsWithLegacyTLS);
    HTTPServer mixedContentServer({
        { "/", { {{ "Content-Type", "text/html" }}, makeString("<img src='https://127.0.0.1:", insecureServer.port(), "/'></img>") } },
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
        { "/", { "hello" } }
    }, HTTPServer::Protocol::HttpsWithLegacyTLS);

    HTTPServer modernTLSServer({
        { "/", { makeString("<script>fetch('https://127.0.0.1:", legacyTLSServer.port(), "/',{mode:'no-cors'})</script>") } },
        { "/pageWithoutSubresource", { "hello" }}
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
        { "/", { "hello" }}
    }, HTTPServer::Protocol::Https);

    auto delegate = adoptNS([TLSNavigationDelegate new]);
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    auto dataStoreConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    [dataStoreConfiguration setFastServerTrustEvaluationEnabled:YES];
    [configuration setWebsiteDataStore:[[[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()] autorelease]];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:server.request()];
    NSURLAuthenticationChallenge *challenge = [delegate waitForDidNegotiateModernTLS];
    EXPECT_WK_STREQ(challenge.protectionSpace.host, "127.0.0.1");
    EXPECT_EQ(challenge.protectionSpace.port, server.port());
}

TEST(TLSVersion, BackForwardHasOnlySecureContent)
{
    HTTPServer secureServer({
        { "/", { "hello" }}
    }, HTTPServer::Protocol::Https);
    HTTPServer insecureServer({
        { "/", { "hello" } }
    });
    HTTPServer mixedContentServer({
        { "/", { {{ "Content-Type", "text/html" }}, makeString("<img src='http://127.0.0.1:", insecureServer.port(), "/'></img>") } },
    }, HTTPServer::Protocol::Https);

    auto [webView, delegate] = webViewWithNavigationDelegate();
    EXPECT_FALSE([webView hasOnlySecureContent]);

    [webView loadRequest:secureServer.request()];
    [delegate waitForDidFinishNavigation];
    EXPECT_TRUE([webView hasOnlySecureContent]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://localhost:%d/", mixedContentServer.port()]]]];
    [delegate waitForDidFinishNavigation];
    while ([webView hasOnlySecureContent])
        TestWebKitAPI::Util::spinRunLoop();
    EXPECT_FALSE([webView hasOnlySecureContent]);

    [webView goBack];
    [delegate waitForDidFinishNavigation];
    EXPECT_TRUE([webView hasOnlySecureContent]);

    [webView goForward];
    [delegate waitForDidFinishNavigation];
    while ([webView hasOnlySecureContent])
        TestWebKitAPI::Util::spinRunLoop();
    EXPECT_FALSE([webView hasOnlySecureContent]);
}

#if HAVE(TLS_VERSION_DURING_CHALLENGE)

TEST(TLSVersion, LegacySubresources)
{
    HTTPServer legacyServer({
        { "/frame", { "shouldn't load with fastServerTrustEvaluationEnabled" }}
    }, HTTPServer::Protocol::HttpsWithLegacyTLS);

    HTTPServer modernServer({
        { "/", { makeString("<iframe src='https://127.0.0.1:", legacyServer.port(), "/frame'/>") }}
    }, HTTPServer::Protocol::Https);

    auto dataStoreConfiguration = [[[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration] autorelease];
    dataStoreConfiguration.fastServerTrustEvaluationEnabled = YES;
    auto webViewConfiguration = [[WKWebViewConfiguration new] autorelease];
    webViewConfiguration.websiteDataStore = [[[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration] autorelease];
    auto webView = [[[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration] autorelease];

    auto delegate = [[TestNavigationDelegate new] autorelease];
    [delegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    }];
    [webView setNavigationDelegate:delegate];

    [webView loadRequest:modernServer.request()];
    [delegate waitForDidFinishNavigation];

    EXPECT_EQ(legacyServer.totalRequests(), 0u);
    EXPECT_EQ(modernServer.totalRequests(), 1u);

    auto defaultWebView = [[WKWebView new] autorelease];
    [defaultWebView setNavigationDelegate:delegate];
    [defaultWebView loadRequest:modernServer.request()];
    [delegate waitForDidFinishNavigation];
    EXPECT_EQ(legacyServer.totalRequests(), 1u);
    EXPECT_EQ(modernServer.totalRequests(), 2u);
}

#endif // HAVE(TLS_VERSION_DURING_CHALLENGE)

#endif // HAVE(TLS_PROTOCOL_VERSION_T)

}

#endif // HAVE(SSL)
