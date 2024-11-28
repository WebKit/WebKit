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
#import "TestWebExtensionsDelegate.h"
#import "WebExtensionUtilities.h"

namespace TestWebKitAPI {

static auto *actionPopupManifest = @{
    @"manifest_version": @3,

    @"name": @"Action Test",
    @"description": @"Action Test",
    @"version": @"1",

    @"permissions": @[ @"webNavigation" ],

    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"type": @"module",
        @"persistent": @NO,
    },

    @"action": @{
        @"default_title": @"Test Action",
        @"default_popup": @"popup.html",
        @"default_icon": @{
            @"16": @"toolbar-16.png",
            @"32": @"toolbar-32.png",
        },
    },
};

TEST(WKWebExtensionAPIAction, Errors)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertThrows(() => browser.action.setTitle({tabId: 'bad', title: 'Test'}), /'tabId' is expected to be a number, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.action.setTitle({windowId: 'bad', title: 'Test'}), /'windowId' is expected to be a number, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.action.setIcon({path: 123}), /'path' is expected to be a string or an object or null, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.action.setIcon({tabId: true, path: 'icon.png'}), /'tabId' is expected to be a number, but a boolean was provided/i)",
        @"browser.test.assertThrows(() => browser.action.setIcon({windowId: true, path: 'icon.png'}), /'windowId' is expected to be a number, but a boolean was provided/i)",
        @"browser.test.assertThrows(() => browser.action.setPopup({tabId: 'bad', popup: 'popup.html'}), /'tabId' is expected to be a number, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.action.setPopup({windowId: 'bad', popup: 'popup.html'}), /'windowId' is expected to be a number, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.action.setBadgeText({tabId: 1.2, text: '1'}), /'tabId' value is invalid, because it is not a tab identifier/i)",
        @"browser.test.assertThrows(() => browser.action.setBadgeText({windowId: -3, text: '2'}), /'windowId' value is invalid, because it is not a window identifier/i)",
        @"browser.test.assertThrows(() => browser.action.enable('bad'), /'tabId' value is invalid, because a number is expected/i)",
        @"browser.test.assertThrows(() => browser.action.disable('bad'), /'tabId' value is invalid, because a number is expected/i)",
        @"browser.test.assertThrows(() => browser.action.isEnabled({tabId: Infinity}), /'tabId' is expected to be a number, but Infinity was provided/i)",
        @"browser.test.assertThrows(() => browser.action.isEnabled({windowId: NaN}), /'windowId' is expected to be a number, but NaN was provided/i)",

        @"browser.test.assertThrows(() => browser.action.setTitle({tabId: 1}), /'details' value is invalid, because it is missing required keys: 'title'/i)",
        @"browser.test.assertThrows(() => browser.action.setTitle({windowId: 1}), /'details' value is invalid, because it is missing required keys: 'title'/i)",
        @"browser.test.assertThrows(() => browser.action.setPopup({tabId: 1}), /'details' value is invalid, because it is missing required keys: 'popup'/i)",
        @"browser.test.assertThrows(() => browser.action.setPopup({windowId: 1}), /'details' value is invalid, because it is missing required keys: 'popup'/i)",
        @"browser.test.assertThrows(() => browser.action.setBadgeText({tabId: 1}), /'details' value is invalid, because it is missing required keys: 'text'/i)",
        @"browser.test.assertThrows(() => browser.action.setBadgeText({windowId: 1}), /'details' value is invalid, because it is missing required keys: 'text'/i)",

        @"browser.test.assertThrows(() => browser.action.setTitle({tabId: 1, windowId: 1, title: null}), /'details' value is invalid, because it cannot specify both 'tabId' and 'windowId'/i)",
        @"browser.test.assertThrows(() => browser.action.setIcon({tabId: 1, windowId: 1, path: null}), /'details' value is invalid, because it cannot specify both 'tabId' and 'windowId'/i)",
        @"browser.test.assertThrows(() => browser.action.setPopup({tabId: 1, windowId: 1, popup: null}), /'details' value is invalid, because it cannot specify both 'tabId' and 'windowId'/i)",
        @"browser.test.assertThrows(() => browser.action.setBadgeText({tabId: 1, windowId: 1, text: null}), /'details' value is invalid, because it cannot specify both 'tabId' and 'windowId'/i)",

        @"await browser.test.assertRejects(browser.action.getTitle({tabId: 9999}), /tab not found/i)",
        @"await browser.test.assertRejects(browser.action.getTitle({windowId: 9999}), /window not found/i)",
        @"await browser.test.assertRejects(browser.action.getPopup({tabId: 9999}), /tab not found/i)",
        @"await browser.test.assertRejects(browser.action.getPopup({windowId: 9999}), /window not found/i)",
        @"await browser.test.assertRejects(browser.action.getBadgeText({tabId: 9999}), /tab not found/i)",
        @"await browser.test.assertRejects(browser.action.getBadgeText({windowId: 9999}), /window not found/i)",
        @"await browser.test.assertRejects(browser.action.isEnabled({tabId: 9999}), /tab not found/i)",
        @"await browser.test.assertRejects(browser.action.isEnabled({windowId: 9999}), /window not found/i)",

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(actionPopupManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIAction, ClickedEvent)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.action.setPopup({ popup: '' })",

        @"browser.action.onClicked.addListener((tab) => {",
        @"  browser.test.assertEq(typeof tab, 'object', 'The tab should be an object')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Test Action')"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Test Action");

    [manager.get().context performActionForTab:manager.get().defaultTab];

    [manager run];
}

TEST(WKWebExtensionAPIAction, PresentPopupForAction)
{
    auto *popupPage = @"<b>Hello World!</b>";

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.yield('Test Popup Action')"
    ]);

    auto *smallToolbarIcon = Util::makePNGData(CGSizeMake(16, 16), @selector(redColor));
    auto *largeToolbarIcon = Util::makePNGData(CGSizeMake(32, 32), @selector(blueColor));

    auto *resources = @{
        @"background.js": backgroundScript,
        @"popup.html": popupPage,
        @"toolbar-16.png": smallToolbarIcon,
        @"toolbar-32.png": largeToolbarIcon,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentPopupForAction = ^(WKWebExtensionAction *action) {
        EXPECT_TRUE(action.presentsPopup);
        EXPECT_TRUE(action.isEnabled);
        EXPECT_NS_EQUAL(action.badgeText, @"");

        EXPECT_NS_EQUAL(action.label, @"Test Action");

        auto *smallIcon = [action iconForSize:CGSizeMake(16, 16)];
        EXPECT_NOT_NULL(smallIcon);
        EXPECT_TRUE(CGSizeEqualToSize(smallIcon.size, CGSizeMake(16, 16)));

        auto *largeIcon = [action iconForSize:CGSizeMake(32, 32)];
        EXPECT_NOT_NULL(largeIcon);
        EXPECT_TRUE(CGSizeEqualToSize(largeIcon.size, CGSizeMake(32, 32)));

#if PLATFORM(IOS_FAMILY)
        EXPECT_NOT_NULL(action.popupViewController);
#endif

#if PLATFORM(MAC)
        EXPECT_NOT_NULL(action.popupPopover);
#endif

        EXPECT_NOT_NULL(action.popupWebView);
        EXPECT_FALSE(action.popupWebView.loading);

        NSURL *webViewURL = action.popupWebView.URL;
        EXPECT_NS_EQUAL(webViewURL.scheme, @"webkit-extension");
        EXPECT_NS_EQUAL(webViewURL.path, @"/popup.html");

        [action closePopup];

        [manager done];
    };

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Test Popup Action");

    [manager.get().context performActionForTab:manager.get().defaultTab];

    [manager run];
}

