/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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
#import "Helpers/cocoa/WebExtensionUtilities.h"

namespace TestWebKitAPI {

static auto *manifest = @{
    @"manifest_version": @3,

    @"name": @"Test Extension",
    @"description": @"Test Extension",
    @"version": @"1.0",

    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"persistent": @NO
    },

    @"content_scripts": @[@{
        @"matches": @[ @"*://*/*" ],
        @"js": @[ @"content.js" ]
    }]
};

TEST(WKWebExtensionAPITest, TestStartedEvent)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.onTestStarted.addListener((data) => {",
        @"  browser.test.assertEq(data?.testName, 'test', 'data.testName should be')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.sendMessage('Send Test Message')"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager runUntilTestMessage:@"Send Test Message"];

    [manager sendTestStartedWithArgument:@{ @"testName": @"test" }];

    [manager run];
}

TEST(WKWebExtensionAPITest, TestFinishedEvent)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.onTestFinished.addListener((data) => {",
        @"  browser.test.assertEq(data?.testName, 'test', 'data.testName should be')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.sendMessage('Send Test Message')"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager runUntilTestMessage:@"Send Test Message"];

    [manager sendTestFinishedWithArgument:@{ @"testName": @"test" }];

    [manager run];
}

TEST(WKWebExtensionAPITest, MessageEvent)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.onMessage.addListener((message, data) => {",
        @"  browser.test.assertEq(message, 'Test', 'message should be')",
        @"  browser.test.assertEq(data?.key, 'value', 'data.key should be')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.sendMessage('Send Test Message')"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager runUntilTestMessage:@"Send Test Message"];

    [manager sendTestMessage:@"Test" withArgument:@{ @"key": @"value" }];

    [manager run];
}

TEST(WKWebExtensionAPITest, MessageEventInWebPage)
{
    auto *pageScript = Util::constructScript(@[
        @"browser.test.onMessage.addListener((message, data) => {",
        @"  browser.test.assertEq(message, 'Test', 'message should be')",
        @"  browser.test.assertEq(data?.key, 'value', 'data.key should be')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.sendMessage('Ready for Message')"
    ]);

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<script type='module' src='script.js'></script>"_s } },
        { "/script.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, pageScript } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *resources = @{
        @"background.js": @"// This script is intentionally left blank."
    };

    RetainPtr extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources]);
    RetainPtr manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager load];

    [manager.get().defaultTab.webView loadRequest:server.requestWithLocalhost()];

    [manager runUntilTestMessage:@"Ready for Message"];

    [manager sendTestMessage:@"Test" withArgument:@{ @"key": @"value" }];

    [manager run];
}

TEST(WKWebExtensionAPITest, MessageEventInContentScript)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *contentScript = Util::constructScript(@[
        @"browser.test.onMessage.addListener((message, data) => {",
        @"  browser.test.assertEq(message, 'Test', 'message should be')",
        @"  browser.test.assertEq(data?.key, 'value', 'data.key should be')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.sendMessage('Ready for Message')"
    ]);

    auto *resources = @{
        @"background.js": @"// This script is intentionally left blank.",
        @"content.js": contentScript
    };

    RetainPtr extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources]);
    RetainPtr manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager load];

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager runUntilTestMessage:@"Ready for Message"];

    [manager sendTestMessage:@"Test" withArgument:@{ @"key": @"value" }];

    [manager run];
}

TEST(WKWebExtensionAPITest, JSHandleCreationInBackgroundPage)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const canCreateJSHandle = typeof window.webkit?.createJSHandle === 'function'",
        @"browser.test.assertTrue(canCreateJSHandle, 'createJSHandle should be available in an extension page')",

        @"const handle = canCreateJSHandle ? window.webkit.createJSHandle(document.documentElement) : null",
        @"browser.test.assertTrue(!!handle, 'createJSHandle should return a handle for an element')",

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(manifest, @{
        @"background.js": backgroundScript
    });
}

