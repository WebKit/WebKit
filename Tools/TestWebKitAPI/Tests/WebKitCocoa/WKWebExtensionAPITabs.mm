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
#import "WebExtensionUtilities.h"

namespace TestWebKitAPI {

static auto *tabsManifest = @{
    @"manifest_version": @3,

    @"name": @"Tabs Test",
    @"description": @"Tabs Test",
    @"version": @"1",

    @"options_page": @"options.html",

    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"type": @"module",
        @"persistent": @NO,
    },

    @"permissions": @[ @"tabs" ],
};

static auto *tabsManifestV2 = @{
    @"manifest_version": @2,

    @"name": @"Tabs Test",
    @"description": @"Tabs Test",
    @"version": @"1",

    @"permissions": @[ @"*://localhost/*" ],

    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"type": @"module",
        @"persistent": @NO,
    },
};

static auto *tabsContentScriptManifest = @{
    @"manifest_version": @3,

    @"name": @"Tabs Test",
    @"description": @"Tabs Test",
    @"version": @"1",

    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"type": @"module",
        @"persistent": @NO,
    },

    @"content_scripts": @[ @{
        @"js": @[ @"content.js" ],
        @"matches": @[ @"*://localhost/*" ],
        @"all_frames": @YES,
    } ],
};

static auto *activeTabManifest = @{
    @"manifest_version": @3,

    @"name": @"Tabs Test",
    @"description": @"Tabs Test",
    @"version": @"1",

    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"type": @"module",
        @"persistent": @NO,
    },

    @"permissions": @[ @"activeTab" ],
};

