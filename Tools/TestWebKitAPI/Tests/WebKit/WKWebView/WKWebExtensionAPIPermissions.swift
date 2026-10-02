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
private import TestWebKitAPILibrary.Helpers.cocoa.TestWebExtensionsDelegate
private import TestWebKitAPILibrary.Helpers.cocoa.WebExtensionUtilities
import Testing
import WebKit
private import WebKit_Private.WKWebExtensionContextPrivate

import struct Foundation.URL
import struct Swift.String

#if WTF_PLATFORM_MAC
private import AppKit
#else
private import UIKit
#endif

@MainActor
struct WKWebExtensionAPIPermissionsTests {
    private let corsManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "Permissions Test",
        "description": "Permissions Test",
        "version": "1",

        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],

        "optional_host_permissions": ["*://*/*"],
    ]

    @Test
    func errors() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["alarms", "activeTab"],
            "optional_permissions": ["webNavigation", "cookies"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "host_permissions": ["*://*.apple.com/*"],
        ]

        let backgroundScript = """
            await browser.test.assertRejects(browser.permissions.remove({ permissions: ['webRequest'] }), /only permissions specified in the manifest may be removed/i)
            await browser.test.assertRejects(browser.permissions.remove({ origins: ['https://example.com/'] }), /only permissions specified in the manifest may be removed/i)

            await browser.test.assertRejects(browser.permissions.remove({ permissions: ['alarms'] }), /required permissions cannot be removed/i)
            await browser.test.assertRejects(browser.permissions.remove({ origins: ['*://*.apple.com/*'] }), /required permissions cannot be removed/i)

            browser.test.assertThrows(() => browser.permissions.contains({ permissions: ['bad'] }), /'bad' is not a valid permission/i)
            browser.test.assertThrows(() => browser.permissions.contains({ origins: ['bad'] }), /'bad' is not a valid pattern/i)

            await browser.test.assertRejects(browser.permissions.request({ permissions: ['cookies'] }), /must be called during a user gesture/i)

            browser.test.assertThrows(() => browser.permissions.contains(null), /'permissions' value is invalid, because an object is expected/i)
            browser.test.assertThrows(() => browser.permissions.contains('string'), /'permissions' value is invalid, because an object is expected/i)
            browser.test.assertThrows(() => browser.permissions.contains(123), /'permissions' value is invalid, because an object is expected/i)

            browser.test.assertThrows(() => browser.permissions.contains({ permissions: 'notAnArray' }), /'permissions' is expected to be an array of strings, but a string was provided/i)
            browser.test.assertThrows(() => browser.permissions.contains({ permissions: { name: 'storage' } }), /'permissions' is expected to be an array of strings, but an object was provided/i)
            browser.test.assertThrows(() => browser.permissions.contains({ permissions: 123 }), /'permissions' is expected to be an array of strings, but a number was provided/i)

            browser.test.assertThrows(() => browser.permissions.contains({ origins: 'notAnArray' }), /'origins' is expected to be an array of strings, but a string was provided/i)
            browser.test.assertThrows(() => browser.permissions.contains({ origins: { domain: 'https://example.com/' } }), /'origins' is expected to be an array of strings, but an object was provided/i)
            browser.test.assertThrows(() => browser.permissions.contains({ origins: 123 }), /'origins' is expected to be an array of strings, but a number was provided/i)

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func basics() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["alarms", "activeTab"],
            "optional_permissions": ["webNavigation", "cookies"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "host_permissions": ["*://*.apple.com/*"],
        ]

        let backgroundScript = """
            // contains() should return true for all named permissions and granted match patterns.
            browser.test.assertTrue(await browser.permissions.contains({'permissions': ['alarms', 'activeTab']}))
            browser.test.assertTrue(await browser.permissions.contains({'origins': ['*://webkit.org/*']}))

            // contains() should return false for non granted match patterns.
            browser.test.assertFalse(await browser.permissions.contains({'origins': ['*://apple.com/*']}))

            // Removing optional permissions should pass.
            browser.test.assertTrue(await browser.permissions.remove({'permissions': ['cookies']}))

            // Removing functional permissions should fail.
            await browser.test.assertRejects(browser.permissions.remove({'permissions': ['alarms']}))
            await browser.test.assertRejects(browser.permissions.remove({'permissions': ['alarms', 'activeTab']}))
            await browser.test.assertRejects(browser.permissions.remove({'permissions': ['cookies', 'activeTab']}))
            await browser.test.assertRejects(browser.permissions.remove({'permissions': ['scripting']}))

            // getAll() should return all named permissions and granted match patterns.
            let permissions = {'origins': ['*://webkit.org/*'], 'permissions': ['activeTab', 'alarms']}
            const result = await browser.permissions.getAll()
            browser.test.assertDeepEq(result, permissions)

            // Finish
            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        // Grant "permissions" in the manifest.
        for permission in manager.extension.requestedPermissions {
            context.setPermissionStatus(.grantedExplicitly, for: permission)
        }

        // Grant extension access to webkit.org.
        let matchPattern = try WKWebExtension.MatchPattern(string: "*://webkit.org/*")
        context.setPermissionStatus(.grantedExplicitly, for: matchPattern)

        let configuration = WKWebViewConfiguration()
        configuration.webExtensionController = manager.controller

        try await manager.run()
    }

    @Test
    func acceptPermissionsRequest() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["alarms", "activeTab"],
            "optional_permissions": ["webNavigation", "cookies", "declarativeNetRequest", "*://*.apple.com/*"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "host_permissions": ["*://*.apple.com/*"],
        ]

        let backgroundScript = """
            browser.test.runWithUserGesture(async () => {
              browser.test.assertTrue(await browser.permissions.request({'permissions': ['declarativeNetRequest'], 'origins': ['*://*.apple.com/*']}))
            })
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        let configuration = WKWebViewConfiguration()
        configuration.webExtensionController = manager.controller

        let permissions: Set<String> = ["declarativeNetRequest"]
        let matchPatterns: Set<WKWebExtension.MatchPattern> = try [WKWebExtension.MatchPattern(string: "*://*.apple.com/*")]
        let requestDelegate = TestWebExtensionsDelegate()

        var receivedPermissions: Set<String> = []
        var receivedMatchPatterns: Set<WKWebExtension.MatchPattern> = []

        await withCheckedContinuation { continuation in
            // Implement the delegate methods that're called when a call to permissions.request() is made.
            requestDelegate.promptForPermissions = { _, requestedPermissions, callback in
                receivedPermissions = requestedPermissions
                callback(Set(requestedPermissions.map { WKWebExtension.Permission($0) }), nil)
            }

            requestDelegate.promptForPermissionMatchPatterns = { _, requestedMatchPatterns, callback in
                receivedMatchPatterns = requestedMatchPatterns

                DispatchQueue.main.async {
                    callback(requestedMatchPatterns, nil)
                    continuation.resume()
                }
            }

            manager.controllerDelegate = requestDelegate
        }

        #expect(receivedPermissions.count == permissions.count)
        #expect(receivedPermissions == permissions)
        #expect(receivedMatchPatterns.count == matchPatterns.count)
        #expect(receivedMatchPatterns == matchPatterns)
    }

    @Test
    func denyPermissionsRequest() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "optional_permissions": ["declarativeNetRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "optional_host_permissions": ["*://*.apple.com/*"],
        ]

        let backgroundScript = """
            browser.test.runWithUserGesture(async () => {
              browser.test.assertFalse(await browser.permissions.request({'permissions': ['declarativeNetRequest'], 'origins': ['*://*.apple.com/*']}))
            })
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        let configuration = WKWebViewConfiguration()
        configuration.webExtensionController = manager.controller

        let requestDelegate = TestWebExtensionsDelegate()

        await withCheckedContinuation { continuation in
            // Implement the delegate methods, but don't grant the permissions.
            requestDelegate.promptForPermissions = { _, _, callback in
                callback([], .distantPast)
            }

            requestDelegate.promptForPermissionMatchPatterns = { _, _, callback in
                continuation.resume()
                callback([], .distantFuture)
            }

            manager.controllerDelegate = requestDelegate
        }
    }

    @Test
    func acceptPermissionsDenyMatchPatternsRequest() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "optional_permissions": ["declarativeNetRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "optional_host_permissions": ["*://*.apple.com/*"],
        ]

        let backgroundScript = """
            browser.test.runWithUserGesture(async () => {
              browser.test.assertFalse(await browser.permissions.request({'permissions': ['declarativeNetRequest'], 'origins': ['*://*.apple.com/*']}))
            })
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        let configuration = WKWebViewConfiguration()
        configuration.webExtensionController = manager.controller

        let requestDelegate = TestWebExtensionsDelegate()

        await withCheckedContinuation { continuation in
            // Grant the requested permissions.
            requestDelegate.promptForPermissions = { _, requestedPermissions, callback in
                callback(Set(requestedPermissions.map { WKWebExtension.Permission($0) }), nil)
            }

            // Deny the requested match patterns.
            requestDelegate.promptForPermissionMatchPatterns = { _, _, callback in
                continuation.resume()
                callback([], nil)
            }

            manager.controllerDelegate = requestDelegate
        }
    }

    @Test
    func requestPermissionsOnly() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "optional_permissions": ["declarativeNetRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "optional_host_permissions": ["*://*.apple.com/*"],
        ]

        let backgroundScript = """
            browser.test.runWithUserGesture(async () => {
              browser.test.assertTrue(await browser.permissions.request({'permissions': ['declarativeNetRequest']}))
            })
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        let configuration = WKWebViewConfiguration()
        configuration.webExtensionController = manager.controller

        let requestDelegate = TestWebExtensionsDelegate()

        var promptedForMatchPatterns = false

        await withCheckedContinuation { continuation in
            // Grant the requested permissions.
            requestDelegate.promptForPermissions = { _, requestedPermissions, callback in
                DispatchQueue.main.async {
                    continuation.resume()
                    callback(Set(requestedPermissions.map { WKWebExtension.Permission($0) }), nil)
                }
            }

            // Match patterns method should not be called.
            requestDelegate.promptForPermissionMatchPatterns = { _, _, _ in
                promptedForMatchPatterns = true
            }

            manager.controllerDelegate = requestDelegate
        }

        #expect(!promptedForMatchPatterns)
    }

    @Test
    func requestMatchPatternsOnly() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "optional_permissions": ["declarativeNetRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "optional_host_permissions": ["*://*.apple.com/*"],
        ]

        let backgroundScript = """
            browser.test.runWithUserGesture(async () => {
              browser.test.assertTrue(await browser.permissions.request({'origins': ['*://*.apple.com/*']}))
            })
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        let configuration = WKWebViewConfiguration()
        configuration.webExtensionController = manager.controller

        let requestDelegate = TestWebExtensionsDelegate()

        var promptedForPermissions = false

        await withCheckedContinuation { continuation in
            // Permissions method should not be called.
            requestDelegate.promptForPermissions = { _, _, _ in
                promptedForPermissions = true
            }

            // Grant the requested match patterns.
            requestDelegate.promptForPermissionMatchPatterns = { _, requestedMatchPatterns, callback in
                DispatchQueue.main.async {
                    continuation.resume()
                    callback(requestedMatchPatterns, Date(timeIntervalSinceNow: 10))
                }
            }

            manager.controllerDelegate = requestDelegate
        }

        #expect(!promptedForPermissions)
    }

    @Test
    func requestAllURLsMatchPattern() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "optional_host_permissions": ["<all_urls>"],
        ]

        let backgroundScript = """
            browser.test.runWithUserGesture(async () => {
              browser.test.assertTrue(await browser.permissions.request({'origins': ['<all_urls>']}))
            })
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        let configuration = WKWebViewConfiguration()
        configuration.webExtensionController = manager.controller

        let requestDelegate = TestWebExtensionsDelegate()

        var promptedForPermissions = false

        await withCheckedContinuation { continuation in
            // Permissions method should not be called.
            requestDelegate.promptForPermissions = { _, _, _ in
                promptedForPermissions = true
            }

            // Grant the requested match patterns.
            requestDelegate.promptForPermissionMatchPatterns = { _, requestedMatchPatterns, callback in
                DispatchQueue.main.async {
                    continuation.resume()
                    callback(requestedMatchPatterns, Date(timeIntervalSinceNow: 10))
                }
            }

            manager.controllerDelegate = requestDelegate
        }

        #expect(!promptedForPermissions)
    }

    @Test
    func implicitAllHostsAndURLsPermissions() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "permissions": ["tabs"],
        ]

        let backgroundScript = """
            browser.test.onMessage.addListener(async (message, data) => {
              if (message != 'Run test') return

              const permissions = await browser.permissions.getAll()
              browser.test.assertTrue(permissions.origins.includes('*://*/*'))

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Loaded')
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.MatchPattern.allHostsAndSchemes())

        let configuration = WKWebViewConfiguration()
        configuration.webExtensionController = manager.controller

        try await manager.waitForTestMessage("Loaded")

        manager.sendTestMessage("Run test")
        try await manager.run()
    }

    @Test
    func allHostsHostPermissions() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "host_permissions": ["*://*/*"],
        ]

        let backgroundScript = """
            browser.test.onMessage.addListener(async (message, data) => {
              if (message != 'Run test') return

              const permissions = await browser.permissions.getAll()

              browser.test.assertFalse(permissions.origins.includes('<all_urls>'))
              browser.test.assertTrue(permissions.origins.includes('*://*/*'))

              browser.test.sendMessage('Test done')
            })

            browser.test.sendMessage('Loaded')
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        let allHostsMatchPattern = try WKWebExtension.MatchPattern(string: "*://*/*")
        let allURLsMatchPattern = WKWebExtension.MatchPattern.allURLs()
        context.setPermissionStatus(.grantedExplicitly, for: allHostsMatchPattern)

        let configuration = WKWebViewConfiguration()
        configuration.webExtensionController = manager.controller

        try await manager.waitForTestMessage("Loaded")

        manager.sendTestMessage("Run test")
        try await manager.waitForTestMessage("Test done")

        context.setPermissionStatus(.unknown, for: allHostsMatchPattern)
        context.setPermissionStatus(.grantedExplicitly, for: allURLsMatchPattern)

        manager.sendTestMessage("Run test")
        try await manager.waitForTestMessage("Test done")
        manager.done()
    }

    @Test
    func allURLsHostPermissions() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "host_permissions": ["<all_urls>"],
        ]

        let backgroundScript = """
            browser.test.onMessage.addListener(async (message, data) => {
              if (message != 'Run test') return

              const permissions = await browser.permissions.getAll()

              browser.test.assertTrue(permissions.origins.includes('<all_urls>'))
              browser.test.assertFalse(permissions.origins.includes('*://*/*'))

              browser.test.sendMessage('Test done')
            })

            browser.test.sendMessage('Loaded')
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        let allHostsMatchPattern = try WKWebExtension.MatchPattern(string: "*://*/*")
        let allURLsMatchPattern = WKWebExtension.MatchPattern.allURLs()
        context.setPermissionStatus(.grantedExplicitly, for: allHostsMatchPattern)

        let configuration = WKWebViewConfiguration()
        configuration.webExtensionController = manager.controller

        try await manager.waitForTestMessage("Loaded")

        manager.sendTestMessage("Run test")
        try await manager.waitForTestMessage("Test done")

        context.setPermissionStatus(.unknown, for: allHostsMatchPattern)
        context.setPermissionStatus(.grantedExplicitly, for: allURLsMatchPattern)

        manager.sendTestMessage("Run test")
        try await manager.waitForTestMessage("Test done")
        manager.done()
    }

    @Test
    func allHostsOptionalHostPermissions() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "optional_host_permissions": ["*://*/*"],
        ]

        let backgroundScript = """
            browser.test.onMessage.addListener(async (message, data) => {
              if (message != 'Run test') return

              const permissions = await browser.permissions.getAll()

              browser.test.assertFalse(permissions.origins.includes('<all_urls>'))
              browser.test.assertTrue(permissions.origins.includes('*://*/*'))

              browser.test.sendMessage('Test done')
            })

            browser.test.sendMessage('Loaded')
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        let allHostsMatchPattern = try WKWebExtension.MatchPattern(string: "*://*/*")
        let allURLsMatchPattern = WKWebExtension.MatchPattern.allURLs()
        context.setPermissionStatus(.grantedExplicitly, for: allHostsMatchPattern)

        let configuration = WKWebViewConfiguration()
        configuration.webExtensionController = manager.controller

        try await manager.waitForTestMessage("Loaded")

        manager.sendTestMessage("Run test")
        try await manager.waitForTestMessage("Test done")

        context.setPermissionStatus(.unknown, for: allHostsMatchPattern)
        context.setPermissionStatus(.grantedExplicitly, for: allURLsMatchPattern)

        manager.sendTestMessage("Run test")
        try await manager.waitForTestMessage("Test done")
        manager.done()
    }

    @Test
    func allURLsOptionalHostPermissions() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "optional_host_permissions": ["<all_urls>"],
        ]

        let backgroundScript = """
            browser.test.onMessage.addListener(async (message, data) => {
              if (message != 'Run test') return

              const permissions = await browser.permissions.getAll()

              browser.test.assertTrue(permissions.origins.includes('<all_urls>'))
              browser.test.assertFalse(permissions.origins.includes('*://*/*'))

              browser.test.sendMessage('Test done')
            })

            browser.test.sendMessage('Loaded')
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        let allHostsMatchPattern = try WKWebExtension.MatchPattern(string: "*://*/*")
        let allURLsMatchPattern = WKWebExtension.MatchPattern.allURLs()
        context.setPermissionStatus(.grantedExplicitly, for: allHostsMatchPattern)

        let configuration = WKWebViewConfiguration()
        configuration.webExtensionController = manager.controller

        try await manager.waitForTestMessage("Loaded")

        manager.sendTestMessage("Run test")
        try await manager.waitForTestMessage("Test done")

        context.setPermissionStatus(.unknown, for: allHostsMatchPattern)
        context.setPermissionStatus(.grantedExplicitly, for: allURLsMatchPattern)

        manager.sendTestMessage("Run test")
        try await manager.waitForTestMessage("Test done")
        manager.done()
    }

    @Test
    func allHostsAndURLsHostPermissions() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "host_permissions": ["*://*/*", "<all_urls>"],
        ]

        let backgroundScript = """
            browser.test.onMessage.addListener(async (message, data) => {
              if (message != 'Run test') return

              const permissions = await browser.permissions.getAll()

              browser.test.assertTrue(permissions.origins.includes('<all_urls>'))
              browser.test.assertTrue(permissions.origins.includes('*://*/*'))

              browser.test.sendMessage('Test done')
            })

            browser.test.sendMessage('Loaded')
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        let allHostsMatchPattern = try WKWebExtension.MatchPattern(string: "*://*/*")
        let allURLsMatchPattern = WKWebExtension.MatchPattern.allURLs()
        context.setPermissionStatus(.grantedExplicitly, for: allHostsMatchPattern)

        let configuration = WKWebViewConfiguration()
        configuration.webExtensionController = manager.controller

        try await manager.waitForTestMessage("Loaded")

        manager.sendTestMessage("Run test")
        try await manager.waitForTestMessage("Test done")

        context.setPermissionStatus(.unknown, for: allHostsMatchPattern)
        context.setPermissionStatus(.grantedExplicitly, for: allURLsMatchPattern)

        manager.sendTestMessage("Run test")
        try await manager.waitForTestMessage("Test done")
        manager.done()
    }

    @Test
    func allHostsAndURLsOptionalHostPermissions() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "optional_host_permissions": ["*://*/*", "<all_urls>"],
        ]

        let backgroundScript = """
            browser.test.onMessage.addListener(async (message, data) => {
              if (message != 'Run test') return

              const permissions = await browser.permissions.getAll()

              browser.test.assertTrue(permissions.origins.includes('<all_urls>'))
              browser.test.assertTrue(permissions.origins.includes('*://*/*'))

              browser.test.sendMessage('Test done')
            })

            browser.test.sendMessage('Loaded')
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        let allHostsMatchPattern = try WKWebExtension.MatchPattern(string: "*://*/*")
        let allURLsMatchPattern = WKWebExtension.MatchPattern.allURLs()
        context.setPermissionStatus(.grantedExplicitly, for: allHostsMatchPattern)

        let configuration = WKWebViewConfiguration()
        configuration.webExtensionController = manager.controller

        try await manager.waitForTestMessage("Loaded")

        manager.sendTestMessage("Run test")
        try await manager.waitForTestMessage("Test done")

        context.setPermissionStatus(.unknown, for: allHostsMatchPattern)
        context.setPermissionStatus(.grantedExplicitly, for: allURLsMatchPattern)

        manager.sendTestMessage("Run test")
        try await manager.waitForTestMessage("Test done")
        manager.done()
    }

    @Test
    func allHostsAndURLsOptionalAndHostPermissions() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "host_permissions": ["*://*/*"],
            "optional_host_permissions": ["<all_urls>"],
        ]

        let backgroundScript = """
            browser.test.onMessage.addListener(async (message, data) => {
              if (message != 'Run test') return

              const permissions = await browser.permissions.getAll()

              browser.test.assertTrue(permissions.origins.includes('<all_urls>'))
              browser.test.assertTrue(permissions.origins.includes('*://*/*'))

              browser.test.sendMessage('Test done')
            })

            browser.test.sendMessage('Loaded')
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        let allHostsMatchPattern = try WKWebExtension.MatchPattern(string: "*://*/*")
        let allURLsMatchPattern = WKWebExtension.MatchPattern.allURLs()
        context.setPermissionStatus(.grantedExplicitly, for: allHostsMatchPattern)

        let configuration = WKWebViewConfiguration()
        configuration.webExtensionController = manager.controller

        try await manager.waitForTestMessage("Loaded")

        manager.sendTestMessage("Run test")
        try await manager.waitForTestMessage("Test done")

        context.setPermissionStatus(.unknown, for: allHostsMatchPattern)
        context.setPermissionStatus(.grantedExplicitly, for: allURLsMatchPattern)

        manager.sendTestMessage("Run test")
        try await manager.waitForTestMessage("Test done")
        manager.done()
    }

    @Test
    func grantOnlySomePermissions() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "optional_permissions": ["declarativeNetRequest", "alarms"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "optional_host_permissions": ["*://*.apple.com/*"],
        ]

        let backgroundScript = """
            browser.test.runWithUserGesture(async () => {
              browser.test.assertFalse(await browser.permissions.request({'permissions': ['alarms', 'declarativeNetRequest']}))
            })
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        let configuration = WKWebViewConfiguration()
        configuration.webExtensionController = manager.controller

        let requestDelegate = TestWebExtensionsDelegate()

        var promptedForMatchPatterns = false

        await withCheckedContinuation { continuation in
            // Grant the requested permissions.
            requestDelegate.promptForPermissions = { _, requestedPermissions, callback in
                DispatchQueue.main.async {
                    continuation.resume()
                    callback(Set(requestedPermissions.map { WKWebExtension.Permission($0) }), nil)
                }
            }

            // Match patterns method should not be called.
            requestDelegate.promptForPermissionMatchPatterns = { _, _, _ in
                promptedForMatchPatterns = true
            }

            manager.controllerDelegate = requestDelegate
        }

        #expect(!promptedForMatchPatterns)
    }

    @Test
    func grantOnlySomeMatchPatterns() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "optional_permissions": ["declarativeNetRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "optional_host_permissions": ["*://*.apple.com/*", "*://*.example.com/*"],
        ]

        let backgroundScript = """
            browser.test.runWithUserGesture(async () => {
              browser.test.assertFalse(await browser.permissions.request({'origins': ['*://*.apple.com/*', '*://*.example.com/*']}))
            })
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        let configuration = WKWebViewConfiguration()
        configuration.webExtensionController = manager.controller

        let requestDelegate = TestWebExtensionsDelegate()

        var promptedForPermissions = false

        await withCheckedContinuation { continuation in
            // Permissions method should not be called.
            requestDelegate.promptForPermissions = { _, _, _ in
                promptedForPermissions = true
            }

            // Grant only one of the requested match patterns.
            requestDelegate.promptForPermissionMatchPatterns = { _, requestedMatchPatterns, callback in
                continuation.resume()
                callback(Set(requestedMatchPatterns.prefix(1)), .distantFuture)
            }

            manager.controllerDelegate = requestDelegate
        }

        #expect(!promptedForPermissions)
    }

    @Test
    func validMatchPatterns() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["alarms", "activeTab"],
            "optional_permissions": ["webNavigation", "cookies"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "host_permissions": ["*://*.example.com/*"],
        ]

        let backgroundScript = """
            // Supported Schemes
            await browser.test.assertSafeResolve(() => browser.permissions.contains({ origins: [ '*://*.example.com/*' ] }))
            await browser.test.assertSafeResolve(() => browser.permissions.contains({ origins: [ 'http://*.example.com/*' ] }))
            await browser.test.assertSafeResolve(() => browser.permissions.contains({ origins: [ 'https://*.example.com/*' ] }))
            await browser.test.assertSafeResolve(() => browser.permissions.contains({ origins: [ 'webkit-extension://*/*' ] }))

            // Custom Web Extension Schemes
            await browser.test.assertSafeResolve(() => browser.permissions.contains({ origins: [ 'test-extension://*/*' ] }))
            await browser.test.assertSafeResolve(() => browser.permissions.contains({ origins: [ 'other-extension://*/*' ] }))

            // File Scheme (recognized but not granted by default)
            await browser.test.assertSafeResolve(() => browser.permissions.contains({ origins: [ 'file:///*' ] }))

            // Invalid Schemes
            await browser.test.assertThrows(() => browser.permissions.contains({ origins: [ 'ftp://*.example.com/*' ] }), /not a valid pattern/)
            await browser.test.assertThrows(() => browser.permissions.contains({ origins: [ 'data:*' ] }), /not a valid pattern/)
            await browser.test.assertThrows(() => browser.permissions.contains({ origins: [ 'chrome-extension://*/*' ] }), /not a valid pattern/)

            // Finish
            browser.test.notifyPass()
            """

        WKWebExtension.MatchPattern.registerCustomURLScheme("test-extension")
        WKWebExtension.MatchPattern.registerCustomURLScheme("other-extension")

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        let matchPatternApple = try WKWebExtension.MatchPattern(string: "*://*.example.com/*")
        context.setPermissionStatus(.grantedExplicitly, for: matchPatternApple)

        try await manager.run()
    }

    @Test
    func clipboardWrite() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Permissions Test",
            "description": "Permissions Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "permissions": ["clipboardWrite"],
        ]

        let backgroundScript = """
            await navigator.clipboard.writeText('Test Clipboard Write')

            browser.test.sendMessage('Clipboard Written')
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.clipboardWrite)

        try await manager.waitForTestMessage("Clipboard Written")

        #if WTF_PLATFORM_MAC
        let clipboardContent = NSPasteboard.general.string(forType: .string)
        #else
        let clipboardContent = UIPasteboard.general.string
        #endif

        #expect(clipboardContent == "Test Clipboard Write")
    }

    @Test
    func clipboardWriteWithoutPermission() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Permissions Test",
            "description": "Permissions Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let backgroundScript = """
            try {
              await navigator.clipboard.writeText('Attempt Without Permission')

              browser.test.notifyFail('Should not write to clipboard without permission')
            } catch (error) {
              browser.test.assertTrue(error instanceof DOMException, 'Expect a DOMException for insufficient permissions')

              browser.test.notifyPass()
            }
            """

        #if WTF_PLATFORM_MAC
        let clipboardContentBefore = NSPasteboard.general.string(forType: .string)
        #else
        let clipboardContentBefore = UIPasteboard.general.string
        #endif

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.run()

        #if WTF_PLATFORM_MAC
        let clipboardContentAfter = NSPasteboard.general.string(forType: .string)
        #else
        let clipboardContentAfter = UIPasteboard.general.string
        #endif

        #expect(clipboardContentBefore == clipboardContentAfter)
    }

    @Test
    func clipboardWriteWithRequest() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Permissions Test",
            "description": "Permissions Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "optional_permissions": ["clipboardWrite"],
        ]

        let backgroundScript = """
            try {
              await navigator.clipboard.writeText('Initial Attempt Without Permission')
            } catch (error) {
              browser.test.assertTrue(error instanceof DOMException, 'Expect a DOMException for insufficient permissions')
            }

            const permissionGranted = await browser.test.runWithUserGesture(() => {
              return browser.permissions.request({ permissions: [ 'clipboardWrite' ] })
            })

            if (permissionGranted) {
              await navigator.clipboard.writeText('Test Clipboard Write After Permission')

              browser.test.sendMessage('Clipboard Written')
            } else {
              browser.test.notifyFail('Permission was not granted')
            }
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        let requestDelegate = TestWebExtensionsDelegate()

        var receivedPermissions: Set<String> = []

        requestDelegate.promptForPermissions = { _, requestedPermissions, callback in
            receivedPermissions = requestedPermissions

            callback(Set(requestedPermissions.map { WKWebExtension.Permission($0) }), nil)
        }

        manager.controllerDelegate = requestDelegate

        try await manager.waitForTestMessage("Clipboard Written")

        #expect(receivedPermissions.count == 1)
        #expect(receivedPermissions.contains(WKWebExtension.Permission.clipboardWrite.rawValue))

        #if WTF_PLATFORM_MAC
        let clipboardContent = NSPasteboard.general.string(forType: .string)
        #else
        let clipboardContent = UIPasteboard.general.string
        #endif

        #expect(clipboardContent == "Test Clipboard Write After Permission")
    }

    @Test
    func corsUsingFetchWithPermissions() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/subresource", headerFields: ["Content-Type": "application/json", "headerName": "headerValue"]) {
                "{ \"testKey\": \"testValue\" }"
            }
        }

        try await server.run { configuration in
            let backgroundScript = """
                const subresourceURL = 'http://127.0.0.1:\(configuration.port)/subresource'

                try {
                  const response = await fetch(subresourceURL)
                  if (response.headers.get('headerName') !== 'headerValue')
                    throw new Error('CORS failed: Incorrect header value')

                  const json = await response.json()
                  if (json.testKey !== 'testValue')
                    throw new Error('CORS failed: Incorrect JSON value')

                  browser.test.notifyPass()
                } catch (error) {
                  browser.test.notifyFail('CORS failed unexpectedly: ' + error.message)
                }
                """

            let manager = try loadWebExtension(manifest: corsManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.address)

            try await manager.run()
        }
    }

    @Test
    func corsUsingFetchWithoutPermissions() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/subresource", headerFields: ["Content-Type": "application/json", "headerName": "headerValue"]) {
                "{ \"testKey\": \"testValue\" }"
            }
        }

        try await server.run { configuration in
            let subresourceURL = configuration.address.appending(path: "subresource")

            let backgroundScript = """
                const subresourceURL = '\(subresourceURL.absoluteString)'

                browser.permissions.onAdded.addListener(async () => {
                  try {
                    const response = await fetch(subresourceURL)
                    if (response.headers.get('headerName') !== 'headerValue')
                      throw new Error('CORS failed: Incorrect header value')

                    const json = await response.json()
                    if (json.testKey !== 'testValue')
                      throw new Error('CORS failed: Incorrect JSON value')

                    browser.test.notifyPass()
                  } catch (error) {
                    browser.test.notifyFail('CORS failed unexpectedly after permission was granted: ' + error.message)
                  }
                })

                try {
                  const response = await fetch(subresourceURL)
                  browser.test.notifyFail('CORS enabled: Fetch succeeded when it should have failed')
                } catch (error) {
                  // CORS failed as expected
                }
                """

            let manager = try loadWebExtension(manifest: corsManifest, resources: ["background.js": backgroundScript])

            var promptCount = 0
            var requestedURLSets: [Set<URL>] = []

            manager.internalDelegate.promptForPermissionToAccessURLs = { _, requestedURLs, completionHandler in
                requestedURLSets.append(requestedURLs)

                promptCount += 1

                completionHandler(requestedURLs, nil)
            }

            try await manager.run()

            #expect(requestedURLSets.allSatisfy { $0 == [subresourceURL] })
            #expect(promptCount == 1)
        }
    }

    @Test
    func corsUsingFetchWithoutGrantingPermission() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/subresource", headerFields: ["Content-Type": "application/json", "headerName": "headerValue"]) {
                "{ \"testKey\": \"testValue\" }"
            }
        }

        try await server.run { configuration in
            let subresourceURL = configuration.address.appending(path: "subresource")

            let backgroundScript = """
                const subresourceURL = '\(subresourceURL.absoluteString)'

                let fetchAttempt = 0

                async function performFetch() {
                  try {
                    const response = await fetch(subresourceURL)
                    browser.test.notifyFail(`CORS enabled on attempt ${fetchAttempt + 1}, when it should have failed`)
                  } catch (error) {
                    if (++fetchAttempt < 2)
                      performFetch()
                    else
                      browser.test.notifyPass()
                  }
                }

                performFetch()
                """

            let manager = try loadWebExtension(manifest: corsManifest, resources: ["background.js": backgroundScript])

            var promptCount = 0
            var requestedURLSets: [Set<URL>] = []

            manager.internalDelegate.promptForPermissionToAccessURLs = { _, requestedURLs, completionHandler in
                requestedURLSets.append(requestedURLs)

                promptCount += 1

                completionHandler([], nil)
            }

            try await manager.run()

            #expect(requestedURLSets.allSatisfy { $0 == [subresourceURL] })
            #expect(promptCount == 2)
        }
    }

    @Test
    func corsUsingXHRWithPermissions() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/subresource", headerFields: ["Content-Type": "application/json", "headerName": "headerValue"]) {
                "{ \"testKey\": \"testValue\" }"
            }
        }

        try await server.run { configuration in
            let backgroundScript = """
                const subresourceURL = 'http://127.0.0.1:\(configuration.port)/subresource'

                const xhr = new XMLHttpRequest()

                xhr.onload = () => {
                  if (xhr.getResponseHeader('headerName') !== 'headerValue')
                    return browser.test.notifyFail('CORS failed: Incorrect header value')

                  try {
                    const json = JSON.parse(xhr.responseText)
                    if (json.testKey !== 'testValue')
                      throw new Error('Incorrect JSON value')

                    browser.test.notifyPass()
                  } catch (error) {
                    browser.test.notifyFail('CORS failed: JSON parsing error - ' + error.message)
                  }
                }

                xhr.onerror = () => browser.test.notifyFail('CORS failed unexpectedly')

                xhr.open('GET', subresourceURL, true)
                xhr.send()
                """

            let manager = try loadWebExtension(manifest: corsManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.address)

            try await manager.run()
        }
    }

    @Test
    func corsUsingXHRWithoutPermissions() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/subresource", headerFields: ["Content-Type": "application/json", "headerName": "headerValue"]) {
                "{ \"testKey\": \"testValue\" }"
            }
        }

        try await server.run { configuration in
            let subresourceURL = configuration.address.appending(path: "subresource")

            let backgroundScript = """
                const subresourceURL = '\(subresourceURL.absoluteString)'

                browser.permissions.onAdded.addListener(() => {
                  const xhr = new XMLHttpRequest()

                  xhr.onload = () => {
                    if (xhr.getResponseHeader('headerName') !== 'headerValue')
                      return browser.test.notifyFail('CORS failed: Incorrect header value')

                    try {
                      const json = JSON.parse(xhr.responseText)
                      if (json.testKey !== 'testValue')
                        throw new Error('Incorrect JSON value')

                      browser.test.notifyPass()
                    } catch (error) {
                      browser.test.notifyFail('CORS failed: JSON parsing error - ' + error.message)
                    }
                  }

                  xhr.onerror = () => browser.test.notifyFail('CORS failed unexpectedly after permission was granted')

                  xhr.open('GET', subresourceURL, true)
                  xhr.send()
                })

                const xhr = new XMLHttpRequest()

                xhr.onload = () => browser.test.notifyFail('CORS enabled: XHR succeeded when it should have failed')

                xhr.onerror = () => {
                  // Expected CORS failure
                }

                xhr.open('GET', subresourceURL, true)
                xhr.send()
                """

            let manager = try loadWebExtension(manifest: corsManifest, resources: ["background.js": backgroundScript])

            var promptCount = 0
            var requestedURLSets: [Set<URL>] = []

            manager.internalDelegate.promptForPermissionToAccessURLs = { _, requestedURLs, completionHandler in
                requestedURLSets.append(requestedURLs)

                promptCount += 1

                completionHandler(requestedURLs, nil)
            }

            try await manager.run()

            #expect(requestedURLSets.allSatisfy { $0 == [subresourceURL] })
            #expect(promptCount == 1)
        }
    }

    @Test
    func corsUsingXHRWithoutGrantingPermission() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/subresource", headerFields: ["Content-Type": "application/json", "headerName": "headerValue"]) {
                "{ \"testKey\": \"testValue\" }"
            }
        }

        try await server.run { configuration in
            let subresourceURL = configuration.address.appending(path: "subresource")

            let backgroundScript = """
                const subresourceURL = '\(subresourceURL.absoluteString)'

                let fetchAttempt = 0

                function performXHR() {
                  const xhr = new XMLHttpRequest()

                  xhr.onload = () => {
                    browser.test.notifyFail(`CORS enabled on attempt ${fetchAttempt + 1}, when it should have failed`)
                  }

                  xhr.onerror = () => {
                    if (++fetchAttempt < 2)
                      performXHR()
                    else
                      browser.test.notifyPass()
                  }

                  xhr.open('GET', subresourceURL, true)
                  xhr.send()
                }

                performXHR()
                """

            let manager = try loadWebExtension(manifest: corsManifest, resources: ["background.js": backgroundScript])

            var promptCount = 0
            var requestedURLSets: [Set<URL>] = []

            manager.internalDelegate.promptForPermissionToAccessURLs = { _, requestedURLs, completionHandler in
                requestedURLSets.append(requestedURLs)

                promptCount += 1

                // Do not grant the permission in the prompt.
                completionHandler([], nil)
            }

            try await manager.run()

            #expect(requestedURLSets.allSatisfy { $0 == [subresourceURL] })
            #expect(promptCount == 2)
        }
    }

    // rdar://154866064 — the CORS-failure auto-prompt in `WebExtensionContext::resourceLoadDidCompleteWithError`
    // should only fire when the failed CORS request comes from a page belonging to this extension, not the webpage.
    @Test
    func corsFailureFromPageDoesNotPromptExtension() async throws {
        let pageScript = """
            <script>
              fetch('http://127.0.0.1:' + location.port + '/subresource')
                .then(() => browser.test.notifyFail('Page fetch unexpectedly succeeded; CORS should have blocked it'))
                .catch(() => browser.test.notifyPass())
            </script>
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                pageScript
            }

            Route("/subresource", headerFields: ["Content-Type": "application/json"]) {
                "{ \"testKey\": \"testValue\" }"
            }
        }

        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: corsManifest, resources: ["background.js": backgroundScript])
            let webView = try #require(manager.defaultTab?.webView)

            var promptCount = 0
            manager.internalDelegate.promptForPermissionToAccessURLs = { _, _, completionHandler in
                promptCount += 1
                completionHandler([], nil)
            }

            try await manager.waitForTestMessage("Load Tab")

            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()

            #expect(promptCount == 0)
        }
    }

    // The extension Content Security Policy mode should only apply to extension documents, not to web page
    // subframes loaded inside an extension page. Otherwise, the web page's own CSP is parsed with the extension
    // restrictions, and keywords like 'unsafe-inline' are dropped from script-src in Manifest V3.
    @Test
    func cspExtensionModeNotAppliedToWebPageSubframe() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route(
                "/frame.html",
                headerFields: [
                    "Content-Type": "text/html",
                    "Content-Security-Policy": "script-src 'self' 'unsafe-inline'"
                ]
            ) {
                "<script>window.inlineScriptRan = true</script><script src='/frame.js'></script>"
            }

            Route("/frame.js", headerFields: ["Content-Type": "text/javascript"]) {
                "parent.postMessage({ inlineScriptRan: window.inlineScriptRan === true }, '*')"
            }
        }

        try await server.run { configuration in
            let frameURL = configuration.localhostAddress.appending(path: "frame.html")

            let backgroundScript = """
                browser.tabs.create({ url: 'test.html' })
                """

            let testScript = """
                window.addEventListener('message', (event) => {
                  browser.test.assertTrue(event.data?.inlineScriptRan, 'The inline script in the web page subframe should be allowed by its CSP')
                  browser.test.notifyPass()
                })

                document.getElementById('frame').src = '\(frameURL.absoluteString)'
                """

            let testHTML = "<iframe id='frame'></iframe><script type='module' src='test.js'></script>"

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "test.html": testHTML,
                "test.js": testScript,
            ]

            let manager = try loadWebExtension(manifest: corsManifest, resources: resources)
            try await manager.run()
        }
    }

    @Test
    func hasAccessToFileURLsDefaultsToNo() throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "host_permissions": ["<all_urls>"],
        ]

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": ""])
        let context = try #require(manager.context)

        #expect(!context._hasAccessToFileURLs)
    }

    @Test
    func fileURLPermissionGatedByFlag() throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "host_permissions": ["<all_urls>"],
        ]

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": ""])
        let context = try #require(manager.context)

        let allURLs = WKWebExtension.MatchPattern.allURLs()
        context.setPermissionStatus(.grantedExplicitly, for: allURLs)

        let fileURL = try #require(URL(string: "file:///foo/bar.html"))
        let httpURL = try #require(URL(string: "http://example.com/foo/bar.html"))

        // Flag off: file URL hits the early-out gate even though <all_urls> is granted.
        #expect(context.permissionStatus(for: fileURL) == .unknown)
        // <all_urls> is a wildcard host pattern, so non-file URLs come back as GrantedImplicitly.
        #expect(context.permissionStatus(for: httpURL) == .grantedImplicitly)

        // Flag on: <all_urls> now covers file:// URLs too.
        context._hasAccessToFileURLs = true
        #expect(context.permissionStatus(for: fileURL) == .grantedImplicitly)

        // Flipping the flag back off restores the gate.
        context._hasAccessToFileURLs = false
        #expect(context.permissionStatus(for: fileURL) == .unknown)
    }

    @Test
    func filePatternPermissionGatedByFlag() throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "host_permissions": ["file:///*"],
        ]

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": ""])
        let context = try #require(manager.context)

        let filePattern = try WKWebExtension.MatchPattern(string: "file:///*")
        context.setPermissionStatus(.grantedExplicitly, for: filePattern)

        // Flag off: querying the file pattern returns Unknown via the pattern early-out gate.
        #expect(context.permissionStatus(for: filePattern) == .unknown)

        // Flag on: explicit grant of the same pattern resolves to GrantedExplicitly.
        context._hasAccessToFileURLs = true
        #expect(context.permissionStatus(for: filePattern) == .grantedExplicitly)
    }

    @Test
    func grantingFilePatternRequiresFlag() throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "host_permissions": ["file:///*"],
        ]

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": ""])
        let context = try #require(manager.context)

        let filePattern = try WKWebExtension.MatchPattern(string: "file:///*")
        context.setPermissionStatus(.grantedExplicitly, for: filePattern)

        let fileURL = try #require(URL(string: "file:///foo/bar.html"))

        // Even with file:///* granted explicitly, the URL early-out gate still blocks file URLs.
        #expect(context.permissionStatus(for: fileURL) == .unknown)

        // file:///* is a specific (non-wildcard-host) pattern, so it resolves to GrantedExplicitly with the flag on.
        context._hasAccessToFileURLs = true
        #expect(context.permissionStatus(for: fileURL) == .grantedExplicitly)
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
