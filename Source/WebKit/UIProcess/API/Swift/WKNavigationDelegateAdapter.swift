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

final class WKNavigationDelegateAdapter: NSObject, WKNavigationDelegate {
    private let navigationProgressContinuation: AsyncStream<WebPage_v0.NavigationEvent>.Continuation

    init(navigationProgressContinuation: AsyncStream<WebPage_v0.NavigationEvent>.Continuation) {
        self.navigationProgressContinuation = navigationProgressContinuation
    }

    // MARK: Navigation progress reporting

    private func yieldNavigationProgress(kind: WebPage_v0.NavigationEvent.Kind, cocoaNavigation: WKNavigation!) {
        let navigation = WebPage_v0.NavigationEvent(kind: kind, navigationID: .init(cocoaNavigation))
        navigationProgressContinuation.yield(navigation)
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
}

#endif
