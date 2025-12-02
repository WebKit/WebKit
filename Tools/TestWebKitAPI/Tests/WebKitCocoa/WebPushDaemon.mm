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
#import <WebCore/SecurityOriginData.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebsiteDataRecordPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebPushDaemonConstants.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKNotificationData.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKWebPushDaemonConnection.h>
#import <WebKit/_WKWebPushSubscriptionData.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <WebKit/_WKWebsiteDataStoreDelegate.h>
#import <mach/mach_init.h>
#import <mach/task.h>
#import <ranges>
#import <wtf/BlockPtr.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/UUID.h>
#import <wtf/UniqueRef.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/darwin/XPCExtras.h>
#import <wtf/darwin/XPCObjectPtr.h>
#import <wtf/text/Base64.h>
#import <wtf/text/MakeString.h>

// FIXME: Work through enabling on iOS
#if ENABLE(NOTIFICATIONS) && ENABLE(NOTIFICATION_EVENT) && PLATFORM(MAC)

static bool alertReceived = false;
@interface PushNotificationDelegate : NSObject<WKUIDelegatePrivate, _WKWebsiteDataStoreDelegate> {
    RetainPtr<_WKNotificationData> _mostRecentNotification;
    RetainPtr<NSURL> _mostRecentActionURL;
    std::optional<uint64_t> _mostRecentAppBadge;
}
-(void)clearMostRecents;

@property BOOL expectsDelegateNotificationCallbacks;
@property (nonatomic, readonly) RetainPtr<_WKNotificationData> mostRecentNotification;
@property (nonatomic, readonly) RetainPtr<NSURL> mostRecentActionURL;
@property (nonatomic, readonly) std::optional<uint64_t> mostRecentAppBadge;
@end

@implementation PushNotificationDelegate

-(id)init {
    if (self = [super init])
        self.expectsDelegateNotificationCallbacks = YES;
    return self;
}

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
    RELEASE_ASSERT(_expectsDelegateNotificationCallbacks);

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

#if ASSERT_ENABLED
namespace WTF {
template<> bool isValidEnum<WebKit::WebPushD::PushMessageDisposition>(std::underlying_type_t<WebKit::WebPushD::PushMessageDisposition>) { return true; }
template<> bool isValidEnum<WebCore::NotificationDirection>(std::underlying_type_t<WebCore::NotificationDirection>) { return true; }
}
#endif

namespace TestWebKitAPI {

static bool done;

static RetainPtr<NSURL> testWebPushDaemonLocation()
{
    return [currentExecutableDirectory() URLByAppendingPathComponent:@"webpushd" isDirectory:NO];
}

enum LaunchOnlyOnce : BOOL { No, Yes };
enum class InstallDataStoreDelegate : bool { No, Yes };
enum class BuiltInNotificationsEnabled : bool { No, Yes };

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

template<typename T, typename = void> struct TestArgumentCoder;
template<typename T> struct TestArgumentCoder<T, typename std::enable_if_t<std::is_enum_v<T>>> : public IPC::ArgumentCoder<T> { };
template<typename T> struct TestArgumentCoder<Vector<T>> : public IPC::ArgumentCoder<Vector<T>> { };
template<typename T> struct TestArgumentCoder<std::span<T>> : public IPC::ArgumentCoder<std::span<T>> { };
template<typename T> struct TestArgumentCoder<std::optional<T>> : public IPC::ArgumentCoder<std::optional<T>> { };
template<typename... Elements> struct TestArgumentCoder<std::tuple<Elements...>> : public IPC::ArgumentCoder<std::tuple<Elements...>> { };

template<typename T> struct TestArgumentCoder<T, typename std::enable_if_t<std::is_arithmetic_v<T>>> {
    template<typename Encoder> static void encode(Encoder& encoder, T value) { encoder.encodeInteger(value); }
    template<typename Decoder> static std::optional<T> decode(Decoder& decoder) { return decoder.template decodeInteger<T>(); }
};

template<> struct TestArgumentCoder<String> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const String& string)
    {
        if (string.isNull()) {
            encoder << std::numeric_limits<uint32_t>::max();
            return;
        }
        bool is8Bit = string.is8Bit();
        encoder << string.length();
        encoder << is8Bit;

        if (is8Bit)
            encoder.encodeSpan(string.span8());
        else
            encoder.encodeSpan(string.span16());
    }

    template<typename CharacterType, typename Decoder>
    static std::optional<String> decodeStringText(Decoder& decoder, uint32_t length)
    {
        Vector<CharacterType> characters;
        for (size_t i = 0; i < length; i++) {
            auto character = decoder.template decodeInteger<CharacterType>();
            if (!character)
                return std::nullopt;
            characters.append(*character);
        }
        return String(characters.span());
    }

    template<typename Decoder>
    static std::optional<String> decode(Decoder& decoder)
    {
        auto length = decoder.template decode<uint32_t>();
        if (!length)
            return std::nullopt;
        if (*length == std::numeric_limits<uint32_t>::max())
            return String();
        auto is8Bit = decoder.template decode<bool>();
        if (!is8Bit)
            return std::nullopt;
        if (*is8Bit)
            return decodeStringText<Latin1Character>(decoder, *length);
        return decodeStringText<char16_t>(decoder, *length);
    }
};

template<> struct TestArgumentCoder<WTF::UUID> {
    template<typename Encoder> static void encode(Encoder& encoder, const WTF::UUID& uuid)
    {
        encoder << uuid.high();
        encoder << uuid.low();
    }
};

template<> struct TestArgumentCoder<URL> {
    template<typename Encoder> static void encode(Encoder& encoder, const URL& url) { encoder << url.string(); }
    template<typename Decoder> static std::optional<URL> decode(Decoder& decoder)
    {
        auto string = decoder.template decode<String>();
        if (!string)
            return std::nullopt;
        return { URL(WTFMove(*string)) };
    }
};

class TestEncoder {
public:
    template<typename T> void encodeHeader()
    {
        *this << uint8_t();
        *this << T::name();
        *this << uint64_t();
    }
    Vector<uint8_t> takeBytes() { return std::exchange(m_bytes, { }); }
    template<typename T> TestEncoder& operator<<(T&& t)
    {
        TestArgumentCoder<std::remove_cvref_t<T>>::encode(*this, std::forward<T>(t));
        return *this;
    }
    template<typename T, size_t Extent> void encodeSpan(std::span<T, Extent> span)
    {
        while (m_bytes.size() % alignof(T))
            m_bytes.append(0);
        for (auto byte : asBytes(span))
            m_bytes.append(byte);
    }
    template<typename T> void encodeInteger(T object)
    {
        encodeSpan(singleElementSpan(object));
    }
private:
    Vector<uint8_t> m_bytes;
};

class TestDecoder {
public:
    TestDecoder(std::span<const uint8_t> buffer)
        : m_buffer(buffer)
        , m_bufferPosition(m_buffer.begin()) { }

    template<typename T> void ignoreHeader()
    {
        decode<uint8_t>();
        RELEASE_ASSERT(decode<IPC::MessageName>() == T::asyncMessageReplyName());
        decode<uint64_t>();
    }
    template<typename T> std::optional<T> decode() { return TestArgumentCoder<std::remove_cvref_t<T>>::decode(*this); }
    template<typename T> std::optional<T> decodeInteger()
    {
        while (m_bufferPosition != m_buffer.end() && bufferOffset() % alignof(T))
            m_bufferPosition++;
        if (bufferOffset() + sizeof(T) > m_buffer.size())
            return std::nullopt;
        T value = *reinterpret_cast<const T*>(m_buffer.data() + bufferOffset());
        m_bufferPosition += sizeof(T);
        return value;
    }
private:
    size_t bufferOffset() const { return std::distance(m_buffer.begin(), m_bufferPosition); }
    std::span<const uint8_t> m_buffer;
    std::span<const uint8_t>::iterator m_bufferPosition;
};

