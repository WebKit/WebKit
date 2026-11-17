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

#if ENABLE_WK_WEB_EXTENSIONS_SIDEBAR

import Foundation
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestWebExtensionsDelegate
private import TestWebKitAPILibrary.Helpers.cocoa.WebExtensionUtilities
import Testing
import WebKit
private import WebKit_Private._WKWebExtensionSidebar
private import WebKit_Private.WKWebExtensionContextPrivate

import struct Foundation.URL
import struct Swift.String

@MainActor
struct WKWebExtensionAPISidebarTests {
    private let sidebarActionManifest: [String: Any] = [
        "manifest_version": 3,
        "name": "SidebarAction Test",
        "description": "SidebarAction Test",
        "version": "1",

        "permissions": [],
        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],

        "sidebar_action": [
            "default_title": "Test Sidebar",
            "default_panel": "sidebar.html",
            "default_icon": [
                "16": "sidebar-16.png",
                "32": "sidebar-32.png",
            ],
            "open_at_install": false,
        ],
    ]

    private let sidePanelManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "SidePanel Test",
        "description": "SidePanel Test",
        "version": "1",

        "permissions": ["sidePanel"],
        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],

        "side_panel": [
            "default_path": "sidebar.html"
        ],

        "action": [
            "default_title": "Test Action",
            "default_popup": "sidebar.html",
        ],
    ]

    private let sidePanelNoPathManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "SidePanel No Path Test",
        "description": "SidePanel No Path Test",
        "version": "1",

        "permissions": ["sidePanel"],
        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],

        "side_panel": [String: Any](),

        "action": [
            "default_title": "Test Action",
            "default_popup": "popup.html",
        ],
    ]

    private let sidebarActionAndPanelManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "Sidebar Test (both)",
        "description": "Sidebar Test (both)",
        "version": "1",

        "permissions": ["sidePanel"],

        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],

        "sidebar_action": [
            "default_title": "Test Sidebar",
            "default_panel": "sidebar.html",
            "default_icon": [
                "16": "sidebar-16.png",
                "32": "sidebar-32.png",
            ],
            "open_at_install": false,
        ],
    ]

    private let neitherSidebarActionNorPanelManifest: [String: Any] = [
        "manifest_version": 3,
        "name": "Sidebar Test (neither)",
        "description": "Sidebar Test (neither)",
        "version": "1",

        "permissions": [],
        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],
    ]

    // MARK: - Helpers

    /// A controller configuration that turns on the sidebar feature flag, so that each test doesn't have to make one.
    private let configuration: WKWebExtensionController.Configuration

    init() {
        configuration = configurationEnablingFeature("WebExtensionSidebarEnabled")
    }

    private func manager(backgroundScript: String, manifest: [String: Any]) -> TestWebExtensionManager {
        manager(resources: ["background.js": backgroundScript], manifest: manifest)
    }

    private func manager(resources: [String: Any], manifest: [String: Any]) -> TestWebExtensionManager {
        parseWebExtension(manifest: manifest, resources: resources, configuration: configuration)
    }

    private func loadAndRun(manifest: [String: Any], resources: [String: Any]) async throws {
        let manager = try loadWebExtension(manifest: manifest, resources: resources, configuration: configuration)
        try await manager.run()
    }

    /// How many times the delegate has been asked to present and to close a sidebar.
    private final class SidebarCallCounts {
        var present = 0
        var close = 0
    }

    /// Makes the delegate count the requests to present and to close a sidebar, and report them as handled.
    private func installSidebarCallCounters(on manager: TestWebExtensionManager) -> SidebarCallCounts {
        let counts = SidebarCallCounts()

        manager.internalDelegate.presentSidebar = { _ in
            counts.present += 1
        }

        manager.internalDelegate.closeSidebar = { _ in
            counts.close += 1
        }

        return counts
    }

    /// The properties of a sidebar the delegate was asked to present or close, as they were at the time.
    ///
    /// The delegate's blocks run from WebKit callbacks, where a failed check wouldn't be attributed to the test, so they
    /// take one of these for the test to check afterwards. The properties have to be read during the callback, while the
    /// sidebar is on screen, and reading `webView` creates and loads the sidebar's web view.
    private struct SidebarSnapshot {
        let associatedTab: (any WKWebExtensionTab)?
        let associatedWindow: (any WKWebExtensionWindow)?
        let webExtensionContext: WKWebExtensionContext?
        let title: String?
        let webView: WKWebView?
        let viewController: AnyObject?
        let webViewURL: URL?

        @MainActor
        init(_ sidebar: _WKWebExtensionSidebar) {
            associatedTab = sidebar.associatedTab
            associatedWindow = sidebar.associatedWindow
            webExtensionContext = sidebar.webExtensionContext
            title = sidebar.title
            webView = sidebar.webView
            viewController = sidebar.viewController
            webViewURL = webView?.url
        }
    }

    // MARK: - Common Tests

    @Test
    func apisUnavailableWhenManifestDoesNotRequest() async throws {
        let backgroundScript = """
            browser.test.assertDeepEq(browser.sidebarAction, undefined)
            browser.test.assertDeepEq(browser.sidePanel, undefined)
            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: neitherSidebarActionNorPanelManifest, resources: ["background.js": backgroundScript])
    }

    // MARK: - SidebarAction Tests

    @Test
    func sidebarActionAPIAvailableWhenManifestRequests() async throws {
        let backgroundScript = """
            browser.test.assertFalse(browser.sidebarAction === undefined)
            browser.test.assertFalse(browser.sidebarAction.close === undefined)
            browser.test.assertFalse(browser.sidebarAction.getPanel === undefined)
            browser.test.assertFalse(browser.sidebarAction.getTitle === undefined)
            browser.test.assertFalse(browser.sidebarAction.isOpen === undefined)
            browser.test.assertFalse(browser.sidebarAction.open === undefined)
            browser.test.assertFalse(browser.sidebarAction.setIcon === undefined)
            browser.test.assertFalse(browser.sidebarAction.setPanel === undefined)
            browser.test.assertFalse(browser.sidebarAction.setTitle === undefined)
            browser.test.assertFalse(browser.sidebarAction.toggle === undefined)

            browser.test.assertDeepEq(browser.sidePanel, undefined)

            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: sidebarActionManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func sidebarActionAPIAvailableWhenManifestRequestsBoth() async throws {
        let backgroundScript = """
            browser.test.assertFalse(browser.sidebarAction === undefined)
            browser.test.assertFalse(browser.sidebarAction.close === undefined)
            browser.test.assertFalse(browser.sidebarAction.getPanel === undefined)
            browser.test.assertFalse(browser.sidebarAction.getTitle === undefined)
            browser.test.assertFalse(browser.sidebarAction.isOpen === undefined)
            browser.test.assertFalse(browser.sidebarAction.open === undefined)
            browser.test.assertFalse(browser.sidebarAction.setIcon === undefined)
            browser.test.assertFalse(browser.sidebarAction.setPanel === undefined)
            browser.test.assertFalse(browser.sidebarAction.setTitle === undefined)
            browser.test.assertFalse(browser.sidebarAction.toggle === undefined)

            browser.test.assertDeepEq(browser.sidePanel, undefined)

            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: sidebarActionAndPanelManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func sidebarActionAPIDisallowsMissingArguments() async throws {
        let backgroundScript = """
            browser.test.assertThrows(() => browser.sidebarAction.setTitle())
            browser.test.assertThrows(() => browser.sidebarAction.setPanel())
            browser.test.assertThrows(() => browser.sidebarAction.setIcon())

            await browser.test.assertSafeResolve(() => browser.sidebarAction.getTitle())
            await browser.test.assertSafeResolve(() => browser.sidebarAction.getPanel())
            await browser.test.assertSafeResolve(() => browser.sidebarAction.isOpen())

            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: sidebarActionManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func sidebarActionAPIGlobalTitlePersists() async throws {
        let backgroundScript = """
            await browser.sidebarAction.setTitle({ title: 'Here is a title' })
            browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'Here is a title')
            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: sidebarActionManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func sidebarActionAPIWindowSpecificTitlePersists() async throws {
        let backgroundScript = """
            let windows = await browser.windows.getAll()
            let [window1, window2] = windows

            await browser.sidebarAction.setTitle({ windowId: window1.id, title: 'window-specific title' })
            await browser.sidebarAction.setTitle({ title: 'global title' })

            browser.test.assertEq(await browser.sidebarAction.getTitle({ windowId: window1.id }), 'window-specific title')
            browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'global title')
            browser.test.assertEq(await browser.sidebarAction.getTitle({ windowId: window2.id }), 'global title')

            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: sidebarActionManifest)

        manager.openNewWindow()

        try await manager.loadAndRun()
    }

    @Test
    func sidebarActionAPITabSpecificTitlePersists() async throws {
        let backgroundScript = """
            let tabs = await browser.tabs.query({})
            let [tab1, tab2] = tabs

            await browser.sidebarAction.setTitle({ tabId: tab1.id, title: 'tab-specific title' })
            await browser.sidebarAction.setTitle({ title: 'global title' })

            browser.test.assertEq(await browser.sidebarAction.getTitle({ tabId: tab1.id }), 'tab-specific title')
            browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'global title')
            browser.test.assertEq(await browser.sidebarAction.getTitle({ tabId: tab2.id }), 'global title')

            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: sidebarActionManifest)

        try requireNonNil(manager.defaultWindow).openNewTab()

        try await manager.loadAndRun()
    }

    @Test
    func sidebarActionAPIMixedSpecificityTitlesNotConfused() async throws {
        let backgroundScript = """
            let allWindows = await browser.windows.getAll()
            let [window1, window2] = allWindows
            let window1Tabs = await browser.tabs.query({ windowId: window1.id })
            let window2Tabs = await browser.tabs.query({ windowId: window2.id })

            await browser.sidebarAction.setTitle({ title: 'global title' })
            await browser.sidebarAction.setTitle({ windowId: window1.id, title: 'window specific title' })
            await browser.sidebarAction.setTitle({ tabId: window1Tabs[0].id, title: 'window 1 tab specific title' })
            await browser.sidebarAction.setTitle({ tabId: window2Tabs[0].id, title: 'window 2 tab specific title' })

            browser.test.assertEq(await browser.sidebarAction.getTitle({ tabId: window1Tabs[0].id }), 'window 1 tab specific title')
            browser.test.assertEq(await browser.sidebarAction.getTitle({ tabId: window1Tabs[1].id }), 'window specific title')
            browser.test.assertEq(await browser.sidebarAction.getTitle({ windowId: window1.id }), 'window specific title')

            browser.test.assertEq(await browser.sidebarAction.getTitle({ tabId: window2Tabs[0].id }), 'window 2 tab specific title')
            browser.test.assertEq(await browser.sidebarAction.getTitle({ tabId: window2Tabs[1].id }), 'global title')
            browser.test.assertEq(await browser.sidebarAction.getTitle({ windowId: window2.id }), 'global title')
            browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'global title')

            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: sidebarActionManifest)

        try requireNonNil(manager.defaultWindow).openNewTab()

        let window2 = manager.openNewWindow()
        window2.openNewTab()

        try await manager.loadAndRun()
    }

    @Test
    func sidebarActionAPIWindowSpecificPanelPersists() async throws {
        let backgroundScript = """
            let windows = await browser.windows.getAll()
            let [window1, window2] = windows

            await browser.sidebarAction.setPanel({ windowId: window1.id, panel: '/sidebar-1.html' })
            await browser.sidebarAction.setPanel({ panel: '/sidebar-2.html' })

            browser.test.assertEq(await browser.sidebarAction.getPanel({ windowId: window1.id }), '/sidebar-1.html')
            browser.test.assertEq(await browser.sidebarAction.getPanel({}), '/sidebar-2.html')
            browser.test.assertEq(await browser.sidebarAction.getPanel({ windowId: window2.id }), '/sidebar-2.html')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar-1.html": "<h1>Sidebar 1</h1>",
            "sidebar-2.html": "<h1>Sidebar 2</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidebarActionManifest)

        manager.openNewWindow()

        try await manager.loadAndRun()
    }

    @Test
    func sidebarActionAPITabSpecificDocumentPersists() async throws {
        let backgroundScript = """
            let tabs = await browser.tabs.query({})
            let [tab1, tab2] = tabs

            await browser.sidebarAction.setPanel({ tabId: tab1.id, panel: '/sidebar-1.html' })
            await browser.sidebarAction.setPanel({ panel: '/sidebar-2.html' })

            browser.test.assertEq(await browser.sidebarAction.getPanel({ tabId: tab1.id }), '/sidebar-1.html')
            browser.test.assertEq(await browser.sidebarAction.getPanel({}), '/sidebar-2.html')
            browser.test.assertEq(await browser.sidebarAction.getPanel({ tabId: tab2.id }), '/sidebar-2.html')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar-1.html": "<h1>Sidebar 1</h1>",
            "sidebar-2.html": "<h1>Sidebar 2</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidebarActionManifest)

        manager.openNewWindow()

        try await manager.loadAndRun()
    }

    @Test
    func sidebarActionDocumentsMixedSpecificityNotConfused() async throws {
        let backgroundScript = """
            let allWindows = await browser.windows.getAll()
            let [window1, window2] = allWindows
            let window1Tabs = await browser.tabs.query({ windowId: window1.id })
            let window2Tabs = await browser.tabs.query({ windowId: window2.id })

            await browser.sidebarAction.setPanel({ panel: '/global-sidebar.html' })
            await browser.sidebarAction.setPanel({ windowId: window1.id, panel: '/window-1-window-sidebar.html' })
            await browser.sidebarAction.setPanel({ tabId: window1Tabs[0].id, panel: '/window-1-tab-sidebar.html' })
            await browser.sidebarAction.setPanel({ tabId: window2Tabs[0].id, panel: '/window-2-tab-sidebar.html' })

            browser.test.assertEq(await browser.sidebarAction.getPanel({ tabId: window1Tabs[0].id }), '/window-1-tab-sidebar.html')
            browser.test.assertEq(await browser.sidebarAction.getPanel({ tabId: window1Tabs[1].id }), '/window-1-window-sidebar.html')
            browser.test.assertEq(await browser.sidebarAction.getPanel({ windowId: window1.id }), '/window-1-window-sidebar.html')

            browser.test.assertEq(await browser.sidebarAction.getPanel({ tabId: window2Tabs[0].id }), '/window-2-tab-sidebar.html')
            browser.test.assertEq(await browser.sidebarAction.getPanel({ tabId: window2Tabs[1].id }), '/global-sidebar.html')
            browser.test.assertEq(await browser.sidebarAction.getPanel({ windowId: window2.id }), '/global-sidebar.html')
            browser.test.assertEq(await browser.sidebarAction.getPanel({}), '/global-sidebar.html')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "window1-window-sidebar.html": "<h1>Window 1 Window Sidebar</h1>",
            "window1-tab-sidebar.html": "<h1>Window 1 Tab Sidebar</h1>",
            "window2-tab-sidebar.html": "<h1>Window 2 Tab Sidebar</h1>",
            "global-sidebar.html": "<h1>Global Sidebar</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidebarActionManifest)

        try requireNonNil(manager.defaultWindow).openNewTab()

        let window2 = manager.openNewWindow()
        window2.openNewTab()

        try await manager.loadAndRun()
    }

    @Test
    func sidebarActionChangeGlobalTitle() async throws {
        let backgroundScript = """
            await browser.sidebarAction.setTitle({ title: 'Here is a title' })
            browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'Here is a title')
            await browser.sidebarAction.setTitle({ title: 'Here is a different title' })
            browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'Here is a different title')
            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: sidebarActionManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func sidebarActionChangeWindowTitle() async throws {
        let backgroundScript = """
            let windows = await browser.windows.getAll()
            let [window1, window2] = windows

            await browser.sidebarAction.setTitle({ title: 'global title' })
            await browser.sidebarAction.setTitle({ windowId: window1.id, title: 'window-specific title' })
            browser.test.assertEq(await browser.sidebarAction.getTitle({ windowId: window1.id }), 'window-specific title')

            await browser.sidebarAction.setTitle({ windowId: window1.id, title: 'another window-specific title' })
            browser.test.assertEq(await browser.sidebarAction.getTitle({ windowId: window1.id }), 'another window-specific title')

            browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'global title')
            browser.test.assertEq(await browser.sidebarAction.getTitle({ windowId: window2.id }), 'global title')

            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: sidebarActionManifest)

        manager.openNewWindow()

        try await manager.loadAndRun()
    }

    @Test
    func sidebarActionChangeTabTitle() async throws {
        let backgroundScript = """
            let tabs = await browser.tabs.query({})
            let [tab1, tab2] = tabs

            await browser.sidebarAction.setTitle({ tabId: tab1.id, title: 'tab-specific title' })
            await browser.sidebarAction.setTitle({ title: 'global title' })

            browser.test.assertEq(await browser.sidebarAction.getTitle({ tabId: tab1.id }), 'tab-specific title')
            await browser.sidebarAction.setTitle({ tabId: tab1.id, title: 'new tab-specific title' })
            browser.test.assertEq(await browser.sidebarAction.getTitle({ tabId: tab1.id }), 'new tab-specific title')

            browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'global title')
            browser.test.assertEq(await browser.sidebarAction.getTitle({ tabId: tab2.id }), 'global title')

            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: sidebarActionManifest)

        try requireNonNil(manager.defaultWindow).openNewTab()

        try await manager.loadAndRun()
    }

    @Test
    func sidebarActionChangeGlobalPanel() async throws {
        let backgroundScript = """
            await browser.sidebarAction.setPanel({ panel: '/sidebar-1.html' })
            browser.test.assertEq(await browser.sidebarAction.getPanel({}), '/sidebar-1.html')
            await browser.sidebarAction.setPanel({ panel: '/sidebar-2.html' })
            browser.test.assertEq(await browser.sidebarAction.getPanel({}), '/sidebar-2.html')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar-1.html": "<h1>Sidebar 1</h1>",
            "sidebar-2.html": "<h1>Sidebar 2</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidebarActionManifest)

        manager.openNewWindow()

        try await manager.loadAndRun()
    }

    @Test
    func sidebarActionChangeWindowPanel() async throws {
        let backgroundScript = """
            let windows = await browser.windows.getAll()
            let [window1, window2] = windows

            await browser.sidebarAction.setPanel({ panel: '/sidebar-global.html' })

            await browser.sidebarAction.setPanel({ windowId: window1.id, panel: '/sidebar-1.html' })
            browser.test.assertEq(await browser.sidebarAction.getPanel({ windowId: window1.id }), '/sidebar-1.html')
            await browser.sidebarAction.setPanel({ windowId: window1.id, panel: '/sidebar-2.html' })
            browser.test.assertEq(await browser.sidebarAction.getPanel({ windowId: window1.id }), '/sidebar-2.html')

            browser.test.assertEq(await browser.sidebarAction.getPanel({}), '/sidebar-global.html')
            browser.test.assertEq(await browser.sidebarAction.getPanel({ windowId: window2.id }), '/sidebar-global.html')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar-global.html": "<h1>Global Sidebar</h1>",
            "sidebar-1.html": "<h1>Sidebar 2</h1>",
            "sidebar-2.html": "<h1>Sidebar 2</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidebarActionManifest)

        manager.openNewWindow()

        try await manager.loadAndRun()
    }

    @Test
    func sidebarActionChangeTabPanel() async throws {
        let backgroundScript = """
            let tabs = await browser.tabs.query({})
            let [tab1, tab2] = tabs

            await browser.sidebarAction.setPanel({ panel: '/sidebar-global.html' })

            await browser.sidebarAction.setPanel({ tabId: tab1.id, panel: '/sidebar-1.html' })
            browser.test.assertEq(await browser.sidebarAction.getPanel({ tabId: tab1.id }), '/sidebar-1.html')
            await browser.sidebarAction.setPanel({ tabId: tab1.id, panel: '/sidebar-2.html' })
            browser.test.assertEq(await browser.sidebarAction.getPanel({ tabId: tab1.id }), '/sidebar-2.html')

            browser.test.assertEq(await browser.sidebarAction.getPanel({}), '/sidebar-global.html')
            browser.test.assertEq(await browser.sidebarAction.getPanel({ tabId: tab2.id }), '/sidebar-global.html')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar-global.html": "<h1>Global Sidebar</h1>",
            "sidebar-1.html": "<h1>Sidebar 2</h1>",
            "sidebar-2.html": "<h1>Sidebar 2</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidebarActionManifest)

        manager.openNewWindow()

        try await manager.loadAndRun()
    }

    @Test
    func sidebarActionClearGlobalTitle() async throws {
        let backgroundScript = """
            browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'Test Sidebar')

            await browser.sidebarAction.setTitle({ title: 'A new global title' })
            browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'A new global title')

            await browser.sidebarAction.setTitle({ title: null })
            browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'Test Sidebar')

            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: sidebarActionManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func sidebarActionClearWindowTitle() async throws {
        let backgroundScript = """
            let windows = await browser.windows.getAll()
            let [window1, window2] = windows

            await browser.sidebarAction.setTitle({ title: 'global title' })

            await browser.sidebarAction.setTitle({ windowId: window1.id, title: 'window-specific title' })
            browser.test.assertEq(await browser.sidebarAction.getTitle({ windowId: window1.id }), 'window-specific title')

            await browser.sidebarAction.setTitle({ windowId: window1.id, title: null })
            browser.test.assertEq(await browser.sidebarAction.getTitle({ windowId: window1.id }), 'global title')

            browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'global title')
            browser.test.assertEq(await browser.sidebarAction.getTitle({ windowId: window2.id }), 'global title')

            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: sidebarActionManifest)

        manager.openNewWindow()

        try await manager.loadAndRun()
    }

    @Test
    func sidebarActionClearTabTitle() async throws {
        let backgroundScript = """
            let tabs = await browser.tabs.query({})
            let [tab1, tab2] = tabs

            await browser.sidebarAction.setTitle({ tabId: tab1.id, title: 'tab-specific title' })
            await browser.sidebarAction.setTitle({ title: 'global title' })
            browser.test.assertEq(await browser.sidebarAction.getTitle({ tabId: tab1.id }), 'tab-specific title')

            await browser.sidebarAction.setTitle({ tabId: tab1.id, title: null })
            browser.test.assertEq(await browser.sidebarAction.getTitle({ tabId: tab1.id }), 'global title')

            browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'global title')
            browser.test.assertEq(await browser.sidebarAction.getTitle({ tabId: tab2.id }), 'global title')

            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: sidebarActionManifest)

        try requireNonNil(manager.defaultWindow).openNewTab()

        try await manager.loadAndRun()
    }

    @Test
    func sidebarActionClearGlobalPanel() async throws {
        let backgroundScript = """
            browser.test.assertEq(await browser.sidebarAction.getPanel({}), 'sidebar.html')

            await browser.sidebarAction.setPanel({ panel: '/sidebar-1.html' })
            browser.test.assertEq(await browser.sidebarAction.getPanel({}), '/sidebar-1.html')

            await browser.sidebarAction.setPanel({ panel: null })
            browser.test.assertEq(await browser.sidebarAction.getPanel({}), 'sidebar.html')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar-1.html": "<h1>Sidebar 1</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidebarActionManifest)

        manager.openNewWindow()

        try await manager.loadAndRun()
    }

    @Test
    func sidebarActionClearWindowPanel() async throws {
        let backgroundScript = """
            let windows = await browser.windows.getAll()
            let [window1, window2] = windows

            browser.test.assertEq(await browser.sidebarAction.getPanel({}), 'sidebar.html')

            await browser.sidebarAction.setPanel({ windowId: window1.id, panel: '/sidebar-1.html' })
            browser.test.assertEq(await browser.sidebarAction.getPanel({ windowId: window1.id }), '/sidebar-1.html')
            await browser.sidebarAction.setPanel({ windowId: window1.id, panel: null })
            browser.test.assertEq(await browser.sidebarAction.getPanel({ windowId: window1.id }), 'sidebar.html')

            browser.test.assertEq(await browser.sidebarAction.getPanel({}), 'sidebar.html')
            browser.test.assertEq(await browser.sidebarAction.getPanel({ windowId: window2.id }), 'sidebar.html')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar-1.html": "<h1>Sidebar 1</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidebarActionManifest)

        manager.openNewWindow()

        try await manager.loadAndRun()
    }

    @Test
    func sidebarActionClearTabPanel() async throws {
        let backgroundScript = """
            let tabs = await browser.tabs.query({})
            let [tab1, tab2] = tabs

            browser.test.assertEq(await browser.sidebarAction.getPanel({}), 'sidebar.html')

            await browser.sidebarAction.setPanel({ tabId: tab1.id, panel: '/sidebar-1.html' })
            browser.test.assertEq(await browser.sidebarAction.getPanel({ tabId: tab1.id }), '/sidebar-1.html')
            await browser.sidebarAction.setPanel({ tabId: tab1.id, panel: null })
            browser.test.assertEq(await browser.sidebarAction.getPanel({ tabId: tab1.id }), 'sidebar.html')

            browser.test.assertEq(await browser.sidebarAction.getPanel({}), 'sidebar.html')
            browser.test.assertEq(await browser.sidebarAction.getPanel({ tabId: tab2.id }), 'sidebar.html')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar-1.html": "<h1>Sidebar 1</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidebarActionManifest)
        manager.openNewWindow()

        try await manager.loadAndRun()
    }

    @Test
    func sidebarActionOpenFailsWithoutUserGesture() async throws {
        let backgroundScript = """
            browser.test.assertThrows(() => browser.sidebarAction.open())
            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar.html": "<h1>Sidebar</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidebarActionManifest)
        let sidebarCalls = installSidebarCallCounters(on: manager)

        try await manager.loadAndRun()

        expectEqual(sidebarCalls.present, 0)
    }

    @Test
    func sidebarActionCloseFailsWithoutUserGesture() async throws {
        let backgroundScript = """
            browser.test.assertThrows(() => browser.sidebarAction.close())
            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar.html": "<h1>Sidebar</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidebarActionManifest)
        let sidebarCalls = installSidebarCallCounters(on: manager)

        try await manager.loadAndRun()

        expectEqual(sidebarCalls.close, 0)
    }

    @Test
    func sidebarActionToggleFailsWithoutUserGesture() async throws {
        let backgroundScript = """
            browser.test.assertThrows(() => browser.sidebarAction.toggle())
            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar.html": "<h1>Sidebar</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidebarActionManifest)
        let sidebarCalls = installSidebarCallCounters(on: manager)

        try await manager.loadAndRun()

        expectEqual(sidebarCalls.present, 0)
        expectEqual(sidebarCalls.close, 0)
    }

    @Test
    func sidebarActionOpenSucceedsWithUserGesture() async throws {
        let backgroundScript = """
            await browser.test.runWithUserGesture(() => {
              return browser.test.assertSafeResolve(() => browser.sidebarAction.open())
            })

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar.html": "<h1>Sidebar</h1>",
        ]

        var presentedSidebars: [SidebarSnapshot] = []

        let manager = manager(resources: resources, manifest: sidebarActionManifest)
        manager.internalDelegate.presentSidebar = { sidebar in
            presentedSidebars.append(SidebarSnapshot(sidebar))
        }

        try await manager.loadAndRun()

        for sidebar in presentedSidebars {
            expectNil(sidebar.associatedTab)
            expectNotNil(sidebar.associatedWindow)
            expectNotNil(sidebar.webExtensionContext)
            expectNotNil(sidebar.title)
            expectNotNil(sidebar.webView)
            expectNotNil(sidebar.viewController)

            expectEqual(sidebar.webViewURL?.scheme, "webkit-extension")
            expectEqual(sidebar.webViewURL?.path, "/sidebar.html")
        }

        expectEqual(presentedSidebars.count, 1)
    }

    @Test
    func sidebarActionCloseSucceedsWithUserGesture() async throws {
        let backgroundScript = """
            await browser.test.runWithUserGesture(() => {
              return browser.test.assertSafeResolve(() => browser.sidebarAction.close())
            })

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar.html": "<h1>Sidebar</h1>",
        ]

        var closedSidebars: [SidebarSnapshot] = []

        let manager = manager(resources: resources, manifest: sidebarActionManifest)
        manager.internalDelegate.closeSidebar = { sidebar in
            closedSidebars.append(SidebarSnapshot(sidebar))
        }

        try await manager.loadAndRun()

        for sidebar in closedSidebars {
            // Make sure all the properties are still there when delegate closeSidebar is called, since the sidebar is still displayed at the moment of the call
            expectNil(sidebar.associatedTab)
            expectNotNil(sidebar.associatedWindow)
            expectNotNil(sidebar.webExtensionContext)
            expectNotNil(sidebar.title)
            expectNotNil(sidebar.webView)
            expectNotNil(sidebar.viewController)

            expectEqual(sidebar.webViewURL?.scheme, "webkit-extension")
            expectEqual(sidebar.webViewURL?.path, "/sidebar.html")
        }

        expectEqual(closedSidebars.count, 1)
    }

    @Test
    func sidebarActionToggleSucceedsWithUserGesture() async throws {
        let backgroundScript = """
            await browser.test.runWithUserGesture(() => {
              return browser.test.assertSafeResolve(() => browser.sidebarAction.toggle())
            })

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar.html": "<h1>Sidebar</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidebarActionManifest)
        let sidebarCalls = installSidebarCallCounters(on: manager)

        try await manager.loadAndRun()

        expectEqual(sidebarCalls.present, 1)
    }

    // MARK: - SidePanel Tests

    @Test
    func sidePanelAPIAvailableWhenManifestRequests() async throws {
        let backgroundScript = """
            browser.test.assertFalse(browser.sidePanel === undefined)
            browser.test.assertFalse(browser.sidePanel?.open === undefined)
            browser.test.assertFalse(browser.sidePanel?.close === undefined)
            browser.test.assertFalse(browser.sidePanel?.getOptions === undefined)
            browser.test.assertFalse(browser.sidePanel?.setOptions === undefined)
            browser.test.assertFalse(browser.sidePanel?.getPanelBehavior === undefined)
            browser.test.assertFalse(browser.sidePanel?.setPanelBehavior === undefined)
            browser.test.assertFalse(browser.sidePanel?.getLayout === undefined)

            browser.test.assertDeepEq(browser.sidebarAction, undefined)

            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: sidePanelManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func sidePanelAPIDisallowsMissingArguments() async throws {
        let backgroundScript = """
            browser.test.assertThrows(() => browser.sidePanel.setOptions())
            browser.test.assertThrows(() => browser.sidePanel.setPanelBehavior())
            browser.test.assertThrows(() => browser.sidePanel.open())
            browser.test.assertThrows(() => browser.sidePanel.close())

            await browser.test.assertSafeResolve(() => browser.sidePanel.getOptions())
            await browser.test.assertSafeResolve(() => browser.sidePanel.setOptions({ }))

            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: sidePanelManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func sidePanelAPIGlobalPathPersists() async throws {
        let backgroundScript = """
            await browser.sidePanel.setOptions({ path: '/sidebar-1.html' })
            let options = await browser.sidePanel.getOptions({})
            browser.test.assertEq(options.path, '/sidebar-1.html')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar-1.html": "<h1>Sidebar 1</h1>",
        ]

        try await loadAndRun(manifest: sidePanelManifest, resources: resources)
    }

    @Test
    func sidePanelAPITabPathPersists() async throws {
        let backgroundScript = """
            let tabs = await browser.tabs.query({})
            let [tab1, tab2] = tabs

            await browser.sidePanel.setOptions({ tabId: tab1.id, path: '/sidebar-1.html' })
            await browser.sidePanel.setOptions({ path: '/sidebar-global.html' })

            let tabOptions = await browser.sidePanel.getOptions({ tabId: tab1.id })
            let otherTabOptions = await browser.sidePanel.getOptions({ tabId: tab2.id })
            let globalOptions = await browser.sidePanel.getOptions({})

            browser.test.assertEq(tabOptions.path, '/sidebar-1.html')
            browser.test.assertEq(otherTabOptions.path, '/sidebar-global.html')
            browser.test.assertEq(globalOptions.path, '/sidebar-global.html')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar-global.html": "<h1>Global Sidebar</h1>",
            "sidebar-1.html": "<h1>Sidebar 1</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidePanelManifest)
        try requireNonNil(manager.defaultWindow).openNewTab()

        try await manager.loadAndRun()
    }

    @Test
    func sidePanelAPIGlobalEnablePersists() async throws {
        let backgroundScript = """
            let startingOptions = await browser.sidePanel.getOptions({})
            browser.test.assertTrue(startingOptions.enabled)

            await browser.sidePanel.setOptions({ enabled: false })
            let options = await browser.sidePanel.getOptions({})
            browser.test.assertFalse(options.enabled)

            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: sidePanelManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func sidePanelAPITabEnablePersists() async throws {
        let backgroundScript = """
            let tabs = await browser.tabs.query({})
            let [tab1, tab2] = tabs
            let startingOptions = await browser.sidePanel.getOptions({})
            browser.test.assertTrue(startingOptions.enabled)

            await browser.sidePanel.setOptions({ tabId: tab1.id, enabled: false })

            let tabOptions = await browser.sidePanel.getOptions({ tabId: tab1.id })
            let otherTabOptions = await browser.sidePanel.getOptions({ tabId: tab2.id })
            let globalOptions = await browser.sidePanel.getOptions({})

            browser.test.assertFalse(tabOptions.enabled)
            browser.test.assertTrue(otherTabOptions.enabled)
            browser.test.assertTrue(globalOptions.enabled)

            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: sidePanelManifest)

        try requireNonNil(manager.defaultWindow).openNewTab()

        try await manager.loadAndRun()
    }

    @Test
    func sidePanelAPIModifyGlobalPath() async throws {
        let backgroundScript = """
            await browser.sidePanel.setOptions({ path: '/sidebar-1.html' })
            let preModOptions = await browser.sidePanel.getOptions({})
            browser.test.assertEq(preModOptions.path, '/sidebar-1.html')

            await browser.sidePanel.setOptions({ path: '/sidebar-2.html' })
            let postModOptions = await browser.sidePanel.getOptions({})
            browser.test.assertEq(postModOptions.path, '/sidebar-2.html')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar-1.html": "<h1>Sidebar 1</h1>",
            "sidebar-2.html": "<h1>Sidebar 2</h1>",
        ]

        try await loadAndRun(manifest: sidePanelManifest, resources: resources)
    }

    @Test
    func sidePanelAPIModifyTabPath() async throws {
        let backgroundScript = """
            let tabs = await browser.tabs.query({})
            let [tab1, tab2] = tabs

            await browser.sidePanel.setOptions({ tabId: tab1.id, path: '/sidebar-1.html' })
            await browser.sidePanel.setOptions({ path: '/sidebar-global.html' })
            let preModTabOptions = await browser.sidePanel.getOptions({ tabId: tab1.id })
            browser.test.assertEq(preModTabOptions.path, '/sidebar-1.html')

            await browser.sidePanel.setOptions({ tabId: tab1.id, path: '/sidebar-2.html' })
            let postModTabOptions = await browser.sidePanel.getOptions({ tabId: tab1.id })
            browser.test.assertEq(postModTabOptions.path, '/sidebar-2.html')

            let otherTabOptions = await browser.sidePanel.getOptions({ tabId: tab2.id })
            let globalOptions = await browser.sidePanel.getOptions({})
            browser.test.assertEq(otherTabOptions.path, '/sidebar-global.html')
            browser.test.assertEq(globalOptions.path, '/sidebar-global.html')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar-global.html": "<h1>Global Sidebar</h1>",
            "sidebar-1.html": "<h1>Sidebar 1</h1>",
            "sidebar-2.html": "<h1>Sidebar 2</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidePanelManifest)

        try requireNonNil(manager.defaultWindow).openNewTab()

        try await manager.loadAndRun()
    }

    @Test
    func sidePanelModifyGlobalEnable() async throws {
        let backgroundScript = """
            let startingOptions = await browser.sidePanel.getOptions({})
            browser.test.assertTrue(startingOptions.enabled)

            await browser.sidePanel.setOptions({ enabled: false })
            let preModOptions = await browser.sidePanel.getOptions({})
            browser.test.assertFalse(preModOptions.enabled)

            await browser.sidePanel.setOptions({ enabled: true })
            let postModOptions = await browser.sidePanel.getOptions({})
            browser.test.assertTrue(postModOptions.enabled)

            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: sidePanelManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func sidePanelModifyTabEnable() async throws {
        let backgroundScript = """
            let tabs = await browser.tabs.query({})
            let [tab1, tab2] = tabs

            await browser.sidePanel.setOptions({ enabled: true })
            await browser.sidePanel.setOptions({ tabId: tab1.id, enabled: true })
            let preModTabOptions = await browser.sidePanel.getOptions({ tabId: tab1.id })
            browser.test.assertTrue(preModTabOptions.enabled)

            await browser.sidePanel.setOptions({ tabId: tab1.id, enabled: false })
            let postModTabOptions = await browser.sidePanel.getOptions({ tabId: tab1.id })
            browser.test.assertFalse(postModTabOptions.enabled)

            let otherTabOptions = await browser.sidePanel.getOptions({ tabId: tab2.id })
            let globalOptions = await browser.sidePanel.getOptions({})
            browser.test.assertTrue(otherTabOptions.enabled)
            browser.test.assertTrue(globalOptions.enabled)

            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: sidePanelManifest)

        try requireNonNil(manager.defaultWindow).openNewTab()

        try await manager.loadAndRun()
    }

    @Test
    func sidePanelOpenForTabFailsWithoutUserGesture() async throws {
        let backgroundScript = """
            let tabs = await browser.tabs.query({})
            await browser.sidePanel.open({ tabId: tabs[0].id })
                .then(() => browser.test.notifyFail('sidePanel.open() did not throw without user gesture'))
                .catch(() => browser.test.notifyPass())
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar.html": "<h1>Sidebar</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidePanelManifest)
        let sidebarCalls = installSidebarCallCounters(on: manager)

        try await manager.loadAndRun()

        expectEqual(sidebarCalls.present, 0)
    }

    @Test
    func sidePanelOpenForWindowFailsWithoutUserGesture() async throws {
        let backgroundScript = """
            let window = await browser.windows.getCurrent()
            await browser.sidePanel.open({ windowId: window.id })
                .then(() => browser.test.notifyFail('sidePanel.open() did not throw without user gesture'))
                .catch(() => browser.test.notifyPass())
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar.html": "<h1>Sidebar</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidePanelManifest)
        let sidebarCalls = installSidebarCallCounters(on: manager)

        try await manager.loadAndRun()

        expectEqual(sidebarCalls.present, 0)
    }

    @Test
    func sidePanelOpenForTabSucceedsWithUserGesture() async throws {
        let backgroundScript = """
            var tabs = await browser.tabs.query({})

            await browser.test.runWithUserGesture(() => {
              return browser.test.assertSafeResolve(() => browser.sidePanel.open({ tabId: tabs[0].id }))
            })

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar.html": "<h1>Sidebar</h1>",
        ]

        var presentedSidebars: [SidebarSnapshot] = []

        let manager = manager(resources: resources, manifest: sidePanelManifest)
        manager.internalDelegate.presentSidebar = { sidebar in
            presentedSidebars.append(SidebarSnapshot(sidebar))
        }

        try await manager.loadAndRun()

        for sidebar in presentedSidebars {
            expectNil(sidebar.associatedTab)
            expectNotNil(sidebar.associatedWindow)
            expectNotNil(sidebar.webExtensionContext)
            expectNotNil(sidebar.title)
            expectNotNil(sidebar.webView)
            expectNotNil(sidebar.viewController)

            expectEqual(sidebar.webViewURL?.scheme, "webkit-extension")
            expectEqual(sidebar.webViewURL?.path, "/sidebar.html")
        }

        expectEqual(presentedSidebars.count, 1)
    }

    @Test
    func sidePanelOpenForWindowSucceedsWithUserGesture() async throws {
        let backgroundScript = """
            var currentWindow = await browser.windows.getCurrent()

            await browser.test.runWithUserGesture(() => {
              return browser.test.assertSafeResolve(() => browser.sidePanel.open({ windowId: currentWindow.id }))
            })

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar.html": "<h1>Sidebar</h1>",
        ]

        var presentedSidebars: [SidebarSnapshot] = []

        let manager = manager(resources: resources, manifest: sidePanelManifest)
        let defaultWindow = manager.defaultWindow
        let newWindow = manager.openNewWindow()
        manager.focusWindow(defaultWindow)

        manager.internalDelegate.presentSidebar = { sidebar in
            presentedSidebars.append(SidebarSnapshot(sidebar))
        }

        try await manager.loadAndRun()

        for sidebar in presentedSidebars {
            expectNil(sidebar.associatedTab)
            expectNotNil(sidebar.associatedWindow)
            expectNotNil(sidebar.webExtensionContext)
            expectNotNil(sidebar.title)
            expectNotNil(sidebar.webView)
            expectNotNil(sidebar.viewController)

            expectEqual(sidebar.webViewURL?.scheme, "webkit-extension")
            expectEqual(sidebar.webViewURL?.path, "/sidebar.html")

            expectIdentical(sidebar.associatedWindow, defaultWindow)
            expectTrue(sidebar.associatedWindow !== newWindow)
        }

        expectEqual(presentedSidebars.count, 1)
    }

    @Test
    func sidePanelSetPanelBehaviorPersists() async throws {
        let backgroundScript = """
            let panelBehavior = await browser.sidePanel.getPanelBehavior()
            browser.test.assertFalse(panelBehavior.openPanelOnActionClick)

            await browser.test.assertSafeResolve(() => browser.sidePanel.setPanelBehavior({ openPanelOnActionClick: true }))
            let newPanelBehavior = await browser.sidePanel.getPanelBehavior()
            browser.test.assertTrue(newPanelBehavior.openPanelOnActionClick)

            await browser.test.assertSafeResolve(() => browser.sidePanel.setPanelBehavior({ openPanelOnActionClick: false }))
            let newNewPanelBehavior = await browser.sidePanel.getPanelBehavior()
            browser.test.assertFalse(newNewPanelBehavior.openPanelOnActionClick)

            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: sidePanelManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func sidePanelOpensSidebarOnActionClickWhenConfigured() async throws {
        let backgroundScript = """
            await browser.sidePanel.setPanelBehavior({ openPanelOnActionClick: true })
            let initialPanelBehavior = await browser.sidePanel.getPanelBehavior()
            browser.test.assertTrue(initialPanelBehavior.openPanelOnActionClick)

            browser.test.sendMessage('Perform action')

            await browser.sidePanel.setPanelBehavior({ openPanelOnActionClick: false })
            let newPanelBehavior = await browser.sidePanel.getPanelBehavior()
            browser.test.assertFalse(newPanelBehavior.openPanelOnActionClick)

            browser.test.sendMessage('Perform action again')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar.html": "<h1>Sidebar</h1>",
        ]

        var presentSidebarCallCount = 0
        var presentActionPopupCallCount = 0

        let manager = manager(resources: resources, manifest: sidePanelManifest)
        manager.internalDelegate.presentPopupForAction = { [weak manager] _ in
            presentActionPopupCallCount += 1
            manager?.done()
        }
        manager.internalDelegate.presentSidebar = { _ in
            presentSidebarCallCount += 1
        }

        manager.load()
        try await manager.waitForTestMessage("Perform action")

        let context = try requireNonNil(manager.context)
        context.performAction(for: manager.defaultTab)

        try await manager.waitForTestMessage("Perform action again")

        expectEqual(presentSidebarCallCount, 1)
        expectEqual(presentActionPopupCallCount, 0)

        context.performAction(for: manager.defaultTab)

        try await manager.run()

        expectEqual(presentSidebarCallCount, 1)
        expectEqual(presentActionPopupCallCount, 1)
    }

    // Ensure we don't open a sidebar on about:blank if the extension has not configured a target URL
    @Test
    func sidePanelActionClickDoesNotOpenPathlessSidebar() async throws {
        let backgroundScript = """
            await browser.sidePanel.setPanelBehavior({ openPanelOnActionClick: true })
            browser.test.sendMessage('Perform action')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "popup.html": "<h1>Popup</h1>",
        ]

        var presentSidebarCallCount = 0
        var presentActionPopupCallCount = 0

        let manager = manager(resources: resources, manifest: sidePanelNoPathManifest)
        manager.internalDelegate.presentSidebar = { _ in
            presentSidebarCallCount += 1
        }
        manager.internalDelegate.presentPopupForAction = { [weak manager] _ in
            presentActionPopupCallCount += 1
            manager?.done()
        }

        manager.load()
        try await manager.waitForTestMessage("Perform action")

        try requireNonNil(manager.context).performAction(for: manager.defaultTab)
        try await manager.run()

        expectEqual(presentSidebarCallCount, 0)
        expectEqual(presentActionPopupCallCount, 1)
    }

    @Test
    func sidePanelCloseRequiresTabOrWindow() async throws {
        let backgroundScript = """
            browser.test.assertThrows(() => browser.sidePanel.close({}), /it must specify at least one of 'tabId' or 'windowId'/i)
            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: sidePanelManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func sidePanelCloseForTabClosesTabPanel() async throws {
        let backgroundScript = """
            let [tab] = await browser.tabs.query({})

            // Give the tab a panel of its own, then open it for that tab
            await browser.sidePanel.setOptions({ tabId: tab.id, path: '/sidebar.html', enabled: true })
            await browser.test.runWithUserGesture(() => {
              return browser.test.assertSafeResolve(() => browser.sidePanel.open({ tabId: tab.id }))
            })

            // Closing that tab's panel resolves and asks the app to close the sidebar.
            await browser.test.assertSafeResolve(() => browser.sidePanel.close({ tabId: tab.id }))

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar.html": "<h1>Sidebar</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidePanelManifest)
        let sidebarCalls = installSidebarCallCounters(on: manager)

        try await manager.loadAndRun()

        expectEqual(sidebarCalls.present, 1)
        expectEqual(sidebarCalls.close, 1)
    }

    @Test
    func sidePanelCloseForWindowClosesGlobalPanel() async throws {
        let backgroundScript = """
            let currentWindow = await browser.windows.getCurrent()

            await browser.test.runWithUserGesture(() => {
              return browser.test.assertSafeResolve(() => browser.sidePanel.open({ windowId: currentWindow.id }))
            })

            await browser.test.assertSafeResolve(() => browser.sidePanel.close({ windowId: currentWindow.id }))

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar.html": "<h1>Sidebar</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidePanelManifest)
        let sidebarCalls = installSidebarCallCounters(on: manager)

        try await manager.loadAndRun()

        expectEqual(sidebarCalls.present, 1)
        expectEqual(sidebarCalls.close, 1)
    }

    @Test
    func sidePanelCloseForTabRejectsWhenOnlyGlobalOpen() async throws {
        let backgroundScript = """
            let currentWindow = await browser.windows.getCurrent()
            let [tab] = await browser.tabs.query({})

            // Only the global panel is open for this window; the tab has no panel of its own.
            await browser.test.runWithUserGesture(() => {
              return browser.test.assertSafeResolve(() => browser.sidePanel.open({ windowId: currentWindow.id }))
            })

            // Closing by tab when only the global panel is open rejects rather than closing it.
            await browser.test.assertRejects(browser.sidePanel.close({ tabId: tab.id }), /no side panel is open for the specified tab/i)

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar.html": "<h1>Sidebar</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidePanelManifest)
        let sidebarCalls = installSidebarCallCounters(on: manager)

        try await manager.loadAndRun()

        expectEqual(sidebarCalls.present, 1)
        expectEqual(sidebarCalls.close, 0)
    }

    @Test
    func sidePanelCloseIsNoOpWhenAlreadyClosed() async throws {
        let backgroundScript = """
            let currentWindow = await browser.windows.getCurrent()

            // A valid close call with no open panel should no-op / not throw an error
            await browser.test.assertSafeResolve(() => browser.sidePanel.close({ windowId: currentWindow.id }))

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar.html": "<h1>Sidebar</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidePanelManifest)
        // The close delegate must be present so the close resolves; a nil block would make it reject.
        manager.internalDelegate.closeSidebar = { _ in }

        try await manager.loadAndRun()
    }

    @Test
    func sidePanelGetLayoutReturnsSide() async throws {
        let backgroundScript = """
            let layout = await browser.sidePanel.getLayout()
            browser.test.assertEq(typeof layout, 'object')
            browser.test.assertTrue(layout.side === 'left' || layout.side === 'right', 'getLayout().side must be either left or right')
            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: sidePanelManifest, resources: ["background.js": backgroundScript])
    }

    // MARK: - Per-Window Rendering Tests

    @Test
    func tabsWithoutOverridesShareTheirWindowSidebar() async throws {
        let backgroundScript = """
            browser.test.sendMessage('Loaded')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar.html": "<h1>Sidebar</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidebarActionManifest)
        let defaultWindow = try requireNonNil(manager.defaultWindow)
        let firstTab = try requireNonNil(manager.defaultTab)
        let secondTab = defaultWindow.openNewTab()

        manager.load()
        try await manager.waitForTestMessage("Loaded")

        let context = try requireNonNil(manager.context)
        let firstSidebar = context.sidebar(for: firstTab)
        let secondSidebar = context.sidebar(for: secondTab)

        // Neither tab is singled out by the extension, so both are shown their window's sidebar: the same object,
        // in the same web view, so moving between them leaves the sidebar pane alone.
        expectNotNil(firstSidebar)
        expectIdentical(firstSidebar, secondSidebar)
        expectNil(firstSidebar?.associatedTab)
        expectIdentical(firstSidebar?.associatedWindow, defaultWindow)
        expectIdentical(firstSidebar?.webView, secondSidebar?.webView)
    }

    @Test
    func didInvalidateSidebarFiresWhenLastOverrideCleared() async throws {
        let backgroundScript = """
            let tabs = await browser.tabs.query({})
            let [tab] = tabs

            browser.test.onMessage.addListener(async (message) => {
              if (message === 'Override')
                await browser.sidebarAction.setTitle({ tabId: tab.id, title: 'tab title' })
              else if (message === 'Clear')
                await browser.sidebarAction.setTitle({ tabId: tab.id, title: null })
              browser.test.sendMessage('Done')
            })

            browser.test.sendMessage('Loaded')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar.html": "<h1>Sidebar</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidebarActionManifest)

        var lastUpdated: _WKWebExtensionSidebar?
        var lastInvalidated: _WKWebExtensionSidebar?
        manager.internalDelegate.didUpdateSidebar = { sidebar in
            lastUpdated = sidebar
        }
        manager.internalDelegate.didInvalidateSidebar = { sidebar in
            lastInvalidated = sidebar
        }

        manager.load()
        try await manager.waitForTestMessage("Loaded")

        let context = try requireNonNil(manager.context)
        let tab = try requireNonNil(manager.defaultTab)

        // With nothing set for the tab, it is shown its window's sidebar.
        let windowSidebar = context.sidebar(for: tab)
        expectNotNil(windowSidebar)
        expectNil(windowSidebar?.associatedTab)

        // Setting a tab override gives the tab a sidebar of its own and reports it as an update, not an invalidation.
        manager.sendTestMessage("Override")
        try await manager.waitForTestMessage("Done")
        expectTrue(context.sidebar(for: tab) !== windowSidebar)
        expectNotNil(lastUpdated)
        expectIdentical(lastUpdated?.associatedTab, tab)
        expectNil(lastInvalidated)

        lastUpdated = nil

        // Clearing the last override discards the tab's own sidebar: it is shown its window's sidebar again, and the
        // discard is reported as an invalidation carrying that sidebar (so the browser can drop it and re-query),
        // never as an update for the sidebar being gotten rid of.
        manager.sendTestMessage("Clear")
        try await manager.waitForTestMessage("Done")
        expectIdentical(context.sidebar(for: tab), windowSidebar)
        expectNotNil(lastInvalidated)
        expectIdentical(lastInvalidated?.associatedTab, tab)
        expectNil(lastUpdated)
    }

    @Test
    func willOpenSidebarWithoutUserInteractionDoesNotStartUserGesture() async throws {
        let backgroundScript = """
            browser.test.sendMessage('Loaded')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar.html": "<h1>Sidebar</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidebarActionManifest)

        manager.load()
        try await manager.waitForTestMessage("Loaded")

        let context = try requireNonNil(manager.context)
        let tab = try requireNonNil(manager.defaultTab)
        let sidebar = context.sidebar(for: tab)

        expectNotNil(sidebar)
        expectFalse(context.hasActiveUserGesture(in: tab))

        // Replacing which sidebar is on screen is not an interaction with the extension, and the extension can
        // bring one about itself by setting a property for a single tab, so it must not start a user gesture.
        sidebar?.willOpen(fromUserInteraction: false)
        expectFalse(context.hasActiveUserGesture(in: tab))

        sidebar?.willClose()
        sidebar?.willOpen(fromUserInteraction: true)
        expectTrue(context.hasActiveUserGesture(in: tab))
    }

    @Test
    func sidePanelSetOptionsRequiresPathOrEnabled() async throws {
        let backgroundScript = """
            let tabs = await browser.tabs.query({})
            let [tab] = tabs

            // `tabId` only says which sidebar to change, so these calls have nothing to do.
            browser.test.assertThrows(() => browser.sidePanel.setOptions({ tabId: tab.id }))
            browser.test.assertThrows(() => browser.sidePanel.setOptions({ }))

            // Clearing the path, or setting enablement, is a change.
            await browser.test.assertSafeResolve(() => browser.sidePanel.setOptions({ tabId: tab.id, path: '' }))
            await browser.test.assertSafeResolve(() => browser.sidePanel.setOptions({ tabId: tab.id, enabled: false }))

            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: sidePanelManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func tabPanelOverrideSwapsWebViewAndRestoresIt() async throws {
        let backgroundScript = """
            let [tab1, tab2] = await browser.tabs.query({})

            browser.test.onMessage.addListener(async (message) => {
              if (message === 'Override')
                await browser.sidebarAction.setPanel({ tabId: tab1.id, panel: '/tab.html' })
              else if (message === 'Clear')
                await browser.sidebarAction.setPanel({ tabId: tab1.id, panel: null })
              browser.test.sendMessage('Done')
            })

            browser.test.sendMessage('Loaded')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar.html": "<h1>Window Sidebar</h1>",
            "tab.html": "<h1>Tab Sidebar</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidebarActionManifest)
        let defaultWindow = try requireNonNil(manager.defaultWindow)
        let firstTab = try requireNonNil(manager.defaultTab)
        let secondTab = defaultWindow.openNewTab()

        manager.load()
        try await manager.waitForTestMessage("Loaded")

        let context = try requireNonNil(manager.context)

        // Neither tab is singled out, so both are shown their window's sidebar and share its single web view.
        let windowSidebar = context.sidebar(for: firstTab)
        expectIdentical(windowSidebar, context.sidebar(for: secondTab))
        let sharedWebView = windowSidebar?.webView
        expectNotNil(sharedWebView)

        // Giving one tab its own panel promotes it to a distinct sidebar with a web view of its own; the other tab
        // keeps sharing the window's.
        manager.sendTestMessage("Override")
        try await manager.waitForTestMessage("Done")

        let tabSidebar = context.sidebar(for: firstTab)
        expectTrue(tabSidebar !== windowSidebar)
        expectIdentical(tabSidebar?.associatedTab, firstTab)
        expectTrue(tabSidebar?.webView !== sharedWebView)
        expectIdentical(context.sidebar(for: secondTab), windowSidebar)
        expectIdentical(context.sidebar(for: secondTab)?.webView, sharedWebView)

        // Clearing that panel demotes the tab back to sharing its window's sidebar and its original web view.
        manager.sendTestMessage("Clear")
        try await manager.waitForTestMessage("Done")

        expectIdentical(context.sidebar(for: firstTab), windowSidebar)
        expectIdentical(context.sidebar(for: firstTab)?.webView, sharedWebView)
    }

    @Test
    func windowPanelChangeReloadsSharedWebViewInPlace() async throws {
        let backgroundScript = """
            let window = await browser.windows.getCurrent()

            browser.test.onMessage.addListener(async (message) => {
              if (message === 'ChangeWindowPanel')
                await browser.sidebarAction.setPanel({ windowId: window.id, panel: '/window-2.html' })
              browser.test.sendMessage('Done')
            })

            browser.test.sendMessage('Loaded')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar.html": "<h1>Window Sidebar</h1>",
            "window-2.html": "<h1>Window Sidebar 2</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidebarActionManifest)
        let defaultWindow = try requireNonNil(manager.defaultWindow)
        let firstTab = try requireNonNil(manager.defaultTab)
        let secondTab = defaultWindow.openNewTab()

        manager.load()
        try await manager.waitForTestMessage("Loaded")

        let context = try requireNonNil(manager.context)
        let windowSidebar = context.sidebar(for: firstTab)
        let sharedWebView = windowSidebar?.webView
        expectNotNil(sharedWebView)

        // Changing the window's panel reloads the shared web view in place rather than rebuilding it, so the object
        // both tabs are looking at stays the same and sharing is preserved.
        manager.sendTestMessage("ChangeWindowPanel")
        try await manager.waitForTestMessage("Done")

        expectIdentical(context.sidebar(for: firstTab), windowSidebar)
        expectIdentical(windowSidebar?.webView, sharedWebView)
        expectIdentical(context.sidebar(for: secondTab), windowSidebar)
        expectIdentical(context.sidebar(for: secondTab)?.webView, sharedWebView)
    }

    @Test
    func parentTitleChangeNotifiesInheritingTabSidebar() async throws {
        let backgroundScript = """
            let window = await browser.windows.getCurrent()
            let [tab] = await browser.tabs.query({})

            browser.test.onMessage.addListener(async (message) => {
              if (message === 'OverrideTabPanel')
                await browser.sidebarAction.setPanel({ tabId: tab.id, panel: '/tab.html' })
              else if (message === 'ChangeWindowTitle')
                await browser.sidebarAction.setTitle({ windowId: window.id, title: 'Window Title' })
              browser.test.sendMessage('Done')
            })

            browser.test.sendMessage('Loaded')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar.html": "<h1>Window Sidebar</h1>",
            "tab.html": "<h1>Tab Sidebar</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidebarActionManifest)

        var updatedSidebars: [_WKWebExtensionSidebar] = []
        manager.internalDelegate.didUpdateSidebar = { sidebar in
            updatedSidebars.append(sidebar)
        }

        manager.load()
        try await manager.waitForTestMessage("Loaded")

        let context = try requireNonNil(manager.context)
        let tab = try requireNonNil(manager.defaultTab)

        // The tab overrides only its panel, so it keeps a sidebar of its own that still inherits its title from the
        // window.
        manager.sendTestMessage("OverrideTabPanel")
        try await manager.waitForTestMessage("Done")
        expectIdentical(context.sidebar(for: tab)?.associatedTab, tab)

        updatedSidebars.removeAll()

        // Changing the window's title has to reach the inheriting tab sidebar, not only the window's own, since the
        // tab sidebar is what the browser displays for that tab.
        manager.sendTestMessage("ChangeWindowTitle")
        try await manager.waitForTestMessage("Done")

        let notifiedTabSidebar = updatedSidebars.contains { $0.associatedTab === tab }
        expectTrue(notifiedTabSidebar)
        expectEqual(context.sidebar(for: tab)?.title, "Window Title")
    }

    @Test
    func setOptionsNotifiesDelegateAtMostOnce() async throws {
        let backgroundScript = """
            let [tab] = await browser.tabs.query({})

            browser.test.onMessage.addListener(async (message) => {
              if (message === 'SetPathAndEnabled')
                await browser.sidePanel.setOptions({ tabId: tab.id, path: '/tab.html', enabled: false })
              browser.test.sendMessage('Done')
            })

            browser.test.sendMessage('Loaded')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar.html": "<h1>Window Sidebar</h1>",
            "tab.html": "<h1>Tab Sidebar</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidePanelManifest)

        var updateCount = 0
        manager.internalDelegate.didUpdateSidebar = { _ in
            updateCount += 1
        }

        manager.load()
        try await manager.waitForTestMessage("Loaded")

        updateCount = 0

        // Setting the path (which promotes the tab to its own web view) and enablement in one call must report a
        // single update, not one per mutated property.
        manager.sendTestMessage("SetPathAndEnabled")
        try await manager.waitForTestMessage("Done")

        expectEqual(updateCount, 1)
    }

    @Test
    func tabSidebarRelinksToNewWindowOnMove() async throws {
        let backgroundScript = """
            let [windowA, windowB] = await browser.windows.getAll()
            let [tab] = await browser.tabs.query({ windowId: windowA.id })

            browser.test.onMessage.addListener(async (message) => {
              if (message === 'Setup') {
                await browser.sidebarAction.setTitle({ windowId: windowA.id, title: 'A title' })
                await browser.sidebarAction.setTitle({ windowId: windowB.id, title: 'B title' })
                await browser.sidebarAction.setPanel({ tabId: tab.id, panel: '/tab.html' })
              } else if (message === 'ChangeWindowBTitle')
                await browser.sidebarAction.setTitle({ windowId: windowB.id, title: 'B title 2' })
              else if (message === 'ChangeWindowATitle')
                await browser.sidebarAction.setTitle({ windowId: windowA.id, title: 'A title 2' })
              browser.test.sendMessage('Done')
            })

            browser.test.sendMessage('Loaded')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "sidebar.html": "<h1>Window Sidebar</h1>",
            "tab.html": "<h1>Tab Sidebar</h1>",
        ]

        let manager = manager(resources: resources, manifest: sidebarActionManifest)
        let windowB = manager.openNewWindow()

        var updatedSidebars: [_WKWebExtensionSidebar] = []
        manager.internalDelegate.didUpdateSidebar = { sidebar in
            updatedSidebars.append(sidebar)
        }

        manager.load()
        try await manager.waitForTestMessage("Loaded")

        manager.sendTestMessage("Setup")
        try await manager.waitForTestMessage("Done")

        let context = try requireNonNil(manager.context)
        let tab = try requireNonNil(manager.defaultTab)

        // The tab overrides only its panel, so it inherits its title from whichever window holds it -- window A for now.
        let tabSidebar = context.sidebar(for: tab)
        expectIdentical(tabSidebar?.associatedTab, tab)
        expectEqual(tabSidebar?.title, "A title")

        // Moving the tab into window B has to re-point its inheritance at B.
        windowB.moveTab(tab, to: 0)
        expectEqual(context.sidebar(for: tab)?.title, "B title")

        // A change to window B's title now reaches the tab, proving it was linked as a child of B's sidebar.
        updatedSidebars.removeAll()
        manager.sendTestMessage("ChangeWindowBTitle")
        try await manager.waitForTestMessage("Done")

        let notifiedByWindowB = updatedSidebars.contains { $0.associatedTab === tab }
        expectTrue(notifiedByWindowB)
        expectEqual(context.sidebar(for: tab)?.title, "B title 2")

        // A change to window A's title, on the other hand, must no longer reach the tab -- it was unlinked from A.
        updatedSidebars.removeAll()
        manager.sendTestMessage("ChangeWindowATitle")
        try await manager.waitForTestMessage("Done")

        let notifiedByWindowA = updatedSidebars.contains { $0.associatedTab === tab }
        expectFalse(notifiedByWindowA)
        expectEqual(context.sidebar(for: tab)?.title, "B title 2")
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS_SIDEBAR
