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

#if ENABLE_WK_WEB_EXTENSIONS

private import Darwin
import Foundation
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
private import TestWebKitAPILibrary.Helpers.cocoa.WebExtensionUtilities
import Testing
import WebKit
private import WebKit_Private._WKWebsiteDataStoreConfiguration
private import WebKit_Private.WKWebViewConfigurationPrivate
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private.WKWebsiteDataStorePrivate

import struct Foundation.URL
import struct Swift.String

/// The directory that holds the persistent data stores of every test in this file, removed when the process exits, as
/// `Util::temporaryPersistentDataStore()` removes its directory.
///
/// No test removes its own data store's directory, because the network process may still be writing to it.
private let temporaryWebsiteDataDirectory: URL = {
    let directory = URL.temporaryDirectory.appending(path: "WebsiteData-\(UUID().uuidString)", directoryHint: .isDirectory)
    atexit {
        try? FileManager.default.removeItem(at: temporaryWebsiteDataDirectory)
    }
    return directory
}()

@MainActor
struct WKWebExtensionUnpartitionedStorageTests {
    private let reportToParentScript = """
        <script>\
        parent.postMessage({ host: location.hostname, cookie: document.cookie }, '*')\
        </script>
        """

    private let manifestWithLocalhostHostPermission: [String: Any] = [
        "manifest_version": 3,

        "name": "Unpartitioned Storage Test",
        "description": "Unpartitioned Storage Test",
        "version": "1",

        "host_permissions": ["*://localhost/*"],

        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],
    ]

    /// The resources of an extension whose page embeds the server's `/read` page twice, once from the `localhost` host
    /// the extension has access to and once from `127.0.0.1`, and sends `Frames Reported` with what each frame posts to it,
    /// keyed by the frame's `host`.
    private func extensionResources(_ configuration: HTTPServer.Configuration) -> [String: Any] {
        let permittedURL = "http://localhost:\(configuration.port)/read"
        let otherURL = "http://127.0.0.1:\(configuration.port)/read"

        let extensionPageScript = """
            const results = { }
            let remaining = 2

            window.addEventListener('message', (event) => {
              results[event.data.host] = event.data
              if (!--remaining)
                browser.test.sendMessage('Frames Reported', results)
            })

            for (const url of ['\(permittedURL)', '\(otherURL)']) {
              const iframe = document.createElement('iframe')
              iframe.src = url
              document.body.appendChild(iframe)
            }
            """

        let backgroundScript = """
            browser.test.sendMessage('Ready')
            """

        return [
            "background.js": backgroundScript,
            "extension-page.js": extensionPageScript,
            "extension-page.html": "<body><script type='module' src='extension-page.js'></script></body>",
        ]
    }

    /// Loads a first-party page in the default tab, and waits until the server has received two more requests, which for
    /// a `/seed` page are the page and the `/seeded` request it makes.
    private func seedFirstPartyState(
        _ manager: TestWebExtensionManager,
        configuration: HTTPServer.Configuration,
        url: URL,
        sourceLocation: SourceLocation = #_sourceLocation
    ) async throws {
        let expectedRequests = configuration.totalRequests + 2

        let context = try #require(manager.context, sourceLocation: sourceLocation)
        let tab = try #require(manager.defaultTab, sourceLocation: sourceLocation)
        tab.changeWebViewIfNeeded(for: url, for: context)
        try #require(tab.webView, sourceLocation: sourceLocation).load(URLRequest(url: url))

        try await waitForCondition("the first-party page and its request to reach the server", timeout: .seconds(10)) {
            configuration.totalRequests >= expectedRequests
        }
    }

    /// Loads the extension's `extension-page.html` in the default tab, without waiting for it.
    private func loadExtensionPage(_ manager: TestWebExtensionManager, sourceLocation: SourceLocation = #_sourceLocation) throws {
        let context = try #require(manager.context, sourceLocation: sourceLocation)
        let extensionPageURL = try #require(URL(string: "extension-page.html", relativeTo: context.baseURL), sourceLocation: sourceLocation)

        let tab = try #require(manager.defaultTab, sourceLocation: sourceLocation)
        tab.changeWebViewIfNeeded(for: extensionPageURL, for: context)
        try #require(tab.webView, sourceLocation: sourceLocation).load(URLRequest(url: extensionPageURL))
    }

    /// Waits for the extension page to send `Frames Reported`.
    ///
    /// - Returns: What each of the page's frames reported, keyed by the frame's host or name.
    private func waitForFrameReports(_ manager: TestWebExtensionManager) async throws -> [String: [String: Any]] {
        let results = try await manager.waitForTestMessage("Frames Reported") as? [String: Any]
        return results?.compactMapValues { $0 as? [String: Any] } ?? [:]
    }

    /// Loads the extension page in the default tab, and waits for it to send `Frames Reported`.
    ///
    /// - Returns: What each of the page's frames reported, keyed by the frame's host or name.
    @discardableResult
    private func loadExtensionPageAndCollectFrameReports(
        _ manager: TestWebExtensionManager,
        sourceLocation: SourceLocation = #_sourceLocation
    ) async throws -> [String: [String: Any]] {
        try loadExtensionPage(manager, sourceLocation: sourceLocation)

        return try await waitForFrameReports(manager)
    }

    // MARK: - Cookies and Navigation

    @Test
    func sameSiteCookiesAreSentToHostPermittedFrame() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/seed", headerFields: ["Content-Type": "text/html", "Set-Cookie": "key=lax-cookie; SameSite=Lax; Path=/"]) {
                "<script>fetch('/seeded')</script>"
            }
            Route("/seeded", headerFields: ["Content-Type": "text/plain"]) {
                ""
            }
            Route("/read", headerFields: ["Content-Type": "text/html"]) {
                reportToParentScript
            }
        }

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifestWithLocalhostHostPermission, resources: extensionResources(configuration))
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress.appending(path: "read"))

            try await manager.waitForTestMessage("Ready")

            try await seedFirstPartyState(
                manager,
                configuration: configuration,
                url: configuration.localhostAddress.appending(path: "seed")
            )
            try await seedFirstPartyState(
                manager,
                configuration: configuration,
                url: configuration.address.appending(path: "seed")
            )

            let results = try await loadExtensionPageAndCollectFrameReports(manager)

            #expect(results["localhost"]?["cookie"] as? String == "key=lax-cookie")

            #expect(results["127.0.0.1"]?["cookie"] as? String == "")
        }
    }

    @Test
    func sameSiteCookiesAreSentToHostPermittedFrameWithSiteIsolation() async throws {
        let previousShouldEnableSiteIsolation = TestWebExtensionManager.shouldEnableSiteIsolation
        TestWebExtensionManager.shouldEnableSiteIsolation = true
        defer { TestWebExtensionManager.shouldEnableSiteIsolation = previousShouldEnableSiteIsolation }

        var server = try HTTPServer(protocol: .http) {
            Route("/seed", headerFields: ["Content-Type": "text/html", "Set-Cookie": "key=lax-cookie; SameSite=Lax; Path=/"]) {
                "<script>fetch('/seeded')</script>"
            }
            Route("/seeded", headerFields: ["Content-Type": "text/plain"]) {
                ""
            }
            Route("/read", headerFields: ["Content-Type": "text/html"]) {
                reportToParentScript
            }
        }

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifestWithLocalhostHostPermission, resources: extensionResources(configuration))
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress.appending(path: "read"))

            try await manager.waitForTestMessage("Ready")

            try await seedFirstPartyState(
                manager,
                configuration: configuration,
                url: configuration.localhostAddress.appending(path: "seed")
            )
            try await seedFirstPartyState(
                manager,
                configuration: configuration,
                url: configuration.address.appending(path: "seed")
            )

            let results = try await loadExtensionPageAndCollectFrameReports(manager)

            #expect(results["localhost"]?["cookie"] as? String == "key=lax-cookie")

            #expect(results["127.0.0.1"]?["cookie"] as? String == "")
        }
    }

    @Test
    func hostPermissionWithPathAppliesToWholeSite() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/seed", headerFields: ["Content-Type": "text/html", "Set-Cookie": "key=lax-cookie; SameSite=Lax; Path=/"]) {
                "<script>fetch('/seeded')</script>"
            }
            Route("/seeded", headerFields: ["Content-Type": "text/plain"]) {
                ""
            }
            Route("/read", headerFields: ["Content-Type": "text/html"]) {
                reportToParentScript
            }
        }

        var manifest = manifestWithLocalhostHostPermission
        manifest["host_permissions"] = ["*://localhost/read*"]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: extensionResources(configuration))
            let context = try #require(manager.context)

            let matchPattern = try #require(WKWebExtension.MatchPattern.cachedPattern(string: "*://localhost/read*"))
            context.setPermissionStatus(.grantedExplicitly, for: matchPattern)

            try await manager.waitForTestMessage("Ready")

            try await seedFirstPartyState(
                manager,
                configuration: configuration,
                url: configuration.localhostAddress.appending(path: "seed")
            )
            try await seedFirstPartyState(
                manager,
                configuration: configuration,
                url: configuration.address.appending(path: "seed")
            )

            let results = try await loadExtensionPageAndCollectFrameReports(manager)

            #expect(results["localhost"]?["cookie"] as? String == "key=lax-cookie")

            #expect(results["127.0.0.1"]?["cookie"] as? String == "")
        }
    }

    private let reportNamedFrameToTopScript = """
        <script>\
        top.postMessage({ name: window.name }, '*')\
        </script>
        """

    /// The resources of an extension whose page embeds a frame for each name and URL, and sends `Frames Reported` with
    /// what each frame posts to it, keyed by the frame's `name`.
    private func extensionResourcesEmbeddingNamedFrames(_ namesToURLs: [String: String]) -> [String: Any] {
        let frames = namesToURLs.map { name, url in "['\(name)', '\(url)']" }.joined(separator: ", ")

        let extensionPageScript = """
            const results = { }
            let remaining = \(namesToURLs.count)

            window.addEventListener('message', (event) => {
              results[event.data.name] = event.data
              if (!--remaining)
                browser.test.sendMessage('Frames Reported', results)
            })

            for (const [name, url] of [\(frames)]) {
              const iframe = document.createElement('iframe')
              iframe.name = name
              iframe.src = url
              document.body.appendChild(iframe)
            }
            """

        let backgroundScript = """
            browser.test.sendMessage('Ready')
            """

        return [
            "background.js": backgroundScript,
            "extension-page.js": extensionPageScript,
            "extension-page.html": "<body><script type='module' src='extension-page.js'></script></body>",
        ]
    }

    @Test
    func redirectOutOfHostPermittedFrameIsPartitioned() async throws {
        let navigateToRedirectScript = "<script>location.href = '/redirect-to-other'</script>"

        var server = try HTTPServer(protocol: .http) {
            Route("/seed", headerFields: ["Content-Type": "text/html", "Set-Cookie": "key=lax-cookie; SameSite=Lax; Path=/"]) {
                "<script>fetch('/seeded')</script>"
            }
            Route("/seeded", headerFields: ["Content-Type": "text/plain"]) {
                ""
            }
            Route("/start", headerFields: ["Content-Type": "text/html"]) {
                navigateToRedirectScript
            }
            Route("/redirected", headerFields: ["Content-Type": "text/html"]) {
                reportNamedFrameToTopScript
            }
        }

        try await server.run { configuration in
            configuration.addResponses {
                Route(
                    "/redirect-to-other",
                    statusCode: 302,
                    headerFields: ["Location": "http://127.0.0.1:\(configuration.port)/redirected"]
                ) {
                    "redirecting..."
                }
            }

            let resources = extensionResourcesEmbeddingNamedFrames([
                "redirected": "http://localhost:\(configuration.port)/start"
            ])

            let manager = try loadWebExtension(manifest: manifestWithLocalhostHostPermission, resources: resources)
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress.appending(path: "start"))

            try await manager.waitForTestMessage("Ready")

            try await seedFirstPartyState(manager, configuration: configuration, url: configuration.address.appending(path: "seed"))

            try await loadExtensionPageAndCollectFrameReports(manager)

            #expect(configuration.lastRequestCookies == "")
        }
    }

    @Test
    func redirectOutOfHostPermittedFrameIsPartitionedWithSiteIsolation() async throws {
        let previousShouldEnableSiteIsolation = TestWebExtensionManager.shouldEnableSiteIsolation
        TestWebExtensionManager.shouldEnableSiteIsolation = true
        defer { TestWebExtensionManager.shouldEnableSiteIsolation = previousShouldEnableSiteIsolation }

        let navigateToRedirectScript = "<script>location.href = '/redirect-to-other'</script>"

        var server = try HTTPServer(protocol: .http) {
            Route("/seed", headerFields: ["Content-Type": "text/html", "Set-Cookie": "key=lax-cookie; SameSite=Lax; Path=/"]) {
                "<script>fetch('/seeded')</script>"
            }
            Route("/seeded", headerFields: ["Content-Type": "text/plain"]) {
                ""
            }
            Route("/start", headerFields: ["Content-Type": "text/html"]) {
                navigateToRedirectScript
            }
            Route("/redirected", headerFields: ["Content-Type": "text/html"]) {
                reportNamedFrameToTopScript
            }
        }

        try await server.run { configuration in
            configuration.addResponses {
                Route(
                    "/redirect-to-other",
                    statusCode: 302,
                    headerFields: ["Location": "http://127.0.0.1:\(configuration.port)/redirected"]
                ) {
                    "redirecting..."
                }
            }

            let resources = extensionResourcesEmbeddingNamedFrames([
                "redirected": "http://localhost:\(configuration.port)/start"
            ])

            let manager = try loadWebExtension(manifest: manifestWithLocalhostHostPermission, resources: resources)
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress.appending(path: "start"))

            try await manager.waitForTestMessage("Ready")

            try await seedFirstPartyState(manager, configuration: configuration, url: configuration.address.appending(path: "seed"))

            try await loadExtensionPageAndCollectFrameReports(manager)

            #expect(configuration.lastRequestCookies == "")
        }
    }

    private let reportNamedFrameCookieToTopScript = """
        <script>\
        top.postMessage({ name: window.name, cookie: document.cookie }, '*')\
        </script>
        """

    private func runRedirectIntoHostPermittedFrameTest(
        siteIsolation: Bool,
        sourceLocation: SourceLocation = #_sourceLocation
    ) async throws {
        let previousShouldEnableSiteIsolation = TestWebExtensionManager.shouldEnableSiteIsolation
        TestWebExtensionManager.shouldEnableSiteIsolation = siteIsolation
        defer { TestWebExtensionManager.shouldEnableSiteIsolation = previousShouldEnableSiteIsolation }

        var server = try HTTPServer(protocol: .http) {
            Route("/seed", headerFields: ["Content-Type": "text/html", "Set-Cookie": "key=lax-cookie; SameSite=Lax; Path=/"]) {
                "<script>fetch('/seeded')</script>"
            }
            Route("/seeded", headerFields: ["Content-Type": "text/plain"]) {
                ""
            }
            Route("/read", headerFields: ["Content-Type": "text/html"]) {
                reportNamedFrameCookieToTopScript
            }
        }

        try await server.run { configuration in
            configuration.addResponses {
                Route(
                    "/redirect-to-permitted",
                    statusCode: 302,
                    headerFields: ["Location": "http://localhost:\(configuration.port)/read"]
                ) {
                    "redirecting..."
                }
            }

            let resources = extensionResourcesEmbeddingNamedFrames([
                "redirected": "http://127.0.0.1:\(configuration.port)/redirect-to-permitted"
            ])

            let manager = try loadWebExtension(manifest: manifestWithLocalhostHostPermission, resources: resources)
            let context = try #require(manager.context, sourceLocation: sourceLocation)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress.appending(path: "read"))

            try await manager.waitForTestMessage("Ready")

            try await seedFirstPartyState(
                manager,
                configuration: configuration,
                url: configuration.localhostAddress.appending(path: "seed"),
                sourceLocation: sourceLocation
            )

            let results = try await loadExtensionPageAndCollectFrameReports(manager, sourceLocation: sourceLocation)

            #expect(configuration.lastRequestCookies == "key=lax-cookie", sourceLocation: sourceLocation)
            #expect(results["redirected"]?["cookie"] as? String == "key=lax-cookie", sourceLocation: sourceLocation)
        }
    }

    @Test
    func redirectIntoHostPermittedFrameIsUnpartitioned() async throws {
        try await runRedirectIntoHostPermittedFrameTest(siteIsolation: false)
    }

    @Test
    func redirectIntoHostPermittedFrameIsUnpartitionedWithSiteIsolation() async throws {
        try await runRedirectIntoHostPermittedFrameTest(siteIsolation: true)
    }

    @Test
    func aboutBlankChildOfHostPermittedFrameSeesItsCookies() async throws {
        let reportAboutBlankChildToParentScript = """
            <body><script>\
            const child = document.body.appendChild(document.createElement('iframe'));\
            parent.postMessage({ host: location.hostname, cookie: child.contentDocument.cookie }, '*')\
            </script></body>
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/seed", headerFields: ["Content-Type": "text/html", "Set-Cookie": "key=cookie; Path=/"]) {
                "<script>fetch('/seeded')</script>"
            }
            Route("/seeded", headerFields: ["Content-Type": "text/plain"]) {
                ""
            }
            Route("/read", headerFields: ["Content-Type": "text/html"]) {
                reportAboutBlankChildToParentScript
            }
        }

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifestWithLocalhostHostPermission, resources: extensionResources(configuration))
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress.appending(path: "read"))

            try await manager.waitForTestMessage("Ready")

            try await seedFirstPartyState(
                manager,
                configuration: configuration,
                url: configuration.localhostAddress.appending(path: "seed")
            )
            try await seedFirstPartyState(
                manager,
                configuration: configuration,
                url: configuration.address.appending(path: "seed")
            )

            let results = try await loadExtensionPageAndCollectFrameReports(manager)

            #expect(results["localhost"]?["cookie"] as? String == "key=cookie")
            #expect(results["127.0.0.1"]?["cookie"] as? String == "")
        }
    }

    /// The resources of an extension whose page embeds a frame for the URL, and sends `Checked` once the frame posts
    /// `Checked` to it.
    private func extensionResourcesEmbeddingFrame(_ frameURL: String) -> [String: Any] {
        let extensionPageScript = """
            window.addEventListener('message', (event) => {
              if (event.data === 'Checked')
                browser.test.sendMessage('Checked')
            })

            const frame = document.createElement('iframe')
            frame.src = '\(frameURL)'
            document.body.appendChild(frame)
            """

        let backgroundScript = """
            browser.test.sendMessage('Ready')
            """

        return [
            "background.js": backgroundScript,
            "extension-page.js": extensionPageScript,
            "extension-page.html": "<body><script type='module' src='extension-page.js'></script></body>",
        ]
    }

    /// Loads the extension page in the default tab, and waits for it to send `Checked`.
    private func loadExtensionPageAndWaitForCheck(
        _ manager: TestWebExtensionManager,
        sourceLocation: SourceLocation = #_sourceLocation
    ) async throws {
        try loadExtensionPage(manager, sourceLocation: sourceLocation)

        try await manager.waitForTestMessage("Checked")
    }

    @Test
    func crossSiteNavigationOfHostPermittedFrameDoesNotSendStrictCookies() async throws {
        let checkedScript = "<script>parent.postMessage('Checked', '*')</script>"

        var server = try HTTPServer(protocol: .http) {
            Route("/seed", headerFields: ["Content-Type": "text/html", "Set-Cookie": "key=strict-cookie; SameSite=Strict; Path=/"]) {
                "<script>fetch('/seeded')</script>"
            }
            Route("/seeded", headerFields: ["Content-Type": "text/plain"]) {
                ""
            }
            Route("/idle", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
            Route("/check", headerFields: ["Content-Type": "text/html"]) {
                checkedScript
            }
        }

        try await server.run { configuration in
            let extensionPageScript = """
                window.addEventListener('message', (event) => {
                  if (event.data === 'Checked')
                    browser.test.sendMessage('Checked')
                })

                const frame = document.createElement('iframe')
                frame.onload = () => { frame.onload = null; frame.src = 'http://localhost:\(configuration.port)/check' }
                frame.src = 'http://localhost:\(configuration.port)/idle'
                document.body.appendChild(frame)
                """

            let backgroundScript = """
                browser.test.sendMessage('Ready')
                """

            let resources = [
                "background.js": backgroundScript,
                "extension-page.js": extensionPageScript,
                "extension-page.html": "<body><script type='module' src='extension-page.js'></script></body>",
            ]

            let manager = try loadWebExtension(manifest: manifestWithLocalhostHostPermission, resources: resources)
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress.appending(path: "check"))

            try await manager.waitForTestMessage("Ready")

            try await seedFirstPartyState(
                manager,
                configuration: configuration,
                url: configuration.localhostAddress.appending(path: "seed")
            )

            try await loadExtensionPageAndWaitForCheck(manager)

            #expect(configuration.lastRequestCookies == "")
        }
    }

    @Test
    func sameSiteNavigationOfHostPermittedFrameSendsStrictCookies() async throws {
        let checkedScript = "<script>parent.postMessage('Checked', '*')</script>"

        var server = try HTTPServer(protocol: .http) {
            Route("/seed", headerFields: ["Content-Type": "text/html", "Set-Cookie": "key=strict-cookie; SameSite=Strict; Path=/"]) {
                "<script>fetch('/seeded')</script>"
            }
            Route("/seeded", headerFields: ["Content-Type": "text/plain"]) {
                ""
            }
            Route("/navigate", headerFields: ["Content-Type": "text/html"]) {
                "<script>location.href = '/check'</script>"
            }
            Route("/check", headerFields: ["Content-Type": "text/html"]) {
                checkedScript
            }
        }

        try await server.run { configuration in
            let permittedURL = "http://localhost:\(configuration.port)/navigate"

            let manager = try loadWebExtension(
                manifest: manifestWithLocalhostHostPermission,
                resources: extensionResourcesEmbeddingFrame(permittedURL)
            )
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress.appending(path: "check"))

            try await manager.waitForTestMessage("Ready")

            try await seedFirstPartyState(
                manager,
                configuration: configuration,
                url: configuration.localhostAddress.appending(path: "seed")
            )

            try await loadExtensionPageAndWaitForCheck(manager)

            #expect(configuration.lastRequestCookies == "key=strict-cookie")
        }
    }

    @Test
    func nonExtensionPageDoesNotGiveSubframesUnpartitionedAccess() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/seed", headerFields: ["Content-Type": "text/html", "Set-Cookie": "key=lax-cookie; SameSite=Lax; Path=/"]) {
                ""
            }
            Route("/read", headerFields: ["Content-Type": "text/html"]) {
                "<script>top.postMessage(document.cookie, '*')</script>"
            }
        }

        try await server.run { configuration in
            configuration.addResponses {
                Route("/main", headerFields: ["Content-Type": "text/html"]) {
                    "<script>onmessage = (event) => alert(event.data)</script><iframe src='http://localhost:\(configuration.port)/read'></iframe>"
                }
            }

            let webViewConfiguration = WKWebViewConfiguration()
            webViewConfiguration.websiteDataStore = .nonPersistent()
            webViewConfiguration._shouldRelaxThirdPartyCookieBlocking = true
            webViewConfiguration._corsDisablingPatterns = ["*://localhost/*"]

            let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: webViewConfiguration)
            let alerts = AlertRecorder()
            webView.uiDelegate = alerts

            try await webView.loadAndWait(URLRequest(url: configuration.localhostAddress.appending(path: "seed")))
            webView.load(URLRequest(url: configuration.address.appending(path: "main")))

            #expect(try await alerts.nextAlert() == "")
        }
    }

    /// A non-persistent controller configuration whose data store sends every HTTPS load to the server, which acts as a
    /// proxy for them.
    private func controllerConfigurationWithHTTPSProxy(
        _ configuration: HTTPServer.Configuration
    ) -> WKWebExtensionController.Configuration {
        let storeConfiguration = _WKWebsiteDataStoreConfiguration(nonPersistentConfiguration: ())
        storeConfiguration.httpsProxy = configuration.httpsProxy

        let dataStore = WKWebsiteDataStore._store(with: storeConfiguration)
        dataStore._resourceLoadStatisticsEnabled = true

        let controllerConfiguration = WKWebExtensionController.Configuration.nonPersistent()
        controllerConfiguration.defaultWebsiteDataStore = dataStore
        return controllerConfiguration
    }

    private func manifestWithHostPermission(_ hostPermission: String) -> [String: Any] {
        var manifest = manifestWithLocalhostHostPermission
        manifest["host_permissions"] = [hostPermission]
        return manifest
    }

    private func runSameSiteNavigationToOtherHostOfHostPermittedFrameTest(
        siteIsolation: Bool,
        sourceLocation: SourceLocation = #_sourceLocation
    ) async throws {
        let previousShouldEnableSiteIsolation = TestWebExtensionManager.shouldEnableSiteIsolation
        TestWebExtensionManager.shouldEnableSiteIsolation = siteIsolation
        defer { TestWebExtensionManager.shouldEnableSiteIsolation = previousShouldEnableSiteIsolation }

        var server = try HTTPServer(protocol: .httpsProxy) {
            Route(
                "/seed",
                headerFields: [
                    "Content-Type": "text/html",
                    "Set-Cookie": "key=strict-cookie; SameSite=Strict; Secure; Domain=a.com; Path=/",
                ]
            ) {
                "<script>fetch('/seeded')</script>"
            }
            Route("/seeded", headerFields: ["Content-Type": "text/plain"]) {
                ""
            }
            Route("/navigate", headerFields: ["Content-Type": "text/html"]) {
                "<script>location.href = 'https://nested.a.com/read'</script>"
            }
            Route("/read", headerFields: ["Content-Type": "text/html"]) {
                reportNamedFrameCookieToTopScript
            }
        }

        try await server.run { configuration in
            let resources = extensionResourcesEmbeddingNamedFrames([
                "navigated": "https://a.com/navigate"
            ])

            let manager = try loadWebExtension(
                manifest: manifestWithHostPermission("*://a.com/*"),
                resources: resources,
                configuration: controllerConfigurationWithHTTPSProxy(configuration)
            )
            let context = try #require(manager.context, sourceLocation: sourceLocation)

            let matchPattern = try #require(
                WKWebExtension.MatchPattern.cachedPattern(string: "*://a.com/*"),
                sourceLocation: sourceLocation
            )
            context.setPermissionStatus(.grantedExplicitly, for: matchPattern)

            try await manager.waitForTestMessage("Ready")

            let seedURL = try #require(URL(string: "https://a.com/seed"), sourceLocation: sourceLocation)
            try await seedFirstPartyState(manager, configuration: configuration, url: seedURL, sourceLocation: sourceLocation)

            let results = try await loadExtensionPageAndCollectFrameReports(manager, sourceLocation: sourceLocation)

            #expect(configuration.lastRequestCookies == "key=strict-cookie", sourceLocation: sourceLocation)
            #expect(results["navigated"]?["cookie"] as? String == "key=strict-cookie", sourceLocation: sourceLocation)
        }
    }

    @Test
    func sameSiteNavigationToOtherHostOfHostPermittedFrameIsUnpartitioned() async throws {
        try await runSameSiteNavigationToOtherHostOfHostPermittedFrameTest(siteIsolation: false)
    }

    @Test
    func sameSiteNavigationToOtherHostOfHostPermittedFrameIsUnpartitionedWithSiteIsolation() async throws {
        try await runSameSiteNavigationToOtherHostOfHostPermittedFrameTest(siteIsolation: true)
    }

    // WebKit guards its partitioned-cookie code with CFN_COOKIE_ACCEPTS_POLICY_PARTITION, which isn't exported to Swift.
    // Source/WTF/wtf/PlatformHave.h defines it exactly when HAVE_ALLOW_ONLY_PARTITIONED_COOKIES is defined, unless the
    // SDK defines it first.
    #if ENABLE_OPT_IN_PARTITIONED_COOKIES && HAVE_ALLOW_ONLY_PARTITIONED_COOKIES

    @Test
    func redirectOutOfHostPermittedFrameDoesNotUseItsCookiePartition() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/redirect-to-tracker", statusCode: 302, headerFields: ["Location": "https://tracker.com/redirected"]) {
                "redirecting..."
            }
            Route("/redirected", headerFields: ["Content-Type": "text/html"]) {
                reportNamedFrameToTopScript
            }
        }

        try await server.run { configuration in
            let resources = extensionResourcesEmbeddingNamedFrames([
                "redirected": "https://a.com/redirect-to-tracker"
            ])

            let manager = try loadWebExtension(
                manifest: manifestWithHostPermission("*://a.com/*"),
                resources: resources,
                configuration: controllerConfigurationWithHTTPSProxy(configuration)
            )
            let context = try #require(manager.context)

            let matchPattern = try #require(WKWebExtension.MatchPattern.cachedPattern(string: "*://a.com/*"))
            context.setPermissionStatus(.grantedExplicitly, for: matchPattern)

            try await manager.waitForTestMessage("Ready")

            let partitionedCookie = try #require(
                HTTPCookie(properties: [
                    .name: "key",
                    .value: "partitioned-cookie",
                    .domain: "tracker.com",
                    .path: "/",
                    .secure: "TRUE",
                    HTTPCookiePropertyKey("StoragePartition"): "https://a.com",
                ])
            )

            let dataStore = try #require(manager.controller.configuration.defaultWebsiteDataStore)
            await dataStore.httpCookieStore.setCookie(partitionedCookie)

            try await loadExtensionPageAndCollectFrameReports(manager)

            #expect(configuration.lastRequestCookies == "")
        }
    }

    #endif // ENABLE_OPT_IN_PARTITIONED_COOKIES && HAVE_ALLOW_ONLY_PARTITIONED_COOKIES

    // MARK: - Storage

    private let writeLocalStorageScript = "localStorage.setItem('key', 'local-storage-value')"
    private let readLocalStorageScript = "return localStorage.getItem('key') ?? ''"

    private let writeIndexedDBScript = """
        const database = await new Promise((resolve, reject) => {\
          const request = indexedDB.open('database');\
          request.onupgradeneeded = () => request.result.createObjectStore('store');\
          request.onsuccess = () => resolve(request.result);\
          request.onerror = () => reject(request.error);\
        });\
        const transaction = database.transaction('store', 'readwrite');\
        transaction.objectStore('store').put('indexeddb-value', 'key');\
        await new Promise((resolve, reject) => {\
          transaction.oncomplete = resolve;\
          transaction.onerror = () => reject(transaction.error);\
        });\
        database.close();
        """
    private let readIndexedDBScript = """
        if (!(await indexedDB.databases()).some(database => database.name === 'database'))\
          return '';\
        const database = await new Promise((resolve, reject) => {\
          const request = indexedDB.open('database');\
          request.onsuccess = () => resolve(request.result);\
          request.onerror = () => reject(request.error);\
        });\
        const request = database.transaction('store').objectStore('store').get('key');\
        const value = await new Promise((resolve, reject) => {\
          request.onsuccess = () => resolve(request.result);\
          request.onerror = () => reject(request.error);\
        });\
        database.close();\
        return value ?? '';
        """

    private let writeCacheStorageScript = "await (await caches.open('cache')).put('/cached', new Response('cache-storage-value'))"
    private let readCacheStorageScript = """
        const response = await caches.match('/cached');\
        return response ? await response.text() : '';
        """

    private let writeFileSystemScript = """
        const root = await navigator.storage.getDirectory();\
        const writable = await (await root.getFileHandle('file', { create: true })).createWritable();\
        await writable.write('file-system-value');\
        await writable.close();
        """
    private let readFileSystemScript = """
        const root = await navigator.storage.getDirectory();\
        try {\
          return await (await (await root.getFileHandle('file')).getFile()).text();\
        } catch (error) {\
          if (error.name === 'NotFoundError')\
            return '';\
          throw error;\
        }
        """

    private let holdWebLockScript =
        "await new Promise(resolve => navigator.locks.request('lock', () => { resolve(); return new Promise(() => { }) }))"
    private let readHeldWebLocksScript = "return (await navigator.locks.query()).held.map(lock => lock.name).join(', ')"

    /// Wraps an asynchronous function body in a script that runs it, and then runs `report` with the value it returned,
    /// or the error it threw as a string, in `value`.
    private func scriptReportingResult(_ script: String, report: String) -> String {
        """
        let value;\
        try {\
          value = await (async () => { \(script) })();\
        } catch (error) {\
          value = String(error);\
        }\(report)
        """
    }

    /// Serves an empty page at `/` and a `/read` page that runs `frameScript` and posts the result to its parent, along
    /// with its `host`, and calls `body` while the server runs.
    ///
    /// Given a `workerScript`, the server also serves `/worker.js`, a worker that runs it and posts the result, and
    /// `/shared-worker.js`, a shared worker that runs it and posts the result to each port that connects.
    private func withStorageTestServer(
        frameScript: String,
        workerScript: String? = nil,
        _ body: (HTTPServer.Configuration) async throws -> Void
    ) async throws {
        let readPageScript = scriptReportingResult(frameScript, report: "parent.postMessage({ host: location.hostname, value }, '*')")

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
            Route("/read", headerFields: ["Content-Type": "text/html"]) {
                "<script type='module'>\(readPageScript)</script>"
            }
        }

        try await server.run { configuration in
            if let workerScript {
                let sharedWorkerResultScript = scriptReportingResult(workerScript, report: "return value;")

                configuration.addResponses {
                    Route("/worker.js", headerFields: ["Content-Type": "text/javascript"]) {
                        scriptReportingResult(workerScript, report: "postMessage(value)")
                    }
                    Route("/shared-worker.js", headerFields: ["Content-Type": "text/javascript"]) {
                        """
                        const result = (async () => { \(sharedWorkerResultScript) })();\
                        onconnect = async event => event.ports[0].postMessage(await result);
                        """
                    }
                }
            }

            try await body(configuration)
        }
    }

    /// A persistent data store in a new temporary directory, so no test sees another's cookies, storage or
    /// service workers.
    private func makeTemporaryPersistentDataStore() throws -> WKWebsiteDataStore {
        let directory = temporaryWebsiteDataDirectory.appending(path: UUID().uuidString, directoryHint: .isDirectory)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        return WKWebsiteDataStore._store(with: _WKWebsiteDataStoreConfiguration(directory: directory))
    }

    /// A non-persistent controller configuration whose default data store is a new persistent one.
    private func controllerConfigurationWithPersistentDataStore() throws -> WKWebExtensionController.Configuration {
        let controllerConfiguration = WKWebExtensionController.Configuration.nonPersistent()
        controllerConfiguration.defaultWebsiteDataStore = try makeTemporaryPersistentDataStore()
        return controllerConfiguration
    }

    /// Loads an extension with the resources ``extensionResources(_:)`` makes, using a new persistent data store, grants
    /// it access to the server's `localhost` host, and waits for it to send `Ready`.
    private func loadStorageTestExtension(
        _ configuration: HTTPServer.Configuration,
        sourceLocation: SourceLocation = #_sourceLocation
    ) async throws -> TestWebExtensionManager {
        let manager = try loadWebExtension(
            manifest: manifestWithLocalhostHostPermission,
            resources: extensionResources(configuration),
            configuration: controllerConfigurationWithPersistentDataStore()
        )

        let context = try #require(manager.context, sourceLocation: sourceLocation)
        context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress.appending(path: "read"))

        try await manager.waitForTestMessage("Ready")

        return manager
    }

    /// Opens a page in a new web view that uses the extension's data store, and calls `script` in it as an asynchronous
    /// function body.
    private func loadFirstPartyPage(
        _ manager: TestWebExtensionManager,
        url: URL,
        script: String,
        sourceLocation: SourceLocation = #_sourceLocation
    ) async throws -> TestWKWebView {
        let webViewConfiguration = WKWebViewConfiguration()
        webViewConfiguration.websiteDataStore = try #require(
            manager.controller.configuration.defaultWebsiteDataStore,
            sourceLocation: sourceLocation
        )

        let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: webViewConfiguration)
        try await webView.loadAndWait(URLRequest(url: url))

        try await webView.callJavaScript { script }

        return webView
    }

    private func runUnpartitionedStorageTest(
        firstPartyScript: String,
        frameScript: String,
        expectedValue: String,
        workerScript: String? = nil,
        sourceLocation: SourceLocation = #_sourceLocation
    ) async throws {
        try await withStorageTestServer(frameScript: frameScript, workerScript: workerScript) { configuration in
            let manager = try await loadStorageTestExtension(configuration, sourceLocation: sourceLocation)

            let permittedFirstPartyWebView = try await loadFirstPartyPage(
                manager,
                url: configuration.localhostAddress,
                script: firstPartyScript,
                sourceLocation: sourceLocation
            )
            let otherFirstPartyWebView = try await loadFirstPartyPage(
                manager,
                url: configuration.address,
                script: firstPartyScript,
                sourceLocation: sourceLocation
            )
            // Some of what the pages write, such as a held web lock, lasts only as long as the page.
            defer { withExtendedLifetime((permittedFirstPartyWebView, otherFirstPartyWebView)) {} }

            let results = try await loadExtensionPageAndCollectFrameReports(manager, sourceLocation: sourceLocation)

            #expect(results["localhost"]?["value"] as? String == expectedValue, sourceLocation: sourceLocation)
            #expect(results["127.0.0.1"]?["value"] as? String == "", sourceLocation: sourceLocation)
        }
    }

    @Test
    func localStorageIsUnpartitionedInHostPermittedFrame() async throws {
        try await runUnpartitionedStorageTest(
            firstPartyScript: writeLocalStorageScript,
            frameScript: readLocalStorageScript,
            expectedValue: "local-storage-value"
        )
    }

    @Test
    func indexedDBIsUnpartitionedInHostPermittedFrame() async throws {
        try await runUnpartitionedStorageTest(
            firstPartyScript: writeIndexedDBScript,
            frameScript: readIndexedDBScript,
            expectedValue: "indexeddb-value"
        )
    }

    @Test
    func cacheStorageIsUnpartitionedInHostPermittedFrame() async throws {
        try await runUnpartitionedStorageTest(
            firstPartyScript: writeCacheStorageScript,
            frameScript: readCacheStorageScript,
            expectedValue: "cache-storage-value"
        )
    }

    @Test
    func fileSystemIsUnpartitionedInHostPermittedFrame() async throws {
        try await runUnpartitionedStorageTest(
            firstPartyScript: writeFileSystemScript,
            frameScript: readFileSystemScript,
            expectedValue: "file-system-value"
        )
    }

    @Test
    func webLocksAreUnpartitionedInHostPermittedFrame() async throws {
        try await runUnpartitionedStorageTest(
            firstPartyScript: holdWebLockScript,
            frameScript: readHeldWebLocksScript,
            expectedValue: "lock"
        )
    }

    @Test
    func dedicatedWorkerOfHostPermittedFrameIsUnpartitioned() async throws {
        let workerScript =
            "return [await (async () => { \(readIndexedDBScript) })(), await (async () => { \(readFileSystemScript) })()].filter(Boolean).join(', ')"
        let frameScript =
            "return await new Promise(resolve => new Worker('/worker.js', { type: 'module' }).onmessage = event => resolve(event.data))"

        try await runUnpartitionedStorageTest(
            firstPartyScript: writeIndexedDBScript + writeFileSystemScript,
            frameScript: frameScript,
            expectedValue: "indexeddb-value, file-system-value",
            workerScript: workerScript
        )
    }

    @Test
    func sharedWorkerOfHostPermittedFrameIsUnpartitioned() async throws {
        let workerScript =
            "return [await (async () => { \(readIndexedDBScript) })(), await (async () => { \(readCacheStorageScript) })()].filter(Boolean).join(', ')"
        let frameScript = """
            return await new Promise(resolve => {\
              const worker = new SharedWorker('/shared-worker.js');\
              worker.onerror = () => resolve('error');\
              worker.port.onmessage = event => resolve(event.data);\
            })
            """

        try await runUnpartitionedStorageTest(
            firstPartyScript: writeIndexedDBScript + writeCacheStorageScript,
            frameScript: frameScript,
            expectedValue: "indexeddb-value, cache-storage-value",
            workerScript: workerScript
        )
    }

    @Test
    func serviceWorkerControlsHostPermittedFrame() async throws {
        let fetchFromServiceWorkerScript = "return await (await fetch('/from-service-worker')).text()"
        let registerServiceWorkerScript = """
            await navigator.serviceWorker.register('/service-worker.js');\
            await navigator.serviceWorker.ready;
            """

        let frameScript = """
            const frameValue = await (async () => { \(fetchFromServiceWorkerScript) })();\
            const workerValue = await new Promise(resolve => new Worker('/worker.js', { type: 'module' }).onmessage = event => resolve(event.data));\
            return [frameValue, workerValue].filter(Boolean).join(', ')
            """

        try await withStorageTestServer(frameScript: frameScript, workerScript: fetchFromServiceWorkerScript) { configuration in
            configuration.addResponses {
                Route("/from-service-worker", headerFields: ["Content-Type": "text/plain"]) {
                    ""
                }
                Route("/service-worker.js", headerFields: ["Content-Type": "text/javascript"]) {
                    """
                    self.addEventListener('install', () => self.skipWaiting());\
                    self.addEventListener('activate', event => event.waitUntil(self.clients.claim()));\
                    self.addEventListener('fetch', event => {\
                      if (new URL(event.request.url).pathname === '/from-service-worker')\
                        event.respondWith(new Response('service-worker-value'));\
                    });
                    """
                }
            }

            let manager = try await loadStorageTestExtension(configuration)

            let permittedFirstPartyWebView = try await loadFirstPartyPage(
                manager,
                url: configuration.localhostAddress,
                script: registerServiceWorkerScript
            )
            let otherFirstPartyWebView = try await loadFirstPartyPage(
                manager,
                url: configuration.address,
                script: registerServiceWorkerScript
            )
            defer { withExtendedLifetime((permittedFirstPartyWebView, otherFirstPartyWebView)) {} }

            let results = try await loadExtensionPageAndCollectFrameReports(manager)

            #expect(results["localhost"]?["value"] as? String == "service-worker-value, service-worker-value")
            #expect(results["127.0.0.1"]?["value"] as? String == "")
        }
    }

    /// Copies a web view's session storage into another web view.
    private func copySessionStorage(from fromWebView: WKWebView, to toWebView: WKWebView) async throws {
        let sessionStorageData = try await fromWebView.fetchData(of: .sessionStorage)

        try await toWebView.restoreData(sessionStorageData)
    }

    @Test
    func sessionStorageIsUnpartitionedInHostPermittedFrame() async throws {
        try await withStorageTestServer(frameScript: "return sessionStorage.getItem('key') ?? ''") { configuration in
            let manager = try await loadStorageTestExtension(configuration)
            let context = try #require(manager.context)
            let tab = try #require(manager.defaultTab)

            let extensionPageURL = try #require(URL(string: "extension-page.html", relativeTo: context.baseURL))
            tab.changeWebViewIfNeeded(for: extensionPageURL, for: context)

            for url in [configuration.localhostAddress, configuration.address] {
                let firstPartyWebView = try await loadFirstPartyPage(
                    manager,
                    url: url,
                    script: "sessionStorage.setItem('key', 'session-storage-value'); await navigator.storage.estimate()"
                )
                try await copySessionStorage(from: firstPartyWebView, to: #require(tab.webView))
            }

            try #require(tab.webView).load(URLRequest(url: extensionPageURL))

            let results = try await waitForFrameReports(manager)

            #expect(results["localhost"]?["value"] as? String == "session-storage-value")
            #expect(results["127.0.0.1"]?["value"] as? String == "")
        }
    }

    @Test
    func broadcastChannelIsUnpartitionedInHostPermittedFrame() async throws {
        let listenScript = """
            window.receivedMessages = [];\
            window.firstMessage = new Promise(resolve => {\
              window.channel = new BroadcastChannel('channel');\
              channel.onmessage = event => {\
                receivedMessages.push(event.data);\
                resolve(event.data);\
              };\
            });
            """

        let frameScript = "window.channel = new BroadcastChannel('channel'); channel.postMessage(location.hostname); return ''"

        try await withStorageTestServer(frameScript: frameScript) { configuration in
            let manager = try await loadStorageTestExtension(configuration)

            let permittedFirstPartyWebView = try await loadFirstPartyPage(
                manager,
                url: configuration.localhostAddress,
                script: listenScript
            )
            let otherFirstPartyWebView = try await loadFirstPartyPage(
                manager,
                url: configuration.address,
                script: listenScript
            )

            try await loadExtensionPageAndCollectFrameReports(manager)

            #expect(
                try await permittedFirstPartyWebView.callJavaScript(returning: String.self) { "return await firstMessage" } == "localhost"
            )
            // Evaluate without a user gesture, as objectByEvaluatingJavaScript: does.
            #expect(try await otherFirstPartyWebView._evaluateJavaScriptWithoutUserGesture("receivedMessages.join(', ')") as? String == "")
        }
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
