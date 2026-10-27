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

import CoreGraphics
private import TestWebKitAPILibrary
import Testing
import WebKit
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private.WKWebViewPrivateForTestingIOS
private import WebKit_Private._WKActivatedElementInfo

import struct Foundation.URL
import struct Swift.String

@MainActor
@Suite(.timeLimit(.minutes(1)))
struct SiteIsolationLinkIndicatorTests {
    @Test(arguments: ["https://webkit.org/iframe", "https://example.com/iframe"])
    func linkIndicatorForLinkInIframeIsAroundLinkInMainFrameCoordinates(iframeURL: String) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                """
                <body style='margin: 0'>
                <iframe id='iframe' src='\(iframeURL)'
                    style='display: block; margin: 150px 0 0 100px; width: 400px; height: 300px; border: none'></iframe>
                </body>
                """
            }

            Route("/iframe") {
                """
                <!DOCTYPE html>
                <body style='margin: 0'>
                <a id='link' href='https://webkit.org/destination'
                    style='display: block; margin: 50px; width: 100px'><div style='height: 50px; background-color: blue'></div></a>
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
            let linkRect = try await webView.boundingClientRect(ofElementWithID: "link", in: childFrame)
                .offsetBy(dx: iframeRect.minX, dy: iframeRect.minY)

            let elementInfo = try #require(await webView._requestActivatedElement(atPosition: linkRect.center))
            #expect(elementInfo.type == .link)
            #expect(elementInfo.boundingRect == linkRect)

            let linkIndicatorRect = await webView._linkIndicatorBoundingRectAtPoint(forTesting: linkRect.center)
            #expect(linkIndicatorRect == linkRect)
        }
    }
}

extension WKWebView {
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

#endif // WTF_PLATFORM_IOS_FAMILY
