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
#include "BitmapTexturePool.h"
#include "CoordinatedPlatformLayerBufferExternalOES.h"
#include "CoordinatedPlatformLayerBufferRGB.h"
#include "CoordinatedPlatformLayerBufferYUV.h"
#include "GraphicsTypesGL.h"
#include "TextureMapper.h"

#if USE(GSTREAMER_GL)
// Include the <epoxy/gl.h> header before <gst/gl/gl.h>.
#include <epoxy/gl.h>
#include <gst/gl/gl.h>
#endif

#if USE(GBM)
#include "CoordinatedPlatformLayerBufferDMABuf.h"
#include "DMABufBuffer.h"
#include <drm_fourcc.h>
#include <gst/allocators/gstdmabuf.h>
#include <wtf/unix/UnixFileDescriptor.h>
#endif

namespace WebCore {

std::unique_ptr<CoordinatedPlatformLayerBufferVideo> CoordinatedPlatformLayerBufferVideo::create(Ref<VideoFrameGStreamer>&& frame, std::optional<GstVideoDecoderPlatform> videoDecoderPlatform, bool gstGLEnabled, OptionSet<TextureMapperFlags> flags)
{
    auto size = frame->presentationSize();
    return makeUnique<CoordinatedPlatformLayerBufferVideo>(WTF::move(frame), WTF::move(size), videoDecoderPlatform, gstGLEnabled, flags);
}

CoordinatedPlatformLayerBufferVideo::CoordinatedPlatformLayerBufferVideo(Ref<VideoFrameGStreamer>&& frame, IntSize&& size, std::optional<GstVideoDecoderPlatform> videoDecoderPlatform, bool gstGLEnabled, OptionSet<TextureMapperFlags> flags)
    : CoordinatedPlatformLayerBuffer(Type::Video, WTF::move(size), flags, nullptr)
    , m_videoFrame(WTF::move(frame))
    , m_videoDecoderPlatform(videoDecoderPlatform)
    , m_buffer(createBufferIfNeeded(gstGLEnabled))
{
}

CoordinatedPlatformLayerBufferVideo::~CoordinatedPlatformLayerBufferVideo() = default;

std::unique_ptr<CoordinatedPlatformLayerBuffer> CoordinatedPlatformLayerBufferVideo::copyBuffer() const
{
    if (!m_buffer || !is<CoordinatedPlatformLayerBufferRGB>(*m_buffer))
        return nullptr;

    auto& buffer = downcast<CoordinatedPlatformLayerBufferRGB>(*m_buffer);
    auto textureID = buffer.textureID();
    if (!textureID)
        return nullptr;

    auto size = buffer.size();
    auto texture = BitmapTexture::create(size);
    texture->copyFromExternalTexture(textureID, { IntPoint::zero(), size }, { });
    return CoordinatedPlatformLayerBufferRGB::create(WTF::move(texture), m_flags, nullptr);
}

std::unique_ptr<CoordinatedPlatformLayerBuffer> CoordinatedPlatformLayerBufferVideo::createBufferIfNeeded(bool gstGLEnabled)
{
    const auto& sample = m_videoFrame->sample();
    auto buffer = gst_sample_get_buffer(sample.get());
    auto memory = gst_buffer_peek_memory(buffer, 0);

#if USE(GBM)
    if (gst_is_fd_memory(memory) && m_videoDecoderPlatform && *m_videoDecoderPlatform == GstVideoDecoderPlatform::Qualcomm) {
        // The buffers produced by the Qualcomm decoder contain a single GstMemory which stores the
        // GBM FD pointing to the decoded frame. The frame format is YUV (NV12). As this is stored
        // in a single memory the existing DMABuf/YUV layer buffers cannot be used for rendering. So
        // we rely on the EXT_YUV_target OpenGL ES extension to convert it to a RGB texture for
        // rendering.
        auto dmabufFormat = m_videoFrame->dmaBufFormat();
        RELEASE_ASSERT(dmabufFormat);
        return CoordinatedPlatformLayerBufferExternalOES::create(GRefPtr(buffer), dmabufFormat->first, m_size, m_flags);
    }

#if GST_CHECK_VERSION(1, 24, 0)
    if (gst_is_dmabuf_memory(memory))
        return createBufferFromDMABufMemory();
#endif // GST_CHECK_VERSION(1, 24, 0)
#endif // USE(GBM)

#if USE(GSTREAMER_GL)
    if (gstGLEnabled && gst_is_gl_memory(memory))
        return createBufferFromGLMemory();
#else
    UNUSED_PARAM(gstGLEnabled);
#endif

    // When not having a texture, we map the frame here and upload the pixels to a texture in the
    // compositor thread, in paintToTextureMapper(), which also allows us to use the texture mapper
    // bitmap texture pool.
    m_mappedVideoFrame.emplace(GstMappedFrame(sample, GST_MAP_READ));
    if (!*m_mappedVideoFrame) {
        // If mapping failed, clear the GstMappedFrame holder.
        m_mappedVideoFrame = std::nullopt;
        return nullptr;
    }

    if (GST_VIDEO_INFO_HAS_ALPHA(m_mappedVideoFrame->info()))
        m_flags.add({ TextureMapperFlags::ShouldBlend, TextureMapperFlags::ShouldPremultiply });

    return nullptr;
}

#if USE(GBM) && GST_CHECK_VERSION(1, 24, 0)
std::unique_ptr<CoordinatedPlatformLayerBuffer> CoordinatedPlatformLayerBufferVideo::createBufferFromDMABufMemory()
{
    auto videoInfo = m_videoFrame->info();
    if (GST_VIDEO_INFO_HAS_ALPHA(&videoInfo))
        m_flags.add({ TextureMapperFlags::ShouldBlend, TextureMapperFlags::ShouldPremultiply });

    auto dmabuf = m_videoFrame->getDMABuf();
    RELEASE_ASSERT(dmabuf);
    return CoordinatedPlatformLayerBufferDMABuf::create(dmabuf.releaseNonNull(), m_flags, nullptr);
}
#endif // USE(GBM) && GST_CHECK_VERSION(1, 24, 0)

#if USE(GSTREAMER_GL)
static std::optional<CoordinatedPlatformLayerBufferYUV::Format> yuvFormatFromGstVideoFormat(GstVideoFormat format)
{
    switch (format) {
    case GST_VIDEO_FORMAT_BGRx:
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_RGBA:
        return CoordinatedPlatformLayerBufferYUV::Format::AYUV;
    case GST_VIDEO_FORMAT_I420:
        return CoordinatedPlatformLayerBufferYUV::Format::YUV420;
    case GST_VIDEO_FORMAT_YV12:
        return CoordinatedPlatformLayerBufferYUV::Format::YVU420;
    case GST_VIDEO_FORMAT_NV12:
        return CoordinatedPlatformLayerBufferYUV::Format::NV12;
    case GST_VIDEO_FORMAT_NV21:
        return CoordinatedPlatformLayerBufferYUV::Format::NV21;
    case GST_VIDEO_FORMAT_Y444:
        return CoordinatedPlatformLayerBufferYUV::Format::YUV444;
    case GST_VIDEO_FORMAT_Y41B:
        return CoordinatedPlatformLayerBufferYUV::Format::YUV411;
    case GST_VIDEO_FORMAT_Y42B:
        return CoordinatedPlatformLayerBufferYUV::Format::YUV422;
    case GST_VIDEO_FORMAT_P010_10LE:
        return CoordinatedPlatformLayerBufferYUV::Format::P010;
    case GST_VIDEO_FORMAT_A420:
        return CoordinatedPlatformLayerBufferYUV::Format::A420;
    default:
        break;
    }

    return std::nullopt;
}

std::unique_ptr<CoordinatedPlatformLayerBuffer> CoordinatedPlatformLayerBufferVideo::createBufferFromGLMemory()
{
    const auto& sample = m_videoFrame->sample();
    m_mappedVideoFrame.emplace(GstMappedFrame(sample, static_cast<GstMapFlags>(GST_MAP_READ | GST_MAP_GL)));
    if (!*m_mappedVideoFrame) {
        // If mapping failed, clear the GstMappedFrame holder.
        m_mappedVideoFrame = std::nullopt;
        return nullptr;
    }

    if (GST_VIDEO_INFO_HAS_ALPHA(m_mappedVideoFrame->info()))
        m_flags.add({ TextureMapperFlags::ShouldBlend, TextureMapperFlags::ShouldPremultiply });

    auto buffer = gst_sample_get_buffer(sample.get());
    auto textureTarget = gst_gl_memory_get_texture_target(GST_GL_MEMORY_CAST(gst_buffer_peek_memory(buffer, 0)));
    if (textureTarget == GST_GL_TEXTURE_TARGET_EXTERNAL_OES)
        return CoordinatedPlatformLayerBufferExternalOES::create(m_mappedVideoFrame->textureID(0), m_size, m_flags, nullptr);

    if ((GST_VIDEO_INFO_IS_RGB(m_mappedVideoFrame->info()) && GST_VIDEO_INFO_N_PLANES(m_mappedVideoFrame->info()) == 1))
        return CoordinatedPlatformLayerBufferRGB::create(m_mappedVideoFrame->textureID(0), m_size, m_flags, nullptr);

    if (GST_VIDEO_INFO_IS_YUV(m_mappedVideoFrame->info()) && GST_VIDEO_INFO_N_COMPONENTS(m_mappedVideoFrame->info()) >= 3 && GST_VIDEO_INFO_N_PLANES(m_mappedVideoFrame->info()) <= 4) {
        if (m_videoDecoderPlatform && *m_videoDecoderPlatform == GstVideoDecoderPlatform::ImxVPU) {
            // IMX VPU decoder decodes YUV data only into the Y texture from which the sampler
            // then directly produces RGBA data. Textures for other planes aren't used, but
            // that's decoder's problem. We have to treat that Y texture as having RGBA data.
            return CoordinatedPlatformLayerBufferRGB::create(m_mappedVideoFrame->textureID(0), m_size, m_flags, nullptr);
        }

        auto format = yuvFormatFromGstVideoFormat(GST_VIDEO_INFO_FORMAT(m_mappedVideoFrame->info()));
        if (!format) {
            // If format is not supported clear the GstMappedFrame holder, otherwise it will be considered
            // as main memory mapped buffer and we will try to upload it later.
            m_mappedVideoFrame = std::nullopt;
            return nullptr;
        }

        unsigned numberOfPlanes = GST_VIDEO_INFO_N_PLANES(m_mappedVideoFrame->info());
        std::array<GLuint, 4> planes;
        std::array<unsigned, 4> yuvPlane;
        std::array<unsigned, 4> yuvPlaneOffset;
        for (unsigned i = 0; i < numberOfPlanes; ++i)
            planes[i] = m_mappedVideoFrame->textureID(i);
        for (unsigned i = 0; i < numberOfPlanes; ++i) {
            yuvPlane[i] = m_mappedVideoFrame->componentPlane(i);
            yuvPlaneOffset[i] = m_mappedVideoFrame->componentPlaneOffset(i);
        }

        // Default to bt601. This is the same behaviour as GStreamer's glcolorconvert element.
        CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace yuvToRgbColorSpace = CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::Bt601;
        CoordinatedPlatformLayerBufferYUV::TransferFunction transferFunction = CoordinatedPlatformLayerBufferYUV::TransferFunction::Bt709;
        if (gst_video_colorimetry_matches(&GST_VIDEO_INFO_COLORIMETRY(m_mappedVideoFrame->info()), GST_VIDEO_COLORIMETRY_BT709))
            yuvToRgbColorSpace = CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::Bt709;
        else if (gst_video_colorimetry_matches(&GST_VIDEO_INFO_COLORIMETRY(m_mappedVideoFrame->info()), GST_VIDEO_COLORIMETRY_BT2020))
            yuvToRgbColorSpace = CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::Bt2020;
        else if (gst_video_colorimetry_matches(&GST_VIDEO_INFO_COLORIMETRY(m_mappedVideoFrame->info()), GST_VIDEO_COLORIMETRY_BT2100_PQ)) {
            yuvToRgbColorSpace = CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::Bt2020;
            transferFunction = CoordinatedPlatformLayerBufferYUV::TransferFunction::Pq;
        } else if (gst_video_colorimetry_matches(&GST_VIDEO_INFO_COLORIMETRY(m_mappedVideoFrame->info()), GST_VIDEO_COLORIMETRY_SMPTE240M))
            yuvToRgbColorSpace = CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::Smpte240M;

        return CoordinatedPlatformLayerBufferYUV::create(*format, numberOfPlanes, WTF::move(planes), WTF::move(yuvPlane), WTF::move(yuvPlaneOffset), yuvToRgbColorSpace, transferFunction, m_size, m_flags, nullptr);
    }

    return nullptr;
}
#endif

void CoordinatedPlatformLayerBufferVideo::createBufferFromMappedFrameIfNeeded()
{
    if (!m_mappedVideoFrame)
        return;

    RELEASE_ASSERT(*m_mappedVideoFrame);
#if USE(GSTREAMER_GL)
    if (m_videoDecoderPlatform != GstVideoDecoderPlatform::OpenMAX) {
        if (auto* meta = gst_buffer_get_gl_sync_meta(m_mappedVideoFrame->get()->buffer)) {
            GstMemory* memory = gst_buffer_peek_memory(m_mappedVideoFrame->get()->buffer, 0);
            GstGLContext* context = reinterpret_cast<GstGLBaseMemory*>(memory)->context;
            gst_gl_sync_meta_wait_cpu(meta, context);
        }
    }
#endif

    if (m_buffer)
        return;

    OptionSet<BitmapTexture::Flags> textureFlags;
    if (GST_VIDEO_INFO_HAS_ALPHA(m_mappedVideoFrame->info()))
        textureFlags.add(BitmapTexture::Flags::SupportsAlpha);
    auto texture = BitmapTexturePool::singleton().acquireTexture(m_size, textureFlags);

    auto* meta = gst_buffer_get_video_gl_texture_upload_meta(m_mappedVideoFrame->get()->buffer);
    if (meta && meta->n_textures == 1) {
        guint ids[4] = { texture->id(), 0, 0, 0 };
        if (gst_video_gl_texture_upload_meta_upload(meta, ids)) {
            m_buffer = CoordinatedPlatformLayerBufferRGB::create(WTF::move(texture), m_flags, nullptr);
            return;
        }
    }

    int stride = m_mappedVideoFrame->planeStride(0);
    auto srcData = m_mappedVideoFrame->planeData(0);
    IntPoint origin;
    texture->updateContents(srcData.data(), IntRect(origin, m_size), origin, stride, PixelFormat::BGRA8);
    m_buffer = CoordinatedPlatformLayerBufferRGB::create(WTF::move(texture), m_flags, nullptr);
    m_mappedVideoFrame = std::nullopt;
}

void CoordinatedPlatformLayerBufferVideo::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    createBufferFromMappedFrameIfNeeded();

    if (m_buffer)
        m_buffer->paintToTextureMapper(textureMapper, targetRect, modelViewMatrix, opacity);
}

#if USE(SKIA)
sk_sp<SkImage> CoordinatedPlatformLayerBufferVideo::skiaImage()
{
    createBufferFromMappedFrameIfNeeded();

    if (m_buffer)
        return m_buffer->skiaImage();

    return nullptr;
}
#endif

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && ENABLE(VIDEO) && USE(GSTREAMER)
