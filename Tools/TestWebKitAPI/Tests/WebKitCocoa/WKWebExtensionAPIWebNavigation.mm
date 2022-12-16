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

#import "WebExtensionUtilities.h"

#import <WebKit/_WKWebExtensionWebNavigationURLFilter.h>

namespace TestWebKitAPI {

TEST(WKWebExtensionAPIWebNavigation, EventListenerTest)
{
    auto *manifest = @{ @"manifest_version": @3, @"permissions": @[ @"webNavigation" ], @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO } };

    auto *backgroundScript = Util::constructScript(@[
        // Setup
        @"function listener() { browser.test.notifyFail('This listener should not have been called') }",
        @"browser.test.assertFalse(browser.test.testWebNavigationEvent.hasListener(listener), 'Should not have listener')",

        // Test
        @"browser.test.testWebNavigationEvent.addListener(listener)",
        @"browser.test.assertTrue(browser.test.testWebNavigationEvent.hasListener(listener), 'Should have listener')",
        @"browser.test.testWebNavigationEvent.removeListener(listener)",
        @"browser.test.assertFalse(browser.test.testWebNavigationEvent.hasListener(listener), 'Should not have listener')",

        // Finish
        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(manifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIWebNavigation, EventFiringTest)
{
    auto *manifest = @{ @"manifest_version": @3, @"permissions": @[ @"webNavigation" ], @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO } };

    auto *backgroundScript = Util::constructScript(@[
        // Setup
        @"function passListener() { browser.test.notifyPass() }",

        // Test
        @"browser.test.testWebNavigationEvent.addListener(passListener)",

        // The passListener firing will consider the test passed.
        @"browser.test.fireTestWebNavigationEvent('https://webkit.org')"
    ]);

    Util::loadAndRunExtension(manifest, @{ @"background.js": backgroundScript });
}

// FIXME: Add an API test for an invalid filter throwing an exception.

TEST(WKWebExtensionAPIWebNavigation, AllowedFilterTest)
{
    auto *manifest = @{ @"manifest_version": @3, @"permissions": @[ @"webNavigation" ], @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO } };

    auto *backgroundScript = Util::constructScript(@[
        // Setup
        @"function passListener() { browser.test.notifyPass() }",

        // Test
        @"browser.test.testWebNavigationEvent.addListener(passListener, { 'url': [ {'hostContains': 'webkit'} ] })",

        // The passListener firing will consider the test passed.
        @"browser.test.fireTestWebNavigationEvent('https://webkit.org')"
    ]);

    Util::loadAndRunExtension(manifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIWebNavigation, DeniedFilterTest)
{
    auto *manifest = @{ @"manifest_version": @3, @"permissions": @[ @"webNavigation" ], @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO } };

    auto *backgroundScript = Util::constructScript(@[
        // Setup
        @"function passListener() { browser.test.notifyPass() }",
        @"function failListener() { browser.test.notifyFail('This listener should not have been called') }",

        // Test
        @"browser.test.testWebNavigationEvent.addListener(failListener, { 'url': [ {'hostContains': 'example'} ] })",
        @"browser.test.testWebNavigationEvent.addListener(passListener)",

        // The passListener firing will consider the test passed.
        @"browser.test.fireTestWebNavigationEvent('https://webkit.org')"
    ]);

    Util::loadAndRunExtension(manifest, @{ @"background.js": backgroundScript });
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

    test(@{ }, @"Missing required keys: url.");
    test(@{ @"a": @"b" }, @"Missing required keys: url.");
    test(@{ @"a": @"b", @"url": @[ ] }, nil);
    test(@{ @"url": [NSNull null] }, @"Expected an array for 'url'.");
    test(@{ @"url": @[ ] }, nil);
    test(@{ @"url": @[ @"A" ] }, @"Expected objects in the array for 'url', found a string value instead.");
    test(@{ @"url": @[ @[ ] ] }, @"Expected objects in the array for 'url', found an array instead.");
    test(@{ @"url": @[ @{ } ] }, nil);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
