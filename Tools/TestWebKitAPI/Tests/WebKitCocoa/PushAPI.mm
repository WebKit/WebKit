/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#import <WebCore/SWRegistrationDatabase.h>
#import <WebKit/WKNotificationProvider.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKNotificationData.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <WebKit/_WKWebsiteDataStoreDelegate.h>
#import <wtf/HexNumber.h>

static NSDictionary *messageDictionary(NSData *data, NSURL *registration)
{
    return @{
        @"WebKitPushData" : data,
        @"WebKitPushRegistrationURL" : registration,
        @"WebKitPushPartition" : @"TestWebKitAPI",
        @"WebKitNotificationPayload" : [NSNull null]
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

static RetainPtr<WKWebViewConfiguration> createConfigurationWithNotificationsEnabled()
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration preferences] _setNotificationsEnabled:YES];
    [[configuration preferences] _setNotificationEventEnabled:YES];
    return configuration;
}

TEST(PushAPI, firePushEvent)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, scriptBytes } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto configuration = createConfigurationWithNotificationsEnabled();

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

@interface FirePushEventDataStoreDelegate : NSObject <_WKWebsiteDataStoreDelegate>
@property (readwrite, copy) NSDictionary<NSString *, NSNumber *> *permissions;
@property (readonly, copy) NSMutableArray<_WKNotificationData *> *displayedNotifications;
@end

@implementation FirePushEventDataStoreDelegate

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    _displayedNotifications = [[NSMutableArray alloc] init];

    return self;
}

- (NSDictionary<NSString *, NSNumber *> *)notificationPermissionsForWebsiteDataStore:(WKWebsiteDataStore *)dataStore
{
    return _permissions;
}

- (void)websiteDataStore:(WKWebsiteDataStore *)dataStore showNotification:(_WKNotificationData *)notificationData
{
    [_displayedNotifications addObject:notificationData];
}

- (void)websiteDataStore:(WKWebsiteDataStore *)dataStore getDisplayedNotificationsForWorkerOrigin:(WKSecurityOrigin *)workerOrigin completionHandler:(void (^)(NSArray<NSDictionary *> *))completionHandler
{

    NSMutableArray *notifications = [NSMutableArray new];
    for (id notification in _displayedNotifications)
        [notifications addObject:[notification dictionaryRepresentation]];

    completionHandler(notifications);
}

- (void)dealloc
{
    [_displayedNotifications release];
    [super dealloc];
}
@end

TEST(PushAPI, notificationPermissionsDelegateInvalidResponse)
{
    auto configuration = createConfigurationWithNotificationsEnabled();
    RetainPtr<FirePushEventDataStoreDelegate> delegate = adoptNS([FirePushEventDataStoreDelegate new]);
    delegate.get().permissions = @{
        @":" : @YES
    };
    [configuration websiteDataStore]._delegate = delegate.get();
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get() addToWindow:NO]);

    // If the WebView completes the page load successfully, the test passes.
    [webView synchronouslyLoadHTMLString:@"Hello"];
}

TEST(PushAPI, firePushEventDataStoreDelegate)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptBytes } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto messageHandler = adoptNS([[PushAPIMessageHandlerWithExpectedMessage alloc] init]);
    auto configuration = createConfigurationWithNotificationsEnabled();
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    clearWebsiteDataStore([configuration websiteDataStore]);

    RetainPtr<FirePushEventDataStoreDelegate> delegate = adoptNS([FirePushEventDataStoreDelegate new]);
    delegate.get().permissions = @{
        (NSString *)server.origin() : @YES
    };
    [configuration websiteDataStore]._delegate = delegate.get();

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

    EXPECT_TRUE([delegate.get().displayedNotifications.lastObject.title isEqualToString:@"notification"]);
    EXPECT_TRUE([delegate.get().displayedNotifications.lastObject.body isEqualToString:@""]);

    clearWebsiteDataStore([configuration websiteDataStore]);
}


