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

import CoreGraphics
import Foundation
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestWebExtensionsDelegate
private import TestWebKitAPILibrary.Helpers.cocoa.WebExtensionUtilities
import Testing
import WebKit

import struct Foundation.URL
import struct Swift.String

@MainActor
struct WKWebExtensionAPIWindowsTests {
    private let windowsManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "Windows Test",
        "description": "Windows Test",
        "version": "1",

        "options_page": "options.html",

        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],
    ]

    @Test
    func errors() async throws {
        #if WTF_PLATFORM_MAC
        // iOS does not support create() and update().
        let platformSpecificChecks = """
            await browser.test.assertRejects(browser.windows.update(99, { focused: true }), /window not found/i)

            browser.test.assertThrows(() => browser.windows.create({ url: 42 }), /'url' is expected to be a string or an array of strings, but a number was provided/i)
            browser.test.assertThrows(() => browser.windows.create({ left: 'bad' }), /'left' is expected to be a number/i)
            browser.test.assertThrows(() => browser.windows.create({ top: 'bad' }), /'top' is expected to be a number/i)
            browser.test.assertThrows(() => browser.windows.create({ width: 'bad' }), /'width' is expected to be a number/i)
            browser.test.assertThrows(() => browser.windows.create({ height: 'bad' }), /'height' is expected to be a number/i)
            browser.test.assertThrows(() => browser.windows.create({ incognito: 'bad' }), /'incognito' is expected to be a boolean/i)
            browser.test.assertThrows(() => browser.windows.create({ focused: 'bad' }), /'focused' is expected to be a boolean/i)
            browser.test.assertThrows(() => browser.windows.create({ tabId: 'bad' }), /'tabId' is expected to be a number/i)

            const window = await browser.test.assertSafeResolve(() => browser.windows.getCurrent())
            browser.test.assertThrows(() => browser.windows.update(window.id, { left: 'bad' }), /'left' is expected to be a number, but a string was provided/i)
            browser.test.assertThrows(() => browser.windows.update(window.id, { top: 'bad' }), /'top' is expected to be a number, but a string was provided/i)
            browser.test.assertThrows(() => browser.windows.update(window.id, { width: 'bad' }), /'width' is expected to be a number, but a string was provided/i)
            browser.test.assertThrows(() => browser.windows.update(window.id, { height: 'bad' }), /'height' is expected to be a number, but a string was provided/i)
            browser.test.assertThrows(() => browser.windows.update(window.id, { focused: 'bad' }), /'focused' is expected to be a boolean, but a string was provided/i)
            browser.test.assertThrows(() => browser.windows.update(window.id, { state: 'bad' }), /'state' value is invalid, because it must specify 'normal', 'minimized', 'maximized', or 'fullscreen'/i)
            browser.test.assertThrows(() => browser.windows.update(window.id, { top: 100, state: 'fullscreen' }), /'properties' value is invalid, because when 'top', 'left', 'width', or 'height' are specified, 'state' must specify 'normal'/i)
            browser.test.assertThrows(() => browser.windows.update(window.id, { left: 100, state: 'minimized' }), /'properties' value is invalid, because when 'top', 'left', 'width', or 'height' are specified, 'state' must specify 'normal'/i)
            browser.test.assertThrows(() => browser.windows.update(window.id, { width: 100, state: 'maximized' }), /'properties' value is invalid, because when 'top', 'left', 'width', or 'height' are specified, 'state' must specify 'normal'/i)
            browser.test.assertThrows(() => browser.windows.update(window.id, { height: 100, state: 'fullscreen' }), /'properties' value is invalid, because when 'top', 'left', 'width', or 'height' are specified, 'state' must specify 'normal'/i)
            """
        #else
        let platformSpecificChecks = ""
        #endif

        let backgroundScript = """
            browser.test.assertThrows(() => browser.windows.get('bad'), /'windowId' value is invalid, because a number is expected/i)
            browser.test.assertThrows(() => browser.windows.get(NaN), /'windowId' value is invalid, because a number is expected/i)
            browser.test.assertThrows(() => browser.windows.get(Infinity), /'windowId' value is invalid, because a number is expected/i)
            browser.test.assertThrows(() => browser.windows.get(-Infinity), /'windowId' value is invalid, because a number is expected/i)
            browser.test.assertThrows(() => browser.windows.get(-3), /'windowId' value is invalid, because it is not a window identifier/i)
            browser.test.assertThrows(() => browser.windows.get(1.2), /'windowId' value is invalid, because it is not a window identifier/i)
            browser.test.assertThrows(() => browser.windows.get(browser.windows.WINDOW_ID_NONE), /'windowId' value is invalid, because 'windows.WINDOW_ID_NONE' is not allowed/i)

            await browser.test.assertRejects(browser.windows.get(42), /window not found/i)

            \(platformSpecificChecks)

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: windowsManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func getCurrent() async throws {
        let backgroundScript = """
            const window = await browser.windows.getCurrent();

            browser.test.assertEq(typeof window, 'object', 'The window should be an object');
            browser.test.assertEq(typeof window.id, 'number', 'The window id should be a number');
            browser.test.assertEq(window.type, 'normal', 'Window type should be normal');
            browser.test.assertEq(window.state, 'normal', 'Window state should be normal');
            browser.test.assertTrue(window.focused, 'Window should be focused');
            browser.test.assertFalse(window.incognito, 'Window should not be in incognito mode');
            browser.test.assertFalse(window.alwaysOnTop, 'Window should not be always on top');
            browser.test.assertEq(window.top, 50, 'Window top position should be 50');
            browser.test.assertEq(window.left, 100, 'Window left position should be 100');
            browser.test.assertEq(window.width, 800, 'Window width should be 800');
            browser.test.assertEq(window.height, 600, 'Window height should be 600');

            browser.test.notifyPass();
            """

        let manager = try loadWebExtension(manifest: windowsManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func getCurrentFromOptionsPage() async throws {
        let optionsScript = """
            const window = await browser.windows.getCurrent()

            browser.test.assertEq(typeof window, 'object', 'The window should be')
            browser.test.assertTrue(window.focused, 'The current window should be focused')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": "// Not Used",
            "options.html": "<script type='module' src='options.js'></script>",
            "options.js": optionsScript,
        ]

        let manager = try loadWebExtension(manifest: windowsManifest, resources: resources)
        let defaultWindow = try #require(manager.defaultWindow)

        defaultWindow.openNewTab()

        #expect(defaultWindow.tabs.count == 2)

        let context = try #require(manager.context)
        let optionsPageURL = try #require(context.optionsPageURL)
        let defaultTab = try #require(manager.defaultTab)

        defaultTab.changeWebViewIfNeeded(for: optionsPageURL, for: context)
        defaultTab.webView?.load(URLRequest(url: optionsPageURL))

        try await manager.run()
    }

    @Test
    func getCurrentWindowAfterTabMove() async throws {
        let backgroundScript = """
            const initialWindow = await browser.windows.getCurrent()
            browser.test.assertEq(typeof initialWindow, 'object', 'Initial window should be an object')
            browser.test.assertEq(typeof initialWindow.id, 'number', 'Initial window ID should be a number')

            browser.tabs.onAttached.addListener(async (tabId, attachInfo) => {
              const currentWindowAfterMove = await browser.windows.getCurrent()
              browser.test.assertEq(currentWindowAfterMove.id, initialWindow.id, 'Current window ID should remain the initial window')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Move Tab')
            """

        let manager = try loadWebExtension(manifest: windowsManifest, resources: ["background.js": backgroundScript])
        let defaultWindow = try #require(manager.defaultWindow)

        let newTab = defaultWindow.openNewTab()
        let newWindow = manager.openNewWindow()

        try await manager.waitForTestMessage("Move Tab")

        newWindow.moveTab(newTab, to: 0)

        try await manager.run()
    }

    @Test
    func getLastFocused() async throws {
        let backgroundScript = """
            const window = await browser.windows.getLastFocused();

            browser.test.assertEq(typeof window, 'object', 'The window should be an object');
            browser.test.assertEq(typeof window.id, 'number', 'The window id should be a number');
            browser.test.assertEq(window.type, 'normal', 'Window type should be normal');
            browser.test.assertEq(window.state, 'normal', 'Window state should be normal');
            browser.test.assertFalse(window.focused, 'Window should be focused');
            browser.test.assertFalse(window.incognito, 'Window should not be in incognito mode');
            browser.test.assertFalse(window.alwaysOnTop, 'Window should not be always on top');
            browser.test.assertEq(window.top, 50, 'Window top position should be 50');
            browser.test.assertEq(window.left, 100, 'Window left position should be 100');
            browser.test.assertEq(window.width, 800, 'Window width should be 800');
            browser.test.assertEq(window.height, 600, 'Window height should be 600');

            browser.test.notifyPass();
            """

        let manager = parseWebExtension(manifest: windowsManifest, resources: ["background.js": backgroundScript])

        manager.internalDelegate.focusedWindow = { _ in nil }

        try await manager.loadAndRun()
    }

    @Test
    func getLastFocusedWithPrivateAccess() async throws {
        let backgroundScript = """
            const lastFocusedWindow = await browser.windows.getLastFocused()

            browser.test.assertEq(typeof lastFocusedWindow, 'object', 'Last focused window should be an object')
            browser.test.assertTrue(lastFocusedWindow.incognito, 'Last focused window should be incognito')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: windowsManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        context.hasAccessToPrivateData = true

        let privateWindow = manager.openNewWindow(usingPrivateBrowsing: true)
        manager.focusWindow(privateWindow)

        try await manager.run()
    }

    @Test
    func getLastFocusedWithoutPrivateAccess() async throws {
        let backgroundScript = """
            const lastFocusedWindow = await browser.windows.getLastFocused()

            browser.test.assertEq(typeof lastFocusedWindow, 'object', 'Last focused window should be an object')
            browser.test.assertFalse(lastFocusedWindow.incognito, 'Last focused window should not be incognito')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: windowsManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        context.hasAccessToPrivateData = false

        let privateWindow = manager.openNewWindow(usingPrivateBrowsing: true)
        manager.focusWindow(privateWindow)

        try await manager.run()
    }

    @Test
    func getAll() async throws {
        let backgroundScript = """
            const windows = await browser.windows.getAll();
            const windowOne = windows[0];

            browser.test.assertEq(windows.length, 1, 'One window should be returned');

            browser.test.assertEq(typeof windowOne, 'object', 'windowOne should be an object');
            browser.test.assertEq(typeof windowOne.id, 'number', 'windowOne id should be a number');
            browser.test.assertEq(windowOne.type, 'normal', 'windowOne type should be normal');
            browser.test.assertEq(windowOne.state, 'normal', 'windowOne state should be normal');
            browser.test.assertTrue(windowOne.focused, 'windowOne should be focused');
            browser.test.assertFalse(windowOne.incognito, 'windowOne should not be in incognito mode');
            browser.test.assertFalse(windowOne.alwaysOnTop, 'windowOne should not be always on top');
            browser.test.assertEq(windowOne.top, 50, 'windowOne top position should be 50');
            browser.test.assertEq(windowOne.left, 100, 'windowOne left position should be 100');
            browser.test.assertEq(windowOne.width, 800, 'windowOne width should be 800');
            browser.test.assertEq(windowOne.height, 600, 'windowOne height should be 600');

            browser.test.notifyPass();
            """

        let manager = parseWebExtension(manifest: windowsManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        #expect(!context.hasAccessToPrivateData)

        manager.openNewWindow(usingPrivateBrowsing: true)

        try await manager.loadAndRun()
    }

    @Test
    func getAllWithPrivateAccess() async throws {
        let backgroundScript = """
            const windows = await browser.windows.getAll();
            const windowOne = windows[0];
            const windowTwo = windows[1];

            browser.test.assertEq(windows.length, 2, 'Two windows should be returned');

            // Validate first window's properties
            browser.test.assertEq(typeof windowOne, 'object', 'windowOne should be an object');
            browser.test.assertEq(typeof windowOne.id, 'number', 'windowOne id should be a number');
            browser.test.assertEq(windowOne.type, 'normal', 'windowOne type should be normal');
            browser.test.assertEq(windowOne.state, 'normal', 'windowOne state should be normal');
            browser.test.assertTrue(windowOne.focused, 'windowOne should be focused');
            browser.test.assertFalse(windowOne.incognito, 'windowOne should not be in incognito mode');
            browser.test.assertFalse(windowOne.alwaysOnTop, 'windowOne should not be always on top');
            browser.test.assertEq(windowOne.top, 50, 'windowOne top position should be 50');
            browser.test.assertEq(windowOne.left, 100, 'windowOne left position should be 100');
            browser.test.assertEq(windowOne.width, 800, 'windowOne width should be 800');
            browser.test.assertEq(windowOne.height, 600, 'windowOne height should be 600');

            // Validate second window's properties
            browser.test.assertEq(typeof windowTwo, 'object', 'windowTwo should be an object');
            browser.test.assertEq(typeof windowTwo.id, 'number', 'windowTwo id should be a number');
            browser.test.assertEq(windowTwo.type, 'popup', 'windowTwo type should be popup');
            browser.test.assertEq(windowTwo.state, 'minimized', 'windowTwo state should be minimized');
            browser.test.assertFalse(windowTwo.focused, 'windowTwo should not be focused');
            browser.test.assertTrue(windowTwo.incognito, 'windowTwo should be in incognito mode');
            browser.test.assertEq(windowTwo.top, 75, 'windowTwo top position should be 75');
            browser.test.assertEq(windowTwo.left, 110, 'windowTwo left position should be 110');
            browser.test.assertEq(windowTwo.width, 300, 'windowTwo width should be 300');
            browser.test.assertEq(windowTwo.height, 700, 'windowTwo height should be 700');

            // Validate that windowOne.id and windowTwo.id are different
            browser.test.assertTrue(windowOne.id !== windowTwo.id, 'windowOne.id and windowTwo.id should be different');

            browser.test.notifyPass();
            """

        let manager = parseWebExtension(manifest: windowsManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        context.hasAccessToPrivateData = true

        let windowTwo = manager.openNewWindow(usingPrivateBrowsing: true)

        #if WTF_PLATFORM_MAC
        // This is 75pt from top on a screen of 1920 x 1080 in Mac screen coordinates.
        windowTwo.frame = CGRect(x: 110, y: 305, width: 300, height: 700)
        #else
        windowTwo.frame = CGRect(x: 110, y: 75, width: 300, height: 700)
        #endif
        windowTwo.windowState = .minimized
        windowTwo.windowType = .popup

        try await manager.loadAndRun()
    }

    @Test
    func createdEvent() async throws {
        let backgroundScript = """
            browser.windows.onCreated.addListener((window) => {

              browser.test.assertEq(typeof window, 'object', 'The window should be an object');
              browser.test.assertEq(typeof window.id, 'number', 'The window id should be a number');
              browser.test.assertEq(window.type, 'normal', 'Window type should be normal');
              browser.test.assertEq(window.state, 'normal', 'Window state should be normal');
              browser.test.assertTrue(window.focused, 'Window should be focused');
              browser.test.assertFalse(window.incognito, 'Window should not be in incognito mode');
              browser.test.assertFalse(window.alwaysOnTop, 'Window should not be always on top');
              browser.test.assertEq(window.top, 50, 'Window top position should be 50');
              browser.test.assertEq(window.left, 100, 'Window left position should be 100');
              browser.test.assertEq(window.width, 800, 'Window width should be 800');
              browser.test.assertEq(window.height, 600, 'Window height should be 600');

              browser.test.notifyPass();

            });

            browser.test.sendMessage('Open Window');
            """

        let manager = try loadWebExtension(manifest: windowsManifest, resources: ["background.js": backgroundScript])

        try await manager.waitForTestMessage("Open Window")

        manager.openNewWindow()

        try await manager.run()
    }

    @Test
    func focusChangedEvent() async throws {
        let backgroundScript = """
            let focusedTwice = false;

            browser.windows.onFocusChanged.addListener((windowId) => {
              if (windowId !== browser.windows.WINDOW_ID_NONE) {
                if (!focusedTwice) {
                  browser.test.assertEq(typeof windowId, 'number', 'The window id should be a number when a window is focused');
                  browser.test.sendMessage('Focus None');
                  focusedTwice = true;
                } else {
                  browser.test.assertEq(typeof windowId, 'number', 'The window id should be a number when a window is focused again');
                  browser.test.notifyPass();
                }
              } else {
                browser.test.assertEq(windowId, browser.windows.WINDOW_ID_NONE, 'The window id should be WINDOW_ID_NONE when nil is focused');
                browser.test.sendMessage('Focus Window Again');
              }
            });

            browser.test.sendMessage('Focus Window');
            """

        let manager = try loadWebExtension(manifest: windowsManifest, resources: ["background.js": backgroundScript])

        let windowOne = try #require(manager.defaultWindow)
        let windowTwo = manager.openNewWindow()

        try await manager.waitForTestMessage("Focus Window")

        manager.focusWindow(windowOne)

        try await manager.waitForTestMessage("Focus None")

        manager.focusWindow(nil)

        try await manager.waitForTestMessage("Focus Window Again")

        manager.focusWindow(windowTwo)

        try await manager.run()
    }

    @Test
    func removedEvent() async throws {
        let backgroundScript = """
            browser.windows.onRemoved.addListener((windowId) => {
              browser.test.assertEq(typeof windowId, 'number', 'The window id should be a number');

              browser.test.notifyPass();
            });

            browser.test.sendMessage('Close Window');
            """

        let manager = try loadWebExtension(manifest: windowsManifest, resources: ["background.js": backgroundScript])
        let defaultWindow = try #require(manager.defaultWindow)

        manager.openNewWindow()

        try await manager.waitForTestMessage("Close Window")

        manager.closeWindow(defaultWindow)

        try await manager.run()
    }

    @Test
    func removeListenerDuringEvent() async throws {
        let backgroundScript = """
            function windowListener() {
              browser.windows.onCreated.removeListener(windowListener)
              browser.test.assertFalse(browser.windows.onCreated.hasListener(windowListener), 'Listener should be removed')
            }

            browser.windows.onCreated.addListener(windowListener)
            browser.windows.onCreated.addListener(() => browser.test.notifyPass())

            browser.test.assertTrue(browser.windows.onCreated.hasListener(windowListener), 'Listener should be registered')

            browser.test.sendMessage('Open Window')
            """

        let manager = try loadWebExtension(manifest: windowsManifest, resources: ["background.js": backgroundScript])

        try await manager.waitForTestMessage("Open Window")

        manager.openNewWindow()

        try await manager.run()
    }

    #if WTF_PLATFORM_MAC

    // iOS does not support create() and update().

    @Test
    func create() async throws {
        let backgroundScript = """
            const windowOptions = {
              focused: true,
              left: 300,
              height: 400,
              incognito: false,
              state: 'normal',
              type: 'popup',
              url: 'http://example.com/',
            }

            const window = await browser.windows.create(windowOptions)
            browser.test.assertEq(typeof window, 'object', 'The window should be an object')
            browser.test.assertEq(window?.top, 50, 'The window should have the specified top')
            browser.test.assertEq(window?.left, 300, 'The window should have the specified left')
            browser.test.assertEq(window?.width, 800, 'The window should have the specified width')
            browser.test.assertEq(window?.height, 400, 'The window should have the specified height')
            browser.test.assertFalse(window?.incognito, 'The window should not be in incognito mode')
            browser.test.assertEq(window?.type, 'popup', 'The window should be of type popup')
            browser.test.assertEq(window?.state, 'normal', 'The window state should be normal')
            browser.test.assertTrue(window?.focused, 'The window should be focused')

            browser.test.assertEq(typeof window?.tabs, 'object', 'The window should have a tabs array')
            browser.test.assertEq(window?.tabs?.length, 1, 'The tabs array should contain one tab')

            const tab = window?.tabs?.[0]
            browser.test.assertEq(typeof tab, 'object', 'The tab should be an object')
            browser.test.assertEq(tab?.active, true, 'The tab should be active')
            browser.test.assertEq(tab?.incognito, false, 'The tab should not be in incognito mode')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: windowsManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        #expect(!context.hasAccessToPrivateData)
        #expect(manager.windows.count == 1)

        let configuration = try await manager.nextRequestedWindow()

        #expect(configuration.frame.origin.x == 300)
        #expect(configuration.frame.size.height == 400)
        #expect(configuration.frame.origin.y.isNaN)
        #expect(configuration.frame.size.width.isNaN)

        #expect(configuration.shouldBeFocused)
        #expect(!configuration.shouldBePrivate)

        #expect(configuration.windowType == .popup)
        #expect(configuration.windowState == .normal)

        #expect(configuration.tabs.count == 0)

        #expect(configuration.tabURLs == [URL(string: "http://example.com/")!])

        try await manager.run()

        #expect(manager.windows.count == 2)
    }

    @Test
    func createWithRelativeURL() async throws {
        let backgroundScript = """
            const windowOptions = {
              url: 'test.html'
            }

            const window = await browser.windows.create(windowOptions)

            browser.test.assertEq(typeof window, 'object', 'The window should be an object')
            browser.test.assertEq(typeof window?.tabs, 'object', 'The window should have a tabs array')
            browser.test.assertEq(window?.tabs?.length, 1, 'The tabs array should contain one tab')

            const tab = window?.tabs?.[0]
            browser.test.assertEq(typeof tab, 'object', 'The tab should be an object')
            browser.test.assertEq(tab?.url, browser.runtime.getURL('/test.html'), 'The tab URL should match the runtime-generated test.html URL')
            browser.test.assertEq(tab?.active, true, 'The tab should be active')
            browser.test.assertEq(tab?.incognito, false, 'The tab should not be in incognito mode')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(
            manifest: windowsManifest,
            resources: ["background.js": backgroundScript, "test.html": "Hello world!"]
        )
        let context = try #require(manager.context)

        #expect(manager.windows.count == 1)

        let configuration = try await manager.nextRequestedWindow()

        #expect(configuration.tabURLs == [URL(string: "test.html", relativeTo: context.baseURL)!.absoluteURL])

        try await manager.run()

        #expect(manager.windows.count == 2)
    }

    @Test
    func createWithRelativeURLs() async throws {
        let backgroundScript = """
            const windowOptions = {
              url: [ 'one.html', 'two.html' ]
            }

            const window = await browser.windows.create(windowOptions)

            browser.test.assertEq(typeof window, 'object', 'The window should be an object')
            browser.test.assertEq(typeof window?.tabs, 'object', 'The window should have a tabs array')
            browser.test.assertEq(window?.tabs?.length, 2, 'The tabs array should contain two tabs')

            const firstTab = window?.tabs?.[0]
            browser.test.assertEq(typeof firstTab, 'object', 'The first tab should be an object')
            browser.test.assertEq(firstTab?.url, browser.runtime.getURL('/one.html'), 'The first tab URL should match the runtime-generated one.html URL')
            browser.test.assertEq(firstTab?.active, true, 'The first tab should be active')
            browser.test.assertEq(firstTab?.incognito, false, 'The first tab should not be in incognito mode')

            const secondTab = window?.tabs?.[1]
            browser.test.assertEq(typeof secondTab, 'object', 'The second tab should be an object')
            browser.test.assertEq(secondTab?.url, browser.runtime.getURL('/two.html'), 'The second tab URL should match the runtime-generated two.html URL')
            browser.test.assertEq(secondTab?.active, false, 'The second tab should not be active')
            browser.test.assertEq(secondTab?.incognito, false, 'The second tab should not be in incognito mode')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(
            manifest: windowsManifest,
            resources: [
                "background.js": backgroundScript,
                "one.html": "Hello one!",
                "two.html": "Hello two!",
            ]
        )
        let context = try #require(manager.context)

        #expect(manager.windows.count == 1)

        let configuration = try await manager.nextRequestedWindow()

        #expect(
            configuration.tabURLs == [
                URL(string: "one.html", relativeTo: context.baseURL)!.absoluteURL,
                URL(string: "two.html", relativeTo: context.baseURL)!.absoluteURL,
            ]
        )

        try await manager.run()

        #expect(manager.windows.count == 2)
    }

    @Test
    func createIncognitoWithoutPrivateAccess() async throws {
        let backgroundScript = """
            const windowOptions = {
              focused: true,
              left: 300,
              height: 400,
              incognito: true,
              state: 'normal',
              type: 'popup',
            };

            const window = await browser.windows.create(windowOptions);
            browser.test.assertEq(window, null, 'The window should be created but null without access');

            browser.test.notifyPass();
            """

        let manager = try loadWebExtension(manifest: windowsManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        #expect(!context.hasAccessToPrivateData)
        #expect(manager.windows.count == 1)

        try await manager.run()

        #expect(manager.windows.count == 2)
    }

    @Test
    func createIncognitoWithPrivateAccess() async throws {
        let backgroundScript = """
            const windowOptions = {
              focused: true,
              left: 300,
              height: 400,
              incognito: true,
              state: 'normal',
              type: 'popup',
            }

            const window = await browser.windows.create(windowOptions)
            browser.test.assertEq(typeof window, 'object', 'The window should be an object')
            browser.test.assertEq(window?.top, 50, 'The window should have the specified top')
            browser.test.assertEq(window?.left, 300, 'The window should have the specified left')
            browser.test.assertEq(window?.width, 800, 'The window should have the specified width')
            browser.test.assertEq(window?.height, 400, 'The window should have the specified height')
            browser.test.assertTrue(window?.incognito, 'The window should be in incognito mode')
            browser.test.assertEq(window?.type, 'popup', 'The window should be of type popup')
            browser.test.assertEq(window?.state, 'normal', 'The window state should be normal')
            browser.test.assertTrue(window?.focused, 'The window should be focused')

            browser.test.assertEq(typeof window?.tabs, 'object', 'The window should have a tabs array')
            browser.test.assertEq(window?.tabs?.length, 1, 'The tabs array should contain one tab')

            const tab = window?.tabs?.[0]
            browser.test.assertEq(typeof tab, 'object', 'The tab should be an object')
            browser.test.assertEq(tab?.active, true, 'The tab should be active')
            browser.test.assertTrue(tab?.incognito, 'The tab should be in incognito mode')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: windowsManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        context.hasAccessToPrivateData = true

        #expect(manager.windows.count == 1)

        try await manager.run()

        #expect(manager.windows.count == 2)
    }

    @Test
    func update() async throws {
        let backgroundScript = """
            const newWindow = await browser.windows.create();

            browser.test.assertEq(newWindow.top, 50, 'The new window top position should be 50 initially');
            browser.test.assertEq(newWindow.left, 100, 'The new window left position should be 100 initially');
            browser.test.assertEq(newWindow.width, 800, 'The new window width should be 800 initially');
            browser.test.assertEq(newWindow.height, 600, 'The new window height should be 600 initially');
            browser.test.assertTrue(newWindow.focused, 'The new window should be focused initially');
            browser.test.assertEq(newWindow.state, 'normal', 'The window state should be normal initially');

            let updatedWindow = await browser.windows.update(newWindow.id, { top: 10, width: 500, focused: true });

            browser.test.assertEq(updatedWindow.top, 10, 'The window top position should be updated to 10');
            browser.test.assertEq(updatedWindow.left, 100, 'The window left position should remain 100 after the first update');
            browser.test.assertEq(updatedWindow.width, 500, 'The window width should be updated to 500');
            browser.test.assertEq(updatedWindow.height, 600, 'The window height should remain 600 after the first update');
            browser.test.assertTrue(updatedWindow.focused, 'The window should be focused');
            browser.test.assertEq(updatedWindow.state, 'normal', 'The window state should remain normal after the first update');

            updatedWindow = await browser.windows.update(newWindow.id, { state: 'fullscreen' });

            browser.test.assertEq(updatedWindow.state, 'fullscreen', 'The window state should be updated to fullscreen');
            browser.test.assertEq(updatedWindow.top, 0, 'The window top position should be 0 in fullscreen');
            browser.test.assertEq(updatedWindow.left, 0, 'The window left position should be 0 in fullscreen');
            browser.test.assertEq(updatedWindow.width, 1920, 'The window width should be 1920 in fullscreen');
            browser.test.assertEq(updatedWindow.height, 1080, 'The window height should be 1080 in fullscreen');

            updatedWindow = await browser.windows.update(newWindow.id, { state: 'normal' });

            browser.test.assertEq(updatedWindow.top, 10, 'The window top position should be reverted back to 10');
            browser.test.assertEq(updatedWindow.left, 100, 'The window left position should remain 100');
            browser.test.assertEq(updatedWindow.width, 500, 'The window width should be reverted back to 500');
            browser.test.assertEq(updatedWindow.height, 600, 'The window height should be reverted back to 600');
            browser.test.assertEq(updatedWindow.state, 'normal', 'The window state should be reverted back to normal');

            browser.test.notifyPass();
            """

        let manager = try loadWebExtension(manifest: windowsManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func remove() async throws {
        let backgroundScript = """
            // Create a new window and verify the count
            const initialWindows = await browser.windows.getAll();
            const newWindow = await browser.windows.create();

            const windowsAfterCreate = await browser.windows.getAll();
            browser.test.assertEq(windowsAfterCreate.length, initialWindows.length + 1, 'One window should be added after creation');

            // Remove the window and verify the count
            await browser.windows.remove(newWindow.id);

            const windowsAfterRemove = await browser.windows.getAll();
            browser.test.assertEq(windowsAfterRemove.length, initialWindows.length, 'Window count should match the initial count after removal');

            browser.test.notifyPass();
            """

        let manager = try loadWebExtension(manifest: windowsManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func removeUnsupported() async throws {
        let backgroundScript = """
            browser.test.assertEq(browser.windows.remove, undefined)

            browser.test.notifyPass()
            """

        let manager = parseWebExtension(manifest: windowsManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        context.unsupportedAPIs = ["browser.windows.remove"]

        try await manager.loadAndRun()
    }

    #endif // WTF_PLATFORM_MAC
}

#endif // ENABLE_WK_WEB_EXTENSIONS
