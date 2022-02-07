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

#import "DaemonTestUtilities.h"
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKExperimentalFeature.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <mach/mach_init.h>
#import <mach/task.h>

#if PLATFORM(MAC) || PLATFORM(IOS)

static bool alertReceived = false;
@interface NotificationPermissionDelegate : NSObject<WKUIDelegatePrivate>
@end

@implementation NotificationPermissionDelegate

- (void)_webView:(WKWebView *)webView requestNotificationPermissionForSecurityOrigin:(WKSecurityOrigin *)securityOrigin decisionHandler:(void (^)(BOOL))decisionHandler
{
    decisionHandler(true);
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    alertReceived = true;
    completionHandler();
}

@end

namespace TestWebKitAPI {

static RetainPtr<NSURL> testWebPushDaemonLocation()
{
    return [currentExecutableDirectory() URLByAppendingPathComponent:@"webpushd" isDirectory:NO];
}

static NSDictionary<NSString *, id> *testWebPushDaemonPList(NSURL *storageLocation)
{
    return @{
        @"Label" : @"org.webkit.webpushtestdaemon",
        @"LaunchOnlyOnce" : @YES,
        @"StandardErrorPath" : [storageLocation URLByAppendingPathComponent:@"daemon_stderr"].path,
        @"EnvironmentVariables" : @{ @"DYLD_FRAMEWORK_PATH" : currentExecutableDirectory().get().path },
        @"MachServices" : @{ @"org.webkit.webpushtestdaemon.service" : @YES },
        @"ProgramArguments" : @[
            testWebPushDaemonLocation().get().path,
            @"--machServiceName",
            @"org.webkit.webpushtestdaemon.service"
        ]
    };
}

static bool shouldSetupWebPushD()
{
    static bool shouldSetup = true;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSArray<NSString *> *arguments = [[NSProcessInfo processInfo] arguments];
        if ([arguments containsObject:@"--no-webpushd"])
            shouldSetup = false;
    });

    return shouldSetup;
}

static NSURL *setUpTestWebPushD()
{
    if (!shouldSetupWebPushD())
        return nil;

    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSURL *tempDir = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"WebPushDaemonTest"] isDirectory:YES];
    NSError *error = nil;
    if ([fileManager fileExistsAtPath:tempDir.path])
        [fileManager removeItemAtURL:tempDir error:&error];
    EXPECT_NULL(error);

    killFirstInstanceOfDaemon(@"webpushd");

    registerPlistWithLaunchD(testWebPushDaemonPList(tempDir), tempDir);

    return tempDir;
}

static void cleanUpTestWebPushD(NSURL *tempDir)
{
    if (!shouldSetupWebPushD())
        return;

    killFirstInstanceOfDaemon(@"webpushd");

    if (![[NSFileManager defaultManager] fileExistsAtPath:tempDir.path])
        return;

    NSError *error = nil;
    [[NSFileManager defaultManager] removeItemAtURL:tempDir error:&error];

    if (error)
        NSLog(@"Error removing tempDir URL: %@", error);

    EXPECT_NULL(error);
}

static RetainPtr<xpc_object_t> createMessageDictionary(uint8_t messageType, const Vector<uint8_t>& message)
{
    auto dictionary = adoptNS(xpc_dictionary_create(nullptr, nullptr, 0));
    xpc_dictionary_set_uint64(dictionary.get(), "protocol version", 1);
    xpc_dictionary_set_uint64(dictionary.get(), "message type", messageType);
    xpc_dictionary_set_data(dictionary.get(), "encoded message", message.data(), message.size());
    return WTFMove(dictionary);
}

// Uses an existing connection to the daemon for a one-off message
void sendMessageToDaemon(xpc_connection_t connection, uint8_t messageType, const Vector<uint8_t>& message)
{
    auto dictionary = createMessageDictionary(messageType, message);
    xpc_connection_send_message(connection, dictionary.get());
}

// Uses an existing connection to the daemon for a one-off message, waiting for the reply
void sendMessageToDaemonWaitingForReply(xpc_connection_t connection, uint8_t messageType, const Vector<uint8_t>& message)
{
    auto dictionary = createMessageDictionary(messageType, message);

    __block bool done = false;
    xpc_connection_send_message_with_reply(connection, dictionary.get(), dispatch_get_main_queue(), ^(xpc_object_t request) {
        done = true;
    });

    TestWebKitAPI::Util::run(&done);
}

