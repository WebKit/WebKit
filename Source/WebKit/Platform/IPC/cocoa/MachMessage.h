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

#if OS(DARWIN)

#include "Encoder.h"
#include "Timeout.h"
#include <expected>
#include <mach/message.h>
#include <memory>
#include <wtf/FastMalloc.h>
#include <wtf/MmapSpan.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UniqueRef.h>

namespace IPC {

class Decoder;

enum class MachMessageSendResult : uint8_t {
    Success, // The message was sent.
    Timeout, // The destination had no room for the message. Sending it again as it is can send it.
    InvalidDestinationPort, // The destination does not receive messages anymore. The message is unchanged.
};

// A Mach message built from an Encoder and owning all Mach resources it references:
// - Before send(): attachment send rights and the Encoder's out-of-line MmapSpan (if any).
// - After a timed-out send() (pseudo-receive): a send right to the destination port, attachment
//   send rights, and a kernel-allocated mapping of any out-of-line body.
// Destroying an unsent or timed-out message releases everything it owns.
class MachMessage {
    WTF_MAKE_NONCOPYABLE(MachMessage);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(MachMessage);
public:
    static constexpr size_t maxAttachmentCount = 16383;

    using SendResult = MachMessageSendResult;

    // Returns nullptr if the encoder has more than maxAttachmentCount attachments.
    static std::unique_ptr<MachMessage> create(UniqueRef<Encoder>&&);

    ~MachMessage();

    // Sends the message to `destination`. If a prior send() timed out, `destination` must match the
    // original destination port (or the port must now be MACH_PORT_DEAD).
    SendResult send(mach_port_t destination, Timeout = Timeout::now());

    enum class ReceiveResult : uint8_t {
        NoMessage,
        NoSenders,
        Notification,
        InvalidMessage,
        OutOfMemory, // The message could not be received, so it is still in the queue.
    };

    static std::expected<UniqueRef<Decoder>, ReceiveResult> receiveDecoder(mach_port_t, Timeout = Timeout::now());

    ReceiverName messageReceiverName() const { return receiverName(messageName()); }
    MessageName messageName() const { return m_messageName; }

    bool wasReturnedFromSend() const { return m_state == State::TimedOut; }

private:
    MachMessage(MessageName, MmapSpan<uint8_t>&& outOfLineBody);

    enum class State : uint8_t {
        NotSent, // mach_msg() was not called, or it failed with MACH_SEND_INVALID_DEST.
        TimedOut, // mach_msg() failed with MACH_SEND_TIMED_OUT and copied the message back out.
        Sent, // mach_msg() succeeded.
    };

    MessageName m_messageName;
    State m_state { State::NotSent };
    MmapSpan<uint8_t> m_outOfLineBody;
    mach_msg_header_t m_messageHeader[];
};

} // namespace IPC

#endif // OS(DARWIN)
