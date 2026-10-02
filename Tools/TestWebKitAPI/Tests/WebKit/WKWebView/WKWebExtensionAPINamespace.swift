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
private import TestWebKitAPILibrary.Helpers.cocoa.WebExtensionUtilities
import Testing
import WebKit

import struct Foundation.URL
import struct Swift.String

@MainActor
struct WKWebExtensionAPINamespaceTests {
    @Test
    func noWebNavigationObjectWithoutPermission() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["webNavigation"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let backgroundScript = """
            browser.test.assertEq(typeof browser.webNavigation, 'undefined')
            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        // Deny the permission, since TestWebKitAPI auto grants all requested permissions.
        context.setPermissionStatus(.deniedExplicitly, for: WKWebExtension.Permission.webNavigation)

        try await manager.run()
    }

    @Test
    func webNavigationObjectWithPermission() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["webNavigation"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let backgroundScript = """
            browser.test.assertEq(typeof browser.webNavigation, 'object')
            browser.test.notifyPass()
            """

        // TestWebKitAPI auto grants all requested permissions.

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func noNotificationsObjectWithoutPermission() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["notifications"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let backgroundScript = """
            browser.test.assertEq(typeof browser.notifications, 'undefined')
            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        // Deny the permission, since TestWebKitAPI auto grants all requested permissions.
        context.setPermissionStatus(.deniedExplicitly, for: WKWebExtension.Permission("notifications"))

        try await manager.run()
    }

    @Test
    func notificationsObjectWithPermission() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["notifications"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let backgroundScript = """
            browser.test.assertEq(typeof browser.notifications, 'object')
            browser.test.notifyPass()
            """

        // TestWebKitAPI auto grants all requested permissions.

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func notificationsUnsupported() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["notifications"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let backgroundScript = """
            browser.test.assertEq(browser.notifications, undefined)

            browser.test.notifyPass()
            """

        let manager = parseWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        context.unsupportedAPIs = ["browser.notifications"]

        try await manager.loadAndRun()
    }

    @Test
    func objectEquality() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["storage", "tabs"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let backgroundScript = """
            browser.test.assertEq(browser.storage, browser.storage)
            browser.test.assertEq(browser.storage.local, browser.storage.local)
            browser.test.assertEq(browser.storage.session, browser.storage.session)
            browser.test.assertEq(browser.storage.sync, browser.storage.sync)
            browser.test.assertEq(browser.tabs, browser.tabs)
            browser.test.assertEq(browser.windows, browser.windows)

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func browserNamespaceIsAvailableInContentScripts() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": [],
            "host_permissions": ["*://localhost/*"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "content_scripts": [
                [
                    "matches": ["*://localhost/*"],
                    "js": ["content.js"],
                ]
            ],
        ]

        let backgroundScript = """
            browser.test.sendMessage('Background page loaded')
            """

        let contentScript = """
            browser.test.assertEq(typeof browser, 'object')
            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "content.js": contentScript,
        ]

        try await server.run { configuration in
            let manager = parseWebExtension(manifest: manifest, resources: resources)
            let context = try #require(manager.context)
            let webView = try #require(manager.defaultTab?.webView)

            let url = configuration.localhostAddress

            // Steps to reproduce:
            // Step 1: Navigate to a page
            webView.load(URLRequest(url: url))
            try await webView._test_waitForDidFinishNavigation()

            // Step 2: Enable the extension
            manager.load()
            try await manager.waitForTestMessage("Background page loaded")

            // Step 3: Grant it access to the page
            context.setPermissionStatus(.grantedExplicitly, for: url)

            // Step 4: Disable the extension
            manager.unload()

            // Step 5: Refresh the page
            webView.reload()
            try await webView._test_waitForDidFinishNavigation()

            // Step 6: Enable the extension again
            try await manager.loadAndRun()
        }
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
