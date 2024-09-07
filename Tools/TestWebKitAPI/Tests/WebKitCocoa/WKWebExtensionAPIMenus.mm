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

#import "HTTPServer.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import "TestWebExtensionsDelegate.h"
#import "WebExtensionUtilities.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>

namespace TestWebKitAPI {

static auto *menusManifest = @{
    @"manifest_version": @3,

    @"name": @"Menus Test",
    @"description": @"Menus Test",
    @"version": @"1",

    @"permissions": @[ @"menus", @"contextMenus", @"activeTab" ],

    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"type": @"module",
        @"persistent": @NO,
    },

    @"commands": @{ },

    @"action": @{
        @"default_title": @"Test Action",
        @"default_icon": @{
            @"16": @"toolbar-16.png",
            @"32": @"toolbar-32.png",
        },
    },
};

TEST(WKWebExtensionAPIMenus, Errors)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertThrows(() => browser.menus.create({ id: { }, title: 'Test' }), /'id' is expected to be a string or a number, but an object was provided/i)",
        @"browser.test.assertThrows(() => browser.menus.create({ id: '', title: 'Test' }), /'id' value is invalid, because it must not be empty/i)",
        @"browser.test.assertThrows(() => browser.menus.create({ id: NaN, title: 'Test' }), /'id' is expected to be a string or a number, but NaN was provided/i)",
        @"browser.test.assertThrows(() => browser.menus.create({ id: Infinity, title: 'Test' }), /'id' is expected to be a string or a number, but Infinity was provided/i)",

        @"browser.test.assertThrows(() => browser.menus.create({ title: 123 }), /'title' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.menus.create({ title: '' }), /'title' value is invalid, because it must not be empty/i)",

        @"browser.test.assertThrows(() => browser.menus.create({ contexts: 'all', title: 'Test' }), /'contexts' is expected to be an array of strings, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.menus.create({ contexts: [ 42 ], title: 'Test' }), /'contexts' is expected to be an array of strings, but a number was provided in the array/i)",
        @"browser.test.assertThrows(() => browser.menus.create({ contexts: [ NaN ], title: 'Test' }), /'contexts' is expected to be an array of strings, but NaN was provided in the array/i)",
        @"browser.test.assertThrows(() => browser.menus.create({ contexts: [ Infinity ], title: 'Test' }), /'contexts' is expected to be an array of strings, but Infinity was provided in the array/i)",

        @"browser.test.assertThrows(() => browser.menus.create({ parentId: { }, title: 'Test' }), /'parentId' is expected to be a string or a number, but an object was provided/i)",
        @"browser.test.assertThrows(() => browser.menus.create({ parentId: '', title: 'Test' }), /'parentId' value is invalid, because it must not be empty/i)",
        @"browser.test.assertThrows(() => browser.menus.create({ parentId: NaN, title: 'Test' }), /'parentId' is expected to be a string or a number, but NaN was provided/i)",
        @"browser.test.assertThrows(() => browser.menus.create({ parentId: Infinity, title: 'Test' }), /'parentId' is expected to be a string or a number, but Infinity was provided/i)",

        @"browser.test.assertThrows(() => browser.menus.create({ documentUrlPatterns: 'https://*.example.com', title: 'Test' }), /'documentUrlPatterns' is expected to be an array of strings, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.menus.create({ documentUrlPatterns: [ 'bad' ], title: 'Test' }), /'documentUrlPatterns' value is invalid, because 'bad' is not a valid pattern/i)",
        @"browser.test.assertThrows(() => browser.menus.create({ documentUrlPatterns: [ 'foo://*/*' ], title: 'Test' }), /'documentUrlPatterns' value is invalid, because '.*' is not a valid pattern/i)",

        @"browser.test.assertThrows(() => browser.menus.create({ targetUrlPatterns: 'https://*.example.com', title: 'Test' }), /'targetUrlPatterns' is expected to be an array of strings, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.menus.create({ targetUrlPatterns: [ 'bad' ], title: 'Test' }), /'targetUrlPatterns' value is invalid, because 'bad' is not a valid pattern/i)",

        @"browser.test.assertThrows(() => browser.menus.create({ type: 123, title: 'Test' }), /'type' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.menus.create({ type: 'bad', title: 'Test' }), /'type' value is invalid, because it must specify either 'normal', 'checkbox', 'radio', or 'separator'/i)",

        @"browser.test.assertThrows(() => browser.menus.create({ command: 123, title: 'Test' }), /'command' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.menus.create({ command: '', title: 'Test' }), /'command' value is invalid, because it must not be empty/i)",

        @"browser.test.assertThrows(() => browser.menus.create({ checked: 'bad', title: 'Test' }), /'checked' is expected to be a boolean, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.menus.create({ visible: 'bad', title: 'Test' }), /'visible' is expected to be a boolean, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.menus.create({ enabled: 'bad', title: 'Test' }), /'enabled' is expected to be a boolean, but a string was provided/i)",

        @"browser.test.assertThrows(() => browser.menus.create({ icons: 123, title: 'Test' }), /'icons' is expected to be a string or an object or null, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.menus.create({ icons: { 16: 123 }, title: 'Test' }), /'icons\\[16]' value is invalid, because a string is expected, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.menus.create({ icons: { '16': 123 }, title: 'Test' }), /'icons\\[16]' value is invalid, because a string is expected, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.menus.create({ icons: { '1.2': 'test.png' }, title: 'Test' }), /'icons' value is invalid, because '1.2' is not a valid dimension/i)",

        @"browser.test.assertThrows(() => browser.menus.create({ onclick: 'bad', title: 'Test' }), /'onclick' is expected to be a value, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.menus.create({ onclick: { }, title: 'Test' }), /'onclick' is expected to be a value, but an object was provided/i)",
        @"browser.test.assertThrows(() => browser.menus.create({ onclick: new RegExp(), title: 'Test' }), /'onclick' value is invalid, because it must be a function/i)",

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(menusManifest, @{ @"background.js": backgroundScript });
}

#if USE(APPKIT)
using CocoaMenuItem = NSMenuItem;
using CocoaMenuAction = NSMenuItem;
constexpr auto CocoaMenuItemStateOn = NSControlStateValueOn;
constexpr auto CocoaMenuItemStateOff = NSControlStateValueOff;
#else
using CocoaMenuItem = UIMenuElement;
using CocoaMenuAction = UIAction;
constexpr auto CocoaMenuItemStateOn = UIMenuElementStateOn;
constexpr auto CocoaMenuItemStateOff = UIMenuElementStateOff;
#endif

static inline void performMenuItemAction(auto *menuItem)
{
#if USE(APPKIT)
    [menuItem.target performSelector:menuItem.action withObject:nil];
#else
    [dynamic_objc_cast<CocoaMenuAction>(menuItem) performWithSender:nil target:nil];
#endif
}

TEST(WKWebExtensionAPIMenus, MenuCreateWithVariousIds)
{
    auto *backgroundScript = Util::constructScript(@[
        @"let menuItemWithStringId = browser.menus.create({",
        @"  id: 'string-id',",
        @"  title: 'String ID',",
        @"  contexts: [ 'all' ]",
        @"})",

        @"browser.test.assertEq(typeof menuItemWithStringId, 'string')",
        @"browser.test.assertEq(menuItemWithStringId, 'string-id')",

        @"let menuItemWithNumericId = browser.menus.create({",
        @"  id: 12345,",
        @"  title: 'Numeric ID',",
        @"  contexts: [ 'all' ]",
        @"})",

        @"browser.test.assertEq(typeof menuItemWithNumericId, 'number')",
        @"browser.test.assertEq(menuItemWithNumericId, 12345)",

        @"let menuItemWithOmittedId = browser.menus.create({",
        @"  title: 'Omitted ID',",
        @"  contexts: [ 'all' ]",
        @"})",

        @"function validateUUIDv4(uuid) {",
        @"  let regex = /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[8|9|a|b][0-9a-f]{3}-[0-9a-f]{12}$/i",
        @"  return regex.test(uuid)",
        @"}",

        @"browser.test.assertEq(typeof menuItemWithOmittedId, 'string')",
        @"browser.test.assertTrue(validateUUIDv4(menuItemWithOmittedId), 'ID should be a UUID v4 string')",

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(menusManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIMenus, ActionMenus)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.menus.create({",
        @"  id: 'top-level-1',",
        @"  title: 'Top Level 1',",
        @"  contexts: [ 'action' ]",
        @"})",

        @"browser.menus.create({",
        @"  id: 'top-level-2',",
        @"  title: 'Top Level 2',",
        @"  contexts: [ 'page' ]",
        @"})",

        @"browser.menus.create({",
        @"  id: 'top-level-3',",
        @"  title: 'Top Level 3',",
        @"  contexts: [ 'action', 'page' ],",
        @"  documentUrlPatterns: ['*://localhost/*']",
        @"})",

        @"browser.menus.create({",
        @"  id: 'top-level-4',",
        @"  title: 'Top Level 4'",
        @"})",

        @"browser.menus.onClicked.addListener((info, tab) => {",
        @"  browser.test.assertEq(typeof info, 'object')",
        @"  browser.test.assertEq(typeof tab, 'object')",

        @"  browser.test.assertEq(info.menuItemId, 'top-level-1')",
        @"  browser.test.assertEq(info.parentMenuItemId, undefined)",

        @"  browser.test.assertEq(typeof tab.id, 'number')",
        @"  browser.test.assertEq(typeof tab.windowId, 'number')",
        @"  browser.test.assertEq(tab.url, '')",
        @"  browser.test.assertEq(tab.title, '')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Menus Created')"
    ]);

    auto manager = Util::loadAndRunExtension(menusManifest, @{ @"background.js": backgroundScript });

    // Reset activeTab, WKWebExtensionAPIMenus.ActionMenusWithActiveTab tests that.
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusUnknown forPermission:WKWebExtensionPermissionActiveTab];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Created");

    [manager.get().defaultTab.webView loadRequest:server.requestWithLocalhost()];
    [manager runForTimeInterval:1];

    auto *action = [manager.get().context actionForTab:manager.get().defaultTab];
    auto *menuItems = action.menuItems;

    auto *expectedTitles = @[ @"Top Level 1", @"Top Level 3" ];
    EXPECT_EQ(menuItems.count, expectedTitles.count);

    [menuItems enumerateObjectsUsingBlock:^(CocoaMenuItem *menuItem, NSUInteger index, BOOL *stop) {
        EXPECT_TRUE([menuItem isKindOfClass:[CocoaMenuItem class]]);
        EXPECT_NS_EQUAL(menuItem.title, expectedTitles[index]);
    }];

    performMenuItemAction(menuItems.firstObject);

    [manager run];
}

TEST(WKWebExtensionAPIMenus, ActionMenusWithActiveTab)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.menus.create({",
        @"  id: 'top-level-1',",
        @"  title: 'Top Level 1',",
        @"  contexts: [ 'action' ]",
        @"})",

        @"browser.menus.create({",
        @"  id: 'top-level-2',",
        @"  title: 'Top Level 2',",
        @"  contexts: [ 'page' ]",
        @"})",

        @"browser.menus.create({",
        @"  id: 'top-level-3',",
        @"  title: 'Top Level 3',",
        @"  contexts: [ 'action', 'page' ]",
        @"})",

        @"browser.menus.create({",
        @"  id: 'top-level-4',",
        @"  title: 'Top Level 4'",
        @"})",

        @"browser.menus.onClicked.addListener((info, tab) => {",
        @"  browser.test.assertEq(typeof info, 'object')",
        @"  browser.test.assertEq(typeof tab, 'object')",

        @"  browser.test.assertEq(info.menuItemId, 'top-level-1')",
        @"  browser.test.assertEq(info.parentMenuItemId, undefined)",

        @"  browser.test.assertEq(typeof tab.id, 'number')",
        @"  browser.test.assertEq(typeof tab.windowId, 'number')",
        @"  browser.test.assertEq(typeof tab.url, 'string')",
        @"  browser.test.assertTrue(tab.url.startsWith('http://localhost:'))",
        @"  browser.test.assertEq(typeof tab.title, 'string')",
        @"  browser.test.assertTrue(tab.title.length > 0)",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Menus Created')"
    ]);

    auto manager = Util::loadAndRunExtension(menusManifest, @{ @"background.js": backgroundScript });

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Created");

    EXPECT_FALSE([manager.get().context hasActiveUserGestureInTab:manager.get().defaultTab]);

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<title>Test</title>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    [manager.get().defaultTab.webView loadRequest:server.requestWithLocalhost()];
    [manager runForTimeInterval:1];

    auto *action = [manager.get().context actionForTab:manager.get().defaultTab];
    auto *menuItems = action.menuItems;

    auto *expectedTitles = @[ @"Top Level 1", @"Top Level 3" ];
    EXPECT_EQ(menuItems.count, expectedTitles.count);

    [menuItems enumerateObjectsUsingBlock:^(CocoaMenuItem *menuItem, NSUInteger index, BOOL *stop) {
        EXPECT_TRUE([menuItem isKindOfClass:[CocoaMenuItem class]]);
        EXPECT_NS_EQUAL(menuItem.title, expectedTitles[index]);
    }];

    performMenuItemAction(menuItems.firstObject);

    [manager run];

    EXPECT_TRUE([manager.get().context hasActiveUserGestureInTab:manager.get().defaultTab]);
}

