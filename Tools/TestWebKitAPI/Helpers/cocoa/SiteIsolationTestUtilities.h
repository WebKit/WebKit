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

#pragma once

// Shared by the SiteIsolation*.mm API tests.

#ifdef __cplusplus

#import <WebKit/WKWebViewPrivate.h>
#import <utility>
#import <wtf/RetainPtr.h>
#import <wtf/text/ASCIILiteral.h>

@class TestNavigationDelegate;
@class TestWKWebView;
@class WKFrameInfo;
@class WKWebViewConfiguration;

namespace TestWebKitAPI {

class HTTPServer;

void setFeatureEnabled(WKWebViewConfiguration *, NSString *featureName, bool enabled);
void enableSiteIsolation(WKWebViewConfiguration *);

std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewAndDelegate(RetainPtr<WKWebViewConfiguration>, CGRect, bool enable);
std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewAndDelegate(RetainPtr<WKWebViewConfiguration>, CGRect = CGRectZero);
std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewAndDelegate(const HTTPServer&, CGRect = CGRectZero);

// Some main frame text and a 400x300 cross-origin iframe with id 'iframe', loaded from https://webkit.org/iframe.
static constexpr auto mainFrameTextWithCrossOriginIframe = "<body style='margin: 0'>main frame text<iframe id='iframe' style='width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s;

struct WebViewWithFocusedCrossOriginIframe {
    RetainPtr<TestWKWebView> webView;
    RetainPtr<TestNavigationDelegate> navigationDelegate;
    RetainPtr<WKFrameInfo> childFrame;
};

// Loads https://example.com/mainframe in a site-isolated web view, then focuses its first child frame,
// which must be an iframe with id 'iframe' (see mainFrameTextWithCrossOriginIframe).
WebViewWithFocusedCrossOriginIframe webViewWithFocusedCrossOriginIframe(const HTTPServer&);

// Runs a selection script in the frame, then waits for the UI process's editor state to reflect the new
// selection, since some commands check it before sending anything to a web process.
void setSelectionInFrame(TestWKWebView *, WKFrameInfo *, NSString *script, _WKSelectionAttributes expectedSelection);

} // namespace TestWebKitAPI

#endif // __cplusplus
