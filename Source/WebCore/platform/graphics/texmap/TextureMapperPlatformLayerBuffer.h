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

#if USE(COORDINATED_GRAPHICS)

#include "BitmapTexture.h"
#include "TextureMapperFlags.h"
#include "TextureMapperGLHeaders.h"
#include "TextureMapperPlatformLayer.h"
#include <variant>
#include <wtf/MonotonicTime.h>
#include <wtf/OptionSet.h>

namespace WebCore {

class TextureMapperPlatformLayerBuffer : public TextureMapperPlatformLayer {
    WTF_MAKE_NONCOPYABLE(TextureMapperPlatformLayerBuffer);
    WTF_MAKE_FAST_ALLOCATED();
public:
    TextureMapperPlatformLayerBuffer(RefPtr<BitmapTexture>&&, OptionSet<TextureMapperFlags> = { });

    TextureMapperPlatformLayerBuffer(GLuint textureID, const IntSize&, OptionSet<TextureMapperFlags>, GLint internalFormat);

    struct RGBTexture {
        GLuint id;
    };
    struct YUVTexture {
        unsigned numberOfPlanes;
        std::array<GLuint, 4> planes;
        std::array<unsigned, 4> yuvPlane;
        std::array<unsigned, 4> yuvPlaneOffset;
        std::array<GLfloat, 16> yuvToRgbMatrix;
    };
    struct ExternalOESTexture {
        GLuint id;
    };
    using TextureVariant = std::variant<RGBTexture, YUVTexture, ExternalOESTexture>;

    TextureMapperPlatformLayerBuffer(TextureVariant&&, const IntSize&, OptionSet<TextureMapperFlags>, GLint internalFormat);

    virtual ~TextureMapperPlatformLayerBuffer();

    void paintToTextureMapper(TextureMapper&, const FloatRect&, const TransformationMatrix& modelViewMatrix = TransformationMatrix(), float opacity = 1.0) override;

    bool canReuseWithoutReset(const IntSize&, GLint internalFormat);
    BitmapTexture& texture() { return *m_texture; }

    inline void markUsed() { m_timeLastUsed = MonotonicTime::now(); }
    MonotonicTime lastUsedTime() const { return m_timeLastUsed; }

    class UnmanagedBufferDataHolder {
        WTF_MAKE_NONCOPYABLE(UnmanagedBufferDataHolder);
        WTF_MAKE_FAST_ALLOCATED();
    public:
        UnmanagedBufferDataHolder() = default;
        virtual ~UnmanagedBufferDataHolder() = default;

#if USE(GSTREAMER_GL)
        virtual void waitForCPUSync() = 0;
#endif // USE(GSTREAMER_GL)
    };

    bool hasManagedTexture() const { return m_hasManagedTexture; }
    void setUnmanagedBufferDataHolder(std::unique_ptr<UnmanagedBufferDataHolder> holder) { m_unmanagedBufferDataHolder = WTFMove(holder); }
    void setExtraFlags(OptionSet<TextureMapperFlags> flags) { m_extraFlags = flags; }

    virtual std::unique_ptr<TextureMapperPlatformLayerBuffer> clone();

    class HolePunchClient {
        WTF_MAKE_FAST_ALLOCATED();
    public:
        virtual ~HolePunchClient() = default;
        virtual void setVideoRectangle(const IntRect&) = 0;
    };

    void setHolePunchClient(std::unique_ptr<HolePunchClient>&& client) { m_holePunchClient = WTFMove(client); }

    const TextureVariant& textureVariant() const { return m_variant; }
    IntSize size() const { return m_size; }

protected:
    TextureVariant m_variant;

private:
    RefPtr<BitmapTexture> m_texture;
    MonotonicTime m_timeLastUsed;

    IntSize m_size;
    GLint m_internalFormat;
    OptionSet<TextureMapperFlags> m_extraFlags;
    bool m_hasManagedTexture;
    std::unique_ptr<UnmanagedBufferDataHolder> m_unmanagedBufferDataHolder;
    std::unique_ptr<HolePunchClient> m_holePunchClient;
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