TEST(WKWebExtensionAPIMenus, ActionSubmenus)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.menus.create({",
        @"  id: 'top-level-1',",
        @"  title: 'Top Level 1',",
        @"  contexts: [ 'action' ]",
        @"})",

        @"browser.menus.create({",
        @"  id: 'submenu-1',",
        @"  parentId: 'top-level-1',",
        @"  title: 'Submenu 1'",
        @"})",

        @"browser.menus.create({",
        @"  id: 'submenu-2',",
        @"  parentId: 'top-level-1',",
        @"  title: 'Submenu 2',",
        @"  contexts: [ 'action' ]",
        @"})",

        @"browser.menus.create({",
        @"  id: 'submenu-excluded',",
        @"  parentId: 'top-level-1',",
        @"  title: 'Excluded Submenu',",
        @"  contexts: [ 'page' ]",
        @"})",

        @"browser.menus.onClicked.addListener((info, tab) => {",
        @"  browser.test.assertEq(typeof info, 'object')",
        @"  browser.test.assertEq(typeof tab, 'object')",

        @"  browser.test.assertEq(info.menuItemId, 'submenu-1')",
        @"  browser.test.assertEq(info.parentMenuItemId, 'top-level-1')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Menus Created')",
    ]);

    auto manager = Util::loadAndRunExtension(menusManifest, @{ @"background.js": backgroundScript });

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Created");

    auto *action = [manager.get().context actionForTab:manager.get().defaultTab];
    auto *menuItems = action.menuItems;

    EXPECT_EQ(menuItems.count, 1lu);

    auto *topLevelMenuItem = menuItems.firstObject;
    EXPECT_TRUE([topLevelMenuItem isKindOfClass:[CocoaMenuItem class]]);
    EXPECT_NS_EQUAL(topLevelMenuItem.title, @"Top Level 1");

    auto *expectedSubmenuTitles = @[ @"Submenu 1", @"Submenu 2" ];

#if USE(APPKIT)
    EXPECT_EQ(topLevelMenuItem.submenu.itemArray.count, expectedSubmenuTitles.count);

    [topLevelMenuItem.submenu.itemArray enumerateObjectsUsingBlock:^(CocoaMenuItem *submenuItem, NSUInteger index, BOOL *stop) {
        EXPECT_TRUE([submenuItem isKindOfClass:[CocoaMenuItem class]]);
        EXPECT_NS_EQUAL(submenuItem.title, expectedSubmenuTitles[index]);
    }];

    auto *submenuItem = topLevelMenuItem.submenu.itemArray.firstObject;
#else
    auto *parentMenu = dynamic_objc_cast<UIMenu>(topLevelMenuItem);

    EXPECT_EQ(parentMenu.children.count, expectedSubmenuTitles.count);
    [parentMenu.children enumerateObjectsUsingBlock:^(CocoaMenuItem *action, NSUInteger index, BOOL *stop) {
        EXPECT_TRUE([action isKindOfClass:[CocoaMenuItem class]]);
        EXPECT_NS_EQUAL(action.title, expectedSubmenuTitles[index]);
    }];

    auto *submenuItem = parentMenu.children.firstObject;
#endif

    performMenuItemAction(submenuItem);

    [manager run];
}

TEST(WKWebExtensionAPIMenus, ActionSubmenusUpdate)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.menus.create({",
        @"  id: 'top-level-1',",
        @"  title: 'Top Level 1',",
        @"  contexts: [ 'action' ]",
        @"})",

        @"browser.menus.create({",
        @"  id: 'top-level-2',",
        @"  title: 'Top Level 2',",
        @"  contexts: [ 'tab' ]",
        @"})",

        @"browser.menus.create({",
        @"  id: 'submenu-1',",
        @"  parentId: 'top-level-2',",
        @"  title: 'Submenu 1'",
        @"})",

        @"browser.menus.create({",
        @"  id: 'submenu-2',",
        @"  parentId: 'top-level-2',",
        @"  title: 'Submenu 2',",
        @"  contexts: [ 'action' ]",
        @"})",

        @"browser.menus.create({",
        @"  id: 'submenu-excluded',",
        @"  parentId: 'top-level-2',",
        @"  title: 'Excluded Submenu'",
        @"})",

        @"browser.menus.update('submenu-1', {",
        @"  parentId: 'top-level-1',",
        @"  contexts: [ ]",
        @"})",

        @"browser.menus.update('submenu-2', {",
        @"  parentId: 'top-level-1',",
        @"  contexts: [ 'action' ]",
        @"})",

        @"browser.menus.update('submenu-excluded', {",
        @"  parentId: 'top-level-1',",
        @"})",

        @"browser.menus.onClicked.addListener((info, tab) => {",
        @"  browser.test.assertEq(typeof info, 'object')",
        @"  browser.test.assertEq(typeof tab, 'object')",

        @"  browser.test.assertEq(info.menuItemId, 'submenu-1')",
        @"  browser.test.assertEq(info.parentMenuItemId, 'top-level-1')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Menus Created')",
    ]);

    auto manager = Util::loadAndRunExtension(menusManifest, @{ @"background.js": backgroundScript });

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Created");

    auto *action = [manager.get().context actionForTab:manager.get().defaultTab];
    auto *menuItems = action.menuItems;

    EXPECT_EQ(menuItems.count, 1lu);

    auto *topLevelMenuItem = menuItems.firstObject;
    EXPECT_TRUE([topLevelMenuItem isKindOfClass:[CocoaMenuItem class]]);
    EXPECT_NS_EQUAL(topLevelMenuItem.title, @"Top Level 1");

    auto *expectedSubmenuTitles = @[ @"Submenu 1", @"Submenu 2" ];

#if USE(APPKIT)
    EXPECT_EQ(topLevelMenuItem.submenu.itemArray.count, expectedSubmenuTitles.count);

    [topLevelMenuItem.submenu.itemArray enumerateObjectsUsingBlock:^(CocoaMenuItem *submenuItem, NSUInteger index, BOOL *stop) {
        EXPECT_TRUE([submenuItem isKindOfClass:[CocoaMenuItem class]]);
        EXPECT_NS_EQUAL(submenuItem.title, expectedSubmenuTitles[index]);
    }];

    auto *submenuItem = topLevelMenuItem.submenu.itemArray.firstObject;
#else
    auto *parentMenu = dynamic_objc_cast<UIMenu>(topLevelMenuItem);

    EXPECT_EQ(parentMenu.children.count, expectedSubmenuTitles.count);
    [parentMenu.children enumerateObjectsUsingBlock:^(CocoaMenuItem *action, NSUInteger index, BOOL *stop) {
        EXPECT_TRUE([action isKindOfClass:[CocoaMenuItem class]]);
        EXPECT_NS_EQUAL(action.title, expectedSubmenuTitles[index]);
    }];

    auto *submenuItem = parentMenu.children.firstObject;
#endif

    performMenuItemAction(submenuItem);

    [manager run];
}

TEST(WKWebExtensionAPIMenus, TabMenus)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.menus.create({",
        @"  id: 'tab-item-1',",
        @"  title: 'Tab Item 1',",
        @"  contexts: [ 'tab' ]",
        @"})",

        @"browser.menus.create({",
        @"  id: 'tab-item-2',",
        @"  title: 'Tab Item 2',",
        @"  contexts: [ 'tab' ],",
        @"  documentUrlPatterns: ['*://localhost/*']",
        @"})",

        @"browser.menus.create({",
        @"  id: 'excluded-item',",
        @"  title: 'Excluded Item',",
        @"  contexts: [ 'image' ]",
        @"})",

        @"browser.menus.onClicked.addListener((info, tab) => {",
        @"  browser.test.assertEq(typeof info, 'object')",
        @"  browser.test.assertEq(typeof tab, 'object')",

        @"  browser.test.assertEq(info.menuItemId, 'tab-item-1')",
        @"  browser.test.assertEq(typeof tab.id, 'number')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Menus Created')",
    ]);

    auto manager = Util::loadAndRunExtension(menusManifest, @{ @"background.js": backgroundScript });

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Created");

    [manager.get().defaultTab.webView loadRequest:server.requestWithLocalhost()];
    [manager runForTimeInterval:1];

    auto *menuItems = [manager.get().context menuItemsForTab:manager.get().defaultTab];
    EXPECT_EQ(menuItems.count, 1lu);

