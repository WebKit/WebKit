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
#if ENABLE(BUILT_IN_NOTIFICATIONS)

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
#import <wtf/spi/darwin/XPCSPI.h>

#if PLATFORM(IOS) || PLATFORM(VISION)
#import <UIKit/UIApplication.h>
#endif

using namespace WebKit::WebPushD;
using WebCore::PushSubscriptionSetIdentifier;

namespace WebPushD {

static constexpr Seconds s_incomingPushTransactionTimeout { 10_s };\

WebPushDaemon& WebPushDaemon::singleton()
{
    static NeverDestroyed<WebPushDaemon> daemon;
    return daemon;
}

WebPushDaemon::WebPushDaemon()
    : m_incomingPushTransactionTimer { *this, &WebPushDaemon::incomingPushTransactionTimerFired }
{
}

void WebPushDaemon::startMockPushService()
{
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

void WebPushDaemon::broadcastDebugMessage(const String& message)
{
    for (auto& iterator : m_connectionMap) {
        if (iterator.value->debugModeIsEnabled())
            iterator.value->sendDebugMessage(message);
    }
}

void WebPushDaemon::broadcastAllConnectionIdentities()
{
    broadcastDebugMessage("===\nCurrent connections:"_s);

    auto connections = copyToVector(m_connectionMap.values());
    std::sort(connections.begin(), connections.end(), [] (const Ref<PushClientConnection>& a, const Ref<PushClientConnection>& b) {
        return a->identifier() < b->identifier();
    });

    for (auto& iterator : connections)
        iterator->broadcastDebugMessage(""_s);
    broadcastDebugMessage("==="_s);
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
    broadcastDebugMessage(makeString("New connection: 0x", hex(reinterpret_cast<uint64_t>(connection), WTF::HexConversionMode::Lowercase)));

    RELEASE_ASSERT(!m_connectionMap.contains(connection));
    m_connectionMap.set(connection, PushClientConnection::create(connection));
}

void WebPushDaemon::connectionRemoved(xpc_connection_t connection)
{
    RELEASE_ASSERT(m_connectionMap.contains(connection));
    auto clientConnection = m_connectionMap.take(connection);
    clientConnection->connectionClosed();
}

void WebPushDaemon::deletePushRegistration(const PushSubscriptionSetIdentifier& identifier, const String& originString, CompletionHandler<void()>&& callback)
{
    runAfterStartingPushService([this, identifier, originString, callback = WTFMove(callback)]() mutable {
        if (!m_pushService) {
            callback();
            return;
        }

        m_pushService->removeRecordsForSubscriptionSetAndOrigin(identifier, originString, [callback = WTFMove(callback)](auto&&) mutable {
            callback();
        });
    });
}

void WebPushDaemon::setPushAndNotificationsEnabledForOrigin(PushClientConnection& connection, const String& originString, bool enabled, CompletionHandler<void()>&& replySender)
{
    if (!canRegisterForNotifications(connection)) {
        replySender();
        return;
    }

    runAfterStartingPushService([this, identifier = connection.subscriptionSetIdentifier(), originString, enabled, replySender = WTFMove(replySender)]() mutable {
        if (!m_pushService) {
            replySender();
            return;
        }

        m_pushService->setPushesEnabledForSubscriptionSetAndOrigin(identifier, originString, enabled, WTFMove(replySender));
    });
}

void WebPushDaemon::deletePushAndNotificationRegistration(PushClientConnection& connection, const String& originString, CompletionHandler<void(const String&)>&& replySender)
{
    if (!canRegisterForNotifications(connection)) {
        replySender("Could not delete push and notification registrations for connection: Unknown host application code signing identifier"_s);
        return;
    }

    deletePushRegistration(connection.subscriptionSetIdentifier(), originString, [replySender = WTFMove(replySender)]() mutable {
        replySender(emptyString());
    });
}

void WebPushDaemon::injectPushMessageForTesting(PushClientConnection& connection, PushMessageForTesting&& message, CompletionHandler<void(const String&)>&& replySender)
{
    if (!connection.hostAppHasPushInjectEntitlement()) {
        String error = "Attempting to inject a push message from an unentitled process"_s;
        connection.broadcastDebugMessage(error);
        replySender(error);
        return;
    }

    if (message.targetAppCodeSigningIdentifier.isEmpty() || !message.registrationURL.isValid()) {
        String error = "Attempting to inject an invalid push message"_s;
        connection.broadcastDebugMessage(error);
        replySender(error);
        return;
    }

#if ENABLE(DECLARATIVE_WEB_PUSH)
    // Validate a Notification disposition message now by trying to parse it
    // Only for the testing/development push service for now
    if (m_machServiceName == "org.webkit.webpushtestdaemon.service"_s) {
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

    auto addResult = m_testingPushMessages.ensure(message.targetAppCodeSigningIdentifier, [] {
        return Deque<PushMessageForTesting> { };
    });

    connection.broadcastDebugMessage(makeString("Injected a test push message for ", message.targetAppCodeSigningIdentifier, " at ", message.registrationURL.string(), ", there are now ", addResult.iterator->value.size() + 1, " pending messages"));
    connection.broadcastDebugMessage(message.payload);

    addResult.iterator->value.append(WTFMove(message));

    notifyClientPushMessageIsAvailable(PushSubscriptionSetIdentifier { .bundleIdentifier = message.targetAppCodeSigningIdentifier, .pushPartition = message.pushPartitionString });

    replySender({ });
}

void WebPushDaemon::injectEncryptedPushMessageForTesting(PushClientConnection& connection, const String& message, CompletionHandler<void(bool)>&& replySender)
{
    if (!connection.hostAppHasPushInjectEntitlement()) {
        connection.broadcastDebugMessage("Attempting to inject a push message from an unentitled process"_s);
        replySender(false);
        return;
    }

    runAfterStartingPushService([this, message = message, replySender = WTFMove(replySender)]() mutable {
        if (!m_pushService) {
            replySender(false);
            return;
        }

        auto bytes = message.utf8();
        RetainPtr<NSData> data = adoptNS([[NSData alloc] initWithBytes:bytes.data() length: bytes.length()]);

        id obj = [NSJSONSerialization JSONObjectWithData:data.get() options:0 error:nullptr];
        if (!obj || ![obj isKindOfClass:[NSDictionary class]]) {
            replySender(false);
            return;
        }

        m_pushService->didReceivePushMessage(obj[@"topic"], obj[@"userInfo"], [replySender = WTFMove(replySender)]() mutable {
            replySender(true);
        });
    });
}

void WebPushDaemon::handleIncomingPush(const PushSubscriptionSetIdentifier& identifier, WebKit::WebPushMessage&& message)
{
    ensureIncomingPushTransaction();

    auto addResult = m_pushMessages.ensure(identifier, [] {
        return Vector<WebKit::WebPushMessage> { };
    });
    addResult.iterator->value.append(WTFMove(message));

    notifyClientPushMessageIsAvailable(identifier);
}

void WebPushDaemon::notifyClientPushMessageIsAvailable(const WebCore::PushSubscriptionSetIdentifier& subscriptionSetIdentifier)
{
    const auto& bundleIdentifier = subscriptionSetIdentifier.bundleIdentifier;
    RELEASE_LOG(Push, "Launching %{public}s in response to push", bundleIdentifier.utf8().data());

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
            RELEASE_LOG_ERROR(Push, "Failed to open app to handle push");
    }];
#endif
}

void WebPushDaemon::getPendingPushMessages(PushClientConnection& connection, CompletionHandler<void(const Vector<WebKit::WebPushMessage>&)>&& replySender)
{
    auto hostAppCodeSigningIdentifier = connection.hostAppCodeSigningIdentifier();
    if (hostAppCodeSigningIdentifier.isEmpty()) {
        replySender({ });
        return;
    }

    Vector<WebKit::WebPushMessage> resultMessages;

    if (auto iterator = m_pushMessages.find(connection.subscriptionSetIdentifier()); iterator != m_pushMessages.end()) {
        resultMessages = WTFMove(iterator->value);
        m_pushMessages.remove(iterator);
    }

    auto iterator = m_testingPushMessages.find(hostAppCodeSigningIdentifier);
    if (iterator != m_testingPushMessages.end()) {
        for (auto& message : iterator->value) {
            auto data = message.payload.utf8();
#if ENABLE(DECLARATIVE_WEB_PUSH)
            resultMessages.append(WebKit::WebPushMessage { Vector<uint8_t> { reinterpret_cast<const uint8_t*>(data.data()), data.length() }, message.pushPartitionString, message.registrationURL, WTFMove(message.parsedPayload) });
#else
            resultMessages.append(WebKit::WebPushMessage { Vector<uint8_t> { reinterpret_cast<const uint8_t*>(data.data()), data.length() }, message.pushPartitionString, message.registrationURL, { } });
#endif
        }
        m_testingPushMessages.remove(iterator);
    }

    RELEASE_LOG(Push, "Fetched %zu pending push messages for %{public}s", resultMessages.size(), connection.subscriptionSetIdentifier().debugDescription().utf8().data());
    connection.broadcastDebugMessage(makeString("Fetching ", String::number(resultMessages.size()), " pending push messages"));

    replySender(WTFMove(resultMessages));
    
    if (m_pushMessages.isEmpty())
        releaseIncomingPushTransaction();
}

void WebPushDaemon::getPushTopicsForTesting(CompletionHandler<void(Vector<String>, Vector<String>)>&& completionHandler)
{
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
            replySender(makeUnexpected(WebCore::ExceptionData { WebCore::InvalidStateError, "Push service initialization failed"_s }));
            return;
        }

