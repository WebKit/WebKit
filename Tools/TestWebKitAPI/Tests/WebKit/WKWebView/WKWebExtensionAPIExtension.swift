// Copyright (C) 2022-2026 Apple Inc. All rights reserved.
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

import Foundation
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestNavigationDelegate
private import TestWebKitAPILibrary.Helpers.cocoa.TestWebExtensionsDelegate
private import TestWebKitAPILibrary.Helpers.cocoa.WebExtensionUtilities
import Testing
import WebKit
private import WebKit_Private.WKProcessPoolPrivate
private import WebKit_Private.WKWebExtensionContextPrivate

import struct Foundation.URL
import struct Swift.String

@MainActor
struct WKWebExtensionAPIExtensionTests {
    private let extensionManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "Extension Test",
        "description": "Extension Test",
        "version": "1",

        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],

        "action": [
            "default_title": "Test Action",
            "default_popup": "popup.html",
            "default_icon": [
                "16": "toolbar-16.png",
                "32": "toolbar-32.png",
            ],
        ],

        "content_scripts": [
            [
                "js": ["content.js"],
                "matches": ["*://localhost/*"],
            ]
        ],
    ]

    private let extensionServiceWorkerManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "Extension Test",
        "description": "Extension Test",
        "version": "1",

        "background": [
            "service_worker": "background.js"
        ],
    ]

    @Test
    func getURL() async throws {
        let baseURLString = "test-extension://76C788B8-3374-400D-8259-40E5B9DF79D3"

        var backgroundScript = """
            // Variable Setup
            const baseURL = '\(baseURLString)'

            // Error Cases
            browser.test.assertThrows(() => browser.extension.getURL(), /required argument is missing/i)
            browser.test.assertThrows(() => browser.extension.getURL(null), /'resourcePath' value is invalid, because a string is expected/i)
            browser.test.assertThrows(() => browser.extension.getURL(undefined), /'resourcePath' value is invalid, because a string is expected/i)
            browser.test.assertThrows(() => browser.extension.getURL(42), /'resourcePath' value is invalid, because a string is expected/i)
            browser.test.assertThrows(() => browser.extension.getURL(/test/), /'resourcePath' value is invalid, because a string is expected/i)

            // Normal Cases
            browser.test.assertEq(browser.extension.getURL(''), `${baseURL}/`)
            browser.test.assertEq(browser.extension.getURL('test.js'), `${baseURL}/test.js`)
            browser.test.assertEq(browser.extension.getURL('/test.js'), `${baseURL}/test.js`)
            browser.test.assertEq(browser.extension.getURL('../../test.js'), `${baseURL}/test.js`)
            browser.test.assertEq(browser.extension.getURL('./test.js'), `${baseURL}/test.js`)
            browser.test.assertEq(browser.extension.getURL('././/example'), `${baseURL}//example`)
            browser.test.assertEq(browser.extension.getURL('../../example/..//test/'), `${baseURL}//test/`)
            browser.test.assertEq(browser.extension.getURL('.'), `${baseURL}/`)
            browser.test.assertEq(browser.extension.getURL('..//../'), `${baseURL}/`)
            browser.test.assertEq(browser.extension.getURL('.././..'), `${baseURL}/`)
            browser.test.assertEq(browser.extension.getURL('/.././.'), `${baseURL}/`)

            // Unexpected Cases
            // FIXME: <https://webkit.org/b/248154> browser.extension.getURL() has some edge cases that should be failures or return different results
            browser.test.assertEq(browser.extension.getURL('//'), 'test-extension://')
            browser.test.assertEq(browser.extension.getURL('//example'), `test-extension://example`)
            browser.test.assertEq(browser.extension.getURL('///'), 'test-extension:///')

            // Finish
            browser.test.notifyPass()
            """

        var manifest: [String: Any] = [
            "manifest_version": 2,
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let manager = parseWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        // Set a base URL so it is a known value and not the default random one.
        WKWebExtension.MatchPattern.registerCustomURLScheme("test-extension")
        context.baseURL = try #require(URL(string: baseURLString))

        try await manager.loadAndRun()

        // Manifest v3 deprecates getURL(), so it should be an udefined property.

        backgroundScript = """
            browser.test.assertEq(typeof browser.extension.getURL, 'undefined')
            browser.test.assertEq(browser.extension.getURL, undefined)

            // Finish
            browser.test.notifyPass()
            """

        manifest = [
            "manifest_version": 3,
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        try await loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript]).run()
    }

    @Test
    func getBackgroundPageFromBackground() async throws {
        let backgroundScript = """
            window.notifyTestPass = () => {
              browser.test.notifyPass()
            }

            const backgroundPage = browser.extension.getBackgroundPage()

            browser.test.assertTrue(backgroundPage === window, 'Should be able to get the background window from itself')
            browser.test.assertSafe(() => backgroundPage.notifyTestPass())
            """

        let manager = try loadWebExtension(manifest: extensionManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func getBackgroundPageFromTab() async throws {
        let backgroundScript = """
            window.notifyTestPass = () => {
              browser.test.notifyPass()
            }

            browser.tabs.create({ url: 'test.html' })
            """

        let testScript = """
            const backgroundPage = browser.extension.getBackgroundPage()
            browser.test.assertSafe(() => backgroundPage.notifyTestPass())
            """

        let testHTML = "<script type='module' src='test.js'></script>"

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "test.html": testHTML,
            "test.js": testScript,
        ]

        let manager = try loadWebExtension(manifest: extensionManifest, resources: resources)
        try await manager.run()
    }

    @Test
    func getBackgroundPageFromPopup() async throws {
        let backgroundScript = """
            window.notifyTestPass = () => {
              browser.test.notifyPass()
            }

            browser.action.openPopup()
            """

        let popupScript = """
            const backgroundPage = browser.extension.getBackgroundPage()
            browser.test.assertSafe(() => backgroundPage.notifyTestPass())
            """

        let popupHTML = "<script type='module' src='popup.js'></script>"

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "popup.html": popupHTML,
            "popup.js": popupScript,
        ]

        let manager = try loadWebExtension(manifest: extensionManifest, resources: resources)

        manager.internalDelegate.presentPopupForAction = { _ in
            // Do nothing so the popup web view will stay loaded.
        }

        try await manager.run()
    }

    @Test
    func getBackgroundPageForServiceWorker() async throws {
        let backgroundScript = """
            browser.tabs.create({ url: 'test.html' })
            """

        let testScript = """
            const backgroundPage = browser.extension.getBackgroundPage()
            browser.test.assertEq(backgroundPage, null, 'Background page should be null for service workers')
            browser.test.notifyPass()
            """

        let testHTML = "<script type='module' src='test.js'></script>"

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "test.html": testHTML,
            "test.js": testScript,
        ]

        let manager = try loadWebExtension(manifest: extensionServiceWorkerManifest, resources: resources)
        try await manager.run()
    }

    @Test
    func getViewsFromBackgroundPage() async throws {
        let backgroundScript = """
            window.notifyTestPass = () => {
              browser.test.notifyPass()
            }

            const views = browser.extension.getViews()

            browser.test.assertEq(views.length, 1, 'Background view should be included')
            browser.test.assertSafe(() => views[0].notifyTestPass())
            """

        let manager = try loadWebExtension(manifest: extensionManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func getViewsForBackground() async throws {
        let backgroundScript = """
            window.notifyTestPass = () => {
              browser.test.notifyPass()
            }

            browser.tabs.create({ url: 'test.html' })
            """

        let testScript = """
            const views = browser.extension.getViews()
            browser.test.assertEq(views.length, 2)

            for (const view of views) {
              if (view.notifyTestPass) {
                browser.test.assertSafe(() => view.notifyTestPass())
                break
              }
            }
            """

        let testHTML = "<script type='module' src='test.js'></script>"

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "test.html": testHTML,
            "test.js": testScript,
        ]

        let manager = try loadWebExtension(manifest: extensionManifest, resources: resources)
        try await manager.run()
    }

    @Test
    func getViewsForTab() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
              browser.test.assertEq(message, 'ready')

              const views = browser.extension.getViews({ type: 'tab' })
              browser.test.assertEq(views.length, 1)

              browser.test.assertSafe(() => views[0].notifyTestPass())
            })

            browser.tabs.create({ url: 'test.html' })
            """

        let testScript = """
            window.notifyTestPass = () => {
              browser.test.notifyPass()
            }

            browser.runtime.sendMessage('ready')
            """

        let testHTML = "<script type='module' src='test.js'></script>"

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "test.html": testHTML,
            "test.js": testScript,
        ]

        let manager = try loadWebExtension(manifest: extensionManifest, resources: resources)
        try await manager.run()
    }

    @Test
    func getViewsForMultipleTabs() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            let tabCount = 0

            browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
              browser.test.assertEq(message, 'ready')

              if (++tabCount === 2) {
                const views = browser.extension.getViews({ type: 'tab' })
                browser.test.assertEq(views.length, 2)

                for (let i = 0; i < views.length; ++i) {
                  const view = views[i]
                  if (view.notifyTestPass)
                    browser.test.assertSafe(() => view.notifyTestPass(i))
                }
              }
            })

            browser.tabs.create({ url: 'test.html' })
            browser.tabs.create({ url: 'test.html' })
            """

        let testScript = """
            window.notifyTestPass = (index) => {
              if (index === 1)
                browser.test.notifyPass()
            }

            browser.runtime.sendMessage('ready')
            """

        let testHTML = "<script type='module' src='test.js'></script>"

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "test.html": testHTML,
            "test.js": testScript,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: extensionManifest, resources: resources)
            let webView = try #require(manager.defaultTab?.webView)

            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func getViewsForPopup() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
              browser.test.assertEq(message, 'ready')

              const popupViews = browser.extension.getViews({ type: 'popup' })
              browser.test.assertEq(popupViews.length, 1)

              browser.test.assertSafe(() => popupViews[0].notifyTestPass())
            })

            browser.action.openPopup()
            """

        let popupScript = """
            window.notifyTestPass = () => {
              browser.test.notifyPass()
            }

            browser.runtime.sendMessage('ready')
            """

        let popupHTML = "<script type='module' src='popup.js'></script>"

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "popup.html": popupHTML,
            "popup.js": popupScript,
        ]

        let manager = try loadWebExtension(manifest: extensionManifest, resources: resources)

        manager.internalDelegate.presentPopupForAction = { _ in
            // Do nothing so the popup web view will stay loaded.
        }

        try await manager.run()
    }

    @Test
    func getViewsExcludesServiceWorkerBackground() async throws {
        let backgroundScript = """
            const views = browser.extension.getViews()
            browser.test.assertEq(views.length, 0, 'Background view should not be included for service workers')
            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: extensionServiceWorkerManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func handlePendingExtensionTasksWhileStopped() async throws {
        let backgroundScript = """
            function doTest() {
              browser.extension.isAllowedIncognitoAccess(() => {
                Promise.resolve().then(() => { })
                doTest()
              })
            }
            doTest()
            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: extensionServiceWorkerManifest, resources: ["background.js": backgroundScript])
        try await manager.run()

        let context = try #require(manager.context)
        let view = try #require(context._backgroundWebView)

        var didProcessCrash = false
        let navigationDelegate = TestNavigationDelegate()
        navigationDelegate.webContentProcessDidTerminate = { _, _ in
            didProcessCrash = true
        }
        view.navigationDelegate = navigationDelegate

        context.webViewConfiguration?.processPool._terminateServiceWorkers()

        try await Task.sleep(for: .milliseconds(500))
        #expect(!didProcessCrash)
    }

    @Test
    func inIncognitoContext() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.test.assertFalse(browser.extension.inIncognitoContext, 'Should not be in incognito in background script')
            """

        let contentScript = """
            if (browser.extension.inIncognitoContext)
              browser.test.sendMessage('Content Script is Incognito')
            else
              browser.test.sendMessage('Content Script is Not Incognito')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(
                manifest: extensionManifest,
                resources: ["background.js": backgroundScript, "content.js": contentScript]
            )
            let context = try #require(manager.context)
            let webView = try #require(manager.defaultTab?.webView)

            let urlRequest = URLRequest(url: configuration.localhostAddress)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            context.hasAccessToPrivateData = true

            webView.load(urlRequest)

            try await manager.waitForTestMessage("Content Script is Not Incognito")

            manager.closeWindow(try #require(manager.defaultWindow))

            let privateWindow = manager.openNewWindow(usingPrivateBrowsing: true)
            let privateTab = try #require(privateWindow.tabs.first)
            let privateWebView = try #require(privateTab.webView)

            privateWebView.load(urlRequest)

            try await manager.waitForTestMessage("Content Script is Incognito")
        }
    }

    @Test
    func inIncognitoContextWithoutPrivateAccess() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.test.assertFalse(browser.extension.inIncognitoContext, 'Should not be in incognito in background script')
            """

        let contentScript = """
            if (browser.extension.inIncognitoContext)
              browser.test.sendMessage('Content Script is Incognito')
            else
              browser.test.sendMessage('Content Script is Not Incognito')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(
                manifest: extensionManifest,
                resources: ["background.js": backgroundScript, "content.js": contentScript]
            )
            let context = try #require(manager.context)
            let webView = try #require(manager.defaultTab?.webView)

            let urlRequest = URLRequest(url: configuration.localhostAddress)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            webView.load(urlRequest)

            try await manager.waitForTestMessage("Content Script is Not Incognito")

            manager.closeWindow(try #require(manager.defaultWindow))

            let privateWindow = manager.openNewWindow(usingPrivateBrowsing: true)
            let privateTab = try #require(privateWindow.tabs.first)
            let privateWebView = try #require(privateTab.webView)

            privateWebView.load(urlRequest)

            // The content script should not run in the private tab, so timeout after waiting for 3 seconds.
            try await Task.sleep(for: .seconds(3))
            try manager.checkCollectedFailures()
        }
    }

    @Test
    func isAllowedIncognitoAccess() async throws {
        let backgroundScript = """
            const allowed = await browser.extension.isAllowedIncognitoAccess()
            browser.test.sendMessage(allowed ? 'Allowed Incognito Access' : 'Not Allowed Incognito Access')
            """

        let manager = try loadWebExtension(manifest: extensionManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        context.hasAccessToPrivateData = true

        try await manager.waitForTestMessage("Allowed Incognito Access")
    }

    @Test
    func isAllowedIncognitoAccessWithoutPrivateAccess() async throws {
        let backgroundScript = """
            const allowed = await browser.extension.isAllowedIncognitoAccess()
            browser.test.sendMessage(allowed ? 'Allowed Incognito Access' : 'Not Allowed Incognito Access')
            """

        let manager = try loadWebExtension(manifest: extensionManifest, resources: ["background.js": backgroundScript])

        try await manager.waitForTestMessage("Not Allowed Incognito Access")
    }

    @Test
    func isAllowedFileSchemeAccess() async throws {
        let backgroundScript = """
            const allowed = await browser.extension.isAllowedFileSchemeAccess()
            browser.test.sendMessage(allowed ? 'Allowed File Access' : 'Not Allowed File Access')
            """

        let manager = try loadWebExtension(manifest: extensionManifest, resources: ["background.js": backgroundScript])

        try await manager.waitForTestMessage("Not Allowed File Access")
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
