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

#import "Helpers/Test.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
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
