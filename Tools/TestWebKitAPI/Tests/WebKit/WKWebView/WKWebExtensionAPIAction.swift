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
private import TestWebKitAPILibrary.Helpers.cocoa.WebExtensionUtilities
private import TestWebKitAPILibrary.Helpers.cocoa.TestWebExtensionsDelegate
import Testing
import WebKit

import struct Foundation.URL
import struct Swift.String

#if WTF_PLATFORM_MAC
private import AppKit
#else
private import UIKit
#endif

@MainActor
struct WKWebExtensionAPIActionTests {
    private let actionPopupManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "Action Test",
        "description": "Action Test",
        "version": "1",

        "permissions": ["webNavigation"],

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
    ]

    @Test
    func errors() async throws {
        let backgroundScript = """
            browser.test.assertThrows(() => browser.action.setTitle({tabId: 'bad', title: 'Test'}), /'tabId' is expected to be a number, but a string was provided/i)
            browser.test.assertThrows(() => browser.action.setTitle({windowId: 'bad', title: 'Test'}), /'windowId' is expected to be a number, but a string was provided/i)
            browser.test.assertThrows(() => browser.action.setIcon({path: 123}), /'path' is expected to be a string or an object or null, but a number was provided/i)
            browser.test.assertThrows(() => browser.action.setIcon({tabId: true, path: 'icon.png'}), /'tabId' is expected to be a number, but a boolean was provided/i)
            browser.test.assertThrows(() => browser.action.setIcon({windowId: true, path: 'icon.png'}), /'windowId' is expected to be a number, but a boolean was provided/i)
            browser.test.assertThrows(() => browser.action.setPopup({tabId: 'bad', popup: 'popup.html'}), /'tabId' is expected to be a number, but a string was provided/i)
            browser.test.assertThrows(() => browser.action.setPopup({windowId: 'bad', popup: 'popup.html'}), /'windowId' is expected to be a number, but a string was provided/i)
            browser.test.assertThrows(() => browser.action.setBadgeText({tabId: 1.2, text: '1'}), /'tabId' value is invalid, because it is not a tab identifier/i)
            browser.test.assertThrows(() => browser.action.setBadgeText({windowId: -3, text: '2'}), /'windowId' value is invalid, because it is not a window identifier/i)
            browser.test.assertThrows(() => browser.action.enable('bad'), /'tabId' value is invalid, because a number is expected/i)
            browser.test.assertThrows(() => browser.action.disable('bad'), /'tabId' value is invalid, because a number is expected/i)
            browser.test.assertThrows(() => browser.action.isEnabled({tabId: Infinity}), /'tabId' is expected to be a number, but Infinity was provided/i)
            browser.test.assertThrows(() => browser.action.isEnabled({windowId: NaN}), /'windowId' is expected to be a number, but NaN was provided/i)

            browser.test.assertThrows(() => browser.action.setTitle({tabId: 1}), /'details' value is invalid, because it is missing required keys: 'title'/i)
            browser.test.assertThrows(() => browser.action.setTitle({windowId: 1}), /'details' value is invalid, because it is missing required keys: 'title'/i)
            browser.test.assertThrows(() => browser.action.setPopup({tabId: 1}), /'details' value is invalid, because it is missing required keys: 'popup'/i)
            browser.test.assertThrows(() => browser.action.setPopup({windowId: 1}), /'details' value is invalid, because it is missing required keys: 'popup'/i)
            browser.test.assertThrows(() => browser.action.setBadgeText({tabId: 1}), /'details' value is invalid, because it is missing required keys: 'text'/i)
            browser.test.assertThrows(() => browser.action.setBadgeText({windowId: 1}), /'details' value is invalid, because it is missing required keys: 'text'/i)

            browser.test.assertThrows(() => browser.action.setTitle({tabId: 1, windowId: 1, title: null}), /'details' value is invalid, because it cannot specify both 'tabId' and 'windowId'/i)
            browser.test.assertThrows(() => browser.action.setIcon({tabId: 1, windowId: 1, path: null}), /'details' value is invalid, because it cannot specify both 'tabId' and 'windowId'/i)
            browser.test.assertThrows(() => browser.action.setPopup({tabId: 1, windowId: 1, popup: null}), /'details' value is invalid, because it cannot specify both 'tabId' and 'windowId'/i)
            browser.test.assertThrows(() => browser.action.setBadgeText({tabId: 1, windowId: 1, text: null}), /'details' value is invalid, because it cannot specify both 'tabId' and 'windowId'/i)

            await browser.test.assertRejects(browser.action.getTitle({tabId: 9999}), /tab not found/i)
            await browser.test.assertRejects(browser.action.getTitle({windowId: 9999}), /window not found/i)
            await browser.test.assertRejects(browser.action.getPopup({tabId: 9999}), /tab not found/i)
            await browser.test.assertRejects(browser.action.getPopup({windowId: 9999}), /window not found/i)
            await browser.test.assertRejects(browser.action.getBadgeText({tabId: 9999}), /tab not found/i)
            await browser.test.assertRejects(browser.action.getBadgeText({windowId: 9999}), /window not found/i)
            await browser.test.assertRejects(browser.action.isEnabled({tabId: 9999}), /tab not found/i)
            await browser.test.assertRejects(browser.action.isEnabled({windowId: 9999}), /window not found/i)

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func clickedEvent() async throws {
        let backgroundScript = """
            browser.action.setPopup({ popup: '' })

            browser.action.onClicked.addListener((tab) => {
              browser.test.assertEq(typeof tab, 'object', 'The tab should be an object')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Test Action')
            """

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        try await manager.waitForTestMessage("Test Action")

        context.performAction(for: manager.defaultTab)

        try await manager.run()
    }

    @Test
    func presentPopupForAction() async throws {
        let resources: [String: Any] = [
            "background.js": "browser.test.sendMessage('Test Popup Action')",
            "popup.html": "<b>Hello World!</b>",
            "toolbar-16.png": makePNGData(size: CGSize(width: 16, height: 16), color: .red),
            "toolbar-32.png": makePNGData(size: CGSize(width: 32, height: 32), color: .blue),
        ]

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)
        let context = try #require(manager.context)

        try await manager.waitForTestMessage("Test Popup Action")

        let action = try await manager.nextPresentedAction {
            context.performAction(for: manager.defaultTab)
        }

        #expect(action.presentsPopup)
        #expect(action.isEnabled)
        #expect(action.badgeText.isEmpty)
        #expect(action.label == "Test Action")

        let smallIcon = try #require(action.icon(for: CGSize(width: 16, height: 16)))
        #expect(smallIcon.size == CGSize(width: 16, height: 16))

        let largeIcon = try #require(action.icon(for: CGSize(width: 32, height: 32)))
        #expect(largeIcon.size == CGSize(width: 32, height: 32))

        #if WTF_PLATFORM_MAC
        #expect(action.popupPopover != nil)
        #else
        #expect(action.popupViewController != nil)
        #endif

        let popupWebView = try #require(action.popupWebView)
        #expect(!popupWebView.isLoading)

        let popupURL = try #require(popupWebView.url)
        #expect(popupURL.scheme == "webkit-extension")
        #expect(popupURL.path == "/popup.html")

        action.closePopup()
    }

    #if WTF_PLATFORM_MAC
    @Test
    func popoverCloseNotificationCallsClosePopup() async throws {
        let resources: [String: Any] = [
            "background.js": "browser.test.sendMessage('Test Popup Action')",
            "popup.html": "<b>Hello World!</b>",
            "toolbar-16.png": makePNGData(size: CGSize(width: 16, height: 16), color: .red),
            "toolbar-32.png": makePNGData(size: CGSize(width: 32, height: 32), color: .blue),
        ]

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)
        let context = try #require(manager.context)

        try await manager.waitForTestMessage("Test Popup Action")

        let firstAction = try await manager.nextPresentedAction {
            context.performAction(for: manager.defaultTab)
        }
        let firstWebView = try #require(firstAction.popupWebView)

        // Simulate the popup being dismissed by the user (for example, by clicking outside), which
        // posts NSPopoverDidCloseNotification. That should invoke popoverDidClose: and call
        // closePopup(), resetting the popup state so the popup can be presented again.
        NotificationCenter.default.post(name: NSPopover.didCloseNotification, object: firstAction.popupPopover)

        // Without the fix, m_popupPresented remains true and performActionForTab: is a no-op, so
        // the delegate is never called a second time and this await never returns.
        let secondAction = try await manager.nextPresentedAction {
            context.performAction(for: manager.defaultTab)
        }
        let secondWebView = try #require(secondAction.popupWebView)

        // A fresh web view for the second presentation confirms m_popupWebView was cleared.
        #expect(firstWebView !== secondWebView)
    }
    #endif // WTF_PLATFORM_MAC

    @Test
    func getCurrentTabAndWindowFromPopupPage() async throws {
        let popupScript = """
            const tab = await browser.tabs.getCurrent()
            browser.test.assertEq(typeof tab, 'object', 'The tab should be')
            browser.test.assertTrue(tab.active, 'The current tab should be active')

            const window = await browser.windows.getCurrent()
            browser.test.assertEq(typeof window, 'object', 'The window should be')
            browser.test.assertTrue(window.focused, 'The current window should be focused')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": "browser.test.sendMessage('Test Popup Action')",
            "popup.html": "<script type='module' src='popup.js'></script>",
            "popup.js": popupScript,
        ]

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)
        let context = try #require(manager.context)

        try await manager.waitForTestMessage("Test Popup Action")

        let action = try await manager.nextPresentedAction {
            context.performAction(for: manager.defaultTab)
        }

        #expect(action.presentsPopup)
        #expect(action.popupWebView != nil)

        try await manager.run()
    }

    @Test
    func updateTabFromPopupPage() async throws {
        let popupScript = """
            const tab = await browser.tabs.getCurrent()
            browser.test.assertEq(typeof tab, 'object', 'The tab should be')
            browser.test.assertTrue(tab.active, 'The current tab should be active')

            browser.test.assertFalse(tab.mutedInfo.muted, 'The tab should not be initially muted')

            const updatedTab = await browser.tabs.update({
              muted: true,
            })

            browser.test.assertTrue(updatedTab.mutedInfo.muted, 'The tab should be muted after update')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": "browser.test.sendMessage('Test Popup Action')",
            "popup.html": "<script type='module' src='popup.js'></script>",
            "popup.js": popupScript,
        ]

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)
        let context = try #require(manager.context)

        try await manager.waitForTestMessage("Test Popup Action")

        let action = try await manager.nextPresentedAction {
            context.performAction(for: manager.defaultTab)
        }

        #expect(action.presentsPopup)
        #expect(action.popupWebView != nil)

        try await manager.run()
    }

    @Test
    func setDefaultActionProperties() async throws {
        let backgroundScript = """
            browser.test.assertEq(await browser.action.getTitle({ }), 'Test Action', 'Title should be')
            browser.test.assertEq(await browser.action.getPopup({ }), 'popup.html', 'Popup should be')
            browser.test.assertEq(await browser.action.getBadgeText({ }), '', 'Badge text should be')
            browser.test.assertTrue(await browser.action.isEnabled({ }), 'Action should be enabled')

            await browser.action.setTitle({ title: 'Modified Title' })
            await browser.action.setIcon({ path: 'toolbar-48.png' })
            await browser.action.setPopup({ popup: 'alt-popup.html' })
            await browser.action.setBadgeText({ text: '42' })
            await browser.action.disable()

            browser.test.assertEq(await browser.action.getTitle({ }), 'Modified Title', 'Title should be')
            browser.test.assertEq(await browser.action.getPopup({ }), 'alt-popup.html', 'Popup should be')
            browser.test.assertEq(await browser.action.getBadgeText({ }), '42', 'Badge text should be')
            browser.test.assertFalse(await browser.action.isEnabled({ }), 'Action should be disabled')

            browser.action.openPopup()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "alt-popup.html": "<b>Hello World!</b>",
            "toolbar-48.png": makePNGData(size: CGSize(width: 48, height: 48), color: .yellow),
        ]

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)
        let context = try #require(manager.context)

        let action = try await manager.nextPresentedAction()

        let defaultAction = try #require(context.action(for: nil))
        #expect(defaultAction.presentsPopup)
        #expect(!defaultAction.isEnabled)
        #expect(defaultAction.label == "Modified Title")
        #expect(defaultAction.badgeText == "42")
        #expect(!defaultAction.hasUnreadBadgeText)

        #expect(action.associatedTab === manager.defaultTab)

        #expect(!action.isEnabled)
        #expect(action.label == "Modified Title")
        #expect(action.badgeText == "42")
        #expect(!action.hasUnreadBadgeText)

        let icon = try #require(action.icon(for: CGSize(width: 48, height: 48)))
        #expect(icon.size == CGSize(width: 48, height: 48))

        #expect(action.presentsPopup)
        #expect(!action.isEnabled)

        let popupWebView = try #require(action.popupWebView)
        #expect(!popupWebView.isLoading)

        let popupURL = try #require(popupWebView.url)
        #expect(popupURL.scheme == "webkit-extension")
        #expect(popupURL.path == "/alt-popup.html")

        action.closePopup()
    }

    @Test
    func tabSpecificActionProperties() async throws {
        let backgroundScript = """
            const [currentTab] = await browser.tabs.query({ active: true, currentWindow: true })

            browser.test.assertEq(await browser.action.getTitle({ tabId: currentTab.id }), 'Test Action', 'Title should be')
            browser.test.assertEq(await browser.action.getPopup({ tabId: currentTab.id }), 'popup.html', 'Popup should be')
            browser.test.assertEq(await browser.action.getBadgeText({ tabId: currentTab.id }), '', 'Badge text should be')
            browser.test.assertTrue(await browser.action.isEnabled({ tabId: currentTab.id }), 'Action should be enabled')

            browser.action.setTitle({ title: 'Tab Title', tabId: currentTab.id })
            browser.action.setIcon({ path: 'toolbar-48.png', tabId: currentTab.id })
            browser.action.setPopup({ popup: 'alt-popup.html', tabId: currentTab.id })
            browser.action.setBadgeText({ text: '42', tabId: currentTab.id })
            browser.action.disable(currentTab.id)

            browser.test.assertEq(await browser.action.getTitle({ tabId: currentTab.id }), 'Tab Title', 'Title should be')
            browser.test.assertEq(await browser.action.getPopup({ tabId: currentTab.id }), 'alt-popup.html', 'Popup should be')
            browser.test.assertEq(await browser.action.getBadgeText({ tabId: currentTab.id }), '42', 'Badge text should be')
            browser.test.assertFalse(await browser.action.isEnabled({ tabId: currentTab.id }), 'Action should be disabled')

            browser.action.openPopup({ windowId: currentTab.windowId })
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "popup.html": "<b>Hello World!</b>",
            "alt-popup.html": "<b>Hello Alternate World!</b>",
            "toolbar-16.png": makePNGData(size: CGSize(width: 16, height: 16), color: .red),
            "toolbar-32.png": makePNGData(size: CGSize(width: 32, height: 32), color: .blue),
            "toolbar-48.png": makePNGData(size: CGSize(width: 48, height: 48), color: .yellow),
        ]

        let manager = parseWebExtension(manifest: actionPopupManifest, resources: resources)
        let context = try #require(manager.context)
        let defaultWindow = try #require(manager.defaultWindow)

        defaultWindow.openNewTab()
        manager.openNewWindow()
        manager.load()

        let action = try await manager.nextPresentedAction()

        let defaultAction = try #require(context.action(for: nil))
        #expect(defaultAction.presentsPopup)
        #expect(defaultAction.isEnabled)
        #expect(defaultAction.label == "Test Action")
        #expect(defaultAction.badgeText.isEmpty)
        #expect(!defaultAction.hasUnreadBadgeText)

        let defaultIcon = try #require(defaultAction.icon(for: CGSize(width: 32, height: 32)))
        #expect(defaultIcon.size == CGSize(width: 32, height: 32))

        #expect(!action.isEnabled)
        #expect(action.label == "Tab Title")
        #expect(action.badgeText == "42")
        #expect(!action.hasUnreadBadgeText)

        let icon = try #require(action.icon(for: CGSize(width: 48, height: 48)))
        #expect(icon.size == CGSize(width: 48, height: 48))

        #expect(action.presentsPopup)
        #expect(!action.isEnabled)

        let popupWebView = try #require(action.popupWebView)
        #expect(!popupWebView.isLoading)

        let popupURL = try #require(popupWebView.url)
        #expect(popupURL.scheme == "webkit-extension")
        #expect(popupURL.path == "/alt-popup.html")

        let secondTabAction = try #require(context.action(for: defaultWindow.tabs.last))
        #expect(secondTabAction.presentsPopup == defaultAction.presentsPopup)
        #expect(secondTabAction.isEnabled == defaultAction.isEnabled)
        #expect(secondTabAction.label == defaultAction.label)
        #expect(secondTabAction.badgeText == defaultAction.badgeText)
        #expect(!secondTabAction.hasUnreadBadgeText)

        let secondTabIcon = try #require(secondTabAction.icon(for: CGSize(width: 32, height: 32)))
        #expect(secondTabIcon.size == defaultIcon.size)

        let secondTabPopupURL = try #require(secondTabAction.popupWebView?.url)
        #expect(secondTabPopupURL.scheme == "webkit-extension")
        #expect(secondTabPopupURL.path == "/popup.html")

        let secondWindowAction = try #require(context.action(for: manager.windows[1].tabs[0]))
        #expect(secondWindowAction.presentsPopup == defaultAction.presentsPopup)
        #expect(secondWindowAction.isEnabled == defaultAction.isEnabled)
        #expect(secondWindowAction.label == defaultAction.label)
        #expect(secondWindowAction.badgeText == defaultAction.badgeText)
        #expect(!secondWindowAction.hasUnreadBadgeText)

        let secondWindowIcon = try #require(secondWindowAction.icon(for: CGSize(width: 32, height: 32)))
        #expect(secondWindowIcon.size == defaultIcon.size)

        let secondWindowPopupURL = try #require(secondWindowAction.popupWebView?.url)
        #expect(secondWindowPopupURL.scheme == "webkit-extension")
        #expect(secondWindowPopupURL.path == "/popup.html")

        secondTabAction.closePopup()
        secondWindowAction.closePopup()
        action.closePopup()
    }

    @Test
    func windowSpecificActionProperties() async throws {
        let backgroundScript = """
            const [currentTab] = await browser.tabs.query({ active: true, currentWindow: true })
            const currentWindowId = currentTab.windowId

            browser.test.assertEq(await browser.action.getTitle({ windowId: currentWindowId }), 'Test Action', 'Title should be')
            browser.test.assertEq(await browser.action.getPopup({ windowId: currentWindowId }), 'popup.html', 'Popup should be')
            browser.test.assertEq(await browser.action.getBadgeText({ windowId: currentWindowId }), '', 'Badge text should be')

            browser.action.setTitle({ title: 'Window Title', windowId: currentWindowId })
            browser.action.setIcon({ path: 'window-toolbar-48.png', windowId: currentWindowId })
            browser.action.setPopup({ popup: 'window-popup.html', windowId: currentWindowId })
            browser.action.setBadgeText({ text: 'W', windowId: currentWindowId })

            browser.test.assertEq(await browser.action.getTitle({ windowId: currentWindowId }), 'Window Title', 'Title should be')
            browser.test.assertEq(await browser.action.getPopup({ windowId: currentWindowId }), 'window-popup.html', 'Popup should be')
            browser.test.assertEq(await browser.action.getBadgeText({ windowId: currentWindowId }), 'W', 'Badge text should be')

            browser.action.openPopup({ windowId: currentWindowId })
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "popup.html": "<b>Hello World!</b>",
            "window-popup.html": "<b>Window-Specific Popup!</b>",
            "toolbar-32.png": makePNGData(size: CGSize(width: 32, height: 32), color: .red),
            "window-toolbar-48.png": makePNGData(size: CGSize(width: 48, height: 48), color: .green),
        ]

        let manager = parseWebExtension(manifest: actionPopupManifest, resources: resources)
        let context = try #require(manager.context)

        manager.openNewWindow()
        manager.load()

        let action = try await manager.nextPresentedAction()

        let defaultAction = try #require(context.action(for: nil))
        #expect(defaultAction.presentsPopup)
        #expect(defaultAction.isEnabled)
        #expect(defaultAction.label == "Test Action")
        #expect(defaultAction.badgeText.isEmpty)
        #expect(!defaultAction.hasUnreadBadgeText)

        let defaultIcon = try #require(defaultAction.icon(for: CGSize(width: 32, height: 32)))
        #expect(defaultIcon.size == CGSize(width: 32, height: 32))

        #expect(action.isEnabled)
        #expect(action.label == "Window Title")
        #expect(action.badgeText == "W")
        #expect(!action.hasUnreadBadgeText)

        let windowIcon = try #require(action.icon(for: CGSize(width: 48, height: 48)))
        #expect(windowIcon.size == CGSize(width: 48, height: 48))

        #expect(action.presentsPopup)

        let popupURL = try #require(action.popupWebView?.url)
        #expect(popupURL.scheme == "webkit-extension")
        #expect(popupURL.path == "/window-popup.html")

        let secondWindowAction = try #require(context.action(for: manager.windows[1].tabs[0]))
        #expect(secondWindowAction.presentsPopup == defaultAction.presentsPopup)
        #expect(secondWindowAction.isEnabled == defaultAction.isEnabled)
        #expect(secondWindowAction.label == defaultAction.label)
        #expect(secondWindowAction.badgeText == defaultAction.badgeText)
        #expect(!secondWindowAction.hasUnreadBadgeText)

        let secondWindowIcon = try #require(secondWindowAction.icon(for: CGSize(width: 32, height: 32)))
        #expect(secondWindowIcon.size == defaultIcon.size)

        let secondWindowPopupURL = try #require(secondWindowAction.popupWebView?.url)
        #expect(secondWindowPopupURL.scheme == "webkit-extension")
        #expect(secondWindowPopupURL.path == "/popup.html")

        secondWindowAction.closePopup()
        action.closePopup()
    }

    @Test
    func setIconSinglePath() async throws {
        let resources: [String: Any] = [
            "background.js": "await browser.action.setIcon({ path: 'toolbar-48.png' })",
            "toolbar-48.png": makePNGData(size: CGSize(width: 48, height: 48), color: .yellow),
            "popup.html": "Hello world!",
        ]

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)

        let action = try await manager.nextUpdatedAction()

        let icon = try #require(action.icon(for: CGSize(width: 48, height: 48)))
        #expect(icon.size == CGSize(width: 48, height: 48))
    }

    @Test
    func setIconSinglePathRelative() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Action Test",
            "description": "Action Test",
            "version": "1",

            "background": [
                "page": "background/index.html",
                "type": "module",
                "persistent": false,
            ],

            "action": [
                "default_title": "Test Action",
                "default_popup": "popup.html",
                "default_icon": [
                    "16": "icons/toolbar-16.png",
                    "32": "icons/toolbar-32.png",
                ],
            ],
        ]

        let resources: [String: Any] = [
            "background/index.html": "<script type='module' src='script.js'></script>",
            "background/script.js": "await browser.action.setIcon({ path: '../icons/toolbar-48.png' })",
            "icons/toolbar-48.png": makePNGData(size: CGSize(width: 48, height: 48), color: .yellow),
            "popup.html": "Hello world!",
        ]

        let manager = try loadWebExtension(manifest: manifest, resources: resources)

        let action = try await manager.nextUpdatedAction()

        let icon = try #require(action.icon(for: CGSize(width: 48, height: 48)))
        #expect(icon.size == CGSize(width: 48, height: 48))
    }

    @Test
    func setIconMultipleSizes() async throws {
        let backgroundScript =
            "await browser.action.setIcon({ path: { '48': 'toolbar-48.png', '96': 'toolbar-96.png', '128': 'toolbar-128.png' } })"

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "toolbar-48.png": makePNGData(size: CGSize(width: 48, height: 48), color: .yellow),
            "toolbar-96.png": makePNGData(size: CGSize(width: 96, height: 96), color: .green),
            "toolbar-128.png": makePNGData(size: CGSize(width: 128, height: 128), color: .purple),
            "popup.html": "Hello world!",
        ]

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)

        let action = try await manager.nextUpdatedAction()

        let icon48 = try #require(action.icon(for: CGSize(width: 48, height: 48)))
        #expect(icon48.size == CGSize(width: 48, height: 48))

        let icon96 = try #require(action.icon(for: CGSize(width: 96, height: 96)))
        #expect(icon96.size == CGSize(width: 96, height: 96))

        let icon128 = try #require(action.icon(for: CGSize(width: 128, height: 128)))
        #expect(icon128.size == CGSize(width: 128, height: 128))
    }

    @Test
    func setIconMultipleSizesRelative() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Action Test",
            "description": "Action Test",
            "version": "1",

            "background": [
                "page": "background/index.html",
                "type": "module",
                "persistent": false,
            ],

            "action": [
                "default_title": "Test Action",
                "default_popup": "popup.html",
                "default_icon": [
                    "16": "icons/toolbar-16.png",
                    "32": "icons/toolbar-32.png",
                ],
            ],
        ]

        let backgroundScript =
            "await browser.action.setIcon({ path: { '48': '../icons/toolbar-48.png', '96': '../icons/toolbar-96.png', '128': '../icons/toolbar-128.png' } })"

        let resources: [String: Any] = [
            "background/index.html": "<script type='module' src='script.js'></script>",
            "background/script.js": backgroundScript,
            "icons/toolbar-48.png": makePNGData(size: CGSize(width: 48, height: 48), color: .yellow),
            "icons/toolbar-96.png": makePNGData(size: CGSize(width: 96, height: 96), color: .green),
            "icons/toolbar-128.png": makePNGData(size: CGSize(width: 128, height: 128), color: .purple),
            "popup.html": "Hello world!",
        ]

        let manager = try loadWebExtension(manifest: manifest, resources: resources)

        let action = try await manager.nextUpdatedAction()

        let icon48 = try #require(action.icon(for: CGSize(width: 48, height: 48)))
        #expect(icon48.size == CGSize(width: 48, height: 48))

        let icon96 = try #require(action.icon(for: CGSize(width: 96, height: 96)))
        #expect(icon96.size == CGSize(width: 96, height: 96))

        let icon128 = try #require(action.icon(for: CGSize(width: 128, height: 128)))
        #expect(icon128.size == CGSize(width: 128, height: 128))
    }

    @Test
    func setIconWithBadPath() async throws {
        let resources: [String: Any] = [
            "background.js": "await browser.action.setIcon({ path: 'bad.png' })",
            "popup.html": "Hello world!",
        ]

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)

        let action = try await manager.nextUpdatedAction()

        #expect(action.icon(for: CGSize(width: 48, height: 48)) == nil)
    }

    @Test
    func setIconWithImageData() async throws {
        let backgroundScript = """
            const context = new OffscreenCanvas(48, 48).getContext('2d')
            context.fillStyle = 'green'
            context.fillRect(0, 0, 48, 48)

            const imageData = context.getImageData(0, 0, 48, 48)
            await browser.action.setIcon({ imageData })
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "popup.html": "Hello world!",
        ]

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)

        let action = try await manager.nextUpdatedAction()

        let icon = try #require(action.icon(for: CGSize(width: 48, height: 48)))
        #expect(icon.size == CGSize(width: 48, height: 48))
    }

    @Test
    func setIconWithBadImageData() async throws {
        let backgroundScript = """
            browser.test.assertThrows(() => browser.action.setIcon({ imageData: { 16: { data: [ 'bad' ] } } }))

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func setIconWithMultipleImageDataSizes() async throws {
        let backgroundScript = """
            const createImageData = (size, color) => {
              const context = new OffscreenCanvas(size, size).getContext('2d')
              context.fillStyle = color
              context.fillRect(0, 0, size, size)

              return context.getImageData(0, 0, size, size)
            }

            const imageData48 = createImageData(48, 'green')
            const imageData96 = createImageData(96, 'blue')
            const imageData128 = createImageData(128, 'red')

            await browser.action.setIcon({ imageData: { '48': imageData48, '96': imageData96, '128': imageData128 } })
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "popup.html": "Hello world!",
        ]

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)

        let action = try await manager.nextUpdatedAction()

        let icon48 = try #require(action.icon(for: CGSize(width: 48, height: 48)))
        #expect(icon48.size == CGSize(width: 48, height: 48))

        let icon96 = try #require(action.icon(for: CGSize(width: 96, height: 96)))
        #expect(icon96.size == CGSize(width: 96, height: 96))

        let icon128 = try #require(action.icon(for: CGSize(width: 128, height: 128)))
        #expect(icon128.size == CGSize(width: 128, height: 128))
    }

    @Test
    func setIconWithDataURL() async throws {
        let backgroundScript = """
            const canvas = document.createElement('canvas')
            canvas.width = 48
            canvas.height = 48

            const context = canvas.getContext('2d')
            context.fillStyle = 'blue'
            context.fillRect(0, 0, 48, 48)

            const pngDataURL48 = canvas.toDataURL('image/png')

            await browser.action.setIcon({ path: pngDataURL48 })
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "popup.html": "Hello world!",
        ]

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)

        let action = try await manager.nextUpdatedAction()

        let icon = try #require(action.icon(for: CGSize(width: 48, height: 48)))
        #expect(icon.size == CGSize(width: 48, height: 48))
    }

    @Test
    func setIconWithBadDataURL() async throws {
        let backgroundScript = """
            const invalidDataURL = 'data:image/png;base64,INVALIDDATA'

            await browser.action.setIcon({ path: invalidDataURL })
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "popup.html": "Hello world!",
        ]

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)

        let action = try await manager.nextUpdatedAction()

        #expect(action.icon(for: CGSize(width: 48, height: 48)) == nil)
    }

    @Test
    func setIconWithNullPath() async throws {
        let backgroundScript = """
            await browser.action.setIcon({ path: null })
            browser.test.sendMessage('Icon Set')
            """

        try await expectDefaultIconAfterSettingIcon(with: backgroundScript)
    }

    @Test
    func setIconWithNullImageData() async throws {
        let backgroundScript = """
            await browser.action.setIcon({ imageData: null })
            browser.test.sendMessage('Icon Set')
            """

        try await expectDefaultIconAfterSettingIcon(with: backgroundScript)
    }

    @Test
    func setIconWithNullVariants() async throws {
        let backgroundScript = """
            await browser.action.setIcon({ variants: null })
            browser.test.sendMessage('Icon Set')
            """

        try await expectDefaultIconAfterSettingIcon(with: backgroundScript)
    }

    /// Runs a background script that clears the action icon and sends `Icon Set`, then checks that
    /// the action fell back to the icon declared in the manifest.
    private func expectDefaultIconAfterSettingIcon(
        with backgroundScript: String,
        sourceLocation: SourceLocation = #_sourceLocation
    ) async throws {
        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "toolbar-32.png": makePNGData(size: CGSize(width: 32, height: 32), color: .blue),
            "popup.html": "Hello world!",
        ]

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)
        let context = try #require(manager.context)

        try await manager.waitForTestMessage("Icon Set")

        let action = try #require(context.action(for: nil), sourceLocation: sourceLocation)
        let icon = try #require(action.icon(for: CGSize(width: 32, height: 32)), sourceLocation: sourceLocation)
        #expect(icon.size == CGSize(width: 32, height: 32), sourceLocation: sourceLocation)
    }

    @Test
    func setIconWithMultipleDataURLs() async throws {
        let backgroundScript = """
            const canvas = document.createElement('canvas')

            canvas.width = 48
            canvas.height = 48

            let context = canvas.getContext('2d')
            context.fillStyle = 'blue'
            context.fillRect(0, 0, 48, 48)

            const pngDataURL48 = canvas.toDataURL('image/png')

            canvas.width = 96
            canvas.height = 96

            context = canvas.getContext('2d')
            context.fillStyle = 'red'
            context.fillRect(0, 0, 96, 96)

            const pngDataURL96 = canvas.toDataURL('image/png')

            await browser.action.setIcon({ path: { '48': pngDataURL48, '96': pngDataURL96 } })
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "popup.html": "Hello world!",
        ]

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)

        let action = try await manager.nextUpdatedAction()

        let icon48 = try #require(action.icon(for: CGSize(width: 48, height: 48)))
        #expect(icon48.size == CGSize(width: 48, height: 48))

        let icon96 = try #require(action.icon(for: CGSize(width: 96, height: 96)))
        #expect(icon96.size == CGSize(width: 96, height: 96))
    }

    @Test
    func setIconSymbolSinglePath() async throws {
        let resources: [String: Any] = [
            "background.js": "await browser.action.setIcon({ path: 'symbol:star' })",
            "popup.html": "Hello world!",
        ]

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)

        let action = try await manager.nextUpdatedAction()

        let icon = try #require(action.icon(for: CGSize(width: 16, height: 16)))
        #expect(icon.isSymbol)
    }

    @Test
    func setIconSymbolIconsDictionary() async throws {
        let resources: [String: Any] = [
            "background.js": "await browser.action.setIcon({ path: { '16': 'symbol:heart.fill' } })",
            "popup.html": "Hello world!",
        ]

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)

        let action = try await manager.nextUpdatedAction()

        let icon = try #require(action.icon(for: CGSize(width: 16, height: 16)))
        #expect(icon.isSymbol)
    }

    #if ENABLE_WK_WEB_EXTENSIONS_ICON_VARIANTS
    @Test
    func setIconWithVariants() async throws {
        let backgroundScript = """
            await browser.test.assertSafeResolve(() => browser.action.setIcon({
                variants: [
                    { 32: 'action-dark-32.png', 64: 'action-dark-64.png', 'colorSchemes': [ 'dark' ] },
                    { 32: 'action-light-32.png', 64: 'action-light-64.png', 'colorSchemes': [ 'light' ] }
                ]
            }))
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "popup.html": "Hello world!",
            "action-dark-32.png": makePNGData(size: CGSize(width: 32, height: 32), color: .white),
            "action-dark-64.png": makePNGData(size: CGSize(width: 64, height: 64), color: .white),
            "action-light-32.png": makePNGData(size: CGSize(width: 32, height: 32), color: .black),
            "action-light-64.png": makePNGData(size: CGSize(width: 64, height: 64), color: .black),
        ]

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)

        let action = try await manager.nextUpdatedAction()

        let icon32 = try #require(action.icon(for: CGSize(width: 32, height: 32)))
        #expect(icon32.size == CGSize(width: 32, height: 32))

        let icon64 = try #require(action.icon(for: CGSize(width: 64, height: 64)))
        #expect(icon64.size == CGSize(width: 64, height: 64))

        withAppearance(.dark) {
            #expect(compareColors(pixelColor(of: icon32), .white))
            #expect(compareColors(pixelColor(of: icon64), .white))
        }

        withAppearance(.light) {
            #expect(compareColors(pixelColor(of: icon32), .black))
            #expect(compareColors(pixelColor(of: icon64), .black))
        }
    }

    @Test
    func setIconWithImageDataAndVariants() async throws {
        let backgroundScript = """
            const createImageData = (size, color) => {
              const context = new OffscreenCanvas(size, size).getContext('2d')
              context.fillStyle = color
              context.fillRect(0, 0, size, size)

              return context.getImageData(0, 0, size, size)
            }

            const imageDataDark32 = createImageData(32, 'white')
            const imageDataDark64 = createImageData(64, 'white')
            const imageDataLight32 = createImageData(32, 'black')
            const imageDataLight64 = createImageData(64, 'black')

            await browser.test.assertSafeResolve(() => browser.action.setIcon({
                variants: [
                    { 32: imageDataDark32, 64: imageDataDark64, 'colorSchemes': [ 'dark' ] },
                    { 32: imageDataLight32, 64: imageDataLight64, 'colorSchemes': [ 'light' ] }
                ]
            }))
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "popup.html": "Hello world!",
        ]

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)

        let action = try await manager.nextUpdatedAction()

        let icon32 = try #require(action.icon(for: CGSize(width: 32, height: 32)))
        #expect(icon32.size == CGSize(width: 32, height: 32))

        let icon64 = try #require(action.icon(for: CGSize(width: 64, height: 64)))
        #expect(icon64.size == CGSize(width: 64, height: 64))

        withAppearance(.dark) {
            #expect(compareColors(pixelColor(of: icon32), .white))
            #expect(compareColors(pixelColor(of: icon64), .white))
        }

        withAppearance(.light) {
            #expect(compareColors(pixelColor(of: icon32), .black))
            #expect(compareColors(pixelColor(of: icon64), .black))
        }
    }

    @Test
    func setIconThrowsWithNoValidVariants() async throws {
        let backgroundScript = """
            const createImageData = (size, color) => {
              const context = new OffscreenCanvas(size, size).getContext('2d')
              context.fillStyle = color
              context.fillRect(0, 0, size, size)

              return context.getImageData(0, 0, size, size)
            }

            const invalidImageData = createImageData(32, 'white')

            await browser.test.assertThrows(() => browser.action.setIcon({
                variants: [ { 'thirtytwo': invalidImageData, 'colorSchemes': [ 'light' ] } ]
            }), /'variants\\[0\\]' value is invalid, because 'thirtytwo' is not a valid dimension/s)

            await browser.test.assertThrows(() => browser.action.setIcon({
                variants: [ { 32: invalidImageData, 'colorSchemes': [ 'bad' ] } ]
            }), /'variants\\[0\\]\\['colorSchemes'\\]' value is invalid, because it must specify either 'light' or 'dark'/s)

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "popup.html": "Hello world!",
        ]

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)
        try await manager.run()
    }

    @Test
    func setIconWithMixedValidAndInvalidVariants() async throws {
        let backgroundScript = """
            const createImageData = (size, color) => {
              const context = new OffscreenCanvas(size, size).getContext('2d')
              context.fillStyle = color
              context.fillRect(0, 0, size, size)

              return context.getImageData(0, 0, size, size)
            }

            const imageDataLight32 = createImageData(32, 'black')
            const invalidImageData = createImageData(32, 'white')

            await browser.test.assertSafeResolve(() => browser.action.setIcon({
                variants: [
                    { '32': imageDataLight32, 'colorSchemes': ['light'] },
                    { '32.5': invalidImageData, 'colorSchemes': ['dark'] }
                ]
            }))
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "popup.html": "Hello world!",
        ]

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)

        let action = try await manager.nextUpdatedAction()

        let icon32 = try #require(action.icon(for: CGSize(width: 32, height: 32)))
        #expect(icon32.size == CGSize(width: 32, height: 32))

        withAppearance(.light) {
            #expect(compareColors(pixelColor(of: icon32), .black))
        }

        withAppearance(.dark) {
            // Should still be black, as the light variant is used.
            #expect(compareColors(pixelColor(of: icon32), .black))
        }
    }

    @Test
    func setIconWithAnySizeVariantAndSVGDataURL() async throws {
        let backgroundScript = """
            const whiteSVGData = 'data:image/svg+xml;base64,' + btoa(`
              <svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">
                <rect width="100" height="100" fill="white" />
              </svg>`)

            const blackSVGData = 'data:image/svg+xml;base64,' + btoa(`
              <svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">
                <rect width="100" height="100" fill="black" />
              </svg>`)

            await browser.test.assertSafeResolve(() => browser.action.setIcon({
                variants: [
                    { any: whiteSVGData, 'colorSchemes': [ 'dark' ] },
                    { any: blackSVGData, 'colorSchemes': [ 'light' ] }
                ]
            }))
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "popup.html": "Hello World!",
        ]

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)

        let action = try await manager.nextUpdatedAction()

        let iconAnySize = try #require(action.icon(for: CGSize(width: 48, height: 48)))
        #expect(iconAnySize.size == CGSize(width: 48, height: 48))

        withAppearance(.dark) {
            #expect(compareColors(pixelColor(of: iconAnySize), .white))
        }

        withAppearance(.light) {
            #expect(compareColors(pixelColor(of: iconAnySize), .black))
        }
    }

    @Test
    func setIconWithSymbolVariants() async throws {
        let backgroundScript = """
            await browser.test.assertSafeResolve(() => browser.action.setIcon({
                variants: [
                    { any: 'symbol:star' }
                ]
            }))
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "popup.html": "Hello world!",
        ]

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)

        let action = try await manager.nextUpdatedAction()

        let icon = try #require(action.icon(for: CGSize(width: 32, height: 32)))
        #expect(icon.isSymbol)
    }
    #endif // ENABLE_WK_WEB_EXTENSIONS_ICON_VARIANTS

    @Test
    func browserAction() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 2,

            "name": "Browser Action Test",
            "description": "Browser Action Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "browser_action": [
                "default_title": "Test Browser Action",
                "default_popup": "popup.html",
                "default_icon": [
                    "16": "toolbar-16.png",
                    "32": "toolbar-32.png",
                ],
            ],
        ]

        let backgroundScript = """
            await browser.browserAction.setTitle({ title: 'Modified Title' })
            await browser.browserAction.setIcon({ path: 'toolbar-48.png' })
            await browser.browserAction.setPopup({ popup: 'alt-popup.html' })
            await browser.browserAction.setBadgeText({ text: '42' })
            await browser.browserAction.disable()

            browser.browserAction.openPopup()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "alt-popup.html": "<b>Hello World!</b>",
            "toolbar-48.png": makePNGData(size: CGSize(width: 48, height: 48), color: .yellow),
        ]

        let manager = try loadWebExtension(manifest: manifest, resources: resources)
        let context = try #require(manager.context)

        let action = try await manager.nextPresentedAction()

        try expectModifiedAction(action, defaultAction: context.action(for: nil), in: manager)
    }

    @Test
    func pageAction() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 2,

            "name": "Page Action Test",
            "description": "Page Action Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "page_action": [
                "default_title": "Test Page Action",
                "default_popup": "popup.html",
                "default_icon": [
                    "16": "toolbar-16.png",
                    "32": "toolbar-32.png",
                ],
            ],
        ]

        let backgroundScript = """
            await browser.pageAction.setTitle({ title: 'Modified Title' })
            await browser.pageAction.setIcon({ path: 'toolbar-48.png' })
            await browser.pageAction.setPopup({ popup: 'alt-popup.html' })
            await browser.pageAction.setBadgeText({ text: '42' })
            await browser.pageAction.disable()

            browser.pageAction.openPopup()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "alt-popup.html": "<b>Hello World!</b>",
            "toolbar-48.png": makePNGData(size: CGSize(width: 48, height: 48), color: .yellow),
        ]

        let manager = try loadWebExtension(manifest: manifest, resources: resources)
        let context = try #require(manager.context)

        let action = try await manager.nextPresentedAction()

        try expectModifiedAction(action, defaultAction: context.action(for: nil), in: manager)
    }

    /// Checks the state shared by the `browserAction` and `pageAction` manifest v2 tests, whose
    /// background scripts make the same set of changes through their respective namespaces.
    private func expectModifiedAction(
        _ action: WKWebExtension.Action,
        defaultAction: WKWebExtension.Action?,
        in manager: TestWebExtensionManager,
        sourceLocation: SourceLocation = #_sourceLocation
    ) throws {
        let defaultAction = try #require(defaultAction, sourceLocation: sourceLocation)
        #expect(defaultAction.presentsPopup, sourceLocation: sourceLocation)
        #expect(!defaultAction.isEnabled, sourceLocation: sourceLocation)
        #expect(defaultAction.label == "Modified Title", sourceLocation: sourceLocation)
        #expect(defaultAction.badgeText == "42", sourceLocation: sourceLocation)
        #expect(!defaultAction.hasUnreadBadgeText, sourceLocation: sourceLocation)

        #expect(action.associatedTab === manager.defaultTab, sourceLocation: sourceLocation)

        #expect(!action.isEnabled, sourceLocation: sourceLocation)
        #expect(action.label == "Modified Title", sourceLocation: sourceLocation)
        #expect(action.badgeText == "42", sourceLocation: sourceLocation)
        #expect(!action.hasUnreadBadgeText, sourceLocation: sourceLocation)

        let icon = try #require(action.icon(for: CGSize(width: 48, height: 48)), sourceLocation: sourceLocation)
        #expect(icon.size == CGSize(width: 48, height: 48), sourceLocation: sourceLocation)

        #expect(action.presentsPopup, sourceLocation: sourceLocation)
        #expect(!action.isEnabled, sourceLocation: sourceLocation)

        #if WTF_PLATFORM_MAC
        #expect(action.popupPopover != nil, sourceLocation: sourceLocation)
        #else
        #expect(action.popupViewController != nil, sourceLocation: sourceLocation)
        #endif

        let popupWebView = try #require(action.popupWebView, sourceLocation: sourceLocation)
        #expect(!popupWebView.isLoading, sourceLocation: sourceLocation)

        let popupURL = try #require(popupWebView.url, sourceLocation: sourceLocation)
        #expect(popupURL.scheme == "webkit-extension", sourceLocation: sourceLocation)
        #expect(popupURL.path == "/alt-popup.html", sourceLocation: sourceLocation)

        action.closePopup()
    }

    // FIXME when webkit.org/b/314652 is resolved.
    #if WTF_PLATFORM_MAC && !ASSERT_ENABLED
    @Test(.disabled("webkit.org/b/314652"))
    #else
    @Test
    #endif
    func clearTabSpecificActionPropertiesOnNavigation() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            const [currentTab] = await browser.tabs.query({ active: true, currentWindow: true })

            browser.action.setTitle({ title: 'Tab Title', tabId: currentTab.id })
            browser.action.setIcon({ path: 'toolbar-48.png', tabId: currentTab.id })
            browser.action.setPopup({ popup: 'alt-popup.html', tabId: currentTab.id })
            browser.action.setBadgeText({ text: '42', tabId: currentTab.id })
            browser.action.disable(currentTab.id)

            browser.test.assertEq(await browser.action.getTitle({ tabId: currentTab.id }), 'Tab Title', 'Title should be before navigation')
            browser.test.assertEq(await browser.action.getPopup({ tabId: currentTab.id }), 'alt-popup.html', 'Popup should be before navigation')
            browser.test.assertEq(await browser.action.getBadgeText({ tabId: currentTab.id }), '42', 'Badge text should be before navigation')
            browser.test.assertFalse(await browser.action.isEnabled({ tabId: currentTab.id }), 'Action should be disabled before navigation')

            browser.webNavigation.onCompleted.addListener(async (details) => {
              browser.test.assertEq(details.tabId, currentTab.id, 'Only the tab we expect should be changing')
              browser.test.assertEq(details.frameId, 0, 'Only main frame should be changing')

              browser.test.assertEq(await browser.action.getTitle({ tabId: currentTab.id }), 'Test Action', 'Title should be after navigation')
              browser.test.assertEq(await browser.action.getPopup({ tabId: currentTab.id }), 'popup.html', 'Popup should be after navigation')
              browser.test.assertEq(await browser.action.getBadgeText({ tabId: currentTab.id }), '', 'Badge text should be after navigation')
              browser.test.assertTrue(await browser.action.isEnabled({ tabId: currentTab.id }), 'Action should be enabled after navigation')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "toolbar-16.png": makePNGData(size: CGSize(width: 16, height: 16), color: .red),
            "toolbar-32.png": makePNGData(size: CGSize(width: 32, height: 32), color: .blue),
            "toolbar-48.png": makePNGData(size: CGSize(width: 48, height: 48), color: .yellow),
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)

            // The two navigations have to be cross-origin, so one goes through `localhost` and the
            // other through the address of the same server.
            let localhostURL = configuration.localhostAddress
            let addressURL = configuration.address

            context.setPermissionStatus(.grantedExplicitly, for: localhostURL)
            context.setPermissionStatus(.grantedExplicitly, for: addressURL)

            let webView = try #require(manager.defaultTab?.webView)

            webView.load(URLRequest(url: localhostURL))

            try await manager.waitForTestMessage("Load Tab")

            webView.load(URLRequest(url: addressURL))

            try await manager.run()
        }
    }

    @Test
    func hasUnreadBadgeText() async throws {
        let backgroundScript = """
            await browser.action.setBadgeText({ text: 'New' })

            browser.test.sendMessage('Check Unread Badge Text')
            """

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        try await manager.waitForTestMessage("Check Unread Badge Text")

        let defaultAction = try #require(context.action(for: nil))
        let tabAction = try #require(context.action(for: manager.defaultTab))
        #expect(defaultAction.hasUnreadBadgeText)
        #expect(tabAction.hasUnreadBadgeText)

        tabAction.hasUnreadBadgeText = false
        #expect(!defaultAction.hasUnreadBadgeText)
        #expect(!tabAction.hasUnreadBadgeText)

        tabAction.hasUnreadBadgeText = true
        #expect(!defaultAction.hasUnreadBadgeText)
        #expect(tabAction.hasUnreadBadgeText)

        tabAction.hasUnreadBadgeText = false
        #expect(!defaultAction.hasUnreadBadgeText)
        #expect(!tabAction.hasUnreadBadgeText)

        defaultAction.hasUnreadBadgeText = true
        #expect(defaultAction.hasUnreadBadgeText)
        #expect(!tabAction.hasUnreadBadgeText)

        defaultAction.hasUnreadBadgeText = false
        #expect(!defaultAction.hasUnreadBadgeText)
        #expect(!tabAction.hasUnreadBadgeText)
    }

    @Test
    func navigationOpensInNewTab() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        try await server.run { configuration in
            let localhostURL = configuration.localhostAddress

            let resources: [String: Any] = [
                "background.js": "browser.test.sendMessage('Open Popup')",
                "popup.html": "<script type='module' src='popup.js'></script>",
                "popup.js": "document.location.href = '\(localhostURL.absoluteString)'",
            ]

            let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)
            let context = try #require(manager.context)

            manager.internalDelegate.presentPopupForAction = { _ in
                // Do nothing so the popup web view will stay loaded.
            }

            try await manager.waitForTestMessage("Open Popup")

            let tabConfiguration = try await manager.nextRequestedTab {
                context.performAction(for: manager.defaultTab)
            }

            #expect(tabConfiguration.url == localhostURL)
            #expect(tabConfiguration.window === manager.defaultWindow)
            #expect(tabConfiguration.index == 1)
            #expect(tabConfiguration.shouldBeActive)
        }
    }

    @Test
    func windowOpenOpensInNewWindow() async throws {
        let popupScript = """
            browser.test.runWithUserGesture(() => {
              window.open('https://example.com/', '_blank', 'popup, width=100, height=50')
            })
            """

        let resources: [String: Any] = [
            "background.js": "browser.test.sendMessage('Open Popup')",
            "popup.html": "<script type='module' src='popup.js'></script>",
            "popup.js": popupScript,
        ]

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)
        let context = try #require(manager.context)

        manager.internalDelegate.presentPopupForAction = { _ in
            // Do nothing so the popup web view will stay loaded.
        }

        try await manager.waitForTestMessage("Open Popup")

        #if WTF_PLATFORM_MAC
        let windowConfiguration = try await manager.nextRequestedWindow {
            context.performAction(for: manager.defaultTab)
        }

        #expect(windowConfiguration.tabURLs == [URL(string: "https://example.com/")!])

        #expect(windowConfiguration.windowType == .popup)
        #expect(windowConfiguration.windowState == .normal)

        #expect(windowConfiguration.frame.size.width == 100)
        #expect(windowConfiguration.frame.size.height == 50)
        #expect(windowConfiguration.frame.origin.x.isNaN)
        #expect(windowConfiguration.frame.origin.y.isNaN)
        #else
        let tabConfiguration = try await manager.nextRequestedTab {
            context.performAction(for: manager.defaultTab)
        }

        #expect(tabConfiguration.url == URL(string: "https://example.com/"))
        #expect(tabConfiguration.window === manager.defaultWindow)
        #expect(tabConfiguration.index == 1)
        #expect(tabConfiguration.shouldBeActive)
        #endif
    }

    @Test
    func emptyAction() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test Action",
            "description": "Test Action",
            "version": "1.0",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "action": [String: Any](),
        ]

        let backgroundScript = """
            if (browser.action) {
              browser.test.notifyPass()
            } else {
              browser.test.notifyFail('browser.action should be defined when it is empty.')
            }
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func clickedEventAndPermissionsRequest() async throws {
        let backgroundScript = """
            browser.action.setPopup({ popup: '' })

            browser.action.onClicked.addListener(async (tab) => {
              try {
                const result = await browser.permissions.request({ 'permissions': [ 'webNavigation' ] })
                if (result)
                  browser.test.notifyPass()
                else
                  browser.test.notifyFail('Permissions request was rejected')
              } catch (error) {
                browser.test.notifyFail('Permissions request failed')
              }
            })

            browser.test.sendMessage('Test Action')
            """

        let manager = try loadWebExtension(manifest: actionPopupManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        manager.grantRequestedPermissions()

        try await manager.waitForTestMessage("Test Action")

        context.performAction(for: manager.defaultTab)

        try await manager.run()
    }

    @Test
    func subframeNavigation() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<script>browser.test.notifyPass()</script>"
            }
        }

        try await server.run { configuration in
            let resources: [String: Any] = [
                "background.js": "browser.test.sendMessage('Test Popup Action')",
                "popup.html": "<iframe src='\(configuration.localhostAddress.absoluteString)'></iframe>",
            ]

            let manager = try loadWebExtension(manifest: actionPopupManifest, resources: resources)
            let context = try #require(manager.context)

            try await manager.waitForTestMessage("Test Popup Action")

            let action = try await manager.nextPresentedAction {
                context.performAction(for: manager.defaultTab)
            }

            #expect(action.presentsPopup)
            #expect(action.popupWebView != nil)

            try await manager.run()
        }
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
