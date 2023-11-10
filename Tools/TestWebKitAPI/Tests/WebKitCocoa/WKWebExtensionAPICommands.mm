/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "WebExtensionUtilities.h"
#import <WebKit/_WKWebExtensionCommand.h>

namespace TestWebKitAPI {

static auto *commandsManifest = @{
    @"manifest_version": @3,

    @"name": @"Test Commands",
    @"description": @"Test Commands",
    @"version": @"1.0",

    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"type": @"module",
        @"persistent": @NO,
    },

    @"action": @{
        @"default_title": @"Test Action"
    },

    @"commands": @{
        @"_execute_action": @{
            @"suggested_key": @{
                @"default": @"Ctrl+Shift+Y",
                @"mac": @"MacCtrl+Shift+Y"
            },
            @"description": @"Browser Action"
        },
        @"test-command": @{
            @"suggested_key": @{
                @"default": @"",
                @"mac": @"Command+Alt+Z"
            },
            @"description": @"Test Command"
        }
    }
};

TEST(WKWebExtensionAPICommands, GetAllCommands)
{
    auto *backgroundScript = Util::constructScript(@[
        @"let commands = await browser.commands.getAll()",
        @"browser.test.assertEq(commands.length, 2, 'Should be two commands.')",

        @"let executeActionCommand = commands.find(command => command.name === '_execute_action')",
        @"let testCommand = commands.find(command => command.name === 'test-command')",

        @"browser.test.assertTrue(!!executeActionCommand, '_execute_action command should exist')",
        @"browser.test.assertEq(executeActionCommand.description, 'Browser Action', 'The description should be')",
        @"browser.test.assertEq(executeActionCommand.shortcut, 'MacCtrl+Shift+Y', 'The shortcut should be')",

        @"browser.test.assertTrue(!!testCommand, 'test-command command should exist')",
        @"browser.test.assertEq(testCommand.description, 'Test Command', 'The description should be')",
        @"browser.test.assertEq(testCommand.shortcut, 'Alt+Command+Z', 'The shortcut should be')",

        @"browser.test.notifyPass()",
    ]);

    Util::loadAndRunExtension(commandsManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPICommands, CommandEvent)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.commands.onCommand.addListener((command, tab) => {",
        @"  browser.test.assertEq(command, 'test-command', 'The command should be test-command')",
        @"  browser.test.assertTrue(typeof tab === 'object', 'The tab should be an object')",
        @"  browser.test.assertEq(typeof tab.id, 'number', 'The tab object should have an id property')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Perform Command')"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:commandsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Perform Command");

    auto *predicate = [NSPredicate predicateWithFormat:@"identifier == %@", @"test-command"];
    auto *filteredCommands = [manager.get().context.commands filteredArrayUsingPredicate:predicate];
    EXPECT_EQ(filteredCommands.count, 1lu);

    [manager.get().context performCommand:filteredCommands.firstObject];

    [manager run];
}

TEST(WKWebExtensionAPICommands, ExecuteActionCommand)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.commands.onCommand.addListener((command, tab) => {",
        @"  browser.test.notifyFail('The action command should not fire onCommand')",
        @"})",

        @"browser.action.onClicked.addListener((tab) => {",
        @"  browser.test.assertTrue(typeof tab === 'object', 'The tab should be an object')",
        @"  browser.test.assertEq(typeof tab.id, 'number', 'The tab object should have an id property')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Test Execute Action Command')"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:commandsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Test Execute Action Command");

    auto *predicate = [NSPredicate predicateWithFormat:@"identifier == %@", @"_execute_action"];
    auto *filteredCommands = [manager.get().context.commands filteredArrayUsingPredicate:predicate];
    EXPECT_EQ(filteredCommands.count, 1lu);

    [manager.get().context performCommand:filteredCommands.firstObject];

    [manager run];
}

TEST(WKWebExtensionAPICommands, ChangedEvent)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.commands.onChanged.addListener((changeInfo) => {",
        @"  browser.test.assertTrue(typeof changeInfo === 'object', 'The change should be an object')",
        @"  browser.test.assertEq(changeInfo.name, 'test-command', 'The name should be')",
        @"  browser.test.assertEq(changeInfo.oldShortcut, 'Alt+Command+Z', 'The old shortcut should be')",
        @"  browser.test.assertEq(changeInfo.newShortcut, 'Command+N', 'The new shortcut should be')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Test Command Shortcut Change')"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:commandsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];
    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Test Command Shortcut Change");

    auto *predicate = [NSPredicate predicateWithFormat:@"identifier == %@", @"test-command"];
    auto *filteredCommands = [manager.get().context.commands filteredArrayUsingPredicate:predicate];
    EXPECT_EQ(filteredCommands.count, 1lu);

    auto *command = filteredCommands.firstObject;

    command.activationKey = @"N";
#if PLATFORM(MAC)
    command.modifierFlags = NSEventModifierFlagCommand;
#else
    command.modifierFlags = UIKeyModifierCommand;
#endif

    [manager run];
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
