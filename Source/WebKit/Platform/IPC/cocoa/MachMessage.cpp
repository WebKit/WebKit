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

// The maximum mach message data size. If the body or attachment descriptors do not fit, they are sent out-of-line.
// With current limits, fits:
// 0 attachments and 4072 bytes of inline body.
// 127 attachments as inline port descriptors and 2544 bytes of inline body.
// out-of-line port descriptors and 4052 bytes of inline body.
static constexpr size_t messageStorageSize = 4096;

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
        WebKit::logAndSetCrashLogMessage(errorMessage.utf8().data());
        CRASH_WITH_INFO(kr, std::to_underlying(messageName));
    }
}

} // namespace

MachMessage::MachMessage(MessageName messageName)
    : m_messageName { messageName }
{
}

MachMessage::~MachMessage()
{
    if (m_shouldDestroy)
        ::mach_msg_destroy(m_messageHeader);
}

std::unique_ptr<MachMessage> MachMessage::adoptPseudoReceived(MessageName messageName, std::span<const std::byte> message)
{
    // The message is copied over the whole of the allocation that follows the instance, so the memory does
    // not need to be zeroed.
    void* memory = nullptr;
    if (!tryFastMalloc(sizeof(MachMessage) + message.size()).getValue(memory)) [[unlikely]]
        return nullptr;
    std::unique_ptr<MachMessage> result { new (NotNull, memory) MachMessage { messageName } };
    auto newMessage = unsafeMakeSpan(reinterpret_cast<std::byte*>(result->m_messageHeader), message.size());
    memcpySpan(newMessage, message);
    return result;
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
//     mach_msg_port_descriptor_t attachments[attachmentCount]      // up to inlinePortDescriptorMaxCount,
//         | mach_msg_ool_ports_descriptor_t outOfLineAttachments;  // an array of port names beyond that
//     mach_msg_ool_descriptor_t outOfLineEncoderBody               // when the body does not fit,
//         | std::byte encoderBody[];                               // and otherwise the body itself

MachMessage::SendEncoderResult MachMessage::sendEncoder(UniqueRef<Encoder>&& encoderIn, mach_port_t destination, Timeout timeout)
{
    auto encoder = WTF::move(encoderIn);

    // The header and the descriptors of a message are bounded, because the number of attachments a message can
    // carry is bounded and more than inlinePortDescriptorMaxCount of them are named by one descriptor rather
    // than one each. That is what makes the size arithmetic in sendEncoder() safe without checking it.
    static constexpr size_t maxHeaderAndDescriptorsSize = sizeof(mach_msg_header_t) + sizeof(mach_msg_body_t) + MachMessage::inlinePortDescriptorMaxCount * sizeof(mach_msg_port_descriptor_t) + sizeof(mach_msg_ool_descriptor_t);
    static_assert(maxHeaderAndDescriptorsSize <= messageStorageSize);
    static_assert(MachMessage::inlinePortDescriptorMaxCount <= MachMessage::maxAttachmentCount);
    static_assert(MachMessage::maxAttachmentCount <= std::numeric_limits<mach_msg_size_t>::max() / sizeof(mach_port_name_t));

    auto body = encoder->span();
    size_t attachmentCount = encoder->attachmentCount();

    // To optimize the base-case, encoder does not record validity during encoding.
    if (attachmentCount > maxAttachmentCount) [[unlikely]]
        return { SendResult::InvalidEncoder, nullptr };

    bool attachmentsAreOutOfLine = attachmentCount > inlinePortDescriptorMaxCount;
    size_t attachmentDescriptorsSize = attachmentsAreOutOfLine ? sizeof(mach_msg_ool_ports_descriptor_t) : attachmentCount * sizeof(mach_msg_port_descriptor_t);
    size_t headerAndDescriptorsSize = sizeof(mach_msg_header_t) + (attachmentCount ? sizeof(mach_msg_body_t) + attachmentDescriptorsSize : 0);

    bool bodyIsOutOfLine = body.size() > messageStorageSize - headerAndDescriptorsSize;
    size_t messageSize = bodyIsOutOfLine ? round_msg(sizeof(mach_msg_header_t) + sizeof(mach_msg_body_t) + attachmentDescriptorsSize + sizeof(mach_msg_ool_descriptor_t)) : round_msg(headerAndDescriptorsSize + body.size());

    // Every form of every message fits this, which the static assertions above hold to.
    alignas(mach_msg_header_t) std::byte storage[messageStorageSize];
    auto message = std::span<std::byte> { storage }.first(messageSize);

    // The out-of-line array of port names, held here for the whole of the send: the descriptor says not to
    // deallocate it, so the kernel does not, and it is freed with this.
    MallocSpan<mach_port_name_t> attachmentNames;
    if (attachmentsAreOutOfLine) {
        attachmentNames = MallocSpan<mach_port_name_t>::tryMalloc(attachmentCount * sizeof(mach_port_name_t));
        if (!attachmentNames) [[unlikely]]
            return { SendResult::OutOfMemory, nullptr };
    }

    size_t descriptorCount = (attachmentsAreOutOfLine ? 1 : attachmentCount) + bodyIsOutOfLine;
    auto remaining = message;
    auto& header = consumeAndReinterpretCastTo<mach_msg_header_t>(remaining);
    header = {
        .msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0) | (descriptorCount ? MACH_MSGH_BITS_COMPLEX : 0),
        .msgh_size = static_cast<mach_msg_size_t>(messageSize),
        .msgh_remote_port = destination,
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
    if (attachmentsAreOutOfLine) {
        auto names = attachmentNames.mutableSpan();
        auto name = names.begin();
        for (auto attachment = attachments.rbegin(); attachment != attachments.rend(); ++attachment)
            *name++ = attachment->leakSendRight();
        mach_msg_ool_ports_descriptor_t descriptor { };
        descriptor.address = names.data();
        descriptor.count = static_cast<mach_msg_size_t>(attachmentCount);
        descriptor.deallocate = false;
        descriptor.copy = MACH_MSG_PHYSICAL_COPY;
        descriptor.disposition = MACH_MSG_TYPE_MOVE_SEND;
        descriptor.type = MACH_MSG_OOL_PORTS_DESCRIPTOR;
        consumeAndReinterpretCastTo<mach_msg_ool_ports_descriptor_t>(remaining) = descriptor;
    } else {
        for (auto attachment = attachments.rbegin(); attachment != attachments.rend(); ++attachment) {
            mach_msg_port_descriptor_t descriptor { };
            descriptor.name = attachment->leakSendRight();
            descriptor.disposition = MACH_MSG_TYPE_MOVE_SEND;
            descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
            consumeAndReinterpretCastTo<mach_msg_port_descriptor_t>(remaining) = descriptor;
        }
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

    auto result = sendResult(sendMessage(&header, timeout), encoder->messageName());
    if (result == SendResult::Success)
        return { result, nullptr };

    // The kernel copies the message back out when the send times out, so that the resources it named
    // are the caller's again.
    if (result == SendResult::Timeout) {
        // Kernel rewrote the resources. Adopt them for retry.
        auto retryableMessage = adoptPseudoReceived(encoder->messageName(), message);
        if (retryableMessage)
            return { result, WTF::move(retryableMessage) };
        result = SendResult::OutOfMemory;
    }

    ::mach_msg_destroy(&header);
    return { result, nullptr };
}

MachMessage::SendResult MachMessage::send(Timeout timeout)
{
    auto result = sendResult(sendMessage(m_messageHeader, timeout), m_messageName);
    if (result == SendResult::Success)
        m_shouldDestroy = false;
    return result;
}

Expected<UniqueRef<Decoder>, MachMessage::ReceiveResult> MachMessage::receiveDecoder(mach_port_t port, Timeout timeout)
{
    ASSERT(MACH_PORT_VALID(port));
    alignas(mach_msg_header_t) std::byte messageStorage[messageStorageSize + MAX_TRAILER_SIZE];

    std::span<std::byte> message { messageStorage };
    auto* header = &reinterpretCastSpanStartTo<mach_msg_header_t>(message);

    mach_msg_option_t timeoutOption = 0;
    mach_msg_timeout_t timeoutMilliseconds = MACH_MSG_TIMEOUT_NONE;
    if (!timeout.isInfinity()) {
        timeoutOption = MACH_RCV_TIMEOUT;
        timeoutMilliseconds = timeout.secondsUntilDeadline().millisecondsAs<mach_msg_timeout_t>();
    }
    // We intentionally receive only messageStorage sized messages, as that is the maximum of any
    // legitime send. Intentionally not passing MACH_RCV_LARGE.
    kern_return_t kr = ::mach_msg(header, MACH_RCV_MSG | MACH_RCV_VOUCHER | timeoutOption, 0, message.size(), port, timeoutMilliseconds, MACH_PORT_NULL);
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
    if (!(header->msgh_bits & MACH_MSGH_BITS_COMPLEX))
        decoder = Decoder::create(spanReinterpretCast<const uint8_t>(remaining), { });
    else {
        // Cast ok: MACH_MSGH_BITS_COMPLEX -> kernel guarantees remaining at least mach_msg_body_t.
        auto& body = consumeAndReinterpretCastTo<mach_msg_body_t>(remaining);
        mach_msg_size_t descriptorCount = body.msgh_descriptor_count;
        if (!descriptorCount || descriptorCount > maxAttachmentCount + 1) [[unlikely]]
            return makeUnexpected(ReceiveResult::InvalidMessage);

        Vector<Attachment> attachments;
        // Cast ok: msgh_descriptor_count > 0 -> kernel guarantees remaining has at least one mach_msg_type_descriptor_t.
        if (reinterpretCastSpanStartTo<const mach_msg_type_descriptor_t>(remaining).type == MACH_MSG_OOL_PORTS_DESCRIPTOR) {
            // Cast ok: type == MACH_MSG_OOL_PORTS_DESCRIPTOR -> kernel guarantees remaining has at least mach_msg_ool_ports_descriptor_t.
            auto& descriptor = consumeAndReinterpretCastTo<mach_msg_ool_ports_descriptor_t>(remaining);
            --descriptorCount;
            if (descriptor.disposition != MACH_MSG_TYPE_PORT_SEND || !descriptor.address || !descriptor.count || descriptor.count > maxAttachmentCount) [[unlikely]]
                return makeUnexpected(ReceiveResult::InvalidMessage);

            auto names = unsafeMakeSpan(static_cast<mach_port_name_t*>(descriptor.address), descriptor.count);
            attachments = Vector<Attachment>(descriptor.count, [&](size_t i) {
                return Attachment { MachSendRight::adopt(std::exchange(names[i], MACH_PORT_NULL)) };
            });
        } else {
            // A port descriptor each, up to the descriptor of an out-of-line body.
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

} // namespace IPC
