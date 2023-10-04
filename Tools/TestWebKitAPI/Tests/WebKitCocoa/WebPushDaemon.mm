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

#import "DaemonTestUtilities.h"
#import "Encoder.h"
#import "HTTPServer.h"
#import "MessageSenderInlines.h"
#import "PlatformUtilities.h"
#import "PushClientConnectionMessages.h"
#import "PushMessageForTesting.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestNotificationProvider.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import "WebPushDaemonConnectionConfiguration.h"
#import <WebCore/PushSubscriptionIdentifier.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebsiteDataRecordPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebPushDaemonConstants.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKNotificationData.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <WebKit/_WKWebsiteDataStoreDelegate.h>
#import <mach/mach_init.h>
#import <mach/task.h>
#import <wtf/BlockPtr.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/UUID.h>
#import <wtf/UniqueRef.h>
#import <wtf/spi/darwin/XPCSPI.h>
#import <wtf/text/Base64.h>

// FIXME: Work through enabling on iOS
#if ENABLE(NOTIFICATIONS) && ENABLE(NOTIFICATION_EVENT) && PLATFORM(MAC)

static bool alertReceived = false;
@interface PushNotificationDelegate : NSObject<WKUIDelegatePrivate, _WKWebsiteDataStoreDelegate> {
    RetainPtr<_WKNotificationData> _mostRecentNotification;
    RetainPtr<NSURL> _mostRecentActionURL;
    std::optional<uint64_t> _mostRecentAppBadge;
}
-(void)clearMostRecents;

@property (nonatomic, readonly) RetainPtr<_WKNotificationData> mostRecentNotification;
@property (nonatomic, readonly) RetainPtr<NSURL> mostRecentActionURL;
@property (nonatomic, readonly) std::optional<uint64_t> mostRecentAppBadge;
@end

@implementation PushNotificationDelegate

-(void)clearMostRecents
{
    _mostRecentNotification = nullptr;
    _mostRecentActionURL = nullptr;
    _mostRecentAppBadge = std::nullopt;
}

- (void)_webView:(WKWebView *)webView requestNotificationPermissionForSecurityOrigin:(WKSecurityOrigin *)securityOrigin decisionHandler:(void (^)(BOOL))decisionHandler
{
    decisionHandler(true);
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    alertReceived = true;
    completionHandler();
}

- (void)websiteDataStore:(WKWebsiteDataStore *)dataStore showNotification:(_WKNotificationData *)notificationData
{
    _mostRecentNotification = notificationData;
}

- (void)websiteDataStore:(WKWebsiteDataStore *)dataStore workerOrigin:(WKSecurityOrigin *)workerOrigin updatedAppBadge:(NSNumber *)badge
{
    if (badge)
        _mostRecentAppBadge = [badge unsignedLongLongValue];
    else
        _mostRecentAppBadge = std::nullopt;
}

- (void)websiteDataStore:(WKWebsiteDataStore *)dataStore navigateToNotificationActionURL:(NSURL *)url
{
    _mostRecentActionURL = url;
}

@end

namespace TestWebKitAPI {

static RetainPtr<NSURL> testWebPushDaemonLocation()
{
    return [currentExecutableDirectory() URLByAppendingPathComponent:@"webpushd" isDirectory:NO];
}

enum LaunchOnlyOnce : BOOL { No, Yes };
enum class InstallDataStoreDelegate : bool { No, Yes };

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

class WebPushXPCConnectionMessageSender final : public IPC::MessageSender {
public:
    WebPushXPCConnectionMessageSender(xpc_connection_t connection)
        : m_connection(connection)
    {
    }
    ~WebPushXPCConnectionMessageSender() final { };

    void setShouldIncrementProtocolVersionForTesting() { m_shouldIncrementProtocolVersionForTesting = true; }

private:
    bool performSendWithoutUsingIPCConnection(UniqueRef<IPC::Encoder>&&) const final;
    bool performSendWithAsyncReplyWithoutUsingIPCConnection(UniqueRef<IPC::Encoder>&&, CompletionHandler<void(IPC::Decoder*)>&&) const final;

    OSObjectPtr<xpc_object_t> messageDictionaryFromEncoder(UniqueRef<IPC::Encoder>&&) const;

    IPC::Connection* messageSenderConnection() const final { return nullptr; }
    uint64_t messageSenderDestinationID() const final { return 0; }

    OSObjectPtr<xpc_connection_t> m_connection;
    bool m_shouldIncrementProtocolVersionForTesting { false };
};

OSObjectPtr<xpc_object_t> WebPushXPCConnectionMessageSender::messageDictionaryFromEncoder(UniqueRef<IPC::Encoder>&& encoder) const
{
    auto dictionary = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));

    uint64_t protocolVersion = WebKit::WebPushD::protocolVersionValue;
    if (m_shouldIncrementProtocolVersionForTesting)
        ++protocolVersion;
    xpc_dictionary_set_uint64(dictionary.get(), WebKit::WebPushD::protocolVersionKey, protocolVersion);

    __block auto blockEncoder = WTFMove(encoder);
    auto dispatchData = adoptNS(dispatch_data_create(blockEncoder->buffer(), blockEncoder->bufferSize(), dispatch_get_main_queue(), ^{
        // Explicitly clear out the encoder, destroying it.
        blockEncoder.moveToUniquePtr();
    }));
    auto encoderData = adoptOSObject(xpc_data_create_with_dispatch_data(dispatchData.get()));

    xpc_dictionary_set_value(dictionary.get(), WebKit::WebPushD::protocolEncodedMessageKey, encoderData.get());

    return dictionary;
}

bool WebPushXPCConnectionMessageSender::performSendWithoutUsingIPCConnection(UniqueRef<IPC::Encoder>&& encoder) const
{
    auto dictionary = messageDictionaryFromEncoder(WTFMove(encoder));
    xpc_connection_send_message(m_connection.get(), dictionary.get());

    return true;
}

bool WebPushXPCConnectionMessageSender::performSendWithAsyncReplyWithoutUsingIPCConnection(UniqueRef<IPC::Encoder>&& encoder, CompletionHandler<void(IPC::Decoder*)>&& completionHandler) const
{
    auto dictionary = messageDictionaryFromEncoder(WTFMove(encoder));

    xpc_connection_send_message_with_reply(m_connection.get(), dictionary.get(), dispatch_get_main_queue(), makeBlockPtr([this, completionHandler = WTFMove(completionHandler)] (xpc_object_t reply) mutable {
        if (xpc_get_type(reply) == XPC_TYPE_ERROR) {
            // We only expect an error if we were purposefully testing the wrong protocol version.
            RELEASE_ASSERT(m_shouldIncrementProtocolVersionForTesting);
            return completionHandler(nullptr);
        }

        if (xpc_get_type(reply) != XPC_TYPE_DICTIONARY) {
            RELEASE_ASSERT_NOT_REACHED();
            return completionHandler(nullptr);
        }
        if (xpc_dictionary_get_uint64(reply, WebKit::WebPushD::protocolVersionKey) != WebKit::WebPushD::protocolVersionValue) {
            RELEASE_ASSERT_NOT_REACHED();
            return completionHandler(nullptr);
        }

        size_t dataSize { 0 };
        const uint8_t* data = static_cast<const uint8_t *>(xpc_dictionary_get_data(reply, WebKit::WebPushD::protocolEncodedMessageKey, &dataSize));
        auto decoder = IPC::Decoder::create({ data, dataSize }, { });
        ASSERT(decoder);

        completionHandler(decoder.get());
    }).get());

    return true;
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

    WebKit::WebPushD::WebPushDaemonConnectionConfiguration configuration;
    configuration.hostAppAuditTokenData = Vector<unsigned char>(sizeof(token));
    memcpy(configuration.hostAppAuditTokenData->data(), &token, sizeof(token));

    auto sender = WebPushXPCConnectionMessageSender { connection };
    sender.sendWithoutUsingIPCConnection(Messages::PushClientConnection::UpdateConnectionConfiguration(configuration));
}

