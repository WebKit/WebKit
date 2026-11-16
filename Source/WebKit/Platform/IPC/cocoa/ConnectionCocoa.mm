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
    assertIsCurrent(m_connectionQueue.get());
    Locker locker { m_sendLock };

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
    dispatch_source_cancel(protect(m_sendSource));
    m_sendSource = nullptr;
}

void Connection::cancelReceiveSource()
{
    dispatch_source_cancel(protect(m_receiveSource));
    m_receiveSource = nullptr;
    m_receivePort = MACH_PORT_NULL;
}

void Connection::platformInitialize(Identifier&& identifier)
{
    Locker locker { m_sendLock };
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
#if ASSERT_ENABLED
        {
            Locker locker { m_sendLock };
            ASSERT(!m_sendPort);
        }
#endif
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
        // Send the message unconditionally. If the receiver is gone, the message is dropped, which
        // releases serverSendRight and triggers MACH_NOTIFY_NO_SENDERS, so that connectionDidClose()
        // runs.
        auto encoder = makeUniqueRef<Encoder>(MessageName::InitializeConnection, 0);
        encoder.get() << WTF::move(serverSendRight);
        sendMessage(WTF::move(encoder), { });
    }

    // Change the message queue length for the receive port.
    setMachPortQueueLength(m_receivePort, largeOutgoingMessageQueueCountThreshold);

    OSObjectPtr receiveSource = adoptOSObject(dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, m_receivePort, 0, protect(m_connectionQueue->dispatchQueue()).get()));
    m_receiveSource = receiveSource;
    dispatch_source_set_event_handler(receiveSource, [this, protectedThis = Ref { *this }] {
        receiveSourceEventHandler();
    });
    dispatch_source_set_cancel_handler(receiveSource, [protectedThis = Ref { *this }, receivePort = m_receivePort] {
#if !PLATFORM(WATCHOS)
        mach_port_unguard(mach_task_self(), receivePort, reinterpret_cast<mach_port_context_t>(protectedThis.ptr()));
#endif
        clearNoSenderNotifications(receivePort);
        mach_port_mod_refs(mach_task_self(), receivePort, MACH_PORT_RIGHT_RECEIVE, -1);
    });

    m_connectionQueue->dispatch([strongRef = Ref { *this }, this] {
        dispatch_resume(protect(m_receiveSource));
    });

    // Cache the audit token in case the XPC connection will be closed.
    getAuditToken();
}

bool Connection::platformCanSendOutgoingMessages() const
{
    // m_isConnected is only used on the connection queue and when opening. Whether the send source exists is what says
    // whether the connection can send, with m_sendLock held, on any thread.
    return !!m_sendSource;
}

bool Connection::platformHasMessageInFlight() const
{
    return false;
}

auto Connection::platformCreateOutgoingMessage(UniqueRef<Encoder>&& encoder) -> std::optional<OutgoingMessage>
{
    auto messageName = encoder->messageName();
    auto message = MachMessage::create(WTF::move(encoder));
    if (!message) {
        RELEASE_LOG_ERROR(IPC, "platformCreateOutgoingMessage: Dropping message '%" PUBLIC_LOG_STRING "' with more attachments than a mach message can carry", description(messageName).characters());
        return std::nullopt;
    }
    return makeUniqueRefFromNonNullUniquePtr(WTF::move(message));
}

auto Connection::platformSendOutgoingMessage(OutgoingMessage& message) -> PlatformSendResult
{
    assertIsHeld(m_sendLock);
    ASSERT(m_sendSource);

    return didSendMachMessage(message->send(m_sendPort));
}

auto Connection::sendFirstOutgoingMessage(Locker<SendLock>& locker) -> PlatformSendResult
{
    assertIsCurrent(m_connectionQueue.get());
    ASSERT(m_sendSource);

    // While m_sendLock is dropped, other threads can append to m_outgoingMessages, which can move
    // its storage, so hold onto the message itself. The message stays first in the queue, a thread
    // only sends directly when no message is queued, and only the connection queue sends queued
    // messages, so neither the message nor m_sendPort and m_sendSource change meanwhile.
    auto& message = m_outgoingMessages.first().get();
    auto sendPort = m_sendPort;
    // FIXME (rdar://188839922): Send with m_sendLock held, as a direct send does.
    auto sendResult = [&] {
        DropLockForScope unlocker { locker };
        return message.send(sendPort);
    }();

    auto result = didSendMachMessage(sendResult);
    // A message that the receiver had no room for stays first, and markReceiverGone() drops the
    // message if the receiver is gone.
    if (result == PlatformSendResult::Done)
        m_outgoingMessages.removeFirst();
    return result;
}

