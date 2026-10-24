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

#if ENABLE_UNIFIED_PDF && WTF_PLATFORM_MAC

import CoreGraphics
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.PDFTestHelpers
import Testing
import WebKit
private import WebKit_Private.WKWebViewPrivateForTestingMac

import struct Foundation.URL
import struct Swift.String

@MainActor
struct PDFIframeLookUpTests {
    @Test(.timeLimit(.minutes(1)), arguments: [false, true])
    func lookUpAtPointOverPDFInCrossOriginIframeFindsWordInPDF(siteIsolationEnabled: Bool) async throws {
        let pdfData = TestPDFBuilder.pdfDataWithLink()
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                """
                <body style='margin: 0'>
                <iframe src='https://webkit.org/test.pdf'
                    style='position: absolute; left: 100px; top: 120px; width: 400px; height: 300px; border: none'></iframe>
                </body>
                """
            }

            Route("/test.pdf", headerFields: ["Content-Type": "application/pdf"]) {
                pdfData
            }
        }

        try await server.run { serverConfiguration in
            let configuration = TestPDFBuilder.configurationForUnifiedPDF(withHUDEnabled: false)
            configuration.websiteDataStore = WKWebViewConfiguration(httpsProxyFor: serverConfiguration).websiteDataStore
            let (webView, navigationDelegate) = webViewAndDelegate(
                configuration: configuration,
                frame: CGRect(x: 0, y: 0, width: 800, height: 600),
                siteIsolationEnabled: siteIsolationEnabled
            )
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            let childFrame = try #require(await webView.firstChildFrame)
            let iframeRect = try await webView.boundingClientRectOfIframe()
            let pointOverWord = try await webView.pdfLinkRect(in: childFrame)
                .offsetBy(dx: iframeRect.minX, dy: iframeRect.minY)
                .center
            await webView.nextPresentationUpdate()

            var lookedUpText: String?
            var lookedUpRect = CGRect.null
            webView._setDidPerformDictionaryLookupHandler(forTesting: { text, textBoundingRect in
                lookedUpText = text
                lookedUpRect = textBoundingRect
            })

            webView._performDictionaryLookupAtLocation(forTesting: pointOverWord)
            try await waitForCondition("the word in the PDF to be looked up") {
                lookedUpText != nil
            }

            #expect(lookedUpText == "website")
            #expect(lookedUpRect.contains(pointOverWord))
        }
    }
}

extension WKWebView {
    fileprivate func boundingClientRectOfIframe() async throws -> CGRect {
        let values = try await callJavaScript(returning: [Double].self) {
            """
            const rect = document.querySelector('iframe').getBoundingClientRect();
            return [rect.x, rect.y, rect.width, rect.height];
            """
        }
        try #require(values.count == 4)
        return CGRect(x: values[0], y: values[1], width: values[2], height: values[3])
    }

    fileprivate func pdfLinkRect(in frame: WKFrameInfo) async throws -> CGRect {
        var values: [Double] = []
        try await waitForCondition("the PDF to lay out its link") {
            values = try await callJavaScript(returning: [Double].self, in: frame) {
                """
                const plugin = document.querySelector('embed');
                const link = plugin && internals.pdfAnnotationRectsForTesting(plugin)[0];
                if (!link)
                    return [];
                const pluginRect = plugin.getBoundingClientRect();
                return [pluginRect.x + link.x, pluginRect.y + link.y, link.width, link.height];
                """
            }
            return values.count == 4
        }
        return CGRect(x: values[0], y: values[1], width: values[2], height: values[3])
    }
}

#endif // ENABLE_UNIFIED_PDF && WTF_PLATFORM_MAC