RetainPtr<xpc_connection_t> createAndConfigureConnectionToService(const char* serviceName)
{
    auto connection = adoptNS(xpc_connection_create_mach_service(serviceName, dispatch_get_main_queue(), 0));
    xpc_connection_set_event_handler(connection.get(), ^(xpc_object_t) { });
    xpc_connection_activate(connection.get());
    sendConfigurationWithAuditToken(connection.get());

    return WTFMove(connection);
}

TEST(WebPushD, BasicCommunication)
{
    NSURL *tempDir = setUpTestWebPushD();

    auto connection = adoptNS(xpc_connection_create_mach_service("org.webkit.webpushtestdaemon.service", dispatch_get_main_queue(), 0));

    __block bool done = false;
    __block bool interrupted = false;
    xpc_connection_set_event_handler(connection.get(), ^(xpc_object_t request) {
        if (request == XPC_ERROR_CONNECTION_INTERRUPTED) {
            interrupted = true;
            return;
        }
        if (xpc_get_type(request) != XPC_TYPE_DICTIONARY)
            return;
        const char* debugMessage = xpc_dictionary_get_string(request, WebKit::WebPushD::protocolDebugMessageKey);
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
        auto sender = WebPushXPCConnectionMessageSender { connection.get() };
        sender.sendWithoutUsingIPCConnection(Messages::PushClientConnection::SetDebugModeIsEnabled(true));
        TestWebKitAPI::Util::run(&done);
    }

    // Sending a message with a higher protocol version should cause the connection to be terminated
    auto sender = WebPushXPCConnectionMessageSender { connection.get() };
    sender.setShouldIncrementProtocolVersionForTesting();
    __block bool messageReplied = false;
    sender.sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::RemoveAllPushSubscriptions(), ^(bool result) {
        EXPECT_FALSE(result);
        messageReplied = true;
    });

    TestWebKitAPI::Util::run(&messageReplied);
    TestWebKitAPI::Util::run(&interrupted);
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

static ASCIILiteral validServerKey = "BA1Hxzyi1RUM1b5wjxsn7nGxAszw2u61m164i3MrAIxHF6YK5h4SDYic-dRuU_RCPCfA5aq9ojSwk5Y2EmClBPs"_s;
static ASCIILiteral keyThatCausesInjectedFailure = "BEAxaUMo1s8tjORxJfnSSvWhYb4u51kg1hWT2s_9gpV7Zxar1pF_2BQ8AncuAdS2BoLhN4qaxzBy2CwHE8BBzWg"_s;

static constexpr auto navigatorHTMLSource = R"SRC(
<script>
let globalSubscription = null;

function log(msg)
{
    window.webkit.messageHandlers.test.postMessage(msg);
}

window.onload = function()
{
    log("Ready");
}

async function subscribe(key)
{
    try {
        globalSubscription = await navigator.pushManager.subscribe({
            userVisibleOnly: true,
            applicationServerKey: key
        });
        return globalSubscription.toJSON();
    } catch (error) {
        return "Error: " + error;
    }
}

async function unsubscribe()
{
    try {
        let result = await globalSubscription.unsubscribe();
        return result;
    } catch (error) {
        return "Error: " + error;
    }
}

async function getPushSubscription()
{
    try {
        let subscription = await navigator.pushManager.getSubscription();
        return subscription ? subscription.toJSON() : null;
    } catch (error) {
        return "Error: " + error;
    }
}
</script>
)SRC"_s;

static constexpr auto htmlSource = R"SRC(
<script>
let globalRegistration = null;
let globalSubscription = null;

function log(msg)
{
    window.webkit.messageHandlers.test.postMessage(msg);
}

const channel = new MessageChannel();
channel.port1.onmessage = (event) => log(event.data);

navigator.serviceWorker.register('/sw.js').then(async () => {
    globalRegistration = await navigator.serviceWorker.ready;
    globalRegistration.active.postMessage({ message: "setup", port: channel.port2 }, [channel.port2]);
}).catch(function(error) {
    log("Registration failed with: " + error);
});

async function subscribe(key)
{
    try {
        globalSubscription = await globalRegistration.pushManager.subscribe({
            userVisibleOnly: true,
            applicationServerKey: key
        });
        return globalSubscription.toJSON();
    } catch (error) {
        return "Error: " + error;
    }
}

async function unsubscribe()
{
    try {
        let result = await globalSubscription.unsubscribe();
        return result;
    } catch (error) {
        return "Error: " + error;
    }
}

async function getPushSubscription()
{
    try {
        let subscription = await globalRegistration.pushManager.getSubscription();
        return subscription ? subscription.toJSON() : null;
    } catch (error) {
        return "Error: " + error;
    }
}

async function disableShowNotifications()
{
    const channel = new MessageChannel();
    const promise = new Promise((resolve) => {
        channel.port1.onmessage = (event) => resolve(event.data);
    });
    globalRegistration.active.postMessage({ message: "disableShowNotifications", port: channel.port2 }, [channel.port2]);
    return await promise;
}
</script>
)SRC"_s;

static constexpr auto serviceWorkerScriptSource = R"SWRESOURCE(
let globalPort;
let showNotifications = true;

self.addEventListener("message", (event) => {
    let { message, port } = event.data;
    if (message === "setup") {
        globalPort = port;
        port.postMessage("Ready");
    } else if (message === "disableShowNotifications") {
        showNotifications = false;
        port.postMessage(true);
    }
});

self.addEventListener("pushnotification", async (event) => {
    // If the tag is empty, do nothing
    if (!event.proposedNotification.tag)
        return;

    var optionsFromTag = event.proposedNotification.tag.split(" ");
    var newTitle;
    var newBadge;
    var newActionURL;
    if (optionsFromTag[0] == "titleandbadge") {
        newTitle = optionsFromTag[1];
        newBadge = optionsFromTag[2];
    } else if (optionsFromTag[0] == "title")
        newTitle = optionsFromTag[1];
    else if (optionsFromTag[0] == "badge")
        newBadge = optionsFromTag[1];
    else if (optionsFromTag[0] == "datatotitle")
        newTitle = event.proposedNotification.data;
    else if (optionsFromTag[0] == "defaultactionurl")
        newActionURL = optionsFromTag[1];
    else if (optionsFromTag[0] == "emptydefaultactionurl") {
        self.registration.showNotification("Missing default action").then((value) => {
            globalPort.postMessage("showNotification succeeded");
        }, (exception) => {
            globalPort.postMessage("showNotification failed: " + exception);
        });
    }

    if (newTitle || newActionURL) {
        if (!newTitle)
            newTitle = event.proposedNotification.title;
        if (!newActionURL)
            newActionURL = event.proposedNotification.defaultAction;

        self.registration.showNotification(newTitle, { "defaultAction": newActionURL });
    }

    if (newBadge)
        navigator.setAppBadge(newBadge);
});

self.addEventListener("push", async (event) => {
    try {
        if (showNotifications) {
            await self.registration.showNotification("notification");
        }
        if (!event.data) {
            globalPort.postMessage("Received: null data");
            return;
        }
        const value = event.data.text();
        globalPort.postMessage("Received: " + value);
    } catch (e) {
        globalPort.postMessage("Error: " + e);
    }
});

self.addEventListener("notificationclick", () => {
    globalPort.postMessage("Received: notificationclick");
});
)SWRESOURCE"_s;

