/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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

#import "TestCocoa.h"
#import "TestWKWebView.h"
#import "TestWebExtensionsDelegate.h"
#import "WebExtensionUtilities.h"
#import <WebCore/UserGestureIndicator.h>
#import <WebKit/WKFoundation.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/_WKWebExtensionContextPrivate.h>
#import <WebKit/_WKWebExtensionControllerDelegate.h>

namespace TestWebKitAPI {

static void runScriptWithUserGesture(const String& script, WKWebView *backgroundWebView)
{
    ASSERT(backgroundWebView);

    bool callbackComplete = false;
    RetainPtr<id> evalResult;
    [backgroundWebView callAsyncJavaScript:script arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:[&] (id result, NSError *error) {
        evalResult = result;
        callbackComplete = true;
        EXPECT_TRUE(!error);
        if (error)
            NSLog(@"Encountered error: %@ while evaluating script: %@", error, static_cast<NSString *>(script));
    }];

    TestWebKitAPI::Util::run(&callbackComplete);
}

TEST(WKWebExtensionAPIPermissions, Errors)
{
    auto *manifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"alarms", @"activeTab" ],
        @"optional_permissions": @[ @"webNavigation", @"cookies" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"host_permissions": @[ @"*://*.apple.com/*" ]
    };

    auto *backgroundScript = Util::constructScript(@[
        @"await browser.test.assertRejects(browser.permissions.remove({ permissions: ['webRequest'] }), /only permissions specified in the manifest may be removed/i)",
        @"await browser.test.assertRejects(browser.permissions.remove({ origins: ['https://example.com/'] }), /only permissions specified in the manifest may be removed/i)",

        @"await browser.test.assertRejects(browser.permissions.remove({ permissions: ['alarms'] }), /required permissions cannot be removed/i)",
        @"await browser.test.assertRejects(browser.permissions.remove({ origins: ['*://*.apple.com/*'] }), /required permissions cannot be removed/i)",

        @"browser.test.assertThrows(() => browser.permissions.contains({ permissions: ['bad'] }), /'bad' is not a valid permission/i)",
        @"browser.test.assertThrows(() => browser.permissions.contains({ origins: ['bad'] }), /'bad' is not a valid pattern/i)",

        @"await browser.test.assertRejects(browser.permissions.request({ permissions: ['cookies'] }), /must be called during a user gesture/i)",

        @"browser.test.assertThrows(() => browser.permissions.contains(null), /'permissions' value is invalid, because an object is expected/i)",
        @"browser.test.assertThrows(() => browser.permissions.contains('string'), /'permissions' value is invalid, because an object is expected/i)",
        @"browser.test.assertThrows(() => browser.permissions.contains(123), /'permissions' value is invalid, because an object is expected/i)",

        @"browser.test.assertThrows(() => browser.permissions.contains({ permissions: 'notAnArray' }), /'permissions' is expected to be an array of strings, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.permissions.contains({ permissions: { name: 'storage' } }), /'permissions' is expected to be an array of strings, but an object was provided/i)",
        @"browser.test.assertThrows(() => browser.permissions.contains({ permissions: 123 }), /'permissions' is expected to be an array of strings, but a number was provided/i)",

        @"browser.test.assertThrows(() => browser.permissions.contains({ origins: 'notAnArray' }), /'origins' is expected to be an array of strings, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.permissions.contains({ origins: { domain: 'https://example.com/' } }), /'origins' is expected to be an array of strings, but an object was provided/i)",
        @"browser.test.assertThrows(() => browser.permissions.contains({ origins: 123 }), /'origins' is expected to be an array of strings, but a number was provided/i)",

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(manifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIPermissions, Basics)
{
    auto *manifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"alarms", @"activeTab" ],
        @"optional_permissions": @[ @"webNavigation", @"cookies" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"host_permissions": @[ @"*://*.apple.com/*" ]
    };

    auto *backgroundScript = Util::constructScript(@[
        // contains() should return true for all named permissions and granted match patterns.
        @"browser.test.assertTrue(await browser.permissions.contains({'permissions': ['alarms', 'activeTab']}))",
        @"browser.test.assertTrue(await browser.permissions.contains({'origins': ['*://webkit.org/*']}))",

        // contains() should return false for non granted match patterns.
        @"browser.test.assertFalse(await browser.permissions.contains({'origins': ['*://apple.com/*']}))",

        // Removing optional permissions should pass.
        @"browser.test.assertTrue(await browser.permissions.remove({'permissions': ['cookies']}))",

        // Removing functional permissions should fail.
        @"await browser.test.assertRejects(browser.permissions.remove({'permissions': ['alarms']}))",
        @"await browser.test.assertRejects(browser.permissions.remove({'permissions': ['alarms', 'activeTab']}))",
        @"await browser.test.assertRejects(browser.permissions.remove({'permissions': ['cookies', 'activeTab']}))",
        @"await browser.test.assertRejects(browser.permissions.remove({'permissions': ['scripting']}))",

        // getALL() should return all named permissions and granted match patterns.
        @"let permissions = {'origins': ['*://webkit.org/*'], 'permissions': ['alarms', 'activeTab']}",
        @"const result = await browser.permissions.getAll()",
        @"browser.test.assertDeepEq(result, permissions)",

        // Finish
        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:@{ @"background.js": backgroundScript }]);

    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Grant "permissions" in the manifest.
    for (NSString *permission in extension.get().requestedPermissions)
        [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:permission];

    // Grant extension access to webkit.org.
    _WKWebExtensionMatchPattern *matchPattern = [_WKWebExtensionMatchPattern matchPatternWithString:@"*://webkit.org/*"];
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get()._webExtensionController = manager.get().controller;

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIPermissions, AcceptPermissionsRequest)
{
    auto *manifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"alarms", @"activeTab" ],
        @"optional_permissions": @[ @"webNavigation", @"cookies", @"declarativeNetRequest", @"*://*.apple.com/*" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"host_permissions": @[ @"*://*.apple.com/*" ]
    };

    auto *backgroundScript = Util::constructScript(@[
        @"window.runTest = async () => {",
        @"  await browser.test.assertTrue(await browser.permissions.request({'permissions': ['declarativeNetRequest'], 'origins': ['*://*.apple.com/*']}))",
        @"}",

        @"browser.test.yield('Ready')",
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get()._webExtensionController = manager.get().controller;

    NSSet<NSString *> *permissions = [NSSet setWithArray:@[ @"declarativeNetRequest" ]];
    NSSet<_WKWebExtensionMatchPattern *> *matchPatterns = [NSSet setWithArray:@[ [_WKWebExtensionMatchPattern matchPatternWithString:@"*://*.apple.com/*" ]]];
    auto requestDelegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);
    __block bool requestComplete = false;

    // Implement the delegate methods that're called when a call to permissions.request() is made.
    requestDelegate.get().promptForPermissions = ^(id<_WKWebExtensionTab> tab, NSSet<NSString *> *requestedPermissions, void (^callback)(NSSet<NSString *> *)) {
        EXPECT_EQ(requestedPermissions.count, permissions.count);
        EXPECT_TRUE([requestedPermissions isEqualToSet:permissions]);
        callback(requestedPermissions);
    };

    requestDelegate.get().promptForPermissionMatchPatterns = ^(id<_WKWebExtensionTab> tab, NSSet<_WKWebExtensionMatchPattern *> *requestedMatchPatterns, void (^callback)(NSSet<_WKWebExtensionMatchPattern *> *)) {
        EXPECT_EQ(requestedMatchPatterns.count, matchPatterns.count);
        EXPECT_TRUE([requestedMatchPatterns isEqualToSet:matchPatterns]);

        dispatch_async(dispatch_get_main_queue(), ^{
            callback(requestedMatchPatterns);
            requestComplete = true;
        });
    };

    manager.get().controllerDelegate = requestDelegate.get();

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Ready");

    runScriptWithUserGesture("runTest()"_s, manager.get().context._backgroundWebView);

    TestWebKitAPI::Util::run(&requestComplete);
}

TEST(WKWebExtensionAPIPermissions, DenyPermissionsRequest)
{
    auto *manifest = @{
        @"manifest_version": @3,
        @"optional_permissions": @[ @"declarativeNetRequest" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"optional_host_permissions": @[ @"*://*.apple.com/*" ]
    };

    auto *backgroundScript = Util::constructScript(@[
        @"window.runTest = async () => {",
        @"  await browser.test.assertFalse(await browser.permissions.request({'permissions': ['declarativeNetRequest'], 'origins': ['*://*.apple.com/*']}))",
        @"}",

        @"browser.test.yield('Ready')",
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get()._webExtensionController = manager.get().controller;

    auto requestDelegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);
    __block bool requestComplete = false;

    // Implement the delegate methods, but don't grant the permissions.
    requestDelegate.get().promptForPermissions = ^(id<_WKWebExtensionTab> tab, NSSet<NSString *> *requestedPermissions, void (^callback)(NSSet<NSString *> *)) {
        callback(NSSet.set);
    };

    requestDelegate.get().promptForPermissionMatchPatterns = ^(id<_WKWebExtensionTab> tab, NSSet<_WKWebExtensionMatchPattern *> *requestedMatchPatterns, void (^callback)(NSSet<_WKWebExtensionMatchPattern *> *)) {
        requestComplete = true;
        callback(NSSet.set);
    };

    manager.get().controllerDelegate = requestDelegate.get();

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Ready");

    runScriptWithUserGesture("runTest()"_s, manager.get().context._backgroundWebView);

    TestWebKitAPI::Util::run(&requestComplete);
}

TEST(WKWebExtensionAPIPermissions, AcceptPermissionsDenyMatchPatternsRequest)
{
    auto *manifest = @{
        @"manifest_version": @3,
        @"optional_permissions": @[ @"declarativeNetRequest" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"optional_host_permissions": @[ @"*://*.apple.com/*" ]
    };

    auto *backgroundScript = Util::constructScript(@[
        @"window.runTest = async () => {",
        @"  await browser.test.assertFalse(await browser.permissions.request({'permissions': ['declarativeNetRequest'], 'origins': ['*://*.apple.com/*']}))",
        @"}",

        @"browser.test.yield('Ready')",
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get()._webExtensionController = manager.get().controller;

    auto requestDelegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);
    __block bool requestComplete = false;

    // Grant the requested permissions.
    requestDelegate.get().promptForPermissions = ^(id<_WKWebExtensionTab> tab, NSSet<NSString *> *requestedPermissions, void (^callback)(NSSet<NSString *> *)) {
        callback(requestedPermissions);
    };

    // Deny the requested match patterns.
    requestDelegate.get().promptForPermissionMatchPatterns = ^(id<_WKWebExtensionTab> tab, NSSet<_WKWebExtensionMatchPattern *> *requestedMatchPatterns, void (^callback)(NSSet<_WKWebExtensionMatchPattern *> *)) {
        requestComplete = true;
        callback(NSSet.set);
    };

    manager.get().controllerDelegate = requestDelegate.get();

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Ready");

    runScriptWithUserGesture("runTest()"_s, manager.get().context._backgroundWebView);

    TestWebKitAPI::Util::run(&requestComplete);
}

TEST(WKWebExtensionAPIPermissions, RequestPermissionsOnly)
{
    auto *manifest = @{
        @"manifest_version": @3,
        @"optional_permissions": @[ @"declarativeNetRequest" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"optional_host_permissions": @[ @"*://*.apple.com/*" ]
    };

    auto *backgroundScript = Util::constructScript(@[
        @"window.runTest = async () => {",
        @"  await browser.test.assertTrue(await browser.permissions.request({'permissions': ['declarativeNetRequest']}))",
        @"}",

        @"browser.test.yield('Ready')",
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get()._webExtensionController = manager.get().controller;

    auto requestDelegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);
    __block bool requestComplete = false;

    // Grant the requested permissions.
    requestDelegate.get().promptForPermissions = ^(id<_WKWebExtensionTab> tab, NSSet<NSString *> *requestedPermissions, void (^callback)(NSSet<NSString *> *)) {
        dispatch_async(dispatch_get_main_queue(), ^{
            requestComplete = true;
            callback(requestedPermissions);
        });
    };

    // Match patterns method should not be called.
    requestDelegate.get().promptForPermissionMatchPatterns = ^(id<_WKWebExtensionTab> tab, NSSet<_WKWebExtensionMatchPattern *> *requestedMatchPatterns, void (^callback)(NSSet<_WKWebExtensionMatchPattern *> *)) {
        ASSERT_NOT_REACHED();
    };

    manager.get().controllerDelegate = requestDelegate.get();

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Ready");

    runScriptWithUserGesture("runTest()"_s, manager.get().context._backgroundWebView);

    TestWebKitAPI::Util::run(&requestComplete);
}

TEST(WKWebExtensionAPIPermissions, RequestMatchPatternsOnly)
{
    auto *manifest = @{
        @"manifest_version": @3,
        @"optional_permissions": @[ @"declarativeNetRequest" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"optional_host_permissions": @[ @"*://*.apple.com/*" ]
    };

    auto *backgroundScript = Util::constructScript(@[
        @"window.runTest = async () => {",
        @"  await browser.test.assertTrue(await browser.permissions.request({'origins': ['*://*.apple.com/*']}))",
        @"}",

        @"browser.test.yield('Ready')",
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get()._webExtensionController = manager.get().controller;

    auto requestDelegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);
    __block bool requestComplete = false;

    // Permissions method should not be called.
    requestDelegate.get().promptForPermissions = ^(id<_WKWebExtensionTab> tab, NSSet<NSString *> *requestedPermissions, void (^callback)(NSSet<NSString *> *)) {
        ASSERT_NOT_REACHED();
    };

    // Grant the requested match patterns.
    requestDelegate.get().promptForPermissionMatchPatterns = ^(id<_WKWebExtensionTab> tab, NSSet<_WKWebExtensionMatchPattern *> *requestedMatchPatterns, void (^callback)(NSSet<_WKWebExtensionMatchPattern *> *)) {
        dispatch_async(dispatch_get_main_queue(), ^{
            requestComplete = true;
            callback(requestedMatchPatterns);
        });
    };

    manager.get().controllerDelegate = requestDelegate.get();

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Ready");

    runScriptWithUserGesture("runTest()"_s, manager.get().context._backgroundWebView);

    TestWebKitAPI::Util::run(&requestComplete);
}

TEST(WKWebExtensionAPIPermissions, GrantOnlySomePermissions)
{
    auto *manifest = @{
        @"manifest_version": @3,
        @"optional_permissions": @[ @"declarativeNetRequest", @"alarms" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"optional_host_permissions": @[ @"*://*.apple.com/*" ]
    };

    auto *backgroundScript = Util::constructScript(@[
        @"window.runTest = async () => {",
        @"  await browser.test.assertFalse(await browser.permissions.request({'permissions': ['alarms', 'declarativeNetRequest']}))",
        @"}",

        @"browser.test.yield('Ready')",
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get()._webExtensionController = manager.get().controller;

    auto requestDelegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);
    __block bool requestComplete = false;

    // Grant the requested permissions.
    requestDelegate.get().promptForPermissions = ^(id<_WKWebExtensionTab> tab, NSSet<NSString *> *requestedPermissions, void (^callback)(NSSet<NSString *> *)) {
        dispatch_async(dispatch_get_main_queue(), ^{
            requestComplete = true;
            callback(requestedPermissions);
        });
    };

    // Match patterns method should not be called.
    requestDelegate.get().promptForPermissionMatchPatterns = ^(id<_WKWebExtensionTab> tab, NSSet<_WKWebExtensionMatchPattern *> *requestedMatchPatterns, void (^callback)(NSSet<_WKWebExtensionMatchPattern *> *)) {
        ASSERT_NOT_REACHED();
    };

    manager.get().controllerDelegate = requestDelegate.get();

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Ready");

    runScriptWithUserGesture("runTest()"_s, manager.get().context._backgroundWebView);

    TestWebKitAPI::Util::run(&requestComplete);
}

TEST(WKWebExtensionAPIPermissions, GrantOnlySomeMatchPatterns)
{
    auto *manifest = @{
        @"manifest_version": @3,
        @"optional_permissions": @[ @"declarativeNetRequest" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"optional_host_permissions": @[ @"*://*.apple.com/*", @"*://*.example.com/*" ]
    };

    auto *backgroundScript = Util::constructScript(@[
        @"window.runTest = async () => {",
        @"  await browser.test.assertFalse(await browser.permissions.request({'origins': ['*://*.apple.com/*', '*://*.example.com/*']}))",
        @"}",

        @"browser.test.yield('Ready')",
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get()._webExtensionController = manager.get().controller;

    auto requestDelegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);
    __block bool requestComplete = false;

    // Permissions method should not be called.
    requestDelegate.get().promptForPermissions = ^(id<_WKWebExtensionTab> tab, NSSet<NSString *> *requestedPermissions, void (^callback)(NSSet<NSString *> *)) {
        ASSERT_NOT_REACHED();
    };

    // Grant only one of the requested match patterns.
    requestDelegate.get().promptForPermissionMatchPatterns = ^(id<_WKWebExtensionTab> tab, NSSet<_WKWebExtensionMatchPattern *> *requestedMatchPatterns, void (^callback)(NSSet<_WKWebExtensionMatchPattern *> *)) {
        requestComplete = true;
        callback([NSSet setWithObject:requestedMatchPatterns.anyObject]);
    };

    manager.get().controllerDelegate = requestDelegate.get();

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Ready");

    runScriptWithUserGesture("runTest()"_s, manager.get().context._backgroundWebView);

    TestWebKitAPI::Util::run(&requestComplete);
}

TEST(WKWebExtensionAPIPermissions, ValidMatchPatterns)
{
    auto *manifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"alarms", @"activeTab" ],
        @"optional_permissions": @[ @"webNavigation", @"cookies" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"host_permissions": @[ @"*://*.example.com/*" ]
    };

    auto *backgroundScript = Util::constructScript(@[
        // Supported Schemes
        @"await browser.test.assertSafeResolve(() => browser.permissions.contains({ origins: [ '*://*.example.com/*' ] }))",
        @"await browser.test.assertSafeResolve(() => browser.permissions.contains({ origins: [ 'http://*.example.com/*' ] }))",
        @"await browser.test.assertSafeResolve(() => browser.permissions.contains({ origins: [ 'https://*.example.com/*' ] }))",
        @"await browser.test.assertSafeResolve(() => browser.permissions.contains({ origins: [ 'webkit-extension://*/*' ] }))",

        // Custom Web Extension Schemes
        @"await browser.test.assertSafeResolve(() => browser.permissions.contains({ origins: [ 'test-extension://*/*' ] }))",
        @"await browser.test.assertSafeResolve(() => browser.permissions.contains({ origins: [ 'other-extension://*/*' ] }))",

        // Invalid Schemes
        @"await browser.test.assertThrows(() => browser.permissions.contains({ origins: [ 'ftp://*.example.com/*' ] }), /not a valid pattern/)",
        @"await browser.test.assertThrows(() => browser.permissions.contains({ origins: [ 'data:*' ] }), /not a valid pattern/)",
        @"await browser.test.assertThrows(() => browser.permissions.contains({ origins: [ 'file:///*' ] }), /not a valid pattern/)",
        @"await browser.test.assertThrows(() => browser.permissions.contains({ origins: [ 'chrome-extension://*/*' ] }), /not a valid pattern/)",

        // Finish
        @"browser.test.notifyPass()"
    ]);

    [_WKWebExtensionMatchPattern registerCustomURLScheme:@"test-extension"];
    [_WKWebExtensionMatchPattern registerCustomURLScheme:@"other-extension"];

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    _WKWebExtensionMatchPattern *matchPatternApple = [_WKWebExtensionMatchPattern matchPatternWithString:@"*://*.example.com/*"];
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPatternApple];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIPermissions, ClipboardWrite)
{
    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Permissions Test",
        @"description": @"Permissions Test",
        @"version": @"1",

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"type": @"module",
            @"persistent": @NO,
        },

        @"permissions": @[ @"clipboardWrite" ],
    };

    auto *backgroundScript = Util::constructScript(@[
        @"await navigator.clipboard.writeText('Test Clipboard Write')",

        @"browser.test.yield('Clipboard Written')",
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:_WKWebExtensionPermissionClipboardWrite];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Clipboard Written");

#if TARGET_OS_IPHONE
    auto *clipboardContent = UIPasteboard.generalPasteboard.string;
#else
    auto *clipboardContent = [NSPasteboard.generalPasteboard stringForType:NSPasteboardTypeString];
#endif

    EXPECT_NS_EQUAL(clipboardContent, @"Test Clipboard Write");
}

TEST(WKWebExtensionAPIPermissions, ClipboardWriteWithoutPermission)
{
    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Permissions Test",
        @"description": @"Permissions Test",
        @"version": @"1",

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"type": @"module",
            @"persistent": @NO,
        },
    };

    auto *backgroundScript = Util::constructScript(@[
        @"try {",
        @"  await navigator.clipboard.writeText('Attempt Without Permission')",

        @"  browser.test.notifyFail('Should not write to clipboard without permission')",
        @"} catch (error) {",
        @"  browser.test.assertTrue(error instanceof DOMException, 'Expect a DOMException for insufficient permissions')",

        @"  browser.test.notifyPass()",
        @"}"
    ]);

#if TARGET_OS_IPHONE
    auto *clipboardContentBefore = UIPasteboard.generalPasteboard.string;
#else
    auto *clipboardContentBefore = [NSPasteboard.generalPasteboard stringForType:NSPasteboardTypeString];
#endif

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];

#if TARGET_OS_IPHONE
    auto *clipboardContentAfter = UIPasteboard.generalPasteboard.string;
#else
    auto *clipboardContentAfter = [NSPasteboard.generalPasteboard stringForType:NSPasteboardTypeString];
#endif

    EXPECT_NS_EQUAL(clipboardContentBefore, clipboardContentAfter);
}

TEST(WKWebExtensionAPIPermissions, ClipboardWriteWithRequest)
{
    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Permissions Test",
        @"description": @"Permissions Test",
        @"version": @"1",

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"type": @"module",
            @"persistent": @NO,
        },

        @"optional_permissions": @[ @"clipboardWrite" ],
    };

    auto *backgroundScript = Util::constructScript(@[
        @"window.runTest = async () => {",
        @"  try {",
        @"    await navigator.clipboard.writeText('Initial Attempt Without Permission')",
        @"  } catch (error) {",
        @"    browser.test.assertTrue(error instanceof DOMException, 'Expect a DOMException for insufficient permissions')",
        @"  }",

        @"  const permissionGranted = await browser.permissions.request({ permissions: [ 'clipboardWrite' ] })",

        @"  if (permissionGranted) {",
        @"    await navigator.clipboard.writeText('Test Clipboard Write After Permission')",

        @"    browser.test.yield('Clipboard Written')",
        @"  } else {",
        @"    browser.test.notifyFail('Permission was not granted')",
        @"  }",
        @"}",

        @"browser.test.yield('Ready')",
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto requestDelegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);

    requestDelegate.get().promptForPermissions = ^(id<_WKWebExtensionTab> tab, NSSet<NSString *> *requestedPermissions, void (^callback)(NSSet<NSString *> *grantedPermissions)) {
        EXPECT_EQ(requestedPermissions.count, 1ul);
        EXPECT_TRUE([requestedPermissions containsObject:_WKWebExtensionPermissionClipboardWrite]);

        callback(requestedPermissions);
    };

    manager.get().controllerDelegate = requestDelegate.get();

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Ready");

    runScriptWithUserGesture("window.runTest()"_s, manager.get().context._backgroundWebView);

    [manager run];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Clipboard Written");

#if TARGET_OS_IPHONE
    auto *clipboardContent = UIPasteboard.generalPasteboard.string;
#else
    auto *clipboardContent = [NSPasteboard.generalPasteboard stringForType:NSPasteboardTypeString];
#endif

    EXPECT_NS_EQUAL(clipboardContent, @"Test Clipboard Write After Permission");
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
