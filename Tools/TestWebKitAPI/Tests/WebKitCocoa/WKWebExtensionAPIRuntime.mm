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

    @"name": @"Runtime Test",
    @"description": @"Runtime Test",
    @"version": @"1",

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

    @"permissions": @[ @"nativeMessaging" ],
};

static auto *runtimeContentScriptManifest = @{
    @"manifest_version": @3,

    @"name": @"Runtime Test",
    @"description": @"Runtime Test",
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

    @"permissions": @[ @"nativeMessaging" ],
};

static auto *runtimeServiceWorkerManifest = @{
    @"manifest_version": @3,

    @"name": @"Runtime Test",
    @"description": @"Runtime Test",
    @"version": @"1",

    @"background": @{
        @"service_worker": @"background.js",
    },
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

TEST(WKWebExtensionAPIRuntime, GetBackgroundPageFromBackground)
{
    auto *backgroundScript = Util::constructScript(@[
        @"window.notifyTestPass = async () => {",
        @"  browser.test.notifyPass()",
        @"}",

        @"const backgroundPage = await browser.runtime.getBackgroundPage()",

        @"browser.test.assertEq(backgroundPage, window, 'Should be able to get the background window from itself')",
        @"browser.test.assertSafe(() => backgroundPage.notifyTestPass())"
    ]);

    Util::loadAndRunExtension(runtimeManifest, @{
        @"background.js": backgroundScript,
    });
}

TEST(WKWebExtensionAPIRuntime, GetBackgroundPageFromTab)
{
    auto *backgroundScript = Util::constructScript(@[
        @"window.notifyTestPass = () => {",
        @"  browser.test.notifyPass()",
        @"}",

        @"browser.tabs.create({ url: 'test.html' })"
    ]);

    auto *testScript = Util::constructScript(@[
        @"const backgroundPage = await browser.runtime.getBackgroundPage()",
        @"browser.test.assertSafe(() => backgroundPage.notifyTestPass())",
    ]);

    auto *testHTML = @"<script type='module' src='test.js'></script>";

    Util::loadAndRunExtension(runtimeManifest, @{
        @"background.js": backgroundScript,
        @"test.html": testHTML,
        @"test.js": testScript
    });
}

TEST(WKWebExtensionAPIRuntime, GetBackgroundPageFromPopup)
{
    auto *backgroundScript = Util::constructScript(@[
        @"window.notifyTestPass = () => {",
        @"  browser.test.notifyPass()",
        @"}",

        @"browser.action.openPopup()"
    ]);

    auto *popupScript = Util::constructScript(@[
        @"const backgroundPage = await browser.runtime.getBackgroundPage()",
        @"browser.test.assertSafe(() => backgroundPage.notifyTestPass())"
    ]);

    auto *popupHTML = @"<script type='module' src='popup.js'></script>";

    auto resources = @{
        @"background.js": backgroundScript,
        @"popup.html": popupHTML,
        @"popup.js": popupScript
    };

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentActionPopup = ^(_WKWebExtensionAction *action) {
        // Do nothing so the popup web view will stay loaded.
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIRuntime, GetBackgroundPageForServiceWorker)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.tabs.create({ url: 'test.html' })"
    ]);

    auto *testScript = Util::constructScript(@[
        @"const backgroundPage = await browser.runtime.getBackgroundPage()",
        @"browser.test.assertEq(backgroundPage, null, 'Background page should be null for service workers')",
        @"browser.test.notifyPass()"
    ]);

    auto *testHTML = @"<script type='module' src='test.js'></script>";

    Util::loadAndRunExtension(runtimeServiceWorkerManifest, @{
        @"background.js": backgroundScript,
        @"test.html": testHTML,
        @"test.js": testScript
    });
}

TEST(WKWebExtensionAPIRuntime, SetUninstallURL)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertSafeResolve(() => browser.runtime.setUninstallURL('https://example.com/uninstall'))",
        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(runtimeManifest, @{
        @"background.js": backgroundScript,
    });
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
    auto *testManifest = @{
        @"manifest_version": @3,

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"type": @"module",
            @"persistent": @NO,
        },

        @"unused" : [NSNull null],
    };

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertDeepEq(browser.runtime.getManifest(), { manifest_version: 3, background: { persistent: false, scripts: [ 'background.js' ], type: 'module' }, unused: null })",

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(testManifest, @{ @"background.js": backgroundScript });
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

