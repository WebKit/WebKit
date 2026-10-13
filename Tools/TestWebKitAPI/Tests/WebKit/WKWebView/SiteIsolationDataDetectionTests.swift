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

#if ENABLE_DATA_DETECTION && (WTF_PLATFORM_IOS || WTF_PLATFORM_VISION)

import CoreGraphics
private import TestWebKitAPILibrary
import Testing
import WebKit
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private._WKFrameTreeNode

import struct Foundation.URL
import struct Swift.String

// Detecting data, and removing the links it adds, act on every frame of the page. With site isolation, that includes
// frames hosted by other processes, such as cross-site iframes, and frames that come after them in the frame tree.

@MainActor
struct SiteIsolationDataDetectionTests {
    // The completion handlers of these calls only run once every web content process has replied, so give up if one
    // never does.
    @Test(.timeLimit(.minutes(1)))
    func detectAndRemoveDataInCrossOriginIframe() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                "<p>+1-234-567-8900</p><iframe src='https://webkit.org/iframe'></iframe>"
                    + "<iframe src='https://example.com/same-site-iframe'></iframe>"
            }

            Route("/iframe") {
                "<p>+1-234-567-8900</p>"
            }

            Route("/same-site-iframe") {
                "<p>+1-234-567-8900</p>"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(
                configuration: .init(httpsProxyFor: serverConfiguration),
                frame: CGRect(x: 0, y: 0, width: 800, height: 600)
            )
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            let mainFrame = try #require(await webView._frames())
            let childFrames: [_WKFrameTreeNode] = mainFrame.childFrames
            try #require(childFrames.count == 2)
            let crossSiteFrame: WKFrameInfo = childFrames[0].info
            let sameSiteFrame: WKFrameInfo = childFrames[1].info

            func linkCount(in frame: WKFrameInfo?) async throws -> Int {
                try await webView.callJavaScript(returning: Int.self, in: frame) {
                    "return document.querySelectorAll('a[x-apple-data-detectors=true]').length"
                }
            }
            let linkCounts = {
                try await [linkCount(in: nil), linkCount(in: crossSiteFrame), linkCount(in: sameSiteFrame)]
            }
            #expect(try await linkCounts() == [0, 0, 0])

            await webView._detectData(with: .phoneNumber)
            #expect(try await linkCounts() == [1, 1, 1])

            await webView._removeDataDetectedLinks()
            #expect(try await linkCounts() == [0, 0, 0])
        }
    }
}

#endif // ENABLE_DATA_DETECTION && (WTF_PLATFORM_IOS || WTF_PLATFORM_VISION)
