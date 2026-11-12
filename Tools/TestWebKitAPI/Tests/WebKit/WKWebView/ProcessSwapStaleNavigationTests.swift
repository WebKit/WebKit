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
private import WebKit_Private.WKWebViewPrivateForTesting

import struct Foundation.URL
import struct Swift.String

// A navigation to another site loads in a provisional page, in another web content process, until it commits. These
// tests start another navigation in that time, and check that the web view ends up where that navigation took it.
@MainActor
struct ProcessSwapStaleNavigationTests {
    // Going back to the same site is a traversal in the committed process, which moves the back/forward list's cursor
    // as it starts.
    @Test(arguments: [false, true])
    func goingBackWhileCrossSiteNavigationWaitsForItsResponse(siteIsolationEnabled: Bool) async throws {
        try await goBackWhileCrossSiteNavigationWaitsForItsResponse(
            to: "https://example.com/first",
            siteIsolationEnabled: siteIsolationEnabled
        )
    }

    // Going back to another site loads in a provisional page of its own, whose process moves the cursor as well.
    @Test(arguments: [false, true])
    func goingBackToAnotherSiteWhileCrossSiteNavigationWaitsForItsResponse(siteIsolationEnabled: Bool) async throws {
        try await goBackWhileCrossSiteNavigationWaitsForItsResponse(
            to: "https://apple.com/first",
            siteIsolationEnabled: siteIsolationEnabled
        )
    }

