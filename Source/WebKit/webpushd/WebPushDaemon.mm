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
#import "MockAppBundleRegistry.h"

#import <wtf/CompletionHandler.h>
#import <wtf/HexNumber.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/Span.h>

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

FUNCTION(setDebugModeIsEnabled)
ARGUMENTS(bool)
END

FUNCTION(updateConnectionConfiguration)
ARGUMENTS(WebPushDaemonConnectionConfiguration)
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

    // FIXME: This will need platform-specific implementations for real world bundles once implemented.
    replySender({ });
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

ClientConnection* Daemon::toClientConnection(xpc_connection_t connection)
{
    auto clientConnection = m_connectionMap.get(connection);
    RELEASE_ASSERT(clientConnection);
    return clientConnection;
}

} // namespace WebPushD
