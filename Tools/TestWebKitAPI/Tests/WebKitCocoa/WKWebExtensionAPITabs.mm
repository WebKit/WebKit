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

#import "TestWebExtensionsDelegate.h"
#import "WebExtensionUtilities.h"
#import <WebKit/_WKWebExtensionTabCreationOptions.h>

namespace TestWebKitAPI {

static auto *tabsManifest = @{
    @"manifest_version": @3,

    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"type": @"module",
        @"persistent": @NO,
    },
};

TEST(WKWebExtensionAPITabs, Errors)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertThrows(() => browser.tabs.get('bad'), /'tabID' value is invalid, because a number is expected/i)",
        @"browser.test.assertThrows(() => browser.tabs.get(-3), /'tabID' value is invalid, because it is not a tab identifier/i)",
        @"browser.test.assertThrows(() => browser.tabs.duplicate('bad'), /'tabID' value is invalid, because a number is expected/i)",
        @"browser.test.assertThrows(() => browser.tabs.remove('bad'), /'tabIDs' value is invalid, because a number or an array of numbers is expected, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.tabs.remove(['bad']), /'tabIDs' value is invalid, because a number or an array of numbers is expected, but an array with other values was provided/i)",
        @"browser.test.assertThrows(() => browser.tabs.reload('bad'), /an unknown argument was provided/i)",
        @"browser.test.assertThrows(() => browser.tabs.goBack('bad'), /'tabID' value is invalid, because a number is expected/i)",
        @"browser.test.assertThrows(() => browser.tabs.goForward('bad'), /'tabID' value is invalid, because a number is expected/i)",
        @"browser.test.assertThrows(() => browser.tabs.getZoom('bad'), /'tabID' value is invalid, because a number is expected/i)",
        @"browser.test.assertThrows(() => browser.tabs.detectLanguage('bad'), /'tabID' value is invalid, because a number is expected/i)",
        @"browser.test.assertThrows(() => browser.tabs.toggleReaderMode('bad'), /'tabID' value is invalid, because a number is expected/i)",

        @"browser.test.assertThrows(() => browser.tabs.setZoom('bad'), /'zoomFactor' value is invalid, because a number is expected/i)",
        @"browser.test.assertThrows(() => browser.tabs.setZoom(1, 'bad'), /'zoomFactor' value is invalid, because a number is expected/i)",

        @"browser.test.assertThrows(() => browser.tabs.update('bad'), /'properties' value is invalid, because an object is expected/i)",
        @"browser.test.assertThrows(() => browser.tabs.update(1, 'bad'), /'properties' value is invalid, because an object is expected/i)",

        @"browser.test.assertThrows(() => browser.tabs.update(1, { 'openerTabId': true }), /'openerTabId' is expected to be a number, but a boolean was provided/i)",
        @"browser.test.assertThrows(() => browser.tabs.update(1, { 'openerTabId': 4.2 }), /'openerTabId' value is invalid, because '4.2' is not a tab identifier/i)",

        @"browser.test.assertThrows(() => browser.tabs.create({ 'url': 1234 }), /'url' is expected to be a string/i)",

        @"browser.test.assertThrows(() => browser.tabs.query({ status: 'bad' }), /'status' value is invalid, because it must specify either 'loading' or 'complete'/i)",

        @"browser.test.assertThrows(() => browser.tabs.query({ url: 12345 }), /'info' value is invalid, because 'url' is expected to be a string or an array of strings, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.tabs.query({ url: ['bad', 12345] }), /'url' is expected to be a string or an array of strings, but an array with other values was provided/i)",

        @"browser.test.assertThrows(() => browser.tabs.query({ windowId: 'bad' }), /'windowId' is expected to be a number, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.tabs.query({ windowId: -5 }), /'windowId' value is invalid, because '-5' is not a window identifier/i)",

        @"browser.test.assertThrows(() => browser.tabs.query({ active: 'true' }), /'active' is expected to be a boolean, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.tabs.query({ highlighted: 123 }), /'highlighted' is expected to be a boolean, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.tabs.query({ index: 'five' }), /'index' is expected to be a number, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.tabs.query({ audible: 'yes' }), /'audible' is expected to be a boolean, but a string was provided/i)",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, Create)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const allWindows = await browser.windows.getAll({ populate: true })",
        @"const initialTabsCount = allWindows[0].tabs.length",

        @"const newTab = await browser.tabs.create({})",

        @"const updatedWindows = await browser.windows.getAll({ populate: true })",
        @"const updatedTabs = updatedWindows[0].tabs",
        @"browser.test.assertEq(updatedTabs.length, initialTabsCount + 1, 'A new tab should have been added')",
        @"browser.test.assertEq(newTab.index, initialTabsCount, 'The new tab should be the last tab in the window')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);
    auto delegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);
    manager.get().controllerDelegate = delegate.get();

    auto windowOne = adoptNS([[TestWebExtensionWindow alloc] init]);
    auto tabOne = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);

    windowOne.get().tabs = @[ tabOne.get() ];

    delegate.get().openNewTab = ^(_WKWebExtensionTabCreationOptions *options, _WKWebExtensionContext *context, void (^completionHandler)(id<_WKWebExtensionTab>, NSError *)) {
        EXPECT_NS_EQUAL(options.desiredWindow, windowOne.get());
        EXPECT_EQ(options.desiredIndex, windowOne.get().tabs.count);

        EXPECT_NULL(options.desiredParentTab);
        EXPECT_NULL(options.desiredURL);

        EXPECT_TRUE(options.shouldActivate);
        EXPECT_TRUE(options.shouldSelect);
        EXPECT_FALSE(options.shouldPin);
        EXPECT_FALSE(options.shouldMute);
        EXPECT_FALSE(options.shouldShowReaderMode);

        auto newTab = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:context.webExtensionController]);

        windowOne.get().tabs = [windowOne.get().tabs arrayByAddingObject:newTab.get()];

        [context.webExtensionController didOpenTab:newTab.get()];

        completionHandler(newTab.get(), nil);
    };

    delegate.get().openWindows = ^NSArray<id<_WKWebExtensionWindow>> *(_WKWebExtensionContext *) {
        return @[ windowOne.get() ];
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, CreateWithSpecifiedOptions)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const allWindows = await browser.windows.getAll({ populate: true })",
        @"const initialTabsCount = allWindows[0].tabs.length",

        @"const newTab = await browser.tabs.create({",
        @"  url: 'https://example.com/',",
        @"  openerTabId: allWindows[0].tabs[0].id,",
        @"  active: false,",
        @"  pinned: true,",
        @"  muted: true,",
        @"  openInReaderMode: true,",
        @"  index: 1",
        @"});",

        @"const updatedWindows = await browser.windows.getAll({ populate: true })",
        @"const updatedTabs = updatedWindows[0].tabs",
        @"browser.test.assertEq(updatedTabs.length, initialTabsCount + 1, 'A new tab should have been added')",
        @"browser.test.assertEq(newTab.index, 1, 'The new tab should be at index 1')",
        @"browser.test.assertFalse(newTab.active, 'The new tab should not be active')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);
    auto delegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);
    manager.get().controllerDelegate = delegate.get();

    auto windowOne = adoptNS([[TestWebExtensionWindow alloc] init]);
    auto tabOne = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);

    windowOne.get().tabs = @[ tabOne.get() ];

    delegate.get().openNewTab = ^(_WKWebExtensionTabCreationOptions *options, _WKWebExtensionContext *context, void (^completionHandler)(id<_WKWebExtensionTab>, NSError *)) {
        EXPECT_NS_EQUAL(options.desiredWindow, windowOne.get());
        EXPECT_EQ(options.desiredIndex, 1lu);

        EXPECT_NS_EQUAL(options.desiredParentTab, tabOne.get());
        EXPECT_NS_EQUAL(options.desiredURL, [NSURL URLWithString:@"https://example.com/"]);

        EXPECT_FALSE(options.shouldActivate);
        EXPECT_FALSE(options.shouldSelect);
        EXPECT_TRUE(options.shouldPin);
        EXPECT_TRUE(options.shouldMute);
        EXPECT_TRUE(options.shouldShowReaderMode);

        auto newTab = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:context.webExtensionController]);

        windowOne.get().tabs = [windowOne.get().tabs arrayByAddingObject:newTab.get()];

        [context.webExtensionController didOpenTab:newTab.get()];

        completionHandler(newTab.get(), nil);
    };

    delegate.get().openWindows = ^NSArray<id<_WKWebExtensionWindow>> *(_WKWebExtensionContext *) {
        return @[ windowOne.get() ];
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, Duplicate)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const allWindows = await browser.windows.getAll({ populate: true })",
        @"const initialTabsCount = allWindows[0].tabs.length",

        @"const duplicatedTab = await browser.tabs.duplicate(allWindows[0].tabs[0].id)",

        @"const updatedWindows = await browser.windows.getAll({ populate: true })",
        @"const updatedTabs = updatedWindows[0].tabs",
        @"browser.test.assertEq(updatedTabs.length, initialTabsCount + 1, 'A tab should have been duplicated')",
        @"browser.test.assertEq(duplicatedTab.index, initialTabsCount, 'The duplicated tab should be the last tab in the window')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);
    auto delegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);
    manager.get().controllerDelegate = delegate.get();

    auto windowOne = adoptNS([[TestWebExtensionWindow alloc] init]);
    auto tabOne = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);

    windowOne.get().tabs = @[ tabOne.get() ];

    tabOne.get().duplicate = ^(_WKWebExtensionTabCreationOptions *options, void (^completionHandler)(id<_WKWebExtensionTab>, NSError *)) {
        EXPECT_NS_EQUAL(options.desiredWindow, windowOne.get());
        EXPECT_EQ(options.desiredIndex, windowOne.get().tabs.count);

        EXPECT_NULL(options.desiredParentTab);
        EXPECT_NULL(options.desiredURL);

        EXPECT_TRUE(options.shouldActivate);
        EXPECT_TRUE(options.shouldSelect);
        EXPECT_FALSE(options.shouldPin);
        EXPECT_FALSE(options.shouldMute);
        EXPECT_FALSE(options.shouldShowReaderMode);

        auto duplicatedTab = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);

        windowOne.get().tabs = [windowOne.get().tabs arrayByAddingObject:duplicatedTab.get()];

        [manager.get().controller didOpenTab:duplicatedTab.get()];

        completionHandler(duplicatedTab.get(), nil);
    };

    delegate.get().openWindows = ^NSArray<id<_WKWebExtensionWindow>> *(_WKWebExtensionContext *) {
        return @[ windowOne.get() ];
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, DuplicateWithOptions)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const allWindows = await browser.windows.getAll({ populate: true })",
        @"const initialTabsCount = allWindows[0].tabs.length",

        @"const duplicatedTab = await browser.tabs.duplicate(allWindows[0].tabs[0].id, {",
        @"  active: false,",
        @"  index: 1",
        @"});",

        @"const updatedWindows = await browser.windows.getAll({ populate: true })",
        @"const updatedTabs = updatedWindows[0].tabs",
        @"browser.test.assertEq(updatedTabs.length, initialTabsCount + 1, 'A tab should have been duplicated')",
        @"browser.test.assertEq(duplicatedTab.index, 1, 'The duplicated tab should be at index 1')",
        @"browser.test.assertFalse(duplicatedTab.active, 'The duplicated tab should not be active')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);
    auto delegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);
    manager.get().controllerDelegate = delegate.get();

    auto windowOne = adoptNS([[TestWebExtensionWindow alloc] init]);
    auto tabOne = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);

    windowOne.get().tabs = @[ tabOne.get() ];

    tabOne.get().duplicate = ^(_WKWebExtensionTabCreationOptions *options, void (^completionHandler)(id<_WKWebExtensionTab>, NSError *)) {
        EXPECT_NS_EQUAL(options.desiredWindow, windowOne.get());
        EXPECT_EQ(options.desiredIndex, 1lu);

        EXPECT_NULL(options.desiredParentTab);
        EXPECT_NULL(options.desiredURL);

        EXPECT_FALSE(options.shouldActivate);
        EXPECT_FALSE(options.shouldSelect);
        EXPECT_FALSE(options.shouldPin);
        EXPECT_FALSE(options.shouldMute);
        EXPECT_FALSE(options.shouldShowReaderMode);

        auto duplicatedTab = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);

        windowOne.get().tabs = [windowOne.get().tabs arrayByAddingObject:duplicatedTab.get()];

        [manager.get().controller didOpenTab:duplicatedTab.get()];

        completionHandler(duplicatedTab.get(), nil);
    };

    delegate.get().openWindows = ^NSArray<id<_WKWebExtensionWindow>> *(_WKWebExtensionContext *) {
        return @[ windowOne.get() ];
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, Update)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const allWindows = await browser.windows.getAll({ populate: true })",
        @"const secondTab = allWindows[0].tabs[1]",

        @"browser.test.assertFalse(secondTab.active, 'The tab should not be initially active')",
        @"browser.test.assertFalse(secondTab.highlighted, 'The tab should not be initially highlighted')",
        @"browser.test.assertFalse(secondTab.pinned, 'The tab should not be initially pinned')",
        @"browser.test.assertFalse(secondTab.mutedInfo.muted, 'The tab should not be initially muted')",
        @"browser.test.assertEq(secondTab.openerTabId, undefined, 'The initial tab should not have an openerTabId')",

        @"const updatedTab = await browser.tabs.update(secondTab.id, {",
        @"  active: true,",
        @"  highlighted: true,",
        @"  pinned: true,",
        @"  muted: true,",
        @"  openerTabId: allWindows[0].tabs[0].id",
        @"});",

        @"browser.test.assertTrue(updatedTab.active, 'The tab should be active after update')",
        @"browser.test.assertTrue(updatedTab.highlighted, 'The tab should be highlighted after update')",
        @"browser.test.assertTrue(updatedTab.pinned, 'The tab should be pinned after update')",
        @"browser.test.assertTrue(updatedTab.mutedInfo.muted, 'The tab should be muted after update')",
        @"browser.test.assertEq(updatedTab.openerTabId, allWindows[0].tabs[0].id, 'The openerTabId should match the first tab after update')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);
    auto delegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);
    manager.get().controllerDelegate = delegate.get();

    auto windowOne = adoptNS([[TestWebExtensionWindow alloc] init]);
    auto tabOne = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);
    auto tabTwo = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);

    windowOne.get().tabs = @[ tabOne.get(), tabTwo.get() ];

    delegate.get().openWindows = ^NSArray<id<_WKWebExtensionWindow>> *(_WKWebExtensionContext *) {
        return @[ windowOne.get() ];
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, Get)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const allWindows = await browser.windows.getAll({ populate: true })",
        @"const windowId = allWindows[0].id",
        @"const tabId = allWindows[0].tabs[0].id",

        @"const tab = await browser.tabs.get(tabId)",

        @"browser.test.assertEq(typeof tab, 'object', 'The tab should be an object')",
        @"browser.test.assertEq(tab.id, tabId, 'The tab id should match the one used with tabs.get()')",
        @"browser.test.assertEq(tab.windowId, windowId, 'The tab windowId should match the window id from windows.getAll()')",
        @"browser.test.assertEq(tab.index, 0, 'The tab index should be 0')",
        @"browser.test.assertEq(tab.status, 'complete', 'The tab status should be complete')",
        @"browser.test.assertTrue(tab.active, 'The tab active state should be true')",
        @"browser.test.assertTrue(tab.selected, 'The tab selected state should be true')",
        @"browser.test.assertTrue(tab.highlighted, 'The tab highlighted state should be true')",
        @"browser.test.assertFalse(tab.incognito, 'The tab incognito state should be false')",
        @"browser.test.assertEq(tab.url, '', 'The tab url should be an empty string')",
        @"browser.test.assertEq(tab.title, '', 'The tab title should be an empty string')",
        @"browser.test.assertEq(tab.width, 800, 'The tab width should be 800')",
        @"browser.test.assertEq(tab.height, 600, 'The tab height should be 600')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto delegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);
    manager.get().controllerDelegate = delegate.get();

    auto windowOne = adoptNS([[TestWebExtensionWindow alloc] init]);

    auto tabOne = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);
    auto tabTwo = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);

    windowOne.get().tabs = @[ tabOne.get(), tabTwo.get() ];

    delegate.get().openWindows = ^NSArray<id<_WKWebExtensionWindow>> *(_WKWebExtensionContext *) {
        return @[ windowOne.get() ];
    };

    delegate.get().focusedWindow = ^id<_WKWebExtensionWindow>(_WKWebExtensionContext *) {
        return windowOne.get();
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, GetCurrent)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const tab = await browser.tabs.getCurrent()",

        @"browser.test.assertEq(tab, undefined, 'The current tab should be undefined in the background page')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto delegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);
    manager.get().controllerDelegate = delegate.get();

    auto windowOne = adoptNS([[TestWebExtensionWindow alloc] init]);

    auto tabOne = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);
    auto tabTwo = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);

    windowOne.get().tabs = @[ tabOne.get(), tabTwo.get() ];

    delegate.get().openWindows = ^NSArray<id<_WKWebExtensionWindow>> *(_WKWebExtensionContext *) {
        return @[ windowOne.get() ];
    };

    delegate.get().focusedWindow = ^id<_WKWebExtensionWindow>(_WKWebExtensionContext *) {
        return windowOne.get();
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, Query)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const allWindows = await browser.windows.getAll({ populate: true })",
        @"const windowIdOne = allWindows[0].id",
        @"const windowIdTwo = allWindows[1].id",

        @"const tabIdOne = allWindows[0].tabs[0].id",
        @"const tabIdTwo = allWindows[0].tabs[1].id",
        @"const tabIdThree = allWindows[1].tabs[0].id",
        @"const tabIdFour = allWindows[1].tabs[1].id",
        @"const tabIdFive = allWindows[1].tabs[2].id",

        @"const tabsInWindowOne = await browser.tabs.query({ windowId: windowIdOne })",
        @"const tabsInWindowTwo = await browser.tabs.query({ windowId: windowIdTwo })",
        @"browser.test.assertEq(tabsInWindowOne.length, 2, 'There should be 2 tabs in the first window')",
        @"browser.test.assertEq(tabsInWindowTwo.length, 3, 'There should be 3 tabs in the second window')",

        @"const thirdTab = await browser.tabs.query({ index: 0, windowId: windowIdTwo })",
        @"browser.test.assertEq(thirdTab[0].id, tabIdThree, 'Third tab ID should match the first tab of the second window')",

        @"const activeTabs = await browser.tabs.query({ active: true })",
        @"browser.test.assertEq(activeTabs.length, 2, 'There should be 2 active tabs across all windows')",

        @"const hiddenTabs = await browser.tabs.query({ hidden: true })",
        @"browser.test.assertEq(hiddenTabs.length, 3, 'There should be 3 hidden tabs across all windows')",

        @"const lastFocusedTabs = await browser.tabs.query({ lastFocusedWindow: true })",
        @"browser.test.assertEq(lastFocusedTabs.length, 2, 'There should be 2 tabs in the last focused window')",

        @"const pinnedTabs = await browser.tabs.query({ pinned: true })",
        @"browser.test.assertEq(pinnedTabs.length, 0, 'There should be no pinned tabs')",

        @"const loadingTabs = await browser.tabs.query({ status: 'loading' })",
        @"browser.test.assertEq(loadingTabs.length, 0, 'There should be no tabs loading')",

        @"const completeTabs = await browser.tabs.query({ status: 'complete' })",
        @"browser.test.assertEq(completeTabs.length, 5, 'There should be 5 tabs with loading complete')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto delegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);
    manager.get().controllerDelegate = delegate.get();

    auto windowOne = adoptNS([[TestWebExtensionWindow alloc] init]);
    auto windowTwo = adoptNS([[TestWebExtensionWindow alloc] init]);

    auto tabOne = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);
    auto tabTwo = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);
    auto tabThree = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowTwo.get() extensionController:manager.get().controller]);
    auto tabFour = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowTwo.get() extensionController:manager.get().controller]);
    auto tabFive = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowTwo.get() extensionController:manager.get().controller]);

    windowOne.get().tabs = @[ tabOne.get(), tabTwo.get() ];
    windowTwo.get().tabs = @[ tabThree.get(), tabFour.get(), tabFive.get() ];

    delegate.get().openWindows = ^NSArray<id<_WKWebExtensionWindow>> *(_WKWebExtensionContext *) {
        return @[ windowOne.get(), windowTwo.get() ];
    };

    delegate.get().focusedWindow = ^id<_WKWebExtensionWindow>(_WKWebExtensionContext *) {
        return windowOne.get();
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, Zoom)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const allWindows = await browser.windows.getAll({ populate: true })",
        @"const windowId = allWindows[0].id",
        @"const tabId = allWindows[0].tabs[0].id",

        @"await browser.tabs.setZoom(tabId, 1.5)",

        @"const zoomLevel = await browser.tabs.getZoom(tabId)",

        @"browser.test.assertEq(typeof zoomLevel, 'number', 'The zoomLevel should be a number')",
        @"browser.test.assertEq(zoomLevel, 1.5, 'The tab zoom level should be 1.5')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto delegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);
    manager.get().controllerDelegate = delegate.get();

    auto windowOne = adoptNS([[TestWebExtensionWindow alloc] init]);
    auto tabOne = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);

    windowOne.get().tabs = @[ tabOne.get() ];

    delegate.get().openWindows = ^NSArray<id<_WKWebExtensionWindow>> *(_WKWebExtensionContext *) {
        return @[ windowOne.get() ];
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, ToggleReaderMode)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const allWindows = await browser.windows.getAll({ populate: true })",
        @"const tabId = allWindows[0].tabs[0].id",

        @"await browser.tabs.toggleReaderMode(tabId)",

        @"let tab = await browser.tabs.get(tabId)",
        @"browser.test.assertTrue(tab.isInReaderMode, 'The tab should be in reader mode after toggling')",

        @"await browser.tabs.toggleReaderMode(tabId)",

        @"tab = await browser.tabs.get(tabId)",
        @"browser.test.assertFalse(tab.isInReaderMode, 'The tab should not be in reader mode after toggling again')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto delegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);
    manager.get().controllerDelegate = delegate.get();

    auto windowOne = adoptNS([[TestWebExtensionWindow alloc] init]);
    auto tabOne = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);

    __block size_t toggleReaderModeCounter = 0;
    tabOne.get().toggleReaderMode = ^{
        ++toggleReaderModeCounter;
    };

    windowOne.get().tabs = @[ tabOne.get() ];

    delegate.get().openWindows = ^NSArray<id<_WKWebExtensionWindow>> *(_WKWebExtensionContext *) {
        return @[ windowOne.get() ];
    };

    [manager loadAndRun];

    ASSERT_EQ(toggleReaderModeCounter, 2lu);
}

