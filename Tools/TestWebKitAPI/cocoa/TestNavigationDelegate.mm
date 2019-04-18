/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#import "TestNavigationDelegate.h"

#import "Utilities.h"
#import <wtf/RetainPtr.h>

@implementation TestNavigationDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    if (_decidePolicyForNavigationAction)
        _decidePolicyForNavigationAction(navigationAction, decisionHandler);
    else
        decisionHandler(WKNavigationActionPolicyAllow);
}

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation
{
    if (_didStartProvisionalNavigation)
        _didStartProvisionalNavigation(webView, navigation);
}

- (void)webView:(WKWebView *)webView didCommitNavigation:(WKNavigation *)navigation
{
    if (_didCommitNavigation)
        _didCommitNavigation(webView, navigation);
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    if (_didFailProvisionalNavigation)
        _didFailProvisionalNavigation(webView, navigation, error);
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    if (_didFinishNavigation)
        _didFinishNavigation(webView, navigation);
}

- (void)webViewWebContentProcessDidTerminate:(WKWebView *)webView
{
    if (_webContentProcessDidTerminate)
        _webContentProcessDidTerminate(webView);
}

- (void)_webView:(WKWebView *)webView renderingProgressDidChange:(_WKRenderingProgressEvents)progressEvents
{
    if (_renderingProgressDidChange)
        _renderingProgressDidChange(webView, progressEvents);
}

- (void)waitForDidStartProvisionalNavigation
{
    EXPECT_FALSE(self.didStartProvisionalNavigation);

    __block bool finished = false;
    self.didStartProvisionalNavigation = ^(WKWebView *, WKNavigation *) {
        finished = true;
    };

    TestWebKitAPI::Util::run(&finished);

    self.didStartProvisionalNavigation = nil;
}

- (void)waitForDidFinishNavigation
{
    EXPECT_FALSE(self.didFinishNavigation);

    __block bool finished = false;
    self.didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finished = true;
    };

    TestWebKitAPI::Util::run(&finished);

    self.didFinishNavigation = nil;
}

@end

@implementation WKWebView (TestWebKitAPIExtras)

- (void)_test_waitForDidStartProvisionalNavigation
{
    EXPECT_FALSE(self.navigationDelegate);

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    self.navigationDelegate = navigationDelegate.get();
    [navigationDelegate waitForDidStartProvisionalNavigation];

    self.navigationDelegate = nil;
}

- (void)_test_waitForDidFinishNavigation
{
    EXPECT_FALSE(self.navigationDelegate);

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    self.navigationDelegate = navigationDelegate.get();
    [navigationDelegate waitForDidFinishNavigation];

    self.navigationDelegate = nil;

#if PLATFORM(IOS_FAMILY)
    __block bool presentationUpdateHappened = false;
    [self _doAfterNextPresentationUpdateWithoutWaitingForAnimatedResizeForTesting:^{
        presentationUpdateHappened = true;
    }];
    TestWebKitAPI::Util::run(&presentationUpdateHappened);
#endif
}

@end
