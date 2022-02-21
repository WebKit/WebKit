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
#import "WebPushDaemon.h"

#import "AppBundleRequest.h"
#import "DaemonDecoder.h"
#import "DaemonEncoder.h"
#import "DaemonUtilities.h"
#import "HandleMessage.h"
#import "ICAppBundle.h"
#import "MockAppBundleRegistry.h"

#import <WebCore/PushPermissionState.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <wtf/CompletionHandler.h>
#import <wtf/HexNumber.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/Span.h>
#import <wtf/URL.h>
#import <wtf/WorkQueue.h>

using namespace WebKit::WebPushD;

namespace WebPushD {

namespace MessageInfo {

#define FUNCTION(mf) struct mf { static constexpr auto MemberFunction = &WebPushD::Daemon::mf;
#define ARGUMENTS(...) using ArgsTuple = std::tuple<__VA_ARGS__>;
#define REPLY(...) using Reply = CompletionHandler<void(__VA_ARGS__)>; \
    static WebPushD::EncodedMessage encodeReply(__VA_ARGS__);
#define END };

FUNCTION(echoTwice)
ARGUMENTS(String)
REPLY(String)
END

FUNCTION(getOriginsWithPushAndNotificationPermissions)
ARGUMENTS()
REPLY(const Vector<String>&)
END

FUNCTION(deletePushAndNotificationRegistration)
ARGUMENTS(String)
REPLY(String)
END

FUNCTION(requestSystemNotificationPermission)
ARGUMENTS(String)
REPLY(bool)
END

FUNCTION(getPendingPushMessages)
ARGUMENTS()
REPLY(const Vector<WebKit::WebPushMessage>&)
END

FUNCTION(setDebugModeIsEnabled)
ARGUMENTS(bool)
END

FUNCTION(updateConnectionConfiguration)
ARGUMENTS(WebPushDaemonConnectionConfiguration)
END

FUNCTION(injectPushMessageForTesting)
ARGUMENTS(PushMessageForTesting)
REPLY(bool)
END

FUNCTION(injectEncryptedPushMessageForTesting)
ARGUMENTS(String)
REPLY(bool)
END

FUNCTION(subscribeToPushService)
ARGUMENTS(URL, Vector<uint8_t>)
REPLY(const Expected<WebCore::PushSubscriptionData, WebCore::ExceptionData>&)
END

FUNCTION(unsubscribeFromPushService)
ARGUMENTS(URL, WebCore::PushSubscriptionIdentifier)
REPLY(const Expected<bool, WebCore::ExceptionData>&)
END

FUNCTION(getPushSubscription)
ARGUMENTS(URL)
REPLY(const Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>&)
END

FUNCTION(getPushPermissionState)
ARGUMENTS(URL)
REPLY(const Expected<uint8_t, WebCore::ExceptionData>&)
END

#undef FUNCTION
#undef ARGUMENTS
#undef REPLY
#undef END

WebPushD::EncodedMessage echoTwice::encodeReply(String reply)
{
    WebKit::Daemon::Encoder encoder;
    encoder << reply;
    return encoder.takeBuffer();
}

WebPushD::EncodedMessage getOriginsWithPushAndNotificationPermissions::encodeReply(const Vector<String>& reply)
{
    WebKit::Daemon::Encoder encoder;
    encoder << reply;
    return encoder.takeBuffer();
}

WebPushD::EncodedMessage deletePushAndNotificationRegistration::encodeReply(String reply)
{
    WebKit::Daemon::Encoder encoder;
    encoder << reply;
    return encoder.takeBuffer();
}

WebPushD::EncodedMessage requestSystemNotificationPermission::encodeReply(bool reply)
{
    WebKit::Daemon::Encoder encoder;
    encoder << reply;
    return encoder.takeBuffer();
}

WebPushD::EncodedMessage injectPushMessageForTesting::encodeReply(bool reply)
{
    WebKit::Daemon::Encoder encoder;
    encoder << reply;
    return encoder.takeBuffer();
}

WebPushD::EncodedMessage injectEncryptedPushMessageForTesting::encodeReply(bool reply)
{
    WebKit::Daemon::Encoder encoder;
    encoder << reply;
    return encoder.takeBuffer();
}

WebPushD::EncodedMessage getPendingPushMessages::encodeReply(const Vector<WebKit::WebPushMessage>& reply)
{
    WebKit::Daemon::Encoder encoder;
    encoder << reply;
    return encoder.takeBuffer();
}

WebPushD::EncodedMessage subscribeToPushService::encodeReply(const Expected<WebCore::PushSubscriptionData, WebCore::ExceptionData>& reply)
{
    WebKit::Daemon::Encoder encoder;
    encoder << reply;
    return encoder.takeBuffer();
}

WebPushD::EncodedMessage unsubscribeFromPushService::encodeReply(const Expected<bool, WebCore::ExceptionData>& reply)
{
    WebKit::Daemon::Encoder encoder;
    encoder << reply;
    return encoder.takeBuffer();
}

WebPushD::EncodedMessage getPushSubscription::encodeReply(const Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>& reply)
{
    WebKit::Daemon::Encoder encoder;
    encoder << reply;
    return encoder.takeBuffer();
}

WebPushD::EncodedMessage getPushPermissionState::encodeReply(const Expected<uint8_t, WebCore::ExceptionData>& reply)
{
    WebKit::Daemon::Encoder encoder;
    encoder << reply;
    return encoder.takeBuffer();
}

} // namespace MessageInfo

template<typename Info>
void handleWebPushDMessageWithReply(ClientConnection* connection, Span<const uint8_t> encodedMessage, CompletionHandler<void(WebPushD::EncodedMessage&&)>&& replySender)
{
    WebKit::Daemon::Decoder decoder(encodedMessage);

    std::optional<typename Info::ArgsTuple> arguments;
    decoder >> arguments;
    if (UNLIKELY(!arguments))
        return;

    typename Info::Reply completionHandler { [replySender = WTFMove(replySender)] (auto&&... args) mutable {
        replySender(Info::encodeReply(args...));
    } };

    IPC::callMemberFunction(tuple_cat(std::make_tuple(connection), WTFMove(*arguments)), WTFMove(completionHandler), &WebPushD::Daemon::singleton(), Info::MemberFunction);
}

template<typename Info>
void handleWebPushDMessage(ClientConnection* connection, Span<const uint8_t> encodedMessage)
{
    WebKit::Daemon::Decoder decoder(encodedMessage);

    std::optional<typename Info::ArgsTuple> arguments;
    decoder >> arguments;
    if (UNLIKELY(!arguments))
        return;

    IPC::callMemberFunction(tuple_cat(std::make_tuple(connection), WTFMove(*arguments)), &WebPushD::Daemon::singleton(), Info::MemberFunction);
}

Daemon& Daemon::singleton()
{
    static NeverDestroyed<Daemon> daemon;
    return daemon;
}

void Daemon::startMockPushService()
{
    auto messageHandler = [this](const String& bundleIdentifier, WebKit::WebPushMessage&& message) {
        handleIncomingPush(bundleIdentifier, WTFMove(message));
    };
    PushService::createMockService(WTFMove(messageHandler), [this](auto&& pushService) mutable {
        setPushService(WTFMove(pushService));
    });
}

void Daemon::startPushService(const String& incomingPushServiceName, const String& databasePath)
{
    auto messageHandler = [this](const String& bundleIdentifier, WebKit::WebPushMessage&& message) {
        handleIncomingPush(bundleIdentifier, WTFMove(message));
    };
    PushService::create(incomingPushServiceName, databasePath, WTFMove(messageHandler), [this](auto&& pushService) mutable {
        setPushService(WTFMove(pushService));
    });
}

void Daemon::setPushService(std::unique_ptr<PushService>&& pushService)
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

void Daemon::runAfterStartingPushService(Function<void()>&& function)
{
    if (!m_pushServiceStarted) {
        m_pendingPushServiceFunctions.append(WTFMove(function));
        return;
    }
    function();
}

void Daemon::broadcastDebugMessage(JSC::MessageLevel messageLevel, const String& message)
{
    auto dictionary = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
    xpc_dictionary_set_uint64(dictionary.get(), protocolDebugMessageLevelKey, static_cast<uint64_t>(messageLevel));
    xpc_dictionary_set_string(dictionary.get(), protocolDebugMessageKey, message.utf8().data());
    for (auto& iterator : m_connectionMap) {
        if (iterator.value->debugModeIsEnabled())
            xpc_connection_send_message(iterator.key, dictionary.get());
    }
}

void Daemon::broadcastAllConnectionIdentities()
{
    broadcastDebugMessage((JSC::MessageLevel)4, "===\nCurrent connections:");

    auto connections = copyToVector(m_connectionMap.values());
    std::sort(connections.begin(), connections.end(), [] (const Ref<ClientConnection>& a, const Ref<ClientConnection>& b) {
        return a->identifier() < b->identifier();
    });

    for (auto& iterator : connections)
        iterator->broadcastDebugMessage("");
    broadcastDebugMessage((JSC::MessageLevel)4, "===");
}

void Daemon::connectionEventHandler(xpc_object_t request)
{
    if (xpc_get_type(request) != XPC_TYPE_DICTIONARY)
        return;
    
    if (xpc_dictionary_get_uint64(request, protocolVersionKey) != protocolVersionValue) {
        NSLog(@"Received request that was not the current protocol version");
        // FIXME: Cut off this connection
        return;
    }

    auto messageType { static_cast<MessageType>(xpc_dictionary_get_uint64(request, protocolMessageTypeKey)) };
    size_t dataSize { 0 };
    const void* data = xpc_dictionary_get_data(request, protocolEncodedMessageKey, &dataSize);
    Span<const uint8_t> encodedMessage { static_cast<const uint8_t*>(data), dataSize };
    
    decodeAndHandleMessage(xpc_dictionary_get_remote_connection(request), messageType, encodedMessage, createReplySender(messageType, request));
}

void Daemon::connectionAdded(xpc_connection_t connection)
{
    broadcastDebugMessage((JSC::MessageLevel)0, makeString("New connection: 0x", hex(reinterpret_cast<uint64_t>(connection), WTF::HexConversionMode::Lowercase)));

    RELEASE_ASSERT(!m_connectionMap.contains(connection));
    m_connectionMap.set(connection, ClientConnection::create(connection));
}

void Daemon::connectionRemoved(xpc_connection_t connection)
{
    RELEASE_ASSERT(m_connectionMap.contains(connection));
    auto clientConnection = m_connectionMap.take(connection);
    clientConnection->connectionClosed();
}

CompletionHandler<void(EncodedMessage&&)> Daemon::createReplySender(MessageType messageType, OSObjectPtr<xpc_object_t>&& request)
{
    if (!messageTypeSendsReply(messageType))
        return nullptr;

    return [request = WTFMove(request)] (EncodedMessage&& message) {
        auto reply = adoptNS(xpc_dictionary_create_reply(request.get()));
        ASSERT(xpc_get_type(reply.get()) == XPC_TYPE_DICTIONARY);
        xpc_dictionary_set_uint64(reply.get(), protocolVersionKey, protocolVersionValue);
        xpc_dictionary_set_value(reply.get(), protocolEncodedMessageKey, WebKit::vectorToXPCData(WTFMove(message)).get());
        xpc_connection_send_message(xpc_dictionary_get_remote_connection(request.get()), reply.get());
    };
}

void Daemon::decodeAndHandleMessage(xpc_connection_t connection, MessageType messageType, Span<const uint8_t> encodedMessage, CompletionHandler<void(EncodedMessage&&)>&& replySender)
{
    ASSERT(messageTypeSendsReply(messageType) == !!replySender);

    auto* clientConnection = toClientConnection(connection);

    switch (messageType) {
    case MessageType::EchoTwice:
        handleWebPushDMessageWithReply<MessageInfo::echoTwice>(clientConnection, encodedMessage, WTFMove(replySender));
        break;
    case MessageType::GetOriginsWithPushAndNotificationPermissions:
        handleWebPushDMessageWithReply<MessageInfo::getOriginsWithPushAndNotificationPermissions>(clientConnection, encodedMessage, WTFMove(replySender));
        break;
    case MessageType::DeletePushAndNotificationRegistration:
        handleWebPushDMessageWithReply<MessageInfo::deletePushAndNotificationRegistration>(clientConnection, encodedMessage, WTFMove(replySender));
        break;
    case MessageType::RequestSystemNotificationPermission:
        handleWebPushDMessageWithReply<MessageInfo::requestSystemNotificationPermission>(clientConnection, encodedMessage, WTFMove(replySender));
        break;
    case MessageType::SetDebugModeIsEnabled:
        handleWebPushDMessage<MessageInfo::setDebugModeIsEnabled>(clientConnection, encodedMessage);
        break;
    case MessageType::UpdateConnectionConfiguration:
        handleWebPushDMessage<MessageInfo::updateConnectionConfiguration>(clientConnection, encodedMessage);
        break;
    case MessageType::InjectPushMessageForTesting:
        handleWebPushDMessageWithReply<MessageInfo::injectPushMessageForTesting>(clientConnection, encodedMessage, WTFMove(replySender));
        break;
    case MessageType::InjectEncryptedPushMessageForTesting:
        handleWebPushDMessageWithReply<MessageInfo::injectEncryptedPushMessageForTesting>(clientConnection, encodedMessage, WTFMove(replySender));
        break;
    case MessageType::GetPendingPushMessages:
        handleWebPushDMessageWithReply<MessageInfo::getPendingPushMessages>(clientConnection, encodedMessage, WTFMove(replySender));
        break;
    case MessageType::SubscribeToPushService:
        handleWebPushDMessageWithReply<MessageInfo::subscribeToPushService>(clientConnection, encodedMessage, WTFMove(replySender));
        break;
    case MessageType::UnsubscribeFromPushService:
        handleWebPushDMessageWithReply<MessageInfo::unsubscribeFromPushService>(clientConnection, encodedMessage, WTFMove(replySender));
        break;
    case MessageType::GetPushSubscription:
        handleWebPushDMessageWithReply<MessageInfo::getPushSubscription>(clientConnection, encodedMessage, WTFMove(replySender));
        break;
    case MessageType::GetPushPermissionState:
        handleWebPushDMessageWithReply<MessageInfo::getPushPermissionState>(clientConnection, encodedMessage, WTFMove(replySender));
        break;
    }
}

void Daemon::echoTwice(ClientConnection*, const String& message, CompletionHandler<void(const String&)>&& replySender)
{
    replySender(makeString(message, message));
}

bool Daemon::canRegisterForNotifications(ClientConnection& connection)
{
    if (connection.hostAppCodeSigningIdentifier().isEmpty()) {
        NSLog(@"ClientConnection cannot interact with notifications: Unknown host application code signing identifier");
        return false;
    }

    return true;
}

void Daemon::requestSystemNotificationPermission(ClientConnection* connection, const String& originString, CompletionHandler<void(bool)>&& replySender)
{
    if (!canRegisterForNotifications(*connection)) {
        replySender(false);
        return;
    }

    connection->enqueueAppBundleRequest(makeUnique<AppBundlePermissionsRequest>(*connection, originString, WTFMove(replySender)));
}

void Daemon::getOriginsWithPushAndNotificationPermissions(ClientConnection* connection, CompletionHandler<void(const Vector<String>&)>&& replySender)
{
    if (!canRegisterForNotifications(*connection)) {
        replySender({ });
        return;
    }

    if (connection->useMockBundlesForTesting()) {
        replySender(MockAppBundleRegistry::singleton().getOriginsWithRegistrations(connection->hostAppCodeSigningIdentifier()));
        return;
    }

#if ENABLE(INSTALL_COORDINATION_BUNDLES)
    ICAppBundle::getOriginsWithRegistrations(*connection, WTFMove(replySender));
#else
    RELEASE_ASSERT_NOT_REACHED();
#endif
}

void Daemon::deletePushAndNotificationRegistration(ClientConnection* connection, const String& originString, CompletionHandler<void(const String&)>&& replySender)
{
    if (!canRegisterForNotifications(*connection)) {
        replySender("Could not delete push and notification registrations for connection: Unknown host application code signing identifier");
        return;
    }

    connection->enqueueAppBundleRequest(makeUnique<AppBundleDeletionRequest>(*connection, originString, WTFMove(replySender)));
}

void Daemon::setDebugModeIsEnabled(ClientConnection* clientConnection, bool enabled)
{
    clientConnection->setDebugModeIsEnabled(enabled);
}

void Daemon::updateConnectionConfiguration(ClientConnection* clientConnection, const WebPushDaemonConnectionConfiguration& configuration)
{
    clientConnection->updateConnectionConfiguration(configuration);
}

void Daemon::injectPushMessageForTesting(ClientConnection* connection, const PushMessageForTesting& message, CompletionHandler<void(bool)>&& replySender)
{
    if (!connection->hostAppHasPushInjectEntitlement()) {
        connection->broadcastDebugMessage("Attempting to inject a push message from an unentitled process");
        replySender(false);
        return;
    }

    if (message.targetAppCodeSigningIdentifier.isEmpty() || !message.registrationURL.isValid()) {
        connection->broadcastDebugMessage("Attempting to inject an invalid push message");
        replySender(false);
        return;
    }

    connection->broadcastDebugMessage(makeString("Injected a test push message for ", message.targetAppCodeSigningIdentifier, " at ", message.registrationURL.string()));
    connection->broadcastDebugMessage(message.message);

    auto addResult = m_testingPushMessages.ensure(message.targetAppCodeSigningIdentifier, [] {
        return Deque<PushMessageForTesting> { };
    });
    addResult.iterator->value.append(message);

    notifyClientPushMessageIsAvailable(message.targetAppCodeSigningIdentifier);

    replySender(true);
}

void Daemon::injectEncryptedPushMessageForTesting(ClientConnection* connection, const String& message, CompletionHandler<void(bool)>&& replySender)
{
    if (!connection->hostAppHasPushInjectEntitlement()) {
        connection->broadcastDebugMessage("Attempting to inject a push message from an unentitled process");
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
        NSLog(@"got obj: %@", obj);

        m_pushService->didReceivePushMessage(obj[@"topic"], obj[@"userInfo"], [replySender = WTFMove(replySender)]() mutable {
            replySender(true);
        });
    });
}

