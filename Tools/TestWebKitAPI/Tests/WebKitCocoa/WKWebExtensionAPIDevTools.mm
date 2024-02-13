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

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS) && ENABLE(INSPECTOR_EXTENSIONS)
