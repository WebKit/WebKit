/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(WK_WEB_EXTENSIONS) && ENABLE(INSPECTOR_EXTENSIONS)

#import "WebExtensionUtilities.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKInspectorPrivate.h>

namespace TestWebKitAPI {

static auto *devToolsManifest = @{
    @"manifest_version": @3,

    @"name": @"Test DevTools",
    @"description": @"Test DevTools",
    @"version": @"1.0",

    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"type": @"module",
        @"persistent": @NO,
    },

    @"devtools_page": @"devtools.html"
};

TEST(WKWebExtensionAPIDevTools, Basics)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(browser?.devtools, undefined, 'browser.devtools should be undefined in background script')",
    ]);

    auto *devToolsScript = Util::constructScript(@[
        @"browser.test.assertEq(typeof browser?.devtools, 'object', 'browser.devtools should be available in devtools page script')",
        @"browser.test.assertEq(typeof browser?.devtools?.panels, 'object', 'browser.devtools.panels should be available')",
        @"browser.test.assertEq(typeof browser?.devtools?.network, 'object', 'browser.devtools.network should be available')",
        @"browser.test.assertEq(typeof browser?.devtools?.inspectedWindow, 'object', 'browser.devtools.inspectedWindow should be available')",
        @"browser.test.assertEq(typeof browser?.devtools?.inspectedWindow?.tabId, 'number', 'browser.devtools.inspectedWindow.tabId should be available')",
        @"browser.test.assertTrue(browser?.devtools?.inspectedWindow?.tabId > 0, 'browser.devtools.inspectedWindow.tabId should be a positive value')",

        @"browser.test.notifyPass()",
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"devtools.html": @"<script type='module' src='devtools.js'></script>",
        @"devtools.js": devToolsScript
    };

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:devToolsManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().defaultTab.mainWebView loadRequest:server.requestWithLocalhost()];
    [manager.get().defaultTab.mainWebView._inspector show];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIDevTools, CreatePanel)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *devToolsScript = Util::constructScript(@[
        @"let cachedPanelWindow = null",

        @"let panel = await browser.test.assertSafeResolve(() => browser.devtools.panels.create('Test Panel', 'icon.svg', 'panel.html'))",
        @"browser.test.assertEq(typeof panel, 'object', 'panel should be an object')",

        @"panel?.onShown.addListener((panelWindow) => {",
        @"  browser.test.assertEq(typeof panelWindow, 'object', 'panelWindow should be an object')",

        @"  cachedPanelWindow = panelWindow",

        @"  browser.test.yield('Panel Shown')",
        @"})",

        @"panel?.onHidden.addListener(() => {",
        @"  browser.test.assertTrue(cachedPanelWindow !== null, 'panel should be shown first')",
        @"  browser.test.assertEq(typeof cachedPanelWindow?.notifyHidden, 'function', 'cachedPanelWindow should have the notifyHidden function')",

        @"  cachedPanelWindow?.notifyHidden()",
        @"})",

        @"browser.test.yield('Panel Created')",
    ]);

    auto *panelScript = Util::constructScript(@[
        @"window.notifyHidden = () => {",
        @"  browser.test.yield('Panel Hidden')",
        @"}"
    ]);

    auto *iconSVG = @"<svg width='16' height='16' xmlns='http://www.w3.org/2000/svg'><circle cx='8' cy='8' r='8' fill='red' /></svg>";

    auto *resources = @{
        @"background.js": @"// This script is intentionally left blank.",
        @"devtools.html": @"<script type='module' src='devtools.js'></script>",
        @"devtools.js": devToolsScript,
        @"panel.html": @"<script type='module' src='panel.js'></script>",
        @"panel.js": panelScript,
        @"icon.svg": iconSVG,
    };

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:devToolsManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().defaultTab.mainWebView loadRequest:server.requestWithLocalhost()];
    [manager.get().defaultTab.mainWebView._inspector show];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Panel Created");

    NSString *extensionIdentifier = [NSString stringWithFormat:@"WebExtensionTab-%@-1", manager.get().context.uniqueIdentifier];
    [manager.get().defaultTab.mainWebView._inspector showExtensionTabWithIdentifier:extensionIdentifier completionHandler:^(NSError *error) {
        EXPECT_NULL(error);
    }];

    [manager run];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Panel Shown");

    [manager.get().defaultTab.mainWebView._inspector showResources];

    [manager run];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Panel Hidden");
}

