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
#import <WebKit/WebPushDaemonConstants.h>
#import <WebKit/_WKExperimentalFeature.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <mach/mach_init.h>
#import <mach/task.h>
#import <wtf/BlockPtr.h>
#import <wtf/text/Base64.h>

#if PLATFORM(MAC) || PLATFORM(IOS)

using WebKit::WebPushD::MessageType;

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

@interface NotificationScriptMessageHandler : NSObject<WKScriptMessageHandler> {
    BlockPtr<void(id)> _messageHandler;
}
@end

@implementation NotificationScriptMessageHandler

- (void)setMessageHandler:(void (^)(id))handler
{
    _messageHandler = handler;
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    if (_messageHandler)
        _messageHandler(message.body);
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
            @"org.webkit.webpushtestdaemon.service",
            @"--useMockPushService"
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

static RetainPtr<xpc_object_t> createMessageDictionary(MessageType messageType, const Vector<uint8_t>& message)
{
    auto dictionary = adoptNS(xpc_dictionary_create(nullptr, nullptr, 0));
    xpc_dictionary_set_uint64(dictionary.get(), "protocol version", 1);
    xpc_dictionary_set_uint64(dictionary.get(), "message type", static_cast<uint64_t>(messageType));
    xpc_dictionary_set_data(dictionary.get(), "encoded message", message.data(), message.size());
    return WTFMove(dictionary);
}

// Uses an existing connection to the daemon for a one-off message
void sendMessageToDaemon(xpc_connection_t connection, MessageType messageType, const Vector<uint8_t>& message)
{
    auto dictionary = createMessageDictionary(messageType, message);
    xpc_connection_send_message(connection, dictionary.get());
}

// Uses an existing connection to the daemon for a one-off message, waiting for the reply
void sendMessageToDaemonWaitingForReply(xpc_connection_t connection, MessageType messageType, const Vector<uint8_t>& message)
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
        sendMessageToDaemon(connection, MessageType::UpdateConnectionConfiguration, encodedMessage);
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
        sendMessageToDaemon(connection.get(), MessageType::SetDebugModeIsEnabled, encodedMessage);
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

static void clearWebsiteDataStore(WKWebsiteDataStore *store)
{
    __block bool clearedStore = false;
    [store removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        clearedStore = true;
    }];
    TestWebKitAPI::Util::run(&clearedStore);
}

class WebPushDTest : public ::testing::Test {
protected:
    WebPushDTest()
    {
        [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

        m_tempDirectory = retainPtr(setUpTestWebPushD());

        auto dataStoreConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
        dataStoreConfiguration.get().webPushMachServiceName = @"org.webkit.webpushtestdaemon.service";
        dataStoreConfiguration.get().webPushDaemonUsesMockBundlesForTesting = YES;
        m_dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);

        m_configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        m_configuration.get().websiteDataStore = m_dataStore.get();
        clearWebsiteDataStore([m_configuration websiteDataStore]);

        [m_configuration.get().preferences _setNotificationsEnabled:YES];
        for (_WKExperimentalFeature *feature in [WKPreferences _experimentalFeatures]) {
            if ([feature.key isEqualToString:@"BuiltInNotificationsEnabled"])
                [[m_configuration preferences] _setEnabled:YES forFeature:feature];
            else if ([feature.key isEqualToString:@"PushAPIEnabled"])
                [[m_configuration preferences] _setEnabled:YES forFeature:feature];
        }

        m_testMessageHandler = adoptNS([[TestMessageHandler alloc] init]);
        [[m_configuration userContentController] addScriptMessageHandler:m_testMessageHandler.get() name:@"test"];

        m_notificationMessageHandler = adoptNS([[NotificationScriptMessageHandler alloc] init]);
        [[m_configuration userContentController] addScriptMessageHandler:m_notificationMessageHandler.get() name:@"note"];

        m_webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:m_configuration.get()]);
        m_uiDelegate = adoptNS([[NotificationPermissionDelegate alloc] init]);
        [m_webView setUIDelegate:m_uiDelegate.get()];
    }

    void loadRequest(const char* htmlSource, const char* serviceWorkerScriptSource)
    {
        static const char* constants = R"SRC(
            const VALID_SERVER_KEY = "BA1Hxzyi1RUM1b5wjxsn7nGxAszw2u61m164i3MrAIxHF6YK5h4SDYic-dRuU_RCPCfA5aq9ojSwk5Y2EmClBPs";
            const VALID_SERVER_KEY_THAT_CAUSES_INJECTED_FAILURE = "BEAxaUMo1s8tjORxJfnSSvWhYb4u51kg1hWT2s_9gpV7Zxar1pF_2BQ8AncuAdS2BoLhN4qaxzBy2CwHE8BBzWg";
        )SRC";
        m_server.reset(new TestWebKitAPI::HTTPServer({
            { "/", { htmlSource } },
            { "/constants.js", { { { "Content-Type", "application/javascript" } }, constants } },
            { "/sw.js", { { { "Content-Type", "application/javascript" } }, serviceWorkerScriptSource } }
        }, TestWebKitAPI::HTTPServer::Protocol::Http));

        [m_webView loadRequest:m_server->request()];
    }

    ~WebPushDTest()
    {
        cleanUpTestWebPushD(m_tempDirectory.get());
    }

    RetainPtr<NSURL> m_tempDirectory;
    RetainPtr<WKWebsiteDataStore> m_dataStore;
    RetainPtr<WKWebViewConfiguration> m_configuration;
    RetainPtr<TestMessageHandler> m_testMessageHandler;
    RetainPtr<NotificationScriptMessageHandler> m_notificationMessageHandler;
    std::unique_ptr<TestWebKitAPI::HTTPServer> m_server;
    RetainPtr<WKWebView> m_webView;
    RetainPtr<id<WKUIDelegatePrivate>> m_uiDelegate;
};

