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
#if ENABLE(WEB_PUSH_NOTIFICATIONS)

#import "WebPushDaemon.h"

#import "DaemonDecoder.h"
#import "DaemonEncoder.h"
#import "DaemonUtilities.h"
#import "FrontBoardServicesSPI.h"
#import "HandleMessage.h"
#import "LaunchServicesSPI.h"
#import "MessageNames.h"
#import "UserNotificationsSPI.h"
#import "_WKMockUserNotificationCenter.h"

#import <WebCore/LocalizedStrings.h>
#import <WebCore/NotificationData.h>
#import <WebCore/NotificationPayload.h>
#import <WebCore/SecurityOrigin.h>
#import <WebCore/SecurityOriginData.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <span>
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/HexNumber.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RunLoop.h>
#import <wtf/URL.h>
#import <wtf/WorkQueue.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/spi/darwin/XPCSPI.h>
#import <wtf/text/MakeString.h>

#if PLATFORM(IOS) || PLATFORM(VISION)
#import "UIKitSPI.h"
#import <UIKit/UIApplication.h>
#endif

#define WEBPUSHDAEMON_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%{public}s [connection=%p, app=%{public}s]: " fmt, __func__, &connection, connection.subscriptionSetIdentifier().debugDescription().utf8().data(), ##__VA_ARGS__)
#define WEBPUSHDAEMON_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%{public}s [connection=%p, app=%{public}s]: " fmt, __func__, &connection, connection.subscriptionSetIdentifier().debugDescription().utf8().data(), ##__VA_ARGS__)

using namespace WebKit::WebPushD;
using WebCore::PushSubscriptionSetIdentifier;