static constexpr auto testSilentFlagScriptBytes = R"SWRESOURCE(
let port;
self.addEventListener("message", (event) => {
    port = event.data.port;
    port.postMessage("Ready");
    port.onmessage = (event) => {
        if (event.data == "getAllNotifications") {
            self.registration.getNotifications().then(notifications => {
                var result = "";
                notifications.forEach(notification => {
                    result += notification.title + " " + notification.silent + " " + notification.tag + " ";
                });
                port.postMessage(result);
            });
        } else {
            self.registration.getNotifications({ tag: event.data }).then(notifications => {
                var result = "";
                notifications.forEach(notification => {
                    result += notification.title + " " + notification.silent + " " + notification.tag + " ";
                });
                port.postMessage(result);
            });
        }
    };
});
self.addEventListener("push", (event) => {
    try {
        if (!event.data) {
            port.postMessage("Received: null data");
            return;
        }
        const value = event.data.text();
        if (value == "nothing")
            self.registration.showNotification("nothing", { tag: "nothingTag" });
        else if (value == "true")
            self.registration.showNotification("true", { silent: true, tag: "somethingTag" });
        else if (value == "false")
            self.registration.showNotification("false", { silent: false, tag: "somethingTag" });
        port.postMessage("Done");
    } catch (e) {
        port.postMessage("Got exception " + e);
    }
});
)SWRESOURCE"_s;

TEST(PushAPI, testSilentFlag)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, testSilentFlagScriptBytes } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto messageHandler = adoptNS([[PushAPIMessageHandlerWithExpectedMessage alloc] init]);
    auto configuration = createConfigurationWithNotificationsEnabled();
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    clearWebsiteDataStore([configuration websiteDataStore]);

    RetainPtr<FirePushEventDataStoreDelegate> delegate = adoptNS([FirePushEventDataStoreDelegate new]);
    delegate.get().permissions = @{
        (NSString *)server.origin() : @YES
    };
    [configuration websiteDataStore]._delegate = delegate.get();

    expectedMessage = "Ready"_s;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);

    expectedMessage = "Done"_s;

    done = false;
    pushMessageProcessed = false;
    pushMessageSuccessful = false;
    NSString *message = @"nothing";
    [[configuration websiteDataStore] _processPushMessage:messageDictionary([message dataUsingEncoding:NSUTF8StringEncoding], [server.request() URL]) completionHandler:^(bool result) {
        pushMessageSuccessful = result;
        pushMessageProcessed = true;
    }];
    TestWebKitAPI::Util::run(&done);
    TestWebKitAPI::Util::run(&pushMessageProcessed);

    EXPECT_TRUE(pushMessageSuccessful);
    EXPECT_TRUE([delegate.get().displayedNotifications.lastObject.title isEqualToString:@"nothing"]);
    EXPECT_EQ(delegate.get().displayedNotifications.lastObject.alert, _WKNotificationAlertDefault);
    EXPECT_TRUE(delegate.get().displayedNotifications.lastObject.userInfo[@"WebNotificationSilentKey"] == nil);

    done = false;
    pushMessageProcessed = false;
    pushMessageSuccessful = false;
    message = @"true";
    [[configuration websiteDataStore] _processPushMessage:messageDictionary([message dataUsingEncoding:NSUTF8StringEncoding], [server.request() URL]) completionHandler:^(bool result) {
        pushMessageSuccessful = result;
        pushMessageProcessed = true;
    }];
    TestWebKitAPI::Util::run(&done);
    TestWebKitAPI::Util::run(&pushMessageProcessed);

    EXPECT_TRUE(pushMessageSuccessful);
    EXPECT_TRUE([delegate.get().displayedNotifications.lastObject.title isEqualToString:@"true"]);
    EXPECT_EQ(delegate.get().displayedNotifications.lastObject.alert, _WKNotificationAlertSilent);
    EXPECT_TRUE([delegate.get().displayedNotifications.lastObject.userInfo[@"WebNotificationSilentKey"] isEqual:[NSNumber numberWithBool:YES]]);

    done = false;
    pushMessageProcessed = false;
    pushMessageSuccessful = false;
    message = @"false";
    [[configuration websiteDataStore] _processPushMessage:messageDictionary([message dataUsingEncoding:NSUTF8StringEncoding], [server.request() URL]) completionHandler:^(bool result) {
        pushMessageSuccessful = result;
        pushMessageProcessed = true;
    }];
    TestWebKitAPI::Util::run(&done);
    TestWebKitAPI::Util::run(&pushMessageProcessed);

    EXPECT_TRUE(pushMessageSuccessful);
    EXPECT_TRUE([delegate.get().displayedNotifications.lastObject.title isEqualToString:@"false"]);
    EXPECT_EQ(delegate.get().displayedNotifications.lastObject.alert, _WKNotificationAlertEnabled);
    EXPECT_TRUE([delegate.get().displayedNotifications.lastObject.userInfo[@"WebNotificationSilentKey"] isEqual:[NSNumber numberWithBool:NO]]);

    // Use the ServiceWorkerRegistration.getNotifications() API to grab them and verify the expected silent values
    static constexpr auto getPersistentNotificationsScript = R"SWRESOURCE(
    var oldOnMessage = channel.port1.onmessage;
    const promise = new Promise((resolve) => {
        channel.port1.onmessage = (event) => resolve(event.data);
    });

    channel.port1.postMessage(theMessage);

    await promise;
    channel.port1.onmessage = oldOnMessage;
    return promise;
    )SWRESOURCE";

    pushMessageProcessed = false;
    [webView callAsyncJavaScript:@(getPersistentNotificationsScript) arguments:@{ @"theMessage" : @"getAllNotifications" } inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isEqualToString:@"nothing null nothingTag true true somethingTag false false somethingTag "]);
        pushMessageProcessed = true;
    }];
    TestWebKitAPI::Util::run(&pushMessageProcessed);

    pushMessageProcessed = false;
    // Passing an empty tag will explicitly add an empty "tag" value to the getNotifications() call,
    // but the results should be the same as not specifying a tag at all like in the previous test.
    [webView callAsyncJavaScript:@(getPersistentNotificationsScript) arguments:@{ @"theMessage" : @"" } inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isEqualToString:@"nothing null nothingTag true true somethingTag false false somethingTag "]);
        pushMessageProcessed = true;
    }];
    TestWebKitAPI::Util::run(&pushMessageProcessed);

    pushMessageProcessed = false;
    [webView callAsyncJavaScript:@(getPersistentNotificationsScript) arguments:@{ @"theMessage" : @"nothingTag" } inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isEqualToString:@"nothing null nothingTag "]);
        pushMessageProcessed = true;
    }];
    TestWebKitAPI::Util::run(&pushMessageProcessed);

    pushMessageProcessed = false;
    [webView callAsyncJavaScript:@(getPersistentNotificationsScript) arguments:@{ @"theMessage" : @"somethingTag" } inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isEqualToString:@"true true somethingTag false false somethingTag "]);
        pushMessageProcessed = true;
    }];
    TestWebKitAPI::Util::run(&pushMessageProcessed);

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
    done = false;
    [configuration.websiteDataStore _storeServiceWorkerRegistrations:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    [configuration.websiteDataStore _terminateNetworkProcess];
}

