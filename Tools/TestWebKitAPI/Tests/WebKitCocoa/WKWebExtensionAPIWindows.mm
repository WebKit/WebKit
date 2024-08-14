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

#import "TestWebExtensionsDelegate.h"
#import "WebExtensionUtilities.h"
#import <WebKit/WKWebExtensionWindowConfiguration.h>

@interface DummyWebExtensionWindow : NSObject <WKWebExtensionWindow>
@end

@implementation DummyWebExtensionWindow
@end

namespace TestWebKitAPI {

static auto *windowsManifest = @{
    @"manifest_version": @3,

    @"name": @"Windows Test",
    @"description": @"Windows Test",
    @"version": @"1",

    @"options_page": @"options.html",

    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"type": @"module",
        @"persistent": @NO,
    },
};

TEST(WKWebExtensionAPIWindows, Errors)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertThrows(() => browser.windows.get('bad'), /'windowId' value is invalid, because a number is expected/i)",
        @"browser.test.assertThrows(() => browser.windows.get(NaN), /'windowId' value is invalid, because a number is expected/i)",
        @"browser.test.assertThrows(() => browser.windows.get(Infinity), /'windowId' value is invalid, because a number is expected/i)",
        @"browser.test.assertThrows(() => browser.windows.get(-Infinity), /'windowId' value is invalid, because a number is expected/i)",
        @"browser.test.assertThrows(() => browser.windows.get(-3), /'windowId' value is invalid, because it is not a window identifier/i)",
        @"browser.test.assertThrows(() => browser.windows.get(1.2), /'windowId' value is invalid, because it is not a window identifier/i)",
        @"browser.test.assertThrows(() => browser.windows.get(browser.windows.WINDOW_ID_NONE), /'windowId' value is invalid, because 'windows.WINDOW_ID_NONE' is not allowed/i)",

        @"await browser.test.assertRejects(browser.windows.get(42), /window not found/i)",

#if PLATFORM(MAC)
        // iOS does not support create() and update().
        @"await browser.test.assertRejects(browser.windows.update(99, { focused: true }), /window not found/i)",

        @"browser.test.assertThrows(() => browser.windows.create({ url: 42 }), /'url' is expected to be a string or an array of strings, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.windows.create({ left: 'bad' }), /'left' is expected to be a number/i)",
        @"browser.test.assertThrows(() => browser.windows.create({ top: 'bad' }), /'top' is expected to be a number/i)",
        @"browser.test.assertThrows(() => browser.windows.create({ width: 'bad' }), /'width' is expected to be a number/i)",
        @"browser.test.assertThrows(() => browser.windows.create({ height: 'bad' }), /'height' is expected to be a number/i)",
        @"browser.test.assertThrows(() => browser.windows.create({ incognito: 'bad' }), /'incognito' is expected to be a boolean/i)",
        @"browser.test.assertThrows(() => browser.windows.create({ focused: 'bad' }), /'focused' is expected to be a boolean/i)",
        @"browser.test.assertThrows(() => browser.windows.create({ tabId: 'bad' }), /'tabId' is expected to be a number/i)",

        @"const window = await browser.test.assertSafeResolve(() => browser.windows.getCurrent())",
        @"browser.test.assertThrows(() => browser.windows.update(window.id, { left: 'bad' }), /'left' is expected to be a number, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.windows.update(window.id, { top: 'bad' }), /'top' is expected to be a number, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.windows.update(window.id, { width: 'bad' }), /'width' is expected to be a number, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.windows.update(window.id, { height: 'bad' }), /'height' is expected to be a number, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.windows.update(window.id, { focused: 'bad' }), /'focused' is expected to be a boolean, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.windows.update(window.id, { state: 'bad' }), /'state' value is invalid, because it must specify 'normal', 'minimized', 'maximized', or 'fullscreen'/i)",
        @"browser.test.assertThrows(() => browser.windows.update(window.id, { top: 100, state: 'fullscreen' }), /'properties' value is invalid, because when 'top', 'left', 'width', or 'height' are specified, 'state' must specify 'normal'/i)",
        @"browser.test.assertThrows(() => browser.windows.update(window.id, { left: 100, state: 'minimized' }), /'properties' value is invalid, because when 'top', 'left', 'width', or 'height' are specified, 'state' must specify 'normal'/i)",
        @"browser.test.assertThrows(() => browser.windows.update(window.id, { width: 100, state: 'maximized' }), /'properties' value is invalid, because when 'top', 'left', 'width', or 'height' are specified, 'state' must specify 'normal'/i)",
        @"browser.test.assertThrows(() => browser.windows.update(window.id, { height: 100, state: 'fullscreen' }), /'properties' value is invalid, because when 'top', 'left', 'width', or 'height' are specified, 'state' must specify 'normal'/i)",