TEST(WKWebExtensionAPIAction, GetCurrentTabAndWindowFromPopupPage)
{
    auto *popupScript = Util::constructScript(@[
        @"const tab = await browser.tabs.getCurrent()",
        @"browser.test.assertEq(typeof tab, 'object', 'The tab should be')",
        @"browser.test.assertTrue(tab.active, 'The current tab should be active')",

        @"const window = await browser.windows.getCurrent()",
        @"browser.test.assertEq(typeof window, 'object', 'The window should be')",
        @"browser.test.assertTrue(window.focused, 'The current window should be focused')",

        @"browser.test.notifyPass()"
    ]);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.yield('Test Popup Action')"
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"popup.html": @"<script type='module' src='popup.js'></script>",
        @"popup.js": popupScript
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentPopupForAction = ^(WKWebExtensionAction *action) {
        EXPECT_TRUE(action.presentsPopup);
        EXPECT_NOT_NULL(action.popupWebView);
    };

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Test Popup Action");

    [manager.get().context performActionForTab:manager.get().defaultTab];

    [manager run];
}

TEST(WKWebExtensionAPIAction, UpdateTabFromPopupPage)
{
    auto *popupScript = Util::constructScript(@[
        @"const tab = await browser.tabs.getCurrent()",
        @"browser.test.assertEq(typeof tab, 'object', 'The tab should be')",
        @"browser.test.assertTrue(tab.active, 'The current tab should be active')",

        @"browser.test.assertFalse(tab.mutedInfo.muted, 'The tab should not be initially muted')",

        @"const updatedTab = await browser.tabs.update({",
        @"  muted: true,",
        @"})",

        @"browser.test.assertTrue(updatedTab.mutedInfo.muted, 'The tab should be muted after update')",

        @"browser.test.notifyPass()"
    ]);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.yield('Test Popup Action')"
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"popup.html": @"<script type='module' src='popup.js'></script>",
        @"popup.js": popupScript
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentPopupForAction = ^(WKWebExtensionAction *action) {
        EXPECT_TRUE(action.presentsPopup);
        EXPECT_NOT_NULL(action.popupWebView);
    };

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Test Popup Action");

    [manager.get().context performActionForTab:manager.get().defaultTab];

    [manager run];
}

