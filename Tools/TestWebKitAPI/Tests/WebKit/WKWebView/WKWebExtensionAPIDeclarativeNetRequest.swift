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
private import WebKit_Private._WKFeature
private import WebKit_Private._WKWebExtensionDeclarativeNetRequestRule
private import WebKit_Private._WKWebExtensionDeclarativeNetRequestTranslator
private import WebKit_Private.WKNavigationActionPrivate
private import WebKit_Private.WKPreferencesPrivate
private import WebKit_Private.WKWebExtensionControllerConfigurationPrivate

import struct Foundation.URL
import struct Swift.String

@MainActor
struct WKWebExtensionAPIDeclarativeNetRequestTests {
    @Test
    func blockedLoadTest() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<body style='background-color: blue'></body>"
            }
        }

        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        let declarativeNetRequestManifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "blockFrame",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        let rules =
            "[ { \"id\" : 1, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"frame\" } } ]"

        try await server.run { configuration in
            let manager = try loadWebExtension(
                manifest: declarativeNetRequestManifest,
                resources: ["background.js": backgroundScript, "rules.json": rules]
            )
            let context = try #require(manager.context)

            // Grant the declarativeNetRequest permission.
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            let navigationDelegate = TestNavigationDelegate()

            webView.navigationDelegate = navigationDelegate

            await navigationDelegate.nextContentRuleListAction {
                webView.load(URLRequest(url: configuration.localhostAddress))
            }
        }
    }

    // FIXME when webkit.org/b/301720 is resolved.
    #if ASSERT_ENABLED
    @Test(.disabled("webkit.org/b/301720"))
    #else
    @Test
    #endif
    func blockedLoadInPrivateBrowsingTest() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<body style='background-color: blue'></body>"
            }
        }

        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        let declarativeNetRequestManifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "blockFrame",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        let rules =
            "[ { \"id\" : 1, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"frame\" } } ]"

        try await server.run { configuration in
            let manager = try loadWebExtension(
                manifest: declarativeNetRequestManifest,
                resources: ["background.js": backgroundScript, "rules.json": rules]
            )
            let context = try #require(manager.context)

            // Grant the declarativeNetRequest permission.
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)

            try await manager.waitForTestMessage("Load Tab")

            context.hasAccessToPrivateData = true

            let defaultWindow = try #require(manager.defaultWindow)
            manager.closeWindow(defaultWindow)

            let privateWindow = manager.openNewWindow(usingPrivateBrowsing: true)
            let privateTab = try #require(privateWindow.tabs.first)
            let webView = try #require(privateTab.webView)

            let navigationDelegate = TestNavigationDelegate()

            webView.navigationDelegate = navigationDelegate

            await navigationDelegate.nextContentRuleListAction {
                webView.load(URLRequest(url: configuration.localhostAddress))
            }
        }
    }

    @Test
    func getEnabledRulesets() async throws {
        let backgroundScript = """
            const enabledRulesets = await browser.declarativeNetRequest.getEnabledRulesets()
            browser.test.assertEq(enabledRulesets.length, 1, 'One ruleset should have been enabled')
            browser.test.assertEq(enabledRulesets[0], 'blockFrame', 'blockFrame should have been enabled')

            browser.test.notifyPass()
            """

        let declarativeNetRequestManifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "blockFrame",
                        "enabled": true,
                        "path": "rules.json",
                    ],
                    [
                        "id": "blockFrame2",
                        "enabled": false,
                        "path": "rules.json",
                    ],
                ]
            ],
        ]

        let rules =
            "[ { \"id\" : 1, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"frame\" } } ]"

        let manager = try loadWebExtension(
            manifest: declarativeNetRequestManifest,
            resources: ["background.js": backgroundScript, "rules.json": rules]
        )
        let context = try #require(manager.context)

        // Grant the declarativeNetRequest permission.
        context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)

        try await manager.run()
    }

    @Test
    func updateEnabledRulesets() async throws {
        let backgroundScript = """
            // Test invalid argument types
            browser.test.assertThrows(() => browser.declarativeNetRequest.updateEnabledRulesets({ enableRulesetIds: 5 }), /'options' value is invalid, because 'enableRulesetIds' is expected to be an array of strings/i)
            browser.test.assertThrows(() => browser.declarativeNetRequest.updateEnabledRulesets({ enableRulesetIds: '5' }), /'options' value is invalid, because 'enableRulesetIds' is expected to be an array of strings/i)
            browser.test.assertThrows(() => browser.declarativeNetRequest.updateEnabledRulesets({ enableRulesetIds: [5] }), /'options' value is invalid, because 'enableRulesetIds' is expected to be an array of strings/i)

            browser.test.assertThrows(() => browser.declarativeNetRequest.updateEnabledRulesets({ disableRulesetIds: 5 }), /'options' value is invalid, because 'disableRulesetIds' is expected to be an array of strings/i)
            browser.test.assertThrows(() => browser.declarativeNetRequest.updateEnabledRulesets({ disableRulesetIds: '5' }), /'options' value is invalid, because 'disableRulesetIds' is expected to be an array of strings/i)
            browser.test.assertThrows(() => browser.declarativeNetRequest.updateEnabledRulesets({ disableRulesetIds: [5] }), /'options' value is invalid, because 'disableRulesetIds' is expected to be an array of strings/i)

            // The given identifiers must be specified in the manifest.
            await browser.test.assertRejects(browser.declarativeNetRequest.updateEnabledRulesets({ enableRulesetIds: ['notPresentIdentifier'] }), /Invalid ruleset id/i)
            await browser.test.assertRejects(browser.declarativeNetRequest.updateEnabledRulesets({ disableRulesetIds: ['notPresentIdentifier'] }), /Invalid ruleset id/i)

            // Before any modifications
            let enabledRulesets = await browser.declarativeNetRequest.getEnabledRulesets()
            browser.test.assertEq(enabledRulesets.length, 1, 'One ruleset should have been enabled')
            browser.test.assertEq(enabledRulesets[0], 'blockFrame', 'blockFrame should have been enabled')

            // Turn off `blockFrame` and turn on `blockFrame2`.
            await browser.declarativeNetRequest.updateEnabledRulesets({ enableRulesetIds: ['blockFrame2'], disableRulesetIds: ['blockFrame'] })

            // After the modifications
            enabledRulesets = await browser.declarativeNetRequest.getEnabledRulesets()
            browser.test.assertEq(enabledRulesets.length, 1, 'One ruleset should have been enabled')
            browser.test.assertEq(enabledRulesets[0], 'blockFrame2', 'blockFrame2 should have been enabled')

            browser.test.notifyPass()
            """

        let declarativeNetRequestManifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "blockFrame",
                        "enabled": true,
                        "path": "rules.json",
                    ],
                    [
                        "id": "blockFrame2",
                        "enabled": false,
                        "path": "rules.json",
                    ],
                ]
            ],
        ]

        let rules =
            "[ { \"id\" : 1, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"frame\" } } ]"

        let manager = try loadWebExtension(
            manifest: declarativeNetRequestManifest,
            resources: ["background.js": backgroundScript, "rules.json": rules]
        )
        let context = try #require(manager.context)

        // Grant the declarativeNetRequest permission.
        context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)

        try await manager.run()
    }

    @Test
    func updateEnabledRulesetsPerformsCompilation() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<body style='background-color: blue'></body>"
            }
        }

        let backgroundScript = """
            // Before any modifications
            let enabledRulesets = await browser.declarativeNetRequest.getEnabledRulesets()
            browser.test.assertEq(enabledRulesets.length, 0, 'No rulesets should have been enabled')

            // Turn on `blockFrame`.
            await browser.declarativeNetRequest.updateEnabledRulesets({ enableRulesetIds: ['blockFrame'] })

            // After the modifications
            enabledRulesets = await browser.declarativeNetRequest.getEnabledRulesets()
            browser.test.assertEq(enabledRulesets.length, 1, 'One ruleset should have been enabled')
            browser.test.assertEq(enabledRulesets[0], 'blockFrame', 'blockFrame should have been enabled')

            browser.test.sendMessage('Load Tab')
            """

        let declarativeNetRequestManifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "blockFrame",
                        "enabled": false,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        let rules =
            "[ { \"id\" : 1, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"frame\" } } ]"

        try await server.run { configuration in
            let manager = try loadWebExtension(
                manifest: declarativeNetRequestManifest,
                resources: ["background.js": backgroundScript, "rules.json": rules]
            )
            let context = try #require(manager.context)

            // Grant the declarativeNetRequest permission.
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            let navigationDelegate = TestNavigationDelegate()

            webView.navigationDelegate = navigationDelegate

            await navigationDelegate.nextContentRuleListAction {
                webView.load(URLRequest(url: configuration.localhostAddress))
            }
        }
    }

    @Test
    func isRegexSupported() async throws {
        let backgroundScript = """
            // Invalid arguments
            browser.test.assertThrows(() => browser.declarativeNetRequest.isRegexSupported({ }), /'regexOptions' value is invalid, because it is missing required keys: 'regex'/i)
            browser.test.assertThrows(() => browser.declarativeNetRequest.isRegexSupported({ regex: 5 }), /'regexOptions' value is invalid, because 'regex' is expected to be a string/i)
            browser.test.assertThrows(() => browser.declarativeNetRequest.isRegexSupported({ regex: ['.*'] }), /'regexOptions' value is invalid, because 'regex' is expected to be a string/i)
            browser.test.assertThrows(() => browser.declarativeNetRequest.isRegexSupported({ regex: '.*', isCaseSensitive: 'foo' }), /'regexOptions' value is invalid, because 'isCaseSensitive' is expected to be a boolean/i)
            browser.test.assertThrows(() => browser.declarativeNetRequest.isRegexSupported({ regex: '.*', isCaseSensitive: 5 }), /'regexOptions' value is invalid, because 'isCaseSensitive' is expected to be a boolean/i)
            browser.test.assertThrows(() => browser.declarativeNetRequest.isRegexSupported({ regex: '.*', requireCapturing: 'bar' }), /'regexOptions' value is invalid, because 'requireCapturing' is expected to be a boolean/i)
            browser.test.assertThrows(() => browser.declarativeNetRequest.isRegexSupported({ regex: '.*', requireCapturing: 5 }), /'regexOptions' value is invalid, because 'requireCapturing' is expected to be a boolean/i)

            // Passing cases
            browser.test.assertTrue((await browser.declarativeNetRequest.isRegexSupported({ regex: '.*' })).isSupported)
            browser.test.assertTrue((await browser.declarativeNetRequest.isRegexSupported({ regex: 'a.*b' })).isSupported)

            // Failing cases
            browser.test.assertFalse((await browser.declarativeNetRequest.isRegexSupported({ regex: 'Ä' })).isSupported)
            browser.test.assertFalse((await browser.declarativeNetRequest.isRegexSupported({ regex: '' })).isSupported)
            browser.test.assertFalse((await browser.declarativeNetRequest.isRegexSupported({ regex: 'a^' })).isSupported)
            browser.test.assertFalse((await browser.declarativeNetRequest.isRegexSupported({ regex: 'this|that' })).isSupported)
            browser.test.assertFalse((await browser.declarativeNetRequest.isRegexSupported({ regex: '$$' })).isSupported)

            browser.test.notifyPass()
            """

        let declarativeNetRequestManifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let manager = try loadWebExtension(manifest: declarativeNetRequestManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        // Grant the declarativeNetRequest permission.
        context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)

        try await manager.run()
    }

    @Test
    func setExtensionActionOptions() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<body style='background-color: blue'></body>"
            }
        }

        let backgroundScript = """
            const [currentTab] = await browser.tabs.query({ active: true, currentWindow: true })
            browser.declarativeNetRequest.setExtensionActionOptions({ displayActionCountAsBadgeText: true })

            setTimeout(() => {
              browser.declarativeNetRequest.setExtensionActionOptions({ tabUpdate: { tabId: currentTab.id, increment: 2 } })
              browser.test.sendMessage('Check badge text')
            }, 1000)

            browser.test.sendMessage('Load Tab')
            """

        let declarativeNetRequestManifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest", "tabs"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "blockFrame",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        let rules =
            "[ { \"id\" : 1, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"frame\" } } ]"

        try await server.run { configuration in
            let manager = try loadWebExtension(
                manifest: declarativeNetRequestManifest,
                resources: ["background.js": backgroundScript, "rules.json": rules]
            )
            let context = try #require(manager.context)

            // Grant the declarativeNetRequest and tabs permission.
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.tabs)

            try await manager.waitForTestMessage("Load Tab")

            let defaultTab = try #require(manager.defaultTab)
            let webView = try #require(defaultTab.webView)
            let navigationDelegate = TestNavigationDelegate()

            webView.navigationDelegate = navigationDelegate

            await navigationDelegate.nextContentRuleListAction {
                webView.load(URLRequest(url: configuration.localhostAddress))
            }

            let action = try #require(context.action(for: defaultTab))

            // The badge text should be "1" to match the one resource that was blocked.
            #expect(action.badgeText == "1")

            try await manager.waitForTestMessage("Check badge text")

            // The badge text should now be "3" since we incremented it by two.
            #expect(action.badgeText == "3")
        }
    }

    @Test
    func getMatchedRules() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<body style='background-color: blue'></body>"
            }
        }

        let backgroundScript = """
            setTimeout(async () => {
              const matchedRules = await browser.declarativeNetRequest.getMatchedRules()
              browser.test.assertEq(matchedRules.rulesMatchedInfo.length, 1)
              const matchedURL = matchedRules.rulesMatchedInfo[0].request.url
              browser.test.assertTrue(matchedURL.includes('localhost'), 'URL should include localhost')
              browser.test.assertTrue(matchedURL.includes('frame'), 'URL should include frame')
              browser.test.notifyPass()
            }, 1000)

            browser.test.sendMessage('Load Tab')
            """

        let declarativeNetRequestManifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest", "declarativeNetRequestFeedback"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "blockFrame",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        let rules =
            "[ { \"id\" : 1, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"frame\" } } ]"

        try await server.run { configuration in
            let manager = try loadWebExtension(
                manifest: declarativeNetRequestManifest,
                resources: ["background.js": backgroundScript, "rules.json": rules]
            )
            let context = try #require(manager.context)

            // Grant the declarativeNetRequestFeedback permission.
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequestFeedback)

            let requestURL = configuration.localhostAddress
            let urlRequest = URLRequest(url: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL.appending(path: "frame.html"))

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(urlRequest)

            try await manager.run()
        }
    }

    @Test
    func sessionRules() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<body style='background-color: blue'></body>"
            }
        }

        let backgroundScript = """
            let sessionRules = await browser.declarativeNetRequest.getSessionRules()
            browser.test.assertEq(sessionRules.length, 0)
            await browser.declarativeNetRequest.updateSessionRules({ addRules: [{ id: 1, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'frame' } }] })
            sessionRules = await browser.declarativeNetRequest.getSessionRules()
            browser.test.assertEq(sessionRules.length, 1)

            browser.test.sendMessage('Load Tab')
            """

        let declarativeNetRequestManifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: declarativeNetRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            // Grant the declarativeNetRequest permission.
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)

            #expect(!context.hasContentModificationRules)

            try await manager.waitForTestMessage("Load Tab")

            #expect(context.hasContentModificationRules)

            let webView = try #require(manager.defaultTab?.webView)
            let navigationDelegate = TestNavigationDelegate()

            webView.navigationDelegate = navigationDelegate

            await navigationDelegate.nextContentRuleListAction {
                webView.load(URLRequest(url: configuration.localhostAddress))
            }
        }
    }

    @Test
    func getSessionRules() async throws {
        let backgroundScript = """
            let sessionRules = await browser.declarativeNetRequest.getSessionRules()
            browser.test.assertEq(sessionRules.length, 0)

            await browser.declarativeNetRequest.updateSessionRules({ addRules: [{ id: 1, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'foo' } }] })
            await browser.declarativeNetRequest.updateSessionRules({ addRules: [{ id: 2, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'bar' } }] })
            await browser.declarativeNetRequest.updateSessionRules({ addRules: [{ id: 3, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'baz' } }] })

            await browser.test.assertRejects(browser.declarativeNetRequest.getSessionRules({ ruleIds: 1 }))
            await browser.test.assertRejects(browser.declarativeNetRequest.getSessionRules({ ruleIds: '' }))
            await browser.test.assertRejects(browser.declarativeNetRequest.getSessionRules({ ruleIds: true }))
            await browser.test.assertRejects(browser.declarativeNetRequest.getSessionRules({ ruleIds: { } }))
            await browser.test.assertRejects(browser.declarativeNetRequest.getSessionRules({ ruleIds: function foo() { } }))
            await browser.test.assertRejects(browser.declarativeNetRequest.getSessionRules({ ruleIds: [ '' ] }))
            await browser.test.assertRejects(browser.declarativeNetRequest.getSessionRules({ ruleIds: [ true ] }))
            await browser.test.assertRejects(browser.declarativeNetRequest.getSessionRules({ ruleIds: [ { } ] }))
            await browser.test.assertRejects(browser.declarativeNetRequest.getSessionRules({ ruleIds: [ function foo() { } ] }))

            sessionRules = await browser.declarativeNetRequest.getSessionRules({ })
            sessionRules = sessionRules.sort((a, b) => { a.id < b.id })
            browser.test.assertEq(sessionRules.length, 3)
            browser.test.assertEq(sessionRules[0].id, 1)
            browser.test.assertEq(sessionRules[1].id, 2)
            browser.test.assertEq(sessionRules[2].id, 3)

            sessionRules = await browser.declarativeNetRequest.getSessionRules({ ruleIds: [ ] })
            sessionRules = sessionRules.sort((a, b) => { a.id < b.id })
            browser.test.assertEq(sessionRules.length, 3)
            browser.test.assertEq(sessionRules[0].id, 1)
            browser.test.assertEq(sessionRules[1].id, 2)
            browser.test.assertEq(sessionRules[2].id, 3)

            sessionRules = await browser.declarativeNetRequest.getSessionRules({ ruleIds: [ 1 ] })
            browser.test.assertEq(sessionRules.length, 1)
            browser.test.assertEq(sessionRules[0].id, 1)

            sessionRules = await browser.declarativeNetRequest.getSessionRules({ ruleIds: [ 1, 2 ] })
            sessionRules = sessionRules.sort((a, b) => { a.id < b.id })
            browser.test.assertEq(sessionRules.length, 2)
            browser.test.assertEq(sessionRules[0].id, 1)
            browser.test.assertEq(sessionRules[1].id, 2)

            sessionRules = await browser.declarativeNetRequest.getSessionRules({ ruleIds: [ 1, 2, 3 ] })
            sessionRules = sessionRules.sort((a, b) => { a.id < b.id })
            browser.test.assertEq(sessionRules.length, 3)
            browser.test.assertEq(sessionRules[0].id, 1)
            browser.test.assertEq(sessionRules[1].id, 2)
            browser.test.assertEq(sessionRules[2].id, 3)

            browser.test.notifyPass()
            """

        let declarativeNetRequestManifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let manager = try loadWebExtension(manifest: declarativeNetRequestManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func removeSessionRules() async throws {
        let backgroundScript = """
            let sessionRules = await browser.declarativeNetRequest.getSessionRules()
            browser.test.assertEq(sessionRules.length, 0)

            await browser.declarativeNetRequest.updateSessionRules({ addRules: [{ id: 1, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'foo' } }] })
            await browser.declarativeNetRequest.updateSessionRules({ addRules: [{ id: 2, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'bar' } }] })
            await browser.declarativeNetRequest.updateSessionRules({ addRules: [{ id: 3, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'baz' } }] })

            sessionRules = await browser.declarativeNetRequest.getSessionRules({ })
            browser.test.assertEq(sessionRules.length, 3)

            await browser.declarativeNetRequest.updateSessionRules({ removeRuleIds: [1, 2, 3] })

            sessionRules = await browser.declarativeNetRequest.getSessionRules({ })
            browser.test.assertEq(sessionRules.length, 0)

            browser.test.notifyPass()
            """

        let declarativeNetRequestManifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let manager = try loadWebExtension(manifest: declarativeNetRequestManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func dynamicRules() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<body style='background-color: blue'></body>"
            }
        }

        let backgroundScript = """
            let dynamicRules = await browser.declarativeNetRequest.getDynamicRules()
            if (dynamicRules.length == 0) {
              await browser.declarativeNetRequest.updateDynamicRules({ addRules: [{ id: 1, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'frame' } }] })
              dynamicRules = await browser.declarativeNetRequest.getDynamicRules()
              browser.test.assertEq(dynamicRules.length, 1)

              browser.test.sendMessage('Unload extension')
            } else {
              browser.test.assertEq(dynamicRules.length, 1)

              browser.test.sendMessage('Load Tab')
            }
            """

        let declarativeNetRequestManifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        try await server.run { configuration in
            let manager = parseWebExtension(
                manifest: declarativeNetRequestManifest,
                resources: ["background.js": backgroundScript],
                configuration: ._temporary()
            )
            let context = try #require(manager.context)

            // Give the extension a unique identifier so it opts into saving data in the temporary configuration.
            context.uniqueIdentifier = "org.webkit.test.extension (76C788B8)"

            // Grant the declarativeNetRequest permission.
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)

            #expect(!context.hasContentModificationRules)

            manager.load()
            try await manager.waitForTestMessage("Unload extension")

            #expect(context.hasContentModificationRules)

            var storageDirectory = try #require(manager.controller.configuration._storageDirectoryPath)
            storageDirectory = (storageDirectory as NSString).appendingPathComponent(context.uniqueIdentifier)
            #expect(
                FileManager.default.fileExists(
                    atPath: (storageDirectory as NSString).appendingPathComponent("DeclarativeNetRequestContentRuleList.data")
                )
            )

            manager.unload()
            manager.load()
            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            let navigationDelegate = TestNavigationDelegate()

            webView.navigationDelegate = navigationDelegate

            await navigationDelegate.nextContentRuleListAction {
                webView.load(URLRequest(url: configuration.localhostAddress))
            }
        }
    }

    @Test
    func getDynamicRules() async throws {
        let backgroundScript = """
            let dynamicRules = await browser.declarativeNetRequest.getDynamicRules()
            browser.test.assertEq(dynamicRules.length, 0)

            await browser.declarativeNetRequest.updateDynamicRules({ addRules: [{ id: 1, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'foo' } }] })
            await browser.declarativeNetRequest.updateDynamicRules({ addRules: [{ id: 2, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'bar' } }] })
            await browser.declarativeNetRequest.updateDynamicRules({ addRules: [{ id: 3, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'baz' } }] })

            await browser.test.assertRejects(browser.declarativeNetRequest.getDynamicRules({ ruleIds: 1 }))
            await browser.test.assertRejects(browser.declarativeNetRequest.getDynamicRules({ ruleIds: '' }))
            await browser.test.assertRejects(browser.declarativeNetRequest.getDynamicRules({ ruleIds: true }))
            await browser.test.assertRejects(browser.declarativeNetRequest.getDynamicRules({ ruleIds: { } }))
            await browser.test.assertRejects(browser.declarativeNetRequest.getDynamicRules({ ruleIds: function foo() { } }))
            await browser.test.assertRejects(browser.declarativeNetRequest.getDynamicRules({ ruleIds: [ '' ] }))
            await browser.test.assertRejects(browser.declarativeNetRequest.getDynamicRules({ ruleIds: [ true ] }))
            await browser.test.assertRejects(browser.declarativeNetRequest.getDynamicRules({ ruleIds: [ { } ] }))
            await browser.test.assertRejects(browser.declarativeNetRequest.getDynamicRules({ ruleIds: [ function foo() { } ] }))

            dynamicRules = await browser.declarativeNetRequest.getDynamicRules({ })
            dynamicRules = dynamicRules.sort((a, b) => { a.id < b.id })
            browser.test.assertEq(dynamicRules.length, 3)
            browser.test.assertEq(dynamicRules[0].id, 1)
            browser.test.assertEq(dynamicRules[1].id, 2)
            browser.test.assertEq(dynamicRules[2].id, 3)

            dynamicRules = await browser.declarativeNetRequest.getDynamicRules({ ruleIds: [ ] })
            dynamicRules = dynamicRules.sort((a, b) => { a.id < b.id })
            browser.test.assertEq(dynamicRules.length, 3)
            browser.test.assertEq(dynamicRules[0].id, 1)
            browser.test.assertEq(dynamicRules[1].id, 2)
            browser.test.assertEq(dynamicRules[2].id, 3)

            dynamicRules = await browser.declarativeNetRequest.getDynamicRules({ ruleIds: [ 1 ] })
            browser.test.assertEq(dynamicRules.length, 1)
            browser.test.assertEq(dynamicRules[0].id, 1)

            dynamicRules = await browser.declarativeNetRequest.getDynamicRules({ ruleIds: [ 1, 2 ] })
            dynamicRules = dynamicRules.sort((a, b) => { a.id < b.id })
            browser.test.assertEq(dynamicRules.length, 2)
            browser.test.assertEq(dynamicRules[0].id, 1)
            browser.test.assertEq(dynamicRules[1].id, 2)

            dynamicRules = await browser.declarativeNetRequest.getDynamicRules({ ruleIds: [ 1, 2, 3 ] })
            dynamicRules = dynamicRules.sort((a, b) => { a.id < b.id })
            browser.test.assertEq(dynamicRules.length, 3)
            browser.test.assertEq(dynamicRules[0].id, 1)
            browser.test.assertEq(dynamicRules[1].id, 2)
            browser.test.assertEq(dynamicRules[2].id, 3)

            browser.test.notifyPass()
            """

        let declarativeNetRequestManifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let manager = try loadWebExtension(manifest: declarativeNetRequestManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func removeDynamicRules() async throws {
        let backgroundScript = """
            let dynamicRules = await browser.declarativeNetRequest.getDynamicRules()
            browser.test.assertEq(dynamicRules.length, 0)

            await browser.declarativeNetRequest.updateDynamicRules({ addRules: [{ id: 1, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'foo' } }] })
            await browser.declarativeNetRequest.updateDynamicRules({ addRules: [{ id: 2, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'bar' } }] })
            await browser.declarativeNetRequest.updateDynamicRules({ addRules: [{ id: 3, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'baz' } }] })

            dynamicRules = await browser.declarativeNetRequest.getDynamicRules({ })
            browser.test.assertEq(dynamicRules.length, 3)

            await browser.declarativeNetRequest.updateDynamicRules({ removeRuleIds: [1, 2, 3] })

            dynamicRules = await browser.declarativeNetRequest.getDynamicRules({ })
            browser.test.assertEq(dynamicRules.length, 0)

            browser.test.notifyPass()
            """

        let declarativeNetRequestManifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let manager = try loadWebExtension(manifest: declarativeNetRequestManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    private func runRedirectRule(usesEnhancedSecurity: Bool) async throws {
        let pageScript = """
            <script>
              browser.test.assertTrue(location.href.startsWith('http://127.0.0.1'), 'Final load should be via IP address')

              browser.test.notifyPass()
            </script>
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                pageScript
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "description": "Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "permissions": ["declarativeNetRequestWithHostAccess"],
            "host_permissions": ["*://localhost/*"],

            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "redirectRule",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        let extensionConfiguration: WKWebExtensionController.Configuration = .nonPersistent()

        if usesEnhancedSecurity {
            let preferences = extensionConfiguration.webViewConfiguration.preferences
            for feature in WKPreferences._features() where feature.key == "EnhancedSecurityHeuristicsEnabled" {
                preferences._setEnabled(true, for: feature)
            }
        }

        try await server.run { configuration in
            let urlRequest = URLRequest(url: configuration.localhostAddress)
            let redirectURL = configuration.address.absoluteString

            let rules: [[String: Any]] = [
                [
                    "id": 1,
                    "priority": 1,

                    "action": [
                        "type": "redirect",
                        "redirect": [
                            "url": redirectURL
                        ],
                    ],

                    "condition": [
                        "urlFilter": "localhost",
                        "resourceTypes": ["main_frame"],
                    ],
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "rules.json": rules,
            ]

            let manager = try loadWebExtension(
                manifest: manifest,
                resources: resources,
                configuration: extensionConfiguration,
                usesEnhancedSecurity: usesEnhancedSecurity
            )
            let context = try #require(manager.context)

            var redirectNavigationActions: [WKNavigationAction] = []

            let navigationDelegate = TestNavigationDelegate()
            navigationDelegate.decidePolicyForNavigationAction = { navigationAction, decisionHandler in
                decisionHandler(.allow)

                if navigationAction.request.url?.absoluteString == redirectURL {
                    redirectNavigationActions.append(navigationAction)
                }
            }

            let webView = try #require(manager.defaultTab?.webView)
            webView.navigationDelegate = navigationDelegate

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            webView.load(urlRequest)

            try await manager.run()

            #expect(redirectNavigationActions.allSatisfy { $0.isContentRuleListRedirect })
        }
    }

    @Test
    func redirectRule() async throws {
        try await runRedirectRule(usesEnhancedSecurity: false)
    }

    @Test
    func redirectRuleWithEnhancedSecurity() async throws {
        try await runRedirectRule(usesEnhancedSecurity: true)
    }

    @Test
    func highPriorityStaticRedirectFavoredOverLowPrioritySessionRedirect() async throws {
        let correctPageScript = """
            <script>
              browser.test.notifyPass()
            </script>
            """

        let incorrectPageScript = """
            <script>
              browser.test.notifyFail('Low-priority session redirect should not win over high-priority static redirect')
            </script>
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<body></body>"
            }
            Route("/correct", headerFields: ["Content-Type": "text/html"]) {
                correctPageScript
            }
            Route("/incorrect", headerFields: ["Content-Type": "text/html"]) {
                incorrectPageScript
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "description": "Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "permissions": ["declarativeNetRequestWithHostAccess"],
            "host_permissions": ["*://localhost/*"],

            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "redirectRule",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        try await server.run { configuration in
            let urlRequest = URLRequest(url: configuration.localhostAddress)
            let correctRedirectURL = configuration.address.appending(path: "correct").absoluteString
            let incorrectRedirectURL = configuration.address.appending(path: "incorrect").absoluteString

            let rules: [[String: Any]] = [
                [
                    "id": 1,
                    "priority": 200,

                    "action": [
                        "type": "redirect",
                        "redirect": [
                            "url": correctRedirectURL
                        ],
                    ],

                    "condition": [
                        "urlFilter": "localhost",
                        "resourceTypes": ["main_frame"],
                    ],
                ]
            ]

            let backgroundScript = """
                await browser.declarativeNetRequest.updateSessionRules({ addRules: [{ id: 2, priority: 1, action: { type: 'redirect', redirect: { url: '\(incorrectRedirectURL)' } }, condition: { urlFilter: 'localhost', resourceTypes: ['main_frame'] } }] })
                browser.test.sendMessage('Load Tab')
                """

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "rules.json": rules,
            ]

            let manager = try loadWebExtension(manifest: manifest, resources: resources)
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(urlRequest)

            try await manager.run()
        }
    }

    @Test
    func highestPriorityRedirectWinsRegardlessOfDeclarationOrder() async throws {
        let correctPageScript = """
            <script>
              browser.test.notifyPass()
            </script>
            """

        let incorrectPageScript = """
            <script>
              browser.test.notifyFail('Low-priority redirect should not win over high-priority redirect declared later')
            </script>
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<body></body>"
            }
            Route("/correct", headerFields: ["Content-Type": "text/html"]) {
                correctPageScript
            }
            Route("/incorrect", headerFields: ["Content-Type": "text/html"]) {
                incorrectPageScript
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "description": "Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "permissions": ["declarativeNetRequestWithHostAccess"],
            "host_permissions": ["*://localhost/*"],

            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "redirectRule",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let urlRequest = URLRequest(url: configuration.localhostAddress)
            let correctRedirectURL = configuration.address.appending(path: "correct").absoluteString
            let incorrectRedirectURL = configuration.address.appending(path: "incorrect").absoluteString

            let rules: [[String: Any]] = [
                [
                    "id": 1,
                    "priority": 1,

                    "action": [
                        "type": "redirect",
                        "redirect": [
                            "url": incorrectRedirectURL
                        ],
                    ],

                    "condition": [
                        "urlFilter": "localhost",
                        "resourceTypes": ["main_frame"],
                    ],
                ],
                [
                    "id": 2,
                    "priority": 100,

                    "action": [
                        "type": "redirect",
                        "redirect": [
                            "url": correctRedirectURL
                        ],
                    ],

                    "condition": [
                        "urlFilter": "localhost",
                        "resourceTypes": ["main_frame"],
                    ],
                ],
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "rules.json": rules,
            ]

            let manager = try loadWebExtension(manifest: manifest, resources: resources)
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(urlRequest)

            try await manager.run()
        }
    }

    @Test
    func higherPriorityNoOpRedirectSuppressesLowerPriorityRedirect() async throws {
        let correctPageScript = """
            <script>
              browser.test.notifyPass()
            </script>
            """

        let incorrectPageScript = """
            <script>
              browser.test.notifyFail('Lower-priority redirect should not apply when a higher-priority no-op redirect matches')
            </script>
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                correctPageScript
            }
            Route("/incorrect", headerFields: ["Content-Type": "text/html"]) {
                incorrectPageScript
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "description": "Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "permissions": ["declarativeNetRequestWithHostAccess"],
            "host_permissions": ["*://localhost/*"],

            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "redirectRule",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let urlRequest = URLRequest(url: configuration.localhostAddress)
            let noOpRedirectURL = configuration.localhostAddress.absoluteString
            let incorrectRedirectURL = configuration.address.appending(path: "incorrect").absoluteString

            let rules: [[String: Any]] = [
                [
                    "id": 1,
                    "priority": 2,

                    "action": [
                        "type": "redirect",
                        "redirect": [
                            "url": noOpRedirectURL
                        ],
                    ],

                    "condition": [
                        "urlFilter": "localhost",
                        "resourceTypes": ["main_frame"],
                    ],
                ],
                [
                    "id": 2,
                    "priority": 1,

                    "action": [
                        "type": "redirect",
                        "redirect": [
                            "url": incorrectRedirectURL
                        ],
                    ],

                    "condition": [
                        "urlFilter": "localhost",
                        "resourceTypes": ["main_frame"],
                    ],
                ],
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "rules.json": rules,
            ]

            let manager = try loadWebExtension(manifest: manifest, resources: resources)
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(urlRequest)

            try await manager.run()
        }
    }

    @Test
    func lowerPriorityRedirectDoesNotComposeOntoWinner() async throws {
        let correctPageScript = """
            <script>
              browser.test.notifyPass()
            </script>
            """

        let noRedirectPageScript = """
            <script>
              browser.test.notifyFail('No redirect fired')
            </script>
            """

        let composedPageScript = """
            <script>
              browser.test.notifyFail('Lower-priority transform should not compose onto the winning redirect')
            </script>
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                noRedirectPageScript
            }
            Route("/winner", headerFields: ["Content-Type": "text/html"]) {
                correctPageScript
            }
            Route("/winner?b=2", headerFields: ["Content-Type": "text/html"]) {
                composedPageScript
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "description": "Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "permissions": ["declarativeNetRequestWithHostAccess"],
            "host_permissions": ["*://localhost/*"],

            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "redirectRule",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let urlRequest = URLRequest(url: configuration.localhostAddress)
            let winnerRedirectURL = configuration.address.appending(path: "winner").absoluteString

            let rules: [[String: Any]] = [
                [
                    "id": 1,
                    "priority": 2,

                    "action": [
                        "type": "redirect",
                        "redirect": [
                            "url": winnerRedirectURL
                        ],
                    ],

                    "condition": [
                        "urlFilter": "localhost",
                        "resourceTypes": ["main_frame"],
                    ],
                ],
                [
                    "id": 2,
                    "priority": 1,

                    "action": [
                        "type": "redirect",
                        "redirect": [
                            "transform": [
                                "queryTransform": [
                                    "addOrReplaceParams": [["key": "b", "value": "2"]]
                                ]
                            ]
                        ],
                    ],

                    "condition": [
                        "urlFilter": "localhost",
                        "resourceTypes": ["main_frame"],
                    ],
                ],
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "rules.json": rules,
            ]

            let manager = try loadWebExtension(manifest: manifest, resources: resources)
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(urlRequest)

            try await manager.run()
        }
    }

    @Test
    func lastDeclaredRedirectWinsAmongEqualPriority() async throws {
        let correctPageScript = """
            <script>
              browser.test.notifyPass()
            </script>
            """

        let incorrectPageScript = """
            <script>
              browser.test.notifyFail('First-declared redirect should not win over a later-declared redirect of equal priority')
            </script>
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<body></body>"
            }
            Route("/correct", headerFields: ["Content-Type": "text/html"]) {
                correctPageScript
            }
            Route("/incorrect", headerFields: ["Content-Type": "text/html"]) {
                incorrectPageScript
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "description": "Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "permissions": ["declarativeNetRequestWithHostAccess"],
            "host_permissions": ["*://localhost/*"],

            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "redirectRule",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let urlRequest = URLRequest(url: configuration.localhostAddress)
            let correctRedirectURL = configuration.address.appending(path: "correct").absoluteString
            let incorrectRedirectURL = configuration.address.appending(path: "incorrect").absoluteString

            let rules: [[String: Any]] = [
                [
                    "id": 1,
                    "priority": 10,

                    "action": [
                        "type": "redirect",
                        "redirect": [
                            "url": incorrectRedirectURL
                        ],
                    ],

                    "condition": [
                        "urlFilter": "localhost",
                        "resourceTypes": ["main_frame"],
                    ],
                ],
                [
                    "id": 2,
                    "priority": 10,

                    "action": [
                        "type": "redirect",
                        "redirect": [
                            "url": correctRedirectURL
                        ],
                    ],

                    "condition": [
                        "urlFilter": "localhost",
                        "resourceTypes": ["main_frame"],
                    ],
                ],
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "rules.json": rules,
            ]

            let manager = try loadWebExtension(manifest: manifest, resources: resources)
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(urlRequest)

            try await manager.run()
        }
    }

    @Test
    func redirectRuleWithoutHostAccessPermission() async throws {
        let pageScript = """
            <script>
              browser.test.assertTrue(location.href.startsWith('http://localhost'), 'Final load should be via localhost')

              browser.test.notifyPass()
            </script>
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                pageScript
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "description": "Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "permissions": ["declarativeNetRequestWithHostAccess"],
            "host_permissions": ["*://localhost/*"],

            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "redirectRule",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let urlRequest = URLRequest(url: configuration.localhostAddress)
            let redirectURL = configuration.address.absoluteString

            let rules: [[String: Any]] = [
                [
                    "id": 1,
                    "priority": 1,

                    "action": [
                        "type": "redirect",
                        "redirect": [
                            "url": redirectURL
                        ],
                    ],

                    "condition": [
                        "urlFilter": "localhost",
                        "resourceTypes": ["main_frame"],
                    ],
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "rules.json": rules,
            ]

            let manager = try loadWebExtension(manifest: manifest, resources: resources)
            let context = try #require(manager.context)

            context.setPermissionStatus(.deniedExplicitly, for: WKWebExtension.Permission.declarativeNetRequestWithHostAccess)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(urlRequest)

            try await manager.run()
        }
    }

    @Test
    func redirectRuleWithoutHostPermission() async throws {
        let pageScript = """
            <script>
              browser.test.assertTrue(location.href.startsWith('http://localhost'), 'Final load should be via localhost')

              browser.test.notifyPass()
            </script>
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                pageScript
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "description": "Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "permissions": ["declarativeNetRequestWithHostAccess"],
            "host_permissions": ["*://localhost/*"],

            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "redirectRule",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let urlRequest = URLRequest(url: configuration.localhostAddress)
            let redirectURL = configuration.address.absoluteString

            let rules: [[String: Any]] = [
                [
                    "id": 1,
                    "priority": 1,

                    "action": [
                        "type": "redirect",
                        "redirect": [
                            "url": redirectURL
                        ],
                    ],

                    "condition": [
                        "urlFilter": "localhost",
                        "resourceTypes": ["main_frame"],
                    ],
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "rules.json": rules,
            ]

            let manager = try loadWebExtension(manifest: manifest, resources: resources)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(urlRequest)

            try await manager.run()
        }
    }

    @Test
    func modifyHeadersRule() async throws {
        let pageScript = """
            <script>
              browser.test.assertEq(document.referrer, 'https://example.com/')

              browser.test.notifyPass()
            </script>
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                pageScript
            }
        }

        let rules: [[String: Any]] = [
            [
                "id": 1,
                "priority": 1,

                "action": [
                    "type": "modifyHeaders",
                    "requestHeaders": [
                        [
                            "header": "Referer",
                            "operation": "set",
                            "value": "https://example.com/",
                        ]
                    ],
                ],

                "condition": [
                    "urlFilter": "localhost",
                    "resourceTypes": ["main_frame"],
                ],
            ]
        ]

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "description": "Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "permissions": ["declarativeNetRequestWithHostAccess"],
            "host_permissions": ["*://localhost/*"],

            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "modifyHeadersRule",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "rules.json": rules,
        ]

        try await server.run { configuration in
            let urlRequest = URLRequest(url: configuration.localhostAddress)

            let manager = try loadWebExtension(manifest: manifest, resources: resources)
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(urlRequest)

            try await manager.run()
        }
    }

    @Test
    func modifyHeadersRuleWithoutHostAccessPermission() async throws {
        let pageScript = """
            <script>
              browser.test.assertEq(document.referrer, '')

              browser.test.notifyPass()
            </script>
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                pageScript
            }
        }

        let rules: [[String: Any]] = [
            [
                "id": 1,
                "priority": 1,

                "action": [
                    "type": "modifyHeaders",
                    "requestHeaders": [
                        [
                            "header": "Referer",
                            "operation": "set",
                            "value": "https://example.com/",
                        ]
                    ],
                ],

                "condition": [
                    "urlFilter": "localhost",
                    "resourceTypes": ["main_frame"],
                ],
            ]
        ]

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "description": "Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "permissions": ["declarativeNetRequestWithHostAccess"],
            "host_permissions": ["*://localhost/*"],

            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "modifyHeadersRule",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "rules.json": rules,
        ]

        try await server.run { configuration in
            let urlRequest = URLRequest(url: configuration.localhostAddress)

            let manager = try loadWebExtension(manifest: manifest, resources: resources)
            let context = try #require(manager.context)

            context.setPermissionStatus(.deniedExplicitly, for: WKWebExtension.Permission.declarativeNetRequestWithHostAccess)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(urlRequest)

            try await manager.run()
        }
    }

    @Test
    func modifyHeadersRuleWithoutHostPermission() async throws {
        let pageScript = """
            <script>
              browser.test.assertEq(document.referrer, '')

              browser.test.notifyPass()
            </script>
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                pageScript
            }
        }

        let rules: [[String: Any]] = [
            [
                "id": 1,
                "priority": 1,

                "action": [
                    "type": "modifyHeaders",
                    "requestHeaders": [
                        [
                            "header": "Referer",
                            "operation": "set",
                            "value": "https://example.com/",
                        ]
                    ],
                ],

                "condition": [
                    "urlFilter": "localhost",
                    "resourceTypes": ["main_frame"],
                ],
            ]
        ]

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "description": "Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "permissions": ["declarativeNetRequestWithHostAccess"],
            "host_permissions": ["*://localhost/*"],

            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "modifyHeadersRule",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "rules.json": rules,
        ]

        try await server.run { configuration in
            let urlRequest = URLRequest(url: configuration.localhostAddress)

            let manager = try loadWebExtension(manifest: manifest, resources: resources)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(urlRequest)

            try await manager.run()
        }
    }

    @Test
    func mainFrameAllowAllRequests() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<script>browser.test.notifyPass()</script>"
            }
        }

        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        let rules =
            "[ { \"id\" : 1, \"priority\": 2, \"action\" : { \"type\" : \"allowAllRequests\" }, \"condition\" : { \"urlFilter\" : \"*\", \"resourceTypes\" : [ \"main_frame\" ] } }, { \"id\" : 2, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"frame\" } } ]"

        let declarativeNetRequestManifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "blockAndAllowRules",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(
                manifest: declarativeNetRequestManifest,
                resources: ["background.js": backgroundScript, "rules.json": rules]
            )
            let context = try #require(manager.context)

            // Grant the declarativeNetRequest permission.
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)

            let requestURL = configuration.localhostAddress
            let urlRequest = URLRequest(url: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL.appending(path: "frame.html"))

            webView.load(urlRequest)

            try await manager.run()
        }
    }

    #if ENABLE_DNR_ON_RULE_MATCHED_DEBUG
    @Test
    func onRuleMatchedDebugWithoutPermission() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let backgroundScript = """
            browser.test.assertEq(typeof browser.declarativeNetRequest, 'object')
            browser.test.assertEq(typeof browser.declarativeNetRequest.onRuleMatchedDebug, 'undefined')
            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func onRuleMatchedDebug() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<h1>Hello, world!</h1>"
            }
            Route("/script.html", headerFields: ["Content-Type": "text/html"]) {
                "<script type='module' src='/script.js'></script>"
            }
            Route("/script.js", headerFields: ["Content-Type": "application/javascript"]) {
                "browser.test.notifyFail('This script shouldn't load')"
            }
        }

        // FIXME: <rdar://159289161> Add checks for parentDocumentId once we support it
        let backgroundScript = """
            browser.declarativeNetRequest.onRuleMatchedDebug.addListener((info) => {
              switch (info.rule.rulesetId) {
                case 'sub_frame rules':
                  browser.test.assertEq(typeof info.request.documentId, 'string')
                  browser.test.assertEq(info.request.documentLifecycle, undefined)
                  browser.test.assertTrue(info.request.frameId > 0)
                  browser.test.assertEq(info.request.frameType, 'sub_frame')
                  // FIXME: <rdar://159231459> Initiator of a sub-frame is null; it should be src of the iframe.
                  browser.test.assertEq(info.request.initiator, undefined)
                  browser.test.assertEq(info.request.method, 'GET')
                  browser.test.assertTrue(info.request.parentFrameId > 0)
                  browser.test.assertTrue(info.request.tabId > 0)
                  browser.test.assertEq(info.request.type, 'sub_frame')
                  browser.test.assertEq(new URL(info.request.url).pathname, '/frame.html')

                  browser.test.assertEq(info.rule.ruleId, 1)
                  browser.test.assertEq(info.rule.extensionId, undefined)

                  browser.test.sendMessage('Done')
                  break
                case 'main_frame rules':
                  browser.test.assertEq(typeof info.request.documentId, 'string')
                  browser.test.assertEq(info.request.documentLifecycle, undefined)
                  browser.test.assertEq(info.request.frameId, 0)
                  browser.test.assertEq(info.request.frameType, 'outermost_frame')
                  browser.test.assertEq(info.request.initiator, 'localhost')
                  browser.test.assertEq(info.request.method, 'GET')
                  browser.test.assertEq(info.request.parentFrameId, -1)
                  browser.test.assertTrue(info.request.tabId > 0)
                  browser.test.assertEq(info.request.type, 'main_frame')
                  browser.test.assertEq(new URL(info.request.url).pathname, '/frame.html')

                  browser.test.assertEq(info.rule.ruleId, 2)
                  browser.test.assertEq(info.rule.extensionId, undefined)

                  browser.test.sendMessage('Done')
                  break
                case 'non_frame rules':
                  browser.test.assertEq(info.request.documentId, undefined)
                  browser.test.assertEq(info.request.documentLifecycle, undefined)
                  browser.test.assertEq(info.request.frameId, 0)
                  browser.test.assertEq(info.request.frameType, undefined)
                  browser.test.assertEq(info.request.initiator, 'localhost')
                  browser.test.assertEq(info.request.method, 'GET')
                  browser.test.assertEq(info.request.parentFrameId, -1)
                  browser.test.assertTrue(info.request.tabId > 0)
                  browser.test.assertEq(info.request.type, 'script')
                  browser.test.assertEq(new URL(info.request.url).pathname, '/script.js')

                  browser.test.assertEq(info.rule.ruleId, 3)
                  browser.test.assertEq(info.rule.extensionId, undefined)

                  browser.test.notifyPass()
                  break
                default:
                  browser.test.notifyFail('Received an unexpected rulesetId.')
              }
            })

            browser.test.sendMessage('Load Tab')
            """

        let subFrameRules =
            "[{\"id\":1,\"action\":{\"type\":\"block\"},\"condition\":{\"urlFilter\":\"frame\",\"resourceTypes\":[\"sub_frame\"]}}]"
        let mainFrameRules =
            "[{\"id\":2,\"action\":{\"type\":\"block\"},\"condition\":{\"urlFilter\":\"frame\",\"resourceTypes\":[\"main_frame\"]}}]"
        let nonFrameRules =
            "[{\"id\":3,\"action\":{\"type\":\"block\"},\"condition\":{\"urlFilter\":\"script.js\",\"resourceTypes\":[\"script\"]}}]"

        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest", "declarativeNetRequestFeedback"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "sub_frame rules",
                        "enabled": true,
                        "path": "sub_frame.json",
                    ],
                    [
                        "id": "main_frame rules",
                        "enabled": true,
                        "path": "main_frame.json",
                    ],
                    [
                        "id": "non_frame rules",
                        "enabled": true,
                        "path": "non_frame.json",
                    ],
                ]
            ],
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(
                manifest: manifest,
                resources: [
                    "background.js": backgroundScript,
                    "sub_frame.json": subFrameRules,
                    "main_frame.json": mainFrameRules,
                    "non_frame.json": nonFrameRules,
                ]
            )
            let context = try #require(manager.context)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequestFeedback)
            try await manager.waitForTestMessage("Load Tab")

            // Test sub_frame rules
            let requestURL = configuration.localhostAddress
            let urlRequest = URLRequest(url: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL.appending(path: "frame.html"))
            let webView = try #require(manager.defaultTab?.webView)
            webView.load(urlRequest)

            try await manager.waitForTestMessage("Done")

            // Test main_frame rules
            webView.load(URLRequest(url: configuration.localhostAddress.appending(path: "frame.html")))
            try await manager.waitForTestMessage("Done")

            // Test non_frame rules
            webView.load(URLRequest(url: configuration.localhostAddress.appending(path: "script.html")))
            try await manager.run()
        }
    }

    @Test
    func onRuleMatchedDebugSessionRules() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<h1>Hello, world!</h1>"
            }
        }

        let backgroundScript = """
            browser.declarativeNetRequest.onRuleMatchedDebug.addListener((info) => {
              browser.test.assertEq(info.rule.rulesetId, '_session')
              browser.test.assertEq(info.rule.ruleId, 1)
              browser.test.notifyPass()
            })

            let sessionRules = await browser.declarativeNetRequest.getSessionRules()
            browser.test.assertEq(sessionRules.length, 0)
            await browser.declarativeNetRequest.updateSessionRules({ addRules: [{ id: 1, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'frame' } }] })
            sessionRules = await browser.declarativeNetRequest.getSessionRules()
            browser.test.assertEq(sessionRules.length, 1)
            browser.test.sendMessage('Load Tab')
            """

        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest", "declarativeNetRequestFeedback"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequestFeedback)
            try await manager.waitForTestMessage("Load Tab")

            let requestURL = configuration.localhostAddress
            let urlRequest = URLRequest(url: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL.appending(path: "frame.html"))
            let webView = try #require(manager.defaultTab?.webView)
            webView.load(urlRequest)

            try await manager.run()
        }
    }

    @Test
    func onRuleMatchedDebugDynamicRules() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<h1>Hello, world!</h1>"
            }
        }

        let backgroundScript = """
            browser.declarativeNetRequest.onRuleMatchedDebug.addListener((info) => {
              browser.test.assertEq(info.rule.rulesetId, '_dynamic')
              browser.test.assertEq(info.rule.ruleId, 1)
              browser.test.notifyPass()
            })

            let dynamicRules = await browser.declarativeNetRequest.getDynamicRules()
            browser.test.assertEq(dynamicRules.length, 0)
            await browser.declarativeNetRequest.updateDynamicRules({ addRules: [{ id: 1, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'frame' } }] })
            dynamicRules = await browser.declarativeNetRequest.getDynamicRules()
            browser.test.assertEq(dynamicRules.length, 1)
            browser.test.sendMessage('Load Tab')
            """

        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest", "declarativeNetRequestFeedback"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequestFeedback)
            try await manager.waitForTestMessage("Load Tab")

            let requestURL = configuration.localhostAddress
            let urlRequest = URLRequest(url: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL.appending(path: "frame.html"))
            let webView = try #require(manager.defaultTab?.webView)
            webView.load(urlRequest)

            try await manager.run()
        }
    }

    @Test
    func duplicatedRuleIDs() async throws {
        let backgroundScript = """
            let enabledRulesets = await browser.declarativeNetRequest.getEnabledRulesets()
            browser.test.assertEq(enabledRulesets.length, 1, 'The static ruleset should be enabled.')

            browser.test.sendMessage('Load Tab')
            """

        let declarativeNetRequestManifest: [String: Any] = [
            "name": "Test",
            "description": "Test dNR extension",
            "version": "1",
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "duplicated_rule_id",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        let rules =
            "[ { \"id\" : 1, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"foo\" } }, { \"id\" : 1, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"bar\" } } ]"

        let manager = try loadWebExtension(
            manifest: declarativeNetRequestManifest,
            resources: ["background.js": backgroundScript, "rules.json": rules]
        )
        let context = try #require(manager.context)
        context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)

        try await manager.waitForTestMessage("Load Tab")

        let errors = context.errors
        try #require(errors.count == 1)
        try #require(
            errors.first?.localizedDescription
                == "`declarative_net_request` ruleset with id `duplicated_rule_id` duplicates the rule id `1`."
        )
    }

    @Test
    func duplicatedRuleIDsInDifferentRulesets() async throws {
        let backgroundScript = """
            let enabledRulesets = await browser.declarativeNetRequest.getEnabledRulesets()
            browser.test.assertEq(enabledRulesets.length, 2, 'The static rulesets should be enabled.')

            browser.test.sendMessage('Load Tab')
            """

        let declarativeNetRequestManifest: [String: Any] = [
            "name": "Test",
            "description": "Test dNR extension",
            "version": "1",
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "ruleset_1",
                        "enabled": true,
                        "path": "rules1.json",
                    ],
                    [
                        "id": "ruleset_2",
                        "enabled": true,
                        "path": "rules2.json",
                    ],
                ]
            ],
        ]

        let rules1 =
            "[ { \"id\" : 1, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"foo\" } } ]"
        let rules2 =
            "[ { \"id\" : 1, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"bar\" } } ]"

        let manager = try loadWebExtension(
            manifest: declarativeNetRequestManifest,
            resources: ["background.js": backgroundScript, "rules1.json": rules1, "rules2.json": rules2]
        )
        let context = try #require(manager.context)
        context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)

        try await manager.waitForTestMessage("Load Tab")
        try #require(context.errors.count == 0)
    }

    @Test
    func onRuleMatchedDebugExcludedRequestDomains() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<h1>Hello, world!</h1>"
            }
        }

        let backgroundScript = """
            browser.declarativeNetRequest.onRuleMatchedDebug.addListener((info) => {
              browser.test.notifyFail('onRuleMatchedDebug should not be called for an excluded request domain.')
            })

            browser.test.sendMessage('Load Tab')
            """

        let rules =
            "[{\"id\":1,\"priority\":1,\"action\":{\"type\":\"block\"},\"condition\":{\"urlFilter\":\"frame\",\"excludedRequestDomains\":[\"localhost\"],\"resourceTypes\":[\"sub_frame\"]}}]"
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest", "declarativeNetRequestFeedback"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "rules",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript, "rules.json": rules])
            let context = try #require(manager.context)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequestFeedback)
            try await manager.waitForTestMessage("Load Tab")

            let requestURL = configuration.localhostAddress
            let urlRequest = URLRequest(url: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL.appending(path: "frame.html"))
            let webView = try #require(manager.defaultTab?.webView)
            webView.load(urlRequest)
            try await webView._test_waitForDidFinishNavigation()
            manager.done()
            try manager.checkCollectedFailures()
        }
    }

    @Test
    func onRuleMatchedDebugExcludedRequestMethods() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<h1>Hello, world!</h1>"
            }
        }

        let backgroundScript = """
            browser.declarativeNetRequest.onRuleMatchedDebug.addListener((info) => {
              browser.test.notifyFail('onRuleMatchedDebug should not be called for an excluded request method.')
            })

            browser.test.sendMessage('Load Tab')
            """

        let rules =
            "[{\"id\":1,\"priority\":1,\"action\":{\"type\":\"block\"},\"condition\":{\"urlFilter\":\"frame\",\"excludedRequestMethods\":[\"get\"],\"resourceTypes\":[\"sub_frame\"]}}]"
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest", "declarativeNetRequestFeedback"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "rules",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript, "rules.json": rules])
            let context = try #require(manager.context)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequestFeedback)
            try await manager.waitForTestMessage("Load Tab")

            let requestURL = configuration.localhostAddress
            let urlRequest = URLRequest(url: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL.appending(path: "frame.html"))
            let webView = try #require(manager.defaultTab?.webView)
            webView.load(urlRequest)
            try await webView._test_waitForDidFinishNavigation()
            manager.done()
            try manager.checkCollectedFailures()
        }
    }

    @Test
    func onRuleMatchedDebugExcludedRequestDomainsAndExcludedRequestMethods() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<h1>Hello, world!</h1>"
            }
        }

        let backgroundScript = """
            browser.declarativeNetRequest.onRuleMatchedDebug.addListener((info) => {
              browser.test.notifyFail('onRuleMatchedDebug should not be called for an excluded request domain or an excluded request method.')
            })

            browser.test.sendMessage('Load Tab')
            """

        let rules =
            "[{\"id\":1,\"priority\":1,\"action\":{\"type\":\"block\"},\"condition\":{\"urlFilter\":\"frame\",\"excludedRequestDomains\":[\"localhost\"],\"excludedRequestMethods\":[\"get\"],\"resourceTypes\":[\"sub_frame\"]}}]"
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest", "declarativeNetRequestFeedback"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "rules",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript, "rules.json": rules])
            let context = try #require(manager.context)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequestFeedback)
            try await manager.waitForTestMessage("Load Tab")

            let requestURL = configuration.localhostAddress
            let urlRequest = URLRequest(url: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL.appending(path: "frame.html"))
            let webView = try #require(manager.defaultTab?.webView)
            webView.load(urlRequest)
            try await webView._test_waitForDidFinishNavigation()
            manager.done()
            try manager.checkCollectedFailures()
        }
    }

    @Test
    func onRuleMatchedDebugRequestDomains() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<h1>Hello, world!</h1>"
            }
        }

        let backgroundScript = """
            browser.declarativeNetRequest.onRuleMatchedDebug.addListener((info) => {
              browser.test.assertEq(info.rule.rulesetId, 'rules');
              browser.test.assertEq(info.rule.ruleId, 1);
              browser.test.assertEq(info.request.type, 'sub_frame');
              browser.test.notifyPass();
            })

            browser.test.sendMessage('Load Tab');
            """

        let rules =
            "[{\"id\":1,\"priority\":1,\"action\":{\"type\":\"block\"},\"condition\":{\"urlFilter\":\"frame\",\"requestDomains\":[\"localhost\"],\"resourceTypes\":[\"sub_frame\"]}}]"
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest", "declarativeNetRequestFeedback"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "rules",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript, "rules.json": rules])
            let context = try #require(manager.context)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequestFeedback)
            try await manager.waitForTestMessage("Load Tab")

            let requestURL = configuration.localhostAddress
            let urlRequest = URLRequest(url: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL.appending(path: "frame.html"))
            let webView = try #require(manager.defaultTab?.webView)
            webView.load(urlRequest)
            try await manager.run()
        }
    }

    @Test
    func onRuleMatchedDebugRequestMethods() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<h1>Hello, world!</h1>"
            }
        }

        let backgroundScript = """
            browser.declarativeNetRequest.onRuleMatchedDebug.addListener((info) => {
              browser.test.assertEq(info.rule.rulesetId, 'rules');
              browser.test.assertEq(info.rule.ruleId, 1);
              browser.test.assertEq(info.request.type, 'sub_frame');
              browser.test.assertEq(info.request.method, 'GET');
              browser.test.notifyPass();
            })

            browser.test.sendMessage('Load Tab');
            """

        let rules =
            "[{\"id\":1,\"priority\":1,\"action\":{\"type\":\"block\"},\"condition\":{\"urlFilter\":\"frame\",\"requestMethods\":[\"get\"],\"resourceTypes\":[\"sub_frame\"]}}]"
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest", "declarativeNetRequestFeedback"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "rules",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript, "rules.json": rules])
            let context = try #require(manager.context)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequestFeedback)
            try await manager.waitForTestMessage("Load Tab")

            let requestURL = configuration.localhostAddress
            let urlRequest = URLRequest(url: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL.appending(path: "frame.html"))
            let webView = try #require(manager.defaultTab?.webView)
            webView.load(urlRequest)
            try await manager.run()
        }
    }

    @Test
    func onRuleMatchedDebugRequestDomainsAndRequestMethods() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<h1>Hello, world!</h1>"
            }
        }

        let backgroundScript = """
            browser.declarativeNetRequest.onRuleMatchedDebug.addListener((info) => {
              browser.test.assertEq(info.rule.rulesetId, 'rules');
              browser.test.assertEq(info.rule.ruleId, 1);
              browser.test.assertEq(info.request.type, 'sub_frame');
              browser.test.assertEq(info.request.method, 'GET');
              browser.test.notifyPass();
            })
            browser.test.sendMessage('Load Tab');
            """

        let rules =
            "[{\"id\":1,\"priority\":1,\"action\":{\"type\":\"block\"},\"condition\":{\"urlFilter\":\"frame\",\"requestDomains\":[\"localhost\"],\"requestMethods\":[\"get\"],\"resourceTypes\":[\"sub_frame\"]}}]"
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest", "declarativeNetRequestFeedback"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "rules",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript, "rules.json": rules])
            let context = try #require(manager.context)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequestFeedback)
            try await manager.waitForTestMessage("Load Tab")

            let requestURL = configuration.localhostAddress
            let urlRequest = URLRequest(url: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL.appending(path: "frame.html"))
            let webView = try #require(manager.defaultTab?.webView)
            webView.load(urlRequest)
            try await manager.run()
        }
    }

    @Test
    func onRuleMatchedDebugRequestInitiatorDomainsAndExcludedInitiatorDomains() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<h1>Hello, world!</h1>"
            }
        }

        let backgroundScript = """
            browser.declarativeNetRequest.onRuleMatchedDebug.addListener((info) => {
              browser.test.notifyFail('onRuleMatchedDebug should not be called for an excluded initiator domain.')
            })

            browser.test.sendMessage('Load Tab')
            """

        let rules =
            "[{\"id\":1,\"priority\":1,\"action\":{\"type\":\"block\"},\"condition\":{\"urlFilter\":\"frame\",\"initiatorDomains\":[\"example.com\"],\"excludedInitiatorDomains\":[\"localhost\"],\"resourceTypes\":[\"sub_frame\"]}}]"
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest", "declarativeNetRequestFeedback"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "rules",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript, "rules.json": rules])
            let context = try #require(manager.context)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequestFeedback)
            try await manager.waitForTestMessage("Load Tab")

            let requestURL = configuration.localhostAddress
            let urlRequest = URLRequest(url: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL.appending(path: "frame.html"))
            let webView = try #require(manager.defaultTab?.webView)
            webView.load(urlRequest)
            try await webView._test_waitForDidFinishNavigation()
            manager.done()
            try manager.checkCollectedFailures()
        }
    }

    @Test
    func onRuleMatchedDebugRequestUpgradeScheme() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<h1>Hello, world!</h1>"
            }
        }

        let backgroundScript = """
            var onRuleMatchedDebugCount = 0

            browser.declarativeNetRequest.onRuleMatchedDebug.addListener((info) => {
              onRuleMatchedDebugCount++
              browser.test.assertEq(onRuleMatchedDebugCount, 1, 'onRuleMatchedDebug should only be called once an upgrade action type.')
            })

            browser.test.sendMessage('Load Tab')
            """

        let rules =
            "[{\"id\":1,\"priority\":1,\"action\":{\"type\":\"upgradeScheme\"},\"condition\":{\"urlFilter\":\"frame\",\"resourceTypes\":[\"sub_frame\"]}}]"
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest", "declarativeNetRequestFeedback"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "rules",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript, "rules.json": rules])
            let context = try #require(manager.context)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequestFeedback)
            try await manager.waitForTestMessage("Load Tab")

            let requestURL = configuration.localhostAddress
            let urlRequest = URLRequest(url: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL.appending(path: "frame.html"))
            let webView = try #require(manager.defaultTab?.webView)
            webView.load(urlRequest)
            try await webView._test_waitForDidFinishNavigation()
            manager.done()
            try manager.checkCollectedFailures()
        }
    }
    #endif

    // MARK: Rule translation tests

    /// Adds the keys a converted rule carries for `onRuleMatchedDebug`, when that is enabled.
    private func withDebugIdentifiers(_ rule: [String: Any], ruleID: Int = 1) -> [String: Any] {
        #if ENABLE_DNR_ON_RULE_MATCHED_DEBUG
        rule.merging(["_identifier": ruleID, "_rulesetIdentifier": "Test Ruleset"]) { $1 }
        #else
        rule
        #endif
    }

    /// The resource types in a converted rule's trigger, which the converter does not keep in a stable order.
    private func resourceTypes(of rule: [String: Any]) -> Set<String> {
        Set((rule["trigger"] as? [String: Any])?["resource-type"] as? [String] ?? [])
    }

    /// A converted rule without the resource types in its trigger.
    private func removingResourceTypes(from rule: [String: Any]) -> [String: Any] {
        guard var trigger = rule["trigger"] as? [String: Any] else {
            return rule
        }

        trigger["resource-type"] = nil

        var rule = rule
        rule["trigger"] = trigger
        return rule
    }

    /// The type of a translated rule's action.
    private func actionType(of rule: [String: Any]) -> String? {
        (rule["action"] as? [String: Any])?["type"] as? String
    }

    @Test
    func requiredAndOptionalKeys() {
        let rule1: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "urlFilter": "crouton.net",
                "resourceTypes": ["script"],
            ],
        ]

        let validatedRule1 = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule1, rulesetID: "Test Ruleset", errorString: nil)
        #expect(validatedRule1 != nil)

        let rule2: [String: Any] = [
            "action": ["type": "block"],
            "condition": [
                "urlFilter": "crouton.net",
                "resourceTypes": ["script"],
            ],
        ]
        let validatedRule2 = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule2, rulesetID: "Test Ruleset", errorString: nil)
        #expect(validatedRule2 == nil)

        let rule3: [String: Any] = [
            "id": 1,
            "condition": [
                "urlFilter": "crouton.net",
                "resourceTypes": ["script"],
            ],
        ]
        let validatedRule3 = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule3, rulesetID: "Test Ruleset", errorString: nil)
        #expect(validatedRule3 == nil)

        let rule4: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
        ]
        let validatedRule4 = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule4, rulesetID: "Test Ruleset", errorString: nil)
        #expect(validatedRule4 == nil)
    }

    @Test
    func propertiesHaveCorrectType() {
        let rule1: [String: Any] = [
            "id": "rule_id",
            "priority": 1,
            "action": ["type": "block"],
            "condition": [
                "urlFilter": "crouton.net",
                "resourceTypes": ["script"],
            ],
        ]

        let validatedRule1 = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule1, rulesetID: "Test Ruleset", errorString: nil)
        #expect(validatedRule1 == nil)

        let rule2: [String: Any] = [
            "id": 1,
            "priority": 1,
            "action": "block",
            "condition": [
                "urlFilter": "crouton.net",
                "resourceTypes": ["script"],
            ],
        ]
        let validatedRule2 = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule2, rulesetID: "Test Ruleset", errorString: nil)
        #expect(validatedRule2 == nil)
    }

    @Test
    func numbersArePositiveIntegers() throws {
        let rule1: [String: Any] = [
            "id": -1,
            "priority": 1,
            "action": ["type": "block"],
            "condition": [
                "urlFilter": "crouton.net",
                "resourceTypes": ["script"],
            ],
        ]

        let validatedRule1 = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule1, rulesetID: "Test Ruleset", errorString: nil)
        #expect(validatedRule1 == nil)

        let rule2: [String: Any] = [
            "id": 1,
            "priority": 0,
            "action": ["type": "block"],
            "condition": [
                "urlFilter": "crouton.net",
                "resourceTypes": ["script"],
            ],
        ]

        let validatedRule2 = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule2, rulesetID: "Test Ruleset", errorString: nil)
        #expect(validatedRule2 == nil)

        let ruleWithNonIntegerPriority: [String: Any] = [
            "id": 80,
            "priority": 5.8,
            "action": ["type": "block"],
            "condition": [
                "urlFilter": "crouton.net",
                "resourceTypes": ["script"],
            ],
        ]

        let validatedRuleWithNonIntegerPriority = try #require(
            unsafe _WKWebExtensionDeclarativeNetRequestRule(
                dictionary: ruleWithNonIntegerPriority,
                rulesetID: "Test Ruleset",
                errorString: nil
            )
        )
        #expect(validatedRuleWithNonIntegerPriority.ruleID == 80)
        #expect(validatedRuleWithNonIntegerPriority.priority == 5)
    }

    @Test
    func onlyOneOfResourceTypesAndExcludedResourceTypesIsSpecified() {
        let rule: [String: Any] = [
            "id": 1,
            "priority": 1,
            "action": ["type": "block"],
            "condition": [
                "urlFilter": "boatnerd.com",
                "resourceTypes": ["font"],
                "excludedResourceTypes": ["script"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        #expect(validatedRule == nil)
    }

    @Test
    func regexRuleConversion() throws {
        let rule: [String: Any] = [
            "id": 1,
            "priority": 1,
            "action": ["type": "block"],
            "condition": [
                "regexFilter": ".*\\.com",
                "resourceTypes": ["script"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRule = try #require(validatedRule?.ruleInWebKitFormat.first)

        let correctRuleConversion = withDebugIdentifiers([
            "action": [
                "type": "block"
            ],
            "trigger": [
                "url-filter": ".*\\.com",
                "resource-type": ["script"],
            ],
        ])

        #expect(convertedRule as NSDictionary == correctRuleConversion as NSDictionary)
    }

    @Test
    func basicValidRuleParsing() throws {
        let conditionDictionary: [String: Any] = [
            "urlFilter": "crouton.net",
            "resourceTypes": ["script"],
        ]

        let rule: [String: Any] = [
            "id": 1,
            "priority": 3,
            "action": ["type": "block"],
            "condition": conditionDictionary,
        ]

        let validatedRule = try #require(
            unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        )
        #expect(validatedRule.ruleID == 1)
        #expect(validatedRule.priority == 3)
        #expect(validatedRule.action as NSDictionary == ["type": "block"] as NSDictionary)
        #expect(validatedRule.condition as NSDictionary == conditionDictionary as NSDictionary)
    }

    @Test
    func basicRuleConversion() throws {
        let rule: [String: Any] = [
            "id": 1,
            "priority": 1,
            "action": ["type": "block"],
            "condition": [
                "urlFilter": "crouton.net",
                "resourceTypes": ["font"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRule = try #require(validatedRule?.ruleInWebKitFormat.first)

        let correctRuleConversion = withDebugIdentifiers([
            "action": [
                "type": "block"
            ],
            "trigger": [
                "url-filter": "crouton\\.net",
                "resource-type": ["font"],
            ],
        ])

        #expect(convertedRule as NSDictionary == correctRuleConversion as NSDictionary)
    }

    @Test
    func mainFrameResourceRuleConversion() throws {
        let rule: [String: Any] = [
            "id": 1,
            "priority": 1,
            "action": ["type": "block"],
            "condition": [
                "urlFilter": "crouton.net",
                "resourceTypes": ["main_frame"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRule = try #require(validatedRule?.ruleInWebKitFormat.first)

        let correctRuleConversion = withDebugIdentifiers([
            "action": [
                "type": "block"
            ],
            "trigger": [
                "url-filter": "crouton\\.net",
                "resource-type": ["top-document"],
            ],
        ])

        #expect(convertedRule as NSDictionary == correctRuleConversion as NSDictionary)
    }

    @Test
    func subFrameResourceRuleConversion() throws {
        let rule: [String: Any] = [
            "id": 1,
            "priority": 1,
            "action": ["type": "allow"],
            "condition": [
                "urlFilter": "crouton.net",
                "resourceTypes": ["sub_frame"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRule = try #require(validatedRule?.ruleInWebKitFormat.first)

        let correctRuleConversion = withDebugIdentifiers([
            "action": [
                "type": "ignore-following-rules"
            ],
            "trigger": [
                "url-filter": "crouton\\.net",
                "resource-type": ["child-document"],
            ],
        ])

        #expect(convertedRule as NSDictionary == correctRuleConversion as NSDictionary)
    }

    @Test
    func repeatedMainFrameResourceRuleConversion() throws {
        let rule: [String: Any] = [
            "id": 1,
            "priority": 1,
            "action": ["type": "allow"],
            "condition": [
                "urlFilter": "crouton.net",
                "resourceTypes": ["main_frame", "main_frame"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRule = try #require(validatedRule?.ruleInWebKitFormat.first)

        let correctRuleConversion = withDebugIdentifiers([
            "action": [
                "type": "ignore-following-rules"
            ],
            "trigger": [
                "url-filter": "crouton\\.net",
                "resource-type": ["top-document"],
            ],
        ])

        #expect(convertedRule as NSDictionary == correctRuleConversion as NSDictionary)
    }

    @Test
    func caseSensitiveConversion() throws {
        let rule1: [String: Any] = [
            "id": 1,
            "priority": 1,
            "action": ["type": "block"],
            "condition": [
                "urlFilter": "crouton.net",
                "isUrlFilterCaseSensitive": true,
                "resourceTypes": ["font"],
            ],
        ]

        let validatedRule1 = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule1, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRule1 = try #require(validatedRule1?.ruleInWebKitFormat.first)

        let correctRuleConversion1 = withDebugIdentifiers([
            "action": [
                "type": "block"
            ],
            "trigger": [
                "url-filter": "crouton\\.net",
                "url-filter-is-case-sensitive": true,
                "resource-type": ["font"],
            ],
        ])
        #expect(convertedRule1 as NSDictionary == correctRuleConversion1 as NSDictionary)

        let rule2: [String: Any] = [
            "id": 1,
            "priority": 1,
            "action": ["type": "block"],
            "condition": [
                "urlFilter": "crouton.net",
                "isUrlFilterCaseSensitive": false,
                "resourceTypes": ["font"],
            ],
        ]

        let validatedRule2 = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule2, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRule2 = try #require(validatedRule2?.ruleInWebKitFormat.first)

        let correctRuleConversion2 = withDebugIdentifiers([
            "action": [
                "type": "block"
            ],
            "trigger": [
                "url-filter": "crouton\\.net",
                "resource-type": ["font"],
            ],
        ])
        #expect(convertedRule2 as NSDictionary == correctRuleConversion2 as NSDictionary)
    }

    @Test
    func convertingMultipleResourceTypes() throws {
        let rule: [String: Any] = [
            "id": 1,
            "priority": 1,
            "action": ["type": "block"],
            "condition": [
                "urlFilter": "crouton.net",
                "resourceTypes": ["script", "stylesheet", "ping"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRule = try #require(validatedRule?.ruleInWebKitFormat.first)

        let correctRuleConversion = withDebugIdentifiers([
            "action": [
                "type": "block"
            ],
            "trigger": [
                "url-filter": "crouton\\.net",
                "resource-type": ["script", "style-sheet", "ping"],
            ],
        ])
        #expect(resourceTypes(of: convertedRule) == resourceTypes(of: correctRuleConversion))
        #expect(
            removingResourceTypes(from: convertedRule) as NSDictionary == removingResourceTypes(from: correctRuleConversion) as NSDictionary
        )
    }

    @Test
    func convertingXHRWebSocketAndOtherTypes() throws {
        let rule: [String: Any] = [
            "id": 1,
            "priority": 1,
            "action": ["type": "block"],
            "condition": [
                "urlFilter": "crouton.net",
                "resourceTypes": ["xmlhttprequest", "websocket", "other"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRule = try #require(validatedRule?.ruleInWebKitFormat.first)

        let correctRuleConversion = withDebugIdentifiers([
            "action": [
                "type": "block"
            ],
            "trigger": [
                "url-filter": "crouton\\.net",
                "resource-type": ["fetch", "websocket", "other"],
            ],
        ])
        #expect(resourceTypes(of: convertedRule) == resourceTypes(of: correctRuleConversion))
        #expect(
            removingResourceTypes(from: convertedRule) as NSDictionary == removingResourceTypes(from: correctRuleConversion) as NSDictionary
        )
    }

    @Test
    func upgradeSchemeRuleConversion() throws {
        let rule: [String: Any] = [
            "id": 1,
            "priority": 1,
            "action": ["type": "upgradeScheme"],
            "condition": [
                "urlFilter": "crouton.net",
                "resourceTypes": ["image"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRules = try #require(validatedRule?.ruleInWebKitFormat)
        #expect(convertedRules.count == 2)

        let makeHTTPSRule = withDebugIdentifiers([
            "action": [
                "type": "make-https"
            ],
            "trigger": [
                "url-filter": "crouton\\.net",
                "resource-type": ["image"],
            ],
        ])

        let sortingRule = withDebugIdentifiers([
            "action": [
                "type": "ignore-following-rules"
            ],
            "trigger": [
                "url-filter": "crouton\\.net",
                "resource-type": ["image"],
            ],
        ])

        #expect(convertedRules[0] as NSDictionary == makeHTTPSRule as NSDictionary)
        #expect(convertedRules[1] as NSDictionary == sortingRule as NSDictionary)
    }

    @Test
    func upgradeSchemeForMainFrameRuleConversion() throws {
        let rule: [String: Any] = [
            "id": 1,
            "priority": 1,
            "action": ["type": "upgradeScheme"],
            "condition": [
                "urlFilter": "crouton.net",
                "resourceTypes": ["main_frame"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRules = try #require(validatedRule?.ruleInWebKitFormat)
        #expect(convertedRules.count == 2)

        let makeHTTPSRule = withDebugIdentifiers([
            "action": [
                "type": "make-https"
            ],
            "trigger": [
                "url-filter": "crouton\\.net",
                "resource-type": ["top-document"],
            ],
        ])
        #expect(convertedRules[0] as NSDictionary == makeHTTPSRule as NSDictionary)

        let sortingRule = withDebugIdentifiers([
            "action": [
                "type": "ignore-following-rules"
            ],
            "trigger": [
                "url-filter": "crouton\\.net",
                "resource-type": ["top-document"],
            ],
        ])
        #expect(convertedRules[1] as NSDictionary == sortingRule as NSDictionary)
    }

    @Test
    func ruleWithoutAPriority() throws {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "upgradeScheme"],
            "condition": [
                "urlFilter": "crouton.net",
                "resourceTypes": ["script"],
            ],
        ]

        let validatedRule = try #require(
            unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        )
        #expect(validatedRule.priority == 1)
    }

    @Test
    func ruleWithInvalidDomainType() {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "allow"],
            "condition": [
                "urlFilter": "crouton.net",
                "domainType": "secondParty",
                "resourceTypes": ["ping"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        #expect(validatedRule == nil)
    }

    @Test
    func ruleConversionWithDomainType() throws {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "urlFilter": "boatnerd.com",
                "resourceTypes": ["script"],
                "domainType": "firstParty",
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRule = try #require(validatedRule?.ruleInWebKitFormat.first)

        let correctRuleConversion = withDebugIdentifiers([
            "action": [
                "type": "block"
            ],
            "trigger": [
                "url-filter": "boatnerd\\.com",
                "resource-type": ["script"],
                "load-type": ["first-party"],
            ],
        ])

        #expect(convertedRule as NSDictionary == correctRuleConversion as NSDictionary)
    }

    @Test
    func ruleConversionWithNoSpecifiedResourceTypes() throws {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "allow"],
            "condition": [
                "regexFilter": ".*"
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRule = try #require(validatedRule?.ruleInWebKitFormat.first)

        let correctRuleConversion = withDebugIdentifiers([
            "action": [
                "type": "ignore-following-rules"
            ],
            "trigger": [
                "url-filter": ".*",
                "resource-type": [
                    "fetch", "font", "image", "media", "other", "ping", "script", "style-sheet", "websocket", "child-document",
                ],
            ],
        ])
        #expect(resourceTypes(of: convertedRule) == resourceTypes(of: correctRuleConversion))
        #expect(
            removingResourceTypes(from: convertedRule) as NSDictionary == removingResourceTypes(from: correctRuleConversion) as NSDictionary
        )
    }

    @Test
    func ruleConversionWithUnsupportedResourceTypes() throws {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "urlFilter": ".*",
                "resourceTypes": ["script", "not_a_real_resource_type", "font"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRule = try #require(validatedRule?.ruleInWebKitFormat.first)

        #expect(resourceTypes(of: convertedRule) == ["script", "font"])
    }

    @Test
    func ruleConversionWithExactlyOneUnsupportedResourceType() {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "urlFilter": ".*",
                "resourceTypes": ["not_a_real_resource_type"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRule = validatedRule?.ruleInWebKitFormat.first
        #expect(convertedRule == nil)
    }

    @Test
    func emptyResourceTypes() {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "urlFilter": ".*",
                "resourceTypes": [],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        #expect(validatedRule == nil)
    }

    @Test
    func ruleConversionWithExcludedResourceTypes() throws {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "urlFilter": ".*",
                "excludedResourceTypes": ["main_frame", "sub_frame", "font"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRule = try #require(validatedRule?.ruleInWebKitFormat.first)

        #expect(resourceTypes(of: convertedRule) == ["fetch", "image", "media", "other", "ping", "script", "style-sheet", "websocket"])
    }

    @Test
    func ruleConversionWithUnsupportedExcludedResourceTypes() throws {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "urlFilter": "*",
                "excludedResourceTypes": ["image", "🦠"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRule = try #require(validatedRule?.ruleInWebKitFormat.first)

        let correctRuleConversion = withDebugIdentifiers([
            "action": [
                "type": "block"
            ],
            "trigger": [
                "url-filter": ".*",
                "resource-type": [
                    "fetch", "font", "media", "other", "ping", "script", "style-sheet", "websocket", "top-document", "child-document",
                ],
            ],
        ])
        #expect(resourceTypes(of: convertedRule) == resourceTypes(of: correctRuleConversion))
        #expect(
            removingResourceTypes(from: convertedRule) as NSDictionary == removingResourceTypes(from: correctRuleConversion) as NSDictionary
        )
    }

    @Test
    func ruleConversionWithEmptyExcludedResourceTypes() throws {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "urlFilter": "*",
                "excludedResourceTypes": [],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRule = try #require(validatedRule?.ruleInWebKitFormat.first)

        let correctRuleConversion = withDebugIdentifiers([
            "action": [
                "type": "block"
            ],
            "trigger": [
                "url-filter": ".*",
                "resource-type": [
                    "fetch", "font", "image", "media", "other", "ping", "script", "style-sheet", "websocket", "top-document",
                    "child-document",
                ],
            ],
        ])

        #expect(resourceTypes(of: convertedRule) == resourceTypes(of: correctRuleConversion))
        #expect(
            removingResourceTypes(from: convertedRule) as NSDictionary == removingResourceTypes(from: correctRuleConversion) as NSDictionary
        )
    }

    @Test
    func emptyDomains() {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "urlFilter": ".*",
                "domains": [],
                "resourceTypes": ["font"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        #expect(validatedRule == nil)
    }

    @Test
    func nonASCIIDomains() {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "urlFilter": ".*",
                "domains": ["🙃🙃🙃", "apple.com"],
                "resourceTypes": ["font"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        #expect(validatedRule == nil)
    }

    @Test
    func ruleConversionWithDomains() throws {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "domains": ["apple.com", "facebook.com", "google.com"],
                "resourceTypes": ["font"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRule = try #require(validatedRule?.ruleInWebKitFormat.first)

        let correctRuleConversion = withDebugIdentifiers([
            "action": [
                "type": "block"
            ],
            "trigger": [
                "if-domain": ["*apple.com", "*facebook.com", "*google.com"],
                "resource-type": ["font"],
                "url-filter": ".*",
            ],
        ])

        #expect(convertedRule as NSDictionary == correctRuleConversion as NSDictionary)
    }

    @Test
    func ruleConversionWithURLFilterAndRequestDomains() throws {
        func testPattern(
            _ requestDomain: String,
            _ urlFilter: String?,
            _ expectedRegexPattern: String,
            sourceLocation: SourceLocation = #_sourceLocation
        ) throws {
            var condition: [String: Any] = [
                "resourceTypes": ["script"],
                "requestDomains": [requestDomain],
            ]

            if let urlFilter {
                condition["urlFilter"] = urlFilter
            }

            let rule: [String: Any] = [
                "id": 1,
                "action": ["type": "block"],
                "condition": condition,
            ]

            let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(
                dictionary: rule,
                rulesetID: "Test Ruleset",
                errorString: nil
            )
            let convertedRule = try #require(validatedRule?.ruleInWebKitFormat.first, sourceLocation: sourceLocation)

            let correctRuleConversion = withDebugIdentifiers([
                "action": [
                    "type": "block"
                ],
                "trigger": [
                    "url-filter": expectedRegexPattern,
                    "resource-type": ["script"],
                ],
            ])

            #expect(convertedRule as NSDictionary == correctRuleConversion as NSDictionary, sourceLocation: sourceLocation)
        }

        // No URL filter
        try testPattern("apple.com", nil, "^[^:]+://+([^:/]+\\.)?apple\\.com")

        // URL filter contains the request domain
        try testPattern("com", ".com/foo/bar", "^[^:]+://+([^:/]+\\.)?\\.com/foo/bar")

        // Part of the request domain is in the URL filter
        try testPattern("foo.com", "bar-*.com", "^[^:]+://+([^:/]+\\.)?bar-.*foo\\.com")
        try testPattern("foo.com", "foo.*/bar/baz", "^[^:]+://+([^:/]+\\.)?foo\\.com.*/bar/baz")

        // || or :// prefix in the URL filter
        try testPattern("com", "||foo", "^[^:]+://+([^:/]+\\.)?foo.*com")
        try testPattern("com", "://www.", "://www\\..*com")
        try testPattern("com", "://www.*", "://www\\..*com")
        try testPattern("com", "://www./foo/bar", "://www\\..*com/foo/bar")
        try testPattern("com", "://www.*/foo/bar", "://www\\..*com/foo/bar")

        // Concatenating the request domain and URL filter
        try testPattern("com", "/foo/bar", "^[^:]+://+([^:/]+\\.)?com.*/foo/bar")
        try testPattern("com", "&foo=bar|", "^[^:]+://+([^:/]+\\.)?com.*&foo=bar$")
    }

    @Test
    func ruleConversionWithRequestDomains() throws {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                // Some extensions will have added leading * to match subdomains to work around Safari's bug in the past (rdar://113865040), make sure we still handle that case correctly.
                "requestDomains": ["apple.com", "facebook.com", "*google.com"],
                "resourceTypes": ["font"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRules = try #require(validatedRule?.ruleInWebKitFormat)
        #expect(convertedRules.count == 3)

        let appleURLFilterRuleConversion = withDebugIdentifiers([
            "action": [
                "type": "block"
            ],
            "trigger": [
                "resource-type": ["font"],
                "url-filter": "^[^:]+://+([^:/]+\\.)?apple\\.com",
            ],
        ])
        #expect(convertedRules[0] as NSDictionary == appleURLFilterRuleConversion as NSDictionary)

        let facebookURLFilterRuleConversion = withDebugIdentifiers([
            "action": [
                "type": "block"
            ],
            "trigger": [
                "resource-type": ["font"],
                "url-filter": "^[^:]+://+([^:/]+\\.)?facebook\\.com",
            ],
        ])
        #expect(convertedRules[1] as NSDictionary == facebookURLFilterRuleConversion as NSDictionary)

        let googleURLFilterRuleConversion = withDebugIdentifiers([
            "action": [
                "type": "block"
            ],
            "trigger": [
                "resource-type": ["font"],
                "url-filter": "^[^:]+://+([^:/]+\\.)?.*google\\.com",
            ],
        ])
        #expect(convertedRules[2] as NSDictionary == googleURLFilterRuleConversion as NSDictionary)
    }

    @Test
    func ruleConversionWithInitiatorDomains() throws {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "initiatorDomains": ["apple.com", "facebook.com", "google.com"],
                "resourceTypes": ["font"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRule = try #require(validatedRule?.ruleInWebKitFormat.first)

        let correctRuleConversion = withDebugIdentifiers([
            "action": [
                "type": "block"
            ],
            "trigger": [
                "if-frame-url": [
                    "^[^:]+://+([^:/]+\\.)?apple\\.com/.*", "^[^:]+://+([^:/]+\\.)?facebook\\.com/.*",
                    "^[^:]+://+([^:/]+\\.)?google\\.com/.*",
                ],
                "resource-type": ["font"],
                "url-filter": ".*",
            ],
        ])

        #expect(convertedRule as NSDictionary == correctRuleConversion as NSDictionary)
    }

    @Test
    func ruleConversionWithExcludedInitiatorDomains() throws {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "excludedInitiatorDomains": ["apple.com", "facebook.com", "google.com"],
                "resourceTypes": ["font"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRule = try #require(validatedRule?.ruleInWebKitFormat.first)

        let correctRuleConversion = withDebugIdentifiers([
            "action": [
                "type": "block"
            ],
            "trigger": [
                "unless-frame-url": [
                    "^[^:]+://+([^:/]+\\.)?apple\\.com/.*", "^[^:]+://+([^:/]+\\.)?facebook\\.com/.*",
                    "^[^:]+://+([^:/]+\\.)?google\\.com/.*",
                ],
                "resource-type": ["font"],
                "url-filter": ".*",
            ],
        ])

        #expect(convertedRule as NSDictionary == correctRuleConversion as NSDictionary)
    }

    @Test
    func ruleConversionWithExcludedInitiatorDomainsAndInitiatorDomains() throws {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "initiatorDomains": ["example.com"],
                "excludedInitiatorDomains": ["blog.example.com"],
                "resourceTypes": ["font"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRules = try #require(validatedRule?.ruleInWebKitFormat)

        let correctRuleConversion = [
            withDebugIdentifiers([
                "action": [
                    "type": "ignore-following-rules"
                ],
                "trigger": [
                    "if-frame-url": ["^[^:]+://+([^:/]+\\.)?blog\\.example\\.com/.*"],
                    "resource-type": ["font"],
                    "url-filter": ".*",
                ],
            ]),
            withDebugIdentifiers([
                "action": [
                    "type": "block"
                ],
                "trigger": [
                    "if-frame-url": ["^[^:]+://+([^:/]+\\.)?example\\.com/.*"],
                    "resource-type": ["font"],
                    "url-filter": ".*",
                ],
            ]),
        ]

        #expect(convertedRules as NSArray == correctRuleConversion as NSArray)
    }

    @Test
    func ruleConversionWithEmptyExcludedDomains() throws {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "excludedDomains": [],
                "resourceTypes": ["media"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRule = try #require(validatedRule?.ruleInWebKitFormat.first)

        let correctRuleConversion = withDebugIdentifiers([
            "action": [
                "type": "block"
            ],
            "trigger": [
                "unless-domain": [],
                "resource-type": ["media"],
                "url-filter": ".*",
            ],
        ])

        #expect(convertedRule as NSDictionary == correctRuleConversion as NSDictionary)
    }

    @Test
    func nonASCIIExcludedDomains() {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "urlFilter": ".*",
                "domains": ["🧭"],
                "resourceTypes": ["font"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        #expect(validatedRule == nil)
    }

    @Test
    func ruleConversionWithExcludedDomains() throws {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "excludedDomains": ["apple.com"],
                "resourceTypes": ["media"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRule = try #require(validatedRule?.ruleInWebKitFormat.first)

        let correctRuleConversion = withDebugIdentifiers([
            "action": [
                "type": "block"
            ],
            "trigger": [
                "unless-domain": ["*apple.com"],
                "resource-type": ["media"],
                "url-filter": ".*",
            ],
        ])

        #expect(convertedRule as NSDictionary == correctRuleConversion as NSDictionary)
    }

    @Test
    func ruleConversionWithExcludedRequestDomains() throws {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "excludedRequestDomains": ["apple.com"],
                "resourceTypes": ["media"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRules = try #require(validatedRule?.ruleInWebKitFormat)
        #expect(convertedRules.count == 2)

        let passRuleConversion = withDebugIdentifiers([
            "action": [
                "type": "ignore-following-rules"
            ],
            "trigger": [
                "resource-type": ["media"],
                "url-filter": "apple\\.com",
            ],
        ])
        #expect(convertedRules[0] as NSDictionary == passRuleConversion as NSDictionary)

        let blockRuleConversion = withDebugIdentifiers([
            "action": [
                "type": "block"
            ],
            "trigger": [
                "resource-type": ["media"],
                "url-filter": ".*",
            ],
        ])
        #expect(convertedRules[1] as NSDictionary == blockRuleConversion as NSDictionary)
    }

    @Test
    func ruleConversionWithInvalidRequestMethods() {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "requestMethods": ["bad"],
                "resourceTypes": ["main_frame"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        #expect(validatedRule == nil)
    }

    @Test
    func ruleConversionWithInvalidExcludedRequestMethods() {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "excludedRequestMethods": ["bad"],
                "resourceTypes": ["main_frame"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        #expect(validatedRule == nil)
    }

    @Test
    func ruleConversionWithRequestMethods() throws {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "requestMethods": ["get", "post"],
                "resourceTypes": ["main_frame"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRules = try #require(validatedRule?.ruleInWebKitFormat)

        let correctRuleConversion = [
            withDebugIdentifiers([
                "action": [
                    "type": "block"
                ],
                "trigger": [
                    "url-filter": ".*",
                    "resource-type": ["top-document"],
                    "request-method": "get",
                ],
            ]),
            withDebugIdentifiers([
                "action": [
                    "type": "block"
                ],
                "trigger": [
                    "url-filter": ".*",
                    "resource-type": ["top-document"],
                    "request-method": "post",
                ],
            ]),
        ]

        #expect(convertedRules as NSArray == correctRuleConversion as NSArray)
    }

    @Test
    func ruleConversionWithExcludedRequestMethods() throws {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "excludedRequestMethods": ["get", "post"],
                "resourceTypes": ["main_frame"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRules = try #require(validatedRule?.ruleInWebKitFormat)

        let correctRuleConversion = [
            withDebugIdentifiers([
                "action": [
                    "type": "ignore-following-rules"
                ],
                "trigger": [
                    "url-filter": ".*",
                    "resource-type": ["top-document"],
                    "request-method": "get",
                ],
            ]),
            withDebugIdentifiers([
                "action": [
                    "type": "ignore-following-rules"
                ],
                "trigger": [
                    "url-filter": ".*",
                    "resource-type": ["top-document"],
                    "request-method": "post",
                ],
            ]),
            withDebugIdentifiers([
                "action": [
                    "type": "block"
                ],
                "trigger": [
                    "url-filter": ".*",
                    "resource-type": ["top-document"],
                ],
            ]),
        ]

        #expect(convertedRules as NSArray == correctRuleConversion as NSArray)
    }

    @Test
    func ruleConversionWithRequestMethodsAndExcludedRequestMethodsAndRequestDomainsAndExcludedRequestDomains() throws {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "requestDomains": ["apple.com"],
                "excludedRequestDomains": ["google.com"],
                "requestMethods": ["get"],
                "excludedRequestMethods": ["post"],
                "resourceTypes": ["main_frame"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRules = try #require(validatedRule?.ruleInWebKitFormat)

        let correctRuleConversion = [
            withDebugIdentifiers([
                "action": [
                    "type": "ignore-following-rules"
                ],
                "trigger": [
                    "url-filter": "google\\.com",
                    "resource-type": ["top-document"],
                    "request-method": "post",
                ],
            ]),
            withDebugIdentifiers([
                "action": [
                    "type": "block"
                ],
                "trigger": [
                    "url-filter": "^[^:]+://+([^:/]+\\.)?apple\\.com",
                    "resource-type": ["top-document"],
                    "request-method": "get",
                ],
            ]),
        ]

        #expect(convertedRules as NSArray == correctRuleConversion as NSArray)
    }

    @Test
    func nonASCIIURLFilter() {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "urlFilter": "🔮.com",
                "resourceTypes": ["script"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        #expect(validatedRule == nil)
    }

    @Test
    func nonASCIIRegexFilter() {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "block"],
            "condition": [
                "regexFilter": "〽️.com",
                "resourceTypes": ["script"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        #expect(validatedRule == nil)
    }

    @Test
    func urlFilterSpecialCharacters() throws {
        func testPattern(_ chromePattern: String, _ expectedRegexPattern: String, sourceLocation: SourceLocation = #_sourceLocation) throws
        {
            let rule: [String: Any] = [
                "id": 1,
                "action": ["type": "block"],
                "condition": [
                    "urlFilter": chromePattern,
                    "resourceTypes": ["script"],
                ],
            ]

            let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(
                dictionary: rule,
                rulesetID: "Test Ruleset",
                errorString: nil
            )
            let convertedRule = try #require(validatedRule?.ruleInWebKitFormat.first, sourceLocation: sourceLocation)

            let correctRuleConversion = withDebugIdentifiers([
                "action": [
                    "type": "block"
                ],
                "trigger": [
                    "url-filter": expectedRegexPattern,
                    "resource-type": ["script"],
                ],
            ])

            #expect(convertedRule as NSDictionary == correctRuleConversion as NSDictionary, sourceLocation: sourceLocation)
        }

        // Testing domain anchor.
        try testPattern("||apple.com", "^[^:]+://+([^:/]+\\.)?apple\\.com")
        try testPattern("apple||.com", "apple\\|\\|\\.com")

        // Testing start and end anchors.
        try testPattern("|apple|.com|", "^apple\\|\\.com$")
        try testPattern("|apple.com", "^apple\\.com")
        try testPattern("apple.com|", "apple\\.com$")
        try testPattern("apple|com", "apple\\|com")

        // Testing wildcard.
        try testPattern("*apple.com", ".*apple\\.com")
        try testPattern("*apple*com", ".*apple.*com")
        try testPattern("apple.com*", "apple\\.com.*")

        // Testing separator character.
        try testPattern("apple^com", "apple[^a-zA-Z0-9_.%-]com")
    }

    @Test
    func unacceptableResourceTypeForAllowAllRequests() {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "allowAllRequests"],
            "condition": [
                "urlFilter": "apple.com",
                "resourceTypes": ["main_frame", "script"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        #expect(validatedRule == nil)
    }

    @Test
    func excludedResourceTypeForAllowAllRequests() {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "allowAllRequests"],
            "condition": [
                "urlFilter": "apple.com",
                "excludedResourceTypes": [
                    "sub_frame", "stylesheet", "script", "image", "font", "object", "xmlhttprequest", "ping", "csp_report", "media",
                    "websocket", "other",
                ],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        #expect(validatedRule == nil)
    }

    @Test
    func noResourceTypeForAllowAllRequests() {
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "allowAllRequests"],
            "condition": [
                "urlFilter": "apple.com"
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        #expect(validatedRule == nil)
    }

    @Test
    func ruleConversionWithMainFrameAllowAllRequests() throws {
        // FIXME: rdar://154124673 (Write tests for sub_frame allowAllRequests rules)
        let rule: [String: Any] = [
            "id": 1,
            "action": ["type": "allowAllRequests"],
            "condition": [
                "urlFilter": "apple.com",
                "resourceTypes": ["main_frame"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRule = try #require(validatedRule?.ruleInWebKitFormat.first)

        let correctRuleConversion = withDebugIdentifiers([
            "action": [
                "type": "ignore-following-rules"
            ],
            "trigger": [
                "url-filter": ".*",
                "if-top-url": ["apple\\.com"],
            ],
        ])

        #expect(convertedRule as NSDictionary == correctRuleConversion as NSDictionary)
    }

    @Test
    func ruleConversionWithRedirect() throws {
        func testRedirect(
            _ inputRedirect: [String: Any],
            _ expectedRedirect: [String: Any]?,
            sourceLocation: SourceLocation = #_sourceLocation
        ) throws {
            let rule: [String: Any] = [
                "id": 1,
                "action": [
                    "type": "redirect",
                    "redirect": inputRedirect,
                ],
                "condition": [
                    "regexFilter": ".*",
                    "resourceTypes": ["script"],
                ],
            ]

            let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(
                dictionary: rule,
                rulesetID: "Test Ruleset",
                errorString: nil
            )

            guard let expectedRedirect else {
                #expect(validatedRule?.ruleInWebKitFormat.first == nil, sourceLocation: sourceLocation)
                return
            }

            let convertedRule = try #require(validatedRule?.ruleInWebKitFormat.first, sourceLocation: sourceLocation)

            let correctRuleConversion = withDebugIdentifiers([
                "action": [
                    "type": "redirect",
                    "redirect": expectedRedirect,
                ],
                "trigger": [
                    "url-filter": ".*",
                    "resource-type": ["script"],
                ],
            ])

            #expect(convertedRule as NSDictionary == correctRuleConversion as NSDictionary, sourceLocation: sourceLocation)
        }

        try testRedirect([:], nil)

        try testRedirect(["url": ""], nil)
        try testRedirect(["url": []], nil)
        try testRedirect(["url": "ftp://example.com"], nil)
        try testRedirect(["url": "https://example.com"], ["url": "https://example.com"])

        try testRedirect(["extensionPath": ""], nil)
        try testRedirect(["extensionPath": []], nil)
        try testRedirect(["extensionPath": "foo.js"], nil)
        try testRedirect(["extensionPath": "/foo.js"], ["extension-path": "/foo.js"])

        try testRedirect(["regexSubstitution": ""], nil)
        try testRedirect(["regexSubstitution": []], nil)
        try testRedirect(["regexSubstitution": "http://example.com/\\1"], ["regex-substitution": "http://example.com/\\1"])

        try testRedirect(["transform": ""], nil)
        try testRedirect(["transform": []], nil)
        try testRedirect(["transform": [:]], nil)
        try testRedirect(
            ["transform": ["scheme": "https", "host": "new.example.com"]],
            ["transform": ["scheme": "https", "host": "new.example.com"]]
        )
        try testRedirect(["transform": ["username": "foo", "password": "bar"]], ["transform": ["username": "foo", "password": "bar"]])
        try testRedirect(["transform": ["query": "foo"]], ["transform": ["query": "foo"]])

        try testRedirect(["transform": ["queryTransform": ""]], nil)
        try testRedirect(["transform": ["queryTransform": []]], nil)
        try testRedirect(["transform": ["queryTransform": [:]]], nil)
        try testRedirect(["transform": ["queryTransform": ["addOrReplaceParams": ""]]], nil)
        try testRedirect(["transform": ["queryTransform": ["addOrReplaceParams": []]]], nil)
        try testRedirect(["transform": ["queryTransform": ["addOrReplaceParams": [[:]]]]], nil)
        try testRedirect(["transform": ["queryTransform": ["addOrReplaceParams": [["key": "foo"]]]]], nil)
        try testRedirect(["transform": ["queryTransform": ["addOrReplaceParams": [["replaceOnly": ""]]]]], nil)
        try testRedirect(["transform": ["queryTransform": ["removeParams": ""]]], nil)
        try testRedirect(["transform": ["queryTransform": ["removeParams": []]]], nil)
        try testRedirect(
            ["transform": ["queryTransform": ["addOrReplaceParams": [["key": "foo", "value": "bar"]]]]],
            ["transform": ["query-transform": ["add-or-replace-parameters": [["key": "foo", "value": "bar"]]]]]
        )
        try testRedirect(
            ["transform": ["queryTransform": ["addOrReplaceParams": [["key": "foo", "value": "bar", "replaceOnly": true]]]]],
            ["transform": ["query-transform": ["add-or-replace-parameters": [["key": "foo", "value": "bar", "replace-only": true]]]]]
        )
        try testRedirect(
            ["transform": ["queryTransform": ["removeParams": ["foo"]]]],
            ["transform": ["query-transform": ["remove-parameters": ["foo"]]]]
        )
    }

    @Test
    func ruleConversionWithModifyHeaders() throws {
        func testModifyHeaders(
            _ inputHeaderType: String,
            _ inputModifyHeadersInfo: [[String: Any]],
            _ expectedHeaderType: String?,
            _ expectedModifyHeadersInfo: [[String: Any]]?,
            sourceLocation: SourceLocation = #_sourceLocation
        ) throws {
            let rule: [String: Any] = [
                "id": 10,
                "priority": 2,
                "action": [
                    "type": "modifyHeaders",
                    inputHeaderType: inputModifyHeadersInfo,
                ],
                "condition": [
                    "urlFilter": "*",
                    "resourceTypes": ["script"],
                ],
            ]

            let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(
                dictionary: rule,
                rulesetID: "Test Ruleset",
                errorString: nil
            )

            guard let expectedHeaderType, let expectedModifyHeadersInfo else {
                #expect(validatedRule?.ruleInWebKitFormat.first == nil, sourceLocation: sourceLocation)
                return
            }

            let convertedRule = try #require(validatedRule?.ruleInWebKitFormat.first, sourceLocation: sourceLocation)

            let correctRuleConversion = withDebugIdentifiers(
                [
                    "action": [
                        "type": "modify-headers",
                        "priority": 2,
                        expectedHeaderType: expectedModifyHeadersInfo,
                    ],
                    "trigger": [
                        "url-filter": ".*",
                        "resource-type": ["script"],
                    ],
                ],
                ruleID: 10
            )

            #expect(convertedRule as NSDictionary == correctRuleConversion as NSDictionary, sourceLocation: sourceLocation)
        }

        try testModifyHeaders("responseHeaders", [], nil, nil)
        try testModifyHeaders("responseHeaders", [[:]], nil, nil)
        try testModifyHeaders("requestHeaders", [[:]], nil, nil)
        try testModifyHeaders("responseHeaders", [["wrong dictionary structure": [[:]]]], nil, nil)
        try testModifyHeaders("responseHeaders", [["header": "accept", "operation": "invalid header operation", "value": "v1"]], nil, nil)
        try testModifyHeaders("responseHeaders", [["operation": "set", "value": "v1"]], nil, nil)
        try testModifyHeaders("responseHeaders", [["header": "accept", "operation": "set"]], nil, nil)
        try testModifyHeaders("responseHeaders", [["header": "accept", "operation": "append"]], nil, nil)
        try testModifyHeaders("responseHeaders", [["header": "accept", "operation": "remove", "value": "v1"]], nil, nil)
        try testModifyHeaders("requestHeaders", [["operation": "set", "value": "v1"]], nil, nil)
        try testModifyHeaders("requestHeaders", [["header": "ellie's custom header", "operation": "set", "value": "v1"]], nil, nil)

        try testModifyHeaders(
            "requestHeaders",
            [["header": "accept", "operation": "set", "value": "v1"]],
            "request-headers",
            [["header": "accept", "operation": "set", "value": "v1"]]
        )
        try testModifyHeaders(
            "requestHeaders",
            [["header": "accept", "operation": "remove"]],
            "request-headers",
            [["header": "accept", "operation": "remove"]]
        )
        try testModifyHeaders(
            "requestHeaders",
            [["header": "accept", "operation": "append", "value": "v1"]],
            "request-headers",
            [["header": "accept", "operation": "append", "value": "v1"]]
        )
        try testModifyHeaders(
            "requestHeaders",
            [["header": "accept", "operation": "append", "value": "v1"], ["header": "accept", "operation": "set", "value": "v1"]],
            "request-headers",
            [["header": "accept", "operation": "append", "value": "v1"], ["header": "accept", "operation": "set", "value": "v1"]]
        )
        try testModifyHeaders(
            "responseHeaders",
            [["header": "accept", "operation": "set", "value": "v1"]],
            "response-headers",
            [["header": "accept", "operation": "set", "value": "v1"]]
        )
    }

    @Test
    func ruleConversionWithModifyHeadersWithBothResponseAndRequestHeaders() throws {
        let rule: [String: Any] = [
            "id": 10,
            "action": [
                "type": "modifyHeaders",
                "responseHeaders": [
                    ["header": "age", "operation": "set", "value": "5"],
                    ["header": "date", "operation": "remove"],
                ],
                "requestHeaders": [
                    ["header": "cache-control", "operation": "set", "value": "something"],
                    ["header": "Cookie", "operation": "remove"],
                ],
            ],
            "condition": [
                "urlFilter": "*",
                "resourceTypes": ["script"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRule = try #require(validatedRule?.ruleInWebKitFormat.first)

        let correctRuleConversion = withDebugIdentifiers(
            [
                "action": [
                    "type": "modify-headers",
                    "priority": 1,
                    "response-headers": [
                        ["header": "age", "operation": "set", "value": "5"],
                        ["header": "date", "operation": "remove"],
                    ],
                    "request-headers": [
                        ["header": "cache-control", "operation": "set", "value": "something"],
                        ["header": "Cookie", "operation": "remove"],
                    ],
                ],
                "trigger": [
                    "url-filter": ".*",
                    "resource-type": ["script"],
                ],
            ],
            ruleID: 10
        )

        #expect(convertedRule as NSDictionary == correctRuleConversion as NSDictionary)
    }

    @Test
    func ruleConversionWithModifyHeadersWithoutAnyHeadersSpecified() {
        let rule: [String: Any] = [
            "id": 10,
            "priority": 2,
            "action": [
                "type": "modifyHeaders"
            ],
            "condition": [
                "urlFilter": "*",
                "resourceTypes": ["script"],
            ],
        ]

        let validatedRule = unsafe _WKWebExtensionDeclarativeNetRequestRule(dictionary: rule, rulesetID: "Test Ruleset", errorString: nil)
        let convertedRule = validatedRule?.ruleInWebKitFormat.first
        #expect(convertedRule == nil)
    }

    @Test
    func rulesSortByPriorityFromDifferentRulesets() {
        let rules: [String: [[String: Any]]] = [
            "Test Ruleset 1": [
                [
                    "id": 1,
                    "priority": 2,
                    "action": ["type": "allow"],
                    "condition": [
                        "regexFilter": "apple.com",
                        "resourceTypes": ["script"],
                    ],
                ]
            ],
            "Test Ruleset 2": [
                [
                    "id": 1,
                    "priority": 1,
                    "action": ["type": "block"],
                    "condition": [
                        "regexFilter": "bananas.com",
                        "resourceTypes": ["script"],
                    ],
                ]
            ],
        ]

        let sortedTranslatedRules = unsafe _WKWebExtensionDeclarativeNetRequestTranslator.translateRules(rules, errorStrings: nil)

        #expect(actionType(of: sortedTranslatedRules[0]) == "ignore-following-rules")
        #expect(actionType(of: sortedTranslatedRules[1]) == "block")
    }

    @Test
    func rulesSortWithoutExplicitPriority() {
        let rules: [String: [[String: Any]]] = [
            "Test Ruleset": [
                [
                    "id": 1,
                    "action": ["type": "upgradeScheme"],
                    "condition": [
                        "regexFilter": "bananas.com",
                        "resourceTypes": ["script"],
                    ],
                ],
                [
                    "id": 1,
                    "priority": 3,
                    "action": ["type": "block"],
                    "condition": [
                        "regexFilter": "bananas.com",
                        "resourceTypes": ["script"],
                    ],
                ],
            ]
        ]

        let sortedTranslatedRules = unsafe _WKWebExtensionDeclarativeNetRequestTranslator.translateRules(rules, errorStrings: nil)

        #expect(actionType(of: sortedTranslatedRules[0]) == "block")
        #expect(actionType(of: sortedTranslatedRules[1]) == "make-https")
        #expect(actionType(of: sortedTranslatedRules[2]) == "ignore-following-rules")
    }

    @Test
    func rulesSortByActionType() {
        let rules: [String: [[String: Any]]] = [
            "Test Ruleset": [
                [
                    "id": 1,
                    "action": ["type": "upgradeScheme"],
                    "condition": [
                        "regexFilter": "bananas.com",
                        "resourceTypes": ["script"],
                    ],
                ],
                [
                    "id": 1,
                    "action": ["type": "allowAllRequests"],
                    "condition": [
                        "regexFilter": "bananas.com",
                        "resourceTypes": ["main_frame"],
                    ],
                ],
                [
                    "id": 1,
                    "action": ["type": "allow"],
                    "condition": [
                        "regexFilter": "bananas.com",
                        "resourceTypes": ["script"],
                    ],
                ],
                [
                    "id": 1,
                    "action": ["type": "block"],
                    "condition": [
                        "regexFilter": "bananas.com",
                        "resourceTypes": ["script"],
                    ],
                ],
                [
                    "id": 1,
                    "action": [
                        "type": "modifyHeaders",
                        "requestHeaders": [
                            ["header": "DNT", "operation": "set", "value": "1"]
                        ],
                    ],
                    "condition": [
                        "regexFilter": "bananas.com",
                        "resourceTypes": ["script"],
                    ],
                ],
            ]
        ]

        let sortedTranslatedRules = unsafe _WKWebExtensionDeclarativeNetRequestTranslator.translateRules(rules, errorStrings: nil)

        #expect(actionType(of: sortedTranslatedRules[0]) == "ignore-following-rules")
        #expect(actionType(of: sortedTranslatedRules[1]) == "ignore-following-rules")
        #expect(actionType(of: sortedTranslatedRules[2]) == "block")
        #expect(actionType(of: sortedTranslatedRules[3]) == "make-https")
        #expect(actionType(of: sortedTranslatedRules[4]) == "ignore-following-rules")
        #expect(actionType(of: sortedTranslatedRules[5]) == "modify-headers")
    }

    @Test
    func removeAllContentRuleListsDoesNotRemoveWebExtensionRuleLists() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }

            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<body style='background-color: blue'></body>"
            }
        }

        let backgroundScript = """
            browser.test.sendMessage('Remove RuleLists and Load Tab')
            """

        let declarativeNetRequestManifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest"],
            "background": ["scripts": ["background.js"], "type": "module", "persistent": false],
            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "blockFrame",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        let rules =
            "[ { \"id\" : 1, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"frame\" } } ]"

        try await server.run { configuration in
            let manager = try loadWebExtension(
                manifest: declarativeNetRequestManifest,
                resources: ["background.js": backgroundScript, "rules.json": rules]
            )
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)

            try await manager.waitForTestMessage("Remove RuleLists and Load Tab")

            let webView = try #require(manager.defaultTab?.webView)

            webView.configuration.userContentController.removeAllContentRuleLists()

            let navigationDelegate = TestNavigationDelegate()

            webView.navigationDelegate = navigationDelegate

            await navigationDelegate.nextContentRuleListAction {
                webView.load(URLRequest(url: configuration.localhostAddress))
            }
        }
    }

    @Test
    func migrateDeclarativeNetRequestDataToNewFormat() async throws {
        let backgroundScript = """
            var expectedResults = [{
              'id': 1,
              'condition': {
                  'urlFilter': 'blocksub'
              },
              'action': {
                  'type': 'block'
              }
            }]

            var results
            results = await browser.declarativeNetRequest.getDynamicRules()

            browser.test.assertDeepEq(results, expectedResults)

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript
        ]

        let declarativeNetRequestManifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["declarativeNetRequest"],
            "background": ["scripts": ["background.js"], "type": "module", "persistent": false],
        ]

        let manager = parseWebExtension(manifest: declarativeNetRequestManifest, resources: resources, configuration: ._temporary())
        let context = try #require(manager.context)

        // Give the extension a unique identifier so it opts into saving data in the temporary configuration.
        context.uniqueIdentifier = "org.webkit.test.extension (76C788B8)"

        manager.load()

        let storageDirectoryPath = try #require(manager.controller.configuration._storageDirectoryPath)
        let storageDirectory = (storageDirectoryPath as NSString).appendingPathComponent(context.uniqueIdentifier)

        let files = [
            try #require(Bundle.testResources.url(forResource: "DeclarativeNetRequestRules", withExtension: "db")),
            try #require(Bundle.testResources.url(forResource: "DeclarativeNetRequestRules", withExtension: "db-shm")),
            try #require(Bundle.testResources.url(forResource: "DeclarativeNetRequestRules", withExtension: "db-wal")),
        ]

        for file in files {
            let combinedPath = (storageDirectory as NSString).appendingPathComponent(file.lastPathComponent)
            try FileManager.default.copyItem(at: file, to: URL(fileURLWithPath: combinedPath))
        }

        try await manager.run()
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
