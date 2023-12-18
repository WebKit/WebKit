/*
 * Copyright (C) 2009, 2019, 2020 Igalia S.L
 * Copyright (C) 2015, 2019 Metrological Group B.V.
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
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if ENABLE(VIDEO) && USE(GSTREAMER) && USE(TEXTURE_MAPPER)

#include "MediaPlayerPrivateGStreamer.h"
#include "TextureMapperPlatformLayerBuffer.h"
#include <wtf/OptionSet.h>

namespace WebCore {

enum class TextureMapperFlags : uint16_t;

class GstVideoFrameHolder : public TextureMapperPlatformLayerBuffer::UnmanagedBufferDataHolder {
public:
    explicit GstVideoFrameHolder(GstSample*, std::optional<GstVideoDecoderPlatform>, OptionSet<TextureMapperFlags>, bool gstGLEnabled);
    virtual ~GstVideoFrameHolder();

#if USE(GSTREAMER_GL)
    virtual void waitForCPUSync();
#endif

    const IntSize& size() const { return m_size; }
    bool hasAlphaChannel() const { return m_hasAlphaChannel; }
    OptionSet<TextureMapperFlags> flags() const { return m_flags; }
    GLuint textureID() const { return m_textureID; }
    bool hasMappedTextures() const { return m_hasMappedTextures; }
    const GstVideoFrame& videoFrame() const { return m_videoFrame; }
    void updateTexture(BitmapTexture&);
    std::unique_ptr<TextureMapperPlatformLayerBuffer> platformLayerBuffer();

    bool hasDMABuf() const { return false; }

private:
    GRefPtr<GstBuffer> m_buffer;
    GstVideoFrame m_videoFrame { };
    IntSize m_size;
    bool m_hasAlphaChannel;
    std::optional<GstVideoDecoderPlatform> m_videoDecoderPlatform;
    OptionSet<TextureMapperFlags> m_flags;
    GLuint m_textureID { 0 };
#if USE(GSTREAMER_GL)
    GstGLTextureTarget m_textureTarget { GST_GL_TEXTURE_TARGET_NONE };
#endif
    bool m_isMapped { false };
    bool m_hasMappedTextures { false };
};

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER) && USE(TEXTURE_MAPPER)

