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

#if ENABLE(NOTIFICATIONS) && ENABLE(NOTIFICATION_EVENT)

#import "DeprecatedGlobalValues.h"
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNotificationProvider.h"
#import "TestWKWebView.h"
#import <WebCore/RegistrationDatabase.h>
#import <WebKit/WKNotificationProvider.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/HexNumber.h>

static NSDictionary *messageDictionary(NSData *data, NSURL *registration)
{
    return @{
        @"WebKitPushData" : data,
        @"WebKitPushRegistrationURL" : registration
    };
}

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
static constexpr auto mainBytes = R"SWRESOURCE(
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
)SWRESOURCE"_s;

static constexpr auto scriptBytes = R"SWRESOURCE(
let port;
self.addEventListener("message", (event) => {
    port = event.data.port;
    port.postMessage("Ready");
});
self.addEventListener("push", (event) => {
    self.registration.showNotification("notification");
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
)SWRESOURCE"_s;

static void clearWebsiteDataStore(WKWebsiteDataStore *store)
{
    __block bool clearedStore = false;
    [store removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
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
        TestWebKitAPI::Util::runFor(0.1_s);
    } while (++timeout < 100);
    return false;
}

TEST(PushAPI, firePushEvent)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, scriptBytes } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto provider = TestWebKitAPI::TestNotificationProvider({ [[configuration processPool] _notificationManagerForTesting], WKNotificationManagerGetSharedServiceWorkerNotificationManager() });
    provider.setPermission(server.origin(), true);

    auto messageHandler = adoptNS([[PushAPIMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    clearWebsiteDataStore([configuration websiteDataStore]);

    expectedMessage = "Ready"_s;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);

    done = false;
    pushMessageProcessed = false;
    pushMessageSuccessful = false;
    NSString *message = @"Sweet Potatoes";
    expectedMessage = "Received: Sweet Potatoes"_s;

    [[configuration websiteDataStore] _processPushMessage:messageDictionary([message dataUsingEncoding:NSUTF8StringEncoding], [server.request() URL]) completionHandler:^(bool result) {
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
    expectedMessage = "Received: Rotten Potatoes"_s;
    [[configuration websiteDataStore] _processPushMessage:messageDictionary([message dataUsingEncoding:NSUTF8StringEncoding], [server.request() URL]) completionHandler:^(bool result) {
        pushMessageSuccessful = result;
        pushMessageProcessed = true;
    }];
    TestWebKitAPI::Util::run(&done);

    TestWebKitAPI::Util::run(&pushMessageProcessed);
    EXPECT_FALSE(pushMessageSuccessful);

    clearWebsiteDataStore([configuration websiteDataStore]);
}

static constexpr auto waitOneSecondScriptBytes = R"SWRESOURCE(
let port;
self.addEventListener("message", (event) => {
    port = event.data.port;
    port.postMessage("Ready");
});
self.addEventListener("push", (event) => {
    self.registration.showNotification("notification");
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
)SWRESOURCE"_s;

static void terminateNetworkProcessWhileRegistrationIsStored(WKWebViewConfiguration *configuration)
{
    auto path = configuration.websiteDataStore._configuration._serviceWorkerRegistrationDirectory.path;
    NSURL* directory = [NSURL fileURLWithPath:path isDirectory:YES];
    auto filename = makeString("ServiceWorkerRegistrations-"_s, WebCore::RegistrationDatabase::schemaVersion, ".sqlite3");
    NSURL *swDBPath = [directory URLByAppendingPathComponent:filename];
    unsigned timeout = 0;
    while (![[NSFileManager defaultManager] fileExistsAtPath:swDBPath.path] && ++timeout < 100)
        TestWebKitAPI::Util::runFor(0.1_s);
    // Let's close the SQL database.
    [configuration.websiteDataStore _sendNetworkProcessWillSuspendImminently];
    [configuration.websiteDataStore _terminateNetworkProcess];
}

TEST(PushAPI, firePushEventWithNoPagesSuccessful)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, waitOneSecondScriptBytes } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    clearWebsiteDataStore([configuration websiteDataStore]);

    auto provider = TestWebKitAPI::TestNotificationProvider({ [[configuration processPool] _notificationManagerForTesting], WKNotificationManagerGetSharedServiceWorkerNotificationManager() });
    provider.setPermission(server.origin(), true);

    auto messageHandler = adoptNS([[PushAPIMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    expectedMessage = "Ready"_s;
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
    [[configuration websiteDataStore] _processPushMessage:messageDictionary([message dataUsingEncoding:NSUTF8StringEncoding], [server.request() URL]) completionHandler:^(bool result) {
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
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, waitOneSecondScriptBytes } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    clearWebsiteDataStore([configuration websiteDataStore]);

    auto provider = TestWebKitAPI::TestNotificationProvider({ [[configuration processPool] _notificationManagerForTesting], WKNotificationManagerGetSharedServiceWorkerNotificationManager() });
    provider.setPermission(server.origin(), true);

    auto messageHandler = adoptNS([[PushAPIMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    expectedMessage = "Ready"_s;
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
    [[configuration websiteDataStore] _processPushMessage:messageDictionary([message dataUsingEncoding:NSUTF8StringEncoding], [server.request() URL]) completionHandler:^(bool result) {
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
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, waitOneSecondScriptBytes } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Disable service worker delay for the purpose of testing, push event should timeout after 1 second.
    auto dataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [dataStoreConfiguration setServiceWorkerProcessTerminationDelayEnabled:NO];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().websiteDataStore = dataStore.get();
    clearWebsiteDataStore([configuration websiteDataStore]);

    auto provider = TestWebKitAPI::TestNotificationProvider({ [[configuration processPool] _notificationManagerForTesting], WKNotificationManagerGetSharedServiceWorkerNotificationManager() });
    provider.setPermission(server.origin(), true);

    auto messageHandler = adoptNS([[PushAPIMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    expectedMessage = "Ready"_s;

    @autoreleasepool {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        [webView loadRequest:server.request()];
        TestWebKitAPI::Util::run(&done);
    }

    terminateNetworkProcessWhileRegistrationIsStored(configuration.get());

    // Push event for service worker without any related page, should timeout so fail.
    pushMessageProcessed = false;
    pushMessageSuccessful = false;
    NSString *message = @"Timeless Potatoes";
    [[configuration websiteDataStore] _processPushMessage:messageDictionary([message dataUsingEncoding:NSUTF8StringEncoding], [server.request() URL]) completionHandler:^(bool result) {
        pushMessageSuccessful = result;
        pushMessageProcessed = true;
    }];

    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] { return [[configuration websiteDataStore] _hasServiceWorkerBackgroundActivityForTesting]; }));

    TestWebKitAPI::Util::run(&pushMessageProcessed);
    EXPECT_FALSE(pushMessageSuccessful);
    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] { return ![[configuration websiteDataStore] _hasServiceWorkerBackgroundActivityForTesting]; }));

    clearWebsiteDataStore([configuration websiteDataStore]);
}

#if WK_HAVE_C_SPI

static constexpr auto pushEventsAndInspectedServiceWorkerScriptBytes = R"SWRESOURCE(
let port;
self.addEventListener("message", (event) => {
    port = event.data.port;
    port.postMessage(self.internals ? "Ready" : "No internals");
});
self.addEventListener("push", (event) => {
    self.registration.showNotification("notification");
    if (event.data.text() === 'first') {
        self.internals.setAsInspected(true);
        self.previousMessageData = 'first';
        return;
    }
    if (event.data.text() === 'second') {
        self.internals.setAsInspected(false);
        if (self.previousMessageData !== 'first')
            event.waitUntil(Promise.reject('expected first'));
        return;
    }
    if (event.data.text() === 'third') {
        if (self.previousMessageData !== 'second')
            event.waitUntil(Promise.reject('expected second'));
        return;
    }
    if (event.data.text() === 'fourth') {
        if (self.previousMessageData !== undefined)
            event.waitUntil(Promise.reject('expected undefined'));
        return;
    }
    self.previousMessageData = event.data.text();
    event.waitUntil(Promise.reject('expected a known message'));
});
)SWRESOURCE"_s;

TEST(PushAPI, pushEventsAndInspectedServiceWorker)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, pushEventsAndInspectedServiceWorkerScriptBytes } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    clearWebsiteDataStore([configuration websiteDataStore]);

    auto context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    [configuration setProcessPool:(WKProcessPool *)context.get()];

    auto provider = TestWebKitAPI::TestNotificationProvider({ [[configuration processPool] _notificationManagerForTesting], WKNotificationManagerGetSharedServiceWorkerNotificationManager() });
    provider.setPermission(server.origin(), true);

    auto messageHandler = adoptNS([[PushAPIMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    expectedMessage = "Ready"_s;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);

    [webView _close];
    webView = nullptr;

    terminateNetworkProcessWhileRegistrationIsStored(configuration.get());

    // Push event for service worker without any related page.
    pushMessageProcessed = false;
    pushMessageSuccessful = false;
    NSString *message = @"first";
    [[configuration websiteDataStore] _processPushMessage:messageDictionary([message dataUsingEncoding:NSUTF8StringEncoding], [server.request() URL]) completionHandler:^(bool result) {
        pushMessageSuccessful = result;
        pushMessageProcessed = true;
    }];
    TestWebKitAPI::Util::run(&pushMessageProcessed);
    EXPECT_TRUE(pushMessageSuccessful);

    pushMessageProcessed = false;
    pushMessageSuccessful = false;
    message = @"second";
    [[configuration websiteDataStore] _processPushMessage:messageDictionary([message dataUsingEncoding:NSUTF8StringEncoding], [server.request() URL]) completionHandler:^(bool result) {
        pushMessageSuccessful = result;
        pushMessageProcessed = true;
    }];
    TestWebKitAPI::Util::run(&pushMessageProcessed);
    EXPECT_TRUE(pushMessageSuccessful);

    pushMessageProcessed = false;
    pushMessageSuccessful = false;
    message = @"third";
    [[configuration websiteDataStore] _processPushMessage:messageDictionary([message dataUsingEncoding:NSUTF8StringEncoding], [server.request() URL]) completionHandler:^(bool result) {
        pushMessageSuccessful = result;
        pushMessageProcessed = true;
    }];
    TestWebKitAPI::Util::run(&pushMessageProcessed);
    EXPECT_FALSE(pushMessageSuccessful);

    // We delay so that the timer to terminate service worker kicks in.
    sleep(3);

    pushMessageProcessed = false;
    pushMessageSuccessful = false;
    message = @"fourth";
    [[configuration websiteDataStore] _processPushMessage:messageDictionary([message dataUsingEncoding:NSUTF8StringEncoding], [server.request() URL]) completionHandler:^(bool result) {
        pushMessageSuccessful = result;
        pushMessageProcessed = true;
    }];
    TestWebKitAPI::Util::run(&pushMessageProcessed);
    EXPECT_TRUE(pushMessageSuccessful);
}

static constexpr auto inspectedServiceWorkerWithoutPageScriptBytes = R"SWRESOURCE(
let port;
self.addEventListener("message", (event) => {
    port = event.data.port;
    port.postMessage(self.internals ? "Ready" : "No internals");
});
self.addEventListener("push", async (event) => {
    self.registration.showNotification("notification");
    if (event.data.text() === 'firstmessage-inspected' || event.data.text() === 'firstmessage-not-inspected') {
        if (event.data.text() === 'firstmessage-inspected')
            self.internals.setAsInspected(true);
        if (self.previousMessageData !== undefined)
            event.waitUntil(Promise.reject('unexpected state with inspected message'));
        self.previousMessageData = 'inspected';
        // Wait for client to go away before resolving the event promise;
        let resolve;
        event.waitUntil(new Promise(r => resolve = r));
        while (true) {
            const clients = await self.clients.matchAll({ includeUncontrolled : true });
            if (!clients.length)
                resolve();
        }
        return;
    }
    if (event.data.text() === 'not inspected') {
        setTimeout(() => self.internals.setAsInspected(false), 10);
        if (self.previousMessageData !== 'inspected')
            event.waitUntil(Promise.reject('unexpected state with not inspected message'));
        self.previousMessageData = 'not inspected';
        return;
    }
    if (event.data.text() === 'last') {
        if (self.previousMessageData !== undefined)
            event.waitUntil(Promise.reject('unexpected state with last message'));
    }
});
)SWRESOURCE"_s;

static void testInspectedServiceWorkerWithoutPage(bool enableServiceWorkerInspection)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, inspectedServiceWorkerWithoutPageScriptBytes } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto dataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [dataStoreConfiguration setServiceWorkerProcessTerminationDelayEnabled:NO];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().websiteDataStore = dataStore.get();
    clearWebsiteDataStore([configuration websiteDataStore]);

    auto context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    [configuration setProcessPool:(WKProcessPool *)context.get()];

    auto provider = TestWebKitAPI::TestNotificationProvider({ [[configuration processPool] _notificationManagerForTesting], WKNotificationManagerGetSharedServiceWorkerNotificationManager() });
    provider.setPermission(server.origin(), true);

    auto messageHandler = adoptNS([[PushAPIMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    expectedMessage = "Ready"_s;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);

    // Push event for service worker without any related page.
    pushMessageProcessed = false;
    pushMessageSuccessful = false;
    NSString *message = @"firstmessage-inspected";
    if (!enableServiceWorkerInspection)
        message = @"firstmessage-not-inspected";
    [[configuration websiteDataStore] _processPushMessage:messageDictionary([message dataUsingEncoding:NSUTF8StringEncoding], [server.request() URL]) completionHandler:^(bool result) {
        pushMessageSuccessful = result;
        pushMessageProcessed = true;
    }];

    // We delay so that the push message will happen before the unregistration of the service worker client.
    sleep(0.5);

    // Closing the web view should not terminate the service worker as service worker is inspected.
    [webView _close];
    webView = nullptr;

    TestWebKitAPI::Util::run(&pushMessageProcessed);
    EXPECT_TRUE(pushMessageSuccessful);

    // We delay so that the timer to terminate service worker kicks in, at most up to the max push message allowed time, aka 2 seconds.
    sleep(3);

    // Send message at which point the service worker will not be inspected anymore and will be closed.
    pushMessageProcessed = false;
    pushMessageSuccessful = false;
    message = @"not inspected";
    [[configuration websiteDataStore] _processPushMessage:messageDictionary([message dataUsingEncoding:NSUTF8StringEncoding], [server.request() URL]) completionHandler:^(bool result) {
        pushMessageSuccessful = result;
        pushMessageProcessed = true;
    }];
    TestWebKitAPI::Util::run(&pushMessageProcessed);
    EXPECT_EQ(pushMessageSuccessful, enableServiceWorkerInspection);

    // We delay so that the timer to terminate service worker kicks in, at most up to the max push message allowed time, aka 2 seconds.
    sleep(3);

    pushMessageProcessed = false;
    pushMessageSuccessful = false;
    message = @"last";
    [[configuration websiteDataStore] _processPushMessage:messageDictionary([message dataUsingEncoding:NSUTF8StringEncoding], [server.request() URL]) completionHandler:^(bool result) {
        pushMessageSuccessful = result;
        pushMessageProcessed = true;
    }];

    TestWebKitAPI::Util::run(&pushMessageProcessed);
    EXPECT_TRUE(pushMessageSuccessful);
}

TEST(PushAPI, inspectedServiceWorkerWithoutPage)
{
    bool enableServiceWorkerInspection = true;
    testInspectedServiceWorkerWithoutPage(enableServiceWorkerInspection);
}

TEST(PushAPI, uninspectedServiceWorkerWithoutPage)
{
    bool enableServiceWorkerInspection = false;
    testInspectedServiceWorkerWithoutPage(enableServiceWorkerInspection);
}

static constexpr auto fireNotificationClickEventMainBytes = R"SWRESOURCE(
<script>
function log(msg)
{
    window.webkit.messageHandlers.sw.postMessage(msg);
}

const channel = new MessageChannel();
channel.port1.onmessage = (event) => log(event.data);
navigator.serviceWorker.onmessage = (event) => log(event.data);

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
)SWRESOURCE"_s;

static constexpr auto fireNotificationClickEventScriptBytes = R"SWRESOURCE(
let port;
self.addEventListener("message", (event) => {
    port = event.data.port;
    port.postMessage("Ready");
});
self.addEventListener("push", (event) => {
    self.registration.showNotification("notification");
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
self.addEventListener("notificationclick", async (event) => {
    for (let client of await self.clients.matchAll({includeUncontrolled:true}))
        client.postMessage("Received notificationclick");
});
)SWRESOURCE"_s;

TEST(PushAPI, fireNotificationClickEvent)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { fireNotificationClickEventMainBytes } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, fireNotificationClickEventScriptBytes } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto provider = TestWebKitAPI::TestNotificationProvider({ [[configuration processPool] _notificationManagerForTesting], WKNotificationManagerGetSharedServiceWorkerNotificationManager() });
    provider.setPermission(server.origin(), true);

    auto messageHandler = adoptNS([[PushAPIMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    clearWebsiteDataStore([configuration websiteDataStore]);

    expectedMessage = "Ready"_s;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);

    provider.resetHasReceivedNotification();
    auto& providerRef = provider;

    done = false;
    pushMessageProcessed = false;
    pushMessageSuccessful = false;
    NSString *message = @"Sweet Potatoes";
    expectedMessage = "Received: Sweet Potatoes"_s;

    [[configuration websiteDataStore] _processPushMessage:messageDictionary([message dataUsingEncoding:NSUTF8StringEncoding], [server.request() URL]) completionHandler:^(bool result) {
        EXPECT_TRUE(providerRef.hasReceivedNotification());
        pushMessageSuccessful = result;
        pushMessageProcessed = true;
    }];
    TestWebKitAPI::Util::run(&done);

    TestWebKitAPI::Util::run(&pushMessageProcessed);
    EXPECT_TRUE(pushMessageSuccessful);

    terminateNetworkProcessWhileRegistrationIsStored(configuration.get());

    EXPECT_TRUE(provider.simulateNotificationClick());

    done = false;
    expectedMessage = "Received notificationclick"_s;
    TestWebKitAPI::Util::run(&done);

    clearWebsiteDataStore([configuration websiteDataStore]);
}

#endif // WK_HAVE_C_SPI

#endif // ENABLE(NOTIFICATIONS) && ENABLE(NOTIFICATION_EVENT)