class WebPushDTestWebView {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WebPushDTestWebView(const String& pushPartition, const std::optional<WTF::UUID>& dataStoreIdentifier, WKProcessPool *processPool, TestNotificationProvider& notificationProvider, ASCIILiteral html, InstallDataStoreDelegate installDataStoreDelegate)
        : m_pushPartition(pushPartition)
        , m_dataStoreIdentifier(dataStoreIdentifier)
        , m_notificationProvider(notificationProvider)
    {
        m_origin = "https://example.com"_s;
        m_url = adoptNS([[NSURL alloc] initWithString:@"https://example.com/"]);
        m_testMessageHandler = adoptNS([[TestMessageHandler alloc] init]);

        __block bool ready = false;
        [m_testMessageHandler addMessage:@"Ready" withHandler:^{
            ready = true;
        }];

        m_server.reset(new TestWebKitAPI::HTTPServer({
            { "/"_s, { html } },
            { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, serviceWorkerScriptSource } }
        }, TestWebKitAPI::HTTPServer::Protocol::HttpsProxy));

        RetainPtr<_WKWebsiteDataStoreConfiguration> dataStoreConfiguration;
        if (dataStoreIdentifier)
            dataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initWithIdentifier:*dataStoreIdentifier]);
        else
            dataStoreConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
        [dataStoreConfiguration setWebPushPartitionString:pushPartition];
        [dataStoreConfiguration setProxyConfiguration:@{
            (NSString *)kCFStreamPropertyHTTPSProxyHost: @"127.0.0.1",
            (NSString *)kCFStreamPropertyHTTPSProxyPort: @(m_server->port())
        }];
        dataStoreConfiguration.get().webPushMachServiceName = @"org.webkit.webpushtestdaemon.service";
        dataStoreConfiguration.get().webPushDaemonUsesMockBundlesForTesting = YES;

#if ENABLE(DECLARATIVE_WEB_PUSH)
        dataStoreConfiguration.get().isDeclarativeWebPushEnabled = YES;
#endif

        // FIXME: This seems like it shouldn't be necessary, but _clearResourceLoadStatistics (called by clearWebsiteDataStore) doesn't seem to work.
        [[NSFileManager defaultManager] removeItemAtURL:[dataStoreConfiguration _resourceLoadStatisticsDirectory] error:nil];

        m_dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);

        if (installDataStoreDelegate == InstallDataStoreDelegate::Yes) {
            m_delegate = adoptNS([[PushNotificationDelegate alloc] init]);
            m_dataStore.get()._delegate = m_delegate.get();
        }

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

        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [configuration setProcessPool:processPool];
        [configuration setWebsiteDataStore:m_dataStore.get()];
        configuration.get().preferences._appBadgeEnabled = YES;

        auto userContentController = [configuration userContentController];
        [userContentController addScriptMessageHandler:m_testMessageHandler.get() name:@"test"];

        [[configuration preferences] _setPushAPIEnabled:YES];
        m_notificationProvider.setPermission(m_origin, true);

#if ENABLE(DECLARATIVE_WEB_PUSH)
        NSArray<_WKFeature *> * features = WKPreferences._features;
        for (_WKFeature *feature in features) {
            if ([feature.key isEqualToString:@"DeclarativeWebPush"]) {
                [configuration.get().preferences _setEnabled:YES forFeature:feature];
                break;
            }
        }
#endif

        m_webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

        [m_webView setUIDelegate:m_delegate.get()];

        auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
        navigationDelegate.get().didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
            completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
        };
        [m_webView setNavigationDelegate:navigationDelegate.get()];

        [m_webView loadRequest:[NSURLRequest requestWithURL:m_url.get()]];

        TestWebKitAPI::Util::run(&ready);
    }

    std::optional<WTF::UUID> dataStoreIdentifier() { return m_dataStoreIdentifier; }

    const String& origin() { return m_origin; }

    RetainPtr<WKWebsiteDataStore> dataStore() { return m_dataStore; }

    id subscribe(String key = validServerKey)
    {
        NSError *error = nil;
        auto script = makeString("return await subscribe('"_s, key, "')"_s);
        id obj = [m_webView objectByCallingAsyncFunction:script withArguments:@{ } error:&error];
        return error ?: obj;
    }

    id unsubscribe()
    {
        NSError *error = nil;
        id obj = [m_webView objectByCallingAsyncFunction:@"return await unsubscribe()" withArguments:@{ } error:&error];
        return error ?: obj;
    }

    id getPushSubscription()
    {
        NSError *error = nil;
        id obj = [m_webView objectByCallingAsyncFunction:@"return await getPushSubscription()" withArguments:@{ } error:&error];
        return error ?: obj;
    }

    bool hasPushSubscription()
    {
        return [getPushSubscription() isKindOfClass:[NSDictionary class]];
    }

    // Can be used in cases where the service worker was unregistered (in which case
    // hasPushSubscription would fail, since PushManager.getSubscription() fails if there is no
    // active service worker).
    bool hasPushSubscriptionForTesting()
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

    id unregisterServiceWorker()
    {
        NSError *error = nil;
        id obj = [m_webView objectByCallingAsyncFunction:@"return await globalRegistration.unregister()" withArguments:@{ } error:&error];
        return error ?: obj;
    }

    void disableShowNotifications()
    {
        NSError *error = nil;
        id obj = [m_webView objectByCallingAsyncFunction:@"return await disableShowNotifications()" withArguments:@{ } error:&error];
        ASSERT_FALSE(error);
        ASSERT_TRUE([obj isEqual:@YES]);
    }

    void resetPermission()
    {
        m_notificationProvider.resetPermission(m_origin);
    }

    void setPermission(bool value)
    {
        m_notificationProvider.setPermission(m_origin, value);
    }


#if ENABLE(DECLARATIVE_WEB_PUSH)
    void injectDeclarativePushMessage(ASCIILiteral json, ASCIILiteral url = "https://example.com"_s)
    {
        WebKit::WebPushD::PushMessageForTesting message;
        message.targetAppCodeSigningIdentifier = "com.apple.WebKit.TestWebKitAPI"_s;
        message.registrationURL = URL(url);
        message.disposition = WebKit::WebPushD::PushMessageDisposition::Notification;
        message.payload = json;

        auto utilityConnection = createAndConfigureConnectionToService("org.webkit.webpushtestdaemon.service");
        auto sender = WebPushXPCConnectionMessageSender { utilityConnection.get() };
        __block bool done = false;
        sender.sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::InjectPushMessageForTesting(message), ^(const String& error) {
            if (!error.isEmpty())
                NSLog(@"ERROR: %s", error.utf8().data());
            done = true;
        });
        TestWebKitAPI::Util::run(&done);
    }
