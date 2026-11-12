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

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/_WKPageLoadTiming.h>
#import <wtf/RetainPtr.h>

// Regression test for rdar://172104196
// Verifies that page load timing is not delayed by canSendDisplayDidRefresh
// returning false when a single pending commit has reached CommitLayerTree state.
// The pre-307235@main behavior allowed DisplayDidRefresh to be sent as soon as
// NotifyFlushingLayerTree was received; the regression deferred it until the
// full CommitLayerTree IPC arrived, adding per-frame latency.

TEST(RemoteLayerTree, DisplayRefreshNotDelayedDuringPageLoad)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    // Load a page that triggers multiple layout passes to exercise the
    // DisplayDidRefresh pipeline during page load.
    NSString *html = @"<html><body>"
        "<div style='width:100%;height:200px;background:red'></div>"
        "<script>"
        "for (var i = 0; i < 20; i++) {"
        "  var d = document.createElement('div');"
        "  d.style.width = '100%';"
        "  d.style.height = '50px';"
        "  d.style.background = (i % 2 == 0) ? 'blue' : 'green';"
        "  document.body.appendChild(d);"
        "}"
        "</script>"
        "</body></html>";

    __block bool firstPaintReceived = false;
    __block NSTimeInterval navigationToFirstPaint = 0;

    delegate.get().didGeneratePageLoadTiming = ^(_WKPageLoadTiming *timing) {
        if (timing.firstVisualLayout && timing.navigationStart) {
            navigationToFirstPaint = [timing.firstVisualLayout timeIntervalSinceDate:timing.navigationStart];
            firstPaintReceived = true;
        }
    };

    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"about:blank"]];
    [delegate waitForDidFinishNavigation];

    // Wait for paint timing to be reported.
    if (!firstPaintReceived)
        TestWebKitAPI::Util::run(&firstPaintReceived);
