// Copyright (C) 2024 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

#if ENABLE_SWIFTUI

import Foundation
internal import WebKit_Private

private struct DefaultNavigationDecider: WebPage.NavigationDeciding {
}

@MainActor
final class WKNavigationDelegateAdapter: NSObject, WKNavigationDelegate {
    init(navigationDecider: (any WebPage.NavigationDeciding)?) {
        self.navigationDecider = navigationDecider ?? DefaultNavigationDecider()
    }

    weak var owner: WebPage? = nil

    private var navigationDecider: any WebPage.NavigationDeciding

    // MARK: Navigation progress reporting

    private func yieldNavigationProgress(kind: WebPage.NavigationEvent, cocoaNavigation: WKNavigation!) {
        let addEvent = { [weak owner] () -> Void in
            owner?.addNavigationEvent(.success(kind), for: cocoaNavigation)
        }

        if kind == .finished {
            // A presentation update is only guaranteed when a navigation has finished.
            owner?.backingWebView._do(afterNextPresentationUpdate: addEvent)
        } else {
            addEvent()
        }
    }

    private func failNavigationProgress(kind: some Error, cocoaNavigation: WKNavigation?) {
        owner?.addNavigationEvent(.failure(kind), for: cocoaNavigation)
    }

    func webView(_ webView: WKWebView, didStartProvisionalNavigation navigation: WKNavigation!) {
        yieldNavigationProgress(kind: .startedProvisionalNavigation, cocoaNavigation: navigation)
    }

    func webView(_ webView: WKWebView, didReceiveServerRedirectForProvisionalNavigation navigation: WKNavigation!) {
        yieldNavigationProgress(kind: .receivedServerRedirect, cocoaNavigation: navigation)
    }

    func webView(_ webView: WKWebView, didCommit navigation: WKNavigation!) {
        yieldNavigationProgress(kind: .committed, cocoaNavigation: navigation)
    }

    func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) {
        yieldNavigationProgress(kind: .finished, cocoaNavigation: navigation)
    }

    func webView(_ webView: WKWebView, didFailProvisionalNavigation navigation: WKNavigation!, withError error: any Error) {
        failNavigationProgress(kind: WebPage.NavigationError.failedProvisionalNavigation(error), cocoaNavigation: navigation)
    }

    func webView(_ webView: WKWebView, didFail navigation: WKNavigation!, withError error: any Error) {
        failNavigationProgress(kind: error, cocoaNavigation: navigation)
    }

    func webViewWebContentProcessDidTerminate(_ webView: WKWebView) {
        failNavigationProgress(kind: WebPage.NavigationError.webContentProcessTerminated, cocoaNavigation: nil)
    }

    // MARK: Back-forward list support

    // swift-format-ignore: NoLeadingUnderscores
    @objc(_webView:backForwardListItemAdded:removed:)
    func _webView(
        _ webView: WKWebView!,
        backForwardListItemAdded itemAdded: WKBackForwardListItem!,
        removed itemsRemoved: [WKBackForwardListItem]!
    ) {
        owner?.backForwardList = .init(webView.backForwardList)
    }

    // MARK: Navigation decisions

    func webView(
        _ webView: WKWebView,
        decidePolicyFor navigationAction: WKNavigationAction,
        preferences: WKWebpagePreferences
    ) async -> (WKNavigationActionPolicy, WKWebpagePreferences) {
        let convertedAction = WebPage.NavigationAction(navigationAction)
        var convertedPreferences = WebPage.NavigationPreferences(preferences)

        let result = await navigationDecider.decidePolicy(for: convertedAction, preferences: &convertedPreferences)
        let newPreferences = WKWebpagePreferences(convertedPreferences)

        return (result, newPreferences)
    }

    func webView(_ webView: WKWebView, decidePolicyFor navigationResponse: WKNavigationResponse) async -> WKNavigationResponsePolicy {
        let convertedResponse = WebPage.NavigationResponse(navigationResponse)
        return await navigationDecider.decidePolicy(for: convertedResponse)
    }

    func webView(
        _ webView: WKWebView,
        respondTo challenge: URLAuthenticationChallenge
    ) async -> (URLSession.AuthChallengeDisposition, URLCredential?) {
        await navigationDecider.decideAuthenticationChallengeDisposition(for: challenge)
    }
}

#endif
