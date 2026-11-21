// Copyright (C) 2023-2026 Apple Inc. All rights reserved.
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
private import WebKit_Private._WKWebExtensionContext
private import WebKit_Private.WKProcessPoolPrivate
private import WebKit_Private.WKUserContentControllerPrivate
private import WebKit_Private.WKWebExtensionContextPrivate
private import WebKit_Private.WKWebExtensionControllerConfigurationPrivate
private import WebKit_Private.WKWebViewPrivate

import struct Foundation.URL
import struct Swift.String

@MainActor
struct WKWebExtensionAPIScriptingTests {
    private let scriptingManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "Scripting Test",
        "description": "Scripting Test",
        "version": "1",

        "permissions": ["scripting", "webNavigation"],
        "host_permissions": ["*://localhost/*"],

        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],
    ]

    private let scriptingActionManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "Scripting Test",
        "description": "Scripting Test",
        "version": "1",

        "permissions": ["scripting"],
        "host_permissions": ["*://localhost/*"],

        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],

        "action": [String: Any](),
    ]

    private let activeTabContentScriptsManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "ActiveTab Content Scripts Test",
        "description": "ActiveTab Content Scripts Test",
        "version": "1.0",

        "permissions": ["activeTab"],

        "content_scripts": [
            [
                "matches": ["*://localhost/*"],
                "js": ["content.js"],
                "css": ["content.css"],
                "run_at": "document_idle",
            ]
        ],
    ]

    private let activeTabContentScriptsResources: [String: Any] = [
        "content.js": "document.body.dataset.scriptInjected = 'true'",
        "content.css": "body { background-color: rgb(255, 0, 0) !important; }",
    ]

    private let changeBackgroundColorScript = "document.body.style.background = 'pink'"
    private let changeBackgroundFontScript = "document.body.style.fontSize = '555px'"

    /// The JavaScript that reads a page's ``ContentScriptState``, as a dictionary with the same keys.
    private let contentScriptStateScript =
        "({ scriptInjected: document.body.dataset.scriptInjected === 'true', backgroundColor: getComputedStyle(document.body).backgroundColor })"

    /// Whether the active tab content script ran in a page, and the page's background color, which its style sheet sets.
    private struct ContentScriptState {
        let scriptInjected: Bool
        let backgroundColor: String
    }

    /// Reads a page's ``ContentScriptState``.
    private func contentScriptState(
        _ webView: WKWebView,
        fileID: String = #fileID,
        filePath: String = #filePath,
        line: Int = #line,
        column: Int = #column
    ) async throws -> ContentScriptState {
        // Evaluate without a user gesture, as objectByEvaluatingJavaScript: does.
        let result = try await webView._evaluateJavaScriptWithoutUserGesture(contentScriptStateScript)
        let state = try requireNonNil(result as? [String: Any], fileID: fileID, filePath: filePath, line: line, column: column)
        let scriptInjected = try requireNonNil(
            state["scriptInjected"] as? Bool,
            fileID: fileID,
            filePath: filePath,
            line: line,
            column: column
        )
        let backgroundColor = try requireNonNil(
            state["backgroundColor"] as? String,
            fileID: fileID,
            filePath: filePath,
            line: line,
            column: column
        )

        return ContentScriptState(scriptInjected: scriptInjected, backgroundColor: backgroundColor)
    }

    /// How many times the content script ran in a page, which it counts in the body's dataset.
    private func contentScriptRunCount(_ webView: WKWebView) async throws -> String? {
        // Evaluate without a user gesture, as objectByEvaluatingJavaScript: does.
        try await webView._evaluateJavaScriptWithoutUserGesture("document.body.dataset.runCount") as? String
    }

    /// What a permission match patterns notification said: the match patterns, and the tab if the change was only for one.
    private struct MatchPatternsNotificationInfo: Sendable {
        let matchPatterns: Set<WKWebExtension.MatchPattern>?
        let tab: ObjectIdentifier?

        init(_ notification: Notification) {
            matchPatterns =
                notification.userInfo?[_WKWebExtensionContextNotificationUserInfoKeyMatchPatterns] as? Set<WKWebExtension.MatchPattern>
            tab = notification.userInfo?[_WKWebExtensionContextNotificationUserInfoKeyTab].map { ObjectIdentifier($0 as AnyObject) }
        }
    }

    /// Collects the notifications an observer receives on the main queue.
    @MainActor
    private final class MatchPatternsNotificationRecorder {
        private(set) var notifications: [MatchPatternsNotificationInfo] = []

        func record(_ notification: MatchPatternsNotificationInfo) {
            notifications.append(notification)
        }
    }

    /// Records each match patterns notification with the given name that the context posts, until the returned observer is removed.
    private func observeMatchPatternsNotifications(
        named name: Notification.Name,
        object context: WKWebExtensionContext,
        into recorder: MatchPatternsNotificationRecorder
    ) -> any NSObjectProtocol {
        NotificationCenter.default.addObserver(forName: name, object: context, queue: .main) { notification in
            // Read the user info here, since the notification itself can't be sent to the main actor.
            let info = MatchPatternsNotificationInfo(notification)
            MainActor.assumeIsolated {
                recorder.record(info)
            }
        }
    }

    private func loadAndRun(backgroundScript: String) async throws {
        let manager = try loadWebExtension(manifest: scriptingManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func errorsExecuteScript() async throws {
        try await loadAndRun(
            backgroundScript: """
                browser.test.assertThrows(() => browser.scripting.executeScript(), /a required argument is missing/i)
                browser.test.assertThrows(() => browser.scripting.executeScript({}), /missing required keys: 'target'./i)
                browser.test.assertThrows(() => browser.scripting.executeScript({'target' : { }}), /missing required keys: 'tabId'./i)
                browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 'bad'}}), /'tabId' is expected to be a number, but a string was provided./i)

                browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 1 }, 'func': () => { console.log('function') }, 'function': () => {console.log('function')}}), /it cannot specify both 'func' and 'function'. Please use 'func'./i)
                browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 1 }, args: ['args'], func: () => 'function', arguments: ['arguments']}), /it cannot specify both 'args' and 'arguments'. Please use 'args'./i)
                browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 1 }, args: ['args'], files: ['path/to/file']}), /it must specify both 'func' and 'args'./i)

                browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 1 }, args: 0, func: () => 'function' }), /'args' is expected to be an array, but a number was provided./i)
                browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 1 }, func: () => 'function', args: [ () => 'arguments' ] }), /it is not JSON-serializable./i)

                const notAFunction = null
                browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 1 }, func: 'not a function' }), /is expected to be a value, but a string was provided./i)

                browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 1 }}), /it must specify either 'func' or 'files'./i)
                browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 1 }, args: ['args'], files: [0]}), /'files' is expected to be an array of strings, but a number was provided./i)

                browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 1, allFrames: true, frameIds: [0] }, files: ['path/to/file']}), /it cannot specify both 'allFrames' and 'frameIds'./i)
                browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 1, frameIds: ['0'] }, files: ['path/to/file']}), /'frameIds' is expected to be an array of numbers, but a string was provided./i)
                browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 1, frameIds: [-1] }, files: ['path/to/file']}), /'-1' is not a frame identifier./i)

                browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 1 }, world: 'world', files: ['path/to/file']}), /it must specify either 'isolated' or 'main'./i)

                browser.test.notifyPass()
                """
        )
    }

    @Test
    func errorsCSS() async throws {
        try await loadAndRun(
            backgroundScript: """
                browser.test.assertThrows(() => browser.scripting.insertCSS(), /a required argument is missing./i)
                browser.test.assertThrows(() => browser.scripting.insertCSS({}), /missing required keys: 'target'./i)
                browser.test.assertThrows(() => browser.scripting.insertCSS({ target: {} }), /missing required keys: 'tabId'./i)
                browser.test.assertThrows(() => browser.scripting.insertCSS({target: { tabId: 1 }, files: ['path/to/file'], css: 'css'}), /it cannot specify both 'css' and 'files'./i)
                browser.test.assertThrows(() => browser.scripting.insertCSS({target: { tabId: 1 }}), /it must specify either 'css' or 'files'./i)
                browser.test.assertThrows(() => browser.scripting.insertCSS({target: { tabId: '1' }, files: ['path/to/file'], css: 'css'}), /'tabId' is expected to be a number, but a string was provided./i)
                browser.test.assertThrows(() => browser.scripting.insertCSS({target: { tabId: 1 }, css: 'body { color: red }', origin: 'bad' }), /'origin' value is invalid, because it must specify either 'AUTHOR' or 'USER'/i);

                browser.test.assertThrows(() => browser.scripting.removeCSS(), /a required argument is missing./i)
                browser.test.assertThrows(() => browser.scripting.removeCSS({}), /missing required keys: 'target'./i)
                browser.test.assertThrows(() => browser.scripting.removeCSS({ target: {} }), /missing required keys: 'tabId'./i)
                browser.test.assertThrows(() => browser.scripting.removeCSS({target: { tabId: 1 }, files: ['path/to/file'], css: 'css'}), /it cannot specify both 'css' and 'files'./i)
                browser.test.assertThrows(() => browser.scripting.removeCSS({target: { tabId: 1 } }), /it must specify either 'css' or 'files'./i)
                browser.test.assertThrows(() => browser.scripting.removeCSS({target: { tabId: 'bad' }, files: ['path/to/file'], css: 'css'}), /'tabId' is expected to be a number, but a string was provided./i)

                browser.test.notifyPass()
                """
        )
    }

    @Test
    func errorsRegisteredContentScript() async throws {
        try await loadAndRun(
            backgroundScript: """
                browser.test.assertThrows(() => browser.scripting.registerContentScripts(), /a required argument is missing/i)
                browser.test.assertThrows(() => browser.scripting.registerContentScripts({}), /an array is expected/i)
                browser.test.assertThrows(() => browser.scripting.registerContentScripts([{}]), /it is missing required keys: 'id'/i)

                browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: 0 }]), /'id' is expected to be a string, but a number was provided/i)
                browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: '' }]), /it must not be empty/i)
                browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: '_0' }]), /it must not start with '_'/i)

                browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: '0' }]), /it must specify at least one match pattern for script with ID '0'/i)
                browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: '0', matches: [] }]), /it must specify at least one match pattern for script with ID '0'/i)

                browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: '0', matches: [ '*://*/*' ] }]), /it must specify at least one 'css' or 'js' file/i)
                browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: '0', matches: [ '*://*/*' ], js: ['path/to/file'], runAt: 'invalid_runAt' }]), /it must specify either 'document_start', 'document_end', or 'document_idle'/i)
                browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: '0', matches: [ '*://*/*' ], js: ['path/to/file'], world: 'invalid_world' }]), /it must specify either 'isolated' or 'main'/i)
                browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: '0', matches: [ '*://*/*' ], css: ['style.css'], cssOrigin: 'bad' }]), /'cssOrigin' value is invalid, because it must specify either 'author' or 'user'/i)

                browser.test.assertThrows(() => browser.scripting.updateContentScripts([{ id: '_0' }]), /it must not start with '_'/i)
                browser.test.assertThrows(() => browser.scripting.updateContentScripts([{ id: '0', matches: [ ], js: ['path/to/file'] }]), /it must not be empty/i)
                browser.test.assertThrows(() => browser.scripting.updateContentScripts([{ id: '0', matches: [ '*://*/*' ], js: ['path/to/file'], world: 'invalid_world' }]), /it must specify either 'isolated' or 'main'/i)

                browser.test.assertThrows(() => browser.scripting.getRegisteredContentScripts([]), /an object is expected/i)
                browser.test.assertThrows(() => browser.scripting.getRegisteredContentScripts({ ids: 0 }), /'ids' is expected to be an array of strings, but a number was provided/i)
                browser.test.assertThrows(() => browser.scripting.getRegisteredContentScripts({ ids: [ 0 ] }), /'ids' is expected to be an array of strings, but a number was provided/i)

                browser.test.assertThrows(() => browser.scripting.unregisterContentScripts([]), /an object is expected/i)
                browser.test.assertThrows(() => browser.scripting.unregisterContentScripts({ ids: 0 }), /'ids' is expected to be an array of strings, but a number was provided/i)
                browser.test.assertThrows(() => browser.scripting.unregisterContentScripts({ ids: [ 0 ] }), /'ids' is expected to be an array of strings, but a number was provided/i)

                await browser.test.assertRejects(browser.scripting.registerContentScripts([{ id: '1', matches: ['*://*/*'], js: ['invalidFile.js'] }]), /Invalid resource 'invalidFile.js'/i)

                browser.test.notifyPass()
                """
        )
    }

    @Test
    func executeScript() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webNavigation.onCompleted.addListener(async (details) => {
              const tabId = details.tabId

              const blueValue = 'rgb(0, 0, 255)'

              function changeBackgroundColor(color) { document.body.style.background = color }
              function getBackgroundColor() { return window.getComputedStyle(document.body).getPropertyValue('background-color') }

              let results = await browser.scripting.executeScript({ target: { tabId, allFrames: false }, files: [ 'backgroundColor.js' ] })
              browser.test.assertEq(results?.[0]?.result, 'pink', 'Result should be')
              browser.test.assertEq(results?.[0]?.frameId, 0, 'Frame should be')

              results = await browser.scripting.executeScript({ target: { tabId, frameIds: [ 0 ] }, files: [ 'backgroundColor.js' ] })
              browser.test.assertEq(results?.[0]?.result, 'pink', 'Result should be')
              browser.test.assertEq(results?.[0]?.frameId, 0, 'Frame should be')

              results = await browser.scripting.executeScript({ target: { tabId }, files: [ 'backgroundColor.js'] })
              browser.test.assertEq(results?.[0]?.result, 'pink', 'Result should be')
              browser.test.assertEq(results?.[0]?.frameId, 0, 'Frame should be')

              results = await browser.scripting.executeScript({ target: { tabId, frameIds: [ 0 ] }, func: changeBackgroundColor, args: ['pink'] })
              browser.test.assertEq(results?.[0]?.result, null, 'Result should be')
              browser.test.assertEq(results?.[0]?.frameId, 0, 'Frame should be')

              results = await browser.scripting.executeScript({
                target: { tabId },
                func: (bool, number, string, dict, array) => {
                  browser.test.assertEq(typeof bool, 'boolean', 'Boolean argument should be')
                  browser.test.assertEq(typeof number, 'number', 'Number argument should be')
                  browser.test.assertEq(typeof string, 'string', 'String argument should be')
                  browser.test.assertEq(typeof dict, 'object', 'Object argument should be')
                  browser.test.assertTrue(Array.isArray(array), 'Array argument should be an array')
                },
                args: [true, 10, 'string', {}, []]
              })
              browser.test.assertEq(results?.[0]?.result, null, 'Result should be')
              browser.test.assertEq(results?.[0]?.frameId, 0, 'Frame should be')

              await browser.scripting.executeScript({ target: { tabId, allFrames: true }, func: changeBackgroundColor, args: ['blue'] })
              results = await browser.scripting.executeScript({ target: { tabId, allFrames: true }, func: getBackgroundColor })
              browser.test.assertEq(results?.[0]?.result, blueValue, 'Result should be')
              browser.test.assertEq(results?.[0]?.frameId, 0, 'Frame should be')

              await browser.test.assertSafeResolve(() => browser.scripting.executeScript({ target: { tabId, frameIds: [0], allFrames: false }, func: () => {} }))

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let backgroundColor = "document.body.style.background = 'pink'"

        let resources: [String: Any] = ["background.js": backgroundScript, "backgroundColor.js": backgroundColor]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: scriptingManifest, resources: resources)
            let context = try requireNonNil(manager.context)

            let url = configuration.localhostAddress

            let matchPattern = try requireNonNil(WKWebExtension.MatchPattern.cachedPattern(matchingAllPathsOf: url))
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)
            context.setPermissionStatus(.grantedExplicitly, for: matchPattern)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: url))

            try await manager.run()
        }
    }

    @Test
    func executeScriptJSONTypes() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webNavigation.onCompleted.addListener(async (details) => {
              const tabId = details.tabId

              const expectedResult = { 'boolean': true, 'number': 42, 'string': 'Test String', 'object': { 'key': 'value' }, 'array': [1, 2, 3] }

              function returnJSONTypes() {
                return { 'boolean': true, 'number': 42, 'string': 'Test String', 'object': { 'key': 'value' }, 'array': [1, 2, 3] };
              }

              const results = await browser.scripting.executeScript({
                target: { tabId: tabId, allFrames: false },
                func: returnJSONTypes
              });

              browser.test.assertDeepEq(results?.[0]?.result, expectedResult);

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: scriptingManifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func executeScriptResultDoesNotAliasEmptyContainers() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webNavigation.onCompleted.addListener(async (details) => {
              function returnEmptyContainers() {
                return { firstArray: [], secondArray: [], firstObject: {}, secondObject: {}, nested: { array: [], object: {} }, list: [[], []] }
              }

              const results = await browser.scripting.executeScript({ target: { tabId: details.tabId }, func: returnEmptyContainers })
              const result = results?.[0]?.result

              const containers = [result.firstArray, result.secondArray, result.firstObject, result.secondObject, result.nested.array, result.nested.object, result.list[0], result.list[1]]
              for (let i = 0; i < containers.length; ++i) {
                for (let j = i + 1; j < containers.length; ++j)
                  browser.test.assertFalse(containers[i] === containers[j], `Result containers ${i} and ${j} should be distinct`)
              }

              result.firstArray.push(1)
              browser.test.assertEq(result.secondArray.length, 0, 'Mutating one empty array should not change the other')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: scriptingManifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func executeScriptWithFrameIds() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                " "
            }
        }

        let backgroundScript = """
            browser.runtime.onMessage.addListener(async (message, sender) => {
              browser.test.assertEq(message, 'Hello from frame')

              const frameId = sender.frameId
              browser.test.assertTrue(frameId !== 0, 'frameId should not be 0 for an iframe')

              const result = await browser.scripting.executeScript({
                target: { tabId: sender.tab.id, frameIds: [frameId] },
                func: () => location.pathname
              })

              browser.test.assertEq(result[0].result, '/frame.html', 'Should execute in the iframe and return the correct path')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            browser.runtime.sendMessage('Hello from frame')
            """

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Scripting API Test",
            "description": "Scripting API Test",
            "version": "1.0",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
            ],

            "content_scripts": [
                [
                    "js": ["content.js"],
                    "matches": ["*://localhost/frame.html"],
                    "all_frames": true,
                ]
            ],

            "permissions": ["scripting"],
        ]

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "content.js": contentScript,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: resources)
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress.appending(path: "frame.html"))

            try await manager.waitForTestMessage("Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func executeScriptWithDocumentIds() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<span>Main Document</span><iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<span>Frame Document</span>"
            }
        }

        let backgroundScript = """
            browser.runtime.onMessage.addListener(async (message, sender) => {
              browser.test.assertEq(message, 'Hello from frame')

              const documentId = sender?.documentId
              browser.test.assertEq(typeof documentId, 'string', 'sender.documentId should be')
              browser.test.assertEq(documentId.length, 36, 'sender.documentId should be')

              const result = await browser.scripting.executeScript({
                target: { tabId: sender?.tab?.id, documentIds: [ documentId ] },
                func: () => document.body.innerText.trim()
              })

              browser.test.assertEq(result?.[0]?.result, 'Frame Document', 'Result should be')
              browser.test.assertEq(typeof result?.[0]?.documentId, 'string', 'Result documentId should be')
              browser.test.assertEq(result?.[0]?.documentId.length, 36, 'Result documentId should be')
              browser.test.assertEq(result?.[0]?.documentId, documentId, 'Result documentId should be')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            browser.runtime.sendMessage('Hello from frame')
            """

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Scripting API Test",
            "description": "Scripting API Test",
            "version": "1.0",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
            ],

            "content_scripts": [
                [
                    "js": ["content.js"],
                    "matches": ["*://localhost/frame.html"],
                    "all_frames": true,
                ]
            ],

            "permissions": ["scripting"],
        ]

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "content.js": contentScript,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: resources)
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress.appending(path: "frame.html"))

            try await manager.waitForTestMessage("Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func executeScriptWithUserGesture() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.action.setPopup({ popup: '' })

            browser.tabs.onUpdated.addListener((tabId, changeInfo) => {
              if (changeInfo.status === 'complete')
                browser.test.sendMessage('Page Ready')
            })

            browser.action.onClicked.addListener(async (tab) => {
              browser.test.assertTrue(navigator.userActivation.isActive, 'User gesture should be active at the scripting.executeScript call site')

              function checkUserGesture() { return navigator.userActivation.isActive }

              const results = await browser.test.assertSafeResolve(() => browser.scripting.executeScript({
                target: { tabId: tab.id },
                func: checkUserGesture
              }))

              browser.test.assertTrue(results?.[0]?.result, 'Injected script should run with an active user gesture when executeScript is called from a user action')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Background Ready')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: scriptingActionManifest, resources: ["background.js": backgroundScript])

            try await manager.waitForTestMessage("Background Ready")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.waitForTestMessage("Page Ready")

            let context = try requireNonNil(manager.context)
            context.performAction(for: manager.defaultTab)

            try await manager.run()
        }
    }

    @Test
    func executeScriptWithoutUserGesture() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.tabs.onUpdated.addListener(async (tabId, changeInfo) => {
              if (changeInfo.status !== 'complete')
                return

              function checkUserGesture() { return navigator.userActivation.isActive }

              const results = await browser.test.assertSafeResolve(() => browser.scripting.executeScript({
                target: { tabId },
                func: checkUserGesture
              }))

              browser.test.assertFalse(results?.[0]?.result, 'Injected script should not have an active user gesture when executeScript is called without a prior user action')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Background Ready')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: scriptingActionManifest, resources: ["background.js": backgroundScript])

            try await manager.waitForTestMessage("Background Ready")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func insertAndRemoveCSS() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<body style='background-color: blue'></body>"
            }
        }

        let backgroundScript = """
            const pinkValue = 'rgb(255, 192, 203)'
            const blueValue = 'rgb(0, 0, 255)'
            const transparentValue = 'rgba(0, 0, 0, 0)'

            browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {
              if (tab.status !== 'complete')
                return

              function getBackgroundColor() { return window.getComputedStyle(document.body).getPropertyValue('background-color') }
              function getFontSize() { return window.getComputedStyle(document.body).getPropertyValue('font-size') }

              await browser.scripting.insertCSS( { target: { tabId: tabId, allFrames: false }, files: [ 'backgroundColor.css' ] })
              let results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })
              browser.test.assertEq(results[0].result, pinkValue)
              browser.test.assertEq(results[1].result, blueValue)

              await browser.scripting.removeCSS( { target: { tabId: tabId, allFrames: false }, files: [ 'backgroundColor.css' ] })
              results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })
              browser.test.assertEq(results[0].result, transparentValue)
              browser.test.assertEq(results[1].result, blueValue)

              await browser.scripting.insertCSS( { target: { tabId: tabId, allFrames: false }, css: 'body { background-color: pink !important }' })
              results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })
              browser.test.assertEq(results[0].result, pinkValue)
              browser.test.assertEq(results[1].result, blueValue)

              await browser.scripting.removeCSS( { target: { tabId: tabId, allFrames: false }, css: 'body { background-color: pink !important }' })
              results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })
              browser.test.assertEq(results[0].result, transparentValue)
              browser.test.assertEq(results[1].result, blueValue)

              await browser.scripting.insertCSS( { target: { tabId: tabId, allFrames: true }, files: [ 'backgroundColor.css' ] })
              results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })
              browser.test.assertEq(results[0].result, pinkValue)
              browser.test.assertEq(results[1].result, pinkValue)

              await browser.scripting.removeCSS( { target: { tabId: tabId, allFrames: true }, files: [ 'backgroundColor.css' ] })
              results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })
              browser.test.assertEq(results[0].result, transparentValue)
              browser.test.assertEq(results[1].result, blueValue)

              // Storing original font size.
              results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: false }, func: getFontSize })
              let originalFontSize = results[0].result

              await browser.scripting.insertCSS( { target: { tabId: tabId, allFrames: true }, files: [ 'backgroundColor.css', 'fontSize.css' ] })
              results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })
              browser.test.assertEq(results[0].result, pinkValue)
              browser.test.assertEq(results[1].result, pinkValue)

              results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getFontSize })
              browser.test.assertEq(results[0].result, '104px')
              browser.test.assertEq(results[1].result, '104px')

              await browser.scripting.removeCSS( { target: { tabId: tabId, allFrames: true }, files: [ 'backgroundColor.css', 'fontSize.css' ] })
              results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })
              browser.test.assertEq(results[0].result, transparentValue)
              browser.test.assertEq(results[1].result, blueValue)

              results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getFontSize })
              browser.test.assertEq(results[0].result, originalFontSize)
              browser.test.assertEq(results[1].result, originalFontSize)

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let backgroundColor = "body { background-color: pink !important }"
        let fontSize = "body { font-size: 104px }"

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "backgroundColor.css": backgroundColor,
            "fontSize.css": fontSize,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: scriptingManifest, resources: resources)
            let context = try requireNonNil(manager.context)

            let url = configuration.localhostAddress

            let matchPattern = try requireNonNil(WKWebExtension.MatchPattern.cachedPattern(matchingAllPathsOf: url))
            context.setPermissionStatus(.grantedExplicitly, for: matchPattern)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: url))

            try await manager.run()
        }
    }

    // The main frame is served from 127.0.0.1 and the subframe from localhost, so with site isolation the
    // subframe gets its own web content process.
    @Test
    func insertCSSInAllFramesWithCrossSiteFrame() async throws {
        let previousShouldEnableSiteIsolation = TestWebExtensionManager.shouldEnableSiteIsolation
        TestWebExtensionManager.shouldEnableSiteIsolation = true
        defer { TestWebExtensionManager.shouldEnableSiteIsolation = previousShouldEnableSiteIsolation }

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Scripting Test",
            "description": "Scripting Test",
            "version": "1",

            "permissions": ["scripting"],
            "host_permissions": ["*://localhost/*", "*://127.0.0.1/*"],

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<script>onload = () => { const frame = document.createElement('iframe'); frame.src = 'http://localhost:' + window.location.port + '/frame.html'; document.body.appendChild(frame); }</script>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<body style='background-color: blue'></body>"
            }
        }

        let backgroundScript = """
            const pinkValue = 'rgb(255, 192, 203)'
            const blueValue = 'rgb(0, 0, 255)'

            function getBackgroundColor() { return window.getComputedStyle(document.body).getPropertyValue('background-color') }

            function readAllFrames(tabId) {
              return browser.scripting.executeScript({ target: { tabId, allFrames: true }, func: getBackgroundColor })
            }

            // The cross-site subframe is not loaded yet when the main frame reaches 'complete'.
            async function readBothFrames(tabId) {
              for (let attempt = 0; attempt < 200; ++attempt) {
                const results = await readAllFrames(tabId)
                if (results.length === 2)
                  return results
                await new Promise((resolve) => setTimeout(resolve, 50))
              }
              browser.test.notifyFail('Timed out waiting for the cross-site subframe to appear')
            }

            browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {
              if (tab.status !== 'complete')
                return

              let results = await readBothFrames(tabId)
              browser.test.assertTrue(results.some((result) => result.result === blueValue), 'The cross-site subframe should start out blue')

              await browser.scripting.insertCSS({ target: { tabId, allFrames: true }, css: 'body { background-color: pink !important }' })

              results = await readAllFrames(tabId)
              browser.test.assertEq(results.length, 2, 'Both frames should be reachable')

              for (const result of results)
                browser.test.assertEq(result.result, pinkValue, `Frame ${result.frameId} should be styled by insertCSS with allFrames`)

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)

            let url = configuration.address

            for permittedURL in [url, configuration.localhostAddress] {
                let matchPattern = try requireNonNil(WKWebExtension.MatchPattern.cachedPattern(matchingAllPathsOf: permittedURL))
                context.setPermissionStatus(.grantedExplicitly, for: matchPattern)
            }

            try await manager.waitForTestMessage("Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: url))

            try await manager.run()
        }
    }

    @Test
    func insertAndRemoveCSSWithFrameIds() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<body style='background-color: blue'></body>"
            }
        }

        let backgroundScript = """
            const pinkValue = 'rgb(255, 192, 203)'
            const blueValue = 'rgb(0, 0, 255)'
            const transparentValue = 'rgba(0, 0, 0, 0)'

            browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {
              if (tab.status !== 'complete')
                return

              function getBackgroundColor() { return window.getComputedStyle(document.body).getPropertyValue('background-color') }
              function getFontSize() { return window.getComputedStyle(document.body).getPropertyValue('font-size') }

              function logMessage() { console.log('Logging message') }

              await browser.scripting.insertCSS( { target: { tabId: tabId, frameIds: [ 0 ] }, files: [ 'backgroundColor.css' ] })
              let results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })
              browser.test.assertEq(results[0].result, pinkValue)
              browser.test.assertEq(results[1].result, blueValue)

              await browser.scripting.removeCSS( { target: { tabId: tabId, frameIds: [ 0 ] }, files: [ 'backgroundColor.css' ] })
              results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })
              browser.test.assertEq(results[0].result, transparentValue)
              browser.test.assertEq(results[1].result, blueValue)

              await browser.scripting.insertCSS( { target: { tabId: tabId, frameIds: [ 0 ] }, css: 'body { background-color: pink !important }' })
              results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })
              browser.test.assertEq(results[0].result, pinkValue)
              browser.test.assertEq(results[1].result, blueValue)

              await browser.scripting.removeCSS( { target: { tabId: tabId, frameIds: [ 0 ] }, css: 'body { background-color: pink !important }' })
              results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })
              browser.test.assertEq(results[0].result, transparentValue)
              browser.test.assertEq(results[1].result, blueValue)

              // FIXME: <https://webkit.org/b/262491> Test with subframe once there's support for style injections for specific frame Ids.

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let backgroundColor = "body { background-color: pink !important }"
        let fontSize = "body { font-size: 104px }"

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "backgroundColor.css": backgroundColor,
            "fontSize.css": fontSize,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: scriptingManifest, resources: resources)
            let context = try requireNonNil(manager.context)

            let url = configuration.localhostAddress

            let matchPattern = try requireNonNil(WKWebExtension.MatchPattern.cachedPattern(matchingAllPathsOf: url))
            context.setPermissionStatus(.grantedExplicitly, for: matchPattern)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: url))

            try await manager.run()
        }
    }

    @Test
    func cssUserOrigin() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<body style='color: red'></body>"
            }
        }

        let backgroundScript = """
            browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {
              if (tab.status === 'complete') {
                await browser.test.assertSafeResolve(() => browser.scripting.insertCSS({ target: { tabId }, css: 'body { color: green !important }', origin: 'user' }))

                let results = await browser.test.assertSafeResolve(() => browser.scripting.executeScript({ target: { tabId }, func: () => getComputedStyle(document.body).color }))
                browser.test.assertEq(results[0]?.result, 'rgb(0, 128, 0)', 'Color should be green')

                browser.test.notifyPass()
              }
            })

            browser.test.sendMessage('Load Tab')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: scriptingManifest, resources: resources)
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func cssAuthorOrigin() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<style> body { color: red } </style>"
            }
        }

        let backgroundScript = """
            browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {
              if (tab.status === 'complete') {
                await browser.test.assertSafeResolve(() => browser.scripting.insertCSS({ target: { tabId }, css: 'body { color: green !important }', origin: 'author' }))

                let results = await browser.test.assertSafeResolve(() => browser.scripting.executeScript({ target: { tabId }, func: () => getComputedStyle(document.body).color }))
                browser.test.assertEq(results[0]?.result, 'rgb(0, 128, 0)', 'Color should be green')

                browser.test.notifyPass()
              }
            })

            browser.test.sendMessage('Load Tab')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: scriptingManifest, resources: resources)
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func executionWorld() async throws {
        try await loadAndRun(
            backgroundScript: """
                browser.test.assertEq(typeof browser.scripting.ExecutionWorld, 'object', 'ExecutionWorld should be an object')

                browser.test.assertEq(browser.scripting.ExecutionWorld.ISOLATED, 'ISOLATED', 'ISOLATED should be defined')
                browser.test.assertEq(browser.scripting.ExecutionWorld.MAIN, 'MAIN', 'MAIN should be defined')

                browser.test.assertDeepEq(Object.keys(browser.scripting.ExecutionWorld).sort(), [ 'ISOLATED', 'MAIN' ], 'ExecutionWorld should only define ISOLATED and MAIN')

                browser.test.notifyPass()
                """
        )
    }

    @Test
    func world() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<script> const world = 'MAIN'; </script>"
            }
        }

        let backgroundScript = """
            function testWorld() {
              if (world)
                browser.test.notifyPass()
              else
                throw 'Variable not defined'
            }

            browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {
              if (tab.status === 'complete') {
                var results = await browser.scripting.executeScript( { target: { tabId, world: 'ISOLATED'}, func: testWorld })
                browser.test.assertEq(results[0]?.error, 'A JavaScript exception occurred')

                await browser.scripting.executeScript( { target: { tabId, world: 'MAIN'}, func: testWorld })

                browser.test.notifyPass()
              }
            })

            browser.test.sendMessage('Load Tab')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: scriptingManifest, resources: resources)
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func registerContentScripts() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            function getBackgroundColor() { return window.getComputedStyle(document.body).getPropertyValue('background-color') }

            function getFontSize() { return window.getComputedStyle(document.body).getPropertyValue('font-size') }

              const pinkValue = 'rgb(255, 192, 203)'

            browser.webNavigation.onCompleted.addListener(async (details) => {
              const tabId = details.tabId

              var expectedResults = [{
                id: '1',
                js: ['changeBackgroundColorScript.js', 'changeBackgroundFontScript.js'],
                matches: ['*://localhost/*'],
                persistAcrossSessions: true,
              }]

              var results
              var resultsPassingIds

              await browser.scripting.registerContentScripts([{ id: '1', matches: ['*://localhost/*'], js: ['changeBackgroundColorScript.js', 'changeBackgroundFontScript.js'] }])
              results = await browser.scripting.getRegisteredContentScripts()
              resultsPassingIds = await browser.scripting.getRegisteredContentScripts({ ids: ['1'] })

              browser.test.assertDeepEq(results, resultsPassingIds)
              browser.test.assertDeepEq(results, expectedResults)

              // Verify scripts injected.

              results = await browser.scripting.executeScript({ target: { tabId: tabId }, func: getBackgroundColor })
              browser.test.assertEq(results[0].result, pinkValue)

              results = await browser.scripting.executeScript({ target: { tabId: tabId }, func: getFontSize })
              browser.test.assertEq(results[0].result, '555px')

              await browser.scripting.unregisterContentScripts()

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "changeBackgroundColorScript.js": changeBackgroundColorScript,
            "changeBackgroundFontScript.js": changeBackgroundFontScript,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: scriptingManifest, resources: resources)
            let context = try requireNonNil(manager.context)

            let url = configuration.localhostAddress

            let matchPattern = try requireNonNil(WKWebExtension.MatchPattern.cachedPattern(matchingAllPathsOf: url))
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)
            context.setPermissionStatus(.grantedExplicitly, for: matchPattern)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: url))

            try await manager.run()
        }
    }

    @Test
    func registerContentScriptsWithCSSUserOrigin() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<body style='color: red'></body>"
            }
        }

        let backgroundScript = """
            browser.webNavigation.onCompleted.addListener(async (details) => {
              const tabId = details.tabId

              await browser.scripting.registerContentScripts([{
                id: 'changeColor',
                matches: ['*://localhost/*'],
                css: ['changeColor.css'],
                cssOrigin: 'user'
              }])

              let results = await browser.test.assertSafeResolve(() =>
                browser.scripting.executeScript({
                  target: { tabId: tabId },
                  func: () => getComputedStyle(document.body).color
                })
              )

              browser.test.assertEq(results[0].result, 'rgb(0, 128, 0)', 'Color should be green')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let css = "body { color: green !important }"

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "changeColor.css": css,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: scriptingManifest, resources: resources)
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func registerContentScriptsWithCSSAuthorOrigin() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<style> body { color: red } </style>"
            }
        }

        let backgroundScript = """
            browser.webNavigation.onCompleted.addListener(async (details) => {
              const tabId = details.tabId

              await browser.scripting.registerContentScripts([{
                id: 'changeColor',
                matches: ['*://localhost/*'],
                css: ['changeColor.css'],
                cssOrigin: 'user'
              }])

              let results = await browser.test.assertSafeResolve(() =>
                browser.scripting.executeScript({
                  target: { tabId: tabId },
                  func: () => getComputedStyle(document.body).color
                })
              )

              browser.test.assertEq(results[0].result, 'rgb(0, 128, 0)', 'Color should be green')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let css = "body { color: green !important }"

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "changeColor.css": css,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: scriptingManifest, resources: resources)
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func registerContentScriptsMatchOriginAsFallback() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='about:blank'></iframe>"
            }
        }

        let backgroundScript = """
            await browser.scripting.registerContentScripts([{
                id: 'applyStylesAndScript',
                matches: ['*://localhost/*'],
                matchOriginAsFallback: true,
                allFrames: true,
                js: ['content.js'],
                css: ['applyStyles.css']
            }])

            browser.test.sendMessage('Scripts Registered');
            """

        let contentStyle = "body { background-color: green !important }"

        let contentScript = """
            browser.test.assertEq(document.body.dataset.injected, undefined, 'Script should not have run before')
            document.body.dataset.injected = 'true'

            const bgColor = getComputedStyle(document.body).backgroundColor
            browser.test.assertEq(bgColor, 'rgb(0, 128, 0)', 'CSS should be applied correctly')

            browser.test.sendMessage(window.top === window ? 'Main Frame Injected' : 'Sub-Frame Injected')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "content.js": contentScript,
            "applyStyles.css": contentStyle,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: scriptingManifest, resources: resources)
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Scripts Registered")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.waitForTestMessage("Main Frame Injected")
            try await manager.waitForTestMessage("Sub-Frame Injected")
        }
    }

    @Test
    func updateContentScripts() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<body style='background-color: blue'></body>"
            }
        }

        let backgroundScript = """
            function getBackgroundColor() { return window.getComputedStyle(document.body).getPropertyValue('background-color') }

            function getFontSize() { return window.getComputedStyle(document.body).getPropertyValue('font-size') }

              const pinkValue = 'rgb(255, 192, 203)'

            browser.webNavigation.onCompleted.addListener(async (details) => {
              const tabId = details.tabId

              var expectedResults = [{
                id: '1',
                js: ['changeBackgroundColorScript.js'],
                matches: ['*://localhost/*'],
                persistAcrossSessions: true,
              }]

              var results

              await browser.scripting.registerContentScripts([{ id: '1', matches: ['*://localhost/*'], js: ['changeBackgroundColorScript.js'] }])

              await browser.scripting.updateContentScripts([{ id: '1', allFrames: true, persistAcrossSessions: false, runAt: 'document_start', world: 'MAIN' }])
              results = await browser.scripting.getRegisteredContentScripts()
              expectedResults[0].allFrames = true
              expectedResults[0].persistAcrossSessions = false
              expectedResults[0].runAt = 'document_start'
              expectedResults[0].world = 'main'

              browser.test.assertDeepEq(results, expectedResults)

              await browser.scripting.updateContentScripts([{ id: '1', js: ['changeBackgroundColorScript.js', 'changeBackgroundFontScript.js'] }])
              expectedResults[0].js = ['changeBackgroundColorScript.js', 'changeBackgroundFontScript.js']
              results = await browser.scripting.getRegisteredContentScripts()

              browser.test.assertDeepEq(results, expectedResults)

              // Verify scripts injected.

              results = await browser.scripting.executeScript({ target: { tabId: tabId, allFrames:true }, func: getBackgroundColor })
              browser.test.assertEq(results[0].result, pinkValue)
              browser.test.assertEq(results[1].result, pinkValue)

              results = await browser.scripting.executeScript({ target: { tabId: tabId, allFrames:true }, func: getFontSize })
              browser.test.assertEq(results[0].result, '555px')
              browser.test.assertEq(results[1].result, '555px')

              await browser.scripting.unregisterContentScripts()

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "changeBackgroundColorScript.js": changeBackgroundColorScript,
            "changeBackgroundFontScript.js": changeBackgroundFontScript,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: scriptingManifest, resources: resources)
            let context = try requireNonNil(manager.context)

            let url = configuration.localhostAddress

            let matchPattern = try requireNonNil(WKWebExtension.MatchPattern.cachedPattern(matchingAllPathsOf: url))
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)
            context.setPermissionStatus(.grantedExplicitly, for: matchPattern)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: url))

            try await manager.run()
        }
    }

    @Test
    func updateContentScriptsWithMinimalParametersShouldNotCrash() async throws {
        let backgroundScript = """
            await browser.scripting.registerContentScripts([{ id: '1', matches: ['*://localhost/*'], js: ['script.js'] }])
            await browser.scripting.updateContentScripts([{ id: '1', allFrames: true }])

            const results = await browser.scripting.getRegisteredContentScripts()
            browser.test.assertDeepEq(results, [{ id: '1', matches: ['*://localhost/*'], js: ['script.js'], allFrames: true, persistAcrossSessions: true }])

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "script.js": "document.body.style.backgroundColor = 'red'",
        ]

        let manager = try loadWebExtension(manifest: scriptingManifest, resources: resources)
        try await manager.run()
    }

    @Test
    func getContentScripts() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webNavigation.onCompleted.addListener(async () => {

              var expectedResults = [{
                id: '1',
                js: ['changeBackgroundColorScript.js'],
                matches: ['*://localhost/*'],
                persistAcrossSessions: true,
              }]

              var results

              await browser.scripting.registerContentScripts([{ id: '1', matches: ['*://localhost/*'], js: ['changeBackgroundColorScript.js'] }])

              results = await browser.scripting.getRegisteredContentScripts()
              browser.test.assertDeepEq(results, expectedResults)

              results = await browser.scripting.getRegisteredContentScripts({ 'ids': ['1'] })
              browser.test.assertDeepEq(results, expectedResults)

              // Unrecognized ids should be ignored.
              results = await browser.scripting.getRegisteredContentScripts({ 'ids': ['1', '2'] })
              browser.test.assertDeepEq(results, expectedResults)

              await browser.scripting.registerContentScripts([{ id: '2', matches: ['*://localhost/*'], js: ['changeBackgroundFontScript.js'], runAt: 'document_start', world: 'MAIN' }])

              results = await browser.scripting.getRegisteredContentScripts({ 'ids': ['1', '2'] })
              expectedResults.push({ id: '2', js: ['changeBackgroundFontScript.js'], matches: ['*://localhost/*'], persistAcrossSessions: true, runAt: 'document_start', world: 'main' })
              browser.test.assertDeepEq(results, expectedResults)

              await browser.scripting.unregisterContentScripts()

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "changeBackgroundColorScript.js": changeBackgroundColorScript,
            "changeBackgroundFontScript.js": changeBackgroundFontScript,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: scriptingManifest, resources: resources)
            let context = try requireNonNil(manager.context)

            let url = configuration.localhostAddress

            let matchPattern = try requireNonNil(WKWebExtension.MatchPattern.cachedPattern(matchingAllPathsOf: url))
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)
            context.setPermissionStatus(.grantedExplicitly, for: matchPattern)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: url))

            try await manager.run()
        }
    }

    @Test
    func unregisterContentScripts() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            function getBackgroundColor() { return window.getComputedStyle(document.body).getPropertyValue('background-color') }

              const pinkValue = 'rgb(255, 192, 203)'
              const transparentValue = 'rgba(0, 0, 0, 0)'

              browser.webNavigation.onCompleted.addListener(async (details) => {
                const tabId = details.tabId

                await browser.scripting.registerContentScripts([{ id: '1', matches: ['*://localhost/*'], js: ['changeBackgroundColorScript.js'] }])

                var results
                results = await browser.scripting.getRegisteredContentScripts()

                browser.test.assertEq(results.length, 1)

                await browser.scripting.unregisterContentScripts()
                results = await browser.scripting.getRegisteredContentScripts()
                browser.test.assertEq(results.length, 0)

                // Unrecognized ids should return an error and result in a no-op.

                await browser.scripting.registerContentScripts([{ id: '1', matches: ['*://localhost/*'], js: ['changeBackgroundColorScript.js'] }])
                await browser.test.assertRejects(browser.scripting.unregisterContentScripts({ 'ids': ['1', '2'] }))

                results = await browser.scripting.executeScript({ target: { tabId: tabId }, func: getBackgroundColor })
                browser.test.assertEq(results[0].result, pinkValue)

                // Tests removal with multiple ids.

                await browser.scripting.registerContentScripts([{ id: '2', matches: ['*://localhost/*'], js: ['changeBackgroundFontScript.js'] }])
                results = await browser.scripting.getRegisteredContentScripts()
                browser.test.assertEq(results.length, 2)

                await browser.scripting.unregisterContentScripts({ 'ids': ['1', '2'] })
                results = await browser.scripting.getRegisteredContentScripts()
                browser.test.assertEq(results.length, 0)

                browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "changeBackgroundColorScript.js": changeBackgroundColorScript,
            "changeBackgroundFontScript.js": changeBackgroundFontScript,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: scriptingManifest, resources: resources)
            let context = try requireNonNil(manager.context)

            let url = configuration.localhostAddress

            let matchPattern = try requireNonNil(WKWebExtension.MatchPattern.cachedPattern(matchingAllPathsOf: url))
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)
            context.setPermissionStatus(.grantedExplicitly, for: matchPattern)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: url))

            try await manager.run()
        }
    }

    @Test
    func registeredScriptIsInjectedAfterContextReloads() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webNavigation.onCompleted.addListener(async (details) => {
              const pinkValue = 'rgb(255, 192, 203)'
              function getBackgroundColor() { return window.getComputedStyle(document.body).getPropertyValue('background-color') }

              let results = await browser.scripting.executeScript({ target: { tabId: details.tabId, allFrames: false }, func: getBackgroundColor })
              browser.test.assertEq(results?.[0]?.result, pinkValue)

              browser.test.notifyPass()
            })

            let registeredScripts = await browser.scripting.getRegisteredContentScripts()
            if (!registeredScripts.length) {
              await browser.scripting.registerContentScripts([{ id: '1', matches: [ '*://localhost/*' ], js: [ 'changeBackgroundColorScript.js' ] }])

              registeredScripts = await browser.scripting.getRegisteredContentScripts()
              browser.test.assertEq(registeredScripts.length, 1)

              browser.test.sendMessage('Unload extension')
            } else {
              browser.test.assertEq(registeredScripts.length, 1)

              browser.test.sendMessage('Load Tab')
            }
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "changeBackgroundColorScript.js": changeBackgroundColorScript,
        ]

        try await server.run { configuration in
            let manager = parseWebExtension(manifest: scriptingManifest, resources: resources, configuration: ._temporary())
            let context = try requireNonNil(manager.context)

            // Give the extension a unique identifier so it opts into saving data in the temporary configuration.
            context.uniqueIdentifier = "org.webkit.test.extension (76C788B8)"

            expectFalse(context.hasInjectedContent)

            manager.load()
            try await manager.waitForTestMessage("Unload extension")

            expectTrue(context.hasInjectedContent)

            manager.unload()

            expectFalse(context.hasInjectedContent)

            manager.load()
            try await manager.waitForTestMessage("Load Tab")

            expectTrue(context.hasInjectedContent)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func persistentRegisteredScriptIsInjectedAfterContextReloads() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webNavigation.onCompleted.addListener(async (details) => {
              const pinkValue = 'rgb(255, 192, 203)'
              function getBackgroundColor() { return window.getComputedStyle(document.body).getPropertyValue('background-color') }

              let results = await browser.scripting.executeScript({ target: { tabId: details.tabId, allFrames: false }, func: getBackgroundColor })
              browser.test.assertEq(results?.[0]?.result, pinkValue)

              browser.test.notifyPass()
            })

            let registeredScripts = await browser.scripting.getRegisteredContentScripts()
            if (!registeredScripts.length) {
              await browser.scripting.registerContentScripts([{ id: '1', matches: [ '*://localhost/*' ], js: [ 'changeBackgroundColorScript.js' ], 'allFrames': true, 'persistAcrossSessions': true }])

              registeredScripts = await browser.scripting.getRegisteredContentScripts()
              browser.test.assertEq(registeredScripts.length, 1)

              browser.test.sendMessage('Unload extension')
            } else {
              browser.test.assertEq(registeredScripts.length, 1)

              browser.test.sendMessage('Load Tab')
            }
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "changeBackgroundColorScript.js": changeBackgroundColorScript,
        ]

        try await server.run { configuration in
            let manager = parseWebExtension(manifest: scriptingManifest, resources: resources, configuration: ._temporary())
            let context = try requireNonNil(manager.context)

            // Give the extension a unique identifier so it opts into saving data in the temporary configuration.
            context.uniqueIdentifier = "org.webkit.test.extension (76C788B8)"

            expectFalse(context.hasInjectedContent)

            manager.load()
            try await manager.waitForTestMessage("Unload extension")

            expectTrue(context.hasInjectedContent)

            manager.unload()

            expectFalse(context.hasInjectedContent)

            manager.load()
            try await manager.waitForTestMessage("Load Tab")

            expectTrue(context.hasInjectedContent)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func registeredScriptExcludeMatches() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webNavigation.onCompleted.addListener(async () => {

              var expectedResults = [{
                id: '1',
                js: ['changeBackgroundColorScript.js'],
                matches: ['*://*/*'],
                persistAcrossSessions: true,
                excludeMatches: ['*://*.example.com/*']
              }]

              await browser.scripting.registerContentScripts([{ id: '1', matches: ['*://*/*'], js: ['changeBackgroundColorScript.js'], excludeMatches: ['*://*.example.com/*']}])

              var results = await browser.scripting.getRegisteredContentScripts()
              browser.test.assertDeepEq(results, expectedResults)

              results = await browser.scripting.getRegisteredContentScripts({ 'ids': ['1'] })
              browser.test.assertDeepEq(results, expectedResults)

              await browser.scripting.unregisterContentScripts()

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "changeBackgroundColorScript.js": changeBackgroundColorScript,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: scriptingManifest, resources: resources)
            let testContext = try requireNonNil(manager.context)

            // Confirm that the script can't be injected on example.com.
            let exampleURL = try requireNonNil(URL(string: "https://example.com/"))
            expectFalse(testContext.hasInjectedContent(for: exampleURL))

            let url = configuration.localhostAddress

            let matchPattern = try requireNonNil(WKWebExtension.MatchPattern.cachedPattern(matchingAllPathsOf: url))
            testContext.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)
            testContext.setPermissionStatus(.grantedExplicitly, for: matchPattern)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: url))

            try await manager.run()
        }
    }

    @Test
    func injectedOnlyOnce() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='about:blank'></iframe>"
            }
        }

        let contentScriptsManifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Scripting Test",
            "description": "Scripting Test",
            "version": "1.0",

            "content_scripts": [
                [
                    "matches": ["*://localhost/*"],
                    "js": ["content_script.js"],
                ]
            ],
        ]

        let contentScript = """
            if (document.body.dataset.injected)
                browser.test.notifyFail('Script injected more than once')

            document.body.dataset.injected = 'true'

            browser.test.sendMessage('script-injected')
            """

        let resources: [String: Any] = [
            "content_script.js": contentScript
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: contentScriptsManifest, resources: resources)
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.waitForTestMessage("script-injected")

            try await Task.sleep(for: .seconds(3))
            try manager.checkCollectedFailures()
        }
    }

    @Test
    func mainWorld() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<script>window.secretValue = 'This is a secret.'</script>"
            }
        }

        let contentScriptsManifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Scripting Test",
            "description": "Scripting Test",
            "version": "1.0",

            "content_scripts": [
                [
                    "matches": ["*://localhost/*"],
                    "js": ["content_script.js"],
                    "world": "MAIN",
                ]
            ],
        ]

        let contentScript = """
            browser.test.assertEq(window.secretValue, 'This is a secret.')

            // Externally connectable exposes a limited browser but not chrome.
            browser.test.assertEq(window.chrome, undefined)
            browser.test.assertEq(typeof window.browser?.runtime, 'object')
            browser.test.assertEq(window.browser?.runtime?.id, undefined)

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "content_script.js": contentScript
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: contentScriptsManifest, resources: resources)
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func isolatedWorld() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<script>window.secretValue = 'This is a secret.'</script>"
            }
        }

        let contentScriptsManifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Scripting Test",
            "description": "Scripting Test",
            "version": "1.0",

            "content_scripts": [
                [
                    "matches": ["*://localhost/*"],
                    "js": ["content_script.js"],
                    "world": "ISOLATED",
                ]
            ],
        ]

        let contentScript = """
            browser.test.assertEq(window.secretValue, undefined)

            // Both chrome and browser should be exposed.
            browser.test.assertEq(typeof window.chrome?.runtime?.id, 'string')
            browser.test.assertEq(typeof window.browser?.runtime?.id, 'string')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "content_script.js": contentScript
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: contentScriptsManifest, resources: resources)
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func matchAboutBlank() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='about:blank'></iframe>"
            }
        }

        let contentScriptsManifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Scripting Test",
            "description": "Scripting Test",
            "version": "1.0",

            "content_scripts": [
                [
                    "matches": ["*://localhost/*"],
                    "match_about_blank": true,
                    "all_frames": true,
                    "js": ["content.js"],
                    "css": ["content.css"],
                ]
            ],
        ]

        let contentStyle = "body { background-color: red !important; }"

        let contentScript = """
            browser.test.assertEq(document.body.dataset.injected, undefined, 'Script should not have run before')
            document.body.dataset.injected = 'true'

            const bgColor = getComputedStyle(document.body).backgroundColor
            browser.test.assertEq(bgColor, 'rgb(255, 0, 0)', 'CSS should be applied correctly')

            browser.test.sendMessage(window.top === window ? 'Main Frame Injected' : 'Sub-Frame Injected')
            """

        let resources: [String: Any] = [
            "content.js": contentScript,
            "content.css": contentStyle,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: contentScriptsManifest, resources: resources)
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.waitForTestMessage("Main Frame Injected")
            try await manager.waitForTestMessage("Sub-Frame Injected")
        }
    }

    @Test
    func matchOriginAsFallbackWithAboutBlank() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='about:blank'></iframe>"
            }
        }

        let contentScriptsManifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Scripting Test",
            "description": "Scripting Test",
            "version": "1.0",

            "content_scripts": [
                [
                    "matches": ["*://localhost/*"],
                    "match_origin_as_fallback": true,
                    "all_frames": true,
                    "js": ["content.js"],
                    "css": ["content.css"],
                ]
            ],
        ]

        let contentStyle = "body { background-color: green !important; }"

        let contentScript = """
            browser.test.assertEq(document.body.dataset.injected, undefined, 'Script should not have run before')
            document.body.dataset.injected = 'true'

            const bgColor = getComputedStyle(document.body).backgroundColor
            browser.test.assertEq(bgColor, 'rgb(0, 128, 0)', 'CSS should be applied correctly')

            browser.test.sendMessage(window.top === window ? 'Main Frame Injected' : 'Sub-Frame Injected')
            """

        let resources: [String: Any] = [
            "content.js": contentScript,
            "content.css": contentStyle,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: contentScriptsManifest, resources: resources)
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.waitForTestMessage("Main Frame Injected")
            try await manager.waitForTestMessage("Sub-Frame Injected")
        }
    }

    @Test
    func matchOriginAsFallbackWithData() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='data:text/html,<body></body>'></iframe>"
            }
        }

        let contentScriptsManifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Scripting Test",
            "description": "Scripting Test",
            "version": "1.0",
            "content_scripts": [
                [
                    "matches": ["*://localhost/*"],
                    "match_origin_as_fallback": true,
                    "all_frames": true,
                    "js": ["content.js"],
                    "css": ["content.css"],
                ]
            ],
        ]

        let contentStyle = "body { background-color: blue !important; }"

        let contentScript = """
            browser.test.assertEq(document.body.dataset.injected, undefined, 'Script should not have run before')
            document.body.dataset.injected = 'true'

            const bgColor = getComputedStyle(document.body).backgroundColor
            browser.test.assertEq(bgColor, 'rgb(0, 0, 255)', 'CSS should be applied correctly')

            browser.test.sendMessage(window.top === window ? 'Main Frame Injected' : 'Sub-Frame Injected')
            """

        let resources: [String: Any] = [
            "content.js": contentScript,
            "content.css": contentStyle,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: contentScriptsManifest, resources: resources)
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.waitForTestMessage("Main Frame Injected")
            try await manager.waitForTestMessage("Sub-Frame Injected")
        }
    }

    @Test
    func matchOriginAsFallbackWithBlob() async throws {
        let pageScript = """
            <script>
              const blob = new Blob(['<body></body>'], { type: 'text/html' })
              const blobURL = URL.createObjectURL(blob)
              document.write(`<iframe src='${blobURL}'></iframe>`)
            </script>
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                pageScript
            }
        }

        let contentScriptsManifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Scripting Test",
            "description": "Scripting Test",
            "version": "1.0",

            "content_scripts": [
                [
                    "matches": ["*://localhost/*"],
                    "match_origin_as_fallback": true,
                    "all_frames": true,
                    "js": ["content.js"],
                    "css": ["content.css"],
                ]
            ],
        ]

        let contentStyle = "body { background-color: green !important; }"

        let contentScript = """
            browser.test.assertEq(document.body.dataset.injected, undefined, 'Script should not have run before')
            document.body.dataset.injected = 'true'

            const bgColor = getComputedStyle(document.body).backgroundColor
            browser.test.assertEq(bgColor, 'rgb(0, 128, 0)', 'CSS should be applied correctly')

            browser.test.sendMessage(window.top === window ? 'Main Frame Injected' : 'Sub-Frame Injected')
            """

        let resources: [String: Any] = [
            "content.js": contentScript,
            "content.css": contentStyle,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: contentScriptsManifest, resources: resources)
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.waitForTestMessage("Main Frame Injected")
            try await manager.waitForTestMessage("Sub-Frame Injected")
        }
    }

    @Test
    func removeAllUserScriptsDoesNotRemoveWebExtensionScripts() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let contentScriptsManifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Scripting Test",
            "description": "Scripting Test",
            "version": "1.0",

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
            browser.test.sendMessage('Remove Scripts and Load Tab')
            """

        let contentScript = """
            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "content.js": contentScript,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: contentScriptsManifest, resources: resources)
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Remove Scripts and Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.configuration.userContentController.removeAllUserScripts()
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func removeAllUserStyleSheetsDoesNotRemoveWebExtensionStyleSheets() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<div id='test'>Test</div>"
            }
        }

        let contentScriptsManifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Scripting Test",
            "description": "Scripting Test",
            "version": "1.0",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "content_scripts": [
                [
                    "matches": ["*://localhost/*"],
                    "css": ["content.css"],
                    "js": ["content.js"],
                ]
            ],
        ]

        let backgroundScript = """
            browser.test.sendMessage('Remove StyleSheets and Load Tab')
            """

        let contentScript = """
            const testElement = document.getElementById('test')
            const style = window.getComputedStyle(testElement)
            browser.test.assertEq(style.color, 'rgb(255, 0, 0)', 'CSS should still apply after removing user stylesheets')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "content.css": "#test { color: red; }",
            "content.js": contentScript,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: contentScriptsManifest, resources: resources)
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Remove StyleSheets and Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.configuration.userContentController._removeAllUserStyleSheets()
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func injectScriptWithTrustedTypesCSP() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html", "Content-Security-Policy": "require-trusted-types-for 'script'"]) {
                ""
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Scripting Test",
            "description": "Scripting Test",
            "version": "1.0",

            "content_scripts": [
                [
                    "matches": ["*://localhost/*"],
                    "js": ["content.js"],
                ]
            ],
        ]

        let contentScript = """
            try {
              document.body.innerHTML = '<p>Injected</p>'
              browser.test.notifyPass()
            } catch (error) {
              browser.test.notifyFail('CSP blocked innerHTML: ' + error.message)
            }
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: ["content.js": contentScript])
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func injectExtensionScriptElementWithStrictDynamicReportOnlyCSP() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route(
                "/",
                headerFields: [
                    "Content-Type": "text/html",
                    "Content-Security-Policy": "script-src 'self'",
                    "Content-Security-Policy-Report-Only": "script-src 'nonce-r4nd0m' 'strict-dynamic'",
                ]
            ) {
                ""
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Scripting Test",
            "description": "Scripting Test",
            "version": "1.0",

            "web_accessible_resources": [
                [
                    "resources": ["page.js"],
                    "matches": ["*://localhost/*"],
                ]
            ],

            "content_scripts": [
                [
                    "matches": ["*://localhost/*"],
                    "js": ["content.js"],
                ]
            ],
        ]

        let contentScript = """
            const script = document.createElement('script')
            script.src = browser.runtime.getURL('page.js')
            script.onload = () => browser.test.notifyPass()
            script.onerror = () => browser.test.notifyFail('Extension script was blocked by CSP')
            document.head.appendChild(script)
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: ["content.js": contentScript, "page.js": ""])
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func migrateScriptDataToNewFormat() async throws {
        let backgroundScript = """
            var expectedResults = [{
              id: '1',
              js: ['content.js'],
              matches: ['*://localhost/*'],
              persistAcrossSessions: true
            }]

            var results
            var resultsPassingIds

            results = await browser.scripting.getRegisteredContentScripts()
            resultsPassingIds = await browser.scripting.getRegisteredContentScripts({ ids: ['1'] })

            browser.test.assertDeepEq(results, resultsPassingIds)
            browser.test.assertDeepEq(results, expectedResults)

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "content.js": "",
        ]

        let manager = parseWebExtension(manifest: scriptingManifest, resources: resources, configuration: ._temporary())
        let context = try requireNonNil(manager.context)

        // Give the extension a unique identifier so it opts into saving data in the temporary configuration.
        context.uniqueIdentifier = "org.webkit.test.extension (76C788B8)"

        let storageDirectoryPath = try requireNonNil(manager.controller.configuration._storageDirectoryPath)
        let storageDirectory = (storageDirectoryPath as NSString).appendingPathComponent(context.uniqueIdentifier)

        try FileManager.default.createDirectory(atPath: storageDirectory, withIntermediateDirectories: true)

        let files = [
            try requireNonNil(Bundle.testResources.url(forResource: "RegisteredContentScripts", withExtension: "db")),
            try requireNonNil(Bundle.testResources.url(forResource: "RegisteredContentScripts", withExtension: "db-shm")),
            try requireNonNil(Bundle.testResources.url(forResource: "RegisteredContentScripts", withExtension: "db-wal")),
        ]

        for file in files {
            let combinedPath = (storageDirectory as NSString).appendingPathComponent(file.lastPathComponent)
            try FileManager.default.copyItem(at: file, to: URL(fileURLWithPath: combinedPath))
        }

        try await manager.loadAndRun()
    }

    #if hasAttribute(diagnose)
    @diagnose(DeprecatedDeclaration, as: ignored, reason: "Intentionally uses process pool SPI")
    #endif
    @Test
    func contentScriptsAndStyleSheetsWithManyMatchPatterns() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<div id='test'>Test</div>"
            }
        }

        let matchPatterns: [String] = ["*://localhost/*"] + (1...50).map { "*://foo\($0)/*" }

        let contentScriptsManifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Scripting Test",
            "description": "Scripting Test",
            "version": "1.0",

            "content_scripts": [
                [
                    "matches": matchPatterns,
                    "css": ["content.css", "empty.css"],
                    "js": ["content.js", "empty.js"],
                ]
            ],
        ]

        let userStyle = "#test { color: red; font-size: 555px; }"
        let userScript = """
            const testElement = document.getElementById('test')
            const style = window.getComputedStyle(testElement)
            browser.test.assertEq(style.color, 'rgb(255, 0, 0)', 'CSS should apply')
            browser.test.assertEq(style.fontSize, '555px', 'Font size should apply')

            browser.test.notifyPass()
            """

        let oneMB = String(repeating: " ", count: 1 << 20)
        let resources: [String: Any] = [
            "content.css": userStyle + oneMB,
            "content.js": userScript + oneMB,
            "empty.css": "",
            "empty.js": "",
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: contentScriptsManifest, resources: resources)
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()

            var totalFootprint = 0
            for info in WKProcessPool._webContentProcessInfo() {
                totalFootprint += info.physicalFootprint
            }

            let maxExpectedFootprint = 100 * 1024 * 1024
            #expect(totalFootprint < maxExpectedFootprint)
        }
    }

    @Test
    func contentScriptsRespectDeniedMatchPatterns() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Content Scripts Test",
            "description": "Content Scripts Test",
            "version": "1.0",

            "content_scripts": [
                [
                    "matches": ["*://*/*"],
                    "js": ["content.js"],
                    "css": ["content.css"],
                    "run_at": "document_end",
                ]
            ],
        ]

        let contentScript = """
            document.body.dataset.scriptInjected = 'true'
            """

        let contentStyle = "body { background-color: rgb(255, 0, 0) !important; }"

        let resources: [String: Any] = [
            "content.js": contentScript,
            "content.css": contentStyle,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: resources)
            let context = try requireNonNil(manager.context)

            // Grant all hosts and schemes, but deny localhost specifically
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.MatchPattern.allHostsAndSchemes())

            context.setPermissionStatus(.deniedExplicitly, for: configuration.localhostAddress)

            // Test 1: Load localhost, verify script and style are NOT injected due to denied pattern
            let webView = try requireNonNil(manager.defaultTab?.webView)
            try await webView.loadAndWait(URLRequest(url: configuration.localhostAddress))

            // Verify the script and style were NOT injected by checking the page
            let localhostResult = try await webView.evaluateJavaScript(contentScriptStateScript) as? [String: Any]
            expectEqual(localhostResult?["scriptInjected"] as? Bool, false)
            expectNotEqual(localhostResult?["backgroundColor"] as? String, "rgb(255, 0, 0)")

            // Test 2: Load 127.0.0.1 (loopback), verify script and style ARE injected
            // This should work because allHostsAndSchemesMatchPattern is granted and loopback is not denied
            try await webView.loadAndWait(URLRequest(url: configuration.address))

            // Verify the script and style WERE injected
            let loopbackResult = try await webView.evaluateJavaScript(contentScriptStateScript) as? [String: Any]
            expectEqual(loopbackResult?["scriptInjected"] as? Bool, true)
            expectEqual(loopbackResult?["backgroundColor"] as? String, "rgb(255, 0, 0)")
        }
    }

    @Test
    func activeTabInjectsContentScriptsOnlyInGrantedTab() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<body></body>"
            }
        }

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: activeTabContentScriptsManifest, resources: activeTabContentScriptsResources)
            let context = try requireNonNil(manager.context)

            let grantedTab = try requireNonNil(manager.defaultTab)
            let otherTab = try requireNonNil(manager.defaultWindow).openNewTab()

            let grantedWebView = try requireNonNil(grantedTab.webView)
            let otherWebView = try requireNonNil(otherTab.webView)

            try await grantedWebView.loadAndWait(URLRequest(url: configuration.localhostAddress))
            try await otherWebView.loadAndWait(URLRequest(url: configuration.localhostAddress))

            // Without a grant, no content is injected.
            expectFalse(try await contentScriptState(grantedWebView).scriptInjected)
            expectFalse(try await contentScriptState(otherWebView).scriptInjected)

            context.userGesturePerformed(in: grantedTab)

            // The granted tab gets the scripts and styles...
            try await waitForCondition("the granted tab to get the content script and style sheet", timeout: .seconds(10)) {
                let state = try await contentScriptState(grantedWebView)
                return state.scriptInjected && state.backgroundColor == "rgb(255, 0, 0)"
            }

            // ...but another tab on the same host does not, since activeTab only covers the tab the gesture was in.
            let otherState = try await contentScriptState(otherWebView)
            expectFalse(otherState.scriptInjected)
            expectNotEqual(otherState.backgroundColor, "rgb(255, 0, 0)")

            // The content stays for as long as the grant does, so it is injected again when the tab reloads.
            grantedWebView.reload()
            try await grantedWebView._test_waitForDidFinishNavigation()

            let reloadedState = try await contentScriptState(grantedWebView)
            expectTrue(reloadedState.scriptInjected)
            expectEqual(reloadedState.backgroundColor, "rgb(255, 0, 0)")

            otherWebView.reload()
            try await otherWebView._test_waitForDidFinishNavigation()

            expectFalse(try await contentScriptState(otherWebView).scriptInjected)
        }
    }

    @Test
    func activeTabContentRemovedWhenGestureCleared() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<body></body>"
            }
        }

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: activeTabContentScriptsManifest, resources: activeTabContentScriptsResources)
            let context = try requireNonNil(manager.context)

            let tab = try requireNonNil(manager.defaultTab)
            let webView = try requireNonNil(tab.webView)

            try await webView.loadAndWait(URLRequest(url: configuration.localhostAddress))

            context.userGesturePerformed(in: tab)

            try await waitForCondition("the tab to get the style sheet", timeout: .seconds(10)) {
                try await contentScriptState(webView).backgroundColor == "rgb(255, 0, 0)"
            }

            // Revoking the grant takes the content away.
            context.clearUserGesture(in: tab)

            try await waitForCondition("the tab to lose the style sheet", timeout: .seconds(10)) {
                try await contentScriptState(webView).backgroundColor != "rgb(255, 0, 0)"
            }

            // Nothing is left registered, so a reload does not bring the content back.
            webView.reload()
            try await webView._test_waitForDidFinishNavigation()

            let state = try await contentScriptState(webView)
            expectFalse(state.scriptInjected)
            expectNotEqual(state.backgroundColor, "rgb(255, 0, 0)")
        }
    }

    @Test
    func activeTabAndHostGrantInjectContentScriptsOnce() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<body></body>"
            }
        }

        let resources: [String: Any] = [
            "content.js": "document.body.dataset.runCount = String(Number(document.body.dataset.runCount || 0) + 1)",
            "content.css": "body { background-color: rgb(255, 0, 0) !important; }",
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: activeTabContentScriptsManifest, resources: resources)

            let context = try requireNonNil(manager.context)
            let tab = try requireNonNil(manager.defaultTab)
            let webView = try requireNonNil(tab.webView)

            try await webView.loadAndWait(URLRequest(url: configuration.localhostAddress))

            context.userGesturePerformed(in: tab)

            try await waitForCondition("the content script to run", timeout: .seconds(10)) {
                try await contentScriptRunCount(webView) == "1"
            }

            // A host grant while the activeTab grant is active adds the same scripts for every tab, which must not run them again.
            let localhostPattern = try requireNonNil(WKWebExtension.MatchPattern.cachedPattern(string: "*://localhost/*"))
            context.setPermissionStatus(.grantedExplicitly, for: localhostPattern)

            expectEqual(try await contentScriptRunCount(webView), "1")

            // Loading the document again runs both copies at document end, and each script still runs once.
            webView.reload()
            try await webView._test_waitForDidFinishNavigation()

            expectEqual(try await contentScriptRunCount(webView), "1")
        }
    }

    @Test
    func activeTabContentRemovedAfterCrossHostNavigation() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<body></body>"
            }
        }

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: activeTabContentScriptsManifest, resources: activeTabContentScriptsResources)
            let context = try requireNonNil(manager.context)

            let tab = try requireNonNil(manager.defaultTab)
            let webView = try requireNonNil(tab.webView)

            try await webView.loadAndWait(URLRequest(url: configuration.localhostAddress))

            context.userGesturePerformed(in: tab)

            try await waitForCondition("the tab to get the content script", timeout: .seconds(10)) {
                try await contentScriptState(webView).scriptInjected
            }

            // Navigating to a different host ends the grant.
            try await webView.loadAndWait(URLRequest(url: configuration.address))

            // Coming back to the original host does not bring the content back without a new gesture.
            try await webView.loadAndWait(URLRequest(url: configuration.localhostAddress))

            let state = try await contentScriptState(webView)
            expectFalse(state.scriptInjected)
            expectNotEqual(state.backgroundColor, "rgb(255, 0, 0)")
        }
    }

    @Test
    func activeTabContentKeptWhenInjectedContentIsUpdated() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<body></body>"
            }
        }

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: activeTabContentScriptsManifest, resources: activeTabContentScriptsResources)

            let context = try requireNonNil(manager.context)
            let tab = try requireNonNil(manager.defaultTab)
            let webView = try requireNonNil(tab.webView)

            try await webView.loadAndWait(URLRequest(url: configuration.localhostAddress))

            context.userGesturePerformed(in: tab)

            try await waitForCondition("the tab to get the content script and style sheet", timeout: .seconds(10)) {
                let state = try await contentScriptState(webView)
                return state.scriptInjected && state.backgroundColor == "rgb(255, 0, 0)"
            }

            // Denying another site refreshes the content for the extension's patterns, which must leave the tab's content alone.
            let otherPattern = try requireNonNil(WKWebExtension.MatchPattern.cachedPattern(string: "*://example.com/*"))
            context.setPermissionStatus(.deniedExplicitly, for: otherPattern)

            // The tab's content is injected again on reload only if it was kept.
            webView.reload()
            try await webView._test_waitForDidFinishNavigation()

            let deniedState = try await contentScriptState(webView)
            expectTrue(deniedState.scriptInjected)
            expectEqual(deniedState.backgroundColor, "rgb(255, 0, 0)")

            // Changing file URL access refreshes the content the same way.
            context._hasAccessToFileURLs = true

            webView.reload()
            try await webView._test_waitForDidFinishNavigation()

            let fileAccessState = try await contentScriptState(webView)
            expectTrue(fileAccessState.scriptInjected)
            expectEqual(fileAccessState.backgroundColor, "rgb(255, 0, 0)")

            // The grant still owns the content, so revoking it still takes the content away.
            context.clearUserGesture(in: tab)

            try await waitForCondition("the tab to lose the style sheet", timeout: .seconds(10)) {
                try await contentScriptState(webView).backgroundColor != "rgb(255, 0, 0)"
            }
        }
    }

    @Test
    func activeTabInjectsRegisteredContentScripts() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<body></body>"
            }
        }

        let backgroundScript = """
            browser.test.onMessage.addListener(async (message) => {
              if (message === 'Register')
                await browser.scripting.registerContentScripts([{ id: 'registered', matches: ['*://localhost/*'], js: ['registered.js'], css: ['registered.css'], persistAcrossSessions: false }])
              else if (message === 'Update')
                await browser.scripting.updateContentScripts([{ id: 'registered', css: ['updated.css'] }])
              else if (message === 'Unregister')
                await browser.scripting.unregisterContentScripts({ ids: ['registered'] })

              browser.test.sendMessage('Done')
            })

            browser.test.sendMessage('Ready')
            """

        // The extension has no content scripts of its own, so everything the tab gets comes from the registered script.
        var manifest = activeTabContentScriptsManifest
        manifest.removeValue(forKey: "content_scripts")
        manifest["permissions"] = ["activeTab", "scripting"]
        manifest["background"] = [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ]

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "registered.js": "document.body.dataset.scriptInjected = 'true'",
            "registered.css": "body { background-color: rgb(255, 0, 0) !important; }",
            "updated.css": "body { background-color: rgb(0, 0, 255) !important; }",
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: resources)
            try await manager.waitForTestMessage("Ready")

            let context = try requireNonNil(manager.context)
            let grantedTab = try requireNonNil(manager.defaultTab)
            let otherTab = try requireNonNil(manager.defaultWindow).openNewTab()

            let grantedWebView = try requireNonNil(grantedTab.webView)
            let otherWebView = try requireNonNil(otherTab.webView)

            try await grantedWebView.loadAndWait(URLRequest(url: configuration.localhostAddress))
            try await otherWebView.loadAndWait(URLRequest(url: configuration.localhostAddress))

            context.userGesturePerformed(in: grantedTab)

            // A script registered after the grant reaches the granted tab only.
            manager.sendTestMessage("Register")
            try await manager.waitForTestMessage("Done")

            try await waitForCondition("the granted tab to get the registered script and style sheet", timeout: .seconds(10)) {
                let state = try await contentScriptState(grantedWebView)
                return state.scriptInjected && state.backgroundColor == "rgb(255, 0, 0)"
            }

            expectFalse(try await contentScriptState(otherWebView).scriptInjected)

            // Updating it changes what the granted tab has.
            manager.sendTestMessage("Update")
            try await manager.waitForTestMessage("Done")

            try await waitForCondition("the granted tab to get the updated style sheet", timeout: .seconds(10)) {
                try await contentScriptState(grantedWebView).backgroundColor == "rgb(0, 0, 255)"
            }

            // Unregistering it takes it away, even though the grant is still active.
            manager.sendTestMessage("Unregister")
            try await manager.waitForTestMessage("Done")

            try await waitForCondition("the granted tab to lose the updated style sheet", timeout: .seconds(10)) {
                try await contentScriptState(grantedWebView).backgroundColor != "rgb(0, 0, 255)"
            }

            grantedWebView.reload()
            try await grantedWebView._test_waitForDidFinishNavigation()

            let state = try await contentScriptState(grantedWebView)
            expectFalse(state.scriptInjected)
            expectNotEqual(state.backgroundColor, "rgb(255, 0, 0)")
            expectNotEqual(state.backgroundColor, "rgb(0, 0, 255)")
        }
    }

    @Test
    func activeTabFiresPermissionsEvents() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<body></body>"
            }
        }

        let backgroundScript = """
            browser.permissions.onAdded.addListener((permissions) => browser.test.sendMessage('Added', permissions.origins))
            browser.permissions.onRemoved.addListener((permissions) => browser.test.sendMessage('Removed', permissions.origins))

            browser.test.sendMessage('Ready')
            """

        var manifest = activeTabContentScriptsManifest
        manifest["background"] = [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ]

        var resources = activeTabContentScriptsResources
        resources["background.js"] = backgroundScript

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: resources)
            try await manager.waitForTestMessage("Ready")

            let context = try requireNonNil(manager.context)
            let tab = try requireNonNil(manager.defaultTab)
            let webView = try requireNonNil(tab.webView)

            try await webView.loadAndWait(URLRequest(url: configuration.localhostAddress))

            // The activeTab grant adds access to the tab's host, and losing it removes that access.
            context.userGesturePerformed(in: tab)

            let addedOrigins = try await manager.waitForTestMessage("Added") as? [String]
            expectEqual(addedOrigins?.count, 1)
            expectTrue(addedOrigins?.first?.contains("localhost") == true)

            context.clearUserGesture(in: tab)

            let removedOrigins = try await manager.waitForTestMessage("Removed") as? [String]
            expectEqual(removedOrigins?.count, 1)
            expectTrue(removedOrigins?.first?.contains("localhost") == true)
        }
    }

    @Test
    func activeTabDoesNotHideRequestedPermissionStatus() async throws {
        let url = try requireNonNil(URL(string: "http://localhost/"))
        let pattern = try requireNonNil(WKWebExtension.MatchPattern.cachedPattern(string: "*://localhost/*"))

        // Without activeTab, the site the content script matches is reported as requested.
        var manifestWithoutActiveTab = activeTabContentScriptsManifest
        manifestWithoutActiveTab.removeValue(forKey: "permissions")

        let managerWithoutActiveTab = try loadWebExtension(manifest: manifestWithoutActiveTab, resources: activeTabContentScriptsResources)
        let contextWithoutActiveTab = try requireNonNil(managerWithoutActiveTab.context)
        expectEqual(contextWithoutActiveTab.permissionStatus(for: url), .requestedExplicitly)
        expectEqual(contextWithoutActiveTab.permissionStatus(for: pattern), .requestedExplicitly)

        // With activeTab, requested sites are still reported as requested, with or without a tab, so clients can notify the user.
        let manager = try loadWebExtension(manifest: activeTabContentScriptsManifest, resources: activeTabContentScriptsResources)
        let context = try requireNonNil(manager.context)
        let tab = manager.defaultTab

        expectEqual(context.permissionStatus(for: url), .requestedExplicitly)
        expectEqual(context.permissionStatus(for: url, in: tab), .requestedExplicitly)
        expectEqual(context.permissionStatus(for: pattern), .requestedExplicitly)
        expectEqual(context.permissionStatus(for: pattern, in: tab), .requestedExplicitly)

        // A revoked site is still reported, so the client can tell it was revoked.
        context.setPermissionStatus(.deniedExplicitly, for: url)
        expectEqual(context.permissionStatus(for: url, in: tab), .deniedExplicitly)

        context.setPermissionStatus(.unknown, for: url)
        expectEqual(context.permissionStatus(for: url, in: tab), .requestedExplicitly)
    }

    @Test
    func activeTabNotificationsIncludeTab() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<body></body>"
            }
        }

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: activeTabContentScriptsManifest, resources: activeTabContentScriptsResources)

            let context = try requireNonNil(manager.context)
            let tab = try requireNonNil(manager.defaultTab)
            let webView = try requireNonNil(tab.webView)

            try await webView.loadAndWait(URLRequest(url: configuration.localhostAddress))

            let granted = MatchPatternsNotificationRecorder()
            let removed = MatchPatternsNotificationRecorder()

            let grantedObserver = observeMatchPatternsNotifications(
                named: WKWebExtensionContext.permissionMatchPatternsWereGrantedNotification,
                object: context,
                into: granted
            )
            defer { NotificationCenter.default.removeObserver(grantedObserver) }

            let removedObserver = observeMatchPatternsNotifications(
                named: WKWebExtensionContext.grantedPermissionMatchPatternsWereRemovedNotification,
                object: context,
                into: removed
            )
            defer { NotificationCenter.default.removeObserver(removedObserver) }

            // Granting access to the whole extension is not for a single tab, so there is no tab in the notification.
            let otherPattern = try requireNonNil(WKWebExtension.MatchPattern.cachedPattern(string: "*://example.com/*"))
            context.setPermissionStatus(.grantedExplicitly, for: otherPattern)

            try await waitForCondition("the first grant notification", timeout: .seconds(10)) {
                granted.notifications.count == 1
            }

            expectNotNil(granted.notifications[0].matchPatterns)
            expectNil(granted.notifications[0].tab)

            // The activeTab permission is only for the tab with the gesture, so the notification says which tab.
            context.userGesturePerformed(in: tab)

            try await waitForCondition("the activeTab grant notification", timeout: .seconds(10)) {
                granted.notifications.count == 2
            }

            expectNotNil(granted.notifications[1].matchPatterns)
            expectEqual(granted.notifications[1].tab, ObjectIdentifier(tab))

            // Losing it says the same.
            context.clearUserGesture(in: tab)

            try await waitForCondition("the activeTab removal notification", timeout: .seconds(10)) {
                removed.notifications.count == 1
            }

            expectNotNil(removed.notifications[0].matchPatterns)
            expectEqual(removed.notifications[0].tab, ObjectIdentifier(tab))
        }
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