#if USE(APPKIT)
    menuItems = menuItems.firstObject.submenu.itemArray;
#else
    auto *parentMenu = dynamic_objc_cast<UIMenu>(menuItems.firstObject);
    menuItems = parentMenu.children;
#endif

    EXPECT_EQ(menuItems.count, 2lu);

    auto *expectedTitles = @[ @"Tab Item 1", @"Tab Item 2" ];
    EXPECT_EQ(menuItems.count, expectedTitles.count);

    [menuItems enumerateObjectsUsingBlock:^(CocoaMenuItem *menuItem, NSUInteger index, BOOL *stop) {
        EXPECT_TRUE([menuItem isKindOfClass:[CocoaMenuItem class]]);
        EXPECT_NS_EQUAL(menuItem.title, expectedTitles[index]);
    }];

    performMenuItemAction(menuItems.firstObject);

    [manager run];
}

TEST(WKWebExtensionAPIMenus, MenuItemProperties)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.menus.create({",
        @"  id: 'menu-item-properties',",
        @"  title: 'Menu &Item with Ampersand && More',",
        @"  type: 'checkbox',",
        @"  checked: true,",
        @"  contexts: [ 'all' ],",
        @"  enabled: false,",
        @"  visible: false,",
        @"  icons: { 16: 'icon-16.png', 20: 'icon-20.png' }",
        @"})",

        @"browser.menus.create({",
        @"  id: 'menu-item-command',",
        @"  title: 'Menu Item with Command',",
        @"  command: '_execute_action',",
        @"  contexts: [ 'all' ]",
        @"})",

        @"browser.action.onClicked.addListener((tab) => {",
        @"  browser.test.assertEq(typeof tab, 'object')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Menus Created')",
    ]);

    auto *smallIcon = Util::makePNGData(CGSizeMake(16, 16), @selector(greenColor));
    auto *largerIcon = Util::makePNGData(CGSizeMake(20, 20), @selector(blueColor));

    auto *resources = @{
        @"background.js": backgroundScript,
        @"icon-16.png": smallIcon,
        @"icon-20.png": largerIcon,
    };

    auto manager = Util::loadAndRunExtension(menusManifest, resources);

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Created");

    auto *menuItems = [manager.get().context menuItemsForTab:manager.get().defaultTab];
    EXPECT_EQ(menuItems.count, 1lu);

#if USE(APPKIT)
    menuItems = menuItems.firstObject.submenu.itemArray;
#else
    auto *parentMenu = dynamic_objc_cast<UIMenu>(menuItems.firstObject);
    menuItems = parentMenu.children;
#endif

    EXPECT_EQ(menuItems.count, 2lu);

    auto *firstMenuItem = dynamic_objc_cast<CocoaMenuAction>(menuItems.firstObject);
    EXPECT_TRUE([firstMenuItem isKindOfClass:[CocoaMenuItem class]]);
    EXPECT_EQ(firstMenuItem.state, CocoaMenuItemStateOn);
    EXPECT_NS_EQUAL(firstMenuItem.title, @"Menu Item with Ampersand & More");
    EXPECT_NOT_NULL(firstMenuItem.image);

#if USE(APPKIT)
    EXPECT_FALSE(firstMenuItem.enabled);
    EXPECT_TRUE(firstMenuItem.hidden);
    EXPECT_TRUE(CGSizeEqualToSize(firstMenuItem.image.size, CGSizeMake(16, 16)));
#else
    EXPECT_EQ(firstMenuItem.attributes, (UIMenuElementAttributesDisabled | UIMenuElementAttributesHidden));
    EXPECT_TRUE(CGSizeEqualToSize(firstMenuItem.image.size, CGSizeMake(20, 20)));
#endif

    auto *secondMenuItem = dynamic_objc_cast<CocoaMenuAction>(menuItems.lastObject);
    EXPECT_TRUE([secondMenuItem isKindOfClass:[CocoaMenuItem class]]);
    EXPECT_NS_EQUAL(secondMenuItem.title, @"Menu Item with Command");

    performMenuItemAction(secondMenuItem);

    [manager run];
}

TEST(WKWebExtensionAPIMenus, MenuItemPropertiesUpdate)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.menus.create({",
        @"  id: 'menu-item-properties',",
        @"  title: 'Old Title',",
        @"})",

        @"browser.menus.create({",
        @"  id: 'menu-item-command',",
        @"  title: 'Old Command Title',",
        @"})",

        @"browser.menus.update('menu-item-properties', {",
        @"  id: 'menu-item-properties-new',",
        @"  title: 'Menu &Item with Ampersand && More',",
        @"  type: 'checkbox',",
        @"  checked: true,",
        @"  contexts: [ 'all' ],",
        @"  enabled: false,",
        @"  visible: false,",
        @"  icons: { 16: 'icon-16.png', 20: 'icon-20.png' }",
        @"})",

        @"browser.menus.update('menu-item-command', {",
        @"  id: 'menu-item-command-new',",
        @"  title: 'Menu Item with Command',",
        @"  command: '_execute_action',",
        @"  contexts: [ 'all' ]",
        @"})",

        @"browser.action.onClicked.addListener((tab) => {",
        @"  browser.test.assertEq(typeof tab, 'object')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Menus Created')",
    ]);

    auto *smallIcon = Util::makePNGData(CGSizeMake(16, 16), @selector(greenColor));
    auto *largerIcon = Util::makePNGData(CGSizeMake(20, 20), @selector(blueColor));

    auto *resources = @{
        @"background.js": backgroundScript,
        @"icon-16.png": smallIcon,
        @"icon-20.png": largerIcon,
    };

    auto manager = Util::loadAndRunExtension(menusManifest, resources);

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Created");

    auto *menuItems = [manager.get().context menuItemsForTab:manager.get().defaultTab];
    EXPECT_EQ(menuItems.count, 1lu);

#if USE(APPKIT)
    menuItems = menuItems.firstObject.submenu.itemArray;
#else
    auto *parentMenu = dynamic_objc_cast<UIMenu>(menuItems.firstObject);
    menuItems = parentMenu.children;
#endif

    EXPECT_EQ(menuItems.count, 2lu);

    auto *firstMenuItem = dynamic_objc_cast<CocoaMenuAction>(menuItems.firstObject);
    EXPECT_TRUE([firstMenuItem isKindOfClass:[CocoaMenuItem class]]);
    EXPECT_EQ(firstMenuItem.state, CocoaMenuItemStateOn);
    EXPECT_NS_EQUAL(firstMenuItem.title, @"Menu Item with Ampersand & More");
    EXPECT_NOT_NULL(firstMenuItem.image);

#if USE(APPKIT)
    EXPECT_FALSE(firstMenuItem.enabled);
    EXPECT_TRUE(firstMenuItem.hidden);
    EXPECT_TRUE(CGSizeEqualToSize(firstMenuItem.image.size, CGSizeMake(16, 16)));
#else
    EXPECT_EQ(firstMenuItem.attributes, (UIMenuElementAttributesDisabled | UIMenuElementAttributesHidden));
    EXPECT_TRUE(CGSizeEqualToSize(firstMenuItem.image.size, CGSizeMake(20, 20)));
#endif

    auto *secondMenuItem = dynamic_objc_cast<CocoaMenuAction>(menuItems.lastObject);
    EXPECT_TRUE([secondMenuItem isKindOfClass:[CocoaMenuItem class]]);
    EXPECT_NS_EQUAL(secondMenuItem.title, @"Menu Item with Command");

    performMenuItemAction(secondMenuItem);

    [manager run];
}

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
TEST(WKWebExtensionAPIMenus, MenuItemWithIconVariants)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertSafe(() => browser.menus.create({",
        @"  id: 'menu-item-with-icon-variants',",
        @"  title: 'Menu Item with Icon Variants',",
        @"  icon_variants: [",
        @"    { 16: 'icon-dark-16.png', 'color_schemes': [ 'dark' ] },",
        @"    { 16: 'icon-light-16.png', 'color_schemes': [ 'light' ] }",
        @"  ],",
        @"  contexts: [ 'action' ]",
        @"}))",

        @"browser.test.yield('Menus Created')",
    ]);

    auto *darkIcon16 = Util::makePNGData(CGSizeMake(16, 16), @selector(whiteColor));
    auto *lightIcon16 = Util::makePNGData(CGSizeMake(16, 16), @selector(blackColor));

    auto *resources = @{
        @"background.js": backgroundScript,
        @"icon-dark-16.png": darkIcon16,
        @"icon-light-16.png": lightIcon16,
    };

    auto manager = Util::loadAndRunExtension(menusManifest, resources);

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Created");

    auto *action = [manager.get().context actionForTab:manager.get().defaultTab];
    auto *menuItems = action.menuItems;

    EXPECT_EQ(menuItems.count, 1lu);

    auto *menuItem = dynamic_objc_cast<CocoaMenuAction>(menuItems.firstObject);
    EXPECT_TRUE([menuItem isKindOfClass:[CocoaMenuItem class]]);
    EXPECT_NS_EQUAL(menuItem.title, @"Menu Item with Icon Variants");

#if USE(APPKIT)
    EXPECT_TRUE(CGSizeEqualToSize(menuItem.image.size, CGSizeMake(16, 16)));
#else
    EXPECT_TRUE(CGSizeEqualToSize(menuItem.image.size, CGSizeMake(20, 20)));
#endif

    Util::performWithAppearance(Util::Appearance::Dark, ^{
        EXPECT_TRUE(Util::compareColors(Util::pixelColor(menuItem.image), [CocoaColor whiteColor]));
    });

    Util::performWithAppearance(Util::Appearance::Light, ^{
        EXPECT_TRUE(Util::compareColors(Util::pixelColor(menuItem.image), [CocoaColor blackColor]));
    });
}

TEST(WKWebExtensionAPIMenus, MenuItemWithImageDataVariants)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const createImageData = (size, color) => {",
        @"  const context = new OffscreenCanvas(size, size).getContext('2d')",
        @"  context.fillStyle = color",
        @"  context.fillRect(0, 0, size, size)",

        @"  return context.getImageData(0, 0, size, size)",
        @"}",

        @"const darkImageData = createImageData(16, 'white')",
        @"const lightImageData = createImageData(16, 'black')",

        @"browser.test.assertSafe(() => browser.menus.create({",
        @"  id: 'menu-item-with-image-data-variants',",
        @"  title: 'Menu Item with ImageData Variants',",
        @"  icon_variants: [",
        @"    { 16: darkImageData, 'color_schemes': [ 'dark' ] },",
        @"    { 16: lightImageData, 'color_schemes': [ 'light' ] }",
        @"  ],",
        @"  contexts: [ 'action' ]",
        @"}))",

        @"browser.test.yield('Menus Created')",
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
    };

    auto manager = Util::loadAndRunExtension(menusManifest, resources);

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Created");

    auto *action = [manager.get().context actionForTab:manager.get().defaultTab];
    auto *menuItems = action.menuItems;

    EXPECT_EQ(menuItems.count, 1lu);

    auto *menuItem = dynamic_objc_cast<CocoaMenuAction>(menuItems.firstObject);
    EXPECT_TRUE([menuItem isKindOfClass:[CocoaMenuItem class]]);
    EXPECT_NS_EQUAL(menuItem.title, @"Menu Item with ImageData Variants");