#endif

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(windowsManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIWindows, GetCurrent)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const window = await browser.windows.getCurrent();",

        @"browser.test.assertEq(typeof window, 'object', 'The window should be an object');",
        @"browser.test.assertEq(typeof window.id, 'number', 'The window id should be a number');",
        @"browser.test.assertEq(window.type, 'normal', 'Window type should be normal');",
        @"browser.test.assertEq(window.state, 'normal', 'Window state should be normal');",
        @"browser.test.assertTrue(window.focused, 'Window should be focused');",
        @"browser.test.assertFalse(window.incognito, 'Window should not be in incognito mode');",
        @"browser.test.assertFalse(window.alwaysOnTop, 'Window should not be always on top');",
        @"browser.test.assertEq(window.top, 50, 'Window top position should be 50');",
        @"browser.test.assertEq(window.left, 100, 'Window left position should be 100');",
        @"browser.test.assertEq(window.width, 800, 'Window width should be 800');",
        @"browser.test.assertEq(window.height, 600, 'Window height should be 600');",

        @"browser.test.notifyPass();"
    ]);

    Util::loadAndRunExtension(windowsManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIWindows, GetCurrentFromOptionsPage)
{
    auto *optionsScript = Util::constructScript(@[
        @"const window = await browser.windows.getCurrent()",

        @"browser.test.assertEq(typeof window, 'object', 'The window should be')",
        @"browser.test.assertTrue(window.focused, 'The current window should be focused')",

        @"browser.test.notifyPass()"
    ]);

    auto *resources = @{
        @"background.js": @"// Not Used",
        @"options.html": @"<script type='module' src='options.js'></script>",
        @"options.js": optionsScript
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:windowsManifest resources:resources]);
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