auto Connection::didSendMachMessage(MachMessageSendResult result) -> PlatformSendResult
{
    switch (result) {
    case MachMessageSendResult::Success:
        return PlatformSendResult::Done;

    case MachMessageSendResult::Timeout:
        return PlatformSendResult::ReceiverIsFull;

    case MachMessageSendResult::InvalidDestinationPort:
        // The other end has destroyed the receive right to the port we are trying to send to.
        // Cancel the send source, so that we do not try to send more messages needlessly. This
        // instance is invalidated once it gets the NO_SENDERS notification. See markReceiverGone().
        cancelSendSource();
        return PlatformSendResult::ReceiverIsGone;
    }
    ASSERT_NOT_REACHED();
    return PlatformSendResult::ReceiverIsGone;
}

void Connection::initializeSendSource()
{
    ASSERT(m_isConnected);
    Locker locker { m_sendLock };
    if (m_sendPort == MACH_PORT_DEAD) {
        markReceiverGone();
        return;
    }
    RELEASE_ASSERT(m_sendPort != MACH_PORT_NULL);

    OSObjectPtr sendSource = adoptOSObject(dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_SEND, m_sendPort, DISPATCH_MACH_SEND_POSSIBLE, protect(m_connectionQueue->dispatchQueue()).get()));
    m_sendSource = sendSource;
    // Messages queued before the source was registered get no send-possible notification, so send
    // them once the source is registered.
    dispatch_source_set_registration_handler(sendSource, [protectedThis = Ref { *this }] {
        protectedThis->sendOutgoingMessages();
    });
    // The receiver may have room for the queued messages now.
    dispatch_source_set_event_handler(sendSource, [protectedThis = Ref { *this }] {
        protectedThis->sendOutgoingMessages();
    });

    mach_port_t sendPort = m_sendPort;
    dispatch_source_set_cancel_handler(sendSource, ^{
        // Release our send right.
        deallocateSendRightSafely(sendPort);
    });
    dispatch_resume(sendSource);
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

        {
            Locker locker { m_sendLock };
            ASSERT(!m_sendPort);
            // If the client went away before this message was received, the send right is dead.
            // The connection stays open until it has received the messages that the client sent.
            // Then it closes on the no-senders notification.
            m_sendPort = sendRight->leakSendRight();
        }
        initializeSendSource();
        return;
    }

    if (shouldLogIncomingMessageHandling()) [[unlikely]]
        RELEASE_LOG(IPCMessages, "Connection::processIncomingMessage(%p) received %" PUBLIC_LOG_STRING " from port 0x%08x", this, description(decoder->messageName()).characters(), m_receivePort);

    processIncomingMessage(WTF::move(decoder));
}

IPC::Connection::Identifier Connection::identifier() const
{
    Locker locker { m_sendLock };
    return Identifier(m_isServer ? m_receivePort : m_sendPort, m_xpcConnection);
}

std::optional<audit_token_t> Connection::getAuditToken()
{
    if (m_auditToken)
        return m_auditToken;

    if (!m_xpcConnection)
        return std::nullopt;

    audit_token_t auditToken;
    xpc_connection_get_audit_token(protect(m_xpcConnection), &auditToken);
    m_auditToken = auditToken;
    return WTF::move(auditToken);
}

#if !USE(EXTENSIONKIT_PROCESS_TERMINATION)
bool Connection::kill(std::optional<MessageName> invalidMessageName)
{
    if (OSObjectPtr xpcConnection = m_xpcConnection) {
        auto reasonCode = invalidMessageName ? WebKitReasonCode::MessageCheckKilled : WebKitReasonCode::ConnectionKilled;
        WebKit::terminateWithReason(xpcConnection, reasonCode, "Connection::kill", invalidMessageName);
        m_didRequestProcessTermination = true;
        return true;
    }
    return false;
}
#endif

pid_t Connection::remoteProcessID() const
{
    OSObjectPtr xpcConnection = m_xpcConnection;
    return xpcConnection ? xpc_connection_get_pid(xpcConnection) : 0;
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
