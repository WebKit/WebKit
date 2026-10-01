/*
 * Copyright (C) 2024, 2026 Igalia S.L.
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

#if ENABLE(WEBGL) && USE(COORDINATED_GRAPHICS)
#include <WebCore/GraphicsContextGLEGL.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class CoordinatedWebGLTextureWrapper;

class GraphicsContextGLCoordinated : public GraphicsContextGLEGL {
public:
    static RefPtr<GraphicsContextGLCoordinated> create(GraphicsContextGLAttributes&&);
    virtual ~GraphicsContextGLCoordinated();

    void prepareForDisplay() override;

protected:
    explicit GraphicsContextGLCoordinated(GraphicsContextGLAttributes&&);

private:
    bool platformInitialize() override;
    bool reshapeDrawingBuffer() override;
    RefPtr<PixelBuffer> readCompositedResults() override;

    GCGLuint setupCurrentTexture() const;
    void freeDrawingBuffers();
    void freeObsoleteDrawingBuffers();
    bool bindNextDrawingBuffer();

    static constexpr size_t maxReusedDrawingBuffers { 3 };

    class DrawingBuffer {
        WTF_MAKE_NONCOPYABLE(DrawingBuffer);
    public:
        DrawingBuffer() = default;
        DrawingBuffer(GCGLuint, Ref<CoordinatedWebGLTextureWrapper>&&);
        DrawingBuffer(DrawingBuffer&&);
        DrawingBuffer& operator=(DrawingBuffer&&);
        ~DrawingBuffer();

        operator bool() const { return !!m_texture; }

        GCGLuint texture() const { return m_texture; }
        CoordinatedWebGLTextureWrapper* textureWrapper() LIFETIME_BOUND { return m_textureWrapper.get(); }

        bool isInUse() const;
        GCGLuint release();

    private:
        GCGLuint m_texture { 0 };
        RefPtr<CoordinatedWebGLTextureWrapper> m_textureWrapper;
    };
    DrawingBuffer createDrawingBuffer() const;
    void destroyDrawingBuffer(DrawingBuffer&) const;
    DrawingBuffer& drawingBuffer() { return m_drawingBuffers[m_currentDrawingBufferIndex % maxReusedDrawingBuffers]; }
    DrawingBuffer& displayBuffer() { return m_drawingBuffers[(m_currentDrawingBufferIndex + maxReusedDrawingBuffers - 1u) % maxReusedDrawingBuffers]; }

    std::array<DrawingBuffer, maxReusedDrawingBuffers> m_drawingBuffers;
    size_t m_currentDrawingBufferIndex { 0 };
    Vector<DrawingBuffer> m_obsoleteDrawingBuffers;
};

class CoordinatedWebGLTextureWrapper final : public ThreadSafeRefCounted<CoordinatedWebGLTextureWrapper> {
public:
    static Ref<CoordinatedWebGLTextureWrapper> create(GCGLuint textureID)
    {
        return adoptRef(*new CoordinatedWebGLTextureWrapper(textureID));
    }

    ~CoordinatedWebGLTextureWrapper() = default;

    GCGLuint id() const { return m_textureID; }

private:
    explicit CoordinatedWebGLTextureWrapper(GCGLuint textureID)
        : m_textureID(textureID)
    {
    }

    GCGLuint m_textureID { 0 };
};

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(COORDINATED_GRAPHICS)
