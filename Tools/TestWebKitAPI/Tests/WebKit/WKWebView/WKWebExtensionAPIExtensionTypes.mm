/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/WebExtensionUtilities.h"

namespace TestWebKitAPI {

static auto *extensionTypesManifest = @{
    @"manifest_version": @3,

    @"name": @"ExtensionTypes Test",
    @"description": @"ExtensionTypes Test",
    @"version": @"1",

    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"type": @"module",
        @"persistent": @NO,
    },
};

TEST(WKWebExtensionAPIExtensionTypes, ImageFormat)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(typeof browser.extensionTypes, 'object', 'extensionTypes should be an object')",
        @"browser.test.assertEq(typeof browser.extensionTypes.ImageFormat, 'object', 'ImageFormat should be an object')",

        @"browser.test.assertEq(browser.extensionTypes.ImageFormat.JPEG, 'jpeg', 'JPEG should be defined')",
        @"browser.test.assertEq(browser.extensionTypes.ImageFormat.PNG, 'png', 'PNG should be defined')",

        @"browser.test.assertDeepEq(Object.keys(browser.extensionTypes.ImageFormat).sort(), [ 'JPEG', 'PNG' ], 'ImageFormat should only define the supported values')",

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(extensionTypesManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIExtensionTypes, RunAt)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(typeof browser.extensionTypes.RunAt, 'object', 'RunAt should be an object')",

        @"browser.test.assertEq(browser.extensionTypes.RunAt.DOCUMENT_START, 'document_start', 'DOCUMENT_START should be defined')",
        @"browser.test.assertEq(browser.extensionTypes.RunAt.DOCUMENT_END, 'document_end', 'DOCUMENT_END should be defined')",
        @"browser.test.assertEq(browser.extensionTypes.RunAt.DOCUMENT_IDLE, 'document_idle', 'DOCUMENT_IDLE should be defined')",

        @"browser.test.assertDeepEq(Object.keys(browser.extensionTypes.RunAt).sort(), [ 'DOCUMENT_END', 'DOCUMENT_IDLE', 'DOCUMENT_START' ], 'RunAt should only define the supported values')",

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(extensionTypesManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIExtensionTypes, CSSOrigin)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(typeof browser.extensionTypes.CSSOrigin, 'object', 'CSSOrigin should be an object')",

        @"browser.test.assertEq(browser.extensionTypes.CSSOrigin.AUTHOR, 'author', 'AUTHOR should be defined')",
        @"browser.test.assertEq(browser.extensionTypes.CSSOrigin.USER, 'user', 'USER should be defined')",

        @"browser.test.assertDeepEq(Object.keys(browser.extensionTypes.CSSOrigin).sort(), [ 'AUTHOR', 'USER' ], 'CSSOrigin should only define the supported values')",

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(extensionTypesManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIExtensionTypes, ExecutionWorld)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(typeof browser.extensionTypes.ExecutionWorld, 'object', 'ExecutionWorld should be an object')",

        @"browser.test.assertEq(browser.extensionTypes.ExecutionWorld.ISOLATED, 'ISOLATED', 'ISOLATED should be defined')",
        @"browser.test.assertEq(browser.extensionTypes.ExecutionWorld.MAIN, 'MAIN', 'MAIN should be defined')",

        @"browser.test.assertDeepEq(Object.keys(browser.extensionTypes.ExecutionWorld).sort(), [ 'ISOLATED', 'MAIN' ], 'ExecutionWorld should only define the supported values')",

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(extensionTypesManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIExtensionTypes, AvailableInContentScript)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"ExtensionTypes Content Script Test",
        @"description": @"ExtensionTypes Content Script Test",
        @"version": @"1",

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"type": @"module",
            @"persistent": @NO,
        },

        @"content_scripts": @[ @{
            @"js": @[ @"content.js" ],
            @"matches": @[ @"*://localhost/*" ],
        } ],
    };

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.sendMessage('Load Tab')"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"browser.test.assertEq(typeof browser.extensionTypes, 'object', 'extensionTypes should be available in content scripts')",
        @"browser.test.assertEq(browser.extensionTypes.ImageFormat.PNG, 'png', 'ImageFormat.PNG should be defined')",
        @"browser.test.assertEq(browser.extensionTypes.RunAt.DOCUMENT_IDLE, 'document_idle', 'RunAt.DOCUMENT_IDLE should be defined')",

        @"browser.test.notifyPass()"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript, @"content.js": contentScript });

    auto *urlRequest = server.requestWithLocalhost();

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager runUntilTestMessage:@"Load Tab"];

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