TEST(WKWebExtensionAPITabs, DetectLanguage)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const allWindows = await browser.windows.getAll({ populate: true })",
        @"const tabId = allWindows[0].tabs[0].id",

        @"const detectedLanguage = await browser.tabs.detectLanguage(tabId)",

        @"browser.test.assertEq(detectedLanguage, 'en-US', 'The detected language should be English')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto delegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);
    manager.get().controllerDelegate = delegate.get();

    auto windowOne = adoptNS([[TestWebExtensionWindow alloc] init]);
    auto tabOne = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);

    __block bool detectWebpageLocaleCalled = false;
    tabOne.get().detectWebpageLocale = ^{
        detectWebpageLocaleCalled = true;
        return [NSLocale localeWithLocaleIdentifier:@"en-US"];
    };

    windowOne.get().tabs = @[ tabOne.get() ];

    delegate.get().openWindows = ^NSArray<id<_WKWebExtensionWindow>> *(_WKWebExtensionContext *) {
        return @[ windowOne.get() ];
    };

    [manager loadAndRun];

    ASSERT_TRUE(detectWebpageLocaleCalled);
}

TEST(WKWebExtensionAPITabs, Reload)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const allWindows = await browser.windows.getAll({ populate: true })",
        @"const tabId = allWindows[0].tabs[0].id",

        @"await browser.tabs.reload(tabId)",
        @"await browser.tabs.reload(tabId, { bypassCache: true })",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto delegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);
    manager.get().controllerDelegate = delegate.get();

    auto windowOne = adoptNS([[TestWebExtensionWindow alloc] init]);
    auto tabOne = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);

    __block bool reloadCalled = false;
    __block bool reloadFromOriginCalled = false;

    tabOne.get().reload = ^{
        reloadCalled = true;
    };

    tabOne.get().reloadFromOrigin = ^{
        reloadFromOriginCalled = true;
    };

    windowOne.get().tabs = @[ tabOne.get() ];

    delegate.get().openWindows = ^NSArray<id<_WKWebExtensionWindow>> *(_WKWebExtensionContext *) {
        return @[ windowOne.get() ];
    };

    [manager loadAndRun];

    ASSERT_TRUE(reloadCalled);
    ASSERT_TRUE(reloadFromOriginCalled);
}

