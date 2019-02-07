/*
 * Copyright (C) 2015 Igalia S.L.
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

#if USE(COORDINATED_GRAPHICS_THREADED)

#include "BitmapTextureGL.h"
#include "TextureMapperGLHeaders.h"
#include "TextureMapperPlatformLayer.h"
#include <wtf/MonotonicTime.h>

namespace WebCore {

class TextureMapperPlatformLayerBuffer : public TextureMapperPlatformLayer {
    WTF_MAKE_NONCOPYABLE(TextureMapperPlatformLayerBuffer);
    WTF_MAKE_FAST_ALLOCATED();
public:
    TextureMapperPlatformLayerBuffer(RefPtr<BitmapTexture>&&, TextureMapperGL::Flags = 0);
    TextureMapperPlatformLayerBuffer(GLuint textureID, const IntSize&, TextureMapperGL::Flags, GLint internalFormat);

    virtual ~TextureMapperPlatformLayerBuffer() = default;

    void paintToTextureMapper(TextureMapper&, const FloatRect&, const TransformationMatrix& modelViewMatrix = TransformationMatrix(), float opacity = 1.0) final;

    bool canReuseWithoutReset(const IntSize&, GLint internalFormat);
    BitmapTextureGL& textureGL() { return static_cast<BitmapTextureGL&>(*m_texture); }

    inline void markUsed() { m_timeLastUsed = MonotonicTime::now(); }
    MonotonicTime lastUsedTime() const { return m_timeLastUsed; }

    class UnmanagedBufferDataHolder {
        WTF_MAKE_NONCOPYABLE(UnmanagedBufferDataHolder);
        WTF_MAKE_FAST_ALLOCATED();
    public:
        UnmanagedBufferDataHolder() = default;
        virtual ~UnmanagedBufferDataHolder() = default;
    };

    bool hasManagedTexture() const { return m_hasManagedTexture; }
    void setUnmanagedBufferDataHolder(std::unique_ptr<UnmanagedBufferDataHolder> holder) { m_unmanagedBufferDataHolder = WTFMove(holder); }
    void setExtraFlags(TextureMapperGL::Flags flags) { m_extraFlags = flags; }

    std::unique_ptr<TextureMapperPlatformLayerBuffer> clone();

    class HolePunchClient {
    public:
        virtual void setVideoRectangle(const IntRect&) = 0;
    };

    void setHolePunchClient(std::unique_ptr<HolePunchClient>&& client) { m_holePunchClient = WTFMove(client); }

private:

    RefPtr<BitmapTexture> m_texture;
    MonotonicTime m_timeLastUsed;

    GLuint m_textureID;
    IntSize m_size;
    GLint m_internalFormat;
    TextureMapperGL::Flags m_extraFlags;
    bool m_hasManagedTexture;
    std::unique_ptr<UnmanagedBufferDataHolder> m_unmanagedBufferDataHolder;
    std::unique_ptr<HolePunchClient> m_holePunchClient;
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS_THREADED)
