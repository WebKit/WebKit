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

#pragma once

#if PLATFORM(COCOA)

#include "MessageNames.h"
#include <mach/message.h>
#include <memory>
#include <wtf/CheckedArithmetic.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UniqueRef.h>
#include <wtf/text/CString.h>

namespace IPC {

class Encoder;

class MachMessage {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(MachMessage);
public:
    // Messages larger than this are sent with their body out of line.
    static constexpr size_t inlineMessageMaxSize = 4096;

    // Arbitrary message IDs that do not collide with Mach notification messages (used my initials).
    static constexpr mach_msg_id_t inlineBodyMessageID = 0xdba0dba;
    static constexpr mach_msg_id_t outOfLineBodyMessageID = 0xdba1dba;
    static constexpr mach_msg_id_t sharedMemoryBodyMessageID = 0xdba2dba;

    static std::unique_ptr<MachMessage> create(MessageName, size_t);

    static std::unique_ptr<MachMessage> create(UniqueRef<Encoder>&&);

    // Builds a message that carries the attachments of `message` and its body in shared memory, to
    // retry a send that failed with MACH_SEND_TOO_LARGE. `message` must still have its encoder.
    static std::unique_ptr<MachMessage> createWithSharedMemoryBody(std::unique_ptr<MachMessage>);

    ~MachMessage();

    static CheckedSize LEGACY_NODELETE messageSize(size_t bodySize, size_t portDescriptorCount, size_t memoryDescriptorCount);

    size_t size() const { return m_size; }
    mach_msg_header_t* header() LIFETIME_BOUND { return m_messageHeader; }

    std::span<uint8_t> span() LIFETIME_BOUND { return unsafeMakeSpan(reinterpret_cast<uint8_t*>(m_messageHeader), m_size); }

    void LEGACY_NODELETE leakDescriptors();

    ReceiverName messageReceiverName() const { return receiverName(m_messageName); }
    MessageName messageName() const { return m_messageName; }

    // Sends the message to `destination` with MACH_SEND_TIMEOUT and a zero timeout, and with
    // MACH_SEND_NOTIFY, so that a send that times out because the destination's queue is full arms
    // the send-possible notification for it.
    //
    // - MACH_MSG_SUCCESS: the kernel took everything the message names.
    // - MACH_SEND_TIMED_OUT: the kernel returned the message (a pseudo-receive). The message again owns
    //   its port rights and memory, plus a moved send right to its destination. You can send it again
    //   as it is, but `destination` must be the same port. The message no longer keeps its encoder.
    // - Anything else: the kernel didn't take or change anything.
    kern_return_t send(mach_port_t destination);

    // Whether a send timed out and the kernel returned the message, see send().
    bool wasReturnedFromSend() const { return m_wasReturnedFromSend; }

private:
    MachMessage(MessageName, size_t);

    // Writes a port descriptor that moves `sendRight` to the start of `messageSpan`, and advances
    // `messageSpan` past it.
    static void writePortDescriptor(std::span<uint8_t>& messageSpan, mach_port_t sendRight);

    MessageName m_messageName;
    size_t m_size;
    bool m_shouldFreeDescriptors { true };
    bool m_wasReturnedFromSend { false };
    std::unique_ptr<Encoder> m_encoder;
    mach_msg_header_t m_messageHeader[];
};

}

#endif // PLATFORM(COCOA)
