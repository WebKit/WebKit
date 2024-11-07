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

#import <WebKit/_WKWebExtensionWebNavigationURLFilter.h>
#import <wtf/text/MakeString.h>

namespace TestWebKitAPI {

static auto *webNavigationManifest = @{ @"manifest_version": @3, @"permissions": @[ @"webNavigation" ], @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO } };

TEST(WKWebExtensionAPIWebNavigation, EventListenerTest)
{
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

    Util::loadAndRunExtension(webNavigationManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIWebNavigation, EventFiringTest)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

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

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:webNavigationManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Grant the webNavigation permission.
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionWebNavigation];

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIWebNavigation, AllowedFilterTest)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        // Setup
        @"function passListener() { browser.test.notifyPass() }",

        // The passListener firing will consider the test passed.
        @"browser.webNavigation.onCommitted.addListener(passListener, { 'url': [ {'hostContains': 'localhost'} ] })",

        // Yield after creating the listener so we can load a tab.
        @"browser.test.yield('Load Tab')"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:webNavigationManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Grant the webNavigation permission.
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionWebNavigation];

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIWebNavigation, DeniedFilterTest)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

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

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:webNavigationManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Grant the webNavigation permission.
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionWebNavigation];

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIWebNavigation, AllEventsFiredTest)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

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

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:webNavigationManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Grant the webNavigation permission.
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionWebNavigation];

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIWebNavigation, RemoveListenerDuringEvent)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"function navigationListener() {",
        @"  browser.webNavigation.onCommitted.removeListener(navigationListener)",
        @"  browser.test.assertFalse(browser.webNavigation.onCommitted.hasListener(navigationListener), 'Listener should be removed')",
        @"}",

        @"browser.webNavigation.onCommitted.addListener(navigationListener)",
        @"browser.webNavigation.onCommitted.addListener(() => browser.test.notifyPass())",

        @"browser.test.assertTrue(browser.webNavigation.onCommitted.hasListener(navigationListener), 'Listener should be registered')",

        @"browser.test.yield('Load Tab')"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:webNavigationManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionWebNavigation];

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIWebNavigation, OnErrorOccurredProvisionalLoadEvent)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { HTTPResponse::Behavior::TerminateConnectionAfterReceivingResponse } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        // Setup
        @"function errorListener(details) {",
        // This should be a subframe
        @"  browser.test.assertTrue(details.frameId != 0)",

        // The URL of the frame that failed loading should include localhost and frame.
        @"  browser.test.assertTrue(details.url.includes('localhost'))",
        @"  browser.test.assertTrue(details.url.includes('frame'))",
        @"  browser.test.notifyPass()",
        @"}",

        // The passListener firing will consider the test passed.
        @"browser.webNavigation.onErrorOccurred.addListener(errorListener)",

        // Yield after creating the listener so we can load a tab.
        @"browser.test.yield('Load Tab')"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:webNavigationManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Grant the webNavigation permission.
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionWebNavigation];

    auto *urlRequest = server.requestWithLocalhost();
    NSURL *requestURL = urlRequest.URL;
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:requestURL];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:[requestURL URLByAppendingPathComponent:@"frame.html"]];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

template<size_t length>
String longString(LChar c)
{
    Vector<LChar> vector(length, c);
    return vector.span();
}

