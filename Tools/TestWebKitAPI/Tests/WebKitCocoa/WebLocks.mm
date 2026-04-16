/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/text/MakeString.h>

namespace TestWebKitAPI {

static void enableWebLocksAPI(WKWebViewConfiguration *configuration)
{
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"WebLocksAPIEnabled"]) {
            [[configuration preferences] _setEnabled:YES forFeature:feature];
            return;
        }
    }
}

enum class ShouldUseSameProcess : bool { No, Yes };

static void runSnapshotAcrossPagesTest(ShouldUseSameProcess shouldUseSameProcess)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { "foo"_s } }
    });

    auto configuration1 = adoptNS([[WKWebViewConfiguration alloc] init]);
    enableWebLocksAPI(configuration1.get());

    auto webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration1.get()]);
    [webView1 synchronouslyLoadRequest:server.request()];

    auto pid1 = [webView1 _webProcessIdentifier];
    EXPECT_NE(pid1, 0);

    __block bool done = false;
    [webView1 performAfterReceivingAnyMessage:^(NSString *message) {
        EXPECT_WK_STREQ(message, @"ACQUIRED");
        done = true;
    }];
    [webView1 evaluateJavaScript:@"navigator.locks.request('foo', lock => { webkit.messageHandlers.testHandler.postMessage('ACQUIRED'); return new Promise(() => {}); }) && 1" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
    }];
    TestWebKitAPI::Util::run(&done);

    auto configuration2 = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration2.get().processPool = [configuration1 processPool];
    enableWebLocksAPI(configuration2.get());
    if (shouldUseSameProcess == ShouldUseSameProcess::Yes) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        configuration2.get()._relatedWebView = webView1.get();
        ALLOW_DEPRECATED_DECLARATIONS_END
    }
    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration2.get()]);
    [webView2 synchronouslyLoadRequest:server.request()];

    auto pid2 = [webView2 _webProcessIdentifier];
    if (shouldUseSameProcess == ShouldUseSameProcess::Yes)
        EXPECT_EQ(pid1, pid2);
    else
        EXPECT_NE(pid1, pid2);

    done = false;
    [webView2 performAfterReceivingAnyMessage:^(NSString *message) {
        EXPECT_WK_STREQ(message, @"ACQUIRED");
        done = true;
    }];

    [webView2 evaluateJavaScript:@"navigator.locks.request('bar', lock => { webkit.messageHandlers.testHandler.postMessage('ACQUIRED'); return new Promise(() => {}); }) && 1" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [webView1 performAfterReceivingAnyMessage:^(NSString *message) {
        // There should be 0 pending locks, and 2 held ones.
        EXPECT_WK_STREQ(message, @"0-2");
        done = true;
    }];
    [webView1 evaluateJavaScript:@"navigator.locks.query().then(snapshot => { webkit.messageHandlers.testHandler.postMessage('' + snapshot.pending.length + '-' + snapshot.held.length); }) && 1" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [webView2 performAfterReceivingAnyMessage:^(NSString *message) {
        // There should be 0 pending locks, and 2 held ones.
        EXPECT_WK_STREQ(message, @"0-2");
        done = true;
    }];
    [webView2 evaluateJavaScript:@"navigator.locks.query().then(snapshot => { webkit.messageHandlers.testHandler.postMessage('' + snapshot.pending.length + '-' + snapshot.held.length); }) && 1" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WebLocks, SnapshotAcrossPagesInDifferentProcesses)
{
    runSnapshotAcrossPagesTest(ShouldUseSameProcess::No);
}

TEST(WebLocks, SnapshotAcrossPagesInSameProcess)
{
    runSnapshotAcrossPagesTest(ShouldUseSameProcess::Yes);
}

static void runLockRequestWaitingOnAnotherPage(ShouldUseSameProcess shouldUseSameProcess)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { "foo"_s } }
    });

    auto configuration1 = adoptNS([[WKWebViewConfiguration alloc] init]);
    enableWebLocksAPI(configuration1.get());

    auto webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration1.get()]);
    [webView1 synchronouslyLoadRequest:server.request()];

    auto pid1 = [webView1 _webProcessIdentifier];
    EXPECT_NE(pid1, 0);

    __block bool done = false;
    [webView1 performAfterReceivingAnyMessage:^(NSString *message) {
        EXPECT_WK_STREQ(message, @"ACQUIRED");
        done = true;
    }];
    [webView1 evaluateJavaScript:@"navigator.locks.request('foo', lock => { webkit.messageHandlers.testHandler.postMessage('ACQUIRED'); return new Promise(() => {}); }) && 1" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
    }];
    TestWebKitAPI::Util::run(&done);

    auto configuration2 = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration2.get().processPool = [configuration1 processPool];
    enableWebLocksAPI(configuration2.get());
    if (shouldUseSameProcess == ShouldUseSameProcess::Yes) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        configuration2.get()._relatedWebView = webView1.get();
        ALLOW_DEPRECATED_DECLARATIONS_END
    }
    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration2.get()]);
    [webView2 synchronouslyLoadRequest:server.request()];

    auto pid2 = [webView2 _webProcessIdentifier];
    if (shouldUseSameProcess == ShouldUseSameProcess::Yes)
        EXPECT_EQ(pid1, pid2);
    else
        EXPECT_NE(pid1, pid2);

    __block bool lockAcquired = false;
    [webView2 performAfterReceivingAnyMessage:^(NSString *message) {
        EXPECT_WK_STREQ(message, @"ACQUIRED");
        lockAcquired = true;
    }];

    // Requesting the same lock name as webView1 so the request should get queued until webView1 releases it.
    [webView2 evaluateJavaScript:@"navigator.locks.request('foo', lock => { webkit.messageHandlers.testHandler.postMessage('ACQUIRED'); return new Promise(() => {}); }) && 1" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
    }];

    TestWebKitAPI::Util::runFor(0.5_s);
    EXPECT_FALSE(lockAcquired);

    done = false;
    [webView2 performAfterReceivingAnyMessage:^(NSString *message) {
        // There should be 1 pending lock, and 1 held one.
        EXPECT_WK_STREQ(message, @"1-1");
        done = true;
    }];
    [webView2 evaluateJavaScript:@"navigator.locks.query().then(snapshot => { webkit.messageHandlers.testHandler.postMessage('' + snapshot.pending.length + '-' + snapshot.held.length); }) && 1" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
    }];
    TestWebKitAPI::Util::run(&done);

    [webView2 performAfterReceivingAnyMessage:^(NSString *message) {
        EXPECT_WK_STREQ(message, @"ACQUIRED");
        lockAcquired = true;
    }];

    // Now close the webView1. This should release the 'foo' lock and allow webView2 to acquire it.
    [webView1 _close];

    TestWebKitAPI::Util::run(&lockAcquired);

    done = false;
    [webView2 performAfterReceivingAnyMessage:^(NSString *message) {
        // There should be 0 pending locks, and 1 held one.
        EXPECT_WK_STREQ(message, @"0-1");
        done = true;
    }];
    [webView2 evaluateJavaScript:@"navigator.locks.query().then(snapshot => { webkit.messageHandlers.testHandler.postMessage('' + snapshot.pending.length + '-' + snapshot.held.length); }) && 1" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WebLocks, LockRequestWaitingOnAnotherPageInOtherProcess)
{
    runLockRequestWaitingOnAnotherPage(ShouldUseSameProcess::No);
}