void Daemon::handleIncomingPush(const String& bundleIdentifier, WebKit::WebPushMessage&& message)
{
    auto addResult = m_pushMessages.ensure(bundleIdentifier, [] {
        return Vector<WebKit::WebPushMessage> { };
    });
    addResult.iterator->value.append(WTFMove(message));

    notifyClientPushMessageIsAvailable(bundleIdentifier);
}

void Daemon::notifyClientPushMessageIsAvailable(const String& clientCodeSigningIdentifier)
{
#if PLATFORM(MAC)
    CFArrayRef urls = (__bridge CFArrayRef)@[ [NSURL URLWithString:@"webkit-app-launch://1"] ];
    CFStringRef identifier = (__bridge CFStringRef)((NSString *)clientCodeSigningIdentifier);

    CFDictionaryRef options = (__bridge CFDictionaryRef)@{
        (id)_kLSOpenOptionPreferRunningInstanceKey: @(kLSOpenRunningInstanceBehaviorUseRunningProcess),
        (id)_kLSOpenOptionActivateKey: @NO,
        (id)_kLSOpenOptionAddToRecentsKey: @NO,
        (id)_kLSOpenOptionBackgroundLaunchKey: @YES,
        (id)_kLSOpenOptionHideKey: @YES,
    };

    _LSOpenURLsUsingBundleIdentifierWithCompletionHandler(urls, identifier, options, ^(LSASNRef newProcessSerialNumber, Boolean processWasLaunched, CFErrorRef cfError) { });
#else
    // FIXME: Figure out equivalent iOS code here
    UNUSED_PARAM(clientCodeSigningIdentifier);
#endif // PLATFORM(MAC)
}

