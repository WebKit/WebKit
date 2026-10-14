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
private import Network
private import TestWebKitAPILibrary
import Testing
import WebKit
import WebKit_Private.WKWebViewPrivate
private import WebKit_Private.WKWebsiteDataStorePrivate
private import WebKit_Private._WKWebsiteDataStoreConfiguration

import struct Foundation.URL
import struct Foundation.URLRequest
import struct Swift.String

#if ENABLE_SWIFTUI

@_spi(Testing) @_spi(CrossImportOverlay) import WebKit

// MARK: Supporting test types

#if WTF_PLATFORM_WATCHOS || WTF_PLATFORM_APPLETV
private let isEnhancedSecurityEnabledByDefault = true
#else
private let isEnhancedSecurityEnabledByDefault = false
#endif

@MainActor
private class TestNavigationDecider: WebPage.NavigationDeciding {
    var preferencesMutation: (inout WebPage.NavigationPreferences) -> Void = { _ in }

    func decidePolicy(
        for action: WebPage.NavigationAction,
        preferences: inout WebPage.NavigationPreferences
    ) async -> WKNavigationActionPolicy {
        preferencesMutation(&preferences)
        return .allow
    }
}

// MARK: Tests

@MainActor
struct EnhancedSecurityTests {
    @Test(arguments: [
        (mode: WebPage.NavigationPreferences.SecurityRestrictionMode.maximizeCompatibility, expected: true),
        (mode: WebPage.NavigationPreferences.SecurityRestrictionMode.none, expected: isEnhancedSecurityEnabledByDefault),
    ])
    func securityRestrictionModeFromConfiguration(
        mode: WebPage.NavigationPreferences.SecurityRestrictionMode,
        expected: Bool
    ) async throws {
        var configuration = WebPage.Configuration()
        configuration.defaultNavigationPreferences.securityRestrictionMode = mode

        let page = WebPage(configuration: configuration)
        try await page.load(html: "<body></body>").wait()

        #expect(page.backingWebView._webProcessIdentifier != 0)

        let result = await page.backingWebView._isEnhancedSecurityEnabled()
        #expect(result == expected)
    }

    @Test(arguments: [true, false])
    func securityRestrictionModeFromNavigationDecider(enabled: Bool) async throws {
        let decider = TestNavigationDecider()
        decider.preferencesMutation = { preferences in
            preferences.securityRestrictionMode = enabled ? .maximizeCompatibility : .none
        }

        let page = WebPage(navigationDecider: decider)
        try await page.load(html: "<body></body>").wait()

        #expect(page.backingWebView._webProcessIdentifier != 0)

        let result = await page.backingWebView._isEnhancedSecurityEnabled()
        #expect(result == (enabled || isEnhancedSecurityEnabledByDefault))
    }
}

#endif

/// An HTTP proxy for a test that stops a load while the network process has it parked.
///
/// It runs on a queue of its own, so it keeps answering requests while the test blocks the main thread.
private final class ParkedLoadProxy: @unchecked Sendable {
    private static let popupHTML = """
        <!DOCTYPE html><script>
        fetch('/wait-to-stop').then(async () => {
            opener.stop();
            await fetch('/stopped');
        });
        </script>
        """

    private let queue = DispatchQueue(label: "TestWebKitAPI.ParkedLoadProxy")
    private let listener: NWListener
    private let stoppedSemaphore = DispatchSemaphore(value: 0)

    // Only used on `queue`.
    private var connections: [NWConnection] = []
    private var waitToStopConnection: NWConnection?
    private var insecureConnection: NWConnection?
    private var didArmPopup = false
    private var didReceiveInsecureRequest = false
    private var didCloseInsecureConnection = false

    init() throws {
        listener = try NWListener(using: .tcp, on: .any)
    }

    /// The URL to use as the HTTP proxy of a data store, once the proxy has started.
    var endpoint: URL? {
        queue.sync { listener.port.flatMap { URL(string: "http://127.0.0.1:\($0.rawValue)/") } }
    }

    /// Whether the popup is waiting for a response to stop the opener's load.
    var hasArmedPopup: Bool { queue.sync { didArmPopup } }

    var hasInsecureRequest: Bool { queue.sync { didReceiveInsecureRequest } }

    var insecureConnectionClosed: Bool { queue.sync { didCloseInsecureConnection } }

