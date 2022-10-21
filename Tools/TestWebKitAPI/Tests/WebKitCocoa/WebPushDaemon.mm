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
#import "TestNavigationDelegate.h"
#import "TestNotificationProvider.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebsiteDataRecordPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebPushDaemonConstants.h>
#import <WebKit/_WKExperimentalFeature.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <mach/mach_init.h>
#import <mach/task.h>
#import <wtf/BlockPtr.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/spi/darwin/XPCSPI.h>
#import <wtf/text/Base64.h>

#if ENABLE(NOTIFICATIONS) && ENABLE(NOTIFICATION_EVENT) && (PLATFORM(MAC) || PLATFORM(IOS))

using WebKit::WebPushD::MessageType;
using WebKit::WebPushD::RawXPCMessageType;

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

enum LaunchOnlyOnce : BOOL { No, Yes };

static NSDictionary<NSString *, id> *testWebPushDaemonPList(NSURL *storageLocation, LaunchOnlyOnce launchOnlyOnce)
{
    return @{
        @"Label" : @"org.webkit.webpushtestdaemon",
        @"LaunchOnlyOnce" : @(static_cast<BOOL>(launchOnlyOnce)),
        @"ThrottleInterval" : @(1),
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

static NSURL *setUpTestWebPushD(LaunchOnlyOnce launchOnlyOnce = LaunchOnlyOnce::Yes)
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

    registerPlistWithLaunchD(testWebPushDaemonPList(tempDir, launchOnlyOnce), tempDir);

    return tempDir;
}

// Only works if the test daemon was registered with LaunchOnlyOnce::No.
static BOOL restartTestWebPushD()
{
    return restartService(@"org.webkit.webpushtestdaemon", @"webpushd");
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

template <typename T>
static void addMessageHeaders(xpc_object_t request, T messageType, uint64_t version)
{
    xpc_dictionary_set_uint64(request, WebKit::WebPushD::protocolMessageTypeKey, static_cast<uint64_t>(messageType));
    xpc_dictionary_set_uint64(request, WebKit::WebPushD::protocolVersionKey, version);
}

void sendMessageToDaemon(xpc_connection_t connection, MessageType messageType, const Vector<uint8_t>& message, uint64_t version = WebKit::WebPushD::protocolVersionValue)
{
    auto request = adoptNS(xpc_dictionary_create(nullptr, nullptr, 0));
    addMessageHeaders(request.get(), messageType, version);
    xpc_dictionary_set_data(request.get(), WebKit::WebPushD::protocolEncodedMessageKey, message.data(), message.size());
    xpc_connection_send_message(connection, request.get());
}

xpc_object_t sendMessageToDaemonWithReplySync(xpc_connection_t connection, MessageType messageType, const Vector<uint8_t>& message, uint64_t version = WebKit::WebPushD::protocolVersionValue)
{
    auto request = adoptNS(xpc_dictionary_create(nullptr, nullptr, 0));
    addMessageHeaders(request.get(), messageType, version);
    xpc_dictionary_set_data(request.get(), WebKit::WebPushD::protocolEncodedMessageKey, message.data(), message.size());
    return xpc_connection_send_message_with_reply_sync(connection, request.get());
}

xpc_object_t sendRawMessageToDaemonWithReplySync(xpc_connection_t connection, RawXPCMessageType messageType, xpc_object_t request, uint64_t version = WebKit::WebPushD::protocolVersionValue)
{
    addMessageHeaders(request, messageType, version);
    return xpc_connection_send_message_with_reply_sync(connection, request);
}

static Vector<String> toStringVector(xpc_object_t object)
{
    if (xpc_get_type(object) != XPC_TYPE_ARRAY)
        return { };

    Vector<String> result;
    for (size_t i = 0; i < xpc_array_get_count(object); i++) {
        auto element = xpc_array_get_string(object, i);
        if (!element)
            return { };
        result.append(String::fromUTF8(element));
    }
    return result;
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
    auto encodedString = encodeString("hello"_s);
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

    // Sending a message with a higher protocol version should cause the connection to be terminated
    auto reply = sendMessageToDaemonWithReplySync(connection.get(), MessageType::EchoTwice, { }, WebKit::WebPushD::protocolVersionValue + 1);
    EXPECT_EQ(reply, XPC_ERROR_CONNECTION_INTERRUPTED);

    cleanUpTestWebPushD(tempDir);
}

static void clearWebsiteDataStore(WKWebsiteDataStore *store)
{
    __block bool clearedStore = false;
    [store removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        [store _clearResourceLoadStatistics:^{
            clearedStore = true;
        }];
    }];
    TestWebKitAPI::Util::run(&clearedStore);
}

static constexpr auto constants = R"SRC(
    const VALID_SERVER_KEY = "BA1Hxzyi1RUM1b5wjxsn7nGxAszw2u61m164i3MrAIxHF6YK5h4SDYic-dRuU_RCPCfA5aq9ojSwk5Y2EmClBPs";
    const VALID_SERVER_KEY_THAT_CAUSES_INJECTED_FAILURE = "BEAxaUMo1s8tjORxJfnSSvWhYb4u51kg1hWT2s_9gpV7Zxar1pF_2BQ8AncuAdS2BoLhN4qaxzBy2CwHE8BBzWg";
)SRC"_s;

