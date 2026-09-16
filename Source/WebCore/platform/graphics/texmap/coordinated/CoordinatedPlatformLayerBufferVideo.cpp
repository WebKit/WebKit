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

#if USE(TEXTURE_MAPPER)
#include "TextureMapper.h"
#else
#include "CoordinatedPlatformLayerBufferSkiaImage.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkImage.h>
#include <skia/core/SkPixmap.h>
#include <skia/gpu/ganesh/GrYUVABackendTextures.h>
#include <skia/gpu/ganesh/SkImageGanesh.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <skia/private/chromium/GrPromiseImageTexture.h>
#include <skia/private/chromium/SkImageChromium.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif

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

std::unique_ptr<CoordinatedPlatformLayerBufferVideo> CoordinatedPlatformLayerBufferVideo::create(Ref<VideoFrameGStreamer>&& frame, std::optional<GstVideoDecoderPlatform> videoDecoderPlatform, bool gstGLEnabled, OptionSet<TextureMapperFlags> flags, const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext)
{
    auto size = frame->presentationSize();
    return makeUnique<CoordinatedPlatformLayerBufferVideo>(WTF::move(frame), WTF::move(size), videoDecoderPlatform, gstGLEnabled, flags, threadSafeGrContext);
}

CoordinatedPlatformLayerBufferVideo::CoordinatedPlatformLayerBufferVideo(Ref<VideoFrameGStreamer>&& frame, IntSize&& size, std::optional<GstVideoDecoderPlatform> videoDecoderPlatform, bool gstGLEnabled, OptionSet<TextureMapperFlags> flags, const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext)
    : CoordinatedPlatformLayerBuffer(Type::Video, WTF::move(size), flags, nullptr)
    , m_videoFrame(WTF::move(frame))
    , m_videoDecoderPlatform(videoDecoderPlatform)
#if USE(TEXTURE_MAPPER)
    , m_buffer(createBufferIfNeeded(gstGLEnabled))
