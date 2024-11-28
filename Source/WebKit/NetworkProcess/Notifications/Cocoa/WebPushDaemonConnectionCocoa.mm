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
#import "WebPushDaemonConnection.h"

#if PLATFORM(COCOA) && ENABLE(WEB_PUSH_NOTIFICATIONS)

#import "DaemonUtilities.h"
#import "Decoder.h"
#import "HandleMessage.h"
#import "MessageSenderInlines.h"
#import "PushClientConnectionMessages.h"
#import "WebPushDaemonConstants.h"
#import <wtf/spi/darwin/XPCSPI.h>

namespace WebKit::WebPushD { 

void Connection::newConnectionWasInitialized() const
{
    ASSERT(m_connection);
    sendWithoutUsingIPCConnection(Messages::PushClientConnection::InitializeConnection(m_configuration));
}

static OSObjectPtr<xpc_object_t> messageDictionaryFromEncoder(UniqueRef<IPC::Encoder>&& encoder)
{
    auto xpcData = encoderToXPCData(WTFMove(encoder));
    auto dictionary = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
    xpc_dictionary_set_uint64(dictionary.get(), WebPushD::protocolVersionKey, WebPushD::protocolVersionValue);
    xpc_dictionary_set_value(dictionary.get(), WebPushD::protocolEncodedMessageKey, xpcData.get());

    return dictionary;
}

bool Connection::performSendWithoutUsingIPCConnection(UniqueRef<IPC::Encoder>&& encoder) const
{
    auto dictionary = messageDictionaryFromEncoder(WTFMove(encoder));
    Daemon::Connection::send(dictionary.get());

    return true;
}

bool Connection::performSendWithAsyncReplyWithoutUsingIPCConnection(UniqueRef<IPC::Encoder>&& encoder, CompletionHandler<void(IPC::Decoder*)>&& completionHandler) const
{
    auto dictionary = messageDictionaryFromEncoder(WTFMove(encoder));
    Daemon::Connection::sendWithReply(dictionary.get(), [completionHandler = WTFMove(completionHandler)] (xpc_object_t reply) mutable {
        if (xpc_get_type(reply) != XPC_TYPE_DICTIONARY)
            return completionHandler(nullptr);

        if (xpc_dictionary_get_uint64(reply, WebPushD::protocolVersionKey) != WebPushD::protocolVersionValue)
            return completionHandler(nullptr);

        auto data = xpc_dictionary_get_data_span(reply, WebPushD::protocolEncodedMessageKey);
        auto decoder = IPC::Decoder::create(data, { });

        completionHandler(decoder.get());
    });

    return true;
}

} // namespace WebKit::WebPushD

#endif // PLATFORM(COCOA) && ENABLE(WEB_PUSH_NOTIFICATIONS)