class WebPushDTest : public ::testing::Test {
protected:
    WebPushDTest(LaunchOnlyOnce launchOnlyOnce = LaunchOnlyOnce::Yes)
    {
        m_origin = "https://example.com"_s;
        m_url = adoptNS([[NSURL alloc] initWithString:@"https://example.com/"]);
        m_tempDirectory = retainPtr(setUpTestWebPushD(launchOnlyOnce));
        m_testMessageHandler = adoptNS([[TestMessageHandler alloc] init]);
        m_notificationMessageHandler = adoptNS([[NotificationScriptMessageHandler alloc] init]);
    }

    void loadRequest(ASCIILiteral htmlSource, ASCIILiteral serviceWorkerScriptSource)
    {
        ASSERT_FALSE(m_server);
        m_server.reset(new TestWebKitAPI::HTTPServer({
            { "/"_s, { htmlSource } },
            { "/constants.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, constants } },
            { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, serviceWorkerScriptSource } }
        }, TestWebKitAPI::HTTPServer::Protocol::HttpsProxy));

        auto dataStoreConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
        [dataStoreConfiguration setProxyConfiguration:@{
            (NSString *)kCFStreamPropertyHTTPSProxyHost: @"127.0.0.1",
            (NSString *)kCFStreamPropertyHTTPSProxyPort: @(m_server->port())
        }];
        dataStoreConfiguration.get().webPushMachServiceName = @"org.webkit.webpushtestdaemon.service";
        dataStoreConfiguration.get().webPushDaemonUsesMockBundlesForTesting = YES;

        // FIXME: This seems like it shouldn't be necessary, but _clearResourceLoadStatistics (called by clearWebsiteDataStore) doesn't seem to work.
        NSError *error = nil;
        [[NSFileManager defaultManager] removeItemAtURL:[dataStoreConfiguration _resourceLoadStatisticsDirectory] error:&error];
        ASSERT_NULL(error);

        m_dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);
        [m_dataStore _setResourceLoadStatisticsEnabled:YES];
        clearWebsiteDataStore(m_dataStore.get());

        __block bool done = false;
        [m_dataStore _setPrevalentDomain:m_url.get() completionHandler:^{
            done = true;
        }];
        Util::run(&done);
        done = false;
        [m_dataStore _logUserInteraction:m_url.get() completionHandler:^{
            done = true;
        }];
        Util::run(&done);

        m_configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        m_configuration.get().websiteDataStore = m_dataStore.get();

        auto userContentController = m_configuration.get().userContentController;
        [userContentController addScriptMessageHandler:m_testMessageHandler.get() name:@"test"];
        [userContentController addScriptMessageHandler:m_notificationMessageHandler.get() name:@"note"];

        [m_configuration.get().preferences _setPushAPIEnabled:YES];

        m_notificationProvider = makeUnique<TestWebKitAPI::TestNotificationProvider>(Vector<WKNotificationManagerRef> { [[m_configuration processPool] _notificationManagerForTesting], WKNotificationManagerGetSharedServiceWorkerNotificationManager() });
        m_notificationProvider->setPermission(m_origin, true);

        m_webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:m_configuration.get()]);

        m_uiDelegate = adoptNS([[NotificationPermissionDelegate alloc] init]);
        [m_webView setUIDelegate:m_uiDelegate.get()];

        m_navigationDelegate = adoptNS([TestNavigationDelegate new]);
        m_navigationDelegate.get().didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
            completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
        };
        [m_webView setNavigationDelegate:m_navigationDelegate.get()];

        [m_webView loadRequest:[NSURLRequest requestWithURL:m_url.get()]];
    }

    bool hasPushSubscription()
    {
        __block bool done = false;
        __block bool result = false;

        [m_dataStore _scopeURL:m_url.get() hasPushSubscriptionForTesting:^(BOOL fetchedResult) {
            result = fetchedResult;
            done = true;
        }];

        TestWebKitAPI::Util::run(&done);
        return result;
    }

    Vector<String> getOriginsWithPushSubscriptions()
    {
        __block bool done = false;
        __block Vector<String> result;

        [m_dataStore _getOriginsWithPushSubscriptions:^(NSSet<WKSecurityOrigin *> *origins) {
            for (WKSecurityOrigin *origin in origins) {
                if (origin.port)
                    result.append(makeString(String(origin.protocol), "://"_s, String(origin.host), ":"_s, String::number(origin.port)));
                else
                    result.append(makeString(String(origin.protocol), "://"_s, String(origin.host)));
            }
            done = true;
        }];

        TestWebKitAPI::Util::run(&done);
        return result;
    }

    ~WebPushDTest()
    {
        cleanUpTestWebPushD(m_tempDirectory.get());
    }
