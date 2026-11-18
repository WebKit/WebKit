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

#if ENABLE_WK_WEB_EXTENSIONS_NOTIFICATIONS

import Foundation
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.WebExtensionUtilities
private import TestWebKitAPILibrary.Helpers.cocoa.TestWebExtensionsDelegate
import Testing
import WebKit
private import WebKit_Private._WKWebExtensionNotification
private import WebKit_Private.WKWebExtensionContextPrivate

import struct Swift.String

@MainActor
struct WKWebExtensionAPINotificationsTests {
    private let notificationsManifest: [String: Any] = [
        "manifest_version": 3,
        "name": "Notifications Test",
        "description": "Notifications",
        "version": "1",

        "permissions": ["notifications"],
        "background": [
            "service_worker": "background.js",
            "type": "module",
        ],
    ]

    private let configuration: WKWebExtensionController.Configuration

    init() {
        configuration = configurationEnablingFeature("WebExtensionNotificationsEnabled")
    }

    private func manager(for backgroundScript: String) -> TestWebExtensionManager {
        parseWebExtension(manifest: notificationsManifest, resources: ["background.js": backgroundScript], configuration: configuration)
    }

    private func loadAndRun(backgroundScript: String) async throws {
        let manager = try loadWebExtension(
            manifest: notificationsManifest,
            resources: ["background.js": backgroundScript],
            configuration: configuration
        )
        try await manager.run()
    }

    @Test
    func apisAvailableWhenPermissionGranted() async throws {
        let backgroundScript = """
            browser.test.assertFalse(browser.notifications === undefined)
            browser.test.assertFalse(browser.notifications.create === undefined)
            browser.test.assertFalse(browser.notifications.update === undefined)
            browser.test.assertFalse(browser.notifications.clear === undefined)
            browser.test.assertFalse(browser.notifications.getAll === undefined)
            browser.test.assertFalse(browser.notifications.getPermissionLevel === undefined)
            browser.test.assertFalse(browser.notifications.onClicked === undefined)
            browser.test.assertFalse(browser.notifications.onButtonClicked === undefined)
            browser.test.assertFalse(browser.notifications.onClosed === undefined)
            browser.test.notifyPass()
            """

        try await loadAndRun(backgroundScript: backgroundScript)
    }

    @Test
    func createArgumentValidation() async throws {
        let backgroundScript = """
            browser.test.assertThrows(() => browser.notifications.create('id', {}), /missing required keys/)
            browser.test.assertThrows(() => browser.notifications.create('id', { type: 'basic', message: 'M' }), /missing required keys/)
            browser.test.assertThrows(() => browser.notifications.create('id', { type: 'basic', title: 'T' }), /missing required keys/)
            browser.test.assertThrows(() => browser.notifications.create('id', { title: 'T', message: 'M' }), /missing required keys/)

            // Each button requires a title.
            browser.test.assertThrows(() => browser.notifications.create('id', { type: 'basic', title: 'T', message: 'M', buttons: [ { } ] }), /missing required keys/)

            browser.test.notifyPass()
            """

        try await loadAndRun(backgroundScript: backgroundScript)
    }

    @Test
    func createAcceptsUnsupportedTypeAsBasic() async throws {
        let backgroundScript = """
            await browser.notifications.create('typed', { type: 'progress', iconUrl: 'icon.png', imageUrl: 'image.png', title: 'T', message: 'M', priority: 2, eventTime: 1700000000000, silent: true, progress: 50 })
            browser.test.notifyPass()
            """

        let manager = manager(for: backgroundScript)

        var presentedNotification: _WKWebExtensionNotification?
        manager.internalDelegate.presentNotification = { notification in
            presentedNotification = notification
        }

        try await manager.loadAndRun()

        #expect(presentedNotification != nil)
        #expect(presentedNotification?.identifier == "typed")
    }

    @Test
    func createPresentsNotificationThroughDelegate() async throws {
        let backgroundScript = """
            const identifier = await browser.notifications.create('my-notification', {
              type: 'basic',
              title: 'Primary Title',
              message: 'Body text',
              contextMessage: 'Secondary line',
              buttons: [ { title: 'First' }, { title: 'Second' } ],
            })
            browser.test.assertEq(identifier, 'my-notification')
            browser.test.notifyPass()
            """

        let manager = manager(for: backgroundScript)

        var presentedNotification: _WKWebExtensionNotification?
        manager.internalDelegate.presentNotification = { notification in
            presentedNotification = notification
        }

        try await manager.loadAndRun()

        #expect(presentedNotification != nil)
        #expect(presentedNotification?.webExtensionContext === manager.context)
        #expect(presentedNotification?.identifier == "my-notification")
        #expect(presentedNotification?.title == "Primary Title")
        #expect(presentedNotification?.body == "Body text")
        #expect(presentedNotification?.subtitle == "Secondary line")
        #expect(presentedNotification?.buttons.count == 2)
        #expect(presentedNotification?.buttons.first?.title == "First")
        #expect(presentedNotification?.buttons.last?.title == "Second")
    }

