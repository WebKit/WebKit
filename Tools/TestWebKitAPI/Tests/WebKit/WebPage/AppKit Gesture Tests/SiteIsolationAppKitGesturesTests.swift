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

#if HAVE_APPKIT_GESTURES_SUPPORT

import Foundation
import struct Foundation.URL
@_spi(WebKitAdditions_Testing) @_spi(Testing) import WebKit
import SwiftUI
import struct Swift.String
private import struct TestWebKitAPILibrary.DOMRect
import Testing
private import TestWebKitAPILibrary
private import WebKit_Private._WKFrameTreeNode
private import Recap

extension AppKitGesturesTests {
    // Unlike the other gesture suites, this one does not require user action for the editor state to include
    // post-layout data, since its tests select text in the cross-site iframe by script.
    @MainActor
    @Suite(.serialized, .timeLimit(.minutes(1)))
    final class SiteIsolation: AppKitGestureTestSuite {
        static let text = "Here's to the crazy ones."

        let recap = Recap.shared

        let page: WebPage = {
            var configuration = WebPage.Configuration()
            configuration.siteIsolationEnabled = true
            return WebPage(configuration: configuration)
        }()

        let windowHost: TestWindowHost

        init() async throws {
            let contentSize = NSSize(width: 800, height: 600)

            self.windowHost = TestWindowHost(size: contentSize) { [page] in
                WebView(page)
                    .webViewBackForwardNavigationGestures(.enabled)
            }

            await NSApp.waitForActivation()
        }
    }
}

extension AppKitGesturesTests.SiteIsolation {
    @Test(
        .bug("https://webkit.org/b/325693")
    )
    func clickingOnSelectedWordInCrossSiteIframeKeepsTextSelected() async throws {
        let iframeHTML = "<div id='div' style='font-size: 30px;'>\(Self.text)</div>"

        var server = try HTTPServer(protocol: .http) {
            Route("/iframe") {
                iframeHTML
            }
        }

        try await server.run { serverConfiguration in
            // `127.0.0.1` and `localhost` are different sites, so the iframe is loaded in another process.
            let iframeURL = serverConfiguration.localhostAddress.appending(path: "iframe")
            let html = """
                <iframe id="iframe" src="\(iframeURL.absoluteString)"
                    style="position: absolute; left: 100px; top: 150px; width: 600px; height: 200px; border: none;"></iframe>
                """
            try await page.load(html: html, baseURL: serverConfiguration.address).wait()
            await page.waitForNextPresentationUpdate()

            let iframeInfo = try await #require(page.mainFrame?.childFrames.first?.info)
            let iframe = WebPage.FrameInfo(iframeInfo)

            // The editor state comes from the process of the focused frame.
            try await page.callJavaScript {
                "document.getElementById('iframe').focus();"
            }

            let crazyRange = try #require(Self.text.utf16Range(of: "crazy"))
            let crazySelection = JavaScriptSelection.range(
                base: .init(in: "div", at: crazyRange.lowerBound),
                extent: .init(in: "div", at: crazyRange.upperBound)
            )

            let editorStateSnapshots = page.editorStateSnapshots()

            try await page.callJavaScript(JavaScriptMessages.SetSelection(crazySelection), in: iframe)

            _ = try await #require(
                editorStateSnapshots.first { @Sendable state in
                    state.selectionType == .range && state.postLayoutData != nil
                }
            )

            let crazyBounds = try await screenBoundsOfTextInIframe("crazy", in: iframe)

            await recap.play { composer in
                composer._wk_click(at: crazyBounds.center, for: .seconds(0.1))
            }

            await page.waitForPendingMouseEvents()
            await page.waitForNextPresentationUpdate()

            let newSelection = try await page.callJavaScript(JavaScriptMessages.GetSelection(), in: iframe)

            #expect(newSelection == crazySelection)
        }
    }

    // Bounding client rects in the iframe are relative to its viewport, so they are offset by the position of the iframe.
    private func screenBoundsOfTextInIframe(_ text: String, in iframe: WebPage.FrameInfo) async throws -> CGRect {
        let range = try #require(Self.text.utf16Range(of: text))

        let iframeBounds = try await screenBounds(ofElementWithID: "iframe")
        let textBounds = try await page.callJavaScript(JavaScriptMessages.BoundingClientRect(in: "div", range: range), in: iframe)

        return CGRect(textBounds).offsetBy(dx: iframeBounds.minX, dy: iframeBounds.minY)
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