#endif

    void clearMostRecents()
    {
        [m_delegate clearMostRecents];
    }

    _WKNotificationData *mostRecentNotification()
    {
        return m_delegate.get().mostRecentNotification.get();
    }

    NSURL *mostRecentActionURL()
    {
        return m_delegate.get().mostRecentActionURL.get();
    }

    std::optional<uint64_t> mostRecentAppBadge()
    {
        return m_delegate.get().mostRecentAppBadge;
    }

    void injectPushMessage(NSDictionary *apsUserInfo)
    {
        String scope = [m_url absoluteString];
        WebCore::PushSubscriptionSetIdentifier subscriptionSetIdentifier {
            .bundleIdentifier = "com.apple.WebKit.TestWebKitAPI"_s,
            .pushPartition = m_pushPartition,
            .dataStoreIdentifier = m_dataStoreIdentifier
        };
        auto topic = WebCore::makePushTopic(subscriptionSetIdentifier, scope);
        id obj = @{
            @"topic": (NSString *)topic,
            @"userInfo": apsUserInfo
        };
        NSData *data = [NSJSONSerialization dataWithJSONObject:obj options:0 error:nullptr];

        String message { static_cast<const char *>(data.bytes), static_cast<unsigned>(data.length) };

        auto utilityConnection = createAndConfigureConnectionToService("org.webkit.webpushtestdaemon.service");
        auto sender = WebPushXPCConnectionMessageSender { utilityConnection.get() };
        __block bool done = false;
        sender.sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::InjectEncryptedPushMessageForTesting(message), ^(bool injected) {
            done = true;
        });
        TestWebKitAPI::Util::run(&done);
    }

    RetainPtr<NSArray<NSDictionary *>> fetchPushMessages()
    {
        __block bool gotMessages = false;
        __block RetainPtr<NSArray<NSDictionary *>> messages;
        [m_dataStore _getPendingPushMessages:^(NSArray<NSDictionary *> *rawMessages) {
            messages = rawMessages;
            gotMessages = true;
        }];
        TestWebKitAPI::Util::run(&gotMessages);

        return messages;
    }

    bool expectDecryptedMessage(NSString *expectedMessage, NSDictionary *message)
    {
        __block bool gotExpectedMessage = false;
        [m_testMessageHandler addMessage:[NSString stringWithFormat:@"Received: %@", expectedMessage] withHandler:^{
            gotExpectedMessage = true;
        }];

        // This will result in this process grabbing the queued pushes from webpushd and firing the push event in the service worker with that data.
        __block bool pushMessageProcessed = false;
        __block bool pushMessageProcessedResult = false;
        [m_dataStore _processPushMessage:message completionHandler:^(bool result) {
            pushMessageProcessedResult = result;
            pushMessageProcessed = true;
        }];
        TestWebKitAPI::Util::run(&pushMessageProcessed);
        TestWebKitAPI::Util::run(&gotExpectedMessage);

        return pushMessageProcessedResult;
    }

    void expectDataStoreIdentifierSetOnLastNotification()
    {
        auto identifier = m_notificationProvider.lastNotificationDataStoreIdentifier();
        if (m_dataStoreIdentifier)
            EXPECT_WK_STREQ(m_dataStoreIdentifier->toString().utf8().data(), identifier);
        else
            EXPECT_NULL(identifier);
    }

    void simulateNotificationClick()
    {
        __block bool gotNotificationClick = false;
        [m_testMessageHandler addMessage:@"Received: notificationclick" withHandler:^{
            gotNotificationClick = true;
        }];
        ASSERT_TRUE(m_notificationProvider.simulateNotificationClick());
        TestWebKitAPI::Util::run(&gotNotificationClick);
    }

    void setITPTimeAdvance(unsigned daysToAdvance)
    {
        static constexpr Seconds days { 3600.0 * 24 };
        auto advance = days * daysToAdvance;

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

    void assertPushEventSucceeds(unsigned daysToAdvance)
    {
        setITPTimeAdvance(daysToAdvance);

        injectPushMessage(@{ });
        auto messages = fetchPushMessages();
        ASSERT_EQ([messages count], 1u) << "Unexpected push event injection failure after advancing timer by " << daysToAdvance << " days; spurious ITP cleanup?";

        expectDecryptedMessage(@"null data", [messages firstObject]);
    }

    void assertPushEventFails(unsigned daysToAdvance)
    {
        setITPTimeAdvance(daysToAdvance);

        injectPushMessage(@{ });
        auto messages = fetchPushMessages();
        ASSERT_EQ([messages count], 0u) << "Unexpected push event injection success after advancing ITP timer by " << daysToAdvance << " days; missing ITP cleanup?";
    }

    void processPushMessage(NSDictionary *pushMessage)
    {
        __block bool done = false;
        [m_dataStore _processPushMessage:pushMessage completionHandler:^(bool result) {
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
    }

    void captureAllMessages()
    {
        [m_testMessageHandler setWildcardMessageHandler:^(NSString *message){
            m_mostRecentMessage = message;
        }];
    }

    const String& mostRecentMessage() const
    {
        return m_mostRecentMessage;
    }

private:
    String m_pushPartition;
    Markable<WTF::UUID> m_dataStoreIdentifier;
    String m_origin;
    RetainPtr<NSURL> m_url;
    RetainPtr<WKWebsiteDataStore> m_dataStore;
    RetainPtr<PushNotificationDelegate> m_delegate;
    RetainPtr<TestMessageHandler> m_testMessageHandler;
    std::unique_ptr<TestWebKitAPI::HTTPServer> m_server;
    TestNotificationProvider& m_notificationProvider;
    RetainPtr<WKWebView> m_webView;
    String m_mostRecentMessage;
};

class WebPushDTest : public ::testing::Test {
public:
    WebPushDTest(LaunchOnlyOnce launchOnlyOnce = LaunchOnlyOnce::Yes, ASCIILiteral html = htmlSource, InstallDataStoreDelegate installDataStoreDelegate = InstallDataStoreDelegate::No)
        : m_html(html)
        , m_installDataStoreDelegate(installDataStoreDelegate)
    {
        m_tempDirectory = retainPtr(setUpTestWebPushD(launchOnlyOnce));
    }

    void SetUp() override
    {
        auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
        auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

        m_notificationProvider = makeUnique<TestWebKitAPI::TestNotificationProvider>(Vector<WKNotificationManagerRef> { [processPool _notificationManagerForTesting], WKNotificationManagerGetSharedServiceWorkerNotificationManager() });

        auto webView = makeUniqueRef<WebPushDTestWebView>(emptyString(), std::nullopt, processPool.get(), *m_notificationProvider, m_html, m_installDataStoreDelegate);
        m_webViews.append(WTFMove(webView));

        auto webViewWithIdentifier1 = makeUniqueRef<WebPushDTestWebView>(emptyString(), WTF::UUID::parse("0bf5053b-164c-4b7d-8179-832e6bf158df"_s), processPool.get(), *m_notificationProvider, m_html, m_installDataStoreDelegate);
        m_webViews.append(WTFMove(webViewWithIdentifier1));

        auto webViewWithIdentifier2 = makeUniqueRef<WebPushDTestWebView>(emptyString(), WTF::UUID::parse("940e7729-738e-439f-a366-1a8719e23b2d"_s), processPool.get(), *m_notificationProvider, m_html, m_installDataStoreDelegate);
        m_webViews.append(WTFMove(webViewWithIdentifier2));

        auto webViewWithPartition = makeUniqueRef<WebPushDTestWebView>("testPartition"_s, std::nullopt, processPool.get(), *m_notificationProvider, m_html, m_installDataStoreDelegate);
        m_webViews.append(WTFMove(webViewWithPartition));

        auto webViewWithPartitionAndIdentifier = makeUniqueRef<WebPushDTestWebView>("testPartition"_s, WTF::UUID::parse("940e7729-738e-439f-a366-1a8719e23b2d"_s), processPool.get(), *m_notificationProvider, m_html, m_installDataStoreDelegate);
        m_webViews.append(WTFMove(webViewWithPartitionAndIdentifier));
    }

    ~WebPushDTest()
    {
        cleanUpTestWebPushD(m_tempDirectory.get());
    }

    Vector<UniqueRef<WebPushDTestWebView>>& webViews() { return m_webViews; }

    std::pair<Vector<String>, Vector<String>> getPushTopics()
    {
        Vector<String> enabledTopics;
        Vector<String> ignoredTopics;
        auto connection = createAndConfigureConnectionToService("org.webkit.webpushtestdaemon.service");
        auto sender = WebPushXPCConnectionMessageSender { connection.get() };
        bool done = false;
        sender.sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::GetPushTopicsForTesting(), [&](Vector<String> enabled, Vector<String> ignored) {
            enabledTopics = enabled;
            ignoredTopics = ignored;
            done = true;
        });
        TestWebKitAPI::Util::run(&done);

        return std::make_pair(WTFMove(enabledTopics), WTFMove(ignoredTopics));
    }

    size_t subscribedTopicsCount() { return getPushTopics().first.size(); }

protected:
    RetainPtr<NSURL> m_tempDirectory;
    std::unique_ptr<TestWebKitAPI::TestNotificationProvider> m_notificationProvider;
    Vector<UniqueRef<WebPushDTestWebView>> m_webViews;
    ASCIILiteral m_html;
    InstallDataStoreDelegate m_installDataStoreDelegate { InstallDataStoreDelegate::No };
};

class WebPushDMultipleLaunchTest : public WebPushDTest {
public:
    WebPushDMultipleLaunchTest()
        : WebPushDTest(LaunchOnlyOnce::No)
    {
    }
};

class WebPushDNavigatorTest : public WebPushDTest {
public:
    WebPushDNavigatorTest()
        : WebPushDTest(LaunchOnlyOnce::Yes, navigatorHTMLSource)
    {
    }
};

TEST_F(WebPushDTest, SubscribeTest)
{
    for (auto& v : webViews()) {
        ASSERT_FALSE(v->hasPushSubscription());
        id obj = v->subscribe();
        ASSERT_TRUE(v->hasPushSubscription());

        ASSERT_TRUE([obj isKindOfClass:[NSDictionary class]]);
        NSDictionary *subscription = obj;
        ASSERT_TRUE([subscription[@"endpoint"] hasPrefix:@"https://"]);
        ASSERT_TRUE([subscription[@"keys"] isKindOfClass:[NSDictionary class]]);

        // Shared auth secret should be 16 bytes (22 bytes in unpadded base64url).
        ASSERT_EQ([subscription[@"keys"][@"auth"] length], 22u);

        // Client public key should be 65 bytes (87 bytes in unpadded base64url).
        ASSERT_EQ([subscription[@"keys"][@"p256dh"] length], 87u);
    }

    auto lessThan = [](const String& lhs, const String& rhs) {
        return codePointCompare(lhs, rhs) < 0;
    };
    auto topics = getPushTopics();
    auto& subscribed = topics.first;
    std::sort(subscribed.begin(), subscribed.end(), lessThan);

    Vector<String> expected {
        "com.apple.WebKit.TestWebKitAPI ds:0bf5053b-164c-4b7d-8179-832e6bf158df https://example.com/"_s,
        "com.apple.WebKit.TestWebKitAPI ds:940e7729-738e-439f-a366-1a8719e23b2d https://example.com/"_s,
        "com.apple.WebKit.TestWebKitAPI https://example.com/"_s,
        "com.apple.WebKit.TestWebKitAPI part:testPartition ds:940e7729-738e-439f-a366-1a8719e23b2d https://example.com/"_s,
        "com.apple.WebKit.TestWebKitAPI part:testPartition https://example.com/"_s
    };
    ASSERT_EQ(subscribed, expected);

    auto& ignored = topics.second;
    ASSERT_EQ(ignored.size(), 0u);
}

#if ENABLE(DECLARATIVE_WEB_PUSH)
TEST_F(WebPushDNavigatorTest, SubscribeTest)
{
    for (auto& v : webViews()) {
        ASSERT_FALSE(v->hasPushSubscription());
        id obj = v->subscribe();
        ASSERT_TRUE(v->hasPushSubscription());

        ASSERT_TRUE([obj isKindOfClass:[NSDictionary class]]);
        NSDictionary *subscription = obj;
        ASSERT_TRUE([subscription[@"endpoint"] hasPrefix:@"https://"]);
        ASSERT_TRUE([subscription[@"keys"] isKindOfClass:[NSDictionary class]]);

        // Shared auth secret should be 16 bytes (22 bytes in unpadded base64url).
        ASSERT_EQ([subscription[@"keys"][@"auth"] length], 22u);

        // Client public key should be 65 bytes (87 bytes in unpadded base64url).
        ASSERT_EQ([subscription[@"keys"][@"p256dh"] length], 87u);
    }

    auto lessThan = [](const String& lhs, const String& rhs) {
        return codePointCompare(lhs, rhs) < 0;
    };
    auto topics = getPushTopics();
    auto& subscribed = topics.first;
    std::sort(subscribed.begin(), subscribed.end(), lessThan);

    Vector<String> expected {
        "com.apple.WebKit.TestWebKitAPI ds:0bf5053b-164c-4b7d-8179-832e6bf158df https://example.com/"_s,
        "com.apple.WebKit.TestWebKitAPI ds:940e7729-738e-439f-a366-1a8719e23b2d https://example.com/"_s,
        "com.apple.WebKit.TestWebKitAPI https://example.com/"_s,
        "com.apple.WebKit.TestWebKitAPI part:testPartition ds:940e7729-738e-439f-a366-1a8719e23b2d https://example.com/"_s,
        "com.apple.WebKit.TestWebKitAPI part:testPartition https://example.com/"_s
    };
    ASSERT_EQ(subscribed, expected);

    auto& ignored = topics.second;
    ASSERT_EQ(ignored.size(), 0u);
}
#endif // ENABLE(DECLARATIVE_WEB_PUSH)

TEST_F(WebPushDTest, SubscribeFailureTest)
{
    for (auto& v : webViews()) {
        ASSERT_FALSE(v->hasPushSubscription());
        id obj = v->subscribe(keyThatCausesInjectedFailure);
        ASSERT_FALSE(v->hasPushSubscription());

        // Spec says that an error in the push service should be an AbortError.
        ASSERT_TRUE([obj isKindOfClass:[NSString class]]);
        ASSERT_TRUE([obj hasPrefix:@"Error: AbortError"]);
    }

    ASSERT_EQ(subscribedTopicsCount(), 0u);
}

#if ENABLE(DECLARATIVE_WEB_PUSH)
TEST_F(WebPushDNavigatorTest, SubscribeFailureTest)
{
    for (auto& v : webViews()) {
        ASSERT_FALSE(v->hasPushSubscription());
        id obj = v->subscribe(keyThatCausesInjectedFailure);
        ASSERT_FALSE(v->hasPushSubscription());

        // Spec says that an error in the push service should be an AbortError.
        ASSERT_TRUE([obj isKindOfClass:[NSString class]]);
        ASSERT_TRUE([obj hasPrefix:@"Error: AbortError"]);
    }

    ASSERT_EQ(subscribedTopicsCount(), 0u);
}
#endif

TEST_F(WebPushDTest, UnsubscribeTest)
{
    for (auto& v : webViews())
        v->subscribe();
    ASSERT_EQ(subscribedTopicsCount(), webViews().size());

    int i = 1;
    for (auto& v : webViews()) {
        ASSERT_TRUE(v->hasPushSubscription());

        // First unsubscribe should succeed.
        ASSERT_TRUE([v->unsubscribe() isEqual:@YES]);
        ASSERT_FALSE(v->hasPushSubscription());

        // Second unsubscribe should fail since the first one removed the record already.
        ASSERT_TRUE([v->unsubscribe() isEqual:@NO]);
        ASSERT_FALSE(v->hasPushSubscription());

        // Unsubscribing from this data store should not affect subscriptions in other data stores.
        ASSERT_EQ(subscribedTopicsCount(), webViews().size() - i);
        i++;
    }
}

#if ENABLE(DECLARATIVE_WEB_PUSH)
TEST_F(WebPushDNavigatorTest, UnsubscribeTest)
{
    for (auto& v : webViews())
        v->subscribe();
    ASSERT_EQ(subscribedTopicsCount(), webViews().size());

    int i = 1;
    for (auto& v : webViews()) {
        ASSERT_TRUE(v->hasPushSubscription());

        // First unsubscribe should succeed.
        ASSERT_TRUE([v->unsubscribe() isEqual:@YES]);
        ASSERT_FALSE(v->hasPushSubscription());

        // Second unsubscribe should fail since the first one removed the record already.
        ASSERT_TRUE([v->unsubscribe() isEqual:@NO]);
        ASSERT_FALSE(v->hasPushSubscription());

        // Unsubscribing from this data store should not affect subscriptions in other data stores.
        ASSERT_EQ(subscribedTopicsCount(), webViews().size() - i);
        i++;
    }
}
#endif

TEST_F(WebPushDTest, UnsubscribesOnServiceWorkerUnregisterTest)
{
    for (auto& v : webViews())
        v->subscribe();
    ASSERT_EQ(subscribedTopicsCount(), webViews().size());

    int i = 1;
    for (auto& v : webViews()) {
        ASSERT_TRUE(v->hasPushSubscription());
        id result = v->unregisterServiceWorker();
        ASSERT_TRUE([result isEqual:@YES]);
        ASSERT_FALSE(v->hasPushSubscription());

        // Unsubscribing from this data store should not affect subscriptions in other data stores.
        ASSERT_EQ(subscribedTopicsCount(), webViews().size() - i);
        i++;
    }
}

TEST_F(WebPushDTest, UnsubscribesOnClearingAllWebsiteData)
{
    for (auto& v : webViews())
        v->subscribe();
    ASSERT_EQ(subscribedTopicsCount(), webViews().size());

    int i = 1;
    for (auto& v : webViews()) {
        ASSERT_TRUE(v->hasPushSubscription());

        __block bool removedData = false;
        [v->dataStore() removeDataOfTypes:[NSSet setWithObject:WKWebsiteDataTypeServiceWorkerRegistrations] modifiedSince:[NSDate distantPast] completionHandler:^(void) {
            removedData = true;
        }];
        TestWebKitAPI::Util::run(&removedData);

        ASSERT_FALSE(v->hasPushSubscription());

        // Unsubscribing from this data store should not affect subscriptions in other data stores.
        ASSERT_EQ(subscribedTopicsCount(), webViews().size() - i);
        i++;
    }
}

TEST_F(WebPushDTest, UnsubscribesOnClearingWebsiteDataForOrigin)
{
    for (auto& v : webViews())
        v->subscribe();
    ASSERT_EQ(subscribedTopicsCount(), webViews().size());

    int i = 1;
    for (auto& v : webViews()) {
        // First unsubscribe should succeed.
        ASSERT_TRUE(v->hasPushSubscription());

        __block bool fetchedRecords = false;
        __block RetainPtr<NSArray<WKWebsiteDataRecord *>> records;
        [v->dataStore() fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeServiceWorkerRegistrations] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
            records = dataRecords;
            fetchedRecords = true;
        }];
        TestWebKitAPI::Util::run(&fetchedRecords);

        WKWebsiteDataRecord *filteredRecord = nil;
        for (WKWebsiteDataRecord *record in records.get()) {
            for (NSString *originString in record._originsStrings) {
                if ([originString isEqualToString:v->origin()]) {
                    filteredRecord = record;
                    break;
                }
            }
        }
        ASSERT_TRUE(filteredRecord);

        __block bool removedData = false;
        [v->dataStore() removeDataOfTypes:[NSSet setWithObject:WKWebsiteDataTypeServiceWorkerRegistrations] forDataRecords:[NSArray arrayWithObject:filteredRecord] completionHandler:^(void) {
            removedData = true;
        }];
        TestWebKitAPI::Util::run(&removedData);

        ASSERT_FALSE(v->hasPushSubscription());

        // Unsubscribing from this data store should not affect subscriptions in other data stores.
        ASSERT_EQ(subscribedTopicsCount(), webViews().size() - i);
        i++;
    }
}

