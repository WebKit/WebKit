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
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private._WKTextPreview

import struct Foundation.URL
import struct Swift.String

@MainActor
struct SiteIsolationTextPreviewTests {
    @Test(.timeLimit(.minutes(1)), arguments: [false, true])
    func textPreviewOfElementInCrossSiteIframe(siteIsolationEnabled: Bool) async throws {
        try await withCrossSiteIframePage(siteIsolationEnabled: siteIsolationEnabled) { webView in
            let previews = await webView.textPreviews(forElementWithID: "element")
            #expect(previews.count == 1)
            #expect(previews.first?.previewImage != nil)
        }
    }

    @Test(.timeLimit(.minutes(1)), arguments: [false, true])
    func noTextPreviewForUnknownElementIDWithCrossSiteIframe(siteIsolationEnabled: Bool) async throws {
        try await withCrossSiteIframePage(siteIsolationEnabled: siteIsolationEnabled) { webView in
            #expect(await webView.textPreviews(forElementWithID: "missing").isEmpty)
        }
    }

    private func withCrossSiteIframePage(siteIsolationEnabled: Bool, _ body: (WKWebView) async throws -> Void) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                "<iframe src='https://webkit.org/iframe'></iframe>"
            }

            Route("/iframe") {
                "<p id='element'>Hello world!</p>"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = webViewAndDelegate(
                configuration: .init(httpsProxyFor: serverConfiguration),
                frame: CGRect(x: 0, y: 0, width: 800, height: 600),
                siteIsolationEnabled: siteIsolationEnabled
            )
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            try await waitForCondition("the iframe to parse the element") {
                guard let childFrame = await webView.firstChildFrame else {
                    return false
                }
                return try await webView.callJavaScript(returning: Bool.self, in: childFrame) {
                    "return document.getElementById('element') !== null"
                }
            }
            await webView.nextPresentationUpdate()

            try await body(webView)
        }
    }
}

extension WKWebView {
    fileprivate func textPreviews(forElementWithID id: String) async -> [_WKTextPreview] {
        await _textPreviewsForElement(withID: id) ?? []
    }
}

#endif // WTF_PLATFORM_MAC