template<> struct TestArgumentCoder<WebKit::WebPushD::WebPushDaemonConnectionConfiguration> {
    static void encode(TestEncoder& encoder, const WebKit::WebPushD::WebPushDaemonConnectionConfiguration& configuration)
    {
        encoder << configuration.hostAppAuditTokenData;
        encoder << configuration.bundleIdentifierOverride;
        encoder << configuration.pushPartitionString;
        encoder << configuration.dataStoreIdentifier;
        encoder << configuration.declarativeWebPushEnabled;
    }
};

template<> struct TestArgumentCoder<WebKit::WebPushD::PushMessageForTesting> {
    static void encode(TestEncoder& encoder, const WebKit::WebPushD::PushMessageForTesting& message)
    {
        encoder << message.targetAppCodeSigningIdentifier;
        encoder << message.pushPartitionString;
        encoder << message.registrationURL;
        encoder << message.payload;
        encoder << message.disposition;
#if ENABLE(DECLARATIVE_WEB_PUSH)
        encoder << message.parsedPayload;
#endif
    }
};

template<> struct TestArgumentCoder<WebCore::NotificationPayload> {
    static void encode(TestEncoder& encoder, const WebCore::NotificationPayload& payload)
    {
        encoder << payload.defaultActionURL;
        encoder << payload.title;
        encoder << payload.appBadge;
        encoder << payload.options;
        encoder << payload.isMutable;
    }
};

template<> struct TestArgumentCoder<WebCore::NotificationOptionsPayload> {
    static void encode(TestEncoder& encoder, const WebCore::NotificationOptionsPayload& payload)
    {
        encoder << payload.dir;
        encoder << payload.lang;
        encoder << payload.body;
        encoder << payload.tag;
        encoder << payload.icon;
        encoder << payload.dataJSONString;
        encoder << payload.silent;
    }
};

class WebPushXPCConnectionMessageSender {
public:
    WebPushXPCConnectionMessageSender(xpc_connection_t connection)
        : m_connection(connection) { }

    void setShouldIncrementProtocolVersionForTesting() { m_shouldIncrementProtocolVersionForTesting = true; }

    template<typename M>
    void sendWithoutUsingIPCConnection(M&&) const;
    template<typename M, typename CH>
    void sendWithAsyncReplyWithoutUsingIPCConnection(M&&, CH&&) const;

private:
    XPCObjectPtr<xpc_object_t> messageDictionaryFromEncoder(TestEncoder&&) const;

    XPCObjectPtr<xpc_connection_t> m_connection;
    bool m_shouldIncrementProtocolVersionForTesting { false };
};

XPCObjectPtr<xpc_object_t> WebPushXPCConnectionMessageSender::messageDictionaryFromEncoder(TestEncoder&& encoder) const
{
    // FIXME: This is a false positive. <rdar://164843889>
    SUPPRESS_RETAINPTR_CTOR_ADOPT auto dictionary = adoptXPCObject(xpc_dictionary_create(nullptr, nullptr, 0));

    uint64_t protocolVersion = WebKit::WebPushD::protocolVersionValue;
    if (m_shouldIncrementProtocolVersionForTesting)
        ++protocolVersion;
    xpc_dictionary_set_uint64(dictionary.get(), WebKit::WebPushD::protocolVersionKey, protocolVersion);

    __block auto blockBytes = encoder.takeBytes();
    auto buffer = blockBytes.span();
    auto dispatchData = adoptOSObject(dispatch_data_create(buffer.data(), buffer.size(), mainDispatchQueueSingleton(), ^{
        blockBytes.clear();
    }));
    // FIXME: This is a false positive. <rdar://164843889>
    SUPPRESS_RETAINPTR_CTOR_ADOPT auto encoderData = adoptXPCObject(xpc_data_create_with_dispatch_data(dispatchData.get()));

    xpc_dictionary_set_value(dictionary.get(), WebKit::WebPushD::protocolEncodedMessageKey, encoderData.get());

    return dictionary;
}

template<typename M>
void WebPushXPCConnectionMessageSender::sendWithoutUsingIPCConnection(M&& message) const
{
    TestEncoder encoder;
    encoder.encodeHeader<M>();
    message.encode(encoder);
    auto dictionary = messageDictionaryFromEncoder(WTFMove(encoder));
    xpc_connection_send_message(m_connection.get(), dictionary.get());
}

template<typename M, typename CH>
void WebPushXPCConnectionMessageSender::sendWithAsyncReplyWithoutUsingIPCConnection(M&& message, CH&& completionHandler) const
{
    TestEncoder encoder;
    encoder.encodeHeader<M>();
    message.encode(encoder);
    auto dictionary = messageDictionaryFromEncoder(WTFMove(encoder));
    xpc_connection_send_message_with_reply(m_connection.get(), dictionary.get(), mainDispatchQueueSingleton(), makeBlockPtr([this, completionHandler = WTFMove(completionHandler)] (xpc_object_t reply) mutable {
        if (xpc_get_type(reply) == XPC_TYPE_ERROR) {
            // We only expect an error if we were purposefully testing the wrong protocol version.
            RELEASE_ASSERT(m_shouldIncrementProtocolVersionForTesting);
            return IPC::cancelReplyWithoutUsingConnection<M>(WTFMove(completionHandler));
        }

        if (xpc_get_type(reply) != XPC_TYPE_DICTIONARY)
            RELEASE_ASSERT_NOT_REACHED();
        if (xpc_dictionary_get_uint64(reply, WebKit::WebPushD::protocolVersionKey) != WebKit::WebPushD::protocolVersionValue)
            RELEASE_ASSERT_NOT_REACHED();

        auto data = xpcDictionaryGetData(reply, WebKit::WebPushD::protocolEncodedMessageKey);
        TestDecoder decoder(data);
        decoder.ignoreHeader<M>();
        IPC::callReplyWithoutUsingConnection<M>(decoder, WTFMove(completionHandler));
    }).get());
}

static audit_token_t getSelfAuditToken()
{
    audit_token_t auditToken { };
    mach_msg_type_number_t auditTokenCount = TASK_AUDIT_TOKEN_COUNT;
    task_info(mach_task_self(), TASK_AUDIT_TOKEN, (task_info_t)(&auditToken), &auditTokenCount);
    return auditToken;
}

static WebKit::WebPushD::WebPushDaemonConnectionConfiguration defaultWebPushDaemonConfiguration()
{
    auto token = getSelfAuditToken();
    Vector<uint8_t> auditToken(sizeof(token));
    memcpySpan(auditToken.mutableSpan(), asByteSpan(token));

    IGNORE_CLANG_WARNINGS_BEGIN("missing-designated-field-initializers")
    return { .hostAppAuditTokenData = WTFMove(auditToken) };
    IGNORE_CLANG_WARNINGS_END
}

XPCObjectPtr<xpc_connection_t> createAndConfigureConnectionToService(const char* serviceName, std::optional<WebKit::WebPushD::WebPushDaemonConnectionConfiguration> configuration = std::nullopt)
{
    // FIXME: This is a false positive. <rdar://164843889>
    SUPPRESS_RETAINPTR_CTOR_ADOPT auto connection = adoptXPCObject(xpc_connection_create_mach_service(serviceName, mainDispatchQueueSingleton(), 0));
    xpc_connection_set_event_handler(connection.get(), ^(xpc_object_t) { });
    xpc_connection_activate(connection.get());
    auto sender = WebPushXPCConnectionMessageSender { connection.get() };

    if (!configuration)
        configuration = defaultWebPushDaemonConfiguration();
    sender.sendWithoutUsingIPCConnection(Messages::PushClientConnection::InitializeConnection(configuration.value()));

    return connection;
}