TEST(WKWebExtensionAPIWindows, GetLastFocused)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const window = await browser.windows.getLastFocused();",

        @"browser.test.assertEq(typeof window, 'object', 'The window should be an object');",
        @"browser.test.assertEq(typeof window.id, 'number', 'The window id should be a number');",
        @"browser.test.assertEq(window.type, 'normal', 'Window type should be normal');",
        @"browser.test.assertEq(window.state, 'normal', 'Window state should be normal');",
        @"browser.test.assertFalse(window.focused, 'Window should be focused');",
        @"browser.test.assertFalse(window.incognito, 'Window should not be in incognito mode');",
        @"browser.test.assertFalse(window.alwaysOnTop, 'Window should not be always on top');",
        @"browser.test.assertEq(window.top, 50, 'Window top position should be 50');",
        @"browser.test.assertEq(window.left, 100, 'Window left position should be 100');",
        @"browser.test.assertEq(window.width, 800, 'Window width should be 800');",
        @"browser.test.assertEq(window.height, 600, 'Window height should be 600');",

        @"browser.test.notifyPass();"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:windowsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.focusedWindow = ^id<WKWebExtensionWindow>(WKWebExtensionContext *) {
        return nil;
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIWindows, GetAll)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const windows = await browser.windows.getAll();",
        @"const windowOne = windows[0];",

        @"browser.test.assertEq(windows.length, 1, 'One window should be returned');",

        @"browser.test.assertEq(typeof windowOne, 'object', 'windowOne should be an object');",
        @"browser.test.assertEq(typeof windowOne.id, 'number', 'windowOne id should be a number');",
        @"browser.test.assertEq(windowOne.type, 'normal', 'windowOne type should be normal');",
        @"browser.test.assertEq(windowOne.state, 'normal', 'windowOne state should be normal');",
        @"browser.test.assertTrue(windowOne.focused, 'windowOne should be focused');",
        @"browser.test.assertFalse(windowOne.incognito, 'windowOne should not be in incognito mode');",
        @"browser.test.assertFalse(windowOne.alwaysOnTop, 'windowOne should not be always on top');",
        @"browser.test.assertEq(windowOne.top, 50, 'windowOne top position should be 50');",
        @"browser.test.assertEq(windowOne.left, 100, 'windowOne left position should be 100');",
        @"browser.test.assertEq(windowOne.width, 800, 'windowOne width should be 800');",
        @"browser.test.assertEq(windowOne.height, 600, 'windowOne height should be 600');",

        @"browser.test.notifyPass();"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:windowsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    EXPECT_FALSE(manager.get().context.hasAccessToPrivateData);

    [manager openNewWindowUsingPrivateBrowsing:YES];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIWindows, GetAllWithPrivateAccess)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const windows = await browser.windows.getAll();",
        @"const windowOne = windows[0];",
        @"const windowTwo = windows[1];",

        @"browser.test.assertEq(windows.length, 2, 'Two windows should be returned');",

        // Validate first window's properties
        @"browser.test.assertEq(typeof windowOne, 'object', 'windowOne should be an object');",
        @"browser.test.assertEq(typeof windowOne.id, 'number', 'windowOne id should be a number');",
        @"browser.test.assertEq(windowOne.type, 'normal', 'windowOne type should be normal');",
        @"browser.test.assertEq(windowOne.state, 'normal', 'windowOne state should be normal');",
        @"browser.test.assertTrue(windowOne.focused, 'windowOne should be focused');",
        @"browser.test.assertFalse(windowOne.incognito, 'windowOne should not be in incognito mode');",
        @"browser.test.assertFalse(windowOne.alwaysOnTop, 'windowOne should not be always on top');",
        @"browser.test.assertEq(windowOne.top, 50, 'windowOne top position should be 50');",
        @"browser.test.assertEq(windowOne.left, 100, 'windowOne left position should be 100');",
        @"browser.test.assertEq(windowOne.width, 800, 'windowOne width should be 800');",
        @"browser.test.assertEq(windowOne.height, 600, 'windowOne height should be 600');",

        // Validate second window's properties
        @"browser.test.assertEq(typeof windowTwo, 'object', 'windowTwo should be an object');",
        @"browser.test.assertEq(typeof windowTwo.id, 'number', 'windowTwo id should be a number');",
        @"browser.test.assertEq(windowTwo.type, 'popup', 'windowTwo type should be popup');",
        @"browser.test.assertEq(windowTwo.state, 'minimized', 'windowTwo state should be minimized');",
        @"browser.test.assertFalse(windowTwo.focused, 'windowTwo should not be focused');",
        @"browser.test.assertTrue(windowTwo.incognito, 'windowTwo should be in incognito mode');",
        @"browser.test.assertEq(windowTwo.top, 75, 'windowTwo top position should be 75');",
        @"browser.test.assertEq(windowTwo.left, 110, 'windowTwo left position should be 110');",
        @"browser.test.assertEq(windowTwo.width, 300, 'windowTwo width should be 300');",
        @"browser.test.assertEq(windowTwo.height, 700, 'windowTwo height should be 700');",

        // Validate that windowOne.id and windowTwo.id are different
        @"browser.test.assertTrue(windowOne.id !== windowTwo.id, 'windowOne.id and windowTwo.id should be different');",

        @"browser.test.notifyPass();"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:windowsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().context.hasAccessToPrivateData = YES;

    auto *windowTwo = [manager openNewWindowUsingPrivateBrowsing:YES];

#if PLATFORM(MAC)
    // This is 75pt from top on a screen of 1920 x 1080 in Mac screen coordinates.
    windowTwo.frame = CGRectMake(110, 305, 300, 700);
#else
    windowTwo.frame = CGRectMake(110, 75, 300, 700);
