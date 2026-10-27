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

#if PLATFORM(COCOA)

#include "Encoder.h"
#include "Logging.h"
#include <WebCore/SharedMemory.h>
#include <mach/mach.h>
#include <wtf/MachSendRight.h>
#include <wtf/Vector.h>

namespace IPC {

// Version of round_msg() using CheckedSize for extra safety.
static inline CheckedSize LEGACY_NODELETE safeRoundMsg(CheckedSize value)
{
    constexpr size_t alignment = sizeof(natural_t);
    return ((value + (alignment - 1)) / alignment) * alignment;
}

std::unique_ptr<MachMessage> MachMessage::create(MessageName messageName, size_t size)
{
    auto bufferSize = CheckedSize(sizeof(MachMessage)) + size;
    if (bufferSize.hasOverflowed())
        return nullptr;
    void* memory = WTF::fastZeroedMalloc(bufferSize);
    return std::unique_ptr<MachMessage> { new (NotNull, memory) MachMessage { messageName, size } };
}

std::unique_ptr<MachMessage> MachMessage::create(UniqueRef<Encoder>&& encoder)
{
    auto attachments = encoder->releaseAttachments();
    auto numberOfPortDescriptors = attachments.size();

    bool messageBodyIsOOL = false;
    auto size = messageSize(encoder->span().size(), numberOfPortDescriptors, messageBodyIsOOL);
    if (size.hasOverflowed()) [[unlikely]]
        return nullptr;

    if (size > inlineMessageMaxSize) {
        messageBodyIsOOL = true;
        size = messageSize(0, numberOfPortDescriptors, messageBodyIsOOL);
        if (size.hasOverflowed()) [[unlikely]]
            return nullptr;
    }

    size_t safeSize = size;
    auto message = create(encoder->messageName(), safeSize);
    if (!message)
        return nullptr;

    auto messageSpan = message->span();
    auto& header = consumeAndReinterpretCastTo<mach_msg_header_t>(messageSpan);
    header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    header.msgh_size = safeSize;
    header.msgh_remote_port = MACH_PORT_NULL; // send() sets the destination port.
    header.msgh_local_port = MACH_PORT_NULL;
    header.msgh_id = messageBodyIsOOL ? outOfLineBodyMessageID : inlineBodyMessageID;

    bool isComplex = numberOfPortDescriptors || messageBodyIsOOL;
    if (isComplex) {
        header.msgh_bits |= MACH_MSGH_BITS_COMPLEX;

        auto& body = consumeAndReinterpretCastTo<mach_msg_body_t>(messageSpan);
        body.msgh_descriptor_count = numberOfPortDescriptors + messageBodyIsOOL;

        for (auto& attachment : attachments)
            writePortDescriptor(messageSpan, attachment.leakSendRight());

        if (messageBodyIsOOL) {
            auto& descriptor = consumeAndReinterpretCastTo<mach_msg_ool_descriptor_t>(messageSpan);
            auto buffer = encoder->mutableSpan();
            descriptor.address = buffer.data();
            descriptor.size = buffer.size();
            descriptor.copy = MACH_MSG_VIRTUAL_COPY;
            // The encoder, which the message keeps, owns the memory.
            descriptor.deallocate = false;
            descriptor.type = MACH_MSG_OOL_DESCRIPTOR;
        }
    }

    // Copy the data if it is not being sent out-of-line.
    if (!messageBodyIsOOL)
        memcpySpan(messageSpan, encoder->span());

    message->m_encoder = encoder.moveToUniquePtr();
    return message;
}

static Vector<MachSendRight> extractPortDescriptorsFromMessage(MachMessage& message)
{
    auto messageSpan = message.span();
    auto& header = consumeAndReinterpretCastTo<mach_msg_header_t>(messageSpan);

    if (!(header.msgh_bits & MACH_MSGH_BITS_COMPLEX))
        return { };

    auto& body = consumeAndReinterpretCastTo<mach_msg_body_t>(messageSpan);
    bool hasOOLDescriptor = (header.msgh_id == MachMessage::outOfLineBodyMessageID);
    auto portCount = body.msgh_descriptor_count - (hasOOLDescriptor ? 1 : 0);

    Vector<MachSendRight> ports(portCount, [&](size_t) {
        auto& descriptor = consumeAndReinterpretCastTo<mach_msg_port_descriptor_t>(messageSpan);
        return MachSendRight::adopt(descriptor.name);
    });

    message.leakDescriptors();
    return ports;
}

std::unique_ptr<MachMessage> MachMessage::createWithSharedMemoryBody(std::unique_ptr<MachMessage> message)
{
    ASSERT(message->m_encoder);
    auto& encoder = *message->m_encoder;

    // On MACH_SEND_TOO_LARGE the kernel rejects the send without consuming
    // any descriptors, so the ports in the message are still valid.
    auto extractedPorts = extractPortDescriptorsFromMessage(*message);

    // Create a VM-copy memory entry directly from the encoder's data.
    auto shmHandle = WebCore::SharedMemoryHandle::createVMCopy(encoder.span(), WebCore::SharedMemoryProtection::ReadOnly);
    if (!shmHandle) {
        RELEASE_LOG_ERROR(IPC, "createWithSharedMemoryBody: Failed to create memory entry");
        CRASH_WITH_INFO(std::to_underlying(encoder.messageName()));
    }
    uint64_t shmSize = shmHandle->size();
    auto shmSendRight = shmHandle->releaseHandle();

    // Build the new message: extractedPorts + shm port descriptor + uint64_t size inline.
    size_t newPortCount = extractedPorts.size() + 1;
    auto newMessageSize = messageSize(sizeof(uint64_t), newPortCount, 0);
    if (newMessageSize.hasOverflowed()) [[unlikely]]
        return nullptr;

    size_t safeNewMessageSize = newMessageSize;
    auto newMessage = create(encoder.messageName(), safeNewMessageSize);
    if (!newMessage)
        return nullptr;

    auto newMessageSpan = newMessage->span();
    auto& newHeader = consumeAndReinterpretCastTo<mach_msg_header_t>(newMessageSpan);
    newHeader.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0) | MACH_MSGH_BITS_COMPLEX;
    newHeader.msgh_size = safeNewMessageSize;
    newHeader.msgh_remote_port = MACH_PORT_NULL; // send() sets the destination port.
    newHeader.msgh_local_port = MACH_PORT_NULL;
    newHeader.msgh_id = sharedMemoryBodyMessageID;