TEST(WebLocks, LockRequestWaitingOnAnotherPageInSameProcess)
{
    runLockRequestWaitingOnAnotherPage(ShouldUseSameProcess::Yes);
}

TEST(WebLocks, ServiceWorkerLockRequestAfterCrossSiteNavigationInSameProcess)
{
    TestWebKitAPI::HTTPServer server1({ }, TestWebKitAPI::HTTPServer::Protocol::Http);
    TestWebKitAPI::HTTPServer server2({ }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto swScript = "self.addEventListener('message', event => { event.waitUntil(new Promise(() => {})); event.source.postMessage('processing'); function loop() { navigator.locks.request('sw-lock', () => {}).then(loop); } loop(); });"_s;
    auto page2HTML = "<script>webkit.messageHandlers.testHandler.postMessage('LOADED');</script>"_s;
    auto page1HTML = makeString(
        "<script>"
        "navigator.serviceWorker.register('/sw.js').then(() => navigator.serviceWorker.ready).then(reg => {"
        "    navigator.serviceWorker.onmessage = () => { window.location = 'http://localhost:"_s, server2.port(), "/'; };"
        "    reg.active.postMessage('start');"
        "});"
        "</script>"_s);

    server1.addResponse("/sw.js"_s, { { { "Content-Type"_s, "text/javascript"_s } }, swScript });
    server1.addResponse("/"_s, { page1HTML });
    server2.addResponse("/"_s, { page2HTML });

    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().processSwapsOnNavigation = NO;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().processPool = processPool.get();
    enableWebLocksAPI(configuration.get());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    __block bool done = false;
    [webView performAfterReceivingAnyMessage:^(NSString *message) {
        EXPECT_WK_STREQ(message, @"LOADED");
        done = true;
    }];

    [webView synchronouslyLoadRequest:server1.request()];
    TestWebKitAPI::Util::run(&done);
}

} // namespace TestWebKitAPI
