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

// FIXME: Fullscreen tests do not work when run on iOS because the test binary is not a real "app".
#if WTF_PLATFORM_MAC

private import TestWebKitAPILibrary
import Testing
import WebKit
private import WebKit_Private.WKWebViewPrivate

import struct Foundation.URL
import struct Swift.String

// The UI process defers the resize events of a page while it enters fullscreen, so that they come after the
// fullscreenchange event. With site isolation, every web process that hosts a frame of the page needs to defer them, not
// only the one that hosts the main frame. If the process of a cross-site iframe doesn't, the iframe's window fires
// resize as soon as fullscreen resizes the iframe, before the iframe's document fires fullscreenchange.

@MainActor
struct SiteIsolationFullscreenTests {
    @Test
    func fullscreenFromCrossOriginIframeFiresFullscreenChangeBeforeResize() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                """
                <iframe allowfullscreen style='width: 200px; height: 100px; border: none;' src='https://webkit.org/iframe'></iframe>
                """
            }

            Route("/iframe") {
                """
                <body>
                <div id='target'>Fullscreen target</div>
                <script>
                var events = [];
                addEventListener('resize', event => events.push(event.type));
                addEventListener('fullscreenchange', event => events.push(event.type));
                alert('iframe loaded');
                </script>
                </body>
                """
            }
        }

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            configuration.preferences.isElementFullscreenEnabled = true
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(
                configuration: configuration,
                frame: CGRect(x: 0, y: 0, width: 400, height: 300)
            )
            let alerts = AlertRecorder()
            webView.uiDelegate = alerts

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(try await alerts.nextAlert() == "iframe loaded")
            #expect(!webView._isInFullscreen)

            // Calling a script in a frame counts as a user gesture there, which requestFullscreen needs.
            let childFrame = try #require(await webView.firstChildFrame)
            try await webView.callJavaScript(in: childFrame) {
                """
                events.length = 0;
                document.getElementById('target').requestFullscreen();
                """
            }

            var events: [String] = []
            try await waitForCondition("the iframe to receive fullscreenchange and resize events", timeout: .seconds(15)) {
                events = try await webView.callJavaScript(returning: [String].self, in: childFrame) { "return events" }
                return events.contains("fullscreenchange") && events.contains("resize")
            }
            #expect(events.first == "fullscreenchange")
            #expect(webView._isInFullscreen)
        }
    }
}

#endif // WTF_PLATFORM_MAC
