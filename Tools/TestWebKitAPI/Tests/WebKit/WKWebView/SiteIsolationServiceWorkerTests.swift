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

private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestNavigationDelegate
import Testing
import WebKit

import struct Foundation.URL
import struct Swift.String

// With site isolation, a cross-site iframe that a service worker controls is a client in a different process than the
// main frame. These tests check that the service worker can still act on such a client.

@MainActor
struct SiteIsolationServiceWorkerTests {
    // A web view only holds its delegates weakly, so the test holds them for as long as it runs. The navigation
    // delegate has to accept the server's certificate again when the service worker navigates the iframe.
    private let navigationDelegate = TestNavigationDelegate()
    private let alerts = AlertRecorder()

    init() {
        navigationDelegate.allowAnyTLSCertificate()
    }

    @Test
    func windowClientNavigateResolvesToNavigatedClientInControlledCrossSiteIframe() async throws {
        let frameHTML = """
            <script>
            navigator.serviceWorker.onmessage = (event) => alert(event.data);
            if (navigator.serviceWorker.controller) {
                if (!location.search)
                    navigator.serviceWorker.controller.postMessage('navigate');
            } else {
                navigator.serviceWorker.register('/sw.js').then((registration) => {
                    const worker = registration.installing;
                    worker.onstatechange = () => { if (worker.state == 'activated') alert('activated') };
                });
            }
            </script>
            """

        let serviceWorkerScript = """
            self.addEventListener('message', (event) => {
                event.waitUntil(
                    event.source.navigate('/frame?navigated')
                        .then((client) => client ? client.url : 'null', () => 'failed')
                        .then(async (result) => {
                            for (const client of await self.clients.matchAll())
                                client.postMessage(result);
                        }));
            });
            """

        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                "<iframe src='https://webkit.org/frame'></iframe>"
            }

            Route("/frame") {
                frameHTML
            }

            Route("/frame?navigated") {
                frameHTML
            }

            Route("/sw.js", headerFields: ["Content-Type": "application/javascript"]) {
                serviceWorkerScript
            }
        }

        try await server.run { serverConfiguration in
            let (webView, _) = siteIsolatedWebViewAndDelegate(configuration: .init(httpsProxyFor: serverConfiguration))
            webView.navigationDelegate = navigationDelegate
            webView.uiDelegate = alerts

            let mainFrameRequest = URLRequest(url: try #require(URL(string: "https://example.com/mainframe")))
            webView.load(mainFrameRequest)
            let activatedAlert = try await alerts.nextAlert()
            #expect(activatedAlert == "activated")

            // The iframe is now controlled by the service worker, which navigates it and reports the resulting client's URL.
            webView.load(mainFrameRequest)
            let navigatedAlert = try await alerts.nextAlert()
            #expect(navigatedAlert == "https://webkit.org/frame?navigated")
        }
    }
}
