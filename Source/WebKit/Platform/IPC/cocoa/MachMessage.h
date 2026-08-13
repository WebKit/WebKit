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
#include <mach/message.h>
#include <memory>
#include <wtf/Expected.h>
#include <wtf/FastMalloc.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UniqueRef.h>

namespace IPC {

class Decoder;

// A mach message that the kernel returned from a send that failed after it had copied the message in,
// i.e. from a Mach pseudo receive. Such a message owns everything it names -- a reference to the
// destination, the port rights of its attachments and a mapping of its body -- and sending it again as
// it is, with send(), is the only way to deliver it. Messages that are sent without failing this way
// never need an instance: sendEncoder() builds and sends them without one.
class MachMessage {
    WTF_MAKE_NONCOPYABLE(MachMessage);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(MachMessage);
public:
    static constexpr size_t inlinePortDescriptorMaxCount = 127;
    static constexpr size_t maxAttachmentCount = 16383;

    enum class SendResult : uint8_t {
        Success, // The message was sent.
        Timeout, // The destination had no room for the message. Sending it again as it is can send it.
        InvalidDestinationPort, // The destination does not receive messages anymore.
        InvalidEncoder, // The encoder holds more than a mach message can carry, so no message was built.
        OutOfMemory, // The message could not be built or kept for a retry, so it was dropped.
    };

    struct SendEncoderResult {
        SendResult result;
        // The message, set when the result is Timeout: the kernel returned it, and sending it again with
        // send() is the only way to deliver it.
        std::unique_ptr<MachMessage> retryableMessage;
    };

    ~MachMessage();

    // Sends `encoder` to `destination` as a mach message, returns a message to retry or a failure.
    // Always consumes the encoder.
    static SendEncoderResult sendEncoder(UniqueRef<Encoder>&&, mach_port_t destination, Timeout = Timeout::now());

    // Sends the message as it is. Releases the message resources on success, and retains them for
    // another retry otherwise.
    SendResult send(Timeout = Timeout::now());

    enum class ReceiveResult : uint8_t {
        NoMessage,
        NoSenders,
        Notification,
        InvalidMessage,
    };

    static Expected<UniqueRef<Decoder>, ReceiveResult> receiveDecoder(mach_port_t, Timeout = Timeout::now());

    ReceiverName messageReceiverName() const { return receiverName(messageName()); }
    MessageName messageName() const { return m_messageName; }

private:
    MachMessage(MessageName);

    static std::unique_ptr<MachMessage> adoptPseudoReceived(MessageName, std::span<const std::byte>);

    MessageName m_messageName;
    bool m_shouldDestroy { true };
    mach_msg_header_t m_messageHeader[];
};

} // namespace IPC

#endif // OS(DARWIN)
