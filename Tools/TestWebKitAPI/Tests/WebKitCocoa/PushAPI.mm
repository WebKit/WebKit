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
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>

static bool done;
static String expectedMessage;

@interface PushAPIMessageHandlerWithExpectedMessage : NSObject <WKScriptMessageHandler>
@end

@implementation PushAPIMessageHandlerWithExpectedMessage
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    EXPECT_WK_STREQ(message.body, expectedMessage);
    done = true;
}
@end

// FIXME: Update the test to do subscription before pushing message.
static const char* mainBytes = R"SWRESOURCE(
<script>
function log(msg)
{
    window.webkit.messageHandlers.sw.postMessage(msg);
}

const channel = new MessageChannel();
channel.port1.onmessage = (event) => log(event.data);

navigator.serviceWorker.register('/sw.js').then((registration) => {
    if (registration.active) {
        registration.active.postMessage({port: channel.port2}, [channel.port2]);
        return;
    }
    worker = registration.installing;
    worker.addEventListener('statechange', function() {
        if (worker.state == 'activated')
            worker.postMessage({port: channel.port2}, [channel.port2]);
    });
}).catch(function(error) {
    log("Registration failed with: " + error);
});
</script>
)SWRESOURCE";

static const char* scriptBytes = R"SWRESOURCE(
let port;
self.addEventListener("message", (event) => {
    port = event.data.port;
    port.postMessage("Ready");
});
self.addEventListener("push", (event) => {
    try {
        if (!event.data) {
            port.postMessage("Received: null data");
            return;
        }
        const value = event.data.text();
        port.postMessage("Received: " + value);
        if (value != 'Sweet Potatoes')
            event.waitUntil(Promise.reject('I want sweet potatoes'));
    } catch (e) {
        port.postMessage("Got exception " + e);
    }
});
)SWRESOURCE";

static void clearWebsiteDataStore(WKWebsiteDataStore *store)
{
    __block bool clearedStore = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        clearedStore = true;
    }];
    TestWebKitAPI::Util::run(&clearedStore);
}

static bool pushMessageProcessed = false;
static bool pushMessageSuccessful = false;

static bool waitUntilEvaluatesToTrue(const Function<bool()>& f)
{
    unsigned timeout = 0;
    do {
        if (f())
            return true;
        TestWebKitAPI::Util::sleep(0.1);
    } while (++timeout < 100);
    return false;
}