class WebPushDInjectedPushTest : public WebPushDTest {
protected:
    void runTest(NSString *expectedMessage, NSDictionary *pushUserInfo);
};

void WebPushDInjectedPushTest::runTest(NSString *expectedMessage, NSDictionary *pushUserInfo)
{
    static const char* htmlSource = R"SWRESOURCE(
    <script src="/constants.js"></script>
    <script>
    function log(msg)
    {
        window.webkit.messageHandlers.test.postMessage(msg);
    }

    const channel = new MessageChannel();
    channel.port1.onmessage = (event) => log(event.data);

    navigator.serviceWorker.register('/sw.js').then(async () => {
        const registration = await navigator.serviceWorker.ready;
        let subscription = await registration.pushManager.subscribe({
            userVisibleOnly: true,
            applicationServerKey: VALID_SERVER_KEY
        });
        registration.active.postMessage({port: channel.port2}, [channel.port2]);
    }).catch(function(error) {
        log("Registration failed with: " + error);
    });
    </script>
    )SWRESOURCE";

    static const char* serviceWorkerSource = R"SWRESOURCE(
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
        } catch (e) {
            port.postMessage("Error: " + e);
        }
    });
    )SWRESOURCE";

    __block bool ready = false;
    [m_testMessageHandler addMessage:@"Ready" withHandler:^{
        ready = true;
    }];

    __block bool gotExpectedMessage = false;
    [m_testMessageHandler addMessage:expectedMessage withHandler:^{
        gotExpectedMessage = true;
    }];

    loadRequest(htmlSource, serviceWorkerSource);

    TestWebKitAPI::Util::run(&ready);

    String bundleIdentifier = "com.apple.WebKit.TestWebKitAPI"_s;
    String scope = m_server->request().URL.absoluteString;
    String topic = bundleIdentifier + " "_s + scope;

    id obj = @{
        @"topic": (NSString *)topic,
        @"userInfo": pushUserInfo
    };
    NSData *data = [NSJSONSerialization dataWithJSONObject:obj options:0 error:nullptr];

    String message { static_cast<const char *>(data.bytes), static_cast<unsigned>(data.length) };
    auto encodedMessage = encodeString(WTFMove(message));

    auto utilityConnection = createAndConfigureConnectionToService("org.webkit.webpushtestdaemon.service");
    sendMessageToDaemonWaitingForReply(utilityConnection.get(), MessageType::InjectEncryptedPushMessageForTesting, encodedMessage);

    // Fetch push messages
    __block bool gotMessages = false;
    __block RetainPtr<NSArray<NSDictionary *>> messages;
    [m_dataStore _getPendingPushMessages:^(NSArray<NSDictionary *> *rawMessages) {
        messages = rawMessages;
        gotMessages = true;
    }];
    TestWebKitAPI::Util::run(&gotMessages);

    EXPECT_EQ([messages count], 1u);

    // Handle push message
    __block bool pushMessageProcessed = false;
    [m_dataStore _processPushMessage:[messages firstObject] completionHandler:^(bool result) {
        pushMessageProcessed = true;
    }];
    TestWebKitAPI::Util::run(&gotExpectedMessage);
    TestWebKitAPI::Util::run(&pushMessageProcessed);
}

