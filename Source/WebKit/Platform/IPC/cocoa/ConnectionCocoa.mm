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

#import "DataReference.h"
#import "IPCTester.h"
#import "ImportanceAssertion.h"
#import "Logging.h"
#import "MachMessage.h"
#import "MachUtilities.h"
#import "ReasonSPI.h"
#import "WKCrashReporter.h"
#import <WebCore/AXObjectCache.h>
#import <mach/mach_error.h>
#import <mach/vm_map.h>
#import <sys/mman.h>
#import <wtf/HexNumber.h>
#import <wtf/MachSendRight.h>
#import <wtf/RunLoop.h>
#import <wtf/spi/darwin/XPCSPI.h>
#import <wtf/text/StringConcatenate.h>

#if PLATFORM(IOS_FAMILY)
#import "ProcessAssertion.h"
#import <UIKit/UIAccessibility.h>

#if USE(APPLE_INTERNAL_SDK)
#import <AXRuntime/AXDefines.h>
#import <AXRuntime/AXNotificationConstants.h>
#else
#define kAXPidStatusChangedNotification 0
#endif

#endif

#if PLATFORM(MAC)

#import "ApplicationServicesSPI.h"

extern "C" AXError _AXUIElementNotifyProcessSuspendStatus(AXSuspendStatus);

#endif // PLATFORM(MAC)

namespace IPC {

static const size_t inlineMessageMaxSize = 4096;

// Arbitrary message IDs that do not collide with Mach notification messages (used my initials).
constexpr mach_msg_id_t inlineBodyMessageID = 0xdba0dba;
constexpr mach_msg_id_t outOfLineBodyMessageID = 0xdba1dba;

// ConnectionTerminationWatchdog does two things:
// 1) It sets a watchdog timer to kill the peered process.
// 2) On iOS, make the process runnable for the duration of the watchdog
//    to ensure it has a chance to terminate cleanly.
class ConnectionTerminationWatchdog {
public:
    static void createConnectionTerminationWatchdog(OSObjectPtr<xpc_connection_t>& xpcConnection, Seconds interval)
    {
        new ConnectionTerminationWatchdog(xpcConnection, interval);
    }
    
private:
    ConnectionTerminationWatchdog(OSObjectPtr<xpc_connection_t>& xpcConnection, Seconds interval)
        : m_xpcConnection(xpcConnection)
        , m_watchdogTimer(RunLoop::main(), this, &ConnectionTerminationWatchdog::watchdogTimerFired)
#if PLATFORM(IOS_FAMILY)
        , m_assertion(WebKit::ProcessAndUIAssertion::create(xpc_connection_get_pid(m_xpcConnection.get()), "ConnectionTerminationWatchdog"_s, WebKit::ProcessAssertionType::Background))
#endif
    {
        m_watchdogTimer.startOneShot(interval);
    }
    
    void watchdogTimerFired()
    {
        terminateWithReason(m_xpcConnection.get(), WebKit::ReasonCode::WatchdogTimerFired, "ConnectionTerminationWatchdog::watchdogTimerFired");
        delete this;
    }

