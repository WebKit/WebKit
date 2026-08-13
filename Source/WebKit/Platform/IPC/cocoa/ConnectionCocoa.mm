/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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
#import "Connection.h"

#import "Encoder.h"
#import "IPCUtilities.h"
#import "ImportanceAssertion.h"
#import "Logging.h"
#import "MachMessage.h"
#import "MachPort.h"
#import "MachUtilities.h"
#import "WKCrashReporter.h"
#import "XPCUtilities.h"
#import <WebCore/AXObjectCache.h>
#import <WebCore/SharedMemory.h>
#import <mach/mach_error.h>
#import <mach/mach_init.h>
#import <mach/mach_traps.h>
#import <mach/vm_map.h>
#import <sys/mman.h>
#import <wtf/CheckedArithmetic.h>
#import <wtf/HexNumber.h>
#import <wtf/MachSendRight.h>
#import <wtf/RunLoop.h>
#import <wtf/spi/darwin/XPCSPI.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/ParsingUtilities.h>

#if PLATFORM(IOS_FAMILY)
#import "ProcessAssertion.h"
#endif

namespace IPC {

static void requestNoSenderNotifications(mach_port_t port, mach_port_t notify)
{
    mach_port_t previousNotificationPort = MACH_PORT_NULL;
    auto kr = mach_port_request_notification(mach_task_self(), port, MACH_NOTIFY_NO_SENDERS, 0, notify, MACH_MSG_TYPE_MAKE_SEND_ONCE, &previousNotificationPort);
    ASSERT(kr == KERN_SUCCESS);
    if (kr != KERN_SUCCESS) {
        // If mach_port_request_notification fails, 'previousNotificationPort' will be uninitialized.
        LOG_ERROR("mach_port_request_notification failed: (%x) %s", kr, mach_error_string(kr));
    } else
        deallocateSendRightSafely(previousNotificationPort);
}

static void requestNoSenderNotifications(mach_port_t port)
{
    requestNoSenderNotifications(port, port);
}

static void clearNoSenderNotifications(mach_port_t port)
{
    requestNoSenderNotifications(port, MACH_PORT_NULL);
}

void Connection::platformInvalidate()
{
    if (!m_isConnected) {
        if (MACH_PORT_VALID(m_sendPort)) {
            ASSERT(!m_isServer);
            deallocateSendRightSafely(m_sendPort);
            m_sendPort = MACH_PORT_NULL;
        }

        if (m_receiveSource) {
            // For a short period of time, when m_isServer is true and open() has been called, m_receiveSource has been initialized
            // but m_isConnected has not been set to true yet. In this case, we need to cancel m_receiveSource instead of destroying
            // m_receivePort ourselves.
            ASSERT(m_isServer);
            cancelReceiveSource();
        }

        if (m_receivePort) {
            ASSERT(m_isServer);
#if !PLATFORM(WATCHOS)
            mach_port_unguard(mach_task_self(), m_receivePort, reinterpret_cast<mach_port_context_t>(this));
#endif
            clearNoSenderNotifications(m_receivePort);
            mach_port_mod_refs(mach_task_self(), m_receivePort, MACH_PORT_RIGHT_RECEIVE, -1);
            m_receivePort = MACH_PORT_NULL;
        }

        return;
    }

    m_pendingOutgoingMachMessage = nullptr;
    m_isConnected = false;

    ASSERT(m_receivePort);

    cancelSendSource();
    cancelReceiveSource();
}

void Connection::cancelSendSource()
{
    m_sendPort = MACH_PORT_NULL;
    if (!m_sendSource)
        return;
    dispatch_source_cancel(m_sendSource.get());
    m_sendSource = nullptr;
}

void Connection::cancelReceiveSource()
{
    dispatch_source_cancel(m_receiveSource.get());
    m_receiveSource = nullptr;
    m_receivePort = MACH_PORT_NULL;
}

void Connection::platformInitialize(Identifier&& identifier)
{
    if (m_isServer) {
        RELEASE_ASSERT(MACH_PORT_VALID(identifier.port)); // Caller error. MACH_DEAD_NAME does not make sense, as we do not transfer receive rights.
        m_receivePort = identifier.port;
#if !PLATFORM(WATCHOS)
        mach_port_guard(mach_task_self(), m_receivePort, reinterpret_cast<mach_port_context_t>(this), true);
#endif
    } else {
        RELEASE_ASSERT(identifier.port != MACH_PORT_NULL);
        // MACH_DEAD_NAME means that the send port got closed while in transit through another connection.
        // Treat it similar to as if we got a valid port but the port got closed immediately after setting up the
        // connection.
        m_sendPort = identifier.port;
    }
    m_xpcConnection = identifier.xpcConnection;
}

void Connection::platformOpen()
{
    if (m_isServer) {
        ASSERT(!m_sendPort);
        // Client passed m_receivePort. Call Client::didClose() when there are no senders to that port.
        requestNoSenderNotifications(m_receivePort);
    } else {
        ASSERT(!m_receivePort);
        auto kr = allocateImmovableConnectionPort(&m_receivePort);
        if (kr != KERN_SUCCESS) {
            RELEASE_LOG_ERROR(IPC, "Could not allocate mach port, error: %{private}s (%x)", mach_error_string(kr), kr);
            CRASH();
        }
#if !PLATFORM(WATCHOS)
        mach_port_guard(mach_task_self(), m_receivePort, reinterpret_cast<mach_port_context_t>(this), true);
#endif

#if PLATFORM(MAC)
        mach_port_set_attributes(mach_task_self(), m_receivePort, MACH_PORT_DENAP_RECEIVER, (mach_port_info_t)0, 0);
#endif

        m_isConnected = true;

        // Send the initialize message, which contains a send right for the server to use.
        auto serverSendRight = MachSendRight::createFromReceiveRight(m_receivePort);

        // Call Client::didClose() when the serverSendRight gets destroyed.
        requestNoSenderNotifications(m_receivePort);

        initializeSendSource();
        if (m_sendPort != MACH_PORT_DEAD) {
            auto encoder = makeUniqueRef<Encoder>(MessageName::InitializeConnection, 0);
            encoder.get() << WTF::move(serverSendRight);
            sendMessage(WTF::move(encoder), { });
        }
        // When send port is already dead, the serverSendRight goes out of scope and triggers
        // MACH_NOTIFY_NO_SENDERS. This way the connectionDidClose logic will be invoked for
        // dead-on-arrival connections.
    }

    // Change the message queue length for the receive port.
    setMachPortQueueLength(m_receivePort, largeOutgoingMessageQueueCountThreshold);

    m_receiveSource = adoptOSObject(dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, m_receivePort, 0, protect(m_connectionQueue->dispatchQueue()).get()));
    dispatch_source_set_event_handler(m_receiveSource.get(), [this, protectedThis = Ref { *this }] {
        receiveSourceEventHandler();
    });
    dispatch_source_set_cancel_handler(m_receiveSource.get(), [protectedThis = Ref { *this }, receivePort = m_receivePort] {
#if !PLATFORM(WATCHOS)
        mach_port_unguard(mach_task_self(), receivePort, reinterpret_cast<mach_port_context_t>(protectedThis.ptr()));
#endif
        clearNoSenderNotifications(receivePort);
        mach_port_mod_refs(mach_task_self(), receivePort, MACH_PORT_RIGHT_RECEIVE, -1);
    });

