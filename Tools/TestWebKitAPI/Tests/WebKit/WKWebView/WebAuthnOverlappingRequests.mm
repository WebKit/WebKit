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

#if ENABLE(WEB_AUTHN)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKWebViewConfiguration.h>
#import <memory>
#import <wtf/text/MakeString.h>

namespace TestWebKitAPI {

static constexpr auto overlappingRequestsOrigin = "https://example.com"_s;

// Conditional requests are used so that no modal AuthenticationServices UI is presented.
static NSString *overlappingRequestsPage()
{
    return @"<!DOCTYPE html>"
        "<input autocomplete='username webauthn'>"
        "<script>"
        "window.results = [];"
        "window.conditionalGet = function(index) {"
        "  navigator.credentials.get({"
        "    mediation: 'conditional',"
        "    publicKey: { challenge: new Uint8Array(32), rpId: 'example.com', allowCredentials: [] }"
        "  }).then(() => { window.results[index] = 'resolved'; }, e => { window.results[index] = e.name; });"
        "};"
        "</script>";
}

static RetainPtr<NSString> waitForResult(TestWKWebView *webView, unsigned index)
{
    RetainPtr script = [NSString stringWithFormat:@"window.results[%u] || ''", index];
    auto outcome = std::make_shared<RetainPtr<NSString>>();
    Util::waitFor([webView, script, outcome] {
        *outcome = [webView stringByEvaluatingJavaScript:script.get()];
        return [*outcome length] > 0;
    }, 200);
    return *outcome;
}

TEST(WebAuthnOverlappingRequests, ConditionalGetSupersededByConditionalGet)
{
    HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, String { overlappingRequestsPage() } } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 300) configuration:server.httpsProxyConfiguration()]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView focus];

    RetainPtr url = adoptNS([[NSURL alloc] initWithString:makeString(overlappingRequestsOrigin, "/"_s).createNSString()]);
    [webView loadRequest:[NSURLRequest requestWithURL:url.get()]];
    [navigationDelegate waitForDidFinishNavigation];

    // Issue both requests in the same task so that the second one reaches the UI process
    // while the first one is still waiting on AuthenticationServices.
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"conditionalGet(0); conditionalGet(1); undefined"];

    // The superseded request must settle rather than being dropped on the floor.
    EXPECT_WK_STREQ(waitForResult(webView.get(), 0).get(), @"NotAllowedError");
    EXPECT_WK_STREQ(waitForResult(webView.get(), 1).get(), @"NotAllowedError");
}

} // namespace TestWebKitAPI

#endif // ENABLE(WEB_AUTHN)