    OSObjectPtr<xpc_connection_t> m_xpcConnection;
    RunLoop::Timer<ConnectionTerminationWatchdog> m_watchdogTimer;
#if PLATFORM(IOS_FAMILY)
    Ref<WebKit::ProcessAndUIAssertion> m_assertion;
#endif
};
    
void Connection::platformInvalidate()
{
    if (!m_isConnected) {
        if (m_sendPort) {
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
            mach_port_mod_refs(mach_task_self(), m_receivePort, MACH_PORT_RIGHT_RECEIVE, -1);
            m_receivePort = MACH_PORT_NULL;
        }

        return;
    }

    m_pendingOutgoingMachMessage = nullptr;
    m_isInitializingSendSource = false;
    m_isConnected = false;

    ASSERT(m_sendPort);
    ASSERT(m_receivePort);

    // Unregister our ports.
    dispatch_source_cancel(m_sendSource.get());
    m_sendSource = nullptr;
    m_sendPort = MACH_PORT_NULL;

    cancelReceiveSource();
}

void Connection::cancelReceiveSource()
{
    dispatch_source_cancel(m_receiveSource.get());
    m_receiveSource = nullptr;
    m_receivePort = MACH_PORT_NULL;
}
    
void Connection::terminateSoon(Seconds interval)
{
    if (m_xpcConnection)
        ConnectionTerminationWatchdog::createConnectionTerminationWatchdog(m_xpcConnection, interval);
}
    
void Connection::platformInitialize(Identifier identifier)
{
    if (!MACH_PORT_VALID(identifier.port))
        return;

    if (m_isServer) {
        m_receivePort = identifier.port;
        m_sendPort = MACH_PORT_NULL;

#if !PLATFORM(WATCHOS)
        mach_port_guard(mach_task_self(), m_receivePort, reinterpret_cast<mach_port_context_t>(this), true);
#endif
    } else {
        m_receivePort = MACH_PORT_NULL;
        m_sendPort = identifier.port;
    }

    m_sendSource = nullptr;
    m_receiveSource = nullptr;

    m_xpcConnection = identifier.xpcConnection;
}

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

bool Connection::open()
{
    if (m_isServer) {
        ASSERT(m_receivePort);
        ASSERT(!m_sendPort);
        ASSERT(MACH_PORT_VALID(m_receivePort));
    } else {
        ASSERT(!m_receivePort);
        ASSERT(m_sendPort);
        ASSERT(MACH_PORT_VALID(m_sendPort));

        auto kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &m_receivePort);
        if (kr != KERN_SUCCESS) {
            LOG_ERROR("Could not allocate mach port, error %x: %s", kr, mach_error_string(kr));
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
        auto encoder = makeUniqueRef<Encoder>(MessageName::InitializeConnection, 0);

        mach_port_insert_right(mach_task_self(), m_receivePort, m_receivePort, MACH_MSG_TYPE_MAKE_SEND);
        MachSendRight right = MachSendRight::adopt(m_receivePort);
        encoder.get() << Attachment { WTFMove(right) };

        initializeSendSource();

        sendMessage(WTFMove(encoder), { });
    }

    // Change the message queue length for the receive port.
    setMachPortQueueLength(m_receivePort, MACH_PORT_QLIMIT_LARGE);

    m_receiveSource = adoptOSObject(dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, m_receivePort, 0, m_connectionQueue->dispatchQueue()));
    dispatch_source_set_event_handler(m_receiveSource.get(), [this, protectedThis = Ref { *this }] {
        receiveSourceEventHandler();
    });
    dispatch_source_set_cancel_handler(m_receiveSource.get(), [protectedThis = Ref { *this }, receivePort = m_receivePort] {
#if !PLATFORM(WATCHOS)
        mach_port_unguard(mach_task_self(), receivePort, reinterpret_cast<mach_port_context_t>(protectedThis.ptr()));
#endif
        mach_port_mod_refs(mach_task_self(), receivePort, MACH_PORT_RIGHT_RECEIVE, -1);
    });
    // Disconnections are normally handled by DISPATCH_MACH_SEND_DEAD on the m_sendSource, but that's not
    // initialized until we receive the connection message from the client, so we need to request MACH_NOTIFY_NO_SENDERS
    // on the receiving port until then.
    if (m_isServer)
        requestNoSenderNotifications(m_receivePort);

    m_connectionQueue->dispatch([strongRef = Ref { *this }, this] {
        dispatch_resume(m_receiveSource.get());

        if (m_sendSource)
            dispatch_resume(m_sendSource.get());
    });

    return true;
}

