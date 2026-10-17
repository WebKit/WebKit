// Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if WTF_PLATFORM_IOS_FAMILY

private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
private import TestWebKitAPILibrary.Helpers.ios.AutocorrectionContextTestingSPI
import Testing
private import UIKit
import WebKit
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private._WKFocusedElementInfo
private import WebKit_Private._WKInputDelegate

import struct Swift.String

// A web process sends the UI process an autocorrection context just before it shows a JavaScript dialog, so that UIKit
// can get one without waiting on a web process that is blocked on the dialog. With site isolation, that web process
// can be the main frame's while the focused element is in an iframe in another process, and the context it sends has
// nothing in it. The UI process has to ignore contexts from processes other than the one with the focused frame, or
// the empty one would replace the focused iframe's, and UIKit would get no text around the insertion point.

@MainActor
private func keyboardUsesInputSystemUI() -> Bool {
    UIKeyboard.usesInputSystemUI()
}

/// The text of an autocorrection context that these tests check.
private struct AutocorrectionContextText {
    let contextBefore: String
    let selectedText: String

    init(_ context: UIWKAutocorrectionContext?) {
        contextBefore = context?.contextBeforeSelection ?? ""
        selectedText = context?.selectedText ?? ""
    }

    // The context from async text input wraps the UIKit one, which has the text.
    init(wrapping context: (any UIKitDocumentContextProviding)?) {
        let documentContext = context?._uikitDocumentContext
        contextBefore = documentContext?.contextBefore as? String ?? ""
        selectedText = documentContext?.selectedText as? String ?? ""
    }
}

/// Asks a web view's content view for its autocorrection context, the way UIKit does.
@MainActor
private func requestAutocorrectionContext(from webView: WKWebView) async throws -> AutocorrectionContextText {
    // textInputContentView is the WKContentView, typed as UIView. Don't use wkContentView(), whose WKContentView
    // return type makes Swift reference the class symbol, which WebKit doesn't export.
    let contentView: UIView = try #require(webView.textInputContentView)
    return await withCheckedContinuation { continuation in
        if webView.hasAsyncTextInput {
            contentView.requestTextContextForAutocorrection { context in
                continuation.resume(returning: AutocorrectionContextText(wrapping: context))
            }
        } else {
            contentView.requestAutocorrectionContext { context in
                continuation.resume(returning: AutocorrectionContextText(context))
            }
        }
    }
}

/// An input delegate that lets focusing an element start an input session, even without user interaction.
@MainActor
private final class InputSessionAllowingDelegate: NSObject, @preconcurrency _WKInputDelegate {
    var didAllowInputSession = false

    // swift-format-ignore: NoLeadingUnderscores
    func _webView(_ webView: WKWebView, decidePolicyForFocusedElement info: any _WKFocusedElementInfo) -> _WKFocusStartsInputSessionPolicy {
        didAllowInputSession = true
        return .allow
    }
}

@MainActor
struct SiteIsolationAutocorrectionTests {
    // With the out-of-process keyboard, UIKit doesn't ask the web process for autocorrection context.
    @Test(
        .timeLimit(.minutes(1)),
        .enabled("The out-of-process keyboard never asks for autocorrection context") { await !keyboardUsesInputSystemUI() }
    )
    func mainFrameAlertDoesNotReplaceFocusedCrossOriginIframeAutocorrectionContext() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                WebViewWithFocusedCrossOriginIframe.mainFrameHTML
            }

            Route("/iframe") {
                "<body contenteditable style='margin: 0; font-size: 20px;'>hello world</body>"
            }
        }

        try await server.run { serverConfiguration in
            let page = try await WebViewWithFocusedCrossOriginIframe(configuration: .init(httpsProxyFor: serverConfiguration))
            let webView = page.webView
            let childFrame = page.childFrame

            // A web view only holds its input delegate and UI delegate weakly, so keep them alive until the end of the test.
            let inputDelegate = InputSessionAllowingDelegate()
            let alerts = AlertRecorder()
            defer { withExtendedLifetime((inputDelegate, alerts)) {} }
            webView._inputDelegate = inputDelegate
            webView.uiDelegate = alerts

            try await webView.callJavaScript(in: childFrame) { "document.body.focus()" }
            try await waitForCondition("focusing the iframe's body to start an input session") {
                inputDelegate.didAllowInputSession
            }

            // The UI process only asks a web process for a new context after it sees the selection change.
            try await webView.setSelection(
                in: childFrame,
                script: "getSelection().setBaseAndExtent(document.body.firstChild, 6, document.body.firstChild, 11)",
                expecting: .isRange
            )

            try await webView.callJavaScript { "setTimeout(() => alert('main frame alert'), 0)" }
            #expect(try await alerts.nextAlert() == "main frame alert")
            // The main frame's process only answers once the alert has been dismissed.
            try await webView.callJavaScript { "" }

            let context = try await requestAutocorrectionContext(from: webView)
            #expect(context.selectedText == "world")
            #expect(context.contextBefore.hasPrefix("hello"))
        }
    }
}

#endif // WTF_PLATFORM_IOS_FAMILY