TEST(WKWebExtensionAPITest, JSHandleCreationInContentScript)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body></body>"_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *contentScript = Util::constructScript(@[
        @"const canCreateJSHandle = typeof window.webkit?.createJSHandle === 'function'",
        @"browser.test.assertTrue(canCreateJSHandle, 'createJSHandle should be available in a content script')",

        @"const handle = canCreateJSHandle ? window.webkit.createJSHandle(document.body) : null",
        @"browser.test.assertTrue(!!handle, 'createJSHandle should return a handle for an element')",

        @"browser.test.notifyPass()"
    ]);

    auto *resources = @{
        @"background.js": @"// This script is intentionally left blank.",
        @"content.js": contentScript
    };

    RetainPtr extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources]);
    RetainPtr manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager load];

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPITest, JSHandleCreationInExtensionFrame)
{
    auto *frameManifest = @{
        @"manifest_version": @3,

        @"name": @"Test Extension",
        @"description": @"Test Extension",
        @"version": @"1.0",

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"persistent": @NO
        },

        @"content_scripts": @[ @{
            @"matches": @[ @"*://localhost/*" ],
            @"js": @[ @"content.js" ]
        } ],

        @"web_accessible_resources": @[ @{
            @"resources": @[ @"*.html" ],
            @"matches": @[ @"*://localhost/*" ]
        } ]
    };

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body></body>"_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *frameScript = Util::constructScript(@[
        @"const canCreateJSHandle = typeof window.webkit?.createJSHandle === 'function'",
        @"browser.test.assertTrue(canCreateJSHandle, 'createJSHandle should be available in an extension frame')",

        @"const handle = canCreateJSHandle ? window.webkit.createJSHandle(document.documentElement) : null",
        @"browser.test.assertTrue(!!handle, 'createJSHandle should return a handle for an element')",

        @"browser.test.notifyPass()"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"const iframe = document.createElement('iframe')",
        @"iframe.src = browser.runtime.getURL('extension-frame.html')",
        @"document.body.appendChild(iframe)"
    ]);

    auto *resources = @{
        @"background.js": @"// This script is intentionally left blank.",
        @"content.js": contentScript,
        @"extension-frame.js": frameScript,
        @"extension-frame.html": @"<script src='extension-frame.js'></script>"
    };

    auto manager = Util::loadExtension(frameManifest, resources);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPITest, JSHandleCreationUnavailableInWebPage)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body><script>document.body.dataset.canCreateJSHandle = String(typeof window.webkit?.createJSHandle === 'function')</script></body>"_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *contentScript = Util::constructScript(@[
        @"browser.test.assertEq(document.body.dataset.canCreateJSHandle, 'false', 'createJSHandle should not be available to the web page')",

        @"browser.test.notifyPass()"
    ]);

    auto *resources = @{
        @"background.js": @"// This script is intentionally left blank.",
        @"content.js": contentScript
    };

    RetainPtr extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources]);
    RetainPtr manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager load];

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPITest, JSHandleCreationUnavailableInWebPageFrameInExtensionPage)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<script>parent.postMessage(typeof window.webkit?.createJSHandle, '*')</script>"_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"const iframe = document.createElement('iframe')",
        [NSString stringWithFormat:@"iframe.src = 'http://localhost:%d/'", server.port()],

        @"window.addEventListener('message', (event) => {",
        @"  browser.test.assertEq(event.data, 'undefined', 'createJSHandle should not be available to a web page frame')",

        @"  browser.test.notifyPass()",
        @"})",

        @"document.body.appendChild(iframe)"
    ]);

    Util::loadAndRunExtension(manifest, @{
        @"background.js": backgroundScript
    });
}

TEST(WKWebExtensionAPITest, MessageEventWithSendMessageReply)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.onMessage.addListener((message, data) => {",
        @"  browser.test.assertEq(message, 'Test', 'message should be')",
        @"  browser.test.assertEq(data, undefined, 'data should be')",

        @"  browser.test.sendMessage('Received')",
        @"})",

        @"browser.test.sendMessage('Ready')",
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager runUntilTestMessage:@"Ready"];
    [manager sendTestMessage:@"Test"];
    [manager runUntilTestMessage:@"Received"];
}

TEST(WKWebExtensionAPITest, SendMessage)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.sendMessage('Test', { key: 'value' });"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    id receivedMessage = [manager runUntilTestMessage:@"Test"];
    EXPECT_NS_EQUAL(receivedMessage, @{ @"key": @"value" });
}

