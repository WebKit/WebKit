/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if HAVE(NETWORK_FRAMEWORK)

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "Utilities.h"
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(Preconnect, HTTP)
{
    bool connected = false;
    bool requested = false;
    HTTPServer server([&] (Connection connection) {
        connected = true;
        connection.receiveHTTPRequest([&](Vector<char>&&) {
            requested = true;
        });
    });
    auto webView = adoptNS([WKWebView new]);
    [webView _preconnectToServer:server.request().URL];
    Util::run(&connected);
    Util::spinRunLoop(10);
    EXPECT_FALSE(requested);
    [webView loadRequest:server.request()];
    Util::run(&requested);
}

// Mojave CFNetwork _preconnect SPI seems to have a bug causing this to time out.
// That's no problem, because this is a test for SPI only to be used on later OS versions.
#if !PLATFORM(MAC) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500

TEST(Preconnect, HTTPS)
{
    bool connected = false;
    bool requested = false;
    __block bool receivedChallenge = false;
    HTTPServer server([&] (Connection connection) {
        connected = true;
        connection.receiveHTTPRequest([&](Vector<char>&&) {
            requested = true;
        });
    }, HTTPServer::Protocol::Https);
    auto webView = adoptNS([WKWebView new]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    [delegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        receivedChallenge = true;
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    }];
    [webView _preconnectToServer:server.request().URL];
    Util::run(&connected);
    Util::spinRunLoop(10);
    EXPECT_FALSE(receivedChallenge);
    EXPECT_FALSE(requested);
    [webView loadRequest:server.request()];
    Util::run(&requested);
    EXPECT_TRUE(receivedChallenge);
}

#endif

}

#endif
