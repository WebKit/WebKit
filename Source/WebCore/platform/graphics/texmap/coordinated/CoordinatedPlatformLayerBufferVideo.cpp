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

#if USE(GBM)
#include "CoordinatedPlatformLayerBufferDMABuf.h"
#include "DMABufBuffer.h"
#include <drm_fourcc.h>
#include <gst/allocators/gstdmabuf.h>
#include <wtf/unix/UnixFileDescriptor.h>
#endif

namespace WebCore {

std::unique_ptr<CoordinatedPlatformLayerBufferVideo> CoordinatedPlatformLayerBufferVideo::create(GstSample* sample, std::optional<GstVideoDecoderPlatform> videoDecoderPlatform, bool gstGLEnabled, OptionSet<TextureMapperFlags> flags)
{
    GstCaps* caps = gst_sample_get_caps(sample);
    if (UNLIKELY(!caps))
        return nullptr;

    GstVideoInfo videoInfo;
    gst_video_info_init(&videoInfo);

    std::optional<std::pair<uint32_t, uint64_t>> dmabufFormat;
#if USE(GBM) && GST_CHECK_VERSION(1, 24, 0)
    GstVideoInfoDmaDrm drmVideoInfo;
    gst_video_info_dma_drm_init(&drmVideoInfo);
    if (gst_video_is_dma_drm_caps(caps)) {
        if (!gst_video_info_dma_drm_from_caps(&drmVideoInfo, caps))
            return nullptr;

        if (!gst_video_info_dma_drm_to_video_info(&drmVideoInfo, &videoInfo))
            return nullptr;

        dmabufFormat = std::pair<uint32_t, uint64_t> { drmVideoInfo.drm_fourcc, drmVideoInfo.drm_modifier };
    }
#endif
    if (!dmabufFormat) {
        if (!gst_video_info_from_caps(&videoInfo, caps))
            return nullptr;
    }

    auto* buffer = gst_sample_get_buffer(sample);
    if (UNLIKELY(!GST_IS_BUFFER(buffer)))
        return nullptr;

    return makeUnique<CoordinatedPlatformLayerBufferVideo>(buffer, &videoInfo, dmabufFormat, videoDecoderPlatform, gstGLEnabled, flags);
}

CoordinatedPlatformLayerBufferVideo::CoordinatedPlatformLayerBufferVideo(GstBuffer* buffer, GstVideoInfo* videoInfo, std::optional<std::pair<uint32_t, uint64_t>> dmabufFormat, std::optional<GstVideoDecoderPlatform> videoDecoderPlatform, bool gstGLEnabled, OptionSet<TextureMapperFlags> flags)
    : CoordinatedPlatformLayerBuffer(Type::Video, IntSize(GST_VIDEO_INFO_WIDTH(videoInfo), GST_VIDEO_INFO_HEIGHT(videoInfo)), flags, nullptr)
    , m_videoDecoderPlatform(videoDecoderPlatform)
    , m_buffer(createBufferIfNeeded(buffer, videoInfo, dmabufFormat, gstGLEnabled))
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

std::unique_ptr<CoordinatedPlatformLayerBuffer> CoordinatedPlatformLayerBufferVideo::createBufferIfNeeded(GstBuffer* buffer, GstVideoInfo* videoInfo, std::optional<std::pair<uint32_t, uint64_t>> dmabufFormat, bool gstGLEnabled)
{
#if USE(GBM)
    if (gst_is_dmabuf_memory(gst_buffer_peek_memory(buffer, 0)))
        return createBufferFromDMABufMemory(buffer, videoInfo, dmabufFormat);
#else
    UNUSED_PARAM(dmabufFormat);
#endif

    if (gstGLEnabled && gst_is_gl_memory(gst_buffer_peek_memory(buffer, 0)))
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

#if USE(GBM)
static uint32_t videoFormatToDRMFourcc(GstVideoFormat format)
{
    switch (format) {
    case GST_VIDEO_FORMAT_BGRx:
        return DRM_FORMAT_XRGB8888;
    case GST_VIDEO_FORMAT_RGBx:
        return DRM_FORMAT_XBGR8888;
    case GST_VIDEO_FORMAT_BGRA:
        return DRM_FORMAT_ARGB8888;
    case GST_VIDEO_FORMAT_RGBA:
        return DRM_FORMAT_ABGR8888;
    case GST_VIDEO_FORMAT_I420:
        return DRM_FORMAT_YUV420;
    case GST_VIDEO_FORMAT_YV12:
        return DRM_FORMAT_YVU420;
    case GST_VIDEO_FORMAT_NV12:
        return DRM_FORMAT_NV12;
    case GST_VIDEO_FORMAT_NV21:
        return DRM_FORMAT_NV21;
    case GST_VIDEO_FORMAT_Y444:
        return DRM_FORMAT_YUV444;
    case GST_VIDEO_FORMAT_Y41B:
        return DRM_FORMAT_YUV411;
    case GST_VIDEO_FORMAT_Y42B:
        return DRM_FORMAT_YUV422;
    case GST_VIDEO_FORMAT_P010_10LE:
        return DRM_FORMAT_P010;
    default:
        break;
    }

    return 0;
}

std::unique_ptr<CoordinatedPlatformLayerBuffer> CoordinatedPlatformLayerBufferVideo::createBufferFromDMABufMemory(GstBuffer* buffer, GstVideoInfo* videoInfo, std::optional<std::pair<uint32_t, uint64_t>> dmabufFormat)
{
    if (GST_VIDEO_INFO_HAS_ALPHA(videoInfo))
        m_flags.add({ TextureMapperFlags::ShouldBlend, TextureMapperFlags::ShouldPremultiply });

    static GQuark dmabufQuark = g_quark_from_static_string("wk-dmabuf-buffer");
    auto* memory = gst_buffer_peek_memory(buffer, 0);
    RefPtr<DMABufBuffer> dmabuf = static_cast<DMABufBuffer*>(gst_mini_object_get_qdata(GST_MINI_OBJECT_CAST(memory), dmabufQuark));
    if (!dmabuf) {
        const auto* videoMeta = gst_buffer_get_video_meta(buffer);
        IntSize size(videoMeta->width, videoMeta->height);
        uint32_t fourcc = dmabufFormat ? dmabufFormat->first : videoFormatToDRMFourcc(GST_VIDEO_INFO_FORMAT(videoInfo));
        uint64_t modifier = dmabufFormat ? dmabufFormat->second : DRM_FORMAT_MOD_INVALID;
        Vector<UnixFileDescriptor> fds;
        Vector<uint32_t> offsets;
        Vector<uint32_t> strides;
        for (unsigned i = 0; i < videoMeta->n_planes; ++i) {
            guint index, length;
            gsize skip;
            if (!gst_buffer_find_memory(buffer, videoMeta->offset[i], 1, &index, &length, &skip))
                return nullptr;

            auto* planeMemory = gst_buffer_peek_memory(buffer, index);
            fds.append(UnixFileDescriptor { gst_dmabuf_memory_get_fd(planeMemory), UnixFileDescriptor::Duplicate });
            offsets.append(planeMemory->offset + skip);
            strides.append(videoMeta->stride[i]);
        }
        dmabuf = DMABufBuffer::create(size, fourcc, WTFMove(fds), WTFMove(offsets), WTFMove(strides), modifier);

        DMABufBuffer::ColorSpace colorSpace = DMABufBuffer::ColorSpace::BT601;
        if (gst_video_colorimetry_matches(&GST_VIDEO_INFO_COLORIMETRY(videoInfo), GST_VIDEO_COLORIMETRY_BT709))
            colorSpace = DMABufBuffer::ColorSpace::BT709;
        else if (gst_video_colorimetry_matches(&GST_VIDEO_INFO_COLORIMETRY(videoInfo), GST_VIDEO_COLORIMETRY_BT2020))
            colorSpace = DMABufBuffer::ColorSpace::BT2020;
        else if (gst_video_colorimetry_matches(&GST_VIDEO_INFO_COLORIMETRY(videoInfo), GST_VIDEO_COLORIMETRY_SMPTE240M))
            colorSpace = DMABufBuffer::ColorSpace::SMPTE240M;
        dmabuf->setColorSpace(colorSpace);

        dmabuf->ref();
        gst_mini_object_set_qdata(GST_MINI_OBJECT_CAST(memory), dmabufQuark, dmabuf.get(), [](gpointer data) {
            static_cast<DMABufBuffer*>(data)->deref();
        });
    }
    return CoordinatedPlatformLayerBufferDMABuf::create(dmabuf.releaseNonNull(), m_flags, nullptr);
}
#endif // USE(GBM)

std::unique_ptr<CoordinatedPlatformLayerBuffer> CoordinatedPlatformLayerBufferVideo::createBufferFromGLMemory(GstBuffer* buffer, GstVideoInfo* videoInfo)
{
    m_isMapped = gst_video_frame_map(&m_videoFrame, videoInfo, buffer, static_cast<GstMapFlags>(GST_MAP_READ | GST_MAP_GL));
    if (!m_isMapped)
        return nullptr;

    if (GST_VIDEO_INFO_HAS_ALPHA(&m_videoFrame.info))
        m_flags.add({ TextureMapperFlags::ShouldBlend, TextureMapperFlags::ShouldPremultiply });

    auto textureTarget = gst_gl_memory_get_texture_target(GST_GL_MEMORY_CAST(gst_buffer_peek_memory(buffer, 0)));
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
    if (m_isMapped) {
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
                texture->updateContents(srcData, IntRect(0, 0, m_size.width(), m_size.height()), IntPoint(0, 0), stride, PixelFormat::BGRA8);
                m_buffer = CoordinatedPlatformLayerBufferRGB::create(WTFMove(texture), m_flags, nullptr);
                gst_video_frame_unmap(&m_videoFrame);
                m_isMapped = false;
            }
        }
    }

    if (m_buffer)
        m_buffer->paintToTextureMapper(textureMapper, targetRect, modelViewMatrix, opacity);
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && ENABLE(VIDEO) && USE(GSTREAMER)