#endif
{
#if USE(TEXTURE_MAPPER)
    UNUSED_PARAM(threadSafeGrContext);
#else
    createSkiaImageIfNeeded(threadSafeGrContext, gstGLEnabled);
#endif
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

#if USE(TEXTURE_MAPPER)
std::unique_ptr<CoordinatedPlatformLayerBuffer> CoordinatedPlatformLayerBufferVideo::createBufferIfNeeded(bool gstGLEnabled)
{
    const auto& sample = m_videoFrame->sample();
    auto buffer = gst_sample_get_buffer(sample.get());
    auto memory = gst_buffer_peek_memory(buffer, 0);

#if USE(GBM)
    if (gst_is_fd_memory(memory) && m_videoDecoderPlatform && *m_videoDecoderPlatform == GstVideoDecoderPlatform::Qualcomm)
        return CoordinatedPlatformLayerBufferDMABuf::create(m_videoFrame->dmabufForQualcommDecoder(m_size), m_flags, nullptr);

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

static std::pair<CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace, CoordinatedPlatformLayerBufferYUV::TransferFunction> yuvColorSpaceFromVideoInfo(const GstVideoInfo& info)
{
    // Default to bt601. This is the same behaviour as GStreamer's glcolorconvert element.
    auto yuvToRgbColorSpace = CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::Bt601;
    auto transferFunction = CoordinatedPlatformLayerBufferYUV::TransferFunction::Bt709;
    const auto& colorimetry = GST_VIDEO_INFO_COLORIMETRY(&info);
    if (gst_video_colorimetry_matches(&colorimetry, GST_VIDEO_COLORIMETRY_BT709))
        yuvToRgbColorSpace = CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::Bt709;
    else if (gst_video_colorimetry_matches(&colorimetry, GST_VIDEO_COLORIMETRY_BT2020))
        yuvToRgbColorSpace = CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::Bt2020;
    else if (gst_video_colorimetry_matches(&colorimetry, GST_VIDEO_COLORIMETRY_BT2100_PQ)) {
        yuvToRgbColorSpace = CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::Bt2020;
        transferFunction = CoordinatedPlatformLayerBufferYUV::TransferFunction::Pq;
    } else if (gst_video_colorimetry_matches(&colorimetry, GST_VIDEO_COLORIMETRY_SMPTE240M))
        yuvToRgbColorSpace = CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::Smpte240M;
    return { yuvToRgbColorSpace, transferFunction };
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

    m_mappedVideoFrame->setNeedsCPUSync(m_videoDecoderPlatform != GstVideoDecoderPlatform::OpenMAX);

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

        auto [yuvToRgbColorSpace, transferFunction] = yuvColorSpaceFromVideoInfo(*m_mappedVideoFrame->info());
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
    m_mappedVideoFrame->waitForCPUSyncIfNeeded();
#endif

    if (m_buffer)
        return;

    OptionSet<BitmapTexture::Flags> textureFlags = { BitmapTexture::Flags::UseBGRALayout };
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

#else

#if USE(GSTREAMER_GL)
class PromiseGLVideoFrameContext final : public ThreadSafeRefCounted<PromiseGLVideoFrameContext> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(PromiseGLVideoFrameContext);
public:
    static Ref<PromiseGLVideoFrameContext> create(std::unique_ptr<GstMappedFrame>&& frame)
    {
        return adoptRef(*new PromiseGLVideoFrameContext(WTF::move(frame)));
    }

    ~PromiseGLVideoFrameContext() = default;

    sk_sp<GrPromiseImageTexture> promiseImageTexture(size_t planeIndex)
    {
        m_mappedFrame->waitForCPUSyncIfNeeded();

        GrGLTextureInfo externalTexture;
        externalTexture.fTarget = GL_TEXTURE_2D;
        externalTexture.fID = m_mappedFrame->textureID(planeIndex);
        externalTexture.fFormat = m_mappedFrame->textureFormat(planeIndex);
        auto textureSize = m_mappedFrame->textureSize(planeIndex);
        return GrPromiseImageTexture::Make(GrBackendTextures::MakeGL(textureSize.width(), textureSize.height(), skgpu::Mipmapped::kNo, externalTexture));
    }

private:
    explicit PromiseGLVideoFrameContext(std::unique_ptr<GstMappedFrame>&& mappedFrame)
        : m_mappedFrame(WTF::move(mappedFrame))
    {
    }

    std::unique_ptr<GstMappedFrame> m_mappedFrame;
};

struct PromiseGLVideoPlaneContext {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(PromiseGLVideoPlaneContext);

    PromiseGLVideoPlaneContext(Ref<PromiseGLVideoFrameContext>&& frameContext, size_t planeIndex)
        : context(WTF::move(frameContext))
        , index(planeIndex)
    {
    }

    Ref<PromiseGLVideoFrameContext> context;
    size_t index { 0 };
};

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(PromiseGLVideoPlaneContext);

static bool isSinglePlaneGLMemory(GstGLMemory* memory, GstVideoInfo* videoInfo, std::optional<GstVideoDecoderPlatform> videoDecoderPlatform)
{
    if (gst_gl_memory_get_texture_target(memory) == GST_GL_TEXTURE_TARGET_EXTERNAL_OES)
        return true;

    if (GST_VIDEO_INFO_IS_RGB(videoInfo) && GST_VIDEO_INFO_N_PLANES(videoInfo) == 1)
        return true;

    if (videoDecoderPlatform && *videoDecoderPlatform == GstVideoDecoderPlatform::ImxVPU && GST_VIDEO_INFO_IS_YUV(videoInfo) && GST_VIDEO_INFO_N_COMPONENTS(videoInfo) >= 3 && GST_VIDEO_INFO_N_PLANES(videoInfo) <= 4)
        return true;

    return false;
}
#endif

void CoordinatedPlatformLayerBufferVideo::createSkiaImageIfNeeded(const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext, bool gstGLEnabled)
{
    const auto& sample = m_videoFrame->sample();
    auto* buffer = gst_sample_get_buffer(sample.get());
    auto* memory = gst_buffer_peek_memory(buffer, 0);

#if USE(GBM)
    if (gst_is_fd_memory(memory) && m_videoDecoderPlatform && *m_videoDecoderPlatform == GstVideoDecoderPlatform::Qualcomm) {
        createSkiaImageForQualcommDecoder(threadSafeGrContext);
        return;
    }

#if GST_CHECK_VERSION(1, 24, 0)
    if (gst_is_dmabuf_memory(memory)) {
        createSkiaImageForDMABufMemory(threadSafeGrContext);
        return;
    }
#endif
#endif // USE(GBM)

    unsigned mapFlags = GST_MAP_READ;
#if USE(GSTREAMER_GL)
    if (gstGLEnabled && gst_is_gl_memory(memory))
        mapFlags |= GST_MAP_GL;
#else
    UNUSED_PARAM(gstGLEnabled);
#endif

    auto mappedFrame = makeUnique<GstMappedFrame>(sample, static_cast<GstMapFlags>(mapFlags));
    if (!mappedFrame->isValid()) {
        LOG_ERROR("Failed to create Skia image for video buffer: error mapping video frame");
        return;
    }

    if (GST_VIDEO_INFO_HAS_ALPHA(mappedFrame->info()))
        m_flags.add({ TextureMapperFlags::ShouldBlend, TextureMapperFlags::ShouldPremultiply });

    if (mapFlags == GST_MAP_READ) {
        createSkiaImageForMainMemory(WTF::move(mappedFrame));
        return;
    }

#if USE(GSTREAMER_GL)
    if (mapFlags & GST_MAP_GL) {
        createSkiaImageForGLMemory(GST_GL_MEMORY_CAST(memory), WTF::move(mappedFrame), threadSafeGrContext);
        return;
    }
#endif
}

#if USE(GBM)
void CoordinatedPlatformLayerBufferVideo::createSkiaImageForQualcommDecoder(const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext)
{
    auto dmabuf = m_videoFrame->dmabufForQualcommDecoder(m_size);
    auto alphaType = m_flags.contains(TextureMapperFlags::ShouldBlend) ? kPremul_SkAlphaType : kOpaque_SkAlphaType;
    auto origin = m_flags.contains(TextureMapperFlags::ShouldFlipTexture) ? kBottomLeft_GrSurfaceOrigin : kTopLeft_GrSurfaceOrigin;
    m_image = dmabuf->createPromiseImageForQualcommVideoFrame(threadSafeGrContext, kRGBA_8888_SkColorType, alphaType, origin);
}

#if GST_CHECK_VERSION(1, 24, 0)
void CoordinatedPlatformLayerBufferVideo::createSkiaImageForDMABufMemory(const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext)
{
    const auto& videoInfo = m_videoFrame->info();
    if (GST_VIDEO_INFO_HAS_ALPHA(&videoInfo))
        m_flags.add({ TextureMapperFlags::ShouldBlend, TextureMapperFlags::ShouldPremultiply });

    auto dmabuf = m_videoFrame->getDMABuf();
    ASSERT(dmabuf);

    auto alphaType = m_flags.contains(TextureMapperFlags::ShouldBlend) ? kPremul_SkAlphaType : kOpaque_SkAlphaType;
    auto origin = m_flags.contains(TextureMapperFlags::ShouldFlipTexture) ? kBottomLeft_GrSurfaceOrigin : kTopLeft_GrSurfaceOrigin;
    m_image = dmabuf->createPromiseImage(threadSafeGrContext, kRGBA_8888_SkColorType, alphaType, origin, nullptr, { });
}
#endif
#endif // USE(GBM)

void CoordinatedPlatformLayerBufferVideo::createSkiaImageForMainMemory(std::unique_ptr<GstMappedFrame>&& mappedFrame)
{
    auto alphaType = GST_VIDEO_INFO_HAS_ALPHA(mappedFrame->info()) ? kUnpremul_SkAlphaType : kOpaque_SkAlphaType;
    auto imageInfo = SkImageInfo::Make(mappedFrame->width(), mappedFrame->height(), kBGRA_8888_SkColorType, alphaType, SkColorSpace::MakeSRGB());
    SkPixmap pixmap(imageInfo, mappedFrame->planeData(0).data(), mappedFrame->planeStride(0));
    m_image = SkImages::RasterFromPixmap(pixmap, [](const void*, void* userData) {
        std::unique_ptr<GstMappedFrame> mappedFrame(static_cast<GstMappedFrame*>(userData));
    }, mappedFrame.release());
}

#if USE(GSTREAMER_GL)
void CoordinatedPlatformLayerBufferVideo::createSkiaImageForGLMemory(GstGLMemory* glMemory, std::unique_ptr<GstMappedFrame>&& mappedFrame, const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext)
{
    mappedFrame->setNeedsCPUSync(m_videoDecoderPlatform != GstVideoDecoderPlatform::OpenMAX);

    auto* videoInfo = mappedFrame->info();
    if (isSinglePlaneGLMemory(glMemory, videoInfo, m_videoDecoderPlatform)) {
        createSkiaImageForSinglePlaneGLMemory(glMemory, WTF::move(mappedFrame), threadSafeGrContext);
        return;
    }

    if (GST_VIDEO_INFO_IS_YUV(videoInfo) && GST_VIDEO_INFO_N_COMPONENTS(videoInfo) >= 3 && GST_VIDEO_INFO_N_PLANES(videoInfo) <= 4)
        createSkiaImageForYUVGLMemory(WTF::move(mappedFrame), threadSafeGrContext);
}

void CoordinatedPlatformLayerBufferVideo::createSkiaImageForSinglePlaneGLMemory(GstGLMemory* glMemory, std::unique_ptr<GstMappedFrame>&& mappedFrame, const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext)
{
    const bool isExternal = gst_gl_memory_get_texture_target(glMemory) == GST_GL_TEXTURE_TARGET_EXTERNAL_OES;
    auto backendFormat = isExternal ? GrBackendFormats::MakeGLExternal() : threadSafeGrContext->defaultBackendFormat(kRGBA_8888_SkColorType, GrRenderable::kYes);
    ASSERT(backendFormat.isValid());

    auto frameSize = SkISize::Make(mappedFrame->width(), mappedFrame->height());
    auto origin = m_flags.contains(TextureMapperFlags::ShouldFlipTexture) ? kBottomLeft_GrSurfaceOrigin : kTopLeft_GrSurfaceOrigin;
    auto alphaType = GST_VIDEO_INFO_HAS_ALPHA(mappedFrame->info()) ? kUnpremul_SkAlphaType : kOpaque_SkAlphaType;

    m_image = SkImages::PromiseTextureFrom(threadSafeGrContext, backendFormat, frameSize, skgpu::Mipmapped::kNo,
        origin, kRGBA_8888_SkColorType, alphaType, SkColorSpace::MakeSRGB(),
        +[](void* userData) -> sk_sp<GrPromiseImageTexture> {
            auto& mappedFrame = *static_cast<GstMappedFrame*>(userData);
            mappedFrame.waitForCPUSyncIfNeeded();

            auto* memory = GST_GL_MEMORY_CAST(gst_buffer_peek_memory(mappedFrame.get()->buffer, 0));
            GrGLTextureInfo externalTexture;
            externalTexture.fTarget = gst_gl_memory_get_texture_target(memory) == GST_GL_TEXTURE_TARGET_EXTERNAL_OES ? GL_TEXTURE_EXTERNAL_OES : GL_TEXTURE_2D;
            externalTexture.fID = mappedFrame.textureID(0);
            externalTexture.fFormat = GL_RGBA8;
            return GrPromiseImageTexture::Make(GrBackendTextures::MakeGL(mappedFrame.width(), mappedFrame.height(), skgpu::Mipmapped::kNo, externalTexture));
        },
        +[](void* userData) {
            std::unique_ptr<GstMappedFrame> mappedFrame(static_cast<GstMappedFrame*>(userData));
        }, mappedFrame.release());
}

static std::optional<SkYUVAInfo> buildYUVAInfoFromMappedFrame(GstMappedFrame& mappedFrame)
{
    auto planeConfig = SkYUVAInfo::PlaneConfig::kUnknown;
    auto subsampling = SkYUVAInfo::Subsampling::kUnknown;
    switch (GST_VIDEO_INFO_FORMAT(mappedFrame.info())) {
    case GST_VIDEO_FORMAT_BGRx:
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_RGBA:
        planeConfig = SkYUVAInfo::PlaneConfig::kYUVA;
        subsampling = SkYUVAInfo::Subsampling::k444;
        break;
    case GST_VIDEO_FORMAT_I420:
        planeConfig = SkYUVAInfo::PlaneConfig::kY_U_V;
        subsampling = SkYUVAInfo::Subsampling::k420;
        break;
    case GST_VIDEO_FORMAT_YV12:
        planeConfig = SkYUVAInfo::PlaneConfig::kY_V_U;
        subsampling = SkYUVAInfo::Subsampling::k420;
        break;
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_P010_10LE:
        planeConfig = SkYUVAInfo::PlaneConfig::kY_UV;
        subsampling = SkYUVAInfo::Subsampling::k420;
        break;
    case GST_VIDEO_FORMAT_NV21:
        planeConfig = SkYUVAInfo::PlaneConfig::kY_VU;
        subsampling = SkYUVAInfo::Subsampling::k420;
        break;
    case GST_VIDEO_FORMAT_Y444:
        planeConfig = SkYUVAInfo::PlaneConfig::kY_U_V;
        subsampling = SkYUVAInfo::Subsampling::k444;
        break;
    case GST_VIDEO_FORMAT_Y41B:
        planeConfig = SkYUVAInfo::PlaneConfig::kY_U_V;
        subsampling = SkYUVAInfo::Subsampling::k411;
        break;
    case GST_VIDEO_FORMAT_Y42B:
        planeConfig = SkYUVAInfo::PlaneConfig::kY_U_V;
        subsampling = SkYUVAInfo::Subsampling::k422;
        break;
    case GST_VIDEO_FORMAT_A420:
        planeConfig = SkYUVAInfo::PlaneConfig::kY_U_V_A;
        subsampling = SkYUVAInfo::Subsampling::k420;
        break;
    default:
        break;
    }

    if (planeConfig == SkYUVAInfo::PlaneConfig::kUnknown) {
        LOG_ERROR("Failed to create Skia image for video buffer: unsupported buffer format");
        return std::nullopt;
    }

    const auto& colorimetry = GST_VIDEO_INFO_COLORIMETRY(mappedFrame.info());

    // Default to bt601. This is the same behaviour as GStreamer's glcolorconvert element.
    SkYUVColorSpace yuvaColorSpace = kRec601_Limited_SkYUVColorSpace;
    if (gst_video_colorimetry_matches(&colorimetry, GST_VIDEO_COLORIMETRY_BT709))
        yuvaColorSpace = kRec709_Full_SkYUVColorSpace;
    else if (gst_video_colorimetry_matches(&colorimetry, GST_VIDEO_COLORIMETRY_BT2020))
        yuvaColorSpace = kBT2020_8bit_Full_SkYUVColorSpace;
    else if (gst_video_colorimetry_matches(&colorimetry, GST_VIDEO_COLORIMETRY_BT2100_PQ))
        yuvaColorSpace = kBT2020_8bit_Full_SkYUVColorSpace;
    else if (gst_video_colorimetry_matches(&colorimetry, GST_VIDEO_COLORIMETRY_SMPTE240M))
        yuvaColorSpace = kSMPTE240_Full_SkYUVColorSpace;

    return SkYUVAInfo(SkISize::Make(mappedFrame.width(), mappedFrame.height()), planeConfig, subsampling, yuvaColorSpace);
}

void CoordinatedPlatformLayerBufferVideo::createSkiaImageForYUVGLMemory(std::unique_ptr<GstMappedFrame>&& mappedFrame, const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext)
{
    auto info = buildYUVAInfoFromMappedFrame(*mappedFrame);
    if (!info)
        return;

    auto* videoInfo = mappedFrame->info();
    unsigned planeCount = GST_VIDEO_INFO_N_PLANES(videoInfo);
    ASSERT(planeCount <= 4);
    std::array<GrBackendFormat, 4> backendFormats;
    for (unsigned i = 0; i < planeCount; ++i)
        backendFormats[i] = GrBackendFormats::MakeGL(mappedFrame->textureFormat(i), GL_TEXTURE_2D);

    auto origin = m_flags.contains(TextureMapperFlags::ShouldFlipTexture) ? kBottomLeft_GrSurfaceOrigin : kTopLeft_GrSurfaceOrigin;
    GrYUVABackendTextureInfo yuvaBackendTexturesInfo(*info, backendFormats.data(), skgpu::Mipmapped::kNo, origin);
    if (!yuvaBackendTexturesInfo.isValid()) {
        LOG_ERROR("Failed to create Skia image for YUV video buffer: invalid backend texture information");
        return;
    }

    Ref context = PromiseGLVideoFrameContext::create(WTF::move(mappedFrame));
    std::array<PromiseGLVideoPlaneContext*, 4> planeContexts;
    for (unsigned i = 0; i < planeCount; ++i)
        planeContexts[i] = makeUnique<PromiseGLVideoPlaneContext>(context.copyRef(), i).release();

    auto colorSpace = [&]() -> sk_sp<SkColorSpace> {
        const auto& colorimetry = GST_VIDEO_INFO_COLORIMETRY(videoInfo);
        if (gst_video_colorimetry_matches(&colorimetry, GST_VIDEO_COLORIMETRY_BT2100_PQ))
            return SkColorSpace::MakeRGB(SkNamedTransferFn::kPQ, SkNamedGamut::kRec2020);
        return SkColorSpace::MakeRGB(SkNamedTransferFn::kRec709, SkNamedGamut::kSRGB);
    }();

    m_image = SkImages::PromiseTextureFromYUVA(threadSafeGrContext, yuvaBackendTexturesInfo, colorSpace,
        +[](void* userData) -> sk_sp<GrPromiseImageTexture> {
            auto& planeContext = *static_cast<PromiseGLVideoPlaneContext*>(userData);
            return planeContext.context->promiseImageTexture(planeContext.index);
        },
        +[](void* userData) {
            std::unique_ptr<PromiseGLVideoPlaneContext> planeContext(static_cast<PromiseGLVideoPlaneContext*>(userData));
        }, reinterpret_cast<void**>(planeContexts.data()));
}
#endif

sk_sp<SkImage> CoordinatedPlatformLayerBufferVideo::skiaImage()
{
    if (m_image)
        return m_image;

    if (m_buffer)
        return m_buffer->skiaImage();

    return nullptr;
}
#endif

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && ENABLE(VIDEO) && USE(GSTREAMER)
