/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

static auto *runtimeManifest = @{
    @"manifest_version": @3,

    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"type": @"module",
        @"persistent": @NO,
    },

    @"unused" : [NSNull null],
};

static auto *runtimeContentScriptManifest = @{
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
    } ],
};

TEST(WKWebExtensionAPIRuntime, GetURL)
{
    auto *baseURLString = @"test-extension://76C788B8-3374-400D-8259-40E5B9DF79D3";

    auto *backgroundScript = Util::constructScript(@[
        // Variable Setup
        [NSString stringWithFormat:@"const baseURL = '%@'", baseURLString],

        // Error Cases
        @"browser.test.assertThrows(() => browser.runtime.getURL(), /required argument is missing/i)",
        @"browser.test.assertThrows(() => browser.runtime.getURL(null), /'resourcePath' value is invalid, because a string is expected/i)",
        @"browser.test.assertThrows(() => browser.runtime.getURL(undefined), /'resourcePath' value is invalid, because a string is expected/i)",
        @"browser.test.assertThrows(() => browser.runtime.getURL(42), /'resourcePath' value is invalid, because a string is expected/i)",
        @"browser.test.assertThrows(() => browser.runtime.getURL(/test/), /'resourcePath' value is invalid, because a string is expected/i)",

        // Normal Cases
        @"browser.test.assertEq(browser.runtime.getURL(''), `${baseURL}/`)",
        @"browser.test.assertEq(browser.runtime.getURL('test.js'), `${baseURL}/test.js`)",
        @"browser.test.assertEq(browser.runtime.getURL('/test.js'), `${baseURL}/test.js`)",
        @"browser.test.assertEq(browser.runtime.getURL('../../test.js'), `${baseURL}/test.js`)",
        @"browser.test.assertEq(browser.runtime.getURL('./test.js'), `${baseURL}/test.js`)",
        @"browser.test.assertEq(browser.runtime.getURL('././/example'), `${baseURL}//example`)",
        @"browser.test.assertEq(browser.runtime.getURL('../../example/..//test/'), `${baseURL}//test/`)",
        @"browser.test.assertEq(browser.runtime.getURL('.'), `${baseURL}/`)",
        @"browser.test.assertEq(browser.runtime.getURL('..//../'), `${baseURL}/`)",
        @"browser.test.assertEq(browser.runtime.getURL('.././..'), `${baseURL}/`)",
        @"browser.test.assertEq(browser.runtime.getURL('/.././.'), `${baseURL}/`)",

        // Unexpected Cases
        // FIXME: <https://webkit.org/b/248154> browser.runtime.getURL() has some edge cases that should be failures or return different results
        @"browser.test.assertEq(browser.runtime.getURL('//'), 'test-extension://')",
        @"browser.test.assertEq(browser.runtime.getURL('//example'), `test-extension://example`)",
        @"browser.test.assertEq(browser.runtime.getURL('///'), 'test-extension:///')",

        // Finish
        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Set a base URL so it is a known value and not the default random one.
    manager.get().context.baseURL = [NSURL URLWithString:baseURLString];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIRuntime, Id)
{
    auto *uniqueIdentifier = @"org.webkit.test.extension (76C788B8)";

    auto *backgroundScript = Util::constructScript(@[
        [NSString stringWithFormat:@"browser.test.assertEq(browser.runtime.id, '%@')", uniqueIdentifier],

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Set an uniqueIdentifier so it is a known value and not the default random one.
    manager.get().context.uniqueIdentifier = uniqueIdentifier;

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIRuntime, GetManifest)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertDeepEq(browser.runtime.getManifest(), { manifest_version: 3, background: { persistent: false, scripts: [ 'background.js' ], type: 'module' }, unused: null })",

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(runtimeManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIRuntime, GetPlatformInfo)
{
#if PLATFORM(MAC) && (CPU(ARM) || CPU(ARM64))
    auto *expectedInfo = @"{ os: 'mac', arch: 'arm' }";
#elif PLATFORM(MAC) && (CPU(X86_64))
    auto *expectedInfo = @"{ os: 'mac', arch: 'x86-64' }";
#elif PLATFORM(IOS_FAMILY) && (CPU(ARM) || CPU(ARM64))
    auto *expectedInfo = @"{ os: 'ios', arch: 'arm' }";
#elif PLATFORM(IOS_FAMILY) && (CPU(X86_64))
    auto *expectedInfo = @"{ os: 'ios', arch: 'x86-64' }";
#else
    auto *expectedInfo = @"{ os: 'unknown', arch: 'unknown' }";
#endif

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertTrue(browser.runtime.getPlatformInfo() instanceof Promise)",
        [NSString stringWithFormat:@"browser.test.assertDeepEq(await browser.runtime.getPlatformInfo(), %@)", expectedInfo],
        [NSString stringWithFormat:@"browser.test.assertEq(browser.runtime.getPlatformInfo((info) => browser.test.assertDeepEq(info, %@)), undefined)", expectedInfo],

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(runtimeManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIRuntime, LastError)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(typeof browser.runtime.lastError, 'object')",
        @"browser.test.assertEq(browser.runtime.lastError, null)",

        // FIXME: <https://webkit.org/b/248156> Need test cases for lastError

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(runtimeManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIRuntime, SendMessageFromContentScript)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();
    auto *urlString = urlRequest.URL.absoluteString;

    auto *backgroundScript = Util::constructScript(@[
        [NSString stringWithFormat:@"const expectedURL = '%@'", urlString],
        [NSString stringWithFormat:@"const expectedOrigin = '%@'", [urlString substringToIndex:urlString.length - 1]],

        @"browser.runtime.onMessage.addListener((message, sender, sendResponse) => {",
        @"  browser.test.assertEq(message.content, 'Hello', 'Should receive the correct message from the content script')",

        @"  browser.test.assertEq(typeof sender, 'object', 'sender should be an object')",

        @"  browser.test.assertEq(typeof sender.tab, 'object', 'sender.tab should be an object')",
        @"  browser.test.assertEq(sender.tab.url, expectedURL, 'sender.tab.url should be the expected URL')",

        @"  browser.test.assertEq(sender.url, expectedURL, 'sender.url should be the expected URL')",
        @"  browser.test.assertEq(sender.origin, expectedOrigin, 'sender.origin should be the expected origin')",

        @"  browser.test.assertEq(sender.frameId, 0, 'sender.frameId should be 0')",

        @"  sendResponse({ content: 'Received' })",
        @"})",

        @"browser.test.yield('Load Tab')"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"(async () => {",
        @"  const response = await browser.runtime.sendMessage({ content: 'Hello' })",

        @"  browser.test.assertEq(response.content, 'Received', 'Should get the response from the background script')",

        @"  browser.test.notifyPass()",
        @"})()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeContentScriptManifest resources:@{ @"background.js": backgroundScript, @"content.js": contentScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIRuntime, SendMessageFromContentScriptWithPromiseReply)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();
    auto *urlString = urlRequest.URL.absoluteString;

    auto *backgroundScript = Util::constructScript(@[
        [NSString stringWithFormat:@"const expectedURL = '%@'", urlString],
        [NSString stringWithFormat:@"const expectedOrigin = '%@'", [urlString substringToIndex:urlString.length - 1]],

        @"browser.runtime.onMessage.addListener((message, sender, sendResponse) => {",
        @"  browser.test.assertEq(message.content, 'Hello', 'Should receive the correct message from the content script')",

        @"  browser.test.assertEq(typeof sender, 'object', 'sender should be an object')",

        @"  browser.test.assertEq(typeof sender.tab, 'object', 'sender.tab should be an object')",
        @"  browser.test.assertEq(sender.tab.url, expectedURL, 'sender.tab.url should be the expected URL')",

        @"  browser.test.assertEq(sender.url, expectedURL, 'sender.url should be the expected URL')",
        @"  browser.test.assertEq(sender.origin, expectedOrigin, 'sender.origin should be the expected origin')",

        @"  browser.test.assertEq(sender.frameId, 0, 'sender.frameId should be 0')",

        @"  return Promise.resolve({ content: 'Received' })",
        @"})",

        @"browser.test.yield('Load Tab')"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"(async () => {",
        @"  const response = await browser.runtime.sendMessage({ content: 'Hello' })",

        @"  browser.test.assertEq(response.content, 'Received', 'Should get the response from the background script')",

        @"  browser.test.notifyPass()",
        @"})()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeContentScriptManifest resources:@{ @"background.js": backgroundScript, @"content.js": contentScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIRuntime, SendMessageFromContentScriptWithNoReply)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();
    auto *urlString = urlRequest.URL.absoluteString;

    auto *backgroundScript = Util::constructScript(@[
        [NSString stringWithFormat:@"const expectedURL = '%@'", urlString],
        [NSString stringWithFormat:@"const expectedOrigin = '%@'", [urlString substringToIndex:urlString.length - 1]],

        @"browser.runtime.onMessage.addListener((message, sender) => {",
        @"  browser.test.assertEq(message.content, 'Hello', 'Should receive the correct message from the content script')",

        @"  browser.test.assertEq(typeof sender, 'object', 'sender should be an object')",

        @"  browser.test.assertEq(typeof sender.tab, 'object', 'sender.tab should be an object')",
        @"  browser.test.assertEq(sender.tab.url, expectedURL, 'sender.tab.url should be the expected URL')",

        @"  browser.test.assertEq(sender.url, expectedURL, 'sender.url should be the expected URL')",
        @"  browser.test.assertEq(sender.origin, expectedOrigin, 'sender.origin should be the expected origin')",

        @"  browser.test.assertEq(sender.frameId, 0, 'sender.frameId should be 0')",
        @"})",

        @"browser.test.yield('Load Tab')"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"(async () => {",
        @"  const response = await browser.runtime.sendMessage({ content: 'Hello' })",

        @"  browser.test.assertEq(response, undefined, 'Should get no response from the background script')",

        @"  browser.test.notifyPass()",
        @"})()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeContentScriptManifest resources:@{ @"background.js": backgroundScript, @"content.js": contentScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager run];
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
