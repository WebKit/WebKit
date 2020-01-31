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

#include "config.h"

#if HAVE(SSL)

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

#if PLATFORM(IOS_FAMILY)
#import <WebKit/WebUIKitSupport.h>
#import <WebKit/WebCoreThread.h>
#endif

#if HAVE(TLS_PROTOCOL_VERSION_T)
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
#endif

@interface TLSNavigationDelegate : NSObject <WKNavigationDelegate>
- (void)waitForDidFinishNavigation;
- (void)waitForDidFailProvisionalNavigation;
- (bool)receivedShouldAllowLegacyTLS;
@property (nonatomic) bool shouldAllowLegacyTLS;
@end

@implementation TLSNavigationDelegate {
    bool _navigationFinished;
    bool _navigationFailed;
    bool _receivedShouldAllowLegacyTLS;
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

- (bool)receivedShouldAllowLegacyTLS
{
    return _receivedShouldAllowLegacyTLS;
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

- (void)_webView:(WKWebView *)webView authenticationChallenge:(NSURLAuthenticationChallenge *)challenge shouldAllowLegacyTLS:(void (^)(BOOL))completionHandler
{
    _receivedShouldAllowLegacyTLS = true;
    completionHandler([self shouldAllowLegacyTLS]);
}

@end


namespace TestWebKitAPI {

const uint16_t tls1_1 = 0x0302;
static NSString *defaultsKey = @"WebKitEnableLegacyTLS";

TEST(TLSVersion, DefaultBehavior)
{
    TCPServer server(TCPServer::Protocol::HTTPS, TCPServer::respondWithOK, tls1_1);
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

// FIXME: This test should remain disabled until rdar://problem/56522601 is fixed.
TEST(TLSVersion, DISABLED_NetworkSession)
{
    static auto delegate = adoptNS([TestNavigationDelegate new]);
    auto makeWebViewWith = [&] (WKWebsiteDataStore *store) {
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
    {
        TCPServer server(TCPServer::Protocol::HTTPS, TCPServer::respondWithOK, tls1_1);
        auto webView = makeWebViewWith([WKWebsiteDataStore defaultDataStore]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]]];
        [delegate waitForDidFinishNavigation];
    }
    {
        TCPServer server(TCPServer::Protocol::HTTPS, TCPServer::respondWithOK, tls1_1);
        auto webView = makeWebViewWith([WKWebsiteDataStore nonPersistentDataStore]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]]];
        [delegate waitForDidFinishNavigation];
    }
    {
        TCPServer server(TCPServer::Protocol::HTTPS, [](SSL *ssl) {
            EXPECT_FALSE(ssl);
        }, tls1_1);
        auto configuration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
        [configuration setLegacyTLSEnabled:NO];
        auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:configuration.get()]);
        auto webView = makeWebViewWith(dataStore.get());
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]]];
        [delegate waitForDidFailProvisionalNavigation];
    }
    [[NSUserDefaults standardUserDefaults] setBool:NO forKey:defaultsKey];
    {
        TCPServer server(TCPServer::Protocol::HTTPS, [](SSL *ssl) {
            EXPECT_FALSE(ssl);
        }, tls1_1);
        auto webView = makeWebViewWith([WKWebsiteDataStore defaultDataStore]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]]];
        [delegate waitForDidFailProvisionalNavigation];
    }
    {
        TCPServer server(TCPServer::Protocol::HTTPS, [](SSL *ssl) {
            EXPECT_FALSE(ssl);
        }, tls1_1);
        auto webView = makeWebViewWith([WKWebsiteDataStore nonPersistentDataStore]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]]];
        [delegate waitForDidFailProvisionalNavigation];
    }
    [[NSUserDefaults standardUserDefaults] removeObjectForKey:defaultsKey];
}

// FIXME: This test should remain disabled until rdar://problem/56522601 is fixed.
TEST(TLSVersion, DISABLED_NavigationDelegateSPI)
{
    {
        auto delegate = adoptNS([TLSNavigationDelegate new]);
        TCPServer server(TCPServer::Protocol::HTTPS, [](SSL *ssl) {
            // FIXME: This is only if we have the new SPI.
            EXPECT_FALSE(ssl);
        }, tls1_1);
        auto webView = adoptNS([WKWebView new]);
        [webView setNavigationDelegate:delegate.get()];
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]]];
        [delegate waitForDidFailProvisionalNavigation];
        EXPECT_TRUE([delegate receivedShouldAllowLegacyTLS]);
    }
    {
        auto delegate = adoptNS([TLSNavigationDelegate new]);
        delegate.get().shouldAllowLegacyTLS = YES;
        TCPServer server(TCPServer::Protocol::HTTPS, TCPServer::respondWithOK, tls1_1);
        auto webView = adoptNS([WKWebView new]);
        [webView setNavigationDelegate:delegate.get()];
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]]];
        [delegate waitForDidFinishNavigation];
        EXPECT_TRUE([delegate receivedShouldAllowLegacyTLS]);
    }
}

#if HAVE(TLS_PROTOCOL_VERSION_T)
TEST(TLSVersion, NegotiatedLegacyTLS)
{
    TCPServer server(TCPServer::Protocol::HTTPS, [] (SSL *ssl) {
        TCPServer::respondWithOK(ssl);
        TCPServer::respondWithOK(ssl);
    }, tls1_1);

    auto delegate = adoptNS([TestNavigationDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];
    [delegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    }];
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
#endif

// FIXME: Add some tests for WKWebView.hasOnlySecureContent

}

#endif // HAVE(SSL)
