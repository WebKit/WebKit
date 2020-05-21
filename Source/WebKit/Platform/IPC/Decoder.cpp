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

#include "config.h"
#include "Decoder.h"

#include "ArgumentCoders.h"
#include "DataReference.h"
#include "MessageFlags.h"
#include <stdio.h>
#include <wtf/StdLibExtras.h>

#if PLATFORM(MAC)
#include "ImportanceAssertion.h"
#endif

namespace IPC {

static const uint8_t* copyBuffer(const uint8_t* buffer, size_t bufferSize)
{
    auto bufferCopy = static_cast<uint8_t*>(fastMalloc(bufferSize));
    memcpy(bufferCopy, buffer, bufferSize);

    return bufferCopy;
}

std::unique_ptr<Decoder> Decoder::create(const uint8_t* buffer, size_t bufferSize, void (*bufferDeallocator)(const uint8_t*, size_t), Vector<Attachment>&& attachments)
{
    auto decoder = makeUnique<Decoder>(buffer, bufferSize, bufferDeallocator, WTFMove(attachments));
    return decoder->isValid() ? WTFMove(decoder) : nullptr;
}

Decoder::Decoder(const uint8_t* buffer, size_t bufferSize, void (*bufferDeallocator)(const uint8_t*, size_t), Vector<Attachment>&& attachments)
    : m_buffer { bufferDeallocator ? buffer : copyBuffer(buffer, bufferSize) }
    , m_bufferPos { m_buffer }
    , m_bufferEnd { m_buffer + bufferSize }
    , m_bufferDeallocator { bufferDeallocator }
    , m_attachments { WTFMove(attachments) }
{
    if (reinterpret_cast<uintptr_t>(m_buffer) % alignof(uint64_t)) {
        markInvalid();
        return;
    }

    if (!decode(m_messageFlags))
        return;

    if (!decode(m_messageName))
        return;

    if (!decode(m_destinationID))
        return;
}

Decoder::~Decoder()
{
    ASSERT(m_buffer);

    if (m_bufferDeallocator)
        m_bufferDeallocator(m_buffer, m_bufferEnd - m_buffer);
    else
        fastFree(const_cast<uint8_t*>(m_buffer));

    // FIXME: We need to dispose of the mach ports in cases of failure.

#if HAVE(QOS_CLASSES)
    if (m_qosClassOverride)
        pthread_override_qos_class_end_np(m_qosClassOverride);
#endif
}

bool Decoder::isSyncMessage() const
{
    return m_messageFlags.contains(MessageFlags::SyncMessage);
}

ShouldDispatchWhenWaitingForSyncReply Decoder::shouldDispatchMessageWhenWaitingForSyncReply() const
{
    if (m_messageFlags.contains(MessageFlags::DispatchMessageWhenWaitingForSyncReply))
        return ShouldDispatchWhenWaitingForSyncReply::Yes;
    if (m_messageFlags.contains(MessageFlags::DispatchMessageWhenWaitingForUnboundedSyncReply))
        return ShouldDispatchWhenWaitingForSyncReply::YesDuringUnboundedIPC;
    return ShouldDispatchWhenWaitingForSyncReply::No;
}

bool Decoder::shouldUseFullySynchronousModeForTesting() const
{
    return m_messageFlags.contains(MessageFlags::UseFullySynchronousModeForTesting);
}

#if PLATFORM(MAC)
void Decoder::setImportanceAssertion(std::unique_ptr<ImportanceAssertion> assertion)
{
    m_importanceAssertion = WTFMove(assertion);
}
#endif

std::unique_ptr<Decoder> Decoder::unwrapForTesting(Decoder& decoder)
{
    ASSERT(decoder.isSyncMessage());

    Vector<Attachment> attachments;
    Attachment attachment;
    while (decoder.removeAttachment(attachment))
        attachments.append(WTFMove(attachment));
    attachments.reverse();

    DataReference wrappedMessage;
    if (!decoder.decode(wrappedMessage))
        return nullptr;

    return Decoder::create(wrappedMessage.data(), wrappedMessage.size(), nullptr, WTFMove(attachments));
}

static inline const uint8_t* roundUpToAlignment(const uint8_t* ptr, size_t alignment)
{
    // Assert that the alignment is a power of 2.
    ASSERT(alignment && !(alignment & (alignment - 1)));

    uintptr_t alignmentMask = alignment - 1;
    return reinterpret_cast<uint8_t*>((reinterpret_cast<uintptr_t>(ptr) + alignmentMask) & ~alignmentMask);
}

static inline bool alignedBufferIsLargeEnoughToContain(const uint8_t* alignedPosition, const uint8_t* bufferStart, const uint8_t* bufferEnd, size_t size)
{
    // When size == 0 for the last argument and it's a variable length byte array,
    // bufferStart == alignedPosition == bufferEnd, so checking (bufferEnd >= alignedPosition)
    // is not an off-by-one error since (static_cast<size_t>(bufferEnd - alignedPosition) >= size)
    // will catch issues when size != 0.
    return bufferEnd >= alignedPosition && bufferStart <= alignedPosition && static_cast<size_t>(bufferEnd - alignedPosition) >= size;
}

bool Decoder::alignBufferPosition(size_t alignment, size_t size)
{
    const uint8_t* alignedPosition = roundUpToAlignment(m_bufferPos, alignment);
    if (!alignedBufferIsLargeEnoughToContain(alignedPosition, m_buffer, m_bufferEnd, size)) {
        // We've walked off the end of this buffer.
        markInvalid();
        return false;
    }
    
    m_bufferPos = alignedPosition;
    return true;
}

bool Decoder::bufferIsLargeEnoughToContain(size_t alignment, size_t size) const
{
    return alignedBufferIsLargeEnoughToContain(roundUpToAlignment(m_bufferPos, alignment), m_buffer, m_bufferEnd, size);
}

bool Decoder::decodeFixedLengthData(uint8_t* data, size_t size, size_t alignment)
{
    if (!alignBufferPosition(alignment, size))
        return false;

    memcpy(data, m_bufferPos, size);
    m_bufferPos += size;

    return true;
}

bool Decoder::decodeVariableLengthByteArray(DataReference& dataReference)
{
    uint64_t size;
    if (!decode(size))
        return false;
    
    if (!alignBufferPosition(1, size))
        return false;

    const uint8_t* data = m_bufferPos;
    m_bufferPos += size;

    dataReference = DataReference(data, size);
    return true;
}

bool Decoder::removeAttachment(Attachment& attachment)
{
    if (m_attachments.isEmpty())
        return false;

    attachment = m_attachments.takeLast();
    return true;
}

} // namespace IPC
