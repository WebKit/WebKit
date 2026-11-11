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
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS OF A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

private import TestWebKitAPILibrary
import Testing
import WebKit
private import WebKit_Private.WKFrameInfoPrivate
private import WebKit_Private.WKPreferencesPrivate
private import WebKit_Private.WKProcessPoolPrivate
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private._WKFrameTreeNode

import struct Foundation.URL
import struct Swift.String

// With site isolation, the UI process chooses a process for a navigation once the client decides its policy. These
// tests check decisions that the client makes after the page has moved on, and the state the page sets up for a
// navigation before the client decides.

@MainActor
struct SiteIsolationPolicyDecisionTests {
    // When a page leaves its group, the process of its child frame closes its WebPage, whose main frame is in another
    // process. A decision for the child frame's navigation that arrives afterward must not crash the process, which
    // keeps hosting a child frame of the window that opened the page.
    @Test
    func childFrameDecisionAfterChildFrameProcessClosesPage() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/opener") {
                "<iframe src='https://webkit.org/child'></iframe><script>w = window.open('https://example.com/opened')</script>"
            }

            Route("/opened") {
                "<iframe src='https://webkit.org/child'></iframe>"
            }

            Route("/child") {
                "child"
            }

            Route("/destination") {
                "destination"
            }

            Route("/next") {
                "next"
            }
        }

        try await server.run { serverConfiguration in
            // Without the back/forward cache, the opened window's processes close its WebPages once it leaves the group.
            let configuration = Self.configuration(serverConfiguration, backForwardCacheEnabled: false)
            configuration.preferences.javaScriptCanOpenWindowsAutomatically = true
            let (opener, openerNavigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: configuration)
            let openedWebViews = OpenedWebViewRecorder()
            opener.uiDelegate = openedWebViews

            opener.load(URLRequest(url: try #require(URL(string: "https://example.com/opener"))))
            try await openerNavigationDelegate.waitForDidFinishNavigation()
            let (opened, openedNavigationDelegate) = try await openedWebViews.nextOpenedWebView()
            try await waitForFrameTrees(of: opener, Self.pageWithChildFrameTrees)
            try await waitForFrameTrees(of: opened, Self.pageWithChildFrameTrees)

            let openerChildFrame = try #require(await opener.firstChildFrame)
            let childFrame = try #require(await opened.firstChildFrame)
            let childFrameProcess = childFrame._processIdentifier
            #expect(openerChildFrame._processIdentifier == childFrameProcess)

            let decideDestinationPolicy = await openedNavigationDelegate.holdNextNavigationActionPolicyDecision {
                $0.request.url?.path == "/destination"
            } triggeredBy: {
                opened.startEvaluatingJavaScript("location.href = 'https://apple.com/destination'", in: childFrame)
            }

            opened.load(URLRequest(url: try #require(URL(string: "https://w3.org/next"))))
            try await openedNavigationDelegate.waitForDidFinishNavigation()
            #expect(await opened.frameTreesInProcesses() == [.local("https://w3.org")])

            // The child frame process handles messages in order: it has closed the opened window's WebPage once it runs
            // a script for the opener. Cancelling answers the process right away, unlike allowing, which first checks
            // whether another app handles the destination, so the process has handled the answer once it runs another
            // script.
            let host = "return location.host"
            #expect(try await opener.callJavaScript(returning: String.self, in: openerChildFrame) { host } == "webkit.org")
            decideDestinationPolicy(.cancel)
            #expect(try await opener.callJavaScript(returning: String.self, in: openerChildFrame) { host } == "webkit.org")
            #expect(isProcessRunning(childFrameProcess))
            #expect(await opener.frameTreesInProcesses() == Self.pageWithChildFrameTrees)
        }
    }

    // MARK: Decisions for frames of a document the page has navigated away from

    /// How the main frame navigates away from the document whose iframe's navigation is waiting for a decision.
    enum MainFrameNavigation: String, CaseIterable, Sendable, CustomTestStringConvertible {
        /// The client loads a cross-site page, which starts a new browsing context group.
        case crossSite

        /// The main frame loads a same-site page whose Cross-Origin-Opener-Policy moves it to a new group when its
        /// response arrives.
        case crossOriginOpenerPolicy

        var testDescription: String { rawValue }
    }

    // A decision for an iframe of the document the main frame navigated away from must not join a process to the page
    // that replaced it, or load anything.
    @Test(arguments: MainFrameNavigation.allCases, [true, false])
    func childFrameDecisionAfterMainFrameNavigatesAwayFromDocumentIsIgnored(
        mainFrameNavigation: MainFrameNavigation,
        backForwardCacheEnabled: Bool
    ) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/main") {
                "<iframe src='https://webkit.org/child'></iframe>"
            }

            Route("/child") {
                "child"
            }

            Route("/destination") {
                "<script>alert('destination loaded')</script>"
            }

            Route("/next") {
                "next"
            }

            Route("/isolated", headerFields: ["Cross-Origin-Opener-Policy": "same-origin"]) {
                "isolated"
            }
        }

        try await server.run { serverConfiguration in
            let configuration = Self.configuration(serverConfiguration, backForwardCacheEnabled: backForwardCacheEnabled)
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: configuration)
            let alerts = AlertRecorder()
            webView.uiDelegate = alerts
            let decisions = HeldNavigationActionDecisions(forPath: "/destination", of: navigationDelegate)

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/main"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            try await waitForFrameTrees(of: webView, Self.pageWithChildFrameTrees)
            let childFrame = try #require(await webView.firstChildFrame)
            try await webView.callJavaScript(in: childFrame) { "location.href = 'https://apple.com/destination'" }
            let decideDestinationPolicy = try await decisions.next()

            let (nextURL, nextOrigin) =
                switch mainFrameNavigation {
                case .crossSite: ("https://w3.org/next", "https://w3.org")
                case .crossOriginOpenerPolicy: ("https://example.com/isolated", "https://example.com")
                }
            webView.load(URLRequest(url: try #require(URL(string: nextURL))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let nextFrameTrees: FrameTreesInProcesses = [.local(nextOrigin)]
            #expect(await webView.frameTreesInProcesses() == nextFrameTrees)

            // Choosing a process for the iframe's destination would take the prewarmed process.
            let processPool = configuration.processPool
            let prewarmedProcesses = try await stablePrewarmedProcessIdentifiers(of: processPool)
            decideDestinationPolicy(.allow)

            // A decision that chose a process or loaded the destination would do so soon after it.
            let alert = try? await alerts.nextAlert(timeout: .seconds(1))
            #expect(alert == nil)
            #expect(processPool._prewarmedProcessIdentifiersForTesting() == prewarmedProcesses)
            #expect(await webView.frameTreesInProcesses() == nextFrameTrees)

            webView.goBack()
            try await navigationDelegate.waitForDidFinishNavigation()
            try await waitForFrameTrees(of: webView, Self.pageWithChildFrameTrees)
            let restoredChildFrame = try #require(await webView.firstChildFrame)
            let childFrameHost = try await webView.callJavaScript(returning: String.self, in: restoredChildFrame) {
                "return location.host"
            }
            #expect(childFrameHost == "webkit.org")
        }
    }

    // A decision for an iframe of a document the main frame navigated away from must not choose a process either when a
    // window that shares the group keeps the group alive, and the window can still load the iframe's destination in an
    // iframe.
    @Test(arguments: [true, false])
    func childFrameDecisionAfterMainFrameNavigatesAwayFromDocumentInOpenedWindow(mainFrameLeavesGroup: Bool) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/opener") {
                "<script>w = window.open('https://example.com/opened')</script>"
            }

            Route("/opened") {
                "<iframe src='https://webkit.org/child'></iframe>"
            }

            Route("/child") {
                "child"
            }

            Route("/destination") {
                "<script>alert('destination loaded')</script>"
            }

            Route("/opener_destination") {
                "<script>alert('opener destination loaded')</script>"
            }

            Route("/next") {
                "next"
            }
        }

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            configuration.preferences.javaScriptCanOpenWindowsAutomatically = true
            let (opener, openerNavigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: configuration)
            let openedWebViews = OpenedWebViewRecorder()
            opener.uiDelegate = openedWebViews
            let openerAlerts = AlertRecorder()

            opener.load(URLRequest(url: try #require(URL(string: "https://example.com/opener"))))
            try await openerNavigationDelegate.waitForDidFinishNavigation()
            let (opened, openedNavigationDelegate) = try await openedWebViews.nextOpenedWebView()
            opener.uiDelegate = openerAlerts
            let openedAlerts = AlertRecorder()
            opened.uiDelegate = openedAlerts
            try await waitForFrameTrees(of: opened, Self.pageWithChildFrameTrees)

            let decisions = HeldNavigationActionDecisions(forPath: "/destination", of: openedNavigationDelegate)
            let childFrame = try #require(await opened.firstChildFrame)
            try await opened.callJavaScript(in: childFrame) { "location.href = 'https://apple.com/destination'" }
            let decideDestinationPolicy = try await decisions.next()

            if mainFrameLeavesGroup {
                opened.load(URLRequest(url: try #require(URL(string: "https://w3.org/next"))))
            } else {
                // Without a user gesture, the navigation stays in the group that the opener shares.
                try await opened.evaluateJavaScriptWithoutUserGesture("location.href = 'https://w3.org/next'")
            }
            try await openedNavigationDelegate.waitForDidFinishNavigation()

            let processPool = configuration.processPool
            let prewarmedProcesses = try await stablePrewarmedProcessIdentifiers(of: processPool)
            decideDestinationPolicy(.allow)
            let alert = try? await openedAlerts.nextAlert(timeout: .seconds(1))
            #expect(alert == nil)
            #expect(processPool._prewarmedProcessIdentifiersForTesting() == prewarmedProcesses)

            let openedFrameTrees = await opened.frameTreesInProcesses()
            #expect(openedFrameTrees.trees.count == (mainFrameLeavesGroup ? 1 : 2), "\(openedFrameTrees)")

            // The opener can still load the site in an iframe.
            try await opener.callJavaScript {
                "document.body.appendChild(document.createElement('iframe')).src = 'https://apple.com/opener_destination'"
            }
            let openerAlert = try await openerAlerts.nextAlert()
            #expect(openerAlert == "opener destination loaded")
            try await waitForFrameTrees(
                of: opener,
                [
                    .local("https://example.com", children: [.remote]),
                    .remote(children: [.local("https://apple.com")]),
                ],
                ignoringProcessesWithoutLocalFrames: true
            )
        }
    }

    // MARK: Helpers

    private static let pageWithChildFrameTrees: FrameTreesInProcesses = [
        .local("https://example.com", children: [.remote]),
        .remote(children: [.local("https://webkit.org")]),
    ]

    private static func configuration(
        _ server: HTTPServer.Configuration,
        backForwardCacheEnabled: Bool
    ) -> WKWebViewConfiguration {
        let configuration = WKWebViewConfiguration(httpsProxyFor: server)
        if backForwardCacheEnabled {
            configuration.setFeature("MultiProcessBackForwardCacheEnabled", enabled: true)
        } else {
            configuration.processPool = .withBackForwardCacheDisabled()
        }
        return configuration
    }
}

/// Holds the client's decisions for navigations to a path, so that a test can make them later, and allows every other
/// navigation.
@MainActor
private final class HeldNavigationActionDecisions {
    private let path: String
    private var decisionHandlers: [(WKNavigationActionPolicy) -> Void] = []

    init(forPath path: String, of navigationDelegate: TestNavigationDelegate) {
        self.path = path
        navigationDelegate.decidePolicyForNavigationAction = { [weak self] navigationAction, decisionHandler in
            guard let self, navigationAction.request.url?.path == path else {
                decisionHandler(.allow)
                return
            }
            decisionHandlers.append(decisionHandler)
        }
    }

    /// Waits for the next navigation to the path to need a decision, if one doesn't already.
    ///
    /// - Returns: The decision handler of the navigation.
    func next() async throws -> (WKNavigationActionPolicy) -> Void {
        try await waitForCondition("a navigation to \(path) to need a decision") {
            !decisionHandlers.isEmpty
        }
        return decisionHandlers.removeFirst()
    }
}

/// Prewarms a process, then waits for the prewarmed processes of a process pool to finish launching and stay the same
/// for a while.
///
/// A navigation that needs a new process takes a prewarmed one, so a change to them shows that a process was chosen.
///
/// - Parameter processPool: The process pool.
/// - Returns: The identifiers of the prewarmed processes.
/// - Throws: ``ConditionTimedOut`` if the process pool has no launched prewarmed process.
@MainActor
private func stablePrewarmedProcessIdentifiers(of processPool: WKProcessPool) async throws -> Set<NSNumber> {
    processPool._warmInitialProcess()
    var identifiers: Set<NSNumber> = []
    var identifiersSince = ContinuousClock.now
    try await waitForCondition("the prewarmed processes to launch") {
        let currentIdentifiers = processPool._prewarmedProcessIdentifiersForTesting() ?? []
        if currentIdentifiers != identifiers {
            identifiers = currentIdentifiers
            identifiersSince = .now
        }
        return !identifiers.isEmpty && !identifiers.contains(0) && ContinuousClock.now - identifiersSince > .milliseconds(200)
    }
    return identifiers
}

/// Waits for the frame trees of a web view to match, as frames in other processes appear some time after a load
/// finishes.
@MainActor
private func waitForFrameTrees(
    of webView: WKWebView,
    _ expectedTrees: FrameTreesInProcesses,
    ignoringProcessesWithoutLocalFrames: Bool = false
) async throws {
    var trees = await webView.frameTreesInProcesses()
    do {
        try await waitForCondition("the frame trees to be \(expectedTrees)") {
            trees = await webView.frameTreesInProcesses()
            guard ignoringProcessesWithoutLocalFrames else {
                return trees == expectedTrees
            }
            let treesWithLocalFrames = trees.trees.filter(\.hasLocalFrame)
            return treesWithLocalFrames.count == expectedTrees.trees.count
                && expectedTrees.trees.allSatisfy { expectedTree in
                    treesWithLocalFrames.filter { $0 == expectedTree }.count
                        == expectedTrees.trees.filter { $0 == expectedTree }.count
                }
        }
    } catch is ConditionTimedOut {
        #expect(trees == expectedTrees)
    }
}

extension FrameTree {
    fileprivate var hasLocalFrame: Bool {
        origin != nil || children.contains(where: \.hasLocalFrame)
    }
}