public:
    RetainPtr<NSDictionary> injectPushMessage(NSDictionary *pushUserInfo)
    {
        String scope = [m_url absoluteString];
        String topic = makeString("com.apple.WebKit.TestWebKitAPI "_s, scope);
        id obj = @{
            @"topic": (NSString *)topic,
            @"userInfo": pushUserInfo
        };
        NSData *data = [NSJSONSerialization dataWithJSONObject:obj options:0 error:nullptr];

        String message { static_cast<const char *>(data.bytes), static_cast<unsigned>(data.length) };
        auto encodedMessage = encodeString(WTFMove(message));

        auto utilityConnection = createAndConfigureConnectionToService("org.webkit.webpushtestdaemon.service");
        sendMessageToDaemonWithReplySync(utilityConnection.get(), MessageType::InjectEncryptedPushMessageForTesting, encodedMessage);

        // Fetch push messages
        __block bool gotMessages = false;
        __block RetainPtr<NSArray<NSDictionary *>> messages;
        [m_dataStore _getPendingPushMessages:^(NSArray<NSDictionary *> *rawMessages) {
            messages = rawMessages;
            gotMessages = true;
        }];
        TestWebKitAPI::Util::run(&gotMessages);

        return [messages count] ? [messages objectAtIndex:0] : nullptr;
    }
protected:
    std::pair<Vector<String>, Vector<String>> getPushTopics()
    {
        auto connection = createAndConfigureConnectionToService("org.webkit.webpushtestdaemon.service");
        auto request = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
        auto reply = sendRawMessageToDaemonWithReplySync(connection.get(), RawXPCMessageType::GetPushTopicsForTesting, request.get());

        auto enabledTopics = toStringVector(xpc_dictionary_get_value(reply, "enabled"));
        auto ignoredTopics = toStringVector(xpc_dictionary_get_value(reply, "ignored"));

        return std::make_pair(WTFMove(enabledTopics), WTFMove(ignoredTopics));
    }

    String m_origin;
    RetainPtr<NSURL> m_url;
    RetainPtr<NSURL> m_tempDirectory;
    RetainPtr<TestMessageHandler> m_testMessageHandler;
    RetainPtr<NotificationScriptMessageHandler> m_notificationMessageHandler;
    bool m_loadedRequest { false };
    RetainPtr<WKWebsiteDataStore> m_dataStore;
    RetainPtr<WKWebViewConfiguration> m_configuration;
    std::unique_ptr<TestWebKitAPI::HTTPServer> m_server;
    std::unique_ptr<TestWebKitAPI::TestNotificationProvider> m_notificationProvider;
    RetainPtr<WKWebView> m_webView;
    RetainPtr<id<WKUIDelegatePrivate>> m_uiDelegate;
    RetainPtr<TestNavigationDelegate> m_navigationDelegate;
};

class WebPushDInjectedPushTest : public WebPushDTest {
protected:
    void runTest(NSString *expectedMessage, NSDictionary *pushUserInfo);
};

class WebPushDMultipleLaunchTest : public WebPushDTest {
public:
    WebPushDMultipleLaunchTest()
        : WebPushDTest(LaunchOnlyOnce::No)
    {
    }
};

static constexpr auto injectedPushHTMLSource = R"SWRESOURCE(
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
)SWRESOURCE"_s;

