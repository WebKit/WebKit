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

@implementation TestNavigationDelegate

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

- (void)_webView:(WKWebView *)webView didFailProvisionalLoadWithRequest:(NSURLRequest *)request inFrame:(WKFrameInfo *)frame withError:(NSError *)error
{
    if (_didFailProvisionalLoadWithRequestInFrameWithError)
        _didFailProvisionalLoadWithRequestInFrameWithError(webView, request, frame, error);
}

- (void)_webView:(WKWebView *)webView navigation:(WKNavigation *)navigation didFailProvisionalLoadInSubframe:(WKFrameInfo *)subframe withError:(NSError *)error
{
    if (_didFailProvisionalLoadInSubframeWithError)
        _didFailProvisionalLoadInSubframeWithError(webView, subframe, error);
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    if (_didFinishNavigation)
        _didFinishNavigation(webView, navigation);
}

- (void)_webView:(WKWebView *)webView navigation:(WKNavigation *)navigation didSameDocumentNavigation:(_WKSameDocumentNavigationType)navigationType
{
    if (_didSameDocumentNavigation)
        _didSameDocumentNavigation(webView, navigation);
}

- (void)_webView:(WKWebView *)webView webContentProcessDidTerminateWithReason:(_WKProcessTerminationReason)reason
{
    if (_webContentProcessDidTerminate)
        _webContentProcessDidTerminate(webView, reason);
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

- (void)_webView:(WKWebView *)webView didGeneratePageLoadTiming:(_WKPageLoadTiming *)timing
{
    if (_didGeneratePageLoadTiming)
        _didGeneratePageLoadTiming(timing);
}

- (void)webView:(WKWebView *)webView willSubmitForm:(WKFormInfo *)formInfo submissionHandler:(void (^)(void))submissionHandler
{
    if (_willSubmitForm)
        _willSubmitForm(formInfo);
    submissionHandler();
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

- (void)waitForDidSameDocumentNavigation
{
    EXPECT_FALSE(self.didSameDocumentNavigation);

    __block bool finished = false;
    self.didSameDocumentNavigation = ^(WKWebView *, WKNavigation *) {
        finished = true;
    };

    TestWebKitAPI::Util::run(&finished);

    self.didSameDocumentNavigation = nil;
}

- (_WKProcessTerminationReason)waitForWebContentProcessDidTerminate
{
    EXPECT_FALSE(self.webContentProcessDidTerminate);

    __block bool crashed = false;
    __block _WKProcessTerminationReason crashReason;
    self.webContentProcessDidTerminate = ^(WKWebView *, _WKProcessTerminationReason reason) {
        crashed = true;
        crashReason = reason;
    };

    TestWebKitAPI::Util::run(&crashed);

    self.webContentProcessDidTerminate = nil;
    return crashReason;
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
    __block RetainPtr<NSError> navigationError;
    self.didFailProvisionalNavigation = ^(WKWebView *, WKNavigation *, NSError *error) {
        navigationError = error;
        finished = true;
    };

    TestWebKitAPI::Util::run(&finished);

    self.didFailProvisionalNavigation = nil;
    return navigationError.autorelease();
}

- (void)_webView:(WKWebView *)webView contentRuleListWithIdentifier:(NSString *)identifier performedAction:(_WKContentRuleListAction *)action forURL:(NSURL *)url
{
    if (_contentRuleListPerformedAction)
        _contentRuleListPerformedAction(webView, identifier, action, url);
}

- (void)_webView:(WKWebView *)webView didChangeLookalikeCharactersFromURL:(NSURL *)originalURL toURL:(NSURL *)adjustedURL
{
    if (_didChangeLookalikeCharactersFromURL)
        _didChangeLookalikeCharactersFromURL(webView, originalURL, adjustedURL);
}

- (void)_webView:(WKWebView *)webView didPromptForStorageAccess:(NSString *)topFrameDomain forSubFrameDomain:(NSString *)subFrameDomain forQuirk:(BOOL)hasQuirk
{
    if (_didPromptForStorageAccess)
        _didPromptForStorageAccess(webView, topFrameDomain, subFrameDomain, hasQuirk);
}

- (void)webView:(WKWebView *)webView navigationAction:(WKNavigationAction *)navigationAction didBecomeDownload:(WKDownload *)download
{
    if (_navigationActionDidBecomeDownload)
        _navigationActionDidBecomeDownload(navigationAction, download);
}

- (void)webView:(WKWebView *)webView navigationResponse:(WKNavigationResponse *)navigationResponse didBecomeDownload:(WKDownload *)download
{
    if (_navigationResponseDidBecomeDownload)
        _navigationResponseDidBecomeDownload(navigationResponse, download);
}

@end

@implementation WKWebView (TestWebKitAPIExtras)

- (void)_test_waitForDidStartProvisionalNavigation
{
    auto *oldNavigationDelegate = self.navigationDelegate;

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    self.navigationDelegate = navigationDelegate.get();
    [navigationDelegate waitForDidStartProvisionalNavigation];

    self.navigationDelegate = oldNavigationDelegate;
}

- (void)_test_waitForDidFailProvisionalNavigation
{
    auto *oldNavigationDelegate = self.navigationDelegate;

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    self.navigationDelegate = navigationDelegate.get();
    [navigationDelegate waitForDidFailProvisionalNavigation];

    self.navigationDelegate = oldNavigationDelegate;
}

- (void)_test_waitForDidFinishNavigationWithoutPresentationUpdate
{
    auto *oldNavigationDelegate = self.navigationDelegate;

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    self.navigationDelegate = navigationDelegate.get();
    [navigationDelegate waitForDidFinishNavigation];

    self.navigationDelegate = oldNavigationDelegate;
}

- (void)_test_waitForDidFinishNavigationWithPreferences:(WKWebpagePreferences *)preferences
{
    auto *oldNavigationDelegate = self.navigationDelegate;

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    self.navigationDelegate = navigationDelegate.get();
    [navigationDelegate waitForDidFinishNavigationWithPreferences:preferences];

    self.navigationDelegate = oldNavigationDelegate;
}

- (void)_test_waitForDidFinishNavigation
{
    auto *oldNavigationDelegate = self.navigationDelegate;

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    self.navigationDelegate = navigationDelegate.get();
    [navigationDelegate waitForDidFinishNavigation];

    self.navigationDelegate = oldNavigationDelegate;

#if PLATFORM(IOS_FAMILY)
    __block bool presentationUpdateHappened = false;
    [self _doAfterNextPresentationUpdateWithoutWaitingForAnimatedResizeForTesting:^{
        presentationUpdateHappened = true;
    }];
    TestWebKitAPI::Util::run(&presentationUpdateHappened);
#endif
}

- (void)_test_waitForDidSameDocumentNavigation
{
    auto *oldNavigationDelegate = self.navigationDelegate;

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    self.navigationDelegate = navigationDelegate.get();
    [navigationDelegate waitForDidSameDocumentNavigation];

    self.navigationDelegate = oldNavigationDelegate;
}

- (void)_test_waitForDidFinishNavigationWhileIgnoringSSLErrors
{
    auto *oldNavigationDelegate = self.navigationDelegate;

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    navigationDelegate.get().didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    };
    self.navigationDelegate = navigationDelegate.get();
    [navigationDelegate waitForDidFinishNavigation];

    self.navigationDelegate = oldNavigationDelegate;

#if PLATFORM(IOS_FAMILY)
    __block bool presentationUpdateHappened = false;
    [self _doAfterNextPresentationUpdateWithoutWaitingForAnimatedResizeForTesting:^{
        presentationUpdateHappened = true;
    }];
    TestWebKitAPI::Util::run(&presentationUpdateHappened);
#endif
}

- (_WKProcessTerminationReason)_test_waitForWebContentProcessDidTerminate
{
    auto *oldNavigationDelegate = self.navigationDelegate;

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    self.navigationDelegate = navigationDelegate.get();
    auto result = [navigationDelegate waitForWebContentProcessDidTerminate];

    self.navigationDelegate = oldNavigationDelegate;

    return result;
}

@end
