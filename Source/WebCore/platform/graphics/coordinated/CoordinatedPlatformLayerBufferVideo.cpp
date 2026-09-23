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

#if USE(COORDINATED_GRAPHICS) && !USE(TEXTURE_MAPPER) && ENABLE(VIDEO) && USE(GSTREAMER)
#include "CoordinatedPlatformLayerBufferSkiaImage.h"
#include "GLFence.h"
#include "ImageOrientation.h"
#include "PlatformDisplay.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkCanvas.h>
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkImage.h>
#include <skia/core/SkPixmap.h>
#include <skia/core/SkSurface.h>
#include <skia/gpu/ganesh/GrYUVABackendTextures.h>
#include <skia/gpu/ganesh/SkImageGanesh.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <skia/private/chromium/GrPromiseImageTexture.h>
#include <skia/private/chromium/SkImageChromium.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

#if USE(GSTREAMER_GL)
// Include the <epoxy/gl.h> header before <gst/gl/gl.h>.
#include <epoxy/gl.h>
#include <gst/gl/gl.h>
#endif

#if USE(GBM)
#include "DMABufBuffer.h"
#include <gst/allocators/gstdmabuf.h>
#endif

namespace WebCore {

std::unique_ptr<CoordinatedPlatformLayerBufferVideo> CoordinatedPlatformLayerBufferVideo::create(Ref<VideoFrameGStreamer>&& frame, std::optional<GstVideoDecoderPlatform> videoDecoderPlatform, bool gstGLEnabled, const ImageOrientation& orientation, const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext)
{
    auto size = frame->presentationSize();
    Origin origin = Origin::TopLeft;
    Rotation rotation = Rotation::None;
    switch (orientation.orientation()) {
    case ImageOrientation::Orientation::OriginTopLeft:
        break;
    case ImageOrientation::Orientation::OriginRightTop:
        rotation = Rotation::Right;
        break;
    case ImageOrientation::Orientation::OriginBottomRight:
        rotation = Rotation::UpsideDown;
        break;
    case ImageOrientation::Orientation::OriginLeftBottom:
        rotation = Rotation::Left;
        break;
    case ImageOrientation::Orientation::OriginBottomLeft:
        origin = Origin::BottomLeft;
        break;
    default:
        // FIXME: Handle OriginTopRight, OriginLeftTop and OriginRightBottom.
        break;
    }
    return makeUnique<CoordinatedPlatformLayerBufferVideo>(WTF::move(frame), WTF::move(size), videoDecoderPlatform, gstGLEnabled, origin, rotation, threadSafeGrContext);
}

CoordinatedPlatformLayerBufferVideo::CoordinatedPlatformLayerBufferVideo(Ref<VideoFrameGStreamer>&& frame, IntSize&& size, std::optional<GstVideoDecoderPlatform> videoDecoderPlatform, bool gstGLEnabled, Origin origin, Rotation rotation, const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext)
    : CoordinatedPlatformLayerBuffer(Type::Video, WTF::move(size), AlphaMode::Opaque, rotation)
    , m_videoFrame(WTF::move(frame))
    , m_videoDecoderPlatform(videoDecoderPlatform)
{
    createSkiaImageIfNeeded(threadSafeGrContext, gstGLEnabled, origin);
}

CoordinatedPlatformLayerBufferVideo::~CoordinatedPlatformLayerBufferVideo() = default;

#if USE(GSTREAMER_GL)
std::unique_ptr<CoordinatedPlatformLayerBuffer> CoordinatedPlatformLayerBufferVideo::copyBuffer() const
{
    if (!m_image)
        return nullptr;

    auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
    auto imageInfo = SkImageInfo::Make(m_image->width(), m_image->height(), kRGBA_8888_SkColorType, toSkiaAlphaType(m_alphaMode), SkColorSpace::MakeSRGB());
    auto surface = SkSurfaces::RenderTarget(grContext, skgpu::Budgeted::kNo, imageInfo, 0, kTopLeft_GrSurfaceOrigin, nullptr);
    if (!surface)
        return nullptr;

    auto* canvas = surface->getCanvas();
    if (!canvas)
        return nullptr;

    SkPaint paint;
    paint.setBlendMode(SkBlendMode::kSrc);
    canvas->drawImage(m_image, 0, 0, SkSamplingOptions(SkFilterMode::kNearest, SkMipmapMode::kNone), &paint);
    grContext->flushAndSubmit(surface.get(), GrSyncCpu::kNo);

    auto image = surface->makeImageSnapshot();
    if (!image)
        return nullptr;

    // We can't use CoordinatedPlatformLayerBufferSkiaImage::create here because we don't want the
    // image to be re-wrapped into a promise image.
    return makeUnique<CoordinatedPlatformLayerBufferSkiaImage>(WTF::move(image), m_alphaMode, m_rotation);
}

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
#endif // USE(GSTREAMER_GL)

void CoordinatedPlatformLayerBufferVideo::createSkiaImageIfNeeded(const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext, bool gstGLEnabled, Origin origin)
{
    const auto& sample = m_videoFrame->sample();
    auto* buffer = gst_sample_get_buffer(sample.get());
    auto* memory = gst_buffer_peek_memory(buffer, 0);

#if USE(GBM)
    if (gst_is_fd_memory(memory) && m_videoDecoderPlatform && *m_videoDecoderPlatform == GstVideoDecoderPlatform::Qualcomm) {
        createSkiaImageForQualcommDecoder(threadSafeGrContext, origin);
        return;
    }

#if GST_CHECK_VERSION(1, 24, 0)
    if (gst_is_dmabuf_memory(memory)) {
        createSkiaImageForDMABufMemory(threadSafeGrContext, origin);
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
        m_alphaMode = AlphaMode::Unpremultiplied;

    if (mapFlags == GST_MAP_READ) {
        createSkiaImageForMainMemory(WTF::move(mappedFrame));
        return;
    }

#if USE(GSTREAMER_GL)
    if (mapFlags & GST_MAP_GL) {
        createSkiaImageForGLMemory(GST_GL_MEMORY_CAST(memory), WTF::move(mappedFrame), threadSafeGrContext, origin);
        return;
    }
#endif
}

#if USE(GBM)
void CoordinatedPlatformLayerBufferVideo::createSkiaImageForQualcommDecoder(const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext, Origin origin)
{
    auto dmabuf = m_videoFrame->dmabufForQualcommDecoder(m_size);
    m_image = dmabuf->createPromiseImageForQualcommVideoFrame(threadSafeGrContext, kRGBA_8888_SkColorType, toSkiaAlphaType(m_alphaMode), toSkiaOrigin(origin));
}

#if GST_CHECK_VERSION(1, 24, 0)
void CoordinatedPlatformLayerBufferVideo::createSkiaImageForDMABufMemory(const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext, Origin origin)
{
    const auto& videoInfo = m_videoFrame->info();
    if (GST_VIDEO_INFO_HAS_ALPHA(&videoInfo))
        m_alphaMode = AlphaMode::Premultiplied;

    auto dmabuf = m_videoFrame->getDMABuf();
    ASSERT(dmabuf);
    m_image = dmabuf->createPromiseImage(threadSafeGrContext, kRGBA_8888_SkColorType, toSkiaAlphaType(m_alphaMode), toSkiaOrigin(origin), nullptr, { });
}
#endif
#endif // USE(GBM)

void CoordinatedPlatformLayerBufferVideo::createSkiaImageForMainMemory(std::unique_ptr<GstMappedFrame>&& mappedFrame)
{
    auto imageInfo = SkImageInfo::Make(mappedFrame->width(), mappedFrame->height(), kBGRA_8888_SkColorType, toSkiaAlphaType(m_alphaMode), SkColorSpace::MakeSRGB());
    SkPixmap pixmap(imageInfo, mappedFrame->planeData(0).data(), mappedFrame->planeStride(0));
    m_image = SkImages::RasterFromPixmap(pixmap, [](const void*, void* userData) {
        std::unique_ptr<GstMappedFrame> mappedFrame(static_cast<GstMappedFrame*>(userData));
    }, mappedFrame.release());
}

#if USE(GSTREAMER_GL)
void CoordinatedPlatformLayerBufferVideo::createSkiaImageForGLMemory(GstGLMemory* glMemory, std::unique_ptr<GstMappedFrame>&& mappedFrame, const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext, Origin origin)
{
    mappedFrame->setNeedsCPUSync(m_videoDecoderPlatform != GstVideoDecoderPlatform::OpenMAX);

    auto* videoInfo = mappedFrame->info();
    if (isSinglePlaneGLMemory(glMemory, videoInfo, m_videoDecoderPlatform)) {
        createSkiaImageForSinglePlaneGLMemory(glMemory, WTF::move(mappedFrame), threadSafeGrContext, origin);
        return;
    }

    if (GST_VIDEO_INFO_IS_YUV(videoInfo) && GST_VIDEO_INFO_N_COMPONENTS(videoInfo) >= 3 && GST_VIDEO_INFO_N_PLANES(videoInfo) <= 4)
        createSkiaImageForYUVGLMemory(WTF::move(mappedFrame), threadSafeGrContext, origin);
}

void CoordinatedPlatformLayerBufferVideo::createSkiaImageForSinglePlaneGLMemory(GstGLMemory* glMemory, std::unique_ptr<GstMappedFrame>&& mappedFrame, const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext, Origin origin)
{
    const bool isExternal = gst_gl_memory_get_texture_target(glMemory) == GST_GL_TEXTURE_TARGET_EXTERNAL_OES;
    auto backendFormat = isExternal ? GrBackendFormats::MakeGLExternal() : threadSafeGrContext->defaultBackendFormat(kRGBA_8888_SkColorType, GrRenderable::kYes);
    ASSERT(backendFormat.isValid());

    auto frameSize = SkISize::Make(mappedFrame->width(), mappedFrame->height());
    m_image = SkImages::PromiseTextureFrom(threadSafeGrContext, backendFormat, frameSize, skgpu::Mipmapped::kNo,
        toSkiaOrigin(origin), kRGBA_8888_SkColorType, toSkiaAlphaType(m_alphaMode), SkColorSpace::MakeSRGB(),
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

void CoordinatedPlatformLayerBufferVideo::createSkiaImageForYUVGLMemory(std::unique_ptr<GstMappedFrame>&& mappedFrame, const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext, Origin origin)
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

    GrYUVABackendTextureInfo yuvaBackendTexturesInfo(*info, backendFormats.data(), skgpu::Mipmapped::kNo, toSkiaOrigin(origin));
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
#endif // USE(GSTREAMER_GL)

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && !USE(TEXTURE_MAPPER) && ENABLE(VIDEO) && USE(GSTREAMER)
