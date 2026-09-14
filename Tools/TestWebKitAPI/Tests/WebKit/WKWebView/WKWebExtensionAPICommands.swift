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
private import WebKit_Private.WKWebExtensionCommandPrivate

import struct Swift.String

#if USE_APPKIT
private import AppKit
private import Carbon
#else
private import UIKit
#endif

@MainActor
struct WKWebExtensionAPICommandsTests {
    private let commandsManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "Test Commands",
        "description": "Test Commands",
        "version": "1.0",

        "permissions": ["webNavigation"],

        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],

        "action": [
            "default_title": "Test Action"
        ],

        "commands": [
            "_execute_action": [
                "suggested_key": [
                    "default": "Ctrl+Shift+Y",
                    "mac": "MacCtrl+Shift+Y",
                ]
            ],
            "test-command": [
                "suggested_key": [
                    "default": "Command+Alt+Z",
                    "mac": "Command+Alt+Z",
                ],
                "description": "Test Command",
            ],
        ],
    ]

    private let emptyCommandsManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "Test Commands",
        "description": "Test Commands",
        "version": "1.0",

        "permissions": ["webNavigation"],

        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],

        "action": [
            "default_title": "Test Action"
        ],

        "commands": [:],
    ]

    /// The extension's single command with this identifier, failing if it is not there exactly once.
    private func command(
        _ identifier: String,
        in context: WKWebExtensionContext,
        sourceLocation: SourceLocation = #_sourceLocation
    ) throws -> WKWebExtension.Command {
        let matches = context.commands.filter { $0.id == identifier }
        #expect(matches.count == 1, sourceLocation: sourceLocation)
        return try #require(matches.first, sourceLocation: sourceLocation)
    }

    @Test
    func getAllCommands() async throws {
        let backgroundScript = """
            let commands = await browser.commands.getAll()
            browser.test.assertEq(commands.length, 2, 'Should be two commands.')

            let executeActionCommand = commands.find(command => command.name === '_execute_action')
            let testCommand = commands.find(command => command.name === 'test-command')

            browser.test.assertTrue(!!executeActionCommand, '_execute_action command should exist')
            browser.test.assertEq(executeActionCommand.description, 'Test Action', 'The description should be')
            browser.test.assertEq(executeActionCommand.shortcut, 'MacCtrl+Shift+Y', 'The shortcut should be')

            browser.test.assertTrue(!!testCommand, 'test-command command should exist')
            browser.test.assertEq(testCommand.description, 'Test Command', 'The description should be')
            browser.test.assertEq(testCommand.shortcut, 'Alt+Command+Z', 'The shortcut should be')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: commandsManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func getAllCommandsEmptyManifest() async throws {
        let backgroundScript = """
            let commands = await browser.commands.getAll()
            browser.test.assertEq(commands.length, 1, 'Should be one command.')

            let executeActionCommand = commands.find(command => command.name === '_execute_action')

            browser.test.assertTrue(!!executeActionCommand, '_execute_action command should exist')
            browser.test.assertEq(executeActionCommand.description, 'Test Action', 'The description should be')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(
            manifest: emptyCommandsManifest,
            resources: ["background.js": backgroundScript]
        )
        try await manager.run()
    }

    @Test
    func getAllCommandsEmptyManifestNoActionName() async throws {
        let emptyCommandsNoActionNameManifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test Commands",
            "description": "Test Commands",
            "version": "1.0",

            "permissions": ["webNavigation"],

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "action": [:],

            "commands": [:],
        ]

        let backgroundScript = """
            let commands = await browser.commands.getAll()
            browser.test.assertEq(commands.length, 1, 'Should be one command.')

            let executeActionCommand = commands.find(command => command.name === '_execute_action')

            browser.test.assertTrue(!!executeActionCommand, '_execute_action command should exist')
            browser.test.assertEq(executeActionCommand.description, 'Test Commands', 'The description should be')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(
            manifest: emptyCommandsNoActionNameManifest,
            resources: ["background.js": backgroundScript]
        )
        try await manager.run()
    }

    @Test
    func commandEvent() async throws {
        let backgroundScript = """
            browser.commands.onCommand.addListener((command, tab) => {
              browser.test.assertEq(command, 'test-command', 'The command should be test-command')
              browser.test.assertEq(typeof tab, 'object', 'The tab should be an object')
              browser.test.assertEq(typeof tab.id, 'number', 'The tab object should have an id property')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Perform Command')
            """

        let manager = try loadWebExtension(manifest: commandsManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        try await manager.waitForTestMessage("Perform Command")

        context.performCommand(try command("test-command", in: context))

        try await manager.run()
    }

    #if USE_APPKIT

    @Test
    func commandForEvent() throws {
        let manager = parseWebExtension(manifest: commandsManifest)
        let context = try #require(manager.context)

        let optionCommandZ = try #require(
            NSEvent.keyEvent(
                with: .keyDown,
                location: .zero,
                modifierFlags: [.command, .option],
                timestamp: 0,
                windowNumber: 0,
                context: nil,
                characters: "Ω",
                charactersIgnoringModifiers: "z",
                isARepeat: false,
                keyCode: UInt16(kVK_ANSI_Z)
            )
        )

        let testCommand = try #require(context.command(for: optionCommandZ))
        #expect(testCommand.id == "test-command")
        #expect(testCommand._userVisibleShortcut == "⌥⌘Z")
        #expect(!testCommand._isActionCommand)

        let controlShiftY = try #require(
            NSEvent.keyEvent(
                with: .keyDown,
                location: .zero,
                modifierFlags: [.control, .shift],
                timestamp: 0,
                windowNumber: 0,
                context: nil,
                characters: "Á",
                charactersIgnoringModifiers: "y",
                isARepeat: false,
                keyCode: UInt16(kVK_ANSI_A)
            )
        )

        let actionCommand = try #require(context.command(for: controlShiftY))
        #expect(actionCommand.id == "_execute_action")
        #expect(actionCommand._userVisibleShortcut == "⌃⇧Y")
        #expect(actionCommand._isActionCommand)

        let optionCommandA = try #require(
            NSEvent.keyEvent(
                with: .keyDown,
                location: .zero,
                modifierFlags: [.command, .option],
                timestamp: 0,
                windowNumber: 0,
                context: nil,
                characters: "å",
                charactersIgnoringModifiers: "a",
                isARepeat: false,
                keyCode: UInt16(kVK_ANSI_A)
            )
        )

        #expect(context.command(for: optionCommandA) == nil)

        // The characters match test-command's shortcut, but the key code does not.
        let commandZWrongKeyCode = try #require(
            NSEvent.keyEvent(
                with: .keyDown,
                location: .zero,
                modifierFlags: .command,
                timestamp: 0,
                windowNumber: 0,
                context: nil,
                characters: "Ω",
                charactersIgnoringModifiers: "z",
                isARepeat: false,
                keyCode: UInt16(kVK_ANSI_A)
            )
        )

        #expect(context.command(for: commandZWrongKeyCode) == nil)

        let mouseEvent = try #require(
            NSEvent.mouseEvent(
                with: .leftMouseDown,
                location: .zero,
                modifierFlags: [],
                timestamp: 0,
                windowNumber: 0,
                context: nil,
                eventNumber: 0,
                clickCount: 0,
                pressure: 0
            )
        )

        #expect(context.command(for: mouseEvent) == nil)
    }

    @Test
    func performCommandForEvent() async throws {
        let backgroundScript = """
            browser.commands.onCommand.addListener((command, tab) => {
              browser.test.assertEq(command, 'test-command', 'The command should be')
              browser.test.assertEq(typeof tab, 'object', 'The tab should be')
              browser.test.assertEq(typeof tab.id, 'number', 'The tab.id object should be')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Test Command Event')
            """

        let manager = try loadWebExtension(manifest: commandsManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        try await manager.waitForTestMessage("Test Command Event")

        let optionCommandZ = try #require(
            NSEvent.keyEvent(
                with: .keyDown,
                location: .zero,
                modifierFlags: [.command, .option],
                timestamp: 0,
                windowNumber: 0,
                context: nil,
                characters: "Ω",
                charactersIgnoringModifiers: "z",
                isARepeat: false,
                keyCode: UInt16(kVK_ANSI_Z)
            )
        )

        #expect(context.performCommand(for: optionCommandZ))

        try await manager.run()
    }

    #endif // USE_APPKIT

    #if WTF_PLATFORM_IOS_FAMILY

    @Test
    func performKeyCommand() async throws {
        let backgroundScript = """
            browser.commands.onCommand.addListener((command, tab) => {
              browser.test.assertEq(command, 'test-command', 'The command should be')
              browser.test.assertEq(typeof tab, 'object', 'The tab should be')
              browser.test.assertEq(typeof tab.id, 'number', 'The tab.id object should be')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Test Command Event')
            """

        let manager = try loadWebExtension(manifest: commandsManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        try await manager.waitForTestMessage("Test Command Event")

        let keyCommand = try #require(try command("test-command", in: context).keyCommand)
        #expect(context.performCommand(for: keyCommand))

        try await manager.run()
    }

    #endif // WTF_PLATFORM_IOS_FAMILY

    @Test
    func performMenuItem() async throws {
        let backgroundScript = """
            browser.commands.onCommand.addListener((command, tab) => {
              browser.test.assertEq(command, 'test-command', 'The command should be')
              browser.test.assertEq(typeof tab, 'object', 'The tab should be')
              browser.test.assertEq(typeof tab.id, 'number', 'The tab.id object should be')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Test Command Event')
            """

        let manager = try loadWebExtension(manifest: commandsManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        try await manager.waitForTestMessage("Test Command Event")

        let menuItem = try command("test-command", in: context).menuItem

        #if USE_APPKIT
        let target = try #require(menuItem.target as? NSObject)
        let action = try #require(menuItem.action)
        // -perform:with: returns an unretained Unmanaged, which strict memory safety rejects. The
        // menu action returns void, so there is nothing to take ownership of.
        _ = unsafe target.perform(action, with: nil)
        #else
        try #require(menuItem as? UIAction).performWithSender(nil, target: nil)
        #endif

        try await manager.run()
    }

    @Test
    func executeActionCommand() async throws {
        let backgroundScript = """
            browser.commands.onCommand.addListener((command, tab) => {
              browser.test.notifyFail('The action command should not fire onCommand')
            })

            browser.action.onClicked.addListener((tab) => {
              browser.test.assertEq(typeof tab, 'object', 'The tab should be an object')
              browser.test.assertEq(typeof tab.id, 'number', 'The tab object should have an id property')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Test Execute Action Command')
            """

        let manager = try loadWebExtension(manifest: commandsManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        try await manager.waitForTestMessage("Test Execute Action Command")

        context.performCommand(try command("_execute_action", in: context))

        try await manager.run()
    }

    @Test
    func changedEvent() async throws {
        let backgroundScript = """
            browser.commands.onChanged.addListener((changeInfo) => {
              browser.test.assertEq(typeof changeInfo, 'object', 'The change should be an object')
              browser.test.assertEq(changeInfo.name, 'test-command', 'The name should be')
              browser.test.assertEq(changeInfo.oldShortcut, 'Alt+Command+Z', 'The old shortcut should be')
              browser.test.assertEq(changeInfo.newShortcut, 'Command+N', 'The new shortcut should be')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Test Command Shortcut Change')
            """

        let manager = try loadWebExtension(manifest: commandsManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        try await manager.waitForTestMessage("Test Command Shortcut Change")

        let testCommand = try command("test-command", in: context)
        testCommand.activationKey = "N"
        testCommand.modifierFlags = .command

        try await manager.run()
    }

    @Test
    func performCommandAndPermissionsRequest() async throws {
        let backgroundScript = """
            browser.commands.onCommand.addListener(async (tab) => {
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

            browser.test.sendMessage('Perform Command')
            """

        let manager = try loadWebExtension(manifest: commandsManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        manager.grantRequestedPermissions()

        try await manager.waitForTestMessage("Perform Command")

        context.performCommand(try command("test-command", in: context))

        try await manager.run()
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