TEST(PushAPI, firePushEventWithNoPagesSuccessful)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, waitOneSecondScriptBytes } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto configuration = createConfigurationWithNotificationsEnabled();
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

    auto configuration = createConfigurationWithNotificationsEnabled();
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

    auto configuration = createConfigurationWithNotificationsEnabled();
    configuration.get().websiteDataStore = dataStore.get();
    clearWebsiteDataStore([configuration websiteDataStore]);

    auto provider = TestWebKitAPI::TestNotificationProvider({ [[configuration processPool] _notificationManagerForTesting], WKNotificationManagerGetSharedServiceWorkerNotificationManager() });
    provider.setPermission(server.origin(), true);

    auto messageHandler = adoptNS([[PushAPIMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    expectedMessage = "Ready"_s;

    fprintf(stderr, "totot1\n");
    @autoreleasepool {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        [webView loadRequest:server.request()];
        TestWebKitAPI::Util::run(&done);
    }

    fprintf(stderr, "totot2\n");
    terminateNetworkProcessWhileRegistrationIsStored(configuration.get());

    // Push event for service worker without any related page, should timeout so fail.
    pushMessageProcessed = false;
    pushMessageSuccessful = false;
    NSString *message = @"Timeless Potatoes";
    fprintf(stderr, "totot3\n");
    [[configuration websiteDataStore] _processPushMessage:messageDictionary([message dataUsingEncoding:NSUTF8StringEncoding], [server.request() URL]) completionHandler:^(bool result) {
        fprintf(stderr, "totot3.1\n");
        pushMessageSuccessful = result;
        pushMessageProcessed = true;
    }];

    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] { return [[configuration websiteDataStore] _hasServiceWorkerBackgroundActivityForTesting]; }));

    TestWebKitAPI::Util::run(&pushMessageProcessed);
    fprintf(stderr, "totot4\n");
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

    auto configuration = createConfigurationWithNotificationsEnabled();
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

    auto configuration = createConfigurationWithNotificationsEnabled();
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
    self.registration.showNotification("notification", { silent: true });
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
        client.postMessage("Received notificationclick: " + event.notification.silent);
});
self.addEventListener("notificationclose", async (event) => {
    for (let client of await self.clients.matchAll({includeUncontrolled:true}))
        client.postMessage("Received notificationclose");
});
)SWRESOURCE"_s;

