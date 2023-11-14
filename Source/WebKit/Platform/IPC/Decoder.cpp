/*
 * Copyright (C) 2010-2023 Apple Inc. All rights reserved.
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
#include "Logging.h"
#include "MessageFlags.h"
#include <stdio.h>
#include <wtf/StdLibExtras.h>

namespace IPC {

static uint8_t* copyBuffer(DataReference buffer)
{
    uint8_t* bufferCopy;
    const size_t bufferSize = buffer.size_bytes();
    if (!tryFastMalloc(bufferSize).getValue(bufferCopy)) {
        RELEASE_LOG_FAULT(IPC, "Decoder::copyBuffer: tryFastMalloc(%lu) failed", bufferSize);
        return nullptr;
    }

    memcpy(bufferCopy, buffer.data(), bufferSize);
    return bufferCopy;
}

std::unique_ptr<Decoder> Decoder::create(DataReference buffer, Vector<Attachment>&& attachments)
{
    return Decoder::create({ copyBuffer(buffer), buffer.size() }, [](DataReference buffer) { fastFree(const_cast<uint8_t*>(buffer.data())); }, WTFMove(attachments)); // NOLINT
}

std::unique_ptr<Decoder> Decoder::create(DataReference buffer, BufferDeallocator&& bufferDeallocator, Vector<Attachment>&& attachments)
{
    ASSERT(bufferDeallocator);
    ASSERT(!!buffer.data());
    if (UNLIKELY(!buffer.data())) {
        RELEASE_LOG_FAULT(IPC, "Decoder::create() called with a null buffer (buffer size: %lu)", buffer.size_bytes());
        return nullptr;
    }
    auto decoder = std::unique_ptr<Decoder>(new Decoder(buffer, WTFMove(bufferDeallocator), WTFMove(attachments)));
    if (!decoder->isValid())
        return nullptr;
    return decoder;
}

Decoder::Decoder(DataReference buffer, BufferDeallocator&& bufferDeallocator, Vector<Attachment>&& attachments)
    : m_buffer { buffer }
    , m_bufferPosition { m_buffer.begin() }
    , m_bufferDeallocator { WTFMove(bufferDeallocator) }
    , m_attachments { WTFMove(attachments) }
{
    if (UNLIKELY(reinterpret_cast<uintptr_t>(m_buffer.data()) % alignof(uint64_t))) {
        markInvalid();
        return;
    }

    if (UNLIKELY(!decode(m_messageFlags)))
        return;

    if (UNLIKELY(!decode(m_messageName)))
        return;

    if (UNLIKELY(!decode(m_destinationID)))
        return;
}

Decoder::Decoder(DataReference stream, uint64_t destinationID)
    : m_buffer { stream }
    , m_bufferPosition { m_buffer.begin() }
    , m_bufferDeallocator { nullptr }
    , m_destinationID { destinationID }
{
    if (UNLIKELY(!decode(m_messageName)))
        return;
}

Decoder::~Decoder()
{
    if (isValid())
        markInvalid();
    // FIXME: We need to dispose of the mach ports in cases of failure.
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

bool Decoder::shouldMaintainOrderingWithAsyncMessages() const
{
    return m_messageFlags.contains(MessageFlags::MaintainOrderingWithAsyncMessages);
}

#if ENABLE(IPC_TESTING_API)
bool Decoder::hasSyncMessageDeserializationFailure() const
{
    return m_messageFlags.contains(MessageFlags::SyncMessageDeserializationFailure);
}
#endif

#if PLATFORM(MAC)
void Decoder::setImportanceAssertion(ImportanceAssertion&& assertion)
{
    m_importanceAssertion = WTFMove(assertion);
}
#endif

std::unique_ptr<Decoder> Decoder::unwrapForTesting(Decoder& decoder)
{
    ASSERT(decoder.isSyncMessage());

    auto attachments = std::exchange(decoder.m_attachments, { });

    DataReference wrappedMessage;
    if (!decoder.decode(wrappedMessage))
        return nullptr;

    auto wrappedDecoder = Decoder::create(wrappedMessage, WTFMove(attachments));
    wrappedDecoder->setIsAllowedWhenWaitingForSyncReplyOverride(true);
    return wrappedDecoder;
}

std::optional<Attachment> Decoder::takeLastAttachment()
{
    if (m_attachments.isEmpty()) {
        markInvalid();
        return std::nullopt;
    }
    return m_attachments.takeLast();
}

} // namespace IPC