TEST(WKWebExtensionAPITest, SendMessageMultipleTimes)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.sendMessage('Test', { key: 'One' });",
        @"browser.test.sendMessage('Test', { key: 'Two' });",
        @"browser.test.sendMessage('Test', { key: 'Three' });"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    id firstMessage = [manager runUntilTestMessage:@"Test"];
    EXPECT_NS_EQUAL(firstMessage, @{ @"key": @"One" });

    id secondMessage = [manager runUntilTestMessage:@"Test"];
    EXPECT_NS_EQUAL(secondMessage, @{ @"key": @"Two" });

    id thirdMessage = [manager runUntilTestMessage:@"Test"];
    EXPECT_NS_EQUAL(thirdMessage, @{ @"key": @"Three" });
}

TEST(WKWebExtensionAPITest, SendMessageOutOfOrder)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.sendMessage('Message 1', { key: 'One' });",
        @"browser.test.sendMessage('Message 2', { key: 'Two' });",
        @"browser.test.sendMessage('Message 3', { key: 'Three' });"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    id secondMessage = [manager runUntilTestMessage:@"Message 2"];
    EXPECT_NS_EQUAL(secondMessage, @{ @"key": @"Two" });

    id thirdMessage = [manager runUntilTestMessage:@"Message 3"];
    EXPECT_NS_EQUAL(thirdMessage, @{ @"key": @"Three" });

    id firstMessage = [manager runUntilTestMessage:@"Message 1"];
    EXPECT_NS_EQUAL(firstMessage, @{ @"key": @"One" });
}

