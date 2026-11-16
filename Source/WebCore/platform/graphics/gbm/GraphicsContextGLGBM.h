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

#pragma once

#if ENABLE(WEBGL) && USE(COORDINATED_GRAPHICS) && USE(GBM)
#include "GraphicsContextGLEGL.h"
#include "GraphicsLayerContentsDisplayDelegate.h"
#include <wtf/unix/UnixFileDescriptor.h>

typedef void* EGLImageKHR;
struct gbm_bo;

namespace WebCore {
class DMABufBuffer;

class GraphicsContextGLGBM final : public GraphicsContextGLEGL {
public:
    static RefPtr<GraphicsContextGLGBM> create(GraphicsContextGLAttributes&&, RefPtr<GraphicsLayerContentsDisplayDelegate>&& = nullptr);
    virtual ~GraphicsContextGLGBM();

    static bool checkRequirements();

    WTF::UnixFileDescriptor createExportedFence() const;

    void prepareForDisplayWithFinishedSignal(NOESCAPE const Function<void()>&);
    DMABufBuffer* displayBufferDMABuf() { return displayBuffer().dmabuf(); }
    Vector<uint64_t> drawingBufferIDs() const;

#if ENABLE(WEBXR)
    GCGLExternalImage createExternalImage(ExternalImageSource&&, GCGLenum internalFormat, GCGLint layer) final;
    void bindExternalImage(GCGLenum target, GCGLExternalImage) final;
    bool enableRequiredWebXRExtensions() final;
#endif

private:
    GraphicsContextGLGBM(GraphicsContextGLAttributes&&, RefPtr<GraphicsLayerContentsDisplayDelegate>&&);

    bool platformInitialize() override;
    bool platformInitializeExtensions() override;
    bool reshapeDrawingBuffer() override;
    void prepareForDisplay() override;
    RefPtr<PixelBuffer> readCompositedResults() final;
#if ENABLE(WEBXR)
    bool enableRequiredWebXRExtensionsImpl();
#endif

    void freeDrawingBuffers();
    bool bindNextDrawingBuffer();

    static constexpr size_t maxReusedDrawingBuffers { 3 };

    class DrawingBuffer {
        WTF_MAKE_NONCOPYABLE(DrawingBuffer);
    public:
        DrawingBuffer() = default;
        DrawingBuffer(Ref<DMABufBuffer>&&, EGLImageKHR);
        DrawingBuffer(DrawingBuffer&&);
        DrawingBuffer& operator=(DrawingBuffer&&);
        ~DrawingBuffer();

        operator bool() const { return !!m_dmabuf; }

        DMABufBuffer* dmabuf() const LIFETIME_BOUND { return m_dmabuf.get(); }
        EGLImageKHR image() const LIFETIME_BOUND { return m_image; }

        bool isInUse() const;
        EGLImageKHR release();

    private:
        RefPtr<DMABufBuffer> m_dmabuf;
        EGLImageKHR m_image { nullptr };
    };
    DrawingBuffer createDrawingBuffer() const;
    void destroyDrawingBuffer(DrawingBuffer&) const;
    DrawingBuffer& drawingBuffer() { return m_drawingBuffers[m_currentDrawingBufferIndex % maxReusedDrawingBuffers]; }
    DrawingBuffer& displayBuffer() { return m_drawingBuffers[(m_currentDrawingBufferIndex + maxReusedDrawingBuffers - 1u) % maxReusedDrawingBuffers]; }

    struct {
        uint32_t fourcc { 0 };
        Vector<uint64_t, 1> modifiers;
    } m_drawingBufferFormat;

    std::array<DrawingBuffer, maxReusedDrawingBuffers> m_drawingBuffers;
    size_t m_currentDrawingBufferIndex { 0 };
};

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(COORDINATED_GRAPHICS) && USE(GBM)
