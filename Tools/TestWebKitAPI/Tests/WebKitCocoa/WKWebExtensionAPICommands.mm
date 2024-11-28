/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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
#import <WebKit/WKWebExtensionCommandPrivate.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

#if USE(APPKIT)
#import <Carbon/Carbon.h>
#endif

namespace TestWebKitAPI {

static auto *commandsManifest = @{
    @"manifest_version": @3,

    @"name": @"Test Commands",
    @"description": @"Test Commands",
    @"version": @"1.0",

    @"permissions": @[ @"webNavigation" ],

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
            }
        },
        @"test-command": @{
            @"suggested_key": @{
                @"default": @"Command+Alt+Z",
                @"mac": @"Command+Alt+Z"
            },
            @"description": @"Test Command"
        }
    }
};

static auto *emptyCommandsManifest = @{
    @"manifest_version": @3,

    @"name": @"Test Commands",
    @"description": @"Test Commands",
    @"version": @"1.0",

    @"permissions": @[ @"webNavigation" ],

    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"type": @"module",
        @"persistent": @NO,
    },

    @"action": @{
        @"default_title": @"Test Action"
    },

    @"commands": @{
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
        @"browser.test.assertEq(executeActionCommand.description, 'Test Action', 'The description should be')",
        @"browser.test.assertEq(executeActionCommand.shortcut, 'MacCtrl+Shift+Y', 'The shortcut should be')",

        @"browser.test.assertTrue(!!testCommand, 'test-command command should exist')",
        @"browser.test.assertEq(testCommand.description, 'Test Command', 'The description should be')",
        @"browser.test.assertEq(testCommand.shortcut, 'Alt+Command+Z', 'The shortcut should be')",

        @"browser.test.notifyPass()",
    ]);

    Util::loadAndRunExtension(commandsManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPICommands, GetAllCommandsEmptyManifest)
{
    auto *backgroundScript = Util::constructScript(@[
        @"let commands = await browser.commands.getAll()",
        @"browser.test.assertEq(commands.length, 1, 'Should be one command.')",

        @"let executeActionCommand = commands.find(command => command.name === '_execute_action')",

        @"browser.test.assertTrue(!!executeActionCommand, '_execute_action command should exist')",
        @"browser.test.assertEq(executeActionCommand.description, 'Test Action', 'The description should be')",

        @"browser.test.notifyPass()",
    ]);

    Util::loadAndRunExtension(emptyCommandsManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPICommands, CommandEvent)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.commands.onCommand.addListener((command, tab) => {",
        @"  browser.test.assertEq(command, 'test-command', 'The command should be test-command')",
        @"  browser.test.assertEq(typeof tab, 'object', 'The tab should be an object')",
        @"  browser.test.assertEq(typeof tab.id, 'number', 'The tab object should have an id property')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Perform Command')"
    ]);

    auto manager = Util::loadAndRunExtension(commandsManifest, @{ @"background.js": backgroundScript });

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Perform Command");

    auto *predicate = [NSPredicate predicateWithFormat:@"identifier == %@", @"test-command"];
    auto *filteredCommands = [manager.get().context.commands filteredArrayUsingPredicate:predicate];
    EXPECT_EQ(filteredCommands.count, 1lu);

    [manager.get().context performCommand:filteredCommands.firstObject];

    [manager run];
}

#if USE(APPKIT)

TEST(WKWebExtensionAPICommands, CommandForEvent)
{
    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:commandsManifest resources:@{ }]);
    auto context = adoptNS([[WKWebExtensionContext alloc] initForExtension:extension.get()]);

    auto *keyCommandEvent = [NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:(NSEventModifierFlagCommand | NSEventModifierFlagOption)
        timestamp:0 windowNumber:0 context:nil characters:@"Ω" charactersIgnoringModifiers:@"z" isARepeat:NO keyCode:kVK_ANSI_Z];
    auto *command = [context commandForEvent:keyCommandEvent];

    EXPECT_NOT_NULL(command);
    EXPECT_NS_EQUAL(command.identifier, @"test-command");
    EXPECT_NS_EQUAL(command._userVisibleShortcut, @"⌥⌘Z");
    EXPECT_FALSE(command._isActionCommand);

    keyCommandEvent = [NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:(NSEventModifierFlagControl | NSEventModifierFlagShift)
        timestamp:0 windowNumber:0 context:nil characters:@"Á" charactersIgnoringModifiers:@"y" isARepeat:NO keyCode:kVK_ANSI_A];
    command = [context commandForEvent:keyCommandEvent];

    EXPECT_NOT_NULL(command);
    EXPECT_NS_EQUAL(command.identifier, @"_execute_action");
    EXPECT_NS_EQUAL(command._userVisibleShortcut, @"⌃⇧Y");
    EXPECT_TRUE(command._isActionCommand);

    keyCommandEvent = [NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:(NSEventModifierFlagCommand | NSEventModifierFlagOption)
        timestamp:0 windowNumber:0 context:nil characters:@"å" charactersIgnoringModifiers:@"a" isARepeat:NO keyCode:kVK_ANSI_A];
    command = [context commandForEvent:keyCommandEvent];

    EXPECT_NULL(command);

    keyCommandEvent = [NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:NSEventModifierFlagCommand
        timestamp:0 windowNumber:0 context:nil characters:@"Ω" charactersIgnoringModifiers:@"z" isARepeat:NO keyCode:kVK_ANSI_A];
    command = [context commandForEvent:keyCommandEvent];

    EXPECT_NULL(command);

    keyCommandEvent = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:0 context:nil eventNumber:0 clickCount:0 pressure:0];
    command = [context commandForEvent:keyCommandEvent];

    EXPECT_NULL(command);
}