static void sendConfigurationWithAuditToken(xpc_connection_t connection)
{
    audit_token_t token = { 0, 0, 0, 0, 0, 0, 0, 0 };
    mach_msg_type_number_t auditTokenCount = TASK_AUDIT_TOKEN_COUNT;
    kern_return_t result = task_info(mach_task_self(), TASK_AUDIT_TOKEN, (task_info_t)(&token), &auditTokenCount);
    if (result != KERN_SUCCESS) {
        EXPECT_TRUE(false);
        return;
    }

    // Send configuration with audit token
    {
        Vector<uint8_t> encodedMessage(42);
        encodedMessage.fill(0);
        encodedMessage[1] = 1;
        encodedMessage[2] = 32;
        memcpy(&encodedMessage[10], &token, sizeof(token));
        sendMessageToDaemon(connection, 6, encodedMessage);
    }
}

RetainPtr<xpc_connection_t> createAndConfigureConnectionToService(const char* serviceName)
{
    auto connection = adoptNS(xpc_connection_create_mach_service(serviceName, dispatch_get_main_queue(), 0));
    xpc_connection_set_event_handler(connection.get(), ^(xpc_object_t) { });
    xpc_connection_activate(connection.get());
    sendConfigurationWithAuditToken(connection.get());

    return WTFMove(connection);
}

static Vector<uint8_t> encodeString(const String& message)
{
    ASSERT(message.is8Bit());
    auto utf8 = message.utf8();

    Vector<uint8_t> result(utf8.length() + 5);
    result[0] = static_cast<uint8_t>(utf8.length());
    result[1] = static_cast<uint8_t>(utf8.length() >> 8);
    result[2] = static_cast<uint8_t>(utf8.length() >> 16);
    result[3] = static_cast<uint8_t>(utf8.length() >> 24);
    result[4] = 0x01;

    auto data = utf8.data();
    for (size_t i = 0; i < utf8.length(); ++i)
        result[5 + i] = data[i];

    return result;
}

TEST(WebPushD, BasicCommunication)
{
    NSURL *tempDir = setUpTestWebPushD();

    auto connection = adoptNS(xpc_connection_create_mach_service("org.webkit.webpushtestdaemon.service", dispatch_get_main_queue(), 0));

    __block bool done = false;
    xpc_connection_set_event_handler(connection.get(), ^(xpc_object_t request) {
        if (xpc_get_type(request) != XPC_TYPE_DICTIONARY)
            return;
        const char* debugMessage = xpc_dictionary_get_string(request, "debug message");
        if (!debugMessage)
            return;

        NSString *nsMessage = [NSString stringWithUTF8String:debugMessage];

        // Ignore possible connections/messages from webpushtool
        if ([nsMessage hasPrefix:@"[webpushtool "])
            return;

        bool stringMatches = [nsMessage hasPrefix:@"[com.apple.WebKit.TestWebKitAPI"] || [nsMessage hasPrefix:@"[TestWebKitAPI"];
        stringMatches = stringMatches && [nsMessage hasSuffix:@" Turned Debug Mode on"];

        EXPECT_TRUE(stringMatches);
        if (!stringMatches)
            WTFLogAlways("String does not match, actual string was %@", nsMessage);

        done = true;
    });

    xpc_connection_activate(connection.get());
    sendConfigurationWithAuditToken(connection.get());

    // Enable debug messages, and wait for the resulting debug message
    {
        auto dictionary = adoptNS(xpc_dictionary_create(nullptr, nullptr, 0));
        Vector<uint8_t> encodedMessage(1);
        encodedMessage[0] = 1;
        sendMessageToDaemon(connection.get(), 5, encodedMessage);
        TestWebKitAPI::Util::run(&done);
    }

    // Echo and wait for a reply
    auto dictionary = adoptNS(xpc_dictionary_create(nullptr, nullptr, 0));
    auto encodedString = encodeString("hello");
    xpc_dictionary_set_uint64(dictionary.get(), "protocol version", 1);
    xpc_dictionary_set_uint64(dictionary.get(), "message type", 1);
    xpc_dictionary_set_data(dictionary.get(), "encoded message", encodedString.data(), encodedString.size());

    done = false;
    xpc_connection_send_message_with_reply(connection.get(), dictionary.get(), dispatch_get_main_queue(), ^(xpc_object_t reply) {
        if (xpc_get_type(reply) != XPC_TYPE_DICTIONARY) {
            NSLog(@"Unexpected non-dictionary: %@", reply);
            done = true;
            EXPECT_TRUE(FALSE);
            return;
        }

        size_t dataSize = 0;
        const void* data = xpc_dictionary_get_data(reply, "encoded message", &dataSize);
        EXPECT_EQ(dataSize, 15u);
        std::array<uint8_t, 15> expectedReply { 10, 0, 0, 0, 1, 'h', 'e', 'l', 'l', 'o' , 'h', 'e', 'l', 'l', 'o' };
        EXPECT_FALSE(memcmp(data, expectedReply.data(), expectedReply.size()));
        done = true;
    });
    TestWebKitAPI::Util::run(&done);

    cleanUpTestWebPushD(tempDir);
}

static const char* mainBytes = R"WEBPUSHRESOURCE(
<script>
    Notification.requestPermission().then(() => { alert("done") })