    @Test
    func createOmitsSubtitleAndButtonsWhenNotProvided() async throws {
        let backgroundScript = """
            await browser.notifications.create('minimal', { type: 'basic', title: 'T', message: 'M' })
            browser.test.notifyPass()
            """

        let manager = manager(for: backgroundScript)

        var presentedNotification: _WKWebExtensionNotification?
        manager.internalDelegate.presentNotification = { notification in
            presentedNotification = notification
        }

        try await manager.loadAndRun()

        #expect(presentedNotification != nil)
        #expect(presentedNotification?.subtitle == nil)
        #expect(presentedNotification?.buttons.count == 0)
    }

    @Test
    func createGeneratesIdentifierWhenOmitted() async throws {
        let backgroundScript = """
            const identifier = await browser.notifications.create({ type: 'basic', title: 'T', message: 'M' })
            browser.test.assertTrue(typeof identifier === 'string' && identifier.length > 0, 'create() should resolve with a generated identifier')
            browser.test.sendMessage('created', identifier)
            """

        let manager = manager(for: backgroundScript)

        var presentedNotification: _WKWebExtensionNotification?
        manager.internalDelegate.presentNotification = { notification in
            presentedNotification = notification
        }

        manager.load()

        let resolvedIdentifier = try await manager.waitForTestMessage("created")

        #expect(presentedNotification != nil)
        #expect((presentedNotification?.identifier.count ?? 0) > 0)
        #expect(presentedNotification?.identifier == resolvedIdentifier as? String)
    }

    @Test
    func createRejectsWhenBrowserReportsError() async throws {
        let backgroundScript = """
            await browser.test.assertRejects(browser.notifications.create('note', { type: 'basic', title: 'T', message: 'M' }), /notifications\\.create/i)
            browser.test.notifyPass()
            """

        try await loadAndRun(backgroundScript: backgroundScript)
    }

    @Test
    func updateReturnsFalseForUnknownIdentifier() async throws {
        let backgroundScript = """
            const wasUpdated = await browser.notifications.update('does-not-exist', { message: 'M' })
            browser.test.assertFalse(wasUpdated, 'update() should resolve false when no notification matches')
            browser.test.notifyPass()
            """

        let manager = manager(for: backgroundScript)

        var updateDelegateCalled = false
        manager.internalDelegate.updateNotification = { _ in
            updateDelegateCalled = true
        }

        try await manager.loadAndRun()

        #expect(!updateDelegateCalled)
    }

    @Test
    func updateDoesNotRequireCreateKeys() async throws {
        let backgroundScript = """
            await browser.notifications.create('note', { type: 'basic', title: 'T', message: 'M' })
            await browser.notifications.update('note', {})
            await browser.notifications.update('note', { title: 'Only title' })
            browser.test.notifyPass()
            """

        let manager = manager(for: backgroundScript)

        manager.internalDelegate.presentNotification = { _ in }
        manager.internalDelegate.updateNotification = { _ in }

        try await manager.loadAndRun()
    }

    @Test
    func updateMergesSuppliedFieldsAndRePresentsThroughDelegate() async throws {
        let backgroundScript = """
            await browser.notifications.create('note', {
              type: 'basic',
              title: 'Original Title',
              message: 'Original body',
              contextMessage: 'Original subtitle',
              buttons: [ { title: 'Only' } ],
            })
            const wasUpdated = await browser.notifications.update('note', { message: 'New body' })
            browser.test.assertTrue(wasUpdated, 'update() should resolve true for an existing notification')
            browser.test.notifyPass()
            """

        let manager = manager(for: backgroundScript)

        manager.internalDelegate.presentNotification = { _ in }

        var updatedNotification: _WKWebExtensionNotification?
        manager.internalDelegate.updateNotification = { notification in
            updatedNotification = notification
        }

        try await manager.loadAndRun()

        #expect(updatedNotification != nil)
        #expect(updatedNotification?.webExtensionContext === manager.context)
        #expect(updatedNotification?.identifier == "note")
        #expect(updatedNotification?.title == "Original Title")
        #expect(updatedNotification?.body == "New body")
        #expect(updatedNotification?.subtitle == "Original subtitle")
        #expect(updatedNotification?.buttons.count == 1)
        #expect(updatedNotification?.buttons.first?.title == "Only")
    }