static constexpr auto injectedPushServiceWorkerSource = R"SWRESOURCE(
let port;
self.addEventListener("message", (event) => {
    port = event.data.port;
    port.postMessage("Ready");
});
self.addEventListener("push", async (event) => {
    try {
        await self.registration.showNotification("notification");
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
self.addEventListener("notificationclick", () => {
    port.postMessage("Received: notificationclick");
});
)SWRESOURCE"_s;

void WebPushDInjectedPushTest::runTest(NSString *expectedMessage, NSDictionary *pushUserInfo)
{
    __block bool ready = false;
    [m_testMessageHandler addMessage:@"Ready" withHandler:^{
        ready = true;
    }];

    __block bool gotExpectedMessage = false;
    [m_testMessageHandler addMessage:expectedMessage withHandler:^{
        gotExpectedMessage = true;
    }];

    loadRequest(injectedPushHTMLSource, injectedPushServiceWorkerSource);
    TestWebKitAPI::Util::run(&ready);

    auto message = injectPushMessage(pushUserInfo);

    __block bool pushMessageProcessed = false;
    __block bool pushMessageProcessedResult = false;
    [m_dataStore _processPushMessage:message.get() completionHandler:^(bool result) {
        pushMessageProcessedResult = result;
        pushMessageProcessed = true;
    }];
    TestWebKitAPI::Util::run(&gotExpectedMessage);
    TestWebKitAPI::Util::run(&pushMessageProcessed);

    EXPECT_TRUE(pushMessageProcessedResult);
}

TEST_F(WebPushDInjectedPushTest, HandleInjectedEmptyPush)
{
    runTest(@"Received: null data", @{ });
}

static NSDictionary *aesGCMPushData()
{
    return @{
        @"content_encoding": @"aesgcm",
        @"as_publickey": @"BC-AgYMhqmzamH7_Aum0YvId8FV1-umgHweJNe6XQ1IMAm3E29loWXqTRndibxH27kJKWcIbyymundODMfVx_UM",
        @"as_salt": @"tkPT5xDeN0lAkSc6lZUkNg",
        @"payload": @"o/u4yvcXI1nap+zyIOBbWXdLqj1qHG2cX+KVhAdBQj1GVAt7lQ=="
    };
}

TEST_F(WebPushDInjectedPushTest, HandleInjectedAESGCMPush)
{
    runTest(@"Received: test aesgcm payload", aesGCMPushData());
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
    static constexpr auto source = R"HTML(
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
    )HTML"_s;

    __block RetainPtr<id> obj = nil;
    __block bool done = false;
    [m_notificationMessageHandler setMessageHandler:^(id message) {
        obj = message;
        done = true;
    }];

    loadRequest(source, ""_s);
    TestWebKitAPI::Util::run(&done);

    ASSERT_TRUE([obj isKindOfClass:[NSDictionary class]]);

    NSDictionary *subscription = obj.get();
    ASSERT_TRUE([subscription[@"endpoint"] hasPrefix:@"https://"]);
    ASSERT_TRUE([subscription[@"keys"] isKindOfClass:[NSDictionary class]]);

    // Shared auth secret should be 16 bytes (22 bytes in unpadded base64url).
    ASSERT_EQ([subscription[@"keys"][@"auth"] length], 22u);

    // Client public key should be 65 bytes (87 bytes in unpadded base64url).
    ASSERT_EQ([subscription[@"keys"][@"p256dh"] length], 87u);

    ASSERT_TRUE(hasPushSubscription());

    auto result = getOriginsWithPushSubscriptions();
    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(result.first(), m_origin);
}

TEST_F(WebPushDTest, SubscribeFailureTest)
{
    static constexpr auto source = R"HTML(
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
    )HTML"_s;

    __block RetainPtr<id> obj = nil;
    __block bool done = false;
    [m_notificationMessageHandler setMessageHandler:^(id message) {
        obj = message;
        done = true;
    }];

    loadRequest(source, ""_s);
    TestWebKitAPI::Util::run(&done);

    // Spec says that an error in the push service should be an AbortError.
    ASSERT_TRUE([obj isKindOfClass:[NSString class]]);
    ASSERT_TRUE([obj hasPrefix:@"Error: AbortError"]);

    ASSERT_FALSE(hasPushSubscription());
}

TEST_F(WebPushDTest, UnsubscribeTest)
{
    static constexpr auto source = R"HTML(
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
    )HTML"_s;

    __block RetainPtr<id> obj = nil;
    __block bool done = false;
    [m_notificationMessageHandler setMessageHandler:^(id message) {
        obj = message;
        done = true;
    }];

    loadRequest(source, ""_s);
    TestWebKitAPI::Util::run(&done);

    // First unsubscribe should succeed. Second one should fail since the first one removed the record from the database.
    id expected = @[@(1), @(0)];
    ASSERT_TRUE([obj isEqual:expected]);

    ASSERT_FALSE(hasPushSubscription());
}

TEST_F(WebPushDTest, UnsubscribesOnServiceWorkerUnregisterTest)
{
    static constexpr auto source = R"HTML(
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
            result = await registration.unregister();
        } catch (e) {
            result = "Error: " + e;
        }
        window.webkit.messageHandlers.note.postMessage(result);
    });
    </script>
    )HTML"_s;

    __block RetainPtr<id> unregisterSucceeded = nil;
    __block bool done = false;
    [m_notificationMessageHandler setMessageHandler:^(id message) {
        unregisterSucceeded = message;
        done = true;
    }];

    loadRequest(source, ""_s);
    TestWebKitAPI::Util::run(&done);

    ASSERT_TRUE([unregisterSucceeded isEqual:@YES]);
    ASSERT_FALSE(hasPushSubscription());
}

