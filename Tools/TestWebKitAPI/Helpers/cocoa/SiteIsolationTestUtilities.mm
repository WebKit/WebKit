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
#import "Helpers/cocoa/SiteIsolationTestUtilities.h"

#import "Helpers/Utilities.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/_WKFeature.h>

namespace TestWebKitAPI {

void setFeatureEnabled(WKWebViewConfiguration *configuration, NSString *featureName, bool enabled)
{
    auto preferences = [configuration preferences];
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:featureName]) {
            [preferences _setEnabled:enabled forFeature:feature];
            break;
        }
    }
}

void enableSiteIsolation(WKWebViewConfiguration *configuration)
{
    setFeatureEnabled(configuration, @"SiteIsolationEnabled", true);
}

std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewAndDelegate(RetainPtr<WKWebViewConfiguration> configuration, CGRect rect, bool enable)
{
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    if (enable)
        enableSiteIsolation(configuration.get());
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:rect configuration:configuration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();
    return { WTF::move(webView), WTF::move(navigationDelegate) };
}

std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewAndDelegate(RetainPtr<WKWebViewConfiguration> configuration, CGRect rect)
{
    return siteIsolatedViewAndDelegate(configuration, rect, true);
}

std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewAndDelegate(const HTTPServer& server, CGRect rect)
{
    return siteIsolatedViewAndDelegate(server.httpsProxyConfiguration(), rect, true);
}

WebViewWithFocusedCrossOriginIframe webViewWithFocusedCrossOriginIframe(const HTTPServer& server)
{
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
#if PLATFORM(IOS_FAMILY)
    [webView focusInWindow];
#endif

    RetainPtr childFrame = [webView firstChildFrame];
    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:nil];
    while (![childFrame _isFocused]) {
        Util::spinRunLoop();
        childFrame = [webView firstChildFrame];
    }

    return { WTF::move(webView), WTF::move(navigationDelegate), WTF::move(childFrame) };
}

void setSelectionInFrame(TestWKWebView *webView, WKFrameInfo *frame, NSString *script, _WKSelectionAttributes expectedSelection)
{
    [webView objectByEvaluatingJavaScript:script inFrame:frame];
    while (!([webView _selectionAttributes] & expectedSelection))
        Util::spinRunLoop();
}

} // namespace TestWebKitAPI
