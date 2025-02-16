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

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