TEST(WKWebExtensionAPITest, SendMessageBeforeListenerAdded)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto testMessage = @"Queued test message";

    auto *contentScript = Util::constructScript(@[
        @"browser.test.onMessage.addListener((message, data) => {",
        [NSString stringWithFormat:@"  browser.test.assertEq(message, '%@')", testMessage],
        @"  browser.test.notifyPass()",
        @"})",
    ]);

    auto *resources = @{
        @"background.js": @"// This script is intentionally left blank.",
        @"content.js": contentScript
    };

    RetainPtr extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources]);
    RetainPtr manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager load];

    [manager sendTestMessage:testMessage];

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPITest, AddAnonymousAsyncTest)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertRejects(browser.test.addTest(async () => {",
        @"  browser.test.assertTrue(true)",
        @"}))",
        @"  .then(() => browser.test.notifyPass())",
        @"  .catch(() => browser.test.notifyFail('Passing an anonymous function into addTest resolved the promise.'))"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager run];

    EXPECT_NS_EQUAL(manager.get().testsAdded, @[ ]);
    EXPECT_NS_EQUAL(manager.get().testsStarted, @[ ]);
    EXPECT_NS_EQUAL(manager.get().testResults, @{ });
}

TEST(WKWebExtensionAPITest, AddAsyncTestThatPasses)
{
    auto *testName = @"passingTest";
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertResolves(browser.test.addTest(async function passingTest() {",
        @"  browser.test.assertTrue(true)",
        @"}))",
        @"  .then(() => browser.test.notifyPass())",
        @"  .catch(() => browser.test.notifyFail('A passing assertion in the addTest method rejected the promise.'))"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager run];

    EXPECT_NS_EQUAL(manager.get().testsAdded, @[ testName ]);
    EXPECT_NS_EQUAL(manager.get().testsStarted, @[ testName ]);
    EXPECT_NS_EQUAL(manager.get().testResults, @{ testName: @YES });
}

TEST(WKWebExtensionAPITest, AddAsyncTestThatFails)
{
    auto *testName = @"failingTest";
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertRejects(browser.test.addTest(async function failingTest() {",
        @"  browser.test.assertTrue(false)",
        @"}))",
        @"  .then(() => browser.test.notifyPass())",
        @"  .catch(() => browser.test.notifyFail('A failing assertion in the addTest method resolved the promise.'))"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager run];

    EXPECT_NS_EQUAL(manager.get().testsAdded, @[ testName ]);
    EXPECT_NS_EQUAL(manager.get().testsStarted, @[ testName ]);
    EXPECT_NS_EQUAL(manager.get().testResults, @{ testName: @NO });
}

TEST(WKWebExtensionAPITest, AddAsyncTestThatThrows)
{
    auto *testName = @"failingTest";
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertRejects(browser.test.addTest(async function failingTest() {",
        @"  throw new Error('fail the test')",
        @"}))",
        @"  .then(() => browser.test.notifyPass())",
        @"  .catch(() => browser.test.notifyFail('Throwing an error in the addTest method resolved the promise.'))"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager run];

    EXPECT_NS_EQUAL(manager.get().testsAdded, @[ testName ]);
    EXPECT_NS_EQUAL(manager.get().testsStarted, @[ testName ]);
    EXPECT_NS_EQUAL(manager.get().testResults, @{ testName: @0 });
}

TEST(WKWebExtensionAPITest, AddMultipleAsyncTestsThatPass)
{
    auto *testNames = @[ @"testA", @"testB" ];
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertResolves(browser.test.addTest(async function testA() {",
        @"  browser.test.assertTrue(true)",
        @"}))",
        @"  .catch(() => browser.test.notifyFail('A passing assertion in the addTest method rejected the promise.'))",

        @"browser.test.assertResolves(browser.test.addTest(async function testB() {",
        @"  browser.test.assertTrue(true)",
        @"}))",
        @"  .then(() => browser.test.notifyPass())",
        @"  .catch(() => browser.test.notifyFail('A passing assertion in the addTest method rejected the promise.'))"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager run];

    EXPECT_NS_EQUAL(manager.get().testsAdded, testNames);
    EXPECT_NS_EQUAL(manager.get().testsStarted, testNames);
    EXPECT_NS_EQUAL(manager.get().testResults, (@{ testNames.firstObject: @YES, testNames.lastObject: @YES }));
}

TEST(WKWebExtensionAPITest, AddMultipleAsyncTestsWithFailure)
{
    auto *testNames = @[ @"testA", @"testB" ];
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertRejects(browser.test.addTest(async function testA() {",
        @"  browser.test.assertTrue(false)",
        @"}))",
        @"  .catch(() => browser.test.notifyFail('A failing assertion in the addTest method resolved the promise.'))",

        @"browser.test.assertResolves(browser.test.addTest(async function testB() {",
        @"  browser.test.assertTrue(true)",
        @"}))",
        @"  .then(() => browser.test.notifyPass())",
        @"  .catch(() => browser.test.notifyFail('A passing assertion in the addTest method rejected the promise.'))"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager run];

    EXPECT_NS_EQUAL(manager.get().testsAdded, testNames);
    EXPECT_NS_EQUAL(manager.get().testsStarted, testNames);
    EXPECT_NS_EQUAL(manager.get().testResults, (@{ testNames.firstObject: @NO, testNames.lastObject: @YES }));
}

TEST(WKWebExtensionAPITest, AddAnonymousTest)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertRejects(browser.test.addTest(() => {",
        @"  browser.test.assertTrue(true)",
        @"}))",
        @"  .then(() => browser.test.notifyPass())",
        @"  .catch(() => browser.test.notifyFail('Passing an anonymous function into addTest resolved the promise.'))"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager run];

    EXPECT_NS_EQUAL(manager.get().testsAdded, @[ ]);
    EXPECT_NS_EQUAL(manager.get().testsStarted, @[ ]);
    EXPECT_NS_EQUAL(manager.get().testResults, @{ });
}

TEST(WKWebExtensionAPITest, AddTestThatPasses)
{
    auto *testName = @"passingTest";
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertResolves(browser.test.addTest(function passingTest() {",
        @"  browser.test.assertTrue(true)",
        @"}))",
        @"  .then(() => browser.test.notifyPass())",
        @"  .catch(() => browser.test.notifyFail('A passing assertion in the addTest method rejected the promise.'))"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager run];

    EXPECT_NS_EQUAL(manager.get().testsAdded, @[ testName ]);
    EXPECT_NS_EQUAL(manager.get().testsStarted, @[ testName ]);
    EXPECT_NS_EQUAL(manager.get().testResults, @{ testName: @YES });
}

TEST(WKWebExtensionAPITest, AddTestThatFails)
{
    auto *testName = @"failingTest";
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertRejects(browser.test.addTest(function failingTest() {",
        @"  browser.test.assertTrue(false)",
        @"}))",
        @"  .then(() => browser.test.notifyPass())",
        @"  .catch(() => browser.test.notifyFail('A failing assertion in the addTest method resolved the promise.'))"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager run];

    EXPECT_NS_EQUAL(manager.get().testsAdded, @[ testName ]);
    EXPECT_NS_EQUAL(manager.get().testsStarted, @[ testName ]);
    EXPECT_NS_EQUAL(manager.get().testResults, @{ testName: @NO });
}

TEST(WKWebExtensionAPITest, AddTestThatThrows)
{
    auto *testName = @"failingTest";
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertRejects(browser.test.addTest(function failingTest() {",
        @"  throw new Error('fail the test')",
        @"}))",
        @"  .then(() => browser.test.notifyPass())",
        @"  .catch(() => browser.test.notifyFail('Throwing an error in the addTest method resolved the promise.'))"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager run];

    EXPECT_NS_EQUAL(manager.get().testsAdded, @[ testName ]);
    EXPECT_NS_EQUAL(manager.get().testsStarted, @[ testName ]);
    EXPECT_NS_EQUAL(manager.get().testResults, @{ testName: @0 });
}

TEST(WKWebExtensionAPITest, AddMultipleTestsThatPass)
{
    auto *testNames = @[ @"testA", @"testB" ];
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertResolves(browser.test.addTest(function testA() {",
        @"  browser.test.assertTrue(true)",
        @"}))",
        @"  .catch(() => browser.test.notifyFail('A passing assertion in the addTest method rejected the promise.'))",

        @"browser.test.assertResolves(browser.test.addTest(function testB() {",
        @"  browser.test.assertTrue(true)",
        @"}))",
        @"  .then(() => browser.test.notifyPass())",
        @"  .catch(() => browser.test.notifyFail('A passing assertion in the addTest method rejected the promise.'))"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager run];

    EXPECT_NS_EQUAL(manager.get().testsAdded, testNames);
    EXPECT_NS_EQUAL(manager.get().testsStarted, testNames);
    EXPECT_NS_EQUAL(manager.get().testResults, (@{ testNames.firstObject: @YES, testNames.lastObject: @YES }));
}

TEST(WKWebExtensionAPITest, AddMultipleTestsWithFailure)
{
    auto *testNames = @[ @"testA", @"testB" ];
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertRejects(browser.test.addTest(function testA() {",
        @"  browser.test.assertTrue(false)",
        @"}))",
        @"  .catch(() => browser.test.notifyFail('A failing assertion in the addTest method resolved the promise.'))",

        @"browser.test.assertResolves(browser.test.addTest(function testB() {",
        @"  browser.test.assertTrue(true)",
        @"}))",
        @"  .then(() => browser.test.notifyPass())",
        @"  .catch(() => browser.test.notifyFail('A passing assertion in the addTest method rejected the promise.'))"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager run];

    EXPECT_NS_EQUAL(manager.get().testsAdded, testNames);
    EXPECT_NS_EQUAL(manager.get().testsStarted, testNames);
    EXPECT_NS_EQUAL(manager.get().testResults, (@{ testNames.firstObject: @NO, testNames.lastObject: @YES }));
}

TEST(WKWebExtensionAPITest, RunAnonymousTests)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertRejects(browser.test.runTests([",
        @"  async () => {",
        @"    browser.test.assertTrue(true)",
        @"  },",
        @"  () => {",
        @"    browser.test.assertTrue(true)",
        @"  }",
        @"]))",
        @"  .then(() => browser.test.notifyPass())",
        @"  .catch(() => browser.test.notifyFail('Passing an anonymous function into runTests resolved the promise.'))"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager run];

    EXPECT_NS_EQUAL(manager.get().testsAdded, @[ ]);
    EXPECT_NS_EQUAL(manager.get().testsStarted, @[ ]);
    EXPECT_NS_EQUAL(manager.get().testResults, @{ });
}

TEST(WKWebExtensionAPITest, RunTestsThatPass)
{
    auto *testNames = @[ @"testA", @"testB" ];
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertResolves(browser.test.runTests([",
        @"  function testA() {",
        @"    browser.test.assertTrue(true)",
        @"  },",
        @"  async function testB() {",
        @"    browser.test.assertTrue(true)",
        @"  }",
        @"]))",
        @"  .then(() => browser.test.notifyPass())",
        @"  .catch(() => browser.test.notifyFail('All passing tests passed into runTests rejected the promise.'))"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager run];

    EXPECT_NS_EQUAL(manager.get().testsAdded, testNames);
    EXPECT_NS_EQUAL(manager.get().testsStarted, testNames);
    EXPECT_NS_EQUAL(manager.get().testResults, (@{ testNames.firstObject: @YES, testNames.lastObject: @YES }));
}

TEST(WKWebExtensionAPITest, RunTestsWithTestThatFails)
{
    auto *testNames = @[ @"testA", @"testB" ];
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertRejects(browser.test.runTests([",
        @"  function testA() {",
        @"    browser.test.assertTrue(false)",
        @"  },",
        @"  async function testB() {",
        @"    browser.test.assertTrue(true)",
        @"  }",
        @"]))",
        @"  .then(() => browser.test.notifyPass())",
        @"  .catch(() => browser.test.notifyFail('A failing test passed into runTests resolved the promise.'))"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager run];

    EXPECT_NS_EQUAL(manager.get().testsAdded, testNames);
    EXPECT_NS_EQUAL(manager.get().testsStarted, testNames);
    EXPECT_NS_EQUAL(manager.get().testResults, (@{ testNames.firstObject: @NO, testNames.lastObject: @YES }));
}

TEST(WKWebExtensionAPITest, RunTestsWithAsyncTestThatFails)
{
    auto *testNames = @[ @"testA", @"testB" ];
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertRejects(browser.test.runTests([",
        @"  function testA() {",
        @"    browser.test.assertTrue(true)",
        @"  },",
        @"  async function testB() {",
        @"    browser.test.assertTrue(false)",
        @"  }",
        @"]))",
        @"  .then(() => browser.test.notifyPass())",
        @"  .catch(() => browser.test.notifyFail('A failing async test passed into runTests resolved the promise.'))"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager run];

    EXPECT_NS_EQUAL(manager.get().testsAdded, testNames);
    EXPECT_NS_EQUAL(manager.get().testsStarted, testNames);
    EXPECT_NS_EQUAL(manager.get().testResults, (@{ testNames.firstObject: @YES, testNames.lastObject: @NO }));
}

TEST(WKWebExtensionAPITest, RunTestsVerifyFailedTestAborts)
{
    auto *testNames = @[ @"testAssertTrue", @"testAssertFalse", @"testAssertEq", @"testAssertDeepEq", @"testAssertThrows" ];
    auto *backgroundScript = Util::constructScript(@[
        @"function testAssertTrue() {",
        @"  browser.test.assertTrue(false)",
        @"  browser.test.notifyFail()",
        @"}",

        @"function testAssertFalse() {",
        @"  browser.test.assertFalse(true)",
        @"  browser.test.notifyFail()",
        @"}",

        @"function testAssertEq() {",
        @"  browser.test.assertEq(false, 4)",
        @"  browser.test.notifyFail()",
        @"}",

        @"function testAssertDeepEq() {",
        @"  browser.test.assertDeepEq({ 'key': 'value' }, { 'key2': 'value2' })",
        @"  browser.test.notifyFail()",
        @"}",

        @"function testAssertThrows() {",
        @"  browser.test.assertThrows(() => browser.permissions.getAll())",
        @"  browser.test.notifyFail()",
        @"}",

        @"browser.test.assertRejects(browser.test.runTests([",
        @"  testAssertTrue, testAssertFalse, testAssertEq, testAssertDeepEq, testAssertThrows",
        @"]))",
        @"  .then(() => browser.test.notifyPass())",
        @"  .catch(() => browser.test.notifyFail('Test(s) with failing assertions resolved the promise.'))"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager run];

    EXPECT_EQ(manager.get().testResults.count, testNames.count);
}

TEST(WKWebExtensionAPITest, AssertionsThatPass)
{
    auto *testNames = @[
        @"assertSafe",
        @"assertSafeResolveWithResolvedPromise",
        @"assertSafeResolveWithNonPromiseResult",
        @"assertResolves",
        @"assertRejects",
        @"assertRejectsWithMatchingError",
        @"assertThrows"
    ];

    auto *backgroundScript = Util::constructScript(@[
        @"const tests = [",

        @"  browser.test.addTest(async function assertSafe() {",
        @"    const result = browser.test.assertSafe(() => 'Plain String')",
        @"    browser.test.assertEq(result, 'Plain String')",
        @"  }),",

        @"  browser.test.addTest(async function assertSafeResolveWithResolvedPromise() {",
        @"    const result = await browser.test.assertSafeResolve(() => Promise.resolve({ key: 'Value' }))",
        @"    browser.test.assertDeepEq(result, { key: 'Value' })",
        @"  }),",

        @"  browser.test.addTest(async function assertSafeResolveWithNonPromiseResult() {",
        @"    const result = await browser.test.assertSafeResolve(() => ({ key: 'Value' }))",
        @"    browser.test.assertDeepEq(result, { key: 'Value' })",
        @"  }),",

        @"  browser.test.addTest(async function assertResolves() {",
        @"    const result = await browser.test.assertResolves(Promise.resolve({ key: 'Value' }))",
        @"    browser.test.assertDeepEq(result, { key: 'Value' })",
        @"  }),",

        @"  browser.test.addTest(async function assertRejects() {",
        @"    await browser.test.assertRejects(Promise.reject(new Error('Expected Error')))",
        @"  }),",

        @"  browser.test.addTest(async function assertRejectsWithMatchingError() {",
        @"    await browser.test.assertRejects(Promise.reject(new Error('Expected Error')), /Expected/)",
        @"  }),",

        @"  browser.test.addTest(async function assertThrows() {",
        @"    browser.test.assertThrows(() => { throw new Error('Expected Error') })",
        @"  })",
        @"]",

        @"Promise.allSettled(tests).then(() => browser.test.notifyPass())"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager run];

    EXPECT_NS_EQUAL(manager.get().testsAdded, testNames);
    EXPECT_EQ(manager.get().testResults.count, testNames.count);

    for (NSString *testName in testNames)
        EXPECT_TRUE([manager.get().testResults[testName] boolValue]) << testName.UTF8String << " should have passed";
}

TEST(WKWebExtensionAPITest, AssertionsThatFail)
{
    auto *testNames = @[
        @"assertSafeWithThrowingFunction",
        @"assertSafeResolveWithRejectedPromise",
        @"assertSafeResolveWithRejectedAsyncFunction",
        @"assertSafeResolveWithThrowingFunction",
        @"assertResolvesWithRejectedPromise",
        @"assertRejectsWithResolvedPromise",
        @"assertRejectsWithMismatchedError",
        @"assertThrowsWithoutThrowing"
    ];

    // The catch blocks hide the rejection that await rethrows, so these only fail if the assertion reports it itself.
    auto *backgroundScript = Util::constructScript(@[
        @"const tests = [",

        @"  browser.test.addTest(async function assertSafeWithThrowingFunction() {",
        @"    browser.test.assertSafe(() => { throw new Error('Expected Error') })",
        @"  }),",

        @"  browser.test.addTest(async function assertSafeResolveWithRejectedPromise() {",
        @"    try {",
        @"      await browser.test.assertSafeResolve(() => Promise.reject(new Error('Expected Error')))",
        @"    } catch (error) { }",
        @"  }),",

        @"  browser.test.addTest(async function assertSafeResolveWithRejectedAsyncFunction() {",
        @"    try {",
        @"      await browser.test.assertSafeResolve(async () => { throw new Error('Expected Error') })",
        @"    } catch (error) { }",
        @"  }),",

        @"  browser.test.addTest(async function assertSafeResolveWithThrowingFunction() {",
        @"    browser.test.assertSafeResolve(() => { throw new Error('Expected Error') })",
        @"  }),",

        @"  browser.test.addTest(async function assertResolvesWithRejectedPromise() {",
        @"    try {",
        @"      await browser.test.assertResolves(Promise.reject(new Error('Expected Error')))",
        @"    } catch (error) { }",
        @"  }),",

        @"  browser.test.addTest(async function assertRejectsWithResolvedPromise() {",
        @"    try {",
        @"      await browser.test.assertRejects(Promise.resolve({ key: 'Value' }))",
        @"    } catch (error) { }",
        @"  }),",

        @"  browser.test.addTest(async function assertRejectsWithMismatchedError() {",
        @"    try {",
        @"      await browser.test.assertRejects(Promise.reject(new Error('Other Error')), /Expected/)",
        @"    } catch (error) { }",
        @"  }),",

        @"  browser.test.addTest(async function assertThrowsWithoutThrowing() {",
        @"    browser.test.assertThrows(() => 'Plain String')",
        @"  })",
        @"]",

        @"Promise.allSettled(tests).then(() => browser.test.notifyPass())"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager run];

    EXPECT_NS_EQUAL(manager.get().testsAdded, testNames);
    EXPECT_EQ(manager.get().testResults.count, testNames.count);

    for (NSString *testName in testNames)
        EXPECT_FALSE([manager.get().testResults[testName] boolValue]) << testName.UTF8String << " should have failed";
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
