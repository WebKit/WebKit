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
#import "TestCocoa.h"
#import "TestNavigationDelegate.h"
#import "WebExtensionUtilities.h"
#import <WebKit/WKFoundation.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/_WKWebExtensionContextPrivate.h>
#import <WebKit/_WKWebExtensionControllerPrivate.h>
#import <WebKit/_WKWebExtensionPrivate.h>

namespace TestWebKitAPI {

TEST(WKWebExtensionController, Configuration)
{
    _WKWebExtensionController *testController = [[_WKWebExtensionController alloc] init];
    EXPECT_TRUE(testController.configuration.persistent);
    EXPECT_NULL(testController.configuration.identifier);

    testController = [[_WKWebExtensionController alloc] initWithConfiguration:_WKWebExtensionControllerConfiguration.nonPersistentConfiguration];
    EXPECT_FALSE(testController.configuration.persistent);
    EXPECT_NULL(testController.configuration.identifier);

    NSUUID *identifier = [NSUUID UUID];
    _WKWebExtensionControllerConfiguration *configuration = [_WKWebExtensionControllerConfiguration configurationWithIdentifier:identifier];

    testController = [[_WKWebExtensionController alloc] initWithConfiguration:configuration];
    EXPECT_TRUE(testController.configuration.persistent);
    EXPECT_NS_EQUAL(testController.configuration.identifier, identifier);
}

TEST(WKWebExtensionController, LoadingAndUnloadingContexts)
{
    _WKWebExtensionController *testController = [[_WKWebExtensionController alloc] init];

    EXPECT_EQ(testController.extensions.count, 0ul);
    EXPECT_EQ(testController.extensionContexts.count, 0ul);

    _WKWebExtension *testExtensionOne = [[_WKWebExtension alloc] _initWithManifestDictionary:@{ @"manifest_version": @2, @"name": @"Test One", @"description": @"Test One", @"version": @"1.0" }];
    _WKWebExtensionContext *testContextOne = [[_WKWebExtensionContext alloc] initWithExtension:testExtensionOne];

    EXPECT_NULL(testExtensionOne.errors);
    EXPECT_FALSE(testContextOne.loaded);
    EXPECT_NULL([testController extensionContextForExtension:testExtensionOne]);

    _WKWebExtension *testExtensionTwo = [[_WKWebExtension alloc] _initWithManifestDictionary:@{ @"manifest_version": @2, @"name": @"Test Two", @"description": @"Test Two", @"version": @"1.0" }];
    _WKWebExtensionContext *testContextTwo = [[_WKWebExtensionContext alloc] initWithExtension:testExtensionTwo];

    EXPECT_NULL(testExtensionTwo.errors);
    EXPECT_FALSE(testContextTwo.loaded);
    EXPECT_NULL([testController extensionContextForExtension:testExtensionTwo]);

    NSError *error;
    EXPECT_TRUE([testController loadExtensionContext:testContextOne error:&error]);
    EXPECT_NULL(error);

    EXPECT_NULL(testExtensionOne.errors);
    EXPECT_TRUE(testContextOne.loaded);

    EXPECT_EQ(testController.extensions.count, 1ul);
    EXPECT_EQ(testController.extensionContexts.count, 1ul);

    EXPECT_FALSE([testController loadExtensionContext:testContextOne error:&error]);
    EXPECT_NOT_NULL(error);
    EXPECT_NS_EQUAL(error.domain, _WKWebExtensionContextErrorDomain);
    EXPECT_EQ(error.code, _WKWebExtensionContextErrorAlreadyLoaded);

    EXPECT_NULL(testExtensionOne.errors);
    EXPECT_TRUE(testContextOne.loaded);

    EXPECT_EQ(testController.extensions.count, 1ul);
    EXPECT_EQ(testController.extensionContexts.count, 1ul);

    EXPECT_TRUE([testController loadExtensionContext:testContextTwo error:&error]);
    EXPECT_NULL(error);

    EXPECT_NULL(testExtensionTwo.errors);
    EXPECT_TRUE(testContextTwo.loaded);

    EXPECT_EQ(testController.extensions.count, 2ul);
    EXPECT_EQ(testController.extensionContexts.count, 2ul);

    EXPECT_TRUE([testController unloadExtensionContext:testContextOne error:&error]);
    EXPECT_NULL(error);

    EXPECT_NULL(testExtensionTwo.errors);
    EXPECT_FALSE(testContextOne.loaded);

    EXPECT_EQ(testController.extensions.count, 1ul);
    EXPECT_EQ(testController.extensionContexts.count, 1ul);

    EXPECT_TRUE([testController unloadExtensionContext:testContextTwo error:&error]);
    EXPECT_NULL(error);

    EXPECT_NULL(testExtensionTwo.errors);
    EXPECT_FALSE(testContextTwo.loaded);

    EXPECT_EQ(testController.extensions.count, 0ul);
    EXPECT_EQ(testController.extensionContexts.count, 0ul);

    EXPECT_FALSE([testController unloadExtensionContext:testContextOne error:&error]);
    EXPECT_NOT_NULL(error);
    EXPECT_NS_EQUAL(error.domain, _WKWebExtensionContextErrorDomain);
    EXPECT_EQ(error.code, _WKWebExtensionContextErrorNotLoaded);

    EXPECT_NULL(testExtensionTwo.errors);
    EXPECT_FALSE(testContextOne.loaded);
}

TEST(WKWebExtensionController, BackgroundPageLoading)
{
    NSDictionary *resources = @{
        @"background.html": @"<body>Hello world!</body>",
        @"background.js": @"console.log('Hello World!')"
    };

    NSMutableDictionary *manifest = [@{ @"manifest_version": @2, @"name": @"Test One", @"description": @"Test One", @"version": @"1.0", @"background": @{ @"page": @"background.html", @"persistent": @NO } } mutableCopy];

    _WKWebExtension *testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources];
    _WKWebExtensionContext *testContext = [[_WKWebExtensionContext alloc] initWithExtension:testExtension];
    _WKWebExtensionController *testController = [[_WKWebExtensionController alloc] init];

