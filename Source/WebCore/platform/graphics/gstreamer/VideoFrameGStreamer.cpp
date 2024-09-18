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

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "BitmapImage.h"
#include "GLContext.h"
#include "GStreamerCommon.h"
#include "GraphicsContext.h"
#include "ImageGStreamer.h"
#include "ImageOrientation.h"
#include "PixelBuffer.h"
#include "PlatformDisplay.h"
#include "VideoPixelFormat.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/TypedArrayInlines.h>

#if USE(CAIRO)
#include <cairo.h>
#elif USE(SKIA)
#include <skia/core/SkData.h>
#include <skia/core/SkImage.h>

IGNORE_CLANG_WARNINGS_BEGIN("cast-align")
#include <skia/core/SkPixmap.h>
IGNORE_CLANG_WARNINGS_END
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

RefPtr<VideoFrame> VideoFrame::createFromPixelBuffer(Ref<PixelBuffer>&& pixelBuffer, PlatformVideoColorSpace&& colorSpace)
{
    return VideoFrameGStreamer::createFromPixelBuffer(WTFMove(pixelBuffer), VideoFrameGStreamer::CanvasContentType::Canvas2D, VideoFrame::Rotation::None, MediaTime::invalidTime(), { }, 1, false, { }, WTFMove(colorSpace));
}

static RefPtr<ImageGStreamer> convertSampleToImage(const GRefPtr<GstSample>& sample)
{
    GstVideoInfo videoInfo;
    if (!gst_video_info_from_caps(&videoInfo, gst_sample_get_caps(sample.get())))
        return nullptr;

    // These caps must match the internal format of a cairo surface with CAIRO_FORMAT_ARGB32,
    // so we don't need to perform color conversions when painting the video frame.
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    auto format = GST_VIDEO_INFO_HAS_ALPHA(&videoInfo) ? "BGRA"_s : "BGRx"_s;
#else
    auto format = GST_VIDEO_INFO_HAS_ALPHA(&videoInfo) ? "ARGB"_s : "xRGB"_s;
#endif
    auto caps = adoptGRef(gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, format.characters(), "framerate", GST_TYPE_FRACTION, GST_VIDEO_INFO_FPS_N(&videoInfo), GST_VIDEO_INFO_FPS_D(&videoInfo), "width", G_TYPE_INT, GST_VIDEO_INFO_WIDTH(&videoInfo), "height", G_TYPE_INT, GST_VIDEO_INFO_HEIGHT(&videoInfo), nullptr));
    auto convertedSample = adoptGRef(gst_video_convert_sample(sample.get(), caps.get(), GST_CLOCK_TIME_NONE, nullptr));
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
    auto sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
    FloatSize presentationSize { static_cast<float>(width), static_cast<float>(height) };
    return VideoFrameGStreamer::create(WTFMove(sample), presentationSize);
}

