/*
 * Copyright (C) 2009, 2015, 2019, 2020, 2024 Igalia S.L.
 * Copyright (C) 2015, 2019 Metrological Group B.V.
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

#include "config.h"
#include "CoordinatedPlatformLayerBufferVideo.h"

#if USE(COORDINATED_GRAPHICS) && ENABLE(VIDEO) && USE(GSTREAMER)
#include "CoordinatedPlatformLayerBufferExternalOES.h"
#include "CoordinatedPlatformLayerBufferRGB.h"
#include "CoordinatedPlatformLayerBufferYUV.h"
#include "TextureMapper.h"

// Include the <epoxy/gl.h> header before <gst/gl/gl.h>.
#include <epoxy/gl.h>
#include <gst/gl/gl.h>

namespace WebCore {

std::unique_ptr<CoordinatedPlatformLayerBufferVideo> CoordinatedPlatformLayerBufferVideo::create(GstSample* sample, std::optional<GstVideoDecoderPlatform> videoDecoderPlatform, bool gstGLEnabled, OptionSet<TextureMapperFlags> flags)
{
    GstVideoInfo videoInfo;
    if (UNLIKELY(!getSampleVideoInfo(sample, videoInfo)))
        return nullptr;

    auto* buffer = gst_sample_get_buffer(sample);
    if (UNLIKELY(!GST_IS_BUFFER(buffer)))
        return nullptr;

    return makeUnique<CoordinatedPlatformLayerBufferVideo>(buffer, &videoInfo, videoDecoderPlatform, gstGLEnabled, flags);
}

CoordinatedPlatformLayerBufferVideo::CoordinatedPlatformLayerBufferVideo(GstBuffer* buffer, GstVideoInfo* videoInfo, std::optional<GstVideoDecoderPlatform> videoDecoderPlatform, bool gstGLEnabled, OptionSet<TextureMapperFlags> flags)
    : CoordinatedPlatformLayerBuffer(Type::Video, IntSize(GST_VIDEO_INFO_WIDTH(videoInfo), GST_VIDEO_INFO_HEIGHT(videoInfo)), flags, nullptr)
    , m_videoDecoderPlatform(videoDecoderPlatform)
    , m_buffer(createBufferIfNeeded(buffer, videoInfo, gstGLEnabled))
{
}

CoordinatedPlatformLayerBufferVideo::~CoordinatedPlatformLayerBufferVideo()
{
    if (m_isMapped)
        gst_video_frame_unmap(&m_videoFrame);
}

std::unique_ptr<CoordinatedPlatformLayerBuffer> CoordinatedPlatformLayerBufferVideo::copyBuffer() const
{
    if (!m_buffer || !is<CoordinatedPlatformLayerBufferRGB>(*m_buffer))
        return nullptr;

    auto& buffer = downcast<CoordinatedPlatformLayerBufferRGB>(*m_buffer);
    auto textureID = buffer.textureID();
    if (!textureID)
        return nullptr;

    auto texture = BitmapTexture::create(buffer.size());
    texture->copyFromExternalTexture(textureID);
    return CoordinatedPlatformLayerBufferRGB::create(WTFMove(texture), m_flags, nullptr);
}

std::unique_ptr<CoordinatedPlatformLayerBuffer> CoordinatedPlatformLayerBufferVideo::createBufferIfNeeded(GstBuffer* buffer, GstVideoInfo* videoInfo, bool gstGLEnabled)
{
    if (gstGLEnabled)
        return createBufferFromGLMemory(buffer, videoInfo);

    // When not having a texture, we map the frame here and upload the pixels to a texture in the
    // compositor thread, in paintToTextureMapper(), which also allows us to use the texture mapper
    // bitmap texture pool.
    m_isMapped = gst_video_frame_map(&m_videoFrame, videoInfo, buffer, GST_MAP_READ);
    if (!m_isMapped)
        return nullptr;

    if (GST_VIDEO_INFO_HAS_ALPHA(&m_videoFrame.info))
        m_flags.add({ TextureMapperFlags::ShouldBlend, TextureMapperFlags::ShouldPremultiply });

    return nullptr;
}

std::unique_ptr<CoordinatedPlatformLayerBuffer> CoordinatedPlatformLayerBufferVideo::createBufferFromGLMemory(GstBuffer* buffer, GstVideoInfo* videoInfo)
{
    GstMemory* memory = gst_buffer_peek_memory(buffer, 0);
    if (!gst_is_gl_memory(memory))
        return nullptr;

    m_isMapped = gst_video_frame_map(&m_videoFrame, videoInfo, buffer, static_cast<GstMapFlags>(GST_MAP_READ | GST_MAP_GL));
    if (!m_isMapped)
        return nullptr;

    if (GST_VIDEO_INFO_HAS_ALPHA(&m_videoFrame.info))
        m_flags.add({ TextureMapperFlags::ShouldBlend, TextureMapperFlags::ShouldPremultiply });

    auto textureTarget = gst_gl_memory_get_texture_target(GST_GL_MEMORY_CAST(memory));
    if (textureTarget == GST_GL_TEXTURE_TARGET_EXTERNAL_OES)
        return CoordinatedPlatformLayerBufferExternalOES::create(*reinterpret_cast<GLuint*>(m_videoFrame.data[0]), m_size, m_flags, nullptr);

    if ((GST_VIDEO_INFO_IS_RGB(&m_videoFrame.info) && GST_VIDEO_INFO_N_PLANES(&m_videoFrame.info) == 1))
        return CoordinatedPlatformLayerBufferRGB::create(*reinterpret_cast<GLuint*>(m_videoFrame.data[0]), m_size, m_flags, nullptr);

    if (GST_VIDEO_INFO_IS_YUV(&m_videoFrame.info) && GST_VIDEO_INFO_N_COMPONENTS(&m_videoFrame.info) >= 3 && GST_VIDEO_INFO_N_PLANES(&m_videoFrame.info) <= 4) {
        if (m_videoDecoderPlatform && *m_videoDecoderPlatform == GstVideoDecoderPlatform::ImxVPU) {
            // IMX VPU decoder decodes YUV data only into the Y texture from which the sampler
            // then directly produces RGBA data. Textures for other planes aren't used, but
            // that's decoder's problem. We have to treat that Y texture as having RGBA data.
            return CoordinatedPlatformLayerBufferRGB::create(*reinterpret_cast<GLuint*>(m_videoFrame.data[0]), m_size, m_flags, nullptr);
        }

        unsigned numberOfPlanes = GST_VIDEO_INFO_N_PLANES(&m_videoFrame.info);
        std::array<GLuint, 4> planes;
        std::array<unsigned, 4> yuvPlane;
        std::array<unsigned, 4> yuvPlaneOffset;
        for (unsigned i = 0; i < numberOfPlanes; ++i)
            planes[i] = *static_cast<GLuint*>(m_videoFrame.data[i]);
        for (unsigned i = 0; i < numberOfPlanes; ++i) {
            yuvPlane[i] = GST_VIDEO_INFO_COMP_PLANE(&m_videoFrame.info, i);
            yuvPlaneOffset[i] = GST_VIDEO_INFO_COMP_POFFSET(&m_videoFrame.info, i);
        }

        // Default to bt601. This is the same behaviour as GStreamer's glcolorconvert element.
        CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace yuvToRgbColorSpace = CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::BT601;
        if (gst_video_colorimetry_matches(&GST_VIDEO_INFO_COLORIMETRY(&m_videoFrame.info), GST_VIDEO_COLORIMETRY_BT709))
            yuvToRgbColorSpace = CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::BT709;
        else if (gst_video_colorimetry_matches(&GST_VIDEO_INFO_COLORIMETRY(&m_videoFrame.info), GST_VIDEO_COLORIMETRY_BT2020))
            yuvToRgbColorSpace = CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::BT2020;
        else if (gst_video_colorimetry_matches(&GST_VIDEO_INFO_COLORIMETRY(&m_videoFrame.info), GST_VIDEO_COLORIMETRY_SMPTE240M))
            yuvToRgbColorSpace = CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::SMPTE240M;

        return CoordinatedPlatformLayerBufferYUV::create(numberOfPlanes, WTFMove(planes), WTFMove(yuvPlane), WTFMove(yuvPlaneOffset), yuvToRgbColorSpace, m_size, m_flags, nullptr);
    }

    return nullptr;
}

void CoordinatedPlatformLayerBufferVideo::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    if (!m_isMapped)
        return;
    if (m_videoDecoderPlatform != GstVideoDecoderPlatform::OpenMAX) {
        if (auto* meta = gst_buffer_get_gl_sync_meta(m_videoFrame.buffer)) {
            GstMemory* memory = gst_buffer_peek_memory(m_videoFrame.buffer, 0);
            GstGLContext* context = reinterpret_cast<GstGLBaseMemory*>(memory)->context;
            gst_gl_sync_meta_wait_cpu(meta, context);
        }
    }

    if (!m_buffer) {
        OptionSet<BitmapTexture::Flags> textureFlags;
        if (GST_VIDEO_INFO_HAS_ALPHA(&m_videoFrame.info))
            textureFlags.add(BitmapTexture::Flags::SupportsAlpha);
        auto texture = textureMapper.acquireTextureFromPool(m_size, textureFlags);

        auto* meta = gst_buffer_get_video_gl_texture_upload_meta(m_videoFrame.buffer);
        if (meta && meta->n_textures == 1) {
            guint ids[4] = { texture->id(), 0, 0, 0 };
            if (gst_video_gl_texture_upload_meta_upload(meta, ids))
                m_buffer = CoordinatedPlatformLayerBufferRGB::create(WTFMove(texture), m_flags, nullptr);
        }

        if (!m_buffer) {
            int stride = GST_VIDEO_FRAME_PLANE_STRIDE(&m_videoFrame, 0);
            const void* srcData = GST_VIDEO_FRAME_PLANE_DATA(&m_videoFrame, 0);
            texture->updateContents(srcData, IntRect(0, 0, m_size.width(), m_size.height()), IntPoint(0, 0), stride);
            m_buffer = CoordinatedPlatformLayerBufferRGB::create(WTFMove(texture), m_flags, nullptr);
            gst_video_frame_unmap(&m_videoFrame);
            m_isMapped = false;
        }
    }

    if (m_buffer)
        m_buffer->paintToTextureMapper(textureMapper, targetRect, modelViewMatrix, opacity);
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && ENABLE(VIDEO) && USE(GSTREAMER)