TEST(PushAPI, fireNotificationClickEvent)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { fireNotificationClickEventMainBytes } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, fireNotificationClickEventScriptBytes } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto configuration = createConfigurationWithNotificationsEnabled();

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
        EXPECT_TRUE(providerRef.hasReceivedShowNotification());
        pushMessageSuccessful = result;
        pushMessageProcessed = true;
    }];
    TestWebKitAPI::Util::run(&done);

    TestWebKitAPI::Util::run(&pushMessageProcessed);
    EXPECT_TRUE(pushMessageSuccessful);

    terminateNetworkProcessWhileRegistrationIsStored(configuration.get());

    EXPECT_TRUE(provider.simulateNotificationClick());

    done = false;
    expectedMessage = "Received notificationclick: true"_s;
    TestWebKitAPI::Util::run(&done);

    clearWebsiteDataStore([configuration websiteDataStore]);
}

TEST(PushAPI, fireNotificationCloseEvent)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { fireNotificationClickEventMainBytes } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, fireNotificationClickEventScriptBytes } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto configuration = createConfigurationWithNotificationsEnabled();

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
        EXPECT_TRUE(providerRef.hasReceivedShowNotification());
        pushMessageSuccessful = result;
        pushMessageProcessed = true;
    }];
    TestWebKitAPI::Util::run(&done);

    TestWebKitAPI::Util::run(&pushMessageProcessed);
    EXPECT_TRUE(pushMessageSuccessful);

    terminateNetworkProcessWhileRegistrationIsStored(configuration.get());

    EXPECT_TRUE(provider.simulateNotificationClose());

    done = false;
    expectedMessage = "Received notificationclose"_s;
    TestWebKitAPI::Util::run(&done);

    clearWebsiteDataStore([configuration websiteDataStore]);
}

static constexpr auto closeNotificationMainBytes = R"SWRESOURCE(
<script>
function log(msg)
{
    window.webkit.messageHandlers.sw.postMessage(msg);
}

const channel = new MessageChannel();
channel.port1.onmessage = (event) => log(event.data);
navigator.serviceWorker.onmessage = (event) => log(event.data);

let swRegistration;
navigator.serviceWorker.register('/sw.js').then((registration) => {
    swRegistration = registration;
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

function closeNotification()
{
    const notifications = swRegistration.getNotifications().then(notifications => {
        notifications[0].close();
        log("PASS close notification");
    });
}
</script>
)SWRESOURCE"_s;

static constexpr auto closeNotificationScriptBytes = R"SWRESOURCE(
let port;
self.addEventListener("message", (event) => {
    if (port)
        return;
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
    } catch (e) {
        port.postMessage("Got exception " + e);
    }
});
)SWRESOURCE"_s;

TEST(PushAPI, callNotificationClose)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { closeNotificationMainBytes } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, closeNotificationScriptBytes } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto configuration = createConfigurationWithNotificationsEnabled();

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
        EXPECT_TRUE(providerRef.hasReceivedShowNotification());
        pushMessageSuccessful = result;
        pushMessageProcessed = true;
    }];
    TestWebKitAPI::Util::run(&done);

    TestWebKitAPI::Util::run(&pushMessageProcessed);
    EXPECT_TRUE(pushMessageSuccessful);

    terminateNetworkProcessWhileRegistrationIsStored(configuration.get());

    [webView evaluateJavaScript:@"closeNotification()" completionHandler:nil];

    EXPECT_FALSE(providerRef.hasReceivedCloseNotification());

    done = false;
    expectedMessage = "PASS close notification"_s;
    TestWebKitAPI::Util::run(&done);

    int counter = 0;
    while (!providerRef.hasReceivedCloseNotification() && ++counter < 10)
        TestWebKitAPI::Util::spinRunLoop(10);

    EXPECT_LT(counter, 10);

    providerRef.resetHasReceivedNotification();

    clearWebsiteDataStore([configuration websiteDataStore]);
}

#endif // WK_HAVE_C_SPI

#endif // ENABLE(NOTIFICATIONS) && ENABLE(NOTIFICATION_EVENT)
