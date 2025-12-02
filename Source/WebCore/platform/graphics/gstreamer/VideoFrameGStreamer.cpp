/*
 * Copyright (C) 2022 Igalia S.L
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

#include "VideoFrameGStreamer.h"
#include "VideoFrameContentHint.h"

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "BitmapImage.h"
#include "GLContext.h"
#include "GStreamerCommon.h"
#include "GStreamerVideoFrameConverter.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "ImageGStreamer.h"
#include "ImageOrientation.h"
#include "PixelBuffer.h"
#include "PlatformDisplay.h"
#include "VideoPixelFormat.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/TypedArrayInlines.h>

#if USE(GBM) && GST_CHECK_VERSION(1, 24, 0)
#include <gst/video/video-info-dma.h>
#endif

#if USE(GSTREAMER_GL)
#include <gst/allocators/gstdmabuf.h>
#include <gst/gl/gl.h>
#endif

#if USE(CAIRO)
#include <cairo.h>
#elif USE(SKIA)
#include <skia/core/SkData.h>
#include <skia/core/SkImage.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkPixmap.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif

GST_DEBUG_CATEGORY(webkit_video_frame_debug);
#define GST_CAT_DEFAULT webkit_video_frame_debug

namespace WebCore {

static void ensureVideoFrameDebugCategoryInitialized()
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_video_frame_debug, "webkitvideoframe", 0, "WebKit Video Frame");
    });
}

VideoFrameGStreamer::Info VideoFrameGStreamer::infoFromCaps(const GRefPtr<GstCaps>& caps)
{
    GstVideoInfo videoInfo;
    gst_video_info_from_caps(&videoInfo, caps.get());

    std::optional<DMABufFormat> dmabufFormat;
#if USE(GBM) && GST_CHECK_VERSION(1, 24, 0)
    if (gst_video_is_dma_drm_caps(caps.get())) {
        GstVideoInfoDmaDrm drmVideoInfo;
        if (!gst_video_info_dma_drm_from_caps(&drmVideoInfo, caps.get()))
            return { videoInfo, std::nullopt };

        if (!gst_video_info_dma_drm_to_video_info(&drmVideoInfo, &videoInfo))
            return { videoInfo, std::nullopt };

        dmabufFormat = { drmVideoInfo.drm_fourcc, drmVideoInfo.drm_modifier };
    }
#endif
    return { videoInfo, dmabufFormat };
}

RefPtr<VideoFrame> VideoFrame::createFromPixelBuffer(Ref<PixelBuffer>&& pixelBuffer, PlatformVideoColorSpace&& colorSpace)
{
    return VideoFrameGStreamer::createFromPixelBuffer(WTFMove(pixelBuffer), { }, 1, { }, WTFMove(colorSpace));
}

static RefPtr<ImageGStreamer> convertSampleToImage(const GRefPtr<GstSample>& sample, const GstVideoInfo& videoInfo)
{
    // These caps must match the internal format of a cairo surface with CAIRO_FORMAT_ARGB32,
    // so we don't need to perform color conversions when painting the video frame.
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    auto format = GST_VIDEO_INFO_HAS_ALPHA(&videoInfo) ? "BGRA"_s : "BGRx"_s;
#else
    auto format = GST_VIDEO_INFO_HAS_ALPHA(&videoInfo) ? "ARGB"_s : "xRGB"_s;
#endif
    auto caps = adoptGRef(gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, format.characters(), "framerate", GST_TYPE_FRACTION, GST_VIDEO_INFO_FPS_N(&videoInfo), GST_VIDEO_INFO_FPS_D(&videoInfo), "width", G_TYPE_INT, GST_VIDEO_INFO_WIDTH(&videoInfo), "height", G_TYPE_INT, GST_VIDEO_INFO_HEIGHT(&videoInfo), nullptr));
    auto convertedSample = GStreamerVideoFrameConverter::singleton().convert(sample, caps);
    if (!convertedSample)
        return nullptr;

    return ImageGStreamer::create(WTFMove(convertedSample));
}

RefPtr<VideoFrame> VideoFrame::fromNativeImage(NativeImage& image)
{
    ensureVideoFrameDebugCategoryInitialized();
    GST_TRACE("Creating VideoFrame from native image");

    size_t offsets[GST_VIDEO_MAX_PLANES] = { 0, };
    int strides[GST_VIDEO_MAX_PLANES] = { 0, };

#if USE(CAIRO)
    auto surface = image.platformImage();
    strides[0] = cairo_image_surface_get_stride(surface.get());
    auto width = cairo_image_surface_get_width(surface.get());
    auto height = cairo_image_surface_get_height(surface.get());
    auto size = height * strides[0];
    auto format = G_BYTE_ORDER == G_LITTLE_ENDIAN ? GST_VIDEO_FORMAT_BGRA : GST_VIDEO_FORMAT_ARGB;

    auto buffer = adoptGRef(gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY, cairo_image_surface_get_data(surface.get()), size, 0, size, cairo_surface_reference(surface.get()), reinterpret_cast<GDestroyNotify>(cairo_surface_destroy)));
#elif USE(SKIA)
    auto platformImage = image.platformImage();
    const auto& imageInfo = platformImage->imageInfo();
    strides[0] = imageInfo.minRowBytes();
    auto width = imageInfo.width();
    auto height = imageInfo.height();
    auto size = imageInfo.computeMinByteSize();

    GRefPtr<GstBuffer> buffer;
    if (platformImage->isTextureBacked()) {
        if (!PlatformDisplay::sharedDisplay().skiaGLContext()->makeContextCurrent())
            return nullptr;

        auto data = SkData::MakeUninitialized(size);
        GrDirectContext* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
        if (!platformImage->readPixels(grContext, imageInfo, static_cast<uint8_t*>(data->writable_data()), strides[0], 0, 0))
            return nullptr;

        auto* bytes = data->writable_data();
        buffer = adoptGRef(gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY, bytes, size, 0, size, data.release(), [](gpointer userData) {
            auto data = sk_sp<SkData>(static_cast<SkData*>(userData));
        }));
    } else {
        SkPixmap pixmap;
        if (!platformImage->peekPixels(&pixmap))
            return nullptr;

        buffer = adoptGRef(gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY, pixmap.writable_addr(), size, 0, size, SkRef(platformImage.get()), [](gpointer userData) {
            SkSafeUnref(static_cast<SkImage*>(userData));
        }));
    }

    GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;
    switch (imageInfo.colorType()) {
    case kBGRA_8888_SkColorType:
        format = imageInfo.alphaType() == kOpaque_SkAlphaType ? GST_VIDEO_FORMAT_BGRx : GST_VIDEO_FORMAT_BGRA;
        break;
    case kRGB_888x_SkColorType:
        format = GST_VIDEO_FORMAT_RGBx;
        break;
    case kRGBA_8888_SkColorType:
        format = GST_VIDEO_FORMAT_RGBA;
        break;
    default:
        return nullptr;
    }
#endif

    gst_buffer_add_video_meta_full(buffer.get(), GST_VIDEO_FRAME_FLAG_NONE, format, width, height, 1, offsets, strides);

    auto caps = adoptGRef(gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, gst_video_format_to_string(format), "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, nullptr));
    auto info = VideoFrameGStreamer::infoFromCaps(caps);
    auto sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
    return VideoFrameGStreamer::create(WTFMove(sample), { { width, height }, WTFMove(info) });
}

static void copyToGstBufferPlane(std::span<uint8_t> destination, const GstVideoInfo& info, size_t planeIndex, std::span<const uint8_t> source, size_t height, uint32_t bytesPerRowSource)
{
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN; // GLib port
    auto destinationOffset = GST_VIDEO_INFO_PLANE_OFFSET(&info, planeIndex);
    uint32_t bytesPerRowDestination = GST_VIDEO_INFO_PLANE_STRIDE(&info, planeIndex);
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END; // GLib port
    gsize sourceOffset = 0;
    auto count = std::min(bytesPerRowSource, bytesPerRowDestination);
    for (size_t i = 0; i < height; ++i) {
        auto sourceSpan = source.subspan(sourceOffset, count);
        auto destinationSpan = destination.subspan(destinationOffset, count);
        memcpySpan(destinationSpan, sourceSpan);
        sourceOffset += bytesPerRowSource;
        destinationOffset += bytesPerRowDestination;
    }
}

RefPtr<VideoFrame> VideoFrame::createNV12(std::span<const uint8_t> span, size_t width, size_t height, const ComputedPlaneLayout& planeY, const ComputedPlaneLayout& planeUV, PlatformVideoColorSpace&& colorSpace)
{
    ensureVideoFrameDebugCategoryInitialized();
    GST_TRACE("Creating VideoFrame from NV12 raw buffer");

    GstVideoInfo info;
    gst_video_info_set_format(&info, GST_VIDEO_FORMAT_NV12, width, height);
    fillVideoInfoColorimetryFromColorSpace(&info, colorSpace);

    auto buffer = adoptGRef(gst_buffer_new_allocate(nullptr, GST_VIDEO_INFO_SIZE(&info), nullptr));
    {
        GstMappedBuffer mappedBuffer(buffer, GST_MAP_WRITE);
        auto destinationSpan = mappedBuffer.mutableSpan<uint8_t>();
        copyToGstBufferPlane(destinationSpan, info, 0, span, height, planeY.sourceWidthBytes);
        copyToGstBufferPlane(destinationSpan, info, 1, span.subspan(planeUV.destinationOffset), height / 2, planeUV.sourceWidthBytes);
    }
    gst_buffer_add_video_meta(buffer.get(), GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_FORMAT_NV12, width, height);

    auto caps = adoptGRef(gst_video_info_to_caps(&info));
    auto sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
    return VideoFrameGStreamer::create(WTFMove(sample), { { static_cast<int>(width), static_cast<int>(height) }, { { info } } }, WTFMove(colorSpace));
}

#define CREATE_RGBA_FRAME(format)                                                                   \
    GstVideoInfo info;                                                                              \
    gst_video_info_set_format(&info, format, width, height);                                        \
    fillVideoInfoColorimetryFromColorSpace(&info, colorSpace);                                      \
    auto buffer = adoptGRef(gst_buffer_new_allocate(nullptr, GST_VIDEO_INFO_SIZE(&info), nullptr)); \
    gst_buffer_fill(buffer.get(), plane.destinationOffset, span.data(), span.size_bytes());         \
    gst_buffer_add_video_meta(buffer.get(), GST_VIDEO_FRAME_FLAG_NONE, format, width, height);      \
    auto caps = adoptGRef(gst_video_info_to_caps(&info));                                           \
    auto sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));            \
    return VideoFrameGStreamer::create(WTFMove(sample), { { static_cast<int>(width), static_cast<int>(height) }, { { info } } }, WTFMove(colorSpace))

RefPtr<VideoFrame> VideoFrame::createRGBA(std::span<const uint8_t> span, size_t width, size_t height, const ComputedPlaneLayout& plane, PlatformVideoColorSpace&& colorSpace)
{
    ensureVideoFrameDebugCategoryInitialized();
    GST_TRACE("Creating VideoFrame from RGBA raw buffer");
    CREATE_RGBA_FRAME(GST_VIDEO_FORMAT_RGBA);
}

RefPtr<VideoFrame> VideoFrame::createBGRA(std::span<const uint8_t> span, size_t width, size_t height, const ComputedPlaneLayout& plane, PlatformVideoColorSpace&& colorSpace)
{
    ensureVideoFrameDebugCategoryInitialized();
    GST_TRACE("Creating VideoFrame from BGRA raw buffer");
    CREATE_RGBA_FRAME(GST_VIDEO_FORMAT_BGRA);
}

#undef CREATE_RGBA_FRAME

RefPtr<VideoFrame> VideoFrame::createI420(std::span<const uint8_t> span, size_t width, size_t height, const ComputedPlaneLayout& planeY, const ComputedPlaneLayout& planeU, const ComputedPlaneLayout& planeV, PlatformVideoColorSpace&& colorSpace)
{
    ensureVideoFrameDebugCategoryInitialized();
    GST_TRACE("Creating VideoFrame from I420 raw buffer");
    GstVideoInfo info;
    gst_video_info_set_format(&info, GST_VIDEO_FORMAT_I420, width, height);
    fillVideoInfoColorimetryFromColorSpace(&info, colorSpace);

    auto buffer = adoptGRef(gst_buffer_new_allocate(nullptr, GST_VIDEO_INFO_SIZE(&info), nullptr));
    gst_buffer_memset(buffer.get(), 0, 0, span.size_bytes());
    {
        GstMappedBuffer mappedBuffer(buffer, GST_MAP_WRITE);
        auto destinationSpan = mappedBuffer.mutableSpan<uint8_t>();
        auto stride = ((height + 1) / 2);
        size_t offsetLayoutU = planeY.sourceLeftBytes + planeY.sourceWidthBytes * height;
        size_t offsetLayoutV = offsetLayoutU + planeU.sourceLeftBytes + planeU.sourceWidthBytes * stride;

        copyToGstBufferPlane(destinationSpan, info, GST_VIDEO_COMP_Y, span, height, planeY.sourceWidthBytes);
        copyToGstBufferPlane(destinationSpan, info, GST_VIDEO_COMP_U, span.subspan(offsetLayoutU), stride, planeU.sourceWidthBytes);
        copyToGstBufferPlane(destinationSpan, info, GST_VIDEO_COMP_V, span.subspan(offsetLayoutV), stride, planeV.sourceWidthBytes);
    }
    gst_buffer_add_video_meta(buffer.get(), GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_FORMAT_I420, width, height);

    auto caps = adoptGRef(gst_video_info_to_caps(&info));
    auto sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
    return VideoFrameGStreamer::create(WTFMove(sample), { { static_cast<int>(width), static_cast<int>(height) }, { { info } } }, WTFMove(colorSpace));
}

RefPtr<VideoFrame> VideoFrame::createI420A(std::span<const uint8_t> span, size_t width, size_t height, const ComputedPlaneLayout& planeY, const ComputedPlaneLayout& planeU, const ComputedPlaneLayout& planeV, const ComputedPlaneLayout& planeA, PlatformVideoColorSpace&& colorSpace)
{
    GstVideoInfo info;
    gst_video_info_set_format(&info, GST_VIDEO_FORMAT_A420, width, height);
    fillVideoInfoColorimetryFromColorSpace(&info, colorSpace);

    auto buffer = adoptGRef(gst_buffer_new_allocate(nullptr, GST_VIDEO_INFO_SIZE(&info), nullptr));
    gst_buffer_memset(buffer.get(), 0, 0, span.size_bytes());
    {
        GstMappedBuffer mappedBuffer(buffer, GST_MAP_WRITE);
        auto destinationSpan = mappedBuffer.mutableSpan<uint8_t>();
        auto stride = ((height + 1) / 2);
        size_t offsetLayoutU = planeY.sourceLeftBytes + planeY.sourceWidthBytes * height;
        size_t offsetLayoutV = offsetLayoutU + planeU.sourceLeftBytes + planeU.sourceWidthBytes * stride;
        size_t offsetLayoutA = offsetLayoutV + planeV.sourceLeftBytes + planeV.sourceWidthBytes * stride;

        copyToGstBufferPlane(destinationSpan, info, GST_VIDEO_COMP_Y, span, height, planeY.sourceWidthBytes);
        copyToGstBufferPlane(destinationSpan, info, GST_VIDEO_COMP_U, span.subspan(offsetLayoutU), stride, planeU.sourceWidthBytes);
        copyToGstBufferPlane(destinationSpan, info, GST_VIDEO_COMP_V, span.subspan(offsetLayoutV), stride, planeV.sourceWidthBytes);
        copyToGstBufferPlane(destinationSpan, info, GST_VIDEO_COMP_A, span.subspan(offsetLayoutA), height, planeA.sourceWidthBytes);
    }
    gst_buffer_add_video_meta(buffer.get(), GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_FORMAT_A420, width, height);

    auto caps = adoptGRef(gst_video_info_to_caps(&info));
    auto sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
    return VideoFrameGStreamer::create(WTFMove(sample), { { static_cast<int>(width), static_cast<int>(height) }, { { info } } }, WTFMove(colorSpace));
}

static inline void setBufferFields(GstBuffer* buffer, const MediaTime& presentationTime, double frameRate)
{
    GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_LIVE);
    GST_BUFFER_DTS(buffer) = GST_BUFFER_PTS(buffer) = toGstClockTime(presentationTime);
    GST_BUFFER_DURATION(buffer) = toGstClockTime(1_s / frameRate);
}

static MediaTime presentationTimeFromSample(const GRefPtr<GstSample>& sample)
{
    auto buffer = gst_sample_get_buffer(sample.get());
    if (GST_BUFFER_PTS_IS_VALID(buffer))
        return fromGstClockTime(GST_BUFFER_PTS(buffer));

    return MediaTime::invalidTime();
}

Ref<VideoFrameGStreamer> VideoFrameGStreamer::create(GRefPtr<GstSample>&& sample, const CreateOptions& options, PlatformVideoColorSpace&& colorSpace)
{
    CreateOptions newOptions = options;
    auto caps = gst_sample_get_caps(sample.get());
    if (!colorSpace.primaries && doCapsHaveType(caps, GST_VIDEO_CAPS_TYPE_PREFIX))
        colorSpace = videoColorSpaceFromCaps(caps);

    if (options.presentationTime.isInvalid())
        newOptions.presentationTime = presentationTimeFromSample(sample);

    return adoptRef(*new VideoFrameGStreamer(WTFMove(sample), newOptions, WTFMove(colorSpace)));
}

Ref<VideoFrameGStreamer> VideoFrameGStreamer::createWrappedSample(const GRefPtr<GstSample>& sample, const MediaTime& presentationTime, Rotation videoRotation)
{
    auto* caps = gst_sample_get_caps(sample.get());
    auto size = getVideoResolutionFromCaps(caps);
    RELEASE_ASSERT(size);
    CreateOptions options({ static_cast<int>(size->width()), static_cast<int>(size->height()) }, infoFromCaps(GRefPtr(caps)));
    options.presentationTime = presentationTime;
    if (options.presentationTime.isInvalid())
        options.presentationTime = presentationTimeFromSample(sample);

    options.rotation = videoRotation;
    return adoptRef(*new VideoFrameGStreamer(sample, options, videoColorSpaceFromCaps(caps)));
}

RefPtr<VideoFrameGStreamer> VideoFrameGStreamer::createFromPixelBuffer(Ref<PixelBuffer>&& pixelBuffer, const IntSize& destinationSize, double frameRate, const CreateOptions& options, PlatformVideoColorSpace&& colorSpace)
{
    ensureGStreamerInitialized();

    ensureVideoFrameDebugCategoryInitialized();
    auto size = pixelBuffer->size();

    GstVideoFormat format;
    switch (pixelBuffer->format().pixelFormat) {
    case PixelFormat::RGBA8:
        format = GST_VIDEO_FORMAT_RGBA;
        break;
    case PixelFormat::BGRX8:
        format = GST_VIDEO_FORMAT_BGRx;
        break;
    case PixelFormat::BGRA8:
        format = GST_VIDEO_FORMAT_BGRA;
        break;
    };

    auto sizeInBytes = pixelBuffer->bytes().size();
    auto dataBaseAddress = pixelBuffer->bytes().data();
    auto leakedPixelBuffer = &pixelBuffer.leakRef();

    auto buffer = adoptGRef(gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY, dataBaseAddress, sizeInBytes, 0, sizeInBytes, leakedPixelBuffer, [](gpointer userData) {
        static_cast<PixelBuffer*>(userData)->deref();
    }));

    auto width = size.width();
    auto height = size.height();

    auto formatName = unsafeSpan(gst_video_format_to_string(format));
    GST_TRACE("Creating %s VideoFrame from pixel buffer", formatName.data());

    int frameRateNumerator, frameRateDenominator;
    gst_util_double_to_fraction(frameRate, &frameRateNumerator, &frameRateDenominator);

    auto caps = adoptGRef(gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, formatName.data(), "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, nullptr));
    if (frameRate)
        gst_caps_set_simple(caps.get(), "framerate", GST_TYPE_FRACTION, frameRateNumerator, frameRateDenominator, nullptr);

    GRefPtr<GstSample> sample;
    Info info;

    // Optionally resize the video frame to fit destinationSize. This code path is used mostly by
    // the mock realtime video source when the gUM constraints specifically required exact width
    // and/or height values.
    if (!destinationSize.isZero() && size != destinationSize) {
        GstVideoInfo inputInfo;
        gst_video_info_from_caps(&inputInfo, caps.get());

        width = destinationSize.width();
        height = destinationSize.height();
        GST_TRACE("Resizing frame from %dx%d to %dx%d", size.width(), size.height(), width, height);
        auto outputCaps = adoptGRef(gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, formatName.data(), "width", G_TYPE_INT, width,
            "height", G_TYPE_INT, height, nullptr));
        if (frameRate)
            gst_caps_set_simple(outputCaps.get(), "framerate", GST_TYPE_FRACTION, frameRateNumerator, frameRateDenominator, nullptr);

        auto inputSample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
        sample = GStreamerVideoFrameConverter::singleton().convert(inputSample, outputCaps);
        if (!sample)
            return nullptr;

        info = infoFromCaps(outputCaps);
        GRefPtr buffer = gst_sample_get_buffer(sample.get());
        auto outputBuffer = webkitGstBufferSetVideoFrameMetadata(WTFMove(buffer), options.timeMetadata, options.rotation, options.isMirrored, options.contentHint);
        gst_buffer_add_video_meta(outputBuffer.get(), GST_VIDEO_FRAME_FLAG_NONE, format, width, height);
        setBufferFields(outputBuffer.get(), options.presentationTime, frameRate);
        sample = adoptGRef(gst_sample_make_writable(sample.leakRef()));
        gst_sample_set_buffer(sample.get(), outputBuffer.get());
    } else {
        auto outputBuffer = webkitGstBufferSetVideoFrameMetadata(WTFMove(buffer), options.timeMetadata, options.rotation, options.isMirrored, options.contentHint);
        gst_buffer_add_video_meta(outputBuffer.get(), GST_VIDEO_FRAME_FLAG_NONE, format, width, height);
        setBufferFields(outputBuffer.get(), options.presentationTime, frameRate);
        sample = adoptGRef(gst_sample_new(outputBuffer.get(), caps.get(), nullptr, nullptr));
        info = infoFromCaps(caps);
    }

    CreateOptions newOptions = options;
    newOptions.info = WTFMove(info);
    newOptions.presentationSize = IntSize(width, height);
    return adoptRef(*new VideoFrameGStreamer(WTFMove(sample), newOptions, WTFMove(colorSpace)));
}

VideoFrameGStreamer::VideoFrameGStreamer(GRefPtr<GstSample>&& sample, const CreateOptions& options, PlatformVideoColorSpace&& colorSpace)
    : VideoFrame(options.presentationTime, options.isMirrored, options.rotation, WTFMove(colorSpace))
    , m_sample(WTFMove(sample))
    , m_presentationSize(options.presentationSize)
{
    ensureVideoFrameDebugCategoryInitialized();
    ASSERT(m_sample);

    m_info = options.info.value_or(infoFromCaps(GRefPtr(gst_sample_get_caps(m_sample.get()))));

    setMemoryTypeFromCaps();

    setMetadataAndContentHint(options.timeMetadata, options.contentHint);
}

VideoFrameGStreamer::VideoFrameGStreamer(const GRefPtr<GstSample>& sample, const CreateOptions& options, PlatformVideoColorSpace&& colorSpace)
    : VideoFrame(options.presentationTime, false, options.rotation, WTFMove(colorSpace))
    , m_sample(sample)
    , m_presentationSize(options.presentationSize)
{
    ensureVideoFrameDebugCategoryInitialized();
    setMemoryTypeFromCaps();

    m_info = options.info.value_or(infoFromCaps(GRefPtr(gst_sample_get_caps(m_sample.get()))));

    auto buffer = gst_sample_get_buffer(sample.get());
    auto [videoRotationFromMeta, isMirrored] = webkitGstBufferGetVideoRotation(buffer);
    initializeCharacteristics(options.presentationTime, isMirrored, videoRotationFromMeta);
}

VideoFrameGStreamer::~VideoFrameGStreamer() = default;

void VideoFrameGStreamer::setFrameRate(double frameRate)
{
    auto caps = adoptGRef(gst_caps_copy(gst_sample_get_caps(m_sample.get())));
    int frameRateNumerator, frameRateDenominator;
    gst_util_double_to_fraction(frameRate, &frameRateNumerator, &frameRateDenominator);
    gst_caps_set_simple(caps.get(), "framerate", GST_TYPE_FRACTION, frameRateNumerator, frameRateDenominator, nullptr);

    auto buffer = gst_sample_get_buffer(m_sample.get());
    GST_BUFFER_DURATION(buffer) = toGstClockTime(1_s / frameRate);

    m_sample = adoptGRef(gst_sample_make_writable(m_sample.leakRef()));
    gst_sample_set_caps(m_sample.get(), caps.get());
}

void VideoFrameGStreamer::setMaxFrameRate(double maxFrameRate)
{
    auto caps = adoptGRef(gst_caps_copy(gst_sample_get_caps(m_sample.get())));
    int frameRateNumerator, frameRateDenominator;
    gst_util_double_to_fraction(maxFrameRate, &frameRateNumerator, &frameRateDenominator);
    gst_caps_set_simple(caps.get(), "framerate", GST_TYPE_FRACTION, 0, 1, "max-framerate", GST_TYPE_FRACTION, frameRateNumerator, frameRateDenominator, nullptr);
    m_sample = adoptGRef(gst_sample_make_writable(m_sample.leakRef()));
    gst_sample_set_caps(m_sample.get(), caps.get());
}

void VideoFrameGStreamer::setPresentationTime(const MediaTime& presentationTime)
{
    updateTimestamp(presentationTime, VideoFrame::ShouldCloneWithDifferentTimestamp::No);
    auto buffer = gst_sample_get_buffer(m_sample.get());
    GST_BUFFER_PTS(buffer) = GST_BUFFER_DTS(buffer) = toGstClockTime(1_s / presentationTime.toDouble());
}

void VideoFrameGStreamer::setMetadataAndContentHint(std::optional<VideoFrameTimeMetadata> metadata, VideoFrameContentHint hint)
{
    GRefPtr buffer = gst_sample_get_buffer(m_sample.get());
    RELEASE_ASSERT(buffer);
    auto modifiedBuffer = webkitGstBufferSetVideoFrameMetadata(WTFMove(buffer), metadata, rotation(), isMirrored(), hint);
    m_sample = adoptGRef(gst_sample_make_writable(m_sample.leakRef()));
    gst_sample_set_buffer(m_sample.get(), modifiedBuffer.get());
}

static void copyPlane(std::span<uint8_t>& destination, const std::span<uint8_t>& source, uint64_t sourceStride, const ComputedPlaneLayout& spanPlaneLayout)
{
    uint64_t sourceOffset = spanPlaneLayout.sourceTop * sourceStride;
    sourceOffset += spanPlaneLayout.sourceLeftBytes;
    uint64_t destinationOffset = spanPlaneLayout.destinationOffset;
    uint64_t rowBytes = spanPlaneLayout.sourceWidthBytes;
    for (size_t rowIndex = 0; rowIndex < spanPlaneLayout.sourceHeight; ++rowIndex) {
        memcpySpan(destination.subspan(destinationOffset, rowBytes), source.subspan(sourceOffset, rowBytes));
        sourceOffset += sourceStride;
        destinationOffset += spanPlaneLayout.destinationStride;
    }
}

void VideoFrame::copyTo(std::span<uint8_t> destination, VideoPixelFormat pixelFormat, Vector<ComputedPlaneLayout>&& computedPlaneLayout, CompletionHandler<void(std::optional<Vector<PlaneLayout>>&&)>&& callback)
{
    ensureVideoFrameDebugCategoryInitialized();
    GstVideoInfo inputInfo;
    auto sample = downcast<VideoFrameGStreamer>(*this).sample();
    auto* inputBuffer = gst_sample_get_buffer(sample);
    auto* inputCaps = gst_sample_get_caps(sample);
    gst_video_info_from_caps(&inputInfo, inputCaps);
    GstMappedFrame inputFrame(inputBuffer, &inputInfo, GST_MAP_READ);
    if (!inputFrame) {
        GST_WARNING("could not map the input frame");
        ASSERT_NOT_REACHED_WITH_MESSAGE("could not map the input frame");
        callback({ });
        return;
    }

#ifndef GST_DISABLE_GST_DEBUG
    GST_TRACE("Copying frame data to %s pixel format", convertVideoPixelFormatToString(pixelFormat).ascii().data());
#endif
    if (pixelFormat == VideoPixelFormat::NV12) {
        auto spanPlaneLayoutY = computedPlaneLayout[GST_VIDEO_COMP_Y];
        auto widthY = inputFrame.componentWidth(GST_VIDEO_COMP_Y);
        PlaneLayout planeLayoutY { spanPlaneLayoutY.destinationOffset, spanPlaneLayoutY.destinationStride ? spanPlaneLayoutY.destinationStride : widthY };

        auto spanPlaneLayoutUV = computedPlaneLayout[GST_VIDEO_COMP_U];
        auto widthUV = inputFrame.componentWidth(GST_VIDEO_COMP_U);
        PlaneLayout planeLayoutUV { spanPlaneLayoutUV.destinationOffset, spanPlaneLayoutUV.destinationStride ? spanPlaneLayoutUV.destinationStride : widthUV };

        auto planeY = inputFrame.planeData(GST_VIDEO_COMP_Y);
        auto bytesPerRowY = inputFrame.componentStride(GST_VIDEO_COMP_Y);
        copyPlane(destination, planeY, bytesPerRowY, spanPlaneLayoutY);

        auto planeUV = inputFrame.planeData(GST_VIDEO_COMP_U);
        auto bytesPerRowUV = inputFrame.componentStride(GST_VIDEO_COMP_U);
        copyPlane(destination, planeUV, bytesPerRowUV, spanPlaneLayoutUV);

        Vector<PlaneLayout> planeLayouts;
        planeLayouts.append(planeLayoutY);
        planeLayouts.append(planeLayoutUV);
        callback(WTFMove(planeLayouts));
        return;
    }

    if (pixelFormat == VideoPixelFormat::I420 || pixelFormat == VideoPixelFormat::I420A) {
        auto spanPlaneLayoutY = computedPlaneLayout[GST_VIDEO_COMP_Y];
        auto widthY = inputFrame.componentWidth(GST_VIDEO_COMP_Y);
        PlaneLayout planeLayoutY { spanPlaneLayoutY.destinationOffset, spanPlaneLayoutY.destinationStride ? spanPlaneLayoutY.destinationStride : widthY };
        auto planeY = inputFrame.planeData(GST_VIDEO_COMP_Y);
        auto bytesPerRowY = inputFrame.planeStride(GST_VIDEO_COMP_Y);
        copyPlane(destination, planeY, bytesPerRowY, spanPlaneLayoutY);

        auto spanPlaneLayoutU = computedPlaneLayout[GST_VIDEO_COMP_U];
        auto widthUV = inputFrame.componentWidth(GST_VIDEO_COMP_U);
        PlaneLayout planeLayoutU { spanPlaneLayoutU.destinationOffset, spanPlaneLayoutU.destinationStride ? spanPlaneLayoutU.destinationStride : widthUV / 2 };

        auto spanPlaneLayoutV = computedPlaneLayout[GST_VIDEO_COMP_V];
        PlaneLayout planeLayoutV { spanPlaneLayoutV.destinationOffset, spanPlaneLayoutV.destinationStride ? spanPlaneLayoutV.destinationStride : widthUV / 2 };

        auto planeU = inputFrame.planeData(GST_VIDEO_COMP_U);
        auto bytesPerRowU = inputFrame.planeStride(GST_VIDEO_COMP_U);
        copyPlane(destination, planeU, bytesPerRowU, spanPlaneLayoutU);

        auto planeV = inputFrame.planeData(GST_VIDEO_COMP_V);
        auto bytesPerRowV = inputFrame.planeStride(GST_VIDEO_COMP_V);
        copyPlane(destination, planeV, bytesPerRowV, spanPlaneLayoutV);

        Vector<PlaneLayout> planeLayouts;
        planeLayouts.append(planeLayoutY);
        planeLayouts.append(planeLayoutU);
        planeLayouts.append(planeLayoutV);

        if (pixelFormat == VideoPixelFormat::I420A) {
            auto spanPlaneLayoutA = computedPlaneLayout[GST_VIDEO_COMP_A];
            auto widthA = inputFrame.componentWidth(GST_VIDEO_COMP_A);
            PlaneLayout planeLayoutA { spanPlaneLayoutA.destinationOffset, spanPlaneLayoutA.destinationStride ? spanPlaneLayoutA.destinationStride : widthA };
            auto planeA = inputFrame.planeData(GST_VIDEO_COMP_A);
            auto bytesPerRowA = inputFrame.componentStride(GST_VIDEO_COMP_A);
            copyPlane(destination, planeA, bytesPerRowA, spanPlaneLayoutA);
            planeLayouts.append(planeLayoutA);
        }

        callback(WTFMove(planeLayouts));
        return;
    }

    if (pixelFormat == VideoPixelFormat::RGBA || pixelFormat == VideoPixelFormat::BGRA) {
        ComputedPlaneLayout planeLayout;
        if (!computedPlaneLayout.isEmpty())
            planeLayout = computedPlaneLayout[0];
        GstMappedBuffer mappedBuffer(inputBuffer, GST_MAP_READ);
        auto plane = mappedBuffer.mutableSpan<uint8_t>();
        auto bytesPerRow = inputFrame.componentStride(0);
        copyPlane(destination, plane, bytesPerRow, planeLayout);
        Vector<PlaneLayout> planeLayouts;
        planeLayouts.append({ planeLayout.destinationOffset, planeLayout.destinationStride ? planeLayout.destinationStride : 4 * inputFrame.width() });
        callback(WTFMove(planeLayouts));
        return;
    }

    // FIXME: Handle I422, I444, RGBX and BGRX formats.
    callback({ });
}

RefPtr<NativeImage> VideoFrameGStreamer::copyNativeImage() const
{
    auto& gstFrame = downcast<VideoFrameGStreamer>(*this);
    auto image = convertSampleToImage(gstFrame.sample(), gstFrame.info());
    if (!image)
        return nullptr;
    return NativeImage::create(image->image());
}

GRefPtr<GstSample> VideoFrameGStreamer::resizedSample(const IntSize& destinationSize)
{
    return convert(static_cast<GstVideoFormat>(pixelFormat()), destinationSize);
}

GRefPtr<GstSample> VideoFrameGStreamer::convert(GstVideoFormat format, const IntSize& destinationSize)
{
    auto* caps = gst_sample_get_caps(m_sample.get());
    const auto* structure = gst_caps_get_structure(caps, 0);
    int frameRateNumerator, frameRateDenominator;
    if (!gst_structure_get_fraction(structure, "framerate", &frameRateNumerator, &frameRateDenominator)) {
        frameRateNumerator = 1;
        frameRateDenominator = 1;
    }

    auto width = destinationSize.width();
    auto height = destinationSize.height();
    auto formatName = unsafeSpan(gst_video_format_to_string(format));
    auto outputCaps = adoptGRef(gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, formatName.data(), "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, "framerate", GST_TYPE_FRACTION, frameRateNumerator, frameRateDenominator, nullptr));

    if (gst_caps_is_equal(caps, outputCaps.get()))
        return GRefPtr<GstSample>(m_sample);

    return GStreamerVideoFrameConverter::singleton().convert(m_sample, outputCaps);
}

GRefPtr<GstSample> VideoFrameGStreamer::downloadSample(std::optional<GstVideoFormat> destinationFormat)
{
    return convert(destinationFormat.value_or(static_cast<GstVideoFormat>(pixelFormat())), presentationSize());
}

RefPtr<VideoFrameGStreamer> VideoFrameGStreamer::resizeTo(const IntSize& destinationSize)
{
    CreateOptions options { IntSize(destinationSize) };
    options.presentationTime = presentationTime();
    options.rotation = rotation();
    options.isMirrored = isMirrored();
    auto colorSpace = this->colorSpace();
    return VideoFrameGStreamer::create(resizedSample(destinationSize), options, WTFMove(colorSpace));
}

RefPtr<ImageGStreamer> VideoFrameGStreamer::convertToImage()
{
    return convertSampleToImage(m_sample, m_info.info);
}

Ref<VideoFrame> VideoFrameGStreamer::clone()
{
    return createWrappedSample(m_sample, presentationTime(), rotation());
}

void VideoFrameGStreamer::setMemoryTypeFromCaps()
{
    auto features = gst_caps_get_features(gst_sample_get_caps(m_sample.get()), 0);
    if (!features) {
        m_memoryType = MemoryType::System;
        return;
    }

#if USE(GSTREAMER_GL)
#if USE(GBM)
    if (gst_caps_features_contains(features, GST_CAPS_FEATURE_MEMORY_DMABUF)) {
        m_memoryType = MemoryType::DMABuf;
        return;
    }
#endif // USE(GBM)
    if (gst_caps_features_contains(features, GST_CAPS_FEATURE_MEMORY_GL_MEMORY)) {
        m_memoryType = MemoryType::GL;
        return;
    }
#else
    m_memoryType = MemoryType::Unsupported;
#endif // USE(GSTREAMER_GL)
}

VideoFrameContentHint VideoFrameGStreamer::contentHint() const
{
    auto buffer = gst_sample_get_buffer(m_sample.get());
    return webkitGstBufferGetContentHint(buffer);
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