#if USE(APPKIT)
    EXPECT_TRUE(CGSizeEqualToSize(menuItem.image.size, CGSizeMake(16, 16)));
#else
    EXPECT_TRUE(CGSizeEqualToSize(menuItem.image.size, CGSizeMake(20, 20)));
#endif

    Util::performWithAppearance(Util::Appearance::Dark, ^{
        EXPECT_TRUE(Util::compareColors(Util::pixelColor(menuItem.image), [CocoaColor whiteColor]));
    });

    Util::performWithAppearance(Util::Appearance::Light, ^{
        EXPECT_TRUE(Util::compareColors(Util::pixelColor(menuItem.image), [CocoaColor blackColor]));
    });
}

TEST(WKWebExtensionAPIMenus, MenuItemWithWithNoValidVariants)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const createImageData = (size, color) => {",
        @"  const context = new OffscreenCanvas(size, size).getContext('2d')",
        @"  context.fillStyle = color",
        @"  context.fillRect(0, 0, size, size)",

        @"  return context.getImageData(0, 0, size, size)",
        @"}",

        @"const validImageData = createImageData(16, 'white')",

        @"await browser.test.assertThrows(() => browser.menus.create({",
        @"    id: 'submenu-item-invalid-dimension',",
        @"    parentId: 'top-level-item',",
        @"    title: 'Submenu with Invalid Dimension Key',",
        @"    icon_variants: [",
        @"      { 'sixteen': validImageData, 'color_schemes': [ 'light' ] }",
        @"    ],",
        @"    contexts: [ 'action' ]",
        @"}), /'icon_variants\\[0\\]' value is invalid, because 'sixteen' is not a valid dimension/)",

        @"await browser.test.assertThrows(() => browser.menus.create({",
        @"    id: 'submenu-item-invalid-color-scheme',",
        @"    parentId: 'top-level-item',",
        @"    title: 'Submenu with Invalid Color Scheme',",
        @"    icon_variants: [",
        @"      { '16': validImageData, 'color_schemes': [ 'bad' ] }",
        @"    ],",
        @"    contexts: [ 'action' ]",
        @"}), /'icon_variants\\[0\\]\\['color_schemes'\\]' value is invalid, because it must specify either 'light' or 'dark'/)",

        @"browser.test.notifyPass()"
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
    };

    Util::loadAndRunExtension(menusManifest, resources);
}

TEST(WKWebExtensionAPIMenus, MenuItemWithMixedValidAndInvalidIconVariants)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const createImageData = (size, color) => {",
        @"  const context = new OffscreenCanvas(size, size).getContext('2d')",
        @"  context.fillStyle = color",
        @"  context.fillRect(0, 0, size, size)",

        @"  return context.getImageData(0, 0, size, size)",
        @"}",

        @"const validImageData = createImageData(16, 'black')",
        @"const invalidImageData = createImageData(16, 'white')",

        @"browser.test.assertSafe(() => browser.menus.create({",
        @"  id: 'menu-item-mixed',",
        @"  title: 'Menu Item with Mixed Variants',",
        @"  icon_variants: [",
        @"    { 'sixteen': invalidImageData, 'color_schemes': [ 'dark' ] },",
        @"    { '16': validImageData, 'color_schemes': [ 'light' ] }",
        @"  ],",
        @"  contexts: [ 'action' ]",
        @"}))",

        @"browser.test.yield('Menus Created')",
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
    };

    auto manager = Util::loadAndRunExtension(menusManifest, resources);

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Created");

    auto *action = [manager.get().context actionForTab:manager.get().defaultTab];
    auto *menuItems = action.menuItems;

    EXPECT_EQ(menuItems.count, 1lu);

    auto *menuItem = dynamic_objc_cast<CocoaMenuAction>(menuItems.firstObject);
    EXPECT_TRUE([menuItem isKindOfClass:[CocoaMenuItem class]]);
    EXPECT_NS_EQUAL(menuItem.title, @"Menu Item with Mixed Variants");

#if USE(APPKIT)
    EXPECT_TRUE(CGSizeEqualToSize(menuItem.image.size, CGSizeMake(16, 16)));
#else
    EXPECT_TRUE(CGSizeEqualToSize(menuItem.image.size, CGSizeMake(20, 20)));
#endif

    Util::performWithAppearance(Util::Appearance::Light, ^{
        EXPECT_TRUE(Util::compareColors(Util::pixelColor(menuItem.image), [CocoaColor blackColor]));
    });

    Util::performWithAppearance(Util::Appearance::Dark, ^{
        // Should still be black, as light variant is used.
        EXPECT_TRUE(Util::compareColors(Util::pixelColor(menuItem.image), [CocoaColor blackColor]));
    });
}

TEST(WKWebExtensionAPIMenus, MenuItemWithAnySizeVariantAndSVGDataURL)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const whiteSVGData = 'data:image/svg+xml;base64,' + btoa(`",
        @"  <svg width=\"100\" height=\"100\" xmlns=\"http://www.w3.org/2000/svg\">",
        @"    <rect width=\"100\" height=\"100\" fill=\"white\" />",
        @"  </svg>`)",

        @"const blackSVGData = 'data:image/svg+xml;base64,' + btoa(`",
        @"  <svg width=\"100\" height=\"100\" xmlns=\"http://www.w3.org/2000/svg\">",
        @"    <rect width=\"100\" height=\"100\" fill=\"black\" />",
        @"  </svg>`)",

        @"browser.test.assertSafe(() => browser.menus.create({",
        @"  id: 'menu-item-with-svg-variants',",
        @"  title: 'Menu Item with SVG Icon Variants',",
        @"  icon_variants: [",
        @"    { any: whiteSVGData, 'color_schemes': [ 'dark' ] },",
        @"    { any: blackSVGData, 'color_schemes': [ 'light' ] }",
        @"  ],",
        @"  contexts: [ 'all' ]",
        @"}))",

        @"browser.test.yield('Menus Created')"
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
    };

    auto manager = Util::loadAndRunExtension(menusManifest, resources);

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Created");

    auto *action = [manager.get().context actionForTab:manager.get().defaultTab];
    auto *menuItems = action.menuItems;

    EXPECT_EQ(menuItems.count, 1lu);

    auto *menuItem = dynamic_objc_cast<CocoaMenuAction>(menuItems.firstObject);
    EXPECT_TRUE([menuItem isKindOfClass:[CocoaMenuItem class]]);
    EXPECT_NS_EQUAL(menuItem.title, @"Menu Item with SVG Icon Variants");

#if USE(APPKIT)
    EXPECT_TRUE(CGSizeEqualToSize(menuItem.image.size, CGSizeMake(16, 16)));
#else
    EXPECT_TRUE(CGSizeEqualToSize(menuItem.image.size, CGSizeMake(20, 20)));
#endif

    Util::performWithAppearance(Util::Appearance::Dark, ^{
        EXPECT_TRUE(Util::compareColors(Util::pixelColor(menuItem.image), [CocoaColor whiteColor]));
    });

    Util::performWithAppearance(Util::Appearance::Light, ^{
        EXPECT_TRUE(Util::compareColors(Util::pixelColor(menuItem.image), [CocoaColor blackColor]));
    });
}

TEST(WKWebExtensionAPIMenus, UpdateMenuItemWithIconVariants)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertSafe(() => browser.menus.create({",
        @"  id: 'menu-item-without-icon-variants',",
        @"  title: 'Menu Item without Icon Variants',",
        @"  contexts: [ 'action' ]",
        @"}))",

        @"browser.test.assertSafe(() => browser.menus.update('menu-item-without-icon-variants', {",
        @"  title: 'Menu Item with Icon Variants',",
        @"  icon_variants: [",
        @"    { 16: 'icon-dark-16.png', 'color_schemes': [ 'dark' ] },",
        @"    { 16: 'icon-light-16.png', 'color_schemes': [ 'light' ] }",
        @"  ]",
        @"}))",

        @"browser.test.yield('Menus Updated')",
    ]);

    auto *darkIcon16 = Util::makePNGData(CGSizeMake(16, 16), @selector(whiteColor));
    auto *lightIcon16 = Util::makePNGData(CGSizeMake(16, 16), @selector(blackColor));

    auto *resources = @{
        @"background.js": backgroundScript,
        @"icon-dark-16.png": darkIcon16,
        @"icon-light-16.png": lightIcon16,
    };

    auto manager = Util::loadAndRunExtension(menusManifest, resources);

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Updated");

    auto *action = [manager.get().context actionForTab:manager.get().defaultTab];
    auto *menuItems = action.menuItems;

    EXPECT_EQ(menuItems.count, 1lu);

    auto *menuItem = dynamic_objc_cast<CocoaMenuAction>(menuItems.firstObject);
    EXPECT_TRUE([menuItem isKindOfClass:[CocoaMenuItem class]]);
    EXPECT_NS_EQUAL(menuItem.title, @"Menu Item with Icon Variants");

#if USE(APPKIT)
    EXPECT_TRUE(CGSizeEqualToSize(menuItem.image.size, CGSizeMake(16, 16)));
#else
    EXPECT_TRUE(CGSizeEqualToSize(menuItem.image.size, CGSizeMake(20, 20)));
#endif

    Util::performWithAppearance(Util::Appearance::Dark, ^{
        EXPECT_TRUE(Util::compareColors(Util::pixelColor(menuItem.image), [CocoaColor whiteColor]));
    });

    Util::performWithAppearance(Util::Appearance::Light, ^{
        EXPECT_TRUE(Util::compareColors(Util::pixelColor(menuItem.image), [CocoaColor blackColor]));
    });
}