bool Connection::sendMessage(std::unique_ptr<MachMessage> message)
{
    ASSERT(message);
    ASSERT(!m_pendingOutgoingMachMessage);
    ASSERT(!m_isInitializingSendSource);

    // Send the message.
    kern_return_t kr = mach_msg(message->header(), MACH_SEND_MSG | MACH_SEND_TIMEOUT | MACH_SEND_NOTIFY, message->size(), 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    switch (kr) {
    case MACH_MSG_SUCCESS:
        // The kernel has already adopted the descriptors.
        message->leakDescriptors();
        return true;

    case MACH_SEND_TIMED_OUT:
        // We timed out, stash away the message for later.
        m_pendingOutgoingMachMessage = WTFMove(message);
        return false;

    case MACH_SEND_INVALID_DEST:
        // The other end has disappeared, we'll get a dead name notification which will cause us to be invalidated.
        return false;

    default:
        auto messageName = message->messageName();
        auto errorMessage = makeString("Unhandled error code 0x", hex(kr), ", message '", description(messageName), "' (", messageName, ')');
        WebKit::logAndSetCrashLogMessage(errorMessage.utf8().data());
        CRASH_WITH_INFO(kr, WTF::enumToUnderlyingType(messageName));
    }
}

bool Connection::platformCanSendOutgoingMessages() const
{
    return !m_pendingOutgoingMachMessage && !m_isInitializingSendSource;
}

bool Connection::sendOutgoingMessage(UniqueRef<Encoder>&& encoder)
{
    ASSERT(!m_pendingOutgoingMachMessage);
    ASSERT(!m_isInitializingSendSource);

    auto attachments = encoder->releaseAttachments();
    
    auto numberOfPortDescriptors = std::count_if(attachments.begin(), attachments.end(), [](auto& attachment)
    {
        return attachment.type() == Attachment::MachPortType;
    });

    bool messageBodyIsOOL = false;
    auto messageSize = MachMessage::messageSize(encoder->bufferSize(), numberOfPortDescriptors, messageBodyIsOOL);
    if (UNLIKELY(messageSize.hasOverflowed()))
        return false;

    if (messageSize > inlineMessageMaxSize) {
        messageBodyIsOOL = true;
        messageSize = MachMessage::messageSize(0, numberOfPortDescriptors, messageBodyIsOOL);
        if (UNLIKELY(messageSize.hasOverflowed()))
            return false;
    }

    size_t safeMessageSize = messageSize;
    auto message = MachMessage::create(encoder->messageName(), safeMessageSize);
    if (!message)
        return false;

    auto* header = message->header();
    header->msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    header->msgh_size = safeMessageSize;
    header->msgh_remote_port = m_sendPort;
    header->msgh_local_port = MACH_PORT_NULL;
    header->msgh_id = messageBodyIsOOL ? outOfLineBodyMessageID : inlineBodyMessageID;

    auto* messageData = reinterpret_cast<uint8_t*>(header + 1);

    bool isComplex = numberOfPortDescriptors || messageBodyIsOOL;
    if (isComplex) {
        header->msgh_bits |= MACH_MSGH_BITS_COMPLEX;

        auto* body = reinterpret_cast<mach_msg_body_t*>(messageData);
        body->msgh_descriptor_count = numberOfPortDescriptors + messageBodyIsOOL;
        messageData = reinterpret_cast<uint8_t*>(body + 1);

        auto getDescriptorAndAdvance = [](uint8_t*& data, std::size_t toAdvance) {
            return reinterpret_cast<mach_msg_descriptor_t*>(std::exchange(data, data + toAdvance));
        };

        for (auto& attachment : attachments) {
            ASSERT(attachment.type() == Attachment::MachPortType);
            if (attachment.type() == Attachment::MachPortType) {
                auto* descriptor = getDescriptorAndAdvance(messageData, sizeof(mach_msg_port_descriptor_t));
                descriptor->port.name = attachment.leakSendRight();
                descriptor->port.disposition = MACH_MSG_TYPE_MOVE_SEND;
                descriptor->port.type = MACH_MSG_PORT_DESCRIPTOR;
            }
        }

        if (messageBodyIsOOL) {
            auto* descriptor = getDescriptorAndAdvance(messageData, sizeof(mach_msg_ool_descriptor_t));
            descriptor->out_of_line.address = encoder->buffer();
            descriptor->out_of_line.size = encoder->bufferSize();
            descriptor->out_of_line.copy = MACH_MSG_VIRTUAL_COPY;
            descriptor->out_of_line.deallocate = false;
            descriptor->out_of_line.type = MACH_MSG_OOL_DESCRIPTOR;
        }
    }

    // Copy the data if it is not being sent out-of-line.
    if (!messageBodyIsOOL)
        memcpy(messageData, encoder->buffer(), encoder->bufferSize());

    ASSERT(m_sendPort);
    ASSERT(MACH_PORT_VALID(m_sendPort));

    return sendMessage(WTFMove(message));
}

void Connection::initializeSendSource()
{
    m_sendSource = adoptOSObject(dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_SEND, m_sendPort, DISPATCH_MACH_SEND_DEAD | DISPATCH_MACH_SEND_POSSIBLE, m_connectionQueue->dispatchQueue()));
    m_isInitializingSendSource = true;

    dispatch_source_set_registration_handler(m_sendSource.get(), [this, protectedThis = Ref { *this }] {
        if (!m_sendSource)
            return;
        m_isInitializingSendSource = false;
        resumeSendSource();
    });
    dispatch_source_set_event_handler(m_sendSource.get(), [this, protectedThis = Ref { *this }] {
        if (!m_sendSource)
            return;

        unsigned long data = dispatch_source_get_data(m_sendSource.get());

        if (data & DISPATCH_MACH_SEND_DEAD) {
            connectionDidClose();
            return;
        }

        if (data & DISPATCH_MACH_SEND_POSSIBLE) {
            // FIXME: Figure out why we get spurious DISPATCH_MACH_SEND_POSSIBLE events.
            resumeSendSource();
            return;
        }
    });

    if (MACH_PORT_VALID(m_sendPort)) {
        mach_port_t sendPort = m_sendPort;
        dispatch_source_set_cancel_handler(m_sendSource.get(), ^{
            // Release our send right.
            deallocateSendRightSafely(sendPort);
        });
    }
}