TEST_F(WebPushDTest, UnsubscribesOnClearingAllWebsiteData)
{
    static constexpr auto source = R"HTML(
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
            result = "Subscribed";
        } catch (e) {
            result = "Error: " + e;
        }
        window.webkit.messageHandlers.note.postMessage(result);
    });
    </script>
    )HTML"_s;

    __block RetainPtr<id> result = nil;
    __block bool done = false;
    [m_notificationMessageHandler setMessageHandler:^(id message) {
        result = message;
        done = true;
    }];

    loadRequest(source, ""_s);
    TestWebKitAPI::Util::run(&done);

    ASSERT_TRUE([result isEqualToString:@"Subscribed"]);

    __block bool removedData = false;
    [m_dataStore removeDataOfTypes:[NSSet setWithObject:WKWebsiteDataTypeServiceWorkerRegistrations] modifiedSince:[NSDate distantPast] completionHandler:^(void) {
        removedData = true;
    }];
    TestWebKitAPI::Util::run(&removedData);

    ASSERT_FALSE(hasPushSubscription());
}

TEST_F(WebPushDTest, UnsubscribesOnClearingWebsiteDataForOrigin)
{
    static constexpr auto source = R"HTML(
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
            result = "Subscribed";
        } catch (e) {
            result = "Error: " + e;
        }
        window.webkit.messageHandlers.note.postMessage(result);
    });
    </script>
    )HTML"_s;

    __block RetainPtr<id> result = nil;
    __block bool done = false;
    [m_notificationMessageHandler setMessageHandler:^(id message) {
        result = message;
        done = true;
    }];

    loadRequest(source, ""_s);
    TestWebKitAPI::Util::run(&done);

    ASSERT_TRUE([result isEqualToString:@"Subscribed"]);

    __block bool fetchedRecords = false;
    __block RetainPtr<NSArray<WKWebsiteDataRecord *>> records;
    [m_dataStore fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeServiceWorkerRegistrations] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        records = dataRecords;
        fetchedRecords = true;
    }];
    TestWebKitAPI::Util::run(&fetchedRecords);

    WKWebsiteDataRecord *filteredRecord = nil;
    for (WKWebsiteDataRecord *record in records.get()) {
        for (NSString *originString in record._originsStrings) {
            if ([originString isEqualToString:m_origin]) {
                filteredRecord = record;
                break;
            }
        }
    }
    ASSERT_TRUE(filteredRecord);

    __block bool removedData = false;
    [m_dataStore removeDataOfTypes:[NSSet setWithObject:WKWebsiteDataTypeServiceWorkerRegistrations] forDataRecords:[NSArray arrayWithObject:filteredRecord] completionHandler:^(void) {
        removedData = true;
    }];
    TestWebKitAPI::Util::run(&removedData);

    ASSERT_FALSE(hasPushSubscription());
}

TEST_F(WebPushDTest, UnsubscribesOnPermissionReset)
{
    static constexpr auto source = R"HTML(
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
            result = "Subscribed";
        } catch (e) {
            result = "Error: " + e;
        }
        window.webkit.messageHandlers.note.postMessage(result);
    });
    </script>
    )HTML"_s;

    __block RetainPtr<id> result = nil;
    __block bool done = false;
    [m_notificationMessageHandler setMessageHandler:^(id message) {
        result = message;
        done = true;
    }];

    loadRequest(source, ""_s);
    TestWebKitAPI::Util::run(&done);

    ASSERT_TRUE([result isEqualToString:@"Subscribed"]);
    ASSERT_TRUE(hasPushSubscription());

    m_notificationProvider->resetPermission(m_origin);

    bool isSubscribed = true;
    TestWebKitAPI::Util::waitForConditionWithLogging([this, &isSubscribed] {
        isSubscribed = hasPushSubscription();
        if (!isSubscribed)
            return true;

        sleep(1);
        return false;
    }, 5, @"Timed out waiting for push subscription to be removed.");

    ASSERT_FALSE(isSubscribed);
}

TEST_F(WebPushDTest, IgnoresSubscriptionOnPermissionDenied)
{
    static constexpr auto source = R"HTML(
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
            result = "Subscribed";
        } catch (e) {
            result = "Error: " + e;
        }
        window.webkit.messageHandlers.note.postMessage(result);
    });
    </script>
    )HTML"_s;

    __block RetainPtr<id> result = nil;
    __block bool done = false;
    [m_notificationMessageHandler setMessageHandler:^(id message) {
        result = message;
        done = true;
    }];

    loadRequest(source, ""_s);
    TestWebKitAPI::Util::run(&done);

    ASSERT_TRUE([result isEqualToString:@"Subscribed"]);
    ASSERT_TRUE(hasPushSubscription());

    // Topic should be moved to ignored list after denying permission, but the subscription should still exist.
    m_notificationProvider->setPermission(m_origin, false);

    bool isIgnored = false;
    TestWebKitAPI::Util::waitForConditionWithLogging([this, &isIgnored] {
        auto [enabledTopics, ignoredTopics] = getPushTopics();
        if (!enabledTopics.size() && ignoredTopics.size()) {
            isIgnored = true;
            return true;
        }

        sleep(1);
        return false;
    }, 5, @"Timed out waiting for push subscription to be ignored.");

    ASSERT_TRUE(isIgnored);
    ASSERT_TRUE(hasPushSubscription());

    // Topic should be moved back to enabled list after allowing permission, and the subscription should still exist.
    m_notificationProvider->setPermission(m_origin, true);

    bool isEnabled = false;
    TestWebKitAPI::Util::waitForConditionWithLogging([this, &isEnabled] {
        auto [enabledTopics, ignoredTopics] = getPushTopics();
        if (enabledTopics.size() && !ignoredTopics.size()) {
            isEnabled = true;
            return true;
        }

        sleep(1);
        return false;
    }, 5, @"Timed out waiting for push subscription to be enabled.");

    ASSERT_TRUE(isEnabled);
    ASSERT_TRUE(hasPushSubscription());
}

