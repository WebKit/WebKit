/*
 * Copyright (C) 2024 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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

    Ref<Nicosia::Buffer> acquireBuffer(const IntSize&, bool supportsAlpha);

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

    Ref<Nicosia::Buffer> createAcceleratedBuffer(const IntSize&, bool supportsAlpha);
    void scheduleReleaseUnusedBuffers();

    void releaseUnusedBuffersTimerFired();

    Vector<Entry> m_buffers;
    RunLoop::Timer m_releaseUnusedBuffersTimer;
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