TEST(WKWebExtensionAPIMenus, ClearMenuItemIconVariantsWithNull)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertSafe(() => browser.menus.create({",
        @"  id: 'menu-item-with-icon-variants',",
        @"  title: 'Menu Item with Icon Variants',",
        @"  icon_variants: [",
        @"    { 16: 'icon-dark-16.png', 'color_schemes': [ 'dark' ] },",
        @"    { 16: 'icon-light-16.png', 'color_schemes': [ 'light' ] }",
        @"  ],",
        @"  contexts: [ 'action' ]",
        @"}))",

        @"browser.test.assertSafe(() => browser.menus.update('menu-item-with-icon-variants', {",
        @"  icon_variants: null,",
        @"  title: 'Menu Item without Icon Variants'",
        @"}))",

        @"browser.test.yield('Menus Updated')",
    ]);

    auto *darkIcon16 = Util::makePNGData(CGSizeMake(16, 16), @selector(whiteColor));
    auto *lightIcon16 = Util::makePNGData(CGSizeMake(16, 16), @selector(blackColor));

    auto *resources = @{
        @"background.js": backgroundScript,
        @"icon-dark-16.png": darkIcon16,
        @"icon-light-16.png": lightIcon16,
    };

    auto manager = Util::loadAndRunExtension(menusManifest, resources);

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Updated");

    auto *action = [manager.get().context actionForTab:manager.get().defaultTab];
    auto *menuItems = action.menuItems;

    EXPECT_EQ(menuItems.count, 1lu);

    auto *menuItem = dynamic_objc_cast<CocoaMenuAction>(menuItems.firstObject);
    EXPECT_TRUE([menuItem isKindOfClass:[CocoaMenuItem class]]);
    EXPECT_NS_EQUAL(menuItem.title, @"Menu Item without Icon Variants");

    // Icon should be null after clearing.
    EXPECT_NULL(menuItem.image);
}

TEST(WKWebExtensionAPIMenus, ClearMenuItemIconVariantsWithEmpty)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertSafe(() => browser.menus.create({",
        @"  id: 'menu-item-with-icon-variants',",
        @"  title: 'Menu Item with Icon Variants',",
        @"  icon_variants: [",
        @"    { 16: 'icon-dark-16.png', 'color_schemes': [ 'dark' ] },",
        @"    { 16: 'icon-light-16.png', 'color_schemes': [ 'light' ] }",
        @"  ],",
        @"  contexts: [ 'action' ]",
        @"}))",

        @"browser.test.assertSafe(() => browser.menus.update('menu-item-with-icon-variants', {",
        @"  icon_variants: [ ],",
        @"  title: 'Menu Item without Icon Variants'",
        @"}))",

        @"browser.test.yield('Menus Updated')",
    ]);

    auto *darkIcon16 = Util::makePNGData(CGSizeMake(16, 16), @selector(whiteColor));
    auto *lightIcon16 = Util::makePNGData(CGSizeMake(16, 16), @selector(blackColor));

    auto *resources = @{
        @"background.js": backgroundScript,
        @"icon-dark-16.png": darkIcon16,
        @"icon-light-16.png": lightIcon16,
    };

    auto manager = Util::loadAndRunExtension(menusManifest, resources);

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Updated");

    auto *action = [manager.get().context actionForTab:manager.get().defaultTab];
    auto *menuItems = action.menuItems;

    EXPECT_EQ(menuItems.count, 1lu);

    auto *menuItem = dynamic_objc_cast<CocoaMenuAction>(menuItems.firstObject);
    EXPECT_TRUE([menuItem isKindOfClass:[CocoaMenuItem class]]);
    EXPECT_NS_EQUAL(menuItem.title, @"Menu Item without Icon Variants");

    // Icon should be null after clearing.
    EXPECT_NULL(menuItem.image);
}
#endif // ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)

TEST(WKWebExtensionAPIMenus, ToggleCheckboxMenuItems)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.menus.create({",
        @"  id: 'checkbox-1',",
        @"  title: 'Checkbox 1',",
        @"  type: 'checkbox',",
        @"  checked: true,",
        @"  contexts: [ 'all' ]",
        @"})",

        @"browser.menus.create({",
        @"  id: 'checkbox-2',",
        @"  title: 'Checkbox 2',",
        @"  type: 'checkbox',",
        @"  contexts: [ 'all' ]",
        @"})",

        @"let clickCount = 0",

        @"browser.menus.onClicked.addListener((info) => {",
        @"  if (info.menuItemId === 'checkbox-1') {",
        @"    browser.test.assertTrue(info.wasChecked)",
        @"    browser.test.assertFalse(info.checked)",
        @"  } else if (info.menuItemId === 'checkbox-2') {",
        @"    browser.test.assertFalse(info.wasChecked)",
        @"    browser.test.assertTrue(info.checked)",
        @"  }",

        @"  if (++clickCount === 2)",
        @"    browser.test.yield('Menus Clicked')",
        @"})",

        @"browser.test.yield('Menus Created')",
    ]);

    auto manager = Util::loadAndRunExtension(menusManifest, @{ @"background.js": backgroundScript });

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Created");

    auto *menuItems = [manager.get().context menuItemsForTab:manager.get().defaultTab];
    EXPECT_EQ(menuItems.count, 1lu);

#if USE(APPKIT)
    menuItems = menuItems.firstObject.submenu.itemArray;
#else
    auto *parentMenu = dynamic_objc_cast<UIMenu>(menuItems.firstObject);
    menuItems = parentMenu.children;
#endif

    EXPECT_EQ(menuItems.count, 2lu);

    auto *checkbox1 = dynamic_objc_cast<CocoaMenuAction>(menuItems.firstObject);
    auto *checkbox2 = dynamic_objc_cast<CocoaMenuAction>(menuItems.lastObject);

    EXPECT_TRUE([checkbox1 isKindOfClass:[CocoaMenuItem class]]);
    EXPECT_TRUE([checkbox2 isKindOfClass:[CocoaMenuItem class]]);
    EXPECT_EQ(checkbox1.state, CocoaMenuItemStateOn);
    EXPECT_EQ(checkbox2.state, CocoaMenuItemStateOff);

    performMenuItemAction(checkbox1);
    performMenuItemAction(checkbox2);

    [manager run];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Clicked");

    auto *updatedMenuItems = [manager.get().context menuItemsForTab:manager.get().defaultTab];
    EXPECT_EQ(updatedMenuItems.count, 1lu);

#if USE(APPKIT)
    updatedMenuItems = updatedMenuItems.firstObject.submenu.itemArray;
#else
    parentMenu = dynamic_objc_cast<UIMenu>(updatedMenuItems.firstObject);
    updatedMenuItems = parentMenu.children;
#endif

    EXPECT_EQ(updatedMenuItems.count, 2lu);

    checkbox1 = dynamic_objc_cast<CocoaMenuAction>(updatedMenuItems.firstObject);
    checkbox2 = dynamic_objc_cast<CocoaMenuAction>(updatedMenuItems.lastObject);

    EXPECT_EQ(checkbox1.state, CocoaMenuItemStateOff);
    EXPECT_EQ(checkbox2.state, CocoaMenuItemStateOn);
}

TEST(WKWebExtensionAPIMenus, RadioItemGrouping)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.menus.create({",
        @"  id: 'radio-1-group-1',",
        @"  title: 'Radio 1 Group 1',",
        @"  type: 'radio',",
        @"  checked: true,",
        @"  contexts: [ 'all' ]",
        @"})",

        @"browser.menus.create({",
        @"  id: 'radio-2-group-1',",
        @"  title: 'Radio 2 Group 1',",
        @"  type: 'radio',",
        @"  contexts: [ 'all' ]",
        @"})",

        @"browser.menus.create({",
        @"  title: 'Normal Item',",
        @"  visible: false,",
        @"  contexts: [ 'all' ]",
        @"})",

        @"browser.menus.create({",
        @"  id: 'radio-1-group-2',",
        @"  title: 'Radio 1 Group 2',",
        @"  type: 'radio',",
        @"  contexts: [ 'all' ]",
        @"})",

        @"browser.menus.create({",
        @"  id: 'radio-2-group-2',",
        @"  title: 'Radio 2 Group 2',",
        @"  type: 'radio',",
        @"  contexts: [ 'all' ]",
        @"})",

        @"browser.menus.create({",
        @"  id: 'radio-3-group-3',",
        @"  title: 'Radio 3 Group 2',",
        @"  type: 'radio',",
        @"  checked: true,",
        @"  contexts: [ 'all' ]",
        @"})",

        @"let clickCount = 0",

        @"browser.menus.onClicked.addListener((info) => {",
        @"  if (info.menuItemId === 'radio-2-group-1') {",
        @"    browser.test.assertTrue(info.wasChecked)",
        @"    browser.test.assertTrue(info.checked)",
        @"  } else if (info.menuItemId === 'radio-2-group-2') {",
        @"    browser.test.assertFalse(info.wasChecked)",
        @"    browser.test.assertTrue(info.checked)",
        @"  }",

        @"  if (++clickCount === 2)",
        @"    browser.test.yield('Menus Clicked')",
        @"})",

        @"browser.test.yield('Menus Created')",
    ]);

    auto manager = Util::loadAndRunExtension(menusManifest, @{ @"background.js": backgroundScript });

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Created");

    auto *menuItems = [manager.get().context menuItemsForTab:manager.get().defaultTab];
    EXPECT_EQ(menuItems.count, 1lu);

#if USE(APPKIT)
    menuItems = menuItems.firstObject.submenu.itemArray;
#else
    auto *parentMenu = dynamic_objc_cast<UIMenu>(menuItems.firstObject);
    menuItems = parentMenu.children;
#endif

    EXPECT_EQ(menuItems.count, 6lu);

    auto *radio1Group1 = dynamic_objc_cast<CocoaMenuAction>(menuItems[0]);
    auto *radio2Group1 = dynamic_objc_cast<CocoaMenuAction>(menuItems[1]);
    auto *radio1Group2 = dynamic_objc_cast<CocoaMenuAction>(menuItems[3]);
    auto *radio2Group2 = dynamic_objc_cast<CocoaMenuAction>(menuItems[4]);
    auto *radio3Group2 = dynamic_objc_cast<CocoaMenuAction>(menuItems[5]);

    EXPECT_EQ(radio1Group1.state, CocoaMenuItemStateOn);
    EXPECT_EQ(radio2Group1.state, CocoaMenuItemStateOff);

    EXPECT_EQ(radio1Group2.state, CocoaMenuItemStateOff);
    EXPECT_EQ(radio2Group2.state, CocoaMenuItemStateOff);
    EXPECT_EQ(radio3Group2.state, CocoaMenuItemStateOn);

    performMenuItemAction(radio1Group1);
    performMenuItemAction(radio2Group2);

    [manager run];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Clicked");

    auto *updatedMenuItems = [manager.get().context menuItemsForTab:manager.get().defaultTab];
    EXPECT_EQ(updatedMenuItems.count, 1lu);

#if USE(APPKIT)
    updatedMenuItems = updatedMenuItems.firstObject.submenu.itemArray;
#else
    parentMenu = dynamic_objc_cast<UIMenu>(updatedMenuItems.firstObject);
    updatedMenuItems = parentMenu.children;