    m_connectionQueue->dispatch([strongRef = Ref { *this }, this] {
        dispatch_resume(m_receiveSource.get());
    });

    // Cache the audit token in case the XPC connection will be closed.
    getAuditToken();
}

bool Connection::platformCanSendOutgoingMessages() const
{
    return !m_pendingOutgoingMachMessage && MACH_PORT_VALID(m_sendPort);
}

bool Connection::sendOutgoingMessage(UniqueRef<Encoder>&& encoder)
{
    ASSERT(canSendOutgoingMessages());
    ASSERT(!m_pendingOutgoingMachMessage);

    auto [result, retryableMessage] = MachMessage::sendEncoder(WTF::move(encoder), m_sendPort);
    switch (result) {
    case MachMessage::SendResult::Success:
        return true;

    case MachMessage::SendResult::Timeout:
        // The message is sent again when the destination has room for it.
        m_pendingOutgoingMachMessage = WTF::move(retryableMessage);
        return false;

    case MachMessage::SendResult::InvalidEncoder:
#if ENABLE(IPC_TESTING_API)
        if (m_ignoreInvalidMessageForTesting)
            return true; // The message is dropped, but the connection can still send.
#endif
        [[fallthrough]];

    case MachMessage::SendResult::OutOfMemory:
    case MachMessage::SendResult::InvalidDestinationPort:
        cancelSendSource();
        return false;
    }
}

void Connection::initializeSendSource()
{
    ASSERT(m_isConnected);
    if (m_sendPort == MACH_PORT_DEAD)
        return;
    RELEASE_ASSERT(m_sendPort != MACH_PORT_NULL);

    m_sendSource = adoptOSObject(dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_SEND, m_sendPort, DISPATCH_MACH_SEND_POSSIBLE, protect(m_connectionQueue->dispatchQueue()).get()));
    dispatch_source_set_registration_handler(m_sendSource.get(), [this, protectedThis = Ref { *this }] {
        if (!m_sendSource)
            return;
        resumeSendSource();
    });
    dispatch_source_set_event_handler(m_sendSource.get(), [this, protectedThis = Ref { *this }] {
        if (!m_sendSource)
            return;

        unsigned long data = dispatch_source_get_data(m_sendSource.get());

        if (data & DISPATCH_MACH_SEND_POSSIBLE) {
            // FIXME: Figure out why we get spurious DISPATCH_MACH_SEND_POSSIBLE events.
            resumeSendSource();
            return;
        }
    });

    mach_port_t sendPort = m_sendPort;
    dispatch_source_set_cancel_handler(m_sendSource.get(), ^{
        // Release our send right.
        deallocateSendRightSafely(sendPort);
    });
    dispatch_resume(m_sendSource.get());
}

