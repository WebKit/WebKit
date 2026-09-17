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

#if ENABLE_WK_WEB_EXTENSIONS_OFFSCREEN

import Foundation
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.WebExtensionUtilities
import Testing
import WebKit
private import WebKit_Private._WKFeature
private import WebKit_Private.WKPreferencesPrivate
private import WebKit_Private.WKWebExtensionContextPrivate

import struct Swift.String

@MainActor
struct WKWebExtensionAPIOffscreenTests {
    private let offscreenManifest: [String: Any] = [
        "manifest_version": 3,
        "name": "Offscreen Test",
        "description": "Offscreen",
        "version": "1",

        "permissions": ["offscreen"],
        "background": [
            "service_worker": "background.js",
            "type": "module",
        ],
    ]

    private let noOffscreenManifest: [String: Any] = [
        "manifest_version": 3,
        "name": "No Offscreen Test",
        "description": "No Offscreen",
        "version": "1",

        "permissions": [],
        "background": [
            "service_worker": "background.js",
            "type": "module",
        ],
    ]

    private let offscreenConfiguration: WKWebExtensionController.Configuration

    init() {
        offscreenConfiguration = .nonPersistent()

        for feature in WKPreferences._features() where feature.key == "WebExtensionOffscreenEnabled" {
            offscreenConfiguration.webViewConfiguration.preferences._setEnabled(true, for: feature)
        }
    }

    private func loadAndRun(manifest: [String: Any], resources: [String: Any]) async throws {
        let manager = try loadWebExtension(manifest: manifest, resources: resources, configuration: offscreenConfiguration)
        try await manager.run()
    }