#if ENABLE(INSTALL_COORDINATION_BUNDLES)
#if USE(APPLE_INTERNAL_SDK)
static void showNotificationsViaWebPushDInsteadOfUIProcess(WKWebViewConfiguration *configuration)
{
    [configuration.preferences _setNotificationsEnabled:YES];
    for (_WKExperimentalFeature *feature in [WKPreferences _experimentalFeatures]) {
        if ([feature.key isEqualToString:@"BuiltInNotificationsEnabled"]) {
            [configuration.preferences _setEnabled:YES forFeature:feature];
            break;
        }
    }
}

TEST(WebPushD, PermissionManagement)
{
    NSURL *tempDirectory = setUpTestWebPushD();

    auto dataStoreConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    dataStoreConfiguration.get().webPushMachServiceName = @"org.webkit.webpushtestdaemon.service";
    dataStoreConfiguration.get().webPushDaemonUsesMockBundlesForTesting = YES;
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().websiteDataStore = dataStore.get();
    showNotificationsViaWebPushDInsteadOfUIProcess(configuration.get());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    auto uiDelegate = adoptNS([[NotificationPermissionDelegate alloc] init]);
    [webView setUIDelegate:uiDelegate.get()];
    [webView synchronouslyLoadHTMLString:@"" baseURL:[NSURL URLWithString:@"https://example.org"]];
    [webView evaluateJavaScript:@"Notification.requestPermission().then(() => { alert('done') })" completionHandler:nil];
    TestWebKitAPI::Util::run(&alertReceived);

    static bool originOperationDone = false;
    static RetainPtr<WKSecurityOrigin> origin;
    [dataStore _getOriginsWithPushAndNotificationPermissions:^(NSSet<WKSecurityOrigin *> *origins) {
        EXPECT_EQ([origins count], 1u);
        origin = [origins anyObject];
        originOperationDone = true;
    }];

    TestWebKitAPI::Util::run(&originOperationDone);

    EXPECT_WK_STREQ(origin.get().protocol, "https");
    EXPECT_WK_STREQ(origin.get().host, "example.org");

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

static const char* mainBytes = R"WEBPUSHRESOURCE(
<script>
    Notification.requestPermission().then(() => { alert("done") })
</script>
)WEBPUSHRESOURCE";

TEST(WebPushD, InstallCoordinationBundles)
{
    NSURL *tempDirectory = setUpTestWebPushD();

    auto dataStoreConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    dataStoreConfiguration.get().webPushMachServiceName = @"org.webkit.webpushtestdaemon.service";
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);

    deleteAllRegistrationsForDataStore(dataStore.get());

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().websiteDataStore = dataStore.get();
    enableNotifications(configuration.get());

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

TEST_F(WebPushDTest, TooManySilentPushesCausesUnsubscribe)
{
    static constexpr auto htmlSource = R"HTML(
    <script src="/constants.js"></script>
    <script>
    let pushManager = null;

    navigator.serviceWorker.register('/sw.js').then(async () => {
        const registration = await navigator.serviceWorker.ready;
        let result = null;
        try {
            pushManager = registration.pushManager;
            let subscription = await pushManager.subscribe({
                userVisibleOnly: true,
                applicationServerKey: VALID_SERVER_KEY
            });
            result = "Subscribed";
        } catch (e) {
            result = "Error: " + e;
        }
        window.webkit.messageHandlers.note.postMessage(result);
    });

    function getPushSubscription()
    {
        pushManager.getSubscription().then((subscription) => {
            window.webkit.messageHandlers.note.postMessage(subscription ? "Subscribed" : "Unsubscribed");
        });
    }
    </script>
    )HTML"_s;
    static constexpr auto serviceWorkerScriptSource = "self.addEventListener('push', (event) => { });"_s;

    __block RetainPtr<id> message = nil;
    __block bool gotMessage = false;
    [m_notificationMessageHandler setMessageHandler:^(id receivedMessage) {
        message = receivedMessage;
        gotMessage = true;
    }];

    loadRequest(htmlSource, serviceWorkerScriptSource);

    TestWebKitAPI::Util::run(&gotMessage);
    ASSERT_TRUE([message isEqualToString:@"Subscribed"]);

    for (unsigned i = 0; i < WebKit::WebPushD::maxSilentPushCount; i++) {
        gotMessage = false;
        [m_webView evaluateJavaScript:@"getPushSubscription()" completionHandler:^(id, NSError*) { }];
        TestWebKitAPI::Util::run(&gotMessage);
        ASSERT_TRUE([message isEqualToString:@"Subscribed"]);

        __block bool processedPush = false;
        __block bool pushResult = false;
        auto message = injectPushMessage(@{ });

        [m_dataStore _processPushMessage:message.get() completionHandler:^(bool result) {
            pushResult = result;
            processedPush = true;
        }];
        TestWebKitAPI::Util::run(&processedPush);

        // WebContent should fail processing the push since no notification was shown.
        EXPECT_FALSE(pushResult);
    }

    gotMessage = false;
    [m_webView evaluateJavaScript:@"getPushSubscription()" completionHandler:^(id, NSError*) { }];
    TestWebKitAPI::Util::run(&gotMessage);
    ASSERT_TRUE([message isEqualToString:@"Unsubscribed"]);
}

