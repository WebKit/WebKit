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

#if ENABLE_DRAG_SUPPORT && WTF_PLATFORM_MAC

import CoreGraphics
private import TestWebKitAPILibrary
import Testing
import WebKit

import struct Foundation.URL
import struct Swift.String

@MainActor
struct SiteIsolationDragAndDropTests {
    private static let recordDragEnterAndLeave = """
        <script>
        var events = [];
        addEventListener('dragenter', event => events.push(event.type));
        addEventListener('dragleave', event => events.push(event.type));
        </script>
        """

    private static let pointInMainFrame = CGPoint(x: 100, y: 100)
    private static let pointInIframe = CGPoint(x: 300, y: 100)

    @Test(.timeLimit(.minutes(1)), arguments: [true, false])
    func dragLeavingWebViewFromCrossOriginIframeFiresDragleaveInIframe(siteIsolationEnabled: Bool) async throws {
        try await withMainFrameAndCrossOriginIframe(siteIsolationEnabled: siteIsolationEnabled) { webView, childFrame in
            let drag = ExternalDrag(over: webView, string: "dragged text")
            drag.enter(at: Self.pointInIframe)
            try await waitForCondition("the iframe to get dragenter") {
                try await webView.dragEvents(in: childFrame) == ["dragenter"]
            }

            drag.exit()
            #expect(try await webView.dragEvents(in: childFrame) == ["dragenter", "dragleave"])
        }
    }

    @Test(.timeLimit(.minutes(1)), arguments: [true, false])
    func dragMovingBetweenMainFrameAndCrossOriginIframeFiresDragleaveInFrameItLeaves(
        siteIsolationEnabled: Bool
    ) async throws {
        try await withMainFrameAndCrossOriginIframe(siteIsolationEnabled: siteIsolationEnabled) { webView, childFrame in
            let drag = ExternalDrag(over: webView, string: "dragged text")
            drag.enter(at: Self.pointInMainFrame)
            #expect(try await webView.dragEvents() == ["dragenter"])

            drag.move(to: Self.pointInIframe)
            try await waitForCondition("the iframe to get dragenter") {
                try await webView.dragEvents(in: childFrame) == ["dragenter"]
            }
            #expect(try await webView.dragEvents() == ["dragenter", "dragleave"])

            drag.move(to: Self.pointInMainFrame)
            // The UI process handles the reply to the move, which tells the iframe's process that the drag left, before
            // the reply to this script.
            #expect(try await webView.dragEvents() == ["dragenter", "dragleave", "dragenter"])
            #expect(try await webView.dragEvents(in: childFrame) == ["dragenter", "dragleave"])

            drag.exit()
            #expect(try await webView.dragEvents() == ["dragenter", "dragleave", "dragenter", "dragleave"])
        }
    }

    @Test(.timeLimit(.minutes(1)), arguments: [true, false])
    func dragMovedIntoCrossOriginIframeAndBackBeforeEitherMoveIsHandledEndsOverMainFrame(
        siteIsolationEnabled: Bool
    ) async throws {
        try await withMainFrameAndCrossOriginIframe(siteIsolationEnabled: siteIsolationEnabled) { webView, childFrame in
            let drag = ExternalDrag(over: webView, string: "dragged text")
            drag.enter(at: Self.pointInMainFrame)
            #expect(try await webView.dragEvents() == ["dragenter"])

            drag.move(to: Self.pointInIframe)
            drag.move(to: Self.pointInMainFrame)
            try await waitForCondition("the iframe to get dragleave") {
                try await webView.dragEvents(in: childFrame) == ["dragenter", "dragleave"]
            }
            #expect(try await webView.dragEvents().last == "dragenter")
        }
    }

    private func withMainFrameAndCrossOriginIframe(
        siteIsolationEnabled: Bool,
        _ body: (WKWebView, WKFrameInfo) async throws -> Void
    ) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                """
                <body style='margin: 0'>
                <iframe src='https://webkit.org/iframe'
                    style='position: absolute; left: 200px; top: 0; width: 200px; height: 200px; border: none'></iframe>
                \(Self.recordDragEnterAndLeave)
                </body>
                """
            }

            Route("/iframe") {
                """
                <body style='margin: 0'>
                \(Self.recordDragEnterAndLeave)
                <script>alert('iframe loaded')</script>
                </body>
                """
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = webViewAndDelegate(
                configuration: .init(httpsProxyFor: serverConfiguration),
                frame: CGRect(x: 0, y: 0, width: 400, height: 200),
                siteIsolationEnabled: siteIsolationEnabled
            )
            let alerts = AlertRecorder()
            webView.uiDelegate = alerts

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(try await alerts.nextAlert() == "iframe loaded")
            await webView.nextPresentationUpdate()

            let childFrame = try #require(await webView.firstChildFrame)
            try await body(webView, childFrame)
        }
    }
}

extension WKWebView {
    fileprivate func dragEvents(in frame: WKFrameInfo? = nil) async throws -> [String] {
        try await callJavaScript(returning: [String].self, in: frame) { "return events" }
    }
}

#endif // ENABLE_DRAG_SUPPORT && WTF_PLATFORM_MAC
