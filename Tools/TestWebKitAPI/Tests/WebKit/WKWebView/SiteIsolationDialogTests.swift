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

import Foundation
private import TestWebKitAPILibrary
import Testing
import WebKit

import struct Foundation.URL
import struct Swift.String

// With site isolation, each web process hosting part of a page can ask the UI process to show a JavaScript dialog.
// The client still sees them one at a time, and a dialog's process waits for its reply without blocking the others.

private struct Dialog: Equatable {
    let message: String
    let host: String
    var defaultText: String? = nil
}

@MainActor
private final class ConfirmAndPromptResponder: NSObject, WKUIDelegate {
    static let promptReply = "prompt reply"

    private(set) var dialogs: [Dialog] = []

    func webView(
        _ webView: WKWebView,
        runJavaScriptConfirmPanelWithMessage message: String,
        initiatedByFrame frame: WKFrameInfo
    ) async -> Bool {
        dialogs.append(Dialog(message: message, host: frame.securityOrigin.host))
        return true
    }

    func webView(
        _ webView: WKWebView,
        runJavaScriptTextInputPanelWithPrompt prompt: String,
        defaultText: String?,
        initiatedByFrame frame: WKFrameInfo
    ) async -> String? {
        dialogs.append(Dialog(message: prompt, host: frame.securityOrigin.host, defaultText: defaultText))
        return Self.promptReply
    }
}

// An alert stays shown, with its frame's process waiting for the reply, until the delegate method returns.
@MainActor
private final class HeldAlertDelegate: NSObject, WKUIDelegate {
    private(set) var alerts: [Dialog] = []
    private(set) var maximumOutstandingAlerts = 0
    private var outstandingAlerts = 0
    private var heldAlerts: [CheckedContinuation<Void, Never>] = []
    private var holdsAlerts = true

    var heldAlertCount: Int {
        heldAlerts.count
    }

    func waitForAlerts(count: Int) async throws {
        try await waitForCondition("\(count) JavaScript alerts") {
            alerts.count >= count
        }
    }

    func releaseOldestAlert() {
        if !heldAlerts.isEmpty {
            heldAlerts.removeFirst().resume()
        }
    }

    // Also returns from later alerts right away, so that no continuation outlives the test.
    func releaseAllAlerts() {
        holdsAlerts = false
        let continuations = heldAlerts
        heldAlerts.removeAll()
        for continuation in continuations {
            continuation.resume()
        }
    }

    func webView(
        _ webView: WKWebView,
        runJavaScriptAlertPanelWithMessage message: String,
        initiatedByFrame frame: WKFrameInfo
    ) async {
        outstandingAlerts += 1
        maximumOutstandingAlerts = max(maximumOutstandingAlerts, outstandingAlerts)
        defer { outstandingAlerts -= 1 }

        alerts.append(Dialog(message: message, host: frame.securityOrigin.host))
        if holdsAlerts {
            await withCheckedContinuation { heldAlerts.append($0) }
        }
    }
}