TEST(WKWebExtensionAPIDevTools, InspectedWindowEval)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<title>Test Page</title><script>const secretNumber = 42; window.customTitle = 'Dynamic Title'</script>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *devToolsScript = Util::constructScript(@[
        // Evaluate a simple expression that should succeed
        @"await browser.test.assertSafeResolve(async () => {",
        @"  const [result, exceptionInfo] = await browser.devtools.inspectedWindow.eval('document.title')",
        @"  browser.test.assertEq(result, 'Test Page', 'Evaluated document.title should match the title element')",
        @"})",

        // Evaluate an expression that should result in an exception
        @"await browser.test.assertSafeResolve(async () => {",
        @"  const [result, exceptionInfo] = await browser.devtools.inspectedWindow.eval('nonExistentFunction();')",
        @"  browser.test.assertTrue(!!exceptionInfo, 'Eval should catch an exception for non-existent function')",
        @"})",

        // Evaluate a script that accesses a variable defined in the page
        @"await browser.test.assertSafeResolve(async () => {",
        @"  const [result, exceptionInfo] = await browser.devtools.inspectedWindow.eval('secretNumber')",
        @"  browser.test.assertEq(result, 42, 'Evaluated secretNumber should match the value defined in the page script')",
        @"})",

        // Evaluate a script to change a property and verify
        @"await browser.test.assertSafeResolve(async () => {",
        @"  const [result, exceptionInfo] = await browser.devtools.inspectedWindow.eval('window.customTitle = \"Updated Title\"; window.customTitle')",
        @"  browser.test.assertEq(result, 'Updated Title', 'The evaluated script should update the window.customTitle')",
        @"})",

        @"browser.test.notifyPass()",
    ]);

    auto *resources = @{
        @"background.js": @"// This script is intentionally left blank.",
        @"devtools.html": @"<script type='module' src='devtools.js'></script>",
        @"devtools.js": devToolsScript
    };

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:devToolsManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().defaultTab.mainWebView loadRequest:server.requestWithLocalhost()];
    [manager.get().defaultTab.mainWebView._inspector show];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIDevTools, InspectedWindowReload)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s }, { "Cache-Control"_s, "max-age=3600"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *devToolsScript = Util::constructScript(@[
        @"browser.test.assertSafe(() => browser.devtools.inspectedWindow.reload())",

        @"browser.test.yield('Reload Called')",
    ]);

    auto *resources = @{
        @"background.js": @"// This script is intentionally left blank.",
        @"devtools.html": @"<script type='module' src='devtools.js'></script>",
        @"devtools.js": devToolsScript
    };

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:devToolsManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().defaultTab.mainWebView loadRequest:server.requestWithLocalhost()];
    [manager.get().defaultTab.mainWebView._inspector show];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Reload Called");

    [manager runForTimeInterval:3];

    // Reload without { ignoreCache: true } should hit the cache.
    // That isn't guaranteed, so allow for both results here.
    EXPECT_TRUE(server.totalRequests() == 1lu || server.totalRequests() == 2lu);
}

TEST(WKWebExtensionAPIDevTools, InspectedWindowReloadIgnoringCache)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s }, { "Cache-Control"_s, "max-age=3600"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *devToolsScript = Util::constructScript(@[
        @"browser.test.assertSafe(() => browser.devtools.inspectedWindow.reload({ ignoreCache: true }))",

        @"browser.test.yield('Reload Called')",
    ]);

    auto *resources = @{
        @"background.js": @"// This script is intentionally left blank.",
        @"devtools.html": @"<script type='module' src='devtools.js'></script>",
        @"devtools.js": devToolsScript
    };

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:devToolsManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().defaultTab.mainWebView loadRequest:server.requestWithLocalhost()];
    [manager.get().defaultTab.mainWebView._inspector show];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Reload Called");

    [manager runForTimeInterval:3];

    // Two requests to the server are expected (original and reload).
    EXPECT_EQ(server.totalRequests(), 2ul);
}

TEST(WKWebExtensionAPIDevTools, NetworkNavigatedEvent)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *devToolsScript = Util::constructScript(@[
        @"browser.devtools.network.onNavigated.addListener((url) => {",
        @"  browser.test.assertEq(typeof url, 'string', 'url should be a string')",
        @"  browser.test.assertTrue(url?.startsWith('http://127.0.0.1:'), 'url should be local IP address')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Load Next Page')",
    ]);

    auto *resources = @{
        @"background.js": @"// This script is intentionally left blank.",
        @"devtools.html": @"<script type='module' src='devtools.js'></script>",
        @"devtools.js": devToolsScript
    };

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:devToolsManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().defaultTab.mainWebView loadRequest:server.requestWithLocalhost()];
    [manager.get().defaultTab.mainWebView._inspector show];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Next Page");

    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:server.request().URL];
    [manager.get().defaultTab.mainWebView loadRequest:server.request()];

    [manager run];
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS) && ENABLE(INSPECTOR_EXTENSIONS)