TEST(PushAPI, firePushEvent)
{
    TestWebKitAPI::HTTPServer server({
        { "/", { mainBytes } },
        { "/sw.js", { {{ "Content-Type", "application/javascript" }}, scriptBytes } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto messageHandler = adoptNS([[PushAPIMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    clearWebsiteDataStore([configuration websiteDataStore]);

    expectedMessage = "Ready";
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);

    done = false;
    pushMessageProcessed = false;
    pushMessageSuccessful = false;
    NSString *message = @"Sweet Potatoes";
    expectedMessage = "Received: Sweet Potatoes";
    [[configuration websiteDataStore] _processPushMessage:[message dataUsingEncoding:NSUTF8StringEncoding] registration:[server.request() URL] completionHandler:^(bool result) {
        pushMessageSuccessful = result;
        pushMessageProcessed = true;
    }];
    TestWebKitAPI::Util::run(&done);

    TestWebKitAPI::Util::run(&pushMessageProcessed);
    EXPECT_TRUE(pushMessageSuccessful);

    done = false;
    pushMessageProcessed = false;
    pushMessageSuccessful = false;
    message = @"Rotten Potatoes";
    expectedMessage = "Received: Rotten Potatoes";
    [[configuration websiteDataStore] _processPushMessage:[message dataUsingEncoding:NSUTF8StringEncoding] registration:[server.request() URL] completionHandler:^(bool result) {
        pushMessageSuccessful = result;
        pushMessageProcessed = true;
    }];
    TestWebKitAPI::Util::run(&done);

    TestWebKitAPI::Util::run(&pushMessageProcessed);
    EXPECT_FALSE(pushMessageSuccessful);

    clearWebsiteDataStore([configuration websiteDataStore]);
}

static const char* waitOneSecondScriptBytes = R"SWRESOURCE(
let port;
self.addEventListener("message", (event) => {
    port = event.data.port;
    port.postMessage("Ready");
});
self.addEventListener("push", (event) => {
    if (!event.data)
        return;
    const value = event.data.text();
    if (value === 'Sweet Potatoes')
        event.waitUntil(new Promise(resolve => setTimeout(resolve, 1000)));
    else if (value === 'Rotten Potatoes')
        event.waitUntil(new Promise((resolve, reject) => setTimeout(reject, 1000)));
    else if (value === 'Timeless Potatoes')
        event.waitUntil(new Promise(resolve => { }));
});
)SWRESOURCE";

static void terminateNetworkProcessWhileRegistrationIsStored(WKWebViewConfiguration *configuration)
{
    auto path = configuration.websiteDataStore._configuration._serviceWorkerRegistrationDirectory.path;
    NSURL* directory = [NSURL fileURLWithPath:path isDirectory:YES];
    NSURL *swDBPath = [directory URLByAppendingPathComponent:@"ServiceWorkerRegistrations-7.sqlite3"];
    unsigned timeout = 0;
    while (![[NSFileManager defaultManager] fileExistsAtPath:swDBPath.path] && ++timeout < 100)
        TestWebKitAPI::Util::sleep(0.1);
    // Let's close the SQL database.
    [configuration.websiteDataStore _sendNetworkProcessWillSuspendImminently];
    [configuration.websiteDataStore _terminateNetworkProcess];
}

TEST(PushAPI, firePushEventWithNoPagesSuccessful)
{
    TestWebKitAPI::HTTPServer server({
        { "/", { mainBytes } },
        { "/sw.js", { {{ "Content-Type", "application/javascript" }}, waitOneSecondScriptBytes } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    clearWebsiteDataStore([configuration websiteDataStore]);

    auto messageHandler = adoptNS([[PushAPIMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    expectedMessage = "Ready";
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);

    [webView _close];
    webView = nullptr;

    terminateNetworkProcessWhileRegistrationIsStored(configuration.get());

    // Push event for service worker without any related page, should succeed.
    pushMessageProcessed = false;
    pushMessageSuccessful = false;
    NSString *message = @"Sweet Potatoes";
    [[configuration websiteDataStore] _processPushMessage:[message dataUsingEncoding:NSUTF8StringEncoding] registration:[server.request() URL] completionHandler:^(bool result) {
        pushMessageSuccessful = result;
        pushMessageProcessed = true;
    }];

    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] { return [[configuration websiteDataStore] _hasServiceWorkerBackgroundActivityForTesting]; }));

    TestWebKitAPI::Util::run(&pushMessageProcessed);
    EXPECT_TRUE(pushMessageSuccessful);

    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] { return ![[configuration websiteDataStore] _hasServiceWorkerBackgroundActivityForTesting]; }));

    clearWebsiteDataStore([configuration websiteDataStore]);
}

TEST(PushAPI, firePushEventWithNoPagesFail)
{
    TestWebKitAPI::HTTPServer server({
        { "/", { mainBytes } },
        { "/sw.js", { {{ "Content-Type", "application/javascript" }}, waitOneSecondScriptBytes } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    clearWebsiteDataStore([configuration websiteDataStore]);

    auto messageHandler = adoptNS([[PushAPIMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    expectedMessage = "Ready";
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);

    [webView _close];
    webView = nullptr;

    terminateNetworkProcessWhileRegistrationIsStored(configuration.get());

    // Push event for service worker without any related page, should fail.
    pushMessageProcessed = false;
    pushMessageSuccessful = false;
    NSString *message = @"Rotten Potatoes";
    [[configuration websiteDataStore] _processPushMessage:[message dataUsingEncoding:NSUTF8StringEncoding] registration:[server.request() URL] completionHandler:^(bool result) {
        pushMessageSuccessful = result;
        pushMessageProcessed = true;
    }];

    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] { return [[configuration websiteDataStore] _hasServiceWorkerBackgroundActivityForTesting]; }));

    TestWebKitAPI::Util::run(&pushMessageProcessed);
    EXPECT_FALSE(pushMessageSuccessful);
    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] { return ![[configuration websiteDataStore] _hasServiceWorkerBackgroundActivityForTesting]; }));

    clearWebsiteDataStore([configuration websiteDataStore]);
}

TEST(PushAPI, firePushEventWithNoPagesTimeout)
{
    TestWebKitAPI::HTTPServer server({
        { "/", { mainBytes } },
        { "/sw.js", { {{ "Content-Type", "application/javascript" }}, waitOneSecondScriptBytes } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Disable service worker delay for the purpose of testing, push event should timeout after 1 second.
    auto dataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().websiteDataStore = dataStore.get();
    clearWebsiteDataStore([configuration websiteDataStore]);

    auto messageHandler = adoptNS([[PushAPIMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    expectedMessage = "Ready";
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);

    [webView _close];
    webView = nullptr;

    [dataStoreConfiguration setServiceWorkerProcessTerminationDelayEnabled:NO];
    dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);
    configuration.get().websiteDataStore = dataStore.get();

    terminateNetworkProcessWhileRegistrationIsStored(configuration.get());

    // Push event for service worker without any related page, should timeout so fail.
    pushMessageProcessed = false;
    pushMessageSuccessful = false;
    NSString *message = @"Timeless Potatoes";
    [[configuration websiteDataStore] _processPushMessage:[message dataUsingEncoding:NSUTF8StringEncoding] registration:[server.request() URL] completionHandler:^(bool result) {
        pushMessageSuccessful = result;
        pushMessageProcessed = true;
    }];

    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] { return [[configuration websiteDataStore] _hasServiceWorkerBackgroundActivityForTesting]; }));

    TestWebKitAPI::Util::run(&pushMessageProcessed);
    EXPECT_FALSE(pushMessageSuccessful);
    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] { return ![[configuration websiteDataStore] _hasServiceWorkerBackgroundActivityForTesting]; }));

    clearWebsiteDataStore([configuration websiteDataStore]);
}
