/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "TestUIDelegate.h"
#import "Utilities.h"
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <pal/spi/cf/CFNetworkSPI.h>

namespace TestWebKitAPI {

#if HAVE(CFNETWORK_NSURLSESSION_HSTS_WITH_UNTRUSTED_ROOT)

std::pair<RetainPtr<WKWebView>, RetainPtr<TestNavigationDelegate>> hstsWebViewAndDelegate(const HTTPServer& httpsServer, const HTTPServer& httpServer)
{
    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", httpsServer.port()]]];
    [storeConfiguration setHTTPProxy:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", httpServer.port()]]];
    [storeConfiguration setAllowsServerPreconnect:NO];
    [storeConfiguration setAllowsHSTSWithUntrustedRootCertificate:YES];
    auto viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    delegate.get().didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    };
    return { WTFMove(webView), WTFMove(delegate) };
}

static HTTPServer hstsServer()
{
    return HTTPServer({{ "/", {{{"Strict-Transport-Security" , "max-age=31536000"}}, "" }}}, HTTPServer::Protocol::HttpsProxy);
}

TEST(HSTS, Basic)
{
    auto httpsServer = hstsServer();

    HTTPServer httpServer({{ "http://example.com/", { {{ "Strict-Transport-Security", "max-age=31536000"}}, "hi" }}});

    auto [webView, delegate] = hstsWebViewAndDelegate(httpsServer, httpServer);

    NSURLRequest *httpRequest = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://example.com/"]];
    NSURLRequest *httpsRequest = [NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/"]];

    [webView loadRequest:httpRequest];
    [delegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ(webView.get().URL.absoluteString, "http://example.com/");

    [webView reload];
    [delegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ(webView.get().URL.absoluteString, "http://example.com/");

    [webView loadRequest:httpsRequest];
    [delegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ(webView.get().URL.absoluteString, "https://example.com/");

    [webView loadRequest:httpRequest];
    [delegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ(webView.get().URL.absoluteString, "https://example.com/");
}

TEST(HSTS, ThirdParty)
{
    auto httpsServer = hstsServer();

    const char* html = "<script>"
        "var xhr = new XMLHttpRequest();"
        "xhr.open('GET', 'http://example.com/');"
        "xhr.onreadystatechange = function () { if(xhr.readyState == 4) { alert(xhr.responseURL + ' ' + xhr.responseText) } };"
        "xhr.send();"
        "</script>";
    
    HTTPServer httpServer({
        { "http://example.com/", { {{ "Access-Control-Allow-Origin", "http://example.org" }}, "hi" }},
        { "http://example.org/", { html }},
    });
    
    auto [webView, delegate] = hstsWebViewAndDelegate(httpsServer, httpServer);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/"]]];
    [delegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ(webView.get().URL.absoluteString, "https://example.com/");

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://example.org/"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "http://example.com/ hi");
    EXPECT_EQ(httpServer.totalRequests(), 2u);
}

TEST(HSTS, CrossOriginRedirect)
{
    auto httpsServer = hstsServer();

    HTTPServer httpServer({
        { "http://example.com/", { "hi" }},
        { "http://example.org/", { 301, {{ "Location", "http://example.com/" }} } },
    });

    auto [webView, delegate] = hstsWebViewAndDelegate(httpsServer, httpServer);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/"]]];
    [delegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ(webView.get().URL.absoluteString, "https://example.com/");

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://example.org/"]]];
    [delegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ(webView.get().URL.absoluteString, "https://example.com/");
    EXPECT_EQ(httpServer.totalRequests(), 1u);
}

#endif // HAVE(CFNETWORK_NSURLSESSION_HSTS_WITH_UNTRUSTED_ROOT)

} // namespace TestWebKitAPI