    @Test
    func updateRejectsWhenBrowserReportsError() async throws {
        let backgroundScript = """
            await browser.notifications.create('note', { type: 'basic', title: 'T', message: 'M' })
            await browser.test.assertRejects(browser.notifications.update('note', { message: 'New body' }), /notifications\\.update/i)
            browser.test.notifyPass()
            """

        let manager = manager(for: backgroundScript)

        manager.internalDelegate.presentNotification = { _ in }

        try await manager.loadAndRun()
    }

    @Test
    func clearReturnsFalseForUnknownIdentifier() async throws {
        let backgroundScript = """
            const wasCleared = await browser.notifications.clear('does-not-exist')
            browser.test.assertFalse(wasCleared, 'clear() should resolve false when no notification matches')
            browser.test.notifyPass()
            """

        let manager = manager(for: backgroundScript)

        var clearDelegateCalled = false
        manager.internalDelegate.clearNotification = { _ in
            clearDelegateCalled = true
        }

        try await manager.loadAndRun()

        #expect(!clearDelegateCalled)
    }

    @Test
    func clearRemovesNotificationThroughDelegate() async throws {
        let backgroundScript = """
            await browser.notifications.create('note', { type: 'basic', title: 'T', message: 'M' })
            const wasCleared = await browser.notifications.clear('note')
            browser.test.assertTrue(wasCleared, 'clear() should resolve true for an existing notification')
            const all = await browser.notifications.getAll()
            browser.test.assertFalse('note' in all, 'cleared notification should no longer appear in getAll()')
            browser.test.notifyPass()
            """

        let manager = manager(for: backgroundScript)

        manager.internalDelegate.presentNotification = { _ in }

        var clearedNotification: _WKWebExtensionNotification?
        manager.internalDelegate.clearNotification = { notification in
            clearedNotification = notification
        }

        try await manager.loadAndRun()

        #expect(clearedNotification != nil)
        #expect(clearedNotification?.identifier == "note")
    }

    @Test
    func clearRejectsWhenBrowserReportsError() async throws {
        let backgroundScript = """
            await browser.notifications.create('note', { type: 'basic', title: 'T', message: 'M' })
            await browser.test.assertRejects(browser.notifications.clear('note'), /notifications\\.clear/i)
            browser.test.notifyPass()
            """

        let manager = manager(for: backgroundScript)

        manager.internalDelegate.presentNotification = { _ in }

        try await manager.loadAndRun()
    }

    @Test
    func getAllReturnsCreatedNotificationIdentifiers() async throws {
        let backgroundScript = """
            const empty = await browser.notifications.getAll()
            browser.test.assertEq(Object.keys(empty).length, 0, 'getAll() should be empty before any notifications are created')
            await browser.notifications.create('first', { type: 'basic', title: 'T', message: 'M' })
            await browser.notifications.create('second', { type: 'basic', title: 'T', message: 'M' })
            const all = await browser.notifications.getAll()
            browser.test.assertEq(all['first'], true, 'getAll() should map created identifiers to true')
            browser.test.assertEq(all['second'], true, 'getAll() should map created identifiers to true')
            browser.test.assertEq(Object.keys(all).length, 2, 'getAll() should contain exactly the created notifications')
            browser.test.notifyPass()
            """

        let manager = manager(for: backgroundScript)

        manager.internalDelegate.presentNotification = { _ in }

        try await manager.loadAndRun()
    }

    @Test
    func getPermissionLevelReturnsGrantedWhenDelegateAllows() async throws {
        let backgroundScript = """
            const level = await browser.notifications.getPermissionLevel()
            browser.test.assertEq(level, 'granted')
            browser.test.notifyPass()
            """

        let manager = manager(for: backgroundScript)

        manager.internalDelegate.mayPresentNotifications = {
            true
        }

        try await manager.loadAndRun()
    }

    @Test
    func getPermissionLevelReturnsDeniedWhenDelegateDenies() async throws {
        let backgroundScript = """
            const level = await browser.notifications.getPermissionLevel()
            browser.test.assertEq(level, 'denied')
            browser.test.notifyPass()
            """

        let manager = manager(for: backgroundScript)

        manager.internalDelegate.mayPresentNotifications = {
            false
        }

        try await manager.loadAndRun()
    }

    @Test
    func getPermissionLevelRejectsWhenBrowserReportsError() async throws {
        let backgroundScript = """
            await browser.test.assertRejects(browser.notifications.getPermissionLevel(), /notifications\\.getPermissionLevel/i)
            browser.test.notifyPass()
            """

        try await loadAndRun(backgroundScript: backgroundScript)
    }