TEST_F(WebPushDTest, UnsubscribesOnPermissionReset)
{
    // FIXME: test on all webviews once we finish refactoring the shared service worker notification
    // managers to be datastore-aware.
    auto& v = webViews().last();
    v->subscribe();
    ASSERT_TRUE(v->hasPushSubscription());

    v->resetPermission();

    bool isSubscribed = true;
    TestWebKitAPI::Util::waitForConditionWithLogging([&v, &isSubscribed]() mutable {
        isSubscribed = v->hasPushSubscription();
        if (!isSubscribed)
            return true;

        sleep(1);
        return false;
    }, 5, @"Timed out waiting for push subscription to be removed.");

    ASSERT_FALSE(isSubscribed);
}

TEST_F(WebPushDTest, IgnoresSubscriptionOnPermissionDenied)
{
    // FIXME: test on all webviews once we finish refactoring the shared service worker notification
    // managers to be datastore-aware.
    auto& v = webViews().last();
    v->subscribe();
    ASSERT_TRUE(v->hasPushSubscription());

    // Topic should be moved to ignored list after denying permission, but the subscription should still exist.
    v->setPermission(false);

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
    ASSERT_TRUE(v->hasPushSubscription());

    // Topic should be moved back to enabled list after allowing permission, and the subscription should still exist.
    v->setPermission(true);

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
    ASSERT_TRUE(v->hasPushSubscription());
}

