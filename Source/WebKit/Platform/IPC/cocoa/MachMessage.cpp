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

#include <mach/mach.h>

namespace IPC {

// Version of round_msg() using CheckedSize for extra safety.
static inline CheckedSize safeRoundMsg(CheckedSize value)
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

}

#endif // PLATFORM(COCOA)
