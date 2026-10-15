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
@_spi(Testing) import WebKit
private import WebKit_Private.WKFrameInfoPrivate
private import WebKit_Private.WKProcessPoolPrivate
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private._WKFrameTreeNode
private import WebKit_Private._WKProcessPoolConfiguration

import struct Foundation.URL
import struct Swift.String

// With site isolation, a navigation that changes the site of a frame moves the frame to another process. These tests
// check which processes host each frame after such navigations.

@MainActor
struct SiteIsolationNavigationTests {
    // Cross-site frames share a process by default, so these tests use a frame that is same-site with the main frame,
    // and is in its process. Each navigation goes to apple.com in another process, then a redirect brings it back.
    private static func beforeUnloadCountingServer() throws -> HTTPServer {
        try HTTPServer(protocol: .httpsProxy) {
            Route("/example") {
                "<iframe src='/frame'></iframe>"
            }

            Route("/frame") {
                "<script>onbeforeunload = () => { sessionStorage.beforeUnloadCount = Number(sessionStorage.beforeUnloadCount || 0) + 1 }</script>"
            }

            Route("/leave", statusCode: 302, headerFields: ["Location": "https://apple.com/return"]) {
                "redirecting..."
            }

            Route("/return", statusCode: 302, headerFields: ["Location": "https://example.com/destination"]) {
                "redirecting..."
            }

            Route("/leave-twice", statusCode: 302, headerFields: ["Location": "https://apple.com/return-and-leave"]) {
                "redirecting..."
            }

            Route("/return-and-leave", statusCode: 302, headerFields: ["Location": "https://example.com/leave"]) {
                "redirecting..."
            }

            Route("/leave-for-no-content", statusCode: 302, headerFields: ["Location": "https://apple.com/return-to-no-content"]) {
                "redirecting..."
            }

            Route("/return-to-no-content", statusCode: 302, headerFields: ["Location": "https://example.com/no-content"]) {
                "redirecting..."
            }

            Route("/no-content", statusCode: 204, headerFields: ["Set-Cookie": "returned=1"]) {
                ""
            }

            Route("/destination") {
                "<script>alert(`beforeunload ${sessionStorage.beforeUnloadCount}`)</script>"
            }
        }
    }

    @Test(arguments: [
        "navigation.navigate('/leave')",
        "location.href = '/leave'",
        "navigation.navigate('/leave-twice')",
        "location.href = '/leave-twice'",
    ])
    func beforeUnloadFiresOnceWhenNavigationReturnsToFrameProcess(navigationScript: String) async throws {
        var server = try Self.beforeUnloadCountingServer()

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: .init(httpsProxyFor: serverConfiguration))
            let uiDelegate = BeforeUnloadAlertRecorder()
            webView.uiDelegate = uiDelegate

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/example"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            let childFrame = try #require(await webView.firstChildFrame)
            try await webView.callJavaScript(in: childFrame) { "setTimeout(() => { \(navigationScript) }, 0)" }

