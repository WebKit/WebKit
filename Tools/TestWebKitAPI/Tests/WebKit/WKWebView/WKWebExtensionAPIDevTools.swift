// Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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

#if ENABLE_WK_WEB_EXTENSIONS && ENABLE_INSPECTOR_EXTENSIONS

import Foundation
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.WebExtensionUtilities
import Testing
import WebKit
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private._WKInspector
private import WebKit_Private._WKInspectorExtensionHost
private import WebKit_Private._WKInspectorIBActions
private import WebKit_Private._WKInspectorPrivate

import struct Swift.String

#if WTF_PLATFORM_MAC
private import AppKit
#endif

@MainActor
struct WKWebExtensionAPIDevToolsTests {
    private let devToolsManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "Test DevTools",
        "description": "Test DevTools",
        "version": "1.0",

        "permissions": ["storage"],

        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],

        "devtools_page": "devtools.html",
    ]

    /// Starts showing the extension's DevTools panel.
    ///
    /// Await the returned task after waiting for the extension's own message, so its error is checked without
    /// changing the order of events.
    private func startShowingExtensionPanel(
        for manager: TestWebExtensionManager,
        sourceLocation: SourceLocation = #_sourceLocation
    ) throws -> Task<Void, any Error> {
        let context = try #require(manager.context, sourceLocation: sourceLocation)
        let inspector = try #require(manager.defaultTab?.webView?._inspector, sourceLocation: sourceLocation)
        let identifier = "WebExtensionTab-\(context.uniqueIdentifier)-1"
        // The task inherits the main actor, so the _WKInspector SPI is called on the main thread. An `async let`
        // would run it off the main thread.
        return Task {
            try await inspector.showExtensionTab(withIdentifier: identifier)
        }
    }

    @Test
    func basics() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.test.assertEq(browser?.devtools, undefined, 'browser.devtools should be undefined in background script')
            """

        let devToolsScript = """
            browser.test.assertEq(typeof browser?.devtools, 'object', 'browser.devtools should be available in devtools page script')
            browser.test.assertEq(typeof browser?.devtools?.panels, 'object', 'browser.devtools.panels should be available')
            browser.test.assertEq(typeof browser?.devtools?.network, 'object', 'browser.devtools.network should be available')
            browser.test.assertEq(typeof browser?.devtools?.inspectedWindow, 'object', 'browser.devtools.inspectedWindow should be available')
            browser.test.assertEq(typeof browser?.devtools?.inspectedWindow?.tabId, 'number', 'browser.devtools.inspectedWindow.tabId should be available')
            browser.test.assertTrue(browser?.devtools?.inspectedWindow?.tabId > 0, 'browser.devtools.inspectedWindow.tabId should be a positive value')

            browser.test.notifyPass()
            """

        let resources = [
            "background.js": backgroundScript,
            "devtools.html": "<script type='module' src='devtools.js'></script>",
            "devtools.js": devToolsScript,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: devToolsManifest, resources: resources)

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))
            try #require(webView._inspector).show()

            try await manager.run()
        }
    }

    @Test
    func storageAccessLevel() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let devToolsScript = """
            // A devtools page is a trusted extension context so all storage areas should be available.
            browser.test.assertEq(typeof browser.storage.local, 'object', 'browser.storage.local should be available in a devtools page')
            browser.test.assertEq(typeof browser.storage.session, 'object', 'browser.storage.session should be available in a devtools page')
            browser.test.assertEq(typeof browser.storage.sync, 'object', 'browser.storage.sync should be available in a devtools page')

            browser.test.assertEq(typeof browser.storage.local.setAccessLevel, 'function', 'storage.local.setAccessLevel should be available in a devtools page')
            browser.test.assertEq(typeof browser.storage.session.setAccessLevel, 'function', 'storage.session.setAccessLevel should be available in a devtools page')
            browser.test.assertEq(typeof browser.storage.sync.setAccessLevel, 'function', 'storage.sync.setAccessLevel should be available in a devtools page')


            browser.test.notifyPass()
            """

        let resources = [
            "background.js": "// This script is intentionally left blank.",
            "devtools.html": "<script type='module' src='devtools.js'></script>",
            "devtools.js": devToolsScript,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: devToolsManifest, resources: resources)

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))
            try #require(webView._inspector).show()

            try await manager.run()
        }
    }

    // FIXME: rdar://137268889 (4x TestWebKitAPI.WKWebExtensionAPIDevTools* (api-tests) are near constant timeouts)
    @Test(.disabled("rdar://137268889"))
    func createPanel() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let devToolsScript = """
            let cachedPanelWindow = null

            let panel = await browser.test.assertSafeResolve(() => browser.devtools.panels.create('Test Panel', 'icon.svg', 'panel.html'))
            browser.test.assertEq(typeof panel, 'object', 'panel should be an object')

            panel?.onShown.addListener((panelWindow) => {
              browser.test.assertEq(typeof panelWindow, 'object', 'panelWindow should be an object')

              cachedPanelWindow = panelWindow
            })

            panel?.onHidden.addListener(() => {
              browser.test.assertTrue(cachedPanelWindow !== null, 'panel should be shown first')
              browser.test.assertEq(typeof cachedPanelWindow?.notifyHidden, 'function', 'cachedPanelWindow should have the notifyHidden function')

              cachedPanelWindow?.notifyHidden()
            })

            browser.test.sendMessage('Panel Created')
            """

        let panelScript = """
            browser.test.assertEq(typeof browser?.devtools?.inspectedWindow?.tabId, 'number', 'browser.devtools.inspectedWindow.tabId should be available')
            browser.test.assertTrue(browser?.devtools?.inspectedWindow?.tabId > 0, 'browser.devtools.inspectedWindow.tabId should be a positive value')

            window.notifyHidden = () => {
              browser.test.sendMessage('Panel Hidden')
            }

            browser.test.sendMessage('Panel Loaded')
            """

        let iconSVG = "<svg width='16' height='16' xmlns='http://www.w3.org/2000/svg'><circle cx='8' cy='8' r='8' fill='red' /></svg>"

        let resources = [
            "background.js": "// This script is intentionally left blank.",
            "devtools.html": "<script type='module' src='devtools.js'></script>",
            "devtools.js": devToolsScript,
            "panel.html": "<script type='module' src='panel.js'></script>",
            "panel.js": panelScript,
            "icon.svg": iconSVG,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: devToolsManifest, resources: resources)

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))
            let inspector = try #require(webView._inspector)
            inspector.show()

            try await manager.waitForTestMessage("Panel Created")

            let showPanel = try startShowingExtensionPanel(for: manager)
            try await manager.waitForTestMessage("Panel Loaded")

            inspector.showResources()

            try await manager.waitForTestMessage("Panel Hidden")
            try await showPanel.value
        }
    }

    // FIXME: rdar://137268889 (4x TestWebKitAPI.WKWebExtensionAPIDevTools* (api-tests) are near constant timeouts)
    @Test(.disabled("rdar://137268889"))
    func inspectedWindowEval() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<title>Test Page</title><script>const secretNumber = 42; window.customTitle = 'Dynamic Title'</script>"
            }
        }

        let devToolsScript = """
            // Evaluate a simple expression that should succeed
            await browser.test.assertSafeResolve(async () => {
              const [result, exceptionInfo] = await browser.devtools.inspectedWindow.eval('document.title')
              browser.test.assertEq(result, 'Test Page', 'Evaluated document.title should match the title element')
            })

            // Evaluate an expression that should result in an exception
            await browser.test.assertSafeResolve(async () => {
              const [result, exceptionInfo] = await browser.devtools.inspectedWindow.eval('nonExistentFunction();')
              browser.test.assertTrue(!!exceptionInfo, 'Eval should catch an exception for non-existent function')
            })

            // Evaluate a script that accesses a variable defined in the page
            await browser.test.assertSafeResolve(async () => {
              const [result, exceptionInfo] = await browser.devtools.inspectedWindow.eval('secretNumber')
              browser.test.assertEq(result, 42, 'Evaluated secretNumber should match the value defined in the page script')
            })

            // Evaluate a script to change a property and verify
            await browser.test.assertSafeResolve(async () => {
              const [result, exceptionInfo] = await browser.devtools.inspectedWindow.eval('window.customTitle = "Updated Title"; window.customTitle')
              browser.test.assertEq(result, 'Updated Title', 'The evaluated script should update the window.customTitle')
            })

            browser.test.notifyPass()
            """

        let resources = [
            "background.js": "// This script is intentionally left blank.",
            "devtools.html": "<script type='module' src='devtools.js'></script>",
            "devtools.js": devToolsScript,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: devToolsManifest, resources: resources)
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))
            try #require(webView._inspector).show()

            try await manager.run()
        }
    }

    @Test
    func inspectedWindowReload() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html", "Cache-Control": "max-age=3600"]) {
                ""
            }
        }

        let devToolsScript = """
            browser.test.assertSafe(() => browser.devtools.inspectedWindow.reload())

            browser.test.sendMessage('Reload Called')
            """

        let resources = [
            "background.js": "// This script is intentionally left blank.",
            "devtools.html": "<script type='module' src='devtools.js'></script>",
            "devtools.js": devToolsScript,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: devToolsManifest, resources: resources)

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))
            try #require(webView._inspector).show()

            try await manager.waitForTestMessage("Reload Called")

            try await Task.sleep(for: .seconds(3))
            try manager.checkCollectedFailures()

            // Reload without { ignoreCache: true } should hit the cache.
            // That isn't guaranteed, so allow for both results here.
            #expect(configuration.totalRequests == 1 || configuration.totalRequests == 2)
        }
    }

    // FIXME: rdar://137268889 (4x TestWebKitAPI.WKWebExtensionAPIDevTools* (api-tests) are near constant timeouts)
    @Test(.disabled("rdar://137268889"))
    func inspectedWindowReloadIgnoringCache() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html", "Cache-Control": "max-age=3600"]) {
                ""
            }
        }

        let devToolsScript = """
            browser.test.assertSafe(() => browser.devtools.inspectedWindow.reload({ ignoreCache: true }))

            browser.test.sendMessage('Reload Called')
            """

        let resources = [
            "background.js": "// This script is intentionally left blank.",
            "devtools.html": "<script type='module' src='devtools.js'></script>",
            "devtools.js": devToolsScript,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: devToolsManifest, resources: resources)

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))
            try #require(webView._inspector).show()

            try await manager.waitForTestMessage("Reload Called")

            try await Task.sleep(for: .seconds(3))
            try manager.checkCollectedFailures()

            // Two requests to the server are expected (original and reload).
            #expect(configuration.totalRequests == 2)
        }
    }

    @Test
    func networkNavigatedEvent() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let devToolsScript = """
            browser.devtools.network.onNavigated.addListener((url) => {
              browser.test.assertEq(typeof url, 'string', 'url should be a string')
              browser.test.assertTrue(url?.startsWith('http://127.0.0.1:'), 'url should be local IP address')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Next Page')
            """

        let resources = [
            "background.js": "// This script is intentionally left blank.",
            "devtools.html": "<script type='module' src='devtools.js'></script>",
            "devtools.js": devToolsScript,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: devToolsManifest, resources: resources)
            let context = try #require(manager.context)

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))
            try #require(webView._inspector).show()

            try await manager.waitForTestMessage("Load Next Page")

            context.setPermissionStatus(.grantedExplicitly, for: configuration.address)
            webView.load(URLRequest(url: configuration.address))

            try await manager.run()
        }
    }

    @Test
    func panelsThemeName() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let devToolsScript = """
            browser.test.assertEq(browser?.devtools?.panels?.themeName, 'light', 'panels.themeName should be light')

            browser?.devtools?.panels?.onThemeChanged.addListener((newTheme) => {
              browser.test.assertEq(newTheme, 'dark', 'newTheme should be dark')
              browser.test.assertEq(browser?.devtools?.panels?.themeName, 'dark', 'panels.themeName should be dark')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Change Theme')
            """

        let resources = [
            "background.js": "// This script is intentionally left blank.",
            "devtools.html": "<script type='module' src='devtools.js'></script>",
            "devtools.js": devToolsScript,
        ]

        // Force light mode for the app, so the switch to dark will trigger the event.
        let previousAppearance = NSApp.appearance
        defer { NSApp.appearance = previousAppearance }
        NSApp.appearance = NSAppearance(named: .aqua)

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: devToolsManifest, resources: resources)

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))
            let inspector = try #require(webView._inspector)
            inspector.show()

            try await manager.waitForTestMessage("Change Theme")

            // Force dark mode on the inspector to trigger the theme change.
            let extensionHostWebView = try #require(inspector.extensionHostWebView)
            extensionHostWebView.appearance = NSAppearance(named: .darkAqua)

            try await manager.run()
        }
    }

    @Test
    func messagePassingToBackground() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
              browser.test.assertEq(message, 'Hello from Inspector', 'Should get the correct message from the Inspector script')
              sendResponse('Acknowledged by background')
            })
            """

        let devToolsScript = """
            const response = await browser.runtime.sendMessage('Hello from Inspector')
            browser.test.assertEq(response, 'Acknowledged by background', 'Should get the correct response from the background script')

            browser.test.notifyPass()
            """

        let resources = [
            "background.js": backgroundScript,
            "devtools.html": "<script type='module' src='devtools.js'></script>",
            "devtools.js": devToolsScript,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: devToolsManifest, resources: resources)

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))
            try #require(webView._inspector).show()

            try await manager.run()
        }
    }

    @Test
    func messagePassingFromPanelToBackground() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
              browser.test.assertEq(message, 'Hello from Inspector panel', 'Should get the correct message from the pane script')
              sendResponse('Acknowledged by Inspector panel')
            })
            """

        let devToolsScript = """
            let panel = await browser.devtools.panels.create('Test Panel', 'icon.svg', 'panel.html')
            browser.test.assertEq(typeof panel, 'object', 'Panel should be created successfully')

            browser.test.sendMessage('Panel Created')
            """

        let panelScript = """
            const response = await browser.runtime.sendMessage('Hello from Inspector panel')
            browser.test.assertEq(response, 'Acknowledged by Inspector panel', 'Should get the correct response from the background script')

            browser.test.notifyPass()
            """

        let iconSVG = "<svg width='16' height='16' xmlns='http://www.w3.org/2000/svg'><circle cx='8' cy='8' r='8' fill='red' /></svg>"

        let resources = [
            "background.js": backgroundScript,
            "devtools.html": "<script type='module' src='devtools.js'></script>",
            "devtools.js": devToolsScript,
            "panel.html": "<script type='module' src='panel.js'></script>",
            "panel.js": panelScript,
            "icon.svg": iconSVG,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: devToolsManifest, resources: resources)

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))
            try #require(webView._inspector).show()

            try await manager.waitForTestMessage("Panel Created")

            let showPanel = try startShowingExtensionPanel(for: manager)
            try await manager.run()
            try await showPanel.value
        }
    }

    @Test
    func messagePassingFromPanelToDevToolsBackground() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let devToolsScript = """
            let panel = await browser.devtools.panels.create('Test Panel', 'icon.svg', 'panel.html')
            browser.test.assertEq(typeof panel, 'object', 'Panel should be created successfully')

            browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
              browser.test.assertEq(message, 'Hello from Inspector panel', 'Should get the correct message from the pane script')
              sendResponse('Acknowledged by Inspector background')
            })

            browser.test.sendMessage('Panel Created')
            """

        let panelScript = """
            const response = await browser.runtime.sendMessage('Hello from Inspector panel')
            browser.test.assertEq(response, 'Acknowledged by Inspector background', 'Should get the correct response from the Inspector background')

            browser.test.notifyPass()
            """

        let iconSVG = "<svg width='16' height='16' xmlns='http://www.w3.org/2000/svg'><circle cx='8' cy='8' r='8' fill='red' /></svg>"

        let resources = [
            "background.js": "// This script is intentionally left blank.",
            "devtools.html": "<script type='module' src='devtools.js'></script>",
            "devtools.js": devToolsScript,
            "panel.html": "<script type='module' src='panel.js'></script>",
            "panel.js": panelScript,
            "icon.svg": iconSVG,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: devToolsManifest, resources: resources)

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))
            try #require(webView._inspector).show()

            try await manager.waitForTestMessage("Panel Created")

            let showPanel = try startShowingExtensionPanel(for: manager)
            try await manager.run()
            try await showPanel.value
        }
    }

    // FIXME: rdar://137268889 (4x TestWebKitAPI.WKWebExtensionAPIDevTools* (api-tests) are near constant timeouts)
    @Test(.disabled("rdar://137268889"))
    func portMessagePassingToBackground() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.runtime.onConnect.addListener(port => {
              port.onMessage.addListener((message) => {
                browser.test.assertEq(message, 'Hello from Inspector', 'Should get the correct message from the Inspector script')
                port.postMessage('Acknowledged by background')
              })
            })
            """

        let devToolsScript = """
            const port = browser.runtime.connect()
            port.postMessage('Hello from Inspector')

            port.onMessage.addListener((response) => {
              browser.test.assertEq(response, 'Acknowledged by background', 'Should get the correct response from the background script')

              browser.test.notifyPass()
            })
            """

        let resources = [
            "background.js": backgroundScript,
            "devtools.html": "<script type='module' src='devtools.js'></script>",
            "devtools.js": devToolsScript,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: devToolsManifest, resources: resources)

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))
            try #require(webView._inspector).show()

            try await manager.run()
        }
    }

    @Test
    func portMessagePassingFromPanelToBackground() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.runtime.onConnect.addListener(port => {
              port.onMessage.addListener((message) => {
                browser.test.assertEq(message, 'Hello from Inspector panel', 'Should get the correct message from the panel script')
                port.postMessage('Acknowledged by Inspector panel')
              })
            })
            """

        let devToolsScript = """
            let panel = await browser.devtools.panels.create('Test Panel', 'icon.svg', 'panel.html')
            browser.test.assertEq(typeof panel, 'object', 'Panel should be created successfully')

            browser.test.sendMessage('Panel Created')
            """

        let panelScript = """
            const port = browser.runtime.connect()
            port.postMessage('Hello from Inspector panel')
            port.onMessage.addListener((response) => {
              browser.test.assertEq(response, 'Acknowledged by Inspector panel', 'Should get the correct response from the background script')

              browser.test.notifyPass()
            })
            """

        let iconSVG = "<svg width='16' height='16' xmlns='http://www.w3.org/2000/svg'><circle cx='8' cy='8' r='8' fill='red' /></svg>"

        let resources = [
            "background.js": backgroundScript,
            "devtools.html": "<script type='module' src='devtools.js'></script>",
            "devtools.js": devToolsScript,
            "panel.html": "<script type='module' src='panel.js'></script>",
            "panel.js": panelScript,
            "icon.svg": iconSVG,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: devToolsManifest, resources: resources)

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))
            try #require(webView._inspector).show()

            try await manager.waitForTestMessage("Panel Created")

            let showPanel = try startShowingExtensionPanel(for: manager)
            try await manager.run()
            try await showPanel.value
        }
    }

    @Test
    func portMessagePassingFromPanelToDevToolsBackground() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let devToolsScript = """
            let panel = await browser.devtools.panels.create('Test Panel', 'icon.svg', 'panel.html')
            browser.test.assertEq(typeof panel, 'object', 'Panel should be created successfully')

            browser.runtime.onConnect.addListener(port => {
              port.onMessage.addListener((message) => {
                browser.test.assertEq(message, 'Hello from Inspector panel', 'Should get the correct message from the panel script')
                port.postMessage('Acknowledged by Inspector background')
              })
            })

            browser.test.sendMessage('Panel Created')
            """

        let panelScript = """
            const port = browser.runtime.connect()
            port.postMessage('Hello from Inspector panel')

            port.onMessage.addListener((response) => {
              browser.test.assertEq(response, 'Acknowledged by Inspector background', 'Should get the correct response from the Inspector background')

              browser.test.notifyPass()
            })
            """

        let iconSVG = "<svg width='16' height='16' xmlns='http://www.w3.org/2000/svg'><circle cx='8' cy='8' r='8' fill='red' /></svg>"

        let resources = [
            "background.js": "// This script is intentionally left blank.",
            "devtools.html": "<script type='module' src='devtools.js'></script>",
            "devtools.js": devToolsScript,
            "panel.html": "<script type='module' src='panel.js'></script>",
            "panel.js": panelScript,
            "icon.svg": iconSVG,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: devToolsManifest, resources: resources)

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))
            try #require(webView._inspector).show()

            try await manager.waitForTestMessage("Panel Created")

            let showPanel = try startShowingExtensionPanel(for: manager)
            try await manager.run()
            try await showPanel.value
        }
    }

    @Test
    func inspectedWindowTabIdFromPanel() async throws {
        let previousShouldEnableSiteIsolation = TestWebExtensionManager.shouldEnableSiteIsolation
        TestWebExtensionManager.shouldEnableSiteIsolation = true
        defer { TestWebExtensionManager.shouldEnableSiteIsolation = previousShouldEnableSiteIsolation }

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let devToolsScript = """
            let panel = await browser.devtools.panels.create('Test Panel', 'icon.svg', 'panel.html')
            browser.test.assertEq(typeof panel, 'object', 'Panel should be created successfully')

            // The devtools_page resolves through its own process, so this half holds either way.
            browser.test.assertTrue(browser.devtools.inspectedWindow.tabId > 0, 'inspectedWindow.tabId should identify the inspected tab from the devtools page')

            browser.test.sendMessage('Panel Created')
            """

        let panelScript = """
            const tabId = browser.devtools.inspectedWindow.tabId
            browser.test.assertEq(typeof tabId, 'number', 'inspectedWindow.tabId should be a number in a panel')
            browser.test.assertTrue(tabId > 0, 'inspectedWindow.tabId should identify the inspected tab from a panel, not be the none value')

            browser.test.notifyPass()
            """

        let iconSVG = "<svg width='16' height='16' xmlns='http://www.w3.org/2000/svg'><circle cx='8' cy='8' r='8' fill='red' /></svg>"

        let resources = [
            "background.js": "// This script is intentionally left blank.",
            "devtools.html": "<script type='module' src='devtools.js'></script>",
            "devtools.js": devToolsScript,
            "panel.html": "<script type='module' src='panel.js'></script>",
            "panel.js": panelScript,
            "icon.svg": iconSVG,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: devToolsManifest, resources: resources)

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))
            try #require(webView._inspector).show()

            try await manager.waitForTestMessage("Panel Created")

            let showPanel = try startShowingExtensionPanel(for: manager)
            try await manager.run()
            try await showPanel.value
        }
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS && ENABLE_INSPECTOR_EXTENSIONS
