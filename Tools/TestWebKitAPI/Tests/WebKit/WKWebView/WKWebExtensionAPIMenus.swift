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
private import TestWebKitAPILibrary.Helpers.cocoa.WebExtensionUtilities
import Testing
import WebKit
private import WebKit_Private.WKWebExtensionContextPrivate

import struct Foundation.URL
import struct Swift.String

#if USE_APPKIT
private import AppKit
#else
private import UIKit
#endif

#if WTF_PLATFORM_MAC
private import TestWebKitAPILibrary.Helpers.cocoa.TestUIDelegate
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
private import TestWebKitAPILibrary.Helpers.cocoa.TestWebExtensionsDelegate
private import WebKit_Private._WKContextMenuElementInfo
#endif

@MainActor
struct WKWebExtensionAPIMenusTests {
    #if USE_APPKIT
    /// The platform type of the items in an extension's menus.
    private typealias CocoaMenuItem = NSMenuItem

    /// The platform type of a menu item that performs an action and shows a state, rather than opening a submenu.
    private typealias CocoaMenuAction = NSMenuItem
    #else
    /// The platform type of the items in an extension's menus.
    private typealias CocoaMenuItem = UIMenuElement

    /// The platform type of a menu item that performs an action and shows a state, rather than opening a submenu.
    private typealias CocoaMenuAction = UIAction
    #endif

    /// Thrown by ``perform(_:)`` for a menu item that has nothing to perform.
    ///
    /// ``perform(_:)`` throws this instead of recording an issue, because it is also called from WebKit callbacks,
    /// where a recorded issue wouldn't be attributed to the test. Swift Testing records it against the test once it
    /// propagates out of the test.
    private struct MenuItemNotPerformable: Error, CustomStringConvertible {
        let title: String

        var description: String {
            #if USE_APPKIT
            "Menu item \"\(title)\" has no target or action, or its target doesn't respond to the action"
            #else
            "Menu item \"\(title)\" isn't a UIAction"
            #endif
        }
    }

    private let menusManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "Menus Test",
        "description": "Menus Test",
        "version": "1",

