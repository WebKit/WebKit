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

#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestUIDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/Utilities.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
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

    RetainPtr configuration1 = adoptNS([[WKWebViewConfiguration alloc] init]);
    enableWebLocksAPI(configuration1.get());

    RetainPtr webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration1.get()]);
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

    RetainPtr configuration2 = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration2.get().processPool = [configuration1 processPool];
    enableWebLocksAPI(configuration2.get());
    if (shouldUseSameProcess == ShouldUseSameProcess::Yes) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        configuration2.get()._relatedWebView = webView1.get();
        ALLOW_DEPRECATED_DECLARATIONS_END
    }
    RetainPtr webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration2.get()]);
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

    RetainPtr configuration1 = adoptNS([[WKWebViewConfiguration alloc] init]);
    enableWebLocksAPI(configuration1.get());

    RetainPtr webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration1.get()]);
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

    RetainPtr configuration2 = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration2.get().processPool = [configuration1 processPool];
    enableWebLocksAPI(configuration2.get());
    if (shouldUseSameProcess == ShouldUseSameProcess::Yes) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        configuration2.get()._relatedWebView = webView1.get();
        ALLOW_DEPRECATED_DECLARATIONS_END
    }
    RetainPtr webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration2.get()]);
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

    RetainPtr processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().processSwapsOnNavigation = NO;
    RetainPtr processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().processPool = processPool.get();
    enableWebLocksAPI(configuration.get());

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    __block bool done = false;
    [webView performAfterReceivingAnyMessage:^(NSString *message) {
        EXPECT_WK_STREQ(message, @"LOADED");
        done = true;
    }];

    [webView synchronouslyLoadRequest:server1.request()];
    TestWebKitAPI::Util::run(&done);
}

TEST(WebLocks, CrossSiteIframeUsingLocksInServiceWorkerHostingProcess)
{
    TestWebKitAPI::HTTPServer server1({ }, TestWebKitAPI::HTTPServer::Protocol::Http);
    TestWebKitAPI::HTTPServer server2({ }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto swScript =
        "self.addEventListener('install', e => self.skipWaiting());"
        "self.addEventListener('activate', e => e.waitUntil(self.clients.claim()));"_s;

    // navigator.locks.request calls WebLockRegistryProxy::requestLock.
    auto iframeHTML =
        "<script>"
        "  navigator.locks.request('iframe-lock', () => new Promise(() => {}));"
        "  parent.postMessage('IFRAME_READY', '*');"
        "</script>"_s;

    auto pageHTML = makeString(
        "<script>"
        "  window.addEventListener('message', e => {"
        "    if (e.data === 'IFRAME_READY')"
        "      webkit.messageHandlers.testHandler.postMessage('IFRAME_READY');"
        "  });"
        "  navigator.serviceWorker.register('/sw.js')"
        "    .then(() => navigator.serviceWorker.ready)"
        "    .then(() => {"
        "      const f = document.createElement('iframe');"
        "      f.id = 'subframe';"
        "      f.src = 'http://localhost:"_s, server2.port(), "/iframe';"
        "      document.body.appendChild(f);"
        "    });"
        "</script>"_s);

    server1.addResponse("/"_s, { pageHTML });
    server1.addResponse("/sw.js"_s, { { { "Content-Type"_s, "text/javascript"_s } }, swScript });
    server2.addResponse("/iframe"_s, { iframeHTML });

    RetainPtr processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().processSwapsOnNavigation = NO;
    RetainPtr processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().processPool = processPool.get();
    enableWebLocksAPI(configuration.get());

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    __block bool done = false;
    __block bool webProcessTerminated = false;
    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    navigationDelegate.get().webContentProcessDidTerminate = ^(WKWebView *, _WKProcessTerminationReason) {
        webProcessTerminated = true;
        done = true;
    };
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView performAfterReceivingAnyMessage:^(NSString *message) {
        if ([message isEqualToString:@"IFRAME_READY"])
            done = true;
    }];

    [webView loadRequest:server1.request()];
    TestWebKitAPI::Util::run(&done);

    // Wait long enough for webContentProcessDidTerminate to fire.
    TestWebKitAPI::Util::runFor(0.1_s);
    EXPECT_FALSE(webProcessTerminated);

    // If the WebProcess was terminated by requestLock's MESSAGE_CHECK, we
    // have already failed and don't need to check clientIsGoingAway.
    if (webProcessTerminated)
        return;

    // Detaching the iframe will call WebLockRegistryProxy::clientIsGoingAway.
    __block bool detachDone = false;
    [webView evaluateJavaScript:@"document.getElementById('subframe').remove(); 1" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
        detachDone = true;
    }];
    TestWebKitAPI::Util::run(&detachDone);

    // Wait long enough for webContentProcessDidTerminate to fire.
    TestWebKitAPI::Util::runFor(0.1_s);
    EXPECT_FALSE(webProcessTerminated);
}

