/*
 * Copyright (C) 2024 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DMABufBuffer.h"

#if USE(GBM)
#include "CoordinatedPlatformLayerBuffer.h"
#include <atomic>

namespace WebCore {

static uint64_t generateID()
{
    static std::atomic<uint64_t> id;
    return ++id;
}

DMABufBuffer::DMABufBuffer(const IntSize& size, uint32_t fourcc, Vector<WTF::UnixFileDescriptor>&& fds, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier)
    : m_id(generateID())
    , m_attributes({ size, fourcc, WTFMove(fds), WTFMove(offsets), WTFMove(strides), modifier })
{
}

DMABufBuffer::DMABufBuffer(uint64_t id, Attributes&& attributes)
    : m_id(id)
    , m_attributes(WTFMove(attributes))
{
}

DMABufBuffer::~DMABufBuffer() = default;

void DMABufBuffer::setBuffer(std::unique_ptr<CoordinatedPlatformLayerBuffer>&& buffer)
{
    m_buffer = WTFMove(buffer);
}

std::optional<DMABufBuffer::Attributes> DMABufBuffer::takeAttributes()
{
    if (m_attributes.fds.isEmpty())
        return std::nullopt;

    return DMABufBuffer::Attributes { WTFMove(m_attributes.size), std::exchange(m_attributes.fourcc, 0), WTFMove(m_attributes.fds), WTFMove(m_attributes.offsets), WTFMove(m_attributes.strides), std::exchange(m_attributes.modifier, 0) };
}

} // namespace WebCore

#endif // USE(GBM)
