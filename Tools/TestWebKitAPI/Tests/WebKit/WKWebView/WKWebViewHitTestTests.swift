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
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private._WKFrameTreeNode

import struct Foundation.URL
import struct Swift.String

@MainActor
private func webViewAndDelegate(
    configuration: WKWebViewConfiguration,
    siteIsolation: Bool
) -> (webView: TestWKWebView, navigationDelegate: TestNavigationDelegate) {
    let frame = CGRect(x: 0, y: 0, width: 400, height: 400)
    if siteIsolation {
        return siteIsolatedWebViewAndDelegate(configuration: configuration, frame: frame)
    }

    let navigationDelegate = TestNavigationDelegate()
    navigationDelegate.allowAnyTLSCertificate()

    let webView = TestWKWebView(frame: frame, configuration: configuration)
    webView.navigationDelegate = navigationDelegate

    return (webView, navigationDelegate)
}

@MainActor
private func makeServer() throws -> HTTPServer {
    try HTTPServer(protocol: .httpsProxy) {
        Route("/example") {
            "<body style='margin: 0'>"
                + "<a id=mainframelink href='/destination' title='Main frame title' style='display: block; width: 200px; height: 100px'>Main frame <b>link</b></a>"
                + "<iframe style='position: absolute; left: 0; top: 200px; width: 300px; height: 150px; border: none' src='https://webkit.org/iframe'></iframe>"
                + "</body>"
        }

        Route("/iframe") {
            "<body style='margin: 0'>"
                + "<a id=iframelink href='/iframedestination' style='display: block; width: 200px; height: 100px'>Iframe link</a>"
                + "</body>"
        }
    }
}

extension WKWebView {
    /// Returns the ID of the element a handle refers to, or of its parent element if it refers to a text node.
    fileprivate func elementID(for handle: WKJSHandle, contentWorld: WKContentWorld) async throws -> String {
        try await callJavaScript(returning: String.self, in: handle.sourceFrame, contentWorld: contentWorld, arguments: ["n": handle]) {
            "return (n.nodeType == Node.ELEMENT_NODE ? n : n.parentElement).id"
        }
    }

    /// Loads `https://example.com/example`, waits until its cross-origin iframe can be hit tested, and returns the iframe.
    fileprivate func loadExampleAndWaitForIframe(navigationDelegate: TestNavigationDelegate) async throws -> WKFrameInfo {
        load(URLRequest(url: try #require(URL(string: "https://example.com/example"))))
        try await navigationDelegate.waitForDidFinishNavigation()

        // Hit testing in main frame coordinates continues into the cross-origin iframe once it has loaded.
        try await waitForCondition("the iframe link to be hit tested") {
            let result = try? await hitTest(at: CGPoint(x: 50, y: 250), inCoordinateSpaceOf: nil, contentWorld: nil)
            return result?.absoluteLinkURL == URL(string: "https://webkit.org/iframedestination")
        }

        let mainFrame = try #require(await _frames())
        return try #require(mainFrame.childFrames.first).info
    }
}

@MainActor
struct WKWebViewHitTestTests {
    @Test(arguments: [false, true])
    func nilFrameAndContentWorldHitTestTheMainFrameInThePageWorld(siteIsolation: Bool) async throws {
        var server = try makeServer()
        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = webViewAndDelegate(
                configuration: .init(httpsProxyFor: serverConfiguration),
                siteIsolation: siteIsolation
            )
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/example"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            let result = try await webView.hitTest(at: CGPoint(x: 50, y: 50), inCoordinateSpaceOf: nil, contentWorld: nil)
            #expect(result.absoluteLinkURL == URL(string: "https://example.com/destination"))
            #expect(result.linkTitle == "Main frame title")
            #expect(result.linkLabel == "Main frame link")

            let handle = try #require(result.domNodeHandle)
            #expect(handle.contentWorld === WKContentWorld.page)
            #expect(handle.sourceFrame.isMainFrame)
            #expect(try await webView.elementID(for: handle, contentWorld: .page) == "mainframelink")

            // A point that isn't over a link has a handle, but no link information.
            let nonLinkResult = try await webView.hitTest(at: CGPoint(x: 350, y: 50), inCoordinateSpaceOf: nil, contentWorld: nil)
            #expect(nonLinkResult.domNodeHandle != nil)
            #expect(nonLinkResult.absoluteLinkURL == nil)
            #expect(nonLinkResult.linkTitle == nil)
            #expect(nonLinkResult.linkLabel == nil)
        }
    }

    @Test(arguments: [false, true])
    func explicitFrameAndContentWorld(siteIsolation: Bool) async throws {
        var server = try makeServer()
        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = webViewAndDelegate(
                configuration: .init(httpsProxyFor: serverConfiguration),
                siteIsolation: siteIsolation
            )
            let childFrame = try await webView.loadExampleAndWaitForIframe(navigationDelegate: navigationDelegate)
            let world = WKContentWorld.world(name: "WKWebViewHitTestTests")

            // The iframe link, with the point in the iframe's coordinate space.
            let result = try await webView.hitTest(at: CGPoint(x: 50, y: 50), inCoordinateSpaceOf: childFrame, contentWorld: world)
            #expect(result.absoluteLinkURL == URL(string: "https://webkit.org/iframedestination"))
            #expect(result.linkTitle == nil)
            #expect(result.linkLabel == "Iframe link")

            let handle = try #require(result.domNodeHandle)
            #expect(handle.contentWorld === world)
            #expect(!handle.sourceFrame.isMainFrame)
            #expect(handle.sourceFrame.securityOrigin.host == "webkit.org")
            #expect(try await webView.elementID(for: handle, contentWorld: world) == "iframelink")
        }
    }