    auto& newBody = consumeAndReinterpretCastTo<mach_msg_body_t>(newMessageSpan);
    newBody.msgh_descriptor_count = newPortCount;

    // Original port descriptors (message attachments).
    for (auto& sendRight : extractedPorts)
        writePortDescriptor(newMessageSpan, sendRight.leakSendRight());

    // SharedMemory port descriptor (last).
    writePortDescriptor(newMessageSpan, shmSendRight.leakSendRight());

    // Inline body: uint64_t shared memory size.
    memcpySpan(newMessageSpan, asByteSpan(shmSize));

    RELEASE_LOG_ERROR(IPC, "createWithSharedMemoryBody: message '%" PUBLIC_LOG_STRING "' (encoderBodySize=%zu, newMessageSize=%zu, portDescriptors=%zu, shmSize=%llu)", description(newMessage->messageName()).characters(), encoder.span().size(), safeNewMessageSize, newPortCount, shmSize);

    return newMessage;
}

void MachMessage::writePortDescriptor(std::span<uint8_t>& messageSpan, mach_port_t sendRight)
{
    auto& descriptor = consumeAndReinterpretCastTo<mach_msg_port_descriptor_t>(messageSpan);
    descriptor.name = sendRight;
    descriptor.disposition = MACH_MSG_TYPE_MOVE_SEND;
    descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
}

MachMessage::MachMessage(MessageName messageName, size_t size)
    : m_messageName { messageName }
    , m_size { size }
{
}

MachMessage::~MachMessage()
{
    if (m_shouldFreeDescriptors)
        ::mach_msg_destroy(header());
}

CheckedSize MachMessage::messageSize(size_t bodySize, size_t portDescriptorCount, size_t memoryDescriptorCount)
{
    CheckedSize messageSize = sizeof(mach_msg_header_t);
    messageSize += bodySize;

    if (portDescriptorCount || memoryDescriptorCount) {
        messageSize += sizeof(mach_msg_body_t);
        messageSize += (portDescriptorCount * sizeof(mach_msg_port_descriptor_t));
        messageSize += (memoryDescriptorCount * sizeof(mach_msg_ool_descriptor_t));
    }

    return safeRoundMsg(messageSize);
}

void MachMessage::leakDescriptors()
{
    m_shouldFreeDescriptors = false;
}

kern_return_t MachMessage::send(mach_port_t destination)
{
    // A message returned from a timed-out send owns a moved send right to its destination.
    // Overwriting msgh_remote_port would leak that right.
    ASSERT_IMPLIES(m_wasReturnedFromSend, destination == header()->msgh_remote_port);
    if (!m_wasReturnedFromSend)
        header()->msgh_remote_port = destination;

    auto kr = mach_msg(header(), MACH_SEND_MSG | MACH_SEND_TIMEOUT | MACH_SEND_NOTIFY, m_size, 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    switch (kr) {
    case MACH_MSG_SUCCESS:
        // The kernel has already adopted the descriptors.
        leakDescriptors();
        break;
    case MACH_SEND_TIMED_OUT:
        m_wasReturnedFromSend = true;
        // The kernel replaced the out-of-line body with a mapping that the message now owns, so
        // the message no longer points into the encoder's buffer. The message also passed the size
        // check, so a retry does not get MACH_SEND_TOO_LARGE and does not need the encoder.
        m_encoder = nullptr;
        break;
    default:
        break;
    }
    return kr;
}

}

#endif // PLATFORM(COCOA)