static void copyToGstBufferPlane(uint8_t* destination, const GstVideoInfo& info, size_t planeIndex, const uint8_t* source, size_t height, uint32_t bytesPerRowSource)
{
    destination += GST_VIDEO_INFO_PLANE_OFFSET(&info, planeIndex);
    uint32_t bytesPerRowDestination = GST_VIDEO_INFO_PLANE_STRIDE(&info, planeIndex);
    for (size_t i = 0; i < height; ++i) {
        std::memcpy(destination, source, std::min(bytesPerRowSource, bytesPerRowDestination));
        source += bytesPerRowSource;
        destination += bytesPerRowDestination;
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
    GstMappedBuffer mappedBuffer(buffer, GST_MAP_WRITE);
    copyToGstBufferPlane(mappedBuffer.data(), info, 0, span.data(), height, planeY.sourceWidthBytes);
    copyToGstBufferPlane(mappedBuffer.data(), info, 1, span.data() + planeUV.destinationOffset, height / 2, planeUV.sourceWidthBytes);
    gst_buffer_add_video_meta(buffer.get(), GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_FORMAT_NV12, width, height);

    auto caps = adoptGRef(gst_video_info_to_caps(&info));
    auto sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
    FloatSize presentationSize { static_cast<float>(width), static_cast<float>(height) };
    return VideoFrameGStreamer::create(WTFMove(sample), presentationSize);
}

#define CREATE_RGBA_FRAME(format)                                       \
    GstVideoInfo info;                                                  \
    gst_video_info_set_format(&info, format, width, height);            \
    fillVideoInfoColorimetryFromColorSpace(&info, colorSpace);          \
    auto buffer = adoptGRef(gst_buffer_new_allocate(nullptr, GST_VIDEO_INFO_SIZE(&info), nullptr)); \
    gst_buffer_fill(buffer.get(), plane.destinationOffset, span.data(), span.size_bytes()); \
    gst_buffer_add_video_meta(buffer.get(), GST_VIDEO_FRAME_FLAG_NONE, format, width, height); \
    auto caps = adoptGRef(gst_video_info_to_caps(&info));               \
    auto sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr)); \
    FloatSize presentationSize { static_cast<float>(width), static_cast<float>(height) }; \
    return VideoFrameGStreamer::create(WTFMove(sample), presentationSize)

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
    GstMappedBuffer mappedBuffer(buffer, GST_MAP_WRITE);
    copyToGstBufferPlane(mappedBuffer.data(), info, 0, span.data(), height, planeY.sourceWidthBytes);
    copyToGstBufferPlane(mappedBuffer.data(), info, 1, span.data() + planeU.destinationOffset, height / 2, planeU.sourceWidthBytes);
    copyToGstBufferPlane(mappedBuffer.data(), info, 2, span.data() + planeV.destinationOffset, height / 2, planeV.sourceWidthBytes);
    gst_buffer_add_video_meta(buffer.get(), GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_FORMAT_I420, width, height);

    auto caps = adoptGRef(gst_video_info_to_caps(&info));
    auto sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
    FloatSize presentationSize { static_cast<float>(width), static_cast<float>(height) };
    return VideoFrameGStreamer::create(WTFMove(sample), presentationSize, MediaTime::invalidTime(), Rotation::None, false, std::nullopt, WTFMove(colorSpace));
}

RefPtr<VideoFrame> VideoFrame::createI420A(std::span<const uint8_t> span, size_t width, size_t height, const ComputedPlaneLayout& planeY, const ComputedPlaneLayout& planeU, const ComputedPlaneLayout& planeV, const ComputedPlaneLayout& planeA, PlatformVideoColorSpace&& colorSpace)
{
    GstVideoInfo info;
    gst_video_info_set_format(&info, GST_VIDEO_FORMAT_A420, width, height);
    fillVideoInfoColorimetryFromColorSpace(&info, colorSpace);

    auto buffer = adoptGRef(gst_buffer_new_allocate(nullptr, GST_VIDEO_INFO_SIZE(&info), nullptr));
    gst_buffer_memset(buffer.get(), 0, 0, span.size_bytes());
    GstMappedBuffer mappedBuffer(buffer, GST_MAP_WRITE);
    copyToGstBufferPlane(mappedBuffer.data(), info, 0, span.data(), height, planeY.sourceWidthBytes);
    copyToGstBufferPlane(mappedBuffer.data(), info, 1, span.data() + planeU.destinationOffset, height / 2, planeU.sourceWidthBytes);
    copyToGstBufferPlane(mappedBuffer.data(), info, 2, span.data() + planeV.destinationOffset, height / 2, planeV.sourceWidthBytes);
    copyToGstBufferPlane(mappedBuffer.data(), info, 3, span.data() + planeA.destinationOffset, height, planeA.sourceWidthBytes);
    gst_buffer_add_video_meta(buffer.get(), GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_FORMAT_A420, width, height);

    auto caps = adoptGRef(gst_video_info_to_caps(&info));
    auto sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
    FloatSize presentationSize { static_cast<float>(width), static_cast<float>(height) };
    return VideoFrameGStreamer::create(WTFMove(sample), presentationSize, MediaTime::invalidTime(), Rotation::None, false, std::nullopt, WTFMove(colorSpace));
}

