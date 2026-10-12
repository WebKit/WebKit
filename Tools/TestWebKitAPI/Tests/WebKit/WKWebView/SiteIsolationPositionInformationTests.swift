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
import Testing
private import UIKit
import WebKit
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private._WKFocusedElementInfo
private import WebKit_Private._WKFrameTreeNode
private import WebKit_Private._WKInputDelegate

import struct Foundation.URL
import struct Swift.String

// With site isolation, one web process can host several local roots of a page, such as the main frame and a same-site
// iframe nested in a cross-site iframe. Position information for a point comes from the local root under the point, so
// it must not use state, such as marked text, from a frame under another local root in the same process.

// WKContentView doesn't declare -pointIsNearMarkedText: in a header that Swift can import, so declare it here and send it
// to the content view through AnyObject.
@objc
private protocol MarkedTextHitTesting {
    func pointIsNearMarkedText(_ point: CGPoint) -> Bool
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
struct SiteIsolationPositionInformationTests {
    @Test
    func markedTextInMainFrameIsNotNearPointInNestedIframeSharingItsProcess() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                "<meta name='viewport' content='width=device-width, initial-scale=1'><body style='margin: 0'>"
                    + "<input style='position: absolute; left: 0; top: 0; width: 300px; height: 40px; border: none; padding: 0; "
                    + "font-size: 20px;'>"
                    + "<iframe style='position: absolute; left: 0; top: 300px; width: 400px; height: 300px; border: none;' "
                    + "src='https://domain2.com/middle'></iframe></body>"
            }

            Route("/middle") {
                "<body style='margin: 0'><iframe style='display: block; width: 400px; height: 300px; border: none;' "
                    + "src='https://example.com/inner'></iframe></body>"
            }

            Route("/inner") {
                "<body style='margin: 0'></body>"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(
                configuration: WKWebViewConfiguration(httpsProxyFor: serverConfiguration),
                frame: CGRect(x: 0, y: 0, width: 800, height: 600)
            )
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            try await waitForCondition("the innermost iframe to be in the frame tree") {
                let mainFrame: _WKFrameTreeNode? = await webView._frames()
                return mainFrame?.childFrames.first?.childFrames.first != nil
            }
            await webView.nextPresentationUpdate()

            let inputDelegate = InputSessionAllowingDelegate()
            webView._inputDelegate = inputDelegate
            try await webView.callJavaScript { "document.querySelector('input').focus()" }
            try await waitForCondition("focusing the input to start an input session") {
                inputDelegate.didAllowInputSession
            }

            let textInput = try #require(webView.textInputContentView)
            textInput.setMarkedText("hello", selectedRange: NSRange(location: 5, length: 0))
            try await waitForCondition("the marked text to be in the input") {
                try await webView.callJavaScript(returning: String.self) { "return document.querySelector('input').value" } == "hello"
            }

            // textInputContentView is the WKContentView, typed as UIView. Don't use wkContentView(), whose WKContentView
            // return type makes Swift reference the class symbol, which WebKit doesn't export.
            let contentView: AnyObject = textInput
            let isNearMarkedTextInMainFrame = contentView.pointIsNearMarkedText?(CGPoint(x: 20, y: 20))
            #expect(isNearMarkedTextInMainFrame == true)

            // The innermost iframe is another local root in the main frame's process, where this point is (20, 20).
            let isNearMarkedTextInNestedIframe = contentView.pointIsNearMarkedText?(CGPoint(x: 20, y: 320))
            #expect(isNearMarkedTextInNestedIframe == false)
        }
    }
}

#endif // WTF_PLATFORM_IOS_FAMILY