TEST_F(WebPushDTest, TooManySilentPushesCausesUnsubscribe)
{
    for (auto& v : webViews()) {
        v->subscribe();
        v->disableShowNotifications();
    }
    ASSERT_EQ(subscribedTopicsCount(), webViews().size());

    int i = 1;
    for (auto& v : webViews()) {
        ASSERT_TRUE(v->hasPushSubscription());

        for (unsigned i = 0; i < WebKit::WebPushD::maxSilentPushCount; i++) {
            v->injectPushMessage(@{ });
            auto messages = v->fetchPushMessages();
            ASSERT_EQ([messages count], 1u);

            // WebContent should fail processing the push since no notification was shown due to the
            // disableShowNotifications call above.
            ASSERT_FALSE(v->expectDecryptedMessage(@"null data", [messages firstObject]));
        }

        ASSERT_FALSE(v->hasPushSubscription());

        // Unsubscribing from this data store should not affect subscriptions in other data stores.
        ASSERT_EQ(subscribedTopicsCount(), webViews().size() - i);
        i++;
    }
}

TEST_F(WebPushDTest, GetPushSubscriptionWithMismatchedPublicToken)
{
    for (auto& v : webViews())
        v->subscribe();
    ASSERT_EQ(subscribedTopicsCount(), webViews().size());

    // If the public token changes, all subscriptions should be invalidated.
    auto utilityConnection = createAndConfigureConnectionToService("org.webkit.webpushtestdaemon.service");
    auto sender = WebPushXPCConnectionMessageSender { utilityConnection.get() };
    bool done = false;
    sender.sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::SetPublicTokenForTesting("foobar"_s), [&]() {
        done = true;
    });
    TestWebKitAPI::Util::run(&done);

    for (auto& v : webViews())
        ASSERT_FALSE(v->hasPushSubscription());

    ASSERT_EQ(subscribedTopicsCount(), 0u);
}

TEST_F(WebPushDMultipleLaunchTest, GetPushSubscriptionAfterDaemonRelaunch)
{
    for (auto& v : webViews())
        v->subscribe();
    ASSERT_EQ(subscribedTopicsCount(), webViews().size());

    ASSERT_TRUE(restartTestWebPushD());

    // Make sure that getSubscription works after killing webpushd. Previously, this didn't work and
    // would fail with an AbortError because we didn't re-send the connection configuration after
    // the daemon relaunched.
    //
    // Note that getSubscription() will return null now since we launch webpushd in in-memory mode
    // when running tests. We're just making sure that this doesn't fail with an AbortError.
    for (auto& v : webViews()) {
        id result = v->getPushSubscription();
        ASSERT_TRUE([result isEqual:[NSNull null]]);
    }

    ASSERT_EQ(subscribedTopicsCount(), 0u);
}

class WebPushDInjectedPushTest : public WebPushDTest {
public:
    void runTest(NSString *expectedMessage, NSDictionary *apsUserInfo)
    {
        for (auto& v : webViews()) {
            v->subscribe();
            v->injectPushMessage(apsUserInfo);
        }

        // Make sure each data store has exactly one push message to process.
        Vector<RetainPtr<NSDictionary>> rawMessages;
        for (auto& v : webViews()) {
            auto messages = v->fetchPushMessages();
            ASSERT_EQ([messages count], 1u);
            rawMessages.append([messages firstObject]);
        }

        int i = 0;
        for (auto& v : webViews()) {
            v->expectDecryptedMessage(expectedMessage, rawMessages[i++].get());
            v->expectDataStoreIdentifierSetOnLastNotification();
        }
    }
};