TEST_F(WebPushDTest, GetPushSubscriptionWithMismatchedPublicToken)
{
    static constexpr auto htmlSource = R"HTML(
    <script src="/constants.js"></script>
    <script>
    let postNoteMessage = window.webkit.messageHandlers.note.postMessage.bind(window.webkit.messageHandlers.note);
    let getPushManager =
        navigator.serviceWorker.register('/sw.js')
            .then(() => navigator.serviceWorker.ready)
            .then(registration => registration.pushManager);

    function subscribe()
    {
        getPushManager
            .then(pushManager => pushManager.subscribe({ userVisibleOnly: true, applicationServerKey: VALID_SERVER_KEY }))
            .then(subscription => subscription.toJSON())
            .then(postNoteMessage)
            .catch(e => postNoteMessage(e.toString()));
    }

    function getSubscription()
    {
        getPushManager
            .then(pushManager => pushManager.getSubscription())
            .then(subscription => subscription ? subscription.toJSON() : null)
            .then(postNoteMessage)
            .catch(e => postNoteMessage(e.toString()));
    }

    postNoteMessage('Ready');
    </script>
    )HTML"_s;

    __block RetainPtr<id> message = nil;
    __block bool gotMessage = false;
    [m_notificationMessageHandler setMessageHandler:^(id receivedMessage) {
        message = receivedMessage;
        gotMessage = true;
    }];

    loadRequest(htmlSource, ""_s);
    TestWebKitAPI::Util::run(&gotMessage);
    ASSERT_TRUE([message isEqualToString:@"Ready"]);

    message = nil;
    gotMessage = false;
    [m_webView evaluateJavaScript:@"subscribe()" completionHandler:^(id, NSError*) { }];
    TestWebKitAPI::Util::run(&gotMessage);
    RetainPtr<id> subscription = message;
    ASSERT_FALSE([subscription isEqual:[NSNull null]]);
    ASSERT_TRUE([subscription isKindOfClass:[NSDictionary class]] && [subscription objectForKey:@"endpoint"]);

    message = nil;
    gotMessage = false;
    [m_webView evaluateJavaScript:@"getSubscription()" completionHandler:^(id, NSError*) { }];
    TestWebKitAPI::Util::run(&gotMessage);
    ASSERT_TRUE([message isEqual:subscription.get()]);

    // If the public token changes, all subscriptions should be invalidated.
    auto utilityConnection = createAndConfigureConnectionToService("org.webkit.webpushtestdaemon.service");
    sendMessageToDaemonWithReplySync(utilityConnection.get(), MessageType::SetPublicTokenForTesting, encodeString("foobar"_s));

    message = nil;
    gotMessage = false;
    [m_webView evaluateJavaScript:@"getSubscription()" completionHandler:^(id, NSError*) { }];
    TestWebKitAPI::Util::run(&gotMessage);
    ASSERT_TRUE([message isEqual:[NSNull null]]);
}