static inline void setBufferFields(GstBuffer* buffer, const MediaTime& presentationTime, double frameRate)
{
    GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_LIVE);
    GST_BUFFER_DTS(buffer) = GST_BUFFER_PTS(buffer) = toGstClockTime(presentationTime);
    GST_BUFFER_DURATION(buffer) = toGstClockTime(1_s / frameRate);
}

Ref<VideoFrameGStreamer> VideoFrameGStreamer::create(GRefPtr<GstSample>&& sample, const FloatSize& presentationSize, const MediaTime& presentationTime, Rotation videoRotation, bool videoMirrored, std::optional<VideoFrameTimeMetadata>&& metadata, std::optional<PlatformVideoColorSpace>&& colorSpace)
{
    PlatformVideoColorSpace platformColorSpace;
    if (colorSpace)
        platformColorSpace = *colorSpace;
    else {
        auto* caps = gst_sample_get_caps(sample.get());
        if (doCapsHaveType(caps, GST_VIDEO_CAPS_TYPE_PREFIX))
            platformColorSpace = videoColorSpaceFromCaps(caps);
    }

    return adoptRef(*new VideoFrameGStreamer(WTFMove(sample), presentationSize, presentationTime, videoRotation, videoMirrored, WTFMove(metadata), WTFMove(platformColorSpace)));
}

Ref<VideoFrameGStreamer> VideoFrameGStreamer::createWrappedSample(const GRefPtr<GstSample>& sample, const MediaTime& presentationTime, Rotation videoRotation)
{
    auto* caps = gst_sample_get_caps(sample.get());
    auto presentationSize = getVideoResolutionFromCaps(caps);
    RELEASE_ASSERT(presentationSize);
    auto colorSpace = videoColorSpaceFromCaps(caps);
    MediaTime timeStamp = presentationTime;
    if (presentationTime.isInvalid())
        timeStamp = fromGstClockTime(GST_BUFFER_PTS(gst_sample_get_buffer(sample.get())));
    return adoptRef(*new VideoFrameGStreamer(sample, *presentationSize, timeStamp, videoRotation, WTFMove(colorSpace)));
}