#endif

    EXPECT_EQ(updatedMenuItems.count, 6lu);

    radio1Group1 = dynamic_objc_cast<CocoaMenuAction>(updatedMenuItems[0]);
    radio2Group1 = dynamic_objc_cast<CocoaMenuAction>(updatedMenuItems[1]);
    radio1Group2 = dynamic_objc_cast<CocoaMenuAction>(updatedMenuItems[3]);
    radio2Group2 = dynamic_objc_cast<CocoaMenuAction>(updatedMenuItems[4]);
    radio3Group2 = dynamic_objc_cast<CocoaMenuAction>(updatedMenuItems[5]);

    // The checked item as clicked again, so it does not change.
    EXPECT_EQ(radio1Group1.state, CocoaMenuItemStateOn);
    EXPECT_EQ(radio2Group1.state, CocoaMenuItemStateOff);

    // A new item was clicked, so it does change.
    EXPECT_EQ(radio1Group2.state, CocoaMenuItemStateOff);
    EXPECT_EQ(radio2Group2.state, CocoaMenuItemStateOn);
    EXPECT_EQ(radio3Group2.state, CocoaMenuItemStateOff);
}

TEST(WKWebExtensionAPIMenus, OnClick)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.menus.create({",
        @"  id: 'click-item',",
        @"  title: 'Click Item',",
        @"  contexts: ['all'],",
        @"  onclick: (info, tab) => {",
        @"    browser.test.assertEq(typeof info, 'object')",
        @"    browser.test.assertEq(typeof tab, 'object')",

        @"    browser.test.assertEq(info.menuItemId, 'click-item')",
        @"    browser.test.assertEq(typeof tab.id, 'number')",

        @"    browser.test.notifyPass()",
        @"  }",
        @"}, () => browser.test.yield('Menu Item Created'))"
    ]);

    auto manager = Util::loadAndRunExtension(menusManifest, @{ @"background.js": backgroundScript });

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menu Item Created");

    auto *menuItems = [manager.get().context menuItemsForTab:manager.get().defaultTab];
    EXPECT_EQ(menuItems.count, 1lu);

    performMenuItemAction(menuItems.firstObject);

    [manager run];
}

TEST(WKWebExtensionAPIMenus, OnClickAfterUpdate)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.menus.create({",
        @"  id: 'click-item',",
        @"  title: 'Click Item',",
        @"  contexts: ['all']",
        @"})",

        @"await browser.menus.update('click-item', {",
        @"  onclick: (info, tab) => {",
        @"    browser.test.assertEq(typeof info, 'object')",
        @"    browser.test.assertEq(typeof tab, 'object')",

        @"    browser.test.assertEq(info.menuItemId, 'click-item')",
        @"    browser.test.assertEq(typeof tab.id, 'number')",

        @"    browser.test.notifyPass()",
        @"  }",
        @"})",

        @"browser.test.yield('Menu Item Created')"
    ]);

    auto manager = Util::loadAndRunExtension(menusManifest, @{ @"background.js": backgroundScript });

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menu Item Created");

    auto *menuItems = [manager.get().context menuItemsForTab:manager.get().defaultTab];
    EXPECT_EQ(menuItems.count, 1lu);

    performMenuItemAction(menuItems.firstObject);

    [manager run];
}

TEST(WKWebExtensionAPIMenus, ContextMenusNamespace)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(typeof browser.contextMenus, 'object')",

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(menusManifest, @{ @"background.js": backgroundScript });
}

#if PLATFORM(MAC)

TEST(WKWebExtensionAPIMenus, MacContextMenuItems)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.menus.create({",
        @"  id: 'context-menu-1',",
        @"  title: 'Context Menu 1'",
        @"})",

        @"browser.menus.create({",
        @"  id: 'context-menu-2',",
        @"  title: 'Context Menu 2'",
        @"})",

        @"browser.menus.onClicked.addListener((info, tab) => {",
        @"  browser.test.assertEq(typeof info, 'object')",
        @"  browser.test.assertEq(typeof tab, 'object')",

        @"  browser.test.assertEq(info.menuItemId, 'context-menu-1')",
        @"  browser.test.assertEq(info.parentMenuItemId, undefined)",
        @"  browser.test.assertEq(info.selectionText, undefined)",
        @"  browser.test.assertEq(typeof info.pageUrl, 'string')",
        @"  browser.test.assertTrue(info.pageUrl.startsWith('http://localhost:'))",
        @"  browser.test.assertEq(info.frameId, 0)",
        @"  browser.test.assertEq(info.editable, false)",

        @"  browser.test.assertEq(typeof tab.id, 'number')",
        @"  browser.test.assertEq(typeof tab.windowId, 'number')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Menus Created')",
    ]);

    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block bool gotContextMenu = false;
    delegate.get().getContextMenuFromProposedMenu = ^(NSMenu *menu, _WKContextMenuElementInfo *elementInfo, id<NSSecureCoding>, void (^completionHandler)(NSMenu *)) {
        gotContextMenu = true;

        NSMenuItem *extensionSubmenuItem = nil;
        for (NSMenuItem *menuItem in menu.itemArray) {
            if ([menuItem.title isEqualToString:@"Menus Test"]) {
                extensionSubmenuItem = menuItem;
                break;
            }
        }

        EXPECT_NOT_NULL(extensionSubmenuItem);

        NSArray *expectedTitles = @[ @"Context Menu 1", @"Context Menu 2" ];
        NSMenu *extensionMenu = extensionSubmenuItem.submenu;
        EXPECT_EQ(extensionMenu.itemArray.count, expectedTitles.count);

        [extensionMenu.itemArray enumerateObjectsUsingBlock:^(NSMenuItem *menuItem, NSUInteger index, BOOL *stop) {
            EXPECT_TRUE([menuItem isKindOfClass:[NSMenuItem class]]);
            EXPECT_NS_EQUAL(menuItem.title, expectedTitles[index]);
        }];

        performMenuItemAction(extensionMenu.itemArray.firstObject);

        completionHandler(nil);
    };

    auto manager = Util::loadAndRunExtension(menusManifest, @{ @"background.js": backgroundScript });

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Created");

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    EXPECT_FALSE([manager.get().context hasActiveUserGestureInTab:manager.get().defaultTab]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().webExtensionController = manager.get().controller;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    webView.get().UIDelegate = delegate.get();

    manager.get().defaultTab.webView = webView.get();

    [webView synchronouslyLoadRequest:urlRequest];
    [webView waitForNextPresentationUpdate];

    [webView.get().window makeFirstResponder:webView.get()];

    [webView mouseDownAtPoint:NSMakePoint(200, 200) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(200, 200) withFlags:0 eventType:NSEventTypeRightMouseUp];

    Util::run(&gotContextMenu);

    [manager run];

    EXPECT_TRUE([manager.get().context hasActiveUserGestureInTab:manager.get().defaultTab]);
}

TEST(WKWebExtensionAPIMenus, MacActiveTabContextMenuItems)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.menus.create({",
        @"  id: 'context-menu',",
        @"  title: 'Context Menu Item'",
        @"})",

        @"browser.menus.onClicked.addListener((info, tab) => {",
        @"  browser.test.assertEq(typeof info, 'object')",
        @"  browser.test.assertEq(typeof tab, 'object')",

        @"  browser.test.assertEq(info.menuItemId, 'context-menu')",
        @"  browser.test.assertEq(info.parentMenuItemId, undefined)",
        @"  browser.test.assertEq(info.selectionText, undefined)",
        @"  browser.test.assertEq(typeof info.pageUrl, 'string')",
        @"  browser.test.assertTrue(info.pageUrl.startsWith('http://localhost:'))",
        @"  browser.test.assertEq(info.frameId, 0)",
        @"  browser.test.assertEq(info.editable, false)",

        @"  browser.test.assertEq(typeof tab.id, 'number')",
        @"  browser.test.assertEq(typeof tab.windowId, 'number')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Menus Created')",
    ]);

    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block bool gotContextMenu = false;
    delegate.get().getContextMenuFromProposedMenu = ^(NSMenu *menu, _WKContextMenuElementInfo *, id<NSSecureCoding>, void (^completionHandler)(NSMenu *)) {
        gotContextMenu = true;

        NSMenuItem *selectionMenuItem = nil;
        for (NSMenuItem *menuItem in menu.itemArray) {
            if ([menuItem.title isEqualToString:@"Context Menu Item"]) {
                selectionMenuItem = menuItem;
                break;
            }
        }

        EXPECT_NOT_NULL(selectionMenuItem);

        performMenuItemAction(selectionMenuItem);

        completionHandler(nil);
    };

    auto manager = Util::loadAndRunExtension(menusManifest, @{ @"background.js": backgroundScript });

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Created");

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    EXPECT_FALSE([manager.get().context hasActiveUserGestureInTab:manager.get().defaultTab]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().webExtensionController = manager.get().controller;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    webView.get().UIDelegate = delegate.get();

    manager.get().defaultTab.webView = webView.get();

    [webView synchronouslyLoadRequest:server.requestWithLocalhost()];
    [webView waitForNextPresentationUpdate];

    [webView.get().window makeFirstResponder:webView.get()];

    [webView mouseDownAtPoint:NSMakePoint(200, 200) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(200, 200) withFlags:0 eventType:NSEventTypeRightMouseUp];

    Util::run(&gotContextMenu);

    [manager run];

    EXPECT_TRUE([manager.get().context hasActiveUserGestureInTab:manager.get().defaultTab]);
}