void Connection::resumeSendSource()
{
    if (m_pendingOutgoingMachMessage) {
        auto message = std::exchange(m_pendingOutgoingMachMessage, nullptr);
        switch (message->send()) {
        case MachMessage::SendResult::Success:
            break;

        case MachMessage::SendResult::Timeout:
            // The destination still has no room for the message, so it is sent again later.
            m_pendingOutgoingMachMessage = WTF::move(message);
            break;

        case MachMessage::SendResult::InvalidEncoder:
#if ENABLE(IPC_TESTING_API)
            if (m_ignoreInvalidMessageForTesting)
                break; // The message is dropped, but the connection can still send.
#endif
            [[fallthrough]];

        case MachMessage::SendResult::OutOfMemory:
        case MachMessage::SendResult::InvalidDestinationPort:
            cancelSendSource();
            return;
        }
    }
    sendOutgoingMessages();
}

static bool shouldLogIncomingMessageHandling()
{
    static bool shouldLog = !!getenv("WEBKIT_LOG_INCOMING_MESSAGES");
    return shouldLog;
}

void Connection::receiveSourceEventHandler()
{
    ASSERT(MACH_PORT_VALID(m_receivePort));
    auto received = MachMessage::receiveDecoder(m_receivePort);
    if (!received) {
        if (received.error() == MachMessage::ReceiveResult::NoSenders)
            connectionDidClose();
        return;
    }
    UniqueRef<Decoder> decoder = WTF::move(received.value());

    if (decoder->messageName() == MessageName::InitializeConnection) {
        ASSERT(m_isServer);
        ASSERT(!m_sendPort);
        if (m_isConnected) {
            // The sender sent an invalid message deliberately, close immediately.
            ASSERT_IS_TESTING_IPC();
            connectionDidClose();
            return;
        }
        auto sendRight = decoder->decode<MachSendRight>();
        if (!sendRight) {
            // The sender sent an invalid message deliberately, close immediately.
            ASSERT_IS_TESTING_IPC();
            connectionDidClose();
            return;
        }

        m_isConnected = true;

        if (!MACH_PORT_VALID(sendRight->sendRight())) {
            // The InitializeConnection message was valid message. We received MACH_PORT_DEAD
            // because by the time we read the message, the port was already closed.
            // Do not initialize the send source, as there is nobody to send to.
            // Keep the receive source, so that we receive the sent messages and then
            // the NO_SENDERS notification.
            return;
        }
        m_sendPort = sendRight->leakSendRight();
        initializeSendSource();
        return;
    }

    if (shouldLogIncomingMessageHandling()) [[unlikely]]
        RELEASE_LOG(IPCMessages, "Connection::processIncomingMessage(%p) received %" PUBLIC_LOG_STRING " from port 0x%08x", this, description(decoder->messageName()).characters(), m_receivePort);

    processIncomingMessage(WTF::move(decoder));
}

IPC::Connection::Identifier Connection::identifier() const
{
    return Identifier(m_isServer ? m_receivePort : m_sendPort, m_xpcConnection);
}

std::optional<audit_token_t> Connection::getAuditToken()
{
    if (m_auditToken)
        return m_auditToken;

    if (!m_xpcConnection)
        return std::nullopt;

    audit_token_t auditToken;
    xpc_connection_get_audit_token(m_xpcConnection.get(), &auditToken);
    m_auditToken = auditToken;
    return WTF::move(auditToken);
}

#if !USE(EXTENSIONKIT_PROCESS_TERMINATION)
bool Connection::kill(std::optional<MessageName> invalidMessageName)
{
    if (m_xpcConnection) {
        auto reasonCode = invalidMessageName ? WebKit::ReasonCode::MessageCheckKilled : WebKit::ReasonCode::ConnectionKilled;
        terminateWithReason(m_xpcConnection.get(), reasonCode, "Connection::kill", invalidMessageName);
        m_didRequestProcessTermination = true;
        return true;
    }
    return false;
}
#endif

pid_t Connection::remoteProcessID() const
{
    if (!m_xpcConnection)
        return 0;

    return xpc_connection_get_pid(m_xpcConnection.get());
}

std::optional<Connection::ConnectionIdentifierPair> Connection::createConnectionIdentifierPair()
{
    // Create the listening port.
    mach_port_t listeningPort = MACH_PORT_NULL;
    auto kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);
    if (kr != KERN_SUCCESS) {
        RELEASE_LOG_ERROR(Process, "Connection::createConnectionIdentifierPair: Could not allocate mach port, error %x", kr);
        return std::nullopt;
    }
    if (!MACH_PORT_VALID(listeningPort)) {
        RELEASE_LOG_ERROR(Process, "Connection::createConnectionIdentifierPair: Could not allocate mach port, returned port was invalid");
        return std::nullopt;
    }
    return ConnectionIdentifierPair { Identifier { listeningPort, nullptr }, MachSendRight::createFromReceiveRight(listeningPort) };
}

} // namespace IPC
