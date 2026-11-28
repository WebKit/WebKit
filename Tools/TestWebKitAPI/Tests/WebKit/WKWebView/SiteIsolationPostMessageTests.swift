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
import Testing
import WebKit

import struct Foundation.URL
import struct Swift.String

// With site isolation, a cross-site iframe is hosted in another process, so window.postMessage() and MessagePort
// messages between it and its parent go through the UI process. These tests check that such messages, and the ports
// they transfer, arrive in order and respect the target origin.

@MainActor
struct SiteIsolationPostMessageTests {
    private static let pageWithChildFrameTrees: FrameTreesInProcesses = [
        .local("https://example.com", children: [.remote]),
        .remote(children: [.local("https://webkit.org")]),
    ]

    #if hasAttribute(diagnose)
    @diagnose(DeprecatedDeclaration, as: ignored, reason: "Intentionally uses a custom process pool")
    #endif
    @Test
    func postMessageWithMessagePorts() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/example") {
                """
                <script>
                    const channel = new MessageChannel();
                    channel.port1.onmessage = (event) => { alert('parent frame received ' + event.data); };
                    onload = () => {
                        document.getElementById('webkit_frame').contentWindow.postMessage('ping', '*', [channel.port2]);
                    };
                </script>
                <iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>
                """
            }

            Route("/webkit") {
                """
                <script>
                    window.addEventListener('message', (event) => {
                        event.ports[0].postMessage('got port and message ' + event.data);
                    });
                </script>
                """
            }

            Route("/example2") {
                """
                <script>
                    onload = () => {
                        const channel = new MessageChannel();
                        document.getElementById('webkit_frame').contentWindow.postMessage('ping', '*', [channel.port2]);
                        channel.port1.postMessage('sent message after sending port');
                    };
                </script>
                <iframe id='webkit_frame' src='https://webkit.org/webkit2'></iframe>
                """
            }

            Route("/webkit2") {
                """
                <script>
                    window.addEventListener('message', (event) => {
                        event.ports[0].onmessage = (e) => { alert('port received message ' + e.data); };
                    });
                </script>
                """
            }
        }

        try await server.run { serverConfiguration in
            // Disable the back/forward cache so the same-site navigation does not cache the page holding the port.
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            configuration.processPool = .withBackForwardCacheDisabled()
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: configuration)
            let alerts = AlertRecorder()
            webView.uiDelegate = alerts

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/example"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            var alert = try await alerts.nextAlert()
            #expect(alert == "parent frame received got port and message ping")

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/example2"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            alert = try await alerts.nextAlert()
            #expect(alert == "port received message sent message after sending port")
        }
    }

    @Test
    func postMessageWithMessagePortsFromIframeToMainFrame() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/example") {
                """
                <script>
                    window.addEventListener('message', (event) => {
                        alert('main frame received ' + event.data + ' with ' + event.ports.length + ' ports');
                        if (event.ports.length)
                            event.ports[0].postMessage('pong');
                    });
                </script>
                <iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>
                """
            }

            Route("/webkit") {
                """
                <script>
                    onload = () => {
                        const channel = new MessageChannel();
                        channel.port1.onmessage = (event) => {
                            parent.postMessage('port message ' + event.data, '*');
                        };
                        parent.postMessage('ping', '*', [channel.port2]);
                        parent.postMessage('message sent after transferring port', '*');
                        parent.postMessage('another message sent after transferring port', '*');
                    };
                </script>
                """
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: .init(httpsProxyFor: serverConfiguration))
            let alerts = AlertRecorder()
            webView.uiDelegate = alerts

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/example"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            // The message transferring the port must be delivered before the messages sent right after it.
            var alert = try await alerts.nextAlert()
            #expect(alert == "main frame received ping with 1 ports")
            alert = try await alerts.nextAlert()
            #expect(alert == "main frame received message sent after transferring port with 0 ports")
            alert = try await alerts.nextAlert()
            #expect(alert == "main frame received another message sent after transferring port with 0 ports")
            alert = try await alerts.nextAlert()
            #expect(alert == "main frame received port message pong with 0 ports")

            #expect(await webView.frameTreesInProcesses() == Self.pageWithChildFrameTrees)
        }
    }

    @Test
    func postMessageWithNotAllowedTargetOrigin() async throws {
        // Both messages go from the main frame to the same iframe, so they arrive in the order they were sent. Had the
        // first one been delivered despite its target origin, its alert would come before the second one's.
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/example") {
                """
                <script>
                    onload = () => {
                        const childWindow = document.getElementById('webkit_frame').contentWindow;
                        childWindow.postMessage('ping', 'https://foo.org');
                        childWindow.postMessage('allowed', 'https://webkit.org');
                    };
                </script>
                <iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>
                """
            }

            Route("/webkit") {
                """
                <script>
                    window.addEventListener('message', (event) => {
                        alert('child frame received ' + event.data);
                    });
                </script>
                """
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: .init(httpsProxyFor: serverConfiguration))
            let alerts = AlertRecorder()
            webView.uiDelegate = alerts

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/example"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let alert = try await alerts.nextAlert()
            #expect(alert == "child frame received allowed")
            #expect(alerts.pendingAlerts.isEmpty)

            #expect(await webView.frameTreesInProcesses() == Self.pageWithChildFrameTrees)
        }
    }

    @Test
    func postMessageToIframeWithOpaqueOrigin() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/example") {
                """
                <script>
                    onload = () => {
                        try {
                            document.getElementById('webkit_frame').contentWindow.postMessage('ping', 'data:');
                        } catch (error) {
                            alert(error);
                        }
                    };
                </script>
                <iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>
                """
            }

            Route("/webkit") {
                """
                <script>
                    window.addEventListener('message', (event) => {
                        alert('child frame received ' + event.data);
                    });
                </script>
                """
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: .init(httpsProxyFor: serverConfiguration))
            let alerts = AlertRecorder()
            webView.uiDelegate = alerts

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/example"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let alert = try await alerts.nextAlert()
            #expect(alert == "SyntaxError: The string did not match the expected pattern.")

            #expect(await webView.frameTreesInProcesses() == Self.pageWithChildFrameTrees)
        }
    }
}
