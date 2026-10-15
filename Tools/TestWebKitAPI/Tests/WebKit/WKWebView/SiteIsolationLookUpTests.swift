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

#if WTF_PLATFORM_MAC

import CoreGraphics
private import TestWebKitAPILibrary
import Testing
import WebKit
private import WebKit_Private.WKWebViewPrivateForTestingMac

import struct Foundation.URL
import struct Swift.String

// Look Up at a point starts with a hit test in the main frame's process. When the point is over a cross-origin iframe,
// that process can't see the iframe's text, so it must forward the point to the iframe's process, converted to the
// iframe's coordinates. If it doesn't, the hit test finds the <iframe> element, which has no text, and nothing is
// looked up.

@MainActor
struct SiteIsolationLookUpTests {
    @Test(.timeLimit(.minutes(1)))
    func lookUpAtPointOverCrossOriginIframeFindsWordInIframe() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                """
                <body style='margin: 0'>
                <iframe id='iframe' src='https://webkit.org/iframe'
                    style='position: absolute; left: 100px; top: 150px; width: 400px; height: 300px; border: none'></iframe>
                </body>
                """
            }

            // The spacer puts the word below the top of the iframe, so a point that isn't converted to the iframe's
            // coordinates misses it.
            Route("/iframe") {
                """
                <body style='margin: 0'>
                <div style='height: 120px'></div>
                <p style='margin: 0 40px; font-size: 30px'><span id='word'>dictionary</span></p>
                </body>
                """
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(
                configuration: .init(httpsProxyFor: serverConfiguration),
                frame: CGRect(x: 0, y: 0, width: 800, height: 600)
            )
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            await webView.nextPresentationUpdate()

            let childFrame = try #require(await webView.firstChildFrame)
            let iframeRect = try await webView.boundingClientRect(ofElementWithID: "iframe")
            // Bounding client rects in the iframe are relative to its viewport, so offset them by the iframe's position.
            let wordRect = try await webView.boundingClientRect(ofElementWithID: "word", in: childFrame)
                .offsetBy(dx: iframeRect.minX, dy: iframeRect.minY)

            var lookedUpText: String?
            var lookedUpRect = CGRect.null
            webView._setDidPerformDictionaryLookupHandler(forTesting: { text, textBoundingRect in
                lookedUpText = text
                lookedUpRect = textBoundingRect
            })

            webView._performDictionaryLookupAtLocation(forTesting: wordRect.center)
            try await waitForCondition("the word in the cross-origin iframe to be looked up") {
                lookedUpText != nil
            }

            #expect(lookedUpText == "dictionary")
            #expect(lookedUpRect.contains(wordRect.center))
            #expect(iframeRect.contains(lookedUpRect))
        }
    }
}

extension WKWebView {
    // The page isn't scrolled or zoomed, so the main frame's bounding client rects are in web view coordinates.
    fileprivate func boundingClientRect(ofElementWithID id: String, in frame: WKFrameInfo? = nil) async throws -> CGRect {
        let values = try await callJavaScript(returning: [Double].self, in: frame, arguments: ["id": id]) {
            """
            const rect = document.getElementById(id).getBoundingClientRect();
            return [rect.x, rect.y, rect.width, rect.height];
            """
        }
        try #require(values.count == 4)
        return CGRect(x: values[0], y: values[1], width: values[2], height: values[3])
    }
}

#endif // WTF_PLATFORM_MAC
