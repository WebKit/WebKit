/*
 * Copyright (C) 2024 e3 Software LLC. All rights reserved.
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
#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(WebKit2, GetMatchedCSSRulesTest)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 120, 200)]);
    auto html = @"<!DOCTYPE html>"
    "<html>"
    "<head><style type=\"text/css\">p { font-size: 100px; }</style></head>"
    "<body><p>Hello</p></body>"
    "</html>";

    // Why use "http://localhost" for the baseURL?
    //
    // Using `nil` or a `file:` baseURL causes the test to fail. This is because
    // when LocalDOMWindow::getMatchedCSSRules enumerates the matched rules, none of
    // them pass the cross-origin check (hasDocumentSecurityOrigin is false for all the
    // rules). This is because when the CSS parser context is created, the logic that
    // sets the value of hasDocumentSecurityOrigin does so by calling SecurityOrigin::canRequest.
    // Peeking inside SecurityOrigin::canRequest, we see why using a `nil` or `file:` baseURL
    // fails:
    //
    // 1. In the case of `nil` baseURL, WebKit turns that into an effective baseURL of "about:blank".
    //    Because "about:blank" is an opaque origin, canRequest returns false.
    //
    // 2. In the case of a `file:` baseURL, none of the patterns (OriginAccessPatternsForWebProcess)
    //    match, so canRequest returns false.
    //
    // Notably, WK1 doesn't have the same problem. In WK1, if you load a page with a `nil` baseURL,
    // WebKit turns it into an effective baseURL of "applewebdata://{uuid}", which passes the
    // isSameSchemeHostPort test in SecurityOrigin::canRequest.
    [webView synchronouslyLoadHTMLString:html baseURL:[NSURL URLWithString:@"http://localhost"]];

    auto world = [WKContentWorld worldWithName:@"NamedWorld"];
    auto js = @"const p = document.querySelector(\"p\");"
    "const list = window.getMatchedCSSRules(p, null, true);"
    "list.length;";
    __block bool isDone = false;
    [webView evaluateJavaScript:js inFrame:nil inContentWorld:world completionHandler:^(id _Nullable reply, NSError * _Nullable error) {
        EXPECT_EQ(static_cast<NSNumber *>(reply).integerValue, 1);
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
}

} // namespace TestWebKitAPI
