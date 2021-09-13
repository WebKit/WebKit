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
#import "PrivateClickMeasurementConnection.h"

#import "PrivateClickMeasurementXPCUtilities.h"
#import <wtf/NeverDestroyed.h>

namespace WebKit {

namespace PCM {

Connection::Connection(CString&& machServiceName)
    : m_machServiceName(WTFMove(machServiceName)) { }

void Connection::initializeConnectionIfNeeded() const
{
    if (m_connection)
        return;
    m_connection = adoptOSObject(xpc_connection_create_mach_service(m_machServiceName.data(), dispatch_get_main_queue(), 0));
    xpc_connection_set_event_handler(m_connection.get(), [weakThis = makeWeakPtr(*this)](xpc_object_t event) {
        if (!weakThis)
            return;
        if (event == XPC_ERROR_CONNECTION_INVALID)
            WTFLogAlways("Failed to connect to mach service %s, likely because it is not registered with launchd", weakThis->m_machServiceName.data());
        if (event == XPC_ERROR_CONNECTION_INTERRUPTED) {
            // Daemon crashed, we will need to make a new connection to a new instance of the daemon.
            weakThis->m_connection = nullptr;
        }
    });
    xpc_connection_activate(m_connection.get());
}

static OSObjectPtr<xpc_object_t> dictionaryFromMessage(MessageType messageType, EncodedMessage&& message)
{
    auto dictionary = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
    addVersionAndEncodedMessageToDictionary(WTFMove(message), dictionary.get());
    xpc_dictionary_set_uint64(dictionary.get(), protocolMessageTypeKey, static_cast<uint64_t>(messageType));
    return dictionary;
}

void Connection::send(MessageType messageType, EncodedMessage&& message) const
{
    ASSERT(RunLoop::isMain());
    initializeConnectionIfNeeded();

    xpc_connection_send_message(m_connection.get(), dictionaryFromMessage(messageType, WTFMove(message)).get());
}

void Connection::sendWithReply(MessageType messageType, EncodedMessage&& message, CompletionHandler<void(EncodedMessage&&)>&& completionHandler) const
{
    ASSERT(RunLoop::isMain());
    initializeConnectionIfNeeded();

    auto dictionary = dictionaryFromMessage(messageType, WTFMove(message));
    xpc_connection_send_message_with_reply(m_connection.get(), dictionary.get(), dispatch_get_main_queue(), makeBlockPtr([completionHandler = WTFMove(completionHandler)] (xpc_object_t reply) mutable {
        ASSERT(RunLoop::isMain());
        if (xpc_get_type(reply) != XPC_TYPE_DICTIONARY) {
            ASSERT_NOT_REACHED();
            return completionHandler({ });
        }
        if (xpc_dictionary_get_uint64(reply, protocolVersionKey) != protocolVersionValue) {
            ASSERT_NOT_REACHED();
            return completionHandler({ });
        }
        size_t dataSize { 0 };
        const void* data = xpc_dictionary_get_data(reply, protocolEncodedMessageKey, &dataSize);
        completionHandler({ static_cast<const uint8_t*>(data), dataSize });
    }).get());
}

} // namespace PCM

} // namespace WebKit