TEST(WebPushD, BasicCommunication)
{
    NSURL *tempDir = setUpTestWebPushD();

    // FIXME: This is a false positive. <rdar://164843889>
    SUPPRESS_RETAINPTR_CTOR_ADOPT auto connection = adoptXPCObject(xpc_connection_create_mach_service("org.webkit.webpushtestdaemon.service", mainDispatchQueueSingleton(), 0));

    __block bool done = false;
    __block bool interrupted = false;
    xpc_connection_set_event_handler(connection.get(), ^(xpc_object_t request) {
        if (request == XPC_ERROR_CONNECTION_INTERRUPTED) {
            interrupted = true;
            return;
        }
    });
    xpc_connection_activate(connection.get());

    // Send a basic message and make sure its reply handler ran.
    auto sender = WebPushXPCConnectionMessageSender { connection.get() };
    sender.sendWithoutUsingIPCConnection(Messages::PushClientConnection::InitializeConnection(defaultWebPushDaemonConfiguration()));
    sender.sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::GetPushTopicsForTesting(), ^(Vector<String>, Vector<String>) {
        done = true;
    });
    TestWebKitAPI::Util::run(&done);

    // Sending a message with a higher protocol version should cause the connection to be terminated.
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
        globalSubscription = await window.pushManager.subscribe({
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
        let subscription = await window.pushManager.getSubscription();
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

async function getNotificationPermissionFromServiceWorker()
{
    const channel = new MessageChannel();
    const promise = new Promise((resolve) => {
        channel.port1.onmessage = (event) => resolve(event.data);
    });
    globalRegistration.active.postMessage({ message: "notificationPermission", port: channel.port2 }, [channel.port2]);
    return await promise;
}

async function getPushPermissionState()
{
    try {
        return await globalRegistration.pushManager.permissionState();
    } catch (error) {
        return "Error: " + error;
    }
}

async function getPushPermissionStateFromServiceWorker()
{
    const channel = new MessageChannel();
    const promise = new Promise((resolve) => {
        channel.port1.onmessage = (event) => resolve(event.data);
    });
    globalRegistration.active.postMessage({ message: "getPushPermissionState", port: channel.port2 }, [channel.port2]);
    return await promise;
}

async function queryPermission(name)
{
    try {
        let status = await navigator.permissions.query({ name });
        return status.state;
    } catch (error) {
        return "Error: " + error;
    }
}

async function queryPermissionFromServiceWorker(name)
{
    const channel = new MessageChannel();
    const promise = new Promise((resolve) => {
        channel.port1.onmessage = (event) => resolve(event.data);
    });
    globalRegistration.active.postMessage({ message: "queryPermission", arguments: [name], port: channel.port2 }, [channel.port2]);
    return await promise;
}

async function getPushSubscriptionFromWindow()
{
    try {
        let subscription = await window.pushManager.getSubscription();
        return subscription;
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
        try {
            let subscription = await getPushSubscriptionFromWindow();
            return subscription ? subscription.toJSON() : null;
        } catch (error2) {
            return "Error(s): " + error + ", " + error2;
        }
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

async function getNotifications()
{
    const channel = new MessageChannel();
    const promise = new Promise((resolve) => {
        channel.port1.onmessage = (event) => resolve(event.data);
    });
    globalRegistration.active.postMessage({ message: "getNotifications", port: channel.port2 }, [channel.port2]);
    return await promise;
}

async function closeAllNotifications()
{
    const channel = new MessageChannel();
    const promise = new Promise((resolve) => {
        channel.port1.onmessage = (event) => resolve(event.data);
    });
    globalRegistration.active.postMessage({ message: "closeAllNotifications", port: channel.port2 }, [channel.port2]);
    return await promise;
}

</script>
)SRC"_s;

static constexpr auto serviceWorkerScriptSource = R"SWRESOURCE(
let globalPort;
let showNotifications = true;

function notificationToString(n)
{
    return "title: " + n.title + " body: " + n.body + " tag: " + n.tag + " dir: " + n.dir + " silent: " + n.silent + " data: " + n.data;
}

self.addEventListener("message", (event) => {
    let { message, arguments, port } = event.data;
    var closeAllNotifications = message === "closeAllNotifications";
    if (message === "setup") {
        globalPort = port;
        port.postMessage("Ready");
    } else if (message === "notificationPermission") {
        port.postMessage(Notification.permission);
    } else if (message === "getPushPermissionState") {
        registration.pushManager.permissionState().then(port.postMessage.bind(port), (error) => {
            port.postMessage("getPushPermissionState failed: " + error);
        });
    } else if (message === "queryPermission") {
        let [name] = arguments;
        navigator.permissions.query({ name }).then((status) => {
            port.postMessage(status.state);
        }, (error) => {
            port.postMessage("queryPermission failed: " + error);
        });
    } else if (message === "disableShowNotifications") {
        showNotifications = false;
        port.postMessage(true);
    } else if (message === "getNotifications" || closeAllNotifications) {
        registration.getNotifications().then((notifications) => {
            var result = "";
            for (n = 0; n < notifications.length; ++n) {
                result += n + " - " + notificationToString(notifications[n]) + " ";
                if (closeAllNotifications)
                    notifications[n].close();
            }
            port.postMessage(result);
        }, (exception) => {
            port.postMessage("getNotifications failed: " + exception);
        });
    }
});

self.addEventListener("push", async (event) => {
    if (event.notification) {
        // If the tag is empty, do nothing
        if (!event.notification.tag)
            return;

        var optionsFromTag = event.notification.tag.split(" ");
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
            newTitle = event.notification.data;
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
                newTitle = event.notification.title;
            if (!newActionURL)
                newActionURL = event.notification.navigate;

            self.registration.showNotification(newTitle, { "navigate": newActionURL });
        }

        if (newBadge)
            navigator.setAppBadge(newBadge);

        return;
    }

    try {
        if (showNotifications) {
            await self.registration.showNotification("notification");
            navigator.setAppBadge(42);
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

static void enableFeatureForPreferences(NSString *featureName, WKPreferences *preferences)
{
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:featureName]) {
            [preferences _setEnabled:YES forFeature:feature];
            break;
        }
    }
}

class WebPushDTestWebView {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(WebPushDTestWebView);
public:
    WebPushDTestWebView(const String& pushPartition, const std::optional<WTF::UUID>& dataStoreIdentifier, WKProcessPool *processPool, TestNotificationProvider& notificationProvider, ASCIILiteral html, InstallDataStoreDelegate installDataStoreDelegate, BuiltInNotificationsEnabled builtInNotificationsEnabled)
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

        m_server = makeUnique<TestWebKitAPI::HTTPServer>(std::initializer_list<std::pair<String, TestWebKitAPI::HTTPResponse>> {
            { "/"_s, { html } },
            { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, serviceWorkerScriptSource } }
        }, TestWebKitAPI::HTTPServer::Protocol::HttpsProxy);

        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        // This step is required early to make sure the first NetworkProcess access has the correct
        // setting in the NetworkProcessInitializationParameters
        if (builtInNotificationsEnabled == BuiltInNotificationsEnabled::Yes)
            enableFeatureForPreferences(@"BuiltInNotificationsEnabled", configuration.get().preferences);

        RetainPtr<_WKWebsiteDataStoreConfiguration> dataStoreConfiguration;
        if (dataStoreIdentifier)
            dataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initWithIdentifier:dataStoreIdentifier->createNSUUID().get()]);
        else
            dataStoreConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
        [dataStoreConfiguration setWebPushPartitionString:pushPartition.createNSString().get()];
        [dataStoreConfiguration setProxyConfiguration:@{
            (NSString *)kCFStreamPropertyHTTPSProxyHost: @"127.0.0.1",
            (NSString *)kCFStreamPropertyHTTPSProxyPort: @(m_server->port())
        }];
        dataStoreConfiguration.get().webPushMachServiceName = @"org.webkit.webpushtestdaemon.service";

#if ENABLE(DECLARATIVE_WEB_PUSH)
        dataStoreConfiguration.get().isDeclarativeWebPushEnabled = YES;
#endif

        // FIXME: This seems like it shouldn't be necessary, but _clearResourceLoadStatistics (called by clearWebsiteDataStore) doesn't seem to work.
        [[NSFileManager defaultManager] removeItemAtURL:[dataStoreConfiguration _resourceLoadStatisticsDirectory] error:nil];

        m_dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);

        if (installDataStoreDelegate == InstallDataStoreDelegate::Yes) {
            m_delegate = adoptNS([[PushNotificationDelegate alloc] init]);
            if (builtInNotificationsEnabled == BuiltInNotificationsEnabled::Yes)
                m_delegate.get().expectsDelegateNotificationCallbacks = NO;

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

        [configuration setProcessPool:processPool];
        [configuration setWebsiteDataStore:m_dataStore.get()];
        configuration.get().preferences._appBadgeEnabled = YES;

        auto userContentController = [configuration userContentController];
        [userContentController addScriptMessageHandler:m_testMessageHandler.get() name:@"test"];

        [[configuration preferences] _setPushAPIEnabled:YES];
        m_notificationProvider.setPermission(m_origin, true);

#if ENABLE(DECLARATIVE_WEB_PUSH)
        enableFeatureForPreferences(@"DeclarativeWebPush", configuration.get().preferences);
#endif

        m_webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

        [m_webView _setDontResetTransientActivationAfterRunJavaScript:YES];

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

    id requestNotificationPermission()
    {
        NSError *error = nil;
        id obj = [m_webView objectByCallingAsyncFunction:@"return await Notification.requestPermission()" withArguments:@{ } error:&error];
        return error ?: obj;
    }

    id subscribe(String key = validServerKey)
    {
        NSError *error = nil;
        auto script = makeString("return await subscribe('"_s, key, "')"_s);
        id obj = [m_webView objectByCallingAsyncFunction:script.createNSString().get() withArguments:@{ } error:&error];
        return error ?: obj;
    }

    id unsubscribe()
    {
        NSError *error = nil;
        id obj = [m_webView objectByCallingAsyncFunction:@"return await unsubscribe()" withArguments:@{ } error:&error];
        return error ?: obj;
    }

    id getNotificationPermission()
    {
        return [m_webView stringByEvaluatingJavaScript:@"Notification.permission"];
    }

    id getNotificationPermissionFromServiceWorker()
    {
        NSError *error = nil;
        id obj = [m_webView objectByCallingAsyncFunction:@"return await getNotificationPermissionFromServiceWorker()" withArguments:@{ } error:&error];
        return error ?: obj;
    }

    id getPushPermissionState()
    {
        NSError *error = nil;
        id obj = [m_webView objectByCallingAsyncFunction:@"return await getPushPermissionState()" withArguments:@{ } error:&error];
        return error ?: obj;
    }

    id getPushPermissionStateFromServiceWorker()
    {
        NSError *error = nil;
        id obj = [m_webView objectByCallingAsyncFunction:@"return await getPushPermissionStateFromServiceWorker()" withArguments:@{ } error:&error];
        return error ?: obj;
    }

    id queryPermission(NSString *name)
    {
        NSError *error = nil;
        id obj = [m_webView objectByCallingAsyncFunction:@"return await queryPermission(name)" withArguments:@{ @"name": name } error:&error];
        return error ?: obj;
    }

    id queryPermissionFromServiceWorker(NSString *name)
    {
        NSError *error = nil;
        id obj = [m_webView objectByCallingAsyncFunction:@"return await queryPermissionFromServiceWorker(name)" withArguments:@{ @"name": name } error:&error];
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

    bool hasServiceWorkerRegistration()
    {
        NSError *error = nil;
        id obj = [m_webView objectByCallingAsyncFunction:@"return await navigator.serviceWorker.getRegistration()" withArguments:@{ } error:&error];
        return error ?: obj;
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

    id getNotifications()
    {
        NSError *error = nil;
        id obj = [m_webView objectByCallingAsyncFunction:@"return await getNotifications()" withArguments:@{ } error:&error];
        return error ?: obj;
    }

    NSNumber *getAppBadge()
    {
        __block bool done = false;
        __block NSNumber *result = nil;
        [m_webView.get().configuration.websiteDataStore _getAppBadgeForTesting:^(NSNumber *badge) {
            result = badge;
            done = true;
        }];

        TestWebKitAPI::Util::run(&done);
        return result;
    }

    void closeAllNotifications()
    {
        NSError *error = nil;
        [m_webView objectByCallingAsyncFunction:@"return await closeAllNotifications()" withArguments:@{ } error:&error];
        EXPECT_NULL(error);
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
    void injectDeclarativePushMessage(ASCIILiteral json)
    {
        WebKit::WebPushD::PushMessageForTesting message;
        message.targetAppCodeSigningIdentifier = "com.apple.WebKit.TestWebKitAPI"_s;
        message.registrationURL = URL("https://example.com"_s);
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
        return m_delegate.get().mostRecentNotification.unsafeGet();
    }

    NSURL *mostRecentActionURL()
    {
        return m_delegate.get().mostRecentActionURL.unsafeGet();
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
            @"topic": topic.createNSString().get(),
            @"userInfo": apsUserInfo
        };

        String message { byteCast<Latin1Character>(span([NSJSONSerialization dataWithJSONObject:obj options:0 error:nullptr])) };

        auto utilityConnection = createAndConfigureConnectionToService("org.webkit.webpushtestdaemon.service");
        auto sender = WebPushXPCConnectionMessageSender { utilityConnection.get() };
        __block bool done = false;
        sender.sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::InjectEncryptedPushMessageForTesting(message), ^(bool injected) {
            done = true;
        });
        TestWebKitAPI::Util::run(&done);
    }

    RetainPtr<NSDictionary> fetchPushMessage()
    {
        __block bool gotMessage = false;
        __block RetainPtr<NSDictionary> message;
        [m_dataStore _getPendingPushMessage:^(NSDictionary *rawMessage) {
            message = rawMessage;
            gotMessage = true;
        }];
        TestWebKitAPI::Util::run(&gotMessage);

        return message;
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
        [m_testMessageHandler setDidReceiveScriptMessage:^(NSString *message) {
            m_mostRecentMessage = message;
        }];
    }

    const String& mostRecentMessage() const
    {
        return m_mostRecentMessage;
    }

    NSURL *url() const { return m_url.get(); }

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
    WebPushDTest(LaunchOnlyOnce launchOnlyOnce = LaunchOnlyOnce::Yes, ASCIILiteral html = htmlSource, InstallDataStoreDelegate installDataStoreDelegate = InstallDataStoreDelegate::No, BuiltInNotificationsEnabled builtInNotificationsEnabled = BuiltInNotificationsEnabled::No)
        : m_html(html)
        , m_installDataStoreDelegate(installDataStoreDelegate)
        , m_builtInNotificationsEnabled(builtInNotificationsEnabled)
    {
        m_tempDirectory = retainPtr(setUpTestWebPushD(launchOnlyOnce));
    }

    void SetUp() override
    {
        auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
        auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

        m_notificationProvider = makeUnique<TestWebKitAPI::TestNotificationProvider>(Vector<WKNotificationManagerRef> { [processPool _notificationManagerForTesting], WKNotificationManagerGetSharedServiceWorkerNotificationManager() });

        auto webView = makeUniqueRef<WebPushDTestWebView>(emptyString(), std::nullopt, processPool.get(), *m_notificationProvider, m_html, m_installDataStoreDelegate, m_builtInNotificationsEnabled);
        m_webViews.append(WTFMove(webView));

        auto webViewWithIdentifier1 = makeUniqueRef<WebPushDTestWebView>(emptyString(), WTF::UUID::parse("0bf5053b-164c-4b7d-8179-832e6bf158df"_s), processPool.get(), *m_notificationProvider, m_html, m_installDataStoreDelegate, m_builtInNotificationsEnabled);
        m_webViews.append(WTFMove(webViewWithIdentifier1));

        auto webViewWithIdentifier2 = makeUniqueRef<WebPushDTestWebView>(emptyString(), WTF::UUID::parse("940e7729-738e-439f-a366-1a8719e23b2d"_s), processPool.get(), *m_notificationProvider, m_html, m_installDataStoreDelegate, m_builtInNotificationsEnabled);
        m_webViews.append(WTFMove(webViewWithIdentifier2));

        auto webViewWithPartition = makeUniqueRef<WebPushDTestWebView>("testPartition"_s, std::nullopt, processPool.get(), *m_notificationProvider, m_html, m_installDataStoreDelegate, m_builtInNotificationsEnabled);
        m_webViews.append(WTFMove(webViewWithPartition));

        auto webViewWithPartitionAndIdentifier = makeUniqueRef<WebPushDTestWebView>("testPartition"_s, WTF::UUID::parse("940e7729-738e-439f-a366-1a8719e23b2d"_s), processPool.get(), *m_notificationProvider, m_html, m_installDataStoreDelegate, m_builtInNotificationsEnabled);
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
    BuiltInNotificationsEnabled m_builtInNotificationsEnabled { BuiltInNotificationsEnabled::No };
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
    std::ranges::sort(subscribed, lessThan);

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

TEST_F(WebPushDTest, SubscribeWithBadIPCVersionRaisesExceptionTest)
{
    auto utilityConnection = createAndConfigureConnectionToService("org.webkit.webpushtestdaemon.service");
    auto sender = WebPushXPCConnectionMessageSender { utilityConnection.get() };
    bool done = false;
    sender.sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::SetProtocolVersionForTesting(WebKit::WebPushD::protocolVersionValue + 1), [&done]() {
        done = true;
    });
    TestWebKitAPI::Util::run(&done);

    for (auto& v : webViews()) {
        ASSERT_FALSE(v->hasPushSubscription());
        id obj = v->subscribe();
        ASSERT_TRUE([obj isEqual:@"Error: AbortError: Connection to web push daemon failed"]);
    }
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
    std::ranges::sort(subscribed, lessThan);

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

    for (auto& v : webViews()) {
        ASSERT_TRUE(v->hasPushSubscription());
        id result = v->unregisterServiceWorker();
        ASSERT_TRUE([result isEqual:@YES]);
        ASSERT_TRUE(v->hasPushSubscription());
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

        RetainPtr vOrigin = v->origin().createNSString();
        WKWebsiteDataRecord *filteredRecord = nil;
        for (WKWebsiteDataRecord *record in records.get()) {
            for (NSString *originString in record._originsStrings) {
                if ([originString isEqualToString:vOrigin.get()]) {
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

class WebPushDBuiltInTest : public WebPushDTest {
public:
    WebPushDBuiltInTest()
        : WebPushDTest(LaunchOnlyOnce::Yes, htmlSource, InstallDataStoreDelegate::Yes, BuiltInNotificationsEnabled::Yes)
    {
    }
};

#if HAVE(FULL_FEATURED_USER_NOTIFICATIONS)
TEST_F(WebPushDBuiltInTest, ShowAndGetNotifications)
{
    auto& view = webViews().last();
    view->subscribe();

    auto dataStore = view->dataStore();

    auto configuration = defaultWebPushDaemonConfiguration();
    configuration.pushPartitionString = dataStore.get()._webPushPartition;
    configuration.dataStoreIdentifier = WTF::UUID::fromNSUUID(dataStore.get()._identifier);
    auto utilityConnection = createAndConfigureConnectionToService("org.webkit.webpushtestdaemon.service", WTFMove(configuration));
    auto sender = WebPushXPCConnectionMessageSender { utilityConnection.get() };

    WebKit::WebPushD::PushMessageForTesting message;
    message.targetAppCodeSigningIdentifier = "com.apple.WebKit.TestWebKitAPI"_s;
    message.pushPartitionString = dataStore.get()._webPushPartition;
    message.registrationURL = view->url();
    message.disposition = WebKit::WebPushD::PushMessageDisposition::Legacy;
    message.payload = @"hello";

    // No badge had been set, so confirm its `nil`
    EXPECT_FALSE(view->getAppBadge());

    done = false;
    sender.sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::InjectPushMessageForTesting(message), ^(const String& error) {
        if (!error.isEmpty())
            NSLog(@"ERROR: %s", error.utf8().data());
        done = true;
    });
    TestWebKitAPI::Util::run(&done);

    done = false;
    RetainPtr delegate = (PushNotificationDelegate *)dataStore.get()._delegate;
    [dataStore _getPendingPushMessages:^(NSArray<NSDictionary *> *messages) {
        EXPECT_EQ(messages.count, 1u);

        [dataStore _processPushMessage:messages.firstObject completionHandler:^(bool handled) {
            EXPECT_TRUE(handled);
            done = true;
        }];
    }];
    TestWebKitAPI::Util::run(&done);

    id result = view->getNotifications();
    EXPECT_TRUE([result isEqualToString:@"0 - title: notification body:  tag:  dir: auto silent: null data: null "]);

    view->closeAllNotifications();

    result = view->getNotifications();
    EXPECT_TRUE([result isEqualToString:@""]);

    // The push message handler should set the app badge to 42
    EXPECT_TRUE([view->getAppBadge() isEqual:@42]);
}

TEST_F(WebPushDBuiltInTest, TestPermissionsAfterNotificatonRequestPermission)
{
    auto& view = webViews().last();

    EXPECT_TRUE([view->getNotificationPermission() isEqual:@"default"]);
    EXPECT_TRUE([view->getNotificationPermissionFromServiceWorker() isEqualToString:@"default"]);
    EXPECT_TRUE([view->getPushPermissionState() isEqual:@"prompt"]);
    EXPECT_TRUE([view->getPushPermissionStateFromServiceWorker() isEqualToString:@"prompt"]);
    EXPECT_TRUE([view->queryPermission(@"push") isEqual:@"prompt"]);
    EXPECT_TRUE([view->queryPermissionFromServiceWorker(@"push") isEqual:@"prompt"]);
    EXPECT_TRUE([view->queryPermission(@"notifications") isEqual:@"prompt"]);
    EXPECT_TRUE([view->queryPermissionFromServiceWorker(@"notifications") isEqual:@"prompt"]);

    EXPECT_TRUE([view->requestNotificationPermission() isEqual:@"granted"]);

    EXPECT_TRUE([view->getNotificationPermission() isEqual:@"granted"]);
    EXPECT_TRUE([view->getNotificationPermissionFromServiceWorker() isEqualToString:@"granted"]);
    EXPECT_TRUE([view->getPushPermissionState() isEqual:@"granted"]);
    EXPECT_TRUE([view->getPushPermissionStateFromServiceWorker() isEqualToString:@"granted"]);
    EXPECT_TRUE([view->queryPermission(@"push") isEqual:@"granted"]);
    EXPECT_TRUE([view->queryPermissionFromServiceWorker(@"push") isEqual:@"granted"]);
    EXPECT_TRUE([view->queryPermission(@"notifications") isEqual:@"granted"]);
    EXPECT_TRUE([view->queryPermissionFromServiceWorker(@"notifications") isEqual:@"granted"]);
}

TEST_F(WebPushDBuiltInTest, TestPermissionsAfterSubscribe)
{
    auto& view = webViews().last();

    EXPECT_FALSE(view->hasPushSubscription());
    EXPECT_TRUE([view->getNotificationPermission() isEqual:@"default"]);
    EXPECT_TRUE([view->getNotificationPermissionFromServiceWorker() isEqualToString:@"default"]);
    EXPECT_TRUE([view->getPushPermissionState() isEqual:@"prompt"]);
    EXPECT_TRUE([view->getPushPermissionStateFromServiceWorker() isEqualToString:@"prompt"]);
    EXPECT_TRUE([view->queryPermission(@"push") isEqual:@"prompt"]);
    EXPECT_TRUE([view->queryPermissionFromServiceWorker(@"push") isEqual:@"prompt"]);
    EXPECT_TRUE([view->queryPermission(@"notifications") isEqual:@"prompt"]);
    EXPECT_TRUE([view->queryPermissionFromServiceWorker(@"notifications") isEqual:@"prompt"]);

    view->subscribe();

    EXPECT_TRUE(view->hasPushSubscription());
    EXPECT_TRUE([view->getNotificationPermission() isEqual:@"granted"]);
    EXPECT_TRUE([view->getNotificationPermissionFromServiceWorker() isEqualToString:@"granted"]);
    EXPECT_TRUE([view->getPushPermissionState() isEqual:@"granted"]);
    EXPECT_TRUE([view->getPushPermissionStateFromServiceWorker() isEqualToString:@"granted"]);
    EXPECT_TRUE([view->queryPermission(@"push") isEqual:@"granted"]);
    EXPECT_TRUE([view->queryPermissionFromServiceWorker(@"push") isEqual:@"granted"]);
    EXPECT_TRUE([view->queryPermission(@"notifications") isEqual:@"granted"]);
    EXPECT_TRUE([view->queryPermissionFromServiceWorker(@"notifications") isEqual:@"granted"]);
}

TEST_F(WebPushDBuiltInTest, ImplicitSilentPushTimerCancelledOnShowingNotification)
{
    for (auto& v : webViews())
        v->subscribe();
    ASSERT_EQ(subscribedTopicsCount(), webViews().size());

    for (auto& v : webViews()) {
        ASSERT_TRUE(v->hasPushSubscription());

        for (unsigned i = 0; i < WebKit::WebPushD::maxSilentPushCount; i++) {
            v->injectPushMessage(@{ });
            auto message = v->fetchPushMessage();
            ASSERT_TRUE(v->expectDecryptedMessage(@"null data", message.get()));
        }

        [NSThread sleepForTimeInterval:(WebKit::WebPushD::silentPushTimeoutForTesting.seconds() + 0.5)];
        ASSERT_TRUE(v->hasPushSubscription());
    }
}

TEST_F(WebPushDBuiltInTest, ImplicitSilentPushTimerIgnoredForInspectedContexts)
{
    auto utilityConnection = createAndConfigureConnectionToService("org.webkit.webpushtestdaemon.service");
    auto sender = WebPushXPCConnectionMessageSender { utilityConnection.get() };

    auto setServiceWorkerIsBeingInspected = [&](const String& originString) {
        __block bool done = false;
        sender.sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::SetServiceWorkerIsBeingInspected(URL(originString), true), ^() {
            done = true;
        });
        TestWebKitAPI::Util::run(&done);
    };

    for (auto& v : webViews()) {
        v->subscribe();
        v->disableShowNotifications();
    }
    ASSERT_EQ(subscribedTopicsCount(), webViews().size());

    for (auto& v : webViews()) {
        ASSERT_TRUE(v->hasPushSubscription());
        setServiceWorkerIsBeingInspected(v->origin());

        for (unsigned i = 0; i < WebKit::WebPushD::maxSilentPushCount; i++) {
            v->injectPushMessage(@{ });
            auto message = v->fetchPushMessage();

            // _processPushMessage should return false since we disabled showing notifications above.
            ASSERT_FALSE(v->expectDecryptedMessage(@"null data", message.get()));
        }

        // Should still be subscribed since we don't enforce the silent push timer while being inspected.
        [NSThread sleepForTimeInterval:(WebKit::WebPushD::silentPushTimeoutForTesting.seconds() + 0.5)];
        ASSERT_TRUE(v->hasPushSubscription());
    }
}

TEST_F(WebPushDBuiltInTest, ImplicitSilentPushTimerCausesUnsubscribe)
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
            auto message = v->fetchPushMessage();

            // _processPushMessage should return false since we disabled showing notifications above.
            ASSERT_FALSE(v->expectDecryptedMessage(@"null data", message.get()));
        }

        bool unsubscribed = false;
        TestWebKitAPI::Util::waitForConditionWithLogging([&] {
            unsubscribed = !v->hasPushSubscription();
            [NSThread sleepForTimeInterval:0.25];
            return unsubscribed;
        }, 5, @"Timed out waiting for push subscription to be unsubscribed.");
        ASSERT_TRUE(unsubscribed);

        // Unsubscribing from this data store should not affect subscriptions in other data stores.
        ASSERT_EQ(subscribedTopicsCount(), webViews().size() - i);
        i++;
    }
}

#endif // HAVE(FULL_FEATURED_USER_NOTIFICATIONS)


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
        @"payload": payloadBase64.createNSString().get()
    });
}

