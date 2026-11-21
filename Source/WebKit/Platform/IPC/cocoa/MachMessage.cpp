/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "config.h"
#include "MachMessage.h"

#include "Attachment.h"
#include "Decoder.h"
#include "ImportanceAssertion.h"
#include "Logging.h"
#include "WKCrashReporter.h"
#include <limits>
#include <mach/mach.h>
#include <wtf/HexNumber.h>
#include <wtf/MallocSpan.h>
#include <wtf/MathExtras.h>
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/MakeString.h>

namespace IPC {

namespace {

// An arbitrary message ID that does not collide with Mach notification messages.
static constexpr mach_msg_id_t webkitMachMessageID = 0xdba0dba;

// Message ID for a "pending attachments" message carrying a batch of inline port descriptors that would not
// fit in the message they belong to. See MachMessage::createPendingAttachments().
static constexpr mach_msg_id_t webkitMachMessageAddPendingAttachmentsID = 0xdba3dba;

static constexpr size_t typicalMaximumAttachmentCount = 16;

// Sized to fit the largest inline body plus a typical number of attachments without a heap
// allocation.
static constexpr size_t messageStorageSize = round_msg(sizeof(mach_msg_header_t) + sizeof(mach_msg_body_t)
    + typicalMaximumAttachmentCount * sizeof(mach_msg_port_descriptor_t) + Encoder::maximumInlineBodySize);

// The Mach IPC layer restricts how many inline port descriptors a single message from certain callers may
// carry (see IPC_KMSG_INLINE_PORT_DESCRIPTOR_LIMIT in XNU). Every message stays strictly below that limit:
// createMessages() splits a larger attachment set across preceding pending attachments messages.
static constexpr size_t maxInlinePortDescriptorsPerMessage = 127;

static constexpr size_t maxMessageSize = sizeof(mach_msg_header_t) + sizeof(mach_msg_body_t)
    + maxInlinePortDescriptorsPerMessage * sizeof(mach_msg_port_descriptor_t) + std::max(sizeof(mach_msg_ool_descriptor_t), Encoder::maximumInlineBodySize);
static_assert(maxMessageSize <= std::numeric_limits<mach_msg_size_t>::max());

// Storage for one received message. The messages that fit messageStorageSize, plus the trailer a received
// message comes with, are received in the inline storage, so that the common case does not allocate.
class MessageBuffer {
    WTF_MAKE_NONCOPYABLE(MessageBuffer);
public:
    static constexpr size_t inlineStorageSize = messageStorageSize + MAX_TRAILER_SIZE;

    MessageBuffer() = default;