TEST(WKWebExtensionAPICommands, PerformCommandForEvent)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.commands.onCommand.addListener((command, tab) => {",
        @"  browser.test.assertEq(command, 'test-command', 'The command should be')",
        @"  browser.test.assertEq(typeof tab, 'object', 'The tab should be')",
        @"  browser.test.assertEq(typeof tab.id, 'number', 'The tab.id object should be')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Test Command Event')"
    ]);

    auto manager = Util::loadAndRunExtension(commandsManifest, @{ @"background.js": backgroundScript });

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Test Command Event");

    auto *keyCommandEvent = [NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:(NSEventModifierFlagCommand | NSEventModifierFlagOption)
        timestamp:0 windowNumber:0 context:nil characters:@"Ω" charactersIgnoringModifiers:@"z" isARepeat:NO keyCode:kVK_ANSI_Z];

    [manager.get().context performCommandForEvent:keyCommandEvent];

    [manager run];
}

#endif // USE(APPKIT)

#if PLATFORM(IOS_FAMILY)

TEST(WKWebExtensionAPICommands, PerformKeyCommand)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.commands.onCommand.addListener((command, tab) => {",
        @"  browser.test.assertEq(command, 'test-command', 'The command should be')",
        @"  browser.test.assertEq(typeof tab, 'object', 'The tab should be')",
        @"  browser.test.assertEq(typeof tab.id, 'number', 'The tab.id object should be')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Test Command Event')"
    ]);

    auto manager = Util::loadAndRunExtension(commandsManifest, @{ @"background.js": backgroundScript });

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Test Command Event");

    auto *predicate = [NSPredicate predicateWithFormat:@"identifier == %@", @"test-command"];
    auto *filteredCommands = [manager.get().context.commands filteredArrayUsingPredicate:predicate];
    EXPECT_EQ(filteredCommands.count, 1lu);

    auto *command = filteredCommands.firstObject;

    [manager.get().context performCommandForKeyCommand:command.keyCommand];

    [manager run];
}

#endif // PLATFORM(IOS_FAMILY)

TEST(WKWebExtensionAPICommands, PerformMenuItem)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.commands.onCommand.addListener((command, tab) => {",
        @"  browser.test.assertEq(command, 'test-command', 'The command should be')",
        @"  browser.test.assertEq(typeof tab, 'object', 'The tab should be')",
        @"  browser.test.assertEq(typeof tab.id, 'number', 'The tab.id object should be')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Test Command Event')"
    ]);

    auto manager = Util::loadAndRunExtension(commandsManifest, @{ @"background.js": backgroundScript });

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Test Command Event");

    auto *predicate = [NSPredicate predicateWithFormat:@"identifier == %@", @"test-command"];
    auto *filteredCommands = [manager.get().context.commands filteredArrayUsingPredicate:predicate];
    EXPECT_EQ(filteredCommands.count, 1lu);

    auto *command = filteredCommands.firstObject;
    auto *menuItem = command.menuItem;

#if USE(APPKIT)
    [menuItem.target performSelector:menuItem.action withObject:nil];
#else
    [dynamic_objc_cast<UIAction>(menuItem) performWithSender:nil target:nil];
#endif

    [manager run];
}

TEST(WKWebExtensionAPICommands, ExecuteActionCommand)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.commands.onCommand.addListener((command, tab) => {",
        @"  browser.test.notifyFail('The action command should not fire onCommand')",
        @"})",

        @"browser.action.onClicked.addListener((tab) => {",
        @"  browser.test.assertEq(typeof tab, 'object', 'The tab should be an object')",
        @"  browser.test.assertEq(typeof tab.id, 'number', 'The tab object should have an id property')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Test Execute Action Command')"
    ]);

    auto manager = Util::loadAndRunExtension(commandsManifest, @{ @"background.js": backgroundScript });

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
        @"  browser.test.assertEq(typeof changeInfo, 'object', 'The change should be an object')",
        @"  browser.test.assertEq(changeInfo.name, 'test-command', 'The name should be')",
        @"  browser.test.assertEq(changeInfo.oldShortcut, 'Alt+Command+Z', 'The old shortcut should be')",
        @"  browser.test.assertEq(changeInfo.newShortcut, 'Command+N', 'The new shortcut should be')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Test Command Shortcut Change')"
    ]);

    auto manager = Util::loadAndRunExtension(commandsManifest, @{ @"background.js": backgroundScript });

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

TEST(WKWebExtensionAPICommands, PerformCommandAndPermissionsRequest)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.commands.onCommand.addListener(async (tab) => {",
        @"  try {",
        @"    const result = await browser.permissions.request({ 'permissions': [ 'webNavigation' ] })",
        @"    if (result)",
        @"      browser.test.notifyPass()",
        @"    else",
        @"      browser.test.notifyFail('Permissions request was rejected')",
        @"  } catch (error) {",
        @"    browser.test.notifyFail('Permissions request failed')",
        @"  }",
        @"})",

        @"browser.test.yield('Perform Command')"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:commandsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.promptForPermissions = ^(id<WKWebExtensionTab> tab, NSSet<NSString *> *requestedPermissions, void (^callback)(NSSet<NSString *> *, NSDate *)) {
        EXPECT_EQ(requestedPermissions.count, 1lu);
        EXPECT_TRUE([requestedPermissions isEqualToSet:[NSSet setWithObject:@"webNavigation"]]);
        callback(requestedPermissions, nil);
    };

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Perform Command");

    auto *predicate = [NSPredicate predicateWithFormat:@"identifier == %@", @"test-command"];
    auto *filteredCommands = [manager.get().context.commands filteredArrayUsingPredicate:predicate];
    EXPECT_EQ(filteredCommands.count, 1lu);

    [manager.get().context performCommand:filteredCommands.firstObject];

    [manager run];
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
