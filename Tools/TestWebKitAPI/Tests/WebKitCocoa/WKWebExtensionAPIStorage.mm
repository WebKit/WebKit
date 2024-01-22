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

#if ENABLE(WK_WEB_EXTENSIONS)

#import "HTTPServer.h"
#import "WebExtensionUtilities.h"

namespace TestWebKitAPI {

static auto *storageManifest = @{
    @"manifest_version": @3,

    @"name": @"Storage Test",
    @"description": @"Storage Test",
    @"version": @"1",

    @"permissions": @[ @"storage" ],
    @"host_permissions": @[ @"*://localhost/*" ],

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

TEST(WKWebExtensionAPIStorage, Errors)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertThrows(() => browser?.storage?.local?.get({ 'key': () => { 'function' } }), /it is not JSON-serializable/i)",
        @"browser.test.assertThrows(() => browser?.storage?.local?.get(Date.now()), /'items' value is invalid, because an object or a string or an array of strings or null is expected, but a number was provided/i)",

        @"browser.test.assertThrows(() => browser?.storage?.local?.getBytesInUse({}), /'keys' value is invalid, because a string or an array of strings or null is expected, but an object was provided/i)",
        @"browser.test.assertThrows(() => browser?.storage?.local?.getBytesInUse([1]), /'keys' value is invalid, because a string or an array of strings or null is expected, but an array of other values was provided/i)",

        @"browser.test.assertThrows(() => browser?.storage?.local?.set(), /A required argument is missing/i)",
        @"browser.test.assertThrows(() => browser?.storage?.local?.set([]), /'items' value is invalid, because an object is expected/i)",
        @"browser.test.assertThrows(() => browser?.storage?.local?.set({ 'key': () => { 'function' } }), /it is not JSON-serializable/i)",

        @"browser.test.assertThrows(() => browser?.storage?.local?.remove(), /A required argument is missing/i)",
        @"browser.test.assertThrows(() => browser?.storage?.local?.remove({}), /'keys' value is invalid, because a string or an array of strings is expected, but an object was provided/i)",
        @"browser.test.assertThrows(() => browser?.storage?.local?.remove([1]), /'keys' value is invalid, because a string or an array of strings is expected, but an array of other values was provided/i)",

        @"browser.test.assertThrows(() => browser?.storage?.session?.setAccessLevel('INVALID_ACCESS_LEVEL'), /'accessOptions' value is invalid, because an object is expected/i)",
        @"browser.test.assertThrows(() => browser?.storage?.session?.setAccessLevel({ 'accessLevel': 'INVALID_ACCESS_LEVEL' }), /'accessLevel' value is invalid, because it must specify either 'TRUSTED_CONTEXTS' or 'TRUSTED_AND_UNTRUSTED_CONTEXTS'/i)",

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(storageManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIStorage, UndefinedProperties)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(browser?.storage?.local?.setAccessLevel, undefined)",
        @"browser.test.assertEq(browser?.storage?.sync?.setAccessLevel, undefined)",

        @"browser.test.assertEq(browser?.storage?.local?.QUOTA_BYTES_PER_ITEM, undefined)",
        @"browser.test.assertEq(browser?.storage?.session?.QUOTA_BYTES_PER_ITEM, undefined)",

        @"browser.test.assertEq(browser?.storage?.local?.MAX_ITEMS, undefined)",
        @"browser.test.assertEq(browser?.storage?.session?.MAX_ITEMS, undefined)",

        @"browser.test.assertEq(browser?.storage?.local?.MAX_WRITE_OPERATIONS_PER_HOUR, undefined)",
        @"browser.test.assertEq(browser?.storage?.session?.MAX_WRITE_OPERATIONS_PER_HOUR, undefined)",

        @"browser.test.assertEq(browser?.storage?.local?.MAX_WRITE_OPERATIONS_PER_MINUTE, undefined)",
        @"browser.test.assertEq(browser?.storage?.session?.MAX_WRITE_OPERATIONS_PER_MINUTE, undefined)",

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(storageManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIStorage, SetAccessLevelTrustedContexts)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener(async (message, sender, sendResponse) => {",
        @"  browser.test.assertEq(message, 'Ready')",

        @"  const tabs = await browser?.tabs?.query({ active: true, currentWindow: true })",
        @"  const tabId = tabs[0].id",

        @"  await browser?.storage?.session?.setAccessLevel({ accessLevel: 'TRUSTED_CONTEXTS' })",

        @"  const response = await browser.test.assertSafeResolve(() => browser?.tabs?.sendMessage(tabId, { content: 'Access level set' }))",
        @"  browser.test.assertEq(response?.content, undefined)",

        @"  browser.test.notifyPass()",
        @"})"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener((message, sender, sendResponse) => {",
        @"  browser.test.assertEq(message?.content, 'Access level set', 'Should receive the correct message content')",

        @"  sendResponse({ content: browser?.storage?.session })",
        @"})",

        @"setTimeout(() => browser.runtime.sendMessage('Ready'), 1000)"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:storageManifest resources:@{ @"background.js": backgroundScript, @"content.js": contentScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIStorage, SetAccessLevelTrustedAndUntrustedContexts)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener(async (message, sender, sendResponse) => {",
        @"  browser.test.assertEq(message, 'Ready')",

        @"  const tabs = await browser?.tabs?.query({ active: true, currentWindow: true })",
        @"  const tabId = tabs[0].id",

        @"  await browser?.storage?.session?.setAccessLevel({ accessLevel: 'TRUSTED_AND_UNTRUSTED_CONTEXTS' })",

        @"  const response = await browser.test.assertSafeResolve(() => browser?.tabs?.sendMessage(tabId, { content: 'Access level set' }))",
        @"  browser.test.assertEq(response?.content, 'object')",

        @"  browser.test.notifyPass()",
        @"})"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener((message, sender, sendResponse) => {",
        @"  browser.test.assertEq(message?.content, 'Access level set', 'Should receive the correct message content')",

        @"  sendResponse({ content: typeof browser?.storage?.session })",
        @"})",

        @"setTimeout(() => browser.runtime.sendMessage('Ready'), 1000)"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:storageManifest resources:@{ @"background.js": backgroundScript, @"content.js": contentScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager loadAndRun];
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
