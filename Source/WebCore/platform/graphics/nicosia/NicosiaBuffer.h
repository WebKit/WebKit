/*
 * Copyright (C) 2017 Metrological Group B.V.
 * Copyright (C) 2017, 2024 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "IntSize.h"
#include <wtf/Condition.h>
#include <wtf/Lock.h>
#include <wtf/MallocPtr.h>
#include <wtf/Ref.h>
#include <wtf/ThreadSafeRefCounted.h>

#if USE(SKIA)
IGNORE_CLANG_WARNINGS_BEGIN("cast-align")
#include <skia/core/SkCanvas.h>
#include <skia/core/SkSurface.h>
IGNORE_CLANG_WARNINGS_END
#endif

namespace WebCore {
class BitmapTexture;
class GLFence;
enum class PixelFormat : uint8_t;
}

namespace Nicosia {

class Buffer : public ThreadSafeRefCounted<Buffer> {
public:
    enum Flag {
        NoFlags = 0,
        SupportsAlpha = 1 << 0,
    };
    using Flags = unsigned;

    WEBCORE_EXPORT virtual ~Buffer();

    virtual WebCore::IntSize size() const = 0;
    virtual bool isBackedByOpenGL() const = 0;

    bool supportsAlpha() const { return m_flags & SupportsAlpha; }

    virtual void beginPainting() = 0;
    virtual void completePainting() = 0;
    virtual void waitUntilPaintingComplete() = 0;

#if USE(SKIA)
    SkCanvas* canvas();
#endif

    static void resetMemoryUsage();
    static double getMemoryUsage();

protected:
    explicit Buffer(Flags);

#if USE(SKIA)
    virtual bool tryEnsureSurface() = 0;
    sk_sp<SkSurface> m_surface;
#endif

    static Lock s_layersMemoryUsageLock;
    static double s_currentLayersMemoryUsage;
    static double s_maxLayersMemoryUsage;

private:
    Flags m_flags;
};

class UnacceleratedBuffer final : public Buffer {
public:
    WEBCORE_EXPORT static Ref<Buffer> create(const WebCore::IntSize&, Flags);
    WEBCORE_EXPORT virtual ~UnacceleratedBuffer();

    int stride() const { return m_size.width() * 4; }
    unsigned char* data() const { return m_data.get(); }

    WebCore::PixelFormat pixelFormat() const;

private:
    UnacceleratedBuffer(const WebCore::IntSize&, Flags);

    bool isBackedByOpenGL() const final { return false; }
    WebCore::IntSize size() const final { return m_size; }

#if USE(SKIA)
    bool tryEnsureSurface() final;
#endif
    void beginPainting() final;
    void completePainting() final;
    void waitUntilPaintingComplete() final;

    MallocPtr<unsigned char> m_data;
    WebCore::IntSize m_size;

    enum class PaintingState {
        InProgress,
        Complete
    };

    struct {
        Lock lock;
        Condition condition;
        PaintingState state { PaintingState::Complete };
    } m_painting;
};

#if USE(SKIA)
class AcceleratedBuffer final : public Buffer {
public:
    WEBCORE_EXPORT static Ref<Buffer> create(Ref<WebCore::BitmapTexture>&&);
    WEBCORE_EXPORT virtual ~AcceleratedBuffer();

    WebCore::BitmapTexture& texture() const { return m_texture.get(); }

private:
    AcceleratedBuffer(Ref<WebCore::BitmapTexture>&&, Flags);

    bool isBackedByOpenGL() const final { return true; }
    WebCore::IntSize size() const final;

    bool tryEnsureSurface() final;
    void beginPainting() final { }
    void completePainting() final;
    void waitUntilPaintingComplete() final;

    Ref<WebCore::BitmapTexture> m_texture;
    std::unique_ptr<WebCore::GLFence> m_fence;
};
#endif

} // namespace Nicosia