TEST(WKWebExtensionAPIRuntime, ConnectFromContentScript)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onConnect.addListener((port) => {",
        @"  browser.test.assertEq(port.name, 'testPort', 'Port name should be testPort')",
        @"  browser.test.assertEq(port.error, null, 'Port error should be null')",

        @"  browser.test.assertEq(typeof port.sender, 'object', 'sender should be an object')",
        @"  browser.test.assertEq(typeof port.sender.url, 'string', 'sender.url should be a string')",
        @"  browser.test.assertEq(typeof port.sender.origin, 'string', 'sender.origin should be a string')",
        @"  browser.test.assertTrue(port.sender.url.startsWith('http'), 'sender.url should start with http')",
        @"  browser.test.assertTrue(port.sender.origin.startsWith('http'), 'sender.origin should start with http')",
        @"  browser.test.assertEq(typeof port.sender.tab, 'object', 'sender.tab should be an object')",
        @"  browser.test.assertEq(port.sender.frameId, 0, 'sender.frameId should be 0')",

        @"  port.onMessage.addListener((message) => {",
        @"    browser.test.assertEq(message, 'Hello', 'Should receive the correct message content')",
        @"    port.postMessage('Received')",
        @"  })",
        @"})"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"setTimeout(() => {",
        @"  const port = browser.runtime.connect({ name: 'testPort' })",
        @"  port.postMessage('Hello')",

        @"  port.onMessage.addListener((response) => {",
        @"    browser.test.assertEq(response, 'Received', 'Should get the response from the background script')",

        @"    browser.test.notifyPass()",
        @"  })",
        @"}, 1000);"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeContentScriptManifest resources:@{ @"background.js": backgroundScript, @"content.js": contentScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIRuntime, ConnectFromContentScriptWithMultipleListeners)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onConnect.addListener((port) => {",
        @"  browser.test.assertEq(port.name, 'testPort', 'Port name should be testPort in first listener')",
        @"  browser.test.assertEq(port.error, null, 'Port error should be null in first listener')",

        @"  browser.test.assertEq(typeof port.sender, 'object', 'sender should be an object')",
        @"  browser.test.assertEq(typeof port.sender.url, 'string', 'sender.url should be a string')",
        @"  browser.test.assertEq(typeof port.sender.origin, 'string', 'sender.origin should be a string')",
        @"  browser.test.assertTrue(port.sender.url.startsWith('http'), 'sender.url should start with http')",
        @"  browser.test.assertTrue(port.sender.origin.startsWith('http'), 'sender.origin should start with http')",
        @"  browser.test.assertEq(typeof port.sender.tab, 'object', 'sender.tab should be an object')",
        @"  browser.test.assertEq(port.sender.frameId, 0, 'sender.frameId should be 0')",

        @"  port.onMessage.addListener((message) => {",
        @"    browser.test.assertEq(message, 'Hello', 'Should receive the correct message content in first listener')",
        @"    port.postMessage('Received')",
        @"  })",
        @"})",

        @"browser.runtime.onConnect.addListener((port) => {",
        @"  browser.test.assertEq(port.name, 'testPort', 'Port name should be testPort in second listener')",
        @"  browser.test.assertEq(port.error, null, 'Port error should be null in second listener')",

        @"  browser.test.assertEq(typeof port.sender, 'object', 'sender should be an object')",
        @"  browser.test.assertEq(typeof port.sender.url, 'string', 'sender.url should be a string')",
        @"  browser.test.assertEq(typeof port.sender.origin, 'string', 'sender.origin should be a string')",
        @"  browser.test.assertTrue(port.sender.url.startsWith('http'), 'sender.url should start with http')",
        @"  browser.test.assertTrue(port.sender.origin.startsWith('http'), 'sender.origin should start with http')",
        @"  browser.test.assertEq(typeof port.sender.tab, 'object', 'sender.tab should be an object')",
        @"  browser.test.assertEq(port.sender.frameId, 0, 'sender.frameId should be 0')",

        @"  port.onMessage.addListener((message) => {",
        @"    browser.test.assertEq(message, 'Hello', 'Should receive the correct message content in second listener')",
        @"    port.postMessage('Received')",
        @"  })",
        @"})"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"setTimeout(() => {",
        @"  const port = browser.runtime.connect({ name: 'testPort' })",
        @"  port.postMessage('Hello')",

        @"  let receivedMessages = 0",
        @"  port.onMessage.addListener((response) => {",
        @"    browser.test.assertEq(response, 'Received', 'Should get the response from the background script')",

        @"    if (++receivedMessages === 2)",
        @"      browser.test.notifyPass()",
        @"  })",
        @"}, 1000);"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeContentScriptManifest resources:@{ @"background.js": backgroundScript, @"content.js": contentScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIRuntime, PortDisconnectFromContentScript)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onConnect.addListener((port) => {",
        @"  browser.test.assertEq(port.name, 'testPort', 'Port name should be testPort')",

        @"  port.onMessage.addListener((message) => {",
        @"    browser.test.assertEq(message, 'Hello', 'Should receive the correct message content')",
        @"  })",

        @"  port.onDisconnect.addListener(() => {",
        @"    browser.test.assertTrue(true, 'Should trigger the onDisconnect event in the background script')",

        @"    browser.test.notifyPass()",
        @"  })",
        @"})"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"setTimeout(() => {",
        @"  const port = browser.runtime.connect({ name: 'testPort' })",
        @"  port.postMessage('Hello')",

        @"  port.onDisconnect.addListener(() => {",
        @"    browser.test.assertTrue(true, 'Should trigger the onDisconnect event in the content script')",
        @"  })",

        @"  port.disconnect()",
        @"}, 1000);"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeContentScriptManifest resources:@{ @"background.js": backgroundScript, @"content.js": contentScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIRuntime, PortDisconnectFromContentScriptWithMultipleListeners)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onConnect.addListener((port) => {",
        @"  browser.test.assertEq(port.name, 'testPort', 'Port name should be testPort')",
        @"  port.onMessage.addListener((message) => {",
        @"    browser.test.assertEq(message, 'Hello', 'Should receive the correct message content')",
        @"    port.postMessage('Hello')",
        @"    port.disconnect()",
        @"  })",
        @"})",

        @"browser.runtime.onConnect.addListener((port) => {",
        @"  browser.test.assertEq(port.name, 'testPort', 'Port name should be testPort')",
        @"  port.onMessage.addListener((message) => {",
        @"    browser.test.assertEq(message, 'Hello', 'Should receive the correct message content')",
        @"    port.postMessage('Hello')",
        @"  })",
        @"})"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"let messagesReceived = 0",

        @"setTimeout(() => {",
        @"  const port = browser.runtime.connect({ name: 'testPort' })",
        @"  port.postMessage('Hello')",

        @"  port.onMessage.addListener((message) => {",
        @"    browser.test.assertEq(message, 'Hello', 'Should receive the correct message content')",

        @"    if (++messagesReceived === 2)",
        @"      browser.test.notifyPass()",
        @"  })",
        @"}, 1000);"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeContentScriptManifest resources:@{ @"background.js": backgroundScript, @"content.js": contentScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIRuntime, SendNativeMessage)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const response = await browser.test.assertSafeResolve(() => browser.runtime.sendNativeMessage('test', 'Hello'))",
        @"browser.test.assertEq(response, 'Received', 'Should get the response from the native app')",

        @"browser.test.notifyPass()",
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:_WKWebExtensionPermissionNativeMessaging];

    manager.get().internalDelegate.sendMessage = ^(id message, NSString *applicationIdentifier, void (^replyHandler)(id replyMessage, NSError *error)) {
        EXPECT_NS_EQUAL(applicationIdentifier, @"test");
        EXPECT_NS_EQUAL(message, @"Hello");

        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            replyHandler(@"Received", nil);
        });
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIRuntime, ConnectNative)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const port = browser.runtime.connectNative('test')",

        @"let messagesReceived = 0",
        @"port.onMessage.addListener((response) => {",
        @"  browser.test.assertTrue(response.startsWith('Received '), 'Should get the response from the native code')",
        @"  ++messagesReceived",
        @"})",

        @"port.onDisconnect.addListener(() => {",
        @"  if (messagesReceived === 2)",
        @"    browser.test.notifyPass()",
        @"  else",
        @"    browser.test.notifyFail('Not all messages were received')",
        @"})",

        @"port.postMessage('Hello')"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:_WKWebExtensionPermissionNativeMessaging];

    manager.get().internalDelegate.connectUsingMessagePort = ^(_WKWebExtensionMessagePort *messagePort) {
        EXPECT_NS_EQUAL(messagePort.applicationIdentifier, @"test");

        messagePort.messageHandler = ^(id _Nullable message, NSError * _Nullable error) {
            EXPECT_NULL(error);
            EXPECT_NS_EQUAL(message, @"Hello");

            [messagePort sendMessage:@"Received 1" completionHandler:^(BOOL success, NSError *error) {
                EXPECT_TRUE(success);
                EXPECT_NULL(error);
            }];

            [messagePort sendMessage:@"Received 2" completionHandler:^(BOOL success, NSError *error) {
                EXPECT_TRUE(success);
                EXPECT_NULL(error);
            }];

            [messagePort disconnectWithError:nil];
        };
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIRuntime, StartupEvent)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onStartup.addListener(() => {",
        @"  browser.test.yield('Startup Event Fired')",
        @"})",

        @"setTimeout(() => {",
        @"  browser.test.yield('Startup Event Did Not Fire')",
        @"}, 1000)"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Startup Event Fired");

    [manager.get().controller unloadExtensionContext:manager.get().context error:nullptr];

    [manager runForTimeInterval:5];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Startup Event Did Not Fire");
}

TEST(WKWebExtensionAPIRuntime, InstalledEvent)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onInstalled.addListener((details) => {",
        @"  browser.test.assertEq(details.reason, 'install')",

        @"  browser.test.yield('Installed Event Fired')",
        @"})",

        @"setTimeout(() => {",
        @"  browser.test.yield('Installed Event Did Not Fire')",
        @"}, 1000)"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Wait until after startup event time expires.
    [manager runForTimeInterval:5];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Installed Event Fired");

    [manager.get().controller unloadExtensionContext:manager.get().context error:nullptr];

    [manager runForTimeInterval:1];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Installed Event Fired");
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
