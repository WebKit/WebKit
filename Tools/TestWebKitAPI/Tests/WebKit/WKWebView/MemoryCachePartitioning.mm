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

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/_WKUserStyleSheet.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

namespace TestWebKitAPI {

TEST(MemoryCache, FontFromUserStyleSheetKeepsCachePartition)
{
    HTTPServer server({
        { "/first.html"_s, { "<!DOCTYPE html><body>hello</body>"_s } },
        { "/second.html"_s, { "<!DOCTYPE html><body>hello</body>"_s } },
        { "/font.ttf"_s, { 404, { }, "no font here"_s } },
    });

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);

    [[configuration preferences] _setStorageBlockingPolicy:_WKStorageBlockingPolicyBlockThirdParty];

    RetainPtr styleSheetSource = [NSString stringWithFormat:@"@font-face { font-family: TestFont; src: url(\"%@/font.ttf\"); } body { font-family: TestFont; }", server.origin().createNSString().get()];
    RetainPtr styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource.get() forMainFrameOnly:YES]);
    [[configuration userContentController] _addUserStyleSheet:styleSheet.get()];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);

    __block bool navigationFinished = false;
    RetainPtr delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        navigationFinished = true;
    };
    [webView setNavigationDelegate:delegate.get()];

    auto loadAndWait = ^(NSURLRequest *request) {
        navigationFinished = false;
        [webView loadRequest:request];
        Util::run(&navigationFinished);
    };

    loadAndWait(server.request("/first.html"_s));
    EXPECT_TRUE(Util::waitFor([&] {
        return server.totalRequests() == 2;
    }));

    loadAndWait(server.request("/second.html"_s));
    Util::waitFor([&] {
        return server.totalRequests() == 4;
    });
}

} // namespace TestWebKitAPI