TEST(WKWebExtensionAPIWebNavigation, OnErrorOccurredDuringLoadEvent)
{
    TestWebKitAPI::HTTPServer server(TestWebKitAPI::HTTPServer::UseCoroutines::Yes, [&](auto connection) -> TestWebKitAPI::Task {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = TestWebKitAPI::HTTPServer::parsePath(request);
            if (path == "/"_s) {
                constexpr auto body = "<iframe src='/frame.html'></iframe>"_s;
                auto reply = makeString(
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html\r\n"
                    "Content-Length: "_s, body.length(), "\r\n"
                    "Connection: close\r\n"
                    "\r\n"_s, body
                );
                co_await connection.awaitableSend(WTFMove(reply));
                continue;
            }
            if (path == "/frame.html"_s) {
                auto response = makeString(
                    "HTTP/1.1 200 OK\r\n"_s,
                    "Content-Length: 1000000\r\n"
                    "\r\n"_s, longString<500000>(' ')
                );

                co_await connection.awaitableSend(WTFMove(response));
                connection.terminate();
                continue;
            }
            EXPECT_FALSE(true);
        }
    });

    auto *backgroundScript = Util::constructScript(@[
        // Setup
        @"async function errorListener(details) {",
        // This should be a subframe
        @"  browser.test.assertTrue(details.frameId != 0)",

        // The URL of the frame that failed loading should include localhost and frame.
        @"  browser.test.assertTrue(details.url.includes('localhost'))",
        @"  browser.test.assertTrue(details.url.includes('frame'))",

        @"  const frame = await browser.webNavigation.getFrame({ tabId: details.tabId, frameId: details.frameId })",
        @"  browser.test.assertEq(frame.parentFrameId, 0)",
        @"  browser.test.assertTrue(frame.errorOccurred)",

        // And since the failure happened after the load had been committed, the frame object has the URL set as well.
        @"  browser.test.assertTrue(frame.url.includes('localhost'))",
        @"  browser.test.assertTrue(frame.url.includes('frame'))",

        @"  browser.test.notifyPass()",
        @"}",

        // The passListener firing will consider the test passed.
        @"browser.webNavigation.onErrorOccurred.addListener(errorListener)",

        // Yield after creating the listener so we can load a tab.
        @"browser.test.yield('Load Tab')"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:webNavigationManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Grant the webNavigation permission.
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionWebNavigation];

    auto *urlRequest = server.requestWithLocalhost();
    NSURL *requestURL = urlRequest.URL;
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:requestURL];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:[requestURL URLByAppendingPathComponent:@"frame.html"]];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIWebNavigation, GetMainFrame)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body style='background-color: blue'></body>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        // Setup
        @"function completedListener(details) {",
        // Only listen for when the main frame loads so we don't call this method more than once.
        @"  if (details.frameId !== 0)",
        @"    return",
        @"  browser.webNavigation.getFrame({ tabId: details.tabId, frameId: 0 }, function(frame) {",
        // Main frame
        @"    browser.test.assertEq(frame.parentFrameId, -1)",
        @"    browser.test.assertTrue(frame.url.includes('localhost'))",
        @"    browser.test.assertFalse(frame.url.includes('frame'))",
        @"    browser.test.notifyPass()",
        @"  })",
        @"}",

        // The passListener firing will consider the test passed.
        @"browser.webNavigation.onCompleted.addListener(completedListener)",

        // Yield after creating the listener so we can load a tab.
        @"browser.test.yield('Load Tab')"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:webNavigationManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Grant the webNavigation permission.
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionWebNavigation];

    auto *urlRequest = server.requestWithLocalhost();
    NSURL *requestURL = urlRequest.URL;
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:requestURL];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:[requestURL URLByAppendingPathComponent:@"frame.html"]];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIWebNavigation, GetSubframe)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body style='background-color: blue'></body>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        // Setup
        @"function completedListener(details) {",
        // Only listen for when the subframe loads. We need its frameId to be able to look it up.
        @"  if (details.frameId === 0)",
        @"    return",
        @"  browser.webNavigation.getFrame({ tabId: details.tabId, frameId: details.frameId }, function(frame) {",
        // Main frame
        @"    browser.test.assertEq(frame.parentFrameId, 0)",
        @"    browser.test.assertTrue(frame.url.includes('localhost'))",
        @"    browser.test.assertTrue(frame.url.includes('frame'))",
        @"    browser.test.notifyPass()",
        @"  })",
        @"}",

        // The passListener firing will consider the test passed.
        @"browser.webNavigation.onCompleted.addListener(completedListener)",

        // Yield after creating the listener so we can load a tab.
        @"browser.test.yield('Load Tab')"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:webNavigationManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Grant the webNavigation permission.
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionWebNavigation];

    auto *urlRequest = server.requestWithLocalhost();
    NSURL *requestURL = urlRequest.URL;
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:requestURL];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:[requestURL URLByAppendingPathComponent:@"frame.html"]];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIWebNavigation, GetAllFrames)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body style='background-color: blue'></body>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        // Setup
        @"function completedListener(details) {",
        // Only listen for when the main frame loads so we don't call this method more than once.
        @"  if (details.frameId !== 0)",
        @"    return",
        @"  browser.webNavigation.getAllFrames({ tabId: details.tabId }, function(frames) {",
        @"    browser.test.assertEq(frames.length, 2)",
        @"    for (let frame of frames) {",
        @"      if (frame.frameId === 0) {",
        // Main frame
        @"        browser.test.assertEq(frame.parentFrameId, -1)",
        @"        browser.test.assertTrue(frame.url.includes('localhost'))",
        @"        browser.test.assertFalse(frame.url.includes('frame'))",
        @"      } else {",
        // Subframe
        @"        browser.test.assertEq(frame.parentFrameId, 0)",
        @"        browser.test.assertTrue(frame.url.includes('localhost'))",
        @"        browser.test.assertTrue(frame.url.includes('frame'))",
        @"      }",
        @"    }",
        @"    browser.test.notifyPass()",
        @"  })",
        @"}",

        // The passListener firing will consider the test passed.
        @"browser.webNavigation.onCompleted.addListener(completedListener)",

        // Yield after creating the listener so we can load a tab.
        @"browser.test.yield('Load Tab')"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:webNavigationManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Grant the webNavigation permission.
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionWebNavigation];

    auto *urlRequest = server.requestWithLocalhost();
    NSURL *requestURL = urlRequest.URL;
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:requestURL];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:[requestURL URLByAppendingPathComponent:@"frame.html"]];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIWebNavigation, ErrorOccurred)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { HTTPResponse::Behavior::TerminateConnectionAfterReceivingResponse } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        // Setup
        @"function errorListener(details) {",
        // A subframe should have been the one to have the error.
        @"  browser.test.assertFalse(details.frameId == 0)",
        @"  browser.test.assertEq(details.parentFrameId, 0)",

        // Get more information about the frame to verify the errorOccurred bit was set.
        @"  browser.webNavigation.getFrame({ tabId: details.tabId, frameId: details.frameId }, function(frame) {",
        @"    browser.test.assertEq(frame.parentFrameId, 0)",
        @"    browser.test.assertTrue(frame.errorOccurred)",

        // One thing to note here is that if the provisional load fails, there won't be a URL in the details.
        @"    browser.test.assertEq(frame.url, '')",

        @"    browser.test.notifyPass()",
        @"  })",
        @"}",

        // The passListener firing will consider the test passed.
        @"browser.webNavigation.onErrorOccurred.addListener(errorListener)",

        // Yield after creating the listener so we can load a tab.
        @"browser.test.yield('Load Tab')"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:webNavigationManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Grant the webNavigation permission.
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionWebNavigation];

    auto *urlRequest = server.requestWithLocalhost();
    NSURL *requestURL = urlRequest.URL;
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:requestURL];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:[requestURL URLByAppendingPathComponent:@"frame.html"]];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIWebNavigation, Errors)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body style='background-color: blue'></body>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        // Setup
        @"async function completedListener(details) {",
        // Only listen for when the main frame loads so we don't call this method more than once.
        @"  if (details.frameId !== 0)",
        @"    return",
        @"  const activeTab = await browser.tabs.query({ active: true })",
        // Make sure invalid tab/frame IDs vend an error message - use arbitrary frame and tabIds.
        @"  await browser.test.assertRejects(browser.webNavigation.getFrame({tabId: (details.tabId + 1), frameId: 0}), /tab not found/i)",
        @"  await browser.test.assertRejects(browser.webNavigation.getFrame({tabId: details.tabId, frameId: 42}), /frame not found/i)",
        @"  browser.test.notifyPass()",
        @"}",

        // The passListener firing will consider the test passed.
        @"browser.webNavigation.onCompleted.addListener(completedListener)",

        // Yield after creating the listener so we can load a tab.
        @"browser.test.yield('Load Tab')"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:webNavigationManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Grant the webNavigation permission.
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionWebNavigation];

    auto *urlRequest = server.requestWithLocalhost();
    NSURL *requestURL = urlRequest.URL;
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:requestURL];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:[requestURL URLByAppendingPathComponent:@"frame.html"]];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

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
    test(@{ @"url": [NSNull null] }, @"The 'filters' value is invalid, because 'url' is expected to be an array of objects, but null was provided.");
    test(@{ @"url": @[ ] }, nil);
    test(@{ @"url": @[ @"A" ] }, @"The 'filters' value is invalid, because 'url' is expected to be an array of objects, but a string was provided in the array.");
    test(@{ @"url": @[ @[ ] ] }, @"The 'filters' value is invalid, because 'url' is expected to be an array of objects, but an array was provided in the array.");
    test(@{ @"url": @[ @{ } ] }, nil);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
