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

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:@{ @"background.js": backgroundScript }]);
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

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentPopupForAction = ^(_WKWebExtensionAction *action) {
        EXPECT_TRUE(action.presentsPopup);
        EXPECT_TRUE(action.isEnabled);
        EXPECT_NULL(action.badgeText);

        EXPECT_NS_EQUAL(action.label, @"Test Action");

        auto *smallIcon = [action iconForSize:CGSizeMake(16, 16)];
        EXPECT_NOT_NULL(smallIcon);
        EXPECT_TRUE(CGSizeEqualToSize(smallIcon.size, CGSizeMake(16, 16)));

        auto *largeIcon = [action iconForSize:CGSizeMake(32, 32)];
        EXPECT_NOT_NULL(largeIcon);
        EXPECT_TRUE(CGSizeEqualToSize(largeIcon.size, CGSizeMake(32, 32)));

        EXPECT_NOT_NULL(action.popupWebView);
        EXPECT_FALSE(action.popupWebView.loading);

        NSURL *webViewURL = action.popupWebView.URL;
        EXPECT_NS_EQUAL(webViewURL.scheme, @"webkit-extension");
        EXPECT_NS_EQUAL(webViewURL.path, @"/popup.html");

        [action closePopupWebView];
    };

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Test Popup Action");

    [manager.get().context performActionForTab:manager.get().defaultTab];
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

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentPopupForAction = ^(_WKWebExtensionAction *action) {
        auto *defaultAction = [manager.get().context actionForTab:nil];

        EXPECT_TRUE(defaultAction.presentsPopup);
        EXPECT_FALSE(defaultAction.isEnabled);
        EXPECT_NS_EQUAL(defaultAction.label, @"Modified Title");
        EXPECT_NS_EQUAL(defaultAction.badgeText, @"42");

        EXPECT_NULL(action.associatedTab);

        EXPECT_FALSE(action.isEnabled);
        EXPECT_NS_EQUAL(action.label, @"Modified Title");
        EXPECT_NS_EQUAL(action.badgeText, @"42");

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

        [action closePopupWebView];

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

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentPopupForAction = ^(_WKWebExtensionAction *action) {
        auto *defaultAction = [manager.get().context actionForTab:nil];

        EXPECT_TRUE(defaultAction.presentsPopup);
        EXPECT_TRUE(defaultAction.isEnabled);
        EXPECT_NS_EQUAL(defaultAction.label, @"Test Action");
        EXPECT_NS_EQUAL(defaultAction.badgeText, @"");

        auto *defaultIcon = [defaultAction iconForSize:CGSizeMake(32, 32)];
        EXPECT_NOT_NULL(defaultIcon);
        EXPECT_TRUE(CGSizeEqualToSize(defaultIcon.size, CGSizeMake(32, 32)));

        EXPECT_FALSE(action.isEnabled);
        EXPECT_NS_EQUAL(action.label, @"Tab Title");
        EXPECT_NS_EQUAL(action.badgeText, @"42");

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

        icon = [secondWindowAction iconForSize:CGSizeMake(32, 32)];
        EXPECT_NOT_NULL(icon);
        EXPECT_TRUE(CGSizeEqualToSize(icon.size, defaultIcon.size));

        webViewURL = secondWindowAction.popupWebView.URL;
        EXPECT_NS_EQUAL(webViewURL.scheme, @"webkit-extension");
        EXPECT_NS_EQUAL(webViewURL.path, @"/popup.html");

        [secondTabAction closePopupWebView];
        [secondWindowAction closePopupWebView];
        [action closePopupWebView];

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

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentPopupForAction = ^(_WKWebExtensionAction *action) {
        auto *defaultAction = [manager.get().context actionForTab:nil];

        EXPECT_TRUE(defaultAction.presentsPopup);
        EXPECT_TRUE(defaultAction.isEnabled);
        EXPECT_NS_EQUAL(defaultAction.label, @"Test Action");
        EXPECT_NS_EQUAL(defaultAction.badgeText, @"");

        auto *defaultIcon = [defaultAction iconForSize:CGSizeMake(32, 32)];
        EXPECT_NOT_NULL(defaultIcon);
        EXPECT_TRUE(CGSizeEqualToSize(defaultIcon.size, CGSizeMake(32, 32)));

        EXPECT_TRUE(action.isEnabled);
        EXPECT_NS_EQUAL(action.label, @"Window Title");
        EXPECT_NS_EQUAL(action.badgeText, @"W");

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

        auto *secondWindowIcon = [secondWindowAction iconForSize:CGSizeMake(32, 32)];
        EXPECT_NOT_NULL(secondWindowIcon);
        EXPECT_TRUE(CGSizeEqualToSize(secondWindowIcon.size, defaultIcon.size));

        webViewURL = secondWindowAction.popupWebView.URL;
        EXPECT_NS_EQUAL(webViewURL.scheme, @"webkit-extension");
        EXPECT_NS_EQUAL(webViewURL.path, @"/popup.html");

        [secondWindowAction closePopupWebView];
        [action closePopupWebView];

        [manager done];
    };

    [manager openNewWindow];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIAction, SetIconSinglePath)
{
    auto *backgroundScript = Util::constructScript(@[
        @"await browser.action.setIcon({ path: 'toolbar-48.png' })",

        @"browser.action.openPopup()"
    ]);

    auto *extraLargeToolbarIcon = Util::makePNGData(CGSizeMake(48, 48), @selector(yellowColor));

    auto *resources = @{
        @"background.js": backgroundScript,
        @"toolbar-48.png": extraLargeToolbarIcon,
    };

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentPopupForAction = ^(_WKWebExtensionAction *action) {
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

        @"browser.action.openPopup()"
    ]);

    auto *largeToolbarIcon = Util::makePNGData(CGSizeMake(48, 48), @selector(yellowColor));
    auto *extraLargeToolbarIcon = Util::makePNGData(CGSizeMake(96, 96), @selector(greenColor));
    auto *superExtraLargeToolbarIcon = Util::makePNGData(CGSizeMake(128, 128), @selector(purpleColor));

    auto *resources = @{
        @"background.js": backgroundScript,
        @"toolbar-48.png": largeToolbarIcon,
        @"toolbar-96.png": extraLargeToolbarIcon,
        @"toolbar-128.png": superExtraLargeToolbarIcon,
    };

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentPopupForAction = ^(_WKWebExtensionAction *action) {
        auto *icon48 = [action iconForSize:CGSizeMake(48, 48)];
        auto *icon96 = [action iconForSize:CGSizeMake(96, 96)];
        auto *icon128 = [action iconForSize:CGSizeMake(128, 128)];

        EXPECT_NOT_NULL(icon48);
#if USE(APPKIT)
        EXPECT_TRUE(CGSizeEqualToSize(icon48.size, CGSizeMake(48, 48)));
#else
        EXPECT_TRUE(CGSizeEqualToSize(icon48.size, CGSizeMake(128, 128)));
#endif

        EXPECT_NOT_NULL(icon96);
#if USE(APPKIT)
        EXPECT_TRUE(CGSizeEqualToSize(icon96.size, CGSizeMake(96, 96)));
#else
        EXPECT_TRUE(CGSizeEqualToSize(icon96.size, CGSizeMake(128, 128)));
#endif

        EXPECT_NOT_NULL(icon128);
        EXPECT_TRUE(CGSizeEqualToSize(icon128.size, CGSizeMake(128, 128)));

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

        @"browser.action.openPopup()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentPopupForAction = ^(_WKWebExtensionAction *action) {
        auto *icon = [action iconForSize:CGSizeMake(48, 48)];

        EXPECT_NOT_NULL(icon);
        EXPECT_TRUE(CGSizeEqualToSize(icon.size, CGSizeMake(48, 48)));

        [manager done];
    };

    [manager loadAndRun];
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

        @"browser.action.openPopup()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentPopupForAction = ^(_WKWebExtensionAction *action) {
        auto *icon48 = [action iconForSize:CGSizeMake(48, 48)];
        auto *icon96 = [action iconForSize:CGSizeMake(96, 96)];
        auto *icon128 = [action iconForSize:CGSizeMake(128, 128)];

        EXPECT_NOT_NULL(icon48);
#if USE(APPKIT)
        EXPECT_TRUE(CGSizeEqualToSize(icon48.size, CGSizeMake(48, 48)));
#else
        EXPECT_TRUE(CGSizeEqualToSize(icon48.size, CGSizeMake(128, 128)));
#endif

        EXPECT_NOT_NULL(icon96);
#if USE(APPKIT)
        EXPECT_TRUE(CGSizeEqualToSize(icon96.size, CGSizeMake(96, 96)));
#else
        EXPECT_TRUE(CGSizeEqualToSize(icon96.size, CGSizeMake(128, 128)));
#endif

        EXPECT_NOT_NULL(icon128);
        EXPECT_TRUE(CGSizeEqualToSize(icon128.size, CGSizeMake(128, 128)));

        [manager done];
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIAction, SetIconWithSVGDataURL)
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

        @"browser.action.openPopup()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentPopupForAction = ^(_WKWebExtensionAction *action) {
        auto *icon = [action iconForSize:CGSizeMake(48, 48)];
        EXPECT_NOT_NULL(icon);
        EXPECT_TRUE(CGSizeEqualToSize(icon.size, CGSizeMake(48, 48)));

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

        @"browser.action.openPopup();"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentPopupForAction = ^(_WKWebExtensionAction *action) {
        auto *icon48 = [action iconForSize:CGSizeMake(48, 48)];
        EXPECT_NOT_NULL(icon48);
#if USE(APPKIT)
        EXPECT_TRUE(CGSizeEqualToSize(icon48.size, CGSizeMake(48, 48)));
#else
        EXPECT_TRUE(CGSizeEqualToSize(icon48.size, CGSizeMake(96, 96)));
#endif

        auto *icon96 = [action iconForSize:CGSizeMake(96, 96)];
        EXPECT_NOT_NULL(icon96);
        EXPECT_TRUE(CGSizeEqualToSize(icon96.size, CGSizeMake(96, 96)));

        [manager done];
    };

    [manager loadAndRun];
}

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

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:browserActionManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentPopupForAction = ^(_WKWebExtensionAction *action) {
        auto *defaultAction = [manager.get().context actionForTab:nil];

        EXPECT_TRUE(defaultAction.presentsPopup);
        EXPECT_FALSE(defaultAction.isEnabled);
        EXPECT_NS_EQUAL(defaultAction.label, @"Modified Title");
        EXPECT_NS_EQUAL(defaultAction.badgeText, @"42");

        EXPECT_NULL(action.associatedTab);

        EXPECT_FALSE(action.isEnabled);
        EXPECT_NS_EQUAL(action.label, @"Modified Title");
        EXPECT_NS_EQUAL(action.badgeText, @"42");

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

        [action closePopupWebView];

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

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:pageActionManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentPopupForAction = ^(_WKWebExtensionAction *action) {
        auto *defaultAction = [manager.get().context actionForTab:nil];

        EXPECT_TRUE(defaultAction.presentsPopup);
        EXPECT_FALSE(defaultAction.isEnabled);
        EXPECT_NS_EQUAL(defaultAction.label, @"Modified Title");
        EXPECT_NS_EQUAL(defaultAction.badgeText, @"42");

        EXPECT_NULL(action.associatedTab);

        EXPECT_FALSE(action.isEnabled);
        EXPECT_NS_EQUAL(action.label, @"Modified Title");
        EXPECT_NS_EQUAL(action.badgeText, @"42");

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

        [action closePopupWebView];

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

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:_WKWebExtensionPermissionWebNavigation];

    auto *localhostRequest = server.requestWithLocalhost();
    auto *addressRequest = server.request();

    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:localhostRequest.URL];
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:addressRequest.URL];

    [manager.get().defaultTab.mainWebView loadRequest:localhostRequest];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.mainWebView loadRequest:addressRequest];

    [manager run];
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