TEST_F(WebPushDInjectedPushTest, HandleInjectedEmptyPush)
{
    runTest(@"Received: null data", @{ });
}

TEST_F(WebPushDInjectedPushTest, HandleInjectedAESGCMPush)
{
    runTest(@"Received: test aesgcm payload", @{
        @"content_encoding": @"aesgcm",
        @"as_publickey": @"BC-AgYMhqmzamH7_Aum0YvId8FV1-umgHweJNe6XQ1IMAm3E29loWXqTRndibxH27kJKWcIbyymundODMfVx_UM",
        @"as_salt": @"tkPT5xDeN0lAkSc6lZUkNg",
        @"payload": @"o/u4yvcXI1nap+zyIOBbWXdLqj1qHG2cX+KVhAdBQj1GVAt7lQ=="
    });
}

TEST_F(WebPushDInjectedPushTest, HandleInjectedAES128GCMPush)
{
    // From example in RFC8291 Section 5.
    String payloadBase64URL = "DGv6ra1nlYgDCS1FRnbzlwAAEABBBP4z9KsN6nGRTbVYI_c7VJSPQTBtkgcy27mlmlMoZIIgDll6e3vCYLocInmYWAmS6TlzAC8wEqKK6PBru3jl7A_yl95bQpu6cVPTpK4Mqgkf1CXztLVBSt2Ks3oZwbuwXPXLWyouBWLVWGNWQexSgSxsj_Qulcy4a-fN"_s;
    String payloadBase64 = base64EncodeToString(base64URLDecode(payloadBase64URL).value());

    runTest(@"Received: When I grow up, I want to be a watermelon", @{
        @"content_encoding": @"aes128gcm",
        @"payload": (NSString *)payloadBase64
    });
}

TEST_F(WebPushDTest, SubscribeTest)
{
    static const char* source = R"HTML(
    <script src="/constants.js"></script>
    <script>
    navigator.serviceWorker.register('/sw.js').then(async () => {
        const registration = await navigator.serviceWorker.ready;
        let result = null;
        try {
            let subscription = await registration.pushManager.subscribe({
                userVisibleOnly: true,
                applicationServerKey: VALID_SERVER_KEY
            });
            result = subscription.toJSON();
        } catch (e) {
            result = "Error: " + e;
        }
        window.webkit.messageHandlers.note.postMessage(result);
    });
    </script>
    )HTML";

    __block RetainPtr<id> obj = nil;
    __block bool done = false;
    [m_notificationMessageHandler setMessageHandler:^(id message) {
        obj = message;
        done = true;
    }];

    loadRequest(source, "");
    TestWebKitAPI::Util::run(&done);

    ASSERT_TRUE([obj isKindOfClass:[NSDictionary class]]);

    NSDictionary *subscription = obj.get();
    ASSERT_TRUE([subscription[@"endpoint"] hasPrefix:@"https://"]);
    ASSERT_TRUE([subscription[@"keys"] isKindOfClass:[NSDictionary class]]);

    // Shared auth secret should be 16 bytes (22 bytes in unpadded base64url).
    ASSERT_EQ([subscription[@"keys"][@"auth"] length], 22u);

    // Client public key should be 65 bytes (87 bytes in unpadded base64url).
    ASSERT_EQ([subscription[@"keys"][@"p256dh"] length], 87u);
}

TEST_F(WebPushDTest, SubscribeFailureTest)
{
    static const char* source = R"HTML(
    <script src="/constants.js"></script>
    <script>
    navigator.serviceWorker.register('/sw.js').then(async () => {
        const registration = await navigator.serviceWorker.ready;
        let result = null;
        try {
            let subscription = await registration.pushManager.subscribe({
                userVisibleOnly: true,
                applicationServerKey: VALID_SERVER_KEY_THAT_CAUSES_INJECTED_FAILURE
            });
            result = subscription.toJSON();
        } catch (e) {
            result = "Error: " + e;
        }
        window.webkit.messageHandlers.note.postMessage(result);
    });
    </script>
    )HTML";

    __block RetainPtr<id> obj = nil;
    __block bool done = false;
    [m_notificationMessageHandler setMessageHandler:^(id message) {
        obj = message;
        done = true;
    }];

    loadRequest(source, "");
    TestWebKitAPI::Util::run(&done);

    // Spec says that an error in the push service should be an AbortError.
    ASSERT_TRUE([obj isKindOfClass:[NSString class]]);
    ASSERT_TRUE([obj hasPrefix:@"Error: AbortError"]);
}