</script>
)WEBPUSHRESOURCE";

TEST(WebPushD, PermissionManagement)
{
    NSURL *tempDirectory = setUpTestWebPushD();

    auto dataStoreConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    dataStoreConfiguration.get().webPushMachServiceName = @"org.webkit.webpushtestdaemon.service";
    dataStoreConfiguration.get().webPushDaemonUsesMockBundlesForTesting = YES;
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().websiteDataStore = dataStore.get();
    [configuration.get().preferences _setNotificationsEnabled:YES];
    for (_WKExperimentalFeature *feature in [WKPreferences _experimentalFeatures]) {
        if ([feature.key isEqualToString:@"BuiltInNotificationsEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    auto handler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testing"];

    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[NSData dataWithBytes:mainBytes length:strlen(mainBytes)]];
        [task didFinish];
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    auto uiDelegate = adoptNS([[NotificationPermissionDelegate alloc] init]);
    [webView setUIDelegate:uiDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"testing://main/index.html"]]];
    TestWebKitAPI::Util::run(&alertReceived);

    static bool originOperationDone = false;
    static RetainPtr<WKSecurityOrigin> origin;
    [dataStore _getOriginsWithPushAndNotificationPermissions:^(NSSet<WKSecurityOrigin *> *origins) {
        EXPECT_EQ([origins count], 1u);
        origin = [origins anyObject];
        originOperationDone = true;
    }];

    TestWebKitAPI::Util::run(&originOperationDone);

    EXPECT_WK_STREQ(origin.get().protocol, "testing");
    EXPECT_WK_STREQ(origin.get().host, "main");

    // If we failed to retrieve an expected origin, we will have failed the above checks
    if (!origin) {
        cleanUpTestWebPushD(tempDirectory);
        return;
    }

    originOperationDone = false;
    [dataStore _deletePushAndNotificationRegistration:origin.get() completionHandler:^(NSError *error) {
        EXPECT_FALSE(!!error);
        originOperationDone = true;
    }];

    TestWebKitAPI::Util::run(&originOperationDone);

    originOperationDone = false;
    [dataStore _getOriginsWithPushAndNotificationPermissions:^(NSSet<WKSecurityOrigin *> *origins) {
        EXPECT_EQ([origins count], 0u);
        originOperationDone = true;
    }];
    TestWebKitAPI::Util::run(&originOperationDone);

    cleanUpTestWebPushD(tempDirectory);
}

static const char* mainSWBytes = R"SWRESOURCE(
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

TEST(WebPushD, HandleInjectedPush)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    NSURL *tempDirectory = setUpTestWebPushD();

    auto dataStoreConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    dataStoreConfiguration.get().webPushMachServiceName = @"org.webkit.webpushtestdaemon.service";
    dataStoreConfiguration.get().webPushDaemonUsesMockBundlesForTesting = YES;
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().websiteDataStore = dataStore.get();
    clearWebsiteDataStore([configuration websiteDataStore]);

    [configuration.get().preferences _setNotificationsEnabled:YES];
    for (_WKExperimentalFeature *feature in [WKPreferences _experimentalFeatures]) {
        if ([feature.key isEqualToString:@"BuiltInNotificationsEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    auto messageHandler = adoptNS([[TestMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];
    __block bool done = false;
    [messageHandler addMessage:@"Ready" withHandler:^{
        done = true;
    }];
    [messageHandler addMessage:@"Received: Hello World" withHandler:^{
        done = true;
    }];

    TestWebKitAPI::HTTPServer server({
        { "/", { mainSWBytes } },
        { "/sw.js", { { { "Content-Type", "application/javascript" } }, scriptBytes } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);
    done = false;

    // Inject push message
    auto encodedMessage = encodeString("com.apple.WebKit.TestWebKitAPI");
    encodedMessage.appendVector(encodeString(server.request().URL.absoluteString));
    encodedMessage.appendVector(encodeString("Hello World"));

    auto utilityConnection = createAndConfigureConnectionToService("org.webkit.webpushtestdaemon.service");
    sendMessageToDaemonWaitingForReply(utilityConnection.get(), 7, encodedMessage);

    // Fetch push messages
    __block RetainPtr<NSArray<NSDictionary *>> messages;
    [dataStore _getPendingPushMessages:^(NSArray<NSDictionary *> *rawMessages) {
        messages = rawMessages;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ([messages count], 1u);

    // Handle push message
    __block bool pushMessageProcessed = false;
    [dataStore _processPushMessage:[messages firstObject] completionHandler:^(bool result) {
        pushMessageProcessed = true;
    }];
    TestWebKitAPI::Util::run(&done);
    TestWebKitAPI::Util::run(&pushMessageProcessed);

    cleanUpTestWebPushD(tempDirectory);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC) || PLATFORM(IOS)
