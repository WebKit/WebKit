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

#import <WebCore/NotificationData.h>
#import <WebCore/NotificationPayload.h>
#import <WebCore/SecurityOriginData.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <span>
#import <wtf/CompletionHandler.h>
#import <wtf/HexNumber.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/URL.h>
#import <wtf/WorkQueue.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/spi/darwin/XPCSPI.h>
#import <wtf/text/MakeString.h>

#if PLATFORM(IOS) || PLATFORM(VISION)
#import <UIKit/UIApplication.h>
#endif

#define WEBPUSHDAEMON_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%{public}s [connection=%p, app=%{public}s]: " fmt, __func__, &connection, connection.subscriptionSetIdentifier().debugDescription().utf8().data(), ##__VA_ARGS__)
#define WEBPUSHDAEMON_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%{public}s [connection=%p, app=%{public}s]: " fmt, __func__, &connection, connection.subscriptionSetIdentifier().debugDescription().utf8().data(), ##__VA_ARGS__)

using namespace WebKit::WebPushD;
using WebCore::PushSubscriptionSetIdentifier;

namespace WebPushD {

static constexpr Seconds s_incomingPushTransactionTimeout { 10_s };

WebPushDaemon& WebPushDaemon::singleton()
{
    static NeverDestroyed<WebPushDaemon> daemon;
    return daemon;
}

WebPushDaemon::WebPushDaemon()
    : m_incomingPushTransactionTimer { *this, &WebPushDaemon::incomingPushTransactionTimerFired }
    , m_silentPushTimer { *this, &WebPushDaemon::silentPushTimerFired }
{
}

void WebPushDaemon::startMockPushService()
{
    m_usingMockPushService = true;
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

    auto xpcConnection = OSObjectPtr { xpc_dictionary_get_remote_connection(request) };
    auto pushConnection = m_connectionMap.get(xpcConnection.get());
    if (!pushConnection) {
        RELEASE_LOG_ERROR(Push, "WebPushDaemon::connectionEventHandler - Could not find a PushClientConnection mapped to this xpc request");
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
        RELEASE_LOG_ERROR(Push, "WebPushDaemon::connectionEventHandler - Failed to create decoder for xpc messasge");
        tryCloseRequestConnection(request);
        return;
    }

    auto reply = adoptOSObject(xpc_dictionary_create_reply(request));
    auto replyHandler = [xpcConnection = WTFMove(xpcConnection), reply = WTFMove(reply)] (UniqueRef<IPC::Encoder>&& encoder) {
        auto xpcData = WebKit::encoderToXPCData(WTFMove(encoder));
        xpc_dictionary_set_uint64(reply.get(), WebKit::WebPushD::protocolVersionKey, WebKit::WebPushD::protocolVersionValue);
        xpc_dictionary_set_value(reply.get(), protocolEncodedMessageKey, xpcData.get());

        xpc_connection_send_message(xpcConnection.get(), reply.get());
    };

    pushConnection->didReceiveMessageWithReplyHandler(*decoder, WTFMove(replyHandler));
}

void WebPushDaemon::connectionAdded(xpc_connection_t connection)
{
    RELEASE_ASSERT(!m_connectionMap.contains(connection));
    m_connectionMap.set(connection, PushClientConnection::create(connection));
}

void WebPushDaemon::connectionRemoved(xpc_connection_t connection)
{
    RELEASE_ASSERT(m_connectionMap.contains(connection));
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

    PushSubscriptionSetIdentifier identifier { .bundleIdentifier = message.targetAppCodeSigningIdentifier, .pushPartition = message.pushPartitionString };
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

void WebPushDaemon::didShowNotificationImpl(const WebCore::PushSubscriptionSetIdentifier& identifier, const String& scope)
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
    auto hostAppCodeSigningIdentifier = connection.hostAppCodeSigningIdentifier();
    if (hostAppCodeSigningIdentifier.isEmpty())
        return replySender(std::nullopt);

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
    auto hostAppCodeSigningIdentifier = connection.hostAppCodeSigningIdentifier();
    if (hostAppCodeSigningIdentifier.isEmpty())
        return replySender({ });

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

void WebPushDaemon::didShowNotificationForTesting(PushClientConnection& connection, const URL& scopeURL, CompletionHandler<void()>&& replySender)
{
    runAfterStartingPushService([this, identifier = connection.subscriptionSetIdentifier(), scope = scopeURL.string(), replySender = WTFMove(replySender)]() mutable {
        if (!m_pushService)
            return replySender();

        didShowNotificationImpl(identifier, scope);
        return replySender();
    });
}

PushClientConnection* WebPushDaemon::toPushClientConnection(xpc_connection_t connection)
{
    auto clientConnection = m_connectionMap.get(connection);
    RELEASE_ASSERT(clientConnection);
    return clientConnection;
}

} // namespace WebPushD

#endif // ENABLE(WEB_PUSH_NOTIFICATIONS)