TEST_F(WebPushDTest, NotificationClickExtendsITPCleanupTimerBy30Days)
{
    // FIXME: test on all webviews once we finish refactoring the shared service worker notification
    // managers to be datastore-aware.
    auto& v = webViews().last();
    v->subscribe();

    EXPECT_TRUE(v->hasServiceWorkerRegistration());
    EXPECT_TRUE(v->hasPushSubscription());

    v->assertPushEventSucceeds(0);
    v->assertPushEventSucceeds(29);
    v->simulateNotificationClick();
    EXPECT_TRUE(v->hasServiceWorkerRegistration());
    EXPECT_TRUE(v->hasPushSubscription());

    v->assertPushEventSucceeds(58);
    EXPECT_TRUE(v->hasServiceWorkerRegistration());
    EXPECT_TRUE(v->hasPushSubscription());

    v->setITPTimeAdvance(61);
    EXPECT_FALSE(v->hasServiceWorkerRegistration());
    EXPECT_TRUE(v->hasPushSubscription());

    // Verify that even though the service worker is gone, push messages do make it through (because the subscription is still active)
    v->injectPushMessage(@{ });
    auto messages = v->fetchPushMessages();
    ASSERT_EQ([messages count], 1u) << "Unexpected push event injection failure after advancing ITP timer by 61 days; ITP cleanup removed subscription?";
}