TEST_F(WebPushDMultipleLaunchTest, GetPushSubscriptionAfterDaemonRelaunch)
{
    static constexpr auto htmlSource = R"HTML(
    <script src="/constants.js"></script>
    <script>
    let postNoteMessage = window.webkit.messageHandlers.note.postMessage.bind(window.webkit.messageHandlers.note);
    let getPushManager =
        navigator.serviceWorker.register('/sw.js')
            .then(() => navigator.serviceWorker.ready)
            .then(registration => registration.pushManager);

    function getSubscription()
    {
        getPushManager
            .then(pushManager => pushManager.getSubscription())
            .then(subscription => subscription ? subscription.toJSON() : null)
            .then(postNoteMessage)
            .catch(e => postNoteMessage(e.toString()));
    }

    postNoteMessage('Ready');
    </script>
    )HTML"_s;

    __block RetainPtr<id> message = nil;
    __block bool gotMessage = false;
    [m_notificationMessageHandler setMessageHandler:^(id receivedMessage) {
        message = receivedMessage;
        gotMessage = true;
    }];

    loadRequest(htmlSource, ""_s);
    TestWebKitAPI::Util::run(&gotMessage);
    ASSERT_TRUE([message isEqualToString:@"Ready"]);

    message = nil;
    gotMessage = false;
    [m_webView evaluateJavaScript:@"getSubscription()" completionHandler:^(id, NSError*) { }];
    TestWebKitAPI::Util::run(&gotMessage);
    ASSERT_TRUE([message isEqual:[NSNull null]]);

    ASSERT_TRUE(restartTestWebPushD());

    // Make sure that getSubscription works after killing webpushd. Previously, this didn't work and
    // would fail with an AbortError because we didn't re-send the connection configuration after
    // the daemon relaunched.
    message = nil;
    gotMessage = false;
    [m_webView evaluateJavaScript:@"getSubscription()" completionHandler:^(id, NSError*) { }];
    TestWebKitAPI::Util::run(&gotMessage);
    ASSERT_TRUE([message isEqual:[NSNull null]]);
}

class WebPushDITPTest : public WebPushDTest {
public:
    static constexpr Seconds days { 3600.0 * 24 };

    void SetUp() override
    {
        __block bool ready = false;
        [m_testMessageHandler addMessage:@"Ready" withHandler:^{
            ready = true;
        }];
        [m_testMessageHandler addMessage:@"Received: test aesgcm payload" withHandler:^{
            m_gotPushEventInServiceWorker = true;
        }];
        [m_testMessageHandler addMessage:@"Received: notificationclick" withHandler:^{
            m_gotNotificationClick = true;
        }];

        loadRequest(injectedPushHTMLSource, injectedPushServiceWorkerSource);
        TestWebKitAPI::Util::run(&ready);
    }

    void setITPTimeAdvance(Seconds advance)
    {
        if (advance.seconds() <= 0)
            return;

        __block bool done = false;
        [m_dataStore _setResourceLoadStatisticsTimeAdvanceForTesting:advance.value() completionHandler:^{
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);

        done = false;
        [m_dataStore _processStatisticsAndDataRecords:^{
            done = true;
        }];
        Util::run(&done);
    }

    void assertPushEventSucceeds(Seconds advance)
    {
        setITPTimeAdvance(advance);

        m_gotPushEventInServiceWorker = false;
        auto message = injectPushMessage(aesGCMPushData());
        ASSERT_TRUE(message) << "Unexpected push event injection failure after advancing timer by " << (advance / days) << " days; spurious ITP cleanup?";

        __block bool pushResult = false;
        __block bool processedPush = false;
        [m_dataStore _processPushMessage:message.get() completionHandler:^(bool result) {
            pushResult = result;
            processedPush = true;
        }];
        TestWebKitAPI::Util::run(&processedPush);
        ASSERT_TRUE(pushResult) << "Unexpected push event processing failure after advancing ITP timer by " << (advance / days) << " days; spurious ITP cleanup?";

        TestWebKitAPI::Util::run(&m_gotPushEventInServiceWorker);
    }

    void assertPushEventFails(Seconds advance)
    {
        setITPTimeAdvance(advance);

        auto message = injectPushMessage(aesGCMPushData());
        ASSERT_FALSE(message) << "Unexpected push event injection success after advancing ITP timer by " << (advance / days) << " days; missing ITP cleanup?";
    }

    void simulateNotificationClick()
    {
        m_gotNotificationClick = false;
        ASSERT_TRUE(m_notificationProvider->simulateNotificationClick());
        TestWebKitAPI::Util::run(&m_gotNotificationClick);
    }

private:
    bool m_gotPushEventInServiceWorker { false };
    bool m_gotNotificationClick { false };
};

TEST_F(WebPushDITPTest, PushSubscriptionExtendsITPCleanupTimerBy30Days)
{
    assertPushEventSucceeds(0 * days);
    assertPushEventSucceeds(29 * days);
    EXPECT_TRUE(hasPushSubscription());

    assertPushEventFails(31 * days);
    assertPushEventFails(100 * days);
    EXPECT_FALSE(hasPushSubscription());
}

TEST_F(WebPushDITPTest, NotificationClickExtendsITPCleanupTimerBy30Days)
{
    assertPushEventSucceeds(0 * days);
    assertPushEventSucceeds(29 * days);
    simulateNotificationClick();
    EXPECT_TRUE(hasPushSubscription());

    assertPushEventSucceeds(58 * days);
    EXPECT_TRUE(hasPushSubscription());

    assertPushEventFails(61 * days);
    EXPECT_FALSE(hasPushSubscription());
}

} // namespace TestWebKitAPI

#endif // ENABLE(NOTIFICATIONS) && ENABLE(NOTIFICATION_EVENT) && (PLATFORM(MAC) || PLATFORM(IOS))