TEST(WebLocks, CrossSiteIframeUsingLocksInsideAboutBlankPopup)
{
    TestWebKitAPI::HTTPServer server1({ }, TestWebKitAPI::HTTPServer::Protocol::Http);
    TestWebKitAPI::HTTPServer server2({ }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto iframeHTML =
        "<script>"
        "  navigator.locks.request('iframe-lock', () => new Promise(() => {}));"
        "</script>"_s;

    // Opener page: opens an about:blank popup (which inherits server1's origin),
    // then injects a cross-site iframe pointing at server2 into that popup.
    auto openerHTML = makeString(
        "<script>"
        "  function runTest() {"
        "    const popup = window.open('');"
        "    window.popupRef = popup;"
        "    const f = popup.document.createElement('iframe');"
        "    f.id = 'subframe';"
        "    f.src = 'http://localhost:"_s, server2.port(), "/iframe';"
        "    f.onload = () => webkit.messageHandlers.testHandler.postMessage('IFRAME_READY');"
        "    popup.document.body.appendChild(f);"
        "  }"
        "</script>"_s);

    server1.addResponse("/"_s, { openerHTML });
    server2.addResponse("/iframe"_s, { iframeHTML });

    // No process swap on navigation keeps the popup in the opener's process.
    RetainPtr processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().processSwapsOnNavigation = NO;
    RetainPtr processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().processPool = processPool.get();
    enableWebLocksAPI(configuration.get());
    configuration.get().preferences.javaScriptCanOpenWindowsAutomatically = YES;

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    __block RetainPtr<TestWKWebView> popupWebView;
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().createWebViewWithConfiguration = ^WKWebView *(WKWebViewConfiguration *cfg, WKNavigationAction *, WKWindowFeatures *) {
        popupWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:cfg]);
        return popupWebView.get();
    };
    [webView setUIDelegate:uiDelegate.get()];

    __block bool done = false;
    __block bool webProcessTerminated = false;
    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    navigationDelegate.get().webContentProcessDidTerminate = ^(WKWebView *, _WKProcessTerminationReason) {
        webProcessTerminated = true;
        done = true;
    };
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView performAfterReceivingAnyMessage:^(NSString *message) {
        if ([message isEqualToString:@"IFRAME_READY"])
            done = true;
    }];

    [webView synchronouslyLoadRequest:server1.request()];
    [webView evaluateJavaScript:@"runTest()" completionHandler:nil];
    TestWebKitAPI::Util::run(&done);

    // Wait long enough for webContentProcessDidTerminate to fire.
    TestWebKitAPI::Util::runFor(0.1_s);
    EXPECT_FALSE(webProcessTerminated);

    // If the WebProcess was terminated by requestLock's MESSAGE_CHECK, we
    // have already failed and don't need to check clientIsGoingAway.
    if (webProcessTerminated)
        return;

    // Detaching the iframe will call WebLockRegistryProxy::clientIsGoingAway.
    __block bool detachDone = false;
    [webView evaluateJavaScript:@"window.popupRef.document.getElementById('subframe').remove(); 1" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
        detachDone = true;
    }];
    TestWebKitAPI::Util::run(&detachDone);

    // Wait long enough for webContentProcessDidTerminate to fire.
    TestWebKitAPI::Util::runFor(0.1_s);
    EXPECT_FALSE(webProcessTerminated);
}

} // namespace TestWebKitAPI
