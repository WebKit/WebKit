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
@_spi(WebKitAdditions_Testing) @_spi(Testing) @_spi(CrossImportOverlay) import WebKit
private import WebKit_Private.WKPreferencesPrivate
import SwiftUI
import struct Swift.String
private import struct TestWebKitAPILibrary.DOMRect
import Testing
private import TestWebKitAPILibrary
private import WebKit_Private._WKFrameTreeNode
private import Recap
private import TestWebKitAPILibrary.Helpers.cocoa.PDFTestHelpers

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

    @Test(arguments: [0, 100])
    func singleClickOnLinkInCrossSiteIframeShowsClickHighlight(mainFrameScrollOffset: Int) async throws {
        let iframeHTML = """
            <body style="margin: 0">
            <a id="target" href="#" style="display: block; position: absolute; left: 50px; top: 40px; width: 200px; height: 50px;">Link</a>
            <script>
            document.getElementById("target").addEventListener("click", event => {
                event.preventDefault();
                window.didClickTarget = true;
            });
            </script>
            </body>
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/iframe") {
                iframeHTML
            }
        }

        try await server.run { serverConfiguration in
            let iframeURL = serverConfiguration.localhostAddress.appending(path: "iframe")
            let html = """
                <body style="margin: 0; height: 3000px;">
                <iframe id="iframe" src="\(iframeURL.absoluteString)"
                    style="position: absolute; left: 100px; top: 250px; width: 600px; height: 200px; border: none;"></iframe>
                </body>
                """
            try await page.load(html: html, baseURL: serverConfiguration.address).wait()
            try await page.callJavaScript {
                "window.scrollTo(0, \(mainFrameScrollOffset));"
            }
            await page.waitForNextPresentationUpdate()

            let iframeInfo = try await #require(page.mainFrame?.childFrames.first?.info)
            let iframe = WebPage.FrameInfo(iframeInfo)

            let iframeRect = try await CGRect(page.callJavaScript(JavaScriptMessages.BoundingClientRect(elementID: "iframe")))
            let targetRectInIframe = try await CGRect(
                page.callJavaScript(JavaScriptMessages.BoundingClientRect(elementID: "target"), in: iframe)
            )
            let iframeScreenBounds = try await screenBounds(ofElementWithID: "iframe")
            let targetScreenBounds = targetRectInIframe.offsetBy(dx: iframeScreenBounds.minX, dy: iframeScreenBounds.minY)

            await recap.play { composer in
                composer._wk_click(at: targetScreenBounds.center, for: .seconds(0.05))
            }

            await page.waitForPendingMouseEvents()
            try await waitForCondition("The target is clicked") {
                try await page.callJavaScript(returning: Bool.self, in: iframe) {
                    "return !!window.didClickTarget;"
                }
            }
            await page.waitForNextPresentationUpdate()

            let expectedRect =
                targetRectInIframe
                .offsetBy(dx: iframeRect.minX, dy: iframeRect.minY + Self.topInset)
                .insetBy(dx: -4, dy: -4)
            #expect(page.clickHighlightRect == expectedRect)
        }
    }

    @Test(arguments: [false, true])
    func clickHandledAsHoverInIframeReportsHoverActivatedByClickBounds(crossSite: Bool) async throws {
        let iframeHTML = """
            <body style="margin: 0">
            <div id="source" style="position: absolute; left: 50px; top: 40px; width: 300px; height: 200px; background: linear-gradient(red, blue);"></div>
            <div id="preview" style="display: none; position: absolute; left: 400px; top: 40px; width: 150px; height: 150px; background: green;"></div>
            <script>
            window.pageEvents = [];
            for (const type of ["mousedown", "mouseup", "click"])
                document.addEventListener(type, event => window.pageEvents.push(event.type), true);
            // The click is handled as hover only if the content revealed on hover is actionable.
            document.getElementById("preview").addEventListener("click", () => { });
            // Unlike mouseenter, a mouseover listener makes #source the node responding to the click.
            document.getElementById("source").addEventListener("mouseover", () => {
                document.getElementById("preview").style.display = "block";
            });
            </script>
            </body>
            """

        try await clickElementInIframeAndExpectHoverActivatedByClickBounds(iframeHTML: iframeHTML, elementID: "source", crossSite: crossSite, expectsClick: false)
    }

    @Test(arguments: [false, true])
    func clickThatZoomsImageInPlaceInIframeReportsHoverActivatedByClickBounds(crossSite: Bool) async throws {
        let iframeHTML = """
            <body style="margin: 0">
            <div id="wrapper" style="position: absolute; left: 50px; top: 40px; width: 300px; height: 300px;">
                <img id="photo" src="photo.png" style="display: block; width: 300px; height: 300px;">
            </div>
            <script>
            window.pageEvents = [];
            for (const type of ["mousedown", "mouseup", "click"])
                document.addEventListener(type, event => window.pageEvents.push(event.type), true);
            document.getElementById("photo").addEventListener("click", () => {
                document.getElementById("wrapper").classList.add("zoomed");
            });
            </script>
            </body>
            """

        try await clickElementInIframeAndExpectHoverActivatedByClickBounds(iframeHTML: iframeHTML, elementID: "photo", crossSite: crossSite, expectsClick: true)
    }

    // Clicks the center of the element in the iframe, then expects the bounds of the element whose click was
    // handled as hover to be the element's bounds in view coordinates, which include the position of the iframe.
    private func clickElementInIframeAndExpectHoverActivatedByClickBounds(iframeHTML: String, elementID: String, crossSite: Bool, expectsClick: Bool) async throws {
        let imageURL = try #require(Bundle.testResources.url(forResource: "400x400-green", withExtension: "png"))
        let imageData = try Data(contentsOf: imageURL)

        var server = try HTTPServer(protocol: .http) {
            Route("/iframe") {
                iframeHTML
            }
            Route("/photo.png", headerFields: ["Content-Type": "image/png"]) {
                imageData
            }
        }

        page.backingWebView.configuration.preferences._contentChangeObserverEnabled = true

        try await server.run { serverConfiguration in
            // `127.0.0.1` and `localhost` are different sites, so only the latter is loaded in another process.
            let iframeBaseURL = crossSite ? serverConfiguration.localhostAddress : serverConfiguration.address
            let iframeURL = iframeBaseURL.appending(path: "iframe")
            let html = """
                <body style="margin: 0">
                <iframe id="iframe" src="\(iframeURL.absoluteString)"
                    style="position: absolute; left: 100px; top: 150px; width: 600px; height: 400px; border: none;"></iframe>
                </body>
                """
            try await page.load(html: html, baseURL: serverConfiguration.address).wait()
            await page.waitForNextPresentationUpdate()

            let iframeInfo = try await #require(page.mainFrame?.childFrames.first?.info)
            let iframe = WebPage.FrameInfo(iframeInfo)

            let iframeRect = try await CGRect(page.callJavaScript(JavaScriptMessages.BoundingClientRect(elementID: "iframe")))
            let elementRectInIframe = try await CGRect(
                page.callJavaScript(JavaScriptMessages.BoundingClientRect(elementID: elementID), in: iframe)
            )
            let iframeScreenBounds = try await screenBounds(ofElementWithID: "iframe")
            let elementScreenBounds = elementRectInIframe.offsetBy(dx: iframeScreenBounds.minX, dy: iframeScreenBounds.minY)

            await recap.play { composer in
                composer._wk_click(at: elementScreenBounds.center, for: .seconds(0.05))
            }
            await page.waitForPendingMouseEvents()
            await page.waitForNextPresentationUpdate()

            let events = try await page.callJavaScript(returning: [String].self, in: iframe) {
                "return window.pageEvents;"
            }
            try #require(events.contains("click") == expectsClick, "crossSite: \(crossSite), events in the iframe: \(events)")

            try await waitForCondition("The bounds of the element whose click was handled as hover are set") {
                page.hoverActivatedByClickBounds != .zero
            }

            let expectedBounds = elementRectInIframe.offsetBy(dx: iframeRect.minX, dy: iframeRect.minY + Self.topInset)
            #expect(page.hoverActivatedByClickBounds == expectedBounds)
        }
    }

    // Bounding client rects in the iframe are relative to its viewport, so they are offset by the position of the iframe.
    private func screenBoundsOfTextInIframe(_ text: String, in iframe: WebPage.FrameInfo) async throws -> CGRect {
        let range = try #require(Self.text.utf16Range(of: text))

        let iframeBounds = try await screenBounds(ofElementWithID: "iframe")
        let textBounds = try await page.callJavaScript(JavaScriptMessages.BoundingClientRect(in: "div", range: range), in: iframe)

        return CGRect(textBounds).offsetBy(dx: iframeBounds.minX, dy: iframeBounds.minY)
    }

    @Test(
        .bug("https://webkit.org/b/330327"),
        arguments: [false, true]
    )
    func draggingToExtendSelectionInPDFInIframeSelectsTextUnderPointer(crossSite: Bool) async throws {
        let pdfData = TestPDFBuilder.pdfDataWithLink()

        var server = try HTTPServer(protocol: .http) {
            Route("/test.pdf", headerFields: ["Content-Type": "application/pdf"]) {
                pdfData
            }
        }

        for feature in WKPreferences._features() where feature.key == "UnifiedPDFEnabled" {
            page.backingWebView.configuration.preferences._setEnabled(true, for: feature)
        }

        try await server.run { serverConfiguration in
            // The PDF page is 400pt wide, and the plugin lays it out with a 16px margin on each side, so a 432px wide
            // iframe shows the PDF at a scale of 1, with the page's top left corner at (16, 18) in the iframe.
            // `127.0.0.1` and `localhost` are different sites, so only the latter is loaded in another process.
            let pdfBaseURL = crossSite ? serverConfiguration.localhostAddress : serverConfiguration.address
            let pdfURL = pdfBaseURL.appending(path: "test.pdf")
            let html = """
                <body style="margin: 0">
                <iframe id="iframe" src="\(pdfURL.absoluteString)"
                    style="position: absolute; left: 100px; top: 150px; width: 432px; height: 300px; border: none;"></iframe>
                </body>
                """
            try await page.load(html: html, baseURL: serverConfiguration.address).wait()
            await page.waitForNextPresentationUpdate()

            let iframeInfo = try await #require(page.mainFrame?.childFrames.first?.info)
            let iframe = WebPage.FrameInfo(iframeInfo)
            try await waitForCondition("the PDF plugin to be created") {
                try await page.callJavaScript(returning: Bool.self, in: iframe) { "return !!document.querySelector('embed');" }
            }
            await page.waitForNextPresentationUpdate()

            // The PDF's only line of text, "Visit our website for details", is drawn in 24pt Helvetica starting 20pt
            // from the page's left edge, with its baseline 100pt from the page's top and bottom edges. The press is
            // over the "u" in "our" and the drag ends over the "o" in "for", so the selection must include "website"
            // but neither "Visit" nor "details". Were the drag's end point not converted to the iframe's coordinates,
            // it would land 100px to the right of and 150px below where it should, below the text.
            let iframeScreenBounds = try await screenBounds(ofElementWithID: "iframe")
            let pageOrigin = CGPoint(x: iframeScreenBounds.minX + 16, y: iframeScreenBounds.minY + 18)
            let textLeft = pageOrigin.x + 20
            // Halfway up the lowercase letters, which are about 12pt tall in 24pt Helvetica.
            let textCenterY = pageOrigin.y + 100 - 6
            // The "u" in "our" spans about 65pt to 79pt from the text's left edge, and the "o" in "for" about
            // 188pt to 201pt.
            let pressPoint = CGPoint(x: textLeft + 72, y: textCenterY)
            let dragEndPoint = CGPoint(x: textLeft + 195, y: textCenterY)

            await recap.play { composer in
                // Click-and-hold to begin a range selection, then drag to extend it.
                composer._wk_click(at: pressPoint, for: .seconds(0.5))
                composer.advanceTime(0.1)
                composer._wk_drag(withStart: pressPoint, end: dragEndPoint, duration: .seconds(0.5), release: false)
                composer.advanceTime(0.1)
                composer._wk_mouseUp()
            }

            await page.waitForPendingMouseEvents()
            await page.waitForNextPresentationUpdate()

            let selectedText = try await copySelectedText()
            #expect(selectedText.contains("website"))
            #expect(!selectedText.contains("Visit"))
            #expect(!selectedText.contains("details"))
        }
    }

    // The PDF plugin's selection is not exposed to script, so copy it to the general pasteboard.
    private func copySelectedText() async throws -> String {
        NSPasteboard.general.clearContents()
        let changeCount = NSPasteboard.general.changeCount

        NSApp.sendAction(#selector(NSText.copy(_:)), to: page.backingWebView, from: nil)

        try await waitForCondition("the selection to be copied") {
            await MainActor.run { NSPasteboard.general.changeCount != changeCount }
        }
        return NSPasteboard.general.string(forType: .string) ?? ""
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