    // Returns storage for `size` bytes, or an empty span if it could not be allocated. The storage is inline
    // when `size` is at most inlineStorageSize. Invalidates the storage returned by an earlier call.
    std::span<std::byte> allocate(size_t size)
    {
        if (size <= inlineStorageSize)
            return std::span<std::byte> { m_inlineStorage }.first(size);
        m_heapStorage = MallocSpan<std::byte>::tryMalloc(size);
        if (!m_heapStorage) [[unlikely]]
            return { };
        return m_heapStorage.mutableSpan();
    }

private:
    alignas(mach_msg_header_t) std::array<std::byte, inlineStorageSize> m_inlineStorage;
    MallocSpan<std::byte> m_heapStorage;
};

static kern_return_t sendMessage(mach_msg_header_t* header, Timeout timeout)
{
    mach_msg_option_t options = 0;
    mach_msg_timeout_t timeoutMilliseconds = MACH_MSG_TIMEOUT_NONE;
    if (!timeout.isInfinity()) [[likely]] {
        options = MACH_SEND_TIMEOUT | MACH_SEND_NOTIFY;
        timeoutMilliseconds = timeout.secondsUntilDeadline().millisecondsAs<mach_msg_timeout_t>();
    }
    return ::mach_msg(header, MACH_SEND_MSG | options, header->msgh_size, 0, MACH_PORT_NULL, timeoutMilliseconds, MACH_PORT_NULL);
}

// The result of sending a message, for a kern_return_t that is not MACH_MSG_SUCCESS and not one that
// the caller handles itself.
static MachMessage::SendResult sendResult(kern_return_t kr, MessageName messageName)
{
    using SendResult = MachMessage::SendResult;
    switch (kr) {
    case MACH_MSG_SUCCESS:
        return SendResult::Success;

    case MACH_SEND_TIMED_OUT:
        return SendResult::Timeout;

    case MACH_SEND_INVALID_DEST:
        return SendResult::InvalidDestinationPort;

    default:
        auto errorMessage = makeString("Unhandled error code 0x"_s, hex(kr), ", message '"_s, description(messageName), "' ("_s, messageName, ")"_s);
        WebKit::logAndSetCrashLogMessage(errorMessage.utf8());
        CRASH_WITH_INFO(kr, std::to_underlying(messageName));
    }
}

} // namespace

MachMessage::MachMessage(MessageName messageName, MmapSpan<uint8_t>&& outOfLineBody)
    : m_messageName { messageName }
    , m_outOfLineBody { WTF::move(outOfLineBody) }
{
}

MachMessage::~MachMessage()
{
    // Successful message sends already consume the rights associated with the message.
    if (m_state != State::Sent)
        ::mach_msg_destroy(m_messageHeader);
}

// A message with no attachments and a body that fits is the header and the body of the encoder:
//     mach_msg_header_t header;
//     std::byte encoderBody[];
//
// Anything else is a complex message, and its descriptors come in a fixed order: the attachments and then
// the body, when the body is out-of-line. So the first descriptor says which form the attachments are in,
// and the body is what is left over:
//     mach_msg_header_t header;
//     mach_msg_body_t body;
//     mach_msg_port_descriptor_t attachments[attachmentCount];
//     mach_msg_ool_descriptor_t outOfLineEncoderBody  // when the encoder keeps the body out-of-line,
//         | std::byte encoderBody[];                  // and otherwise the body itself
//
// The attachments are not sent as an out-of-line array of port names, even though that would keep every
// message within messageStorageSize: the kernel refuses a mach_msg_ool_ports_descriptor_t sent to an
// ordinary connection port with a fatal EXC_GUARD, see IPC_SEC_POLICY_RESTRICT_OOL_PORT_ARRAYS.

UniqueRef<MachMessage> MachMessage::createBody(UniqueRef<Encoder>&& encoderIn)
{
    auto encoder = WTF::move(encoderIn);

    size_t attachmentCount = encoder->attachmentCount();
    RELEASE_ASSERT(attachmentCount <= maxInlinePortDescriptorsPerMessage);

    auto body = encoder->span();
    auto outOfLineBuffer = encoder->releaseOutOfLineBuffer();
    bool bodyIsOutOfLine = !!outOfLineBuffer;
    ASSERT(bodyIsOutOfLine || body.size() <= Encoder::maximumInlineBodySize);

    // The number of attachments a message may carry is bounded, so the arithmetic below cannot overflow,
    // which is what maxMessageSize holds to.
    size_t attachmentDescriptorsSize = attachmentCount * sizeof(mach_msg_port_descriptor_t);
    size_t headerAndDescriptorsSize = sizeof(mach_msg_header_t) + (attachmentCount ? sizeof(mach_msg_body_t) + attachmentDescriptorsSize : 0);
    size_t messageSize = round_msg(bodyIsOutOfLine
        ? sizeof(mach_msg_header_t) + sizeof(mach_msg_body_t) + attachmentDescriptorsSize + sizeof(mach_msg_ool_descriptor_t)
        : headerAndDescriptorsSize + body.size());
    ASSERT(messageSize <= maxMessageSize);

    // Every byte of the trailing messageSize storage is initialized below (including padding).
    void* memory = fastMalloc(sizeof(MachMessage) + messageSize);
    auto result = makeUniqueRefFromNonNullUniquePtr(std::unique_ptr<MachMessage> { new (NotNull, memory) MachMessage { encoder->messageName(), WTF::move(outOfLineBuffer) } });

    size_t descriptorCount = attachmentCount + bodyIsOutOfLine;
    auto remaining = unsafeMakeSpan(reinterpret_cast<std::byte*>(result->m_messageHeader), messageSize);
    consumeAndReinterpretCastTo<mach_msg_header_t>(remaining) = {
        .msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0) | (descriptorCount ? MACH_MSGH_BITS_COMPLEX : 0),
        .msgh_size = static_cast<mach_msg_size_t>(messageSize),
        .msgh_remote_port = MACH_PORT_NULL,
        .msgh_local_port = MACH_PORT_NULL,
        .msgh_voucher_port = MACH_PORT_NULL,
        .msgh_id = webkitMachMessageID
    };

    if (descriptorCount) {
        consumeAndReinterpretCastTo<mach_msg_body_t>(remaining) = {
            .msgh_descriptor_count = static_cast<mach_msg_size_t>(descriptorCount)
        };
    }

