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
private import TestWebKitAPILibrary.Helpers.cocoa.TestWebExtensionsDelegate
private import TestWebKitAPILibrary.Helpers.cocoa.WebExtensionUtilities
import Testing
import WebKit
private import WebKit_Private.WKWebExtensionContextPrivate

import struct Foundation.URL
import struct Swift.String

@MainActor
struct WKWebExtensionAPITabsTests {
    private let tabsManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "Tabs Test",
        "description": "Tabs Test",
        "version": "1",

        "options_page": "options.html",

        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],

        "permissions": ["tabs"],
    ]

    private let tabsManifestV2: [String: Any] = [
        "manifest_version": 2,

        "name": "Tabs Test",
        "description": "Tabs Test",
        "version": "1",

        "permissions": ["*://localhost/*"],

        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],
    ]

    private let tabsContentScriptManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "Tabs Test",
        "description": "Tabs Test",
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
                "all_frames": true,
            ]
        ],
    ]

    private let activeTabManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "Tabs Test",
        "description": "Tabs Test",
        "version": "1",

        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],

        "permissions": ["activeTab"],
    ]

    /// A tab the extension asked to open, with the number of tabs in the window when it asked.
    private struct RequestedTab {
        let configuration: WKWebExtension.TabConfiguration
        let tabCount: Int
    }

    private func loadAndRun(manifest: [String: Any]? = nil, backgroundScript: String) async throws {
        let manager = try loadWebExtension(manifest: manifest ?? tabsManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    /// Serves an empty page, loads an extension that has been granted access to it, and calls `body` with the
    /// extension's manager and the page's `localhost` URL.
    private func withEmptyPage(
        manifest: [String: Any],
        resources: [String: Any],
        fileID: String = #fileID,
        filePath: String = #filePath,
        line: Int = #line,
        column: Int = #column,
        _ body: (TestWebExtensionManager, URL) async throws -> Void
    ) async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: resources)
            let context = try requireNonNil(manager.context, fileID: fileID, filePath: filePath, line: line, column: column)

            let url = configuration.localhostAddress
            context.setPermissionStatus(.grantedExplicitly, for: url)

            try await body(manager, url)
        }
    }

    /// Serves an empty page, loads an extension that has been granted access to it, loads the page in the default tab
    /// once the extension sends `Load Tab`, and runs the extension until it finishes.
    private func loadAndRunWithEmptyPage(
        manifest: [String: Any],
        resources: [String: Any],
        fileID: String = #fileID,
        filePath: String = #filePath,
        line: Int = #line,
        column: Int = #column
    ) async throws {
        try await withEmptyPage(
            manifest: manifest,
            resources: resources,
            fileID: fileID,
            filePath: filePath,
            line: line,
            column: column
        ) { manager, url in
            try await manager.waitForTestMessage("Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView, fileID: fileID, filePath: filePath, line: line, column: column)
            webView.load(URLRequest(url: url))

            try await manager.run()
        }
    }

    /// Serves a page with a same-site subframe, loads an extension with the content script manifest that has been granted
    /// access to every `localhost` URL, loads the page in the default tab once the extension sends `Load Tab`, and runs
    /// the extension until it finishes.
    private func loadAndRunWithFramePage(
        resources: [String: Any],
        fileID: String = #fileID,
        filePath: String = #filePath,
        line: Int = #line,
        column: Int = #column
    ) async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                " "
            }
        }

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: tabsContentScriptManifest, resources: resources)
            let context = try requireNonNil(manager.context, fileID: fileID, filePath: filePath, line: line, column: column)

            let matchPattern = try requireNonNil(
                WKWebExtension.MatchPattern.cachedPattern(scheme: "*", host: "localhost", path: "/*"),
                fileID: fileID,
                filePath: filePath,
                line: line,
                column: column
            )
            context.setPermissionStatus(.grantedExplicitly, for: matchPattern)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView, fileID: fileID, filePath: filePath, line: line, column: column)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    // MARK: - Basics, Create, Duplicate, Update, Get, and Query

    @Test
    func tabStatus() async throws {
        try await loadAndRun(
            backgroundScript: """
                browser.test.assertEq(typeof browser.tabs.TabStatus, 'object', 'TabStatus should be an object')

                browser.test.assertEq(browser.tabs.TabStatus.LOADING, 'loading', 'LOADING should be defined')
                browser.test.assertEq(browser.tabs.TabStatus.COMPLETE, 'complete', 'COMPLETE should be defined')

                browser.test.assertDeepEq(Object.keys(browser.tabs.TabStatus).sort(), [ 'COMPLETE', 'LOADING' ], 'TabStatus should only define the supported values')

                browser.test.notifyPass()
                """
        )
    }

    @Test
    func windowType() async throws {
        try await loadAndRun(
            backgroundScript: """
                browser.test.assertEq(typeof browser.tabs.WindowType, 'object', 'WindowType should be an object')

                browser.test.assertEq(browser.tabs.WindowType.NORMAL, 'normal', 'NORMAL should be defined')
                browser.test.assertEq(browser.tabs.WindowType.POPUP, 'popup', 'POPUP should be defined')

                browser.test.assertDeepEq(Object.keys(browser.tabs.WindowType).sort(), [ 'NORMAL', 'POPUP' ], 'WindowType should only define the supported values')

                browser.test.notifyPass()
                """
        )
    }

    @Test
    func errors() async throws {
        try await loadAndRun(
            backgroundScript: """
                browser.test.assertThrows(() => browser.tabs.get('bad'), /'tabId' value is invalid, because a number is expected/i)
                browser.test.assertThrows(() => browser.tabs.get(-3), /'tabId' value is invalid, because it is not a tab identifier/i)
                browser.test.assertThrows(() => browser.tabs.duplicate('bad'), /'tabId' value is invalid, because a number is expected/i)
                browser.test.assertThrows(() => browser.tabs.remove('bad'), /'tabIDs' value is invalid, because a number or an array of numbers is expected, but a string was provided/i)
                browser.test.assertThrows(() => browser.tabs.remove(['bad']), /'tabIDs' value is invalid, because a number or an array of numbers is expected, but an array of other values was provided/i)
                browser.test.assertThrows(() => browser.tabs.reload('bad'), /an unknown argument was provided/i)
                browser.test.assertThrows(() => browser.tabs.goBack('bad'), /'tabId' value is invalid, because a number is expected/i)
                browser.test.assertThrows(() => browser.tabs.goForward('bad'), /'tabId' value is invalid, because a number is expected/i)
                browser.test.assertThrows(() => browser.tabs.getZoom('bad'), /'tabId' value is invalid, because a number is expected/i)
                browser.test.assertThrows(() => browser.tabs.detectLanguage('bad'), /'tabId' value is invalid, because a number is expected/i)
                browser.test.assertThrows(() => browser.tabs.toggleReaderMode('bad'), /'tabId' value is invalid, because a number is expected/i)

                browser.test.assertThrows(() => browser.tabs.setZoom('bad'), /'zoomFactor' value is invalid, because a number is expected/i)
                browser.test.assertThrows(() => browser.tabs.setZoom(1, 'bad'), /'zoomFactor' value is invalid, because a number is expected/i)

                browser.test.assertThrows(() => browser.tabs.update('bad'), /'properties' value is invalid, because an object is expected/i)
                browser.test.assertThrows(() => browser.tabs.update(1, 'bad'), /'properties' value is invalid, because an object is expected/i)

                browser.test.assertThrows(() => browser.tabs.update(1, { 'openerTabId': true }), /'openerTabId' is expected to be a number, but a boolean was provided/i)
                browser.test.assertThrows(() => browser.tabs.update(1, { 'openerTabId': 4.5 }), /'openerTabId' value is invalid, because '4.5' is not a tab identifier/i)

                browser.test.assertThrows(() => browser.tabs.create({ 'url': 1234 }), /'url' is expected to be a string/i)

                browser.test.assertThrows(() => browser.tabs.query({ status: 'bad' }), /'status' value is invalid, because it must specify either 'loading' or 'complete'/i)

                browser.test.assertThrows(() => browser.tabs.query({ url: 12345 }), /'info' value is invalid, because 'url' is expected to be a string or an array of strings, but a number was provided/i)
                browser.test.assertThrows(() => browser.tabs.query({ url: ['bad', 12345] }), /'url' is expected to be a string or an array of strings, but an array of other values was provided/i)

                browser.test.assertThrows(() => browser.tabs.query({ windowId: 'bad' }), /'windowId' is expected to be a number, but a string was provided/i)
                browser.test.assertThrows(() => browser.tabs.query({ windowId: -5 }), /'windowId' value is invalid, because '-5' is not a window identifier/i)

                browser.test.assertThrows(() => browser.tabs.query({ active: 'true' }), /'active' is expected to be a boolean, but a string was provided/i)
                browser.test.assertThrows(() => browser.tabs.query({ highlighted: 123 }), /'highlighted' is expected to be a boolean, but a number was provided/i)
                browser.test.assertThrows(() => browser.tabs.query({ index: 'five' }), /'index' is expected to be a number, but a string was provided/i)
                browser.test.assertThrows(() => browser.tabs.query({ audible: 'yes' }), /'audible' is expected to be a boolean, but a string was provided/i)

                browser.test.assertThrows(() => browser.tabs.captureVisibleTab('bad', { format: 'png' }), /an unknown argument was provided/i)
                browser.test.assertThrows(() => browser.tabs.captureVisibleTab(undefined, { format: 'bad' }), /'format' value is invalid, because it must specify either 'png' or 'jpeg'/i)
                browser.test.assertThrows(() => browser.tabs.captureVisibleTab(undefined, 'bad'), /an unknown argument was provided/i)
                browser.test.assertThrows(() => browser.tabs.captureVisibleTab(undefined, { format: 'jpeg', quality: 'high' }), /'quality' is expected to be a number, but a string was provided/i)
                browser.test.assertThrows(() => browser.tabs.captureVisibleTab(undefined, { format: 'jpeg', quality: 200 }), /'quality' value is invalid, because it must specify a value between 0 and 100/i)

                browser.test.assertThrows(() => browser.tabs.sendMessage(1, 'message', { frameId: 0, documentId: 'some-document-id' }), /cannot specify both 'frameId' and 'documentId'/i);
                browser.test.assertThrows(() => browser.tabs.connect(1, { frameId: 0, documentId: 'some-document-id' }), /cannot specify both 'frameId' and 'documentId'/i);

                await browser.test.assertRejects(browser.tabs.captureVisibleTab(9999), /window not found/i)
                await browser.test.assertRejects(browser.tabs.captureVisibleTab(), /'activeTab' permission or granted host permissions for the current website are required/i)

                await browser.test.assertRejects(browser.tabs.get(9999), /tab not found/i)
                await browser.test.assertRejects(browser.tabs.duplicate(9999), /tab not found/i)
                await browser.test.assertRejects(browser.tabs.remove(9999), /tab '9999' was not found/i)
                await browser.test.assertRejects(browser.tabs.reload(9999), /tab not found/i)
                await browser.test.assertRejects(browser.tabs.goBack(9999), /tab not found/i)
                await browser.test.assertRejects(browser.tabs.goForward(9999), /tab not found/i)
                await browser.test.assertRejects(browser.tabs.setZoom(9999, 1.5), /tab not found/i)
                await browser.test.assertRejects(browser.tabs.detectLanguage(9999), /tab not found/i)
                await browser.test.assertRejects(browser.tabs.toggleReaderMode(9999), /tab not found/i)

                browser.test.notifyPass()
                """
        )

        try await loadAndRun(
            manifest: tabsManifestV2,
            backgroundScript: """
                browser.test.assertThrows(() => browser.tabs.executeScript(), /a required argument is missing/i)

                await browser.test.assertRejects(browser.tabs.executeScript(9999, { code: 'code to execute' }), /tab not found/i)

                browser.test.assertThrows(() => browser.tabs.executeScript({ code: 'code to execute', file: 'path/to/file.js' }), /it cannot specify both 'file' and 'code'./i)
                browser.test.assertThrows(() => browser.tabs.executeScript({ }), /it must specify either 'file' or 'code'./i)
                browser.test.assertThrows(() => browser.tabs.executeScript({ file: 0, frameId: 0 }), /'file' is expected to be a string, but a number was provided./i)

                browser.test.assertThrows(() => browser.tabs.executeScript({ code: 'code to execute', frameId: 0, allFrames: true }), /it cannot specify both 'allFrames' and 'frameId'./i)
                browser.test.assertThrows(() => browser.tabs.executeScript({ file: 'path/to/file.js', frameId: '0' }), /'frameId' is expected to be a number, but a string was provided./i)
                browser.test.assertThrows(() => browser.tabs.executeScript({ file: 'path/to/file.js', allFrames: 'true' }), /'allFrames' is expected to be a boolean, but a string was provided./i)
                browser.test.assertThrows(() => browser.tabs.executeScript({ file: 'path/to/file.js', frameId: -1 }), /'-1' is not a frame identifier./i)

                browser.test.assertThrows(() => browser.tabs.insertCSS(), /a required argument is missing./i)

                await browser.test.assertRejects(browser.tabs.insertCSS(9999, { code: 'code to execute' }), /tab not found/i)

                browser.test.assertThrows(() => browser.tabs.insertCSS({ code: 'code to execute', file: 'path/to/file.js' }), /it cannot specify both 'file' and 'code'./i)
                browser.test.assertThrows(() => browser.tabs.insertCSS({ }), /it must specify either 'file' or 'code'./i)
                browser.test.assertThrows(() => browser.tabs.insertCSS({ file: 'path/to/file.js', frameId: '0' }), /'frameId' is expected to be a number, but a string was provided./i)

                browser.test.assertThrows(() => browser.tabs.insertCSS({ code: 'code to execute', frameId: 0, allFrames: true }), /it cannot specify both 'allFrames' and 'frameId'./i)
                browser.test.assertThrows(() => browser.tabs.insertCSS({ file: 'path/to/file.js', frameId: '0' }), /'frameId' is expected to be a number, but a string was provided./i)
                browser.test.assertThrows(() => browser.tabs.insertCSS({ file: 'path/to/file.js', allFrames: 'true' }), /'allFrames' is expected to be a boolean, but a string was provided./i)

                browser.test.assertThrows(() => browser.tabs.insertCSS({ file: 'path/to/file.js', frameId: -1 }), /'-1' is not a frame identifier./i)

                browser.test.assertThrows(() => browser.tabs.insertCSS({ code: 'body { color: red }', cssOrigin: 'bad' }), /'cssOrigin' value is invalid, because it must specify either 'author' or 'user'/i);

                browser.test.assertThrows(() => browser.tabs.removeCSS(), /a required argument is missing./i)

                await browser.test.assertRejects(browser.tabs.removeCSS(9999, { code: 'code to execute' }), /tab not found/i)

                browser.test.assertThrows(() => browser.tabs.removeCSS({ code: 'code to execute', file: 'path/to/file.js' }), /it cannot specify both 'file' and 'code'./i)
                browser.test.assertThrows(() => browser.tabs.removeCSS({ }), /it must specify either 'file' or 'code'./i)
                browser.test.assertThrows(() => browser.tabs.removeCSS({ file: 0, frameId: 0 }), /'file' is expected to be a string, but a number was provided./i)

                browser.test.assertThrows(() => browser.tabs.removeCSS({ code: 'code to execute', frameId: 0, allFrames: true }), /it cannot specify both 'allFrames' and 'frameId'./i)
                browser.test.assertThrows(() => browser.tabs.removeCSS({ file: 'path/to/file.js', allFrames: 'true' }), /'allFrames' is expected to be a boolean, but a string was provided./i)

                browser.test.assertThrows(() => browser.tabs.removeCSS({ file: 'path/to/file.js', frameId: -1 }), /'-1' is not a frame identifier./i)

                browser.test.notifyPass()
                """
        )
    }

    @Test
    func create() async throws {
        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            const initialTabsCount = allWindows[0].tabs.length

            const newTab = await browser.tabs.create({})

            const updatedWindows = await browser.windows.getAll({ populate: true })
            const updatedTabs = updatedWindows[0].tabs
            browser.test.assertEq(updatedTabs.length, initialTabsCount + 1, 'A new tab should have been added')
            browser.test.assertEq(newTab.index, initialTabsCount, 'The new tab should be the last tab in the window')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        let window = try requireNonNil(manager.defaultWindow)
        let originalOpenNewTab = manager.internalDelegate.openNewTab

        var requestedTabs: [RequestedTab] = []

        manager.internalDelegate.openNewTab = { configuration, extensionContext, completionHandler in
            requestedTabs.append(RequestedTab(configuration: configuration, tabCount: window.tabs.count))

            originalOpenNewTab?(configuration, extensionContext, completionHandler)
        }

        try await manager.run()

        for requestedTab in requestedTabs {
            let configuration = requestedTab.configuration

            expectIdentical(configuration.window, window)
            expectEqual(configuration.index, requestedTab.tabCount)

            expectNil(configuration.parentTab)
            expectNil(configuration.url)

            expectTrue(configuration.shouldBeActive)
            expectTrue(configuration.shouldAddToSelection)
            expectFalse(configuration.shouldBePinned)
            expectFalse(configuration.shouldBeMuted)
            expectFalse(configuration.shouldReaderModeBeActive)
        }
    }

    @Test
    func createTabsOverflowIndex() async throws {
        let backgroundScript = """
            var allWindows = await browser.windows.getAll({ populate: true })
            var initialTabsCount = allWindows[0].tabs.length
            var largeIndex = 13000000000000000000000000000000000
            var newTab = await browser.tabs.create({ index: largeIndex })

            var updatedWindows = await browser.windows.getAll({ populate: true })
            var updatedTabs = updatedWindows[0].tabs
            browser.test.assertEq(updatedTabs.length, initialTabsCount + 1, 'One new tab should have been added')
            browser.test.assertEq(newTab.index, initialTabsCount, 'The new tab should be posted at the end')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        let window = try requireNonNil(manager.defaultWindow)
        let originalOpenNewTab = manager.internalDelegate.openNewTab

        var requestedConfigurations: [WKWebExtension.TabConfiguration] = []

        manager.internalDelegate.openNewTab = { configuration, extensionContext, completionHandler in
            requestedConfigurations.append(configuration)

            originalOpenNewTab?(configuration, extensionContext, completionHandler)
        }

        try await manager.run()

        for configuration in requestedConfigurations {
            expectIdentical(configuration.window, window)
            // The largest integer a JavaScript number can represent exactly.
            expectEqual(configuration.index, 9_007_199_254_740_991)
        }
    }

    @Test
    func createTabsZeroIndex() async throws {
        let backgroundScript = """
            var allWindows = await browser.windows.getAll({ populate: true })
            var initialTabsCount = allWindows[0].tabs.length

            var newTab = await browser.tabs.create({ index: 0 })

            var updatedWindows = await browser.windows.getAll({ populate: true })
            var updatedTabs = updatedWindows[0].tabs
            browser.test.assertEq(updatedTabs.length, initialTabsCount + 1, 'One new tab should have been added')
            browser.test.assertEq(newTab.index, initialTabsCount - 1, 'The new tab should be posted at the start')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        let window = try requireNonNil(manager.defaultWindow)
        let originalOpenNewTab = manager.internalDelegate.openNewTab

        var requestedConfigurations: [WKWebExtension.TabConfiguration] = []

        manager.internalDelegate.openNewTab = { configuration, extensionContext, completionHandler in
            requestedConfigurations.append(configuration)

            originalOpenNewTab?(configuration, extensionContext, completionHandler)
        }

        try await manager.run()

        for configuration in requestedConfigurations {
            expectIdentical(configuration.window, window)
            expectEqual(configuration.index, 0)
        }
    }

    @Test
    func createWithSpecifiedOptions() async throws {
        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            const initialTabsCount = allWindows[0].tabs.length

            const newTab = await browser.tabs.create({
              url: 'https://example.com/',
              openerTabId: allWindows[0].tabs[0].id,
              active: false,
              pinned: true,
              muted: true,
              openInReaderMode: true,
              index: 1
            })

            const updatedWindows = await browser.windows.getAll({ populate: true })
            const updatedTabs = updatedWindows[0].tabs
            browser.test.assertEq(updatedTabs.length, initialTabsCount + 1, 'A new tab should have been added')
            browser.test.assertEq(newTab.index, 1, 'The new tab should be at index 1')
            browser.test.assertFalse(newTab.active, 'The new tab should not be active')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        let window = try requireNonNil(manager.defaultWindow)
        let tab = try requireNonNil(manager.defaultTab)
        let originalOpenNewTab = manager.internalDelegate.openNewTab

        var requestedConfigurations: [WKWebExtension.TabConfiguration] = []

        manager.internalDelegate.openNewTab = { configuration, extensionContext, completionHandler in
            requestedConfigurations.append(configuration)

            originalOpenNewTab?(configuration, extensionContext, completionHandler)
        }

        try await manager.run()

        for configuration in requestedConfigurations {
            expectIdentical(configuration.window, window)
            expectEqual(configuration.index, 1)

            expectIdentical(configuration.parentTab, tab)
            expectEqual(configuration.url, URL(string: "https://example.com/"))

            expectFalse(configuration.shouldBeActive)
            expectFalse(configuration.shouldAddToSelection)
            expectTrue(configuration.shouldBePinned)
            expectTrue(configuration.shouldBeMuted)
            expectTrue(configuration.shouldReaderModeBeActive)
        }
    }

    @Test
    func createWithJavaScriptURLIsRejected() async throws {
        let backgroundScript = """
            await browser.test.assertRejects(browser.tabs.create({ url: 'javascript:void(0)' }), /it cannot be a JavaScript URL/i)

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        var openedTab = false

        manager.internalDelegate.openNewTab = { _, _, completionHandler in
            openedTab = true
            completionHandler(nil, nil)
        }

        try await manager.run()

        expectFalse(openedTab)
    }

    @Test
    func createWithFileURLIsRejected() async throws {
        let backgroundScript = """
            await browser.test.assertRejects(browser.tabs.create({ url: 'file:///tmp/test.html' }), /it cannot be a local file URL/i)

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        var openedTab = false

        manager.internalDelegate.openNewTab = { _, _, completionHandler in
            openedTab = true
            completionHandler(nil, nil)
        }

        try await manager.run()

        expectFalse(openedTab)
    }

    @Test
    func createWithFileURLWhenFileAccessIsAllowed() async throws {
        let backgroundScript = """
            await browser.tabs.create({ url: 'file:///tmp/test.html' })

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
        let context = try requireNonNil(manager.context)

        context._hasAccessToFileURLs = true

        let originalOpenNewTab = manager.internalDelegate.openNewTab

        var requestedURLs: [URL?] = []
        var openedTab = false

        manager.internalDelegate.openNewTab = { configuration, extensionContext, completionHandler in
            requestedURLs.append(configuration.url)
            openedTab = true

            originalOpenNewTab?(configuration, extensionContext, completionHandler)
        }

        try await manager.run()

        for url in requestedURLs {
            expectEqual(url, URL(string: "file:///tmp/test.html"))
        }

        expectTrue(openedTab)
    }

    @Test
    func createWithDataURL() async throws {
        let backgroundScript = """
            await browser.tabs.create({ url: 'data:text/html,Hello' })

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        let originalOpenNewTab = manager.internalDelegate.openNewTab

        var requestedURLs: [URL?] = []
        var openedTab = false

        manager.internalDelegate.openNewTab = { configuration, extensionContext, completionHandler in
            requestedURLs.append(configuration.url)
            openedTab = true

            originalOpenNewTab?(configuration, extensionContext, completionHandler)
        }

        try await manager.run()

        for url in requestedURLs {
            expectEqual(url, URL(string: "data:text/html,Hello"))
        }

        expectTrue(openedTab)
    }

    @Test
    func createWithMailtoURL() async throws {
        let backgroundScript = """
            await browser.tabs.create({ url: 'mailto:hello@example.com' })

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        let originalOpenNewTab = manager.internalDelegate.openNewTab

        var requestedURLs: [URL?] = []
        var openedTab = false

        manager.internalDelegate.openNewTab = { configuration, extensionContext, completionHandler in
            requestedURLs.append(configuration.url)
            openedTab = true

            originalOpenNewTab?(configuration, extensionContext, completionHandler)
        }

        try await manager.run()

        for url in requestedURLs {
            expectEqual(url, URL(string: "mailto:hello@example.com"))
        }

        expectTrue(openedTab)
    }

    @Test
    func createWithRelativeURL() async throws {
        let backgroundScript = """
            const newTab = await browser.tabs.create({
              url: 'test.html'
            })

            browser.test.assertEq(newTab.url, browser.runtime.getURL('test.html'), 'The new tab should have the correct URL')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(
            manifest: tabsManifest,
            resources: ["background.js": backgroundScript, "test.html": "Hello world!"]
        )
        let context = try requireNonNil(manager.context)

        let originalOpenNewTab = manager.internalDelegate.openNewTab

        var requestedURLs: [URL?] = []

        manager.internalDelegate.openNewTab = { configuration, extensionContext, completionHandler in
            requestedURLs.append(configuration.url)

            originalOpenNewTab?(configuration, extensionContext, completionHandler)
        }

        try await manager.run()

        for url in requestedURLs {
            expectEqual(url, URL(string: "test.html", relativeTo: context.baseURL)?.absoluteURL)
        }
    }

    @Test
    func duplicate() async throws {
        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            const initialTabsCount = allWindows[0].tabs.length

            const duplicatedTab = await browser.tabs.duplicate(allWindows[0].tabs[0].id)

            const updatedWindows = await browser.windows.getAll({ populate: true })
            const updatedTabs = updatedWindows[0].tabs
            browser.test.assertEq(updatedTabs.length, initialTabsCount + 1, 'A tab should have been duplicated')
            browser.test.assertEq(duplicatedTab.index, initialTabsCount, 'The duplicated tab should be the last tab in the window')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        let window = try requireNonNil(manager.defaultWindow)
        let tab = try requireNonNil(manager.defaultTab)
        let originalDuplicate = tab.duplicate

        var requestedTabs: [RequestedTab] = []

        tab.duplicate = { configuration, completionHandler in
            requestedTabs.append(RequestedTab(configuration: configuration, tabCount: window.tabs.count))

            originalDuplicate?(configuration, completionHandler)
        }

        try await manager.run()

        for requestedTab in requestedTabs {
            let configuration = requestedTab.configuration

            expectIdentical(configuration.window, window)
            expectEqual(configuration.index, requestedTab.tabCount)

            expectNil(configuration.parentTab)
            expectNil(configuration.url)

            expectTrue(configuration.shouldBeActive)
            expectTrue(configuration.shouldAddToSelection)
            expectFalse(configuration.shouldBePinned)
            expectFalse(configuration.shouldBeMuted)
            expectFalse(configuration.shouldReaderModeBeActive)
        }
    }

    @Test
    func duplicateWithOptions() async throws {
        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            const initialTabsCount = allWindows[0].tabs.length

            const duplicatedTab = await browser.tabs.duplicate(allWindows[0].tabs[0].id, {
              active: false,
              index: 1
            })

            const updatedWindows = await browser.windows.getAll({ populate: true })
            const updatedTabs = updatedWindows[0].tabs
            browser.test.assertEq(updatedTabs.length, initialTabsCount + 1, 'A tab should have been duplicated')
            browser.test.assertEq(duplicatedTab.index, 1, 'The duplicated tab should be at index 1')
            browser.test.assertFalse(duplicatedTab.active, 'The duplicated tab should not be active')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        let window = try requireNonNil(manager.defaultWindow)
        let tab = try requireNonNil(manager.defaultTab)
        let originalDuplicate = tab.duplicate

        var requestedConfigurations: [WKWebExtension.TabConfiguration] = []

        tab.duplicate = { configuration, completionHandler in
            requestedConfigurations.append(configuration)

            originalDuplicate?(configuration, completionHandler)
        }

        try await manager.run()

        for configuration in requestedConfigurations {
            expectIdentical(configuration.window, window)
            expectEqual(configuration.index, 1)

            expectNil(configuration.parentTab)
            expectNil(configuration.url)

            expectFalse(configuration.shouldBeActive)
            expectFalse(configuration.shouldAddToSelection)
            expectFalse(configuration.shouldBePinned)
            expectFalse(configuration.shouldBeMuted)
            expectFalse(configuration.shouldReaderModeBeActive)
        }
    }

    @Test
    func optionalPropertiesOmitted() async throws {
        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            const firstTab = allWindows[0].tabs[0]
            const secondTab = allWindows[0].tabs[1]

            const queriedWithEmptyObject = await browser.tabs.query({ })
            const queriedWithoutArguments = await browser.tabs.query()
            browser.test.assertEq(queriedWithoutArguments.length, queriedWithEmptyObject.length, 'query() should match query({ })')

            await browser.tabs.update(secondTab.id, { openerTabId: firstTab.id })

            const emptyTab = await browser.tabs.update(secondTab.id, { })
            browser.test.assertEq(emptyTab.openerTabId, firstTab.id, 'update(tabId, { }) should keep the openerTabId')

            const pinnedTab = await browser.tabs.update(secondTab.id, { pinned: true })
            browser.test.assertEq(pinnedTab.openerTabId, firstTab.id, 'Updating another property should keep the openerTabId')

            const createdTab = await browser.tabs.create()
            browser.test.assertEq(typeof createdTab.id, 'number', 'create() should open a tab')
            browser.test.assertEq(createdTab.windowId, firstTab.windowId, 'create() should open the tab in the current window')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        try await Task.sleep(for: .seconds(5))
        try manager.checkCollectedFailures()
    }

    @Test
    func update() async throws {
        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            const secondTab = allWindows[0].tabs[1]

            browser.test.assertFalse(secondTab.active, 'The tab should not be initially active')
            browser.test.assertFalse(secondTab.highlighted, 'The tab should not be initially highlighted')
            browser.test.assertFalse(secondTab.pinned, 'The tab should not be initially pinned')
            browser.test.assertFalse(secondTab.mutedInfo.muted, 'The tab should not be initially muted')
            browser.test.assertEq(secondTab.openerTabId, undefined, 'The initial tab should not have an openerTabId')

            const updatedTab = await browser.tabs.update(secondTab.id, {
              active: true,
              highlighted: true,
              pinned: true,
              muted: true,
              openerTabId: allWindows[0].tabs[0].id
            })

            browser.test.assertTrue(updatedTab.active, 'The tab should be active after update')
            browser.test.assertTrue(updatedTab.highlighted, 'The tab should be highlighted after update')
            browser.test.assertTrue(updatedTab.pinned, 'The tab should be pinned after update')
            browser.test.assertTrue(updatedTab.mutedInfo.muted, 'The tab should be muted after update')
            browser.test.assertEq(updatedTab.openerTabId, allWindows[0].tabs[0].id, 'The openerTabId should match the first tab after update')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
        let defaultWindow = try requireNonNil(manager.defaultWindow)

        defaultWindow.openNewTab()

        expectEqual(defaultWindow.tabs.count, 2)

        try await manager.run()
    }

    @Test
    func updateWithFileURLIsRejected() async throws {
        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            const secondTab = allWindows[0].tabs[1]

            await browser.test.assertRejects(browser.tabs.update(secondTab.id, { url: 'file:///tmp/test.html' }), /it cannot be a local file URL/i)

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        try requireNonNil(manager.defaultWindow).openNewTab()

        try await manager.run()
    }

    @Test
    func updateWithJavaScriptURLIsRejected() async throws {
        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            const secondTab = allWindows[0].tabs[1]

            await browser.test.assertRejects(browser.tabs.update(secondTab.id, { url: 'javascript:void(0)' }), /it cannot be a JavaScript URL/i)

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        try requireNonNil(manager.defaultWindow).openNewTab()

        try await manager.run()
    }

    @Test
    func updateWithFileURLWhenFileAccessIsAllowed() async throws {
        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            const secondTab = allWindows[0].tabs[1]

            await browser.tabs.update(secondTab.id, { url: 'file:///tmp/test.html' })

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
        let context = try requireNonNil(manager.context)

        context._hasAccessToFileURLs = true

        try requireNonNil(manager.defaultWindow).openNewTab()

        try await manager.run()
    }

    @Test
    func updateWithDataURL() async throws {
        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            const secondTab = allWindows[0].tabs[1]

            await browser.tabs.update(secondTab.id, { url: 'data:text/html,<p>Hello</p>' })

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        try requireNonNil(manager.defaultWindow).openNewTab()

        try await manager.run()
    }

    @Test
    func updateWithoutTabId() async throws {
        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            const activeTab = allWindows[0].tabs[0]

            browser.test.assertFalse(activeTab.mutedInfo.muted, 'The tab should not be initially muted')

            const updatedTab = await browser.tabs.update({
              muted: true,
            })

            browser.test.assertTrue(updatedTab.mutedInfo.muted, 'The tab should be muted after update')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        expectEqual(manager.defaultWindow?.tabs.count, 1)

        try await manager.run()
    }

    @Test
    func get() async throws {
        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            const windowId = allWindows[0].id
            const tabId = allWindows[0].tabs[0].id

            const tab = await browser.tabs.get(tabId)

            browser.test.assertEq(typeof tab, 'object', 'The tab should be an object')
            browser.test.assertEq(tab.id, tabId, 'The tab id should match the one used with tabs.get()')
            browser.test.assertEq(tab.windowId, windowId, 'The tab windowId should match the window id from windows.getAll()')
            browser.test.assertEq(tab.index, 0, 'The tab index should be 0')
            browser.test.assertEq(tab.status, 'complete', 'The tab status should be complete')
            browser.test.assertTrue(tab.active, 'The tab active state should be true')
            browser.test.assertTrue(tab.selected, 'The tab selected state should be true')
            browser.test.assertTrue(tab.highlighted, 'The tab highlighted state should be true')
            browser.test.assertFalse(tab.incognito, 'The tab incognito state should be false')
            browser.test.assertEq(tab.url, '', 'The tab url should be an empty string')
            browser.test.assertEq(tab.title, '', 'The tab title should be an empty string')
            browser.test.assertEq(tab.width, 800, 'The tab width should be 800')
            browser.test.assertEq(tab.height, 600, 'The tab height should be 600')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
        let defaultWindow = try requireNonNil(manager.defaultWindow)

        defaultWindow.openNewTab()

        expectEqual(defaultWindow.tabs.count, 2)

        try await manager.run()
    }

    @Test
    func getCurrentFromBackgroundPage() async throws {
        let backgroundScript = """
            const [currentTab] = await browser.tabs.query({ active: true, currentWindow: true })
            const tab = await browser.tabs.getCurrent()

            browser.test.assertDeepEq(tab, currentTab, 'The current tab should be')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
        let defaultWindow = try requireNonNil(manager.defaultWindow)

        defaultWindow.openNewTab()

        expectEqual(defaultWindow.tabs.count, 2)

        try await manager.run()
    }

    @Test
    func getCurrentFromOptionsPage() async throws {
        let optionsScript = """
            const tab = await browser.tabs.getCurrent()

            browser.test.assertEq(typeof tab, 'object', 'The tab should be')
            browser.test.assertTrue(tab.active, 'The current tab should be active')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": "// Not Used",
            "options.html": "<script type='module' src='options.js'></script>",
            "options.js": optionsScript,
        ]

        let manager = try loadWebExtension(manifest: tabsManifest, resources: resources)
        let context = try requireNonNil(manager.context)
        let defaultWindow = try requireNonNil(manager.defaultWindow)

        defaultWindow.openNewTab()

        expectEqual(defaultWindow.tabs.count, 2)

        let optionsPageURL = try requireNonNil(context.optionsPageURL)

        let defaultTab = try requireNonNil(manager.defaultTab)

        defaultTab.changeWebViewIfNeeded(for: optionsPageURL, for: context)
        try requireNonNil(defaultTab.webView).load(URLRequest(url: optionsPageURL))

        try await manager.run()
    }

    @Test
    func getSelected() async throws {
        let backgroundScript = """
            const [selectedTab] = await browser.tabs.query({ active: true, currentWindow: true })
            const tab = await browser.tabs.getSelected()

            browser.test.assertDeepEq(tab, selectedTab, 'The selected tab should match the active tab in the current window')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifestV2, resources: ["background.js": backgroundScript])
        let defaultWindow = try requireNonNil(manager.defaultWindow)

        defaultWindow.openNewTab()

        expectEqual(defaultWindow.tabs.count, 2)

        try await manager.run()
    }

    @Test
    func query() async throws {
        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            browser.test.assertEq(allWindows?.length, 2, 'There should be 2 windows')

            const windowIdOne = allWindows?.[0]?.id
            const windowIdTwo = allWindows?.[1]?.id

            const tabIdOne = allWindows?.[0]?.tabs?.[0]?.id
            const tabIdTwo = allWindows?.[0]?.tabs?.[1]?.id
            const tabIdThree = allWindows?.[1]?.tabs?.[0]?.id
            const tabIdFour = allWindows?.[1]?.tabs?.[1]?.id
            const tabIdFive = allWindows?.[1]?.tabs?.[2]?.id

            const tabsInWindowOne = await browser.tabs.query({ windowId: windowIdOne })
            const tabsInWindowTwo = await browser.tabs.query({ windowId: windowIdTwo })
            browser.test.assertEq(tabsInWindowOne?.length, 2, 'There should be 2 tabs in the first window')
            browser.test.assertEq(tabsInWindowTwo?.length, 3, 'There should be 3 tabs in the second window')

            const thirdTab = await browser.tabs.query({ index: 0, windowId: windowIdTwo })
            browser.test.assertEq(thirdTab?.[0]?.id, tabIdThree, 'Third tab ID should match the first tab of the second window')

            const activeTabs = await browser.tabs.query({ active: true })
            browser.test.assertEq(activeTabs?.length, 2, 'There should be 2 active tabs across all windows')

            const hiddenTabs = await browser.tabs.query({ hidden: true })
            browser.test.assertEq(hiddenTabs?.length, 3, 'There should be 3 hidden tabs across all windows')

            const lastFocusedTabs = await browser.tabs.query({ lastFocusedWindow: true })
            browser.test.assertEq(lastFocusedTabs?.length, 2, 'There should be 2 tabs in the last focused window')

            const pinnedTabs = await browser.tabs.query({ pinned: true })
            browser.test.assertEq(pinnedTabs?.length, 0, 'There should be no pinned tabs')

            const loadingTabs = await browser.tabs.query({ status: 'loading' })
            browser.test.assertEq(loadingTabs?.length, 0, 'There should be no tabs loading')

            const completeTabs = await browser.tabs.query({ status: 'complete' })
            browser.test.assertEq(completeTabs?.length, 5, 'There should be 5 tabs with loading complete')

            browser.test.notifyPass()
            """

        let manager = parseWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        let windowOne = try requireNonNil(manager.defaultWindow)
        windowOne.openNewTab()

        let windowTwo = manager.openNewWindow()
        windowTwo.openNewTab()
        windowTwo.openNewTab()

        let windowThree = manager.openNewWindow(usingPrivateBrowsing: true)
        windowThree.openNewTab()

        expectEqual(manager.windows.count, 3)
        expectEqual(windowOne.tabs.count, 2)
        expectEqual(windowTwo.tabs.count, 3)
        expectEqual(windowThree.tabs.count, 2)

        try await manager.loadAndRun()
    }

    @Test
    func queryWithPrivateAccess() async throws {
        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            const normalWindows = allWindows.filter(window => !window.incognito)
            const incognitoWindows = allWindows.filter(window => window.incognito)

            const windowIdOne = normalWindows?.[0]?.id
            const windowIdTwo = normalWindows?.[1]?.id
            const incognitoWindowId = incognitoWindows?.[0]?.id

            const tabIdOne = normalWindows?.[0]?.tabs?.[0]?.id
            const tabIdTwo = normalWindows?.[0]?.tabs?.[1]?.id
            const tabIdThree = normalWindows?.[1]?.tabs?.[0]?.id
            const tabIdFour = normalWindows?.[1]?.tabs?.[1]?.id
            const tabIdFive = normalWindows?.[1]?.tabs?.[2]?.id
            const incognitoTabIdOne = incognitoWindows?.[0]?.tabs?.[0]?.id
            const incognitoTabIdTwo = incognitoWindows?.[0]?.tabs?.[1]?.id

            const tabsInWindowOne = await browser.tabs.query({ windowId: windowIdOne })
            const tabsInWindowTwo = await browser.tabs.query({ windowId: windowIdTwo })
            const tabsInIncognitoWindow = await browser.tabs.query({ windowId: incognitoWindowId })

            browser.test.assertEq(tabsInWindowOne?.length, 2, 'There should be 2 tabs in the first normal window')
            browser.test.assertEq(tabsInWindowTwo?.length, 3, 'There should be 3 tabs in the second normal window')
            browser.test.assertEq(tabsInIncognitoWindow?.length, 2, 'There should be 2 tabs in the incognito window')

            const thirdTab = await browser.tabs.query({ index: 0, windowId: windowIdTwo })
            browser.test.assertEq(thirdTab?.[0]?.id, tabIdThree, 'Third tab ID should match the first tab of the second window')

            const activeTabs = await browser.tabs.query({ active: true })
            browser.test.assertEq(activeTabs?.length, 3, 'There should be 3 active tabs across all windows')

            const hiddenTabs = await browser.tabs.query({ hidden: true })
            browser.test.assertEq(hiddenTabs?.length, 4, 'There should be 4 hidden tabs across all windows')

            const lastFocusedTabs = await browser.tabs.query({ lastFocusedWindow: true })
            browser.test.assertEq(lastFocusedTabs?.length, 2, 'There should be 2 tabs in the last focused window')

            const pinnedTabs = await browser.tabs.query({ pinned: true })
            browser.test.assertEq(pinnedTabs?.length, 0, 'There should be no pinned tabs')

            const loadingTabs = await browser.tabs.query({ status: 'loading' })
            browser.test.assertEq(loadingTabs?.length, 0, 'There should be no tabs loading')

            const completeTabs = await browser.tabs.query({ status: 'complete' })
            browser.test.assertEq(completeTabs?.length, 7, 'There should be 7 tabs with loading complete')

            browser.test.notifyPass()
            """

        let manager = parseWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
        let context = try requireNonNil(manager.context)

        context.hasAccessToPrivateData = true

        let windowOne = try requireNonNil(manager.defaultWindow)
        windowOne.openNewTab()

        let windowTwo = manager.openNewWindow()
        windowTwo.openNewTab()
        windowTwo.openNewTab()

        let windowThree = manager.openNewWindow(usingPrivateBrowsing: true)
        windowThree.openNewTab()

        expectEqual(manager.windows.count, 3)
        expectEqual(windowOne.tabs.count, 2)
        expectEqual(windowTwo.tabs.count, 3)
        expectEqual(windowThree.tabs.count, 2)

        try await manager.loadAndRun()
    }

    @Test
    func queryWithAccessPrompt() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            const windowIdOne = allWindows?.[0]?.id

            const tabIdOne = allWindows?.[0]?.tabs?.[0]?.id
            const tabIdTwo = allWindows?.[0]?.tabs?.[1]?.id

            browser.test.assertTrue(allWindows?.[0]?.tabs?.[0]?.url.startsWith('http://localhost'), 'First tab URL should be localhost')
            browser.test.assertEq(allWindows?.[0]?.tabs?.[1]?.url, '', 'Second tab URL should be empty')

            const tabsInWindowOne = await browser.tabs.query({ windowId: windowIdOne })

            browser.test.assertTrue(tabsInWindowOne.length >= 2, 'There should be at least 2 tabs in window one')
            browser.test.assertTrue(tabsInWindowOne[0].url.startsWith('http://localhost'), 'First tab URL should be localhost')
            browser.test.assertEq(tabsInWindowOne[1].url, '', 'Second tab URL should be empty')

            browser.test.notifyPass()
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: .tabs)

            var firstPermissionRequest = true
            let localhostURL = configuration.localhostAddress
            let loopbackURL = configuration.address

            var requestedURLSets: [Set<URL>] = []

            manager.internalDelegate.promptForPermissionToAccessURLs = { _, requestedURLs, completionHandler in
                requestedURLSets.append(requestedURLs)

                if firstPermissionRequest {
                    // Only approve localhost for the first request.
                    DispatchQueue.main.async {
                        completionHandler([localhostURL], nil)
                    }

                    firstPermissionRequest = false
                } else {
                    // Approve nothing for the second request.
                    DispatchQueue.main.async {
                        completionHandler([], nil)
                    }
                }
            }

            let windowOne = try requireNonNil(manager.defaultWindow)
            windowOne.openNewTab()

            try requireNonNil(windowOne.tabs.first?.webView).load(URLRequest(url: configuration.localhostAddress))
            try requireNonNil(windowOne.tabs.last?.webView).load(URLRequest(url: configuration.address))

            expectEqual(manager.windows.count, 1)
            expectEqual(windowOne.tabs.count, 2)

            try await manager.run()

            for (index, requestedURLs) in requestedURLSets.enumerated() {
                if index == 0 {
                    expectTrue(requestedURLs.contains(localhostURL))
                    expectTrue(requestedURLs.contains(loopbackURL))
                } else {
                    // The second request should only have loopbackURL, as localhostURL was granted previously.
                    expectFalse(requestedURLs.contains(localhostURL))
                    expectTrue(requestedURLs.contains(loopbackURL))
                }
            }
        }
    }

    @Test
    func queryWithCurrentWindow() async throws {
        let backgroundScript = """
            const tabs = await browser.tabs.query({ windowId: browser.windows.WINDOW_ID_CURRENT })
            browser.test.assertEq(tabs.length, 2, 'Should return exactly two tabs for the current window')

            browser.test.assertEq(typeof tabs[0].id, 'number', 'The first tab should have a valid id')
            browser.test.assertEq(typeof tabs[0].windowId, 'number', 'The first tab should have a valid windowId')

            browser.test.assertEq(typeof tabs[1].id, 'number', 'The second tab should have a valid id')
            browser.test.assertEq(typeof tabs[1].windowId, 'number', 'The second tab should have a valid windowId')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
        let defaultWindow = try requireNonNil(manager.defaultWindow)

        defaultWindow.openNewTab()

        expectEqual(defaultWindow.tabs.count, 2)

        try await manager.run()
    }

    @Test
    func queryWithPermissionBypass() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            const windowIdOne = allWindows?.[0]?.id

            const tabIdOne = allWindows?.[0]?.tabs?.[0]?.id
            const tabIdTwo = allWindows?.[0]?.tabs?.[1]?.id

            browser.test.assertTrue(allWindows?.[0]?.tabs?.[0]?.url.startsWith('http://localhost'), 'First tab URL should be localhost')
            browser.test.assertEq(allWindows?.[0]?.tabs?.[1]?.url, '', 'Second tab URL should be empty')

            const tabsInWindowOne = await browser.tabs.query({ windowId: windowIdOne })

            browser.test.assertTrue(tabsInWindowOne.length >= 2, 'There should be at least 2 tabs in window one')
            browser.test.assertTrue(tabsInWindowOne[0].url.startsWith('http://localhost'), 'First tab URL should be localhost')
            browser.test.assertEq(tabsInWindowOne[1].url, '', 'Second tab URL should be empty')

            browser.test.notifyPass()
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: .tabs)

            manager.internalDelegate.promptForPermissionToAccessURLs = { _, _, completionHandler in
                DispatchQueue.main.async {
                    completionHandler([], nil)
                }
            }

            let windowOne = try requireNonNil(manager.defaultWindow)
            windowOne.openNewTab()
            windowOne.tabs.first?.shouldBypassPermissions = true

            try requireNonNil(windowOne.tabs.first?.webView).load(URLRequest(url: configuration.localhostAddress))
            try requireNonNil(windowOne.tabs.last?.webView).load(URLRequest(url: configuration.address))

            expectEqual(manager.windows.count, 1)
            expectEqual(windowOne.tabs.count, 2)

            try await manager.run()
        }
    }

    // MARK: - Zoom, Reader Mode, Language Detection, Capture, Reload, and History

    @Test
    func zoom() async throws {
        try await loadAndRun(
            backgroundScript: """
                const allWindows = await browser.windows.getAll({ populate: true })
                const windowId = allWindows[0].id
                const tabId = allWindows[0].tabs[0].id

                await browser.tabs.setZoom(tabId, 1.5)

                const zoomLevel = await browser.tabs.getZoom(tabId)

                browser.test.assertEq(typeof zoomLevel, 'number', 'The zoomLevel should be a number')
                browser.test.assertEq(zoomLevel, 1.5, 'The tab zoom level should be 1.5')

                browser.test.notifyPass()
                """
        )
    }

    @Test
    func toggleReaderMode() async throws {
        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            const tabId = allWindows[0].tabs[0].id

            await browser.tabs.toggleReaderMode(tabId)

            let tab = await browser.tabs.get(tabId)
            browser.test.assertTrue(tab.isInReaderMode, 'The tab should be in reader mode after toggling')

            await browser.tabs.toggleReaderMode(tabId)

            tab = await browser.tabs.get(tabId)
            browser.test.assertFalse(tab.isInReaderMode, 'The tab should not be in reader mode after toggling again')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        var toggleReaderModeCounter = 0

        try requireNonNil(manager.defaultTab).setReaderModeShowing = { _ in
            toggleReaderModeCounter += 1
        }

        try await manager.run()

        try requireEqual(toggleReaderModeCounter, 2)
    }

    @Test
    func toggleReaderModeUnavailable() async throws {
        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            const tabId = allWindows[0].tabs[0].id

            await browser.test.assertRejects(browser.tabs.toggleReaderMode(tabId), /cannot be placed into reader mode/i)

            const tab = await browser.tabs.get(tabId)
            browser.test.assertFalse(tab.isInReaderMode, 'The tab should not be in reader mode')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
        let defaultTab = try requireNonNil(manager.defaultTab)

        defaultTab.readerModeUnavailable = true

        var toggleReaderModeCounter = 0

        defaultTab.setReaderModeShowing = { _ in
            toggleReaderModeCounter += 1
        }

        try await manager.run()

        try requireEqual(toggleReaderModeCounter, 0)
    }

    @Test
    func detectLanguage() async throws {
        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            const tabId = allWindows[0].tabs[0].id

            const detectedLanguage = await browser.tabs.detectLanguage(tabId)

            browser.test.assertEq(detectedLanguage, 'en-US', 'The detected language should be English')

            browser.test.notifyPass()
            """

        try await withEmptyPage(manifest: tabsManifest, resources: ["background.js": backgroundScript]) { manager, url in
            let defaultTab = try requireNonNil(manager.defaultTab)

            try requireNonNil(defaultTab.webView).load(URLRequest(url: url))

            var detectWebpageLocaleCalled = false

            defaultTab.webpageLocale = {
                detectWebpageLocaleCalled = true
                return Locale(identifier: "en-US")
            }

            try await manager.run()

            try requireEqual(detectWebpageLocaleCalled, true)
        }
    }

    @Test
    func captureVisibleTab() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        try await server.run { configuration in
            let url = configuration.localhostAddress

            let backgroundScript = """
                const newTab = await browser.tabs.create({ url: '\(url.absoluteString)' })

                let dataURLPNG = await browser.tabs.captureVisibleTab(newTab.windowId, { format: 'png' })
                browser.test.assertTrue(dataURLPNG.startsWith('data:image/png;'), 'The data URL should start with the PNG mime type for PNG capture')

                let dataURLJPEG = await browser.tabs.captureVisibleTab(newTab.windowId, { format: 'jpeg', quality: 90 })
                browser.test.assertTrue(dataURLJPEG.startsWith('data:image/jpeg;'), 'The data URL should start with the JPEG mime type for JPEG capture')

                browser.test.notifyPass()
                """

            let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: url)

            try await manager.run()
        }
    }

    @Test
    func reload() async throws {
        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            const tabId = allWindows[0].tabs[0].id

            await browser.tabs.reload(tabId)
            await browser.tabs.reload(tabId, { bypassCache: true })

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        var reloadCalled = false
        var reloadFromOriginCalled = false

        try requireNonNil(manager.defaultTab).reload = { fromOrigin in
            if fromOrigin {
                reloadFromOriginCalled = true
            } else {
                reloadCalled = true
            }
        }

        try await manager.run()

        try requireEqual(reloadCalled, true)
        try requireEqual(reloadFromOriginCalled, true)
    }

    @Test
    func goBack() async throws {
        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            const tabId = allWindows[0].tabs[0].id

            await browser.tabs.goBack(tabId)

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        var goBackCalled = false

        try requireNonNil(manager.defaultTab).goBack = {
            goBackCalled = true
        }

        try await manager.run()

        try requireEqual(goBackCalled, true)
    }

    @Test
    func goForward() async throws {
        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            const tabId = allWindows[0].tabs[0].id

            await browser.tabs.goForward(tabId)

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        var goForwardCalled = false

        try requireNonNil(manager.defaultTab).goForward = {
            goForwardCalled = true
        }

        try await manager.run()

        try requireEqual(goForwardCalled, true)
    }

    // MARK: - Remove and Move

    @Test
    func remove() async throws {
        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            const tabIdToRemove = allWindows[0].tabs[1].id

            await browser.tabs.remove(tabIdToRemove)

            const updatedAllWindows = await browser.windows.getAll({ populate: true })
            const remainingTabs = updatedAllWindows[0].tabs.map(tab => tab.id)

            browser.test.assertFalse(remainingTabs.includes(tabIdToRemove), 'The tab should be removed')
            await browser.test.assertRejects(browser.tabs.get(tabIdToRemove), /tab not found/i, 'The removed tab should not be retrievable with tabs.get()')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
        let defaultWindow = try requireNonNil(manager.defaultWindow)

        defaultWindow.openNewTab()

        expectEqual(defaultWindow.tabs.count, 2)

        try await manager.run()
    }

    @Test
    func removeMultipleTabs() async throws {
        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            const tabIdsToRemove = [allWindows[0].tabs[1].id, allWindows[0].tabs[2].id]

            await browser.tabs.remove(tabIdsToRemove)

            const updatedAllWindows = await browser.windows.getAll({ populate: true })
            const remainingTabs = updatedAllWindows[0].tabs.map(tab => tab.id)

            for (let id of tabIdsToRemove) {
                browser.test.assertFalse(remainingTabs.includes(id), 'Removed tabs should not be retrievable')
            }

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
        let defaultWindow = try requireNonNil(manager.defaultWindow)

        defaultWindow.openNewTab()
        defaultWindow.openNewTab()

        expectEqual(defaultWindow.tabs.count, 3)

        try await manager.run()
    }

    @Test
    func removeMultipleInvalidTabs() async throws {
        let backgroundScript = """
            const allWindows = await browser.windows.getAll({ populate: true })
            const tabs = allWindows[0].tabs

            const validTabId = tabs[1].id

            await browser.test.assertRejects(browser.tabs.remove([9999, 9998]), /tab '9999' was not found/i, 'Removing multiple invalid tabs should reject once with the first invalid id')

            // A valid id batched with an invalid id should also reject without removing anything
            await browser.test.assertRejects(browser.tabs.remove([validTabId, 9999]), /tab '9999' was not found/i, 'A trailing invalid id should reject and remove nothing')
            await browser.test.assertRejects(browser.tabs.remove([9999, validTabId]), /tab '9999' was not found/i, 'A leading invalid id should reject and remove nothing')

            const remainingTabs = (await browser.windows.getAll({ populate: true }))[0].tabs.map(tab => tab.id)
            browser.test.assertEq(remainingTabs.length, tabs.length, 'No tabs should be removed when an invalid id is provided')
            browser.test.assertTrue(remainingTabs.includes(validTabId), 'The valid tab should not have been removed')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
        let defaultWindow = try requireNonNil(manager.defaultWindow)

        defaultWindow.openNewTab()
        defaultWindow.openNewTab()

        expectEqual(defaultWindow.tabs.count, 3)

        try await manager.run()
    }

    @Test
    func moveSingleTabInSingleWindow() async throws {
        let backgroundScript = """
            const win = (await browser.windows.getAll({ populate: true }))[0]
            const tabIds = win.tabs.map(tab => tab.id)
            browser.test.assertEq(win.tabs.length, 3, 'There should be 3 tabs')

            // Move the first tab to the end of the window...
            const moved = await browser.tabs.move(tabIds[0], { index: 2 })

            browser.test.assertFalse(Array.isArray(moved), 'Moving a single tab should resolve with a Tab object, not an array')
            browser.test.assertEq(moved.id, tabIds[0], 'The moved tab should be returned')
            browser.test.assertEq(moved.index, 2, 'The moved tab should now be at index 2')

            const movedOrder = (await browser.tabs.query({ windowId: win.id })).map(tab => tab.id)
            browser.test.assertEq(JSON.stringify(movedOrder), JSON.stringify([tabIds[1], tabIds[2], tabIds[0]]), 'Tab order should reflect the move')

            // ...then move it back to the front.
            const movedBack = await browser.tabs.move(tabIds[0], { index: 0 })
            browser.test.assertEq(movedBack.index, 0, 'The tab should move back to index 0')

            const restoredOrder = (await browser.tabs.query({ windowId: win.id })).map(tab => tab.id)
            browser.test.assertEq(JSON.stringify(restoredOrder), JSON.stringify([tabIds[0], tabIds[1], tabIds[2]]), 'Tab order should be restored')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
        let defaultWindow = try requireNonNil(manager.defaultWindow)

        defaultWindow.openNewTab()
        defaultWindow.openNewTab()

        expectEqual(defaultWindow.tabs.count, 3)

        try await manager.run()
    }

    @Test
    func moveSingleTabToEndOfWindow() async throws {
        let backgroundScript = """
            const win = (await browser.windows.getAll({ populate: true }))[0]
            const tabIds = win.tabs.map(tab => tab.id)

            const moved = await browser.tabs.move(tabIds[0], { index: -1 })
            browser.test.assertEq(moved.index, tabIds.length - 1, 'index:-1 should move the tab to the last position')

            const order = (await browser.tabs.query({ windowId: win.id })).map(tab => tab.id)
            browser.test.assertEq(order[order.length - 1], tabIds[0], 'The moved tab should be last')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
        let defaultWindow = try requireNonNil(manager.defaultWindow)

        defaultWindow.openNewTab()
        defaultWindow.openNewTab()

        try await manager.run()
    }

    @Test
    func moveOverflowIndexClampsToEnd() async throws {
        let backgroundScript = """
            const win = (await browser.windows.getAll({ populate: true }))[0]
            const tabIds = win.tabs.map(tab => tab.id)

            const moved = await browser.tabs.move(tabIds[0], { index: 9999 })
            browser.test.assertEq(moved.index, tabIds.length - 1, 'An out-of-range index should clamp to the last position')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
        let defaultWindow = try requireNonNil(manager.defaultWindow)

        defaultWindow.openNewTab()
        defaultWindow.openNewTab()

        try await manager.run()
    }

    @Test
    func moveToCurrentIndexReturnsTab() async throws {
        let backgroundScript = """
            const win = (await browser.windows.getAll({ populate: true }))[0]
            const tabId = win.tabs[1].id

            const moved = await browser.tabs.move(tabId, { index: 1 })
            browser.test.assertFalse(Array.isArray(moved), 'A no-op move should still resolve with a Tab object')
            browser.test.assertEq(moved.id, tabId, 'The tab should be returned even when its position is unchanged')
            browser.test.assertEq(moved.index, 1, 'The tab should remain at its index')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
        let defaultWindow = try requireNonNil(manager.defaultWindow)

        defaultWindow.openNewTab()
        defaultWindow.openNewTab()

        try await manager.run()
    }

    @Test
    func moveMultipleTabsReturnsArray() async throws {
        let backgroundScript = """
            const windows = await browser.windows.getAll({ populate: true })
            const source = windows[0]
            const destination = windows[1]
            const movingIds = [source.tabs[0].id, source.tabs[1].id]

            const moved = await browser.tabs.move(movingIds, { windowId: destination.id, index: 0 })

            browser.test.assertTrue(Array.isArray(moved), 'Moving multiple tabs should resolve with an array')
            browser.test.assertEq(moved.length, 2, 'There should be one returned entry per moved tab')
            for (const tab of moved)
              browser.test.assertEq(tab.windowId, destination.id, 'Each moved tab should report the destination window')

            const destIds = (await browser.tabs.query({ windowId: destination.id })).map(tab => tab.id)
            // The moved tabs should occupy index 0 and 1, in the order they were given.
            browser.test.assertEq(destIds[0], movingIds[0], 'The first moved tab should be at the destination index')
            browser.test.assertEq(destIds[1], movingIds[1], 'The second moved tab should follow the first, preserving order')

            browser.test.notifyPass()
            """

        let manager = parseWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
        let defaultWindow = try requireNonNil(manager.defaultWindow)

        defaultWindow.openNewTab()
        defaultWindow.openNewTab()

        let destination = manager.openNewWindow()
        destination.openNewTab()

        expectEqual(defaultWindow.tabs.count, 3)

        try await manager.loadAndRun()
    }

    @Test
    func moveWithoutWindowIdGroupsTabsByWindow() async throws {
        let backgroundScript = """
            const windows = await browser.windows.getAll({ populate: true })
            const first = windows[0]
            const second = windows[1]

            // Take the last tab of each window. With no windowId, each tab should move to index 0 within its own current window
            const firstMovingId = first.tabs[first.tabs.length - 1].id
            const secondMovingId = second.tabs[second.tabs.length - 1].id

            const moved = await browser.tabs.move([firstMovingId, secondMovingId], { index: 0 })

            browser.test.assertTrue(Array.isArray(moved), 'Moving multiple tabs should resolve with an array')
            browser.test.assertEq(moved.length, 2, 'Both tabs should be returned')

            const movedFirst = moved.find(tab => tab.id === firstMovingId)
            const movedSecond = moved.find(tab => tab.id === secondMovingId)
            browser.test.assertEq(movedFirst.windowId, first.id, 'A tab moved without a windowId should stay in its own window')
            browser.test.assertEq(movedSecond.windowId, second.id, 'A tab moved without a windowId should stay in its own window')
            browser.test.assertEq(movedFirst.index, 0, 'The tab should move to the front of its own window')
            browser.test.assertEq(movedSecond.index, 0, 'The tab should move to the front of its own window')

            const firstOrder = (await browser.tabs.query({ windowId: first.id })).map(tab => tab.id)
            const secondOrder = (await browser.tabs.query({ windowId: second.id })).map(tab => tab.id)
            browser.test.assertEq(firstOrder[0], firstMovingId, 'The moved tab should be first in its window')
            browser.test.assertEq(secondOrder[0], secondMovingId, 'The moved tab should be first in its window')

            browser.test.notifyPass()
            """

        let manager = parseWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        try requireNonNil(manager.defaultWindow).openNewTab()

        let secondWindow = manager.openNewWindow()
        secondWindow.openNewTab()

        try await manager.loadAndRun()
    }

    @Test
    func moveToAnotherWindow() async throws {
        let backgroundScript = """
            const windows = await browser.windows.getAll({ populate: true })
            const source = windows[0]
            const destination = windows[1]
            const movingId = source.tabs[0].id

            const moved = await browser.tabs.move(movingId, { windowId: destination.id, index: -1 })
            browser.test.assertEq(moved.windowId, destination.id, 'The tab should report the destination window')

            const destIds = (await browser.tabs.query({ windowId: destination.id })).map(tab => tab.id)
            browser.test.assertEq(destIds[destIds.length - 1], movingId, 'index:-1 should append the tab to the end of the destination window')

            browser.test.notifyPass()
            """

        let manager = parseWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        try requireNonNil(manager.defaultWindow).openNewTab()

        let destination = manager.openNewWindow()
        destination.openNewTab()

        try await manager.loadAndRun()
    }

    @Test
    func moveToOrFromPopupWindowFails() async throws {
        let backgroundScript = """
            const windows = await browser.windows.getAll({ populate: true })
            const normal = windows.find(window => window.type === 'normal')
            const popup = windows.find(window => window.type === 'popup')

            // Tabs can only be moved to and from normal windows, in either direction.
            await browser.test.assertRejects(browser.tabs.move(normal.tabs[0].id, { windowId: popup.id, index: 0 }), /the destination window is not a normal window/i, 'Moving a tab into a popup window should fail')
            await browser.test.assertRejects(browser.tabs.move(popup.tabs[0].id, { index: 0 }), /it is not possible to move a tab that is not in a normal window/i, 'Moving a tab out of a popup window should fail')

            browser.test.notifyPass()
            """

        let manager = parseWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        let popup = manager.openNewWindow()
        popup.windowType = .popup
        popup.openNewTab()

        try await manager.loadAndRun()
    }

    @Test
    func moveBetweenPrivateAndNonPrivateFails() async throws {
        let backgroundScript = """
            const windows = await browser.windows.getAll({ populate: true })
            const normal = windows.find(window => !window.incognito)
            const privateWindow = windows.find(window => window.incognito)
            const normalTabId = normal.tabs[0].id
            const privateTabId = privateWindow.tabs[0].id

            await browser.test.assertRejects(browser.tabs.move(normalTabId, { windowId: privateWindow.id, index: 0 }), /private and non-private windows/i)
            await browser.test.assertRejects(browser.tabs.move(privateTabId, { windowId: normal.id, index: 0 }), /private and non-private windows/i)

            browser.test.notifyPass()
            """

        let manager = parseWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
        let context = try requireNonNil(manager.context)

        context.hasAccessToPrivateData = true

        manager.openNewWindow(usingPrivateBrowsing: true)

        try await manager.loadAndRun()
    }

    @Test
    func moveDelegateErrorIsPropagated() async throws {
        let backgroundScript = """
            const win = (await browser.windows.getAll({ populate: true }))[0]
            const tabId = win.tabs[0].id

            await browser.test.assertRejects(browser.tabs.move(tabId, { index: 0 }), /simulated move failure/i, 'An error returned by the move delegate should reject the promise')

            browser.test.notifyPass()
            """

        let manager = parseWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        manager.internalDelegate.moveTabs = { _, _, _, _, completionHandler in
            completionHandler(
                NSError(domain: "TestWebExtensionErrorDomain", code: 1, userInfo: [NSLocalizedDescriptionKey: "simulated move failure"])
            )
        }

        try await manager.loadAndRun()
    }

    @Test
    func moveMultiWindowGroupErrorIsPropagated() async throws {
        let backgroundScript = """
            const windows = await browser.windows.getAll({ populate: true })
            const first = windows[0]
            const second = windows[1]

            // One tab from each window, no windowId, so the move splits into two per-window delegate calls.
            const movingIds = [first.tabs[0].id, second.tabs[0].id]

            await browser.test.assertRejects(browser.tabs.move(movingIds, { index: 0 }), /one group failed/i, 'An error from one window group should reject the whole move')

            browser.test.notifyPass()
            """

        let manager = parseWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        let erroringWindow = manager.openNewWindow()

        // Fail only the second window's group; the first window's group succeeds. The aggregated result must
        // still reject, surfacing the failing group's error.
        var moveCallCount = 0
        manager.internalDelegate.moveTabs = { _, _, window, _, completionHandler in
            moveCallCount += 1

            if window === erroringWindow {
                completionHandler(
                    NSError(domain: "TestWebExtensionErrorDomain", code: 1, userInfo: [NSLocalizedDescriptionKey: "one group failed"])
                )
                return
            }

            completionHandler(nil)
        }

        try await manager.loadAndRun()

        expectEqual(moveCallCount, 2)
    }

    @Test
    func movePrivateTabsAllowed() async throws {
        let backgroundScript = """
            const windows = await browser.windows.getAll({ populate: true })
            const privateWindows = windows.filter(window => window.incognito)
            browser.test.assertEq(privateWindows.length, 2, 'There should be two private windows')

            const source = privateWindows[0]
            const destination = privateWindows[1]

            // A private tab can move within its own private window (no windowId).
            const withinId = source.tabs[source.tabs.length - 1].id
            const movedWithin = await browser.tabs.move(withinId, { index: 0 })
            browser.test.assertEq(movedWithin.windowId, source.id, 'A private tab should move within its own private window')
            browser.test.assertEq(movedWithin.index, 0, 'The private tab should move to the front of its window')

            // A private tab can move to another private window.
            const crossId = source.tabs[0].id
            const movedCross = await browser.tabs.move(crossId, { windowId: destination.id, index: -1 })
            browser.test.assertEq(movedCross.windowId, destination.id, 'A private tab should be allowed to move to another private window')
            browser.test.assertEq(movedCross.index, 1, 'The private tab should be appended to the end of the destination window')

            browser.test.notifyPass()
            """

        let manager = parseWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
        let context = try requireNonNil(manager.context)

        context.hasAccessToPrivateData = true

        let source = manager.openNewWindow(usingPrivateBrowsing: true)
        source.openNewTab()

        manager.openNewWindow(usingPrivateBrowsing: true)

        try await manager.loadAndRun()
    }

    @Test
    func moveErrors() async throws {
        try await loadAndRun(
            backgroundScript: """
                const win = (await browser.windows.getAll({ populate: true }))[0]
                const tabId = win.tabs[0].id

                browser.test.assertThrows(() => browser.tabs.move(0, { index: 0 }), /'tabIDs' value is invalid, because '0' is not a tab identifier/i)
                browser.test.assertThrows(() => browser.tabs.move([tabId, 0], { index: 0 }), /'tabIDs' value is invalid, because '0' is not a tab identifier/i)
                browser.test.assertThrows(() => browser.tabs.move([], { index: 0 }), /'tabIDs' value is invalid, because no tabs were specified/i)
                browser.test.assertThrows(() => browser.tabs.move(tabId), /a required argument is missing/i)
                browser.test.assertThrows(() => browser.tabs.move(tabId, null), /'properties' value is invalid, because an object is expected/i)
                browser.test.assertThrows(() => browser.tabs.move(tabId, { }), /'properties' value is invalid, because it is missing required keys: 'index'/i)
                browser.test.assertThrows(() => browser.tabs.move(tabId, { index: 'two' }), /'index' is expected to be a number, but a string was provided/i)
                browser.test.assertThrows(() => browser.tabs.move(tabId, { index: 1.5 }), /'index' value is invalid, because it must be an integer/i)
                browser.test.assertThrows(() => browser.tabs.move(tabId, { index: -2 }), /'index' value is invalid, because it must be -1 or greater/i)
                browser.test.assertThrows(() => browser.tabs.move(tabId, { index: 0, windowId: -5 }), /'windowId' value is invalid, because '-5' is not a window identifier/i)

                await browser.test.assertRejects(browser.tabs.move(9999, { index: 0 }), /tab '9999' was not found/i)
                await browser.test.assertRejects(browser.tabs.move(tabId, { index: 0, windowId: 9999 }), /window not found/i)

                browser.test.notifyPass()
                """
        )
    }

    // MARK: - Events

    @Test
    func createdEvent() async throws {
        try await loadAndRun(
            backgroundScript: """
                browser.tabs.onCreated.addListener((newTab) => {
                  browser.test.assertFalse(newTab.active, 'The new tab should not be active')
                  browser.test.assertFalse(newTab.pinned, 'The new tab should not be pinned')
                  browser.test.assertEq(newTab.index, 1, 'The new tab index should be 1')
                  browser.test.assertEq(newTab.openerTabId, undefined, 'The new tab should not have an openerTabId by default')

                  browser.test.notifyPass()
                })

                browser.tabs.create({})
                """
        )
    }

    @Test
    func updatedEvent() async throws {
        try await loadAndRun(
            backgroundScript: """
                const newTab = await browser.tabs.create({ active: false, muted: false, pinned: false })

                let mutedUpdated = false
                let pinnedUpdated = false

                const verifyUpdates = () => {
                  if (mutedUpdated && pinnedUpdated)
                    browser.test.notifyPass()
                }

                browser.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
                  if ('mutedInfo' in changeInfo) {
                    browser.test.assertEq(tabId, newTab.id, 'The updated tab should have the correct id for muted change')
                    browser.test.assertTrue(changeInfo.mutedInfo.muted, 'The tab should be muted')
                    browser.test.assertDeepEq(changeInfo.mutedInfo, tab.mutedInfo, 'The mutedInfo in changeInfo should match the mutedInfo of the tab')
                    mutedUpdated = true
                  }

                  if ('pinned' in changeInfo) {
                    browser.test.assertEq(tabId, newTab.id, 'The updated tab should have the correct id for pinned change')
                    browser.test.assertTrue(changeInfo.pinned, 'The tab should be pinned')
                    browser.test.assertEq(changeInfo.pinned, tab.pinned, 'The pinned status in changeInfo should match the pinned status of the tab')
                    pinnedUpdated = true
                  }

                  verifyUpdates()
                })

                browser.tabs.update(newTab.id, { muted: true, pinned: true })
                """
        )
    }

    @Test
    func updatedEventWithoutPrivateAccess() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
              browser.test.notifyFail('tabs.onUpdated should not fire for private tabs when no permission is granted.')
            })

            setTimeout(() => browser.test.notifyPass(), 2000)

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

            try await manager.waitForTestMessage("Load Tab")

            let privateWindow = manager.openNewWindow(usingPrivateBrowsing: true)
            try requireNonNil(privateWindow.activeTab?.webView).load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func updatedEventReportsRapidToggles() async throws {
        let backgroundScript = """
            const pinnedValues = []

            browser.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
              if (changeInfo.pinned === undefined)
                return

              browser.test.assertEq(changeInfo.pinned, tab.pinned, 'The pinned value in changeInfo should match the tab in the same event')

              pinnedValues.push(changeInfo.pinned)
              if (pinnedValues.length < 2)
                return

              browser.test.assertDeepEq(pinnedValues, [ true, false ], 'Both changes should be reported in order')
              browser.test.notifyPass()
            })

            browser.test.sendMessage('Ready')
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        try await manager.waitForTestMessage("Ready")

        let defaultTab = try requireNonNil(manager.defaultTab)

        // Pinned flips on and back off before the coalescing delay ends, and both values must reach the extension.
        // Keep both changes in one coalescing window by not suspending between them.
        defaultTab.pinned = true
        manager.controller.didChangeTabProperties(.pinned, for: defaultTab)

        defaultTab.pinned = false
        manager.controller.didChangeTabProperties(.pinned, for: defaultTab)

        try await manager.run()
    }

    @Test
    func updatedEventOmitsUnchangedProperties() async throws {
        let backgroundScript = """
            browser.tabs.onUpdated.addListener((tabId, changeInfo) => {
              browser.test.assertEq(changeInfo.pinned, undefined, 'A property that did not change should not be in changeInfo')
              browser.test.assertTrue(changeInfo.mutedInfo?.muted, 'Muted should be reported')
              browser.test.notifyPass()
            })

            browser.test.sendMessage('Ready')
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        try await manager.waitForTestMessage("Ready")

        let defaultTab = try requireNonNil(manager.defaultTab)

        // Pinned is notified without a new value, and shares the coalescing window with a real muted change.
        // Keep both changes in one coalescing window by not suspending between them.
        manager.controller.didChangeTabProperties(.pinned, for: defaultTab)

        defaultTab.muted = true
        manager.controller.didChangeTabProperties(.muted, for: defaultTab)

        try await manager.run()
    }

    @Test
    func updatedEventWithPrivateAccess() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
              browser.test.notifyPass()
            })

            setTimeout(() => browser.test.notifyFail('tabs.onUpdated did not fire for private tab.'), 2000)

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = parseWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)

            context.hasAccessToPrivateData = true

            manager.load()
            try await manager.waitForTestMessage("Load Tab")

            let privateWindow = manager.openNewWindow(usingPrivateBrowsing: true)
            try requireNonNil(privateWindow.activeTab?.webView).load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func removedEvent() async throws {
        try await loadAndRun(
            backgroundScript: """
                const newTab = await browser.tabs.create({})

                browser.tabs.onRemoved.addListener((tabId, removeInfo) => {
                  browser.test.assertEq(tabId, newTab.id, 'The removed tab should have the correct id')
                  browser.test.assertFalse(removeInfo.isWindowClosing, 'The window should not be closing')

                  browser.test.notifyPass()
                })

                browser.tabs.remove(newTab.id)
                """
        )
    }

    @Test
    func replacedEvent() async throws {
        let backgroundScript = """
            let createdTabId = null
            let removedTabId = null

            browser.tabs.onCreated.addListener((tab) => {
              browser.test.assertEq(createdTabId, null, 'No tab should be created yet')
              createdTabId = tab.id
            })

            browser.tabs.onRemoved.addListener((tabId) => {
              browser.test.assertEq(removedTabId, null, 'No tab should be removed yet')
              removedTabId = tabId
            })

            browser.tabs.onReplaced.addListener((addedTabId, removedTabId) => {
              browser.test.assertTrue(addedTabId !== removedTabId, 'The added tab should not match the removed tab')
              browser.test.assertEq(addedTabId, createdTabId, 'The added tab should match the created tab')
              browser.test.assertEq(removedTabId, initialTabId, 'The removed tab should match the initial tab')

              browser.test.notifyPass()
            })

            const allTabs = await browser.tabs.query({windowId: browser.windows.WINDOW_ID_CURRENT})
            const initialTabId = allTabs[0].id

            browser.test.sendMessage('Replace Tab')
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])

        try await manager.waitForTestMessage("Replace Tab")

        let defaultWindow = try requireNonNil(manager.defaultWindow)
        let initialTab = try requireNonNil(defaultWindow.tabs.first)
        let newTab = TestWebExtensionTab(window: defaultWindow, extensionController: manager.controller)

        defaultWindow.replaceTab(initialTab, with: newTab)
        try await manager.run()

        expectEqual(defaultWindow.tabs.count, 1)
    }

    @Test
    func movedEvent() async throws {
        let backgroundScript = """
            browser.tabs.onMoved.addListener((tabId, moveInfo) => {
              browser.test.assertEq(tabId, movedTabId, 'Moved tab id should match the provided tab id')
              browser.test.assertEq(moveInfo.fromIndex, 1, 'Tab should have been moved from index 1')
              browser.test.assertEq(moveInfo.toIndex, 2, 'Tab should have been moved to index 2')

              browser.test.notifyPass()
            })

            const allTabs = await browser.tabs.query({ currentWindow: true })
            const movedTabId = allTabs[1].id

            browser.test.sendMessage('Move Tab')
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
        let defaultWindow = try requireNonNil(manager.defaultWindow)

        let tabToMove = defaultWindow.openNewTab()
        defaultWindow.openNewTab()

        try await manager.waitForTestMessage("Move Tab")

        defaultWindow.moveTab(tabToMove, to: 2)

        try await manager.run()
    }

    @Test
    func detachedAndAttachedEvent() async throws {
        let backgroundScript = """
            let detachedTabId

            const currentWindow = await browser.windows.getCurrent()
            const currentWindowId = currentWindow.id

            const allWindows = await browser.windows.getAll()
            const newWindow = allWindows.find(window => window.id !== currentWindowId)
            const newWindowId = newWindow.id

            browser.tabs.onDetached.addListener((tabId, detachInfo) => {
              detachedTabId = tabId
              browser.test.assertEq(detachInfo.oldWindowId, currentWindowId, 'Tab should have been detached from the current window')
              browser.test.assertEq(detachInfo.oldPosition, 1, 'Tab should have been detached from index 1')
            })

            browser.tabs.onAttached.addListener((tabId, attachInfo) => {
              browser.test.assertEq(detachedTabId, tabId, 'The detached and attached tab ids should match')
              browser.test.assertEq(attachInfo.newWindowId, newWindowId, 'Tab should have been attached to the new window')
              browser.test.assertEq(attachInfo.newPosition, 0, 'Tab should have been attached at index 0')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Move Tab')
            """

        let manager = parseWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
        let defaultWindow = try requireNonNil(manager.defaultWindow)

        let tabToMove = defaultWindow.openNewTab()
        let secondWindow = manager.openNewWindow()

        expectEqual(defaultWindow.tabs.count, 2)
        expectEqual(secondWindow.tabs.count, 1)

        manager.load()
        try await manager.waitForTestMessage("Move Tab")

        secondWindow.moveTab(tabToMove, to: 0)

        expectEqual(defaultWindow.tabs.count, 1)
        expectEqual(secondWindow.tabs.count, 2)

        try await manager.run()
    }

    @Test
    func detachAndAttachToWindowIDNone() async throws {
        let backgroundScript = """
            let detachedTabId

            const currentWindow = await browser.windows.getCurrent()
            const currentWindowId = currentWindow.id

            browser.tabs.onDetached.addListener((tabId, detachInfo) => {
              detachedTabId = tabId
              browser.test.assertEq(detachInfo.oldWindowId, currentWindowId, 'Tab should have been detached from the current window')
              browser.test.assertEq(detachInfo.oldPosition, 1, 'Tab should have been detached from index 1')
            })

            browser.tabs.onAttached.addListener((tabId, attachInfo) => {
              browser.test.assertEq(detachedTabId, tabId, 'The detached and attached tab ids should match')
              browser.test.assertEq(attachInfo.newWindowId, browser.windows.WINDOW_ID_NONE, 'Tab should have been attached to WINDOW_ID_NONE')
              browser.test.assertTrue(isNaN(attachInfo.newPosition), 'Tab should have been attached at index NaN')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Detach Tab')
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
        let defaultWindow = try requireNonNil(manager.defaultWindow)

        let tabToMove = defaultWindow.openNewTab()

        expectEqual(defaultWindow.tabs.count, 2)

        try await manager.waitForTestMessage("Detach Tab")

        tabToMove.window = nil

        expectEqual(defaultWindow.tabs.count, 1)

        try await manager.run()
    }

    @Test
    func detachAndAttachFromWindowIDNone() async throws {
        let backgroundScript = """
            let detachedTabId

            const currentWindow = await browser.windows.getCurrent()
            const currentWindowId = currentWindow.id

            browser.tabs.onDetached.addListener((tabId, detachInfo) => {
              detachedTabId = tabId
              browser.test.assertEq(detachInfo.oldWindowId, browser.windows.WINDOW_ID_NONE, 'Tab should have been detached from WINDOW_ID_NONE')
              browser.test.assertTrue(isNaN(detachInfo.oldPosition), 'Tab should have been detached from index NaN')
            })

            browser.tabs.onAttached.addListener((tabId, attachInfo) => {
              browser.test.assertEq(detachedTabId, tabId, 'The detached and attached tab ids should match')
              browser.test.assertEq(attachInfo.newWindowId, currentWindowId, 'Tab should have been attached to the current window')
              browser.test.assertEq(attachInfo.newPosition, 1, 'Tab should have been attached to index 1')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Attach Tab')
            """

        let manager = try loadWebExtension(manifest: tabsManifest, resources: ["background.js": backgroundScript])
        let defaultWindow = try requireNonNil(manager.defaultWindow)

        let tabToMove = defaultWindow.openNewTab()

        expectEqual(defaultWindow.tabs.count, 2)

        // Detach before loading the extension.
        tabToMove.window = nil

        expectEqual(defaultWindow.tabs.count, 1)

        try await manager.waitForTestMessage("Attach Tab")

        tabToMove.window = defaultWindow

        expectEqual(defaultWindow.tabs.count, 2)

        try await manager.run()
    }

    @Test
    func activatedEvent() async throws {
        try await loadAndRun(
            backgroundScript: """
                const newTab = await browser.tabs.create({ active: false })

                browser.tabs.onActivated.addListener((activeInfo) => {
                  browser.test.assertEq(activeInfo.tabId, newTab.id, 'The activated tab should have the correct id')

                  browser.test.notifyPass()
                })

                browser.tabs.update(newTab.id, { active: true })
                """
        )
    }

    @Test
    func highlightedEvent() async throws {
        try await loadAndRun(
            backgroundScript: """
                const initialTabs = await browser.tabs.query({ active: true, currentWindow: true })
                const newTab = await browser.tabs.create({ active: false })

                browser.tabs.onActivated.addListener((activeInfo) => {
                  browser.test.notifyFail('The tab should be highlighted but not activated')
                })

                browser.tabs.onHighlighted.addListener((highlightInfo) => {
                  browser.test.assertTrue(highlightInfo.tabIds.includes(newTab.id), 'The highlighted tabs should contain the new tab id')
                  browser.test.assertTrue(highlightInfo.tabIds.includes(initialTabs[0].id), 'The initial tab should still be highlighted')

                  browser.test.notifyPass()
                })

                browser.tabs.update(newTab.id, { active: false, highlighted: true })
                """
        )
    }

    @Test
    func highlightedAlsoActivatesTab() async throws {
        try await loadAndRun(
            backgroundScript: """
                const newTab = await browser.tabs.create({ active: false })

                let tabActivated = false
                let tabHighlighted = false

                let checkCompletion = () => {
                  if (tabActivated && tabHighlighted)
                    browser.test.notifyPass()
                }

                browser.tabs.onActivated.addListener((activeInfo) => {
                  tabActivated = true
                  browser.test.assertEq(activeInfo.tabId, newTab.id, 'The activated tab should have the correct id')
                  checkCompletion()
                })

                browser.tabs.onHighlighted.addListener((highlightInfo) => {
                  tabHighlighted = true
                  browser.test.assertTrue(highlightInfo.tabIds.includes(newTab.id), 'The highlighted tabs should contain the new tab id')
                  checkCompletion()
                })

                browser.tabs.update(newTab.id, { highlighted: true })
                """
        )
    }

    // MARK: - Send Message

    @Test
    func sendMessage() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener(async (message, sender, sendResponse) => {
              browser.test.assertEq(message, 'Ready')

              const tabs = await browser.tabs.query({ active: true, currentWindow: true })
              const tabId = tabs[0].id

              const response = await browser.test.assertSafeResolve(() => browser.tabs.sendMessage(tabId, { content: 'Hello' }))
              browser.test.assertEq(response?.content, 'Received', 'Should get the response from the content script')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
              browser.test.assertEq(message.content, 'Hello', 'Should receive the correct message content')

              browser.test.assertEq(typeof sender, 'object', 'sender should be an object')

              browser.test.assertEq(typeof sender.url, 'string', 'sender.url should be a string')
              browser.test.assertEq(typeof sender.origin, 'string', 'sender.origin should be a string')

              browser.test.assertTrue(sender.url.startsWith('webkit-extension://'), 'sender.url should start with webkit-extension://')
              browser.test.assertTrue(sender.origin.startsWith('webkit-extension://'), 'sender.origin should start with webkit-extension://')

              browser.test.assertEq(sender.tab, undefined, 'sender.tab should be undefined')
              browser.test.assertEq(sender.windowId, undefined, 'sender.windowId should be undefined for a background page')
              browser.test.assertEq(sender.frameId, undefined, 'sender.frameId should be undefined')

              sendResponse({ content: 'Received' })
            })

            browser.runtime.sendMessage('Ready')
            """

        try await loadAndRunWithEmptyPage(
            manifest: tabsContentScriptManifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    @Test
    func sendMessageWithStructuredClone() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener(async (message, sender, sendResponse) => {
              browser.test.assertEq(message, 'Ready')

              const tabs = await browser.tabs.query({ active: true, currentWindow: true })
              const tabId = tabs[0].id

              const response = await browser.test.assertSafeResolve(() => browser.tabs.sendMessage(tabId, {
                map: new Map([[ 'Content-Language', 'en' ]]),
                buffer: new Uint8Array([ 1, 2, 3 ]).buffer
              }))

              browser.test.assertTrue(response?.date instanceof Date, 'Reply Date should arrive as a Date')
              browser.test.assertEq(response?.date?.getTime(), 1700000000000, 'Reply Date should keep its time')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
              browser.test.assertTrue(message.map instanceof Map, 'Map should arrive as a Map')
              browser.test.assertEq(message.map.get('Content-Language'), 'en', 'Map should keep its entries')
              browser.test.assertDeepEq(Array.from(new Uint8Array(message.buffer)), [ 1, 2, 3 ], 'ArrayBuffer should keep its bytes')

              sendResponse({ date: new Date(1700000000000) })
            })

            browser.runtime.sendMessage('Ready')
            """

        var manifest = tabsContentScriptManifest
        manifest["message_serialization"] = "structured_clone"

        try await loadAndRunWithEmptyPage(
            manifest: manifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    @Test
    func sendMessageWithAsyncReply() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener(async (message, sender, sendResponse) => {
              browser.test.assertEq(message, 'Ready')

              const tabs = await browser.tabs.query({ active: true, currentWindow: true })
              const tabId = tabs[0].id

              const response = await browser.test.assertSafeResolve(() => browser.tabs.sendMessage(tabId, { content: 'Hello' }))
              browser.test.assertEq(response?.content, 'Received', 'Should get the response from the content script')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
              browser.test.assertEq(message.content, 'Hello', 'Should receive the correct message content')

              browser.test.assertEq(typeof sender, 'object', 'sender should be an object')

              browser.test.assertEq(typeof sender.url, 'string', 'sender.url should be a string')
              browser.test.assertEq(typeof sender.origin, 'string', 'sender.origin should be a string')

              browser.test.assertTrue(sender.url.startsWith('webkit-extension://'), 'sender.url should start with webkit-extension://')
              browser.test.assertTrue(sender.origin.startsWith('webkit-extension://'), 'sender.origin should start with webkit-extension://')

              browser.test.assertEq(sender.tab, undefined, 'sender.tab should be undefined')
              browser.test.assertEq(sender.frameId, undefined, 'sender.frameId should be undefined')

              sendResponse({ content: 'Received' })

              return true
            })

            browser.runtime.sendMessage('Ready')
            """

        try await loadAndRunWithEmptyPage(
            manifest: tabsContentScriptManifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    @Test
    func sendMessageWithPromiseReply() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener(async (message, sender) => {
              browser.test.assertEq(message, 'Ready')

              const tabs = await browser.tabs.query({ active: true, currentWindow: true })
              const tabId = tabs[0].id

              const response = await browser.test.assertSafeResolve(() => browser.tabs.sendMessage(tabId, { content: 'Hello' }))
              browser.test.assertEq(response?.content, 'Received', 'Should get the response from the content script')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            browser.runtime.onMessage.addListener((message, sender) => {
              browser.test.assertEq(message.content, 'Hello', 'Should receive the correct message content')

              browser.test.assertEq(typeof sender, 'object', 'sender should be an object')

              browser.test.assertEq(typeof sender.url, 'string', 'sender.url should be a string')
              browser.test.assertEq(typeof sender.origin, 'string', 'sender.origin should be a string')

              browser.test.assertTrue(sender.url.startsWith('webkit-extension://'), 'sender.url should start with webkit-extension://')
              browser.test.assertTrue(sender.origin.startsWith('webkit-extension://'), 'sender.origin should start with webkit-extension://')

              browser.test.assertEq(sender.tab, undefined, 'sender.tab should be undefined')
              browser.test.assertEq(sender.frameId, undefined, 'sender.frameId should be undefined')

              return Promise.resolve({ content: 'Received' })
            })

            browser.runtime.sendMessage('Ready')
            """

        try await loadAndRunWithEmptyPage(
            manifest: tabsContentScriptManifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    @Test
    func sendMessageWithAsyncPromiseReply() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener(async (message, sender) => {
              browser.test.assertEq(message, 'Ready')

              const tabs = await browser.tabs.query({ active: true, currentWindow: true })
              const tabId = tabs[0].id

              const response = await browser.test.assertSafeResolve(() => browser.tabs.sendMessage(tabId, { content: 'Hello' }))
              browser.test.assertEq(response?.content, 'Received', 'Should get the response from the content script')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            browser.runtime.onMessage.addListener((message, sender) => {
              browser.test.assertEq(message.content, 'Hello', 'Should receive the correct message content')

              browser.test.assertEq(typeof sender, 'object', 'sender should be an object')

              browser.test.assertEq(typeof sender.url, 'string', 'sender.url should be a string')
              browser.test.assertEq(typeof sender.origin, 'string', 'sender.origin should be a string')

              browser.test.assertTrue(sender.url.startsWith('webkit-extension://'), 'sender.url should start with webkit-extension://')
              browser.test.assertTrue(sender.origin.startsWith('webkit-extension://'), 'sender.origin should start with webkit-extension://')

              browser.test.assertEq(sender.tab, undefined, 'sender.tab should be undefined')
              browser.test.assertEq(sender.frameId, undefined, 'sender.frameId should be undefined')

              return new Promise((resolve) => {
                resolve({ content: 'Received' })
              })
            })

            browser.runtime.sendMessage('Ready')
            """

        try await loadAndRunWithEmptyPage(
            manifest: tabsContentScriptManifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    @Test
    func sendMessageWithoutReply() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener(async (message, sender) => {
              const tabs = await browser.tabs.query({ active: true, currentWindow: true })
              const tabId = tabs[0].id

              const response = await browser.tabs.sendMessage(tabId, { content: 'Hello' })
              browser.test.assertEq(response, undefined, 'Should resolve with undefined as there was no reply')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            browser.runtime.onMessage.addListener((message, sender) => {
              browser.test.assertEq(message.content, 'Hello', 'Should receive the correct message content')

              browser.test.assertEq(typeof sender, 'object', 'sender should be an object')

              browser.test.assertEq(typeof sender.url, 'string', 'sender.url should be a string')
              browser.test.assertEq(typeof sender.origin, 'string', 'sender.origin should be a string')

              browser.test.assertTrue(sender.url.startsWith('webkit-extension://'), 'sender.url should start with webkit-extension://')
              browser.test.assertTrue(sender.origin.startsWith('webkit-extension://'), 'sender.origin should start with webkit-extension://')

              browser.test.assertEq(sender.tab, undefined, 'sender.tab should be undefined')
              browser.test.assertEq(sender.frameId, undefined, 'sender.frameId should be undefined')

              return false
            })

            browser.runtime.sendMessage('Ready')
            """

        try await loadAndRunWithEmptyPage(
            manifest: tabsContentScriptManifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    @Test
    func sendMessageFromBackgroundPageToFullPageExtensionContent() async throws {
        let optionsScript = """
            browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
              browser.test.assertEq(message?.content, 'Hello', 'Should receive the correct message from the background page')

              browser.test.assertEq(typeof sender, 'object', 'sender should be an object')

              browser.test.notifyPass()
            })

            browser.runtime.sendMessage({ content: 'Ready' })
            """

        let backgroundScript = """
            browser.runtime.onMessage.addListener(async (message, sender, sendResponse) => {
              browser.test.assertEq(message?.content, 'Ready', 'Should receive the correct message from the options page')

              browser.test.assertEq(typeof sender, 'object', 'sender should be an object')

              const tabs = await browser.tabs.query({ active: true, currentWindow: true })
              const tabId = tabs[0].id

              browser.tabs.sendMessage(tabId, { content: 'Hello' })
            })

            browser.test.sendMessage('Load Tab')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "options.html": "<script type='module' src='options.js'></script>",
            "options.js": optionsScript,
        ]

        let manager = try loadWebExtension(manifest: tabsManifest, resources: resources)
        let context = try requireNonNil(manager.context)

        try await manager.waitForTestMessage("Load Tab")

        let defaultWindow = try requireNonNil(manager.defaultWindow)
        defaultWindow.openNewTab()

        expectEqual(defaultWindow.tabs.count, 2)

        let optionsPageURL = try requireNonNil(context.optionsPageURL)

        let defaultTab = try requireNonNil(manager.defaultTab)

        defaultTab.changeWebViewIfNeeded(for: optionsPageURL, for: context)
        try requireNonNil(defaultTab.webView).load(URLRequest(url: optionsPageURL))

        try await manager.run()
    }

    @Test
    func sendMessageFromBackgroundToSubframe() async throws {
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
            browser.test.sendMessage('Load Tab')

            await new Promise(resolve => {
              browser.runtime.onMessage.addListener(function handler(message) {
                if (message === 'Subframe Ready') {
                  browser.runtime.onMessage.removeListener(handler)
                  resolve()
                }
              })
            })

            const tabs = await browser.tabs.query({ })
            browser.test.assertEq(tabs.length, 1)

            const response = await browser.tabs.sendMessage(tabs[0].id, { content: 'Hello Subframe' })
            browser.test.assertEq(response?.content, 'Received from subframe', 'Response should be received from subframe')

            browser.test.notifyPass()
            """

        let contentScript = """
            browser.runtime.sendMessage('Subframe Ready')

            browser.runtime.onMessage.addListener((message, sender) => {
              browser.test.assertEq(message?.content, 'Hello Subframe', 'Should receive the correct message from the background page')
              return Promise.resolve({ content: 'Received from subframe' })
            })
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
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: subframeURL)

            try await manager.waitForTestMessage("Load Tab")

            try requireNonNil(manager.defaultTab?.webView).load(URLRequest(url: mainURL))

            try await manager.run()
        }
    }

    @Test
    func sendMessageFromBackgroundToSpecificFrame() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener(async (message, sender) => {
              browser.test.assertEq(message, 'Begin Test', 'Message content should match')

              const frameId = sender?.frameId
              browser.test.assertTrue(frameId !== 0, 'frameId should not be 0 for an iframe')

              const response = await browser.tabs.sendMessage(sender.tab.id, 'Hello, iframe!', { frameId })
              browser.test.assertEq(response, 'Message received', 'Should receive response from content script')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            if (window.top === window) {
              browser.runtime.onMessage.addListener(() => {
                browser.test.notifyFail('Main frame should not receive message intended for iframe')
              })
            } else {
              browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
                browser.test.assertEq(message, 'Hello, iframe!', 'Message should be received in the iframe')
                sendResponse('Message received')
              })

              browser.runtime.sendMessage('Begin Test')
            }
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "content.js": contentScript,
        ]

        try await loadAndRunWithFramePage(resources: resources)
    }

    @Test
    func sendMessageFromBackgroundToSpecificDocument() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener(async (message, sender) => {
              browser.test.assertEq(message, 'Begin Test', 'Message content should match')

              const documentId = sender?.documentId
              browser.test.assertEq(typeof documentId, 'string', 'sender.documentId should be a string')
              browser.test.assertEq(documentId.length, 36, 'sender.documentId.length should be 36')

              const response = await browser.tabs.sendMessage(sender.tab.id, 'Hello, iframe!', { documentId })
              browser.test.assertEq(response, 'Message received', 'Should receive response from content script')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            if (window.top === window) {
              browser.runtime.onMessage.addListener(() => {
                browser.test.notifyFail('Main frame should not receive message intended for iframe')
              })
            } else {
              browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
                browser.test.assertEq(message, 'Hello, iframe!', 'Background script message should be received')
                sendResponse('Message received')
              })

              browser.runtime.sendMessage('Begin Test')
            }
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "content.js": contentScript,
        ]

        try await loadAndRunWithFramePage(resources: resources)
    }

    @Test
    func sendMessageBackAndForwardNavigation() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame'></iframe>"
            }
            Route("/frame", headerFields: ["Content-Type": "text/html"]) {
                "<p>Frame Content</p>"
            }
        }

        let backgroundScript = """
            const expectedHostnames = ['localhost', '127.0.0.1', 'localhost', '127.0.0.1']
            let step = 0

            browser.test.onMessage.addListener(async (message) => {
              browser.test.assertEq(message, 'Go', 'Message content should match')

              const [tab] = await browser.tabs.query({ active: true, currentWindow: true })
              const frames = await browser.webNavigation.getAllFrames({ tabId: tab.id })

              const responses = await Promise.all(frames.map(async (frame) => {
                return await browser.tabs.sendMessage(tab.id, 'Ping', { frameId: frame.frameId })
              }))

              browser.test.assertEq(responses.length, 2, 'Should receive 2 responses')

              const mainFrameHost = new URL(responses[0]).hostname
              const subframeHost = new URL(responses[1]).hostname

              browser.test.assertEq(mainFrameHost, expectedHostnames[step], 'Main frame host should be')
              browser.test.assertEq(subframeHost, expectedHostnames[step], 'Subframe host should be')

              ++step

              browser.test.sendMessage('Messages Sent')
            })

            browser.test.sendMessage('Ready')
            """

        let contentScript = """
            browser.runtime.onMessage.addListener((message) => {
              browser.test.assertEq(message, 'Ping', 'Message content should match')
              return Promise.resolve(window?.location?.href)
            })
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "content.js": contentScript,
        ]

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Tabs Test",
            "description": "Tabs Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "content_scripts": [
                [
                    "js": ["content.js"],
                    "matches": ["*://*/*"],
                    "all_frames": true,
                ]
            ],

            "permissions": ["webNavigation", "tabs"],
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: resources)
            let context = try requireNonNil(manager.context)

            let localhostRequest = URLRequest(url: configuration.localhostAddress)
            let altHostRequest = URLRequest(url: configuration.address)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.address)

            try await manager.waitForTestMessage("Ready")

            let webView = try requireNonNil(manager.defaultTab?.webView)

            // Load localhost page.
            try await webView.loadAndWait(localhostRequest)

            manager.sendTestMessage("Go")
            try await manager.waitForTestMessage("Messages Sent")

            // Load 127.0.0.1 page.
            try await webView.loadAndWait(altHostRequest)

            manager.sendTestMessage("Go")
            try await manager.waitForTestMessage("Messages Sent")

            // Go back to localhost.
            webView.goBack()
            try await webView._test_waitForDidFinishNavigation()

            manager.sendTestMessage("Go")
            try await manager.waitForTestMessage("Messages Sent")

            // Go forward to 127.0.0.1.
            webView.goForward()
            try await webView._test_waitForDidFinishNavigation()

            manager.sendTestMessage("Go")
            try await manager.waitForTestMessage("Messages Sent")
        }
    }

    @Test
    func sendMessageWithUserGesture() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Tabs User Gesture Test",
            "description": "Tabs User Gesture Test",
            "version": "1",
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "action": [String: Any](),
            "content_scripts": [
                [
                    "js": ["content.js"],
                    "matches": ["*://localhost/*"],
                ]
            ],
        ]

        let backgroundScript = """
            browser.action.setPopup({ popup: '' })

            browser.action.onClicked.addListener(async (tab) => {
              browser.test.assertTrue(navigator.userActivation.isActive, 'User gesture should be active at the tabs.sendMessage call site')
              await browser.test.assertSafeResolve(() => browser.tabs.sendMessage(tab.id, 'check-gesture'))
              browser.test.notifyPass()
            })

            browser.test.sendMessage('Background Ready')
            """

        let contentScript = """
            browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
              if (message !== 'check-gesture')
                return

              browser.test.assertTrue(navigator.userActivation.isActive, 'User gesture should be active in onMessage handler after tabs.sendMessage')
              sendResponse({ })
            })

            browser.test.sendMessage('Content Ready')
            """

        try await withEmptyPage(
            manifest: manifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        ) { manager, url in
            try requireNonNil(manager.defaultTab?.webView).load(URLRequest(url: url))

            try await manager.waitForTestMessage("Background Ready")
            try await manager.waitForTestMessage("Content Ready")

            try requireNonNil(manager.context).performAction(for: manager.defaultTab)

            try await manager.run()
        }
    }

    @Test
    func sendMessageWithoutUserGesture() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener(async (message, sender) => {
              if (message !== 'Ready')
                return

              const tabs = await browser.tabs.query({ active: true, currentWindow: true })
              const tabId = tabs[0].id

              browser.test.assertFalse(navigator.userActivation.isActive, 'User gesture should not be active at the tabs.sendMessage call site')
              await browser.test.assertSafeResolve(() => browser.tabs.sendMessage(tabId, 'check-gesture'))

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Background Ready')
            """

        let contentScript = """
            browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
              if (message !== 'check-gesture')
                return

              browser.test.assertFalse(navigator.userActivation.isActive, 'User gesture should not be active in onMessage handler when tabs.sendMessage was not triggered by a user action')
              sendResponse({ })
            })

            browser.test.onMessage.addListener((message) => {
              if (message !== 'Trigger')
                return

              browser.runtime.sendMessage('Ready')
            })

            browser.test.sendMessage('Content Ready')
            """

        try await withEmptyPage(
            manifest: tabsContentScriptManifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        ) { manager, url in
            try requireNonNil(manager.defaultTab?.webView).load(URLRequest(url: url))

            try await manager.waitForTestMessage("Background Ready")
            try await manager.waitForTestMessage("Content Ready")

            manager.sendTestMessage("Trigger")

            try await manager.run()
        }
    }

    // MARK: - Connect and Port

    @Test
    func connectWithUserGesture() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Tabs Connect User Gesture Test",
            "description": "Tabs Connect User Gesture Test",
            "version": "1",
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "action": [String: Any](),
            "content_scripts": [
                [
                    "js": ["content.js"],
                    "matches": ["*://localhost/*"],
                ]
            ],
        ]

        let backgroundScript = """
            browser.action.setPopup({ popup: '' })

            browser.action.onClicked.addListener(async (tab) => {
              browser.test.assertTrue(navigator.userActivation.isActive, 'User gesture should be active at the tabs.connect call site')
              browser.tabs.connect(tab.id, { name: 'gesturePort' })
            })

            browser.test.sendMessage('Background Ready')
            """

        let contentScript = """
            browser.runtime.onConnect.addListener((port) => {
              browser.test.assertTrue(navigator.userActivation.isActive, 'User gesture should be active in onConnect handler after tabs.connect')
              browser.test.notifyPass()
            })

            browser.test.sendMessage('Content Ready')
            """

        try await withEmptyPage(
            manifest: manifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        ) { manager, url in
            try requireNonNil(manager.defaultTab?.webView).load(URLRequest(url: url))

            try await manager.waitForTestMessage("Background Ready")
            try await manager.waitForTestMessage("Content Ready")

            try requireNonNil(manager.context).performAction(for: manager.defaultTab)

            try await manager.run()
        }
    }

    @Test
    func connectWithoutUserGesture() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener(async (message, sender) => {
              if (message !== 'Ready')
                return

              const tabs = await browser.tabs.query({ active: true, currentWindow: true })
              const tabId = tabs[0].id

              browser.test.assertFalse(navigator.userActivation.isActive, 'User gesture should not be active at the tabs.connect call site')
              browser.tabs.connect(tabId, { name: 'gesturePort' })
            })

            browser.test.sendMessage('Background Ready')
            """

        let contentScript = """
            browser.runtime.onConnect.addListener((port) => {
              browser.test.assertFalse(navigator.userActivation.isActive, 'User gesture should not be active in onConnect handler when tabs.connect was not called inside a user action')
              browser.test.notifyPass()
            })

            browser.test.onMessage.addListener((message) => {
              if (message !== 'Trigger')
                return
              browser.runtime.sendMessage('Ready')
            })

            browser.test.sendMessage('Content Ready')
            """

        try await withEmptyPage(
            manifest: tabsContentScriptManifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        ) { manager, url in
            try requireNonNil(manager.defaultTab?.webView).load(URLRequest(url: url))

            try await manager.waitForTestMessage("Background Ready")
            try await manager.waitForTestMessage("Content Ready")

            manager.sendTestMessage("Trigger")

            try await manager.run()
        }
    }

    @Test
    func portPostMessageWithUserGesture() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Port PostMessage User Gesture Test",
            "description": "Port PostMessage User Gesture Test",
            "version": "1",
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "action": [String: Any](),
            "content_scripts": [
                [
                    "js": ["content.js"],
                    "matches": ["*://localhost/*"],
                ]
            ],
        ]

        let backgroundScript = """
            browser.action.setPopup({ popup: '' })

            browser.action.onClicked.addListener(async (tab) => {
              browser.test.assertTrue(navigator.userActivation.isActive, 'User gesture should be active at the port.postMessage call site')

              const port = browser.tabs.connect(tab.id, { name: 'gesturePort' })
              port.postMessage('check-gesture')
            })

            browser.test.sendMessage('Background Ready')
            """

        let contentScript = """
            browser.runtime.onConnect.addListener((port) => {
              browser.test.assertTrue(navigator.userActivation.isActive, 'User gesture should be active in onConnect handler when tabs.connect was called inside onClicked')

              port.onMessage.addListener((message) => {
                if (message !== 'check-gesture')
                  return

                browser.test.assertTrue(navigator.userActivation.isActive, 'User gesture should be active in port.onMessage handler when postMessage was sent inside onClicked')

                browser.test.notifyPass()
              })
            })

            browser.test.sendMessage('Content Ready')
            """

        try await withEmptyPage(
            manifest: manifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        ) { manager, url in
            try requireNonNil(manager.defaultTab?.webView).load(URLRequest(url: url))

            try await manager.waitForTestMessage("Background Ready")
            try await manager.waitForTestMessage("Content Ready")

            try requireNonNil(manager.context).performAction(for: manager.defaultTab)

            try await manager.run()
        }
    }

    @Test
    func portPostMessageWithoutUserGesture() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener(async (message, sender) => {
              if (message !== 'Ready')
                return

              const tabs = await browser.tabs.query({ active: true, currentWindow: true })
              const tabId = tabs[0].id

              browser.test.assertFalse(navigator.userActivation.isActive, 'User gesture should not be active at the port.postMessage call site')

              const port = browser.tabs.connect(tabId, { name: 'gesturePort' })
              port.postMessage('check-gesture')
            })

            browser.test.sendMessage('Background Ready')
            """

        let contentScript = """
            browser.runtime.onConnect.addListener((port) => {
              browser.test.assertFalse(navigator.userActivation.isActive, 'User gesture should not be active in onConnect handler when tabs.connect was not called inside a user action')

              port.onMessage.addListener((message) => {
                if (message !== 'check-gesture')
                  return

                browser.test.assertFalse(navigator.userActivation.isActive, 'User gesture should not be active in port.onMessage when postMessage was not sent inside a user action')

                browser.test.notifyPass()
              })
            })

            browser.test.onMessage.addListener((message) => {
              if (message !== 'Trigger')
                return
              browser.runtime.sendMessage('Ready')
            })

            browser.test.sendMessage('Content Ready')
            """

        try await withEmptyPage(
            manifest: tabsContentScriptManifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        ) { manager, url in
            try requireNonNil(manager.defaultTab?.webView).load(URLRequest(url: url))

            try await manager.waitForTestMessage("Background Ready")
            try await manager.waitForTestMessage("Content Ready")

            manager.sendTestMessage("Trigger")

            try await manager.run()
        }
    }

    @Test
    func portPostMessageGestureFromContentScriptIsNotPropagated() async throws {
        // Gestures from content scripts are intentionally not propagated to extension pages,
        // even when the content script sends a port message inside a user gesture.

        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Port Gesture Security Test",
            "description": "Port Gesture Security Test",
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
        ]

        let backgroundScript = """
            browser.test.onMessage.addListener(async (message) => {
              if (message !== 'Connect')
                return

              const [currentTab] = await browser.tabs.query({ active: true, currentWindow: true })
              const port = browser.tabs.connect(currentTab.id, { name: 'gesturePort' })
              port.onMessage.addListener((message) => {
                if (message !== 'check-gesture')
                  return

                browser.test.assertFalse(navigator.userActivation.isActive, 'Gesture from content script port.postMessage should not propagate to background')

                browser.test.notifyPass()
              })
            })

            browser.test.sendMessage('Background Ready')
            """

        let contentScript = """
            browser.runtime.onConnect.addListener((port) => {
              browser.test.runWithUserGesture(() => {
                port.postMessage('check-gesture')
              })
            })

            browser.test.sendMessage('Content Ready')
            """

        try await withEmptyPage(
            manifest: manifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        ) { manager, url in
            try requireNonNil(manager.defaultTab?.webView).load(URLRequest(url: url))

            try await manager.waitForTestMessage("Background Ready")
            try await manager.waitForTestMessage("Content Ready")

            manager.sendTestMessage("Connect")

            try await manager.run()
        }
    }

    @Test
    func connect() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener(async (message, sender) => {
              browser.test.assertEq(message?.readyToConnect, true, 'Should be ready to connect')

              const tabs = await browser.tabs.query({ active: true, currentWindow: true })
              const tabId = tabs[0].id

              const port = browser.tabs.connect(tabId, { name: 'testPort' })
              browser.test.assertEq(typeof port, 'object', 'Port should be an object')
              browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')
              browser.test.assertEq(port?.sender, null, 'Port sender should be null')

              port.postMessage('Hello')

              port.onMessage.addListener((response) => {
                browser.test.assertEq(response, 'Received', 'Should get the response from the content script')

                browser.test.notifyPass()
              })
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            browser.runtime.onConnect.addListener((port) => {
              browser.test.assertEq(port.name, 'testPort', 'Port name should be testPort')
              browser.test.assertEq(port.error, null, 'Port error should be null')

              browser.test.assertEq(typeof port.sender, 'object', 'sender should be an object')
              browser.test.assertEq(typeof port.sender.url, 'string', 'sender.url should be a string')
              browser.test.assertEq(typeof port.sender.origin, 'string', 'sender.origin should be a string')
              browser.test.assertTrue(port.sender.url.startsWith('webkit-extension://'), 'sender.url should start with webkit-extension://')
              browser.test.assertTrue(port.sender.origin.startsWith('webkit-extension://'), 'sender.origin should start with webkit-extension://')
              browser.test.assertEq(port.sender.tab, undefined, 'sender.tab should be undefined')
              browser.test.assertEq(port.sender.frameId, undefined, 'sender.frameId should be undefined')

              port.onMessage.addListener((message) => {
                browser.test.assertEq(message, 'Hello', 'Should receive the correct message content')
                port.postMessage('Received')
              })
            })

            browser.runtime.sendMessage({ readyToConnect: true })
            """

        try await loadAndRunWithEmptyPage(
            manifest: tabsContentScriptManifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    @Test
    func connectToSpecificFrame() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener(async (message, sender) => {
              browser.test.assertEq(message, 'Begin Test', 'Message content should match')

              const frameId = sender?.frameId
              browser.test.assertTrue(frameId !== 0, 'frameId should not be 0 for an iframe')

              const port = browser.tabs.connect(sender.tab.id, { frameId })
              browser.test.assertEq(typeof port, 'object', 'Port should be an object')

              port.onMessage.addListener((response) => {
                browser.test.assertEq(response, 'Message received', 'Should receive response from content script')
                browser.test.notifyPass()
              })

              port.postMessage('Hello, iframe!')
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            if (window.top === window) {
              browser.runtime.onConnect.addListener(() => {
                browser.test.notifyFail('Main frame should not receive connection intended for iframe')
              })
            } else {
              browser.runtime.onConnect.addListener((port) => {
                port.onMessage.addListener((message) => {
                  browser.test.assertEq(message, 'Hello, iframe!', 'Background script message should be received')
                  port.postMessage('Message received')
                })
              })

              browser.runtime.sendMessage('Begin Test')
            }
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "content.js": contentScript,
        ]

        try await loadAndRunWithFramePage(resources: resources)
    }

    @Test
    func connectToSpecificDocument() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener(async (message, sender) => {
              browser.test.assertEq(message, 'Begin Test', 'Message content should match')

              const documentId = sender?.documentId
              browser.test.assertEq(typeof documentId, 'string', 'sender.documentId should be a string')
              browser.test.assertEq(documentId.length, 36, 'sender.documentId.length should be 36')

              const port = browser.tabs.connect(sender.tab.id, { documentId })
              browser.test.assertEq(typeof port, 'object', 'Port should be an object')

              port.onMessage.addListener((response) => {
                browser.test.assertEq(response, 'Message received', 'Should receive response from content script')
                browser.test.notifyPass()
              })

              port.postMessage('Hello, iframe!')
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            if (window.top === window) {
              browser.runtime.onConnect.addListener(() => {
                browser.test.notifyFail('Main frame should not receive connection intended for iframe')
              })
            } else {
              browser.runtime.onConnect.addListener((port) => {
                port.onMessage.addListener((message) => {
                  browser.test.assertEq(message, 'Hello, iframe!', 'Background script message should be received')
                  port.postMessage('Message received')
                })
              })

              browser.runtime.sendMessage('Begin Test')
            }
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "content.js": contentScript,
        ]

        try await loadAndRunWithFramePage(resources: resources)
    }

    @Test
    func connectToSubframe() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/main", headerFields: ["Content-Type": "text/html"]) {
                "<body><script>"
                    + "  document.write('<iframe src=\"http://127.0.0.1:' + location.port + '/subframe\"></iframe>')"
                    + "</script></body>"
            }
            Route("/subframe", headerFields: ["Content-Type": "text/html"]) {
                "<p>Subframe content</p>"
            }
        }

        let backgroundScript = """
            browser.test.sendMessage('Load Tab')

            await new Promise(resolve => {
              browser.runtime.onMessage.addListener(function handler(message) {
                if (message === 'Subframe Ready') {
                  browser.runtime.onMessage.removeListener(handler)
                  resolve()
                }
              })
            })

            const tabs = await browser.tabs.query({ })
            browser.test.assertEq(tabs.length, 1)

            const port = browser.tabs.connect(tabs[0].id, { name: 'testPort' })
            browser.test.assertEq(typeof port, 'object', 'Port should be an object')
            browser.test.assertEq(port.name, 'testPort', 'Port name should be testPort')

            port.onMessage.addListener((response) => {
              browser.test.assertEq(response, 'Received from subframe', 'Should get the response from the subframe')
              browser.test.notifyPass()
            })

            port.postMessage('Hello Subframe')
            """

        let contentScript = """
            browser.runtime.sendMessage('Subframe Ready')

            browser.runtime.onConnect.addListener((port) => {
              browser.test.assertEq(port.name, 'testPort', 'Port name should be testPort')

              port.onMessage.addListener((message) => {
                browser.test.assertEq(message, 'Hello Subframe', 'Should receive the correct message content')
                port.postMessage('Received from subframe')
              })
            })
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
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: subframeURL)

            try await manager.waitForTestMessage("Load Tab")

            try requireNonNil(manager.defaultTab?.webView).load(URLRequest(url: mainURL))

            try await manager.run()
        }
    }

    @Test
    func portDisconnect() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener(async (message, sender) => {
              browser.test.assertEq(message?.readyToConnect, true, 'Should be ready to connect')

              const tabs = await browser.tabs.query({ active: true, currentWindow: true })
              const tabId = tabs[0].id

              const port = browser.tabs.connect(tabId)
              browser.test.assertEq(typeof port, 'object', 'Port should be an object')
              browser.test.assertEq(port?.name, '', 'Port name should be empty')
              browser.test.assertEq(port?.sender, null, 'Port sender should be null')

              port.postMessage('Hello')

              port.onDisconnect.addListener(() => {
                browser.test.assertTrue(false, 'Should not trigger the onDisconnect event for the port that called disconnect()')
              })

              port.disconnect()
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            browser.runtime.onConnect.addListener((port) => {
              port.onMessage.addListener((message) => {
                browser.test.assertEq(message, 'Hello', 'Should receive the correct message content')
              })

              port.onDisconnect.addListener(() => {
                browser.test.assertTrue(true, 'Should trigger the onDisconnect event in the content script')
                browser.test.notifyPass()
              })
            })

            browser.runtime.sendMessage({ readyToConnect: true })
            """

        try await loadAndRunWithEmptyPage(
            manifest: tabsContentScriptManifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    @Test
    func connectWithoutListenersFiresOnDisconnectWithError() async throws {
        try await loadAndRun(
            backgroundScript: """
                const [currentTab] = await browser.tabs.query({ active: true, currentWindow: true })
                const port = browser.tabs.connect(currentTab.id, { name: 'testPort' })

                port.onDisconnect.addListener(() => {
                  browser.test.assertTrue(!!port.error, 'Port error should be set')
                  browser.test.assertTrue(/no runtime.onConnect listeners found/i.test(port.error?.message), 'Port error should describe the failure')

                  browser.test.notifyPass()
                })
                """
        )
    }

    @Test
    func connectWithMultipleListeners() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener(async (message, sender) => {
              browser.test.assertEq(message?.readyToConnect, true, 'Should be ready to connect')

              const tabs = await browser.tabs.query({ active: true, currentWindow: true })
              const tabId = tabs[0].id

              const port = browser.tabs.connect(tabId)
              browser.test.assertEq(typeof port, 'object', 'Port should be an object')
              browser.test.assertEq(port?.name, '', 'Port name should be empty')
              browser.test.assertEq(port?.sender, null, 'Port sender should be null')

              port.postMessage('Hello')

              let receivedMessages = 0
              port.onMessage.addListener((response) => {
                browser.test.assertEq(response, 'Received', 'Should get the response from the content script')

                if (++receivedMessages === 2)
                  browser.test.notifyPass()
              })
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            let firstListenerHandled = false
            let secondListenerHandled = false

            browser.runtime.onConnect.addListener((port) => {
              port.onMessage.addListener((message) => {
                if (!firstListenerHandled) {
                  browser.test.assertEq(message, 'Hello', 'Should receive the correct message content')
                  port.postMessage('Received')
                  firstListenerHandled = true
                }
              })
            })

            browser.runtime.onConnect.addListener((port) => {
              port.onMessage.addListener((message) => {
                if (!secondListenerHandled) {
                  browser.test.assertEq(message, 'Hello', 'Should receive the correct message content')
                  port.postMessage('Received')
                  secondListenerHandled = true
                }
              })
            })

            browser.runtime.sendMessage({ readyToConnect: true })
            """

        try await loadAndRunWithEmptyPage(
            manifest: tabsContentScriptManifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    @Test
    func portDisconnectWithMultipleListeners() async throws {
        let backgroundScript = """
            browser.runtime.onMessage.addListener(async (message, sender) => {
              browser.test.assertEq(message?.readyToConnect, true, 'Should be ready to connect')

              const tabs = await browser.tabs.query({ active: true, currentWindow: true })
              const tabId = tabs[0].id

              const port = browser.tabs.connect(tabId)
              browser.test.assertEq(typeof port, 'object', 'Port should be an object')
              browser.test.assertEq(port?.name, '', 'Port name should be empty')
              browser.test.assertEq(port?.sender, null, 'Port sender should be null')

              port.postMessage('Hello')

              let receivedMessages = 0
              port.onMessage.addListener((response) => {
                browser.test.assertEq(response, 'Received', 'Should get the response from the content script')

                if (++receivedMessages === 2)
                  browser.test.notifyPass()
              })
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            browser.runtime.onConnect.addListener((port) => {
              port.onMessage.addListener((message) => {
                browser.test.assertEq(message, 'Hello', 'Should receive the correct message content')
                port.postMessage('Received')
                port.disconnect()
              })
            })

            browser.runtime.onConnect.addListener((port) => {
              port.onMessage.addListener((message) => {
                browser.test.assertEq(message, 'Hello', 'Should receive the correct message content')
                port.postMessage('Received')
              })
            })

            browser.runtime.sendMessage({ readyToConnect: true })
            """

        try await loadAndRunWithEmptyPage(
            manifest: tabsContentScriptManifest,
            resources: ["background.js": backgroundScript, "content.js": contentScript]
        )
    }

    // MARK: - Execute Script, Insert and Remove CSS, and Manifest V3

    @Test
    func executeScript() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<body style='background-color: blue'></body>"
            }
        }

        let javaScript = "document.body.style.background = 'pink'"

        let backgroundScript = """
            const tabs = await browser.tabs.query({ active: true, currentWindow: true })
            const tabId = tabs[0].id

            browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {
              if (tab.status === 'complete') {
                let results = await browser.tabs.executeScript(tabId, { allFrames: false, file: 'executeScript.js' })
                browser.test.assertEq(results[0], 'pink')

                results = await browser.tabs.executeScript(tabId, { frameId: 0, file: 'executeScript.js' })
                browser.test.assertEq(results[0], 'pink')

                results = await browser.tabs.executeScript(tabId, { allFrames: true, file: 'executeScript.js' })
                browser.test.assertEq(results[0], 'pink')
                browser.test.assertEq(results[1], 'pink')

                results = await browser.tabs.executeScript(tabId, { allFrames: false, code: "document.body.style.background = 'pink'" })
                browser.test.assertEq(results[0], 'pink')

                await browser.test.assertSafeResolve(() => browser.tabs.executeScript(tabId, { allFrames: false, frameId: 0, file: 'executeScript.js' }))

                browser.test.notifyPass()
              }
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(
                manifest: tabsManifestV2,
                resources: ["background.js": backgroundScript, "executeScript.js": javaScript]
            )
            let context = try requireNonNil(manager.context)

            let url = configuration.localhostAddress
            let matchPattern = try requireNonNil(WKWebExtension.MatchPattern.cachedPattern(matchingAllPathsOf: url))
            context.setPermissionStatus(.grantedExplicitly, for: matchPattern)

            try await manager.waitForTestMessage("Load Tab")

            try requireNonNil(manager.defaultTab?.webView).load(URLRequest(url: url))

            try await manager.run()
        }
    }

    @Test
    func executeScriptWithUserGesture() async throws {
        let backgroundScript = """
            browser.tabs.onUpdated.addListener((tabId, changeInfo) => {
              if (changeInfo.status !== 'complete')
                return

              browser.test.runWithUserGesture(async () => {
                browser.test.assertTrue(navigator.userActivation.isActive, 'User gesture should be active at the tabs.executeScript call site')

                const results = await browser.test.assertSafeResolve(() => browser.tabs.executeScript(tabId, {
                  code: 'navigator.userActivation.isActive'
                }))

                browser.test.assertTrue(results?.[0], 'Injected code should run with an active user gesture when called from a user gesture context')

                browser.test.notifyPass()
              })
            })

            browser.test.sendMessage('Load Tab')
            """

        try await loadAndRunWithEmptyPage(manifest: tabsManifestV2, resources: ["background.js": backgroundScript])
    }

    @Test
    func executeScriptWithoutUserGesture() async throws {
        let backgroundScript = """
            browser.tabs.onUpdated.addListener(async (tabId, changeInfo) => {
              if (changeInfo.status !== 'complete')
                return

              const results = await browser.test.assertSafeResolve(() => browser.tabs.executeScript(tabId, {
                code: 'navigator.userActivation.isActive'
              }))

              browser.test.assertFalse(results?.[0], 'Injected code should not have an active user gesture when called without a prior user action')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        try await loadAndRunWithEmptyPage(manifest: tabsManifestV2, resources: ["background.js": backgroundScript])
    }

    @Test
    func executeScriptWithFrameId() async throws {
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

              const results = await browser.tabs.executeScript(sender.tab.id, { frameId, code: 'location.pathname' })
              browser.test.assertEq(results[0], '/frame.html', 'Should execute in the iframe and return the correct path')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            browser.runtime.sendMessage('Hello from frame')
            """

        let manifest: [String: Any] = [
            "manifest_version": 2,

            "name": "Tabs Test",
            "description": "Tabs Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "content_scripts": [
                [
                    "js": ["content.js"],
                    "matches": ["*://localhost/frame.html"],
                    "all_frames": true,
                ]
            ],
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

            try requireNonNil(manager.defaultTab?.webView).load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func executeScriptWithDocumentId() async throws {
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

              const result = await browser.tabs.executeScript(sender?.tab?.id, { documentId, code: 'document.body.innerText.trim()' })
              browser.test.assertEq(result?.[0], 'Frame Document', 'Result should be')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            browser.runtime.sendMessage('Hello from frame')
            """

        let manifest: [String: Any] = [
            "manifest_version": 2,

            "name": "Tabs Test",
            "description": "Tabs Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "persistent": false,
            ],

            "content_scripts": [
                [
                    "js": ["content.js"],
                    "matches": ["*://localhost/frame.html"],
                    "all_frames": true,
                ]
            ],
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

            try requireNonNil(manager.defaultTab?.webView).load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func executeScriptJSONTypes() async throws {
        let backgroundScript = """
            browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {
              if (tab.status === 'complete') {
                const expectedResult = { 'boolean': true, 'number': 42, 'string': 'Test String', 'object': { 'key': 'value' }, 'array': [1, 2, 3] };

                function returnJSONTypes() {
                  return { 'boolean': true, 'number': 42, 'string': 'Test String', 'object': { 'key': 'value' }, 'array': [1, 2, 3] };
                }

                const results = await browser.tabs.executeScript(tabId, { code: '(' + returnJSONTypes + ')()' });
                browser.test.assertDeepEq(results?.[0], expectedResult);

                browser.test.notifyPass()
              }
            })

            browser.test.sendMessage('Load Tab')
            """

        try await loadAndRunWithEmptyPage(manifest: tabsManifestV2, resources: ["background.js": backgroundScript])
    }

    @Test
    func insertAndRemoveCSSInMainFrame() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let css = "body { background-color: pink !important }"

        let backgroundScript = """
            const pinkValue = 'rgb(255, 192, 203)'
            const blueValue = 'rgb(0, 0, 255)'
            const transparentValue = 'rgba(0, 0, 0, 0)'

            browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {
              if (tab.status !== 'complete')
                return

              await browser.tabs.insertCSS(tabId, { allFrames: false, file: 'styles.css' })
              let result = await browser.tabs.executeScript(tabId, { code: "window.getComputedStyle(document.body).getPropertyValue('background-color')", allFrames: false })
              browser.test.assertEq(result[0], pinkValue)

              await browser.tabs.removeCSS(tabId, { allFrames: false, file: 'styles.css' })
              result = await browser.tabs.executeScript(tabId, { code: "window.getComputedStyle(document.body).getPropertyValue('background-color')", allFrames: false })
              browser.test.assertEq(result[0], transparentValue)

              await browser.tabs.insertCSS(tabId, { frameId: 0, file: 'styles.css' })
              result = await browser.tabs.executeScript(tabId, { code: "window.getComputedStyle(document.body).getPropertyValue('background-color')", allFrames: false })
              browser.test.assertEq(result[0], pinkValue)

              await browser.tabs.removeCSS(tabId, { frameId: 0, file: 'styles.css' })
              result = await browser.tabs.executeScript(tabId, { code: "window.getComputedStyle(document.body).getPropertyValue('background-color')", allFrames: false })
              browser.test.assertEq(result[0], transparentValue)

              await browser.tabs.insertCSS(tabId, { frameId: 0, code: 'body { background-color: blue !important }' })
              result = await browser.tabs.executeScript(tabId, { code: "window.getComputedStyle(document.body).getPropertyValue('background-color')", allFrames: false })
              browser.test.assertEq(result[0], blueValue)

              await browser.tabs.removeCSS(tabId, { frameId: 0, code: 'body { background-color: blue !important }' })
              result = await browser.tabs.executeScript(tabId, { code: "window.getComputedStyle(document.body).getPropertyValue('background-color')", allFrames: false })
              browser.test.assertEq(result[0], transparentValue)

              // Stylesheet being removed should match the one inserted.
              await browser.tabs.insertCSS(tabId, { frameId: 0, code: 'body { background-color: blue !important }' })
              await browser.tabs.removeCSS(tabId, { allFrames: true, code: 'body { background-color: blue !important }' })
              result = await browser.tabs.executeScript(tabId, { code: "window.getComputedStyle(document.body).getPropertyValue('background-color')", allFrames: false })
              browser.test.assertEq(result[0], blueValue)

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: tabsManifestV2, resources: ["background.js": backgroundScript, "styles.css": css])
            let context = try requireNonNil(manager.context)

            let url = configuration.localhostAddress
            let matchPattern = try requireNonNil(WKWebExtension.MatchPattern.cachedPattern(matchingAllPathsOf: url))
            context.setPermissionStatus(.grantedExplicitly, for: matchPattern)

            try await manager.waitForTestMessage("Load Tab")

            try requireNonNil(manager.defaultTab?.webView).load(URLRequest(url: url))

            try await manager.run()
        }
    }

    @Test
    func insertAndRemoveCSSInAllFrames() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<body><iframe src='/frame.html'/></body>"
            }
            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<body style='background-color: blue'></body>"
            }
        }

        let css = "body { background-color: pink !important }"

        let backgroundScript = """
            const pinkValue = 'rgb(255, 192, 203)'
            const blueValue = 'rgb(0, 0, 255)'
            const transparentValue = 'rgba(0, 0, 0, 0)'

            browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {
              if (tab.status !== 'complete')
                return

              await browser.tabs.insertCSS(tabId, { allFrames: true, file: 'styles.css' })

              let result = await browser.tabs.executeScript(tabId, { code: "window.getComputedStyle(document.body).getPropertyValue('background-color')", allFrames: true })

              browser.test.assertEq(result[0], pinkValue)
              browser.test.assertEq(result[1], pinkValue)

              await browser.tabs.removeCSS(tabId, { allFrames: true, file: 'styles.css' })
              result = await browser.tabs.executeScript(tabId, { code: "window.getComputedStyle(document.body).getPropertyValue('background-color')", allFrames: true })

              browser.test.assertEq(result[0], transparentValue)
              browser.test.assertEq(result[1], blueValue)

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: tabsManifestV2, resources: ["background.js": backgroundScript, "styles.css": css])
            let context = try requireNonNil(manager.context)

            let url = configuration.localhostAddress
            let matchPattern = try requireNonNil(WKWebExtension.MatchPattern.cachedPattern(matchingAllPathsOf: url))
            context.setPermissionStatus(.grantedExplicitly, for: matchPattern)

            try await manager.waitForTestMessage("Load Tab")

            try requireNonNil(manager.defaultTab?.webView).load(URLRequest(url: url))

            try await manager.run()
        }
    }

    // tabs.insertCSS shares injectStyleSheets() with scripting.insertCSS. The main frame is served from
    // 127.0.0.1 and the subframe from localhost, so with site isolation the subframe gets its own process.
    @Test
    func insertCSSInAllFramesWithCrossSiteFrame() async throws {
        let previousShouldEnableSiteIsolation = TestWebExtensionManager.shouldEnableSiteIsolation
        TestWebExtensionManager.shouldEnableSiteIsolation = true
        defer { TestWebExtensionManager.shouldEnableSiteIsolation = previousShouldEnableSiteIsolation }

        let manifest: [String: Any] = [
            "manifest_version": 2,

            "name": "Tabs Test",
            "description": "Tabs Test",
            "version": "1",

            "permissions": ["*://localhost/*", "*://127.0.0.1/*"],

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
            const readBackgroundColor = "window.getComputedStyle(document.body).getPropertyValue('background-color')"

            function readAllFrames(tabId) {
              return browser.tabs.executeScript(tabId, { allFrames: true, code: readBackgroundColor })
            }

            // The cross-site subframe is not loaded yet when the main frame reaches 'complete'.
            async function readBothFrames(tabId) {
              for (let attempt = 0; attempt < 200; ++attempt) {
                const result = await readAllFrames(tabId)
                if (result.length === 2)
                  return result
                await new Promise((resolve) => setTimeout(resolve, 50))
              }
              browser.test.notifyFail('Timed out waiting for the cross-site subframe to appear')
            }

            browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {
              if (tab.status !== 'complete')
                return

              let result = await readBothFrames(tabId)
              browser.test.assertTrue(result.includes(blueValue), 'The cross-site subframe should start out blue')

              await browser.tabs.insertCSS(tabId, { allFrames: true, code: 'body { background-color: pink !important }' })

              result = await readAllFrames(tabId)
              browser.test.assertEq(result.length, 2, 'Both frames should be reachable')

              for (const value of result)
                browser.test.assertEq(value, pinkValue, 'Every frame should be styled by insertCSS with allFrames')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)

            let crossSiteURL = configuration.address

            for url in [crossSiteURL, configuration.localhostAddress] {
                let matchPattern = try requireNonNil(WKWebExtension.MatchPattern.cachedPattern(matchingAllPathsOf: url))
                context.setPermissionStatus(.grantedExplicitly, for: matchPattern)
            }

            try await manager.waitForTestMessage("Load Tab")

            try requireNonNil(manager.defaultTab?.webView).load(URLRequest(url: crossSiteURL))

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
              if (changeInfo.status === 'complete') {
                await browser.test.assertSafeResolve(() => browser.tabs.insertCSS(tabId, { code: 'body { color: green !important }', cssOrigin: 'user' }))

                let result = await browser.test.assertSafeResolve(() => browser.tabs.executeScript(tabId, { code: 'getComputedStyle(document.body).color' }))
                browser.test.assertEq(result[0], 'rgb(0, 128, 0)', 'Color should be green')

                browser.test.notifyPass()
              }
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let urlRequest = URLRequest(url: configuration.localhostAddress)

            let manager = try loadWebExtension(manifest: tabsManifestV2, resources: ["background.js": backgroundScript])

            try await manager.waitForTestMessage("Load Tab")

            try requireNonNil(manager.defaultTab?.webView).load(urlRequest)

            try await manager.run()
        }
    }

    @Test
    func cssAuthorOrigin() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<style> body { color: red; } </style>"
            }
        }

        let backgroundScript = """
            browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {
              if (changeInfo.status === 'complete') {
                await browser.test.assertSafeResolve(() => browser.tabs.insertCSS(tabId, { code: 'body { color: green !important; }', cssOrigin: 'author' }))

                let result = await browser.test.assertSafeResolve(() => browser.tabs.executeScript(tabId, { code: 'getComputedStyle(document.body).color' }))
                browser.test.assertEq(result[0], 'rgb(0, 128, 0)', 'Color should be green')

                browser.test.notifyPass()
              }
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let urlRequest = URLRequest(url: configuration.localhostAddress)

            let manager = try loadWebExtension(manifest: tabsManifestV2, resources: ["background.js": backgroundScript])

            try await manager.waitForTestMessage("Load Tab")

            try requireNonNil(manager.defaultTab?.webView).load(urlRequest)

            try await manager.run()
        }
    }

    @Test
    func unsupportedMV3APIs() async throws {
        // Manifest v3 deprecates these APIs, so they should be an undefined property.

        try await loadAndRun(
            backgroundScript: """
                browser.test.assertEq(browser.tabs.insertCSS, undefined)
                browser.test.assertEq(browser.tabs.removeCSS, undefined)
                browser.test.assertEq(browser.tabs.executeScript, undefined)

                browser.test.assertEq(browser.tabs.getSelected, undefined)

                browser.test.notifyPass()
                """
        )
    }

    // MARK: - Active Tab

    @Test
    func activeTab() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<head><title>Test Title</title></head>"
            }
            Route("/next.html", headerFields: ["Content-Type": "text/html"]) {
                "<head><title>Next Title</title></head>"
            }
        }

        let backgroundScript = """
            const [currentTab] = await browser.tabs.query({ active: true, currentWindow: true })
            let updateCount = 0

            browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {
              browser.test.assertEq(tabId, currentTab.id, 'Only the tab we expect should be changing')

              if ('url' in changeInfo && 'title' in changeInfo) {
                ++updateCount

                if (updateCount === 1) {
                  browser.test.assertEq(changeInfo.url, '', 'URL should be empty before user gesture')
                  browser.test.assertEq(changeInfo.title, '', 'Title should be empty before user gesture')

                  browser.test.sendMessage('Perform User Gesture')
                } else if (updateCount === 2) {
                  browser.test.assertTrue(changeInfo.url.includes('localhost'), 'URL should be localhost after user gesture')
                  browser.test.assertEq(changeInfo.title, 'Test Title', 'Title should be provided after user gesture')

                  browser.test.sendMessage('Load Next Page')
                } else if (updateCount === 3) {
                  browser.test.assertTrue(changeInfo.url.includes('localhost'), 'URL should be localhost after same site navigation')
                  browser.test.assertEq(changeInfo.title, 'Next Title', 'Title should be provided after same site navigation')

                  browser.test.sendMessage('Load IP Address')
                } else {
                  browser.test.assertEq(updateCount, 4, 'Update count should be')
                  browser.test.assertEq(changeInfo.url, '', 'URL should be empty after navigation without permission')
                  browser.test.assertEq(changeInfo.title, '', 'Title should be empty after navigation without permission')

                  browser.test.notifyPass()
                }
              }
            })

            browser.test.sendMessage('Load Localhost')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: activeTabManifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)
            let defaultTab = try requireNonNil(manager.defaultTab)
            let webView = try requireNonNil(defaultTab.webView)

            let localhostURL = configuration.localhostAddress
            let addressURL = configuration.address

            try await manager.waitForTestMessage("Load Localhost")

            webView.load(URLRequest(url: localhostURL))

            try await manager.waitForTestMessage("Perform User Gesture")

            expectFalse(context.hasPermission(.tabs, in: defaultTab))
            expectFalse(context.hasAccess(to: localhostURL, in: defaultTab))
            expectFalse(context.hasAccess(to: addressURL, in: defaultTab))
            expectFalse(context.hasAccess(to: localhostURL))
            expectFalse(context.hasAccess(to: addressURL))

            context.userGesturePerformed(in: defaultTab)

            expectTrue(context.hasPermission(.tabs, in: defaultTab))
            expectTrue(context.hasAccess(to: localhostURL, in: defaultTab))
            expectFalse(context.hasAccess(to: addressURL, in: defaultTab))
            expectFalse(context.hasAccess(to: localhostURL))
            expectFalse(context.hasAccess(to: addressURL))

            try await manager.waitForTestMessage("Load Next Page")

            expectTrue(context.hasPermission(.tabs, in: defaultTab))
            expectTrue(context.hasAccess(to: localhostURL, in: defaultTab))
            expectFalse(context.hasAccess(to: addressURL, in: defaultTab))
            expectFalse(context.hasAccess(to: localhostURL))
            expectFalse(context.hasAccess(to: addressURL))

            webView.load(URLRequest(url: localhostURL.appending(path: "next.html")))

            try await manager.waitForTestMessage("Load IP Address")

            webView.load(URLRequest(url: addressURL))

            try await manager.run()

            expectFalse(context.hasPermission(.tabs, in: defaultTab))
            expectFalse(context.hasAccess(to: localhostURL))
            expectFalse(context.hasAccess(to: addressURL))
            expectFalse(context.hasAccess(to: localhostURL, in: defaultTab))
            expectFalse(context.hasAccess(to: addressURL, in: defaultTab))
        }
    }

    @Test
    func userGestureWithoutActiveTab() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<head><title>Test Title</title></head>"
            }
        }

        let backgroundScript = """
            const [currentTab] = await browser.tabs.query({ active: true, currentWindow: true })

            browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {
              browser.test.assertEq(tabId, currentTab.id, 'Only the tab we expect should be changing')

              if ('url' in changeInfo && 'title' in changeInfo) {
                browser.test.assertEq(changeInfo.url, '', 'URL should be empty before user gesture')
                browser.test.assertEq(changeInfo.title, '', 'Title should be empty before user gesture')
                browser.test.sendMessage('Perform User Gesture')
              }
            })

            browser.test.sendMessage('Load Localhost')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: activeTabManifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)
            let defaultTab = try requireNonNil(manager.defaultTab)

            // Reset activeTab, WKWebExtensionAPITabsTests.activeTab() tests that.
            context.setPermissionStatus(.unknown, for: .activeTab)

            expectFalse(context.hasPermission(.activeTab))
            expectFalse(context.hasPermission(.tabs))

            let localhostURL = configuration.localhostAddress
            let addressURL = configuration.address

            try await manager.waitForTestMessage("Load Localhost")

            try requireNonNil(defaultTab.webView).load(URLRequest(url: localhostURL))

            try await manager.waitForTestMessage("Perform User Gesture")

            context.userGesturePerformed(in: defaultTab)

            expectFalse(context.hasPermission(.activeTab))
            expectFalse(context.hasPermission(.tabs))
            expectFalse(context.hasPermission(.tabs, in: defaultTab))
            expectFalse(context.hasAccess(to: localhostURL))
            expectFalse(context.hasAccess(to: addressURL))
            expectFalse(context.hasAccess(to: localhostURL, in: defaultTab))
            expectFalse(context.hasAccess(to: addressURL, in: defaultTab))
        }
    }

    @Test
    func activeTabWithDeniedPermissions() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<head><title>Test Title</title></head>"
            }
        }

        let backgroundScript = """
            const [currentTab] = await browser.tabs.query({ active: true, currentWindow: true })

            browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {
              browser.test.assertEq(tabId, currentTab.id, 'Only the tab we expect should be changing')

              if ('url' in changeInfo && 'title' in changeInfo) {
                browser.test.assertEq(changeInfo.url, '', 'URL should be empty before user gesture')
                browser.test.assertEq(changeInfo.title, '', 'Title should be empty before user gesture')
                browser.test.sendMessage('Perform User Gesture')
              }
            })

            browser.test.sendMessage('Load Localhost')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: activeTabManifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)
            let defaultTab = try requireNonNil(manager.defaultTab)

            let localhostURL = configuration.localhostAddress

            context.setPermissionStatus(.deniedExplicitly, for: localhostURL)

            expectTrue(context.hasPermission(.activeTab))
            expectFalse(context.hasPermission(.tabs))
            expectFalse(context.hasAccess(to: localhostURL))
            expectFalse(context.hasAccess(to: localhostURL, in: defaultTab))

            try await manager.waitForTestMessage("Load Localhost")

            try requireNonNil(defaultTab.webView).load(URLRequest(url: localhostURL))

            try await manager.waitForTestMessage("Perform User Gesture")

            context.userGesturePerformed(in: defaultTab)

            expectTrue(context.hasPermission(.activeTab))
            expectFalse(context.hasPermission(.tabs))
            expectFalse(context.hasPermission(.tabs, in: defaultTab))
            expectFalse(context.hasAccess(to: localhostURL))
            expectFalse(context.hasAccess(to: localhostURL, in: defaultTab))
        }
    }

    @Test
    func activeTabRemovedWithDeniedPermissions() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<head><title>Test Title</title></head>"
            }
        }

        let backgroundScript = """
            const [currentTab] = await browser.tabs.query({ active: true, currentWindow: true })

            browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {
              browser.test.assertEq(tabId, currentTab.id, 'Only the tab we expect should be changing')

              if ('url' in changeInfo && 'title' in changeInfo) {
                browser.test.assertEq(changeInfo.url, '', 'URL should be empty before user gesture')
                browser.test.assertEq(changeInfo.title, '', 'Title should be empty before user gesture')
                browser.test.sendMessage('Perform User Gesture')
              }
            })

            browser.test.sendMessage('Load Localhost')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: activeTabManifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)
            let defaultTab = try requireNonNil(manager.defaultTab)

            let localhostURL = configuration.localhostAddress

            expectTrue(context.hasPermission(.activeTab))
            expectFalse(context.hasPermission(.tabs))
            expectFalse(context.hasAccess(to: localhostURL))
            expectFalse(context.hasAccess(to: localhostURL, in: defaultTab))

            try await manager.waitForTestMessage("Load Localhost")

            try requireNonNil(defaultTab.webView).load(URLRequest(url: localhostURL))

            try await manager.waitForTestMessage("Perform User Gesture")

            expectTrue(context.hasPermission(.activeTab))
            expectFalse(context.hasPermission(.tabs, in: defaultTab))
            expectFalse(context.hasAccess(to: localhostURL, in: defaultTab))
            expectFalse(context.hasAccess(to: localhostURL))

            context.userGesturePerformed(in: defaultTab)

            expectTrue(context.hasPermission(.activeTab))
            expectTrue(context.hasPermission(.tabs, in: defaultTab))
            expectTrue(context.hasAccess(to: localhostURL, in: defaultTab))
            expectFalse(context.hasAccess(to: localhostURL))

            context.setPermissionStatus(.deniedExplicitly, for: localhostURL)

            expectTrue(context.hasPermission(.activeTab))
            expectFalse(context.hasPermission(.tabs))
            expectFalse(context.hasPermission(.tabs, in: defaultTab))
            expectFalse(context.hasAccess(to: localhostURL))
            expectFalse(context.hasAccess(to: localhostURL, in: defaultTab))
        }
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