    @Test
    func noNodeHitReturnsResultWithoutHandle() async throws {
        let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 400))
        try await webView.load(html: "<body>text</body>")

        // Without a document element, there is no node to hit.
        try await webView.callJavaScript { "document.documentElement.remove()" }

        let result = try await webView.hitTest(at: CGPoint(x: 10, y: 10), inCoordinateSpaceOf: nil, contentWorld: nil)
        #expect(result.domNodeHandle == nil)
        #expect(result.absoluteLinkURL == nil)
        #expect(result.linkTitle == nil)
        #expect(result.linkLabel == nil)
    }

    @Test
    func frameFromAnotherWebViewThrowsInvalidFrameTarget() async throws {
        let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 400))
        try await webView.load(html: "<body>first</body>")

        let otherWebView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 400))
        try await otherWebView.load(html: "<body>second</body>")
        let otherMainFrame = try #require(await otherWebView._frames()).info

        let error = await #expect(throws: WKError.self) {
            try await webView.hitTest(at: CGPoint(x: 10, y: 10), inCoordinateSpaceOf: otherMainFrame, contentWorld: nil)
        }
        #expect(error?.code == .javaScriptInvalidFrameTarget)
    }

    @Test
    func closingWebViewThrowsWebViewInvalidated() async throws {
        let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 400))
        try await webView.load(html: "<body>text</body>")

        let hitTest = Task {
            try await webView.hitTest(at: CGPoint(x: 10, y: 10), inCoordinateSpaceOf: nil, contentWorld: nil)
        }

        // Let the hit test start before closing the web view.
        await Task.yield()
        webView._close()

        let error = await #expect(throws: WKError.self) { try await hitTest.value }
        #expect(error?.code == .webViewInvalidated)
    }

    @Test
    func webContentProcessTerminationThrowsWebContentProcessTerminated() async throws {
        let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 400))
        try await webView.load(html: "<body>text</body>")

        // Keep the web content process busy, so that it can't reply to the hit test before it is killed.
        let busyLoop = Task {
            try await webView.callJavaScript { "for (;;) { }" }
        }
        let hitTest = Task {
            try await webView.hitTest(at: CGPoint(x: 10, y: 10), inCoordinateSpaceOf: nil, contentWorld: nil)
        }

        // Let the script and the hit test start before killing the web content process.
        await Task.yield()
        webView._killWebContentProcess()

        let error = await #expect(throws: WKError.self) { try await hitTest.value }
        #expect(error?.code == .webContentProcessTerminated)
        _ = try? await busyLoop.value
    }
}
