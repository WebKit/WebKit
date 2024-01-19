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

#import <WebKit/_WKWebExtensionWebRequestFilter.h>

namespace TestWebKitAPI {

static auto *webRequestManifest = @{ @"manifest_version": @3, @"permissions": @[ @"webRequest" ], @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO } };

TEST(WKWebExtensionAPIWebRequest, EventListenerTest)
{
    auto *backgroundScript = Util::constructScript(@[
        // Setup
        @"function listener() { browser.test.notifyFail('This listener should not have been called') }",
        @"browser.test.assertFalse(browser.webRequest.onCompleted.hasListener(listener), 'Should not have listener')",

        // Test
        @"browser.webRequest.onCompleted.addListener(listener)",
        @"browser.test.assertTrue(browser.webRequest.onCompleted.hasListener(listener), 'Should have listener')",
        @"browser.webRequest.onCompleted.removeListener(listener)",
        @"browser.test.assertFalse(browser.webRequest.onCompleted.hasListener(listener), 'Should not have listener')",

        // Finish
        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(webRequestManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIWebRequest, Initialization)
{
    NSString *error;
    _WKWebExtensionWebRequestFilter *filter;

    filter = [[_WKWebExtensionWebRequestFilter alloc] initWithDictionary:@{
        @"urls": @[ @"http://foo.com/*", @"http://bar.org/*" ]
    } outErrorMessage:&error];
    EXPECT_NOT_NULL(filter);
    EXPECT_NULL(error);

    filter = [[_WKWebExtensionWebRequestFilter alloc] initWithDictionary:@{
        @"urls": @[ @"http://foo.com/*", @"http://bar.org/*" ],
        @"types": @[ @"main_frame", @"sub_frame" ],
        @"tabId": @123
    } outErrorMessage:&error];
    EXPECT_NOT_NULL(filter);
    EXPECT_NULL(error);

    filter = [[_WKWebExtensionWebRequestFilter alloc] initWithDictionary:@{
        @"urls": @[ @"http://foo.com/*", @"http://bar.org/*" ],
        @"types": @[ @"main_frame", @"sub_frame" ],
        @"windowId": @123
    } outErrorMessage:&error];
    EXPECT_NOT_NULL(filter);
    EXPECT_NULL(error);

    filter = [[_WKWebExtensionWebRequestFilter alloc] initWithDictionary:@{
        @"urls": @[ @"http://has.both.a.tab.id.and.window.id.com/*" ],
        @"types": @[ @"main_frame", @"sub_frame" ],
        @"windowId": @123,
        @"tabId": @9001
    } outErrorMessage:&error];
    EXPECT_NOT_NULL(filter);
    EXPECT_NULL(error);

    filter = [[_WKWebExtensionWebRequestFilter alloc] initWithDictionary:@{
        @"urls": @[ ],
    } outErrorMessage:&error];
    EXPECT_NOT_NULL(filter);
    EXPECT_NULL(error);

    filter = [[_WKWebExtensionWebRequestFilter alloc] initWithDictionary:@{
        @"urls": @[ @"http://foo.com/*", @3 ],
    } outErrorMessage:&error];
    EXPECT_NULL(filter);
    EXPECT_NS_EQUAL(error, @"'urls' is expected to be an array of strings, but a number was provided in the array");

    filter = [[_WKWebExtensionWebRequestFilter alloc] initWithDictionary:@{
        @"urls": @[ @"$" ]
    } outErrorMessage:&error];
    EXPECT_NULL(filter);

    // FIXME: We shouldn't be generating an error message with two periods.
    EXPECT_NS_EQUAL(error, @"The 'urls' value is invalid, because '$' is an invalid match pattern. \"$\" cannot be parsed because it doesn't have a scheme..");
}

static _WKWebExtensionWebRequestFilter *filterWithDictionary(NSDictionary *dictionary)
{
    NSString *error;
    _WKWebExtensionWebRequestFilter *filter = [[_WKWebExtensionWebRequestFilter alloc] initWithDictionary:dictionary outErrorMessage:&error];
    EXPECT_NOT_NULL(filter);
    EXPECT_NULL(error);

    return filter;
}

TEST(WKWebExtensionAPIWebRequest, TabIDMatch)
{
    NSURL *url = [NSURL URLWithString:@"http://example.com/a"];
    _WKWebExtensionWebRequestResourceType type = _WKWebExtensionWebRequestResourceTypeOther;

    auto filter = filterWithDictionary(@{
        @"urls": @[ @"http://example.com/*" ],
    });
    EXPECT_TRUE([filter matchesRequestForResourceOfType:type URL:url tabID:100 windowID:1]);

    filter = filterWithDictionary(@{
        @"urls": @[ @"http://example.com/*" ],
        @"tabId": @(-1),
    });
    EXPECT_TRUE([filter matchesRequestForResourceOfType:type URL:url tabID:100 windowID:1]);

    filter = filterWithDictionary(@{
        @"urls": @[ @"http://example.com/*" ],
        @"tabId": @(100),
    });
    EXPECT_TRUE([filter matchesRequestForResourceOfType:type URL:url tabID:100 windowID:1]);
    EXPECT_FALSE([filter matchesRequestForResourceOfType:type URL:url tabID:200 windowID:1]);
}

TEST(WKWebExtensionAPIWebRequest, WindowIDMatch)
{
    NSURL *url = [NSURL URLWithString:@"http://example.com/a"];
    _WKWebExtensionWebRequestResourceType type = _WKWebExtensionWebRequestResourceTypeOther;

    auto filter = filterWithDictionary(@{
        @"urls": @[ @"http://example.com/*" ],
    });
    EXPECT_TRUE([filter matchesRequestForResourceOfType:type URL:url tabID:-1 windowID:-1]);

    filter = filterWithDictionary(@{
        @"urls": @[ @"http://example.com/*" ],
        @"windowId": @(-1),
    });
    EXPECT_TRUE([filter matchesRequestForResourceOfType:type URL:url tabID:1 windowID:1]);

    filter = filterWithDictionary(@{
        @"urls": @[ @"http://example.com/*" ],
        @"windowId": @(100),
    });
    EXPECT_TRUE([filter matchesRequestForResourceOfType:type URL:url tabID:1 windowID:100]);
    EXPECT_FALSE([filter matchesRequestForResourceOfType:type URL:url tabID:1 windowID:200]);
}

TEST(WKWebExtensionAPIWebRequest, URLMatch)
{
    NSURL *url = [NSURL URLWithString:@"http://example.com/a/b"];
    NSURL *otherURL = [NSURL URLWithString:@"http://some-other-website.biz/a/b"];
    _WKWebExtensionWebRequestResourceType type = _WKWebExtensionWebRequestResourceTypeOther;

    auto filter = filterWithDictionary(@{
        @"urls": @[ ],
    });
    EXPECT_TRUE([filter matchesRequestForResourceOfType:type URL:url tabID:1 windowID:1]);
    EXPECT_TRUE([filter matchesRequestForResourceOfType:type URL:otherURL tabID:1 windowID:1]);

    filter = filterWithDictionary(@{
        @"urls": @[ @"http://example.com/*" ],
    });
    EXPECT_TRUE([filter matchesRequestForResourceOfType:type URL:url tabID:1 windowID:1]);
    EXPECT_FALSE([filter matchesRequestForResourceOfType:type URL:otherURL tabID:1 windowID:1]);

    filter = filterWithDictionary(@{
        @"urls": @[ @"http://not-example.com/*", @"http://not-some-other-website.biz/*" ],
    });
    EXPECT_FALSE([filter matchesRequestForResourceOfType:type URL:url tabID:1 windowID:1]);

    filter = filterWithDictionary(@{
        @"urls": @[ @"http://example.com/*", @"http://not-some-other-website.biz/*" ],
    });
    EXPECT_TRUE([filter matchesRequestForResourceOfType:type URL:url tabID:1 windowID:1]);
    EXPECT_FALSE([filter matchesRequestForResourceOfType:type URL:otherURL tabID:1 windowID:1]);

    filter = filterWithDictionary(@{
        @"urls": @[ @"http://example.com/*/b", @"http://example.com/a/*" ]
    });

    EXPECT_TRUE([filter matchesRequestForResourceOfType:type URL:url tabID:1 windowID:1]);
}

TEST(WKWebExtensionAPIWebRequest, ResourceTypesMatch)
{
    NSURL *url = [NSURL URLWithString:@"http://example.com/a/b"];

    auto filter = filterWithDictionary(@{
        @"urls": @[ ],
    });
    EXPECT_TRUE([filter matchesRequestForResourceOfType:_WKWebExtensionWebRequestResourceTypeImage URL:url tabID:1 windowID:1]);
    EXPECT_TRUE([filter matchesRequestForResourceOfType:_WKWebExtensionWebRequestResourceTypeScript URL:url tabID:1 windowID:1]);

    filter = filterWithDictionary(@{
        @"urls": @[ ],
        @"types": @[ ],
    });
    EXPECT_TRUE([filter matchesRequestForResourceOfType:_WKWebExtensionWebRequestResourceTypeImage URL:url tabID:1 windowID:1]);
    EXPECT_TRUE([filter matchesRequestForResourceOfType:_WKWebExtensionWebRequestResourceTypeScript URL:url tabID:1 windowID:1]);

    filter = filterWithDictionary(@{
        @"urls": @[ ],
        @"types": @[ @"image" ],
    });
    EXPECT_TRUE([filter matchesRequestForResourceOfType:_WKWebExtensionWebRequestResourceTypeImage URL:url tabID:1 windowID:1]);
    EXPECT_FALSE([filter matchesRequestForResourceOfType:_WKWebExtensionWebRequestResourceTypeScript URL:url tabID:1 windowID:1]);

    filter = filterWithDictionary(@{
        @"urls": @[ ],
        @"types": @[ @"image", @"script" ],
    });
    EXPECT_TRUE([filter matchesRequestForResourceOfType:_WKWebExtensionWebRequestResourceTypeImage URL:url tabID:1 windowID:1]);
    EXPECT_TRUE([filter matchesRequestForResourceOfType:_WKWebExtensionWebRequestResourceTypeScript URL:url tabID:1 windowID:1]);
    EXPECT_FALSE([filter matchesRequestForResourceOfType:_WKWebExtensionWebRequestResourceTypeWebsocket URL:url tabID:1 windowID:1]);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