        "permissions": ["menus", "contextMenus", "activeTab"],

        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],

        "commands": [String: Any](),

        "action": [
            "default_title": "Test Action",
            "default_icon": [
                "16": "toolbar-16.png",
                "32": "toolbar-32.png",
            ],
        ],
    ]

    /// Chooses a menu item, as the user would.
    ///
    /// - Throws: ``MenuItemNotPerformable`` if the menu item has nothing to perform.
    private func perform(_ menuItem: CocoaMenuItem) throws {
        #if USE_APPKIT
        // Check the target, since sending an action to a nil target walks the responder chain instead.
        guard let target = menuItem.target, let action = menuItem.action else {
            throw MenuItemNotPerformable(title: menuItem.title)
        }

        guard NSApp.sendAction(action, to: target, from: nil) else {
            throw MenuItemNotPerformable(title: menuItem.title)
        }
        #else
        guard let action = menuItem as? UIAction else {
            throw MenuItemNotPerformable(title: menuItem.title)
        }

        action.performWithSender(nil, target: nil)
        #endif
    }

    /// A menu item as a ``CocoaMenuAction``, or `nil` if it isn't one.
    ///
    /// Every AppKit menu item is one.
    private func menuAction(_ menuItem: CocoaMenuItem?) -> CocoaMenuAction? {
        #if USE_APPKIT
        menuItem
        #else
        menuItem as? UIAction
        #endif
    }

    /// The items in a menu item's submenu, or none if it has no submenu.
    private func itemsInSubmenu(of menuItem: CocoaMenuItem?) -> [CocoaMenuItem] {
        #if USE_APPKIT
        menuItem?.submenu?.items ?? []
        #else
        (menuItem as? UIMenu)?.children ?? []
        #endif
    }

    #if WTF_PLATFORM_MAC
    /// Gives the extension's default tab a new 400×400 web view that can show context menus, and loads a URL in it.
    private func makeDefaultTabWebView(for manager: TestWebExtensionManager, loading url: URL) async throws -> TestWKWebView {
        let viewConfiguration = WKWebViewConfiguration()
        viewConfiguration.webExtensionController = manager.controller

        let webView = TestWKWebView(frame: NSRect(x: 0, y: 0, width: 400, height: 400), configuration: viewConfiguration)
        manager.defaultTab?.webView = webView

        try await webView.loadAndWait(URLRequest(url: url))
        await webView.nextPresentationUpdate()

        return webView
    }

    /// Right-clicks a point in a web view, by default the middle of a 400×400 one, and runs `handler` on the context
    /// menu WebKit proposes before letting the menu be dismissed.
    ///
    /// `handler` runs from a WebKit callback, where a failed check wouldn't be attributed to the test, so it should only
    /// record what it finds and perform menu items. Check what it recorded after this returns. Anything `handler` throws
    /// is rethrown from here. Any further context menu proposed before this returns is dismissed without running
    /// `handler`.
    private func withNextContextMenu(
        in webView: TestWKWebView,
        at point: NSPoint = NSPoint(x: 200, y: 200),
        _ handler: @escaping (NSMenu) throws -> Void
    ) async throws {
        // A web view only holds its UI delegate weakly, so keep it alive until the menu has been handled.
        let uiDelegate = TestUIDelegate()
        defer { extendLifetime(uiDelegate) }
        webView.uiDelegate = uiDelegate

        var handledMenu = false

        let result = await withCheckedContinuation { (continuation: CheckedContinuation<Result<Void, any Error>, Never>) in
            uiDelegate.getContextMenuFromProposedMenu = { menu, _, _, completionHandler in
                // Keep this block, since without one TestUIDelegate shows the menu modally.
                guard !handledMenu else {
                    completionHandler?(nil)
                    return
                }

                handledMenu = true

                // WebKit always proposes a menu. If it didn't, an empty one makes the test's checks fail.
                let result = Result { try handler(menu ?? NSMenu()) }
                completionHandler?(nil)
                continuation.resume(returning: result)
            }

            webView.hostWindow()?.makeFirstResponder(webView)
            webView.rightClick(at: point)
        }

        try result.get()
    }
    #endif // WTF_PLATFORM_MAC

    private func loadAndRun(backgroundScript: String) async throws {
        let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func contextType() async throws {
        try await loadAndRun(
            backgroundScript: """
                browser.test.assertEq(typeof browser.menus.ContextType, 'object', 'ContextType should be an object')

                browser.test.assertEq(browser.menus.ContextType.ALL, 'all', 'ALL should be defined')
                browser.test.assertEq(browser.menus.ContextType.PAGE, 'page', 'PAGE should be defined')
                browser.test.assertEq(browser.menus.ContextType.FRAME, 'frame', 'FRAME should be defined')
                browser.test.assertEq(browser.menus.ContextType.SELECTION, 'selection', 'SELECTION should be defined')
                browser.test.assertEq(browser.menus.ContextType.LINK, 'link', 'LINK should be defined')
                browser.test.assertEq(browser.menus.ContextType.EDITABLE, 'editable', 'EDITABLE should be defined')
                browser.test.assertEq(browser.menus.ContextType.IMAGE, 'image', 'IMAGE should be defined')
                browser.test.assertEq(browser.menus.ContextType.VIDEO, 'video', 'VIDEO should be defined')
                browser.test.assertEq(browser.menus.ContextType.AUDIO, 'audio', 'AUDIO should be defined')
                browser.test.assertEq(browser.menus.ContextType.ACTION, 'action', 'ACTION should be defined')
                browser.test.assertEq(browser.menus.ContextType.BROWSER_ACTION, 'browser_action', 'BROWSER_ACTION should be defined')
                browser.test.assertEq(browser.menus.ContextType.PAGE_ACTION, 'page_action', 'PAGE_ACTION should be defined')
                browser.test.assertEq(browser.menus.ContextType.TAB, 'tab', 'TAB should be defined')

                browser.test.assertDeepEq(Object.keys(browser.menus.ContextType).sort(), [ 'ACTION', 'ALL', 'AUDIO', 'BROWSER_ACTION', 'EDITABLE', 'FRAME', 'IMAGE', 'LINK', 'PAGE', 'PAGE_ACTION', 'SELECTION', 'TAB', 'VIDEO' ], 'ContextType should only define the supported values')

                browser.test.assertEq(typeof browser.contextMenus.ContextType, 'object', 'ContextType should be an object')

                browser.test.assertEq(browser.contextMenus.ContextType.ALL, 'all', 'ALL should be defined')
                browser.test.assertEq(browser.contextMenus.ContextType.PAGE, 'page', 'PAGE should be defined')
                browser.test.assertEq(browser.contextMenus.ContextType.FRAME, 'frame', 'FRAME should be defined')
                browser.test.assertEq(browser.contextMenus.ContextType.SELECTION, 'selection', 'SELECTION should be defined')
                browser.test.assertEq(browser.contextMenus.ContextType.LINK, 'link', 'LINK should be defined')
                browser.test.assertEq(browser.contextMenus.ContextType.EDITABLE, 'editable', 'EDITABLE should be defined')
                browser.test.assertEq(browser.contextMenus.ContextType.IMAGE, 'image', 'IMAGE should be defined')
                browser.test.assertEq(browser.contextMenus.ContextType.VIDEO, 'video', 'VIDEO should be defined')
                browser.test.assertEq(browser.contextMenus.ContextType.AUDIO, 'audio', 'AUDIO should be defined')
                browser.test.assertEq(browser.contextMenus.ContextType.ACTION, 'action', 'ACTION should be defined')
                browser.test.assertEq(browser.contextMenus.ContextType.BROWSER_ACTION, 'browser_action', 'BROWSER_ACTION should be defined')
                browser.test.assertEq(browser.contextMenus.ContextType.PAGE_ACTION, 'page_action', 'PAGE_ACTION should be defined')
                browser.test.assertEq(browser.contextMenus.ContextType.TAB, 'tab', 'TAB should be defined')

                browser.test.assertDeepEq(Object.keys(browser.contextMenus.ContextType).sort(), [ 'ACTION', 'ALL', 'AUDIO', 'BROWSER_ACTION', 'EDITABLE', 'FRAME', 'IMAGE', 'LINK', 'PAGE', 'PAGE_ACTION', 'SELECTION', 'TAB', 'VIDEO' ], 'ContextType should only define the supported values')

                browser.test.notifyPass()
                """
        )
    }

    @Test
    func itemType() async throws {
        try await loadAndRun(
            backgroundScript: """
                browser.test.assertEq(typeof browser.menus.ItemType, 'object', 'ItemType should be an object')

                browser.test.assertEq(browser.menus.ItemType.NORMAL, 'normal', 'NORMAL should be defined')
                browser.test.assertEq(browser.menus.ItemType.CHECKBOX, 'checkbox', 'CHECKBOX should be defined')
                browser.test.assertEq(browser.menus.ItemType.RADIO, 'radio', 'RADIO should be defined')
                browser.test.assertEq(browser.menus.ItemType.SEPARATOR, 'separator', 'SEPARATOR should be defined')

                browser.test.assertDeepEq(Object.keys(browser.menus.ItemType).sort(), [ 'CHECKBOX', 'NORMAL', 'RADIO', 'SEPARATOR' ], 'ItemType should only define the supported values')

                browser.test.assertEq(typeof browser.contextMenus.ItemType, 'object', 'ItemType should be an object')

                browser.test.assertEq(browser.contextMenus.ItemType.NORMAL, 'normal', 'NORMAL should be defined')
                browser.test.assertEq(browser.contextMenus.ItemType.CHECKBOX, 'checkbox', 'CHECKBOX should be defined')
                browser.test.assertEq(browser.contextMenus.ItemType.RADIO, 'radio', 'RADIO should be defined')
                browser.test.assertEq(browser.contextMenus.ItemType.SEPARATOR, 'separator', 'SEPARATOR should be defined')

                browser.test.assertDeepEq(Object.keys(browser.contextMenus.ItemType).sort(), [ 'CHECKBOX', 'NORMAL', 'RADIO', 'SEPARATOR' ], 'ItemType should only define the supported values')

                browser.test.notifyPass()
                """
        )
    }

    @Test
    func errors() async throws {
        try await loadAndRun(
            backgroundScript: """
                browser.test.assertThrows(() => browser.menus.create({ id: { }, title: 'Test' }), /'id' is expected to be a string or a number, but an object was provided/i)
                browser.test.assertThrows(() => browser.menus.create({ id: '', title: 'Test' }), /'id' value is invalid, because it must not be empty/i)
                browser.test.assertThrows(() => browser.menus.create({ id: NaN, title: 'Test' }), /'id' is expected to be a string or a number, but NaN was provided/i)
                browser.test.assertThrows(() => browser.menus.create({ id: Infinity, title: 'Test' }), /'id' is expected to be a string or a number, but Infinity was provided/i)

                browser.test.assertThrows(() => browser.menus.create({ title: 123 }), /'title' is expected to be a string, but a number was provided/i)
                browser.test.assertThrows(() => browser.menus.create({ title: '' }), /'title' value is invalid, because it must not be empty/i)

                browser.test.assertThrows(() => browser.menus.create({ contexts: 'all', title: 'Test' }), /'contexts' is expected to be an array of strings, but a string was provided/i)
                browser.test.assertThrows(() => browser.menus.create({ contexts: [ 42 ], title: 'Test' }), /'contexts' is expected to be an array of strings, but a number was provided in the array/i)
                browser.test.assertThrows(() => browser.menus.create({ contexts: [ NaN ], title: 'Test' }), /'contexts' is expected to be an array of strings, but NaN was provided in the array/i)
                browser.test.assertThrows(() => browser.menus.create({ contexts: [ Infinity ], title: 'Test' }), /'contexts' is expected to be an array of strings, but Infinity was provided in the array/i)

                browser.test.assertThrows(() => browser.menus.create({ parentId: { }, title: 'Test' }), /'parentId' is expected to be a string or a number, but an object was provided/i)
                browser.test.assertThrows(() => browser.menus.create({ parentId: '', title: 'Test' }), /'parentId' value is invalid, because it must not be empty/i)
                browser.test.assertThrows(() => browser.menus.create({ parentId: NaN, title: 'Test' }), /'parentId' is expected to be a string or a number, but NaN was provided/i)
                browser.test.assertThrows(() => browser.menus.create({ parentId: Infinity, title: 'Test' }), /'parentId' is expected to be a string or a number, but Infinity was provided/i)

                browser.test.assertThrows(() => browser.menus.create({ documentUrlPatterns: 'https://*.example.com', title: 'Test' }), /'documentUrlPatterns' is expected to be an array of strings, but a string was provided/i)
                browser.test.assertThrows(() => browser.menus.create({ documentUrlPatterns: [ 'bad' ], title: 'Test' }), /'documentUrlPatterns' value is invalid, because 'bad' is not a valid pattern/i)
                browser.test.assertThrows(() => browser.menus.create({ documentUrlPatterns: [ 'foo://*/*' ], title: 'Test' }), /'documentUrlPatterns' value is invalid, because '.*' is not a valid pattern/i)

                browser.test.assertThrows(() => browser.menus.create({ targetUrlPatterns: 'https://*.example.com', title: 'Test' }), /'targetUrlPatterns' is expected to be an array of strings, but a string was provided/i)
                browser.test.assertThrows(() => browser.menus.create({ targetUrlPatterns: [ 'bad' ], title: 'Test' }), /'targetUrlPatterns' value is invalid, because 'bad' is not a valid pattern/i)

                browser.test.assertThrows(() => browser.menus.create({ type: 123, title: 'Test' }), /'type' is expected to be a string, but a number was provided/i)
                browser.test.assertThrows(() => browser.menus.create({ type: 'bad', title: 'Test' }), /'type' value is invalid, because it must specify either 'normal', 'checkbox', 'radio', or 'separator'/i)

                browser.test.assertThrows(() => browser.menus.create({ command: 123, title: 'Test' }), /'command' is expected to be a string, but a number was provided/i)
                browser.test.assertThrows(() => browser.menus.create({ command: '', title: 'Test' }), /'command' value is invalid, because it must not be empty/i)

                browser.test.assertThrows(() => browser.menus.create({ checked: 'bad', title: 'Test' }), /'checked' is expected to be a boolean, but a string was provided/i)
                browser.test.assertThrows(() => browser.menus.create({ visible: 'bad', title: 'Test' }), /'visible' is expected to be a boolean, but a string was provided/i)
                browser.test.assertThrows(() => browser.menus.create({ enabled: 'bad', title: 'Test' }), /'enabled' is expected to be a boolean, but a string was provided/i)

                browser.test.assertThrows(() => browser.menus.create({ icons: 123, title: 'Test' }), /'icons' is expected to be a string or an object or null, but a number was provided/i)
                browser.test.assertThrows(() => browser.menus.create({ icons: { 16: 123 }, title: 'Test' }), /'icons\\[16]' value is invalid, because a string is expected, but a number was provided/i)
                browser.test.assertThrows(() => browser.menus.create({ icons: { '16': 123 }, title: 'Test' }), /'icons\\[16]' value is invalid, because a string is expected, but a number was provided/i)
                browser.test.assertThrows(() => browser.menus.create({ icons: { '1.2': 'test.png' }, title: 'Test' }), /'icons' value is invalid, because '1.2' is not a valid dimension/i)

                browser.test.assertThrows(() => browser.menus.create({ onclick: 'bad', title: 'Test' }), /'onclick' is expected to be a value, but a string was provided/i)
                browser.test.assertThrows(() => browser.menus.create({ onclick: { }, title: 'Test' }), /'onclick' is expected to be a value, but an object was provided/i)
                browser.test.assertThrows(() => browser.menus.create({ onclick: new RegExp(), title: 'Test' }), /'onclick' value is invalid, because it must be a function/i)

                browser.test.notifyPass()
                """
        )
    }

    @Test
    func menuCreateWithVariousIds() async throws {
        try await loadAndRun(
            backgroundScript: """
                let menuItemWithStringId = browser.menus.create({
                  id: 'string-id',
                  title: 'String ID',
                  contexts: [ 'all' ]
                })

                browser.test.assertEq(typeof menuItemWithStringId, 'string')
                browser.test.assertEq(menuItemWithStringId, 'string-id')

                let menuItemWithNumericId = browser.menus.create({
                  id: 12345,
                  title: 'Numeric ID',
                  contexts: [ 'all' ]
                })

                browser.test.assertEq(typeof menuItemWithNumericId, 'number')
                browser.test.assertEq(menuItemWithNumericId, 12345)

                let menuItemWithOmittedId = browser.menus.create({
                  title: 'Omitted ID',
                  contexts: [ 'all' ]
                })

                function validateUUIDv4(uuid) {
                  let regex = /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[8|9|a|b][0-9a-f]{3}-[0-9a-f]{12}$/i
                  return regex.test(uuid)
                }

                browser.test.assertEq(typeof menuItemWithOmittedId, 'string')
                browser.test.assertTrue(validateUUIDv4(menuItemWithOmittedId), 'ID should be a UUID v4 string')

                browser.test.notifyPass()
                """
        )
    }

    @Test
    func createMenuWithDeprecatedKeys() async throws {
        let backgroundScript = """
            const createImageData = (size, color) => {
              const context = new OffscreenCanvas(size, size).getContext('2d')
              context.fillStyle = color
              context.fillRect(0, 0, size, size)

              return context.getImageData(0, 0, size, size)
            }

            const lightImageData = createImageData(16, 'black')
            const darkImageData = createImageData(16, 'white')

            browser.test.assertSafe(() => browser.menus.create({
              id: 'menu-item-with-deprecated-color-schemes-key',
              title: 'Menu Item Duplicate Color Schemes',
              iconVariants: [
                { 16: darkImageData, 'colorSchemes': [ 'dark' ], 'color_schemes': [ 'light' ] },
                { 16: lightImageData, 'colorSchemes': [ 'light' ], 'color_schemes': [ 'dark' ] }
              ],
              contexts: [ 'action' ]
            }))

            browser.test.assertSafe(() => browser.menus.create({
              id: 'menu-item-with-deprecated-icon-variants-key',
              title: 'Menu Item Duplicate Icon Variants',
              iconVariants: [
                { 16: darkImageData, 'colorSchemes': [ 'dark' ] },
                { 16: lightImageData, 'colorSchemes': [ 'light' ] }
              ],
              icon_variants: [
                { 16: darkImageData, 'colorSchemes': [ 'light' ] },
                { 16: lightImageData, 'colorSchemes': [ 'dark' ] }
              ],
              contexts: [ 'action' ]
            }))

            browser.test.sendMessage('Menus Created')
            """

        let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
        let context = try requireNonNil(manager.context)

        try await manager.waitForTestMessage("Menus Created")

        let action = try requireNonNil(context.action(for: manager.defaultTab))
        let menuItems = action.menuItems

        expectEqual(menuItems.count, 2)

        let firstMenuItem = try requireNonNil(menuAction(menuItems.first))
        expectTrue(firstMenuItem.isKind(of: CocoaMenuItem.self))
        expectEqual(firstMenuItem.title, "Menu Item Duplicate Color Schemes")

        let secondMenuItem = try requireNonNil(menuAction(menuItems.last))
        expectTrue(secondMenuItem.isKind(of: CocoaMenuItem.self))
        expectEqual(secondMenuItem.title, "Menu Item Duplicate Icon Variants")

        let firstImage = try requireNonNil(firstMenuItem.image)
        let secondImage = try requireNonNil(secondMenuItem.image)

        withAppearance(.dark) {
            expectTrue(compareColors(pixelColor(of: firstImage), .white))
            expectTrue(compareColors(pixelColor(of: secondImage), .white))
        }

        withAppearance(.light) {
            expectTrue(compareColors(pixelColor(of: firstImage), .black))
            expectTrue(compareColors(pixelColor(of: secondImage), .black))
        }
    }

    @Test
    func actionMenus() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.menus.create({
              id: 'top-level-1',
              title: 'Top Level 1',
              contexts: [ 'action' ]
            })

            browser.menus.create({
              id: 'top-level-2',
              title: 'Top Level 2',
              contexts: [ 'page' ]
            })

            browser.menus.create({
              id: 'top-level-3',
              title: 'Top Level 3',
              contexts: [ 'action', 'page' ],
              documentUrlPatterns: ['*://localhost/*']
            })

            browser.menus.create({
              id: 'top-level-4',
              title: 'Top Level 4'
            })

            browser.menus.onClicked.addListener((info, tab) => {
              browser.test.assertEq(typeof info, 'object')
              browser.test.assertEq(typeof tab, 'object')

              browser.test.assertEq(info.menuItemId, 'top-level-1')
              browser.test.assertEq(info.parentMenuItemId, undefined)

              browser.test.assertEq(typeof tab.id, 'number')
              browser.test.assertEq(typeof tab.windowId, 'number')
              browser.test.assertEq(tab.url, '')
              browser.test.assertEq(tab.title, '')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Menus Created')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)

            // Reset activeTab, actionMenusWithActiveTab() tests that.
            context.setPermissionStatus(.unknown, for: .activeTab)

            try await manager.waitForTestMessage("Menus Created")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await Task.sleep(for: .seconds(1))
            try manager.checkCollectedFailures()

            let action = try requireNonNil(context.action(for: manager.defaultTab))
            let menuItems = action.menuItems

            let expectedTitles = ["Top Level 1", "Top Level 3"]
            expectEqual(menuItems.count, expectedTitles.count)

            for (menuItem, expectedTitle) in zip(menuItems, expectedTitles) {
                expectTrue(menuItem.isKind(of: CocoaMenuItem.self))
                expectEqual(menuItem.title, expectedTitle)
            }

            try perform(requireNonNil(menuItems.first))

            try await manager.run()
        }
    }

    @Test
    func actionMenusWithActiveTab() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<title>Test</title>"
            }
        }

        let backgroundScript = """
            browser.menus.create({
              id: 'top-level-1',
              title: 'Top Level 1',
              contexts: [ 'action' ]
            })

            browser.menus.create({
              id: 'top-level-2',
              title: 'Top Level 2',
              contexts: [ 'page' ]
            })

            browser.menus.create({
              id: 'top-level-3',
              title: 'Top Level 3',
              contexts: [ 'action', 'page' ]
            })

            browser.menus.create({
              id: 'top-level-4',
              title: 'Top Level 4'
            })

            browser.menus.onClicked.addListener((info, tab) => {
              browser.test.assertEq(typeof info, 'object')
              browser.test.assertEq(typeof tab, 'object')

              browser.test.assertEq(info.menuItemId, 'top-level-1')
              browser.test.assertEq(info.parentMenuItemId, undefined)

              browser.test.assertEq(typeof tab.id, 'number')
              browser.test.assertEq(typeof tab.windowId, 'number')
              browser.test.assertEq(typeof tab.url, 'string')
              browser.test.assertTrue(tab.url.startsWith('http://localhost:'))
              browser.test.assertEq(typeof tab.title, 'string')
              browser.test.assertTrue(tab.title.length > 0)

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Menus Created')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)
            let defaultTab = try requireNonNil(manager.defaultTab)

            try await manager.waitForTestMessage("Menus Created")

            expectFalse(context.hasActiveUserGesture(in: defaultTab))

            let webView = try requireNonNil(defaultTab.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await Task.sleep(for: .seconds(1))
            try manager.checkCollectedFailures()

            let action = try requireNonNil(context.action(for: defaultTab))
            let menuItems = action.menuItems

            let expectedTitles = ["Top Level 1", "Top Level 3"]
            expectEqual(menuItems.count, expectedTitles.count)

            for (menuItem, expectedTitle) in zip(menuItems, expectedTitles) {
                expectTrue(menuItem.isKind(of: CocoaMenuItem.self))
                expectEqual(menuItem.title, expectedTitle)
            }

            try perform(requireNonNil(menuItems.first))

            try await manager.run()

            expectTrue(context.hasActiveUserGesture(in: defaultTab))
        }
    }

    @Test
    func actionSubmenus() async throws {
        let backgroundScript = """
            browser.menus.create({
              id: 'top-level-1',
              title: 'Top Level 1',
              contexts: [ 'action' ]
            })

            browser.menus.create({
              id: 'submenu-1',
              parentId: 'top-level-1',
              title: 'Submenu 1'
            })

            browser.menus.create({
              id: 'submenu-2',
              parentId: 'top-level-1',
              title: 'Submenu 2',
              contexts: [ 'action' ]
            })

            browser.menus.create({
              id: 'submenu-excluded',
              parentId: 'top-level-1',
              title: 'Excluded Submenu',
              contexts: [ 'page' ]
            })

            browser.menus.onClicked.addListener((info, tab) => {
              browser.test.assertEq(typeof info, 'object')
              browser.test.assertEq(typeof tab, 'object')

              browser.test.assertEq(info.menuItemId, 'submenu-1')
              browser.test.assertEq(info.parentMenuItemId, 'top-level-1')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Menus Created')
            """

        let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
        let context = try requireNonNil(manager.context)

        try await manager.waitForTestMessage("Menus Created")

        let action = try requireNonNil(context.action(for: manager.defaultTab))
        let menuItems = action.menuItems

        expectEqual(menuItems.count, 1)

        let topLevelMenuItem = try requireNonNil(menuItems.first)
        expectTrue(topLevelMenuItem.isKind(of: CocoaMenuItem.self))
        expectEqual(topLevelMenuItem.title, "Top Level 1")

        let expectedSubmenuTitles = ["Submenu 1", "Submenu 2"]

        let submenuItems = itemsInSubmenu(of: topLevelMenuItem)
        expectEqual(submenuItems.count, expectedSubmenuTitles.count)

        for (submenuItem, expectedTitle) in zip(submenuItems, expectedSubmenuTitles) {
            expectTrue(submenuItem.isKind(of: CocoaMenuItem.self))
            expectEqual(submenuItem.title, expectedTitle)
        }

        try perform(requireNonNil(submenuItems.first))

        try await manager.run()
    }

    @Test
    func actionSubmenusUpdate() async throws {
        let backgroundScript = """
            browser.menus.create({
              id: 'top-level-1',
              title: 'Top Level 1',
              contexts: [ 'action' ]
            })

            browser.menus.create({
              id: 'top-level-2',
              title: 'Top Level 2',
              contexts: [ 'tab' ]
            })

            browser.menus.create({
              id: 'submenu-1',
              parentId: 'top-level-2',
              title: 'Submenu 1'
            })

            browser.menus.create({
              id: 'submenu-2',
              parentId: 'top-level-2',
              title: 'Submenu 2',
              contexts: [ 'action' ]
            })

            browser.menus.create({
              id: 'submenu-excluded',
              parentId: 'top-level-2',
              title: 'Excluded Submenu'
            })

            browser.menus.update('submenu-1', {
              parentId: 'top-level-1',
              contexts: [ ]
            })

            browser.menus.update('submenu-2', {
              parentId: 'top-level-1',
              contexts: [ 'action' ]
            })

            browser.menus.update('submenu-excluded', {
              parentId: 'top-level-1',
            })

            browser.menus.onClicked.addListener((info, tab) => {
              browser.test.assertEq(typeof info, 'object')
              browser.test.assertEq(typeof tab, 'object')

              browser.test.assertEq(info.menuItemId, 'submenu-1')
              browser.test.assertEq(info.parentMenuItemId, 'top-level-1')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Menus Created')
            """

        let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
        let context = try requireNonNil(manager.context)

        try await manager.waitForTestMessage("Menus Created")

        let action = try requireNonNil(context.action(for: manager.defaultTab))
        let menuItems = action.menuItems

        expectEqual(menuItems.count, 1)

        let topLevelMenuItem = try requireNonNil(menuItems.first)
        expectTrue(topLevelMenuItem.isKind(of: CocoaMenuItem.self))
        expectEqual(topLevelMenuItem.title, "Top Level 1")

        let expectedSubmenuTitles = ["Submenu 1", "Submenu 2"]

        let submenuItems = itemsInSubmenu(of: topLevelMenuItem)
        expectEqual(submenuItems.count, expectedSubmenuTitles.count)

        for (submenuItem, expectedTitle) in zip(submenuItems, expectedSubmenuTitles) {
            expectTrue(submenuItem.isKind(of: CocoaMenuItem.self))
            expectEqual(submenuItem.title, expectedTitle)
        }

        try perform(requireNonNil(submenuItems.first))

        try await manager.run()
    }

    @Test
    func tabMenus() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.menus.create({
              id: 'tab-item-1',
              title: 'Tab Item 1',
              contexts: [ 'tab' ]
            })

            browser.menus.create({
              id: 'tab-item-2',
              title: 'Tab Item 2',
              contexts: [ 'tab' ],
              documentUrlPatterns: ['*://localhost/*']
            })

            browser.menus.create({
              id: 'excluded-item',
              title: 'Excluded Item',
              contexts: [ 'image' ]
            })

            browser.menus.onClicked.addListener((info, tab) => {
              browser.test.assertEq(typeof info, 'object')
              browser.test.assertEq(typeof tab, 'object')

              browser.test.assertEq(info.menuItemId, 'tab-item-1')
              browser.test.assertEq(typeof tab.id, 'number')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Menus Created')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)
            let defaultTab = try requireNonNil(manager.defaultTab)

            try await manager.waitForTestMessage("Menus Created")

            let webView = try requireNonNil(defaultTab.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await Task.sleep(for: .seconds(1))
            try manager.checkCollectedFailures()

            let menuItems = context.menuItems(for: defaultTab)
            expectEqual(menuItems.count, 1)

            let submenuItems = itemsInSubmenu(of: menuItems.first)
            expectEqual(submenuItems.count, 2)

            let expectedTitles = ["Tab Item 1", "Tab Item 2"]
            expectEqual(submenuItems.count, expectedTitles.count)

            for (submenuItem, expectedTitle) in zip(submenuItems, expectedTitles) {
                expectTrue(submenuItem.isKind(of: CocoaMenuItem.self))
                expectEqual(submenuItem.title, expectedTitle)
            }

            try perform(requireNonNil(submenuItems.first))

            try await manager.run()
        }
    }

    @Test
    func menuItemProperties() async throws {
        let backgroundScript = """
            browser.menus.create({
              id: 'menu-item-properties',
              title: 'Menu &Item with Ampersand && More',
              type: 'checkbox',
              checked: true,
              contexts: [ 'all' ],
              enabled: false,
              visible: false,
              icons: { 16: 'icon-16.png', 20: 'icon-20.png' }
            })

            browser.menus.create({
              id: 'menu-item-command',
              title: 'Menu Item with Command',
              command: '_execute_action',
              contexts: [ 'all' ]
            })

            browser.action.onClicked.addListener((tab) => {
              browser.test.assertEq(typeof tab, 'object')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Menus Created')
            """

        let smallIcon = makePNGData(size: CGSize(width: 16, height: 16), color: .green)
        let largerIcon = makePNGData(size: CGSize(width: 20, height: 20), color: .blue)

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "icon-16.png": smallIcon,
            "icon-20.png": largerIcon,
        ]

        let manager = try loadWebExtension(manifest: menusManifest, resources: resources)
        let context = try requireNonNil(manager.context)
        let defaultTab = try requireNonNil(manager.defaultTab)

        try await manager.waitForTestMessage("Menus Created")

        let menuItems = context.menuItems(for: defaultTab)
        expectEqual(menuItems.count, 1)

        let submenuItems = itemsInSubmenu(of: menuItems.first)
        expectEqual(submenuItems.count, 2)

        let firstMenuItem = try requireNonNil(menuAction(submenuItems.first))
        expectTrue(firstMenuItem.isKind(of: CocoaMenuItem.self))
        expectEqual(firstMenuItem.state, .on)
        expectEqual(firstMenuItem.title, "Menu Item with Ampersand & More")
        expectNotNil(firstMenuItem.image)

        #if USE_APPKIT
        expectFalse(firstMenuItem.isEnabled)
        expectTrue(firstMenuItem.isHidden)
        expectEqual(firstMenuItem.image?.size, CGSize(width: 16, height: 16))
        #else
        expectEqual(firstMenuItem.attributes, [.disabled, .hidden])
        expectEqual(firstMenuItem.image?.size, CGSize(width: 20, height: 20))
        #endif

        let secondMenuItem = try requireNonNil(menuAction(submenuItems.last))
        expectTrue(secondMenuItem.isKind(of: CocoaMenuItem.self))
        expectEqual(secondMenuItem.title, "Menu Item with Command")

        try perform(secondMenuItem)

        try await manager.run()
    }

    @Test
    func menuItemPropertiesUpdate() async throws {
        let backgroundScript = """
            browser.menus.create({
              id: 'menu-item-properties',
              title: 'Old Title',
            })

            browser.menus.create({
              id: 'menu-item-command',
              title: 'Old Command Title',
            })

            browser.menus.update('menu-item-properties', {
              id: 'menu-item-properties-new',
              title: 'Menu &Item with Ampersand && More',
              type: 'checkbox',
              checked: true,
              contexts: [ 'all' ],
              enabled: false,
              visible: false,
              icons: { 16: 'icon-16.png', 20: 'icon-20.png' }
            })

            browser.menus.update('menu-item-command', {
              id: 'menu-item-command-new',
              title: 'Menu Item with Command',
              command: '_execute_action',
              contexts: [ 'all' ]
            })

            browser.action.onClicked.addListener((tab) => {
              browser.test.assertEq(typeof tab, 'object')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Menus Created')
            """

        let smallIcon = makePNGData(size: CGSize(width: 16, height: 16), color: .green)
        let largerIcon = makePNGData(size: CGSize(width: 20, height: 20), color: .blue)

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "icon-16.png": smallIcon,
            "icon-20.png": largerIcon,
        ]

        let manager = try loadWebExtension(manifest: menusManifest, resources: resources)
        let context = try requireNonNil(manager.context)
        let defaultTab = try requireNonNil(manager.defaultTab)

        try await manager.waitForTestMessage("Menus Created")

        let menuItems = context.menuItems(for: defaultTab)
        expectEqual(menuItems.count, 1)

        let submenuItems = itemsInSubmenu(of: menuItems.first)
        expectEqual(submenuItems.count, 2)

        let firstMenuItem = try requireNonNil(menuAction(submenuItems.first))
        expectTrue(firstMenuItem.isKind(of: CocoaMenuItem.self))
        expectEqual(firstMenuItem.state, .on)
        expectEqual(firstMenuItem.title, "Menu Item with Ampersand & More")
        expectNotNil(firstMenuItem.image)

        #if USE_APPKIT
        expectFalse(firstMenuItem.isEnabled)
        expectTrue(firstMenuItem.isHidden)
        expectEqual(firstMenuItem.image?.size, CGSize(width: 16, height: 16))
        #else
        expectEqual(firstMenuItem.attributes, [.disabled, .hidden])
        expectEqual(firstMenuItem.image?.size, CGSize(width: 20, height: 20))
        #endif

        let secondMenuItem = try requireNonNil(menuAction(submenuItems.last))
        expectTrue(secondMenuItem.isKind(of: CocoaMenuItem.self))
        expectEqual(secondMenuItem.title, "Menu Item with Command")

        try perform(secondMenuItem)

        try await manager.run()
    }

    #if ENABLE_WK_WEB_EXTENSIONS_ICON_VARIANTS
    @Test
    func menuItemWithIconVariants() async throws {
        let backgroundScript = """
            browser.test.assertSafe(() => browser.menus.create({
              id: 'menu-item-with-icon-variants',
              title: 'Menu Item with Icon Variants',
              iconVariants: [
                { 16: 'icon-dark-16.png', 'colorSchemes': [ 'dark' ] },
                { 16: 'icon-light-16.png', 'colorSchemes': [ 'light' ] }
              ],
              contexts: [ 'action' ]
            }))

            browser.test.sendMessage('Menus Created')
            """

        let darkIcon16 = makePNGData(size: CGSize(width: 16, height: 16), color: .white)
        let lightIcon16 = makePNGData(size: CGSize(width: 16, height: 16), color: .black)

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "icon-dark-16.png": darkIcon16,
            "icon-light-16.png": lightIcon16,
        ]

        let manager = try loadWebExtension(manifest: menusManifest, resources: resources)
        let context = try requireNonNil(manager.context)

        try await manager.waitForTestMessage("Menus Created")

        let action = try requireNonNil(context.action(for: manager.defaultTab))
        let menuItems = action.menuItems

        expectEqual(menuItems.count, 1)

        let menuItem = try requireNonNil(menuAction(menuItems.first))
        expectTrue(menuItem.isKind(of: CocoaMenuItem.self))
        expectEqual(menuItem.title, "Menu Item with Icon Variants")

        let image = try requireNonNil(menuItem.image)

        #if USE_APPKIT
        expectEqual(image.size, CGSize(width: 16, height: 16))
        #else
        expectEqual(image.size, CGSize(width: 20, height: 20))
        #endif

        withAppearance(.dark) {
            expectTrue(compareColors(pixelColor(of: image), .white))
        }

        withAppearance(.light) {
            expectTrue(compareColors(pixelColor(of: image), .black))
        }
    }

    @Test
    func menuItemWithImageDataVariants() async throws {
        let backgroundScript = """
            const createImageData = (size, color) => {
              const context = new OffscreenCanvas(size, size).getContext('2d')
              context.fillStyle = color
              context.fillRect(0, 0, size, size)

              return context.getImageData(0, 0, size, size)
            }

            const darkImageData = createImageData(16, 'white')
            const lightImageData = createImageData(16, 'black')

            browser.test.assertSafe(() => browser.menus.create({
              id: 'menu-item-with-image-data-variants',
              title: 'Menu Item with ImageData Variants',
              iconVariants: [
                { 16: darkImageData, 'colorSchemes': [ 'dark' ] },
                { 16: lightImageData, 'colorSchemes': [ 'light' ] }
              ],
              contexts: [ 'action' ]
            }))

            browser.test.sendMessage('Menus Created')
            """

        let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
        let context = try requireNonNil(manager.context)

        try await manager.waitForTestMessage("Menus Created")

        let action = try requireNonNil(context.action(for: manager.defaultTab))
        let menuItems = action.menuItems

        expectEqual(menuItems.count, 1)

        let menuItem = try requireNonNil(menuAction(menuItems.first))
        expectTrue(menuItem.isKind(of: CocoaMenuItem.self))
        expectEqual(menuItem.title, "Menu Item with ImageData Variants")

        let image = try requireNonNil(menuItem.image)

        #if USE_APPKIT
        expectEqual(image.size, CGSize(width: 16, height: 16))
        #else
        expectEqual(image.size, CGSize(width: 20, height: 20))
        #endif

        withAppearance(.dark) {
            expectTrue(compareColors(pixelColor(of: image), .white))
        }

        withAppearance(.light) {
            expectTrue(compareColors(pixelColor(of: image), .black))
        }
    }

    @Test
    func menuItemWithWithNoValidVariants() async throws {
        try await loadAndRun(
            backgroundScript: """
                const createImageData = (size, color) => {
                  const context = new OffscreenCanvas(size, size).getContext('2d')
                  context.fillStyle = color
                  context.fillRect(0, 0, size, size)

                  return context.getImageData(0, 0, size, size)
                }

                const validImageData = createImageData(16, 'white')

                await browser.test.assertThrows(() => browser.menus.create({
                    id: 'submenu-item-invalid-dimension',
                    parentId: 'top-level-item',
                    title: 'Submenu with Invalid Dimension Key',
                    iconVariants: [
                      { 'sixteen': validImageData, 'colorSchemes': [ 'light' ] }
                    ],
                    contexts: [ 'action' ]
                }), /'iconVariants\\[0\\]' value is invalid, because 'sixteen' is not a valid dimension/)

                await browser.test.assertThrows(() => browser.menus.create({
                    id: 'submenu-item-invalid-color-scheme',
                    parentId: 'top-level-item',
                    title: 'Submenu with Invalid Color Scheme',
                    iconVariants: [
                      { '16': validImageData, 'colorSchemes': [ 'bad' ] }
                    ],
                    contexts: [ 'action' ]
                }), /'iconVariants\\[0\\]\\['colorSchemes'\\]' value is invalid, because it must specify either 'light' or 'dark'/)

                browser.test.notifyPass()
                """
        )
    }

    @Test
    func menuItemWithMixedValidAndInvalidIconVariants() async throws {
        let backgroundScript = """
            const createImageData = (size, color) => {
              const context = new OffscreenCanvas(size, size).getContext('2d')
              context.fillStyle = color
              context.fillRect(0, 0, size, size)

              return context.getImageData(0, 0, size, size)
            }

            const validImageData = createImageData(16, 'black')
            const invalidImageData = createImageData(16, 'white')

            browser.test.assertSafe(() => browser.menus.create({
              id: 'menu-item-mixed',
              title: 'Menu Item with Mixed Variants',
              iconVariants: [
                { 'sixteen': invalidImageData, 'colorSchemes': [ 'dark' ] },
                { '16': validImageData, 'colorSchemes': [ 'light' ] }
              ],
              contexts: [ 'action' ]
            }))

            browser.test.sendMessage('Menus Created')
            """

        let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
        let context = try requireNonNil(manager.context)

        try await manager.waitForTestMessage("Menus Created")

        let action = try requireNonNil(context.action(for: manager.defaultTab))
        let menuItems = action.menuItems

        expectEqual(menuItems.count, 1)

        let menuItem = try requireNonNil(menuAction(menuItems.first))
        expectTrue(menuItem.isKind(of: CocoaMenuItem.self))
        expectEqual(menuItem.title, "Menu Item with Mixed Variants")

        let image = try requireNonNil(menuItem.image)

        #if USE_APPKIT
        expectEqual(image.size, CGSize(width: 16, height: 16))
        #else
        expectEqual(image.size, CGSize(width: 20, height: 20))
        #endif

        withAppearance(.light) {
            expectTrue(compareColors(pixelColor(of: image), .black))
        }

        withAppearance(.dark) {
            // Should still be black, as light variant is used.
            expectTrue(compareColors(pixelColor(of: image), .black))
        }
    }

    @Test
    func menuItemWithAnySizeVariantAndSVGDataURL() async throws {
        let backgroundScript = """
            const whiteSVGData = 'data:image/svg+xml;base64,' + btoa(`
              <svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">
                <rect width="100" height="100" fill="white" />
              </svg>`)

            const blackSVGData = 'data:image/svg+xml;base64,' + btoa(`
              <svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">
                <rect width="100" height="100" fill="black" />
              </svg>`)

            browser.test.assertSafe(() => browser.menus.create({
              id: 'menu-item-with-svg-variants',
              title: 'Menu Item with SVG Icon Variants',
              iconVariants: [
                { any: whiteSVGData, 'colorSchemes': [ 'dark' ] },
                { any: blackSVGData, 'colorSchemes': [ 'light' ] }
              ],
              contexts: [ 'all' ]
            }))

            browser.test.sendMessage('Menus Created')
            """

        let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
        let context = try requireNonNil(manager.context)

        try await manager.waitForTestMessage("Menus Created")

        let action = try requireNonNil(context.action(for: manager.defaultTab))
        let menuItems = action.menuItems

        expectEqual(menuItems.count, 1)

        let menuItem = try requireNonNil(menuAction(menuItems.first))
        expectTrue(menuItem.isKind(of: CocoaMenuItem.self))
        expectEqual(menuItem.title, "Menu Item with SVG Icon Variants")

        let image = try requireNonNil(menuItem.image)

        #if USE_APPKIT
        expectEqual(image.size, CGSize(width: 16, height: 16))
        #else
        expectEqual(image.size, CGSize(width: 20, height: 20))
        #endif

        withAppearance(.dark) {
            expectTrue(compareColors(pixelColor(of: image), .white))
        }

        withAppearance(.light) {
            expectTrue(compareColors(pixelColor(of: image), .black))
        }
    }

    @Test
    func updateMenuItemWithIconVariants() async throws {
        let backgroundScript = """
            browser.test.assertSafe(() => browser.menus.create({
              id: 'menu-item-without-icon-variants',
              title: 'Menu Item without Icon Variants',
              contexts: [ 'action' ]
            }))

            browser.test.assertSafe(() => browser.menus.update('menu-item-without-icon-variants', {
              title: 'Menu Item with Icon Variants',
              iconVariants: [
                { 16: 'icon-dark-16.png', 'colorSchemes': [ 'dark' ] },
                { 16: 'icon-light-16.png', 'colorSchemes': [ 'light' ] }
              ]
            }))

            browser.test.sendMessage('Menus Updated')
            """

        let darkIcon16 = makePNGData(size: CGSize(width: 16, height: 16), color: .white)
        let lightIcon16 = makePNGData(size: CGSize(width: 16, height: 16), color: .black)

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "icon-dark-16.png": darkIcon16,
            "icon-light-16.png": lightIcon16,
        ]

        let manager = try loadWebExtension(manifest: menusManifest, resources: resources)
        let context = try requireNonNil(manager.context)

        try await manager.waitForTestMessage("Menus Updated")

        let action = try requireNonNil(context.action(for: manager.defaultTab))
        let menuItems = action.menuItems

        expectEqual(menuItems.count, 1)

        let menuItem = try requireNonNil(menuAction(menuItems.first))
        expectTrue(menuItem.isKind(of: CocoaMenuItem.self))
        expectEqual(menuItem.title, "Menu Item with Icon Variants")

        let image = try requireNonNil(menuItem.image)

        #if USE_APPKIT
        expectEqual(image.size, CGSize(width: 16, height: 16))
        #else
        expectEqual(image.size, CGSize(width: 20, height: 20))
        #endif

        withAppearance(.dark) {
            expectTrue(compareColors(pixelColor(of: image), .white))
        }

        withAppearance(.light) {
            expectTrue(compareColors(pixelColor(of: image), .black))
        }
    }

    @Test
    func clearMenuItemIconVariantsWithNull() async throws {
        let backgroundScript = """
            browser.test.assertSafe(() => browser.menus.create({
              id: 'menu-item-with-icon-variants',
              title: 'Menu Item with Icon Variants',
              iconVariants: [
                { 16: 'icon-dark-16.png', 'colorSchemes': [ 'dark' ] },
                { 16: 'icon-light-16.png', 'colorSchemes': [ 'light' ] }
              ],
              contexts: [ 'action' ]
            }))

            browser.test.assertSafe(() => browser.menus.update('menu-item-with-icon-variants', {
              iconVariants: null,
              title: 'Menu Item without Icon Variants'
            }))

            browser.test.sendMessage('Menus Updated')
            """

        let darkIcon16 = makePNGData(size: CGSize(width: 16, height: 16), color: .white)
        let lightIcon16 = makePNGData(size: CGSize(width: 16, height: 16), color: .black)

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "icon-dark-16.png": darkIcon16,
            "icon-light-16.png": lightIcon16,
        ]

        let manager = try loadWebExtension(manifest: menusManifest, resources: resources)
        let context = try requireNonNil(manager.context)

        try await manager.waitForTestMessage("Menus Updated")

        let action = try requireNonNil(context.action(for: manager.defaultTab))
        let menuItems = action.menuItems

        expectEqual(menuItems.count, 1)

        let menuItem = try requireNonNil(menuAction(menuItems.first))
        expectTrue(menuItem.isKind(of: CocoaMenuItem.self))
        expectEqual(menuItem.title, "Menu Item without Icon Variants")

        // Icon should be null after clearing.
        expectNil(menuItem.image)
    }

    @Test
    func clearMenuItemIconVariantsWithEmpty() async throws {
        let backgroundScript = """
            browser.test.assertSafe(() => browser.menus.create({
              id: 'menu-item-with-icon-variants',
              title: 'Menu Item with Icon Variants',
              iconVariants: [
                { 16: 'icon-dark-16.png', 'colorSchemes': [ 'dark' ] },
                { 16: 'icon-light-16.png', 'colorSchemes': [ 'light' ] }
              ],
              contexts: [ 'action' ]
            }))

            browser.test.assertSafe(() => browser.menus.update('menu-item-with-icon-variants', {
              iconVariants: [ ],
              title: 'Menu Item without Icon Variants'
            }))

            browser.test.sendMessage('Menus Updated')
            """

        let darkIcon16 = makePNGData(size: CGSize(width: 16, height: 16), color: .white)
        let lightIcon16 = makePNGData(size: CGSize(width: 16, height: 16), color: .black)

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "icon-dark-16.png": darkIcon16,
            "icon-light-16.png": lightIcon16,
        ]

        let manager = try loadWebExtension(manifest: menusManifest, resources: resources)
        let context = try requireNonNil(manager.context)

        try await manager.waitForTestMessage("Menus Updated")

        let action = try requireNonNil(context.action(for: manager.defaultTab))
        let menuItems = action.menuItems

        expectEqual(menuItems.count, 1)

        let menuItem = try requireNonNil(menuAction(menuItems.first))
        expectTrue(menuItem.isKind(of: CocoaMenuItem.self))
        expectEqual(menuItem.title, "Menu Item without Icon Variants")

        // Icon should be null after clearing.
        expectNil(menuItem.image)
    }

    @Test
    func menuItemWithSymbolImageIconVariants() async throws {
        let backgroundScript = """
            browser.test.assertSafe(() => browser.menus.create({
              id: 'menu-item-with-symbol-variants',
              title: 'Menu Item with Symbol Variants',
              iconVariants: [
                { any: 'symbol:star' }
              ],
              contexts: [ 'action' ]
            }))

            browser.test.sendMessage('Menus Created')
            """

        let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
        let context = try requireNonNil(manager.context)

        try await manager.waitForTestMessage("Menus Created")

        let action = try requireNonNil(context.action(for: manager.defaultTab))
        let menuItems = action.menuItems

        expectEqual(menuItems.count, 1)

        let menuItem = try requireNonNil(menuAction(menuItems.first))
        expectTrue(menuItem.isKind(of: CocoaMenuItem.self))
        expectEqual(menuItem.title, "Menu Item with Symbol Variants")

        let image = try requireNonNil(menuItem.image)
        expectTrue(image.isKind(of: CocoaImage.self))
        expectTrue(image.isSymbol)
    }
    #endif // ENABLE_WK_WEB_EXTENSIONS_ICON_VARIANTS

    @Test
    func menuItemWithSymbolImageIcon() async throws {
        let backgroundScript = """
            browser.test.assertSafe(() => browser.menus.create({
              id: 'menu-item-with-symbol-icon',
              title: 'Menu Item with Symbol Icon',
              icons: {
                16: 'symbol:star'
              },
              contexts: [ 'action' ]
            }))

            browser.test.sendMessage('Menus Created')
            """

        let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
        let context = try requireNonNil(manager.context)

        try await manager.waitForTestMessage("Menus Created")

        let action = try requireNonNil(context.action(for: manager.defaultTab))
        let menuItems = action.menuItems

        expectEqual(menuItems.count, 1)

        let menuItem = try requireNonNil(menuAction(menuItems.first))
        expectTrue(menuItem.isKind(of: CocoaMenuItem.self))
        expectEqual(menuItem.title, "Menu Item with Symbol Icon")

        let image = try requireNonNil(menuItem.image)
        expectTrue(image.isKind(of: CocoaImage.self))
        expectTrue(image.isSymbol)
    }

    @Test
    func toggleCheckboxMenuItems() async throws {
        let backgroundScript = """
            browser.menus.create({
              id: 'checkbox-1',
              title: 'Checkbox 1',
              type: 'checkbox',
              checked: true,
              contexts: [ 'all' ]
            })

            browser.menus.create({
              id: 'checkbox-2',
              title: 'Checkbox 2',
              type: 'checkbox',
              contexts: [ 'all' ]
            })

            let clickCount = 0

            browser.menus.onClicked.addListener((info) => {
              if (info.menuItemId === 'checkbox-1') {
                browser.test.assertTrue(info.wasChecked)
                browser.test.assertFalse(info.checked)
              } else if (info.menuItemId === 'checkbox-2') {
                browser.test.assertFalse(info.wasChecked)
                browser.test.assertTrue(info.checked)
              }

              if (++clickCount === 2)
                browser.test.sendMessage('Menus Clicked')
            })

            browser.test.sendMessage('Menus Created')
            """

        let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
        let context = try requireNonNil(manager.context)
        let defaultTab = try requireNonNil(manager.defaultTab)

        try await manager.waitForTestMessage("Menus Created")

        let menuItems = context.menuItems(for: defaultTab)
        expectEqual(menuItems.count, 1)

        let submenuItems = itemsInSubmenu(of: menuItems.first)
        expectEqual(submenuItems.count, 2)

        var checkbox1 = try requireNonNil(menuAction(submenuItems.first))
        var checkbox2 = try requireNonNil(menuAction(submenuItems.last))

        expectTrue(checkbox1.isKind(of: CocoaMenuItem.self))
        expectTrue(checkbox2.isKind(of: CocoaMenuItem.self))
        expectEqual(checkbox1.state, .on)
        expectEqual(checkbox2.state, .off)

        try perform(checkbox1)
        try perform(checkbox2)

        try await manager.waitForTestMessage("Menus Clicked")

        let updatedMenuItems = context.menuItems(for: defaultTab)
        expectEqual(updatedMenuItems.count, 1)

        let updatedSubmenuItems = itemsInSubmenu(of: updatedMenuItems.first)
        expectEqual(updatedSubmenuItems.count, 2)

        checkbox1 = try requireNonNil(menuAction(updatedSubmenuItems.first))
        checkbox2 = try requireNonNil(menuAction(updatedSubmenuItems.last))

        expectEqual(checkbox1.state, .off)
        expectEqual(checkbox2.state, .on)
    }

    @Test
    func radioItemGrouping() async throws {
        let backgroundScript = """
            browser.menus.create({
              id: 'radio-1-group-1',
              title: 'Radio 1 Group 1',
              type: 'radio',
              checked: true,
              contexts: [ 'all' ]
            })

            browser.menus.create({
              id: 'radio-2-group-1',
              title: 'Radio 2 Group 1',
              type: 'radio',
              contexts: [ 'all' ]
            })

            browser.menus.create({
              title: 'Normal Item',
              visible: false,
              contexts: [ 'all' ]
            })

            browser.menus.create({
              id: 'radio-1-group-2',
              title: 'Radio 1 Group 2',
              type: 'radio',
              contexts: [ 'all' ]
            })

            browser.menus.create({
              id: 'radio-2-group-2',
              title: 'Radio 2 Group 2',
              type: 'radio',
              contexts: [ 'all' ]
            })

            browser.menus.create({
              id: 'radio-3-group-3',
              title: 'Radio 3 Group 2',
              type: 'radio',
              checked: true,
              contexts: [ 'all' ]
            })

            let clickCount = 0

            browser.menus.onClicked.addListener((info) => {
              if (info.menuItemId === 'radio-2-group-1') {
                browser.test.assertTrue(info.wasChecked)
                browser.test.assertTrue(info.checked)
              } else if (info.menuItemId === 'radio-2-group-2') {
                browser.test.assertFalse(info.wasChecked)
                browser.test.assertTrue(info.checked)
              }

              if (++clickCount === 2)
                browser.test.sendMessage('Menus Clicked')
            })

            browser.test.sendMessage('Menus Created')
            """

        let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
        let context = try requireNonNil(manager.context)
        let defaultTab = try requireNonNil(manager.defaultTab)

        try await manager.waitForTestMessage("Menus Created")

        let menuItems = context.menuItems(for: defaultTab)
        expectEqual(menuItems.count, 1)

        // Indexing the items below would crash if there were fewer of them, so stop if the count is wrong.
        let submenuItems = itemsInSubmenu(of: menuItems.first)
        try requireEqual(submenuItems.count, 6)

        var radio1Group1 = try requireNonNil(menuAction(submenuItems[0]))
        var radio2Group1 = try requireNonNil(menuAction(submenuItems[1]))
        var radio1Group2 = try requireNonNil(menuAction(submenuItems[3]))
        var radio2Group2 = try requireNonNil(menuAction(submenuItems[4]))
        var radio3Group2 = try requireNonNil(menuAction(submenuItems[5]))

        expectEqual(radio1Group1.state, .on)
        expectEqual(radio2Group1.state, .off)

        expectEqual(radio1Group2.state, .off)
        expectEqual(radio2Group2.state, .off)
        expectEqual(radio3Group2.state, .on)

        try perform(radio1Group1)
        try perform(radio2Group2)

        try await manager.waitForTestMessage("Menus Clicked")

        let updatedMenuItems = context.menuItems(for: defaultTab)
        expectEqual(updatedMenuItems.count, 1)

        let updatedSubmenuItems = itemsInSubmenu(of: updatedMenuItems.first)
        try requireEqual(updatedSubmenuItems.count, 6)

        radio1Group1 = try requireNonNil(menuAction(updatedSubmenuItems[0]))
        radio2Group1 = try requireNonNil(menuAction(updatedSubmenuItems[1]))
        radio1Group2 = try requireNonNil(menuAction(updatedSubmenuItems[3]))
        radio2Group2 = try requireNonNil(menuAction(updatedSubmenuItems[4]))
        radio3Group2 = try requireNonNil(menuAction(updatedSubmenuItems[5]))

        // The checked item as clicked again, so it does not change.
        expectEqual(radio1Group1.state, .on)
        expectEqual(radio2Group1.state, .off)

        // A new item was clicked, so it does change.
        expectEqual(radio1Group2.state, .off)
        expectEqual(radio2Group2.state, .on)
        expectEqual(radio3Group2.state, .off)
    }

    @Test
    func onClick() async throws {
        let backgroundScript = """
            browser.menus.create({
              id: 'click-item',
              title: 'Click Item',
              contexts: ['all'],
              onclick: (info, tab) => {
                browser.test.assertEq(typeof info, 'object')
                browser.test.assertEq(typeof tab, 'object')

                browser.test.assertEq(info.menuItemId, 'click-item')
                browser.test.assertEq(typeof tab.id, 'number')

                browser.test.notifyPass()
              }
            }, () => browser.test.sendMessage('Menu Item Created'))
            """

        let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
        let context = try requireNonNil(manager.context)
        let defaultTab = try requireNonNil(manager.defaultTab)

        try await manager.waitForTestMessage("Menu Item Created")

        let menuItems = context.menuItems(for: defaultTab)
        expectEqual(menuItems.count, 1)

        try perform(requireNonNil(menuItems.first))

        try await manager.run()
    }

    @Test
    func onClickAfterUpdate() async throws {
        let backgroundScript = """
            browser.menus.create({
              id: 'click-item',
              title: 'Click Item',
              contexts: ['all']
            })

            await browser.menus.update('click-item', {
              onclick: (info, tab) => {
                browser.test.assertEq(typeof info, 'object')
                browser.test.assertEq(typeof tab, 'object')

                browser.test.assertEq(info.menuItemId, 'click-item')
                browser.test.assertEq(typeof tab.id, 'number')

                browser.test.notifyPass()
              }
            })

            browser.test.sendMessage('Menu Item Created')
            """

        let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
        let context = try requireNonNil(manager.context)
        let defaultTab = try requireNonNil(manager.defaultTab)

        try await manager.waitForTestMessage("Menu Item Created")

        let menuItems = context.menuItems(for: defaultTab)
        expectEqual(menuItems.count, 1)

        try perform(requireNonNil(menuItems.first))

        try await manager.run()
    }

    @Test
    func removeAfterBackgroundPageReload() async throws {
        let backgroundScript = """
            browser.test.onMessage.addListener(async (message) => {
              if (message == 'Create') {
                browser.menus.create({ id: 'test-item', title: 'Test' }, () => {
                  browser.test.sendMessage('Created')
                })
              }

              if (message == 'Remove') {
                await browser.menus.remove('test-item')
                browser.test.sendMessage('Removed')
              }
            })

            browser.test.sendMessage('Ready')
            """

        let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
        let context = try requireNonNil(manager.context)

        try await manager.waitForTestMessage("Ready")

        manager.sendTestMessage("Create")
        try await manager.waitForTestMessage("Created")

        try await waitForCondition("the background web view to unload", timeout: .seconds(10)) {
            context._backgroundWebView == nil
        }

        manager.sendTestMessage("Remove")
        try await manager.waitForTestMessage("Ready")
        try await manager.waitForTestMessage("Removed")
    }

    @Test
    func remove() async throws {
        let backgroundScript = """
            browser.menus.create({ id: 'item-1', title: 'Item 1', contexts: [ 'all' ] })
            browser.menus.create({ id: 'item-2', title: 'Item 2', contexts: [ 'all' ] })
            browser.menus.create({ id: 'item-3', title: 'Item 3', contexts: [ 'all' ] })

            await browser.menus.remove('item-2')

            browser.test.sendMessage('Menu Item Removed')
            """

        let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
        let context = try requireNonNil(manager.context)
        let defaultTab = try requireNonNil(manager.defaultTab)

        try await manager.waitForTestMessage("Menu Item Removed")

        let menuItems = itemsInSubmenu(of: context.menuItems(for: defaultTab).first)
        expectEqual(menuItems.count, 2)

        expectEqual(menuItems.first?.title, "Item 1")
        expectEqual(menuItems.last?.title, "Item 3")
    }

    @Test
    func removeWithSubmenuItems() async throws {
        let backgroundScript = """
            browser.menus.create({ id: 'parent', title: 'Parent', contexts: [ 'all' ] })
            browser.menus.create({ id: 'child-1', parentId: 'parent', title: 'Child 1', contexts: [ 'all' ] })
            browser.menus.create({ id: 'child-2', parentId: 'parent', title: 'Child 2', contexts: [ 'all' ] })
            browser.menus.create({ id: 'sibling', title: 'Sibling', contexts: [ 'all' ] })

            await browser.menus.remove('parent')

            // The submenu items were removed along with their parent.
            await browser.test.assertRejects(browser.menus.remove('child-1'), /menu item not found/i)
            await browser.test.assertRejects(browser.menus.remove('child-2'), /menu item not found/i)
            await browser.test.assertRejects(browser.menus.remove('parent'), /menu item not found/i)

            browser.test.sendMessage('Menu Item Removed')
            """

        let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
        let context = try requireNonNil(manager.context)
        let defaultTab = try requireNonNil(manager.defaultTab)

        try await manager.waitForTestMessage("Menu Item Removed")

        let menuItems = context.menuItems(for: defaultTab)
        expectEqual(menuItems.count, 1)

        expectEqual(menuItems.first?.title, "Sibling")
    }

    @Test
    func removeAll() async throws {
        let backgroundScript = """
            browser.menus.create({ id: 'parent', title: 'Parent', contexts: [ 'all' ] })
            browser.menus.create({ id: 'child', parentId: 'parent', title: 'Child', contexts: [ 'all' ] })
            browser.menus.create({ id: 'item', title: 'Item', contexts: [ 'all' ] })

            await browser.menus.removeAll()

            await browser.test.assertRejects(browser.menus.remove('item'), /menu item not found/i)
            await browser.test.assertRejects(browser.menus.remove('child'), /menu item not found/i)

            // Removing from an empty set succeeds, and the same identifiers can be created again.
            await browser.menus.removeAll()
            browser.menus.create({ id: 'item', title: 'Recreated Item', contexts: [ 'all' ] })

            browser.test.sendMessage('Menu Items Removed')
            """

        let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
        let context = try requireNonNil(manager.context)
        let defaultTab = try requireNonNil(manager.defaultTab)

        try await manager.waitForTestMessage("Menu Items Removed")

        let menuItems = context.menuItems(for: defaultTab)
        expectEqual(menuItems.count, 1)

        expectEqual(menuItems.first?.title, "Recreated Item")
    }

    @Test
    func contextMenusNamespace() async throws {
        try await loadAndRun(
            backgroundScript: """
                browser.test.assertEq(typeof browser.contextMenus, 'object')

                browser.test.notifyPass()
                """
        )
    }

    #if WTF_PLATFORM_MAC

    @Test
    func macContextMenuItems() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.menus.create({
              id: 'context-menu-1',
              title: 'Context Menu 1'
            })

            browser.menus.create({
              id: 'context-menu-2',
              title: 'Context Menu 2'
            })

            browser.menus.onClicked.addListener((info, tab) => {
              browser.test.assertEq(typeof info, 'object')
              browser.test.assertEq(typeof tab, 'object')

              browser.test.assertEq(info.menuItemId, 'context-menu-1')
              browser.test.assertEq(info.parentMenuItemId, undefined)
              browser.test.assertEq(info.selectionText, undefined)
              browser.test.assertEq(typeof info.pageUrl, 'string')
              browser.test.assertTrue(info.pageUrl.startsWith('http://localhost:'))
              browser.test.assertEq(info.frameId, 0)
              browser.test.assertEq(info.editable, false)

              browser.test.assertEq(typeof tab.id, 'number')
              browser.test.assertEq(typeof tab.windowId, 'number')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Menus Created')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)
            let defaultTab = try requireNonNil(manager.defaultTab)

            try await manager.waitForTestMessage("Menus Created")

            expectFalse(context.hasActiveUserGesture(in: defaultTab))

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            let webView = try await makeDefaultTabWebView(for: manager, loading: configuration.localhostAddress)

            var extensionSubmenuItem: NSMenuItem?
            var extensionMenuItems: [NSMenuItem] = []

            try await withNextContextMenu(in: webView) { menu in
                extensionSubmenuItem = menu.items.first { $0.title == "Menus Test" }
                extensionMenuItems = extensionSubmenuItem?.submenu?.items ?? []

                if let menuItem = extensionMenuItems.first {
                    try perform(menuItem)
                }
            }

            expectNotNil(extensionSubmenuItem)

            let expectedTitles = ["Context Menu 1", "Context Menu 2"]
            expectEqual(extensionMenuItems.count, expectedTitles.count)

            for (menuItem, expectedTitle) in zip(extensionMenuItems, expectedTitles) {
                expectTrue(menuItem.isKind(of: NSMenuItem.self))
                expectEqual(menuItem.title, expectedTitle)
            }

            try await manager.run()

            expectTrue(context.hasActiveUserGesture(in: defaultTab))
        }
    }

    @Test
    func macActiveTabContextMenuItems() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.menus.create({
              id: 'context-menu',
              title: 'Context Menu Item'
            })

            browser.menus.onClicked.addListener((info, tab) => {
              browser.test.assertEq(typeof info, 'object')
              browser.test.assertEq(typeof tab, 'object')

              browser.test.assertEq(info.menuItemId, 'context-menu')
              browser.test.assertEq(info.parentMenuItemId, undefined)
              browser.test.assertEq(info.selectionText, undefined)
              browser.test.assertEq(typeof info.pageUrl, 'string')
              browser.test.assertTrue(info.pageUrl.startsWith('http://localhost:'))
              browser.test.assertEq(info.frameId, 0)
              browser.test.assertEq(info.editable, false)

              browser.test.assertEq(typeof tab.id, 'number')
              browser.test.assertEq(typeof tab.windowId, 'number')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Menus Created')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)
            let defaultTab = try requireNonNil(manager.defaultTab)

            try await manager.waitForTestMessage("Menus Created")

            expectFalse(context.hasActiveUserGesture(in: defaultTab))

            let webView = try await makeDefaultTabWebView(for: manager, loading: configuration.localhostAddress)

            var selectionMenuItem: NSMenuItem?

            try await withNextContextMenu(in: webView) { menu in
                selectionMenuItem = menu.items.first { $0.title == "Context Menu Item" }

                if let selectionMenuItem {
                    try perform(selectionMenuItem)
                }
            }

            expectNotNil(selectionMenuItem)

            try await manager.run()

            expectTrue(context.hasActiveUserGesture(in: defaultTab))
        }
    }

    @Test
    func macURLPatternContextMenuItems() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<a href='http://example.com/test' style='font-size: 100px'>Large Example Link</p>"
            }
        }

        let backgroundScript = """
            browser.menus.create({
              id: 'url-pattern-menu-item',
              title: 'URL Pattern Item',
              contexts: ['link'],
              targetUrlPatterns: ['*://*.example.com/*'],
              documentUrlPatterns: ['*://localhost/*']
            })

            browser.menus.create({
              id: 'non-matching-menu-item',
              title: 'Non-Matching Item',
              contexts: ['link'],
              targetUrlPatterns: ['*://*.other.com/*'],
              documentUrlPatterns: ['*://*.apple.com/*']
            })

            browser.menus.onClicked.addListener((info, tab) => {
              browser.test.assertEq(typeof info, 'object')
              browser.test.assertEq(typeof tab, 'object')

              browser.test.assertEq(info.menuItemId, 'url-pattern-menu-item')
              browser.test.assertEq(info.parentMenuItemId, undefined)
              browser.test.assertEq(info.selectionText, 'Large Example Link')
              browser.test.assertEq(info.linkText, 'Large Example Link')
              browser.test.assertEq(info.linkUrl, 'http://example.com/test')
              browser.test.assertEq(typeof info.pageUrl, 'string')
              browser.test.assertTrue(info.pageUrl.startsWith('http://localhost:'))
              browser.test.assertEq(info.frameId, 0)
              browser.test.assertEq(info.editable, false)

              browser.test.assertEq(typeof tab.id, 'number')
              browser.test.assertEq(typeof tab.windowId, 'number')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Menus Created')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)

            try await manager.waitForTestMessage("Menus Created")

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            let webView = try await makeDefaultTabWebView(for: manager, loading: configuration.localhostAddress)

            var urlPatternMenuItem: NSMenuItem?
            var nonMatchingMenuItem: NSMenuItem?

            try await withNextContextMenu(in: webView, at: NSPoint(x: 50, y: 50)) { menu in
                for menuItem in menu.items {
                    if menuItem.title == "URL Pattern Item" {
                        urlPatternMenuItem = menuItem
                    } else if menuItem.title == "Non-Matching Item" {
                        nonMatchingMenuItem = menuItem
                    }
                }

                if let urlPatternMenuItem {
                    try perform(urlPatternMenuItem)
                }
            }

            expectNotNil(urlPatternMenuItem)
            expectNil(nonMatchingMenuItem)

            try await manager.run()
        }
    }

    @Test
    func macSelectionContextMenuItems() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<p style='font-size: 100px'>Selection Example Text</p>"
            }
        }

        let backgroundScript = """
            browser.menus.create({
              id: 'selection-menu-item',
              title: 'Selected: %s',
              contexts: [ 'selection' ]
            })

            browser.menus.onClicked.addListener((info, tab) => {
              browser.test.assertEq(typeof info, 'object')
              browser.test.assertEq(typeof tab, 'object')

              browser.test.assertEq(info.menuItemId, 'selection-menu-item')
              browser.test.assertEq(info.parentMenuItemId, undefined)
              browser.test.assertEq(info.selectionText, 'Example')
              browser.test.assertEq(typeof info.pageUrl, 'string')
              browser.test.assertTrue(info.pageUrl.startsWith('http://localhost:'))
              browser.test.assertEq(info.frameId, 0)
              browser.test.assertEq(info.editable, false)

              browser.test.assertEq(typeof tab.id, 'number')
              browser.test.assertEq(typeof tab.windowId, 'number')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Menus Created')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)

            try await manager.waitForTestMessage("Menus Created")

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            let webView = try await makeDefaultTabWebView(for: manager, loading: configuration.localhostAddress)

            var selectionMenuItem: NSMenuItem?

            try await withNextContextMenu(in: webView) { menu in
                selectionMenuItem = menu.items.first { $0.title.hasPrefix("Selected: ") }

                if let selectionMenuItem {
                    try perform(selectionMenuItem)
                }
            }

            expectNotNil(selectionMenuItem)
            expectEqual(selectionMenuItem?.title, "Selected: Example")

            try await manager.run()
        }
    }

    @Test
    func macLinkContextMenuItems() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<a href='http://example.com/test' style='font-size: 100px'>Large Example Link</p>"
            }
        }

        let backgroundScript = """
            browser.menus.create({
              id: 'link-menu-item',
              title: 'Link Item',
              contexts: [ 'link' ]
            })

            browser.menus.onClicked.addListener((info, tab) => {
              browser.test.assertEq(typeof info, 'object')
              browser.test.assertEq(typeof tab, 'object')

              browser.test.assertEq(info.menuItemId, 'link-menu-item')
              browser.test.assertEq(info.parentMenuItemId, undefined)
              browser.test.assertEq(info.selectionText, 'Large Example Link')
              browser.test.assertEq(info.linkText, 'Large Example Link')
              browser.test.assertEq(info.linkUrl, 'http://example.com/test')
              browser.test.assertEq(typeof info.pageUrl, 'string')
              browser.test.assertTrue(info.pageUrl.startsWith('http://localhost:'))
              browser.test.assertEq(info.frameId, 0)
              browser.test.assertEq(info.editable, false)

              browser.test.assertEq(typeof tab.id, 'number')
              browser.test.assertEq(typeof tab.windowId, 'number')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Menus Created')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)

            try await manager.waitForTestMessage("Menus Created")

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            let webView = try await makeDefaultTabWebView(for: manager, loading: configuration.localhostAddress)

            var linkMenuItem: NSMenuItem?

            try await withNextContextMenu(in: webView) { menu in
                linkMenuItem = menu.items.first { $0.title == "Link Item" }

                if let linkMenuItem {
                    try perform(linkMenuItem)
                }
            }

            expectNotNil(linkMenuItem)

            try await manager.run()
        }
    }

    @Test
    func macImageContextMenuItems() async throws {
        let imageURL = try requireNonNil(Bundle.testResources.url(forResource: "400x400-green", withExtension: "png"))
        let imageData = try Data(contentsOf: imageURL)

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<img src='test.png' style='width: 400px; height: 400px'>"
            }

            Route("/test.png") {
                imageData
            }
        }

        let backgroundScript = """
            browser.menus.create({
              id: 'image-menu-item',
              title: 'Image Item',
              contexts: [ 'image' ]
            })

            browser.menus.onClicked.addListener((info, tab) => {
              browser.test.assertEq(typeof info, 'object')
              browser.test.assertEq(typeof tab, 'object')

              browser.test.assertEq(info.menuItemId, 'image-menu-item')
              browser.test.assertEq(info.parentMenuItemId, undefined)
              browser.test.assertEq(info.mediaType, 'image')
              browser.test.assertTrue(info.srcUrl.endsWith('/test.png'))
              browser.test.assertEq(typeof info.pageUrl, 'string')
              browser.test.assertTrue(info.pageUrl.startsWith('http://localhost:'))
              browser.test.assertEq(info.frameId, 0)
              browser.test.assertEq(info.editable, false)

              browser.test.assertEq(typeof tab.id, 'number')
              browser.test.assertEq(typeof tab.windowId, 'number')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Menus Created')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)

            try await manager.waitForTestMessage("Menus Created")

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            let webView = try await makeDefaultTabWebView(for: manager, loading: configuration.localhostAddress)

            var imageMenuItem: NSMenuItem?

            try await withNextContextMenu(in: webView) { menu in
                imageMenuItem = menu.items.first { $0.title == "Image Item" }

                if let imageMenuItem {
                    try perform(imageMenuItem)
                }
            }

            expectNotNil(imageMenuItem)

            try await manager.run()
        }
    }

    @Test
    func macVideoContextMenuItems() async throws {
        let videoURL = try requireNonNil(Bundle.testResources.url(forResource: "test", withExtension: "mp4"))
        let videoData = try Data(contentsOf: videoURL)

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<video src='test.mp4' style='width: 400px; height: 400px' controls></video>"
            }

            Route("/test.mp4") {
                videoData
            }

            Route("/favicon.ico", statusCode: 404)
        }

        let backgroundScript = """
            browser.menus.create({
              id: 'video-menu-item',
              title: 'Video Item',
              contexts: [ 'video' ]
            })

            browser.menus.onClicked.addListener((info, tab) => {
              browser.test.assertEq(typeof info, 'object')
              browser.test.assertEq(typeof tab, 'object')

              browser.test.assertEq(info.menuItemId, 'video-menu-item')
              browser.test.assertEq(info.parentMenuItemId, undefined)
              browser.test.assertEq(info.mediaType, 'video')
              browser.test.assertTrue(info.srcUrl.endsWith('/test.mp4'))
              browser.test.assertEq(typeof info.pageUrl, 'string')
              browser.test.assertTrue(info.pageUrl.startsWith('http://localhost:'))
              browser.test.assertEq(info.frameId, 0)
              browser.test.assertEq(info.editable, false)

              browser.test.assertEq(typeof tab.id, 'number')
              browser.test.assertEq(typeof tab.windowId, 'number')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Menus Created')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)

            try await manager.waitForTestMessage("Menus Created")

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            let webView = try await makeDefaultTabWebView(for: manager, loading: configuration.localhostAddress)

            var videoMenuItem: NSMenuItem?

            try await withNextContextMenu(in: webView) { menu in
                videoMenuItem = menu.items.first { $0.title == "Video Item" }

                if let videoMenuItem {
                    try perform(videoMenuItem)
                }
            }

            expectNotNil(videoMenuItem)

            try await manager.run()
        }
    }

    @Test
    func macAudioContextMenuItems() async throws {
        let audioURL = try requireNonNil(Bundle.testResources.url(forResource: "silence-long", withExtension: "m4a"))
        let audioData = try Data(contentsOf: audioURL)

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<audio src='test.m4a' style='width: 400px; height: 400px' controls></audio>"
            }

            Route("/test.m4a") {
                audioData
            }

            Route("/favicon.ico", statusCode: 404)
        }

        let backgroundScript = """
            browser.menus.create({
              id: 'audio-menu-item',
              title: 'Audio Item',
              contexts: [ 'audio' ]
            })

            browser.menus.onClicked.addListener((info, tab) => {
              browser.test.assertEq(typeof info, 'object')
              browser.test.assertEq(typeof tab, 'object')

              browser.test.assertEq(info.menuItemId, 'audio-menu-item')
              browser.test.assertEq(info.parentMenuItemId, undefined)
              browser.test.assertEq(info.mediaType, 'audio')
              browser.test.assertTrue(info.srcUrl.endsWith('/test.m4a'))
              browser.test.assertEq(typeof info.pageUrl, 'string')
              browser.test.assertTrue(info.pageUrl.startsWith('http://localhost:'))
              browser.test.assertEq(info.frameId, 0)
              browser.test.assertEq(info.editable, false)

              browser.test.assertEq(typeof tab.id, 'number')
              browser.test.assertEq(typeof tab.windowId, 'number')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Menus Created')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)

            try await manager.waitForTestMessage("Menus Created")

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            let webView = try await makeDefaultTabWebView(for: manager, loading: configuration.localhostAddress)

            var audioMenuItem: NSMenuItem?

            try await withNextContextMenu(in: webView) { menu in
                audioMenuItem = menu.items.first { $0.title == "Audio Item" }

                if let audioMenuItem {
                    try perform(audioMenuItem)
                }
            }

            expectNotNil(audioMenuItem)

            try await manager.run()
        }
    }

    @Test
    func macEditableContextMenuItems() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<textarea style='font-size: 50px; width: 400px; height: 400px; white-space: nowrap;'>Editable Text Area</textarea>"
            }
        }

        let backgroundScript = """
            browser.menus.create({
              id: 'editable-menu-item',
              title: 'Editable Item',
              contexts: [ 'editable' ]
            })

            browser.menus.onClicked.addListener((info, tab) => {
              browser.test.assertEq(typeof info, 'object')
              browser.test.assertEq(typeof tab, 'object')

              browser.test.assertEq(info.menuItemId, 'editable-menu-item')
              browser.test.assertEq(info.parentMenuItemId, undefined)
              browser.test.assertEq(info.selectionText, 'Area')
              browser.test.assertEq(info.editable, true)

              browser.test.assertEq(typeof tab.id, 'number')
              browser.test.assertEq(typeof tab.windowId, 'number')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Menus Created')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)

            try await manager.waitForTestMessage("Menus Created")

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            let webView = try await makeDefaultTabWebView(for: manager, loading: configuration.localhostAddress)

            var editableMenuItem: NSMenuItem?

            try await withNextContextMenu(in: webView) { menu in
                editableMenuItem = menu.items.first { $0.title == "Editable Item" }

                if let editableMenuItem {
                    try perform(editableMenuItem)
                }
            }

            expectNotNil(editableMenuItem)

            try await manager.run()
        }
    }

    @Test
    func macFrameContextMenuItems() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='frame.html' style='width: 400px; height: 400px'>"
            }

            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.menus.create({
              id: 'frame-menu-item',
              title: 'Frame Item',
              contexts: [ 'page', 'frame' ]
            })

            browser.menus.onClicked.addListener((info, tab) => {
              browser.test.assertEq(typeof info, 'object')
              browser.test.assertEq(typeof tab, 'object')

              browser.test.assertEq(info.menuItemId, 'frame-menu-item')
              browser.test.assertEq(info.parentMenuItemId, undefined)
              browser.test.assertEq(info.selectionText, undefined)
              browser.test.assertEq(typeof info.frameUrl, 'string')
              browser.test.assertTrue(info.frameUrl.endsWith('frame.html'))
              browser.test.assertTrue(info.frameId !== 0)

              browser.test.assertEq(typeof tab.id, 'number')
              browser.test.assertEq(typeof tab.windowId, 'number')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Menus Created')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
            let context = try requireNonNil(manager.context)

            try await manager.waitForTestMessage("Menus Created")

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress.appending(path: "frame.html"))

            let webView = try await makeDefaultTabWebView(for: manager, loading: configuration.localhostAddress)

            var frameMenuItem: NSMenuItem?

            try await withNextContextMenu(in: webView) { menu in
                frameMenuItem = menu.items.first { $0.title == "Frame Item" }

                if let frameMenuItem {
                    try perform(frameMenuItem)
                }
            }

            expectNotNil(frameMenuItem)

            try await manager.run()
        }
    }

    @Test
    func clickedMenuItemAndPermissionsRequest() async throws {
        let backgroundScript = """
            browser.menus.create({
              id: 'click-item',
              title: 'Click Item',
              contexts: ['all'],
              onclick: async (info, tab) => {
                try {
                  const result = await browser.permissions.request({ 'permissions': [ 'menus' ] })
                  if (result)
                    browser.test.notifyPass()
                  else
                    browser.test.notifyFail('Permissions request was rejected')
                } catch (error) {
                  browser.test.notifyFail('Permissions request failed')
                }
              }
            }, () => browser.test.sendMessage('Menu Item Created'))
            """

        let manager = try loadWebExtension(manifest: menusManifest, resources: ["background.js": backgroundScript])
        let context = try requireNonNil(manager.context)
        let defaultTab = try requireNonNil(manager.defaultTab)

        // The extension already has the menus permission, so this may never be asked. Check each request it gets.
        var requestedPermissionSets: [Set<String>] = []

        manager.internalDelegate.promptForPermissions = { _, requestedPermissions, callback in
            requestedPermissionSets.append(requestedPermissions)
            callback(Set(requestedPermissions.map { WKWebExtension.Permission($0) }), nil)
        }

        try await manager.waitForTestMessage("Menu Item Created")

        let menuItems = context.menuItems(for: defaultTab)
        expectEqual(menuItems.count, 1)

        try perform(requireNonNil(menuItems.first))

        try await manager.run()

        for requestedPermissions in requestedPermissionSets {
            expectEqual(requestedPermissions.count, 1)
            expectEqual(requestedPermissions, ["menus"])
        }
    }

    #endif // WTF_PLATFORM_MAC
}

#endif // ENABLE_WK_WEB_EXTENSIONS