    #if hasAttribute(diagnose)
    @diagnose(DeprecatedDeclaration, as: ignored, reason: "Intentionally uses a custom process pool")
    #endif
    private func goBackWhileCrossSiteNavigationWaitsForItsResponse(to backURL: String, siteIsolationEnabled: Bool) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/first") { "first" }
            Route("/second") { "second" }
            Route("/target") { "target" }
        }

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            configuration.processPool = WKProcessPool.processSwapping()

            let (webView, navigationDelegate) = webViewAndDelegate(configuration: configuration, siteIsolationEnabled: siteIsolationEnabled)

            webView.load(URLRequest(url: try #require(URL(string: backURL))))
            try await navigationDelegate.waitForDidFinishNavigation()
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/second"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            var finishedURLs: [URL] = []
            navigationDelegate.didFinishNavigation = { webView, _ in
                if let url = webView.url {
                    finishedURLs.append(url)
                }
            }

            // Holding the response's policy decision keeps the navigation in its provisional page.
            var decideTargetResponsePolicy: ((WKNavigationResponsePolicy) -> Void)?
            navigationDelegate.decidePolicyForNavigationResponse = { navigationResponse, decisionHandler in
                guard navigationResponse.response.url?.path == "/target" else {
                    decisionHandler(.allow)
                    return
                }

                decideTargetResponsePolicy = decisionHandler
            }

            webView.load(URLRequest(url: try #require(URL(string: "https://webkit.org/target"))))
            try await waitForCondition("The navigation to /target waits for its response policy") {
                decideTargetResponsePolicy != nil
            }
            #expect(webView._provisionalWebProcessIdentifier != 0)
            #expect(webView._committedURL?.path == "/second")

            webView.goBack()
            try await waitForCondition("The back navigation finishes") { !finishedURLs.isEmpty }

            #expect(finishedURLs.map(\.absoluteString) == [backURL])
            #expect(webView._provisionalWebProcessIdentifier == 0)
            #expect(webView.url?.absoluteString == backURL)
            #expect(webView.backForwardList.currentItem?.url.absoluteString == backURL)
            #expect(webView.backForwardList.forwardItem?.url.path == "/second")
            #expect(try await webView.callJavaScript(returning: String.self) { "return document.body.textContent" } == "first")

            decideTargetResponsePolicy?(.cancel)
            extendLifetime(navigationDelegate)
        }
    }

    @Test(arguments: [false, true])
    func stoppingLoadingBeforeCOOPResponseArrivesKeepsCurrentPage(siteIsolationEnabled: Bool) async throws {
        try await interruptCOOPNavigationWhileItsProcessIsBusy(siteIsolationEnabled: siteIsolationEnabled) { webView in
            webView.stopLoading()
        } check: { webView, recorder in
            #expect(recorder.swapDecisions == ["cancelLoad"])
            try await waitForCondition("The navigation to /coop ends") {
                !recorder.failedProvisionalPaths.isEmpty || !recorder.finishedPaths.isEmpty
            }

            #expect(recorder.failedProvisionalPaths == ["/coop"])
            #expect(recorder.committedPaths.isEmpty)
            #expect(webView._committedURL?.path == "/second")
            #expect(webView.backForwardList.currentItem?.url.path == "/second")
            #expect(try await webView.callJavaScript(returning: String.self) { "return document.body.textContent" } == "second")
        }
    }

    @Test(arguments: [false, true])
    func loadingAnotherPageBeforeCOOPResponseArrivesLoadsThatPage(siteIsolationEnabled: Bool) async throws {
        try await interruptCOOPNavigationWhileItsProcessIsBusy(siteIsolationEnabled: siteIsolationEnabled) { webView in
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/third"))))
        } check: { webView, recorder in
            try await expectLoadOfThirdPage(in: webView, recorder: recorder, after: "/second")
        }
    }

    @Test
    func loadingAnotherPageBeforeCOOPResponseArrivesLoadsThatPageWhileCrossSiteIframeNavigates() async throws {
        var iframe: WKFrameInfo?
        try await interruptCOOPNavigationWhileItsProcessIsBusy(from: "/second-with-iframe", siteIsolationEnabled: true) { webView in
            iframe = try #require(await webView.firstChildFrame)
        } interrupt: { webView in
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/third"))))
            try await webView.callJavaScript(in: try #require(iframe)) { "history.replaceState(null, '', '/frame-replaced')" }
        } check: { webView, recorder in
            try await expectLoadOfThirdPage(in: webView, recorder: recorder, after: "/second-with-iframe")
        }
    }

    @Test(arguments: [false, true])
    func goingBackBeforeCOOPResponseArrivesGoesBack(siteIsolationEnabled: Bool) async throws {
        try await interruptCOOPNavigationWhileItsProcessIsBusy(siteIsolationEnabled: siteIsolationEnabled) { webView in
            webView.goBack()
        } check: { webView, recorder in
            #expect(recorder.swapDecisions == ["cancelLoad"])
            try await waitForCondition("A navigation finishes") { !recorder.finishedPaths.isEmpty }

            #expect(recorder.committedPaths == ["/first"])
            #expect(recorder.finishedPaths == ["/first"])
            #expect(webView.backForwardList.currentItem?.url.path == "/first")
            #expect(webView.backForwardList.forwardItem?.url.path == "/second")
        }
    }

    @Test(arguments: [false, true])
    func goingBackInIframeBeforeCOOPResponseArrivesStillLoadsCOOPPage(siteIsolationEnabled: Bool) async throws {
        try await interruptCOOPNavigationWhileItsProcessIsBusy(
            from: "/second-with-iframe",
            siteIsolationEnabled: siteIsolationEnabled
        ) { webView in
            let iframe = try #require(await webView.firstChildFrame)
            try await webView.callJavaScript(in: iframe) { "location.href = '/frame2'" }
            try await waitForCondition("The iframe navigation adds a back/forward list item") {
                webView.backForwardList.backList.count == 2
            }
        } interrupt: { webView in
            webView.goBack()
        } check: { webView, recorder in
            #expect(recorder.swapDecisions == ["swap"])
            #expect(recorder.committedPaths == ["/coop"])
            #expect(webView._committedURL?.path == "/coop")
        }
    }

    private func expectLoadOfThirdPage(
        in webView: TestWKWebView,
        recorder: COOPNavigationRecorder,
        after previousPath: String
    ) async throws {
        #expect(recorder.swapDecisions == ["cancelLoad"])
        try await waitForCondition("A navigation finishes") { !recorder.finishedPaths.isEmpty }

        #expect(recorder.committedPaths == ["/third"])
        #expect(recorder.finishedPaths == ["/third"])
        #expect(webView.backForwardList.currentItem?.url.path == "/third")
        #expect(webView.backForwardList.backItem?.url.path == previousPath)
    }

    private func interruptCOOPNavigationWhileItsProcessIsBusy(
        from previousPath: String = "/second",
        siteIsolationEnabled: Bool,
        prepare: (TestWKWebView) async throws -> Void = { _ in },
        interrupt: (TestWKWebView) async throws -> Void,
        check: (TestWKWebView, COOPNavigationRecorder) async throws -> Void
    ) async throws {
        let responses = COOPNavigationResponses()
        var server = try responses.makeServer()

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            configuration.enableCrossOriginOpenerAndEmbedderPolicies()
            let (webView, navigationDelegate) = webViewAndDelegate(configuration: configuration, siteIsolationEnabled: siteIsolationEnabled)

            for path in ["/first", previousPath] {
                webView.load(URLRequest(url: try #require(URL(string: "https://example.com\(path)"))))
                try await navigationDelegate.waitForDidFinishNavigation()
            }
            try await prepare(webView)

            let recorder = COOPNavigationRecorder(webView: webView, navigationDelegate: navigationDelegate)
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/coop"))))
            try await waitForCondition("The server receives the request for /coop") { responses.coop.hasReceivedRequest }

            webView.startEvaluatingJavaScript("const request = new XMLHttpRequest(); request.open('GET', '/busy', false); request.send()")
            try await waitForCondition("The page waits for /busy") { responses.busy.hasReceivedRequest }

            try await interrupt(webView)
            responses.coop.send()
            try await waitForCondition("The UI process decides whether /coop moves to another process") {
                !recorder.swapDecisions.isEmpty
            }

            try await waitForCondition("The navigation to /coop commits in another process, or doesn't move there") {
                recorder.swapDecisions != ["swap"] || recorder.committedPaths.contains("/coop")
            }
            responses.busy.send()

            try await check(webView, recorder)
            extendLifetime(navigationDelegate)
        }
    }
}

@MainActor
private struct COOPNavigationResponses {
    let coop = HeldResponse()

    let busy = HeldResponse()

    func makeServer() throws -> HTTPServer {
        let html = ["Content-Type": "text/html"]
        let coopHeaders = ["Content-Type": "text/html", "Cross-Origin-Opener-Policy": "same-origin"]
        return try HTTPServer(protocol: .httpsProxy) {
            Route("/first") { "first" }
            Route("/second") { "second" }
            Route("/second-with-iframe", headerFields: html) { "second<iframe src='https://apple.com/frame'></iframe>" }
            Route("/frame", headerFields: html) { "frame" }
            Route("/frame2", headerFields: html) { "frame2" }
            Route("/third") { "third" }
            Route("/coop", headerFields: coopHeaders, heldBy: coop) { "coop" }
            Route("/busy", headerFields: ["Content-Type": "text/plain"], heldBy: busy) { "busy" }
        }
    }
}

@MainActor
private final class COOPNavigationRecorder {
    var swapDecisions: [String] = []

    var committedPaths: [String] = []

    var finishedPaths: [String] = []

    var failedProvisionalPaths: [String] = []

    init(webView: TestWKWebView, navigationDelegate: TestNavigationDelegate) {
        navigationDelegate.didCommitNavigation = { webView, _ in
            self.committedPaths.append(webView._committedURL?.path ?? "")
        }
        navigationDelegate.didFinishNavigation = { webView, _ in
            self.finishedPaths.append(webView._committedURL?.path ?? "")
        }
        navigationDelegate.didFailProvisionalNavigation = { _, _, error in
            let failingURL = (error as NSError).userInfo[NSURLErrorFailingURLErrorKey] as? URL
            self.failedProvisionalPaths.append(failingURL?.path ?? "")
        }
        webView._setNavigationResponseProcessSwapReplyHandler(forTesting: { decision, sendReply in
            self.swapDecisions.append(Self.name(of: decision))
            sendReply()
        })
    }

    private static func name(of decision: _WKNavigationResponseProcessSwapDecision) -> String {
        switch decision {
        case .swap: "swap"
        case .resumeLoad: "resumeLoad"
        case .cancelLoad: "cancelLoad"
        @unknown default: "unknown"
        }
    }
}