        m_pushService->subscribe(identifier, scope, vapidPublicKey, WTFMove(replySender));
    });
}

void WebPushDaemon::unsubscribeFromPushService(PushClientConnection& connection, const URL& scopeURL, std::optional<WebCore::PushSubscriptionIdentifier> subscriptionIdentifier, CompletionHandler<void(const Expected<bool, WebCore::ExceptionData>&)>&& replySender)
{
    runAfterStartingPushService([this, identifier = connection.subscriptionSetIdentifier(), scope = scopeURL.string(), subscriptionIdentifier, replySender = WTFMove(replySender)]() mutable {
        if (!m_pushService) {
            replySender(makeUnexpected(WebCore::ExceptionData { WebCore::InvalidStateError, "Push service initialization failed"_s }));
            return;
        }

        m_pushService->unsubscribe(identifier, scope, subscriptionIdentifier, WTFMove(replySender));
    });
}

void WebPushDaemon::getPushSubscription(PushClientConnection& connection, const URL& scopeURL, CompletionHandler<void(const Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>&)>&& replySender)
{
    runAfterStartingPushService([this, identifier = connection.subscriptionSetIdentifier(), scope = scopeURL.string(), replySender = WTFMove(replySender)]() mutable {
        if (!m_pushService) {
            replySender(makeUnexpected(WebCore::ExceptionData { WebCore::InvalidStateError, "Push service initialization failed"_s }));
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

void WebPushDaemon::setPublicTokenForTesting(const String& publicToken, CompletionHandler<void()>&& replySender)
{
    runAfterStartingPushService([this, publicToken, replySender = WTFMove(replySender)]() mutable {
        if (!m_pushService) {
            replySender();
            return;
        }

        m_pushService->setPublicTokenForTesting(Vector<uint8_t> { publicToken.utf8().bytes() });
        replySender();
    });
}

PushClientConnection* WebPushDaemon::toPushClientConnection(xpc_connection_t connection)
{
    auto clientConnection = m_connectionMap.get(connection);
    RELEASE_ASSERT(clientConnection);
    return clientConnection;
}

bool WebPushDaemon::canRegisterForNotifications(PushClientConnection& connection)
{
    if (connection.hostAppCodeSigningIdentifier().isEmpty()) {
        RELEASE_LOG_ERROR(Push, "PushClientConnection cannot interact with notifications: Unknown host application code signing identifier");
        return false;
    }

    return true;
}

} // namespace WebPushD

#endif // ENABLE(BUILT_IN_NOTIFICATIONS)