RefPtr<VideoFrameGStreamer> VideoFrameGStreamer::createFromPixelBuffer(Ref<PixelBuffer>&& pixelBuffer, CanvasContentType canvasContentType, Rotation videoRotation, const MediaTime& presentationTime, const IntSize& destinationSize, double frameRate, bool videoMirrored, std::optional<VideoFrameTimeMetadata>&& metadata, PlatformVideoColorSpace&& colorSpace)
{
    ensureGStreamerInitialized();

    ensureVideoFrameDebugCategoryInitialized();
    auto size = pixelBuffer->size();

    auto sizeInBytes = pixelBuffer->bytes().size();
    auto dataBaseAddress = pixelBuffer->bytes().data();
    auto leakedPixelBuffer = &pixelBuffer.leakRef();

    auto buffer = adoptGRef(gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY, dataBaseAddress, sizeInBytes, 0, sizeInBytes, leakedPixelBuffer, [](gpointer userData) {
        static_cast<PixelBuffer*>(userData)->deref();
    }));

    auto width = size.width();
    auto height = size.height();
    GstVideoFormat format;

    switch (canvasContentType) {
    case CanvasContentType::WebGL:
        format = GST_VIDEO_FORMAT_RGBA;
        break;
    case CanvasContentType::Canvas2D:
        format = GST_VIDEO_FORMAT_BGRA;
        break;
    }
    auto formatName = WTF::span(gst_video_format_to_string(format));
    GST_TRACE("Creating %s VideoFrame from pixel buffer", formatName.data());

    int frameRateNumerator, frameRateDenominator;
    gst_util_double_to_fraction(frameRate, &frameRateNumerator, &frameRateDenominator);

    auto caps = adoptGRef(gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, formatName.data(), "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, nullptr));
    if (frameRate)
        gst_caps_set_simple(caps.get(), "framerate", GST_TYPE_FRACTION, frameRateNumerator, frameRateDenominator, nullptr);

    GRefPtr<GstSample> sample;

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
        GUniqueOutPtr<GError> error;
        sample = adoptGRef(gst_video_convert_sample(inputSample.get(), outputCaps.get(), GST_CLOCK_TIME_NONE, &error.outPtr()));
        if (!sample) {
            GST_ERROR("Video sample conversion failed: %s", error->message);
            return nullptr;
        }

        auto outputBuffer = gst_sample_get_buffer(sample.get());
        gst_buffer_add_video_meta(outputBuffer, GST_VIDEO_FRAME_FLAG_NONE, format, width, height);
        if (metadata)
            webkitGstBufferSetVideoFrameTimeMetadata(outputBuffer, *metadata);

        setBufferFields(outputBuffer, presentationTime, frameRate);
    } else {
        gst_buffer_add_video_meta(buffer.get(), GST_VIDEO_FRAME_FLAG_NONE, format, width, height);
        if (metadata)
            buffer = webkitGstBufferSetVideoFrameTimeMetadata(buffer.get(), *metadata);

        setBufferFields(buffer.get(), presentationTime, frameRate);
        sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
    }

    return adoptRef(*new VideoFrameGStreamer(WTFMove(sample), FloatSize(width, height), presentationTime, videoRotation, videoMirrored, { }, WTFMove(colorSpace)));
}

VideoFrameGStreamer::VideoFrameGStreamer(GRefPtr<GstSample>&& sample, const FloatSize& presentationSize, const MediaTime& presentationTime, Rotation videoRotation, bool videoMirrored, std::optional<VideoFrameTimeMetadata>&& metadata, PlatformVideoColorSpace&& colorSpace)
    : VideoFrame(presentationTime, videoMirrored, videoRotation, WTFMove(colorSpace))
    , m_sample(WTFMove(sample))
    , m_presentationSize(presentationSize)
{
    ensureVideoFrameDebugCategoryInitialized();
    ASSERT(m_sample);

    if (!metadata)
        return;

    GstBuffer* buffer = gst_sample_get_buffer(m_sample.get());
    RELEASE_ASSERT(buffer);
    buffer = webkitGstBufferSetVideoFrameTimeMetadata(buffer, WTFMove(metadata));
    m_sample = adoptGRef(gst_sample_make_writable(m_sample.leakRef()));
    gst_sample_set_buffer(m_sample.get(), buffer);
}

VideoFrameGStreamer::VideoFrameGStreamer(const GRefPtr<GstSample>& sample, const FloatSize& presentationSize, const MediaTime& presentationTime, Rotation videoRotation, PlatformVideoColorSpace&& colorSpace)
    : VideoFrame(presentationTime, false, videoRotation, WTFMove(colorSpace))
    , m_sample(sample)
    , m_presentationSize(presentationSize)
{
    ensureVideoFrameDebugCategoryInitialized();
}

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

