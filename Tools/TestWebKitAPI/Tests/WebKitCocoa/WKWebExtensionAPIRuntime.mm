/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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

    @"options_page": @"options.html",

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

static auto *externallyConnectableManifest = @{
    @"manifest_version": @3,

    @"name": @"Externally Connectable Test",
    @"description": @"Runtime Test",
    @"version": @"1",

    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"type": @"module",
        @"persistent": @NO,
    },

    @"externally_connectable": @{
        @"matches": @[ @"*://localhost/*" ],
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
    [_WKWebExtensionMatchPattern registerCustomURLScheme:@"test-extension"];
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

    manager.get().internalDelegate.presentPopupForAction = ^(_WKWebExtensionAction *action) {
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

TEST(WKWebExtensionAPIRuntime, GetFrameId)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/subframe'></iframe>"_s } },
        { "/subframe"_s, { { { "Content-Type"_s, "text/html"_s } }, "Subframe Content"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.yield('Load Tab')"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"if (window.top === window) {",
        @"  let mainFrameId = browser.runtime.getFrameId(window)",
        @"  browser.test.assertEq(mainFrameId, 0, 'Main frame should have frameId 0')",

        @"  let frameElement = document.querySelector('iframe')",
        @"  let subFrameElementId = browser.runtime.getFrameId(frameElement)",
        @"  browser.test.assertTrue(subFrameElementId > 0, 'Subframes should have a positive non-zero frameId')",

        @"  let subFrameWindow = frameElement.contentWindow",
        @"  let subFrameWindowId = browser.runtime.getFrameId(subFrameWindow)",
        @"  browser.test.assertTrue(subFrameWindowId > 0, 'Subframes should have a positive non-zero frameId')",

        @"  browser.test.assertEq(subFrameElementId, subFrameWindowId, 'Subframes should have the same frameId from window and element')",
        @"} else {",
        @"  let subFrameId = browser.runtime.getFrameId(window)",
        @"  browser.test.assertTrue(subFrameId > 0, 'Subframes should have a positive non-zero frameId')",

        @"  let topFrameId = browser.runtime.getFrameId(window.top)",
        @"  browser.test.assertEq(topFrameId, 0, 'The top window in a subframe context should have frameId 0')",
        @"}",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeContentScriptManifest resources:@{ @"background.js": backgroundScript, @"content.js": contentScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager run];
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
        @"  browser.test.assertEq(message?.content, 'Hello', 'Should receive the correct message from the content script')",

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

        @"  browser.test.assertEq(response?.content, 'Received', 'Should get the response from the background script')",

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

TEST(WKWebExtensionAPIRuntime, SendMessageFromContentScriptWithAsyncReply)
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
        @"  browser.test.assertEq(message?.content, 'Hello', 'Should receive the correct message from the content script')",

        @"  browser.test.assertEq(typeof sender, 'object', 'sender should be an object')",

        @"  browser.test.assertEq(typeof sender.tab, 'object', 'sender.tab should be an object')",
        @"  browser.test.assertEq(sender.tab.url, expectedURL, 'sender.tab.url should be the expected URL')",

        @"  browser.test.assertEq(sender.url, expectedURL, 'sender.url should be the expected URL')",
        @"  browser.test.assertEq(sender.origin, expectedOrigin, 'sender.origin should be the expected origin')",

        @"  browser.test.assertEq(sender.frameId, 0, 'sender.frameId should be 0')",

        @"  setTimeout(() => sendResponse({ content: 'Received' }), 1000)",

        @"  return true",
        @"})",

        @"browser.test.yield('Load Tab')"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"(async () => {",
        @"  const response = await browser.runtime.sendMessage({ content: 'Hello' })",

        @"  browser.test.assertEq(response?.content, 'Received', 'Should get the response from the background script')",

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
        @"  browser.test.assertEq(message?.content, 'Hello', 'Should receive the correct message from the content script')",

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

        @"  browser.test.assertEq(response?.content, 'Received', 'Should get the response from the background script')",

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

TEST(WKWebExtensionAPIRuntime, SendMessageFromContentScriptWithAsyncPromiseReply)
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
        @"  browser.test.assertEq(message?.content, 'Hello', 'Should receive the correct message from the content script')",

        @"  browser.test.assertEq(typeof sender, 'object', 'sender should be an object')",

        @"  browser.test.assertEq(typeof sender.tab, 'object', 'sender.tab should be an object')",
        @"  browser.test.assertEq(sender.tab.url, expectedURL, 'sender.tab.url should be the expected URL')",

        @"  browser.test.assertEq(sender.url, expectedURL, 'sender.url should be the expected URL')",
        @"  browser.test.assertEq(sender.origin, expectedOrigin, 'sender.origin should be the expected origin')",

        @"  browser.test.assertEq(sender.frameId, 0, 'sender.frameId should be 0')",

        @"  return new Promise((resolve) => {",
        @"    setTimeout(() => resolve({ content: 'Received' }), 1000)",
        @"  })",
        @"})",

        @"browser.test.yield('Load Tab')"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"(async () => {",
        @"  const response = await browser.runtime.sendMessage({ content: 'Hello' })",

        @"  browser.test.assertEq(response?.content, 'Received', 'Should get the response from the background script')",

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
        @"  browser.test.assertEq(message?.content, 'Hello', 'Should receive the correct message from the content script')",

        @"  browser.test.assertEq(typeof sender, 'object', 'sender should be an object')",

        @"  browser.test.assertEq(typeof sender.tab, 'object', 'sender.tab should be an object')",
        @"  browser.test.assertEq(sender.tab.url, expectedURL, 'sender.tab.url should be the expected URL')",

        @"  browser.test.assertEq(sender.url, expectedURL, 'sender.url should be the expected URL')",
        @"  browser.test.assertEq(sender.origin, expectedOrigin, 'sender.origin should be the expected origin')",

        @"  browser.test.assertEq(sender.frameId, 0, 'sender.frameId should be 0')",

        @"  return false",
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

TEST(WKWebExtensionAPIRuntime, SendMessageFromSubframe)
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
        @"browser.runtime.onMessage.addListener((message, sender) => {",
        @"  browser.test.assertEq(message?.content, 'Hello from Subframe', 'Should receive the correct message from the subframe content script')",
        @"  return Promise.resolve({ content: 'Received your message' })",
        @"})",

        @"browser.test.yield('Load Tab')"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"(async () => {",
        @"  const response = await browser.runtime.sendMessage({ content: 'Hello from Subframe' })",
        @"  browser.test.assertEq(response?.content, 'Received your message', 'Should get the response from the background script')",

        @"  browser.test.notifyPass()",
        @"})()"
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

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequestSubframe.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.mainWebView loadRequest:urlRequestMain];

    [manager run];
}

TEST(WKWebExtensionAPIRuntime, SendMessageWithTabFrameAndAsyncReply)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *extensionManifest = @{
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

        @"web_accessible_resources": @[ @{
            @"resources": @[ @"*.html" ],
            @"matches": @[ @"*://localhost/*" ]
        } ],
    };

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener((message, sender, sendResponse) => {",
        @"  if (message?.content === 'Hello from iframe')",
        @"    setTimeout(() => sendResponse({ content: 'Async reply from background' }), 500)",

        @"  return true",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    auto *iframeScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener((message, sender, sendResponse) => {",
        @"  // This listener should not be called since it is in the same frame as the sender,",
        @"  // but having it is needed to verify the background script is the responder.",

        @"  browser.test.notifyFail('Frame listener should not be called')",
        @"})",

        @"const response = await browser.runtime.sendMessage({ content: 'Hello from iframe' })",
        @"browser.test.assertEq(response?.content, 'Async reply from background', 'Should receive the correct async reply from background script')",

        @"browser.test.notifyPass()",
    ]);

    auto *contentScript = Util::constructScript(@[
        @"(function() {",
        @"  const iframe = document.createElement('iframe')",
        @"  iframe.src = browser.runtime.getURL('extension-frame.html')",
        @"  document.body.appendChild(iframe)",
        @"})()",
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"content.js": contentScript,
        @"extension-frame.js": iframeScript,
        @"extension-frame.html": @"<script type='module' src='extension-frame.js'></script>",
    };

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:extensionManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIRuntime, SendMessageWithNaNValue)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener((message, sender, sendResponse) => {",
        @"  browser.test.assertEq(message?.value, null, 'The message value should be null')",

        @"  sendResponse({ value: NaN })",
        @"})",

        @"browser.test.yield('Load Tab')"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"(async () => {",
        @"  const response = await browser.runtime.sendMessage({ value: NaN })",

        @"  browser.test.assertEq(response?.value, null, 'Should get a null response from the background script')",

        @"  browser.test.notifyPass()",
        @"})()"
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"content.js": contentScript
    };

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeContentScriptManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.mainWebView loadRequest:server.requestWithLocalhost()];

    [manager run];
}

TEST(WKWebExtensionAPIRuntime, ConnectFromContentScript)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime?.onConnect.addListener((port) => {",
        @"  browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')",
        @"  browser.test.assertEq(port?.error, null, 'Port error should be null')",

        @"  browser.test.assertEq(typeof port?.sender, 'object', 'sender should be an object')",
        @"  browser.test.assertEq(typeof port?.sender?.url, 'string', 'sender.url should be a string')",
        @"  browser.test.assertEq(typeof port?.sender?.origin, 'string', 'sender.origin should be a string')",
        @"  browser.test.assertTrue(port?.sender?.url?.startsWith('http'), 'sender.url should start with http')",
        @"  browser.test.assertTrue(port?.sender?.origin?.startsWith('http'), 'sender.origin should start with http')",
        @"  browser.test.assertEq(typeof port?.sender?.tab, 'object', 'sender.tab should be an object')",
        @"  browser.test.assertEq(port?.sender?.frameId, 0, 'sender.frameId should be 0')",

        @"  port?.onMessage.addListener((message) => {",
        @"    browser.test.assertEq(message, 'Hello', 'Should receive the correct message content')",
        @"    port?.postMessage('Received')",
        @"  })",
        @"})"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"setTimeout(() => {",
        @"  const port = browser.runtime?.connect({ name: 'testPort' })",
        @"  browser.test.assertEq(typeof port, 'object', 'Port should be an object')",
        @"  browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')",
        @"  browser.test.assertEq(port?.sender, null, 'Port sender should be null')",

        @"  port?.postMessage('Hello')",

        @"  port?.onMessage.addListener((response) => {",
        @"    browser.test.assertEq(response, 'Received', 'Should get the response from the background script')",

        @"    browser.test.notifyPass()",
        @"  })",
        @"}, 1000)"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeContentScriptManifest resources:@{ @"background.js": backgroundScript, @"content.js": contentScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIRuntime, ConnectFromSubframe)
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
        @"browser.runtime.onConnect.addListener((port) => {",
        @"  browser.test.assertEq(port.name, 'subframePort', 'Port name should be subframePort')",

        @"  port.onMessage.addListener((message) => {",
        @"    browser.test.assertEq(message, 'Hello from Subframe', 'Should receive the correct message content from subframe')",
        @"    port.postMessage('Received from Background')",
        @"  })",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    auto *contentScript = Util::constructScript(@[
        @"(async () => {",
        @"  const port = browser.runtime.connect({ name: 'subframePort' })",
        @"  port.postMessage('Hello from Subframe')",

        @"  port.onMessage.addListener((response) => {",
        @"    browser.test.assertEq(response, 'Received from Background', 'Should get the response from the background script')",

        @"    browser.test.notifyPass()",
        @"  })",
        @"})()"
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

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequestSubframe.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.mainWebView loadRequest:urlRequestMain];

    [manager run];
}

TEST(WKWebExtensionAPIRuntime, ConnectFromContentScriptWithMultipleListeners)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime?.onConnect.addListener((port) => {",
        @"  browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort in first listener')",
        @"  browser.test.assertEq(port?.error, null, 'Port error should be null in first listener')",

        @"  browser.test.assertEq(typeof port?.sender, 'object', 'sender should be an object')",
        @"  browser.test.assertEq(typeof port?.sender?.url, 'string', 'sender.url should be a string')",
        @"  browser.test.assertEq(typeof port?.sender?.origin, 'string', 'sender.origin should be a string')",
        @"  browser.test.assertTrue(port?.sender?.url?.startsWith('http'), 'sender.url should start with http')",
        @"  browser.test.assertTrue(port?.sender?.origin?.startsWith('http'), 'sender.origin should start with http')",
        @"  browser.test.assertEq(typeof port?.sender?.tab, 'object', 'sender.tab should be an object')",
        @"  browser.test.assertEq(port?.sender?.frameId, 0, 'sender.frameId should be 0')",

        @"  port?.onMessage.addListener((message) => {",
        @"    browser.test.assertEq(message, 'Hello', 'Should receive the correct message content in first listener')",
        @"    port?.postMessage('Received')",
        @"  })",
        @"})",

        @"browser.runtime?.onConnect.addListener((port) => {",
        @"  browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort in second listener')",
        @"  browser.test.assertEq(port?.error, null, 'Port error should be null in second listener')",

        @"  browser.test.assertEq(typeof port?.sender, 'object', 'sender should be an object')",
        @"  browser.test.assertEq(typeof port?.sender?.url, 'string', 'sender.url should be a string')",
        @"  browser.test.assertEq(typeof port?.sender?.origin, 'string', 'sender.origin should be a string')",
        @"  browser.test.assertTrue(port?.sender?.url?.startsWith('http'), 'sender.url should start with http')",
        @"  browser.test.assertTrue(port?.sender?.origin?.startsWith('http'), 'sender.origin should start with http')",
        @"  browser.test.assertEq(typeof port?.sender?.tab, 'object', 'sender.tab should be an object')",
        @"  browser.test.assertEq(port?.sender?.frameId, 0, 'sender.frameId should be 0')",

        @"  port?.onMessage.addListener((message) => {",
        @"    browser.test.assertEq(message, 'Hello', 'Should receive the correct message content in second listener')",
        @"    port?.postMessage('Received')",
        @"  })",
        @"})"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"setTimeout(() => {",
        @"  const port = browser.runtime?.connect({ name: 'testPort' })",
        @"  browser.test.assertEq(typeof port, 'object', 'Port should be an object')",
        @"  browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')",
        @"  browser.test.assertEq(port?.sender, null, 'Port sender should be null')",

        @"  port?.postMessage('Hello')",

        @"  let receivedMessages = 0",
        @"  port?.onMessage.addListener((response) => {",
        @"    browser.test.assertEq(response, 'Received', 'Should get the response from the background script')",

        @"    if (++receivedMessages === 2)",
        @"      browser.test.notifyPass()",
        @"  })",
        @"}, 1000)"
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
        @"browser.runtime?.onConnect.addListener((port) => {",
        @"  browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')",

        @"  port?.onMessage.addListener((message) => {",
        @"    browser.test.assertEq(message, 'Message from content script to background', 'Should receive the correct message content from the content script')",
        @"  })",

        @"  port?.onDisconnect.addListener(() => {",
        @"    browser.test.assertTrue(true, 'Should trigger the onDisconnect event in the background script')",

        @"    browser.test.notifyPass()",
        @"  })",
        @"})"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"setTimeout(() => {",
        @"  const port = browser.runtime?.connect({ name: 'testPort' })",
        @"  browser.test.assertEq(typeof port, 'object', 'Port should be an object')",
        @"  browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')",
        @"  browser.test.assertEq(port?.sender, null, 'Port sender should be null')",

        @"  port?.postMessage('Message from content script to background')",

        @"  port?.onDisconnect.addListener(() => {",
        @"    browser.test.assertTrue(true, 'Should trigger the onDisconnect event in the content script')",
        @"  })",

        @"  port?.disconnect()",
        @"}, 1000)"
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
        @"browser.runtime?.onConnect.addListener((port) => {",
        @"  browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')",

        @"  port?.onMessage.addListener((message) => {",
        @"    browser.test.assertEq(message, 'Message from content script', 'Should receive the correct message content from content script')",

        @"    port?.postMessage('Response from background script 1')",
        @"    port?.disconnect()",
        @"  })",
        @"})",

        @"browser.runtime?.onConnect.addListener((port) => {",
        @"  browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')",

        @"  port?.onMessage.addListener((message) => {",
        @"    browser.test.assertEq(message, 'Message from content script', 'Should receive the correct message content from content script')",

        @"    port?.postMessage('Response from background script 2')",
        @"  })",
        @"})"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"let messagesReceived = 0",

        @"setTimeout(() => {",
        @"  const port = browser.runtime?.connect({ name: 'testPort' })",
        @"  browser.test.assertEq(typeof port, 'object', 'Port should be an object')",
        @"  browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')",
        @"  browser.test.assertEq(port?.sender, null, 'Port sender should be null')",

        @"  port?.postMessage('Message from content script')",

        @"  port?.onMessage.addListener((message) => {",
        @"    browser.test.assertTrue(message?.startsWith('Response from background script '), 'Should receive the correct message content')",

        @"    if (++messagesReceived === 2)",
        @"      browser.test.notifyPass()",
        @"  })",
        @"}, 1000)"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeContentScriptManifest resources:@{ @"background.js": backgroundScript, @"content.js": contentScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIRuntime, ConnectFromPopup)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime?.onConnect.addListener((port) => {",
        @"  port?.onMessage.addListener((message) => {",
        @"    browser.test.assertEq(message, 'Hello from popup', 'Should receive the correct message content')",

        @"    port?.postMessage('Received by background')",
        @"  })",
        @"})",

        @"browser.action?.openPopup()"
    ]);

    auto *popupScript = Util::constructScript(@[
        @"const port = browser.runtime?.connect({ name: 'testPort' })",
        @"browser.test.assertEq(typeof port, 'object', 'Port should be an object')",
        @"browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')",
        @"browser.test.assertEq(port?.sender, null, 'Port sender should be null')",

        @"port?.onMessage.addListener((response) => {",
        @"  browser.test.assertEq(response, 'Received by background', 'Should get the response from the background script')",

        @"  browser.test.notifyPass()",
        @"})",

        @"port?.postMessage('Hello from popup')",
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"popup.html": @"<script type='module' src='popup.js'></script>",
        @"popup.js": popupScript
    };

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentPopupForAction = ^(_WKWebExtensionAction *action) {
        // Do nothing so the popup web view will stay loaded.
    };

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

TEST(WKWebExtensionAPIRuntime, SendNativeMessageWithInvalidReply)
{
    auto *backgroundScript = Util::constructScript(@[
        @"await browser.test.assertRejects(browser.runtime.sendNativeMessage('test', 'Hello'), /reply message was not JSON-serializable/i)",

        @"browser.test.notifyPass()",
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:_WKWebExtensionPermissionNativeMessaging];

    manager.get().internalDelegate.sendMessage = ^(id message, NSString *applicationIdentifier, void (^replyHandler)(id replyMessage, NSError *error)) {
        EXPECT_NS_EQUAL(applicationIdentifier, @"test");
        EXPECT_NS_EQUAL(message, @"Hello");

        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            replyHandler(@{ @"bad": NSUUID.UUID }, nil);
        });
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIRuntime, ConnectNative)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const port = browser.runtime?.connectNative('test')",
        @"browser.test.assertEq(typeof port, 'object', 'Port should be an object')",
        @"browser.test.assertEq(port?.name, 'test', 'Port name should be test')",
        @"browser.test.assertEq(port?.sender, null, 'Port sender should be null')",

        @"let messagesReceived = 0",
        @"port?.onMessage.addListener((response) => {",
        @"  browser.test.assertTrue(response?.startsWith('Received '), 'Should get the response from the native code')",

        @"  ++messagesReceived",
        @"})",

        @"port?.onDisconnect.addListener(() => {",
        @"  if (messagesReceived === 2)",
        @"    browser.test.notifyPass()",
        @"  else",
        @"    browser.test.notifyFail('Not all messages were received')",
        @"})",

        @"port?.postMessage('Hello')"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:_WKWebExtensionPermissionNativeMessaging];

    manager.get().internalDelegate.connectUsingMessagePort = ^(_WKWebExtensionMessagePort *messagePort) {
        EXPECT_NS_EQUAL(messagePort.applicationIdentifier, @"test");

        messagePort.messageHandler = ^(id message, NSError *error) {
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

TEST(WKWebExtensionAPIRuntime, ConnectNativeWithInvalidMessage)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const port = browser.runtime?.connectNative('test')",
        @"port?.postMessage('Hello')"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:_WKWebExtensionPermissionNativeMessaging];

    manager.get().internalDelegate.connectUsingMessagePort = ^(_WKWebExtensionMessagePort *messagePort) {
        EXPECT_NS_EQUAL(messagePort.applicationIdentifier, @"test");

        messagePort.messageHandler = ^(id message, NSError *error) {
            EXPECT_NULL(error);
            EXPECT_NS_EQUAL(message, @"Hello");

            [messagePort sendMessage:@{ @"bad": NSUUID.UUID } completionHandler:^(BOOL success, NSError *error) {
                EXPECT_FALSE(success);
                EXPECT_NOT_NULL(error);
                EXPECT_EQ(error.code, _WKWebExtensionMessagePortErrorMessageInvalid);
            }];

            [messagePort disconnectWithError:nil];

            [manager done];
        };
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIRuntime, ConnectNativeWithUndefinedMessage)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const port = browser.runtime?.connectNative('test')",
        @"browser.test.assertEq(typeof port, 'object', 'Port should be an object')",
        @"browser.test.assertEq(port?.name, 'test', 'Port name should be test')",
        @"browser.test.assertEq(port?.sender, null, 'Port sender should be null')",

        @"port?.onMessage.addListener((response) => {",
        @"  browser.test.assertEq(response, undefined, 'Response should be undefined when sending null message')",

        @"  browser.test.notifyPass()",
        @"})",

        @"port?.postMessage('Hello')"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:_WKWebExtensionPermissionNativeMessaging];

    manager.get().internalDelegate.connectUsingMessagePort = ^(_WKWebExtensionMessagePort *messagePort) {
        EXPECT_NS_EQUAL(messagePort.applicationIdentifier, @"test");

        messagePort.messageHandler = ^(id message, NSError *error) {
            EXPECT_NULL(error);
            EXPECT_NS_EQUAL(message, @"Hello");

            [messagePort sendMessage:nil completionHandler:^(BOOL success, NSError *error) {
                EXPECT_TRUE(success);
                EXPECT_NULL(error);
            }];

            [messagePort disconnectWithError:nil];

            [manager done];
        };
    };

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIRuntime, Reload)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.yield('Loaded')",
        @"browser.runtime.reload()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Loaded");

    [manager run];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Loaded");

    [manager done];
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

TEST(WKWebExtensionAPIRuntime, OpenOptionsPage)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertSafeResolve(() => browser.runtime.openOptionsPage())"
    ]);

    auto resources = @{
        @"background.js": backgroundScript,
        @"options.html": @"Hello world!"
    };

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:runtimeManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    __block bool optionsPageOpened = false;
    manager.get().internalDelegate.openOptionsPage = ^(_WKWebExtensionContext *, void (^completionHandler)(NSError *)) {
        optionsPageOpened = true;

        EXPECT_NS_EQUAL(manager.get().context.optionsPageURL, [NSURL URLWithString:@"options.html" relativeToURL:manager.get().context.baseURL].absoluteURL);

        completionHandler(nil);

        [manager done];
    };

    [manager loadAndRun];

    EXPECT_TRUE(optionsPageOpened);
}

TEST(WKWebExtensionAPIRuntime, ConnectFromWebPage)
{
    auto *uniqueIdentifier = @"org.webkit.test.extension (76C788B8)";

    auto *webpageScript = Util::constructScript(@[
        @"<script>",
        @"setTimeout(() => {",
        [NSString stringWithFormat:@"const port = browser?.runtime?.connect('%@', { name: 'testPort' })", uniqueIdentifier],
        @"  browser.test.assertEq(typeof port, 'object', 'Port should be an object')",
        @"  browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')",
        @"  browser.test.assertEq(port?.sender, null, 'Port sender should be null')",

        @"  port?.postMessage('Hello')",

        @"  port?.onMessage.addListener((response) => {",
        @"    browser.test.assertEq(response, 'Received')",
        @"    port?.postMessage('Success')",
        @"  })",
        @"}, 1000)",
        @"</script>"
    ]);

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, webpageScript } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime?.onConnectExternal.addListener((port) => {",
        @"  browser.test.assertEq(port?.name, 'testPort', 'Port name should be testPort')",
        @"  browser.test.assertEq(port?.error, null, 'Port error should be null')",

        @"  browser.test.assertEq(typeof port?.sender, 'object', 'sender should be an object')",
        @"  browser.test.assertEq(typeof port?.sender?.url, 'string', 'sender.url should be a string')",
        @"  browser.test.assertEq(typeof port?.sender?.origin, 'string', 'sender.origin should be a string')",
        @"  browser.test.assertTrue(port?.sender?.url?.startsWith('http'), 'sender.url should start with http')",
        @"  browser.test.assertTrue(port?.sender?.origin?.startsWith('http'), 'sender.origin should start with http')",
        @"  browser.test.assertEq(typeof port?.sender?.tab, 'object', 'sender.tab should be an object')",
        @"  browser.test.assertEq(port?.sender?.frameId, 0, 'sender.frameId should be 0')",

        @"  port?.onMessage.addListener((message) => {",
        @"    if (message == 'Hello')",
        @"      port?.postMessage('Received')",
        @"    else if (message == 'Success')",
        @"      browser.test.notifyPass()",
        @"  })",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:externallyConnectableManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    // Set an uniqueIdentifier so it is a known value and not the default random one.
    manager.get().context.uniqueIdentifier = uniqueIdentifier;

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIRuntime, ConnectFromWebPageWithWrongIdentifier)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessageExternal.addListener((message, sender, sendResponse) => {",
        @"  browser.test.notifyFail('The page should not be able to connect')",
        @"})",

        @"browser.test.yield('Load Tab')"
    ]);

    auto *webpageScript = Util::constructScript(@[
        @"<script>",
        @"  const startTime = performance.now()",

        @"  let port = browser.runtime.connect('org.webkit.test.extension (Incorrect)', { name: 'testPort' })",
        @"  browser.test.assertTrue(!!port, 'Port should be returned even with a wrong extension identifier')",

        @"  port.onDisconnect.addListener((port) => {",
        @"    browser.test.assertTrue((performance.now() - startTime) >= 100, 'Disconnect should happen after a delay of at least 100ms')",

        @"    browser.test.notifyPass()",
        @"  })",
        @"</script>"
    ]);

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, webpageScript } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:externallyConnectableManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIRuntime, SendMessageFromWebPage)
{
    auto *uniqueIdentifier = @"org.webkit.test.extension (SendMessageTest)";

    auto *webpageScript = Util::constructScript(@[
        @"<script>",
        @"setTimeout(() => {",
        [NSString stringWithFormat:@"browser.runtime.sendMessage('%@', 'Hello', (response) => {", uniqueIdentifier],
        @"  browser.test.assertEq(response, 'Received')",

        @"  browser.test.notifyPass()",
        @"})",
        @"}, 1000)",
        @"</script>"
    ]);

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, webpageScript } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessageExternal.addListener((message, sender, sendResponse) => {",
        @"  browser.test.assertEq(message, 'Hello', 'Should receive the correct message')",

        @"  browser.test.assertEq(typeof sender, 'object', 'sender should be an object')",
        @"  browser.test.assertEq(typeof sender?.url, 'string', 'sender.url should be a string')",
        @"  browser.test.assertEq(typeof sender?.origin, 'string', 'sender.origin should be a string')",
        @"  browser.test.assertTrue(sender?.url?.startsWith('http'), 'sender.url should start with http')",
        @"  browser.test.assertTrue(sender?.origin?.startsWith('http'), 'sender.origin should start with http')",
        @"  browser.test.assertEq(typeof sender?.tab, 'object', 'sender.tab should be an object')",
        @"  browser.test.assertEq(sender?.frameId, 0, 'sender.frameId should be 0')",

        @"  sendResponse('Received')",
        @"})",

        @"browser.test.yield('Load Tab')"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:externallyConnectableManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    // Set an uniqueIdentifier so it is a known value and not the default random one.
    manager.get().context.uniqueIdentifier = uniqueIdentifier;

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIRuntime, SendMessageFromWebPageWithTabFrameAndAsyncReply)
{
    auto *extensionManifest = @{
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

        @"web_accessible_resources": @[ @{
            @"resources": @[ @"*.html" ],
            @"matches": @[ @"*://localhost/*" ]
        } ],

        @"externally_connectable": @{
            @"matches": @[ @"*://localhost/*" ]
        },
    };

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessageExternal.addListener((message, sender, sendResponse) => {",
        @"  browser.test.assertEq(message?.content, 'Hello from webpage', 'Should receive the correct message from the web page')",

        @"  setTimeout(() => sendResponse({ content: 'Async reply from background' }), 500)",

        @"  return true",
        @"})",

        @"browser.test.yield('Load Tabs')"
    ]);

    auto *iframeScript = Util::constructScript(@[
        @"browser.runtime.onMessageExternal.addListener((message, sender, sendResponse) => {",
        @"  browser.test.assertEq(message?.content, 'Hello from webpage', 'Should receive the correct message from the web page')",
        @"})",
    ]);

    auto *contentScript = Util::constructScript(@[
        @"(function() {",
        @"  const iframe = document.createElement('iframe')",
        @"  iframe.src = browser.runtime.getURL('extension-frame.html')",
        @"  document.body.appendChild(iframe)",
        @"})()",
    ]);

    auto *webpageScript = Util::constructScript(@[
        @"<script>",
        @"setTimeout(() => {",
        @"  browser.runtime.sendMessage('org.webkit.test.extension (SendMessageTest)', { content: 'Hello from webpage' }, (response) => {",
        @"    browser.test.assertEq(response?.content, 'Async reply from background', 'Should receive the correct reply from the extension frame')",

        @"    browser.test.notifyPass()",
        @"  })",
        @"}, 1000)",
        @"</script>"
    ]);

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, webpageScript } },
        { "/second-tab.html"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();

    auto *resources = @{
        @"background.js": backgroundScript,
        @"content.js": contentScript,
        @"extension-frame.js": iframeScript,
        @"extension-frame.html": @"<script type='module' src='extension-frame.js'></script>",
    };

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:extensionManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    // Set an uniqueIdentifier so it is a known value and not the default random one.
    manager.get().context.uniqueIdentifier = @"org.webkit.test.extension (SendMessageTest)";

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tabs");

    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    auto *secondTab = [manager.get().defaultWindow openNewTab];
    [secondTab.mainWebView loadRequest:server.requestWithLocalhost("/second-tab.html"_s)];

    [manager run];
}

TEST(WKWebExtensionAPIRuntime, SendMessageFromWebPageWithWrongIdentifier)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessageExternal.addListener(async (message, sender, sendResponse) => {",
        @"  browser.test.notifyFail('The page should not be able to send a message with a wrong identifier')",
        @"})",

        @"browser.test.yield('Load Tab')"
    ]);

    auto *webpageScript = Util::constructScript(@[
        @"<script type='module'>",
        @"  const startTime = performance.now()",

        @"  const response = await browser.runtime.sendMessage('org.webkit.test.extension (Incorrect)', 'Hello')",
        @"  browser.test.assertEq(response, undefined, 'The response should be undefined')",

        @"  browser.test.assertTrue((performance.now() - startTime) >= 100, 'Should take at least 100ms to attempt the message send')",

        @"  browser.test.notifyPass()",
        @"</script>"
    ]);

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, webpageScript } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:externallyConnectableManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager run];
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
