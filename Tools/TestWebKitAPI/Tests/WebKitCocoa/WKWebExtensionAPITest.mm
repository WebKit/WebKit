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

#import "HTTPServer.h"
#import "WebExtensionUtilities.h"

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

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

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

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager load];

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager runUntilTestMessage:@"Ready for Message"];

    [manager sendTestMessage:@"Test" withArgument:@{ @"key": @"value" }];

    [manager run];
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

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

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

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