static void copyPlane(uint8_t* destination, const uint8_t* source, uint64_t sourceStride, const ComputedPlaneLayout& spanPlaneLayout)
{
    uint64_t sourceOffset = spanPlaneLayout.sourceTop * sourceStride;
    sourceOffset += spanPlaneLayout.sourceLeftBytes;
    uint64_t destinationOffset = spanPlaneLayout.destinationOffset;
    uint64_t rowBytes = spanPlaneLayout.sourceWidthBytes;
    for (size_t rowIndex = 0; rowIndex < spanPlaneLayout.sourceHeight; ++rowIndex) {
        std::memcpy(destination + destinationOffset, source + sourceOffset, rowBytes);
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

    GST_TRACE("Copying frame data to pixel format %d", static_cast<int>(pixelFormat));
    if (pixelFormat == VideoPixelFormat::NV12) {
        auto spanPlaneLayoutY = computedPlaneLayout[0];
        auto widthY = GST_VIDEO_FRAME_COMP_WIDTH(inputFrame.get(), 0);
        PlaneLayout planeLayoutY { spanPlaneLayoutY.destinationOffset, spanPlaneLayoutY.destinationStride ? spanPlaneLayoutY.destinationStride : widthY };

        auto spanPlaneLayoutUV = computedPlaneLayout[1];
        auto widthUV = GST_VIDEO_FRAME_COMP_WIDTH(inputFrame.get(), 1);
        PlaneLayout planeLayoutUV { spanPlaneLayoutUV.destinationOffset, spanPlaneLayoutUV.destinationStride ? spanPlaneLayoutUV.destinationStride : widthUV };

        auto planeY = inputFrame.componentData(0);
        auto bytesPerRowY = inputFrame.componentStride(0);
        copyPlane(destination.data(), planeY, bytesPerRowY, spanPlaneLayoutY);

        auto planeUV = inputFrame.componentData(1);
        auto bytesPerRowUV = inputFrame.componentStride(1);
        copyPlane(destination.data(), planeUV, bytesPerRowUV, spanPlaneLayoutUV);

        Vector<PlaneLayout> planeLayouts;
        planeLayouts.append(planeLayoutY);
        planeLayouts.append(planeLayoutUV);
        callback(WTFMove(planeLayouts));
        return;
    }

    if (pixelFormat == VideoPixelFormat::I420 || pixelFormat == VideoPixelFormat::I420A) {
        auto spanPlaneLayoutY = computedPlaneLayout[0];
        auto widthY = GST_VIDEO_FRAME_COMP_WIDTH(inputFrame.get(), 0);
        PlaneLayout planeLayoutY { spanPlaneLayoutY.destinationOffset, spanPlaneLayoutY.destinationStride ? spanPlaneLayoutY.destinationStride : widthY };
        auto planeY = inputFrame.componentData(0);
        auto bytesPerRowY = inputFrame.componentStride(0);
        copyPlane(destination.data(), planeY, bytesPerRowY, spanPlaneLayoutY);

        auto spanPlaneLayoutU = computedPlaneLayout[1];
        auto widthUV = GST_VIDEO_FRAME_COMP_WIDTH(inputFrame.get(), 1);
        PlaneLayout planeLayoutU { spanPlaneLayoutU.destinationOffset, spanPlaneLayoutU.destinationStride ? spanPlaneLayoutU.destinationStride : widthUV / 2 };

        auto spanPlaneLayoutV = computedPlaneLayout[2];
        PlaneLayout planeLayoutV { spanPlaneLayoutV.destinationOffset, spanPlaneLayoutV.destinationStride ? spanPlaneLayoutV.destinationStride : widthUV / 2 };

        auto planeU = inputFrame.componentData(1);
        auto bytesPerRowU = inputFrame.componentStride(1);
        copyPlane(destination.data(), planeU, bytesPerRowU, spanPlaneLayoutU);

        auto planeV = inputFrame.componentData(2);
        auto bytesPerRowV = inputFrame.componentStride(2);
        copyPlane(destination.data(), planeV, bytesPerRowV, spanPlaneLayoutV);

        Vector<PlaneLayout> planeLayouts;
        planeLayouts.append(planeLayoutY);
        planeLayouts.append(planeLayoutU);
        planeLayouts.append(planeLayoutV);

        if (pixelFormat == VideoPixelFormat::I420A) {
            auto spanPlaneLayoutA = computedPlaneLayout[3];
            auto widthA = GST_VIDEO_FRAME_COMP_WIDTH(inputFrame.get(), 3);
            PlaneLayout planeLayoutA { spanPlaneLayoutA.destinationOffset, spanPlaneLayoutA.destinationStride ? spanPlaneLayoutA.destinationStride : widthA };
            auto planeA = inputFrame.componentData(3);
            auto bytesPerRowA = inputFrame.componentStride(3);
            copyPlane(destination.data(), planeA, bytesPerRowA, spanPlaneLayoutA);
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
        auto plane = mappedBuffer.data();
        auto bytesPerRow = inputFrame.componentStride(0);
        copyPlane(destination.data(), plane, bytesPerRow, planeLayout);
        Vector<PlaneLayout> planeLayouts;
        planeLayouts.append({ planeLayout.destinationOffset, planeLayout.destinationStride ? planeLayout.destinationStride : 4 * inputFrame.width() });
        callback(WTFMove(planeLayouts));
        return;
    }

    // FIXME: Handle I422, I444, RGBX and BGRX formats.
    callback({ });
}

void VideoFrame::draw(GraphicsContext& context, const FloatRect& destination, ImageOrientation destinationImageOrientation, bool shouldDiscardAlpha)
{
    auto image = convertSampleToImage(downcast<VideoFrameGStreamer>(*this).sample());
    if (!image)
        return;

    auto imageRect = image->rect();
    auto source = destinationImageOrientation.usesWidthAsHeight() ? FloatRect(imageRect.location(), imageRect.size().transposedSize()) : imageRect;
    auto compositeOperator = !shouldDiscardAlpha && image->hasAlpha() ? CompositeOperator::SourceOver : CompositeOperator::Copy;
    auto platformImage = image->image();
    auto bitmapImage = BitmapImage::create(WTFMove(platformImage));
    if (!bitmapImage)
        return;
    context.drawImage(*bitmapImage.get(), destination, source, { compositeOperator, destinationImageOrientation });
}

uint32_t VideoFrameGStreamer::pixelFormat() const
{
    if (m_cachedVideoFormat != GST_VIDEO_FORMAT_UNKNOWN)
        return m_cachedVideoFormat;

    GstVideoInfo inputInfo;
    gst_video_info_from_caps(&inputInfo, gst_sample_get_caps(m_sample.get()));
    m_cachedVideoFormat = GST_VIDEO_INFO_FORMAT(&inputInfo);
    return m_cachedVideoFormat;
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
    auto formatName = WTF::span(gst_video_format_to_string(format));
    auto outputCaps = adoptGRef(gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, formatName.data(), "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, "framerate", GST_TYPE_FRACTION, frameRateNumerator, frameRateDenominator, nullptr));

    if (gst_caps_is_equal(caps, outputCaps.get()))
        return GRefPtr<GstSample>(m_sample);

    GUniqueOutPtr<GError> error;
    auto convertedSample = adoptGRef(gst_video_convert_sample(m_sample.get(), outputCaps.get(), GST_CLOCK_TIME_NONE, &error.outPtr()));
    if (!convertedSample)
        GST_ERROR("Conversion to %s failed: %s", formatName.data(), error->message);

    return convertedSample;
}

GRefPtr<GstSample> VideoFrameGStreamer::downloadSample(std::optional<GstVideoFormat> destinationFormat)
{
    return convert(destinationFormat.value_or(static_cast<GstVideoFormat>(pixelFormat())), roundedIntSize(presentationSize()));
}

RefPtr<VideoFrameGStreamer> VideoFrameGStreamer::resizeTo(const IntSize& destinationSize)
{
    return VideoFrameGStreamer::create(resizedSample(destinationSize), destinationSize, presentationTime(), rotation(), isMirrored());
}

RefPtr<ImageGStreamer> VideoFrameGStreamer::convertToImage()
{
    return convertSampleToImage(m_sample);
}

Ref<VideoFrame> VideoFrameGStreamer::clone()
{
    return createWrappedSample(m_sample, presentationTime(), rotation());
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
