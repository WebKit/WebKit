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