TEST(WKWebExtensionAPITabs, GoBack)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const allWindows = await browser.windows.getAll({ populate: true })",
        @"const tabId = allWindows[0].tabs[0].id",

        @"await browser.tabs.goBack(tabId)",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);
    auto delegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);
    manager.get().controllerDelegate = delegate.get();

    auto windowOne = adoptNS([[TestWebExtensionWindow alloc] init]);
    auto tabOne = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);

    __block bool goBackCalled = false;

    tabOne.get().goBack = ^{
        goBackCalled = true;
    };

    windowOne.get().tabs = @[ tabOne.get() ];

    delegate.get().openWindows = ^NSArray<id<_WKWebExtensionWindow>> *(_WKWebExtensionContext *) {
        return @[ windowOne.get() ];
    };

    [manager loadAndRun];

    ASSERT_TRUE(goBackCalled);
}

TEST(WKWebExtensionAPITabs, GoForward)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const allWindows = await browser.windows.getAll({ populate: true })",
        @"const tabId = allWindows[0].tabs[0].id",

        @"await browser.tabs.goForward(tabId)",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);
    auto delegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);
    manager.get().controllerDelegate = delegate.get();

    auto windowOne = adoptNS([[TestWebExtensionWindow alloc] init]);
    auto tabOne = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);

    __block bool goForwardCalled = false;

    tabOne.get().goForward = ^{
        goForwardCalled = true;
    };

    windowOne.get().tabs = @[ tabOne.get() ];

    delegate.get().openWindows = ^NSArray<id<_WKWebExtensionWindow>> *(_WKWebExtensionContext *) {
        return @[ windowOne.get() ];
    };

    [manager loadAndRun];

    ASSERT_TRUE(goForwardCalled);
}