void Connection::resumeSendSource()
{
    ASSERT(!m_isInitializingSendSource);
    if (m_pendingOutgoingMachMessage)
        sendMessage(WTFMove(m_pendingOutgoingMachMessage));
    sendOutgoingMessages();
}

static std::unique_ptr<Decoder> createMessageDecoder(mach_msg_header_t* header, size_t bufferSize)
{
    if (UNLIKELY(header->msgh_size > bufferSize)) {
        RELEASE_LOG_FAULT(IPC, "createMessageDecoder: msgh_size is greater than bufferSize (header->msgh_size: %lu, bufferSize: %lu)", static_cast<unsigned long>(header->msgh_size), bufferSize);
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    if (!(header->msgh_bits & MACH_MSGH_BITS_COMPLEX)) {
        // We have a simple message.
        uint8_t* body = reinterpret_cast<uint8_t*>(header + 1);
        auto bodySize = CheckedSize { header->msgh_size } - sizeof(mach_msg_header_t);
        if (UNLIKELY(bodySize.hasOverflowed())) {
            RELEASE_LOG_FAULT(IPC, "createMessageDecoder: Overflow when computing bodySize (header->msgh_size: %lu, sizeof(mach_msg_header_t): %lu)", static_cast<unsigned long>(header->msgh_size), sizeof(mach_msg_header_t));
            ASSERT_NOT_REACHED();
            return nullptr;
        }

        return Decoder::create(body, bodySize, { });
    }

    mach_msg_body_t* body = reinterpret_cast<mach_msg_body_t*>(header + 1);
    mach_msg_size_t numberOfPortDescriptors = body->msgh_descriptor_count;
    ASSERT(numberOfPortDescriptors);
    if (UNLIKELY(!numberOfPortDescriptors))
        return nullptr;

    auto sizeWithPortDescriptors = CheckedSize { sizeof(mach_msg_header_t) + sizeof(mach_msg_body_t) } + CheckedSize { numberOfPortDescriptors } * sizeof(mach_msg_port_descriptor_t);
    if (UNLIKELY(sizeWithPortDescriptors.hasOverflowed() || sizeWithPortDescriptors.value() > bufferSize)) {
        RELEASE_LOG_FAULT(IPC, "createMessageDecoder: Overflow when computing sizeWithPortDescriptors (numberOfPortDescriptors: %lu)", static_cast<unsigned long>(numberOfPortDescriptors));
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    uint8_t* descriptorData = reinterpret_cast<uint8_t*>(body + 1);

    // If the message body was sent out-of-line, don't treat the last descriptor
    // as an attachment, since it is really the message body.
    bool messageBodyIsOOL = header->msgh_id == outOfLineBodyMessageID;
    mach_msg_size_t numberOfAttachments = messageBodyIsOOL ? numberOfPortDescriptors - 1 : numberOfPortDescriptors;

    // Build attachment list
    Vector<Attachment> attachments(numberOfAttachments);

    for (mach_msg_size_t i = 0; i < numberOfAttachments; ++i) {
        mach_msg_descriptor_t* descriptor = reinterpret_cast<mach_msg_descriptor_t*>(descriptorData);
        ASSERT(descriptor->type.type == MACH_MSG_PORT_DESCRIPTOR);
        if (descriptor->type.type != MACH_MSG_PORT_DESCRIPTOR)
            return nullptr;
        ASSERT(descriptor->port.disposition == MACH_MSG_TYPE_PORT_SEND);
        MachSendRight right = MachSendRight::adopt(descriptor->port.name);

        attachments[numberOfAttachments - i - 1] = Attachment { WTFMove(right) };
        descriptorData += sizeof(mach_msg_port_descriptor_t);
    }

    if (messageBodyIsOOL) {
        mach_msg_descriptor_t* descriptor = reinterpret_cast<mach_msg_descriptor_t*>(descriptorData);
        ASSERT(descriptor->type.type == MACH_MSG_OOL_DESCRIPTOR);
        if (descriptor->type.type != MACH_MSG_OOL_DESCRIPTOR)
            return nullptr;

        uint8_t* messageBody = static_cast<uint8_t*>(descriptor->out_of_line.address);
        size_t messageBodySize = descriptor->out_of_line.size;
        descriptor->out_of_line.deallocate = false; // We are taking ownership of the memory.

        return Decoder::create(messageBody, messageBodySize, [](const uint8_t* buffer, size_t length) {
            // FIXME: <rdar://problem/62086358> bufferDeallocator block ignores mach_msg_ool_descriptor_t->deallocate
            vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(buffer), length);
        }, WTFMove(attachments));
    }

    uint8_t* messageBody = descriptorData;
    ASSERT((reinterpret_cast<uint8_t*>(header) + sizeWithPortDescriptors.value()) == messageBody);
    auto messageBodySize = header->msgh_size - sizeWithPortDescriptors;
    if (UNLIKELY(messageBodySize.hasOverflowed())) {
        RELEASE_LOG_FAULT(IPC, "createMessageDecoder: Overflow when computing bodySize (header->msgh_size: %lu, sizeWithPortDescriptors: %lu)", static_cast<unsigned long>(header->msgh_size), static_cast<unsigned long>(sizeWithPortDescriptors.value()));
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    return Decoder::create(messageBody, messageBodySize, WTFMove(attachments));
}

// The receive buffer size should always include the maximum trailer size.
static const size_t receiveBufferSize = inlineMessageMaxSize + MAX_TRAILER_SIZE;
typedef Vector<char, receiveBufferSize> ReceiveBuffer;

static mach_msg_header_t* readFromMachPort(mach_port_t machPort, ReceiveBuffer& buffer)
{
    ASSERT(MACH_PORT_VALID(machPort));

    buffer.resize(receiveBufferSize);

    mach_msg_header_t* header = reinterpret_cast<mach_msg_header_t*>(buffer.data());
    kern_return_t kr = mach_msg(header, MACH_RCV_MSG | MACH_RCV_LARGE | MACH_RCV_TIMEOUT | MACH_RCV_VOUCHER, 0, buffer.size(), machPort, 0, MACH_PORT_NULL);
    if (kr == MACH_RCV_TIMED_OUT)
        return nullptr;

    if (kr == MACH_RCV_TOO_LARGE) {
        // The message was too large, resize the buffer and try again.
        buffer.resize(header->msgh_size + MAX_TRAILER_SIZE);
        header = reinterpret_cast<mach_msg_header_t*>(buffer.data());
        
        kr = mach_msg(header, MACH_RCV_MSG | MACH_RCV_LARGE | MACH_RCV_TIMEOUT | MACH_RCV_VOUCHER, 0, buffer.size(), machPort, 0, MACH_PORT_NULL);
        ASSERT(kr != MACH_RCV_TOO_LARGE);
    }

    if (kr != MACH_MSG_SUCCESS) {
#if ASSERT_ENABLED
        auto errorMessage = makeString("Unhandled error code 0x", hex(kr), " from mach_msg, receive port is 0x", hex(machPort));
        WebKit::logAndSetCrashLogMessage(errorMessage.utf8().data());
#endif
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    return header;
}

void Connection::receiveSourceEventHandler()
{
    ReceiveBuffer buffer;

    ASSERT(MACH_PORT_VALID(m_receivePort));
    mach_msg_header_t* header = readFromMachPort(m_receivePort, buffer);
    if (!header)
        return;

    switch (header->msgh_id) {
    case MACH_NOTIFY_NO_SENDERS:
        ASSERT(m_isServer);
        if (!m_sendPort)
            connectionDidClose();
        return;

    case inlineBodyMessageID:
    case outOfLineBodyMessageID:
        break;

    case MACH_NOTIFY_SEND_ONCE:
    default:
        return;
    }

    std::unique_ptr<Decoder> decoder = createMessageDecoder(header, buffer.size());
    if (!decoder)
        return;

#if PLATFORM(MAC)
    decoder->setImportanceAssertion(ImportanceAssertion { header });
#endif
    
    if (decoder->messageName() == MessageName::InitializeConnection) {
        ASSERT(m_isServer);
        ASSERT(!m_sendPort);
        if (m_isConnected) {
            ASSERT_IS_TESTING_IPC();
            return;
        }

        Attachment attachment;
        if (!decoder->decode(attachment)) {
            // FIXME: Disconnect.
            return;
        }

        m_sendPort = attachment.leakSendRight();
        
        if (m_sendPort) {
            ASSERT(MACH_PORT_VALID(m_receivePort));
            clearNoSenderNotifications(m_receivePort);

            initializeSendSource();
            dispatch_resume(m_sendSource.get());
        }

        m_isConnected = true;

        // Send any pending outgoing messages.
        sendOutgoingMessages();
        
        return;
    }

    processIncomingMessage(WTFMove(decoder));
}    

IPC::Connection::Identifier Connection::identifier() const
{
    return Identifier(m_isServer ? m_receivePort : m_sendPort, m_xpcConnection);
}

std::optional<audit_token_t> Connection::getAuditToken()
{
    if (!m_xpcConnection)
        return std::nullopt;
    
    audit_token_t auditToken;
    xpc_connection_get_audit_token(m_xpcConnection.get(), &auditToken);
    return WTFMove(auditToken);
}

bool Connection::kill()
{
    if (m_xpcConnection) {
        terminateWithReason(m_xpcConnection.get(), WebKit::ReasonCode::ConnectionKilled, "Connection::kill");
        m_wasKilled = true;
        return true;
    }

    return false;
}

void AccessibilityProcessSuspendedNotification(bool suspended)
{
#if PLATFORM(MAC)
    _AXUIElementNotifyProcessSuspendStatus(suspended ? AXSuspendStatusSuspended : AXSuspendStatusRunning);
#elif PLATFORM(IOS_FAMILY)
    UIAccessibilityPostNotification(kAXPidStatusChangedNotification, @{ @"pid" : @(getpid()), @"suspended" : @(suspended) });
#else
    UNUSED_PARAM(suspended);
#endif
}

void Connection::willSendSyncMessage(OptionSet<SendSyncOption> sendSyncOptions)
{
    if (sendSyncOptions.contains(IPC::SendSyncOption::InformPlatformProcessWillSuspend) && WebCore::AXObjectCache::accessibilityEnabled())
        AccessibilityProcessSuspendedNotification(true);
}

void Connection::didReceiveSyncReply(OptionSet<SendSyncOption> sendSyncOptions)
{
    if (sendSyncOptions.contains(IPC::SendSyncOption::InformPlatformProcessWillSuspend) && WebCore::AXObjectCache::accessibilityEnabled())
        AccessibilityProcessSuspendedNotification(false);
}

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
    mach_port_insert_right(mach_task_self(), listeningPort, listeningPort, MACH_MSG_TYPE_MAKE_SEND);
    MachSendRight right = MachSendRight::adopt(listeningPort);

    return ConnectionIdentifierPair { Connection::Identifier { listeningPort }, Attachment { WTFMove(right) } };
}
} // namespace IPC