#if ENABLE(DECLARATIVE_WEB_PUSH)

static constexpr ASCIILiteral json5 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "foo"
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json6 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/"
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json7 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": ""
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json8 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": 4
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json9 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "app_badge": "",
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!"
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json10 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "app_badge": -1,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!"
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json11 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "app_badge": { },
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!"
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json12 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "app_badge": 10,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!"
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json13 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!",
        "options": 0
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json14 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!",
        "options": { }
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json15 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!",
        "dir": 0
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json16 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!",
        "dir": "auto"
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json17 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!",
        "dir": "ltr"
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json18 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!",
        "dir": "rtl"
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json19 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!",
        "dir": "nonsense"
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json20 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!",
        "lang": { }
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json21 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!",
        "lang": "language"
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json22 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!",
        "body": { }
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json23 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!",
        "body": "world"
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json24 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!",
        "tag": { }
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json25 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!",
        "tag": "world"
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json26 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!",
        "icon": 0
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json27 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!",
        "icon": "world"
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json28 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!",
        "icon": "https://example.com/icon.png"
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json29 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!",
        "silent": 0
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json30 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!",
        "silent": true
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json31 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!",
        "silent": false
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json32 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "app_badge": "20",
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!"
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json33 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "app_badge": "8031",
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!"
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json34 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "app_badge": "18446744073709551616",
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!"
    }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json35 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!"
    },
    "mutable": 39
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json36 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!"
    },
    "mutable": { }
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json37 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!"
    },
    "mutable": "true"
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json38 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!",
    },
    "mutable": true
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json39 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "app_badge": "12",
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!"
    },
    "mutable": true
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json40 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "app_badge": "12",
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!",
        "tag": "title Gotcha!"
    },
    "mutable": true
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json41 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "app_badge": "12",
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!",
        "tag": "badge 1024"
    },
    "mutable": true
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json42 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "app_badge": "12",
    "notification": {
        "navigate": "https://example.com/",
        "title": "Hello world!",
        "tag": "titleandbadge ThisRules 4096"
    },
    "mutable": true
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json43 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "app_badge": "12",
    "notification": {
        "navigate": "https://example.com/",
        "title": "Test the data object",
        "tag": "datatotitle",
        "data": "Raw string"
    },
    "mutable": true
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json44 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "app_badge": "12",
    "notification": {
        "navigate": "https://example.com/",
        "title": "Test the data object",
        "tag": "datatotitle",
        "data": { "key": "value" }
    },
    "mutable": true
}
)JSONRESOURCE"_s;
static constexpr ASCIILiteral json45 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "app_badge": "12",
    "notification": {
        "navigate": "https://example.com/",
        "title": "Test a default action URL override",
        "tag": "defaultactionurl https://webkit.org/"
    },
    "mutable": true
}
)JSONRESOURCE"_s;
// Intentionally keep mutable as a child of notification here until we fix webkit.org/b/297389.
static constexpr ASCIILiteral json46 = R"JSONRESOURCE(
{
    "web_push": 8030,
    "app_badge": "12",
    "notification": {
        "navigate": "https://example.com/",
        "title": "Test a missing default action URL override",
        "mutable": true,
        "tag": "emptydefaultactionurl"
    }
}
)JSONRESOURCE"_s;

