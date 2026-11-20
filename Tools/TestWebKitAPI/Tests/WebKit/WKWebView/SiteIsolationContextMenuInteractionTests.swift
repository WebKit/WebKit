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

#if WTF_PLATFORM_IOS_FAMILY && USE_UICONTEXTMENU

import CoreGraphics
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
import Testing
private import UIKit
import WebKit
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private.WKWebViewPrivateForTestingIOS
private import WebKit_Private._WKFrameTreeNode

import struct Foundation.URL
import struct Swift.String

@MainActor
struct SiteIsolationContextMenuInteractionTests {
    private static func contextMenuInteractionStates(in webView: WKWebView, frames: [WKFrameInfo]) async -> [Bool] {
        var states: [Bool] = []
        for frame in frames {
            states.append(await webView._hasActiveContextMenuInteraction(inFrame: frame))
        }
        return states
    }

    // Each web process keeps its own flag for an active context menu interaction. With site isolation, the cross-site
    // iframe's process has to get the begin and end messages too.
    @Test(.timeLimit(.minutes(1)), arguments: [false, true])
    func contextMenuInteractionReachesCrossSiteIframeProcess(siteIsolationEnabled: Bool) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                "<iframe src='https://webkit.org/iframe'></iframe>"
            }

            Route("/iframe") {
                "<body>iframe</body>"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = webViewAndDelegate(
                configuration: WKWebViewConfiguration(httpsProxyFor: serverConfiguration),
                frame: CGRect(x: 0, y: 0, width: 800, height: 600),
                siteIsolationEnabled: siteIsolationEnabled
            )
            defer { extendLifetime(navigationDelegate) }

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            let mainFrameNode: _WKFrameTreeNode? = await webView._frames()
            let mainFrame = try #require(mainFrameNode?.info)
            let childFrame = try #require(mainFrameNode?.childFrames.first?.info)

            try await waitForCondition("the iframe to finish loading") {
                try await webView.callJavaScript(returning: String.self, in: childFrame) { "return document.readyState" } == "complete"
            }

            // Without the iframe in its own process, the two cases would be the same.
            let expectedFrameTrees: FrameTreesInProcesses =
                siteIsolationEnabled
                ? [
                    .local("https://example.com", children: [.remote]),
                    .remote(children: [.local("https://webkit.org")]),
                ]
                : [
                    .local("https://example.com", children: [.local("https://webkit.org")])
                ]
            #expect(await webView.frameTreesInProcesses() == expectedFrameTrees)

            let frames = [mainFrame, childFrame]
            #expect(await Self.contextMenuInteractionStates(in: webView, frames: frames) == [false, false])

            // textInputContentView is the WKContentView, typed as UIView.
            let contentView: UIView = try #require(webView.textInputContentView)
            let delegate = try #require(contentView as? any UIContextMenuInteractionDelegate)
            let interaction = UIContextMenuInteraction(delegate: delegate)
            let configuration = UIContextMenuConfiguration(identifier: nil, previewProvider: nil, actionProvider: nil)

            delegate.contextMenuInteraction?(interaction, willDisplayMenuFor: configuration, animator: nil)
            #expect(await Self.contextMenuInteractionStates(in: webView, frames: frames) == [true, true])

            delegate.contextMenuInteraction?(interaction, willEndFor: configuration, animator: nil)
            #expect(await Self.contextMenuInteractionStates(in: webView, frames: frames) == [false, false])
        }
    }
}

#endif // WTF_PLATFORM_IOS_FAMILY && USE_UICONTEXTMENU
