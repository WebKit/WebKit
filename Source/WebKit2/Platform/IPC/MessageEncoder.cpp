/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "MessageEncoder.h"

#include "ArgumentCoders.h"
#include "DataReference.h"
#include "MessageFlags.h"
#include "MessageRecorder.h"
#include "StringReference.h"

namespace IPC {

static const uint8_t defaultMessageFlags = 0;

#if HAVE(DTRACE)
MessageEncoder::MessageEncoder(StringReference messageReceiverName, StringReference messageName, uint64_t destinationID, const uuid_t& UUID)
    : m_messageReceiverName(messageReceiverName)
    , m_messageName(messageName)
    , m_destinationID(destinationID)
{
    uuid_copy(m_UUID, UUID);
    encodeHeader();
}
#endif

MessageEncoder::MessageEncoder(StringReference messageReceiverName, StringReference messageName, uint64_t destinationID)
    : m_messageReceiverName(messageReceiverName)
    , m_messageName(messageName)
    , m_destinationID(destinationID)
{
#if HAVE(DTRACE)
    uuid_generate(m_UUID);
#endif
    encodeHeader();
}

MessageEncoder::~MessageEncoder()
{
}

void MessageEncoder::encodeHeader()
{
    ASSERT(!m_messageReceiverName.isEmpty());

    *this << defaultMessageFlags;
    *this << m_messageReceiverName;
    *this << m_messageName;
    *this << m_destinationID;
#if HAVE(DTRACE)
    *this << m_UUID;
#endif
}

bool MessageEncoder::isSyncMessage() const
{
    return *buffer() & SyncMessage;
}

bool MessageEncoder::shouldDispatchMessageWhenWaitingForSyncReply() const
{
    return *buffer() & DispatchMessageWhenWaitingForSyncReply;
}

void MessageEncoder::setIsSyncMessage(bool isSyncMessage)
{
    if (isSyncMessage)
        *buffer() |= SyncMessage;
    else
        *buffer() &= ~SyncMessage;
}

void MessageEncoder::setShouldDispatchMessageWhenWaitingForSyncReply(bool shouldDispatchMessageWhenWaitingForSyncReply)
{
    if (shouldDispatchMessageWhenWaitingForSyncReply)
        *buffer() |= DispatchMessageWhenWaitingForSyncReply;
    else
        *buffer() &= ~DispatchMessageWhenWaitingForSyncReply;
}

void MessageEncoder::setFullySynchronousModeForTesting()
{
    *buffer() |= UseFullySynchronousModeForTesting;
}

void MessageEncoder::wrapForTesting(std::unique_ptr<MessageEncoder> original)
{
    ASSERT(isSyncMessage());
    ASSERT(!original->isSyncMessage());

    original->setShouldDispatchMessageWhenWaitingForSyncReply(true);

    encodeVariableLengthByteArray(DataReference(original->buffer(), original->bufferSize()));

    Vector<Attachment> attachments = original->releaseAttachments();
    reserve(attachments.size());
    for (Attachment& attachment : attachments)
        addAttachment(WTFMove(attachment));
}

} // namespace IPC
