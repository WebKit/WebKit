/*
 * Copyright (C) 2021-2024 Apple Inc. All rights reserved.
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
#import "DaemonConnection.h"

#import "DaemonEncoder.h"
#import "PrivateClickMeasurementConnection.h"
#import "WebPushDaemonConnection.h"
#import <wtf/BlockPtr.h>
#import <wtf/RunLoop.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebKit {

namespace Daemon {

void Connection::send(xpc_object_t message) const
{
    ASSERT(RunLoop::isMain());
    initializeConnectionIfNeeded();

    ASSERT(m_connection.get());
    ASSERT(xpc_get_type(message) == XPC_TYPE_DICTIONARY);
    xpc_connection_send_message(m_connection.get(), message);
}

void Connection::sendWithReply(xpc_object_t message, CompletionHandler<void(xpc_object_t)>&& completionHandler) const
{
    ASSERT(RunLoop::isMain());
    initializeConnectionIfNeeded();

    ASSERT(m_connection.get());
    ASSERT(xpc_get_type(message) == XPC_TYPE_DICTIONARY);
    xpc_connection_send_message_with_reply(m_connection.get(), message, dispatch_get_main_queue(), makeBlockPtr([completionHandler = WTFMove(completionHandler)] (xpc_object_t reply) mutable {
        ASSERT(RunLoop::isMain());
        completionHandler(reply);
    }).get());
}

template<typename Traits>
void ConnectionToMachService<Traits>::initializeConnectionIfNeeded() const
{
    if (m_connection)
        return;
    m_connection = adoptOSObject(xpc_connection_create_mach_service(m_machServiceName.data(), dispatch_get_main_queue(), 0));
    xpc_connection_set_event_handler(m_connection.get(), [weakThis = WeakPtr { *this }](xpc_object_t event) {
        if (!weakThis)
            return;
        if (event == XPC_ERROR_CONNECTION_INVALID) {
#if HAVE(XPC_CONNECTION_COPY_INVALIDATION_REASON)
            auto reason = std::unique_ptr<char[]>(xpc_connection_copy_invalidation_reason(weakThis->m_connection.get()));
            WTFLogAlways("Failed to connect to mach service %s, reason: %s", weakThis->m_machServiceName.data(), reason.get());
#else
            WTFLogAlways("Failed to connect to mach service %s, likely because it is not registered with launchd", weakThis->m_machServiceName.data());
#endif
        }
        if (event == XPC_ERROR_CONNECTION_INTERRUPTED) {
            // Daemon crashed, we will need to make a new connection to a new instance of the daemon.
            weakThis->m_connection = nullptr;
        }
        weakThis->connectionReceivedEvent(event);
    });
    xpc_connection_activate(m_connection.get());

    newConnectionWasInitialized();
}

template<typename Traits>
void ConnectionToMachService<Traits>::send(typename Traits::MessageType messageType, EncodedMessage&& message) const
{
    initializeConnectionIfNeeded();
    Connection::send(dictionaryFromMessage(messageType, WTFMove(message)).get());
}

template<typename Traits>
void ConnectionToMachService<Traits>::sendWithReply(typename Traits::MessageType messageType, EncodedMessage&& message, CompletionHandler<void(EncodedMessage&&)>&& completionHandler) const
{
    ASSERT(RunLoop::isMain());
    initializeConnectionIfNeeded();

    Connection::sendWithReply(dictionaryFromMessage(messageType, WTFMove(message)).get(), [completionHandler = WTFMove(completionHandler)] (xpc_object_t reply) mutable {
        if (xpc_get_type(reply) != XPC_TYPE_DICTIONARY) {
            ASSERT_NOT_REACHED();
            return completionHandler({ });
        }
        if (xpc_dictionary_get_uint64(reply, Traits::protocolVersionKey) != Traits::protocolVersionValue) {
            ASSERT_NOT_REACHED();
            return completionHandler({ });
        }
        completionHandler(xpc_dictionary_get_data_span(reply, Traits::protocolEncodedMessageKey));
    });
}

template class ConnectionToMachService<PCM::ConnectionTraits>;

#if ENABLE(WEB_PUSH_NOTIFICATIONS)
template class ConnectionToMachService<WebPushD::ConnectionTraits>;
#endif

} // namespace Daemon

} // namespace WebKit

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