    // The attachments are written in reverse: the decoder takes them from the end of what it is given, so
    // the last one in the message is the first one decoded, which is the order they were encoded in. Writing
    // them this way is what saves the receiver from turning them around.
    auto attachments = encoder->releaseAttachments();
    for (auto attachment = attachments.rbegin(); attachment != attachments.rend(); ++attachment) {
        mach_msg_port_descriptor_t descriptor { };
        descriptor.name = attachment->leakSendRight();
        descriptor.disposition = MACH_MSG_TYPE_MOVE_SEND;
        descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
        consumeAndReinterpretCastTo<mach_msg_port_descriptor_t>(remaining) = descriptor;
    }

    if (bodyIsOutOfLine) {
        mach_msg_ool_descriptor_t descriptor { };
        descriptor.address = const_cast<uint8_t*>(body.data());
        descriptor.size = static_cast<mach_msg_size_t>(body.size());
        descriptor.deallocate = false;
        descriptor.copy = MACH_MSG_VIRTUAL_COPY;
        descriptor.type = MACH_MSG_OOL_DESCRIPTOR;
        consumeAndReinterpretCastTo<mach_msg_ool_descriptor_t>(remaining) = descriptor;
    } else
        memcpySpan(consumeSpan(remaining, body.size()), body);

    // round_msg() rounded the size of the message up, so zero the padding after the body.
    if (!remaining.empty())
        zeroSpan(remaining);

    return result;
}

UniqueRef<MachMessage> MachMessage::createPendingAttachments(Vector<Attachment>&& attachments)
{
    size_t attachmentCount = attachments.size();
    ASSERT(attachmentCount && attachmentCount <= maxInlinePortDescriptorsPerMessage);
    // Header, body, and one inline port descriptor per attachment; never out-of-line, always well
    // under maxMessageSize because attachmentCount <= maxInlinePortDescriptorsPerMessage.
    size_t messageSize = round_msg(sizeof(mach_msg_header_t) + sizeof(mach_msg_body_t)
        + attachmentCount * sizeof(mach_msg_port_descriptor_t));
    ASSERT(messageSize <= maxMessageSize);

    void* memory = fastMalloc(sizeof(MachMessage) + messageSize);
    auto result = makeUniqueRefFromNonNullUniquePtr(std::unique_ptr<MachMessage> { new (NotNull, memory) MachMessage { } });

    auto remaining = unsafeMakeSpan(reinterpret_cast<std::byte*>(result->m_messageHeader), messageSize);
    consumeAndReinterpretCastTo<mach_msg_header_t>(remaining) = {
        .msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0) | MACH_MSGH_BITS_COMPLEX,
        .msgh_size = static_cast<mach_msg_size_t>(messageSize),
        .msgh_remote_port = MACH_PORT_NULL,
        .msgh_local_port = MACH_PORT_NULL,
        .msgh_voucher_port = MACH_PORT_NULL,
        .msgh_id = webkitMachMessageAddPendingAttachmentsID
    };
    consumeAndReinterpretCastTo<mach_msg_body_t>(remaining) = {
        .msgh_descriptor_count = static_cast<mach_msg_size_t>(attachmentCount)
    };

    // Written in the order given, the last encoded first, which is the order that receiveDecoder() collects
    // them in and adds them to the following message in.
    for (auto& attachment : attachments) {
        mach_msg_port_descriptor_t descriptor { };
        descriptor.name = attachment.leakSendRight();
        descriptor.disposition = MACH_MSG_TYPE_MOVE_SEND;
        descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
        consumeAndReinterpretCastTo<mach_msg_port_descriptor_t>(remaining) = descriptor;
    }

    // round_msg() rounded the size up, so zero any padding after the descriptors.
    if (!remaining.empty())
        zeroSpan(remaining);

    return result;
}

Vector<UniqueRef<MachMessage>, 1> MachMessage::createMessages(UniqueRef<Encoder>&& encoderIn)
{
    // Take the encoder, so that a refused one is released with its attachments here.
    auto encoder = WTF::move(encoderIn);
    Vector<UniqueRef<MachMessage>, 1> messages;

    // Reject an over-large attachment set up front, so that a sender cannot expand an arbitrarily large
    // attachment vector into many pending attachments messages instead of being dropped.
    if (encoder->attachmentCount() > maxAttachmentCount)
        return messages;

    // While the attachments exceed what one message may carry inline, peel batches off the end into
    // pending attachments messages, sent before the message that carries the remaining, leading attachments.
    while (encoder->attachmentCount() > maxInlinePortDescriptorsPerMessage) {
        Vector<Attachment> batch;
        batch.reserveInitialCapacity(maxInlinePortDescriptorsPerMessage);
        for (size_t i = 0; i < maxInlinePortDescriptorsPerMessage; ++i)
            batch.append(encoder->takeLastAttachment());

        messages.append(createPendingAttachments(WTF::move(batch)));
    }

    messages.append(createBody(WTF::move(encoder)));
    return messages;
}