TEST(WKWebExtensionAPIAction, SetDefaultActionProperties)
{
    auto *popupPage = @"<b>Hello World!</b>";

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(await browser.action.getTitle({ }), 'Test Action', 'Title should be')",
        @"browser.test.assertEq(await browser.action.getPopup({ }), 'popup.html', 'Popup should be')",
        @"browser.test.assertEq(await browser.action.getBadgeText({ }), '', 'Badge text should be')",
        @"browser.test.assertTrue(await browser.action.isEnabled({ }), 'Action should be enabled')",

        @"await browser.action.setTitle({ title: 'Modified Title' })",
        @"await browser.action.setIcon({ path: 'toolbar-48.png' })",
        @"await browser.action.setPopup({ popup: 'alt-popup.html' })",
        @"await browser.action.setBadgeText({ text: '42' })",
        @"await browser.action.disable()",

        @"browser.test.assertEq(await browser.action.getTitle({ }), 'Modified Title', 'Title should be')",
        @"browser.test.assertEq(await browser.action.getPopup({ }), 'alt-popup.html', 'Popup should be')",
        @"browser.test.assertEq(await browser.action.getBadgeText({ }), '42', 'Badge text should be')",
        @"browser.test.assertFalse(await browser.action.isEnabled({ }), 'Action should be disabled')",

        @"browser.action.openPopup()"
    ]);

    auto *extraLargeToolbarIcon = Util::makePNGData(CGSizeMake(48, 48), @selector(yellowColor));

    auto *resources = @{
        @"background.js": backgroundScript,
        @"alt-popup.html": popupPage,
        @"toolbar-48.png": extraLargeToolbarIcon,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentPopupForAction = ^(WKWebExtensionAction *action) {
        auto *defaultAction = [manager.get().context actionForTab:nil];

        EXPECT_TRUE(defaultAction.presentsPopup);
        EXPECT_FALSE(defaultAction.isEnabled);
        EXPECT_NS_EQUAL(defaultAction.label, @"Modified Title");
        EXPECT_NS_EQUAL(defaultAction.badgeText, @"42");
        EXPECT_FALSE(defaultAction.hasUnreadBadgeText);

        EXPECT_NS_EQUAL(action.associatedTab, manager.get().defaultTab);

        EXPECT_FALSE(action.isEnabled);
        EXPECT_NS_EQUAL(action.label, @"Modified Title");
        EXPECT_NS_EQUAL(action.badgeText, @"42");
        EXPECT_FALSE(action.hasUnreadBadgeText);

        auto *icon = [action iconForSize:CGSizeMake(48, 48)];
        EXPECT_NOT_NULL(icon);
        EXPECT_TRUE(CGSizeEqualToSize(icon.size, CGSizeMake(48, 48)));

        EXPECT_TRUE(action.presentsPopup);
        EXPECT_FALSE(action.isEnabled);

        EXPECT_NOT_NULL(action.popupWebView);
        EXPECT_FALSE(action.popupWebView.loading);

        NSURL *webViewURL = action.popupWebView.URL;
        EXPECT_NS_EQUAL(webViewURL.scheme, @"webkit-extension");
        EXPECT_NS_EQUAL(webViewURL.path, @"/alt-popup.html");

        [action closePopup];

        [manager done];
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIAction, TabSpecificActionProperties)
{
    auto *popupPage = @"<b>Hello World!</b>";
    auto *altPopupPage = @"<b>Hello Alternate World!</b>";

    auto *backgroundScript = Util::constructScript(@[
        @"const [currentTab] = await browser.tabs.query({ active: true, currentWindow: true })",

        @"browser.test.assertEq(await browser.action.getTitle({ tabId: currentTab.id }), 'Test Action', 'Title should be')",
        @"browser.test.assertEq(await browser.action.getPopup({ tabId: currentTab.id }), 'popup.html', 'Popup should be')",
        @"browser.test.assertEq(await browser.action.getBadgeText({ tabId: currentTab.id }), '', 'Badge text should be')",
        @"browser.test.assertTrue(await browser.action.isEnabled({ tabId: currentTab.id }), 'Action should be enabled')",

        @"browser.action.setTitle({ title: 'Tab Title', tabId: currentTab.id })",
        @"browser.action.setIcon({ path: 'toolbar-48.png', tabId: currentTab.id })",
        @"browser.action.setPopup({ popup: 'alt-popup.html', tabId: currentTab.id })",
        @"browser.action.setBadgeText({ text: '42', tabId: currentTab.id })",
        @"browser.action.disable(currentTab.id)",

        @"browser.test.assertEq(await browser.action.getTitle({ tabId: currentTab.id }), 'Tab Title', 'Title should be')",
        @"browser.test.assertEq(await browser.action.getPopup({ tabId: currentTab.id }), 'alt-popup.html', 'Popup should be')",
        @"browser.test.assertEq(await browser.action.getBadgeText({ tabId: currentTab.id }), '42', 'Badge text should be')",
        @"browser.test.assertFalse(await browser.action.isEnabled({ tabId: currentTab.id }), 'Action should be disabled')",

        @"browser.action.openPopup({ windowId: currentTab.windowId })"
    ]);

    auto *smallToolbarIcon = Util::makePNGData(CGSizeMake(16, 16), @selector(redColor));
    auto *largeToolbarIcon = Util::makePNGData(CGSizeMake(32, 32), @selector(blueColor));
    auto *extraLargeToolbarIcon = Util::makePNGData(CGSizeMake(48, 48), @selector(yellowColor));

    auto *resources = @{
        @"background.js": backgroundScript,
        @"popup.html": popupPage,
        @"alt-popup.html": altPopupPage,
        @"toolbar-16.png": smallToolbarIcon,
        @"toolbar-32.png": largeToolbarIcon,
        @"toolbar-48.png": extraLargeToolbarIcon,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentPopupForAction = ^(WKWebExtensionAction *action) {
        auto *defaultAction = [manager.get().context actionForTab:nil];

        EXPECT_TRUE(defaultAction.presentsPopup);
        EXPECT_TRUE(defaultAction.isEnabled);
        EXPECT_NS_EQUAL(defaultAction.label, @"Test Action");
        EXPECT_NS_EQUAL(defaultAction.badgeText, @"");
        EXPECT_FALSE(defaultAction.hasUnreadBadgeText);

        auto *defaultIcon = [defaultAction iconForSize:CGSizeMake(32, 32)];
        EXPECT_NOT_NULL(defaultIcon);
        EXPECT_TRUE(CGSizeEqualToSize(defaultIcon.size, CGSizeMake(32, 32)));

        EXPECT_FALSE(action.isEnabled);
        EXPECT_NS_EQUAL(action.label, @"Tab Title");
        EXPECT_NS_EQUAL(action.badgeText, @"42");
        EXPECT_FALSE(action.hasUnreadBadgeText);

        auto *icon = [action iconForSize:CGSizeMake(48, 48)];
        EXPECT_NOT_NULL(icon);
        EXPECT_TRUE(CGSizeEqualToSize(icon.size, CGSizeMake(48, 48)));

        EXPECT_TRUE(action.presentsPopup);
        EXPECT_FALSE(action.isEnabled);

        EXPECT_NOT_NULL(action.popupWebView);
        EXPECT_FALSE(action.popupWebView.loading);

        NSURL *webViewURL = action.popupWebView.URL;
        EXPECT_NS_EQUAL(webViewURL.scheme, @"webkit-extension");
        EXPECT_NS_EQUAL(webViewURL.path, @"/alt-popup.html");

        auto *secondTabAction = [manager.get().context actionForTab:manager.get().defaultWindow.tabs.lastObject];

        EXPECT_EQ(secondTabAction.presentsPopup, defaultAction.presentsPopup);
        EXPECT_EQ(secondTabAction.isEnabled, defaultAction.isEnabled);
        EXPECT_NS_EQUAL(secondTabAction.label, defaultAction.label);
        EXPECT_NS_EQUAL(secondTabAction.badgeText, defaultAction.badgeText);
        EXPECT_FALSE(secondTabAction.hasUnreadBadgeText);

        icon = [secondTabAction iconForSize:CGSizeMake(32, 32)];
        EXPECT_NOT_NULL(icon);
        EXPECT_TRUE(CGSizeEqualToSize(icon.size, defaultIcon.size));

        webViewURL = secondTabAction.popupWebView.URL;
        EXPECT_NS_EQUAL(webViewURL.scheme, @"webkit-extension");
        EXPECT_NS_EQUAL(webViewURL.path, @"/popup.html");

        auto *secondWindowAction = [manager.get().context actionForTab:manager.get().windows[1].tabs[0]];
        EXPECT_EQ(secondWindowAction.presentsPopup, defaultAction.presentsPopup);
        EXPECT_EQ(secondWindowAction.isEnabled, defaultAction.isEnabled);
        EXPECT_NS_EQUAL(secondWindowAction.label, defaultAction.label);
        EXPECT_NS_EQUAL(secondWindowAction.badgeText, defaultAction.badgeText);
        EXPECT_FALSE(secondWindowAction.hasUnreadBadgeText);

        icon = [secondWindowAction iconForSize:CGSizeMake(32, 32)];
        EXPECT_NOT_NULL(icon);
        EXPECT_TRUE(CGSizeEqualToSize(icon.size, defaultIcon.size));

        webViewURL = secondWindowAction.popupWebView.URL;
        EXPECT_NS_EQUAL(webViewURL.scheme, @"webkit-extension");
        EXPECT_NS_EQUAL(webViewURL.path, @"/popup.html");

        [secondTabAction closePopup];
        [secondWindowAction closePopup];
        [action closePopup];

        [manager done];
    };

    [manager.get().defaultWindow openNewTab];
    [manager openNewWindow];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIAction, WindowSpecificActionProperties)
{
    auto *popupPage = @"<b>Hello World!</b>";
    auto *windowPopupPage = @"<b>Window-Specific Popup!</b>";

    auto *backgroundScript = Util::constructScript(@[
        @"const [currentTab] = await browser.tabs.query({ active: true, currentWindow: true })",
        @"const currentWindowId = currentTab.windowId",

        @"browser.test.assertEq(await browser.action.getTitle({ windowId: currentWindowId }), 'Test Action', 'Title should be')",
        @"browser.test.assertEq(await browser.action.getPopup({ windowId: currentWindowId }), 'popup.html', 'Popup should be')",
        @"browser.test.assertEq(await browser.action.getBadgeText({ windowId: currentWindowId }), '', 'Badge text should be')",

        @"browser.action.setTitle({ title: 'Window Title', windowId: currentWindowId })",
        @"browser.action.setIcon({ path: 'window-toolbar-48.png', windowId: currentWindowId })",
        @"browser.action.setPopup({ popup: 'window-popup.html', windowId: currentWindowId })",
        @"browser.action.setBadgeText({ text: 'W', windowId: currentWindowId })",

        @"browser.test.assertEq(await browser.action.getTitle({ windowId: currentWindowId }), 'Window Title', 'Title should be')",
        @"browser.test.assertEq(await browser.action.getPopup({ windowId: currentWindowId }), 'window-popup.html', 'Popup should be')",
        @"browser.test.assertEq(await browser.action.getBadgeText({ windowId: currentWindowId }), 'W', 'Badge text should be')",

        @"browser.action.openPopup({ windowId: currentWindowId })"
    ]);

    auto *defaultToolbarIcon = Util::makePNGData(CGSizeMake(32, 32), @selector(redColor));
    auto *windowToolbarIcon = Util::makePNGData(CGSizeMake(48, 48), @selector(greenColor));

    auto *resources = @{
        @"background.js": backgroundScript,
        @"popup.html": popupPage,
        @"window-popup.html": windowPopupPage,
        @"toolbar-32.png": defaultToolbarIcon,
        @"window-toolbar-48.png": windowToolbarIcon,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentPopupForAction = ^(WKWebExtensionAction *action) {
        auto *defaultAction = [manager.get().context actionForTab:nil];

        EXPECT_TRUE(defaultAction.presentsPopup);
        EXPECT_TRUE(defaultAction.isEnabled);
        EXPECT_NS_EQUAL(defaultAction.label, @"Test Action");
        EXPECT_NS_EQUAL(defaultAction.badgeText, @"");
        EXPECT_FALSE(defaultAction.hasUnreadBadgeText);

        auto *defaultIcon = [defaultAction iconForSize:CGSizeMake(32, 32)];
        EXPECT_NOT_NULL(defaultIcon);
        EXPECT_TRUE(CGSizeEqualToSize(defaultIcon.size, CGSizeMake(32, 32)));

        EXPECT_TRUE(action.isEnabled);
        EXPECT_NS_EQUAL(action.label, @"Window Title");
        EXPECT_NS_EQUAL(action.badgeText, @"W");
        EXPECT_FALSE(action.hasUnreadBadgeText);

        auto *windowIcon = [action iconForSize:CGSizeMake(48, 48)];
        EXPECT_NOT_NULL(windowIcon);
        EXPECT_TRUE(CGSizeEqualToSize(windowIcon.size, CGSizeMake(48, 48)));

        EXPECT_TRUE(action.presentsPopup);
        NSURL *webViewURL = action.popupWebView.URL;
        EXPECT_NS_EQUAL(webViewURL.scheme, @"webkit-extension");
        EXPECT_NS_EQUAL(webViewURL.path, @"/window-popup.html");

        auto *secondWindowAction = [manager.get().context actionForTab:manager.get().windows[1].tabs[0]];
        EXPECT_EQ(secondWindowAction.presentsPopup, defaultAction.presentsPopup);
        EXPECT_EQ(secondWindowAction.isEnabled, defaultAction.isEnabled);
        EXPECT_NS_EQUAL(secondWindowAction.label, defaultAction.label);
        EXPECT_NS_EQUAL(secondWindowAction.badgeText, defaultAction.badgeText);
        EXPECT_FALSE(secondWindowAction.hasUnreadBadgeText);

        auto *secondWindowIcon = [secondWindowAction iconForSize:CGSizeMake(32, 32)];
        EXPECT_NOT_NULL(secondWindowIcon);
        EXPECT_TRUE(CGSizeEqualToSize(secondWindowIcon.size, defaultIcon.size));

        webViewURL = secondWindowAction.popupWebView.URL;
        EXPECT_NS_EQUAL(webViewURL.scheme, @"webkit-extension");
        EXPECT_NS_EQUAL(webViewURL.path, @"/popup.html");

        [secondWindowAction closePopup];
        [action closePopup];

        [manager done];
    };

    [manager openNewWindow];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIAction, SetIconSinglePath)
{
    auto *backgroundScript = Util::constructScript(@[
        @"await browser.action.setIcon({ path: 'toolbar-48.png' })",
    ]);

    auto *extraLargeToolbarIcon = Util::makePNGData(CGSizeMake(48, 48), @selector(yellowColor));

    auto *resources = @{
        @"background.js": backgroundScript,
        @"toolbar-48.png": extraLargeToolbarIcon,
        @"popup.html": @"Hello world!",
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.didUpdateAction = ^(WKWebExtensionAction *action) {
        auto *icon = [action iconForSize:CGSizeMake(48, 48)];
        EXPECT_NOT_NULL(icon);
        EXPECT_TRUE(CGSizeEqualToSize(icon.size, CGSizeMake(48, 48)));

        [manager done];
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIAction, SetIconSinglePathRelative)
{
    auto *backgroundScript = Util::constructScript(@[
        @"await browser.action.setIcon({ path: '../icons/toolbar-48.png' })",
    ]);

    auto *extraLargeToolbarIcon = Util::makePNGData(CGSizeMake(48, 48), @selector(yellowColor));

    auto *resources = @{
        @"background/index.html": @"<script type='module' src='script.js'></script>",
        @"background/script.js": backgroundScript,
        @"icons/toolbar-48.png": extraLargeToolbarIcon,
        @"popup.html": @"Hello world!",
    };

    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Action Test",
        @"description": @"Action Test",
        @"version": @"1",

        @"background": @{
            @"page": @"background/index.html",
            @"type": @"module",
            @"persistent": @NO,
        },

        @"action": @{
            @"default_title": @"Test Action",
            @"default_popup": @"popup.html",
            @"default_icon": @{
                @"16": @"icons/toolbar-16.png",
                @"32": @"icons/toolbar-32.png",
            },
        },
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.didUpdateAction = ^(WKWebExtensionAction *action) {
        auto *icon = [action iconForSize:CGSizeMake(48, 48)];
        EXPECT_NOT_NULL(icon);
        EXPECT_TRUE(CGSizeEqualToSize(icon.size, CGSizeMake(48, 48)));

        [manager done];
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIAction, SetIconMultipleSizes)
{
    auto *backgroundScript = Util::constructScript(@[
        @"await browser.action.setIcon({ path: { '48': 'toolbar-48.png', '96': 'toolbar-96.png', '128': 'toolbar-128.png' } })",
    ]);

    auto *largeToolbarIcon = Util::makePNGData(CGSizeMake(48, 48), @selector(yellowColor));
    auto *extraLargeToolbarIcon = Util::makePNGData(CGSizeMake(96, 96), @selector(greenColor));
    auto *superExtraLargeToolbarIcon = Util::makePNGData(CGSizeMake(128, 128), @selector(purpleColor));

    auto *resources = @{
        @"background.js": backgroundScript,
        @"toolbar-48.png": largeToolbarIcon,
        @"toolbar-96.png": extraLargeToolbarIcon,
        @"toolbar-128.png": superExtraLargeToolbarIcon,
        @"popup.html": @"Hello world!",
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.didUpdateAction = ^(WKWebExtensionAction *action) {
        auto *icon48 = [action iconForSize:CGSizeMake(48, 48)];
        auto *icon96 = [action iconForSize:CGSizeMake(96, 96)];
        auto *icon128 = [action iconForSize:CGSizeMake(128, 128)];

        EXPECT_NOT_NULL(icon48);
        EXPECT_TRUE(CGSizeEqualToSize(icon48.size, CGSizeMake(48, 48)));

        EXPECT_NOT_NULL(icon96);
        EXPECT_TRUE(CGSizeEqualToSize(icon96.size, CGSizeMake(96, 96)));

        EXPECT_NOT_NULL(icon128);
        EXPECT_TRUE(CGSizeEqualToSize(icon128.size, CGSizeMake(128, 128)));

        [manager done];
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIAction, SetIconMultipleSizesRelative)
{
    auto *backgroundScript = Util::constructScript(@[
        @"await browser.action.setIcon({ path: { '48': '../icons/toolbar-48.png', '96': '../icons/toolbar-96.png', '128': '../icons/toolbar-128.png' } })",
    ]);

    auto *largeToolbarIcon = Util::makePNGData(CGSizeMake(48, 48), @selector(yellowColor));
    auto *extraLargeToolbarIcon = Util::makePNGData(CGSizeMake(96, 96), @selector(greenColor));
    auto *superExtraLargeToolbarIcon = Util::makePNGData(CGSizeMake(128, 128), @selector(purpleColor));

    auto *resources = @{
        @"background/index.html": @"<script type='module' src='script.js'></script>",
        @"background/script.js": backgroundScript,
        @"icons/toolbar-48.png": largeToolbarIcon,
        @"icons/toolbar-96.png": extraLargeToolbarIcon,
        @"icons/toolbar-128.png": superExtraLargeToolbarIcon,
        @"popup.html": @"Hello world!",
    };

    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Action Test",
        @"description": @"Action Test",
        @"version": @"1",

        @"background": @{
            @"page": @"background/index.html",
            @"type": @"module",
            @"persistent": @NO,
        },

        @"action": @{
            @"default_title": @"Test Action",
            @"default_popup": @"popup.html",
            @"default_icon": @{
                @"16": @"icons/toolbar-16.png",
                @"32": @"icons/toolbar-32.png",
            },
        },
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.didUpdateAction = ^(WKWebExtensionAction *action) {
        auto *icon48 = [action iconForSize:CGSizeMake(48, 48)];
        auto *icon96 = [action iconForSize:CGSizeMake(96, 96)];
        auto *icon128 = [action iconForSize:CGSizeMake(128, 128)];

        EXPECT_NOT_NULL(icon48);
        EXPECT_TRUE(CGSizeEqualToSize(icon48.size, CGSizeMake(48, 48)));

        EXPECT_NOT_NULL(icon96);
        EXPECT_TRUE(CGSizeEqualToSize(icon96.size, CGSizeMake(96, 96)));

        EXPECT_NOT_NULL(icon128);
        EXPECT_TRUE(CGSizeEqualToSize(icon128.size, CGSizeMake(128, 128)));

        [manager done];
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIAction, SetIconWithBadPath)
{
    auto *backgroundScript = Util::constructScript(@[
        @"await browser.action.setIcon({ path: 'bad.png' })",
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"popup.html": @"Hello world!",
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.didUpdateAction = ^(WKWebExtensionAction *action) {
        auto *icon = [action iconForSize:CGSizeMake(48, 48)];
        EXPECT_NULL(icon);

        [manager done];
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIAction, SetIconWithImageData)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const context = new OffscreenCanvas(48, 48).getContext('2d')",
        @"context.fillStyle = 'green'",
        @"context.fillRect(0, 0, 48, 48)",

        @"const imageData = context.getImageData(0, 0, 48, 48)",
        @"await browser.action.setIcon({ imageData })",
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"popup.html": @"Hello world!",
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.didUpdateAction = ^(WKWebExtensionAction *action) {
        auto *icon = [action iconForSize:CGSizeMake(48, 48)];

        EXPECT_NOT_NULL(icon);
        EXPECT_TRUE(CGSizeEqualToSize(icon.size, CGSizeMake(48, 48)));

        [manager done];
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIAction, SetIconWithBadImageData)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertThrows(() => browser.action.setIcon({ imageData: { 16: { data: [ 'bad' ] } } }))",

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(actionPopupManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIAction, SetIconWithMultipleImageDataSizes)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const createImageData = (size, color) => {",
        @"  const context = new OffscreenCanvas(size, size).getContext('2d')",
        @"  context.fillStyle = color",
        @"  context.fillRect(0, 0, size, size)",

        @"  return context.getImageData(0, 0, size, size)",
        @"}",

        @"const imageData48 = createImageData(48, 'green')",
        @"const imageData96 = createImageData(96, 'blue')",
        @"const imageData128 = createImageData(128, 'red')",

        @"await browser.action.setIcon({ imageData: { '48': imageData48, '96': imageData96, '128': imageData128 } })",
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"popup.html": @"Hello world!",
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.didUpdateAction = ^(WKWebExtensionAction *action) {
        auto *icon48 = [action iconForSize:CGSizeMake(48, 48)];
        auto *icon96 = [action iconForSize:CGSizeMake(96, 96)];
        auto *icon128 = [action iconForSize:CGSizeMake(128, 128)];

        EXPECT_NOT_NULL(icon48);
        EXPECT_TRUE(CGSizeEqualToSize(icon48.size, CGSizeMake(48, 48)));

        EXPECT_NOT_NULL(icon96);
        EXPECT_TRUE(CGSizeEqualToSize(icon96.size, CGSizeMake(96, 96)));

        EXPECT_NOT_NULL(icon128);
        EXPECT_TRUE(CGSizeEqualToSize(icon128.size, CGSizeMake(128, 128)));

        [manager done];
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIAction, SetIconWithDataURL)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const canvas = document.createElement('canvas')",
        @"canvas.width = 48",
        @"canvas.height = 48",

        @"const context = canvas.getContext('2d')",
        @"context.fillStyle = 'blue'",
        @"context.fillRect(0, 0, 48, 48)",

        @"const pngDataURL48 = canvas.toDataURL('image/png')",

        @"await browser.action.setIcon({ path: pngDataURL48 })",
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"popup.html": @"Hello world!",
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.didUpdateAction = ^(WKWebExtensionAction *action) {
        auto *icon = [action iconForSize:CGSizeMake(48, 48)];
        EXPECT_NOT_NULL(icon);
        EXPECT_TRUE(CGSizeEqualToSize(icon.size, CGSizeMake(48, 48)));

        [manager done];
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIAction, SetIconWithBadDataURL)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const invalidDataURL = 'data:image/png;base64,INVALIDDATA'",

        @"await browser.action.setIcon({ path: invalidDataURL })",
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"popup.html": @"Hello world!",
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.didUpdateAction = ^(WKWebExtensionAction *action) {
        auto *icon = [action iconForSize:CGSizeMake(48, 48)];
        EXPECT_NULL(icon);

        [manager done];
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIAction, SetIconWithMultipleDataURLs)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const canvas = document.createElement('canvas')",

        @"canvas.width = 48",
        @"canvas.height = 48",

        @"let context = canvas.getContext('2d')",
        @"context.fillStyle = 'blue'",
        @"context.fillRect(0, 0, 48, 48)",

        @"const pngDataURL48 = canvas.toDataURL('image/png')",

        @"canvas.width = 96",
        @"canvas.height = 96",

        @"context = canvas.getContext('2d')",
        @"context.fillStyle = 'red'",
        @"context.fillRect(0, 0, 96, 96)",

        @"const pngDataURL96 = canvas.toDataURL('image/png')",

        @"await browser.action.setIcon({ path: { '48': pngDataURL48, '96': pngDataURL96 } })",
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"popup.html": @"Hello world!",
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.didUpdateAction = ^(WKWebExtensionAction *action) {
        auto *icon48 = [action iconForSize:CGSizeMake(48, 48)];
        EXPECT_NOT_NULL(icon48);
        EXPECT_TRUE(CGSizeEqualToSize(icon48.size, CGSizeMake(48, 48)));

        auto *icon96 = [action iconForSize:CGSizeMake(96, 96)];
        EXPECT_NOT_NULL(icon96);
        EXPECT_TRUE(CGSizeEqualToSize(icon96.size, CGSizeMake(96, 96)));

        [manager done];
    };

    [manager loadAndRun];
}

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
TEST(WKWebExtensionAPIAction, SetIconWithVariants)
{
    auto *backgroundScript = Util::constructScript(@[
        @"await browser.test.assertSafeResolve(() => browser.action.setIcon({",
        @"    variants: [",
        @"        { 32: 'action-dark-32.png', 64: 'action-dark-64.png', 'color_schemes': [ 'dark' ] },",
        @"        { 32: 'action-light-32.png', 64: 'action-light-64.png', 'color_schemes': [ 'light' ] }",
        @"    ]",
        @"}))",
    ]);

    auto *dark32Icon = Util::makePNGData(CGSizeMake(32, 32), @selector(whiteColor));
    auto *dark64Icon = Util::makePNGData(CGSizeMake(64, 64), @selector(whiteColor));
    auto *light32Icon = Util::makePNGData(CGSizeMake(32, 32), @selector(blackColor));
    auto *light64Icon = Util::makePNGData(CGSizeMake(64, 64), @selector(blackColor));

    auto *resources = @{
        @"background.js": backgroundScript,
        @"popup.html": @"Hello world!",
        @"action-dark-32.png": dark32Icon,
        @"action-dark-64.png": dark64Icon,
        @"action-light-32.png": light32Icon,
        @"action-light-64.png": light64Icon,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.didUpdateAction = ^(WKWebExtensionAction *action) {
        auto *icon32 = [action iconForSize:CGSizeMake(32, 32)];
        EXPECT_NOT_NULL(icon32);
        EXPECT_TRUE(CGSizeEqualToSize(icon32.size, CGSizeMake(32, 32)));

        auto *icon64 = [action iconForSize:CGSizeMake(64, 64)];
        EXPECT_NOT_NULL(icon64);
        EXPECT_TRUE(CGSizeEqualToSize(icon64.size, CGSizeMake(64, 64)));

        Util::performWithAppearance(Util::Appearance::Dark, ^{
            EXPECT_TRUE(Util::compareColors(Util::pixelColor(icon32), [CocoaColor whiteColor]));
            EXPECT_TRUE(Util::compareColors(Util::pixelColor(icon64), [CocoaColor whiteColor]));
        });

        Util::performWithAppearance(Util::Appearance::Light, ^{
            EXPECT_TRUE(Util::compareColors(Util::pixelColor(icon32), [CocoaColor blackColor]));
            EXPECT_TRUE(Util::compareColors(Util::pixelColor(icon64), [CocoaColor blackColor]));
        });

        [manager done];
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIAction, SetIconWithImageDataAndVariants)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const createImageData = (size, color) => {",
        @"  const context = new OffscreenCanvas(size, size).getContext('2d')",
        @"  context.fillStyle = color",
        @"  context.fillRect(0, 0, size, size)",

        @"  return context.getImageData(0, 0, size, size)",
        @"}",

        @"const imageDataDark32 = createImageData(32, 'white')",
        @"const imageDataDark64 = createImageData(64, 'white')",
        @"const imageDataLight32 = createImageData(32, 'black')",
        @"const imageDataLight64 = createImageData(64, 'black')",

        @"await browser.test.assertSafeResolve(() => browser.action.setIcon({",
        @"    variants: [",
        @"        { 32: imageDataDark32, 64: imageDataDark64, 'color_schemes': [ 'dark' ] },",
        @"        { 32: imageDataLight32, 64: imageDataLight64, 'color_schemes': [ 'light' ] }",
        @"    ]",
        @"}))",
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"popup.html": @"Hello world!",
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.didUpdateAction = ^(WKWebExtensionAction *action) {
        auto *icon32 = [action iconForSize:CGSizeMake(32, 32)];
        auto *icon64 = [action iconForSize:CGSizeMake(64, 64)];

        EXPECT_NOT_NULL(icon32);
        EXPECT_TRUE(CGSizeEqualToSize(icon32.size, CGSizeMake(32, 32)));

        EXPECT_NOT_NULL(icon64);
        EXPECT_TRUE(CGSizeEqualToSize(icon64.size, CGSizeMake(64, 64)));

        Util::performWithAppearance(Util::Appearance::Dark, ^{
            EXPECT_TRUE(Util::compareColors(Util::pixelColor(icon32), [CocoaColor whiteColor]));
            EXPECT_TRUE(Util::compareColors(Util::pixelColor(icon64), [CocoaColor whiteColor]));
        });

        Util::performWithAppearance(Util::Appearance::Light, ^{
            EXPECT_TRUE(Util::compareColors(Util::pixelColor(icon32), [CocoaColor blackColor]));
            EXPECT_TRUE(Util::compareColors(Util::pixelColor(icon64), [CocoaColor blackColor]));
        });

        [manager done];
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIAction, SetIconThrowsWithNoValidVariants)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const createImageData = (size, color) => {",
        @"  const context = new OffscreenCanvas(size, size).getContext('2d')",
        @"  context.fillStyle = color",
        @"  context.fillRect(0, 0, size, size)",

        @"  return context.getImageData(0, 0, size, size)",
        @"}",

        @"const invalidImageData = createImageData(32, 'white')",

        @"await browser.test.assertThrows(() => browser.action.setIcon({",
        @"    variants: [ { 'thirtytwo': invalidImageData, 'color_schemes': [ 'light' ] } ]",
        @"}), /'variants\\[0\\]' value is invalid, because 'thirtytwo' is not a valid dimension/s)",

        @"await browser.test.assertThrows(() => browser.action.setIcon({",
        @"    variants: [ { 32: invalidImageData, 'color_schemes': [ 'bad' ] } ]",
        @"}), /'variants\\[0\\]\\['color_schemes'\\]' value is invalid, because it must specify either 'light' or 'dark'/s)",

        @"browser.test.notifyPass()"
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"popup.html": @"Hello world!",
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIAction, SetIconWithMixedValidAndInvalidVariants)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const createImageData = (size, color) => {",
        @"  const context = new OffscreenCanvas(size, size).getContext('2d')",
        @"  context.fillStyle = color",
        @"  context.fillRect(0, 0, size, size)",

        @"  return context.getImageData(0, 0, size, size)",
        @"}",

        @"const imageDataLight32 = createImageData(32, 'black')",
        @"const invalidImageData = createImageData(32, 'white')",

        @"await browser.test.assertSafeResolve(() => browser.action.setIcon({",
        @"    variants: [",
        @"        { '32': imageDataLight32, 'color_schemes': ['light'] },",
        @"        { '32.5': invalidImageData, 'color_schemes': ['dark'] }",
        @"    ]",
        @"}))",
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"popup.html": @"Hello world!",
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.didUpdateAction = ^(WKWebExtensionAction *action) {
        auto *icon32 = [action iconForSize:CGSizeMake(32, 32)];
        EXPECT_NOT_NULL(icon32);
        EXPECT_TRUE(CGSizeEqualToSize(icon32.size, CGSizeMake(32, 32)));

        Util::performWithAppearance(Util::Appearance::Light, ^{
            EXPECT_TRUE(Util::compareColors(Util::pixelColor(icon32), [CocoaColor blackColor]));
        });

        Util::performWithAppearance(Util::Appearance::Dark, ^{
            // Should still be black, as light variant is used.
            EXPECT_TRUE(Util::compareColors(Util::pixelColor(icon32), [CocoaColor blackColor]));
        });

        [manager done];
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIAction, SetIconWithAnySizeVariantAndSVGDataURL)
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

        @"await browser.test.assertSafeResolve(() => browser.action.setIcon({",
        @"    variants: [",
        @"        { any: whiteSVGData, 'color_schemes': [ 'dark' ] },",
        @"        { any: blackSVGData, 'color_schemes': [ 'light' ] }",
        @"    ]",
        @"}))",
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"popup.html": @"Hello World!",
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.didUpdateAction = ^(WKWebExtensionAction *action) {
        auto *iconAnySize = [action iconForSize:CGSizeMake(48, 48)];

        EXPECT_NOT_NULL(iconAnySize);
        EXPECT_TRUE(CGSizeEqualToSize(iconAnySize.size, CGSizeMake(48, 48)));

        Util::performWithAppearance(Util::Appearance::Dark, ^{
            EXPECT_TRUE(Util::compareColors(Util::pixelColor(iconAnySize), [CocoaColor whiteColor]));
        });

        Util::performWithAppearance(Util::Appearance::Light, ^{
            EXPECT_TRUE(Util::compareColors(Util::pixelColor(iconAnySize), [CocoaColor blackColor]));
        });

        [manager done];
    };

    [manager loadAndRun];
}
#endif // ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)

TEST(WKWebExtensionAPIAction, BrowserAction)
{
    auto *browserActionManifest = @{
        @"manifest_version": @2,

        @"name": @"Browser Action Test",
        @"description": @"Browser Action Test",
        @"version": @"1",

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"type": @"module",
            @"persistent": @NO,
        },

        @"browser_action": @{
            @"default_title": @"Test Browser Action",
            @"default_popup": @"popup.html",
            @"default_icon": @{
                @"16": @"toolbar-16.png",
                @"32": @"toolbar-32.png",
            },
        },
    };

    auto *popupPage = @"<b>Hello World!</b>";

    auto *backgroundScript = Util::constructScript(@[
        @"await browser.browserAction.setTitle({ title: 'Modified Title' })",
        @"await browser.browserAction.setIcon({ path: 'toolbar-48.png' })",
        @"await browser.browserAction.setPopup({ popup: 'alt-popup.html' })",
        @"await browser.browserAction.setBadgeText({ text: '42' })",
        @"await browser.browserAction.disable()",

        @"browser.browserAction.openPopup()"
    ]);

    auto *extraLargeToolbarIcon = Util::makePNGData(CGSizeMake(48, 48), @selector(yellowColor));

    auto *resources = @{
        @"background.js": backgroundScript,
        @"alt-popup.html": popupPage,
        @"toolbar-48.png": extraLargeToolbarIcon,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:browserActionManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentPopupForAction = ^(WKWebExtensionAction *action) {
        auto *defaultAction = [manager.get().context actionForTab:nil];

        EXPECT_TRUE(defaultAction.presentsPopup);
        EXPECT_FALSE(defaultAction.isEnabled);
        EXPECT_NS_EQUAL(defaultAction.label, @"Modified Title");
        EXPECT_NS_EQUAL(defaultAction.badgeText, @"42");
        EXPECT_FALSE(defaultAction.hasUnreadBadgeText);

        EXPECT_NS_EQUAL(action.associatedTab, manager.get().defaultTab);

        EXPECT_FALSE(action.isEnabled);
        EXPECT_NS_EQUAL(action.label, @"Modified Title");
        EXPECT_NS_EQUAL(action.badgeText, @"42");
        EXPECT_FALSE(action.hasUnreadBadgeText);

        auto *icon = [action iconForSize:CGSizeMake(48, 48)];
        EXPECT_NOT_NULL(icon);
        EXPECT_TRUE(CGSizeEqualToSize(icon.size, CGSizeMake(48, 48)));

        EXPECT_TRUE(action.presentsPopup);
        EXPECT_FALSE(action.isEnabled);

#if PLATFORM(IOS_FAMILY)
        EXPECT_NOT_NULL(action.popupViewController);
#endif

#if PLATFORM(MAC)
        EXPECT_NOT_NULL(action.popupPopover);
#endif

        EXPECT_NOT_NULL(action.popupWebView);
        EXPECT_FALSE(action.popupWebView.loading);

        NSURL *webViewURL = action.popupWebView.URL;
        EXPECT_NS_EQUAL(webViewURL.scheme, @"webkit-extension");
        EXPECT_NS_EQUAL(webViewURL.path, @"/alt-popup.html");

        [action closePopup];

        [manager done];
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIAction, PageAction)
{
    auto *pageActionManifest = @{
        @"manifest_version": @2,

        @"name": @"Page Action Test",
        @"description": @"Page Action Test",
        @"version": @"1",

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"type": @"module",
            @"persistent": @NO,
        },

        @"page_action": @{
            @"default_title": @"Test Page Action",
            @"default_popup": @"popup.html",
            @"default_icon": @{
                @"16": @"toolbar-16.png",
                @"32": @"toolbar-32.png",
            },
        },
    };

    auto *popupPage = @"<b>Hello World!</b>";

    auto *backgroundScript = Util::constructScript(@[
        @"await browser.pageAction.setTitle({ title: 'Modified Title' })",
        @"await browser.pageAction.setIcon({ path: 'toolbar-48.png' })",
        @"await browser.pageAction.setPopup({ popup: 'alt-popup.html' })",
        @"await browser.pageAction.setBadgeText({ text: '42' })",
        @"await browser.pageAction.disable()",

        @"browser.pageAction.openPopup()"
    ]);

    auto *extraLargeToolbarIcon = Util::makePNGData(CGSizeMake(48, 48), @selector(yellowColor));

    auto *resources = @{
        @"background.js": backgroundScript,
        @"alt-popup.html": popupPage,
        @"toolbar-48.png": extraLargeToolbarIcon,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:pageActionManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentPopupForAction = ^(WKWebExtensionAction *action) {
        auto *defaultAction = [manager.get().context actionForTab:nil];

        EXPECT_TRUE(defaultAction.presentsPopup);
        EXPECT_FALSE(defaultAction.isEnabled);
        EXPECT_NS_EQUAL(defaultAction.label, @"Modified Title");
        EXPECT_NS_EQUAL(defaultAction.badgeText, @"42");
        EXPECT_FALSE(defaultAction.hasUnreadBadgeText);

        EXPECT_NS_EQUAL(action.associatedTab, manager.get().defaultTab);

        EXPECT_FALSE(action.isEnabled);
        EXPECT_NS_EQUAL(action.label, @"Modified Title");
        EXPECT_NS_EQUAL(action.badgeText, @"42");
        EXPECT_FALSE(action.hasUnreadBadgeText);

        auto *icon = [action iconForSize:CGSizeMake(48, 48)];
        EXPECT_NOT_NULL(icon);
        EXPECT_TRUE(CGSizeEqualToSize(icon.size, CGSizeMake(48, 48)));

        EXPECT_TRUE(action.presentsPopup);
        EXPECT_FALSE(action.isEnabled);

#if PLATFORM(IOS_FAMILY)
        EXPECT_NOT_NULL(action.popupViewController);
#endif

#if PLATFORM(MAC)
        EXPECT_NOT_NULL(action.popupPopover);
#endif

        EXPECT_NOT_NULL(action.popupWebView);
        EXPECT_FALSE(action.popupWebView.loading);

        NSURL *webViewURL = action.popupWebView.URL;
        EXPECT_NS_EQUAL(webViewURL.scheme, @"webkit-extension");
        EXPECT_NS_EQUAL(webViewURL.path, @"/alt-popup.html");

        [action closePopup];

        [manager done];
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIAction, ClearTabSpecificActionPropertiesOnNavigation)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"const [currentTab] = await browser.tabs.query({ active: true, currentWindow: true })",

        @"browser.action.setTitle({ title: 'Tab Title', tabId: currentTab.id })",
        @"browser.action.setIcon({ path: 'toolbar-48.png', tabId: currentTab.id })",
        @"browser.action.setPopup({ popup: 'alt-popup.html', tabId: currentTab.id })",
        @"browser.action.setBadgeText({ text: '42', tabId: currentTab.id })",
        @"browser.action.disable(currentTab.id)",

        @"browser.test.assertEq(await browser.action.getTitle({ tabId: currentTab.id }), 'Tab Title', 'Title should be before navigation')",
        @"browser.test.assertEq(await browser.action.getPopup({ tabId: currentTab.id }), 'alt-popup.html', 'Popup should be before navigation')",
        @"browser.test.assertEq(await browser.action.getBadgeText({ tabId: currentTab.id }), '42', 'Badge text should be before navigation')",
        @"browser.test.assertFalse(await browser.action.isEnabled({ tabId: currentTab.id }), 'Action should be disabled before navigation')",

        @"browser.webNavigation.onCompleted.addListener(async (details) => {",
        @"  browser.test.assertEq(details.tabId, currentTab.id, 'Only the tab we expect should be changing')",
        @"  browser.test.assertEq(details.frameId, 0, 'Only main frame should be changing')",

        @"  browser.test.assertEq(await browser.action.getTitle({ tabId: currentTab.id }), 'Test Action', 'Title should be after navigation')",
        @"  browser.test.assertEq(await browser.action.getPopup({ tabId: currentTab.id }), 'popup.html', 'Popup should be after navigation')",
        @"  browser.test.assertEq(await browser.action.getBadgeText({ tabId: currentTab.id }), '', 'Badge text should be after navigation')",
        @"  browser.test.assertTrue(await browser.action.isEnabled({ tabId: currentTab.id }), 'Action should be enabled after navigation')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    auto *smallToolbarIcon = Util::makePNGData(CGSizeMake(16, 16), @selector(redColor));
    auto *largeToolbarIcon = Util::makePNGData(CGSizeMake(32, 32), @selector(blueColor));
    auto *extraLargeToolbarIcon = Util::makePNGData(CGSizeMake(48, 48), @selector(yellowColor));

    auto *resources = @{
        @"background.js": backgroundScript,
        @"toolbar-16.png": smallToolbarIcon,
        @"toolbar-32.png": largeToolbarIcon,
        @"toolbar-48.png": extraLargeToolbarIcon,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionWebNavigation];

    auto *localhostRequest = server.requestWithLocalhost();
    auto *addressRequest = server.request();

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:localhostRequest.URL];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:addressRequest.URL];

    [manager.get().defaultTab.webView loadRequest:localhostRequest];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:addressRequest];

    [manager run];
}

TEST(WKWebExtensionAPIAction, HasUnreadBadgeText)
{
    auto *backgroundScript = Util::constructScript(@[
        @"await browser.action.setBadgeText({ text: 'New' })",

        @"browser.test.yield('Check Unread Badge Text')"
    ]);

    auto manager = Util::loadAndRunExtension(actionPopupManifest, @{ @"background.js": backgroundScript });

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Check Unread Badge Text");

    auto *defaultAction = [manager.get().context actionForTab:nil];
    auto *tabAction = [manager.get().context actionForTab:manager.get().defaultTab];
    EXPECT_TRUE(defaultAction.hasUnreadBadgeText);
    EXPECT_TRUE(tabAction.hasUnreadBadgeText);

    tabAction.hasUnreadBadgeText = NO;
    EXPECT_FALSE(defaultAction.hasUnreadBadgeText);
    EXPECT_FALSE(tabAction.hasUnreadBadgeText);

    tabAction.hasUnreadBadgeText = YES;
    EXPECT_FALSE(defaultAction.hasUnreadBadgeText);
    EXPECT_TRUE(tabAction.hasUnreadBadgeText);

    tabAction.hasUnreadBadgeText = NO;
    EXPECT_FALSE(defaultAction.hasUnreadBadgeText);
    EXPECT_FALSE(tabAction.hasUnreadBadgeText);

    defaultAction.hasUnreadBadgeText = YES;
    EXPECT_TRUE(defaultAction.hasUnreadBadgeText);
    EXPECT_FALSE(tabAction.hasUnreadBadgeText);

    defaultAction.hasUnreadBadgeText = NO;
    EXPECT_FALSE(defaultAction.hasUnreadBadgeText);
    EXPECT_FALSE(tabAction.hasUnreadBadgeText);
}

TEST(WKWebExtensionAPIAction, NavigationOpensInNewTab)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *localhostRequest = server.requestWithLocalhost();

    auto *popupScript = Util::constructScript(@[
        [NSString stringWithFormat:@"document.location.href = '%@'", localhostRequest.URL.absoluteString],
    ]);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.yield('Open Popup')"
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"popup.html": @"<script type='module' src='popup.js'></script>",
        @"popup.js": popupScript
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);
    auto originalOpenNewTab = manager.get().internalDelegate.openNewTab;

    manager.get().internalDelegate.presentPopupForAction = ^(WKWebExtensionAction *action) {
        EXPECT_NOT_NULL(action);
    };

    manager.get().internalDelegate.openNewTab = ^(WKWebExtensionTabConfiguration *configuration, WKWebExtensionContext *context, void (^completionHandler)(id<WKWebExtensionTab>, NSError *)) {
        EXPECT_NS_EQUAL(configuration.url, localhostRequest.URL);
        EXPECT_NS_EQUAL(configuration.window, manager.get().defaultWindow);
        EXPECT_EQ(configuration.index, 1ul);
        EXPECT_EQ(configuration.shouldBeActive, YES);

        originalOpenNewTab(configuration, context, completionHandler);

        [manager done];
    };

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Open Popup");

    [manager.get().context performActionForTab:manager.get().defaultTab];

    [manager run];
}

TEST(WKWebExtensionAPIAction, WindowOpenOpensInNewWindow)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.yield('Open Popup')"
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"popup.html": @"",
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentPopupForAction = ^(WKWebExtensionAction *action) {
        EXPECT_NOT_NULL(action);

        Util::runScriptWithUserGesture("window.open('https://example.com/', '_blank', 'popup, width=100, height=50')"_s, action.popupWebView);
    };

#if PLATFORM(MAC)
    auto originalOpenNewWindow = manager.get().internalDelegate.openNewWindow;
    manager.get().internalDelegate.openNewWindow = ^(WKWebExtensionWindowConfiguration *configuration, WKWebExtensionContext *context, void (^completionHandler)(id<WKWebExtensionWindow>, NSError *)) {
        EXPECT_EQ(configuration.tabURLs.count, 1lu);
        EXPECT_NS_EQUAL(configuration.tabURLs.firstObject, [NSURL URLWithString:@"https://example.com/"]);

        EXPECT_EQ(configuration.windowType, WKWebExtensionWindowTypePopup);
        EXPECT_EQ(configuration.windowState, WKWebExtensionWindowStateNormal);

        EXPECT_EQ(configuration.frame.size.width, 100);
        EXPECT_EQ(configuration.frame.size.height, 50);
        EXPECT_TRUE(std::isnan(configuration.frame.origin.x));
        EXPECT_TRUE(std::isnan(configuration.frame.origin.y));

        originalOpenNewWindow(configuration, context, completionHandler);

        [manager done];
    };
#else
    auto originalOpenNewTab = manager.get().internalDelegate.openNewTab;
    manager.get().internalDelegate.openNewTab = ^(WKWebExtensionTabConfiguration *configuration, WKWebExtensionContext *context, void (^completionHandler)(id<WKWebExtensionTab>, NSError *)) {
        EXPECT_NS_EQUAL(configuration.url, [NSURL URLWithString:@"https://example.com/"]);
        EXPECT_NS_EQUAL(configuration.window, manager.get().defaultWindow);
        EXPECT_EQ(configuration.index, 1ul);
        EXPECT_EQ(configuration.shouldBeActive, YES);

        originalOpenNewTab(configuration, context, completionHandler);

        [manager done];
    };
#endif

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Open Popup");

    [manager.get().context performActionForTab:manager.get().defaultTab];

    [manager run];
}

TEST(WKWebExtensionAPIAction, EmptyAction)
{
    static auto *actionManifest = @{
        @"manifest_version": @3,

        @"name": @"Test Action",
        @"description": @"Test Action",
        @"version": @"1.0",

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"type": @"module",
            @"persistent": @NO,
        },

        @"action": @{ }
    };

    auto *backgroundScript = Util::constructScript(@[
        @"if (browser.action) {",
        @"  browser.test.notifyPass()",
        @"} else {",
        @"  browser.test.notifyFail('browser.action should be defined when it is empty.')",
        @"}"
    ]);

    Util::loadAndRunExtension(actionManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIAction, ClickedEventAndPermissionsRequest)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.action.setPopup({ popup: '' })",

        @"browser.action.onClicked.addListener(async (tab) => {",
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

        @"browser.test.yield('Test Action')"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.promptForPermissions = ^(id<WKWebExtensionTab> tab, NSSet<NSString *> *requestedPermissions, void (^callback)(NSSet<NSString *> *, NSDate *)) {
        EXPECT_EQ(requestedPermissions.count, 1lu);
        EXPECT_TRUE([requestedPermissions isEqualToSet:[NSSet setWithObject:@"webNavigation"]]);
        callback(requestedPermissions, nil);
    };

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Test Action");

    [manager.get().context performActionForTab:manager.get().defaultTab];

    [manager run];
}

TEST(WKWebExtensionAPIAction, SubframeNavigation)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<script>browser.test.notifyPass()</script>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.yield('Test Popup Action')"
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"popup.html": [NSString stringWithFormat:@"<iframe src='%@'></iframe>", server.requestWithLocalhost("/"_s).URL],
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentPopupForAction = ^(WKWebExtensionAction *action) {
        EXPECT_TRUE(action.presentsPopup);
        EXPECT_NOT_NULL(action.popupWebView);
    };

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Test Popup Action");

    [manager.get().context performActionForTab:manager.get().defaultTab];

    [manager run];
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