static constexpr ASCIILiteral errors[] = {
    "does not contain valid JSON"_s,
    "top level JSON value is not an object"_s,
    "'navigate' member is specified but does not represent a valid URL"_s,
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

        for (NSDictionary *message in messages)
            EXPECT_TRUE(message[@"WebKitNotificationPayload"]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

// Verifies that handling a declarative web push message - with no service worker even registered - calls
// back into the client for showing the notification, etc.
TEST(WebPushD, DeclarativeWebPushHandling)
{
    setUpTestWebPushD();

    auto dataStoreConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    dataStoreConfiguration.get().webPushMachServiceName = @"org.webkit.webpushtestdaemon.service";
    dataStoreConfiguration.get().isDeclarativeWebPushEnabled = YES;
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);
    clearWebsiteDataStore(dataStore.get());

    auto delegate = adoptNS([[PushNotificationDelegate alloc] init]);
    dataStore.get()._delegate = delegate.get();

    auto utilityConnection = createAndConfigureConnectionToService("org.webkit.webpushtestdaemon.service");
    auto sender = WebPushXPCConnectionMessageSender { utilityConnection.get() };
    static bool done = false;

    WebKit::WebPushD::PushMessageForTesting message;
    message.pushPartitionString = "TestWebKitAPI"_s;
    message.targetAppCodeSigningIdentifier = "com.apple.WebKit.TestWebKitAPI"_s;
    message.registrationURL = URL("https://example.com"_s);
    message.disposition = WebKit::WebPushD::PushMessageDisposition::Notification;
    message.payload = json33;
    sender.sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::InjectPushMessageForTesting(message), [&](const String& error) {
        EXPECT_TRUE(error.isEmpty());
        done = true;
    });
    TestWebKitAPI::Util::run(&done);

    // Verify that even after having sent a push message to the daemon, there are no pending messages, as it was already handled.
    done = false;
    [dataStore _getPendingPushMessages:^(NSArray<NSDictionary *> *messages) {
        EXPECT_EQ(messages.count, 0u);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

#if HAVE(FULL_FEATURED_USER_NOTIFICATIONS)
    RetainPtr configuration = adoptNS([[_WKWebPushDaemonConnectionConfiguration alloc] init]);
    configuration.get().machServiceName = @"org.webkit.webpushtestdaemon.service";
    configuration.get().bundleIdentifierOverrideForTesting = @"com.apple.WebKit.TestWebKitAPI";
    configuration.get().hostApplicationAuditToken = getSelfAuditToken();
    configuration.get().partition = @"TestWebKitAPI";
    RetainPtr connection = adoptNS([[_WKWebPushDaemonConnection alloc] initWithConfiguration:configuration.get()]);

    done = false;

    [connection getNotifications:message.registrationURL.createNSURL().get() tag:@"" completionHandler:^(NSArray<_WKNotificationData *> *notifications, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(notifications);
        EXPECT_EQ(notifications.count, 1u);
        EXPECT_TRUE([notifications[0].title isEqualToString:@"Hello world!"]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
#endif // HAVE(FULL_FEATURED_USER_NOTIFICATIONS)

    // FIXME: Figure out how to activate the notification programtically to verify the appropriate delegate callbacks are made
}

#if HAVE(FULL_FEATURED_USER_NOTIFICATIONS)

TEST(WebPushD, WKWebPushDaemonConnectionRequestPushPermission)
{
    setUpTestWebPushD();

    auto configuration = adoptNS([[_WKWebPushDaemonConnectionConfiguration alloc] init]);
    configuration.get().machServiceName = @"org.webkit.webpushtestdaemon.service";
    configuration.get().hostApplicationAuditToken = getSelfAuditToken();
    auto connection = adoptNS([[_WKWebPushDaemonConnection alloc] initWithConfiguration:configuration.get()]);
    auto url = adoptNS([[NSURL alloc] initWithString:@"https://webkit.org"]);

    __block bool done = false;
    [connection getPushPermissionStateForOrigin:url.get() completionHandler:^(_WKWebPushPermissionState state) {
        done = true;
        EXPECT_EQ(state, _WKWebPushPermissionStatePrompt);
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [connection requestPushPermissionForOrigin:url.get() completionHandler:^(BOOL granted) {
        done = true;
        EXPECT_TRUE(granted);
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [connection getPushPermissionStateForOrigin:url.get() completionHandler:^(_WKWebPushPermissionState state) {
        done = true;
        EXPECT_EQ(state, _WKWebPushPermissionStateGranted);
    }];
    TestWebKitAPI::Util::run(&done);
}
#endif

TEST(WebPushD, WKWebPushDaemonConnectionPushNotifications)
{
    setUpTestWebPushD();

    auto configuration = adoptNS([[_WKWebPushDaemonConnectionConfiguration alloc] init]);
    configuration.get().machServiceName = @"org.webkit.webpushtestdaemon.service";
    // Bundle identifier is required for making push subscription.
    configuration.get().bundleIdentifierOverrideForTesting = @"com.apple.WebKit.TestWebKitAPI";
    configuration.get().hostApplicationAuditToken = getSelfAuditToken();
    auto connection = adoptNS([[_WKWebPushDaemonConnection alloc] initWithConfiguration:configuration.get()]);
    auto url = adoptNS([[NSURL alloc] initWithString:@"https://webkit.org/sw.js"]);
    RetainPtr applicationServerKey = [NSData dataWithBytes:(const void *)validServerKey.characters() length:validServerKey.length()];

    __block bool done = false;
    [connection subscribeToPushServiceForScope:url.get() applicationServerKey:applicationServerKey.get() completionHandler:^(_WKWebPushSubscriptionData *subscription, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(subscription);
        EXPECT_WK_STREQ(@"https://webkit.org/push", subscription.endpoint.absoluteString);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [connection getSubscriptionForScope:url.get() completionHandler:^(_WKWebPushSubscriptionData *subscription, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(subscription);
        EXPECT_WK_STREQ(@"https://webkit.org/push", subscription.endpoint.absoluteString);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [connection unsubscribeFromPushServiceForScope:url.get() completionHandler:^(BOOL unsubscribed, NSError * error) {
        EXPECT_NULL(error);
        EXPECT_TRUE(unsubscribed);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [connection getSubscriptionForScope:url.get() completionHandler:^(_WKWebPushSubscriptionData *subscription, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_NULL(subscription);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

#if HAVE(FULL_FEATURED_USER_NOTIFICATIONS)
    done = false;
    [connection getNotifications:url.get() tag:@"" completionHandler:^(NSArray<_WKNotificationData *> *notifications, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(notifications);
        EXPECT_EQ(notifications.count, 0u);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    RetainPtr notification = adoptNS([[_WKMutableNotificationData alloc] init]);
    notification.get().title = @"Hello World!";
    notification.get().dir = _WKNotificationDirectionLTR;
    notification.get().lang = @"en";
    notification.get().body = @"Body1";
    notification.get().tag = @"Tag1";
    notification.get().alert = _WKNotificationAlertSilent;
    notification.get().data = [NSData data];
    notification.get().securityOrigin = url.get();
    notification.get().serviceWorkerRegistrationURL = url.get();
    RetainPtr uuid1 = [NSUUID UUID];
    notification.get().uuid = uuid1.get();

    done = false;
    [connection showNotification:notification.get() completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [connection getNotifications:url.get() tag:@"" completionHandler:^(NSArray<_WKNotificationData *> *notifications, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_EQ(notifications.count, 1u);
        if (notifications.count) {
            EXPECT_TRUE([notifications[0].title isEqualToString:@"Hello World!"]);
            EXPECT_EQ(notifications[0].dir, _WKNotificationDirectionLTR);
            EXPECT_TRUE([notifications[0].lang isEqualToString:@"en"]);
            EXPECT_TRUE([notifications[0].body isEqualToString:@"Body1"]);
            EXPECT_TRUE([notifications[0].tag isEqualToString:@"Tag1"]);
            EXPECT_EQ(notifications[0].alert, _WKNotificationAlertSilent);
            EXPECT_TRUE([notifications[0].data isEqual:[NSData data]]);
            EXPECT_TRUE([notifications[0].securityOrigin isEqual:notification.get().securityOrigin]);
            EXPECT_TRUE([notifications[0].serviceWorkerRegistrationURL isEqual:url.get()]);
            EXPECT_TRUE([notifications[0].uuid isEqual:uuid1.get()]);
        }
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    [connection cancelNotification:url.get() uuid:uuid1.get()];

    done = false;
    [connection getNotifications:url.get() tag:@"" completionHandler:^(NSArray<_WKNotificationData *> *notifications, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(notifications);
        EXPECT_EQ(notifications.count, 0u);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [connection showNotification:notification.get() completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    notification.get().body = @"Body2";
    notification.get().tag = @"Tag2";
    RetainPtr uuid2 = [NSUUID UUID];
    notification.get().uuid = uuid2.get();
    done = false;
    [connection showNotification:notification.get() completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [connection getNotifications:url.get() tag:@"" completionHandler:^(NSArray<_WKNotificationData *> *notifications, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_EQ(notifications.count, 2u);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [connection getNotifications:url.get() tag:@"Tag1" completionHandler:^(NSArray<_WKNotificationData *> *notifications, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_EQ(notifications.count, 1u);
        if (notifications.count)
            EXPECT_TRUE([notifications[0].body isEqualToString:@"Body1"]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [connection getNotifications:url.get() tag:@"Tag2" completionHandler:^(NSArray<_WKNotificationData *> *notifications, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_EQ(notifications.count, 1u);
        if (notifications.count)
            EXPECT_TRUE([notifications[0].body isEqualToString:@"Body2"]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    [connection cancelNotification:url.get() uuid:uuid1.get()];

    done = false;
    [connection getNotifications:url.get() tag:@"" completionHandler:^(NSArray<_WKNotificationData *> *notifications, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_EQ(notifications.count, 1u);
        if (notifications.count) {
            EXPECT_TRUE([notifications[0].body isEqualToString:@"Body2"]);
            EXPECT_TRUE([notifications[0].uuid isEqual:uuid2.get()]);
        }
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
#endif // HAVE(FULL_FEATURED_USER_NOTIFICATIONS)
}

TEST(WebPushD, WKWebPushDaemonConnectionSubscribeWithBadIPCVersionRaisesException)
{
    setUpTestWebPushD();

    auto utilityConnection = createAndConfigureConnectionToService("org.webkit.webpushtestdaemon.service");
    auto sender = WebPushXPCConnectionMessageSender { utilityConnection.get() };
    bool done = false;
    sender.sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::SetProtocolVersionForTesting(WebKit::WebPushD::protocolVersionValue + 1), [&done]() {
        done = true;
    });
    TestWebKitAPI::Util::run(&done);

    auto configuration = adoptNS([[_WKWebPushDaemonConnectionConfiguration alloc] init]);
    configuration.get().machServiceName = @"org.webkit.webpushtestdaemon.service";
    // Bundle identifier is required for making push subscription.
    configuration.get().bundleIdentifierOverrideForTesting = @"com.apple.WebKit.TestWebKitAPI";
    configuration.get().hostApplicationAuditToken = getSelfAuditToken();
    auto connection = adoptNS([[_WKWebPushDaemonConnection alloc] initWithConfiguration:configuration.get()]);
    auto url = adoptNS([[NSURL alloc] initWithString:@"https://webkit.org/sw.js"]);
    RetainPtr applicationServerKey = [NSData dataWithBytes:(const void *)validServerKey.characters() length:validServerKey.length()];

    done = false;
    RetainPtr<NSError> error;
    [connection subscribeToPushServiceForScope:url.get() applicationServerKey:applicationServerKey.get() completionHandler:[&done, &error] (_WKWebPushSubscriptionData *subscription, NSError *subscriptionError) {
        error = subscriptionError;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    ASSERT_TRUE([[error description] containsString:@"Connection to web push daemon failed"]);
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

        auto message = webViews().first()->fetchPushMessage();
        EXPECT_TRUE(message);

        webViews().first()->captureAllMessages();
        webViews().first()->processPushMessage(message.get());
    }

    void waitForMessageAndVerify(NSString *message)
    {
        while (webViews().first()->mostRecentMessage().isEmpty())
            TestWebKitAPI::Util::runFor(0.05_s);

        EXPECT_TRUE([webViews().first()->mostRecentMessage().createNSString() isEqualToString:message]);
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

    EXPECT_TRUE(webViews().first()->hasPushSubscription());

    runTest(json40);
    checkLastNotificationTitle(@"Gotcha!");
    checkLastAppBadge(12);

    EXPECT_TRUE(webViews().first()->hasPushSubscription());

    runTest(json41);
    checkLastNotificationTitle(@"Hello world!");
    checkLastAppBadge(1024);

    EXPECT_TRUE(webViews().first()->hasPushSubscription());

    runTest(json42);
    checkLastNotificationTitle(@"ThisRules");
    checkLastAppBadge(4096);

    EXPECT_TRUE(webViews().first()->hasPushSubscription());

    runTest(json43);
    checkLastNotificationTitle(@"Raw string");
    checkLastAppBadge(12);

    EXPECT_TRUE(webViews().first()->hasPushSubscription());

    runTest(json44);
    checkLastNotificationTitle(@"[object Object]");
    checkLastAppBadge(12);

    EXPECT_TRUE(webViews().first()->hasPushSubscription());

    runTest(json45);
    checkLastNotificationTitle(@"Test a default action URL override");
    checkLastNotificationDefaultActionURL(@"https://webkit.org/");
    checkLastAppBadge(12);

    runTest(json46);
    checkLastNotificationTitle(@"Test a missing default action URL override");
    checkLastNotificationDefaultActionURL(@"https://example.com/");
    waitForMessageAndVerify(@"showNotification failed: TypeError: Call to showNotification() while handling a `push` event did not include NotificationOptions that specify a valid defaultAction url");

    // After the slew of above messages that were handled by service workers, silent push tracking should *not* have
    // kicked in, and therefore there should still be a push subscription.
    [NSThread sleepForTimeInterval:(WebKit::WebPushD::silentPushTimeoutForTesting.seconds() + 0.5)];
    EXPECT_TRUE(webViews().first()->hasPushSubscription());
}

#endif // ENABLE(DECLARATIVE_WEB_PUSH)

} // namespace TestWebKitAPI

#endif // ENABLE(NOTIFICATIONS) && ENABLE(NOTIFICATION_EVENT) && (PLATFORM(MAC)