TEST(WKWebExtensionAPITabs, Errors)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertThrows(() => browser.tabs.get('bad'), /'tabId' value is invalid, because a number is expected/i)",
        @"browser.test.assertThrows(() => browser.tabs.get(-3), /'tabId' value is invalid, because it is not a tab identifier/i)",
        @"browser.test.assertThrows(() => browser.tabs.duplicate('bad'), /'tabId' value is invalid, because a number is expected/i)",
        @"browser.test.assertThrows(() => browser.tabs.remove('bad'), /'tabIDs' value is invalid, because a number or an array of numbers is expected, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.tabs.remove(['bad']), /'tabIDs' value is invalid, because a number or an array of numbers is expected, but an array of other values was provided/i)",
        @"browser.test.assertThrows(() => browser.tabs.reload('bad'), /an unknown argument was provided/i)",
        @"browser.test.assertThrows(() => browser.tabs.goBack('bad'), /'tabId' value is invalid, because a number is expected/i)",
        @"browser.test.assertThrows(() => browser.tabs.goForward('bad'), /'tabId' value is invalid, because a number is expected/i)",
        @"browser.test.assertThrows(() => browser.tabs.getZoom('bad'), /'tabId' value is invalid, because a number is expected/i)",
        @"browser.test.assertThrows(() => browser.tabs.detectLanguage('bad'), /'tabId' value is invalid, because a number is expected/i)",
        @"browser.test.assertThrows(() => browser.tabs.toggleReaderMode('bad'), /'tabId' value is invalid, because a number is expected/i)",

        @"browser.test.assertThrows(() => browser.tabs.setZoom('bad'), /'zoomFactor' value is invalid, because a number is expected/i)",
        @"browser.test.assertThrows(() => browser.tabs.setZoom(1, 'bad'), /'zoomFactor' value is invalid, because a number is expected/i)",

        @"browser.test.assertThrows(() => browser.tabs.update('bad'), /'properties' value is invalid, because an object is expected/i)",
        @"browser.test.assertThrows(() => browser.tabs.update(1, 'bad'), /'properties' value is invalid, because an object is expected/i)",

        @"browser.test.assertThrows(() => browser.tabs.update(1, { 'openerTabId': true }), /'openerTabId' is expected to be a number, but a boolean was provided/i)",
        @"browser.test.assertThrows(() => browser.tabs.update(1, { 'openerTabId': 4.2 }), /'openerTabId' value is invalid, because '4.2' is not a tab identifier/i)",

        @"browser.test.assertThrows(() => browser.tabs.create({ 'url': 1234 }), /'url' is expected to be a string/i)",

        @"browser.test.assertThrows(() => browser.tabs.query({ status: 'bad' }), /'status' value is invalid, because it must specify either 'loading' or 'complete'/i)",

        @"browser.test.assertThrows(() => browser.tabs.query({ url: 12345 }), /'info' value is invalid, because 'url' is expected to be a string or an array of strings, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.tabs.query({ url: ['bad', 12345] }), /'url' is expected to be a string or an array of strings, but an array of other values was provided/i)",

        @"browser.test.assertThrows(() => browser.tabs.query({ windowId: 'bad' }), /'windowId' is expected to be a number, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.tabs.query({ windowId: -5 }), /'windowId' value is invalid, because '-5' is not a window identifier/i)",

        @"browser.test.assertThrows(() => browser.tabs.query({ active: 'true' }), /'active' is expected to be a boolean, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.tabs.query({ highlighted: 123 }), /'highlighted' is expected to be a boolean, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.tabs.query({ index: 'five' }), /'index' is expected to be a number, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.tabs.query({ audible: 'yes' }), /'audible' is expected to be a boolean, but a string was provided/i)",

        @"browser.test.assertThrows(() => browser.tabs.captureVisibleTab('bad', { format: 'png' }), /an unknown argument was provided/i)",
        @"browser.test.assertThrows(() => browser.tabs.captureVisibleTab(undefined, { format: 'bad' }), /'format' value is invalid, because it must specify either 'png' or 'jpeg'/i)",
        @"browser.test.assertThrows(() => browser.tabs.captureVisibleTab(undefined, 'bad'), /an unknown argument was provided/i)",
        @"browser.test.assertThrows(() => browser.tabs.captureVisibleTab(undefined, { format: 'jpeg', quality: 'high' }), /'quality' is expected to be a number, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.tabs.captureVisibleTab(undefined, { format: 'jpeg', quality: 200 }), /'quality' value is invalid, because it must specify a value between 0 and 100/i)",

        @"browser.test.assertThrows(() => browser.tabs.sendMessage(1, 'message', { frameId: 0, documentId: 'some-document-id' }), /cannot specify both 'frameId' and 'documentId'/i);",
        @"browser.test.assertThrows(() => browser.tabs.connect(1, { frameId: 0, documentId: 'some-document-id' }), /cannot specify both 'frameId' and 'documentId'/i);",

        @"await browser.test.assertRejects(browser.tabs.captureVisibleTab(9999), /window not found/i)",
        @"await browser.test.assertRejects(browser.tabs.captureVisibleTab(), /'activeTab' permission or granted host permissions for the current website are required/i)",

        @"await browser.test.assertRejects(browser.tabs.get(9999), /tab not found/i)",
        @"await browser.test.assertRejects(browser.tabs.duplicate(9999), /tab not found/i)",
        @"await browser.test.assertRejects(browser.tabs.remove(9999), /tab '9999' was not found/i)",
        @"await browser.test.assertRejects(browser.tabs.reload(9999), /tab not found/i)",
        @"await browser.test.assertRejects(browser.tabs.goBack(9999), /tab not found/i)",
        @"await browser.test.assertRejects(browser.tabs.goForward(9999), /tab not found/i)",
        @"await browser.test.assertRejects(browser.tabs.setZoom(9999, 1.5), /tab not found/i)",
        @"await browser.test.assertRejects(browser.tabs.detectLanguage(9999), /tab not found/i)",
        @"await browser.test.assertRejects(browser.tabs.toggleReaderMode(9999), /tab not found/i)",

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(tabsManifest, @{ @"background.js": backgroundScript });

    backgroundScript = Util::constructScript(@[
        @"browser.test.assertThrows(() => browser.tabs.executeScript(), /a required argument is missing/i)",

        @"await browser.test.assertRejects(browser.tabs.executeScript(9999, { code: 'code to execute' }), /tab not found/i)",

        @"browser.test.assertThrows(() => browser.tabs.executeScript({ code: 'code to execute', file: 'path/to/file.js' }), /it cannot specify both 'file' and 'code'./i)",
        @"browser.test.assertThrows(() => browser.tabs.executeScript({ }), /it must specify either 'file' or 'code'./i)",
        @"browser.test.assertThrows(() => browser.tabs.executeScript({ file: 0, frameId: 0 }), /'file' is expected to be a string, but a number was provided./i)",

        @"browser.test.assertThrows(() => browser.tabs.executeScript({ code: 'code to execute', frameId: 0, allFrames: true }), /it cannot specify both 'allFrames' and 'frameId'./i)",
        @"browser.test.assertThrows(() => browser.tabs.executeScript({ file: 'path/to/file.js', frameId: '0' }), /'frameId' is expected to be a number, but a string was provided./i)",
        @"browser.test.assertThrows(() => browser.tabs.executeScript({ file: 'path/to/file.js', allFrames: 'true' }), /'allFrames' is expected to be a boolean, but a string was provided./i)",
        @"browser.test.assertThrows(() => browser.tabs.executeScript({ file: 'path/to/file.js', frameId: -1 }), /'-1' is not a frame identifier./i)",

        @"browser.test.assertThrows(() => browser.tabs.insertCSS(), /a required argument is missing./i)",

        @"await browser.test.assertRejects(browser.tabs.insertCSS(9999, { code: 'code to execute' }), /tab not found/i)",

        @"browser.test.assertThrows(() => browser.tabs.insertCSS({ code: 'code to execute', file: 'path/to/file.js' }), /it cannot specify both 'file' and 'code'./i)",
        @"browser.test.assertThrows(() => browser.tabs.insertCSS({ }), /it must specify either 'file' or 'code'./i)",
        @"browser.test.assertThrows(() => browser.tabs.insertCSS({ file: 'path/to/file.js', frameId: '0' }), /'frameId' is expected to be a number, but a string was provided./i)",

        @"browser.test.assertThrows(() => browser.tabs.insertCSS({ code: 'code to execute', frameId: 0, allFrames: true }), /it cannot specify both 'allFrames' and 'frameId'./i)",
        @"browser.test.assertThrows(() => browser.tabs.insertCSS({ file: 'path/to/file.js', frameId: '0' }), /'frameId' is expected to be a number, but a string was provided./i)",
        @"browser.test.assertThrows(() => browser.tabs.insertCSS({ file: 'path/to/file.js', allFrames: 'true' }), /'allFrames' is expected to be a boolean, but a string was provided./i)",

        @"browser.test.assertThrows(() => browser.tabs.insertCSS({ file: 'path/to/file.js', frameId: -1 }), /'-1' is not a frame identifier./i)",

        @"browser.test.assertThrows(() => browser.tabs.insertCSS({ code: 'body { color: red }', cssOrigin: 'bad' }), /'cssOrigin' value is invalid, because it must specify either 'author' or 'user'/i);",

        @"browser.test.assertThrows(() => browser.tabs.removeCSS(), /a required argument is missing./i)",

        @"await browser.test.assertRejects(browser.tabs.removeCSS(9999, { code: 'code to execute' }), /tab not found/i)",

        @"browser.test.assertThrows(() => browser.tabs.removeCSS({ code: 'code to execute', file: 'path/to/file.js' }), /it cannot specify both 'file' and 'code'./i)",
        @"browser.test.assertThrows(() => browser.tabs.removeCSS({ }), /it must specify either 'file' or 'code'./i)",
        @"browser.test.assertThrows(() => browser.tabs.removeCSS({ file: 0, frameId: 0 }), /'file' is expected to be a string, but a number was provided./i)",

        @"browser.test.assertThrows(() => browser.tabs.removeCSS({ code: 'code to execute', frameId: 0, allFrames: true }), /it cannot specify both 'allFrames' and 'frameId'./i)",
        @"browser.test.assertThrows(() => browser.tabs.removeCSS({ file: 'path/to/file.js', allFrames: 'true' }), /'allFrames' is expected to be a boolean, but a string was provided./i)",

        @"browser.test.assertThrows(() => browser.tabs.removeCSS({ file: 'path/to/file.js', frameId: -1 }), /'-1' is not a frame identifier./i)",

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(tabsManifestV2, @{ @"background.js": backgroundScript });
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

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *window = manager.get().defaultWindow;
    auto originalOpenNewTab = manager.get().internalDelegate.openNewTab;

    manager.get().internalDelegate.openNewTab = ^(WKWebExtensionTabConfiguration *configuration, WKWebExtensionContext *context, void (^completionHandler)(id<WKWebExtensionTab>, NSError *)) {
        EXPECT_NS_EQUAL(configuration.window, window);
        EXPECT_EQ(configuration.index, window.tabs.count);

        EXPECT_NULL(configuration.parentTab);
        EXPECT_NULL(configuration.url);

        EXPECT_TRUE(configuration.shouldBeActive);
        EXPECT_TRUE(configuration.shouldAddToSelection);
        EXPECT_FALSE(configuration.shouldBePinned);
        EXPECT_FALSE(configuration.shouldBeMuted);
        EXPECT_FALSE(configuration.shouldReaderModeBeActive);

        originalOpenNewTab(configuration, context, completionHandler);
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
        @"})",

        @"const updatedWindows = await browser.windows.getAll({ populate: true })",
        @"const updatedTabs = updatedWindows[0].tabs",
        @"browser.test.assertEq(updatedTabs.length, initialTabsCount + 1, 'A new tab should have been added')",
        @"browser.test.assertEq(newTab.index, 1, 'The new tab should be at index 1')",
        @"browser.test.assertFalse(newTab.active, 'The new tab should not be active')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *window = manager.get().defaultWindow;
    auto *tab = manager.get().defaultTab;
    auto originalOpenNewTab = manager.get().internalDelegate.openNewTab;

    manager.get().internalDelegate.openNewTab = ^(WKWebExtensionTabConfiguration *configuration, WKWebExtensionContext *context, void (^completionHandler)(id<WKWebExtensionTab>, NSError *)) {
        EXPECT_NS_EQUAL(configuration.window, window);
        EXPECT_EQ(configuration.index, 1lu);

        EXPECT_NS_EQUAL(configuration.parentTab, tab);
        EXPECT_NS_EQUAL(configuration.url, [NSURL URLWithString:@"https://example.com/"]);

        EXPECT_FALSE(configuration.shouldBeActive);
        EXPECT_FALSE(configuration.shouldAddToSelection);
        EXPECT_TRUE(configuration.shouldBePinned);
        EXPECT_TRUE(configuration.shouldBeMuted);
        EXPECT_TRUE(configuration.shouldReaderModeBeActive);

        originalOpenNewTab(configuration, context, completionHandler);
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, CreateWithRelativeURL)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const newTab = await browser.tabs.create({",
        @"  url: 'test.html'",
        @"})",

        @"browser.test.assertEq(newTab.url, browser.runtime.getURL('test.html'), 'The new tab should have the correct URL')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript, @"test.html": @"Hello world!" }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto originalOpenNewTab = manager.get().internalDelegate.openNewTab;

    manager.get().internalDelegate.openNewTab = ^(WKWebExtensionTabConfiguration *configuration, WKWebExtensionContext *context, void (^completionHandler)(id<WKWebExtensionTab>, NSError *)) {
        EXPECT_NS_EQUAL(configuration.url, [NSURL URLWithString:@"test.html" relativeToURL:manager.get().context.baseURL].absoluteURL);

        originalOpenNewTab(configuration, context, completionHandler);
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

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *window = manager.get().defaultWindow;
    auto *tab = manager.get().defaultTab;
    auto originalDuplicate = tab.duplicate;

    tab.duplicate = ^(WKWebExtensionTabConfiguration *configuration, void (^completionHandler)(TestWebExtensionTab *, NSError *)) {
        EXPECT_NS_EQUAL(configuration.window, window);
        EXPECT_EQ(configuration.index, window.tabs.count);

        EXPECT_NULL(configuration.parentTab);
        EXPECT_NULL(configuration.url);

        EXPECT_TRUE(configuration.shouldBeActive);
        EXPECT_TRUE(configuration.shouldAddToSelection);
        EXPECT_FALSE(configuration.shouldBePinned);
        EXPECT_FALSE(configuration.shouldBeMuted);
        EXPECT_FALSE(configuration.shouldReaderModeBeActive);

        originalDuplicate(configuration, completionHandler);
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
        @"})",

        @"const updatedWindows = await browser.windows.getAll({ populate: true })",
        @"const updatedTabs = updatedWindows[0].tabs",
        @"browser.test.assertEq(updatedTabs.length, initialTabsCount + 1, 'A tab should have been duplicated')",
        @"browser.test.assertEq(duplicatedTab.index, 1, 'The duplicated tab should be at index 1')",
        @"browser.test.assertFalse(duplicatedTab.active, 'The duplicated tab should not be active')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *window = manager.get().defaultWindow;
    auto *tab = manager.get().defaultTab;
    auto originalDuplicate = tab.duplicate;

    tab.duplicate = ^(WKWebExtensionTabConfiguration *configuration, void (^completionHandler)(TestWebExtensionTab *, NSError *)) {
        EXPECT_NS_EQUAL(configuration.window, window);
        EXPECT_EQ(configuration.index, 1lu);

        EXPECT_NULL(configuration.parentTab);
        EXPECT_NULL(configuration.url);

        EXPECT_FALSE(configuration.shouldBeActive);
        EXPECT_FALSE(configuration.shouldAddToSelection);
        EXPECT_FALSE(configuration.shouldBePinned);
        EXPECT_FALSE(configuration.shouldBeMuted);
        EXPECT_FALSE(configuration.shouldReaderModeBeActive);

        originalDuplicate(configuration, completionHandler);
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
        @"})",

        @"browser.test.assertTrue(updatedTab.active, 'The tab should be active after update')",
        @"browser.test.assertTrue(updatedTab.highlighted, 'The tab should be highlighted after update')",
        @"browser.test.assertTrue(updatedTab.pinned, 'The tab should be pinned after update')",
        @"browser.test.assertTrue(updatedTab.mutedInfo.muted, 'The tab should be muted after update')",
        @"browser.test.assertEq(updatedTab.openerTabId, allWindows[0].tabs[0].id, 'The openerTabId should match the first tab after update')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().defaultWindow openNewTab];

    EXPECT_EQ(manager.get().defaultWindow.tabs.count, 2lu);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, UpdateWithoutTabId)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const allWindows = await browser.windows.getAll({ populate: true })",
        @"const activeTab = allWindows[0].tabs[0]",

        @"browser.test.assertFalse(activeTab.mutedInfo.muted, 'The tab should not be initially muted')",

        @"const updatedTab = await browser.tabs.update({",
        @"  muted: true,",
        @"})",

        @"browser.test.assertTrue(updatedTab.mutedInfo.muted, 'The tab should be muted after update')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    EXPECT_EQ(manager.get().defaultWindow.tabs.count, 1lu);

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

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().defaultWindow openNewTab];

    EXPECT_EQ(manager.get().defaultWindow.tabs.count, 2lu);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, GetCurrentFromBackgroundPage)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const [currentTab] = await browser.tabs.query({ active: true, currentWindow: true })",
        @"const tab = await browser.tabs.getCurrent()",

        @"browser.test.assertDeepEq(tab, currentTab, 'The current tab should be')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().defaultWindow openNewTab];

    EXPECT_EQ(manager.get().defaultWindow.tabs.count, 2lu);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, GetCurrentFromOptionsPage)
{
    auto *optionsScript = Util::constructScript(@[
        @"const tab = await browser.tabs.getCurrent()",

        @"browser.test.assertEq(typeof tab, 'object', 'The tab should be')",
        @"browser.test.assertTrue(tab.active, 'The current tab should be active')",

        @"browser.test.notifyPass()"
    ]);

    auto *resources = @{
        @"background.js": @"// Not Used",
        @"options.html": @"<script type='module' src='options.js'></script>",
        @"options.js": optionsScript
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager load];

    [manager.get().defaultWindow openNewTab];

    EXPECT_EQ(manager.get().defaultWindow.tabs.count, 2lu);

    auto *optionsPageURL = manager.get().context.optionsPageURL;
    EXPECT_NOT_NULL(optionsPageURL);

    auto *defaultTab = manager.get().defaultTab;
    EXPECT_NOT_NULL(defaultTab);

    [defaultTab changeWebViewIfNeededForURL:optionsPageURL forExtensionContext:manager.get().context];
    [defaultTab.webView loadRequest:[NSURLRequest requestWithURL:optionsPageURL]];

    [manager run];
}

TEST(WKWebExtensionAPITabs, GetSelected)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const [selectedTab] = await browser.tabs.query({ active: true, currentWindow: true })",
        @"const tab = await browser.tabs.getSelected()",

        @"browser.test.assertDeepEq(tab, selectedTab, 'The selected tab should match the active tab in the current window')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifestV2 resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().defaultWindow openNewTab];

    EXPECT_EQ(manager.get().defaultWindow.tabs.count, 2lu);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, Query)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const allWindows = await browser.windows.getAll({ populate: true })",
        @"browser.test.assertEq(allWindows?.length, 2, 'There should be 2 windows')",

        @"const windowIdOne = allWindows?.[0]?.id",
        @"const windowIdTwo = allWindows?.[1]?.id",

        @"const tabIdOne = allWindows?.[0]?.tabs?.[0]?.id",
        @"const tabIdTwo = allWindows?.[0]?.tabs?.[1]?.id",
        @"const tabIdThree = allWindows?.[1]?.tabs?.[0]?.id",
        @"const tabIdFour = allWindows?.[1]?.tabs?.[1]?.id",
        @"const tabIdFive = allWindows?.[1]?.tabs?.[2]?.id",

        @"const tabsInWindowOne = await browser.tabs.query({ windowId: windowIdOne })",
        @"const tabsInWindowTwo = await browser.tabs.query({ windowId: windowIdTwo })",
        @"browser.test.assertEq(tabsInWindowOne?.length, 2, 'There should be 2 tabs in the first window')",
        @"browser.test.assertEq(tabsInWindowTwo?.length, 3, 'There should be 3 tabs in the second window')",

        @"const thirdTab = await browser.tabs.query({ index: 0, windowId: windowIdTwo })",
        @"browser.test.assertEq(thirdTab?.[0]?.id, tabIdThree, 'Third tab ID should match the first tab of the second window')",

        @"const activeTabs = await browser.tabs.query({ active: true })",
        @"browser.test.assertEq(activeTabs?.length, 2, 'There should be 2 active tabs across all windows')",

        @"const hiddenTabs = await browser.tabs.query({ hidden: true })",
        @"browser.test.assertEq(hiddenTabs?.length, 3, 'There should be 3 hidden tabs across all windows')",

        @"const lastFocusedTabs = await browser.tabs.query({ lastFocusedWindow: true })",
        @"browser.test.assertEq(lastFocusedTabs?.length, 2, 'There should be 2 tabs in the last focused window')",

        @"const pinnedTabs = await browser.tabs.query({ pinned: true })",
        @"browser.test.assertEq(pinnedTabs?.length, 0, 'There should be no pinned tabs')",

        @"const loadingTabs = await browser.tabs.query({ status: 'loading' })",
        @"browser.test.assertEq(loadingTabs?.length, 0, 'There should be no tabs loading')",

        @"const completeTabs = await browser.tabs.query({ status: 'complete' })",
        @"browser.test.assertEq(completeTabs?.length, 5, 'There should be 5 tabs with loading complete')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *windowOne = manager.get().defaultWindow;
    [windowOne openNewTab];

    auto *windowTwo = [manager openNewWindow];
    [windowTwo openNewTab];
    [windowTwo openNewTab];

    auto *windowThree = [manager openNewWindowUsingPrivateBrowsing:YES];
    [windowThree openNewTab];

    EXPECT_EQ(manager.get().windows.count, 3lu);
    EXPECT_EQ(windowOne.tabs.count, 2lu);
    EXPECT_EQ(windowTwo.tabs.count, 3lu);
    EXPECT_EQ(windowThree.tabs.count, 2lu);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, QueryWithPrivateAccess)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const allWindows = await browser.windows.getAll({ populate: true })",
        @"const normalWindows = allWindows.filter(window => !window.incognito)",
        @"const incognitoWindows = allWindows.filter(window => window.incognito)",

        @"const windowIdOne = normalWindows?.[0]?.id",
        @"const windowIdTwo = normalWindows?.[1]?.id",
        @"const incognitoWindowId = incognitoWindows?.[0]?.id",

        @"const tabIdOne = normalWindows?.[0]?.tabs?.[0]?.id",
        @"const tabIdTwo = normalWindows?.[0]?.tabs?.[1]?.id",
        @"const tabIdThree = normalWindows?.[1]?.tabs?.[0]?.id",
        @"const tabIdFour = normalWindows?.[1]?.tabs?.[1]?.id",
        @"const tabIdFive = normalWindows?.[1]?.tabs?.[2]?.id",
        @"const incognitoTabIdOne = incognitoWindows?.[0]?.tabs?.[0]?.id",
        @"const incognitoTabIdTwo = incognitoWindows?.[0]?.tabs?.[1]?.id",

        @"const tabsInWindowOne = await browser.tabs.query({ windowId: windowIdOne })",
        @"const tabsInWindowTwo = await browser.tabs.query({ windowId: windowIdTwo })",
        @"const tabsInIncognitoWindow = await browser.tabs.query({ windowId: incognitoWindowId })",

        @"browser.test.assertEq(tabsInWindowOne?.length, 2, 'There should be 2 tabs in the first normal window')",
        @"browser.test.assertEq(tabsInWindowTwo?.length, 3, 'There should be 3 tabs in the second normal window')",
        @"browser.test.assertEq(tabsInIncognitoWindow?.length, 2, 'There should be 2 tabs in the incognito window')",

        @"const thirdTab = await browser.tabs.query({ index: 0, windowId: windowIdTwo })",
        @"browser.test.assertEq(thirdTab?.[0]?.id, tabIdThree, 'Third tab ID should match the first tab of the second window')",

        @"const activeTabs = await browser.tabs.query({ active: true })",
        @"browser.test.assertEq(activeTabs?.length, 3, 'There should be 3 active tabs across all windows')",

        @"const hiddenTabs = await browser.tabs.query({ hidden: true })",
        @"browser.test.assertEq(hiddenTabs?.length, 4, 'There should be 4 hidden tabs across all windows')",

        @"const lastFocusedTabs = await browser.tabs.query({ lastFocusedWindow: true })",
        @"browser.test.assertEq(lastFocusedTabs?.length, 2, 'There should be 2 tabs in the last focused window')",

        @"const pinnedTabs = await browser.tabs.query({ pinned: true })",
        @"browser.test.assertEq(pinnedTabs?.length, 0, 'There should be no pinned tabs')",

        @"const loadingTabs = await browser.tabs.query({ status: 'loading' })",
        @"browser.test.assertEq(loadingTabs?.length, 0, 'There should be no tabs loading')",

        @"const completeTabs = await browser.tabs.query({ status: 'complete' })",
        @"browser.test.assertEq(completeTabs?.length, 7, 'There should be 7 tabs with loading complete')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().context.hasAccessToPrivateData = YES;

    auto *windowOne = manager.get().defaultWindow;
    [windowOne openNewTab];

    auto *windowTwo = [manager openNewWindow];
    [windowTwo openNewTab];
    [windowTwo openNewTab];

    auto *windowThree = [manager openNewWindowUsingPrivateBrowsing:YES];
    [windowThree openNewTab];

    EXPECT_EQ(manager.get().windows.count, 3lu);
    EXPECT_EQ(windowOne.tabs.count, 2lu);
    EXPECT_EQ(windowTwo.tabs.count, 3lu);
    EXPECT_EQ(windowThree.tabs.count, 2lu);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, QueryWithAccessPrompt)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"const allWindows = await browser.windows.getAll({ populate: true })",
        @"const windowIdOne = allWindows?.[0]?.id",

        @"const tabIdOne = allWindows?.[0]?.tabs?.[0]?.id",
        @"const tabIdTwo = allWindows?.[0]?.tabs?.[1]?.id",

        @"browser.test.assertTrue(allWindows?.[0]?.tabs?.[0]?.url.startsWith('http://localhost'), 'First tab URL should be localhost')",
        @"browser.test.assertEq(allWindows?.[0]?.tabs?.[1]?.url, '', 'Second tab URL should be empty')",

        @"const tabsInWindowOne = await browser.tabs.query({ windowId: windowIdOne })",

        @"browser.test.assertTrue(tabsInWindowOne.length >= 2, 'There should be at least 2 tabs in window one')",
        @"browser.test.assertTrue(tabsInWindowOne[0].url.startsWith('http://localhost'), 'First tab URL should be localhost')",
        @"browser.test.assertEq(tabsInWindowOne[1].url, '', 'Second tab URL should be empty')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionTabs];

    __block bool firstPermissionRequest = true;
    auto *localhostURL = server.requestWithLocalhost().URL;
    auto *loopbackURL = server.request().URL;

    manager.get().internalDelegate.promptForPermissionToAccessURLs = ^(id<WKWebExtensionTab>, NSSet<NSURL *> *requestedURLs, void (^completionHandler)(NSSet<NSURL *> *allowedURLs, NSDate *)) {
        if (firstPermissionRequest) {
            EXPECT_TRUE([requestedURLs containsObject:localhostURL]);
            EXPECT_TRUE([requestedURLs containsObject:loopbackURL]);

            // Only approve localhost for the first request.
            dispatch_async(dispatch_get_main_queue(), ^{
                completionHandler([NSSet setWithObject:localhostURL], nil);
            });

            firstPermissionRequest = false;
        } else {
            // The second request should only have loopbackURL, as localhostURL was granted previously.
            EXPECT_FALSE([requestedURLs containsObject:localhostURL]);
            EXPECT_TRUE([requestedURLs containsObject:loopbackURL]);

            // Approve nothing for the second request.
            dispatch_async(dispatch_get_main_queue(), ^{
                completionHandler(NSSet.set, nil);
            });
        }
    };

    auto *windowOne = manager.get().defaultWindow;
    [windowOne openNewTab];

    [windowOne.tabs.firstObject.webView loadRequest:server.requestWithLocalhost()];
    [windowOne.tabs.lastObject.webView loadRequest:server.request()];

    EXPECT_EQ(manager.get().windows.count, 1lu);
    EXPECT_EQ(windowOne.tabs.count, 2lu);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, QueryWithCurrentWindow)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const tabs = await browser.tabs.query({ windowId: browser.windows.WINDOW_ID_CURRENT })",
        @"browser.test.assertEq(tabs.length, 2, 'Should return exactly two tabs for the current window')",

        @"browser.test.assertEq(typeof tabs[0].id, 'number', 'The first tab should have a valid id')",
        @"browser.test.assertEq(typeof tabs[0].windowId, 'number', 'The first tab should have a valid windowId')",

        @"browser.test.assertEq(typeof tabs[1].id, 'number', 'The second tab should have a valid id')",
        @"browser.test.assertEq(typeof tabs[1].windowId, 'number', 'The second tab should have a valid windowId')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().defaultWindow openNewTab];

    EXPECT_EQ(manager.get().defaultWindow.tabs.count, 2lu);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, QueryWithPermissionBypass)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"const allWindows = await browser.windows.getAll({ populate: true })",
        @"const windowIdOne = allWindows?.[0]?.id",

        @"const tabIdOne = allWindows?.[0]?.tabs?.[0]?.id",
        @"const tabIdTwo = allWindows?.[0]?.tabs?.[1]?.id",

        @"browser.test.assertTrue(allWindows?.[0]?.tabs?.[0]?.url.startsWith('http://localhost'), 'First tab URL should be localhost')",
        @"browser.test.assertEq(allWindows?.[0]?.tabs?.[1]?.url, '', 'Second tab URL should be empty')",

        @"const tabsInWindowOne = await browser.tabs.query({ windowId: windowIdOne })",

        @"browser.test.assertTrue(tabsInWindowOne.length >= 2, 'There should be at least 2 tabs in window one')",
        @"browser.test.assertTrue(tabsInWindowOne[0].url.startsWith('http://localhost'), 'First tab URL should be localhost')",
        @"browser.test.assertEq(tabsInWindowOne[1].url, '', 'Second tab URL should be empty')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionTabs];

    manager.get().internalDelegate.promptForPermissionToAccessURLs = ^(id<WKWebExtensionTab>, NSSet<NSURL *> *requestedURLs, void (^completionHandler)(NSSet<NSURL *> *allowedURLs, NSDate *)) {
        dispatch_async(dispatch_get_main_queue(), ^{
            completionHandler(NSSet.set, nil);
        });
    };

    auto *windowOne = manager.get().defaultWindow;
    [windowOne openNewTab];
    windowOne.tabs.firstObject.shouldBypassPermissions = YES;

    [windowOne.tabs.firstObject.webView loadRequest:server.requestWithLocalhost()];
    [windowOne.tabs.lastObject.webView loadRequest:server.request()];

    EXPECT_EQ(manager.get().windows.count, 1lu);
    EXPECT_EQ(windowOne.tabs.count, 2lu);

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

    Util::loadAndRunExtension(tabsManifest, @{ @"background.js": backgroundScript });
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

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    __block size_t toggleReaderModeCounter = 0;

    manager.get().defaultTab.setReaderModeShowing = ^(BOOL showing) {
        ++toggleReaderModeCounter;
    };

    [manager loadAndRun];

    ASSERT_EQ(toggleReaderModeCounter, 2lu);
}

