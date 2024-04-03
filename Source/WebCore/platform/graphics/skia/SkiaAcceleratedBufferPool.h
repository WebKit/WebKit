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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "NicosiaBuffer.h"
#include <wtf/FastMalloc.h>
#include <wtf/MonotonicTime.h>
#include <wtf/RunLoop.h>
#include <wtf/Vector.h>

namespace WebCore {

class SkiaAcceleratedBufferPool {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(SkiaAcceleratedBufferPool);
public:
    SkiaAcceleratedBufferPool();
    ~SkiaAcceleratedBufferPool();

    RefPtr<Nicosia::Buffer> acquireBuffer(const IntSize&, bool supportsAlpha);

private:
    struct Entry {
        explicit Entry(Ref<Nicosia::Buffer>&& buffer)
            : m_buffer(WTFMove(buffer))
        {
        }

        void markIsInUse() { m_lastUsedTime = MonotonicTime::now(); }
        bool canBeReleased (MonotonicTime minUsedTime) const { return m_lastUsedTime < minUsedTime && m_buffer->refCount() == 1; }

        Ref<Nicosia::Buffer> m_buffer;
        MonotonicTime m_lastUsedTime;
    };

    RefPtr<Nicosia::Buffer> createAcceleratedBuffer(const IntSize&, bool supportsAlpha);
    void scheduleReleaseUnusedBuffers();

    void releaseUnusedBuffersTimerFired();

    Vector<Entry> m_buffers;
    RunLoop::Timer m_releaseUnusedBuffersTimer;
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
