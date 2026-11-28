// Copyright (C) 2019-2026 Apple Inc. All rights reserved.
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

#if HAVE_APP_SSO

import Foundation
import WebKit_Internal
import WebKit_Private
import WebKit_Private.WKNavigationDelegatePrivate

extension SOAuthorizationSession {
    // The pop-up session's hidden web view, which loads the response for the opener, and its delegate.
    final class SecretWebView {
        fileprivate var webView: WKWebView?
        fileprivate var delegate: SOAuthorizationSecretDelegate?
    }

    private func createSecretWebView(_ pageConfiguration: API.PageConfiguration, in secret: SecretWebView) -> WKWebView {
        WebKit.LogChannel.AppSSO().releaseLog
            .log("\(self, privacy: .public) \(#function, privacy: .public)")

        // swift-format-ignore: NeverForceUnwrap
        let configuration = pageConfiguration.wrapper() as! WKWebViewConfiguration
        // swift-format-ignore: NeverForceUnwrap
        let secretViewPreferences = configuration.preferences.copy() as! WKPreferences
        secretViewPreferences._isExtensibleSSOEnabled = false
        configuration.preferences = secretViewPreferences

        let secretWebView = WKWebView(frame: .zero, configuration: configuration)
        let secretDelegate = SOAuthorizationSecretDelegate(session: self)
        secretWebView.uiDelegate = secretDelegate
        secretWebView.navigationDelegate = secretDelegate
        precondition(secretWebView._protectedPage().get()?.preferences().isExtensibleSSOEnabled() == false)
        secret.webView = secretWebView
        secret.delegate = secretDelegate
        WebKit.LogChannel.AppSSO().releaseLog
            .log("\(self, privacy: .public) \(#function, privacy: .public): SecretWebView is created.")

        return secretWebView
    }

    // Creates the secret web view and gives its page to the opener.
    private func openSecretWebView(
        _ configuration: API.PageConfiguration,
        newPageCallback: WebKit.SOAuthorizationNewPageCallback,
        in secret: SecretWebView
    ) -> WKWebView {
        let secretWebView = createSecretWebView(configuration, in: secret)
        newPageCallback.pointee(consuming: secretWebView._protectedPage())
        return secretWebView
    }

    func abortPopUp(configuration: API.PageConfiguration, newPageCallback: WebKit.SOAuthorizationNewPageCallback, secret: SecretWebView) {
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): page=\(self.page.get().map { $0.logIdentifier() } ?? 0, privacy: .public)"
            )

        guard let page = page.get(), !page.isClosed() else {
            newPageCallback.pointee(consuming: WebKit.RefPtrWebPageProxy())
            return
        }

        let secretWebView = openSecretWebView(configuration, newPageCallback: newPageCallback, in: secret)
        secretWebView.evaluateJavaScript("window.close()", completionHandler: nil)
    }

    func completePopUp(
        _ httpResponse: HTTPURLResponse,
        body: Data,
        configuration: API.PageConfiguration,
        newPageCallback: WebKit.SOAuthorizationNewPageCallback,
        secret: SecretWebView
    ) {
        WebKit.LogChannel.AppSSO().releaseLog
            .log("\(self, privacy: .public) \(#function, privacy: .public): httpState=\(httpResponse.statusCode, privacy: .public)")

        guard httpResponse.statusCode == Int(WebCore.httpStatus200OK), let page = page.get(), !page.isClosed(),
            let baseURL = httpResponse.url
        else {
            resumeWebPath()
            return
        }

        let secretWebView = openSecretWebView(configuration, newPageCallback: newPageCallback, in: secret)
        secretWebView.load(body, mimeType: "text/html", characterEncodingName: "UTF-8", baseURL: baseURL)
    }

    fileprivate func close(_ webView: WKWebView) {
        WebKit.LogChannel.AppSSO().releaseLog
            .log("\(self, privacy: .public) \(#function, privacy: .public)")

        guard case .popUp(_, _, _, let secret) = kind, let secretWebView = secret.webView else {
            return
        }
        guard state == .completed, webView === secretWebView else {
            assertionFailure("A secret web view closed out of order")
            return
        }

        secret.webView = nil
        WebKit.LogChannel.AppSSO().releaseLog
            .log("\(self, privacy: .public) \(#function, privacy: .public): SecretWebView is cleaned.")
    }
}

// MARK: SOAuthorizationSecretDelegate

// The pop-up's secret web view's delegate: cancels the first navigation and allows the rest, and tells the session
// when the page closes or finishes loading.
@MainActor
private final class SOAuthorizationSecretDelegate: NSObject, WKNavigationDelegate, WKUIDelegate {
    private weak var session: SOAuthorizationSession?
    private var isFirstNavigation = true

    init(session: SOAuthorizationSession) {
        self.session = session
    }

    func webViewDidClose(_ webView: WKWebView) {
        session?.close(webView)
    }

    // FIXME: (rdar://193726515) The async form can run two decisions out of order, so this keeps the decision handler.
    func webView(
        _ webView: WKWebView,
        decidePolicyFor navigationAction: WKNavigationAction,
        decisionHandler: @escaping @MainActor (WKNavigationActionPolicy) -> Void
    ) {
        // FIXME<rdar://problem/48787839>: We should restrict the load to only substitute data.
        // Use the following heuristic as a workaround right now.
        // Ignore the first load in the secret window, which navigates to the authentication URL.
        if isFirstNavigation {
            isFirstNavigation = false
            decisionHandler(.cancel)
            return
        }

        decisionHandler(_WKNavigationActionPolicyAllowWithoutTryingAppLink)
    }

    func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) {
        session?.close(webView)
    }
}

#endif