void Daemon::getPendingPushMessages(ClientConnection* connection, CompletionHandler<void(const Vector<WebKit::WebPushMessage>&)>&& replySender)
{
    auto hostAppCodeSigningIdentifier = connection->hostAppCodeSigningIdentifier();
    if (hostAppCodeSigningIdentifier.isEmpty()) {
        replySender({ });
        return;
    }

    Vector<WebKit::WebPushMessage> resultMessages;

    if (auto iterator = m_pushMessages.find(hostAppCodeSigningIdentifier); iterator != m_pushMessages.end())
        std::swap(resultMessages, iterator->value);

    auto iterator = m_testingPushMessages.find(hostAppCodeSigningIdentifier);
    if (iterator != m_testingPushMessages.end()) {
        for (auto& message : iterator->value) {
            auto data = message.message.utf8();
            resultMessages.append(WebKit::WebPushMessage { Vector<uint8_t> { reinterpret_cast<const uint8_t*>(data.data()), data.length() }, message.registrationURL });
        }
        m_testingPushMessages.remove(iterator);
    }

    connection->broadcastDebugMessage(makeString("Fetching ", String::number(resultMessages.size()), " pending push messages"));

    replySender(WTFMove(resultMessages));
}

void Daemon::subscribeToPushService(ClientConnection* connection, const URL& scopeURL, const Vector<uint8_t>& vapidPublicKey, CompletionHandler<void(const Expected<WebCore::PushSubscriptionData, WebCore::ExceptionData>&)>&& replySender)
{
    runAfterStartingPushService([this, bundleIdentifier = connection->hostAppCodeSigningIdentifier(), scope = scopeURL.string(), vapidPublicKey, replySender = WTFMove(replySender)]() mutable {
        if (!m_pushService) {
            replySender(makeUnexpected(WebCore::ExceptionData { WebCore::InvalidStateError, "Push service initialization failed"_s }));
            return;
        }

        m_pushService->subscribe(bundleIdentifier, scope, vapidPublicKey, WTFMove(replySender));
    });
}