TEST_F(WebPushDTest, UnsubscribeTest)
{
    static const char* source = R"HTML(
    <script src="/constants.js"></script>
    <script>
    navigator.serviceWorker.register('/sw.js').then(async () => {
        const registration = await navigator.serviceWorker.ready;
        let result = null;
        try {
            let subscription = await registration.pushManager.subscribe({
                userVisibleOnly: true,
                applicationServerKey: VALID_SERVER_KEY
            });
            let result1 = await subscription.unsubscribe();
            let result2 = await subscription.unsubscribe();
            result = [result1, result2];
        } catch (e) {
            result = "Error: " + e;
        }
        window.webkit.messageHandlers.note.postMessage(result);
    });
    </script>
    )HTML";

    __block RetainPtr<id> obj = nil;
    __block bool done = false;
    [m_notificationMessageHandler setMessageHandler:^(id message) {
        obj = message;
        done = true;
    }];

    loadRequest(source, "");
    TestWebKitAPI::Util::run(&done);

    // First unsubscribe should succeed. Second one should fail since the first one removed the record from the database.
    id expected = @[@(1), @(0)];
    ASSERT_TRUE([obj isEqual:expected]);
}

#if ENABLE(INSTALL_COORDINATION_BUNDLES)
#if USE(APPLE_INTERNAL_SDK)
static void deleteAllRegistrationsForDataStore(WKWebsiteDataStore *dataStore)
{
    __block bool originOperationDone = false;
    __block RetainPtr<NSSet<WKSecurityOrigin *>> originSet;
    [dataStore _getOriginsWithPushAndNotificationPermissions:^(NSSet<WKSecurityOrigin *> *origins) {
        originSet = origins;
        originOperationDone = true;
    }];
    TestWebKitAPI::Util::run(&originOperationDone);

    if (![originSet count])
        return;

    __block size_t deletedOrigins = 0;
    originOperationDone = false;
    for (WKSecurityOrigin *origin in originSet.get()) {
        [dataStore _deletePushAndNotificationRegistration:origin completionHandler:^(NSError *error) {
            EXPECT_FALSE(!!error);
            if (++deletedOrigins == [originSet count])
                originOperationDone = true;
        }];
    }
    TestWebKitAPI::Util::run(&originOperationDone);

    originOperationDone = false;
    [dataStore _getOriginsWithPushAndNotificationPermissions:^(NSSet<WKSecurityOrigin *> *origins) {
        EXPECT_EQ([origins count], 0u);
        originOperationDone = true;
    }];
    TestWebKitAPI::Util::run(&originOperationDone);

}

TEST(WebPushD, InstallCoordinationBundles)
{
    NSURL *tempDirectory = setUpTestWebPushD();

    auto dataStoreConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    dataStoreConfiguration.get().webPushMachServiceName = @"org.webkit.webpushtestdaemon.service";
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);

    deleteAllRegistrationsForDataStore(dataStore.get());

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

    alertReceived = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"testing://secondary/index.html"]]];
    TestWebKitAPI::Util::run(&alertReceived);

    static bool originOperationDone = false;
    static RetainPtr<NSSet<WKSecurityOrigin *>> origins;
    [dataStore _getOriginsWithPushAndNotificationPermissions:^(NSSet<WKSecurityOrigin *> *rawOrigins) {
        EXPECT_EQ([rawOrigins count], 2u);
        origins = rawOrigins;
        originOperationDone = true;
    }];
    TestWebKitAPI::Util::run(&originOperationDone);

    for (WKSecurityOrigin *origin in origins.get()) {
        EXPECT_TRUE([origin.protocol isEqualToString:@"testing"]);
        EXPECT_TRUE([origin.host isEqualToString:@"main"] || [origin.host isEqualToString:@"secondary"]);
    }

    deleteAllRegistrationsForDataStore(dataStore.get());
    cleanUpTestWebPushD(tempDirectory);
}
#endif // #if USE(APPLE_INTERNAL_SDK)
#endif // ENABLE(INSTALL_COORDINATION_BUNDLES)

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC) || PLATFORM(IOS)