TEST(WKWebExtensionAPITabs, DetectLanguage)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"const allWindows = await browser.windows.getAll({ populate: true })",
        @"const tabId = allWindows[0].tabs[0].id",

        @"const detectedLanguage = await browser.tabs.detectLanguage(tabId)",

        @"browser.test.assertEq(detectedLanguage, 'en-US', 'The detected language should be English')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    __block bool detectWebpageLocaleCalled = false;

    manager.get().defaultTab.webpageLocale = ^{
        detectWebpageLocaleCalled = true;
        return [NSLocale localeWithLocaleIdentifier:@"en-US"];
    };

    [manager loadAndRun];

    ASSERT_TRUE(detectWebpageLocaleCalled);
}

TEST(WKWebExtensionAPITabs, CaptureVisibleTab)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const newTab = await browser.tabs.create({ url: 'http://example.com' })",

        @"let dataURLPNG = await browser.tabs.captureVisibleTab(newTab.windowId, { format: 'png' })",
        @"browser.test.assertTrue(dataURLPNG.startsWith('data:image/png;'), 'The data URL should start with the PNG mime type for PNG capture')",

        @"let dataURLJPEG = await browser.tabs.captureVisibleTab(newTab.windowId, { format: 'jpeg', quality: 90 })",
        @"browser.test.assertTrue(dataURLJPEG.startsWith('data:image/jpeg;'), 'The data URL should start with the JPEG mime type for JPEG capture')",

        @"browser.test.notifyPass()",
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:[NSURL URLWithString:@"http://example.com/"]];

    [manager loadAndRun];
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

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    __block bool reloadCalled = false;
    __block bool reloadFromOriginCalled = false;

    manager.get().defaultTab.reload = ^(BOOL fromOrigin) {
        if (fromOrigin)
            reloadFromOriginCalled = true;
        else
            reloadCalled = true;
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

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    __block bool goBackCalled = false;

    manager.get().defaultTab.goBack = ^{
        goBackCalled = true;
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

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    __block bool goForwardCalled = false;

    manager.get().defaultTab.goForward = ^{
        goForwardCalled = true;
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

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().defaultWindow openNewTab];

    EXPECT_EQ(manager.get().defaultWindow.tabs.count, 2lu);

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

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().defaultWindow openNewTab];
    [manager.get().defaultWindow openNewTab];

    EXPECT_EQ(manager.get().defaultWindow.tabs.count, 3lu);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, CreatedEvent)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.tabs.onCreated.addListener((newTab) => {",
        @"  browser.test.assertFalse(newTab.active, 'The new tab should not be active')",
        @"  browser.test.assertFalse(newTab.pinned, 'The new tab should not be pinned')",
        @"  browser.test.assertEq(newTab.index, 1, 'The new tab index should be 1')",
        @"  browser.test.assertEq(newTab.openerTabId, undefined, 'The new tab should not have an openerTabId by default')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.tabs.create({})",
    ]);

    Util::loadAndRunExtension(tabsManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPITabs, UpdatedEvent)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const newTab = await browser.tabs.create({ active: false, muted: false, pinned: false })",

        @"let mutedUpdated = false",
        @"let pinnedUpdated = false",

        @"const verifyUpdates = () => {",
        @"  if (mutedUpdated && pinnedUpdated)",
        @"    browser.test.notifyPass()",
        @"}",

        @"browser.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {",
        @"  if ('mutedInfo' in changeInfo) {",
        @"    browser.test.assertEq(tabId, newTab.id, 'The updated tab should have the correct id for muted change')",
        @"    browser.test.assertTrue(changeInfo.mutedInfo.muted, 'The tab should be muted')",
        @"    browser.test.assertDeepEq(changeInfo.mutedInfo, tab.mutedInfo, 'The mutedInfo in changeInfo should match the mutedInfo of the tab')",
        @"    mutedUpdated = true",
        @"  }",

        @"  if ('pinned' in changeInfo) {",
        @"    browser.test.assertEq(tabId, newTab.id, 'The updated tab should have the correct id for pinned change')",
        @"    browser.test.assertTrue(changeInfo.pinned, 'The tab should be pinned')",
        @"    browser.test.assertEq(changeInfo.pinned, tab.pinned, 'The pinned status in changeInfo should match the pinned status of the tab')",
        @"    pinnedUpdated = true",
        @"  }",

        @"  verifyUpdates()",
        @"})",

        @"browser.tabs.update(newTab.id, { muted: true, pinned: true })"
    ]);

    Util::loadAndRunExtension(tabsManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPITabs, UpdatedEventWithoutPrivateAccess)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {",
        @"  browser.test.notifyFail('tabs.onUpdated should not fire for private tabs when no permission is granted.')",
        @"})",

        @"setTimeout(() => browser.test.notifyPass(), 2000)",

        @"browser.test.yield('Load Tab')"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    auto *privateWindow = [manager openNewWindowUsingPrivateBrowsing:YES];
    [privateWindow.activeTab.webView loadRequest:server.requestWithLocalhost()];

    [manager run];
}

