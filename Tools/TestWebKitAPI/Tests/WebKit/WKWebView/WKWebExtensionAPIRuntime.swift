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

import struct Foundation.URL
import struct Swift.String

@MainActor
struct WKWebExtensionAPIRuntimeTests {
    private let runtimeManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "Runtime Test",
        "description": "Runtime Test",
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

        "options_page": "options.html",

        "permissions": ["nativeMessaging"],
    ]

    private let runtimeContentScriptManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "Runtime Test",
        "description": "Runtime Test",
        "version": "1",

        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],

        "content_scripts": [
            [
                "js": ["content.js"],
                "matches": ["*://localhost/*"],
            ]
        ],

        "permissions": ["nativeMessaging"],
    ]

    private let runtimePopupContentScriptManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "Runtime Test",
        "description": "Runtime Test",
        "version": "1",

        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],

        "action": [
            "default_title": "Test Action",
            "default_popup": "popup.html",
        ],

        "content_scripts": [
            [
                "js": ["content.js"],
                "matches": ["*://localhost/*"],
                "all_frames": true,
            ]
        ],
    ]

    private let runtimeServiceWorkerManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "Runtime Test",
        "description": "Runtime Test",
        "version": "1",

        "background": [
            "service_worker": "background.js"
        ],
    ]

    private let externallyConnectableManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "Externally Connectable Test",
        "description": "Runtime Test",
        "version": "1",

        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],

        "externally_connectable": [
            "matches": ["*://localhost/*"]
        ],
    ]

    private func structuredCloneContentScriptManifest() -> [String: Any] {
        var manifest = runtimeContentScriptManifest
        manifest["message_serialization"] = "structured_clone"
        return manifest
    }

    private func loadAndRun(manifest: [String: Any]? = nil, backgroundScript: String) async throws {
        let manager = try loadWebExtension(manifest: manifest ?? runtimeManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    /// Serves an empty page, loads an extension with `resources`, grants the extension access to the page's `localhost`
    /// URL, loads the page in the default tab once the extension sends `Load Tab`, and runs the extension until it finishes.
    private func loadAndRunWithEmptyPage(
        manifest: [String: Any],
        resources: [String: Any],
        sourceLocation: SourceLocation = #_sourceLocation
    ) async throws {
        try await loadAndRunWithEmptyPage(manifest: manifest, sourceLocation: sourceLocation) { _ in resources }
    }

    /// Serves an empty page, loads an extension with the resources `resources` returns for the page's `localhost` URL,
    /// grants the extension access to that URL, loads the page in the default tab once the extension sends `Load Tab`,
    /// and runs the extension until it finishes.
    private func loadAndRunWithEmptyPage(
        manifest: [String: Any],
        sourceLocation: SourceLocation = #_sourceLocation,
        resources: (_ url: URL) -> [String: Any]
    ) async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        try await server.run { configuration in
            let url = configuration.localhostAddress

            let manager = try loadWebExtension(manifest: manifest, resources: resources(url))
            let context = try #require(manager.context, sourceLocation: sourceLocation)

            context.setPermissionStatus(.grantedExplicitly, for: url)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView, sourceLocation: sourceLocation)
            webView.load(URLRequest(url: url))

            try await manager.run()
        }
    }

    // MARK: - Enums and Basic Properties

    @Test
    func onInstalledReason() async throws {
        try await loadAndRun(
            backgroundScript: """
                browser.test.assertEq(typeof browser.runtime.OnInstalledReason, 'object', 'OnInstalledReason should be an object')

                browser.test.assertEq(browser.runtime.OnInstalledReason.INSTALL, 'install', 'INSTALL should be defined')
                browser.test.assertEq(browser.runtime.OnInstalledReason.UPDATE, 'update', 'UPDATE should be defined')
                browser.test.assertEq(browser.runtime.OnInstalledReason.BROWSER_UPDATE, 'browser_update', 'BROWSER_UPDATE should be defined')

                browser.test.assertDeepEq(Object.keys(browser.runtime.OnInstalledReason).sort(), [ 'BROWSER_UPDATE', 'INSTALL', 'UPDATE' ], 'OnInstalledReason should only define the supported values')

                browser.test.notifyPass()
                """
        )
    }

    @Test
    func platformArch() async throws {
        try await loadAndRun(
            backgroundScript: """
                browser.test.assertEq(typeof browser.runtime.PlatformArch, 'object', 'PlatformArch should be an object')

                browser.test.assertEq(browser.runtime.PlatformArch.ARM64, 'arm64', 'ARM64 should be defined')
                browser.test.assertEq(browser.runtime.PlatformArch.X86_64, 'x86-64', 'X86_64 should be defined')

                browser.test.assertDeepEq(Object.keys(browser.runtime.PlatformArch).sort(), [ 'ARM64', 'X86_64' ], 'PlatformArch should only define the supported values')

                browser.test.notifyPass()
                """
        )
    }

    @Test
    func platformOs() async throws {
        try await loadAndRun(
            backgroundScript: """
                browser.test.assertEq(typeof browser.runtime.PlatformOs, 'object', 'PlatformOs should be an object')

                browser.test.assertEq(browser.runtime.PlatformOs.MAC, 'mac', 'MAC should be defined')
                browser.test.assertEq(browser.runtime.PlatformOs.IOS, 'ios', 'IOS should be defined')

                browser.test.assertDeepEq(Object.keys(browser.runtime.PlatformOs).sort(), [ 'IOS', 'MAC' ], 'PlatformOs should only define the supported values')

                browser.test.notifyPass()
                """
        )
    }

    @Test
    func getURL() async throws {
        let baseURLString = "test-extension://76C788B8-3374-400D-8259-40E5B9DF79D3"

        let backgroundScript = """
            // Variable Setup
            const baseURL = '\(baseURLString)'

            // Error Cases
            browser.test.assertThrows(() => browser.runtime.getURL(), /required argument is missing/i)
            browser.test.assertThrows(() => browser.runtime.getURL(null), /'resourcePath' value is invalid, because a string is expected/i)
            browser.test.assertThrows(() => browser.runtime.getURL(undefined), /'resourcePath' value is invalid, because a string is expected/i)
            browser.test.assertThrows(() => browser.runtime.getURL(42), /'resourcePath' value is invalid, because a string is expected/i)
            browser.test.assertThrows(() => browser.runtime.getURL(/test/), /'resourcePath' value is invalid, because a string is expected/i)

            // Normal Cases
            browser.test.assertEq(browser.runtime.getURL(''), `${baseURL}/`)
            browser.test.assertEq(browser.runtime.getURL('test.js'), `${baseURL}/test.js`)
            browser.test.assertEq(browser.runtime.getURL('/test.js'), `${baseURL}/test.js`)
            browser.test.assertEq(browser.runtime.getURL('../../test.js'), `${baseURL}/test.js`)
            browser.test.assertEq(browser.runtime.getURL('./test.js'), `${baseURL}/test.js`)
            browser.test.assertEq(browser.runtime.getURL('././/example'), `${baseURL}//example`)
            browser.test.assertEq(browser.runtime.getURL('../../example/..//test/'), `${baseURL}//test/`)
            browser.test.assertEq(browser.runtime.getURL('.'), `${baseURL}/`)
            browser.test.assertEq(browser.runtime.getURL('..//../'), `${baseURL}/`)
            browser.test.assertEq(browser.runtime.getURL('.././..'), `${baseURL}/`)
            browser.test.assertEq(browser.runtime.getURL('/.././.'), `${baseURL}/`)

            // Unexpected Cases
            // FIXME: <https://webkit.org/b/248154> browser.runtime.getURL() has some edge cases that should be failures or return different results
            browser.test.assertEq(browser.runtime.getURL('//'), 'test-extension://')
            browser.test.assertEq(browser.runtime.getURL('//example'), `test-extension://example`)
            browser.test.assertEq(browser.runtime.getURL('///'), 'test-extension:///')

            // Finish
            browser.test.notifyPass()
            """

        let manager = parseWebExtension(manifest: runtimeManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        // Set a base URL so it is a known value and not the default random one.
        WKWebExtension.MatchPattern.registerCustomURLScheme("test-extension")
        context.baseURL = try #require(URL(string: baseURLString))

        try await manager.loadAndRun()
    }

    @Test
    func getBackgroundPageFromBackground() async throws {
        try await loadAndRun(
            backgroundScript: """
                window.notifyTestPass = async () => {
                  browser.test.notifyPass()
                }

                const backgroundPage = await browser.runtime.getBackgroundPage()

                browser.test.assertEq(backgroundPage, window, 'Should be able to get the background window from itself')
                browser.test.assertSafe(() => backgroundPage.notifyTestPass())
                """
        )
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
            const backgroundPage = await browser.runtime.getBackgroundPage()
            browser.test.assertSafe(() => backgroundPage.notifyTestPass())
            """

        let testHTML = "<script type='module' src='test.js'></script>"

        let manager = try loadWebExtension(
            manifest: runtimeManifest,
            resources: [
                "background.js": backgroundScript,
                "test.html": testHTML,
                "test.js": testScript,
            ]
        )
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
            const backgroundPage = await browser.runtime.getBackgroundPage()
            browser.test.assertSafe(() => backgroundPage.notifyTestPass())
            """

        let popupHTML = "<script type='module' src='popup.js'></script>"

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "popup.html": popupHTML,
            "popup.js": popupScript,
        ]

        let manager = try loadWebExtension(manifest: runtimeManifest, resources: resources)

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
            const backgroundPage = await browser.runtime.getBackgroundPage()
            browser.test.assertEq(backgroundPage, null, 'Background page should be null for service workers')
            browser.test.notifyPass()
            """

        let testHTML = "<script type='module' src='test.js'></script>"

        let manager = try loadWebExtension(
            manifest: runtimeServiceWorkerManifest,
            resources: [
                "background.js": backgroundScript,
                "test.html": testHTML,
                "test.js": testScript,
            ]
        )
        try await manager.run()
    }

    @Test
    func setUninstallURL() async throws {
        try await loadAndRun(
            backgroundScript: """
                await browser.test.assertSafeResolve(() => browser.runtime.setUninstallURL('https://example.com/uninstall'))
                browser.test.notifyPass()
                """
        )
    }

    @Test
    func id() async throws {
        let uniqueIdentifier = "org.webkit.test.extension (76C788B8)"

        let backgroundScript = """
            browser.test.assertEq(browser.runtime.id, '\(uniqueIdentifier)')

            browser.test.notifyPass()
            """

        let manager = parseWebExtension(manifest: runtimeManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        // Set an uniqueIdentifier so it is a known value and not the default random one.
        context.uniqueIdentifier = uniqueIdentifier

        try await manager.loadAndRun()
    }

    @Test
    func getManifest() async throws {
        let testManifest: [String: Any] = [
            "manifest_version": 3,

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "unused": NSNull(),
        ]

        try await loadAndRun(
            manifest: testManifest,
            backgroundScript: """
                browser.test.assertDeepEq(browser.runtime.getManifest(), { manifest_version: 3, background: { persistent: false, scripts: [ 'background.js' ], type: 'module' }, unused: null })

                browser.test.notifyPass()
                """
        )
    }

    @Test
    func backgroundScriptDecodedAsUTF8() async throws {
        let testManifest: [String: Any] = [
            "manifest_version": 3,

            "background": [
                "scripts": ["background.js"],
                "persistent": false,
            ],
        ]

        try await loadAndRun(
            manifest: testManifest,
            backgroundScript: """
                const literal = '✕’ąŠ'
                browser.test.assertEq(literal, '\\u2715\\u2019\\u0105\\u0160')

                browser.test.notifyPass()
                """
        )
    }

    @Test
    func getVersion() async throws {
        let testManifest: [String: Any] = [
            "manifest_version": 3,

            "version": "1.0",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        try await loadAndRun(
            manifest: testManifest,
            backgroundScript: """
                browser.test.assertEq(browser.runtime.getVersion(), '1.0')

                browser.test.notifyPass()
                """
        )
    }

    @Test
    func getPlatformInfo() async throws {
        #if WTF_PLATFORM_MAC
        #if arch(arm64)
        let expectedInfo = "{ os: 'mac', arch: 'arm64' }"
        #elseif arch(x86_64)
        let expectedInfo = "{ os: 'mac', arch: 'x86-64' }"
        #else
        let expectedInfo = "{ os: 'unknown', arch: 'unknown' }"
        #endif
        #elseif WTF_PLATFORM_IOS_FAMILY
        #if arch(arm64)
        let expectedInfo = "{ os: 'ios', arch: 'arm64' }"
        #elseif arch(x86_64)
        let expectedInfo = "{ os: 'ios', arch: 'x86-64' }"
        #else
        let expectedInfo = "{ os: 'unknown', arch: 'unknown' }"
        #endif
        #else
        let expectedInfo = "{ os: 'unknown', arch: 'unknown' }"
        #endif

        try await loadAndRun(
            backgroundScript: """
                browser.test.assertTrue(browser.runtime.getPlatformInfo() instanceof Promise)
                browser.test.assertDeepEq(await browser.runtime.getPlatformInfo(), \(expectedInfo))
                browser.test.assertEq(browser.runtime.getPlatformInfo((info) => browser.test.assertDeepEq(info, \(expectedInfo))), undefined)

                browser.test.notifyPass()
                """
        )
    }

    // MARK: - Frame and Document IDs

    @Test
    func getFrameId() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/subframe'></iframe>"
            }
            Route("/subframe", headerFields: ["Content-Type": "text/html"]) {
                "Subframe Content"
            }
        }

        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            if (window.top === window) {
              let mainFrameId = browser.runtime.getFrameId(window)
              browser.test.assertEq(mainFrameId, 0, 'Main frame should have frameId 0')

              let frameElement = document.querySelector('iframe')
              let subFrameElementId = browser.runtime.getFrameId(frameElement)
              browser.test.assertTrue(subFrameElementId > 0, 'Subframes should have a positive non-zero frameId')

              let subFrameWindow = frameElement.contentWindow
              let subFrameWindowId = browser.runtime.getFrameId(subFrameWindow)
              browser.test.assertTrue(subFrameWindowId > 0, 'Subframes should have a positive non-zero frameId')

              browser.test.assertEq(subFrameElementId, subFrameWindowId, 'Subframes should have the same frameId from window and element')
            } else {
              let subFrameId = browser.runtime.getFrameId(window)
              browser.test.assertTrue(subFrameId > 0, 'Subframes should have a positive non-zero frameId')

              let topFrameId = browser.runtime.getFrameId(window.top)
              browser.test.assertEq(topFrameId, 0, 'The top window in a subframe context should have frameId 0')
            }

            browser.test.notifyPass()
            """

        try await server.run { configuration in
            let url = configuration.localhostAddress

            let manager = try loadWebExtension(
                manifest: runtimeContentScriptManifest,
                resources: ["background.js": backgroundScript, "content.js": contentScript]
            )
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: url)

            try await manager.waitForTestMessage("Load Tab")

            try #require(manager.defaultTab?.webView).load(URLRequest(url: url))

            try await manager.run()
        }
    }

    @Test
    func getDocumentId() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/subframe'></iframe>"
            }
            Route("/subframe", headerFields: ["Content-Type": "text/html"]) {
                "Subframe Content"
            }
        }

        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            function isValidDocId(input) {
              return typeof input === 'string' && input.length > 0
            }

            if (window.top === window) {
              browser.test.assertThrows(() => browser.runtime.getDocumentId(), /required argument is missing/, 'Method should throw on incorrect input undefined')
              browser.test.assertThrows(() => browser.runtime.getDocumentId(null), /value is invalid, because an object is expected/, 'Method should throw on incorrect input type')
              browser.test.assertThrows(() => browser.runtime.getDocumentId('test'), /value is invalid, because an object is expected/, 'Method should throw on incorrect input type')
              browser.test.assertThrows(() => browser.runtime.getDocumentId({}), /not a valid window or frame element/, 'Method should throw on incorrect input objects')

              const mainDocId = browser.runtime.getDocumentId(window)
              browser.test.assertTrue(isValidDocId(mainDocId), 'Main document should return a valid documentId')

              const frameElement = document.querySelector('iframe')
              const subFrameElementDocId = browser.runtime.getDocumentId(frameElement)
              browser.test.assertTrue(isValidDocId(subFrameElementDocId), 'Subframe element should return a valid documentId')

              const subFrameWindow = frameElement.contentWindow
              const subFrameWindowDocId = browser.runtime.getDocumentId(subFrameWindow)
              browser.test.assertEq(subFrameElementDocId, subFrameWindowDocId, 'DocumentId from element and window should match')
              browser.test.assertTrue(mainDocId !== subFrameWindowDocId, 'Main documentId and subframe documentId should be different')
            } else {
              const subFrameDocId = browser.runtime.getDocumentId(window)
              browser.test.assertTrue(isValidDocId(subFrameDocId), 'Subframe document should return a valid documentId')

              const topDocId = browser.runtime.getDocumentId(window.top)
              browser.test.assertTrue(isValidDocId(topDocId), 'Subframe should be able to retrieve the top window documentId')
            }

            browser.test.notifyPass()
            """

        try await server.run { configuration in
            let url = configuration.localhostAddress

            let manager = try loadWebExtension(
                manifest: runtimeContentScriptManifest,
                resources: ["background.js": backgroundScript, "content.js": contentScript]
            )
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: url)

            try await manager.waitForTestMessage("Load Tab")

            try #require(manager.defaultTab?.webView).load(URLRequest(url: url))

            try await manager.run()
        }
    }

    // MARK: - Last Error

    @Test
    func lastError() async throws {
        try await loadAndRun(
            backgroundScript: """
                browser.test.assertEq(typeof browser.runtime.lastError, 'object')
                browser.test.assertEq(browser.runtime.lastError, null)

                browser.test.notifyPass()
                """
        )
    }

    @Test
    func lastErrorSetInCallbackOnFailure() async throws {
        try await loadAndRun(
            backgroundScript: """
                browser.tabs.get(9999, () => {
                  browser.test.assertEq(typeof browser.runtime.lastError, 'object')
                  browser.test.assertTrue(!!browser.runtime.lastError, 'lastError should be set inside the callback')
                  browser.test.assertEq(typeof browser.runtime.lastError.message, 'string')
                  browser.test.assertTrue(/tab not found/i.test(browser.runtime.lastError.message), 'lastError.message should describe the failure')

                  browser.test.notifyPass()
                })
                """
        )
    }

    @Test
    func lastErrorNullInCallbackOnSuccess() async throws {
        try await loadAndRun(
            backgroundScript: """
                browser.tabs.query({ }, () => {
                  browser.test.assertEq(browser.runtime.lastError, null)

                  browser.test.notifyPass()
                })
                """
        )
    }

    @Test
    func lastErrorClearedAfterCallback() async throws {
        try await loadAndRun(
            backgroundScript: """
                browser.tabs.get(9999, () => {
                  browser.test.assertTrue(!!browser.runtime.lastError, 'lastError should be set inside the callback')
                })

                // A second round trip is processed after the failing callback has returned.
                await browser.tabs.query({ })

                browser.test.assertEq(browser.runtime.lastError, null, 'lastError should be cleared once the callback returns')

                browser.test.notifyPass()
                """
        )
    }

    @Test
    func lastErrorFromContentScript() async throws {
        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            browser.test.assertEq(typeof browser.runtime.lastError, 'object', 'lastError should be exposed to content scripts')
            browser.test.assertEq(browser.runtime.lastError, null, 'lastError should be null outside of a failed callback')

            browser.runtime.sendMessage('unknown.extension.identifier', { content: 'Hello' }, () => {
              const error = browser.runtime.lastError
              browser.test.assertEq(typeof error, 'object', 'lastError should be set inside the failed callback')
              browser.test.assertTrue(!!error?.message?.includes('cross-extension messaging is not supported'), 'lastError should describe the failure')

              browser.test.notifyPass()
            })
            """

        try await loadAndRunWithEmptyPage(
            manifest: runtimeContentScriptManifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    // MARK: - Messaging

    @Test
    func sendMessageFromContentScript() async throws {
        try await loadAndRunWithEmptyPage(manifest: runtimeContentScriptManifest) { url in
            let urlString = url.absoluteString

            let backgroundScript = """
                const expectedURL = '\(urlString)'
                const expectedOrigin = '\(urlString.dropLast())'

                browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
                  browser.test.assertEq(message?.content, 'Hello', 'Should receive the correct message from the content script')

                  browser.test.assertEq(typeof sender, 'object', 'sender should be an object')

                  browser.test.assertEq(typeof sender.tab, 'object', 'sender.tab should be an object')
                  browser.test.assertEq(sender?.tab?.url, expectedURL, 'sender.tab.url should be the expected URL')

                  browser.test.assertEq(sender?.url, expectedURL, 'sender.url should be the expected URL')
                  browser.test.assertEq(sender?.origin, expectedOrigin, 'sender.origin should be the expected origin')

                  browser.test.assertEq(sender?.frameId, 0, 'sender.frameId should be 0')

                  browser.test.assertEq(typeof sender?.documentId, 'string', 'sender.documentId should be')
                  browser.test.assertEq(sender?.documentId?.length, 36, 'sender.documentId.length should be')

                  sendResponse({ content: 'Received' })
                })

                browser.test.sendMessage('Load Tab')
                """

            let contentScript = """
                (async () => {
                  const response = await browser.runtime.sendMessage({ content: 'Hello' })

                  browser.test.assertEq(response?.content, 'Received', 'Should get the response from the background script')

                  browser.test.notifyPass()
                })()
                """

            return ["background.js": backgroundScript, "content.js": contentScript]
        }
    }

    @Test
    func sendMessageWithEmptyContainers() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
              const containers = [message.firstArray, message.secondArray, message.firstObject, message.secondObject, message.nested.array, message.nested.object, message.list[0], message.list[1]]
              for (let i = 0; i < containers.length; ++i) {
                for (let j = i + 1; j < containers.length; ++j)
                  browser.test.assertFalse(containers[i] === containers[j], `Message containers ${i} and ${j} should be distinct`)
              }

              message.firstArray.push(1)
              browser.test.assertEq(message.secondArray.length, 0, 'Mutating one empty array should not change the other')

              sendResponse({ firstArray: [], secondArray: [], firstObject: {}, secondObject: {} })
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            (async () => {
              const response = await browser.runtime.sendMessage({ firstArray: [], secondArray: [], firstObject: {}, secondObject: {}, nested: { array: [], object: {} }, list: [[], []] })

              browser.test.assertFalse(response.firstArray === response.secondArray, 'Empty arrays in the response should be distinct')
              browser.test.assertFalse(response.firstObject === response.secondObject, 'Empty objects in the response should be distinct')

              browser.test.notifyPass()
            })()
            """

        try await loadAndRunWithEmptyPage(
            manifest: runtimeContentScriptManifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    @Test
    func messageSenderWindowIdFromContentScript() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
              browser.test.assertEq(typeof sender.windowId, 'number', 'sender.windowId should be a number')
              browser.test.assertEq(sender.windowId, sender.tab?.windowId, 'sender.windowId should match sender.tab.windowId')
              browser.test.assertTrue(sender.windowId !== browser.windows.WINDOW_ID_NONE, 'sender.windowId should be a real window')

              sendResponse({ content: 'Received' })
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            (async () => {
              const response = await browser.runtime.sendMessage({ content: 'Hello' })

              browser.test.assertEq(response?.content, 'Received', 'Should get the response from the background script')

              browser.test.notifyPass()
            })()
            """

        try await loadAndRunWithEmptyPage(
            manifest: runtimeContentScriptManifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    @Test
    func portSenderWindowIdFromContentScript() async throws {
        let backgroundScript = """
            browser.runtime.onConnect.addListener((port) => {
              browser.test.assertEq(typeof port.sender.windowId, 'number', 'port.sender.windowId should be a number')
              browser.test.assertEq(port.sender.windowId, port.sender.tab?.windowId, 'port.sender.windowId should match port.sender.tab.windowId')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            browser.runtime.connect({ name: 'window-id' })
            """

        try await loadAndRunWithEmptyPage(
            manifest: runtimeContentScriptManifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    @Test
    func messageSenderWindowIdFromPopup() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener(async (message, sender) => {
              if (message !== 'check-sender')
                return

              const [activeTab] = await browser.tabs.query({ active: true, currentWindow: true })

              browser.test.assertEq(sender.tab, undefined, 'sender.tab should be absent for a popup')
              browser.test.assertEq(typeof sender.windowId, 'number', 'sender.windowId should be a number for a popup')
              browser.test.assertEq(sender.windowId, activeTab?.windowId, 'sender.windowId should be the window of the tab the popup opened for')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Background Ready')
            """

        let popupScript = """
            browser.runtime.sendMessage('check-sender')
            """

        let manager = try loadWebExtension(
            manifest: runtimeManifest,
            resources: [
                "background.js": backgroundScript,
                "popup.html": "<script type='module' src='popup.js'></script>",
                "popup.js": popupScript,
            ]
        )
        let context = try #require(manager.context)

        try await manager.waitForTestMessage("Background Ready")

        manager.internalDelegate.presentPopupForAction = { _ in }

        context.performAction(for: manager.defaultTab)

        try await manager.run()
    }

    @Test
    func tabsSendMessageSenderWindowIdFromPopup() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
              sendResponse({ tab: sender.tab, windowId: sender.windowId })
            })

            browser.test.sendMessage('Content Ready')
            """

        let popupScript = """
            (async () => {
              const [activeTab] = await browser.tabs.query({ active: true, currentWindow: true })
              const sender = await browser.tabs.sendMessage(activeTab.id, 'check-sender')

              browser.test.assertEq(sender?.tab, undefined, 'sender.tab should be absent for a popup')
              browser.test.assertEq(typeof sender?.windowId, 'number', 'sender.windowId should be a number for a popup')
              browser.test.assertEq(sender?.windowId, activeTab.windowId, 'sender.windowId should be the window of the tab the popup opened for')

              browser.test.notifyPass()
            })()
            """

        try await server.run { configuration in
            let url = configuration.localhostAddress

            let manager = try loadWebExtension(
                manifest: runtimePopupContentScriptManifest,
                resources: [
                    "background.js": backgroundScript,
                    "content.js": contentScript,
                    "popup.html": "<script type='module' src='popup.js'></script>",
                    "popup.js": popupScript,
                ]
            )
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: url)

            try await manager.waitForTestMessage("Load Tab")

            try #require(manager.defaultTab?.webView).load(URLRequest(url: url))

            try await manager.waitForTestMessage("Content Ready")

            manager.internalDelegate.presentPopupForAction = { _ in }

            context.performAction(for: manager.defaultTab)

            try await manager.run()
        }
    }

    @Test
    func tabsConnectSenderWindowIdFromPopup() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            browser.runtime.onConnect.addListener((port) => {
              port.postMessage({ tab: port.sender.tab, windowId: port.sender.windowId })
            })

            browser.test.sendMessage('Content Ready')
            """

        let popupScript = """
            (async () => {
              const [activeTab] = await browser.tabs.query({ active: true, currentWindow: true })
              const port = browser.tabs.connect(activeTab.id)

              port.onMessage.addListener((sender) => {
                browser.test.assertEq(sender?.tab, undefined, 'port.sender.tab should be absent for a popup')
                browser.test.assertEq(typeof sender?.windowId, 'number', 'port.sender.windowId should be a number for a popup')
                browser.test.assertEq(sender?.windowId, activeTab.windowId, 'port.sender.windowId should be the window of the tab the popup opened for')

                browser.test.notifyPass()
              })
            })()
            """

        try await server.run { configuration in
            let url = configuration.localhostAddress

            let manager = try loadWebExtension(
                manifest: runtimePopupContentScriptManifest,
                resources: [
                    "background.js": backgroundScript,
                    "content.js": contentScript,
                    "popup.html": "<script type='module' src='popup.js'></script>",
                    "popup.js": popupScript,
                ]
            )
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: url)

            try await manager.waitForTestMessage("Load Tab")

            try #require(manager.defaultTab?.webView).load(URLRequest(url: url))

            try await manager.waitForTestMessage("Content Ready")

            manager.internalDelegate.presentPopupForAction = { _ in }

            context.performAction(for: manager.defaultTab)

            try await manager.run()
        }
    }

    @Test
    func portPostMessageDeliversEachValueOnceToRepeatedListener() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            browser.runtime.onConnect.addListener((port) => {
              const received = []
              function listener(message) { received.push(message) }

              for (let i = 0; i < 100; ++i)
                port.onMessage.addListener(listener)

              let messageCount = 0

              // Registered after the repeated listener, so it runs after every call to it for the same message.
              port.onMessage.addListener(() => {
                if (++messageCount < 2)
                  return

                browser.test.assertEq(received.length, 2, 'Repeated addListener calls with the same function should register it once')
                browser.test.assertDeepEq(received[0], { value: 1, list: [ 'a', 2, true ] }, 'Should receive the object')
                browser.test.assertEq(received[1], 'text', 'Should receive a top-level string')

                browser.test.notifyPass()
              })
            })

            browser.test.sendMessage('Content Ready')
            """

        let popupScript = """
            (async () => {
              const [activeTab] = await browser.tabs.query({ active: true, currentWindow: true })
              const port = browser.tabs.connect(activeTab.id)

              port.postMessage({ value: 1, list: [ 'a', 2, true ] })
              port.postMessage('text')
            })()
            """

        try await server.run { configuration in
            let url = configuration.localhostAddress

            let manager = try loadWebExtension(
                manifest: runtimePopupContentScriptManifest,
                resources: [
                    "background.js": backgroundScript,
                    "content.js": contentScript,
                    "popup.html": "<script type='module' src='popup.js'></script>",
                    "popup.js": popupScript,
                ]
            )
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: url)

            try await manager.waitForTestMessage("Load Tab")

            try #require(manager.defaultTab?.webView).load(URLRequest(url: url))

            try await manager.waitForTestMessage("Content Ready")

            manager.internalDelegate.presentPopupForAction = { _ in }

            context.performAction(for: manager.defaultTab)

            try await manager.run()
        }
    }

    @Test
    func sendMessageWithStructuredClone() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
              browser.test.assertTrue(message.buffer instanceof ArrayBuffer, 'ArrayBuffer should arrive as an ArrayBuffer')
              browser.test.assertDeepEq(Array.from(new Uint8Array(message.buffer)), [ 1, 2, 3 ], 'ArrayBuffer should keep its bytes')
              browser.test.assertTrue(message.floats instanceof Float32Array, 'Typed arrays should keep their type')
              browser.test.assertTrue(message.map instanceof Map, 'Map should arrive as a Map')
              browser.test.assertEq(message.map.get('Content-Language'), 'en', 'Map should keep its entries')
              browser.test.assertTrue(message.set instanceof Set, 'Set should arrive as a Set')
              browser.test.assertTrue(message.date instanceof Date, 'Date should arrive as a Date')
              browser.test.assertEq(message.date.getTime(), 1700000000000, 'Date should keep its time')
              browser.test.assertEq(message.bigint, 9007199254740993n, 'BigInt should keep its value')
              browser.test.assertEq(message.undefinedValue, undefined, 'undefined should stay present')
              browser.test.assertTrue('undefinedValue' in message, 'undefined properties should not be dropped')
              browser.test.assertEq(message.nullValue, null, 'null should stay null')
              browser.test.assertTrue('nullValue' in message, 'null properties should not be dropped')

              sendResponse({ reply: new Map([[ 'Via', '1.1 proxy-a' ]]), bytes: new Uint8Array([ 4, 5, 6 ]) })
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            (async () => {
              const response = await browser.runtime.sendMessage({
                buffer: new Uint8Array([ 1, 2, 3 ]).buffer,
                floats: new Float32Array([ 1.5, 2.5 ]),
                map: new Map([[ 'Content-Language', 'en' ]]),
                set: new Set([ 'a', 'b' ]),
                date: new Date(1700000000000),
                bigint: 9007199254740993n,
                nullValue: null,
                undefinedValue: undefined
              })

              browser.test.assertTrue(response.reply instanceof Map, 'Reply Map should arrive as a Map')
              browser.test.assertEq(response.reply.get('Via'), '1.1 proxy-a', 'Reply Map should keep its entries')
              browser.test.assertTrue(response.bytes instanceof Uint8Array, 'Reply typed array should keep its type')
              browser.test.assertDeepEq(Array.from(response.bytes), [ 4, 5, 6 ], 'Reply typed array should keep its bytes')

              browser.test.notifyPass()
            })()
            """

        try await loadAndRunWithEmptyPage(
            manifest: structuredCloneContentScriptManifest(),
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    @Test
    func sendMessageWithStructuredCloneNullReply() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
              browser.test.assertEq(message.value, null, 'A null property should arrive as null')

              sendResponse(null)
            })

            browser.test.sendMessage('Load Tab')
            """

        // A null reply is still a reply, so it must not look like no listener responded.
        let contentScript = """
            (async () => {
              const response = await browser.runtime.sendMessage({ value: null })
              browser.test.assertEq(response, null, 'A null reply should arrive as null')

              browser.test.notifyPass()
            })()
            """

        try await loadAndRunWithEmptyPage(
            manifest: structuredCloneContentScriptManifest(),
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    @Test
    func sendMessageWithStructuredCloneBlob() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener(async (message, sender, sendResponse) => {
              browser.test.assertTrue(message.blob instanceof Blob, 'Blob should arrive as a Blob')
              browser.test.assertEq(message.blob.type, 'text/plain', 'Blob should keep its type')
              browser.test.assertEq(message.blob.size, 5, 'Blob should keep its size')
              browser.test.assertEq(await message.blob.text(), 'Hello', 'Blob should keep its contents')

              sendResponse({ blob: new Blob([ 'World' ], { type: 'text/plain' }) })
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            (async () => {
              const blob = new Blob([ 'Hello' ], { type: 'text/plain' })
              const response = await browser.runtime.sendMessage({ blob })

              browser.test.assertTrue(response.blob instanceof Blob, 'Reply Blob should arrive as a Blob')
              browser.test.assertEq(await response.blob.text(), 'World', 'Reply Blob should keep its contents')
              browser.test.assertEq(blob.size, 5, 'Sent Blob should stay usable')

              browser.test.notifyPass()
            })()
            """

        try await loadAndRunWithEmptyPage(
            manifest: structuredCloneContentScriptManifest(),
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    @Test
    func sendMessageWithoutStructuredCloneUsesJSON() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
              browser.test.assertFalse(message.buffer instanceof ArrayBuffer, 'ArrayBuffer should not survive JSON')
              browser.test.assertDeepEq(message.buffer, { }, 'ArrayBuffer should arrive as an empty object')
              browser.test.assertEq(message.date, '2023-11-14T22:13:20.000Z', 'Date should arrive as an ISO string')

              sendResponse({ ok: true })
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            (async () => {
              const response = await browser.runtime.sendMessage({ buffer: new Uint8Array([ 1, 2, 3 ]).buffer, date: new Date(1700000000000) })
              browser.test.assertEq(response?.ok, true, 'Should get the response from the background script')

              browser.test.notifyPass()
            })()
            """

        try await loadAndRunWithEmptyPage(
            manifest: runtimeContentScriptManifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    @Test
    func sendMessageWithStructuredCloneRejectsUnclonableValue() async throws {
        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            (async () => {
              let didThrow = false
              try {
                await browser.runtime.sendMessage({ callback() { } })
              } catch (error) {
                didThrow = true
              }

              browser.test.assertTrue(didThrow, 'Sending a function should fail with structured clone')

              browser.test.notifyPass()
            })()
            """

        try await loadAndRunWithEmptyPage(
            manifest: structuredCloneContentScriptManifest(),
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    @Test
    func portPostMessageWithStructuredClone() async throws {
        let backgroundScript = """
            browser.runtime.onConnect.addListener((port) => {
              port.onMessage.addListener((message) => {
                browser.test.assertTrue(message.map instanceof Map, 'Map should arrive as a Map')
                browser.test.assertEq(message.map.get('Content-Language'), 'en', 'Map should keep its entries')
                browser.test.assertDeepEq(Array.from(new Uint8Array(message.buffer)), [ 7, 8, 9 ], 'ArrayBuffer should keep its bytes')

                port.postMessage({ echo: new Set([ 'a', 'b' ]) })
              })
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            const port = browser.runtime.connect()

            port.onMessage.addListener((message) => {
              browser.test.assertTrue(message.echo instanceof Set, 'Reply Set should arrive as a Set')
              browser.test.assertEq(message.echo.size, 2, 'Reply Set should keep its entries')

              browser.test.notifyPass()
            })

            port.postMessage({ map: new Map([[ 'Content-Language', 'en' ]]), buffer: new Uint8Array([ 7, 8, 9 ]).buffer })
            """

        try await loadAndRunWithEmptyPage(
            manifest: structuredCloneContentScriptManifest(),
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    @Test
    func sendMessageFromContentScriptWhileBackgroundIsLoading() async throws {
        // A content script sends runtime.sendMessage while the non-persistent background content
        // is still loading on initial load (before its onMessage listener has registered). The
        // message must be queued and delivered once the background loads, not dropped.

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
              browser.test.assertEq(message?.content, 'Hello', 'Should receive the message sent while the background was loading')
              sendResponse({ content: 'Received' })
            })
            """

        let contentScript = """
            (async () => {
              const response = await browser.runtime.sendMessage({ content: 'Hello' })

              browser.test.assertEq(response?.content, 'Received', 'Should get the response from the background script')

              browser.test.notifyPass()
            })()
            """

        try await server.run { configuration in
            let url = configuration.localhostAddress

            let manager = try loadWebExtension(
                manifest: runtimeContentScriptManifest,
                resources: ["background.js": backgroundScript, "content.js": contentScript]
            )
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: url)

            // Load the tab immediately, without waiting for the background to be ready, so the content script's
            // runtime.sendMessage races the initial background content load.
            try #require(manager.defaultTab?.webView).load(URLRequest(url: url))

            try await manager.run()
        }
    }

    @Test
    func sendMessageFromContentScriptWithAsyncReply() async throws {
        try await loadAndRunWithEmptyPage(manifest: runtimeContentScriptManifest) { url in
            let urlString = url.absoluteString

            let backgroundScript = """
                const expectedURL = '\(urlString)'
                const expectedOrigin = '\(urlString.dropLast())'

                browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
                  browser.test.assertEq(message?.content, 'Hello', 'Should receive the correct message from the content script')

                  browser.test.assertEq(typeof sender, 'object', 'sender should be an object')

                  browser.test.assertEq(typeof sender.tab, 'object', 'sender.tab should be an object')
                  browser.test.assertEq(sender?.tab?.url, expectedURL, 'sender.tab.url should be the expected URL')

                  browser.test.assertEq(sender?.url, expectedURL, 'sender.url should be the expected URL')
                  browser.test.assertEq(sender?.origin, expectedOrigin, 'sender.origin should be the expected origin')

                  browser.test.assertEq(sender?.frameId, 0, 'sender.frameId should be 0')

                  browser.test.assertEq(typeof sender?.documentId, 'string', 'sender.documentId should be')
                  browser.test.assertEq(sender?.documentId?.length, 36, 'sender.documentId.length should be')

                  setTimeout(() => sendResponse({ content: 'Received' }), 1000)

                  return true
                })

                browser.test.sendMessage('Load Tab')
                """

            let contentScript = """
                (async () => {
                  const response = await browser.runtime.sendMessage({ content: 'Hello' })

                  browser.test.assertEq(response?.content, 'Received', 'Should get the response from the background script')

                  browser.test.notifyPass()
                })()
                """

            return ["background.js": backgroundScript, "content.js": contentScript]
        }
    }

    @Test
    func sendMessageFromContentScriptWithPromiseReply() async throws {
        try await loadAndRunWithEmptyPage(manifest: runtimeContentScriptManifest) { url in
            let urlString = url.absoluteString

            let backgroundScript = """
                const expectedURL = '\(urlString)'
                const expectedOrigin = '\(urlString.dropLast())'

                browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
                  browser.test.assertEq(message?.content, 'Hello', 'Should receive the correct message from the content script')

                  browser.test.assertEq(typeof sender, 'object', 'sender should be an object')

                  browser.test.assertEq(typeof sender.tab, 'object', 'sender.tab should be an object')
                  browser.test.assertEq(sender?.tab?.url, expectedURL, 'sender.tab.url should be the expected URL')

                  browser.test.assertEq(sender?.url, expectedURL, 'sender.url should be the expected URL')
                  browser.test.assertEq(sender?.origin, expectedOrigin, 'sender.origin should be the expected origin')

                  browser.test.assertEq(sender?.frameId, 0, 'sender.frameId should be 0')

                  browser.test.assertEq(typeof sender?.documentId, 'string', 'sender.documentId should be')
                  browser.test.assertEq(sender?.documentId?.length, 36, 'sender.documentId.length should be')

                  return Promise.resolve({ content: 'Received' })
                })

                browser.test.sendMessage('Load Tab')
                """

            let contentScript = """
                (async () => {
                  const response = await browser.runtime.sendMessage({ content: 'Hello' })

                  browser.test.assertEq(response?.content, 'Received', 'Should get the response from the background script')

                  browser.test.notifyPass()
                })()
                """

            return ["background.js": backgroundScript, "content.js": contentScript]
        }
    }

    @Test
    func sendMessageFromContentScriptWithAsyncPromiseReply() async throws {
        try await loadAndRunWithEmptyPage(manifest: runtimeContentScriptManifest) { url in
            let urlString = url.absoluteString

            let backgroundScript = """
                const expectedURL = '\(urlString)'
                const expectedOrigin = '\(urlString.dropLast())'

                browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
                  browser.test.assertEq(message?.content, 'Hello', 'Should receive the correct message from the content script')

                  browser.test.assertEq(typeof sender, 'object', 'sender should be an object')

                  browser.test.assertEq(typeof sender.tab, 'object', 'sender.tab should be an object')
                  browser.test.assertEq(sender?.tab?.url, expectedURL, 'sender.tab.url should be the expected URL')

                  browser.test.assertEq(sender?.url, expectedURL, 'sender.url should be the expected URL')
                  browser.test.assertEq(sender?.origin, expectedOrigin, 'sender.origin should be the expected origin')

                  browser.test.assertEq(sender?.frameId, 0, 'sender.frameId should be 0')

                  browser.test.assertEq(typeof sender?.documentId, 'string', 'sender.documentId should be')
                  browser.test.assertEq(sender?.documentId?.length, 36, 'sender.documentId.length should be')

                  return new Promise((resolve) => {
                    setTimeout(() => resolve({ content: 'Received' }), 1000)
                  })
                })

                browser.test.sendMessage('Load Tab')
                """

            let contentScript = """
                (async () => {
                  const response = await browser.runtime.sendMessage({ content: 'Hello' })

                  browser.test.assertEq(response?.content, 'Received', 'Should get the response from the background script')

                  browser.test.notifyPass()
                })()
                """

            return ["background.js": backgroundScript, "content.js": contentScript]
        }
    }

    @Test
    func sendMessageFromContentScriptWithNoReply() async throws {
        try await loadAndRunWithEmptyPage(manifest: runtimeContentScriptManifest) { url in
            let urlString = url.absoluteString

            let backgroundScript = """
                const expectedURL = '\(urlString)'
                const expectedOrigin = '\(urlString.dropLast())'

                browser.runtime.onMessage.addListener((message, sender) => {
                  browser.test.assertEq(message?.content, 'Hello', 'Should receive the correct message from the content script')

                  browser.test.assertEq(typeof sender, 'object', 'sender should be an object')

                  browser.test.assertEq(typeof sender.tab, 'object', 'sender.tab should be an object')
                  browser.test.assertEq(sender?.tab?.url, expectedURL, 'sender.tab.url should be the expected URL')

                  browser.test.assertEq(sender?.url, expectedURL, 'sender.url should be the expected URL')
                  browser.test.assertEq(sender?.origin, expectedOrigin, 'sender.origin should be the expected origin')

                  browser.test.assertEq(sender?.frameId, 0, 'sender.frameId should be 0')

                  browser.test.assertEq(typeof sender?.documentId, 'string', 'sender.documentId should be')
                  browser.test.assertEq(sender?.documentId?.length, 36, 'sender.documentId.length should be')

                  return false
                })

                browser.test.sendMessage('Load Tab')
                """

            let contentScript = """
                (async () => {
                  const response = await browser.runtime.sendMessage({ content: 'Hello' })

                  browser.test.assertEq(response, undefined, 'Should get no response from the background script')

                  browser.test.notifyPass()
                })()
                """

            return ["background.js": backgroundScript, "content.js": contentScript]
        }
    }

    @Test
    func sendMessageFromSubframe() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/main", headerFields: ["Content-Type": "text/html"]) {
                "<body><script>"
                    + "  document.write('<iframe src=\"http://127.0.0.1:' + location.port + '/subframe\"></iframe>')"
                    + "</script></body>"
            }
            Route("/subframe", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.runtime.onMessage.addListener((message, sender) => {
              browser.test.assertEq(message?.content, 'Hello from Subframe', 'Should receive the correct message from the subframe content script')
              return Promise.resolve({ content: 'Received your message' })
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            (async () => {
              const response = await browser.runtime.sendMessage({ content: 'Hello from Subframe' })
              browser.test.assertEq(response?.content, 'Received your message', 'Should get the response from the background script')

              browser.test.notifyPass()
            })()
            """

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

            "content_scripts": [
                [
                    "matches": ["*://127.0.0.1/*"],
                    "js": ["content.js"],
                    "all_frames": true,
                ]
            ],
        ]

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "content.js": contentScript,
        ]

        try await server.run { configuration in
            let mainURL = configuration.localhostAddress.appending(path: "main")
            let subframeURL = configuration.address.appending(path: "subframe")

            let manager = try loadWebExtension(manifest: manifest, resources: resources)
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: subframeURL)

            try await manager.waitForTestMessage("Load Tab")

            try #require(manager.defaultTab?.webView).load(URLRequest(url: mainURL))

            try await manager.run()
        }
    }

    @Test
    func sendMessageWithTabFrameAndAsyncReply() async throws {
        let extensionManifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Runtime Test",
            "description": "Runtime Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "content_scripts": [
                [
                    "js": ["content.js"],
                    "matches": ["*://localhost/*"],
                ]
            ],

            "web_accessible_resources": [
                [
                    "resources": ["*.html"],
                    "matches": ["*://localhost/*"],
                ]
            ],
        ]

        let backgroundScript = """
            browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
              if (message?.content === 'Hello from iframe')
                setTimeout(() => sendResponse({ content: 'Async reply from background' }), 500)

              return true
            })

            browser.test.sendMessage('Load Tab')
            """

        let iframeScript = """
            browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
              // This listener should not be called since it is in the same frame as the sender,
              // but having it is needed to verify the background script is the responder.

              browser.test.notifyFail('Frame listener should not be called')
            })

            const response = await browser.runtime.sendMessage({ content: 'Hello from iframe' })
            browser.test.assertEq(response?.content, 'Async reply from background', 'Should receive the correct async reply from background script')

            browser.test.notifyPass()
            """

        let contentScript = """
            (function() {
              const iframe = document.createElement('iframe')
              iframe.src = browser.runtime.getURL('extension-frame.html')
              document.body.appendChild(iframe)
            })()
            """

        try await loadAndRunWithEmptyPage(
            manifest: extensionManifest,
            resources: [
                "background.js": backgroundScript,
                "content.js": contentScript,
                "extension-frame.js": iframeScript,
                "extension-frame.html": "<script type='module' src='extension-frame.js'></script>",
            ]
        )
    }

    @Test
    func sendMessageWithNaNValue() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
              browser.test.assertEq(message?.value, null, 'The message value should be null')

              sendResponse({ value: NaN })
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            (async () => {
              const response = await browser.runtime.sendMessage({ value: NaN })

              browser.test.assertEq(response?.value, null, 'Should get a null response from the background script')

              browser.test.notifyPass()
            })()
            """

        try await loadAndRunWithEmptyPage(
            manifest: runtimeContentScriptManifest,
            resources: [
                "background.js": backgroundScript,
                "content.js": contentScript,
            ]
        )
    }

    // MARK: - User Gestures

    @Test
    func sendMessageGestureFromContentScriptIsNotPropagated() async throws {
        // Gestures from content scripts are intentionally not propagated to extension pages
        // to prevent web page interactions from triggering privileged extension APIs.

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
              if (message !== 'check-gesture')
                return

              browser.test.assertFalse(navigator.userActivation.isActive, 'User gesture from content script should not propagate to background onMessage')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Background Ready')
            """

        let contentScript = """
            browser.test.onMessage.addListener((message) => {
              if (message !== 'Send')
                return

              browser.test.runWithUserGesture(() => {
                browser.runtime.sendMessage('check-gesture')
              })
            })

            browser.test.sendMessage('Content Ready')
            """

        try await server.run { configuration in
            let url = configuration.localhostAddress

            let manager = try loadWebExtension(
                manifest: runtimeContentScriptManifest,
                resources: ["background.js": backgroundScript, "content.js": contentScript]
            )
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: url)
            try #require(manager.defaultTab?.webView).load(URLRequest(url: url))

            try await manager.waitForTestMessage("Background Ready")
            try await manager.waitForTestMessage("Content Ready")

            manager.sendTestMessage("Send")

            try await manager.run()
        }
    }

    @Test
    func sendMessageGestureFromPopupIsPropagated() async throws {
        // Gestures from extension pages (popup, sidebar) ARE propagated to other extension pages.
        // These originate from deliberate user interaction with the extension UI, unlike content
        // scripts which run in web page contexts where any page click qualifies as a gesture.

        let backgroundScript = """
            browser.runtime.onMessage.addListener((message) => {
              if (message !== 'check-gesture')
                return

              browser.test.assertTrue(navigator.userActivation.isActive, 'User gesture from popup (extension page) should propagate to background onMessage')
              browser.test.notifyPass()
            })

            browser.test.sendMessage('Background Ready')
            """

        let popupScript = """
            browser.test.runWithUserGesture(() => {
              browser.runtime.sendMessage('check-gesture')
            })
            """

        let manager = try loadWebExtension(
            manifest: runtimeManifest,
            resources: [
                "background.js": backgroundScript,
                "popup.html": "<script type='module' src='popup.js'></script>",
                "popup.js": popupScript,
            ]
        )
        let context = try #require(manager.context)

        try await manager.waitForTestMessage("Background Ready")

        manager.internalDelegate.presentPopupForAction = { _ in }

        context.performAction(for: manager.defaultTab)

        try await manager.run()
    }

    @Test
    func sendMessageWithoutUserGestureFromContentScript() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
              if (message !== 'check-gesture')
                return

              browser.test.assertFalse(navigator.userActivation.isActive, 'User gesture should not be active in background onMessage regardless of sender gesture state')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Background Ready')
            """

        let contentScript = """
            browser.test.onMessage.addListener((message) => {
              if (message !== 'Send')
                return

              browser.runtime.sendMessage('check-gesture')
            })

            browser.test.sendMessage('Content Ready')
            """

        try await server.run { configuration in
            let url = configuration.localhostAddress

            let manager = try loadWebExtension(
                manifest: runtimeContentScriptManifest,
                resources: ["background.js": backgroundScript, "content.js": contentScript]
            )
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: url)
            try #require(manager.defaultTab?.webView).load(URLRequest(url: url))

            try await manager.waitForTestMessage("Background Ready")
            try await manager.waitForTestMessage("Content Ready")

            manager.sendTestMessage("Send")

            try await manager.run()
        }
    }

    // MARK: - Connect and Port

    @Test
    func connectFromContentScript() async throws {
        let backgroundScript = """
            browser.runtime?.onConnect.addListener((port) => {
              browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')
              browser.test.assertEq(port?.error, null, 'Port error should be null')

              browser.test.assertEq(typeof port?.sender, 'object', 'sender should be an object')
              browser.test.assertEq(typeof port?.sender?.url, 'string', 'sender.url should be a string')
              browser.test.assertEq(typeof port?.sender?.origin, 'string', 'sender.origin should be a string')
              browser.test.assertTrue(port?.sender?.url?.startsWith('http'), 'sender.url should start with http')
              browser.test.assertTrue(port?.sender?.origin?.startsWith('http'), 'sender.origin should start with http')
              browser.test.assertEq(typeof port?.sender?.tab, 'object', 'sender.tab should be an object')
              browser.test.assertEq(port?.sender?.frameId, 0, 'sender.frameId should be 0')

              port?.onMessage.addListener((message) => {
                browser.test.assertEq(message, 'Hello', 'Should receive the correct message content')
                port?.postMessage('Received')
              })
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            const port = browser.runtime?.connect({ name: 'testPort' })
            browser.test.assertEq(typeof port, 'object', 'Port should be an object')
            browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')
            browser.test.assertEq(port?.sender, null, 'Port sender should be null')

            port?.postMessage('Hello')

            port?.onMessage.addListener((response) => {
              browser.test.assertEq(response, 'Received', 'Should get the response from the background script')

              browser.test.notifyPass()
            })
            """

        try await loadAndRunWithEmptyPage(
            manifest: runtimeContentScriptManifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    // rdar://145509206 https://bugs.webkit.org/show_bug.cgi?id=288410
    #if WTF_PLATFORM_IOS && ASSERT_ENABLED
    @Test(.disabled("rdar://145509206"))
    #else
    @Test
    #endif
    func connectFromContentScriptWithImmediateMessage() async throws {
        let backgroundScript = """
            browser.runtime.onConnect.addListener((port) => {
              browser.test.assertEq(port.name, 'testPort', 'Port name should be testPort')
              browser.test.assertEq(port.error, null, 'Port error should be null')

              port.postMessage('Hello from Background')
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            const port = browser.runtime.connect({ name: 'testPort' })
            browser.test.assertEq(typeof port, 'object', 'Port should be an object')
            browser.test.assertEq(port.name, 'testPort', 'Port name should be testPort')

            port.onMessage.addListener((message) => {
              browser.test.assertEq(message, 'Hello from Background', 'Should receive the correct message content from the background script')

              browser.test.notifyPass()
            })
            """

        try await loadAndRunWithEmptyPage(
            manifest: runtimeContentScriptManifest,
            resources: [
                "background.js": backgroundScript,
                "content.js": contentScript,
            ]
        )
    }

    @Test
    func portMessageWithEmptyContainers() async throws {
        let backgroundScript = """
            browser.runtime.onConnect.addListener((port) => {
              port.onMessage.addListener((message) => {
                const containers = [message.firstArray, message.secondArray, message.firstObject, message.secondObject, message.nested.array, message.nested.object, message.list[0], message.list[1]]
                for (let i = 0; i < containers.length; ++i) {
                  for (let j = i + 1; j < containers.length; ++j)
                    browser.test.assertFalse(containers[i] === containers[j], `Message containers ${i} and ${j} should be distinct`)
                }

                port.postMessage({ firstArray: [], secondArray: [], firstObject: {}, secondObject: {} })
              })
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            const port = browser.runtime.connect({ name: 'testPort' })

            port.onMessage.addListener((response) => {
              browser.test.assertFalse(response.firstArray === response.secondArray, 'Empty arrays in the response should be distinct')
              browser.test.assertFalse(response.firstObject === response.secondObject, 'Empty objects in the response should be distinct')

              browser.test.notifyPass()
            })

            port.postMessage({ firstArray: [], secondArray: [], firstObject: {}, secondObject: {}, nested: { array: [], object: {} }, list: [[], []] })
            """

        try await loadAndRunWithEmptyPage(
            manifest: runtimeContentScriptManifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    @Test
    func connectFromSubframe() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/main", headerFields: ["Content-Type": "text/html"]) {
                "<body><script>"
                    + "  document.write('<iframe src=\"http://127.0.0.1:' + location.port + '/subframe\"></iframe>')"
                    + "</script></body>"
            }
            Route("/subframe", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.runtime.onConnect.addListener((port) => {
              browser.test.assertEq(port.name, 'subframePort', 'Port name should be subframePort')

              port.onMessage.addListener((message) => {
                browser.test.assertEq(message, 'Hello from Subframe', 'Should receive the correct message content from subframe')
                port.postMessage('Received from Background')
              })
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            (async () => {
              const port = browser.runtime.connect({ name: 'subframePort' })
              port.postMessage('Hello from Subframe')

              port.onMessage.addListener((response) => {
                browser.test.assertEq(response, 'Received from Background', 'Should get the response from the background script')

                browser.test.notifyPass()
              })
            })()
            """

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

            "content_scripts": [
                [
                    "matches": ["*://127.0.0.1/*"],
                    "js": ["content.js"],
                    "all_frames": true,
                ]
            ],
        ]

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "content.js": contentScript,
        ]

        try await server.run { configuration in
            let mainURL = configuration.localhostAddress.appending(path: "main")
            let subframeURL = configuration.address.appending(path: "subframe")

            let manager = try loadWebExtension(manifest: manifest, resources: resources)
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: subframeURL)

            try await manager.waitForTestMessage("Load Tab")

            try #require(manager.defaultTab?.webView).load(URLRequest(url: mainURL))

            try await manager.run()
        }
    }

    @Test
    func connectFromContentScriptWithMultipleListeners() async throws {
        let backgroundScript = """
            browser.runtime?.onConnect.addListener((port) => {
              browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort in first listener')
              browser.test.assertEq(port?.error, null, 'Port error should be null in first listener')

              browser.test.assertEq(typeof port?.sender, 'object', 'sender should be an object')
              browser.test.assertEq(typeof port?.sender?.url, 'string', 'sender.url should be a string')
              browser.test.assertEq(typeof port?.sender?.origin, 'string', 'sender.origin should be a string')
              browser.test.assertTrue(port?.sender?.url?.startsWith('http'), 'sender.url should start with http')
              browser.test.assertTrue(port?.sender?.origin?.startsWith('http'), 'sender.origin should start with http')
              browser.test.assertEq(typeof port?.sender?.tab, 'object', 'sender.tab should be an object')
              browser.test.assertEq(port?.sender?.frameId, 0, 'sender.frameId should be 0')

              port?.onMessage.addListener((message) => {
                browser.test.assertEq(message, 'Hello', 'Should receive the correct message content in first listener')
                port?.postMessage('Received')
              })
            })

            browser.runtime?.onConnect.addListener((port) => {
              browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort in second listener')
              browser.test.assertEq(port?.error, null, 'Port error should be null in second listener')

              browser.test.assertEq(typeof port?.sender, 'object', 'sender should be an object')
              browser.test.assertEq(typeof port?.sender?.url, 'string', 'sender.url should be a string')
              browser.test.assertEq(typeof port?.sender?.origin, 'string', 'sender.origin should be a string')
              browser.test.assertTrue(port?.sender?.url?.startsWith('http'), 'sender.url should start with http')
              browser.test.assertTrue(port?.sender?.origin?.startsWith('http'), 'sender.origin should start with http')
              browser.test.assertEq(typeof port?.sender?.tab, 'object', 'sender.tab should be an object')
              browser.test.assertEq(port?.sender?.frameId, 0, 'sender.frameId should be 0')

              port?.onMessage.addListener((message) => {
                browser.test.assertEq(message, 'Hello', 'Should receive the correct message content in second listener')
                port?.postMessage('Received')
              })
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            const port = browser.runtime?.connect({ name: 'testPort' })
            browser.test.assertEq(typeof port, 'object', 'Port should be an object')
            browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')
            browser.test.assertEq(port?.sender, null, 'Port sender should be null')

            port?.postMessage('Hello')

            let receivedMessages = 0
            port?.onMessage.addListener((response) => {
              browser.test.assertEq(response, 'Received', 'Should get the response from the background script')

              if (++receivedMessages === 2)
                browser.test.notifyPass()
            })
            """

        try await loadAndRunWithEmptyPage(
            manifest: runtimeContentScriptManifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    @Test
    func portDisconnectFromContentScript() async throws {
        let backgroundScript = """
            browser.runtime?.onConnect.addListener((port) => {
              browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')

              port?.onMessage.addListener((message) => {
                browser.test.assertEq(message, 'Message from content script to background', 'Should receive the correct message content from the content script')
              })

              port?.onDisconnect.addListener(() => {
                browser.test.assertTrue(true, 'Should trigger the onDisconnect event in the background script')

                browser.test.notifyPass()
              })
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            const port = browser.runtime?.connect({ name: 'testPort' })
            browser.test.assertEq(typeof port, 'object', 'Port should be an object')
            browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')
            browser.test.assertEq(port?.sender, null, 'Port sender should be null')

            port?.postMessage('Message from content script to background')

            port?.onDisconnect.addListener(() => {
              browser.test.assertTrue(false, 'Should not trigger the onDisconnect event for the port that called disconnect()')
            })

            port?.disconnect()
            """

        try await loadAndRunWithEmptyPage(
            manifest: runtimeContentScriptManifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    @Test
    func portDisconnectFromContentScriptWithMultipleListeners() async throws {
        let backgroundScript = """
            browser.runtime?.onConnect.addListener((port) => {
              browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')

              port?.onMessage.addListener((message) => {
                browser.test.assertEq(message, 'Message from content script', 'Should receive the correct message content from content script')

                port?.postMessage('Response from background script 1')
                port?.disconnect()
              })
            })

            browser.runtime?.onConnect.addListener((port) => {
              browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')

              port?.onMessage.addListener((message) => {
                browser.test.assertEq(message, 'Message from content script', 'Should receive the correct message content from content script')

                port?.postMessage('Response from background script 2')
              })
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            let messagesReceived = 0

            const port = browser.runtime?.connect({ name: 'testPort' })
            browser.test.assertEq(typeof port, 'object', 'Port should be an object')
            browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')
            browser.test.assertEq(port?.sender, null, 'Port sender should be null')

            port?.postMessage('Message from content script')

            port?.onMessage.addListener((message) => {
              browser.test.assertTrue(message?.startsWith('Response from background script '), 'Should receive the correct message content')

              if (++messagesReceived === 2)
                browser.test.notifyPass()
            })
            """

        try await loadAndRunWithEmptyPage(
            manifest: runtimeContentScriptManifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    @Test
    func connectFromPopup() async throws {
        let backgroundScript = """
            browser.runtime?.onConnect.addListener((port) => {
              port?.onMessage.addListener((message) => {
                browser.test.assertEq(message, 'Hello from popup', 'Should receive the correct message content')

                port?.postMessage('Received by background')
              })
            })

            browser.action?.openPopup()
            """

        let popupScript = """
            const port = browser.runtime?.connect({ name: 'testPort' })
            browser.test.assertEq(typeof port, 'object', 'Port should be an object')
            browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')
            browser.test.assertEq(port?.sender, null, 'Port sender should be null')

            port?.onMessage.addListener((response) => {
              browser.test.assertEq(response, 'Received by background', 'Should get the response from the background script')

              browser.test.notifyPass()
            })

            port?.postMessage('Hello from popup')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "popup.html": "<script type='module' src='popup.js'></script>",
            "popup.js": popupScript,
        ]

        let manager = try loadWebExtension(manifest: runtimeManifest, resources: resources)

        manager.internalDelegate.presentPopupForAction = { _ in
            // Do nothing so the popup web view will stay loaded.
        }

        try await manager.run()
    }

    @Test
    func sendMessageFromContentScriptInPopupSubframe() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        try await server.run { configuration in
            let url = configuration.localhostAddress
            let urlString = url.absoluteString

            let backgroundScript = """
                const expectedURL = '\(urlString)'

                browser.runtime.onMessage.addListener((message, sender) => {
                  browser.test.assertEq(message?.content, 'Hello from popup subframe', 'Should receive the correct message from the content script')

                  browser.test.assertEq(sender?.tab, undefined, 'sender.tab should be undefined')
                  browser.test.assertEq(sender?.frameId, undefined, 'sender.frameId should be undefined')
                  browser.test.assertEq(sender?.url, expectedURL, 'sender.url should be the expected URL')
                  browser.test.assertEq(typeof sender?.documentId, 'string', 'sender.documentId should be a string')

                  return Promise.resolve({ content: 'Received your message' })
                })

                browser.action.openPopup()
                """

            let contentScript = """
                (async () => {
                  const response = await browser.runtime.sendMessage({ content: 'Hello from popup subframe' })
                  browser.test.assertEq(response?.content, 'Received your message', 'Should get the response from the background script')

                  browser.test.notifyPass()
                })()
                """

            let popupScript = """
                const frame = document.createElement('iframe')
                frame.src = '\(urlString)'
                document.body.appendChild(frame)
                """

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "content.js": contentScript,
                "popup.html": "<script type='module' src='popup.js'></script>",
                "popup.js": popupScript,
            ]

            let manager = try loadWebExtension(manifest: runtimePopupContentScriptManifest, resources: resources)
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: url)

            manager.internalDelegate.presentPopupForAction = { _ in
                // Do nothing so the popup web view will stay loaded.
            }

            try await manager.run()
        }
    }

    @Test
    func connectFromContentScriptInPopupSubframe() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        try await server.run { configuration in
            let url = configuration.localhostAddress
            let urlString = url.absoluteString

            let backgroundScript = """
                const expectedURL = '\(urlString)'

                browser.runtime.onConnect.addListener((port) => {
                  browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')

                  browser.test.assertEq(port?.sender?.tab, undefined, 'sender.tab should be undefined')
                  browser.test.assertEq(port?.sender?.frameId, undefined, 'sender.frameId should be undefined')
                  browser.test.assertEq(port?.sender?.url, expectedURL, 'sender.url should be the expected URL')

                  port?.onMessage.addListener((message) => {
                    browser.test.assertEq(message, 'Hello from popup subframe', 'Should receive the correct message content')

                    port?.postMessage('Received by background')
                  })
                })

                browser.action.openPopup()
                """

            let contentScript = """
                const port = browser.runtime.connect({ name: 'testPort' })

                port.onMessage.addListener((response) => {
                  browser.test.assertEq(response, 'Received by background', 'Should get the response from the background script')

                  browser.test.notifyPass()
                })

                port.postMessage('Hello from popup subframe')
                """

            let popupScript = """
                const frame = document.createElement('iframe')
                frame.src = '\(urlString)'
                document.body.appendChild(frame)
                """

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "content.js": contentScript,
                "popup.html": "<script type='module' src='popup.js'></script>",
                "popup.js": popupScript,
            ]

            let manager = try loadWebExtension(manifest: runtimePopupContentScriptManifest, resources: resources)
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: url)

            manager.internalDelegate.presentPopupForAction = { _ in
                // Do nothing so the popup web view will stay loaded.
            }

            try await manager.run()
        }
    }

    // MARK: - Native Messaging

    @Test
    func sendNativeMessage() async throws {
        let backgroundScript = """
            const response = await browser.test.assertSafeResolve(() => browser.runtime.sendNativeMessage('test', 'Hello'))
            browser.test.assertEq(response, 'Received', 'Should get the response from the native app')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: runtimeManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        context.setPermissionStatus(.grantedExplicitly, for: .nativeMessaging)

        var sentMessages: [(message: Any, applicationIdentifier: String?)] = []

        manager.internalDelegate.sendMessage = { message, applicationIdentifier, replyHandler in
            sentMessages.append((message, applicationIdentifier))

            Task {
                try? await Task.sleep(for: .milliseconds(100))
                replyHandler("Received", nil)
            }
        }

        try await manager.run()

        for sentMessage in sentMessages {
            #expect(sentMessage.applicationIdentifier == "test")
            #expect(sentMessage.message as? String == "Hello")
        }
    }

    @Test
    func sendNativeMessageWithInvalidReply() async throws {
        let backgroundScript = """
            await browser.test.assertRejects(browser.runtime.sendNativeMessage('test', 'Hello'), /reply message was not JSON-serializable/i)

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: runtimeManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        context.setPermissionStatus(.grantedExplicitly, for: .nativeMessaging)

        var sentMessages: [(message: Any, applicationIdentifier: String?)] = []

        manager.internalDelegate.sendMessage = { message, applicationIdentifier, replyHandler in
            sentMessages.append((message, applicationIdentifier))

            Task {
                try? await Task.sleep(for: .milliseconds(100))
                replyHandler(["bad": NSUUID()], nil)
            }
        }

        try await manager.run()

        for sentMessage in sentMessages {
            #expect(sentMessage.applicationIdentifier == "test")
            #expect(sentMessage.message as? String == "Hello")
        }
    }

    @Test
    func connectNative() async throws {
        let backgroundScript = """
            const port = browser.runtime?.connectNative('test')
            browser.test.assertEq(typeof port, 'object', 'Port should be an object')
            browser.test.assertEq(port?.name, 'test', 'Port name should be test')
            browser.test.assertEq(port?.sender, null, 'Port sender should be null')

            let messagesReceived = 0
            port?.onMessage.addListener((response) => {
              browser.test.assertTrue(response?.startsWith('Received '), 'Should get the response from the native code')

              ++messagesReceived
            })

            port?.onDisconnect.addListener(() => {
              if (messagesReceived === 2)
                browser.test.notifyPass()
              else
                browser.test.notifyFail('Not all messages were received')
            })

            port?.postMessage('Hello')
            """

        let manager = try loadWebExtension(manifest: runtimeManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        context.setPermissionStatus(.grantedExplicitly, for: .nativeMessaging)

        var applicationIdentifiers: [String?] = []
        var receivedMessages: [(message: Any?, error: (any Error)?)] = []
        var firstSendErrors: [(any Error)?] = []
        var secondSendErrors: [(any Error)?] = []

        manager.internalDelegate.connectUsingMessagePort = { messagePort in
            applicationIdentifiers.append(messagePort.applicationIdentifier)

            messagePort.messageHandler = { message, error in
                receivedMessages.append((message, error))

                messagePort.sendMessage("Received 1") { error in
                    firstSendErrors.append(error)
                }

                messagePort.sendMessage("Received 2") { error in
                    secondSendErrors.append(error)
                }

                messagePort.disconnect(throwing: nil)
            }
        }

        try await manager.run()

        for applicationIdentifier in applicationIdentifiers {
            #expect(applicationIdentifier == "test")
        }

        for receivedMessage in receivedMessages {
            #expect(receivedMessage.error == nil)
            #expect(receivedMessage.message as? String == "Hello")
        }

        for error in firstSendErrors {
            #expect(error == nil)
        }

        for error in secondSendErrors {
            #expect(error == nil)
        }
    }

    @Test
    func connectNativeWithMessageSentBeforeDelegateCompletes() async throws {
        let backgroundScript = """
            const port = browser.runtime?.connectNative('test')

            port?.onMessage.addListener((response) => {
              browser.test.assertEq(response, 'Launched', 'Should get the message sent while the native port was connecting')
              browser.test.notifyPass()
            })
            """

        let manager = try loadWebExtension(manifest: runtimeManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        context.setPermissionStatus(.grantedExplicitly, for: .nativeMessaging)

        var sendError: (any Error)?

        manager.internalDelegate.connectUsingMessagePortWithCompletionHandler = { messagePort, completionHandler in
            // Synchronous, so the completion handler form of sendMessage isn't called from the task's async context.
            @MainActor
            func sendMessageAndConnect() {
                // The port calls the completion handler before sendMessage returns, and the extension only gets the
                // message once completionHandler(nil) connects the port, so this is recorded before the test can finish.
                messagePort.sendMessage("Launched") { error in
                    sendError = error
                }

                completionHandler(nil)
            }

            // This task runs outside the test, so it only records the send's outcome for the test to check.
            Task {
                // Let the background page register its onMessage listener before the delegate sends.
                try? await Task.sleep(for: .milliseconds(500))

                sendMessageAndConnect()
            }
        }

        try await manager.run()

        #expect(sendError == nil)
    }

    @Test
    func portMutateListenersDuringDispatch() async throws {
        let backgroundScript = """
            const port = browser.runtime?.connectNative('test')

            const initialListenerCount = 64
            let messageDispatchCount = 0
            let disconnectDispatchCount = 0

            port?.onMessage.addListener(() => {
              ++messageDispatchCount
              for (let i = 0; i < 1024; ++i)
                port?.onMessage.addListener(() => { })
            })

            for (let i = 0; i < initialListenerCount; ++i)
              port?.onMessage.addListener(() => { ++messageDispatchCount })

            port?.onDisconnect.addListener(() => {
              ++disconnectDispatchCount
              for (let i = 0; i < 1024; ++i)
                port?.onDisconnect.addListener(() => { })
            })

            for (let i = 0; i < initialListenerCount; ++i)
              port?.onDisconnect.addListener(() => { ++disconnectDispatchCount })

            port?.onDisconnect.addListener(() => {
              ++disconnectDispatchCount
              browser.test.assertEq(messageDispatchCount, initialListenerCount + 1, 'Should dispatch to the listeners registered when onMessage fired')
              browser.test.assertEq(disconnectDispatchCount, initialListenerCount + 2, 'Should dispatch to the listeners registered when onDisconnect fired')
              browser.test.notifyPass()
            })

            port?.postMessage('Hello')
            """

        let manager = try loadWebExtension(manifest: runtimeManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        context.setPermissionStatus(.grantedExplicitly, for: .nativeMessaging)

        manager.internalDelegate.connectUsingMessagePort = { messagePort in
            messagePort.messageHandler = { _, _ in
                messagePort.sendMessage("Received") { _ in }
                messagePort.disconnect(throwing: nil)
            }
        }

        try await manager.run()
    }

    @Test
    func connectNativeWithInvalidMessage() async throws {
        let backgroundScript = """
            const port = browser.runtime?.connectNative('test')
            port?.postMessage('Hello')
            """

        let manager = try loadWebExtension(manifest: runtimeManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        context.setPermissionStatus(.grantedExplicitly, for: .nativeMessaging)

        var applicationIdentifiers: [String?] = []
        var receivedMessages: [(message: Any?, error: (any Error)?)] = []
        var sendErrors: [(any Error)?] = []

        manager.internalDelegate.connectUsingMessagePort = { [weak manager] messagePort in
            applicationIdentifiers.append(messagePort.applicationIdentifier)

            messagePort.messageHandler = { message, error in
                receivedMessages.append((message, error))

                // The port calls the completion handler before sendMessage returns, so this is recorded before done().
                messagePort.sendMessage(["bad": NSUUID()]) { error in
                    sendErrors.append(error)
                }

                messagePort.disconnect()

                manager?.done()
            }
        }

        try await manager.run()

        for applicationIdentifier in applicationIdentifiers {
            #expect(applicationIdentifier == "test")
        }

        for receivedMessage in receivedMessages {
            #expect(receivedMessage.error == nil)
            #expect(receivedMessage.message as? String == "Hello")
        }

        for error in sendErrors {
            #expect(error != nil)
        }
    }

    @Test
    func connectNativeWithUndefinedMessage() async throws {
        let backgroundScript = """
            const port = browser.runtime?.connectNative('test')
            browser.test.assertEq(typeof port, 'object', 'Port should be an object')
            browser.test.assertEq(port?.name, 'test', 'Port name should be test')
            browser.test.assertEq(port?.sender, null, 'Port sender should be null')

            port?.onMessage.addListener((response) => {
              browser.test.assertEq(response, undefined, 'Response should be undefined when sending null message')

              browser.test.notifyPass()
            })

            port?.postMessage('Hello')
            """

        let manager = try loadWebExtension(manifest: runtimeManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        context.setPermissionStatus(.grantedExplicitly, for: .nativeMessaging)

        var applicationIdentifiers: [String?] = []
        var receivedMessages: [(message: Any?, error: (any Error)?)] = []
        var sendErrors: [(any Error)?] = []

        manager.internalDelegate.connectUsingMessagePort = { [weak manager] messagePort in
            applicationIdentifiers.append(messagePort.applicationIdentifier)

            messagePort.messageHandler = { message, error in
                receivedMessages.append((message, error))

                // The port calls the completion handler before sendMessage returns, so this is recorded before done().
                messagePort.sendMessage(nil) { error in
                    sendErrors.append(error)
                }

                messagePort.disconnect(throwing: nil)

                manager?.done()
            }
        }

        try await manager.run()

        for applicationIdentifier in applicationIdentifiers {
            #expect(applicationIdentifier == "test")
        }

        for receivedMessage in receivedMessages {
            #expect(receivedMessage.error == nil)
            #expect(receivedMessage.message as? String == "Hello")
        }

        for error in sendErrors {
            #expect(error == nil)
        }
    }

    // MARK: - Lifecycle and Options Page

    @Test
    func reload() async throws {
        let backgroundScript = """
            browser.test.sendMessage('Loaded')
            browser.runtime.reload()
            """

        let manager = try loadWebExtension(manifest: runtimeManifest, resources: ["background.js": backgroundScript])

        try await manager.waitForTestMessage("Loaded")
    }

    @Test
    func startupEvent() async throws {
        let backgroundScript = """
            browser.runtime.onStartup.addListener(() => {
              browser.test.sendMessage('Startup Event Fired')
            })

            setTimeout(() => {
              browser.test.sendMessage('Startup Event Did Not Fire')
            }, 1000)
            """

        let manager = try loadWebExtension(manifest: runtimeManifest, resources: ["background.js": backgroundScript])

        try await manager.waitForTestMessage("Startup Event Fired")

        manager.unload()
        try await Task.sleep(for: .seconds(5))
        try manager.checkCollectedFailures()

        manager.load()
        try await manager.waitForTestMessage("Startup Event Did Not Fire")
    }

    @Test
    func installedEvent() async throws {
        let backgroundScript = """
            browser.runtime.onInstalled.addListener((details) => {
              browser.test.assertEq(details.reason, 'install')

              browser.test.sendMessage('Installed Event Fired')
            })

            setTimeout(() => {
              browser.test.sendMessage('Installed Event Did Not Fire')
            }, 1000)
            """

        let manager = parseWebExtension(manifest: runtimeManifest, resources: ["background.js": backgroundScript])

        // Wait until after startup event time expires.
        try await Task.sleep(for: .seconds(5))
        try manager.checkCollectedFailures()

        manager.load()
        try await manager.waitForTestMessage("Installed Event Fired")

        manager.unload()
        try await Task.sleep(for: .seconds(1))
        try manager.checkCollectedFailures()

        manager.load()
        try await manager.waitForTestMessage("Installed Event Fired")
    }

    @Test
    func openOptionsPage() async throws {
        let backgroundScript = """
            await browser.test.assertSafeResolve(() => browser.runtime.openOptionsPage())
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "options.html": "Hello world!",
        ]

        let manager = try loadWebExtension(manifest: runtimeManifest, resources: resources)

        var optionsPageOpened = false
        var optionsPageURLs: [(actual: URL?, expected: URL?)] = []

        manager.internalDelegate.openOptionsPage = { [weak manager] _, completionHandler in
            optionsPageOpened = true

            let context = manager?.context
            optionsPageURLs.append((context?.optionsPageURL, URL(string: "options.html", relativeTo: context?.baseURL)?.absoluteURL))

            completionHandler(nil)

            manager?.done()
        }

        try await manager.run()

        for optionsPageURL in optionsPageURLs {
            #expect(optionsPageURL.actual == optionsPageURL.expected)
        }

        #expect(optionsPageOpened)
    }

    @Test
    func openOptionsPageActivatesExistingTab() async throws {
        let backgroundScript = """
            const [originalTab] = await browser.tabs.query({ active: true, currentWindow: true })

            await browser.runtime.openOptionsPage()

            const [optionsTab] = await browser.tabs.query({ active: true, currentWindow: true })
            browser.test.assertEq((await browser.tabs.query({ })).length, 2, 'The options page should open in a new tab')

            await browser.tabs.update(originalTab.id, { active: true })
            await browser.runtime.openOptionsPage()

            const [activeTab] = await browser.tabs.query({ active: true, currentWindow: true })
            browser.test.assertEq(activeTab.id, optionsTab.id, 'The existing options page tab should be activated')
            browser.test.assertEq((await browser.tabs.query({ })).length, 2, 'A second options page tab should not be opened')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "options.html": "Hello world!",
        ]

        let manager = try loadWebExtension(manifest: runtimeManifest, resources: resources)

        var windowFocused = false
        try #require(manager.defaultWindow).didFocus = {
            windowFocused = true
        }

        try await manager.run()

        #expect(windowFocused)
    }

    @Test
    func openOptionsPageMatchesExistingTabIgnoringFragment() async throws {
        let backgroundScript = """
            await browser.tabs.create({ url: browser.runtime.getURL('options.html?section=general'), active: false })
            const fragmentTab = await browser.tabs.create({ url: browser.runtime.getURL('options.html#general'), active: false })

            await browser.runtime.openOptionsPage()

            const [activeTab] = await browser.tabs.query({ active: true, currentWindow: true })
            browser.test.assertEq(activeTab.id, fragmentTab.id, 'The tab that differs only by fragment should be activated')
            browser.test.assertEq((await browser.tabs.query({ })).length, 3, 'A new options page tab should not be opened')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(
            manifest: runtimeManifest,
            resources: [
                "background.js": backgroundScript,
                "options.html": "Hello world!",
            ]
        )
        try await manager.run()
    }

    // MARK: - Externally Connectable Web Pages

    @Test
    func connectFromWebPage() async throws {
        let uniqueIdentifier = "org.webkit.test.extension (76C788B8)"

        let webpageScript = """
            <script>
            const port = browser?.runtime?.connect('\(uniqueIdentifier)', { name: 'testPort' })
            browser.test.assertEq(typeof port, 'object', 'Port should be an object')
            browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')
            browser.test.assertEq(port?.sender, null, 'Port sender should be null')

            port?.postMessage('Hello')

            port?.onMessage.addListener((response) => {
              browser.test.assertEq(response, 'Received')
              port?.postMessage('Success')
            })
            </script>
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                webpageScript
            }
        }

        let backgroundScript = """
            browser.runtime?.onConnectExternal.addListener((port) => {
              browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')
              browser.test.assertEq(port?.error, null, 'Port error should be null')

              browser.test.assertEq(typeof port?.sender, 'object', 'sender should be an object')
              browser.test.assertEq(typeof port?.sender?.url, 'string', 'sender.url should be a string')
              browser.test.assertEq(typeof port?.sender?.origin, 'string', 'sender.origin should be a string')
              browser.test.assertTrue(port?.sender?.url?.startsWith('http'), 'sender.url should start with http')
              browser.test.assertTrue(port?.sender?.origin?.startsWith('http'), 'sender.origin should start with http')
              browser.test.assertEq(typeof port?.sender?.tab, 'object', 'sender.tab should be an object')
              browser.test.assertEq(port?.sender?.frameId, 0, 'sender.frameId should be 0')

              port?.onMessage.addListener((message) => {
                if (message == 'Hello')
                  port?.postMessage('Received')
                else if (message == 'Success')
                  browser.test.notifyPass()
              })
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let url = configuration.localhostAddress

            let manager = parseWebExtension(manifest: externallyConnectableManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: url)

            // Set an uniqueIdentifier so it is a known value and not the default random one.
            context.uniqueIdentifier = uniqueIdentifier

            manager.load()
            try await manager.waitForTestMessage("Load Tab")

            try #require(manager.defaultTab?.webView).load(URLRequest(url: url))

            try await manager.run()
        }
    }

    @Test
    func connectFromWebPageWithImmediateMessage() async throws {
        let uniqueIdentifier = "org.webkit.test.extension (76C788B8)"

        let webpageScript = """
            <script>
            const port = browser?.runtime?.connect('\(uniqueIdentifier)', { name: 'testPort' })
            console.assert(typeof port === 'object', 'Port should be an object')
            console.assert(port?.name === 'testPort', 'Port name should be testPort')
            console.assert(port?.sender === null, 'Port sender should be null')

            port?.onMessage.addListener((message) => {
              console.assert(message === 'Hello from Background', 'Should receive the correct message content')
              browser.test.notifyPass()
            })
            </script>
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                webpageScript
            }
        }

        let backgroundScript = """
            browser.runtime?.onConnectExternal.addListener((port) => {
              browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')
              browser.test.assertEq(port?.error, null, 'Port error should be null')

              port?.postMessage('Hello from Background')
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let url = configuration.localhostAddress

            let manager = parseWebExtension(manifest: externallyConnectableManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: url)

            // Set an uniqueIdentifier so it is a known value and not the default random one.
            context.uniqueIdentifier = uniqueIdentifier

            manager.load()
            try await manager.waitForTestMessage("Load Tab")

            try #require(manager.defaultTab?.webView).load(URLRequest(url: url))

            try await manager.run()
        }
    }

    @Test
    func connectFromWebPageWithWrongIdentifier() async throws {
        let backgroundScript = """
            browser.runtime.onMessageExternal.addListener((message, sender, sendResponse) => {
              browser.test.notifyFail('The page should not be able to connect')
            })

            browser.test.sendMessage('Load Tab')
            """

        let webpageScript = """
            <script>
              const startTime = performance.now()

              let port = browser.runtime.connect('org.webkit.test.extension (Incorrect)', { name: 'testPort' })
              browser.test.assertTrue(!!port, 'Port should be returned even with a wrong extension identifier')

              port.onDisconnect.addListener((port) => {
                browser.test.assertTrue((performance.now() - startTime) >= 100, 'Disconnect should happen after a delay of at least 100ms')

                browser.test.notifyPass()
              })
            </script>
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                webpageScript
            }
        }

        try await server.run { configuration in
            let url = configuration.localhostAddress

            let manager = try loadWebExtension(manifest: externallyConnectableManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: url)

            try await manager.waitForTestMessage("Load Tab")

            try #require(manager.defaultTab?.webView).load(URLRequest(url: url))

            try await manager.run()
        }
    }

    @Test
    func sendMessageFromWebPage() async throws {
        let uniqueIdentifier = "org.webkit.test.extension (SendMessageTest)"

        let webpageScript = """
            <script>
            browser.runtime.sendMessage('\(uniqueIdentifier)', 'Hello', (response) => {
              browser.test.assertEq(response, 'Received')

              browser.test.notifyPass()
            })
            </script>
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                webpageScript
            }
        }

        let backgroundScript = """
            browser.runtime.onMessageExternal.addListener((message, sender, sendResponse) => {
              browser.test.assertEq(message, 'Hello', 'Should receive the correct message')

              browser.test.assertEq(typeof sender, 'object', 'sender should be an object')
              browser.test.assertEq(typeof sender?.url, 'string', 'sender.url should be a string')
              browser.test.assertEq(typeof sender?.origin, 'string', 'sender.origin should be a string')

              browser.test.assertTrue(sender?.url?.startsWith('http'), 'sender.url should start with http')
              browser.test.assertTrue(sender?.origin?.startsWith('http'), 'sender.origin should start with http')

              browser.test.assertEq(typeof sender?.tab, 'object', 'sender.tab should be an object')

              browser.test.assertEq(sender?.frameId, 0, 'sender.frameId should be 0')

              browser.test.assertEq(typeof sender?.documentId, 'string', 'sender.documentId should be')
              browser.test.assertEq(sender?.documentId?.length, 36, 'sender.documentId.length should be')

              sendResponse('Received')
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let url = configuration.localhostAddress

            let manager = parseWebExtension(manifest: externallyConnectableManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: url)

            // Set an uniqueIdentifier so it is a known value and not the default random one.
            context.uniqueIdentifier = uniqueIdentifier

            manager.load()
            try await manager.waitForTestMessage("Load Tab")

            try #require(manager.defaultTab?.webView).load(URLRequest(url: url))

            try await manager.run()
        }
    }

    @Test
    func sendMessageFromWebPageWithTabFrameAndAsyncReply() async throws {
        let extensionManifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Runtime Test",
            "description": "Runtime Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "content_scripts": [
                [
                    "js": ["content.js"],
                    "matches": ["*://localhost/*"],
                ]
            ],

            "web_accessible_resources": [
                [
                    "resources": ["*.html"],
                    "matches": ["*://localhost/*"],
                ]
            ],

            "externally_connectable": [
                "matches": ["*://localhost/*"]
            ],
        ]

        let backgroundScript = """
            browser.runtime.onMessageExternal.addListener((message, sender, sendResponse) => {
              browser.test.assertEq(message?.content, 'Hello from webpage', 'Should receive the correct message from the web page')

              sendResponse({ content: 'Async reply from background' })
            })

            browser.test.sendMessage('Load Tabs')
            """

        let iframeScript = """
            browser.runtime.onMessageExternal.addListener((message, sender, sendResponse) => {
              browser.test.assertEq(message?.content, 'Hello from webpage', 'Should receive the correct message from the web page')
            })
            """

        let contentScript = """
            (function() {
              const iframe = document.createElement('iframe')
              iframe.src = browser.runtime.getURL('extension-frame.html')
              document.body.appendChild(iframe)
            })()
            """

        let webpageScript = """
            <script>
            browser.runtime.sendMessage('org.webkit.test.extension (SendMessageTest)', { content: 'Hello from webpage' }, (response) => {
              browser.test.assertEq(response?.content, 'Async reply from background', 'Should receive the correct reply from the extension frame')

              browser.test.notifyPass()
            })
            </script>
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                webpageScript
            }
            Route("/second-tab.html", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "content.js": contentScript,
            "extension-frame.js": iframeScript,
            "extension-frame.html": "<script type='module' src='extension-frame.js'></script>",
        ]

        try await server.run { configuration in
            let url = configuration.localhostAddress

            let manager = parseWebExtension(manifest: extensionManifest, resources: resources)
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: url)

            // Set an uniqueIdentifier so it is a known value and not the default random one.
            context.uniqueIdentifier = "org.webkit.test.extension (SendMessageTest)"

            manager.load()
            try await manager.waitForTestMessage("Load Tabs")

            try #require(manager.defaultTab?.webView).load(URLRequest(url: url))

            let secondTab = try #require(manager.defaultWindow).openNewTab()
            try #require(secondTab.webView).load(URLRequest(url: url.appending(path: "second-tab.html")))

            try await manager.run()
        }
    }

    @Test
    func sendMessageFromWebPageWithWrongIdentifier() async throws {
        let backgroundScript = """
            browser.runtime.onMessageExternal.addListener(async (message, sender, sendResponse) => {
              browser.test.notifyFail('The page should not be able to send a message with a wrong identifier')
            })

            browser.test.sendMessage('Load Tab')
            """

        let webpageScript = """
            <script type='module'>
              const startTime = performance.now()

              const response = await browser.runtime.sendMessage('org.webkit.test.extension (Incorrect)', 'Hello')
              browser.test.assertEq(response, undefined, 'The response should be undefined')

              browser.test.assertTrue((performance.now() - startTime) >= 100, 'Should take at least 100ms to attempt the message send')

              browser.test.notifyPass()
            </script>
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                webpageScript
            }
        }

        try await server.run { configuration in
            let url = configuration.localhostAddress

            let manager = try loadWebExtension(manifest: externallyConnectableManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: url)

            try await manager.waitForTestMessage("Load Tab")

            try #require(manager.defaultTab?.webView).load(URLRequest(url: url))

            try await manager.run()
        }
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
