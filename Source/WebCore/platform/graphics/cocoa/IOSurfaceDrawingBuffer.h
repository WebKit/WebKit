/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if HAVE(IOSURFACE)

#include <WebCore/IOSurface.h>
#include <WebCore/NativeImage.h>

namespace WebCore {

// Move-only value type holding a IOSurface that will be used in by drawing to it
// as well as reading from it via CG.
// Important subtle expected behavior is to migrate the existing CGImages from
// IOSurfaces to main memory when the drawing buffer instance is destroyed. This
// is to prevent long-lived images reserving IOSurfaces.
class IOSurfaceDrawingBuffer {
public:
    IOSurfaceDrawingBuffer() = default;
    IOSurfaceDrawingBuffer(IOSurfaceDrawingBuffer&&);
    explicit IOSurfaceDrawingBuffer(std::unique_ptr<IOSurface>&&);
    IOSurfaceDrawingBuffer& operator=(IOSurfaceDrawingBuffer&&);
    operator bool() const { return !!m_surface; }
    IOSurface* surface() const { return m_surface.get(); }

    IntSize size() const;

    // Returns true if surface cannot be modified because it's in
    // cross-process use, and copy-on-write would not work.
    bool isInUse() const;

    // Should be called always when writing to the surface.
    void prepareForWrite();

    // Creates a copy of current contents.
    RefPtr<NativeImage> copyNativeImage() const;
private:
    void forceCopy();
    std::unique_ptr<IOSurface> m_surface;
    mutable RetainPtr<CGContextRef> m_copyOnWriteContext;
    mutable bool m_needCopy { false };
};

inline IOSurfaceDrawingBuffer::IOSurfaceDrawingBuffer(IOSurfaceDrawingBuffer&& other)
    : m_surface(WTFMove(other.m_surface))
    , m_copyOnWriteContext(WTFMove(other.m_copyOnWriteContext))
    , m_needCopy(std::exchange(other.m_needCopy, false))
{
}

inline IOSurfaceDrawingBuffer::IOSurfaceDrawingBuffer(std::unique_ptr<IOSurface>&& surface)
    : m_surface(WTFMove(surface))
{
}

inline IOSurfaceDrawingBuffer& IOSurfaceDrawingBuffer::operator=(IOSurfaceDrawingBuffer&& other)
{
    m_surface = WTFMove(other.m_surface);
    m_copyOnWriteContext = WTFMove(other.m_copyOnWriteContext);
    m_needCopy = std::exchange(other.m_needCopy, false);
    return *this;
}

inline void IOSurfaceDrawingBuffer::prepareForWrite()
{
    if (m_needCopy)
        forceCopy();
}

inline bool IOSurfaceDrawingBuffer::isInUse() const
{
    if (!m_surface)
        return false;
    return m_surface->isInUse();
}

inline IntSize IOSurfaceDrawingBuffer::size() const
{
    if (!m_surface)
        return { };
    return m_surface->size();
}

}

#endif