TEST_F(WebPushDInjectedPushTest, HandleInjectedEmptyPush)
{
    runTest(@"null data", @{ });
}

TEST_F(WebPushDInjectedPushTest, HandleInjectedAESGCMPush)
{
    runTest(@"test aesgcm payload", @{
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

    runTest(@"When I grow up, I want to be a watermelon", @{
        @"content_encoding": @"aes128gcm",
        @"payload": (NSString *)payloadBase64
    });
}

TEST_F(WebPushDTest, PushSubscriptionExtendsITPCleanupTimerBy30Days)
{
    // FIXME: test on all webviews once we finish refactoring the shared service worker notification
    // managers to be datastore-aware.
    auto& v = webViews().last();
    v->subscribe();

    EXPECT_TRUE(v->hasPushSubscription());

    v->assertPushEventSucceeds(0);
    v->assertPushEventSucceeds(29);
    EXPECT_TRUE(v->hasPushSubscription());

    v->assertPushEventFails(31);
    v->assertPushEventFails(100);
    EXPECT_FALSE(v->hasPushSubscription());
}

TEST_F(WebPushDTest, NotificationClickExtendsITPCleanupTimerBy30Days)
{
    // FIXME: test on all webviews once we finish refactoring the shared service worker notification
    // managers to be datastore-aware.
    auto& v = webViews().last();
    v->subscribe();

    EXPECT_TRUE(v->hasPushSubscription());

    v->assertPushEventSucceeds(0);
    v->assertPushEventSucceeds(29);
    v->simulateNotificationClick();
    EXPECT_TRUE(v->hasPushSubscription());

    v->assertPushEventSucceeds(58);
    EXPECT_TRUE(v->hasPushSubscription());

    v->assertPushEventFails(61);
    EXPECT_FALSE(v->hasPushSubscription());
}

#if ENABLE(DECLARATIVE_WEB_PUSH)

static constexpr ASCIILiteral json0 = ""_s;
static constexpr ASCIILiteral json1 = "not really a string"_s;
static constexpr ASCIILiteral json2 = "\"a string\""_s;
static constexpr ASCIILiteral json3 = "4"_s;
static constexpr ASCIILiteral json4 = "{ }"_s;
static constexpr ASCIILiteral json5 = R"JSONRESOURCE(
{
    "default_action_url": "foo"
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json6 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/"
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json7 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": ""
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json8 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": 4
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json9 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "app_badge": ""
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json10 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "app_badge": -1
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json11 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "app_badge": { }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json12 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "app_badge": 10
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json13 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "options": 0
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json14 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "options": { }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json15 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "dir": 0
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json16 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "dir": "auto"
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json17 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "dir": "ltr"
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json18 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "dir": "rtl"
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json19 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "dir": "nonsense"
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json20 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "lang": { }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json21 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "lang": "language"
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json22 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "body": { }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json23 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "body": "world"
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json24 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "tag": { }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json25 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "tag": "world"
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json26 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "icon": 0
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json27 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "icon": "world"
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json28 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "icon": "https://example.com/icon.png"
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json29 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "silent": 0
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json30 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "silent": true
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json31 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "silent": false
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json32 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "app_badge": "20"
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json33 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "app_badge": "18446744073709551615"
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json34 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "app_badge": "18446744073709551616"
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json35 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "mutable": 39
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json36 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "mutable": { }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json37 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "mutable": "true"
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json38 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "mutable": true
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json39 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "mutable": true,
    "app_badge": "12"
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json40 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "mutable": true,
    "tag": "title Gotcha!",
    "app_badge": "12"
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json41 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "mutable": true,
    "tag": "badge 1024",
    "app_badge": "12"
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json42 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Hello world!",
    "mutable": true,
    "tag": "titleandbadge ThisRules 4096",
    "app_badge": "12"
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json43 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Test the data object",
    "mutable": true,
    "tag": "datatotitle",
    "data": "Raw string",
    "app_badge": "12"
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json44 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Test the data object",
    "mutable": true,
    "tag": "datatotitle",
    "data": { "key": "value" },
    "app_badge": "12"
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json45 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Test a default action URL override",
    "mutable": true,
    "tag": "defaultactionurl https://webkit.org/",
    "app_badge": "12"
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json46 = R"JSONRESOURCE(
{
    "default_action_url": "https://example.com/",
    "title": "Test a missing default action URL override",
    "mutable": true,
    "tag": "emptydefaultactionurl",
    "app_badge": "12"
}
)JSONRESOURCE"_s;

static constexpr ASCIILiteral errors[] = {
    "does not contain valid JSON"_s,
    "top level JSON value is not an object"_s,
    "'default_action_url' member is specified but does not represent a valid URL"_s,
    "'title' member is missing or is an empty string"_s,
    "'title' member is specified but is not a string"_s,
    "'app_badge' member is specified as a string that did not parse to to an unsigned long long"_s,
    "'app_badge' member is specified as an number but is not a valid unsigned long long"_s,
    "<intentionally left blank>"_s,
    "'app_badge' member is specified but is not a string or a number"_s,
    "'dir' member is specified but is not a valid NotificationDirection"_s,
    "'dir' member is specified but is not a string"_s,
    "'lang' member is specified but is not a string"_s,
    "'body' member is specified but is not a string"_s,
    "'tag' member is specified but is not a string"_s,
    "'icon' member is specified but is not a string"_s,
    "'icon' member is specified but does not represent a valid URL"_s,
    "'silent' member is specified but is not a boolean"_s,
    "'app_badge' member is specified as a string that did not parse to a valid unsigned long long"_s,
    "'mutable' member is specified but is not a boolean"_s
};

static std::pair<ASCIILiteral, ASCIILiteral> jsonAndErrors[] = {
    { json0, errors[0] },
    { json1, errors[0] },
    { json2, errors[1] },
    { json3, errors[1] },
    { json4, errors[2] },
    { json5, errors[2] },
    { json6, errors[3] },
    { json7, errors[3] },
    { json8, errors[4] },
    { json9, { " "_s } },
    { json10, errors[6] },
    { json11, errors[8] },
    { json12, { " "_s } },
    { json13, { " "_s } },
    { json14, { " "_s } },
    { json15, errors[10] },
    { json16, { " "_s } },
    { json17, { " "_s } },
    { json18, { " "_s } },
    { json19, errors[9] },
    { json20, errors[11] },
    { json21, { " "_s } },
    { json22, errors[12] },
    { json23, { " "_s } },
    { json24, errors[13] },
    { json25, { " "_s } },
    { json26, errors[14] },
    { json27, errors[15] },
    { json28, { " "_s } },
    { json29, errors[16] },
    { json30, { " "_s } },
    { json31, { " "_s } },
    { json32, { " "_s } },
    { json33, { " "_s } },
    { json34, errors[17] },
    { json35, errors[18] },
    { json36, errors[18] },
    { json37, errors[18] },
    { json38, { " "_s } },
    { json39, { " "_s } },
    { json40, { " "_s } },
    { json41, { " "_s } },
    { json42, { " "_s } },
    { json43, { " "_s } },
    { json44, { " "_s } },
    { json45, { " "_s } },
    { json46, { " "_s } },
    { { }, { } }
};

static size_t expectedSuccessfulMessages()
{
    size_t result = 0;
    for (size_t i = 0; !jsonAndErrors[i].first.isNull(); ++i) {
        if (!strcmp(jsonAndErrors[i].second, " "))
            ++result;
    }

    return result;
}

// Directly message the daemon to do JSON parsing validation on the declarative message
TEST(WebPushD, DeclarativeParsing)
{
    setUpTestWebPushD();

    auto utilityConnection = createAndConfigureConnectionToService("org.webkit.webpushtestdaemon.service");

    auto dataStoreConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    dataStoreConfiguration.get().webPushMachServiceName = @"org.webkit.webpushtestdaemon.service";
    dataStoreConfiguration.get().webPushDaemonUsesMockBundlesForTesting = YES;
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);
    clearWebsiteDataStore(dataStore.get());

    auto sender = WebPushXPCConnectionMessageSender { utilityConnection.get() };
    static bool done = false;

    WebKit::WebPushD::PushMessageForTesting message;
    message.targetAppCodeSigningIdentifier = "com.apple.WebKit.TestWebKitAPI"_s;
    message.registrationURL = URL("https://example.com"_s);
    message.disposition = WebKit::WebPushD::PushMessageDisposition::Notification;

    unsigned i = 0;
    while (!jsonAndErrors[i].first.isNull()) {
        message.payload = jsonAndErrors[i].first;
        done = false;
        sender.sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::InjectPushMessageForTesting(message), [&](const String& error) {
            if (!error.isEmpty())
                EXPECT_TRUE(error.endsWith(jsonAndErrors[i].second));
            else
                EXPECT_FALSE(strcmp(jsonAndErrors[i].second, " "));

            done = true;
        });
        TestWebKitAPI::Util::run(&done);
        ++i;
    }

    // Now retrieve the successfully parsed messages like a client would,
    // but validate they make sense like you only can in internals.
    done = false;
    [dataStore _getPendingPushMessages:^(NSArray<NSDictionary *> *messages) {
        EXPECT_EQ(messages.count, expectedSuccessfulMessages());

        for (NSDictionary *message in messages) {
            auto webPushMessage = WebKit::WebPushMessage::fromDictionary(message);
            EXPECT_TRUE(webPushMessage.has_value());
            EXPECT_TRUE(!!webPushMessage->notificationPayload);
        }

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

// Verifies that handling a declarative web push message - with no service worker even registered - calls
// back into the client for showing the notification, etc.
TEST(WebPushD, DeclarativeWebPushHandling)
{
    setUpTestWebPushD();

    auto utilityConnection = createAndConfigureConnectionToService("org.webkit.webpushtestdaemon.service");

    auto dataStoreConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    dataStoreConfiguration.get().webPushMachServiceName = @"org.webkit.webpushtestdaemon.service";
    dataStoreConfiguration.get().webPushDaemonUsesMockBundlesForTesting = YES;
    dataStoreConfiguration.get().isDeclarativeWebPushEnabled = YES;
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);
    clearWebsiteDataStore(dataStore.get());

    auto delegate = adoptNS([[PushNotificationDelegate alloc] init]);
    dataStore.get()._delegate = delegate.get();

    auto sender = WebPushXPCConnectionMessageSender { utilityConnection.get() };
    static bool done = false;

    WebKit::WebPushD::PushMessageForTesting message;
    message.targetAppCodeSigningIdentifier = "com.apple.WebKit.TestWebKitAPI"_s;
    message.registrationURL = URL("https://example.com"_s);
    message.disposition = WebKit::WebPushD::PushMessageDisposition::Notification;
    message.payload = json33;
    sender.sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::InjectPushMessageForTesting(message), [&](const String& error) {
        EXPECT_TRUE(error.isEmpty());
        done = true;
    });
    TestWebKitAPI::Util::run(&done);

    done = false;
    [dataStore _getPendingPushMessages:^(NSArray<NSDictionary *> *messages) {
        EXPECT_EQ(messages.count, 1u);

        [dataStore _processPushMessage:messages.firstObject completionHandler:^(bool handled) {
            EXPECT_TRUE(handled);
            EXPECT_TRUE([delegate.get().mostRecentNotification.get().userInfo[@"WebNotificationDefaultActionURLKey"] isEqualToString:@"https://example.com/"]);
            EXPECT_EQ(delegate.get().mostRecentAppBadge, 18446744073709551615ULL);
            done = true;
        }];
    }];
    TestWebKitAPI::Util::run(&done);


    // Verify that processing the most recent notification results in its action URL being sent to the data store delegate
    done = false;
    [dataStore _processPersistentNotificationClick:delegate.get().mostRecentNotification.get().userInfo completionHandler:^(bool handled) {
        EXPECT_TRUE(handled);
        EXPECT_TRUE([delegate.get().mostRecentActionURL.get().absoluteString isEqualToString:@"https://example.com/"]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

class WebPushDPushNotificationEventTest : public WebPushDTest {
public:
    WebPushDPushNotificationEventTest()
        : WebPushDTest(LaunchOnlyOnce::Yes, htmlSource, InstallDataStoreDelegate::Yes)
    {
    }

    void prep()
    {
        webViews().first()->subscribe();
    }

    void runTest(ASCIILiteral jsonMessage)
    {
        webViews().first()->clearMostRecents();
        webViews().first()->injectDeclarativePushMessage(jsonMessage);

        auto messages = webViews().first()->fetchPushMessages();
        ASSERT_EQ([messages count], 1u);

        webViews().first()->captureAllMessages();
        webViews().first()->processPushMessage([messages firstObject]);
    }

    void waitForMessageAndVerify(NSString *message)
    {
        while (webViews().first()->mostRecentMessage().isEmpty())
            TestWebKitAPI::Util::runFor(0.05_s);

        EXPECT_TRUE([(NSString *)webViews().first()->mostRecentMessage() isEqualToString:message]);
    }

    void checkLastNotificationTitle(NSString *title)
    {
        NSString *recentTitle = webViews().first()->mostRecentNotification().userInfo[@"WebNotificationTitleKey"];
        EXPECT_TRUE([recentTitle isEqualToString:title]);

        if (![recentTitle isEqualToString:title])
            NSLog(@"Most recent title: %@\nExpected title: %@", recentTitle, title);

    }

    void checkLastNotificationDefaultActionURL(NSString *actionURL)
    {
        NSString *notificationActionURL = webViews().first()->mostRecentNotification().userInfo[@"WebNotificationDefaultActionURLKey"];
        EXPECT_TRUE([notificationActionURL isEqualToString:actionURL]);
    }

    void checkLastActionURL(NSString *url)
    {
        NSURL *recentActionURL = webViews().first()->mostRecentActionURL();
        EXPECT_TRUE([url isEqualToString:recentActionURL.absoluteString]);

        if (![url isEqualToString:recentActionURL.absoluteString])
            NSLog(@"Lact action URL: %@\nExpected URL: %@", recentActionURL, url);

    }

    void checkLastAppBadge(std::optional<uint64_t> badge)
    {
        EXPECT_EQ(badge, webViews().first()->mostRecentAppBadge());
    }
};

TEST_F(WebPushDPushNotificationEventTest, Basic)
{
    prep();
    runTest(json39);
    checkLastNotificationTitle(@"Hello world!");
    checkLastAppBadge(12);

    runTest(json40);
    checkLastNotificationTitle(@"Gotcha!");
    checkLastAppBadge(12);

    runTest(json41);
    checkLastNotificationTitle(@"Hello world!");
    checkLastAppBadge(1024);

    runTest(json42);
    checkLastNotificationTitle(@"ThisRules");
    checkLastAppBadge(4096);

    runTest(json43);
    checkLastNotificationTitle(@"Raw string");
    checkLastAppBadge(12);

    runTest(json44);
    checkLastNotificationTitle(@"[object Object]");
    checkLastAppBadge(12);

    runTest(json45);
    checkLastNotificationTitle(@"Test a default action URL override");
    checkLastNotificationDefaultActionURL(@"https://webkit.org/");
    checkLastAppBadge(12);

    runTest(json46);
    checkLastNotificationTitle(@"Test a missing default action URL override");
    checkLastNotificationDefaultActionURL(@"https://example.com/");
    waitForMessageAndVerify(@"showNotification failed: TypeError: Call to showNotification() while handling a `pushnotification` event did not include NotificationOptions that specify a valid defaultAction url");
}

#endif // ENABLE(DECLARATIVE_WEB_PUSH)

} // namespace TestWebKitAPI

#endif // ENABLE(NOTIFICATIONS) && ENABLE(NOTIFICATION_EVENT) && (PLATFORM(MAC)