TEST(WKWebExtensionAPIMenus, MacURLPatternContextMenuItems)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.menus.create({",
        @"  id: 'url-pattern-menu-item',",
        @"  title: 'URL Pattern Item',",
        @"  contexts: ['link'],",
        @"  targetUrlPatterns: ['*://*.example.com/*'],",
        @"  documentUrlPatterns: ['*://localhost/*']",
        @"})",

        @"browser.menus.create({",
        @"  id: 'non-matching-menu-item',",
        @"  title: 'Non-Matching Item',",
        @"  contexts: ['link'],",
        @"  targetUrlPatterns: ['*://*.other.com/*'],",
        @"  documentUrlPatterns: ['*://*.apple.com/*']",
        @"})",

        @"browser.menus.onClicked.addListener((info, tab) => {",
        @"  browser.test.assertEq(typeof info, 'object')",
        @"  browser.test.assertEq(typeof tab, 'object')",

        @"  browser.test.assertEq(info.menuItemId, 'url-pattern-menu-item')",
        @"  browser.test.assertEq(info.parentMenuItemId, undefined)",
        @"  browser.test.assertEq(info.selectionText, 'Large Example Link')",
        @"  browser.test.assertEq(info.linkText, 'Large Example Link')",
        @"  browser.test.assertEq(info.linkUrl, 'http://example.com/test')",
        @"  browser.test.assertEq(typeof info.pageUrl, 'string')",
        @"  browser.test.assertTrue(info.pageUrl.startsWith('http://localhost:'))",
        @"  browser.test.assertEq(info.frameId, 0)",
        @"  browser.test.assertEq(info.editable, false)",

        @"  browser.test.assertEq(typeof tab.id, 'number')",
        @"  browser.test.assertEq(typeof tab.windowId, 'number')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Menus Created')",
    ]);

    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block bool gotContextMenu = false;
    delegate.get().getContextMenuFromProposedMenu = ^(NSMenu *menu, _WKContextMenuElementInfo *elementInfo, id<NSSecureCoding>, void (^completionHandler)(NSMenu *)) {
        gotContextMenu = true;

        NSMenuItem *urlPatternMenuItem = nil;
        NSMenuItem *nonMatchingMenuItem = nil;
        for (NSMenuItem *menuItem in menu.itemArray) {
            if ([menuItem.title isEqualToString:@"URL Pattern Item"])
                urlPatternMenuItem = menuItem;
            else if ([menuItem.title isEqualToString:@"Non-Matching Item"])
                nonMatchingMenuItem = menuItem;
        }

        EXPECT_NOT_NULL(urlPatternMenuItem);
        EXPECT_NULL(nonMatchingMenuItem);

        performMenuItemAction(urlPatternMenuItem);

        completionHandler(nil);
    };

    auto manager = Util::loadAndRunExtension(menusManifest, @{ @"background.js": backgroundScript });

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Created");

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<a href='http://example.com/test' style='font-size: 100px'>Large Example Link</p>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().webExtensionController = manager.get().controller;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    webView.get().UIDelegate = delegate.get();

    manager.get().defaultTab.webView = webView.get();

    [webView synchronouslyLoadRequest:urlRequest];
    [webView waitForNextPresentationUpdate];

    [webView.get().window makeFirstResponder:webView.get()];

    [webView mouseDownAtPoint:NSMakePoint(50, 50) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(50, 50) withFlags:0 eventType:NSEventTypeRightMouseUp];

    Util::run(&gotContextMenu);

    [manager run];
}

TEST(WKWebExtensionAPIMenus, MacSelectionContextMenuItems)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.menus.create({",
        @"  id: 'selection-menu-item',",
        @"  title: 'Selected: %s',",
        @"  contexts: [ 'selection' ]",
        @"})",

        @"browser.menus.onClicked.addListener((info, tab) => {",
        @"  browser.test.assertEq(typeof info, 'object')",
        @"  browser.test.assertEq(typeof tab, 'object')",

        @"  browser.test.assertEq(info.menuItemId, 'selection-menu-item')",
        @"  browser.test.assertEq(info.parentMenuItemId, undefined)",
        @"  browser.test.assertEq(info.selectionText, 'Example')",
        @"  browser.test.assertEq(typeof info.pageUrl, 'string')",
        @"  browser.test.assertTrue(info.pageUrl.startsWith('http://localhost:'))",
        @"  browser.test.assertEq(info.frameId, 0)",
        @"  browser.test.assertEq(info.editable, false)",

        @"  browser.test.assertEq(typeof tab.id, 'number')",
        @"  browser.test.assertEq(typeof tab.windowId, 'number')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Menus Created')",
    ]);

    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block bool gotContextMenu = false;
    delegate.get().getContextMenuFromProposedMenu = ^(NSMenu *menu, _WKContextMenuElementInfo *, id<NSSecureCoding>, void (^completionHandler)(NSMenu *)) {
        gotContextMenu = true;

        NSMenuItem *selectionMenuItem = nil;
        for (NSMenuItem *menuItem in menu.itemArray) {
            if ([menuItem.title hasPrefix:@"Selected: "]) {
                selectionMenuItem = menuItem;
                break;
            }
        }

        EXPECT_NOT_NULL(selectionMenuItem);
        EXPECT_NS_EQUAL(selectionMenuItem.title, @"Selected: Example");

        performMenuItemAction(selectionMenuItem);

        completionHandler(nil);
    };

    auto manager = Util::loadAndRunExtension(menusManifest, @{ @"background.js": backgroundScript });

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Created");

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<p style='font-size: 100px'>Selection Example Text</p>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().webExtensionController = manager.get().controller;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    webView.get().UIDelegate = delegate.get();

    manager.get().defaultTab.webView = webView.get();

    [webView synchronouslyLoadRequest:urlRequest];
    [webView waitForNextPresentationUpdate];

    [webView.get().window makeFirstResponder:webView.get()];

    [webView mouseDownAtPoint:NSMakePoint(200, 200) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(200, 200) withFlags:0 eventType:NSEventTypeRightMouseUp];

    Util::run(&gotContextMenu);

    [manager run];
}

TEST(WKWebExtensionAPIMenus, MacLinkContextMenuItems)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.menus.create({",
        @"  id: 'link-menu-item',",
        @"  title: 'Link Item',",
        @"  contexts: [ 'link' ]",
        @"})",

        @"browser.menus.onClicked.addListener((info, tab) => {",
        @"  browser.test.assertEq(typeof info, 'object')",
        @"  browser.test.assertEq(typeof tab, 'object')",

        @"  browser.test.assertEq(info.menuItemId, 'link-menu-item')",
        @"  browser.test.assertEq(info.parentMenuItemId, undefined)",
        @"  browser.test.assertEq(info.selectionText, 'Large Example Link')",
        @"  browser.test.assertEq(info.linkText, 'Large Example Link')",
        @"  browser.test.assertEq(info.linkUrl, 'http://example.com/test')",
        @"  browser.test.assertEq(typeof info.pageUrl, 'string')",
        @"  browser.test.assertTrue(info.pageUrl.startsWith('http://localhost:'))",
        @"  browser.test.assertEq(info.frameId, 0)",
        @"  browser.test.assertEq(info.editable, false)",

        @"  browser.test.assertEq(typeof tab.id, 'number')",
        @"  browser.test.assertEq(typeof tab.windowId, 'number')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Menus Created')",
    ]);

    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block bool gotContextMenu = false;
    delegate.get().getContextMenuFromProposedMenu = ^(NSMenu *menu, _WKContextMenuElementInfo *, id<NSSecureCoding>, void (^completionHandler)(NSMenu *)) {
        gotContextMenu = true;

        NSMenuItem *linkMenuItem = nil;
        for (NSMenuItem *menuItem in menu.itemArray) {
            if ([menuItem.title isEqualToString:@"Link Item"]) {
                linkMenuItem = menuItem;
                break;
            }
        }

        EXPECT_NOT_NULL(linkMenuItem);

        performMenuItemAction(linkMenuItem);

        completionHandler(nil);
    };

    auto manager = Util::loadAndRunExtension(menusManifest, @{ @"background.js": backgroundScript });

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Created");

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<a href='http://example.com/test' style='font-size: 100px'>Large Example Link</p>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().webExtensionController = manager.get().controller;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    webView.get().UIDelegate = delegate.get();

    manager.get().defaultTab.webView = webView.get();

    [webView synchronouslyLoadRequest:urlRequest];
    [webView waitForNextPresentationUpdate];

    [webView.get().window makeFirstResponder:webView.get()];

    [webView mouseDownAtPoint:NSMakePoint(200, 200) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(200, 200) withFlags:0 eventType:NSEventTypeRightMouseUp];

    Util::run(&gotContextMenu);

    [manager run];
}

TEST(WKWebExtensionAPIMenus, MacImageContextMenuItems)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.menus.create({",
        @"  id: 'image-menu-item',",
        @"  title: 'Image Item',",
        @"  contexts: [ 'image' ]",
        @"})",

        @"browser.menus.onClicked.addListener((info, tab) => {",
        @"  browser.test.assertEq(typeof info, 'object')",
        @"  browser.test.assertEq(typeof tab, 'object')",

        @"  browser.test.assertEq(info.menuItemId, 'image-menu-item')",
        @"  browser.test.assertEq(info.parentMenuItemId, undefined)",
        @"  browser.test.assertEq(info.mediaType, 'image')",
        @"  browser.test.assertEq(info.srcUrl, 'http://example.com/example.png')",
        @"  browser.test.assertEq(typeof info.pageUrl, 'string')",
        @"  browser.test.assertTrue(info.pageUrl.startsWith('http://localhost:'))",
        @"  browser.test.assertEq(info.frameId, 0)",
        @"  browser.test.assertEq(info.editable, false)",

        @"  browser.test.assertEq(typeof tab.id, 'number')",
        @"  browser.test.assertEq(typeof tab.windowId, 'number')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Menus Created')",
    ]);

    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block bool gotContextMenu = false;
    delegate.get().getContextMenuFromProposedMenu = ^(NSMenu *menu, _WKContextMenuElementInfo *, id<NSSecureCoding>, void (^completionHandler)(NSMenu *)) {
        gotContextMenu = true;

        NSMenuItem *imageMenuItem = nil;
        for (NSMenuItem *menuItem in menu.itemArray) {
            if ([menuItem.title isEqualToString:@"Image Item"]) {
                imageMenuItem = menuItem;
                break;
            }
        }

        EXPECT_NOT_NULL(imageMenuItem);

        performMenuItemAction(imageMenuItem);

        completionHandler(nil);
    };

    auto manager = Util::loadAndRunExtension(menusManifest, @{ @"background.js": backgroundScript });

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Created");

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<img src='http://example.com/example.png' style='width: 400px; height: 400px'>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().webExtensionController = manager.get().controller;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    webView.get().UIDelegate = delegate.get();

    manager.get().defaultTab.webView = webView.get();

    [webView synchronouslyLoadRequest:urlRequest];
    [webView waitForNextPresentationUpdate];

    [webView.get().window makeFirstResponder:webView.get()];

    [webView mouseDownAtPoint:NSMakePoint(200, 200) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(200, 200) withFlags:0 eventType:NSEventTypeRightMouseUp];

    Util::run(&gotContextMenu);

    [manager run];
}

