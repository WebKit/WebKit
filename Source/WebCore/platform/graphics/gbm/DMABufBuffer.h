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

#pragma once

#if USE(COORDINATED_GRAPHICS) && USE(GBM)
#include "IntSize.h"
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/unix/UnixFileDescriptor.h>

namespace WebCore {

class CoordinatedPlatformLayerBuffer;

class DMABufBuffer final : public ThreadSafeRefCounted<DMABufBuffer> {
public:
    static Ref<DMABufBuffer> create(const IntSize& size, uint32_t fourcc, Vector<WTF::UnixFileDescriptor>&& fds, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier)
    {
        return adoptRef(*new DMABufBuffer(size, fourcc, WTFMove(fds), WTFMove(offsets), WTFMove(strides), modifier));
    }
    ~DMABufBuffer();

    uint64_t id() const { return m_id; }

    struct Attributes {
        IntSize size;
        uint32_t fourcc { 0 };
        Vector<WTF::UnixFileDescriptor> fds;
        Vector<uint32_t> offsets;
        Vector<uint32_t> strides;
        uint64_t modifier { 0 };
    };
    const Attributes& attributes() const { return m_attributes; }

    CoordinatedPlatformLayerBuffer* buffer() const { return m_buffer.get(); }
    void setBuffer(std::unique_ptr<CoordinatedPlatformLayerBuffer>&&);

private:
    DMABufBuffer(const IntSize&, uint32_t fourcc, Vector<WTF::UnixFileDescriptor>&&, Vector<uint32_t>&&, Vector<uint32_t>&&, uint64_t modifier);

    uint64_t m_id { 0 };
    Attributes m_attributes;
    std::unique_ptr<CoordinatedPlatformLayerBuffer> m_buffer;
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(GBM)
