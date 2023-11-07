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

#import <WebKit/_WKWebExtensionWebNavigationURLFilter.h>

namespace TestWebKitAPI {

TEST(WKWebExtensionAPIWebNavigation, EventListenerTest)
{
    auto *manifest = @{ @"manifest_version": @3, @"permissions": @[ @"webNavigation" ], @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO } };

    auto *backgroundScript = Util::constructScript(@[
        // Setup
        @"function listener() { browser.test.notifyFail('This listener should not have been called') }",
        @"browser.test.assertFalse(browser.webNavigation.onBeforeNavigate.hasListener(listener), 'Should not have listener')",

        // Test
        @"browser.webNavigation.onBeforeNavigate.addListener(listener)",
        @"browser.test.assertTrue(browser.webNavigation.onBeforeNavigate.hasListener(listener), 'Should have listener')",
        @"browser.webNavigation.onBeforeNavigate.removeListener(listener)",
        @"browser.test.assertFalse(browser.webNavigation.onBeforeNavigate.hasListener(listener), 'Should not have listener')",

        // Finish
        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(manifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIWebNavigation, EventFiringTest)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *manifest = @{ @"manifest_version": @3, @"permissions": @[ @"webNavigation" ], @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO } };

    auto *backgroundScript = Util::constructScript(@[
        // Setup
        @"function passListener(details) {",

        // This event was fired on a main frame load, so the frameId should be 0 and we shouldn't have a parent frame.
        @"  browser.test.assertEq(details.frameId, 0)",
        @"  browser.test.assertEq(details.parentFrameId, -1)",

        @"  browser.test.notifyPass()",
        @"}",

        // The passListener firing will consider the test passed.
        @"browser.webNavigation.onCommitted.addListener(passListener)",

        // Yield after creating the listener so we can load a tab.
        @"browser.test.yield('Load Tab')"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIWebNavigation, AllowedFilterTest)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *manifest = @{ @"manifest_version": @3, @"permissions": @[ @"webNavigation" ], @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO } };

    auto *backgroundScript = Util::constructScript(@[
        // Setup
        @"function passListener() { browser.test.notifyPass() }",

        // The passListener firing will consider the test passed.
        @"browser.webNavigation.onCommitted.addListener(passListener, { 'url': [ {'hostContains': 'localhost'} ] })",

        // Yield after creating the listener so we can load a tab.
        @"browser.test.yield('Load Tab')"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIWebNavigation, DeniedFilterTest)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *manifest = @{ @"manifest_version": @3, @"permissions": @[ @"webNavigation" ], @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO } };

    auto *backgroundScript = Util::constructScript(@[
        // Setup
        @"function passListener() { browser.test.notifyPass() }",
        @"function failListener() { browser.test.notifyFail('This listener should not have been called') }",

        // The passListener firing (but not the failListener) will consider the test passed.
        @"browser.webNavigation.onCommitted.addListener(failListener, { 'url': [ {'hostContains': 'example'} ] })",
        @"browser.webNavigation.onCommitted.addListener(passListener)",

        // Yield after creating the listener so we can load a tab.
        @"browser.test.yield('Load Tab')"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIWebNavigation, AllEventsFiredTest)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *manifest = @{ @"manifest_version": @3, @"permissions": @[ @"webNavigation" ], @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO } };

    auto *backgroundScript = Util::constructScript(@[
        // Setup
        @"let beforeNavigateEventFired = false",
        @"let onCommittedEventFired = false",
        @"let onDOMContentLoadedEventFired = false",
        @"function beforeNavigateHandler() { beforeNavigateEventFired = true }",
        @"function onCommittedHandler() { onCommittedEventFired = true }",
        @"function onDOMContentLoadedHandler() { onDOMContentLoadedEventFired = true }",
        @"function onCompletedHandler() {",
        @"  browser.test.assertTrue(beforeNavigateEventFired)",
        @"  browser.test.assertTrue(onCommittedEventFired)",
        @"  browser.test.assertTrue(onDOMContentLoadedEventFired)",
        @"  browser.test.notifyPass()",
        @"}",

        @"browser.webNavigation.onBeforeNavigate.addListener(beforeNavigateHandler)",
        @"browser.webNavigation.onCommitted.addListener(onCommittedHandler)",
        @"browser.webNavigation.onDOMContentLoaded.addListener(onDOMContentLoadedHandler)",
        @"browser.webNavigation.onCompleted.addListener(onCompletedHandler)",

        // Yield after creating the listener so we can load a tab.
        @"browser.test.yield('Load Tab')"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager run];
}


TEST(WKWebExtensionAPIWebNavigation, URLFilterTestMatchAllPredicates)
{
    NSString *errorString = nil;
    NSDictionary *filterDictionary = @{
        @"url": @[
            @{
                @"schemes": @[ @"https" ],
                @"hostEquals": @"apple.com",
            }
        ]
    };

    _WKWebExtensionWebNavigationURLFilter *filter = [[_WKWebExtensionWebNavigationURLFilter alloc] initWithDictionary:filterDictionary outErrorMessage:&errorString];
    EXPECT_NULL(errorString);

    EXPECT_TRUE([filter matchesURL:[NSURL URLWithString:@"https://apple.com"]]);
    EXPECT_FALSE([filter matchesURL:[NSURL URLWithString:@"http://apple.com"]]);
    EXPECT_FALSE([filter matchesURL:[NSURL URLWithString:@"https://example.com"]]);

    [filter release];
}


TEST(WKWebExtensionAPIWebNavigation, URLFilterMatchesOnePredicate)
{
    NSString *errorString = nil;
    NSDictionary *filterDictionary = @{
        @"url": @[
            @{ @"hostEquals": @"apple.com" },
            @{ @"hostEquals": @"example.com" },
        ]
    };

    _WKWebExtensionWebNavigationURLFilter *filter = [[_WKWebExtensionWebNavigationURLFilter alloc] initWithDictionary:filterDictionary outErrorMessage:&errorString];
    EXPECT_NULL(errorString);

    EXPECT_TRUE([filter matchesURL:[NSURL URLWithString:@"http://apple.com"]]);
    EXPECT_TRUE([filter matchesURL:[NSURL URLWithString:@"http://example.com"]]);
    EXPECT_FALSE([filter matchesURL:[NSURL URLWithString:@"about:blank"]]);
    EXPECT_FALSE([filter matchesURL:[NSURL URLWithString:@"file:///dev/null"]]);

    [filter release];
}

TEST(WKWebExtensionAPIWebNavigation, EmptyFilterMatchesEverything)
{
    NSString *errorString = nil;
    NSDictionary *filterDictionary = @{
        @"url": @[ ]
    };

    _WKWebExtensionWebNavigationURLFilter *filter = [[_WKWebExtensionWebNavigationURLFilter alloc] initWithDictionary:filterDictionary outErrorMessage:&errorString];
    EXPECT_NULL(errorString);

    EXPECT_TRUE([filter matchesURL:[NSURL URLWithString:@"about:blank"]]);
    EXPECT_TRUE([filter matchesURL:[NSURL URLWithString:@"http://example.com"]]);
    EXPECT_TRUE([filter matchesURL:[NSURL URLWithString:@"file:///dev/null"]]);

    [filter release];
}

TEST(WKWebExtensionAPIWebNavigation, URLKeyTypeChecking)
{
    __auto_type test = ^(NSDictionary *inputDictionary, NSString *expectedError) {
        _WKWebExtensionWebNavigationURLFilter *filter;
        NSString *error = nil;
        filter = [[_WKWebExtensionWebNavigationURLFilter alloc] initWithDictionary:inputDictionary outErrorMessage:&error];
        if (expectedError) {
            EXPECT_NS_EQUAL(error, expectedError);
            EXPECT_NULL(filter);
        } else {
            EXPECT_NULL(error);
            EXPECT_NOT_NULL(filter);
        }

        [filter release];
    };

    test(@{ }, @"The 'filters' value is invalid, because it is missing required keys: 'url'.");
    test(@{ @"a": @"b" }, @"The 'filters' value is invalid, because it is missing required keys: 'url'.");
    test(@{ @"a": @"b", @"url": @[ ] }, nil);
    test(@{ @"url": [NSNull null] }, @"The 'filters' value is invalid, because 'url' is expected to be an array, but null was provided.");
    test(@{ @"url": @[ ] }, nil);
    test(@{ @"url": @[ @"A" ] }, @"The 'filters' value is invalid, because 'url' is expected to be objects in an array, but a string was provided.");
    test(@{ @"url": @[ @[ ] ] }, @"The 'filters' value is invalid, because 'url' is expected to be objects in an array, but an array with other values was provided.");
    test(@{ @"url": @[ @{ } ] }, nil);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