            let alert = try await uiDelegate.nextAlert()
            #expect(alert == "beforeunload 1", "\(navigationScript)")
            #expect(await webView.frameTreesInProcesses() == [.local("https://example.com", children: [.local("https://example.com")])])
        }
    }

    @Test(arguments: [
        "navigation.navigate('/leave-for-no-content')",
        "location.href = '/leave-for-no-content'",
    ])
    func beforeUnloadFiresForNextNavigationAfterNavigationReturnsWithoutCommitting(navigationScript: String) async throws {
        var server = try Self.beforeUnloadCountingServer()

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: .init(httpsProxyFor: serverConfiguration))
            let uiDelegate = BeforeUnloadAlertRecorder()
            webView.uiDelegate = uiDelegate

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/example"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            let childFrame = try #require(await webView.firstChildFrame)
            try await webView.callJavaScript(in: childFrame) { "setTimeout(() => { \(navigationScript) }, 0)" }
            try await waitForCondition("the navigation to return and get its 204 response") {
                try await webView.callJavaScript(returning: Bool.self, in: childFrame) { "return document.cookie.includes('returned')" }
            }

            try await webView.callJavaScript(in: childFrame) { "setTimeout(() => { location.href = '/destination' }, 0)" }

            let alert = try await uiDelegate.nextAlert()
            #expect(alert == "beforeunload 2", "\(navigationScript)")
        }
    }

    @Test
    func grandchildIframeSameOriginAsGrandparent() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/example") {
                "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"
            }

            Route("/webkit") {
                "<iframe src='https://example.com/example_grandchild'></iframe>"
            }

            Route("/example_grandchild") {
                "<script>alert('grandchild loaded successfully')</script>"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: .init(httpsProxyFor: serverConfiguration))
            let alerts = AlertRecorder()
            webView.uiDelegate = alerts

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/example"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let alert = try await alerts.nextAlert()
            #expect(alert == "grandchild loaded successfully")

            #expect(
                await webView.frameTreesInProcesses() == [
                    .local("https://example.com", children: [.remote(children: [.local("https://example.com")])]),
                    .remote(children: [.local("https://webkit.org", children: [.remote])]),
                ]
            )
        }
    }

    @Test
    func parentNavigatingCrossOriginIframeToSameOrigin() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/example") {
                """
                <iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>
                <script>onload = () => { document.getElementById('webkit_frame').src = 'https://example.com/example_subframe' }</script>
                """
            }

            Route("/example_subframe") {
                "<script>onload = () => { alert('done') }</script>"
            }

            Route("/webkit") {
                "hi"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: .init(httpsProxyFor: serverConfiguration))
            let alerts = AlertRecorder()
            webView.uiDelegate = alerts

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/example"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let alert = try await alerts.nextAlert()
            #expect(alert == "done")

            let mainFrame = try #require(await webView._frames())
            let childFrame = try #require(mainFrame.childFrames.first)
            #expect(mainFrame.info._processIdentifier != 0)
            #expect(childFrame.info._processIdentifier == mainFrame.info._processIdentifier)
            #expect(mainFrame.info.securityOrigin.host == "example.com")
            #expect(childFrame.info.securityOrigin.host == "example.com")

            #expect(
                await webView.frameTreesInProcesses() == [
                    .local("https://example.com", children: [.local("https://example.com")])
                ]
            )
        }
    }

    @Test
    func shutDownFrameProcessesAfterNavigation() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/example") {
                "<iframe src='https://webkit.org/webkit'></iframe>"
            }

            Route("/webkit") {
                "hello"
            }

            Route("/apple") {
                "hello"
            }
        }

        try await server.run { serverConfiguration in
            // With the back/forward cache, the iframe's process would stay alive to host the iframe in the cached page.
            let processPoolConfiguration = _WKProcessPoolConfiguration()
            processPoolConfiguration.pageCacheEnabled = false
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            configuration.processPool = try #require(WKProcessPool._processPool(with: processPoolConfiguration))
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: configuration)

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/example"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            let frameTrees = await webView.frameTreesInProcesses()
            #expect(
                frameTrees == [
                    .local("https://example.com", children: [.remote]),
                    .remote(children: [.local("https://webkit.org")]),
                ]
            )
            let iframeProcessIdentifier = try #require(frameTrees.processIdentifier(hosting: "https://webkit.org"))

            webView.load(URLRequest(url: try #require(URL(string: "https://apple.com/apple"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(await webView.frameTreesInProcesses() == [.local("https://apple.com")])

            try await waitForCondition("the iframe's process to exit") {
                !isProcessRunning(iframeProcessIdentifier)
            }
        }
    }

    #if ENABLE_SWIFTUI

    @Test
    func childNavigatingToNewDomain() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/example") {
                "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"
            }

            Route("/example_subframe") {
                "<script>alert('done')</script>"
            }

            Route("/webkit") {
                "<script>window.location='https://foo.com/example_subframe'</script>"
            }
        }

        try await server.run { serverConfiguration in
            var configuration = WebPage.Configuration(httpsProxyFor: serverConfiguration)
            configuration.siteIsolationEnabled = true

            let alerts = AlertRecorder()
            let page = WebPage(
                configuration: configuration,
                navigationDecider: NavigationDeciderAllowingAnyTLSCertificate(),
                dialogPresenter: alerts
            )

            try await page.load(URL(string: "https://example.com/example")).wait()
            let alert = try await alerts.nextAlert()
            #expect(alert == "done")

            let mainFrame = try #require(await page.mainFrame)
            let childFrame = try #require(mainFrame.childFrames.first)
            #expect(mainFrame.info._processIdentifier != 0)
            #expect(childFrame.info._processIdentifier != 0)
            #expect(childFrame.info._processIdentifier != mainFrame.info._processIdentifier)
            #expect(mainFrame.info.securityOrigin.host == "example.com")
            #expect(childFrame.info.securityOrigin.host == "foo.com")

            #expect(
                await page.frameTreesInProcesses() == [
                    .local("https://example.com", children: [.remote]),
                    .remote(children: [.local("https://foo.com")]),
                ]
            )
        }
    }

    #endif // ENABLE_SWIFTUI
}
