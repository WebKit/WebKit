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

#if ENABLE_ACCESSIBILITY_ANIMATION_CONTROL

import Foundation
private import TestWebKitAPILibrary
import Testing
import WebKit
private import WebKit_Private.WKWebViewPrivate

import struct Foundation.URL
import struct Swift.String

private let animatedIframeHTML = """
    <svg width='10' height='10'><rect width='10' height='10'><animate attributeName='x' from='0' to='10' dur='1s' repeatCount='indefinite'/></rect></svg>
    <script>
    addEventListener('load', () => {
        const state = document.querySelector('svg').animationsPaused() ? 'paused' : 'playing'
        alert(`${document.visibilityState}, ${state}`)
    })
    </script>
    """

@MainActor
struct SiteIsolationAnimationControlTests {
    private let alerts = AlertRecorder()

    private func addCrossSiteIframe(to webView: WKWebView, from site: String) async throws -> String {
        try await webView.callJavaScript {
            "const iframe = document.createElement('iframe'); iframe.src = 'https://\(site)/animated'; document.body.appendChild(iframe)"
        }
        return try await alerts.nextAlert()
    }

    @Test(.timeLimit(.minutes(1)))
    func pauseAndPlayAllAnimationsApplyToCrossSiteIframesCreatedAfterwards() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                "<body>main frame text</body>"
            }

            Route("/animated", headerFields: ["Content-Type": "text/html"]) {
                animatedIframeHTML
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: .init(httpsProxyFor: serverConfiguration))
            defer { withExtendedLifetime(navigationDelegate) {} }
            webView.uiDelegate = alerts
            #if WTF_PLATFORM_MAC
            webView._windowOcclusionDetectionEnabled = false
            #endif
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            await webView._pauseAllAnimations()
            #expect(!webView._allowsAnyAnimationToPlay)
            let iframeStateAfterPause = try await addCrossSiteIframe(to: webView, from: "webkit.org")
            #expect(iframeStateAfterPause == "visible, paused")
            #expect(!webView._allowsAnyAnimationToPlay)

            await webView._playAllAnimations()
            #expect(webView._allowsAnyAnimationToPlay)
            let iframeStateAfterPlay = try await addCrossSiteIframe(to: webView, from: "apple.com")
            #expect(iframeStateAfterPlay == "visible, playing")
            #expect(webView._allowsAnyAnimationToPlay)
        }
    }
}

#endif // ENABLE_ACCESSIBILITY_ANIMATION_CONTROL
