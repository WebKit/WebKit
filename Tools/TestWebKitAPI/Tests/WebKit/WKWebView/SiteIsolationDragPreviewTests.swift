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

#if ENABLE_DRAG_SUPPORT && USE_BROWSERENGINEKIT

import CoreGraphics
import Foundation
private import TestWebKitAPILibrary
import Testing
import UIKit
import WebKit

import struct Foundation.URL
import struct Swift.String

@MainActor
@Suite(.timeLimit(.minutes(1)))
struct SiteIsolationDragPreviewTests {
    @Test(arguments: ["https://example.com/inner", "https://webkit.org/inner"])
    func liftPreviewForImageInOffsetIframe(iframeURL: String) async throws {
        let iframeOrigin = CGPoint(x: 100, y: 150)
        let imageSize = CGSize(width: 200, height: 100)
        let image = makePNGData(size: imageSize, color: .red).base64EncodedString()

        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/main") {
                """
                <meta name='viewport' content='width=device-width, initial-scale=1'>
                <body style='margin: 0'>
                <iframe src='\(iframeURL)' style='position: absolute; left: \(iframeOrigin.x)px; top: \(iframeOrigin.y)px;
                    width: 400px; height: 400px; border: none'></iframe>
                </body>
                """
            }

            Route("/inner") {
                """
                <body style='margin: 0'>
                <img id='image' src='data:image/png;base64,\(image)'
                    style='position: absolute; left: 0; top: 0; width: \(imageSize.width)px; height: \(imageSize.height)px'>
                </body>
                """
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(
                configuration: .init(httpsProxyFor: serverConfiguration),
                frame: CGRect(x: 0, y: 0, width: 800, height: 600)
            )
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/main"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            let iframe = try #require(await webView.firstChildFrame)
            try await webView.callJavaScript(in: iframe) { "await document.getElementById('image').decode()" }
            await webView.nextPresentationUpdate()

            let pointInImage = CGPoint(x: iframeOrigin.x + 50, y: iframeOrigin.y + 50)
            let preview = try await webView.liftPreviewOfDrag(startingAt: pointInImage)
            #expect(preview.view.frame == CGRect(origin: iframeOrigin, size: imageSize))
        }
    }
}

#endif // ENABLE_DRAG_SUPPORT && USE_BROWSERENGINEKIT
