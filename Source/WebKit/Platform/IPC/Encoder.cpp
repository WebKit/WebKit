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
#include "Encoder.h"

#include "ArgumentCoders.h"
#include "MessageFlags.h"
#include <algorithm>
#include <wtf/OptionSet.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/UniqueRef.h>

#if OS(DARWIN)
#include <wtf/MmapSpan.h>
#endif

namespace IPC {

static constexpr uint8_t defaultMessageFlags = 0;

WTF_MAKE_TZONE_ALLOCATED_IMPL(Encoder);

Encoder::Encoder(MessageName messageName, uint64_t destinationID)
    : m_messageName(messageName)
    , m_destinationID(destinationID)
{
    encodeHeader();
}

// FIXME: We need to dispose of the attachments in cases of failure.
Encoder::~Encoder() = default;

void Encoder::setShouldDispatchMessageWhenWaitingForSyncReply(ShouldDispatchWhenWaitingForSyncReply shouldDispatchWhenWaitingForSyncReply)
{
    switch (shouldDispatchWhenWaitingForSyncReply) {
    case ShouldDispatchWhenWaitingForSyncReply::No:
        messageFlags().remove(MessageFlags::DispatchMessageWhenWaitingForSyncReply);
        messageFlags().remove(MessageFlags::DispatchMessageWhenWaitingForUnboundedSyncReply);
        break;
    case ShouldDispatchWhenWaitingForSyncReply::Yes:
        messageFlags().add(MessageFlags::DispatchMessageWhenWaitingForSyncReply);
        messageFlags().remove(MessageFlags::DispatchMessageWhenWaitingForUnboundedSyncReply);
        break;
    case ShouldDispatchWhenWaitingForSyncReply::YesDuringUnboundedIPC:
        messageFlags().remove(MessageFlags::DispatchMessageWhenWaitingForSyncReply);
        messageFlags().add(MessageFlags::DispatchMessageWhenWaitingForUnboundedSyncReply);
        break;
    }
}

bool Encoder::isFullySynchronousModeForTesting() const
{
    return messageFlags().contains(MessageFlags::UseFullySynchronousModeForTesting);
}

void Encoder::setFullySynchronousModeForTesting()
{
    messageFlags().add(MessageFlags::UseFullySynchronousModeForTesting);
}

void Encoder::setShouldMaintainOrderingWithAsyncMessages()
{
    messageFlags().add(MessageFlags::MaintainOrderingWithAsyncMessages);
}

void Encoder::wrapForTesting(UniqueRef<Encoder>&& original)
{
    ASSERT(isSyncMessage());
    ASSERT(!original->isSyncMessage());

    original->setShouldDispatchMessageWhenWaitingForSyncReply(ShouldDispatchWhenWaitingForSyncReply::Yes);

    *this << original->span();

    auto attachments = original->releaseAttachments();
    reserve(attachments.size());
    for (auto&& attachment : WTFMove(attachments))
        addAttachment(WTFMove(attachment));
}

static inline size_t roundUpToAlignment(size_t value, size_t alignment)
{
    return ((value + alignment - 1) / alignment) * alignment;
}

void Encoder::reserve(size_t size)
{
    auto oldCapacityBufferSize = capacityBuffer().size();
    if (size <= oldCapacityBufferSize)
        return;

    size_t newCapacity = roundUpToAlignment(oldCapacityBufferSize * 2, 4096);
    while (newCapacity < size)
        newCapacity *= 2;

    auto newBuffer = OutOfLineBufferSpan::tryAlloc(newCapacity);
    RELEASE_ASSERT(newBuffer);
    memcpySpan(newBuffer.mutableSpan(), span());

    m_outOfLineBuffer = WTFMove(newBuffer);
}

void Encoder::encodeHeader()
{
    *this << defaultMessageFlags;
    *this << m_messageName;
    *this << m_destinationID;
}

OptionSet<MessageFlags>& Encoder::messageFlags()
{
    // FIXME: We should probably pass an OptionSet<MessageFlags> into the Encoder constructor instead of encoding defaultMessageFlags then using this to change it later.
    static_assert(sizeof(OptionSet<MessageFlags>::StorageType) == 1, "Encoder uses the first byte of the buffer for message flags.");
    return reinterpretCastSpanStartTo<OptionSet<MessageFlags>>(capacityBuffer());
}

const OptionSet<MessageFlags>& Encoder::messageFlags() const
{
    return reinterpretCastSpanStartTo<OptionSet<MessageFlags>>(capacityBuffer());
}

std::span<uint8_t> Encoder::grow(size_t alignment, size_t size)
{
    size_t alignedSize = roundUpToAlignment(m_bufferSize, alignment);
    reserve(alignedSize + size);

    auto capacityBuffer = this->capacityBuffer();
    zeroSpan(capacityBuffer.subspan(m_bufferSize, alignedSize - m_bufferSize));

    m_bufferSize = alignedSize + size;

    return capacityBuffer.subspan(alignedSize);
}

std::span<uint8_t> Encoder::capacityBuffer()
{
    if (m_outOfLineBuffer)
        return m_outOfLineBuffer.mutableSpan();
    return m_inlineBuffer;
}

std::span<const uint8_t> Encoder::capacityBuffer() const
{
    if (m_outOfLineBuffer)
        return m_outOfLineBuffer.span();
    return m_inlineBuffer;
}

void Encoder::addAttachment(Attachment&& attachment)
{
    m_attachments.append(WTFMove(attachment));
}

Vector<Attachment> Encoder::releaseAttachments()
{
    return std::exchange(m_attachments, { });
}

bool Encoder::hasAttachments() const
{
    return !m_attachments.isEmpty();
}

} // namespace IPC