#endif
    windowTwo.windowState = WKWebExtensionWindowStateMinimized;
    windowTwo.windowType = WKWebExtensionWindowTypePopup;

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIWindows, CreatedEvent)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.windows.onCreated.addListener((window) => {",

        @"  browser.test.assertEq(typeof window, 'object', 'The window should be an object');",
        @"  browser.test.assertEq(typeof window.id, 'number', 'The window id should be a number');",
        @"  browser.test.assertEq(window.type, 'normal', 'Window type should be normal');",
        @"  browser.test.assertEq(window.state, 'normal', 'Window state should be normal');",
        @"  browser.test.assertTrue(window.focused, 'Window should be focused');",
        @"  browser.test.assertFalse(window.incognito, 'Window should not be in incognito mode');",
        @"  browser.test.assertFalse(window.alwaysOnTop, 'Window should not be always on top');",
        @"  browser.test.assertEq(window.top, 50, 'Window top position should be 50');",
        @"  browser.test.assertEq(window.left, 100, 'Window left position should be 100');",
        @"  browser.test.assertEq(window.width, 800, 'Window width should be 800');",
        @"  browser.test.assertEq(window.height, 600, 'Window height should be 600');",

        @"  browser.test.notifyPass();",

        @"});",

        @"browser.test.yield('Open Window');"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:windowsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Open Window");

    [manager openNewWindow];
    [manager run];
}

TEST(WKWebExtensionAPIWindows, FocusChangedEvent)
{
    auto *backgroundScript = Util::constructScript(@[
        @"let focusedTwice = false;",

        @"browser.windows.onFocusChanged.addListener((windowId) => {",
        @"  if (windowId !== browser.windows.WINDOW_ID_NONE) {",
        @"    if (!focusedTwice) {",
        @"      browser.test.assertEq(typeof windowId, 'number', 'The window id should be a number when a window is focused');",
        @"      browser.test.yield('Focus None');",
        @"      focusedTwice = true;",
        @"    } else {",
        @"      browser.test.assertEq(typeof windowId, 'number', 'The window id should be a number when a window is focused again');",
        @"      browser.test.notifyPass();",
        @"    }",
        @"  } else {",
        @"    browser.test.assertEq(windowId, browser.windows.WINDOW_ID_NONE, 'The window id should be WINDOW_ID_NONE when nil is focused');",
        @"    browser.test.yield('Focus Window Again');",
        @"  }",
        @"});",

        @"browser.test.yield('Focus Window');"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:windowsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *windowOne = manager.get().defaultWindow;

    [manager load];

    auto *windowTwo = [manager openNewWindow];

    [manager run];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Focus Window");

    [manager focusWindow:windowOne];
    [manager run];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Focus None");

    [manager focusWindow:nil];
    [manager run];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Focus Window Again");

    [manager focusWindow:windowTwo];
    [manager run];
}

TEST(WKWebExtensionAPIWindows, RemovedEvent)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.windows.onRemoved.addListener((windowId) => {",
        @"  browser.test.assertEq(typeof windowId, 'number', 'The window id should be a number');",

        @"  browser.test.notifyPass();",
        @"});",

        @"browser.test.yield('Close Window');"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:windowsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager load];

    [manager openNewWindow];
    [manager run];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Close Window");

    [manager closeWindow:manager.get().defaultWindow];
    [manager run];
}

#if PLATFORM(MAC)

// iOS does not support create() and update().

TEST(WKWebExtensionAPIWindows, Create)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const windowOptions = {",
        @"  focused: true,",
        @"  left: 300,",
        @"  height: 400,",
        @"  incognito: false,",
        @"  state: 'normal',",
        @"  type: 'popup',",
        @"  url: 'http://example.com/',",
        @"};",

        @"const window = await browser.windows.create(windowOptions);",
        @"browser.test.assertEq(typeof window, 'object', 'The window should be an object');",
        @"browser.test.assertEq(window.top, 50, 'The window should have the specified top');",
        @"browser.test.assertEq(window.left, 300, 'The window should have the specified left');",
        @"browser.test.assertEq(window.width, 800, 'The window should have the specified width');",
        @"browser.test.assertEq(window.height, 400, 'The window should have the specified height');",
        @"browser.test.assertFalse(window.incognito, 'The window should not be in incognito mode');",
        @"browser.test.assertEq(window.type, 'popup', 'The window should be of type popup');",
        @"browser.test.assertEq(window.state, 'normal', 'The window state should be normal');",
        @"browser.test.assertTrue(window.focused, 'The window should be focused');",

        @"browser.test.notifyPass();"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:windowsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto originalOpenNewWindow = manager.get().internalDelegate.openNewWindow;

    manager.get().internalDelegate.openNewWindow = ^(WKWebExtensionWindowConfiguration *configuration, WKWebExtensionContext *context, void (^completionHandler)(id<WKWebExtensionWindow>, NSError *)) {
        EXPECT_EQ(configuration.frame.origin.x, 300);
        EXPECT_EQ(configuration.frame.size.height, 400);
        EXPECT_TRUE(std::isnan(configuration.frame.origin.y));
        EXPECT_TRUE(std::isnan(configuration.frame.size.width));

        EXPECT_TRUE(configuration.shouldBeFocused);
        EXPECT_FALSE(configuration.shouldBePrivate);

        EXPECT_EQ(configuration.windowType, WKWebExtensionWindowTypePopup);
        EXPECT_EQ(configuration.windowState, WKWebExtensionWindowStateNormal);

        EXPECT_EQ(configuration.tabs.count, 0lu);

        EXPECT_EQ(configuration.tabURLs.count, 1lu);
        EXPECT_NS_EQUAL(configuration.tabURLs.firstObject, [NSURL URLWithString:@"http://example.com/"]);

        originalOpenNewWindow(configuration, context, completionHandler);
    };

    EXPECT_FALSE(manager.get().context.hasAccessToPrivateData);
    EXPECT_EQ(manager.get().windows.count, 1lu);

    [manager loadAndRun];

    EXPECT_EQ(manager.get().windows.count, 2lu);
}

TEST(WKWebExtensionAPIWindows, CreateWithRelativeURL)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const windowOptions = {",
        @"  url: 'test.html'",
        @"}",

        @"const window = await browser.windows.create(windowOptions)",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:windowsManifest resources:@{ @"background.js": backgroundScript, @"test.html": @"Hello world!" }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto originalOpenNewWindow = manager.get().internalDelegate.openNewWindow;

    manager.get().internalDelegate.openNewWindow = ^(WKWebExtensionWindowConfiguration *configuration, WKWebExtensionContext *context, void (^completionHandler)(id<WKWebExtensionWindow>, NSError *)) {
        EXPECT_EQ(configuration.tabURLs.count, 1lu);
        EXPECT_NS_EQUAL(configuration.tabURLs.firstObject, [NSURL URLWithString:@"test.html" relativeToURL:manager.get().context.baseURL].absoluteURL);

        originalOpenNewWindow(configuration, context, completionHandler);
    };

    EXPECT_EQ(manager.get().windows.count, 1lu);

    [manager loadAndRun];

    EXPECT_EQ(manager.get().windows.count, 2lu);
}

TEST(WKWebExtensionAPIWindows, CreateWithRelativeURLs)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const windowOptions = {",
        @"  url: [ 'one.html', 'two.html' ]",
        @"}",

        @"const window = await browser.windows.create(windowOptions)",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:windowsManifest resources:@{ @"background.js": backgroundScript, @"one.html": @"Hello one!", @"two.html": @"Hello two!" }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto originalOpenNewWindow = manager.get().internalDelegate.openNewWindow;

    manager.get().internalDelegate.openNewWindow = ^(WKWebExtensionWindowConfiguration *configuration, WKWebExtensionContext *context, void (^completionHandler)(id<WKWebExtensionWindow>, NSError *)) {
        EXPECT_EQ(configuration.tabURLs.count, 2lu);
        EXPECT_NS_EQUAL(configuration.tabURLs.firstObject, [NSURL URLWithString:@"one.html" relativeToURL:manager.get().context.baseURL].absoluteURL);
        EXPECT_NS_EQUAL(configuration.tabURLs.lastObject, [NSURL URLWithString:@"two.html" relativeToURL:manager.get().context.baseURL].absoluteURL);

        originalOpenNewWindow(configuration, context, completionHandler);
    };

    EXPECT_EQ(manager.get().windows.count, 1lu);

    [manager loadAndRun];

    EXPECT_EQ(manager.get().windows.count, 2lu);
}

TEST(WKWebExtensionAPIWindows, CreateIncognitoWithoutPrivateAccess)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const windowOptions = {",
        @"  focused: true,",
        @"  left: 300,",
        @"  height: 400,",
        @"  incognito: true,",
        @"  state: 'normal',",
        @"  type: 'popup',",
        @"};",

        @"const window = await browser.windows.create(windowOptions);",
        @"browser.test.assertEq(window, null, 'The window should be created but null without access');",

        @"browser.test.notifyPass();"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:windowsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    EXPECT_FALSE(manager.get().context.hasAccessToPrivateData);
    EXPECT_EQ(manager.get().windows.count, 1lu);

    [manager loadAndRun];

    EXPECT_EQ(manager.get().windows.count, 2lu);
}

TEST(WKWebExtensionAPIWindows, CreateIncognitoWithPrivateAccess)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const windowOptions = {",
        @"  focused: true,",
        @"  left: 300,",
        @"  height: 400,",
        @"  incognito: true,",
        @"  state: 'normal',",
        @"  type: 'popup',",
        @"};",

        @"const window = await browser.windows.create(windowOptions);",
        @"browser.test.assertEq(typeof window, 'object', 'The window should be an object');",
        @"browser.test.assertEq(window.top, 50, 'The window should have the specified top');",
        @"browser.test.assertEq(window.left, 300, 'The window should have the specified left');",
        @"browser.test.assertEq(window.width, 800, 'The window should have the specified width');",
        @"browser.test.assertEq(window.height, 400, 'The window should have the specified height');",
        @"browser.test.assertTrue(window.incognito, 'The window should be in incognito mode');",
        @"browser.test.assertEq(window.type, 'popup', 'The window should be of type popup');",
        @"browser.test.assertEq(window.state, 'normal', 'The window state should be normal');",
        @"browser.test.assertTrue(window.focused, 'The window should be focused');",

        @"browser.test.notifyPass();"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:windowsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().context.hasAccessToPrivateData = YES;

    EXPECT_EQ(manager.get().windows.count, 1lu);

    [manager loadAndRun];

    EXPECT_EQ(manager.get().windows.count, 2lu);
}

TEST(WKWebExtensionAPIWindows, Update)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const newWindow = await browser.windows.create();",

        @"browser.test.assertEq(newWindow.top, 50, 'The new window top position should be 50 initially');",
        @"browser.test.assertEq(newWindow.left, 100, 'The new window left position should be 100 initially');",
        @"browser.test.assertEq(newWindow.width, 800, 'The new window width should be 800 initially');",
        @"browser.test.assertEq(newWindow.height, 600, 'The new window height should be 600 initially');",
        @"browser.test.assertTrue(newWindow.focused, 'The new window should be focused initially');",
        @"browser.test.assertEq(newWindow.state, 'normal', 'The window state should be normal initially');",

        @"let updatedWindow = await browser.windows.update(newWindow.id, { top: 10, width: 500, focused: true });",

        @"browser.test.assertEq(updatedWindow.top, 10, 'The window top position should be updated to 10');",
        @"browser.test.assertEq(updatedWindow.left, 100, 'The window left position should remain 100 after the first update');",
        @"browser.test.assertEq(updatedWindow.width, 500, 'The window width should be updated to 500');",
        @"browser.test.assertEq(updatedWindow.height, 600, 'The window height should remain 600 after the first update');",
        @"browser.test.assertTrue(updatedWindow.focused, 'The window should be focused');",
        @"browser.test.assertEq(updatedWindow.state, 'normal', 'The window state should remain normal after the first update');",

        @"updatedWindow = await browser.windows.update(newWindow.id, { state: 'fullscreen' });",

        @"browser.test.assertEq(updatedWindow.state, 'fullscreen', 'The window state should be updated to fullscreen');",
        @"browser.test.assertEq(updatedWindow.top, 0, 'The window top position should be 0 in fullscreen');",
        @"browser.test.assertEq(updatedWindow.left, 0, 'The window left position should be 0 in fullscreen');",
        @"browser.test.assertEq(updatedWindow.width, 1920, 'The window width should be 1920 in fullscreen');",
        @"browser.test.assertEq(updatedWindow.height, 1080, 'The window height should be 1080 in fullscreen');",

        @"updatedWindow = await browser.windows.update(newWindow.id, { state: 'normal' });",

        @"browser.test.assertEq(updatedWindow.top, 10, 'The window top position should be reverted back to 10');",
        @"browser.test.assertEq(updatedWindow.left, 100, 'The window left position should remain 100');",
        @"browser.test.assertEq(updatedWindow.width, 500, 'The window width should be reverted back to 500');",
        @"browser.test.assertEq(updatedWindow.height, 600, 'The window height should be reverted back to 600');",
        @"browser.test.assertEq(updatedWindow.state, 'normal', 'The window state should be reverted back to normal');",

        @"browser.test.notifyPass();"
    ]);

    Util::loadAndRunExtension(windowsManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIWindows, Remove)
{
    auto *backgroundScript = Util::constructScript(@[
        // Create a new window and verify the count
        @"const initialWindows = await browser.windows.getAll();",
        @"const newWindow = await browser.windows.create();",

        @"const windowsAfterCreate = await browser.windows.getAll();",
        @"browser.test.assertEq(windowsAfterCreate.length, initialWindows.length + 1, 'One window should be added after creation');",

        // Remove the window and verify the count
        @"await browser.windows.remove(newWindow.id);",

        @"const windowsAfterRemove = await browser.windows.getAll();",
        @"browser.test.assertEq(windowsAfterRemove.length, initialWindows.length, 'Window count should match the initial count after removal');",

        @"browser.test.notifyPass();"
    ]);

    Util::loadAndRunExtension(windowsManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIWindows, RemoveUnsupported)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(browser.windows.remove, undefined)",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:windowsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().context.unsupportedAPIs = [NSSet setWithObject:@"browser.windows.remove"];

    [manager loadAndRun];
}

#endif // PLATFORM(MAC)

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
