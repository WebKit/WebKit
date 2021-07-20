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

#include "config.h"

#if ENABLE(VIDEO) && USE(GSTREAMER) && USE(TEXTURE_MAPPER_GL)
#include "GStreamerVideoFrameHolder.h"

#include "BitmapTextureGL.h"
#include "BitmapTexturePool.h"
#include "TextureMapperContextAttributes.h"
#include "TextureMapperGL.h"

namespace WebCore {

GstVideoFrameHolder::GstVideoFrameHolder(GstSample* sample, std::optional<GstVideoDecoderPlatform> videoDecoderPlatform, TextureMapperGL::Flags flags, bool gstGLEnabled)
    : m_videoDecoderPlatform(videoDecoderPlatform)
{
    RELEASE_ASSERT(GST_IS_SAMPLE(sample));

    GstVideoInfo videoInfo;
    if (UNLIKELY(!getSampleVideoInfo(sample, videoInfo)))
        return;

    m_size = IntSize(GST_VIDEO_INFO_WIDTH(&videoInfo), GST_VIDEO_INFO_HEIGHT(&videoInfo));
    m_hasAlphaChannel = GST_VIDEO_INFO_HAS_ALPHA(&videoInfo);
    m_buffer = gst_sample_get_buffer(sample);
    if (UNLIKELY(!GST_IS_BUFFER(m_buffer.get())))
        return;

#if USE(GSTREAMER_GL)
    m_flags = flags | (m_hasAlphaChannel ? TextureMapperGL::ShouldBlend | TextureMapperGL::ShouldPremultiply : 0);

    GstMemory* memory = gst_buffer_peek_memory(m_buffer.get(), 0);
    if (gst_is_gl_memory(memory))
        m_textureTarget = gst_gl_memory_get_texture_target(GST_GL_MEMORY_CAST(memory));

#if USE(WPE_VIDEO_PLANE_DISPLAY_DMABUF)
    m_dmabufFD = -1;
    gsize offset;
    if (gst_is_gl_memory_egl(memory)) {
        GstGLMemoryEGL* eglMemory = (GstGLMemoryEGL*) memory;
        gst_egl_image_export_dmabuf(eglMemory->image, &m_dmabufFD, &m_dmabufStride, &offset);
    } else if (gst_is_gl_memory(memory)) {
        GRefPtr<GstEGLImage> eglImage = adoptGRef(gst_egl_image_from_texture(GST_GL_BASE_MEMORY_CAST(memory)->context, GST_GL_MEMORY_CAST(memory), nullptr));

        if (eglImage)
            gst_egl_image_export_dmabuf(eglImage.get(), &m_dmabufFD, &m_dmabufStride, &offset);
    }

    if (hasDMABuf() && m_dmabufStride == -1) {
        m_isMapped = gst_video_frame_map(&m_videoFrame, &videoInfo, m_buffer.get(), GST_MAP_READ);
        if (m_isMapped)
            m_dmabufStride = GST_VIDEO_INFO_PLANE_STRIDE(&m_videoFrame.info, 0);
    }

    if (hasDMABuf() && m_dmabufStride)
        return;

    static std::once_flag s_onceFlag;
    std::call_once(s_onceFlag, [] {
        GST_WARNING("Texture export to DMABuf failed, falling back to internal rendering");
    });
#endif // USE(WPE_VIDEO_PLANE_DISPLAY_DMABUF)

    if (gstGLEnabled) {
        m_isMapped = gst_video_frame_map(&m_videoFrame, &videoInfo, m_buffer.get(), static_cast<GstMapFlags>(GST_MAP_READ | GST_MAP_GL));
        if (m_isMapped) {
            m_textureID = *reinterpret_cast<GLuint*>(m_videoFrame.data[0]);
            m_hasMappedTextures = true;
        }
    } else
#else
    UNUSED_PARAM(flags);
    UNUSED_PARAM(gstGLEnabled);
#endif // USE(GSTREAMER_GL)

    {
        m_textureID = 0;
        m_isMapped = gst_video_frame_map(&m_videoFrame, &videoInfo, m_buffer.get(), GST_MAP_READ);
        if (m_isMapped) {
            // Right now the TextureMapper only supports chromas with one plane.
            ASSERT(GST_VIDEO_INFO_N_PLANES(&videoInfo) == 1);
        }
    }
}

GstVideoFrameHolder::~GstVideoFrameHolder()
{
    if (UNLIKELY(!m_isMapped))
        return;

    gst_video_frame_unmap(&m_videoFrame);
}

#if USE(WPE_VIDEO_PLANE_DISPLAY_DMABUF)
void GstVideoFrameHolder::handoffVideoDmaBuf(struct wpe_video_plane_display_dmabuf_source* videoPlaneDisplayDmaBufSource, const IntRect& rect)
{
    if (m_dmabufFD <= 0)
        return;

    wpe_video_plane_display_dmabuf_source_update(videoPlaneDisplayDmaBufSource, m_dmabufFD, rect.x(), rect.y(), m_size.width(), m_size.height(), m_dmabufStride, [](void* data) {
        gst_buffer_unref(GST_BUFFER_CAST(data));
    }, gst_buffer_ref(m_buffer.get()));

    close(m_dmabufFD);
    m_dmabufFD = 0;
}
#endif

#if USE(GSTREAMER_GL)
void GstVideoFrameHolder::waitForCPUSync()
{
    // No need for OpenGL synchronization when using the OpenMAX decoder.
    if (m_videoDecoderPlatform == GstVideoDecoderPlatform::OpenMAX)
        return;

    GstGLSyncMeta* meta = gst_buffer_get_gl_sync_meta(m_buffer.get());
    if (meta) {
        GstMemory* mem = gst_buffer_peek_memory(m_buffer.get(), 0);
        GstGLContext* context = ((GstGLBaseMemory*)mem)->context;
        gst_gl_sync_meta_wait_cpu(meta, context);
    }
}
#endif // USE(GSTREAMER_GL)

void GstVideoFrameHolder::updateTexture(BitmapTextureGL& texture)
{
    ASSERT(!m_textureID);
    GstVideoGLTextureUploadMeta* meta;
    if (m_buffer && (meta = gst_buffer_get_video_gl_texture_upload_meta(m_buffer.get()))) {
        if (meta->n_textures == 1) { // BRGx & BGRA formats use only one texture.
            guint ids[4] = { texture.id(), 0, 0, 0 };

            if (gst_video_gl_texture_upload_meta_upload(meta, ids))
                return;
        }
    }

    if (!m_isMapped)
        return;

    int stride = GST_VIDEO_FRAME_PLANE_STRIDE(&m_videoFrame, 0);
    const void* srcData = GST_VIDEO_FRAME_PLANE_DATA(&m_videoFrame, 0);

    if (!srcData)
        return;

    texture.updateContents(srcData, WebCore::IntRect(0, 0, m_size.width(), m_size.height()), WebCore::IntPoint(0, 0), stride);
}

std::unique_ptr<TextureMapperPlatformLayerBuffer> GstVideoFrameHolder::platformLayerBuffer()
{
    if (!m_hasMappedTextures)
        return nullptr;

    using Buffer = TextureMapperPlatformLayerBuffer;

#if USE(GSTREAMER_GL)
    if (m_textureTarget == GST_GL_TEXTURE_TARGET_EXTERNAL_OES)
        return makeUnique<Buffer>(Buffer::TextureVariant { Buffer::ExternalOESTexture { m_textureID } }, m_size, m_flags, GL_DONT_CARE);
#endif

    if ((GST_VIDEO_INFO_IS_RGB(&m_videoFrame.info) && GST_VIDEO_INFO_N_PLANES(&m_videoFrame.info) == 1))
        return makeUnique<Buffer>(Buffer::TextureVariant { Buffer::RGBTexture { m_textureID } }, m_size, m_flags, GL_RGBA);

    if (GST_VIDEO_INFO_IS_YUV(&m_videoFrame.info)) {
        if (GST_VIDEO_INFO_N_COMPONENTS(&m_videoFrame.info) < 3 || GST_VIDEO_INFO_N_PLANES(&m_videoFrame.info) > 4)
            return nullptr;

        if (m_videoDecoderPlatform && *m_videoDecoderPlatform == GstVideoDecoderPlatform::ImxVPU) {
            // IMX VPU decoder decodes YUV data only into the Y texture from which the sampler
            // then directly produces RGBA data. Textures for other planes aren't used, but
            // that's decoder's problem. We have to treat that Y texture as having RGBA data.
            return makeUnique<Buffer>(Buffer::TextureVariant { Buffer::RGBTexture { m_textureID } }, m_size, m_flags, GL_RGBA);
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

        std::array<GLfloat, 9> yuvToRgb;
        if (gst_video_colorimetry_matches(&GST_VIDEO_INFO_COLORIMETRY(&m_videoFrame.info), GST_VIDEO_COLORIMETRY_BT709)) {
            yuvToRgb = {
                1.164f,  0.0f,    1.787f,
                1.164f, -0.213f, -0.531f,
                1.164f,  2.112f,  0.0f
            };
        } else {
            // Default to bt601. This is the same behaviour as GStreamer's glcolorconvert element.
            yuvToRgb = {
                1.164f,  0.0f,    1.596f,
                1.164f, -0.391f, -0.813f,
                1.164f,  2.018f,  0.0f
            };
        }

        return makeUnique<Buffer>(Buffer::TextureVariant { Buffer::YUVTexture { numberOfPlanes, planes, yuvPlane, yuvPlaneOffset, yuvToRgb } }, m_size, m_flags, GL_RGBA);
    }

    return nullptr;
}

}
#endif