@MainActor
@Suite(.timeLimit(.minutes(1)))
struct SiteIsolationDialogTests {
    @Test
    func iframeWithConfirm() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/example") {
                "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"
            }

            Route("/webkit") {
                "<script>window.result = confirm('confirm message')</script>"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: .init(httpsProxyFor: serverConfiguration))
            let responder = ConfirmAndPromptResponder()
            defer { extendLifetime(responder) }
            webView.uiDelegate = responder

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/example"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(responder.dialogs == [Dialog(message: "confirm message", host: "webkit.org")])

            let childFrame = try #require(await webView.firstChildFrame)
            #expect(try await webView.callJavaScript(returning: Bool.self, in: childFrame) { "return window.result" })

            #expect(
                await webView.frameTreesInProcesses() == [
                    .local("https://example.com", children: [.remote]),
                    .remote(children: [.local("https://webkit.org")]),
                ]
            )
        }
    }

    @Test
    func iframeWithPrompt() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/example") {
                "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"
            }

            Route("/webkit") {
                "<script>window.result = prompt('prompt message', 'default input')</script>"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: .init(httpsProxyFor: serverConfiguration))
            let responder = ConfirmAndPromptResponder()
            defer { extendLifetime(responder) }
            webView.uiDelegate = responder

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/example"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(responder.dialogs == [Dialog(message: "prompt message", host: "webkit.org", defaultText: "default input")])

            let childFrame = try #require(await webView.firstChildFrame)
            let result = try await webView.callJavaScript(returning: String.self, in: childFrame) { "return window.result" }
            #expect(result == ConfirmAndPromptResponder.promptReply)

            #expect(
                await webView.frameTreesInProcesses() == [
                    .local("https://example.com", children: [.remote]),
                    .remote(children: [.local("https://webkit.org")]),
                ]
            )
        }
    }

    @Test
    func mainFrameHeartbeatContinuesWhileCrossSiteIframeDialogShown() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/example") {
                """
                <script>
                window.__heartbeat = 0;
                setInterval(() => { window.__heartbeat++; }, 10);
                </script>
                <iframe src='https://webkit.org/webkit'></iframe>
                """
            }

            Route("/webkit") {
                "<script>alert('subframe alert')</script>"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: .init(httpsProxyFor: serverConfiguration))
            let alerts = HeldAlertDelegate()
            defer {
                alerts.releaseAllAlerts()
                extendLifetime(navigationDelegate)
            }
            webView.uiDelegate = alerts

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/example"))))
            try await alerts.waitForAlerts(count: 1)
            #expect(alerts.alerts == [Dialog(message: "subframe alert", host: "webkit.org")])

            let firstHeartbeat = try await webView.callJavaScript(returning: Int.self) { "return window.__heartbeat" }
            try await waitForCondition("the main frame's timer to fire while the iframe's alert is shown") {
                try await webView.callJavaScript(returning: Int.self) { "return window.__heartbeat" } > firstHeartbeat
            }
            #expect(alerts.heldAlertCount == 1)
        }
    }

    @Test
    func simultaneousDialogsFromMultipleCrossOriginFrames() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/example") {
                """
                <iframe src='https://webkit.org/webkit'></iframe>
                <iframe src='https://apple.com/apple'></iframe>
                <script>setTimeout(() => alert('example dialog'), 0)</script>
                """
            }

            Route("/webkit") {
                "<script>alert('webkit dialog')</script>"
            }

            Route("/apple") {
                "<script>alert('apple dialog')</script>"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: .init(httpsProxyFor: serverConfiguration))
            let alerts = HeldAlertDelegate()
            defer {
                alerts.releaseAllAlerts()
                extendLifetime(navigationDelegate)
            }
            webView.uiDelegate = alerts

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/example"))))
            for count in 1...3 {
                try await alerts.waitForAlerts(count: count)
                // Gives the processes still waiting to show an alert a chance to wrongly show it alongside this one.
                try await Task.sleep(for: .milliseconds(50))
                alerts.releaseOldestAlert()
            }

            #expect(alerts.maximumOutstandingAlerts == 1)
            #expect(alerts.alerts.count == 3)
            #expect(Set(alerts.alerts.map(\.host)) == Set(["example.com", "webkit.org", "apple.com"]))
        }
    }

    // Same-site iframes share a process, so the first iframe's alert keeps the second iframe's script from running.
    @Test(arguments: ["apple.com", "webkit.org"])
    func queuedDialogPurgedByMainFrameNavigation(secondIframeHost: String) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/example") {
                """
                <iframe src='https://webkit.org/first'></iframe>
                <iframe src='https://\(secondIframeHost)/second'></iframe>
                """
            }

            Route("/first") {
                "<script>alert('first dialog')</script>"
            }

            Route("/second") {
                "<script>alert('second dialog')</script>"
            }

            Route("/next") {
                "<p>next</p>"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: .init(httpsProxyFor: serverConfiguration))
            let alerts = HeldAlertDelegate()
            defer { alerts.releaseAllAlerts() }
            webView.uiDelegate = alerts

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/example"))))
            try await alerts.waitForAlerts(count: 1)

            // Gives the other iframe time to queue its alert behind the held one.
            try await Task.sleep(for: .milliseconds(500))

            // Wait for the commit rather than the provisional load, since the old frames stay in the page until then.
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/next"))))
            await navigationDelegate.waitForDidCommitNavigation()

            // Releasing the held alert would let a queued alert that wasn't purged reach the delegate.
            alerts.releaseAllAlerts()
            try await Task.sleep(for: .milliseconds(500))
            #expect(alerts.alerts.count == 1)
        }
    }
}