    @Test
    func clickedEvent() async throws {
        let backgroundScript = """
            browser.notifications.onClicked.addListener((identifier) => {
              browser.test.assertEq(identifier, 'note')
              browser.test.notifyPass()
            })
            await browser.notifications.create('note', { type: 'basic', title: 'T', message: 'M' })
            browser.test.sendMessage('Created')
            """

        let manager = manager(for: backgroundScript)

        manager.internalDelegate.presentNotification = { _ in }

        manager.load()
        try await manager.waitForTestMessage("Created")

        let context = try #require(manager.context)
        context._didClickNotification(withIdentifier: "note")

        try await manager.run()
    }

    @Test
    func buttonClickedEvent() async throws {
        let backgroundScript = """
            browser.notifications.onButtonClicked.addListener((identifier, buttonIndex) => {
              browser.test.assertEq(identifier, 'note')
              browser.test.assertEq(buttonIndex, 1)
              browser.test.notifyPass()
            })
            await browser.notifications.create('note', { type: 'basic', title: 'T', message: 'M', buttons: [ { title: 'First' }, { title: 'Second' } ] })
            browser.test.sendMessage('Created')
            """

        let manager = manager(for: backgroundScript)

        manager.internalDelegate.presentNotification = { _ in }

        manager.load()
        try await manager.waitForTestMessage("Created")

        let context = try #require(manager.context)
        context._didClickButton(at: 1, forNotificationWithIdentifier: "note")

        try await manager.run()
    }

    @Test
    func closedEventRemovesNotification() async throws {
        let backgroundScript = """
            browser.notifications.onClosed.addListener(async (identifier, byUser) => {
              browser.test.assertEq(identifier, 'note')
              browser.test.assertTrue(byUser)
              const all = await browser.notifications.getAll()
              browser.test.assertFalse('note' in all, 'closed notification should no longer appear in getAll()')
              browser.test.notifyPass()
            })
            await browser.notifications.create('note', { type: 'basic', title: 'T', message: 'M' })
            browser.test.sendMessage('Created')
            """

        let manager = manager(for: backgroundScript)

        manager.internalDelegate.presentNotification = { _ in }

        manager.load()
        try await manager.waitForTestMessage("Created")

        let context = try #require(manager.context)
        context._didCloseNotification(withIdentifier: "note", byUser: true)

        try await manager.run()
    }

    @Test
    func eventsIgnoreUntrackedNotifications() async throws {
        let backgroundScript = """
            browser.notifications.onClicked.addListener(() => browser.test.notifyFail('onClicked should not fire'))
            browser.notifications.onButtonClicked.addListener(() => browser.test.notifyFail('onButtonClicked should not fire'))
            browser.notifications.onClosed.addListener(() => browser.test.notifyFail('onClosed should not fire'))
            browser.test.onMessage.addListener((message) => {
              if (message === 'Finish')
                browser.test.notifyPass()
            })
            await browser.notifications.create('note', { type: 'basic', title: 'T', message: 'M', buttons: [ { title: 'Only' } ] })
            await browser.notifications.create('cleared', { type: 'basic', title: 'T', message: 'M' })
            await browser.notifications.clear('cleared')
            browser.test.sendMessage('Ready')
            """

        let manager = manager(for: backgroundScript)

        manager.internalDelegate.presentNotification = { _ in }
        manager.internalDelegate.clearNotification = { _ in }

        manager.load()
        try await manager.waitForTestMessage("Ready")

        let context = try #require(manager.context)
        context._didClickNotification(withIdentifier: "does-not-exist")
        context._didClickButton(at: 0, forNotificationWithIdentifier: "does-not-exist")
        context._didClickButton(at: 1, forNotificationWithIdentifier: "note")
        context._didCloseNotification(withIdentifier: "cleared", byUser: true)
        context._didCloseNotification(withIdentifier: "does-not-exist", byUser: false)

        manager.sendTestMessage("Finish")

        try await manager.run()
    }

    @Test
    func clickedEventWakesBackgroundContent() async throws {
        let backgroundScript = """
            browser.notifications.onClicked.addListener((identifier) => {
              browser.test.assertEq(identifier, 'note')
              browser.test.notifyPass()
            })
            const all = await browser.notifications.getAll()
            if (!('note' in all)) {
              await browser.notifications.create('note', { type: 'basic', title: 'T', message: 'M' })
              browser.test.sendMessage('Created')
            }
            """

        let manager = manager(for: backgroundScript)

        manager.internalDelegate.presentNotification = { _ in }

        manager.load()
        try await manager.waitForTestMessage("Created")

        let context = try #require(manager.context)
        context._unloadBackgroundContentForTesting()

        try await waitForCondition("background web view to unload", timeout: .seconds(10)) {
            context._backgroundWebView == nil
        }

        context._didClickNotification(withIdentifier: "note")

        try await manager.run()
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS_NOTIFICATIONS
