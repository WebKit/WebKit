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

#import "PlatformUtilities.h"
#import "Utilities.h"
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

@implementation TestNavigationDelegate {
    RetainPtr<NSError> _navigationError;
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction preferences:(WKWebpagePreferences *)preferences decisionHandler:(void (^)(WKNavigationActionPolicy, WKWebpagePreferences *))decisionHandler
{
    if (_decidePolicyForNavigationActionWithPreferences)
        _decidePolicyForNavigationActionWithPreferences(navigationAction, preferences, decisionHandler);
    else if (_decidePolicyForNavigationAction) {
        _decidePolicyForNavigationAction(navigationAction, ^(WKNavigationActionPolicy action) {
            decisionHandler(action, preferences);
        });
    } else
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    if (_decidePolicyForNavigationResponse)
        _decidePolicyForNavigationResponse(navigationResponse, decisionHandler);
    else
        decisionHandler(WKNavigationResponsePolicyAllow);
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

- (void)_webView:(WKWebView *)webView didCommitLoadWithRequest:(NSURLRequest *)request inFrame:(WKFrameInfo *)frame
{
    if (_didCommitLoadWithRequestInFrame)
        _didCommitLoadWithRequestInFrame(webView, request, frame);
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

- (void)webView:(WKWebView *)webView didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential *))completionHandler
{
    if (_didReceiveAuthenticationChallenge)
        _didReceiveAuthenticationChallenge(webView, challenge, completionHandler);
    else
        completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
}

- (void)allowAnyTLSCertificate
{
    EXPECT_FALSE(self.didReceiveAuthenticationChallenge);
    self.didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    };
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

- (void)waitForWebContentProcessDidTerminate
{
    EXPECT_FALSE(self.webContentProcessDidTerminate);

    __block bool crashed = false;
    self.webContentProcessDidTerminate = ^(WKWebView *) {
        crashed = true;
    };

    TestWebKitAPI::Util::run(&crashed);

    self.webContentProcessDidTerminate = nil;
}

- (void)waitForDidFinishNavigationWithPreferences:(WKWebpagePreferences *)preferences
{
    EXPECT_FALSE(self.decidePolicyForNavigationActionWithPreferences);
    EXPECT_TRUE(!!preferences);

    self.decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *action, WKWebpagePreferences *, void (^handler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        handler(WKNavigationActionPolicyAllow, preferences);
    };

    [self waitForDidFinishNavigation];
    self.decidePolicyForNavigationActionWithPreferences = nil;
}

- (NSError *)waitForDidFailProvisionalNavigation
{
    EXPECT_FALSE(self.didFailProvisionalNavigation);

    __block bool finished = false;
    self.didFailProvisionalNavigation = ^(WKWebView *, WKNavigation *, NSError *error) {
        _navigationError = error;
        finished = true;
    };

    TestWebKitAPI::Util::run(&finished);

    self.didFailProvisionalNavigation = nil;
    return _navigationError.autorelease();
}

- (void)_webView:(WKWebView *)webView contentRuleListWithIdentifier:(NSString *)identifier performedAction:(_WKContentRuleListAction *)action forURL:(NSURL *)url
{
    if (_contentRuleListPerformedAction)
        _contentRuleListPerformedAction(webView, identifier, action, url);
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

- (void)_test_waitForDidFailProvisionalNavigation
{
    EXPECT_FALSE(self.navigationDelegate);

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    self.navigationDelegate = navigationDelegate.get();
    [navigationDelegate waitForDidFailProvisionalNavigation];

    self.navigationDelegate = nil;
}

- (void)_test_waitForDidFinishNavigationWithoutPresentationUpdate
{
    EXPECT_FALSE(self.navigationDelegate);

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    self.navigationDelegate = navigationDelegate.get();
    [navigationDelegate waitForDidFinishNavigation];

    self.navigationDelegate = nil;
}

- (void)_test_waitForDidFinishNavigationWithPreferences:(WKWebpagePreferences *)preferences
{
    EXPECT_FALSE(self.navigationDelegate);

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    self.navigationDelegate = navigationDelegate.get();
    [navigationDelegate waitForDidFinishNavigationWithPreferences:preferences];

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

- (void)_test_waitForWebContentProcessDidTerminate
{
    EXPECT_FALSE(self.navigationDelegate);

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    self.navigationDelegate = navigationDelegate.get();
    [navigationDelegate waitForWebContentProcessDidTerminate];

    self.navigationDelegate = nil;
}

@end