MachMessage::SendResult MachMessage::send(mach_port_t destination, Timeout timeout)
{
    RELEASE_ASSERT(m_state != State::Sent);

    auto& header = *m_messageHeader;

    // A message returned from a timed-out send holds its own reference to the destination, so
    // readdressing it would leak that reference. Its msgh_remote_port should be one of:
    //
    // - `destination`, if the port was alive when the kernel returned the message.
    // - MACH_PORT_DEAD, if the port died before the kernel returned the message.
    RELEASE_ASSERT_IMPLIES(m_state == State::TimedOut, header.msgh_remote_port == destination || header.msgh_remote_port == MACH_PORT_DEAD);

    if (m_state == State::NotSent)
        header.msgh_remote_port = destination;

    auto result = sendResult(sendMessage(&header, timeout), m_messageName);
    switch (result) {
    case SendResult::Success:
        m_state = State::Sent;
        m_outOfLineBody = { };
        break;

    case SendResult::Timeout:
        // On timeout, the kernel rewrites the message in place with an OOL descriptor that points
        // to a new mapping that the message owns. So we can release the original OOL body.
        m_state = State::TimedOut;
        m_outOfLineBody = { };
        break;

    case SendResult::InvalidDestinationPort:
        break;
    }
    return result;
}

std::expected<UniqueRef<Decoder>, MachMessage::ReceiveResult> MachMessage::receiveDecoder(mach_port_t port, Vector<Attachment>& pendingAttachments, Timeout timeout)
{
    ASSERT(MACH_PORT_VALID(port));
    ASSERT(pendingAttachments.size() <= maxAttachmentCount);
    if (pendingAttachments.size() > maxAttachmentCount)
        return makeUnexpected(ReceiveResult::InvalidMessage);

    MessageBuffer buffer;
    auto message = buffer.allocate(MessageBuffer::inlineStorageSize);
    auto* header = &reinterpretCastSpanStartTo<mach_msg_header_t>(message);

    mach_msg_option_t timeoutOption = 0;
    mach_msg_timeout_t timeoutMilliseconds = MACH_MSG_TIMEOUT_NONE;
    if (!timeout.isInfinity()) {
        timeoutOption = MACH_RCV_TIMEOUT;
        timeoutMilliseconds = timeout.secondsUntilDeadline().millisecondsAs<mach_msg_timeout_t>();
    }
    // MACH_RCV_LARGE leaves a message that does not fit in the queue and sets msgh_size to the size it
    // needs, so that it can be received into storage of that size instead.
    mach_msg_option_t options = MACH_RCV_MSG | MACH_RCV_LARGE | MACH_RCV_VOUCHER | timeoutOption;
    kern_return_t kr = ::mach_msg(header, options, 0, message.size(), port, timeoutMilliseconds, MACH_PORT_NULL);
    if (kr == MACH_RCV_TOO_LARGE) {
        auto largeMessageSize = checkedSum<size_t>(header->msgh_size, MAX_TRAILER_SIZE);
        if (largeMessageSize.hasOverflowed() || largeMessageSize > maxMessageSize + MAX_TRAILER_SIZE) [[unlikely]] {
            // No legitimate sender sends a message this large. Receiving it without MACH_RCV_LARGE dequeues
            // and destroys it, so that it does not sit in the queue and be found again on every receive.
            ::mach_msg(header, options & ~MACH_RCV_LARGE, 0, message.size(), port, timeoutMilliseconds, MACH_PORT_NULL);
            return makeUnexpected(ReceiveResult::InvalidMessage);
        }
        message = buffer.allocate(largeMessageSize);
        if (message.empty()) [[unlikely]]
            return makeUnexpected(ReceiveResult::OutOfMemory);
        header = &reinterpretCastSpanStartTo<mach_msg_header_t>(message);
        kr = ::mach_msg(header, options, 0, message.size(), port, timeoutMilliseconds, MACH_PORT_NULL);
        ASSERT(kr != MACH_RCV_TOO_LARGE);
    }
    if (kr == MACH_RCV_TIMED_OUT)
        return makeUnexpected(ReceiveResult::NoMessage);

    if (kr != MACH_MSG_SUCCESS)
        return makeUnexpected(ReceiveResult::InvalidMessage);

    // NOTE: currently XNU mach_msg_ool_descriptor_t::deallocate == false for mach_msg_ool_descriptor_t::copy == MACH_MSG_PHYSICAL_COPY.
    // We do not deallocate the vm allocations of these malicious messages, rather leak the mapping. See more: <rdar://problem/62086358>.
    auto destroyMessage = makeScopeExit([header] {
        ::mach_msg_destroy(header);
    });

    switch (header->msgh_id) {
    case MACH_NOTIFY_NO_SENDERS:
        return makeUnexpected(ReceiveResult::NoSenders);

    case webkitMachMessageID:
        break;

    case webkitMachMessageAddPendingAttachmentsID: {
        // A pending attachments message carries only inline port descriptors.
        if (header->msgh_size > message.size() || header->msgh_size < sizeof(mach_msg_header_t)) [[unlikely]]
            return makeUnexpected(ReceiveResult::InvalidMessage);
        if (!(header->msgh_bits & MACH_MSGH_BITS_COMPLEX)) [[unlikely]]
            return makeUnexpected(ReceiveResult::InvalidMessage);

        auto remaining = message.subspan(sizeof(mach_msg_header_t), header->msgh_size - sizeof(mach_msg_header_t));
        // Cast ok: MACH_MSGH_BITS_COMPLEX -> kernel guarantees remaining at least mach_msg_body_t.
        auto& body = consumeAndReinterpretCastTo<mach_msg_body_t>(remaining);
        mach_msg_size_t descriptorCount = body.msgh_descriptor_count;
        if (!descriptorCount || descriptorCount > maxInlinePortDescriptorsPerMessage) [[unlikely]]
            return makeUnexpected(ReceiveResult::InvalidMessage);

        // A compromised sender can send pending attachments messages and never send the message they belong to,
        // making this (possibly privileged) receiver hold attacker-controlled send rights without bound.
        // Cap them at the most attachments a message may have: a real message never exceeds that.
        if (descriptorCount > maxAttachmentCount - pendingAttachments.size()) [[unlikely]] {
            pendingAttachments.clear();
            return makeUnexpected(ReceiveResult::InvalidMessage);
        }

        auto declaredSize = checkedSum<size_t>(sizeof(mach_msg_header_t) + sizeof(mach_msg_body_t), static_cast<size_t>(descriptorCount) * sizeof(mach_msg_port_descriptor_t));
        if (declaredSize.hasOverflowed() || declaredSize > header->msgh_size) [[unlikely]]
            return makeUnexpected(ReceiveResult::InvalidMessage);

        for (; descriptorCount; --descriptorCount) {
            // Cast ok: bounds checked by declaredSize above.
            if (reinterpretCastSpanStartTo<const mach_msg_type_descriptor_t>(remaining).type != MACH_MSG_PORT_DESCRIPTOR) [[unlikely]]
                return makeUnexpected(ReceiveResult::InvalidMessage);
            auto& descriptor = consumeAndReinterpretCastTo<mach_msg_port_descriptor_t>(remaining);
            if (descriptor.disposition != MACH_MSG_TYPE_PORT_SEND) [[unlikely]]
                return makeUnexpected(ReceiveResult::InvalidMessage);
            pendingAttachments.append(Attachment { MachSendRight::adopt(std::exchange(descriptor.name, MACH_PORT_NULL)) });
        }
        return makeUnexpected(ReceiveResult::PendingAttachments);
    }

    case MACH_NOTIFY_SEND_ONCE:
    default:
        return makeUnexpected(ReceiveResult::Notification);
    }

    if (header->msgh_size > message.size() || header->msgh_size < sizeof(mach_msg_header_t)) [[unlikely]] {
        ASSERT_NOT_REACHED();
        return makeUnexpected(ReceiveResult::InvalidMessage);
    }
    auto remaining = message.subspan(sizeof(mach_msg_header_t), header->msgh_size - sizeof(mach_msg_header_t));

    std::unique_ptr<Decoder> decoder;
    if (!(header->msgh_bits & MACH_MSGH_BITS_COMPLEX)) {
        // The message that follows pending attachments messages carries the rest of the attachments, at least one.
        if (!pendingAttachments.isEmpty()) [[unlikely]] {
            pendingAttachments.clear();
            return makeUnexpected(ReceiveResult::InvalidMessage);
        }
        decoder = Decoder::create(spanReinterpretCast<const uint8_t>(remaining), { });
    } else {
        // Cast ok: MACH_MSGH_BITS_COMPLEX -> kernel guarantees remaining at least mach_msg_body_t.
        auto& body = consumeAndReinterpretCastTo<mach_msg_body_t>(remaining);
        mach_msg_size_t descriptorCount = body.msgh_descriptor_count;
        if (!descriptorCount || descriptorCount > maxInlinePortDescriptorsPerMessage + 1) [[unlikely]]
            return makeUnexpected(ReceiveResult::InvalidMessage);

        // A port descriptor each, up to the descriptor of an out-of-line body. An out-of-line array of port
        // names is not a form this accepts, because it is not one that can be sent.
        Vector<Attachment> attachments;
        for (; descriptorCount; --descriptorCount) {
            // Cast ok: descriptorCount > 0 -> kernel guarantees remaining has at least mach_msg_type_descriptor_t.
            if (reinterpretCastSpanStartTo<const mach_msg_type_descriptor_t>(remaining).type != MACH_MSG_PORT_DESCRIPTOR)
                break;
            // Cast ok: type == MACH_MSG_PORT_DESCRIPTOR -> kernel guarantees remaining has at least mach_msg_port_descriptor_t.
            auto& descriptor = consumeAndReinterpretCastTo<mach_msg_port_descriptor_t>(remaining);
            // Note: A null send right is a valid attachment currently.
            if (descriptor.disposition != MACH_MSG_TYPE_PORT_SEND) [[unlikely]]
                return makeUnexpected(ReceiveResult::InvalidMessage);
            attachments.append(Attachment { MachSendRight::adopt(std::exchange(descriptor.name, MACH_PORT_NULL)) });
        }

        if (!pendingAttachments.isEmpty()) {
            if (attachments.isEmpty()) [[unlikely]] {
                pendingAttachments.clear();
                return makeUnexpected(ReceiveResult::InvalidMessage);
            }
            // The pending attachments are the trailing attachments of this message, the last encoded first.
            // The decoder takes attachments from the end, so the ones decoded last go first.
            pendingAttachments.appendVector(WTF::move(attachments));
            attachments = std::exchange(pendingAttachments, { });
        }

        if (descriptorCount) {
            // Cast ok: descriptorCount == 1 -> kernel guarantees remaining has at least mach_msg_type_descriptor_t.
            if (descriptorCount != 1 || reinterpretCastSpanStartTo<const mach_msg_type_descriptor_t>(remaining).type != MACH_MSG_OOL_DESCRIPTOR) [[unlikely]]
                return makeUnexpected(ReceiveResult::InvalidMessage);

            // Cast ok: type == MACH_MSG_OOL_DESCRIPTOR -> kernel guarantees remaining has mach_msg_ool_descriptor_t.
            auto& descriptor = consumeAndReinterpretCastTo<mach_msg_ool_descriptor_t>(remaining);
            if (!descriptor.address || !descriptor.size) [[unlikely]]
                return makeUnexpected(ReceiveResult::InvalidMessage);

            auto span = unsafeMakeSpan(static_cast<uint8_t*>(descriptor.address), descriptor.size);
            descriptor.deallocate = false; // Adopt the mapping. Failing create will also invoke the destruction handler.
            decoder = Decoder::create(span, [address = descriptor.address, size = descriptor.size](auto) {
                vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(address), size);
            }, WTF::move(attachments));
        } else
            decoder = Decoder::create(spanReinterpretCast<const uint8_t>(remaining), WTF::move(attachments));
    }

    if (!decoder)
        return makeUnexpected(ReceiveResult::InvalidMessage);

#if PLATFORM(MAC)
    decoder->setImportanceAssertion(ImportanceAssertion { header });
#endif

    return makeUniqueRefFromNonNullUniquePtr(WTF::move(decoder));
}

std::expected<UniqueRef<Decoder>, MachMessage::ReceiveResult> MachMessage::receiveDecoder(mach_port_t port, Timeout timeout)
{
    Vector<Attachment> pendingAttachments;
    while (true) {
        auto result = receiveDecoder(port, pendingAttachments, timeout);
        if (result || result.error() != ReceiveResult::PendingAttachments)
            return result;
    }
}

} // namespace IPC