    func start() async throws {
        try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Void, any Error>) in
            listener.newConnectionHandler = { [self] connection in
                connections.append(connection)
                connection.start(queue: queue)
                receiveRequest(on: connection)
            }
            listener.stateUpdateHandler = { [self] state in
                switch state {
                case .ready:
                    listener.stateUpdateHandler = nil
                    continuation.resume()
                case .failed(let error):
                    listener.stateUpdateHandler = nil
                    continuation.resume(throwing: error)
                default:
                    break
                }
            }
            listener.start(queue: queue)
        }
    }

    func stop() {
        queue.async { [self] in
            listener.newConnectionHandler = nil
            listener.cancel()
            for connection in connections {
                connection.cancel()
            }
            connections = []
        }
    }

    /// Sends the start of the insecure page's response, then the response that lets the popup stop the load, and
    /// waits for the popup's next request, which reaches the network process after the stop does.
    ///
    /// This blocks the calling thread until then.
    func sendInsecureResponseAndWaitForStop(timeout: DispatchTime) -> DispatchTimeoutResult {
        queue.async { [self] in
            guard let insecureConnection else {
                return
            }

            // Content sniffing holds back the response until it has 512 bytes of the body. The last byte never comes.
            let response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 513\r\n\r\n" + String(repeating: " ", count: 512)
            insecureConnection.send(
                content: Data(response.utf8),
                completion: .contentProcessed { [self] _ in
                    waitForClose(of: insecureConnection)

                    // Gives the network process time to park the load before the popup stops it.
                    queue.asyncAfter(deadline: .now() + .milliseconds(50)) { [self] in
                        if let waitToStopConnection {
                            send(Self.response(body: "ok"), on: waitToStopConnection)
                        }
                    }
                }
            )
        }

        return stoppedSemaphore.wait(timeout: timeout)
    }

    private func receiveRequest(on connection: NWConnection, buffer: Data = Data()) {
        connection.receive(minimumIncompleteLength: 1, maximumLength: .max) { [self] content, _, isComplete, error in
            let buffer = buffer + (content ?? Data())
            if let headerEnd = buffer.range(of: Data("\r\n\r\n".utf8)) {
                respond(to: String(decoding: buffer[..<headerEnd.lowerBound], as: UTF8.self), on: connection)
            } else if isComplete || error != nil {
                connection.cancel()
            } else {
                receiveRequest(on: connection, buffer: buffer)
            }
        }
    }

    private func respond(to request: String, on connection: NWConnection) {
        // A request to a proxy has the whole URL, as in `GET http://parked-load.internal/popup HTTP/1.1`.
        let target = request.split(separator: " ", maxSplits: 2).dropFirst().first.map(String.init) ?? ""
        switch URL(string: target)?.path {
        case "/popup":
            send(Self.response(contentType: "text/html", body: Self.popupHTML), on: connection)
        case "/wait-to-stop":
            waitToStopConnection = connection
            didArmPopup = true
        case "/insecure":
            insecureConnection = connection
            didReceiveInsecureRequest = true
        case "/stopped":
            send(Self.response(body: "ok"), on: connection)
            stoppedSemaphore.signal()
        default:
            send(Self.response(status: "404 Not Found", body: ""), on: connection)
        }
    }

    private func send(_ response: String, on connection: NWConnection) {
        connection.send(
            content: Data(response.utf8),
            completion: .contentProcessed { [self] _ in
                receiveRequest(on: connection)
            }
        )
    }

    private func waitForClose(of connection: NWConnection) {
        connection.receive(minimumIncompleteLength: 1, maximumLength: .max) { [self] _, _, isComplete, error in
            if isComplete || error != nil {
                didCloseInsecureConnection = true
            } else {
                waitForClose(of: connection)
            }
        }
    }

    private static func response(status: String = "200 OK", contentType: String = "text/plain", body: String) -> String {
        "HTTP/1.1 \(status)\r\nContent-Type: \(contentType)\r\nContent-Length: \(body.utf8.count)\r\n\r\n\(body)"
    }
}

@MainActor
struct EnhancedSecurityProcessSwapTests {
    @Test
    func declinedSwapDropsLoadCancelledByItsProcess() async throws {
        let proxy = try ParkedLoadProxy()
        defer { proxy.stop() }
        try await proxy.start()

        let storeConfiguration = _WKWebsiteDataStoreConfiguration(nonPersistentConfiguration: ())
        storeConfiguration.httpProxy = try #require(proxy.endpoint)
        let configuration = WKWebViewConfiguration()
        configuration.websiteDataStore = WKWebsiteDataStore._store(with: storeConfiguration)
        configuration.preferences.javaScriptCanOpenWindowsAutomatically = true
        configuration.setFeature("EnhancedSecurityHeuristicsEnabled", enabled: true)
        configuration.setFeature("SiteIsolationSharedProcessEnabled", enabled: false)
        let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: configuration)
        let openedWebViewRecorder = OpenedWebViewRecorder()
        defer { extendLifetime((webView, openedWebViewRecorder)) }
        webView.uiDelegate = openedWebViewRecorder

        let baseURL = try #require(URL(string: "http://parked-load.internal/"))
        webView.loadHTMLString("<script>open('/popup')</script>", baseURL: baseURL)
        try await navigationDelegate.waitForDidFinishNavigation()

        // The popup shares the browsing context group, so the UI process declines to swap processes for the insecure page.
        let popup = try await openedWebViewRecorder.nextOpenedWebView()
        defer { extendLifetime(popup) }
        #expect(popup.webView._webProcessIdentifier == webView._webProcessIdentifier)
        try await waitForCondition("the popup to wait to stop the load") { proxy.hasArmedPopup }

        webView.load(URLRequest(url: try #require(URL(string: "http://parked-load.internal/insecure"))))
        try await waitForCondition("the request for the insecure page") { proxy.hasInsecureRequest }

        // The network process parks the load until the UI process decides whether its response swaps processes. Block
        // the main thread so the UI process decides only after the popup has stopped the load.
        let waitResult = proxy.sendInsecureResponseAndWaitForStop(timeout: .now() + .seconds(5))
        #expect(waitResult == .success)

        try await waitForCondition("the network process to cancel the load") { proxy.insecureConnectionClosed }
    }
}
