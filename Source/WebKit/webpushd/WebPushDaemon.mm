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

#import "DaemonDecoder.h"
#import "DaemonEncoder.h"
#import "DaemonUtilities.h"
#import "HandleMessage.h"
#import "WebPushDaemonConstants.h"

#import <wtf/CompletionHandler.h>
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
void handleWebPushDMessageWithReply(Span<const uint8_t> encodedMessage, CompletionHandler<void(WebPushD::EncodedMessage&&)>&& replySender)
{
    WebKit::Daemon::Decoder decoder(encodedMessage);

    std::optional<typename Info::ArgsTuple> arguments;
    decoder >> arguments;
    if (UNLIKELY(!arguments))
        return;

    typename Info::Reply completionHandler { [replySender = WTFMove(replySender)] (auto&&... args) mutable {
        replySender(Info::encodeReply(args...));
    } };

    IPC::callMemberFunction(WTFMove(*arguments), WTFMove(completionHandler), &WebPushD::Daemon::singleton(), Info::MemberFunction);
}

Daemon& Daemon::singleton()
{
    static NeverDestroyed<Daemon> daemon;
    return daemon;
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
    
    decodeAndHandleMessage(messageType, encodedMessage, createReplySender(messageType, request));
}

void Daemon::connectionAdded(xpc_connection_t)
{
    // FIXME: Track connections
}

void Daemon::connectionRemoved(xpc_connection_t)
{
    // FIXME: Track connections
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

void Daemon::decodeAndHandleMessage(MessageType messageType, Span<const uint8_t> encodedMessage, CompletionHandler<void(EncodedMessage&&)>&& replySender)
{
    ASSERT(messageTypeSendsReply(messageType) == !!replySender);

    switch (messageType) {
    case MessageType::EchoTwice:
        handleWebPushDMessageWithReply<MessageInfo::echoTwice>(encodedMessage, WTFMove(replySender));
        break;
    case MessageType::GetOriginsWithPushAndNotificationPermissions:
        handleWebPushDMessageWithReply<MessageInfo::getOriginsWithPushAndNotificationPermissions>(encodedMessage, WTFMove(replySender));
        break;
    case MessageType::DeletePushAndNotificationRegistration:
        handleWebPushDMessageWithReply<MessageInfo::deletePushAndNotificationRegistration>(encodedMessage, WTFMove(replySender));
        break;
    case MessageType::RequestSystemNotificationPermission:
        handleWebPushDMessageWithReply<MessageInfo::requestSystemNotificationPermission>(encodedMessage, WTFMove(replySender));
        break;
    }
}

void Daemon::echoTwice(const String& message, CompletionHandler<void(const String&)>&& replySender)
{
    replySender(makeString(message, message));
}

void Daemon::requestSystemNotificationPermission(const String& originString, CompletionHandler<void(bool)>&& replySender)
{
    // FIXME: This is for an API testing checkpoint
    // Next step is actually perform a persistent permissions request on a per-platform basis
    m_inMemoryOriginStringsWithPermissionForTesting.add(originString);
    replySender(true);
}

void Daemon::getOriginsWithPushAndNotificationPermissions(CompletionHandler<void(const Vector<String>&)>&& replySender)
{
    // FIXME: This is for an API testing checkpoint
    // Next step is actually gather persistent permissions from the system on a per-platform basis
    replySender(copyToVector(m_inMemoryOriginStringsWithPermissionForTesting));
}

void Daemon::deletePushAndNotificationRegistration(const String& originString, CompletionHandler<void(const String&)>&& replySender)
{
    // FIXME: This is for an API testing checkpoint
    // Next step is actually delete any persistent permissions on a per-platform basis
    if (m_inMemoryOriginStringsWithPermissionForTesting.remove(originString))
        replySender("");
    else
        replySender(makeString("Origin ", originString, " not registered for push or notifications"));
}

} // namespace WebPushD