    @Test
    func apisUnavailableWhenManifestDoesNotRequest() async throws {
        let backgroundScript = """
            browser.test.assertDeepEq(browser.offscreen, undefined)
            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: noOffscreenManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func offscreenAPIAvailableWhenManifestRequests() async throws {
        let backgroundScript = """
            browser.test.assertFalse(browser.offscreen === undefined)
            browser.test.assertFalse(browser.offscreen.createDocument === undefined)
            browser.test.assertFalse(browser.offscreen.closeDocument === undefined)
            browser.test.assertFalse(browser.offscreen.hasDocument === undefined)

            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: offscreenManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func offscreenCreateDocumentArgumentValidation() async throws {
        let backgroundScript = """
            browser.test.assertFalse(browser.offscreen === undefined)
            browser.test.assertFalse(browser.offscreen.createDocument === undefined)

            browser.test.assertThrows(() => browser.offscreen.createDocument(), /required argument is missing/)

            // Only one argument specified (all three are required).
            browser.test.assertThrows(() => browser.offscreen.createDocument({'justification': 'test'}), /missing required keys: 'url' and 'reasons'/)
            browser.test.assertThrows(() => browser.offscreen.createDocument({'url': 'test.html'}), /missing required keys: 'justification' and 'reasons'/)
            browser.test.assertThrows(() => browser.offscreen.createDocument({'reasons': ['Test']}), /missing required keys: 'url' and 'justification'/)

            // Mising 'reasons'.
            browser.test.assertThrows(() => browser.offscreen.createDocument({'url': 'test.html', 'justification': 'test'}), /missing required keys: 'reasons'/)

            // Missing 'justification'.
            browser.test.assertThrows(() => browser.offscreen.createDocument({'url': 'test.html', 'reasons': ['Test']}), /missing required keys: 'justification'/)

            // Missing 'url'.
            browser.test.assertThrows(() => browser.offscreen.createDocument({'justification': 'test', 'reasons': ['Test']}), /missing required keys: 'url'/)

            // Incorrect types for 'reasons'.
            browser.test.assertThrows(() => browser.offscreen.createDocument({'url': 'test.html', 'justification': 'test', 'reasons': 'Test'}), /'reasons' is expected to be an array of strings, but a string was provided/)
            browser.test.assertThrows(() => browser.offscreen.createDocument({'url': 'test.html', 'justification': 'test', 'reasons': 5}), /'reasons' is expected to be an array of strings, but a number was provided/)

            // Incorrect types for 'justification'.
            browser.test.assertThrows(() => browser.offscreen.createDocument({'url': 'test.html', 'justification': ['test'], 'reasons': ['Test']}), /'justification' is expected to be a string, but an array was provided/)
            browser.test.assertThrows(() => browser.offscreen.createDocument({'url': 'test.html', 'justification': 5, 'reasons': ['Test']}), /'justification' is expected to be a string, but a number was provided/)

            // Incorrect types for 'url'.
            browser.test.assertThrows(() => browser.offscreen.createDocument({'url': ['test.html'], 'justification': 'test', 'reasons': ['Test']}), /'url' is expected to be a string, but an array was provided/)
            browser.test.assertThrows(() => browser.offscreen.createDocument({'url': 5, 'justification': 'test', 'reasons': ['Test']}), /'url' is expected to be a string, but a number was provided/)

            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: offscreenManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func createAndHasDocument() async throws {
        let backgroundScript = """
            browser.offscreen.createDocument({ url: 'offscreen.html', reasons: ['TESTING'], justification: 'test' }).then(async () => {
              browser.test.assertTrue(await browser.offscreen.hasDocument())
              browser.test.notifyPass()
            })
            """

        try await loadAndRun(
            manifest: offscreenManifest,
            resources: [
                "background.js": backgroundScript,
                "offscreen.html": "<!DOCTYPE html><html></html>",
            ]
        )
    }

    @Test
    func createDocumentFails() async throws {
        let backgroundScript = """
            await browser.test.assertRejects(browser.offscreen.createDocument({ url: 'offscreen.html', reasons: ['TESTING'], justification: 'test' }), /Offscreen document was closed/)
            browser.test.assertFalse(await browser.offscreen.hasDocument())
            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: offscreenManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func createDocumentTwiceFails() async throws {
        let backgroundScript = """
            await browser.offscreen.createDocument({ url: 'offscreen.html', reasons: ['TESTING'], justification: 'test' })
            await browser.test.assertRejects(browser.offscreen.createDocument({ url: 'offscreen.html', reasons: ['TESTING'], justification: 'test' }), /Only a single offscreen document/)
            browser.test.notifyPass()
            """

        try await loadAndRun(
            manifest: offscreenManifest,
            resources: [
                "background.js": backgroundScript,
                "offscreen.html": "<!DOCTYPE html><html></html>",
            ]
        )
    }

    @Test
    func closeDocument() async throws {
        let backgroundScript = """
            await browser.offscreen.createDocument({ url: 'offscreen.html', reasons: ['TESTING'], justification: 'test' })
            browser.test.assertTrue(await browser.offscreen.hasDocument())
            await browser.offscreen.closeDocument()
            browser.test.assertFalse(await browser.offscreen.hasDocument())
            browser.test.notifyPass()
            """

        try await loadAndRun(
            manifest: offscreenManifest,
            resources: [
                "background.js": backgroundScript,
                "offscreen.html": "<!DOCTYPE html><html></html>",
            ]
        )
    }

    @Test
    func closeDocumentWithoutOneOpenFails() async throws {
        let backgroundScript = """
            await browser.test.assertRejects(browser.offscreen.closeDocument(), /No offscreen document is open/)
            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: offscreenManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func hasDocumentFalseByDefault() async throws {
        let backgroundScript = """
            browser.test.assertFalse(await browser.offscreen.hasDocument())
            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: offscreenManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func documentContentLoads() async throws {
        let backgroundScript = """
            browser.offscreen.createDocument({ url: 'offscreen.html', reasons: ['TESTING'], justification: 'test' })
            """

        let offscreenScript = """
            browser.test.assertFalse(navigator.serviceWorker.controller === undefined)
            browser.test.notifyPass()
            """

        try await loadAndRun(
            manifest: offscreenManifest,
            resources: [
                "background.js": backgroundScript,
                "offscreen.html": "<script type='module' src='offscreen.js'></script>",
                "offscreen.js": offscreenScript,
            ]
        )
    }

    @Test
    func sendMessageToDocument() async throws {
        let backgroundScript = """
            await browser.offscreen.createDocument({ url: 'offscreen.html', reasons: ['TESTING'], justification: 'test' })
            browser.runtime.sendMessage('Hello from background')
            """

        let offscreenScript = """
            browser.runtime.onMessage.addListener((message) => {
              browser.test.assertEq(message, 'Hello from background', 'Should receive the expected message from the background page')
              browser.test.notifyPass()
            })
            """

        try await loadAndRun(
            manifest: offscreenManifest,
            resources: [
                "background.js": backgroundScript,
                "offscreen.html": "<script type='module' src='offscreen.js'></script>",
                "offscreen.js": offscreenScript,
            ]
        )
    }

    @Test
    func offscreenDocumentAPIAvailability() async throws {
        let backgroundScript = """
            browser.test.assertFalse(browser.dom === undefined)
            browser.test.assertFalse(browser.extension === undefined)
            browser.test.assertFalse(browser.i18n === undefined)
            browser.test.assertFalse(browser.runtime === undefined)
            browser.test.assertFalse(browser.permissions === undefined)
            browser.test.assertFalse(browser.tabs === undefined)
            browser.test.assertFalse(browser.windows === undefined)
            browser.offscreen.createDocument({ url: 'offscreen.html', reasons: ['TESTING'], justification: 'test' })
            """

        let offscreenScript = """
            browser.test.assertFalse(browser.runtime === undefined)

            browser.test.assertTrue(browser.dom === undefined)
            browser.test.assertTrue(browser.extension === undefined)
            browser.test.assertTrue(browser.i18n === undefined)
            browser.test.assertTrue(browser.permissions === undefined)
            browser.test.assertTrue(browser.tabs === undefined)
            browser.test.assertTrue(browser.windows === undefined)
            browser.test.assertTrue(browser.offscreen === undefined)
            browser.test.notifyPass()
            """

        try await loadAndRun(
            manifest: offscreenManifest,
            resources: [
                "background.js": backgroundScript,
                "offscreen.html": "<script type='module' src='offscreen.js'></script>",
                "offscreen.js": offscreenScript,
            ]
        )
    }

    @Test
    func offscreenDocumentVisibleToClientsMatchAll() async throws {
        let backgroundScript = """
            await browser.offscreen.createDocument({ url: 'offscreen.html', reasons: ['TESTING'], justification: 'test' })

            const offscreenURL = browser.runtime.getURL('offscreen.html')

            const controlledClients = await self.clients.matchAll()
            browser.test.assertEq(controlledClients.length, 1)
            browser.test.assertTrue(controlledClients.some((client) => client.url === offscreenURL), 'The offscreen document should be a controlled window client of the background service worker')

            const allClients = await self.clients.matchAll({ includeUncontrolled: true })
            browser.test.assertEq(allClients.length, 1)
            browser.test.assertTrue(allClients.some((client) => client.url === offscreenURL), 'The offscreen document should also appear when including uncontrolled clients')

            browser.test.notifyPass()
            """

        try await loadAndRun(
            manifest: offscreenManifest,
            resources: [
                "background.js": backgroundScript,
                "offscreen.html": "<!DOCTYPE html><html></html>",
            ]
        )
    }

    @Test
    func offscreenDocumentRemovedFromClientsMatchAllAfterClose() async throws {
        let backgroundScript = """
            const offscreenURL = browser.runtime.getURL('offscreen.html')
            const isOffscreenAClient = async () => (await self.clients.matchAll()).some((client) => client.url === offscreenURL)

            await browser.offscreen.createDocument({ url: 'offscreen.html', reasons: ['TESTING'], justification: 'test' })
            browser.test.assertTrue(await isOffscreenAClient(), 'The offscreen document should be a client while it is open')

            await browser.offscreen.closeDocument()

            // After closing the document, give the client time to tear down and unregister itself as a client.
            let stillAClient = true
            for (let attempt = 0; attempt < 50 && stillAClient; ++attempt) {
              stillAClient = await isOffscreenAClient()
              if (stillAClient)
                await new Promise((resolve) => setTimeout(resolve, 50))
            }

            browser.test.assertFalse(stillAClient, 'The offscreen document should stop being a client after it is closed')
            browser.test.notifyPass()
            """

        try await loadAndRun(
            manifest: offscreenManifest,
            resources: [
                "background.js": backgroundScript,
                "offscreen.html": "<!DOCTYPE html><html></html>",
            ]
        )
    }

    @Test
    func clientsMatchAllUsedAsHasDocumentGuard() async throws {
        let backgroundScript = """
            const offscreenURL = browser.runtime.getURL('offscreen.html')
            const hasDocument = async () => (await self.clients.matchAll()).some((client) => client.url === offscreenURL)

            const ensureDocument = async () => {
              if (await hasDocument())
                return
              await browser.offscreen.createDocument({ url: 'offscreen.html', reasons: ['TESTING'], justification: 'test' })
            }

            await ensureDocument()

            browser.test.assertTrue(await browser.offscreen.hasDocument(), 'A single offscreen document should still be open')
            browser.test.notifyPass()
            """

        try await loadAndRun(
            manifest: offscreenManifest,
            resources: [
                "background.js": backgroundScript,
                "offscreen.html": "<!DOCTYPE html><html></html>",
            ]
        )
    }

    @Test
    func tabAndOffscreenDocumentVisibleToClientsMatchAll() async throws {
        let backgroundScript = """
            const offscreenURL = browser.runtime.getURL('offscreen.html')
            const tabURL = browser.runtime.getURL('tab.html')

            const tabReady = new Promise((resolve) => {
              browser.runtime.onMessage.addListener((message) => {
                if (message === 'tab ready')
                  resolve()
              })
            })

            await browser.offscreen.createDocument({ url: 'offscreen.html', reasons: ['TESTING'], justification: 'test' })
            await browser.tabs.create({ url: 'tab.html' })
            await tabReady

            const clients = await self.clients.matchAll({ includeUncontrolled: true })
            browser.test.assertEq(clients.length, 2)
            browser.test.assertTrue(clients.some((client) => client.url === offscreenURL), 'The offscreen document should be a service worker client')
            browser.test.assertTrue(clients.some((client) => client.url === tabURL), 'The tab should be a service worker client')

            browser.test.notifyPass()
            """

        let tabScript = "browser.runtime.sendMessage('tab ready')"

        try await loadAndRun(
            manifest: offscreenManifest,
            resources: [
                "background.js": backgroundScript,
                "offscreen.html": "<!DOCTYPE html><html></html>",
                "tab.html": "<script type='module' src='tab.js'></script>",
                "tab.js": tabScript,
            ]
        )
    }

    @Test
    func offscreenDocumentVisibleToClientsMatchAllAfterBackgroundContentReloads() async throws {
        let backgroundScript = """
            const offscreenURL = browser.runtime.getURL('offscreen.html')
            const isOffscreenAClient = async () => (await self.clients.matchAll()).some((client) => client.url === offscreenURL)

            if (await browser.offscreen.hasDocument()) {
              // The relaunched worker briefly isn't the registration's active worker yet while it installs/activates,
              // during which matchAll() legitimately reports no controlled clients. Poll until that settles.
              let isAClient = false
              for (let attempt = 0; attempt < 50 && !isAClient; ++attempt) {
                isAClient = await isOffscreenAClient()
                if (!isAClient)
                  await new Promise((resolve) => setTimeout(resolve, 50))
              }
              browser.test.assertTrue(isAClient, 'The offscreen document should still be a client after the background content reloads')
              browser.test.sendMessage('Offscreen Document Still A Client')
              browser.test.notifyPass()
            } else {
              await browser.offscreen.createDocument({ url: 'offscreen.html', reasons: ['TESTING'], justification: 'test' })
              browser.test.assertTrue(await isOffscreenAClient(), 'The offscreen document should be a client while it is open')
              browser.test.sendMessage('Offscreen Document Created')
            }
            """

        let manager = try loadWebExtension(
            manifest: offscreenManifest,
            resources: [
                "background.js": backgroundScript,
                "offscreen.html": "<!DOCTYPE html><html></html>",
            ],
            configuration: offscreenConfiguration
        )

        try await manager.waitForTestMessage("Offscreen Document Created")

        let context = try #require(manager.context)
        context._reloadBackgroundContentForTesting()

        try await manager.run()
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS_OFFSCREEN
