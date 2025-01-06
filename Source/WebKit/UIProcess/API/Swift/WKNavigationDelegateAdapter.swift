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

#if ENABLE_SWIFTUI && compiler(>=6.0)

import Foundation
internal import WebKit_Private

fileprivate struct DefaultNavigationDecider: NavigationDeciding {
}

@MainActor
final class WKNavigationDelegateAdapter: NSObject, WKNavigationDelegate {
    init(
        navigationProgressContinuation: AsyncStream<WebPage_v0.NavigationEvent>.Continuation,
        downloadProgressContinuation: AsyncStream<WebPage_v0.DownloadEvent>.Continuation,
        navigationDecider: (any NavigationDeciding)?
    ) {
        self.navigationProgressContinuation = navigationProgressContinuation
        self.downloadProgressContinuation = downloadProgressContinuation
        self.navigationDecider = navigationDecider ?? DefaultNavigationDecider()
    }

    weak var owner: WebPage_v0? = nil

    private let navigationProgressContinuation: AsyncStream<WebPage_v0.NavigationEvent>.Continuation
    private let downloadProgressContinuation: AsyncStream<WebPage_v0.DownloadEvent>.Continuation
    private let navigationDecider: any NavigationDeciding

    // MARK: Navigation progress reporting

    private func yieldNavigationProgress(kind: WebPage_v0.NavigationEvent.Kind, cocoaNavigation: WKNavigation!) {
        let navigation = WebPage_v0.NavigationEvent(kind: kind, navigationID: .init(cocoaNavigation))
        navigationProgressContinuation.yield(navigation)
    }

    private func yieldDownloadProgress(kind: WebPage_v0.DownloadEvent.Kind, download: WKDownload) {
        let downloadEvent = WebPage_v0.DownloadEvent(kind: kind, download: .init(download))
        downloadProgressContinuation.yield(downloadEvent)
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
        yieldNavigationProgress(kind: .failedProvisionalNavigation(underlyingError: error), cocoaNavigation: navigation)
    }

    func webView(_ webView: WKWebView, didFail navigation: WKNavigation!, withError error: any Error) {
        yieldNavigationProgress(kind: .failed(underlyingError: error), cocoaNavigation: navigation)
    }

    // MARK: Downloads

    func webView(_ webView: WKWebView, navigationAction: WKNavigationAction, didBecome download: WKDownload) {
        download.delegate = owner?.backingDownloadDelegate
        yieldDownloadProgress(kind: .started, download: download)
    }

    func webView(_ webView: WKWebView, navigationResponse: WKNavigationResponse, didBecome download: WKDownload) {
        download.delegate = owner?.backingDownloadDelegate
        yieldDownloadProgress(kind: .started, download: download)
    }

    @objc(_webView:contextMenuDidCreateDownload:)
    func _webView(_ webView: WKWebView!, contextMenuDidCreateDownload download: WKDownload!) {
        download.delegate = owner?.backingDownloadDelegate
        yieldDownloadProgress(kind: .started, download: download)
    }

    // MARK: Back-forward list support

    @objc(_webView:backForwardListItemAdded:removed:)
    func _webView(_ webView: WKWebView!, backForwardListItemAdded itemAdded: WKBackForwardListItem!, removed itemsRemoved: [WKBackForwardListItem]!) {
        owner?.backForwardList = .init(webView.backForwardList)
    }

    // MARK: Navigation decisions

    func webView(_ webView: WKWebView, decidePolicyFor navigationAction: WKNavigationAction, preferences: WKWebpagePreferences) async -> (WKNavigationActionPolicy, WKWebpagePreferences) {
        let convertedAction = WebPage_v0.NavigationAction(navigationAction)
        var convertedPreferences = WebPage_v0.NavigationPreferences(preferences)

        let result = await navigationDecider.decidePolicy(for: convertedAction, preferences: &convertedPreferences)
        let newPreferences = WKWebpagePreferences(convertedPreferences)

        return (result, newPreferences)
    }

    func webView(_ webView: WKWebView, decidePolicyFor navigationResponse: WKNavigationResponse) async -> WKNavigationResponsePolicy {
        let convertedResponse = WebPage_v0.NavigationResponse(navigationResponse)
        return await navigationDecider.decidePolicy(for: convertedResponse)
    }

    func webView(_ webView: WKWebView, respondTo challenge: URLAuthenticationChallenge) async -> (URLSession.AuthChallengeDisposition, URLCredential?) {
        await navigationDecider.decideAuthenticationChallengeDisposition(for: challenge)
    }
}

#endif