namespace WebPushD {

static constexpr Seconds s_incomingPushTransactionTimeout { 10_s };

#if HAVE(FULL_FEATURED_USER_NOTIFICATIONS)

static bool platformShouldPlaySound(const WebCore::NotificationData& data)
{
#if PLATFORM(IOS)
    return data.silent == std::nullopt || !(*data.silent);
#elif PLATFORM(MAC)
    return data.silent != std::nullopt && !(*data.silent);
#else
    return false;
#endif
}

static NSString *platformDefaultActionBundleIdentifier()
{
#if PLATFORM(IOS)
    return @"com.apple.webapp";
#else
    // FIXME: Calculate appropriate value on macOS
    return nil;
#endif
}

static RetainPtr<NSString> platformNotificationCenterBundleIdentifier(PushClientConnection& connection)
{
#if PLATFORM(IOS)
    return [NSString stringWithFormat:@"com.apple.WebKit.PushBundle.%@", (NSString *)connection.pushPartitionString()];
#else
    // FIXME: Calculate the correct values on macOS in a non-testing environment.
    RELEASE_ASSERT(connection.hostAppCodeSigningIdentifier() == "com.apple.WebKit.TestWebKitAPI"_s);
    return [NSString stringWithFormat:@"com.apple.WebKit.PushBundle.%@", (NSString *)connection.pushPartitionString()];
#endif
}

static NSString *platformNotificationSourceForDisplay(PushClientConnection& connection)
{
#if PLATFORM(IOS)
    return (NSString *)connection.associatedWebClipTitle();
#else
    // FIXME: Calculate appropriate value on macOS
    return nil;
#endif
}

#endif

WebPushDaemon& WebPushDaemon::singleton()
{
    static NeverDestroyed<WebPushDaemon> daemon;
    return daemon;
}

WebPushDaemon::WebPushDaemon()
    : m_incomingPushTransactionTimer { *this, &WebPushDaemon::incomingPushTransactionTimerFired }
    , m_silentPushTimer { *this, &WebPushDaemon::silentPushTimerFired }
#if HAVE(FULL_FEATURED_USER_NOTIFICATIONS)
    , m_userNotificationCenterClass { [UNUserNotificationCenter class] }
#endif
{
}

void WebPushDaemon::startMockPushService()
{
    m_usingMockPushService = true;

#if HAVE(FULL_FEATURED_USER_NOTIFICATIONS)
    m_userNotificationCenterClass = [_WKMockUserNotificationCenter class];
#endif

    auto messageHandler = [this](const PushSubscriptionSetIdentifier& identifier, WebKit::WebPushMessage&& message) {
        handleIncomingPush(identifier, WTFMove(message));
    };
    PushService::createMockService(WTFMove(messageHandler), [this](auto&& pushService) mutable {
        setPushService(WTFMove(pushService));
    });
}

void WebPushDaemon::startPushService(const String& incomingPushServiceName, const String& databasePath)
{
    auto messageHandler = [this](const PushSubscriptionSetIdentifier& identifier, WebKit::WebPushMessage&& message) {
        handleIncomingPush(identifier, WTFMove(message));
    };
    PushService::create(incomingPushServiceName, databasePath, WTFMove(messageHandler), [this](auto&& pushService) mutable {
        setPushService(WTFMove(pushService));
    });
}

void WebPushDaemon::setPushService(std::unique_ptr<PushService>&& pushService)
{
    m_pushService = WTFMove(pushService);
    m_pushServiceStarted = true;

    if (!m_pendingPushServiceFunctions.size())
        return;

    WorkQueue::main().dispatch([this]() {
        while (m_pendingPushServiceFunctions.size()) {
            auto function = m_pendingPushServiceFunctions.takeFirst();
            function();
        }
    });
}

void WebPushDaemon::runAfterStartingPushService(Function<void()>&& function)
{
    if (!m_pushServiceStarted) {
        m_pendingPushServiceFunctions.append(WTFMove(function));
        return;
    }
    function();
}

void WebPushDaemon::ensureIncomingPushTransaction()
{
    if (!m_incomingPushTransaction)
        m_incomingPushTransaction = adoptOSObject(os_transaction_create("com.apple.webkit.webpushd.daemon.incoming-push"));
    m_incomingPushTransactionTimer.startOneShot(s_incomingPushTransactionTimeout);
}

void WebPushDaemon::releaseIncomingPushTransaction()
{
    m_incomingPushTransactionTimer.stop();
    m_incomingPushTransaction = nullptr;
}

void WebPushDaemon::incomingPushTransactionTimerFired()
{
    RELEASE_LOG_ERROR_IF(m_incomingPushTransaction, Push, "UI process failed to fetch push before incoming push transaction timed out.");
    m_incomingPushTransaction = nullptr;
}

static void tryCloseRequestConnection(xpc_object_t request)
{
    if (auto connection = xpc_dictionary_get_remote_connection(request))
        xpc_connection_cancel(connection);
}

void WebPushDaemon::connectionEventHandler(xpc_object_t request)
{
    if (xpc_get_type(request) != XPC_TYPE_DICTIONARY)
        return;

    auto version = xpc_dictionary_get_uint64(request, protocolVersionKey);
    if (version != protocolVersionValue) {
        RELEASE_LOG_ERROR(Push, "Received request with protocol version %llu not matching daemon protocol version %llu", version, protocolVersionValue);
        tryCloseRequestConnection(request);
        return;
    }

    size_t dataSize { 0 };
    auto data = static_cast<const uint8_t*>(xpc_dictionary_get_data(request, protocolEncodedMessageKey, &dataSize));
    if (!data) {
        RELEASE_LOG_ERROR(Push, "WebPushDaemon::connectionEventHandler - No encoded message data in xpc message");
        tryCloseRequestConnection(request);
        return;
    }

    auto decoder = IPC::Decoder::create({ data, dataSize }, { });
    if (!decoder) {
        RELEASE_LOG_ERROR(Push, "WebPushDaemon::connectionEventHandler - Failed to create decoder for xpc message");
        tryCloseRequestConnection(request);
        return;
    }

    auto xpcConnection = OSObjectPtr { xpc_dictionary_get_remote_connection(request) };
    if (!xpcConnection)
        return;

    if (m_pendingConnectionSet.contains(xpcConnection.get())) {
        m_pendingConnectionSet.remove(xpcConnection.get());

        RefPtr pushConnection = PushClientConnection::create(xpcConnection.get(), *decoder);
        if (!pushConnection) {
            RELEASE_LOG_ERROR(Push, "WebPushDaemon::connectionEventHandler - Could not initialize PushClientConnection");
            tryCloseRequestConnection(request);
            return;
        }

        m_connectionMap.set(xpcConnection.get(), *pushConnection);
        return;
    }

    auto pushConnection = m_connectionMap.get(xpcConnection.get());
    if (!pushConnection) {
        RELEASE_LOG_ERROR(Push, "WebPushDaemon::connectionEventHandler - Could not find a PushClientConnection mapped to this xpc request");
        tryCloseRequestConnection(request);
        return;
    }

    auto reply = adoptOSObject(xpc_dictionary_create_reply(request));
    auto replyHandler = [xpcConnection = WTFMove(xpcConnection), reply = WTFMove(reply)] (UniqueRef<IPC::Encoder>&& encoder) {
        RELEASE_ASSERT(RunLoop::isMain());
        auto xpcData = WebKit::encoderToXPCData(WTFMove(encoder));
        xpc_dictionary_set_uint64(reply.get(), WebKit::WebPushD::protocolVersionKey, WebKit::WebPushD::protocolVersionValue);
        xpc_dictionary_set_value(reply.get(), protocolEncodedMessageKey, xpcData.get());

        xpc_connection_send_message(xpcConnection.get(), reply.get());
    };

    pushConnection->didReceiveMessageWithReplyHandler(*decoder, WTFMove(replyHandler));
}

void WebPushDaemon::connectionAdded(xpc_connection_t connection)
{
    RELEASE_ASSERT(!m_pendingConnectionSet.contains(connection));
    RELEASE_ASSERT(!m_connectionMap.contains(connection));

    m_pendingConnectionSet.add(connection);
}

void WebPushDaemon::connectionRemoved(xpc_connection_t connection)
{
    if (m_pendingConnectionSet.contains(connection)) {
        m_pendingConnectionSet.remove(connection);
        return;
    }

    auto clientConnection = m_connectionMap.take(connection);
    clientConnection->connectionClosed();
}

void WebPushDaemon::setPushAndNotificationsEnabledForOrigin(PushClientConnection& connection, const String& originString, bool enabled, CompletionHandler<void()>&& replySender)
{
    runAfterStartingPushService([this, identifier = connection.subscriptionSetIdentifier(), originString, enabled, replySender = WTFMove(replySender)]() mutable {
        if (!m_pushService) {
            replySender();
            return;
        }

        m_pushService->setPushesEnabledForSubscriptionSetAndOrigin(identifier, originString, enabled, WTFMove(replySender));
    });
}

void WebPushDaemon::injectPushMessageForTesting(PushClientConnection& connection, PushMessageForTesting&& message, CompletionHandler<void(const String&)>&& replySender)
{
    auto checkConnectionAndInjectedMessage = [&]() -> ASCIILiteral {
        if (!connection.hostAppHasPushInjectEntitlement())
            return "Attempting to inject a push message from an unentitled process"_s;
        if (message.targetAppCodeSigningIdentifier.isEmpty() || !message.registrationURL.isValid())
            return "Attempting to inject an invalid push message"_s;
        return nullptr;
    };
    if (auto errorMessage = checkConnectionAndInjectedMessage()) {
        WEBPUSHDAEMON_RELEASE_LOG_ERROR(Push, "%{public}s", errorMessage.characters());
        return replySender(errorMessage);
    }

#if ENABLE(DECLARATIVE_WEB_PUSH)
    // Validate a Notification disposition message now by trying to parse it
    // Only for the testing/development push service for now
    if (m_usingMockPushService) {
        if (message.disposition == PushMessageDisposition::Notification) {
            auto exceptionOrPayload = WebCore::NotificationPayload::parseJSON(message.payload);
            if (exceptionOrPayload.hasException()) {
                replySender(exceptionOrPayload.exception().message());
                return;
            }

            message.parsedPayload = exceptionOrPayload.returnValue();
        }
    }
#endif // ENABLE(DECLARATIVE_WEB_PUSH)

    PushSubscriptionSetIdentifier identifier { .bundleIdentifier = message.targetAppCodeSigningIdentifier, .pushPartition = message.pushPartitionString, .dataStoreIdentifier = connection.dataStoreIdentifier() };
    auto addResult = m_pushMessages.ensure(identifier, [] {
        return Deque<WebKit::WebPushMessage> { };
    });
    auto data = message.payload.utf8();
#if ENABLE(DECLARATIVE_WEB_PUSH)
    WebKit::WebPushMessage pushMessage { Vector(data.span()), message.pushPartitionString, message.registrationURL, WTFMove(message.parsedPayload) };
#else
    WebKit::WebPushMessage pushMessage { Vector(data.span()), message.pushPartitionString, message.registrationURL, { } };
#endif
    addResult.iterator->value.append(pushMessage);

    WEBPUSHDAEMON_RELEASE_LOG(Push, "Injected a test push message for %{public}s at %{public}s with %zu pending messages, payload: %{public}s", message.targetAppCodeSigningIdentifier.utf8().data(), message.registrationURL.string().utf8().data(), addResult.iterator->value.size(), message.payload.utf8().data());

    notifyClientPushMessageIsAvailable(identifier);

    replySender({ });
}

void WebPushDaemon::injectEncryptedPushMessageForTesting(PushClientConnection& connection, const String& message, CompletionHandler<void(bool)>&& replySender)
{
    if (!connection.hostAppHasPushInjectEntitlement()) {
        WEBPUSHDAEMON_RELEASE_LOG_ERROR(Push, "Attempting to inject a push message from an unentitled process");
        return replySender(false);
    }

    runAfterStartingPushService([this, message = message, replySender = WTFMove(replySender)]() mutable {
        if (!m_pushService)
            return replySender(false);

        auto bytes = message.utf8();
        RetainPtr data = toNSData(bytes.span());

        id obj = [NSJSONSerialization JSONObjectWithData:data.get() options:0 error:nullptr];
        if (!obj || ![obj isKindOfClass:[NSDictionary class]])
            return replySender(false);

        m_pushService->didReceivePushMessage(obj[@"topic"], obj[@"userInfo"], [replySender = WTFMove(replySender)]() mutable {
            replySender(true);
        });
    });
}

void WebPushDaemon::handleIncomingPush(const PushSubscriptionSetIdentifier& identifier, WebKit::WebPushMessage&& message)
{
    ensureIncomingPushTransaction();

    auto addResult = m_pushMessages.ensure(identifier, [] {
        return Deque<WebKit::WebPushMessage> { };
    });
    addResult.iterator->value.append(WTFMove(message));

    notifyClientPushMessageIsAvailable(identifier);
}

void WebPushDaemon::notifyClientPushMessageIsAvailable(const WebCore::PushSubscriptionSetIdentifier& subscriptionSetIdentifier)
{
    const auto& bundleIdentifier = subscriptionSetIdentifier.bundleIdentifier;
    RELEASE_LOG(Push, "Launching %{public}s in response to push for %{public}s", bundleIdentifier.utf8().data(), subscriptionSetIdentifier.debugDescription().utf8().data());

#if PLATFORM(MAC)
    CFArrayRef urls = (__bridge CFArrayRef)@[ [NSURL URLWithString:@"x-webkit-app-launch://1"] ];
    CFStringRef identifier = (__bridge CFStringRef)((NSString *)bundleIdentifier);

    CFDictionaryRef options = (__bridge CFDictionaryRef)@{
        (id)_kLSOpenOptionPreferRunningInstanceKey: @(kLSOpenRunningInstanceBehaviorUseRunningProcess),
        (id)_kLSOpenOptionActivateKey: @NO,
        (id)_kLSOpenOptionAddToRecentsKey: @NO,
        (id)_kLSOpenOptionBackgroundLaunchKey: @YES,
        (id)_kLSOpenOptionHideKey: @YES,
    };

    _LSOpenURLsUsingBundleIdentifierWithCompletionHandler(urls, identifier, options, ^(LSASNRef, Boolean, CFErrorRef cfError) {
        RELEASE_LOG_ERROR_IF(cfError, Push, "Failed to launch process in response to push: %{public}@", (__bridge NSError *)cfError);
    });
#elif PLATFORM(IOS) || PLATFORM(VISION)
    const NSString *URLPrefix = @"webapp://web-push/";
    NSURL *launchURL = [NSURL URLWithString:[URLPrefix stringByAppendingFormat:@"%@", (NSString *)subscriptionSetIdentifier.pushPartition]];

    NSDictionary *options = @{
        FBSOpenApplicationOptionKeyActivateForEvent: @{ FBSActivateForEventOptionTypeBackgroundContentFetching: @{ } },
        FBSOpenApplicationOptionKeyPayloadURL : launchURL,
        FBSOpenApplicationOptionKeyPayloadOptions : @{ UIApplicationLaunchOptionsSourceApplicationKey : @"com.apple.WebKit.webpushd" },
    };

    _LSOpenConfiguration *configuration = [[_LSOpenConfiguration alloc] init];
    configuration.sensitive = YES;
    configuration.frontBoardOptions = options;
    configuration.allowURLOverrides = NO;

    [[LSApplicationWorkspace defaultWorkspace] openURL:launchURL configuration:configuration completionHandler:^(NSDictionary<NSString *, id> *result, NSError *error) {
        if (error)
            RELEASE_LOG_ERROR(Push, "Failed to open app to handle push: %{public}@", error);
    }];
#endif
}

Seconds WebPushDaemon::silentPushTimeout() const
{
    return m_usingMockPushService ? silentPushTimeoutForTesting : silentPushTimeoutForProduction;
}

// This only needs to be called if the first entry in m_potentialSilentPushes was changed or removed.
void WebPushDaemon::rescheduleSilentPushTimer()
{
    if (m_potentialSilentPushes.empty()) {
        m_silentPushTimer.stop();
        m_silentPushTransaction = nullptr;
        return;
    }

    auto expirationTime = m_potentialSilentPushes.front().expirationTime;
    auto delay = std::max(expirationTime - MonotonicTime::now(), 0_s);
    m_silentPushTimer.startOneShot(delay);

    if (!m_silentPushTransaction)
        m_silentPushTransaction = adoptOSObject(os_transaction_create("com.apple.webkit.webpushd.daemon.awaiting-show-notification"));
}

void WebPushDaemon::silentPushTimerFired()
{
    auto now = MonotonicTime::now();
    for (auto it = m_potentialSilentPushes.begin(); it != m_potentialSilentPushes.end();) {
        if (it->expirationTime > now)
            break;

        auto origin = WebCore::SecurityOriginData::fromURL(URL { it->scope }).toString();
        m_pushService->incrementSilentPushCount(it->identifier, origin, [identifier = it->identifier, origin](unsigned newSilentPushCount) {
            RELEASE_LOG(Push, "showNotification not called in time for %{public}s (origin = %{sensitive}s), silent push count is now %u", identifier.debugDescription().utf8().data(), origin.utf8().data(), newSilentPushCount);
        });
        it = m_potentialSilentPushes.erase(it);
    }

    rescheduleSilentPushTimer();
}

void WebPushDaemon::didShowNotification(const WebCore::PushSubscriptionSetIdentifier& identifier, const String& scope)
{
    auto now = MonotonicTime::now();
    bool removedFirst = false;

    for (auto it = m_potentialSilentPushes.begin(); it != m_potentialSilentPushes.end();) {
        if (it->identifier == identifier && it->scope == scope && it->expirationTime > now) {
            RELEASE_LOG(Push, "showNotification called in time for %{public}s (origin = %{sensitive}s)", it->identifier.debugDescription().utf8().data(), it->scope.utf8().data());
            removedFirst = (it == m_potentialSilentPushes.begin());
            it = m_potentialSilentPushes.erase(it);
            break;
        }
        ++it;
    }

    if (removedFirst)
        rescheduleSilentPushTimer();
}

void WebPushDaemon::getPendingPushMessage(PushClientConnection& connection, CompletionHandler<void(const std::optional<WebKit::WebPushMessage>&)>&& replySender)
{
    auto it = m_pushMessages.find(connection.subscriptionSetIdentifier());
    if (it == m_pushMessages.end()) {
        WEBPUSHDAEMON_RELEASE_LOG(Push, "No pending push message");
        return replySender(std::nullopt);
    }

    auto& pushMessages = it->value;
    WebKit::WebPushMessage result = pushMessages.takeFirst();
    size_t remainingMessageCount = pushMessages.size();
    if (!remainingMessageCount)
        m_pushMessages.remove(it);

    m_potentialSilentPushes.push_back(PotentialSilentPush { connection.subscriptionSetIdentifier(), result.registrationURL.string(), MonotonicTime::now() + silentPushTimeout() });
    if (m_potentialSilentPushes.size() == 1)
        rescheduleSilentPushTimer();

    WEBPUSHDAEMON_RELEASE_LOG(Push, "Fetched 1 push message, %zu remaining", remainingMessageCount);
    replySender(WTFMove(result));

    if (m_pushMessages.isEmpty())
        releaseIncomingPushTransaction();
}

void WebPushDaemon::getPendingPushMessages(PushClientConnection& connection, CompletionHandler<void(const Vector<WebKit::WebPushMessage>&)>&& replySender)
{
    auto it = m_pushMessages.find(connection.subscriptionSetIdentifier());
    if (it == m_pushMessages.end()) {
        WEBPUSHDAEMON_RELEASE_LOG(Push, "No pending push messages");
        return replySender({ });
    }
    auto messages = m_pushMessages.take(it);

    Vector<WebKit::WebPushMessage> result;
    result.reserveCapacity(messages.size());
    while (!messages.isEmpty())
        result.append(messages.takeFirst());

    WEBPUSHDAEMON_RELEASE_LOG(Push, "Fetched %zu pending push messages", result.size());
    replySender(WTFMove(result));
    
    if (m_pushMessages.isEmpty())
        releaseIncomingPushTransaction();
}

void WebPushDaemon::getPushTopicsForTesting(PushClientConnection& connection, CompletionHandler<void(Vector<String>, Vector<String>)>&& completionHandler)
{
    if (!connection.hostAppHasPushInjectEntitlement()) {
        WEBPUSHDAEMON_RELEASE_LOG_ERROR(Push, "Need entitlement to get push topics");
        return completionHandler({ }, { });
    }

    runAfterStartingPushService([this, completionHandler = WTFMove(completionHandler)]() mutable {
        if (!m_pushService) {
            completionHandler({ }, { });
            return;
        }
        completionHandler(m_pushService->enabledTopics(), m_pushService->ignoredTopics());
    });
}

void WebPushDaemon::subscribeToPushService(PushClientConnection& connection, const URL& scopeURL, const Vector<uint8_t>& vapidPublicKey, CompletionHandler<void(const Expected<WebCore::PushSubscriptionData, WebCore::ExceptionData>&)>&& replySender)
{
#if PLATFORM(IOS)
    auto origin = WebCore::SecurityOriginData::fromURL(scopeURL);

    const auto& webClipIdentifier = connection.subscriptionSetIdentifier().pushPartition;
    RetainPtr webClip = [UIWebClip webClipWithIdentifier:(NSString *)webClipIdentifier];
    auto webClipOrigin = WebCore::SecurityOriginData::fromURL(URL { [webClip pageURL] });

    if (origin.isNull() || origin.isOpaque() || origin != webClipOrigin) {
        WEBPUSHDAEMON_RELEASE_LOG(Push, "Cannot subscribe because web clip origin %{sensitive}s does not match expected origin %{sensitive}s", webClipOrigin.toString().utf8().data(), origin.toString().utf8().data());
        return replySender(makeUnexpected(WebCore::ExceptionData { WebCore::ExceptionCode::InvalidStateError, "Unexpected service worker scope"_s }));
    }
#endif

#if PLATFORM(IOS) && HAVE(FULL_FEATURED_USER_NOTIFICATIONS)
    RetainPtr notificationCenterBundleIdentifier = platformNotificationCenterBundleIdentifier(connection);
    RetainPtr center = adoptNS([[m_userNotificationCenterClass alloc] initWithBundleIdentifier:notificationCenterBundleIdentifier.get()]);
    UNNotificationSettings *settings = [center notificationSettings];
    if (settings.authorizationStatus != UNAuthorizationStatusAuthorized) {
        WEBPUSHDAEMON_RELEASE_LOG(Push, "Cannot subscribe because web clip origin %{sensitive}s does not have correct permissions", origin.toString().utf8().data());
        return replySender(makeUnexpected(WebCore::ExceptionData { WebCore::ExceptionCode::NotAllowedError, "User denied push permission"_s }));
    }
#endif

    runAfterStartingPushService([this, identifier = connection.subscriptionSetIdentifier(), scope = scopeURL.string(), vapidPublicKey, replySender = WTFMove(replySender)]() mutable {
        if (!m_pushService) {
            replySender(makeUnexpected(WebCore::ExceptionData { WebCore::ExceptionCode::InvalidStateError, "Push service initialization failed"_s }));
            return;
        }

        m_pushService->subscribe(identifier, scope, vapidPublicKey, WTFMove(replySender));
    });
}

void WebPushDaemon::unsubscribeFromPushService(PushClientConnection& connection, const URL& scopeURL, std::optional<WebCore::PushSubscriptionIdentifier> subscriptionIdentifier, CompletionHandler<void(const Expected<bool, WebCore::ExceptionData>&)>&& replySender)
{
    runAfterStartingPushService([this, identifier = connection.subscriptionSetIdentifier(), scope = scopeURL.string(), subscriptionIdentifier, replySender = WTFMove(replySender)]() mutable {
        if (!m_pushService) {
            replySender(makeUnexpected(WebCore::ExceptionData { WebCore::ExceptionCode::InvalidStateError, "Push service initialization failed"_s }));
            return;
        }

        m_pushService->unsubscribe(identifier, scope, subscriptionIdentifier, WTFMove(replySender));
    });
}

void WebPushDaemon::getPushSubscription(PushClientConnection& connection, const URL& scopeURL, CompletionHandler<void(const Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>&)>&& replySender)
{
    runAfterStartingPushService([this, identifier = connection.subscriptionSetIdentifier(), scope = scopeURL.string(), replySender = WTFMove(replySender)]() mutable {
        if (!m_pushService) {
            replySender(makeUnexpected(WebCore::ExceptionData { WebCore::ExceptionCode::InvalidStateError, "Push service initialization failed"_s }));
            return;
        }

        m_pushService->getSubscription(identifier, scope, WTFMove(replySender));
    });
}

void WebPushDaemon::incrementSilentPushCount(PushClientConnection& connection, const WebCore::SecurityOriginData& securityOrigin, CompletionHandler<void(unsigned)>&& replySender)
{
    runAfterStartingPushService([this, identifier = connection.subscriptionSetIdentifier(), securityOrigin = securityOrigin.toString(), replySender = WTFMove(replySender)]() mutable {
        if (!m_pushService) {
            replySender(0);
            return;
        }

        m_pushService->incrementSilentPushCount(identifier, securityOrigin, WTFMove(replySender));
    });
}

void WebPushDaemon::removeAllPushSubscriptions(PushClientConnection& connection, CompletionHandler<void(unsigned)>&& replySender)
{
    runAfterStartingPushService([this, identifier = connection.subscriptionSetIdentifier(), replySender = WTFMove(replySender)]() mutable {
        if (!m_pushService) {
            replySender(0);
            return;
        }

        m_pushService->removeRecordsForSubscriptionSet(identifier, WTFMove(replySender));
    });
}

void WebPushDaemon::removePushSubscriptionsForOrigin(PushClientConnection& connection, const WebCore::SecurityOriginData& securityOrigin, CompletionHandler<void(unsigned)>&& replySender)
{
    runAfterStartingPushService([this, identifier = connection.subscriptionSetIdentifier(), securityOrigin = securityOrigin.toString(), replySender = WTFMove(replySender)]() mutable {
        if (!m_pushService) {
            replySender(0);
            return;
        }

        m_pushService->removeRecordsForSubscriptionSetAndOrigin(identifier, securityOrigin, WTFMove(replySender));
    });
}

void WebPushDaemon::setPublicTokenForTesting(PushClientConnection& connection, const String& publicToken, CompletionHandler<void()>&& replySender)
{
    if (!connection.hostAppHasPushInjectEntitlement()) {
        WEBPUSHDAEMON_RELEASE_LOG_ERROR(Push, "Need entitlement to set public token");
        return replySender();
    }

    runAfterStartingPushService([this, publicToken, replySender = WTFMove(replySender)]() mutable {
        if (!m_pushService) {
            replySender();
            return;
        }

        m_pushService->setPublicTokenForTesting(Vector<uint8_t> { publicToken.utf8().span() });
        replySender();
    });
}

PushClientConnection* WebPushDaemon::toPushClientConnection(xpc_connection_t connection)
{
    auto clientConnection = m_connectionMap.get(connection);
    RELEASE_ASSERT(clientConnection);
    return clientConnection;
}

#if HAVE(FULL_FEATURED_USER_NOTIFICATIONS)

void WebPushDaemon::showNotification(PushClientConnection& connection, const WebCore::NotificationData& notificationData, RefPtr<WebCore::NotificationResources> resources, CompletionHandler<void()>&& completionHandler)
{
    RetainPtr content = adoptNS([[UNMutableNotificationContent alloc] init]);

    [content setDefaultActionBundleIdentifier:platformDefaultActionBundleIdentifier()];

    content.get().targetContentIdentifier = (NSString *)connection.pushPartitionString();
    content.get().title = (NSString *)notificationData.title;
    content.get().body = (NSString *)notificationData.body;

    if (platformShouldPlaySound(notificationData))
        content.get().sound = [UNNotificationSound defaultSound];

    auto notificationCenterBundleIdentifier = platformNotificationCenterBundleIdentifier(connection);

#if HAVE(FULL_FEATURED_USER_NOTIFICATIONS)
    content.get().icon = [UNNotificationIcon iconForApplicationIdentifier:notificationCenterBundleIdentifier.get()];
#endif

    NSString *notificationSourceForDisplay = platformNotificationSourceForDisplay(connection);
    if (!notificationSourceForDisplay.length)
        notificationSourceForDisplay = (NSString *)notificationData.originString;

ALLOW_NONLITERAL_FORMAT_BEGIN
    content.get().subtitle = [NSString stringWithFormat:(NSString *)WEB_UI_STRING("from %@", "Web Push Notification string to indicate the name of the Web App/Web Site a notification was sent from, such as 'from Wikipedia'"), notificationSourceForDisplay];
ALLOW_NONLITERAL_FORMAT_END

    content.get().userInfo = notificationData.dictionaryRepresentation();

    UNNotificationRequest *request = [UNNotificationRequest requestWithIdentifier:(NSString *)notificationData.notificationID.toString() content:content.get() trigger:nil];
    RetainPtr center = adoptNS([[m_userNotificationCenterClass alloc] initWithBundleIdentifier:notificationCenterBundleIdentifier.get()]);
    if (!center)
        RELEASE_LOG_ERROR(Push, "Failed to instantiate UNUserNotificationCenter center");

    auto blockPtr = makeBlockPtr([this, identifier = crossThreadCopy(connection.subscriptionSetIdentifier()), scope = crossThreadCopy(notificationData.serviceWorkerRegistrationURL.string()), completionHandler = WTFMove(completionHandler)](NSError *error) mutable {
        WorkQueue::main().dispatch([this, identifier = crossThreadCopy(identifier), scope = crossThreadCopy(scope), error = RetainPtr { error }, completionHandler = WTFMove(completionHandler)] mutable {
            if (error)
                RELEASE_LOG_ERROR(Push, "Failed to add notification request: %{public}@", error.get());
            else
                didShowNotification(identifier, scope);
            completionHandler();
        });
    });

    [center addNotificationRequest:request withCompletionHandler:blockPtr.get()];
}

void WebPushDaemon::getNotifications(PushClientConnection& connection, const URL& registrationURL, const String& tag, CompletionHandler<void(Expected<Vector<WebCore::NotificationData>, WebCore::ExceptionData>&&)>&& completionHandler)
{
    auto placeholderBundleIdentifier = platformNotificationCenterBundleIdentifier(connection);
    RetainPtr center = adoptNS([[m_userNotificationCenterClass alloc] initWithBundleIdentifier:placeholderBundleIdentifier.get()]);

    auto blockPtr = makeBlockPtr([identifier = crossThreadCopy(connection.subscriptionSetIdentifier()), completionHandler = WTFMove(completionHandler)](NSArray<UNNotification *> *notifications) mutable {
        WorkQueue::main().dispatch([identifier = crossThreadCopy(identifier), notifications = RetainPtr { notifications }, completionHandler = WTFMove(completionHandler)] mutable {
            Vector<WebCore::NotificationData> notificationDatas;
            for (UNNotification *notification in notifications.get()) {
                auto notificationData = WebCore::NotificationData::fromDictionary(notification.request.content.userInfo);
                if (!notificationData) {
                    RELEASE_LOG_ERROR(Push, "WebPushDaemon::getNotifications error: skipping notification with invalid Notification userInfo for subscription %{public}s", identifier.debugDescription().utf8().data());
                    continue;
                }
                notificationDatas.append(*notificationData);
            }
            RELEASE_LOG(Push, "WebPushDaemon::getNotifications: returned %zu notifications for subscription %{public}s", notificationDatas.size(), identifier.debugDescription().utf8().data());
            completionHandler(notificationDatas);
        });
    });

    [center getDeliveredNotificationsWithCompletionHandler:blockPtr.get()];
}

void WebPushDaemon::cancelNotification(PushClientConnection& connection, const WTF::UUID& notificationID)
{
    auto placeholderBundleIdentifier = platformNotificationCenterBundleIdentifier(connection);
    RetainPtr center = adoptNS([[m_userNotificationCenterClass alloc] initWithBundleIdentifier:placeholderBundleIdentifier.get()]);

    auto identifiers = @[ (NSString *)notificationID.toString() ];
    [center removePendingNotificationRequestsWithIdentifiers:identifiers];
    [center removeDeliveredNotificationsWithIdentifiers:identifiers];
}

void WebPushDaemon::getPushPermissionState(PushClientConnection& connection, const WebCore::SecurityOriginData& origin, CompletionHandler<void(WebCore::PushPermissionState)>&& replySender)
{
    auto identifier = connection.subscriptionSetIdentifier();

#if PLATFORM(IOS)
    if (identifier.pushPartition.isEmpty()) {
        WEBPUSHDAEMON_RELEASE_LOG_ERROR(Push, "Denied push permission since no pushPartition specified");
        return replySender(WebCore::PushPermissionState::Denied);
    }

    const auto& webClipIdentifier = identifier.pushPartition;
    RetainPtr webClip = [UIWebClip webClipWithIdentifier:(NSString *)webClipIdentifier];
    auto webClipOrigin = WebCore::SecurityOriginData::fromURL(URL { [webClip pageURL] });

    if (origin.isNull() || origin.isOpaque() || origin != webClipOrigin) {
        WEBPUSHDAEMON_RELEASE_LOG(Push, "Denied push permission because web clip origin %{sensitive}s does not match expected origin %{sensitive}s", webClipOrigin.toString().utf8().data(), origin.toString().utf8().data());
        return replySender(WebCore::PushPermissionState::Denied);
    }
#endif

    RetainPtr notificationCenterBundleIdentifier = platformNotificationCenterBundleIdentifier(connection);
    RetainPtr center = adoptNS([[m_userNotificationCenterClass alloc] initWithBundleIdentifier:notificationCenterBundleIdentifier.get()]);

    auto blockPtr = makeBlockPtr([originString = crossThreadCopy(origin.toString()), replySender = WTFMove(replySender)](UNNotificationSettings *settings) mutable {
        auto permissionState = [](UNAuthorizationStatus status) {
            switch (status) {
            case UNAuthorizationStatusNotDetermined: return WebCore::PushPermissionState::Prompt;
            case UNAuthorizationStatusDenied: return WebCore::PushPermissionState::Denied;
            case UNAuthorizationStatusAuthorized: return WebCore::PushPermissionState::Granted;
            default: return WebCore::PushPermissionState::Prompt;
            }
        }(settings.authorizationStatus);
        RELEASE_LOG(Push, "getPushPermissionState for %{sensitive}s with result: %u", originString.utf8().data(), static_cast<unsigned>(permissionState));

        WorkQueue::main().dispatch([replySender = WTFMove(replySender), permissionState] mutable {
            replySender(permissionState);
        });
    });

    [center getNotificationSettingsWithCompletionHandler:blockPtr.get()];
}

void WebPushDaemon::requestPushPermission(PushClientConnection& connection, const WebCore::SecurityOriginData& origin, CompletionHandler<void(bool)>&& replySender)
{
    auto identifier = connection.subscriptionSetIdentifier();

#if PLATFORM(IOS)
    if (identifier.pushPartition.isEmpty()) {
        WEBPUSHDAEMON_RELEASE_LOG_ERROR(Push, "Denied push permission since no pushPartition specified");
        return replySender(false);
    }

    const auto& webClipIdentifier = identifier.pushPartition;
    RetainPtr webClip = [UIWebClip webClipWithIdentifier:(NSString *)webClipIdentifier];
    auto webClipOrigin = WebCore::SecurityOriginData::fromURL(URL { [webClip pageURL] });

    if (origin.isNull() || origin.isOpaque() || origin != webClipOrigin) {
        WEBPUSHDAEMON_RELEASE_LOG(Push, "Denied push permission because web clip origin %{sensitive}s does not match expected origin %{sensitive}s", webClipOrigin.toString().utf8().data(), origin.toString().utf8().data());
        return replySender(false);
    }
#endif

    RetainPtr notificationCenterBundleIdentifier = platformNotificationCenterBundleIdentifier(connection);
    RetainPtr center = adoptNS([[m_userNotificationCenterClass alloc] initWithBundleIdentifier:notificationCenterBundleIdentifier.get()]);
    UNAuthorizationOptions options = UNAuthorizationOptionBadge | UNAuthorizationOptionAlert | UNAuthorizationOptionSound;

    auto blockPtr = makeBlockPtr([originString = crossThreadCopy(origin.toString()), replySender = WTFMove(replySender)](BOOL granted, NSError *error) mutable {
        if (error)
            RELEASE_LOG_ERROR(Push, "Failed to request push permission for %{sensitive}s: %{public}@", originString.utf8().data(), error);
        else
            RELEASE_LOG(Push, "Requested push permission for %{sensitive}s with result: %d", originString.utf8().data(), granted);

        WorkQueue::main().dispatch([replySender = WTFMove(replySender), granted] mutable {
            replySender(granted);
        });
    });

    [center requestAuthorizationWithOptions:options completionHandler:blockPtr.get()];
}

void WebPushDaemon::setAppBadge(PushClientConnection& connection, WebCore::SecurityOriginData&& badgeOriginData, std::optional<uint64_t> appBadge)
{
    URL appPageURL;

#if PLATFORM(IOS)
    auto webClipIdentifier = connection.pushPartitionString();
    RetainPtr webClip = [UIWebClip webClipWithIdentifier:(NSString *)webClipIdentifier];
    appPageURL = [webClip pageURL];
#elif PLATFORM(MAC)
    // FIXME: Establish the app page URL for Mac apps here
#endif

    if (!appPageURL.isEmpty()) {
        auto badgeOrigin = badgeOriginData.securityOrigin();
        auto appOrigin = WebCore::SecurityOrigin::create(appPageURL);
        if (!badgeOrigin->isSameSiteAs(appOrigin.get()))
            return;
    }

#if PLATFORM(IOS)
    RetainPtr state = adoptNS([[UISApplicationState alloc] initWithBundleIdentifier:platformNotificationCenterBundleIdentifier(connection).get()]);
    state.get().badgeValue = appBadge ? [NSNumber numberWithUnsignedLongLong:*appBadge] : nil;
#elif PLATFORM(MAC)
    String bundleIdentifier = connection.pushPartitionString().isEmpty() ? connection.hostAppCodeSigningIdentifier() : connection.pushPartitionString();
    RetainPtr center = adoptNS([[m_userNotificationCenterClass alloc]  initWithBundleIdentifier:(NSString *)bundleIdentifier]);
    if (!center)
        return;

    UNMutableNotificationContent *content = [UNMutableNotificationContent new];
    content.badge = appBadge ? [NSNumber numberWithLongLong:*appBadge] : nil;
    UNNotificationRequest *request = [UNNotificationRequest requestWithIdentifier:NSUUID.UUID.UUIDString content:content trigger:nil];
    RetainPtr debugDescription = (NSString *)connection.subscriptionSetIdentifier().debugDescription();
    [center addNotificationRequest:request withCompletionHandler:^(NSError *error) {
        if (error) {
            RELEASE_LOG_ERROR(Push, "Error attempting to set badge count for web app %{public}@", debugDescription.get());
            return;
        }

        RELEASE_LOG(Push, "%{public}s badge count for web app %{public}@", appBadge ? "Set" : "Reset", debugDescription.get());
    }];
#endif // PLATFORM(MAC)
}

void WebPushDaemon::getAppBadgeForTesting(PushClientConnection& connection, CompletionHandler<void(std::optional<uint64_t>)>&& completionHandler)
{
#if PLATFORM(IOS)
    UNUSED_PARAM(connection);
    completionHandler(std::nullopt);
#else
    RELEASE_ASSERT(m_userNotificationCenterClass == _WKMockUserNotificationCenter.class);

    String bundleIdentifier = connection.pushPartitionString().isEmpty() ? connection.hostAppCodeSigningIdentifier() : connection.pushPartitionString();
    RetainPtr center = adoptNS([[_WKMockUserNotificationCenter alloc] initWithBundleIdentifier:(NSString *)bundleIdentifier]);
    NSNumber *centerBadge = [center getAppBadgeForTesting];

    if (centerBadge)
        completionHandler([centerBadge unsignedLongLongValue]);
    else
        completionHandler(std::nullopt);
#endif
}

#endif // HAVE(FULL_FEATURED_USER_NOTIFICATIONS)

} // namespace WebPushD

#endif // ENABLE(WEB_PUSH_NOTIFICATIONS)