TEST(WKWebExtensionAPIMenus, MacVideoContextMenuItems)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.menus.create({",
        @"  id: 'video-menu-item',",
        @"  title: 'Video Item',",
        @"  contexts: [ 'video' ]",
        @"})",

        @"browser.menus.onClicked.addListener((info, tab) => {",
        @"  browser.test.assertEq(typeof info, 'object')",
        @"  browser.test.assertEq(typeof tab, 'object')",

        @"  browser.test.assertEq(info.menuItemId, 'video-menu-item')",
        @"  browser.test.assertEq(info.parentMenuItemId, undefined)",
        @"  browser.test.assertEq(info.mediaType, 'video')",
        @"  browser.test.assertEq(info.srcUrl, 'http://example.com/example.mp4')",
        @"  browser.test.assertEq(typeof info.pageUrl, 'string')",
        @"  browser.test.assertTrue(info.pageUrl.startsWith('http://localhost:'))",
        @"  browser.test.assertEq(info.frameId, 0)",
        @"  browser.test.assertEq(info.editable, false)",

        @"  browser.test.assertEq(typeof tab.id, 'number')",
        @"  browser.test.assertEq(typeof tab.windowId, 'number')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Menus Created')",
    ]);

    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block bool gotContextMenu = false;
    delegate.get().getContextMenuFromProposedMenu = ^(NSMenu *menu, _WKContextMenuElementInfo *, id<NSSecureCoding>, void (^completionHandler)(NSMenu *)) {
        gotContextMenu = true;

        NSMenuItem *videoMenuItem = nil;
        for (NSMenuItem *menuItem in menu.itemArray) {
            if ([menuItem.title isEqualToString:@"Video Item"]) {
                videoMenuItem = menuItem;
                break;
            }
        }

        EXPECT_NOT_NULL(videoMenuItem);

        performMenuItemAction(videoMenuItem);

        completionHandler(nil);
    };

    auto manager = Util::loadAndRunExtension(menusManifest, @{ @"background.js": backgroundScript });

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Created");

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<video src='http://example.com/example.mp4' style='width: 400px; height: 400px' controls></video>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().webExtensionController = manager.get().controller;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    webView.get().UIDelegate = delegate.get();

    manager.get().defaultTab.webView = webView.get();

    [webView synchronouslyLoadRequest:urlRequest];
    [webView waitForNextPresentationUpdate];

    [webView.get().window makeFirstResponder:webView.get()];

    [webView mouseDownAtPoint:NSMakePoint(200, 200) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(200, 200) withFlags:0 eventType:NSEventTypeRightMouseUp];

    Util::run(&gotContextMenu);

    [manager run];
}

TEST(WKWebExtensionAPIMenus, MacAudioContextMenuItems)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.menus.create({",
        @"  id: 'audio-menu-item',",
        @"  title: 'Audio Item',",
        @"  contexts: [ 'audio' ]",
        @"})",

        @"browser.menus.onClicked.addListener((info, tab) => {",
        @"  browser.test.assertEq(typeof info, 'object')",
        @"  browser.test.assertEq(typeof tab, 'object')",

        @"  browser.test.assertEq(info.menuItemId, 'audio-menu-item')",
        @"  browser.test.assertEq(info.parentMenuItemId, undefined)",
        @"  browser.test.assertEq(info.mediaType, 'audio')",
        @"  browser.test.assertEq(info.srcUrl, 'http://example.com/example.mp3')",
        @"  browser.test.assertEq(typeof info.pageUrl, 'string')",
        @"  browser.test.assertTrue(info.pageUrl.startsWith('http://localhost:'))",
        @"  browser.test.assertEq(info.frameId, 0)",
        @"  browser.test.assertEq(info.editable, false)",

        @"  browser.test.assertEq(typeof tab.id, 'number')",
        @"  browser.test.assertEq(typeof tab.windowId, 'number')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Menus Created')",
    ]);

    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block bool gotContextMenu = false;
    delegate.get().getContextMenuFromProposedMenu = ^(NSMenu *menu, _WKContextMenuElementInfo *, id<NSSecureCoding>, void (^completionHandler)(NSMenu *)) {
        gotContextMenu = true;

        NSMenuItem *audioMenuItem = nil;
        for (NSMenuItem *menuItem in menu.itemArray) {
            if ([menuItem.title isEqualToString:@"Audio Item"]) {
                audioMenuItem = menuItem;
                break;
            }
        }

        EXPECT_NOT_NULL(audioMenuItem);

        performMenuItemAction(audioMenuItem);

        completionHandler(nil);
    };

    auto manager = Util::loadAndRunExtension(menusManifest, @{ @"background.js": backgroundScript });

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Created");

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<audio src='http://example.com/example.mp3' style='width: 400px; height: 400px' controls></audio>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().webExtensionController = manager.get().controller;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    webView.get().UIDelegate = delegate.get();

    manager.get().defaultTab.webView = webView.get();

    [webView synchronouslyLoadRequest:urlRequest];
    [webView waitForNextPresentationUpdate];

    [webView.get().window makeFirstResponder:webView.get()];

    [webView mouseDownAtPoint:NSMakePoint(200, 200) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(200, 200) withFlags:0 eventType:NSEventTypeRightMouseUp];

    Util::run(&gotContextMenu);

    [manager run];
}

TEST(WKWebExtensionAPIMenus, MacEditableContextMenuItems)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.menus.create({",
        @"  id: 'editable-menu-item',",
        @"  title: 'Editable Item',",
        @"  contexts: [ 'editable' ]",
        @"})",

        @"browser.menus.onClicked.addListener((info, tab) => {",
        @"  browser.test.assertEq(typeof info, 'object')",
        @"  browser.test.assertEq(typeof tab, 'object')",

        @"  browser.test.assertEq(info.menuItemId, 'editable-menu-item')",
        @"  browser.test.assertEq(info.parentMenuItemId, undefined)",
        @"  browser.test.assertEq(info.selectionText, 'Area')",
        @"  browser.test.assertEq(info.editable, true)",

        @"  browser.test.assertEq(typeof tab.id, 'number')",
        @"  browser.test.assertEq(typeof tab.windowId, 'number')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Menus Created')",
    ]);

    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block bool gotContextMenu = false;
    delegate.get().getContextMenuFromProposedMenu = ^(NSMenu *menu, _WKContextMenuElementInfo *, id<NSSecureCoding>, void (^completionHandler)(NSMenu *)) {
        gotContextMenu = true;

        NSMenuItem *editableMenuItem = nil;
        for (NSMenuItem *menuItem in menu.itemArray) {
            if ([menuItem.title isEqualToString:@"Editable Item"]) {
                editableMenuItem = menuItem;
                break;
            }
        }

        EXPECT_NOT_NULL(editableMenuItem);

        performMenuItemAction(editableMenuItem);

        completionHandler(nil);
    };

    auto manager = Util::loadAndRunExtension(menusManifest, @{ @"background.js": backgroundScript });

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Created");

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<textarea style='font-size: 100px; width: 400px; height: 400px'>Editable Text Area</textarea>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().webExtensionController = manager.get().controller;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    webView.get().UIDelegate = delegate.get();

    manager.get().defaultTab.webView = webView.get();

    [webView synchronouslyLoadRequest:urlRequest];
    [webView waitForNextPresentationUpdate];

    [webView.get().window makeFirstResponder:webView.get()];

    [webView mouseDownAtPoint:NSMakePoint(200, 200) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(200, 200) withFlags:0 eventType:NSEventTypeRightMouseUp];

    Util::run(&gotContextMenu);

    [manager run];
}

TEST(WKWebExtensionAPIMenus, MacFrameContextMenuItems)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.menus.create({",
        @"  id: 'frame-menu-item',",
        @"  title: 'Frame Item',",
        @"  contexts: [ 'page', 'frame' ]",
        @"})",

        @"browser.menus.onClicked.addListener((info, tab) => {",
        @"  browser.test.assertEq(typeof info, 'object')",
        @"  browser.test.assertEq(typeof tab, 'object')",

        @"  browser.test.assertEq(info.menuItemId, 'frame-menu-item')",
        @"  browser.test.assertEq(info.parentMenuItemId, undefined)",
        @"  browser.test.assertEq(info.selectionText, undefined)",
        @"  browser.test.assertEq(typeof info.frameUrl, 'string')",
        @"  browser.test.assertTrue(info.frameUrl.endsWith('frame.html'))",
        @"  browser.test.assertTrue(info.frameId !== 0)",

        @"  browser.test.assertEq(typeof tab.id, 'number')",
        @"  browser.test.assertEq(typeof tab.windowId, 'number')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Menus Created')",
    ]);

    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block bool gotContextMenu = false;
    delegate.get().getContextMenuFromProposedMenu = ^(NSMenu *menu, _WKContextMenuElementInfo *, id<NSSecureCoding>, void (^completionHandler)(NSMenu *)) {
        gotContextMenu = true;

        NSMenuItem *frameMenuItem = nil;
        for (NSMenuItem *menuItem in menu.itemArray) {
            if ([menuItem.title isEqualToString:@"Frame Item"]) {
                frameMenuItem = menuItem;
                break;
            }
        }

        EXPECT_NOT_NULL(frameMenuItem);

        performMenuItemAction(frameMenuItem);

        completionHandler(nil);
    };

    auto manager = Util::loadAndRunExtension(menusManifest, @{ @"background.js": backgroundScript });

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menus Created");

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='frame.html' style='width: 400px; height: 400px'>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:server.requestWithLocalhost("/frame.html"_s).URL];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().webExtensionController = manager.get().controller;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    webView.get().UIDelegate = delegate.get();

    manager.get().defaultTab.webView = webView.get();

    [webView synchronouslyLoadRequest:urlRequest];
    [webView waitForNextPresentationUpdate];

    [webView.get().window makeFirstResponder:webView.get()];

    [webView mouseDownAtPoint:NSMakePoint(200, 200) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(200, 200) withFlags:0 eventType:NSEventTypeRightMouseUp];

    Util::run(&gotContextMenu);

    [manager run];
}

TEST(WKWebExtensionAPIMenus, ClickedMenuItemAndPermissionsRequest)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.menus.create({",
        @"  id: 'click-item',",
        @"  title: 'Click Item',",
        @"  contexts: ['all'],",
        @"  onclick: async (info, tab) => {",
        @"    try {",
        @"      const result = await browser.permissions.request({ 'permissions': [ 'menus' ] })",
        @"      if (result)",
        @"        browser.test.notifyPass()",
        @"      else",
        @"        browser.test.notifyFail('Permissions request was rejected')",
        @"    } catch (error) {",
        @"      browser.test.notifyFail('Permissions request failed')",
        @"    }",
        @"  }",
        @"}, () => browser.test.yield('Menu Item Created'))"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:menusManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.promptForPermissions = ^(id<WKWebExtensionTab> tab, NSSet<NSString *> *requestedPermissions, void (^callback)(NSSet<NSString *> *, NSDate *)) {
        EXPECT_EQ(requestedPermissions.count, 1lu);
        EXPECT_TRUE([requestedPermissions isEqualToSet:[NSSet setWithObject:@"menus"]]);
        callback(requestedPermissions, nil);
    };

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Menu Item Created");

    auto *menuItems = [manager.get().context menuItemsForTab:manager.get().defaultTab];
    EXPECT_EQ(menuItems.count, 1lu);

    performMenuItemAction(menuItems.firstObject);

    [manager run];
}

#endif // PLATFORM(MAC)

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