TEST(WKWebExtensionAPITabs, UpdatedEventWithPrivateAccess)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {",
        @"  browser.test.notifyPass()",
        @"})",

        @"setTimeout(() => browser.test.notifyFail('tabs.onUpdated did not fire for private tab.'), 2000)",

        @"browser.test.yield('Load Tab')"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];

    manager.get().context.hasAccessToPrivateData = YES;

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    auto *privateWindow = [manager openNewWindowUsingPrivateBrowsing:YES];
    [privateWindow.activeTab.webView loadRequest:server.requestWithLocalhost()];

    [manager run];
}

TEST(WKWebExtensionAPITabs, RemovedEvent)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const newTab = await browser.tabs.create({})",

        @"browser.tabs.onRemoved.addListener((tabId, removeInfo) => {",
        @"  browser.test.assertEq(tabId, newTab.id, 'The removed tab should have the correct id')",
        @"  browser.test.assertFalse(removeInfo.isWindowClosing, 'The window should not be closing')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.tabs.remove(newTab.id)"
    ]);

    Util::loadAndRunExtension(tabsManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPITabs, ReplacedEvent)
{
    auto backgroundScript = Util::constructScript(@[
        @"let createdTabId = null",
        @"let removedTabId = null",

        @"browser.tabs.onCreated.addListener((tab) => {",
        @"  browser.test.assertEq(createdTabId, null, 'No tab should be created yet')",
        @"  createdTabId = tab.id",
        @"})",

        @"browser.tabs.onRemoved.addListener((tabId) => {",
        @"  browser.test.assertEq(removedTabId, null, 'No tab should be removed yet')",
        @"  removedTabId = tabId",
        @"})",

        @"browser.tabs.onReplaced.addListener((addedTabId, removedTabId) => {",
        @"  browser.test.assertTrue(addedTabId !== removedTabId, 'The added tab should not match the removed tab')",
        @"  browser.test.assertEq(addedTabId, createdTabId, 'The added tab should match the created tab')",
        @"  browser.test.assertEq(removedTabId, initialTabId, 'The removed tab should match the initial tab')",

        @"  browser.test.notifyPass()",
        @"})",

        @"const allTabs = await browser.tabs.query({windowId: browser.windows.WINDOW_ID_CURRENT})",
        @"const initialTabId = allTabs[0].id",

        @"browser.test.yield('Replace Tab')"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Replace Tab");

    auto initialTab = manager.get().defaultWindow.tabs.firstObject;
    auto newTab = adoptNS([[TestWebExtensionTab alloc] initWithWindow:manager.get().defaultWindow extensionController:manager.get().controller]);

    [manager.get().defaultWindow replaceTab:initialTab withTab:newTab.get()];
    [manager run];

    EXPECT_EQ(manager.get().defaultWindow.tabs.count, 1lu);
}

TEST(WKWebExtensionAPITabs, MovedEvent)
{
    auto backgroundScript = Util::constructScript(@[
        @"browser.tabs.onMoved.addListener((tabId, moveInfo) => {",
        @"  browser.test.assertEq(tabId, movedTabId, 'Moved tab id should match the provided tab id')",
        @"  browser.test.assertEq(moveInfo.fromIndex, 1, 'Tab should have been moved from index 1')",
        @"  browser.test.assertEq(moveInfo.toIndex, 2, 'Tab should have been moved to index 2')",

        @"  browser.test.notifyPass()",
        @"})",

        @"const allTabs = await browser.tabs.query({ currentWindow: true })",
        @"const movedTabId = allTabs[1].id",

        @"browser.test.yield('Move Tab')"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *tabToMove = [manager.get().defaultWindow openNewTab];
    [manager.get().defaultWindow openNewTab];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Move Tab");

    [manager.get().defaultWindow moveTab:tabToMove toIndex:2];

    [manager run];
}

TEST(WKWebExtensionAPITabs, DetachedAndAttachedEvent)
{
    auto backgroundScript = Util::constructScript(@[
        @"let detachedTabId",

        @"const currentWindow = await browser.windows.getCurrent()",
        @"const currentWindowId = currentWindow.id",

        @"const allWindows = await browser.windows.getAll()",
        @"const newWindow = allWindows.find(window => window.id !== currentWindowId)",
        @"const newWindowId = newWindow.id",

        @"browser.tabs.onDetached.addListener((tabId, detachInfo) => {",
        @"  detachedTabId = tabId",
        @"  browser.test.assertEq(detachInfo.oldWindowId, currentWindowId, 'Tab should have been detached from the current window')",
        @"  browser.test.assertEq(detachInfo.oldPosition, 1, 'Tab should have been detached from index 1')",
        @"})",

        @"browser.tabs.onAttached.addListener((tabId, attachInfo) => {",
        @"  browser.test.assertEq(detachedTabId, tabId, 'The detached and attached tab ids should match')",
        @"  browser.test.assertEq(attachInfo.newWindowId, newWindowId, 'Tab should have been attached to the new window')",
        @"  browser.test.assertEq(attachInfo.newPosition, 0, 'Tab should have been attached at index 0')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Move Tab')"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *tabToMove = [manager.get().defaultWindow openNewTab];
    auto *secondWindow = [manager openNewWindow];

    EXPECT_EQ(manager.get().defaultWindow.tabs.count, 2ul);
    EXPECT_EQ(secondWindow.tabs.count, 1ul);

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Move Tab");

    [secondWindow moveTab:tabToMove toIndex:0];

    EXPECT_EQ(manager.get().defaultWindow.tabs.count, 1ul);
    EXPECT_EQ(secondWindow.tabs.count, 2ul);

    [manager run];
}

TEST(WKWebExtensionAPITabs, DetachAndAttachToWindowIDNone)
{
    auto backgroundScript = Util::constructScript(@[
        @"let detachedTabId",

        @"const currentWindow = await browser.windows.getCurrent()",
        @"const currentWindowId = currentWindow.id",

        @"browser.tabs.onDetached.addListener((tabId, detachInfo) => {",
        @"  detachedTabId = tabId",
        @"  browser.test.assertEq(detachInfo.oldWindowId, currentWindowId, 'Tab should have been detached from the current window')",
        @"  browser.test.assertEq(detachInfo.oldPosition, 1, 'Tab should have been detached from index 1')",
        @"})",

        @"browser.tabs.onAttached.addListener((tabId, attachInfo) => {",
        @"  browser.test.assertEq(detachedTabId, tabId, 'The detached and attached tab ids should match')",
        @"  browser.test.assertEq(attachInfo.newWindowId, browser.windows.WINDOW_ID_NONE, 'Tab should have been attached to WINDOW_ID_NONE')",
        @"  browser.test.assertTrue(isNaN(attachInfo.newPosition), 'Tab should have been attached at index NaN')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Detach Tab')"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *tabToMove = [manager.get().defaultWindow openNewTab];

    EXPECT_EQ(manager.get().defaultWindow.tabs.count, 2ul);

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Detach Tab");

    tabToMove.window = nil;

    EXPECT_EQ(manager.get().defaultWindow.tabs.count, 1ul);

    [manager run];
}

TEST(WKWebExtensionAPITabs, DetachAndAttachFromWindowIDNone)
{
    auto backgroundScript = Util::constructScript(@[
        @"let detachedTabId",

        @"const currentWindow = await browser.windows.getCurrent()",
        @"const currentWindowId = currentWindow.id",

        @"browser.tabs.onDetached.addListener((tabId, detachInfo) => {",
        @"  detachedTabId = tabId",
        @"  browser.test.assertEq(detachInfo.oldWindowId, browser.windows.WINDOW_ID_NONE, 'Tab should have been detached from WINDOW_ID_NONE')",
        @"  browser.test.assertTrue(isNaN(detachInfo.oldPosition), 'Tab should have been detached from index NaN')",
        @"})",

        @"browser.tabs.onAttached.addListener((tabId, attachInfo) => {",
        @"  browser.test.assertEq(detachedTabId, tabId, 'The detached and attached tab ids should match')",
        @"  browser.test.assertEq(attachInfo.newWindowId, currentWindowId, 'Tab should have been attached to the current window')",
        @"  browser.test.assertEq(attachInfo.newPosition, 1, 'Tab should have been attached to index 1')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Attach Tab')"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *tabToMove = [manager.get().defaultWindow openNewTab];

    EXPECT_EQ(manager.get().defaultWindow.tabs.count, 2ul);

    // Detach before loading the extension.
    tabToMove.window = nil;

    EXPECT_EQ(manager.get().defaultWindow.tabs.count, 1ul);

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Attach Tab");

    tabToMove.window = manager.get().defaultWindow;

    EXPECT_EQ(manager.get().defaultWindow.tabs.count, 2ul);

    [manager run];
}


TEST(WKWebExtensionAPITabs, ActivatedEvent)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const newTab = await browser.tabs.create({ active: false })",

        @"browser.tabs.onActivated.addListener((activeInfo) => {",
        @"  browser.test.assertEq(activeInfo.tabId, newTab.id, 'The activated tab should have the correct id')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.tabs.update(newTab.id, { active: true })"
    ]);

    Util::loadAndRunExtension(tabsManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPITabs, HighlightedEvent)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const initialTabs = await browser.tabs.query({ active: true, currentWindow: true })",
        @"const newTab = await browser.tabs.create({ active: false })",

        @"browser.tabs.onActivated.addListener((activeInfo) => {",
        @"  browser.test.notifyFail('The tab should be highlighted but not activated')",
        @"})",

        @"browser.tabs.onHighlighted.addListener((highlightInfo) => {",
        @"  browser.test.assertTrue(highlightInfo.tabIds.includes(newTab.id), 'The highlighted tabs should contain the new tab id')",
        @"  browser.test.assertTrue(highlightInfo.tabIds.includes(initialTabs[0].id), 'The initial tab should still be highlighted')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.tabs.update(newTab.id, { active: false, highlighted: true })"
    ]);

    Util::loadAndRunExtension(tabsManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPITabs, HighlightedAlsoActivatesTab)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const newTab = await browser.tabs.create({ active: false })",

        @"let tabActivated = false",
        @"let tabHighlighted = false",

        @"let checkCompletion = () => {",
        @"  if (tabActivated && tabHighlighted)",
        @"    browser.test.notifyPass()",
        @"}",

        @"browser.tabs.onActivated.addListener((activeInfo) => {",
        @"  tabActivated = true",
        @"  browser.test.assertEq(activeInfo.tabId, newTab.id, 'The activated tab should have the correct id')",
        @"  checkCompletion()",
        @"})",

        @"browser.tabs.onHighlighted.addListener((highlightInfo) => {",
        @"  tabHighlighted = true",
        @"  browser.test.assertTrue(highlightInfo.tabIds.includes(newTab.id), 'The highlighted tabs should contain the new tab id')",
        @"  checkCompletion()",
        @"})",

        @"browser.tabs.update(newTab.id, { highlighted: true })"
    ]);

    Util::loadAndRunExtension(tabsManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPITabs, SendMessage)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener(async (message, sender, sendResponse) => {",
        @"  browser.test.assertEq(message, 'Ready')",

        @"  const tabs = await browser.tabs.query({ active: true, currentWindow: true })",
        @"  const tabId = tabs[0].id",

        @"  const response = await browser.test.assertSafeResolve(() => browser.tabs.sendMessage(tabId, { content: 'Hello' }))",
        @"  browser.test.assertEq(response?.content, 'Received', 'Should get the response from the content script')",

        @"  browser.test.notifyPass()",
        @"})"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener((message, sender, sendResponse) => {",
        @"  browser.test.assertEq(message.content, 'Hello', 'Should receive the correct message content')",

        @"  browser.test.assertEq(typeof sender, 'object', 'sender should be an object')",

        @"  browser.test.assertEq(typeof sender.url, 'string', 'sender.url should be a string')",
        @"  browser.test.assertEq(typeof sender.origin, 'string', 'sender.origin should be a string')",

        @"  browser.test.assertTrue(sender.url.startsWith('webkit-extension://'), 'sender.url should start with webkit-extension://')",
        @"  browser.test.assertTrue(sender.origin.startsWith('webkit-extension://'), 'sender.origin should start with webkit-extension://')",

        @"  browser.test.assertEq(sender.tab, undefined, 'sender.tab should be undefined')",
        @"  browser.test.assertEq(sender.frameId, undefined, 'sender.frameId should be undefined')",

        @"  sendResponse({ content: 'Received' })",
        @"})",

        @"setTimeout(() => browser.runtime.sendMessage('Ready'), 1000)"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsContentScriptManifest resources:@{ @"background.js": backgroundScript, @"content.js": contentScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, SendMessageWithAsyncReply)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener(async (message, sender, sendResponse) => {",
        @"  browser.test.assertEq(message, 'Ready')",

        @"  const tabs = await browser.tabs.query({ active: true, currentWindow: true })",
        @"  const tabId = tabs[0].id",

        @"  const response = await browser.test.assertSafeResolve(() => browser.tabs.sendMessage(tabId, { content: 'Hello' }))",
        @"  browser.test.assertEq(response?.content, 'Received', 'Should get the response from the content script')",

        @"  browser.test.notifyPass()",
        @"})"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener((message, sender, sendResponse) => {",
        @"  browser.test.assertEq(message.content, 'Hello', 'Should receive the correct message content')",

        @"  browser.test.assertEq(typeof sender, 'object', 'sender should be an object')",

        @"  browser.test.assertEq(typeof sender.url, 'string', 'sender.url should be a string')",
        @"  browser.test.assertEq(typeof sender.origin, 'string', 'sender.origin should be a string')",

        @"  browser.test.assertTrue(sender.url.startsWith('webkit-extension://'), 'sender.url should start with webkit-extension://')",
        @"  browser.test.assertTrue(sender.origin.startsWith('webkit-extension://'), 'sender.origin should start with webkit-extension://')",

        @"  browser.test.assertEq(sender.tab, undefined, 'sender.tab should be undefined')",
        @"  browser.test.assertEq(sender.frameId, undefined, 'sender.frameId should be undefined')",

        @"  setTimeout(() => sendResponse({ content: 'Received' }), 1000)",

        @"  return true",
        @"})",

        @"setTimeout(() => browser.runtime.sendMessage('Ready'), 1000)"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsContentScriptManifest resources:@{ @"background.js": backgroundScript, @"content.js": contentScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, SendMessageWithPromiseReply)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener(async (message, sender) => {",
        @"  browser.test.assertEq(message, 'Ready')",

        @"  const tabs = await browser.tabs.query({ active: true, currentWindow: true })",
        @"  const tabId = tabs[0].id",

        @"  const response = await browser.test.assertSafeResolve(() => browser.tabs.sendMessage(tabId, { content: 'Hello' }))",
        @"  browser.test.assertEq(response?.content, 'Received', 'Should get the response from the content script')",

        @"  browser.test.notifyPass()",
        @"})"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener((message, sender) => {",
        @"  browser.test.assertEq(message.content, 'Hello', 'Should receive the correct message content')",

        @"  browser.test.assertEq(typeof sender, 'object', 'sender should be an object')",

        @"  browser.test.assertEq(typeof sender.url, 'string', 'sender.url should be a string')",
        @"  browser.test.assertEq(typeof sender.origin, 'string', 'sender.origin should be a string')",

        @"  browser.test.assertTrue(sender.url.startsWith('webkit-extension://'), 'sender.url should start with webkit-extension://')",
        @"  browser.test.assertTrue(sender.origin.startsWith('webkit-extension://'), 'sender.origin should start with webkit-extension://')",

        @"  browser.test.assertEq(sender.tab, undefined, 'sender.tab should be undefined')",
        @"  browser.test.assertEq(sender.frameId, undefined, 'sender.frameId should be undefined')",

        @"  return Promise.resolve({ content: 'Received' })",
        @"})",

        @"setTimeout(() => browser.runtime.sendMessage('Ready'), 1000)"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsContentScriptManifest resources:@{ @"background.js": backgroundScript, @"content.js": contentScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, SendMessageWithAsyncPromiseReply)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener(async (message, sender) => {",
        @"  browser.test.assertEq(message, 'Ready')",

        @"  const tabs = await browser.tabs.query({ active: true, currentWindow: true })",
        @"  const tabId = tabs[0].id",

        @"  const response = await browser.test.assertSafeResolve(() => browser.tabs.sendMessage(tabId, { content: 'Hello' }))",
        @"  browser.test.assertEq(response?.content, 'Received', 'Should get the response from the content script')",

        @"  browser.test.notifyPass()",
        @"})"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener((message, sender) => {",
        @"  browser.test.assertEq(message.content, 'Hello', 'Should receive the correct message content')",

        @"  browser.test.assertEq(typeof sender, 'object', 'sender should be an object')",

        @"  browser.test.assertEq(typeof sender.url, 'string', 'sender.url should be a string')",
        @"  browser.test.assertEq(typeof sender.origin, 'string', 'sender.origin should be a string')",

        @"  browser.test.assertTrue(sender.url.startsWith('webkit-extension://'), 'sender.url should start with webkit-extension://')",
        @"  browser.test.assertTrue(sender.origin.startsWith('webkit-extension://'), 'sender.origin should start with webkit-extension://')",

        @"  browser.test.assertEq(sender.tab, undefined, 'sender.tab should be undefined')",
        @"  browser.test.assertEq(sender.frameId, undefined, 'sender.frameId should be undefined')",

        @"  return new Promise((resolve) => {",
        @"    setTimeout(() => resolve({ content: 'Received' }), 1000)",
        @"  })",
        @"})",

        @"setTimeout(() => browser.runtime.sendMessage('Ready'), 1000)"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsContentScriptManifest resources:@{ @"background.js": backgroundScript, @"content.js": contentScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, SendMessageWithoutReply)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener(async (message, sender) => {",
        @"  const tabs = await browser.tabs.query({ active: true, currentWindow: true })",
        @"  const tabId = tabs[0].id",

        @"  const response = await browser.tabs.sendMessage(tabId, { content: 'Hello' })",
        @"  browser.test.assertEq(response, undefined, 'Should resolve with undefined as there was no reply')",

        @"  browser.test.notifyPass()",
        @"})"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener((message, sender) => {",
        @"  browser.test.assertEq(message.content, 'Hello', 'Should receive the correct message content')",

        @"  browser.test.assertEq(typeof sender, 'object', 'sender should be an object')",

        @"  browser.test.assertEq(typeof sender.url, 'string', 'sender.url should be a string')",
        @"  browser.test.assertEq(typeof sender.origin, 'string', 'sender.origin should be a string')",

        @"  browser.test.assertTrue(sender.url.startsWith('webkit-extension://'), 'sender.url should start with webkit-extension://')",
        @"  browser.test.assertTrue(sender.origin.startsWith('webkit-extension://'), 'sender.origin should start with webkit-extension://')",

        @"  browser.test.assertEq(sender.tab, undefined, 'sender.tab should be undefined')",
        @"  browser.test.assertEq(sender.frameId, undefined, 'sender.frameId should be undefined')",

        @"  return false",
        @"})",

        @"setTimeout(() => browser.runtime.sendMessage('Ready'), 1000)"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsContentScriptManifest resources:@{ @"background.js": backgroundScript, @"content.js": contentScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, SendMessageFromBackgroundPageToFullPageExtensionContent)
{
    auto *optionsScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener((message, sender, sendResponse) => {",
        @"  browser.test.assertEq(message?.content, 'Hello', 'Should receive the correct message from the background page')",

        @"  browser.test.assertEq(typeof sender, 'object', 'sender should be an object')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.runtime.sendMessage({ content: 'Ready' })",
    ]);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener(async (message, sender, sendResponse) => {",
        @"  browser.test.assertEq(message?.content, 'Ready', 'Should receive the correct message from the options page')",

        @"  browser.test.assertEq(typeof sender, 'object', 'sender should be an object')",

        @"  const tabs = await browser.tabs.query({ active: true, currentWindow: true })",
        @"  const tabId = tabs[0].id",

        @"  browser.tabs.sendMessage(tabId, { content: 'Hello' })",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"options.html": @"<script type='module' src='options.js'></script>",
        @"options.js": optionsScript
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultWindow openNewTab];

    EXPECT_EQ(manager.get().defaultWindow.tabs.count, 2lu);

    auto *optionsPageURL = manager.get().context.optionsPageURL;
    EXPECT_NOT_NULL(optionsPageURL);

    auto *defaultTab = manager.get().defaultTab;
    EXPECT_NOT_NULL(defaultTab);

    [defaultTab changeWebViewIfNeededForURL:optionsPageURL forExtensionContext:manager.get().context];
    [defaultTab.webView loadRequest:[NSURLRequest requestWithURL:optionsPageURL]];

    [manager run];
}

TEST(WKWebExtensionAPITabs, SendMessageFromBackgroundToSubframe)
{
    TestWebKitAPI::HTTPServer server({
        { "/main"_s, { { { "Content-Type"_s, "text/html"_s } },
            "<body><script>"
            "  document.write('<iframe src=\"http://127.0.0.1:' + location.port + '/subframe\"></iframe>')"
            "</script></body>"_s
        } },
        { "/subframe"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequestMain = server.requestWithLocalhost("/main"_s);
    auto *urlRequestSubframe = server.request("/subframe"_s);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.yield('Load Tab')",

        @"await new Promise(resolve => setTimeout(resolve, 1500))",

        @"const tabs = await browser.tabs.query({ })",
        @"browser.test.assertEq(tabs.length, 1)",

        @"const response = await browser.tabs.sendMessage(tabs[0].id, { content: 'Hello Subframe' })",
        @"browser.test.assertEq(response?.content, 'Received from subframe', 'Response should be received from subframe')",

        @"browser.test.notifyPass()",
    ]);

    auto *contentScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener((message, sender) => {",
        @"  browser.test.assertEq(message?.content, 'Hello Subframe', 'Should receive the correct message from the background page')",
        @"  return Promise.resolve({ content: 'Received from subframe' })",
        @"})"
    ]);

    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1",

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"type": @"module",
            @"persistent": @NO,
        },

        @"content_scripts": @[@{
            @"matches": @[ @"*://127.0.0.1/*" ],
            @"js": @[ @"content.js" ],
            @"all_frames": @YES
        }]
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"content.js": contentScript
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequestSubframe.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequestMain];

    [manager run];
}

TEST(WKWebExtensionAPITabs, SendMessageFromBackgroundToSpecificFrame)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, " "_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener(async (message, sender) => {",
        @"  browser.test.assertEq(message, 'Begin Test', 'Message content should match')",

        @"  const frameId = sender?.frameId",
        @"  browser.test.assertTrue(frameId !== 0, 'frameId should not be 0 for an iframe')",

        @"  const response = await browser.tabs.sendMessage(sender.tab.id, 'Hello, iframe!', { frameId })",
        @"  browser.test.assertEq(response, 'Message received', 'Should receive response from content script')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Load Tab')"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"if (window.top === window) {",
        @"  browser.runtime.onMessage.addListener(() => {",
        @"    browser.test.notifyFail('Main frame should not receive message intended for iframe')",
        @"  })",
        @"} else {",
        @"  browser.runtime.onMessage.addListener((message, sender, sendResponse) => {",
        @"    browser.test.assertEq(message, 'Hello, iframe!', 'Message should be received in the iframe')",
        @"    sendResponse('Message received')",
        @"  })",

        @"  browser.runtime.sendMessage('Begin Test')",
        @"}"
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"content.js": contentScript,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsContentScriptManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *matchPattern = [WKWebExtensionMatchPattern matchPatternWithScheme:@"*" host:@"localhost" path:@"/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:server.requestWithLocalhost()];

    [manager run];
}

TEST(WKWebExtensionAPITabs, SendMessageFromBackgroundToSpecificDocument)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, " "_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener(async (message, sender) => {",
        @"  browser.test.assertEq(message, 'Begin Test', 'Message content should match')",

        @"  const documentId = sender?.documentId",
        @"  browser.test.assertEq(typeof documentId, 'string', 'sender.documentId should be a string')",
        @"  browser.test.assertEq(documentId.length, 36, 'sender.documentId.length should be 36')",

        @"  const response = await browser.tabs.sendMessage(sender.tab.id, 'Hello, iframe!', { documentId })",
        @"  browser.test.assertEq(response, 'Message received', 'Should receive response from content script')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Load Tab')"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"if (window.top === window) {",
        @"  browser.runtime.onMessage.addListener(() => {",
        @"    browser.test.notifyFail('Main frame should not receive message intended for iframe')",
        @"  })",
        @"} else {",
        @"  browser.runtime.onMessage.addListener((message, sender, sendResponse) => {",
        @"    browser.test.assertEq(message, 'Hello, iframe!', 'Background script message should be received')",
        @"    sendResponse('Message received')",
        @"  })",

        @"  browser.runtime.sendMessage('Begin Test')",
        @"}"
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"content.js": contentScript,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsContentScriptManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *matchPattern = [WKWebExtensionMatchPattern matchPatternWithScheme:@"*" host:@"localhost" path:@"/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:server.requestWithLocalhost()];

    [manager run];
}

TEST(WKWebExtensionAPITabs, Connect)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener(async (message, sender) => {",
        @"  browser.test.assertEq(message?.readyToConnect, true, 'Should be ready to connect')",

        @"  const tabs = await browser.tabs.query({ active: true, currentWindow: true })",
        @"  const tabId = tabs[0].id",

        @"  const port = browser.tabs.connect(tabId, { name: 'testPort' })",
        @"  browser.test.assertEq(typeof port, 'object', 'Port should be an object')",
        @"  browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')",
        @"  browser.test.assertEq(port?.sender, null, 'Port sender should be null')",

        @"  port.postMessage('Hello')",

        @"  port.onMessage.addListener((response) => {",
        @"    browser.test.assertEq(response, 'Received', 'Should get the response from the content script')",

        @"    browser.test.notifyPass()",
        @"  })",
        @"})"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"browser.runtime.onConnect.addListener((port) => {",
        @"  browser.test.assertEq(port.name, 'testPort', 'Port name should be testPort')",
        @"  browser.test.assertEq(port.error, null, 'Port error should be null')",

        @"  browser.test.assertEq(typeof port.sender, 'object', 'sender should be an object')",
        @"  browser.test.assertEq(typeof port.sender.url, 'string', 'sender.url should be a string')",
        @"  browser.test.assertEq(typeof port.sender.origin, 'string', 'sender.origin should be a string')",
        @"  browser.test.assertTrue(port.sender.url.startsWith('webkit-extension://'), 'sender.url should start with webkit-extension://')",
        @"  browser.test.assertTrue(port.sender.origin.startsWith('webkit-extension://'), 'sender.origin should start with webkit-extension://')",
        @"  browser.test.assertEq(port.sender.tab, undefined, 'sender.tab should be undefined')",
        @"  browser.test.assertEq(port.sender.frameId, undefined, 'sender.frameId should be undefined')",

        @"  port.onMessage.addListener((message) => {",
        @"    browser.test.assertEq(message, 'Hello', 'Should receive the correct message content')",
        @"    port.postMessage('Received')",
        @"  })",
        @"})",

        @"setTimeout(() => {",
        @"  browser.runtime.sendMessage({ readyToConnect: true })",
        @"}, 1000)"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsContentScriptManifest resources:@{ @"background.js": backgroundScript, @"content.js": contentScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, ConnectToSpecificFrame)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, " "_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener(async (message, sender) => {",
        @"  browser.test.assertEq(message, 'Begin Test', 'Message content should match')",

        @"  const frameId = sender?.frameId",
        @"  browser.test.assertTrue(frameId !== 0, 'frameId should not be 0 for an iframe')",

        @"  const port = browser.tabs.connect(sender.tab.id, { frameId })",
        @"  browser.test.assertEq(typeof port, 'object', 'Port should be an object')",

        @"  port.onMessage.addListener((response) => {",
        @"    browser.test.assertEq(response, 'Message received', 'Should receive response from content script')",
        @"    browser.test.notifyPass()",
        @"  })",

        @"  port.postMessage('Hello, iframe!')",
        @"})",

        @"browser.test.yield('Load Tab')"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"if (window.top === window) {",
        @"  browser.runtime.onConnect.addListener(() => {",
        @"    browser.test.notifyFail('Main frame should not receive connection intended for iframe')",
        @"  })",
        @"} else {",
        @"  browser.runtime.onConnect.addListener((port) => {",
        @"    port.onMessage.addListener((message) => {",
        @"      browser.test.assertEq(message, 'Hello, iframe!', 'Background script message should be received')",
        @"      port.postMessage('Message received')",
        @"    })",
        @"  })",

        @"  browser.runtime.sendMessage('Begin Test')",
        @"}"
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"content.js": contentScript,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsContentScriptManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *matchPattern = [WKWebExtensionMatchPattern matchPatternWithScheme:@"*" host:@"localhost" path:@"/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:server.requestWithLocalhost()];

    [manager run];
}

TEST(WKWebExtensionAPITabs, ConnectToSpecificDocument)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, " "_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener(async (message, sender) => {",
        @"  browser.test.assertEq(message, 'Begin Test', 'Message content should match')",

        @"  const documentId = sender?.documentId",
        @"  browser.test.assertEq(typeof documentId, 'string', 'sender.documentId should be a string')",
        @"  browser.test.assertEq(documentId.length, 36, 'sender.documentId.length should be 36')",

        @"  const port = browser.tabs.connect(sender.tab.id, { documentId })",
        @"  browser.test.assertEq(typeof port, 'object', 'Port should be an object')",

        @"  port.onMessage.addListener((response) => {",
        @"    browser.test.assertEq(response, 'Message received', 'Should receive response from content script')",
        @"    browser.test.notifyPass()",
        @"  })",

        @"  port.postMessage('Hello, iframe!')",
        @"})",

        @"browser.test.yield('Load Tab')"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"if (window.top === window) {",
        @"  browser.runtime.onConnect.addListener(() => {",
        @"    browser.test.notifyFail('Main frame should not receive connection intended for iframe')",
        @"  })",
        @"} else {",
        @"  browser.runtime.onConnect.addListener((port) => {",
        @"    port.onMessage.addListener((message) => {",
        @"      browser.test.assertEq(message, 'Hello, iframe!', 'Background script message should be received')",
        @"      port.postMessage('Message received')",
        @"    })",
        @"  })",

        @"  browser.runtime.sendMessage('Begin Test')",
        @"}"
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"content.js": contentScript,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsContentScriptManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *matchPattern = [WKWebExtensionMatchPattern matchPatternWithScheme:@"*" host:@"localhost" path:@"/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:server.requestWithLocalhost()];

    [manager run];
}

TEST(WKWebExtensionAPITabs, ConnectToSubframe)
{
    TestWebKitAPI::HTTPServer server({
        { "/main"_s, { { { "Content-Type"_s, "text/html"_s } },
            "<body><script>"
            "  document.write('<iframe src=\"http://127.0.0.1:' + location.port + '/subframe\"></iframe>')"
            "</script></body>"_s
        } },
        { "/subframe"_s, { { { "Content-Type"_s, "text/html"_s } }, "<p>Subframe content</p>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequestMain = server.requestWithLocalhost("/main"_s);
    auto *urlRequestSubframe = server.request("/subframe"_s);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.yield('Load Tab')",

        @"await new Promise(resolve => setTimeout(resolve, 1500))",

        @"const tabs = await browser.tabs.query({ })",
        @"browser.test.assertEq(tabs.length, 1)",

        @"const port = browser.tabs.connect(tabs[0].id, { name: 'testPort' })",
        @"browser.test.assertEq(typeof port, 'object', 'Port should be an object')",
        @"browser.test.assertEq(port.name, 'testPort', 'Port name should be testPort')",

        @"port.onMessage.addListener((response) => {",
        @"  browser.test.assertEq(response, 'Received from subframe', 'Should get the response from the subframe')",
        @"  browser.test.notifyPass()",
        @"})",

        @"port.postMessage('Hello Subframe')"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"browser.runtime.onConnect.addListener((port) => {",
        @"  browser.test.assertEq(port.name, 'testPort', 'Port name should be testPort')",

        @"  port.onMessage.addListener((message) => {",
        @"    browser.test.assertEq(message, 'Hello Subframe', 'Should receive the correct message content')",
        @"    port.postMessage('Received from subframe')",
        @"  })",
        @"})"
    ]);

    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1",

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"type": @"module",
            @"persistent": @NO,
        },

        @"content_scripts": @[@{
            @"matches": @[ @"*://127.0.0.1/*" ],
            @"js": @[ @"content.js" ],
            @"all_frames": @YES
        }]
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"content.js": contentScript
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequestSubframe.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequestMain];

    [manager run];
}

TEST(WKWebExtensionAPITabs, PortDisconnect)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener(async (message, sender) => {",
        @"  browser.test.assertEq(message?.readyToConnect, true, 'Should be ready to connect')",

        @"  const tabs = await browser.tabs.query({ active: true, currentWindow: true })",
        @"  const tabId = tabs[0].id",

        @"  const port = browser.tabs.connect(tabId)",
        @"  browser.test.assertEq(typeof port, 'object', 'Port should be an object')",
        @"  browser.test.assertEq(port?.name, '', 'Port name should be empty')",
        @"  browser.test.assertEq(port?.sender, null, 'Port sender should be null')",

        @"  port.postMessage('Hello')",

        @"  port.onDisconnect.addListener(() => {",
        @"    browser.test.assertTrue(true, 'Should trigger the onDisconnect event in the background script')",
        @"  })",

        @"  port.disconnect()",
        @"})"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"browser.runtime.onConnect.addListener((port) => {",
        @"  port.onMessage.addListener((message) => {",
        @"    browser.test.assertEq(message, 'Hello', 'Should receive the correct message content')",
        @"  })",

        @"  port.onDisconnect.addListener(() => {",
        @"    browser.test.assertTrue(true, 'Should trigger the onDisconnect event in the content script')",
        @"    browser.test.notifyPass()",
        @"  })",
        @"})",

        @"setTimeout(() => {",
        @"  browser.runtime.sendMessage({ readyToConnect: true })",
        @"}, 1000)"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsContentScriptManifest resources:@{ @"background.js": backgroundScript, @"content.js": contentScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, ConnectWithMultipleListeners)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener(async (message, sender) => {",
        @"  browser.test.assertEq(message?.readyToConnect, true, 'Should be ready to connect')",

        @"  const tabs = await browser.tabs.query({ active: true, currentWindow: true })",
        @"  const tabId = tabs[0].id",

        @"  const port = browser.tabs.connect(tabId)",
        @"  browser.test.assertEq(typeof port, 'object', 'Port should be an object')",
        @"  browser.test.assertEq(port?.name, '', 'Port name should be empty')",
        @"  browser.test.assertEq(port?.sender, null, 'Port sender should be null')",

        @"  port.postMessage('Hello')",

        @"  let receivedMessages = 0",
        @"  port.onMessage.addListener((response) => {",
        @"    browser.test.assertEq(response, 'Received', 'Should get the response from the content script')",

        @"    if (++receivedMessages === 2)",
        @"      browser.test.notifyPass()",
        @"  })",
        @"})"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"let firstListenerHandled = false",
        @"let secondListenerHandled = false",

        @"browser.runtime.onConnect.addListener((port) => {",
        @"  port.onMessage.addListener((message) => {",
        @"    if (!firstListenerHandled) {",
        @"      browser.test.assertEq(message, 'Hello', 'Should receive the correct message content')",
        @"      port.postMessage('Received')",
        @"      firstListenerHandled = true",
        @"    }",
        @"  })",
        @"})",

        @"browser.runtime.onConnect.addListener((port) => {",
        @"  port.onMessage.addListener((message) => {",
        @"    if (!secondListenerHandled) {",
        @"      browser.test.assertEq(message, 'Hello', 'Should receive the correct message content')",
        @"      port.postMessage('Received')",
        @"      secondListenerHandled = true",
        @"    }",
        @"  })",
        @"})",

        @"setTimeout(() => {",
        @"  browser.runtime.sendMessage({ readyToConnect: true })",
        @"}, 1000)"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsContentScriptManifest resources:@{ @"background.js": backgroundScript, @"content.js": contentScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, PortDisconnectWithMultipleListeners)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener(async (message, sender) => {",
        @"  browser.test.assertEq(message?.readyToConnect, true, 'Should be ready to connect')",

        @"  const tabs = await browser.tabs.query({ active: true, currentWindow: true })",
        @"  const tabId = tabs[0].id",

        @"  const port = browser.tabs.connect(tabId)",
        @"  browser.test.assertEq(typeof port, 'object', 'Port should be an object')",
        @"  browser.test.assertEq(port?.name, '', 'Port name should be empty')",
        @"  browser.test.assertEq(port?.sender, null, 'Port sender should be null')",

        @"  port.postMessage('Hello')",

        @"  let receivedMessages = 0",
        @"  port.onMessage.addListener((response) => {",
        @"    browser.test.assertEq(response, 'Received', 'Should get the response from the content script')",

        @"    if (++receivedMessages === 2)",
        @"      browser.test.notifyPass()",
        @"  })",
        @"})"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"browser.runtime.onConnect.addListener((port) => {",
        @"  port.onMessage.addListener((message) => {",
        @"    browser.test.assertEq(message, 'Hello', 'Should receive the correct message content')",
        @"    port.postMessage('Received')",
        @"    port.disconnect()",
        @"  })",
        @"})",

        @"browser.runtime.onConnect.addListener((port) => {",
        @"  port.onMessage.addListener((message) => {",
        @"    browser.test.assertEq(message, 'Hello', 'Should receive the correct message content')",
        @"    port.postMessage('Received')",
        @"  })",
        @"})",

        @"setTimeout(() => {",
        @"  browser.runtime.sendMessage({ readyToConnect: true })",
        @"}, 1000)"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsContentScriptManifest resources:@{ @"background.js": backgroundScript, @"content.js": contentScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPITabs, ExecuteScript)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body style='background-color: blue'></body>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    static auto *javaScript = @"document.body.style.background = 'pink'";

    auto *backgroundScript = Util::constructScript(@[
        @"const tabs = await browser.tabs.query({ active: true, currentWindow: true })",
        @"const tabId = tabs[0].id",

        @"browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {",
        @"  if (tab.status === 'complete') {",
        @"    let results = await browser.tabs.executeScript(tabId, { allFrames: false, file: 'executeScript.js' })",
        @"    browser.test.assertEq(results[0], 'pink')",

        @"    results = await browser.tabs.executeScript(tabId, { frameId: 0, file: 'executeScript.js' })",
        @"    browser.test.assertEq(results[0], 'pink')",

        @"    results = await browser.tabs.executeScript(tabId, { allFrames: true, file: 'executeScript.js' })",
        @"    browser.test.assertEq(results[0], 'pink')",
        @"    browser.test.assertEq(results[1], 'pink')",

        @"    results = await browser.tabs.executeScript(tabId, { allFrames: false, code: \"document.body.style.background = 'pink'\" })",
        @"    browser.test.assertEq(results[0], 'pink')",

        @"    browser.test.assertSafeResolve(() => browser.tabs.executeScript(tabId, { allFrames: false, frameId: 0, file: 'executeScript.js' }))",

        @"    browser.test.notifyPass()",
        @"  }",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifestV2 resources:@{ @"background.js": backgroundScript, @"executeScript.js": javaScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    auto *url = urlRequest.URL;
    auto *matchPattern = [WKWebExtensionMatchPattern matchPatternWithScheme:url.scheme host:url.host path:@"/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPITabs, ExecuteScriptWithFrameId)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, " "_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener(async (message, sender) => {",
        @"  browser.test.assertEq(message, 'Hello from frame')",

        @"  const frameId = sender.frameId",
        @"  browser.test.assertTrue(frameId !== 0, 'frameId should not be 0 for an iframe')",

        @"  const results = await browser.tabs.executeScript(sender.tab.id, { frameId, code: 'location.pathname' })",
        @"  browser.test.assertEq(results[0], '/frame.html', 'Should execute in the iframe and return the correct path')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Load Tab')"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"browser.runtime.sendMessage('Hello from frame')",
    ]);

    static auto *manifest = @{
        @"manifest_version": @2,

        @"name": @"Tabs Test",
        @"description": @"Tabs Test",
        @"version": @"1",

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"type": @"module",
            @"persistent": @NO,
        },

        @"content_scripts": @[ @{
            @"js": @[ @"content.js" ],
            @"matches": @[ @"*://localhost/frame.html" ],
            @"all_frames": @YES
        } ],
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"content.js": contentScript,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:server.requestWithLocalhost("/frame.html"_s).URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:server.requestWithLocalhost()];

    [manager run];
}

TEST(WKWebExtensionAPITabs, ExecuteScriptWithDocumentId)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<span>Main Document</span><iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<span>Frame Document</span>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener(async (message, sender) => {",
        @"  browser.test.assertEq(message, 'Hello from frame')",

        @"  const documentId = sender?.documentId",
        @"  browser.test.assertEq(typeof documentId, 'string', 'sender.documentId should be')",

        @"  const result = await browser.tabs.executeScript(sender?.tab?.id, { documentId, code: 'document.body.innerText.trim()' })",
        @"  browser.test.assertEq(result?.[0], 'Frame Document', 'Result should be')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Load Tab')"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"browser.runtime.sendMessage('Hello from frame')",
    ]);

    static auto *manifest = @{
        @"manifest_version": @2,

        @"name": @"Tabs Test",
        @"description": @"Tabs Test",
        @"version": @"1",

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"persistent": @NO
        },

        @"content_scripts": @[ @{
            @"js": @[ @"content.js" ],
            @"matches": @[ @"*://localhost/frame.html" ],
            @"all_frames": @YES
        } ],
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"content.js": contentScript,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:server.requestWithLocalhost("/frame.html"_s).URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:server.requestWithLocalhost()];

    [manager run];
}

TEST(WKWebExtensionAPITabs, ExecuteScriptJSONTypes)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {",
        @"  if (tab.status === 'complete') {",
        @"    const expectedResult = { 'boolean': true, 'number': 42, 'string': 'Test String', 'object': { 'key': 'value' }, 'array': [1, 2, 3] };",

        @"    function returnJSONTypes() {",
        @"      return { 'boolean': true, 'number': 42, 'string': 'Test String', 'object': { 'key': 'value' }, 'array': [1, 2, 3] };",
        @"    }",

        @"    const results = await browser.tabs.executeScript(tabId, { code: '(' + returnJSONTypes + ')()' });",
        @"    browser.test.assertDeepEq(results?.[0], expectedResult);",

        @"    browser.test.notifyPass()",
        @"  }",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifestV2 resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPITabs, InsertAndRemoveCSSInMainFrame)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    static auto *css = @"body { background-color: pink !important }";

    auto *backgroundScript = Util::constructScript(@[
        @"const pinkValue = 'rgb(255, 192, 203)'",
        @"const blueValue = 'rgb(0, 0, 255)'",
        @"const transparentValue = 'rgba(0, 0, 0, 0)'",

        @"browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {",
        @"  if (tab.status !== 'complete')",
        @"    return",

        @"  await browser.tabs.insertCSS(tabId, { allFrames: false, file: 'styles.css' })",
        @"  let result = await browser.tabs.executeScript(tabId, { code: \"window.getComputedStyle(document.body).getPropertyValue('background-color')\", allFrames: false })",
        @"  browser.test.assertEq(result[0], pinkValue)",

        @"  await browser.tabs.removeCSS(tabId, { allFrames: false, file: 'styles.css' })",
        @"  result = await browser.tabs.executeScript(tabId, { code: \"window.getComputedStyle(document.body).getPropertyValue('background-color')\", allFrames: false })",
        @"  browser.test.assertEq(result[0], transparentValue)",

        @"  await browser.tabs.insertCSS(tabId, { frameId: 0, file: 'styles.css' })",
        @"  result = await browser.tabs.executeScript(tabId, { code: \"window.getComputedStyle(document.body).getPropertyValue('background-color')\", allFrames: false })",
        @"  browser.test.assertEq(result[0], pinkValue)",

        @"  await browser.tabs.removeCSS(tabId, { frameId: 0, file: 'styles.css' })",
        @"  result = await browser.tabs.executeScript(tabId, { code: \"window.getComputedStyle(document.body).getPropertyValue('background-color')\", allFrames: false })",
        @"  browser.test.assertEq(result[0], transparentValue)",

        @"  await browser.tabs.insertCSS(tabId, { frameId: 0, code: 'body { background-color: blue !important }' })",
        @"  result = await browser.tabs.executeScript(tabId, { code: \"window.getComputedStyle(document.body).getPropertyValue('background-color')\", allFrames: false })",
        @"  browser.test.assertEq(result[0], blueValue)",

        @"  await browser.tabs.removeCSS(tabId, { frameId: 0, code: 'body { background-color: blue !important }' })",
        @"  result = await browser.tabs.executeScript(tabId, { code: \"window.getComputedStyle(document.body).getPropertyValue('background-color')\", allFrames: false })",
        @"  browser.test.assertEq(result[0], transparentValue)",

        // Stylesheet being removed should match the one inserted.
        @"  await browser.tabs.insertCSS(tabId, { frameId: 0, code: 'body { background-color: blue !important }' })",
        @"  await browser.tabs.removeCSS(tabId, { allFrames: true, code: 'body { background-color: blue !important }' })",
        @"  result = await browser.tabs.executeScript(tabId, { code: \"window.getComputedStyle(document.body).getPropertyValue('background-color')\", allFrames: false })",
        @"  browser.test.assertEq(result[0], blueValue)",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifestV2 resources:@{ @"background.js": backgroundScript, @"styles.css": css }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    auto *url = urlRequest.URL;
    auto *matchPattern = [WKWebExtensionMatchPattern matchPatternWithScheme:url.scheme host:url.host path:@"/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPITabs, InsertAndRemoveCSSInAllFrames)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body><iframe src='/frame.html'/></body>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body style='background-color: blue'></body>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    static auto *css = @"body { background-color: pink !important }";

    auto *backgroundScript = Util::constructScript(@[
        @"const pinkValue = 'rgb(255, 192, 203)'",
        @"const blueValue = 'rgb(0, 0, 255)'",
        @"const transparentValue = 'rgba(0, 0, 0, 0)'",

        @"browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {",
        @"  if (tab.status !== 'complete')",
        @"    return",

        @"  await browser.tabs.insertCSS(tabId, { allFrames: true, file: 'styles.css' })",

        @"  let result = await browser.tabs.executeScript(tabId, { code: \"window.getComputedStyle(document.body).getPropertyValue('background-color')\", allFrames: true })",

        @"  browser.test.assertEq(result[0], pinkValue)",
        @"  browser.test.assertEq(result[1], pinkValue)",

        @"  await browser.tabs.removeCSS(tabId, { allFrames: true, file: 'styles.css' })",
        @"  result = await browser.tabs.executeScript(tabId, { code: \"window.getComputedStyle(document.body).getPropertyValue('background-color')\", allFrames: true })",

        @"  browser.test.assertEq(result[0], transparentValue)",
        @"  browser.test.assertEq(result[1], blueValue)",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifestV2 resources:@{ @"background.js": backgroundScript, @"styles.css": css }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    auto *url = urlRequest.URL;
    auto *matchPattern = [WKWebExtensionMatchPattern matchPatternWithScheme:url.scheme host:url.host path:@"/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPITabs, CSSUserOrigin)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body style='color: red'></body>"_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {",
        @"  if (changeInfo.status === 'complete') {",
        @"    await browser.test.assertSafeResolve(() => browser.tabs.insertCSS(tabId, { code: 'body { color: green !important }', cssOrigin: 'user' }))",

        @"    let result = await browser.test.assertSafeResolve(() => browser.tabs.executeScript(tabId, { code: 'getComputedStyle(document.body).color' }))",
        @"    browser.test.assertEq(result[0], 'rgb(0, 128, 0)', 'Color should be green')",

        @"    browser.test.notifyPass()",
        @"  }",
        @"})",

        @"browser.test.yield('Load Tab')"
    ]);

    auto *urlRequest = server.requestWithLocalhost();

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifestV2 resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPITabs, CSSAuthorOrigin)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<style> body { color: red; } </style>"_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {",
        @"  if (changeInfo.status === 'complete') {",
        @"    await browser.test.assertSafeResolve(() => browser.tabs.insertCSS(tabId, { code: 'body { color: green !important; }', cssOrigin: 'author' }))",

        @"    let result = await browser.test.assertSafeResolve(() => browser.tabs.executeScript(tabId, { code: 'getComputedStyle(document.body).color' }))",
        @"    browser.test.assertEq(result[0], 'rgb(0, 128, 0)', 'Color should be green')",

        @"    browser.test.notifyPass()",
        @"  }",
        @"})",

        @"browser.test.yield('Load Tab')"
    ]);

    auto *urlRequest = server.requestWithLocalhost();

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:tabsManifestV2 resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPITabs, UnsupportedMV3APIs)
{
    // Manifest v3 deprecates these APIs, so they should be an undefined property.

    static auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(browser.tabs.insertCSS, undefined)",
        @"browser.test.assertEq(browser.tabs.removeCSS, undefined)",
        @"browser.test.assertEq(browser.tabs.executeScript, undefined)",

        @"browser.test.assertEq(browser.tabs.getSelected, undefined)",

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(tabsManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPITabs, ActiveTab)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<head><title>Test Title</title></head>"_s } },
        { "/next.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<head><title>Next Title</title></head>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"const [currentTab] = await browser.tabs.query({ active: true, currentWindow: true })",
        @"let updateCount = 0",

        @"browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {",
        @"  browser.test.assertEq(tabId, currentTab.id, 'Only the tab we expect should be changing')",

        @"  if ('url' in changeInfo && 'title' in changeInfo) {",
        @"    ++updateCount",

        @"    if (updateCount === 1) {",
        @"      browser.test.assertEq(changeInfo.url, '', 'URL should be empty before user gesture')",
        @"      browser.test.assertEq(changeInfo.title, '', 'Title should be empty before user gesture')",

        @"      browser.test.yield('Perform User Gesture')",
        @"    } else if (updateCount === 2) {",
        @"      browser.test.assertTrue(changeInfo.url.includes('localhost'), 'URL should be localhost after user gesture')",
        @"      browser.test.assertEq(changeInfo.title, 'Test Title', 'Title should be provided after user gesture')",

        @"      browser.test.yield('Load Next Page')",
        @"    } else if (updateCount === 3) {",
        @"      browser.test.assertTrue(changeInfo.url.includes('localhost'), 'URL should be localhost after same site navigation')",
        @"      browser.test.assertEq(changeInfo.title, 'Next Title', 'Title should be provided after same site navigation')",

        @"      browser.test.yield('Load IP Address')",
        @"    } else {",
        @"      browser.test.assertEq(updateCount, 4, 'Update count should be')",
        @"      browser.test.assertEq(changeInfo.url, '', 'URL should be empty after navigation without permission')",
        @"      browser.test.assertEq(changeInfo.title, '', 'Title should be empty after navigation without permission')",

        @"      browser.test.notifyPass()",
        @"    }",
        @"  }",
        @"})",

        @"browser.test.yield('Load Localhost')",
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:activeTabManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *localhostRequest = server.requestWithLocalhost();
    auto *addressRequest = server.request();

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Localhost");

    [manager.get().defaultTab.webView loadRequest:localhostRequest];

    [manager run];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Perform User Gesture");

    EXPECT_FALSE([manager.get().context hasPermission:WKWebExtensionPermissionTabs inTab:manager.get().defaultTab]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:localhostRequest.URL inTab:manager.get().defaultTab]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:addressRequest.URL inTab:manager.get().defaultTab]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:localhostRequest.URL]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:addressRequest.URL]);

    [manager.get().context userGesturePerformedInTab:manager.get().defaultTab];

    EXPECT_TRUE([manager.get().context hasPermission:WKWebExtensionPermissionTabs inTab:manager.get().defaultTab]);
    EXPECT_TRUE([manager.get().context hasAccessToURL:localhostRequest.URL inTab:manager.get().defaultTab]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:addressRequest.URL inTab:manager.get().defaultTab]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:localhostRequest.URL]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:addressRequest.URL]);

    [manager run];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Next Page");

    EXPECT_TRUE([manager.get().context hasPermission:WKWebExtensionPermissionTabs inTab:manager.get().defaultTab]);
    EXPECT_TRUE([manager.get().context hasAccessToURL:localhostRequest.URL inTab:manager.get().defaultTab]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:addressRequest.URL inTab:manager.get().defaultTab]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:localhostRequest.URL]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:addressRequest.URL]);

    [manager.get().defaultTab.webView loadRequest:server.requestWithLocalhost("/next.html"_s)];

    [manager run];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load IP Address");

    [manager.get().defaultTab.webView loadRequest:addressRequest];

    [manager run];

    EXPECT_FALSE([manager.get().context hasPermission:WKWebExtensionPermissionTabs inTab:manager.get().defaultTab]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:localhostRequest.URL]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:addressRequest.URL]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:localhostRequest.URL inTab:manager.get().defaultTab]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:addressRequest.URL inTab:manager.get().defaultTab]);
}

TEST(WKWebExtensionAPITabs, UserGestureWithoutActiveTab)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<head><title>Test Title</title></head>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"const [currentTab] = await browser.tabs.query({ active: true, currentWindow: true })",

        @"browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {",
        @"  browser.test.assertEq(tabId, currentTab.id, 'Only the tab we expect should be changing')",

        @"  if ('url' in changeInfo && 'title' in changeInfo) {",
        @"    browser.test.assertEq(changeInfo.url, '', 'URL should be empty before user gesture')",
        @"    browser.test.assertEq(changeInfo.title, '', 'Title should be empty before user gesture')",
        @"    browser.test.yield('Perform User Gesture')",
        @"  }",
        @"})",

        @"browser.test.yield('Load Localhost')"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:activeTabManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Reset activeTab, WKWebExtensionAPITabs.ActiveTab tests that.
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusUnknown forPermission:WKWebExtensionPermissionActiveTab];

    EXPECT_FALSE([manager.get().context hasPermission:WKWebExtensionPermissionActiveTab]);
    EXPECT_FALSE([manager.get().context hasPermission:WKWebExtensionPermissionTabs]);

    auto *localhostRequest = server.requestWithLocalhost();
    auto *addressRequest = server.request();

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Localhost");

    [manager.get().defaultTab.webView loadRequest:localhostRequest];

    [manager run];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Perform User Gesture");

    [manager.get().context userGesturePerformedInTab:manager.get().defaultTab];

    EXPECT_FALSE([manager.get().context hasPermission:WKWebExtensionPermissionActiveTab]);
    EXPECT_FALSE([manager.get().context hasPermission:WKWebExtensionPermissionTabs]);
    EXPECT_FALSE([manager.get().context hasPermission:WKWebExtensionPermissionTabs inTab:manager.get().defaultTab]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:localhostRequest.URL]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:addressRequest.URL]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:localhostRequest.URL inTab:manager.get().defaultTab]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:addressRequest.URL inTab:manager.get().defaultTab]);
}

TEST(WKWebExtensionAPITabs, ActiveTabWithDeniedPermissions)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<head><title>Test Title</title></head>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"const [currentTab] = await browser.tabs.query({ active: true, currentWindow: true })",

        @"browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {",
        @"  browser.test.assertEq(tabId, currentTab.id, 'Only the tab we expect should be changing')",

        @"  if ('url' in changeInfo && 'title' in changeInfo) {",
        @"    browser.test.assertEq(changeInfo.url, '', 'URL should be empty before user gesture')",
        @"    browser.test.assertEq(changeInfo.title, '', 'Title should be empty before user gesture')",
        @"    browser.test.yield('Perform User Gesture')",
        @"  }",
        @"})",

        @"browser.test.yield('Load Localhost')"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:activeTabManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *localhostRequest = server.requestWithLocalhost();

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusDeniedExplicitly forURL:localhostRequest.URL];

    EXPECT_TRUE([manager.get().context hasPermission:WKWebExtensionPermissionActiveTab]);
    EXPECT_FALSE([manager.get().context hasPermission:WKWebExtensionPermissionTabs]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:localhostRequest.URL]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:localhostRequest.URL inTab:manager.get().defaultTab]);

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Localhost");

    [manager.get().defaultTab.webView loadRequest:localhostRequest];

    [manager run];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Perform User Gesture");

    [manager.get().context userGesturePerformedInTab:manager.get().defaultTab];

    EXPECT_TRUE([manager.get().context hasPermission:WKWebExtensionPermissionActiveTab]);
    EXPECT_FALSE([manager.get().context hasPermission:WKWebExtensionPermissionTabs]);
    EXPECT_FALSE([manager.get().context hasPermission:WKWebExtensionPermissionTabs inTab:manager.get().defaultTab]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:localhostRequest.URL]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:localhostRequest.URL inTab:manager.get().defaultTab]);
}

TEST(WKWebExtensionAPITabs, ActiveTabRemovedWithDeniedPermissions)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<head><title>Test Title</title></head>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"const [currentTab] = await browser.tabs.query({ active: true, currentWindow: true })",

        @"browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {",
        @"  browser.test.assertEq(tabId, currentTab.id, 'Only the tab we expect should be changing')",

        @"  if ('url' in changeInfo && 'title' in changeInfo) {",
        @"    browser.test.assertEq(changeInfo.url, '', 'URL should be empty before user gesture')",
        @"    browser.test.assertEq(changeInfo.title, '', 'Title should be empty before user gesture')",
        @"    browser.test.yield('Perform User Gesture')",
        @"  }",
        @"})",

        @"browser.test.yield('Load Localhost')"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:activeTabManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *localhostRequest = server.requestWithLocalhost();

    EXPECT_TRUE([manager.get().context hasPermission:WKWebExtensionPermissionActiveTab]);
    EXPECT_FALSE([manager.get().context hasPermission:WKWebExtensionPermissionTabs]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:localhostRequest.URL]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:localhostRequest.URL inTab:manager.get().defaultTab]);

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Localhost");

    [manager.get().defaultTab.webView loadRequest:localhostRequest];

    [manager run];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Perform User Gesture");

    EXPECT_TRUE([manager.get().context hasPermission:WKWebExtensionPermissionActiveTab]);
    EXPECT_FALSE([manager.get().context hasPermission:WKWebExtensionPermissionTabs inTab:manager.get().defaultTab]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:localhostRequest.URL inTab:manager.get().defaultTab]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:localhostRequest.URL]);

    [manager.get().context userGesturePerformedInTab:manager.get().defaultTab];

    EXPECT_TRUE([manager.get().context hasPermission:WKWebExtensionPermissionActiveTab]);
    EXPECT_TRUE([manager.get().context hasPermission:WKWebExtensionPermissionTabs inTab:manager.get().defaultTab]);
    EXPECT_TRUE([manager.get().context hasAccessToURL:localhostRequest.URL inTab:manager.get().defaultTab]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:localhostRequest.URL]);

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusDeniedExplicitly forURL:localhostRequest.URL];

    EXPECT_TRUE([manager.get().context hasPermission:WKWebExtensionPermissionActiveTab]);
    EXPECT_FALSE([manager.get().context hasPermission:WKWebExtensionPermissionTabs]);
    EXPECT_FALSE([manager.get().context hasPermission:WKWebExtensionPermissionTabs inTab:manager.get().defaultTab]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:localhostRequest.URL]);
    EXPECT_FALSE([manager.get().context hasAccessToURL:localhostRequest.URL inTab:manager.get().defaultTab]);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