TEST(WKWebExtensionAPITabs, Remove)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const allWindows = await browser.windows.getAll({ populate: true })",
        @"const tabIdToRemove = allWindows[0].tabs[1].id",

        @"await browser.tabs.remove(tabIdToRemove)",

        @"const updatedAllWindows = await browser.windows.getAll({ populate: true })",
        @"const remainingTabs = updatedAllWindows[0].tabs.map(tab => tab.id)",

        @"browser.test.assertFalse(remainingTabs.includes(tabIdToRemove), 'The tab should be removed')",
        @"await browser.test.assertRejects(browser.tabs.get(tabIdToRemove), /tab not found/i, 'The removed tab should not be retrievable with tabs.get()')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);
    auto delegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);
    manager.get().controllerDelegate = delegate.get();

    auto windowOne = adoptNS([[TestWebExtensionWindow alloc] init]);
    auto tabOne = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);
    auto tabTwo = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);

    windowOne.get().tabs = @[ tabOne.get(), tabTwo.get() ];

    delegate.get().openWindows = ^NSArray<id<_WKWebExtensionWindow>> *(_WKWebExtensionContext *) {
        return @[ windowOne.get() ];
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, RemoveMultipleTabs)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const allWindows = await browser.windows.getAll({ populate: true })",
        @"const tabIdsToRemove = [allWindows[0].tabs[1].id, allWindows[0].tabs[2].id]",

        @"await browser.tabs.remove(tabIdsToRemove)",

        @"const updatedAllWindows = await browser.windows.getAll({ populate: true })",
        @"const remainingTabs = updatedAllWindows[0].tabs.map(tab => tab.id)",

        @"for (let id of tabIdsToRemove) {",
        @"    browser.test.assertFalse(remainingTabs.includes(id), 'Removed tabs should not be retrievable')",
        @"}",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);
    auto delegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);
    manager.get().controllerDelegate = delegate.get();

    auto windowOne = adoptNS([[TestWebExtensionWindow alloc] init]);
    auto tabOne = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);
    auto tabTwo = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);
    auto tabThree = adoptNS([[TestWebExtensionTab alloc] initWithWindow:windowOne.get() extensionController:manager.get().controller]);

    windowOne.get().tabs = @[ tabOne.get(), tabTwo.get(), tabThree.get() ];

    delegate.get().openWindows = ^NSArray<id<_WKWebExtensionWindow>> *(_WKWebExtensionContext *) {
        return @[ windowOne.get() ];
    };

    [manager loadAndRun];
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
