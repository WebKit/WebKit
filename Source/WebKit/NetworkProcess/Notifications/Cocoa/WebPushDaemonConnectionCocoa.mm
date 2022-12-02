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
#import "WebPushDaemonConnection.h"

#if PLATFORM(COCOA) && ENABLE(BUILT_IN_NOTIFICATIONS)

#import "DaemonDecoder.h"
#import "DaemonEncoder.h"
#import "DaemonUtilities.h"
#import "HandleMessage.h"
#import "WebPushDaemonConstants.h"
#import <wtf/spi/darwin/XPCSPI.h>

namespace WebKit::WebPushD { 

using Daemon::EncodedMessage;

static void addVersionAndEncodedMessageToDictionary(Vector<uint8_t>&& message, xpc_object_t dictionary)
{
    ASSERT(xpc_get_type(dictionary) == XPC_TYPE_DICTIONARY);
    xpc_dictionary_set_uint64(dictionary, WebPushD::protocolVersionKey, WebPushD::protocolVersionValue);
    xpc_dictionary_set_value(dictionary, WebPushD::protocolEncodedMessageKey, vectorToXPCData(WTFMove(message)).get());
}

void Connection::newConnectionWasInitialized() const
{
    ASSERT(m_connection);
    if (networkSession().sessionID().isEphemeral())
        return;

    Daemon::Encoder encoder;
    encoder.encode(m_configuration);
    Daemon::Connection::send(dictionaryFromMessage(WebPushD::MessageType::UpdateConnectionConfiguration, encoder.takeBuffer()).get());
}

namespace MessageInfo {

#define FUNCTION(mf) struct mf { static constexpr auto MemberFunction = &WebKit::WebPushD::Connection::mf;
#define ARGUMENTS(...) using ArgsTuple = std::tuple<__VA_ARGS__>;
#define END };

FUNCTION(debugMessage)
ARGUMENTS(String)
END

} // namespace MessageInfo

template<typename Info>
void handleWebPushDaemonMessage(WebKit::WebPushD::Connection* connection, Span<const uint8_t> encodedMessage)
{
    WebKit::Daemon::Decoder decoder(encodedMessage);

    std::optional<typename Info::ArgsTuple> arguments;
    decoder >> arguments;
    if (UNLIKELY(!arguments))
        return;

    IPC::callMemberFunction(connection, Info::MemberFunction, WTFMove(*arguments));
}

void Connection::connectionReceivedEvent(xpc_object_t request)
{
    if (xpc_get_type(request) != XPC_TYPE_DICTIONARY)
        return;

    if (xpc_dictionary_get_uint64(request, protocolVersionKey) != protocolVersionValue) {
        RELEASE_LOG(Push, "Received request that was not the current protocol version");
        return;
    }

    auto messageType { static_cast<DaemonMessageType>(xpc_dictionary_get_uint64(request, protocolMessageTypeKey)) };
    size_t dataSize { 0 };
    const void* data = xpc_dictionary_get_data(request, protocolEncodedMessageKey, &dataSize);
    Span<const uint8_t> encodedMessage { static_cast<const uint8_t*>(data), dataSize };

    ASSERT(!daemonMessageTypeSendsReply(messageType));

    switch (messageType) {
    case DaemonMessageType::DebugMessage:
        handleWebPushDaemonMessage<MessageInfo::debugMessage>(this, encodedMessage);
        break;
    };
}

RetainPtr<xpc_object_t> Connection::dictionaryFromMessage(MessageType messageType, EncodedMessage&& message) const
{
    auto dictionary = adoptNS(xpc_dictionary_create(nullptr, nullptr, 0));
    addVersionAndEncodedMessageToDictionary(WTFMove(message), dictionary.get());
    xpc_dictionary_set_uint64(dictionary.get(), protocolMessageTypeKey, static_cast<uint64_t>(messageType));
    return dictionary;
}

} // namespace WebKit::WebPushD

#endif // PLATFORM(COCOA) && ENABLE(BUILT_IN_NOTIFICATIONS)