    EXPECT_NULL(testExtension.errors);

    NSError *error;
    EXPECT_TRUE([testController loadExtensionContext:testContext error:&error]);
    EXPECT_NULL(error);

    // Wait for the background to load.
    TestWebKitAPI::Util::runFor(4_s);

    // No errors means success.
    EXPECT_NULL(testExtension.errors);

    EXPECT_TRUE([testController unloadExtensionContext:testContext error:&error]);
    EXPECT_NULL(error);

    EXPECT_NULL(testExtension.errors);

    manifest[@"background"] = @{ @"service_worker": @"background.js" };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources];
    testContext = [[_WKWebExtensionContext alloc] initWithExtension:testExtension];

    EXPECT_NULL(testExtension.errors);

    EXPECT_TRUE([testController loadExtensionContext:testContext error:&error]);
    EXPECT_NULL(error);

    // Wait for the background to load.
    TestWebKitAPI::Util::runFor(4_s);

    // No errors means success.
    EXPECT_NULL(testExtension.errors);

    EXPECT_TRUE([testController unloadExtensionContext:testContext error:&error]);
    EXPECT_NULL(error);

    EXPECT_NULL(testExtension.errors);

    testContext.baseURL = [NSURL URLWithString:@"test-extension://aaabbbcccddd"];

    EXPECT_TRUE([testController loadExtensionContext:testContext error:&error]);
    EXPECT_NULL(error);

    // Wait for the background to load.
    TestWebKitAPI::Util::runFor(4_s);

    // No errors means success.
    EXPECT_NULL(testExtension.errors);

    EXPECT_TRUE([testController unloadExtensionContext:testContext error:&error]);
    EXPECT_NULL(error);

    EXPECT_NULL(testExtension.errors);
}

TEST(WKWebExtensionController, BackgroundPageWithModulesLoading)
{
    NSDictionary *resources = @{
        @"main.js": @"import { x } from './exports.js'; x;",
        @"exports.js": @"const x = 805; export { x };",
    };

    NSMutableDictionary *manifest = [@{ @"manifest_version": @2, @"name": @"Test One", @"description": @"Test One", @"version": @"1.0", @"background": @{ @"scripts": @[ @"main.js", @"exports.js" ], @"type": @"module", @"persistent": @NO } } mutableCopy];

    _WKWebExtension *testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources];
    _WKWebExtensionContext *testContext = [[_WKWebExtensionContext alloc] initWithExtension:testExtension];
    _WKWebExtensionController *testController = [[_WKWebExtensionController alloc] init];

    EXPECT_NULL(testExtension.errors);

    NSError *error;
    EXPECT_TRUE([testController loadExtensionContext:testContext error:&error]);
    EXPECT_NULL(error);

    // Wait for the background to load.
    TestWebKitAPI::Util::runFor(4_s);

    // No errors means success.
    EXPECT_NULL(testExtension.errors);

    EXPECT_TRUE([testController unloadExtensionContext:testContext error:&error]);
    EXPECT_NULL(error);

    EXPECT_NULL(testExtension.errors);

    manifest[@"background"] = @{ @"service_worker": @"main.js", @"type": @"module" };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources];
    testContext = [[_WKWebExtensionContext alloc] initWithExtension:testExtension];

    EXPECT_NULL(testExtension.errors);

    EXPECT_TRUE([testController loadExtensionContext:testContext error:&error]);
    EXPECT_NULL(error);

    // Wait for the background to load.
    TestWebKitAPI::Util::runFor(4_s);

    // No errors means success.
    EXPECT_NULL(testExtension.errors);
}

TEST(WKWebExtensionController, ContentScriptLoading)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "Hello World"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Https);

    auto *manifest = @{ @"manifest_version": @3, @"content_scripts": @[ @{ @"js": @[ @"content.js" ], @"matches": @[ @"*://localhost/*" ] } ] };

    auto *contentScript = Util::constructScript(@[
        // Exposed to content scripts
        @"browser.test.assertEq(typeof browser.runtime.id, 'string')",
        @"browser.test.assertEq(typeof browser.runtime.getManifest(), 'object')",
        @"browser.test.assertEq(typeof browser.runtime.getURL(''), 'string')",

        // Not exposed to content scripts
        @"browser.test.assertEq(browser.runtime.getPlatformInfo, undefined)",
        @"browser.test.assertEq(browser.runtime.lastError, undefined)",

        // Finish
        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:@{ @"content.js": contentScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initWithExtension:extension.get()]);

    _WKWebExtensionMatchPattern *matchPattern = [_WKWebExtensionMatchPattern matchPatternWithString:@"*://localhost/*"];
    [manager.get().context setPermissionState:_WKWebExtensionContextPermissionStateGrantedExplicitly forMatchPattern:matchPattern];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get()._webExtensionController = manager.get().controller;

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);

    navigationDelegate.get().didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    };

    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:server.requestWithLocalhost()];

    [manager loadAndRun];
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