void Daemon::unsubscribeFromPushService(ClientConnection* connection, const URL& scopeURL, WebCore::PushSubscriptionIdentifier subscriptionIdentifier, CompletionHandler<void(const Expected<bool, WebCore::ExceptionData>&)>&& replySender)
{
    runAfterStartingPushService([this, bundleIdentifier = connection->hostAppCodeSigningIdentifier(), scope = scopeURL.string(), subscriptionIdentifier, replySender = WTFMove(replySender)]() mutable {
        if (!m_pushService) {
            replySender(makeUnexpected(WebCore::ExceptionData { WebCore::InvalidStateError, "Push service initialization failed"_s }));
            return;
        }

        m_pushService->unsubscribe(bundleIdentifier, scope, subscriptionIdentifier, WTFMove(replySender));
    });
}

void Daemon::getPushSubscription(ClientConnection* connection, const URL& scopeURL, CompletionHandler<void(const Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>&)>&& replySender)
{
    runAfterStartingPushService([this, bundleIdentifier = connection->hostAppCodeSigningIdentifier(), scope = scopeURL.string(), replySender = WTFMove(replySender)]() mutable {
        if (!m_pushService) {
            replySender(makeUnexpected(WebCore::ExceptionData { WebCore::InvalidStateError, "Push service initialization failed"_s }));
            return;
        }

        m_pushService->getSubscription(bundleIdentifier, scope, WTFMove(replySender));
    });
}

void Daemon::getPushPermissionState(ClientConnection* connection, const URL& scopeURL, CompletionHandler<void(const Expected<uint8_t, WebCore::ExceptionData>&)>&& replySender)
{
    // FIXME: This doesn't actually get called right now, since the permission is currently checked
    // in WebProcess. However, we've left this stub in for now because there is a chance that we
    // will move the permission check into webpushd when supporting other platforms.
    replySender(static_cast<uint8_t>(WebCore::PushPermissionState::Denied));
}

ClientConnection* Daemon::toClientConnection(xpc_connection_t connection)
{
    auto clientConnection = m_connectionMap.get(connection);
    RELEASE_ASSERT(clientConnection);
    return clientConnection;
}

} // namespace WebPushD
