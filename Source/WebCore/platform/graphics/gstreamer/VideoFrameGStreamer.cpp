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

#include "GStreamerCommon.h"
#include "GraphicsContext.h"
#include "ImageGStreamer.h"
#include "ImageOrientation.h"
#include "PixelBuffer.h"
#include "VideoPixelFormat.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/TypedArrayInlines.h>

#if USE(CAIRO)
#include <cairo.h>
#endif

#if USE(GSTREAMER_GL)
#include <gst/gl/gstglcolorconvert.h>
#include <gst/gl/gstglmemory.h>
#endif

GST_DEBUG_CATEGORY(webkit_video_frame_debug);
#define GST_CAT_DEFAULT webkit_video_frame_debug

namespace WebCore {

static void ensureDebugCategoryInitialized()
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_video_frame_debug, "webkitvideoframe", 0, "WebKit Video Frame");
    });
}

class GstSampleColorConverter {
public:
    static GstSampleColorConverter& singleton();

    RefPtr<ImageGStreamer> convertSampleToImage(const GRefPtr<GstSample>&);
    GRefPtr<GstSample> convertSample(const GRefPtr<GstSample>&, const GRefPtr<GstCaps>&, GRefPtr<GstBuffer>&& = nullptr);

private:
#if USE(GSTREAMER_GL)
    GRefPtr<GstGLColorConvert> m_glColorConvert;
#endif
    GUniquePtr<GstVideoConverter> m_colorConvert;
    GRefPtr<GstCaps> m_colorConvertInputCaps;
    GRefPtr<GstCaps> m_colorConvertOutputCaps;
};

GstSampleColorConverter& GstSampleColorConverter::singleton()
{
    ensureDebugCategoryInitialized();
    static NeverDestroyed<GstSampleColorConverter> sharedInstance;
    return sharedInstance;
}

GRefPtr<GstSample> GstSampleColorConverter::convertSample(const GRefPtr<GstSample>& sample, const GRefPtr<GstCaps>& outputCaps, GRefPtr<GstBuffer>&& outputBuffer)
{
    auto* buffer = gst_sample_get_buffer(sample.get());
    if (UNLIKELY(!GST_IS_BUFFER(buffer)))
        return nullptr;

    auto* caps = gst_sample_get_caps(sample.get());

    GstVideoInfo videoInfo;
    if (!gst_video_info_from_caps(&videoInfo, caps))
        return nullptr;

    GST_DEBUG("Converting video frame from %" GST_PTR_FORMAT " to %" GST_PTR_FORMAT, caps, outputCaps.get());

    auto colorConvert = [&]() -> GRefPtr<GstBuffer> {
        GstVideoInfo outputInfo;
        if (!gst_video_info_from_caps(&outputInfo, outputCaps.get()))
            return nullptr;

        if (!m_colorConvertOutputCaps || !gst_caps_is_equal(m_colorConvertOutputCaps.get(), outputCaps.get())
            || !m_colorConvertInputCaps || !gst_caps_is_equal(m_colorConvertInputCaps.get(), caps)) {
            m_colorConvert.reset(gst_video_converter_new(&videoInfo, &outputInfo, nullptr));
            m_colorConvertInputCaps = adoptGRef(gst_caps_copy(caps));
            m_colorConvertOutputCaps = outputCaps;
        }

        if (!outputBuffer)
            outputBuffer = adoptGRef(gst_buffer_new_allocate(nullptr, GST_VIDEO_INFO_SIZE(&outputInfo), nullptr));
        GstMappedFrame inputFrame(buffer, videoInfo, GST_MAP_READ);
        GstMappedFrame outputFrame(outputBuffer.get(), outputInfo, GST_MAP_WRITE);
        gst_video_converter_frame(m_colorConvert.get(), inputFrame.get(), outputFrame.get());
        return outputBuffer;
    };

    GRefPtr<GstBuffer> convertedBuffer;
    auto* memory = gst_buffer_peek_memory(buffer, 0);
#if USE(GSTREAMER_GL)
    if (gst_is_gl_memory(memory)) {
        if (gst_caps_features_contains(gst_caps_get_features(outputCaps.get(), 0), GST_CAPS_FEATURE_MEMORY_GL_MEMORY)) {
            if (!m_colorConvertInputCaps || !gst_caps_is_equal(m_colorConvertInputCaps.get(), caps)) {
                m_glColorConvert = adoptGRef(gst_gl_color_convert_new(GST_GL_BASE_MEMORY_CAST(memory)->context));
                m_colorConvertInputCaps = adoptGRef(gst_caps_copy(caps));
                m_colorConvertOutputCaps = adoptGRef(gst_caps_copy(outputCaps.get()));

                gst_caps_set_features(m_colorConvertOutputCaps.get(), 0, gst_caps_features_new(GST_CAPS_FEATURE_MEMORY_GL_MEMORY, nullptr));
                gst_caps_set_simple(m_colorConvertOutputCaps.get(), "texture-target", G_TYPE_STRING, GST_GL_TEXTURE_TARGET_2D_STR, nullptr);
                if (!gst_gl_color_convert_set_caps(m_glColorConvert.get(), caps, m_colorConvertOutputCaps.get())) {
                    m_colorConvertInputCaps.clear();
                    m_colorConvertOutputCaps.clear();
                    return nullptr;
                }
            }
            convertedBuffer = adoptGRef(gst_gl_color_convert_perform(m_glColorConvert.get(), buffer));
        } else
            convertedBuffer = colorConvert();
    } else
#endif
        convertedBuffer = colorConvert();

    if (UNLIKELY(!GST_IS_BUFFER(convertedBuffer.get())))
        return nullptr;

    const auto* info = gst_sample_get_info(sample.get());
    auto convertedSample = adoptGRef(gst_sample_new(outputBuffer.get(), m_colorConvertOutputCaps.get(),
        gst_sample_get_segment(sample.get()), info ? gst_structure_copy(info) : nullptr));
    return convertedSample;
}

RefPtr<ImageGStreamer> GstSampleColorConverter::convertSampleToImage(const GRefPtr<GstSample>& sample)
{
    GstVideoInfo videoInfo;
    if (!gst_video_info_from_caps(&videoInfo, gst_sample_get_caps(sample.get())))
        return nullptr;

    // These caps must match the internal format of a cairo surface with CAIRO_FORMAT_ARGB32,
    // so we don't need to perform color conversions when painting the video frame.
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    const char* formatString = GST_VIDEO_INFO_HAS_ALPHA(&videoInfo) ? "BGRA" : "BGRx";
#else
    const char* formatString = GST_VIDEO_INFO_HAS_ALPHA(&videoInfo) ? "ARGB" : "xRGB";
#endif
    auto caps = adoptGRef(gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, formatString, "framerate", GST_TYPE_FRACTION, GST_VIDEO_INFO_FPS_N(&videoInfo), GST_VIDEO_INFO_FPS_D(&videoInfo), "width", G_TYPE_INT, GST_VIDEO_INFO_WIDTH(&videoInfo), "height", G_TYPE_INT, GST_VIDEO_INFO_HEIGHT(&videoInfo), nullptr));
    auto convertedSample = convertSample(sample, caps);
    if (!convertedSample)
        return nullptr;

    return ImageGStreamer::createImage(WTFMove(convertedSample));
}

RefPtr<VideoFrame> VideoFrame::fromNativeImage(NativeImage& image)
{
#if USE(CAIRO)
    size_t offsets[GST_VIDEO_MAX_PLANES] = { 0, };
    int strides[GST_VIDEO_MAX_PLANES] = { 0, };

    auto surface = image.platformImage();
    strides[0] = cairo_image_surface_get_stride(surface.get());
    auto width = cairo_image_surface_get_width(surface.get());
    auto height = cairo_image_surface_get_height(surface.get());
    auto size = height * strides[0];
    auto format = G_BYTE_ORDER == G_LITTLE_ENDIAN ? GST_VIDEO_FORMAT_BGRA : GST_VIDEO_FORMAT_ARGB;

    auto buffer = adoptGRef(gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY, cairo_image_surface_get_data(surface.get()), size, 0, size, surface.get(), reinterpret_cast<GDestroyNotify>(cairo_surface_destroy)));
    gst_buffer_add_video_meta_full(buffer.get(), GST_VIDEO_FRAME_FLAG_NONE, format, width, height, 1, offsets, strides);

    auto caps = adoptGRef(gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, gst_video_format_to_string(format), "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, nullptr));
    auto sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
    FloatSize presentationSize { static_cast<float>(width), static_cast<float>(height) };
    return VideoFrameGStreamer::create(WTFMove(sample), presentationSize);
#endif
    return nullptr;
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

RefPtr<VideoFrame> VideoFrame::createNV12(Span<const uint8_t> span, size_t width, size_t height, const ComputedPlaneLayout& planeY, const ComputedPlaneLayout& planeUV, PlatformVideoColorSpace&& colorSpace)
{
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

RefPtr<VideoFrame> VideoFrame::createRGBA(Span<const uint8_t> span, size_t width, size_t height, const ComputedPlaneLayout& plane, PlatformVideoColorSpace&& colorSpace)
{
    CREATE_RGBA_FRAME(GST_VIDEO_FORMAT_RGBA);
}

RefPtr<VideoFrame> VideoFrame::createBGRA(Span<const uint8_t> span, size_t width, size_t height, const ComputedPlaneLayout& plane, PlatformVideoColorSpace&& colorSpace)
{
    CREATE_RGBA_FRAME(GST_VIDEO_FORMAT_BGRA);
}

#undef CREATE_RGBA_FRAME

RefPtr<VideoFrame> VideoFrame::createI420(Span<const uint8_t> span, size_t width, size_t height, const ComputedPlaneLayout& planeY, const ComputedPlaneLayout& planeU, const ComputedPlaneLayout& planeV, PlatformVideoColorSpace&& colorSpace)
{
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
    return adoptRef(*new VideoFrameGStreamer(sample, *presentationSize, presentationTime, videoRotation, WTFMove(colorSpace)));
}

Ref<VideoFrameGStreamer> VideoFrameGStreamer::createFromPixelBuffer(Ref<PixelBuffer>&& pixelBuffer, CanvasContentType canvasContentType, Rotation videoRotation, const MediaTime& presentationTime, const IntSize& destinationSize, double frameRate, bool videoMirrored, std::optional<VideoFrameTimeMetadata>&& metadata)
{
    ensureGStreamerInitialized();

    auto size = pixelBuffer->size();

    auto sizeInBytes = pixelBuffer->sizeInBytes();
    auto dataBaseAddress = pixelBuffer->bytes();
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
    const char* formatName = gst_video_format_to_string(format);
    int frameRateNumerator, frameRateDenominator;
    gst_util_double_to_fraction(frameRate, &frameRateNumerator, &frameRateDenominator);

    auto caps = adoptGRef(gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, formatName, "width", G_TYPE_INT, width,
        "height", G_TYPE_INT, height, "framerate", GST_TYPE_FRACTION, frameRateNumerator, frameRateDenominator, nullptr));

    GRefPtr<GstSample> sample;

    // Optionally resize the video frame to fit destinationSize. This code path is used mostly by
    // the mock realtime video source when the gUM constraints specifically required exact width
    // and/or height values.
    if (!destinationSize.isZero() && size != destinationSize) {
        GstVideoInfo inputInfo;
        gst_video_info_from_caps(&inputInfo, caps.get());

        width = destinationSize.width();
        height = destinationSize.height();
        auto outputCaps = adoptGRef(gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, formatName, "width", G_TYPE_INT, width,
            "height", G_TYPE_INT, height, "framerate", GST_TYPE_FRACTION, frameRateNumerator, frameRateDenominator, nullptr));

        auto inputSample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
        sample = GstSampleColorConverter::singleton().convertSample(inputSample, outputCaps);
        if (sample) {
            auto* outputBuffer = gst_sample_get_buffer(sample.get());
            if (metadata)
                webkitGstBufferSetVideoFrameTimeMetadata(outputBuffer, *metadata);

            setBufferFields(outputBuffer, presentationTime, frameRate);
        }
    } else {
        if (metadata)
            buffer = webkitGstBufferSetVideoFrameTimeMetadata(buffer.get(), *metadata);

        setBufferFields(buffer.get(), presentationTime, frameRate);
        sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
    }

    return adoptRef(*new VideoFrameGStreamer(WTFMove(sample), FloatSize(width, height), presentationTime, videoRotation, videoMirrored, WTFMove(metadata)));
}

VideoFrameGStreamer::VideoFrameGStreamer(GRefPtr<GstSample>&& sample, const FloatSize& presentationSize, const MediaTime& presentationTime, Rotation videoRotation, bool videoMirrored, std::optional<VideoFrameTimeMetadata>&& metadata, PlatformVideoColorSpace&& colorSpace)
    : VideoFrame(presentationTime, videoMirrored, videoRotation, WTFMove(colorSpace))
    , m_sample(WTFMove(sample))
    , m_presentationSize(presentationSize)
{
    ensureDebugCategoryInitialized();
    ASSERT(m_sample);
    GstBuffer* buffer = gst_sample_get_buffer(m_sample.get());
    RELEASE_ASSERT(buffer);

    if (metadata)
        buffer = webkitGstBufferSetVideoFrameTimeMetadata(buffer, WTFMove(metadata));
}

VideoFrameGStreamer::VideoFrameGStreamer(const GRefPtr<GstSample>& sample, const FloatSize& presentationSize, const MediaTime& presentationTime, Rotation videoRotation, PlatformVideoColorSpace&& colorSpace)
    : VideoFrame(presentationTime, false, videoRotation, WTFMove(colorSpace))
    , m_sample(sample)
    , m_presentationSize(presentationSize)
{
    ensureDebugCategoryInitialized();
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

void VideoFrame::copyTo(Span<uint8_t> destination, VideoPixelFormat pixelFormat, Vector<ComputedPlaneLayout>&& computedPlaneLayout, CompletionHandler<void(std::optional<Vector<PlaneLayout>>&&)>&& callback)
{
    GstVideoInfo inputInfo;
    auto sample = downcast<VideoFrameGStreamer>(*this).sample();
    auto* inputBuffer = gst_sample_get_buffer(sample);
    auto* inputCaps = gst_sample_get_caps(sample);
    gst_video_info_from_caps(&inputInfo, inputCaps);
    GstMappedFrame inputFrame(inputBuffer, inputInfo, GST_MAP_READ);

    if (pixelFormat == VideoPixelFormat::NV12) {
        auto spanPlaneLayoutY = computedPlaneLayout[0];
        auto widthY = GST_VIDEO_FRAME_COMP_WIDTH(inputFrame.get(), 0);
        PlaneLayout planeLayoutY { spanPlaneLayoutY.destinationOffset, spanPlaneLayoutY.destinationStride ? spanPlaneLayoutY.destinationStride : widthY };

        auto spanPlaneLayoutUV = computedPlaneLayout[1];
        auto widthUV = GST_VIDEO_FRAME_COMP_WIDTH(inputFrame.get(), 1);
        PlaneLayout planeLayoutUV { spanPlaneLayoutUV.destinationOffset, spanPlaneLayoutUV.destinationStride ? spanPlaneLayoutUV.destinationStride : widthUV };

        auto planeY = inputFrame.ComponentData(0);
        auto bytesPerRowY = inputFrame.ComponentStride(0);
        copyPlane(destination.data(), planeY, bytesPerRowY, spanPlaneLayoutY);

        auto planeUV = inputFrame.ComponentData(1);
        auto bytesPerRowUV = inputFrame.ComponentStride(1);
        copyPlane(destination.data(), planeUV, bytesPerRowUV, spanPlaneLayoutUV);

        Vector<PlaneLayout> planeLayouts;
        planeLayouts.append(planeLayoutY);
        planeLayouts.append(planeLayoutUV);
        callback(WTFMove(planeLayouts));
        return;
    }

    if (pixelFormat == VideoPixelFormat::I420) {
        auto spanPlaneLayoutY = computedPlaneLayout[0];
        auto widthY = GST_VIDEO_FRAME_COMP_WIDTH(inputFrame.get(), 0);
        PlaneLayout planeLayoutY { spanPlaneLayoutY.destinationOffset, spanPlaneLayoutY.destinationStride ? spanPlaneLayoutY.destinationStride : widthY };
        auto planeY = inputFrame.ComponentData(0);
        auto bytesPerRowY = inputFrame.ComponentStride(0);
        copyPlane(destination.data(), planeY, bytesPerRowY, spanPlaneLayoutY);

        auto spanPlaneLayoutU = computedPlaneLayout[1];
        auto widthUV = GST_VIDEO_FRAME_COMP_WIDTH(inputFrame.get(), 1);
        PlaneLayout planeLayoutU { spanPlaneLayoutU.destinationOffset, spanPlaneLayoutU.destinationStride ? spanPlaneLayoutU.destinationStride : widthUV / 2 };

        auto spanPlaneLayoutV = computedPlaneLayout[2];
        PlaneLayout planeLayoutV { spanPlaneLayoutV.destinationOffset, spanPlaneLayoutV.destinationStride ? spanPlaneLayoutV.destinationStride : widthUV / 2 };

        auto planeU = inputFrame.ComponentData(1);
        auto bytesPerRowU = inputFrame.ComponentStride(1);
        copyPlane(destination.data(), planeU, bytesPerRowU, spanPlaneLayoutU);

        auto planeV = inputFrame.ComponentData(2);
        auto bytesPerRowV = inputFrame.ComponentStride(2);
        copyPlane(destination.data(), planeV, bytesPerRowV, spanPlaneLayoutV);

        Vector<PlaneLayout> planeLayouts;
        planeLayouts.append(planeLayoutY);
        planeLayouts.append(planeLayoutU);
        planeLayouts.append(planeLayoutV);
        callback(WTFMove(planeLayouts));
        return;
    }

    if (pixelFormat == VideoPixelFormat::RGBA || pixelFormat == VideoPixelFormat::BGRA) {
        ComputedPlaneLayout planeLayout;
        if (!computedPlaneLayout.isEmpty())
            planeLayout = computedPlaneLayout[0];
        GstMappedBuffer mappedBuffer(inputBuffer, GST_MAP_READ);
        auto plane = mappedBuffer.data();
        auto bytesPerRow = inputFrame.ComponentStride(0);
        copyPlane(destination.data(), plane, bytesPerRow, planeLayout);
        Vector<PlaneLayout> planeLayouts;
        planeLayouts.append({ planeLayout.destinationOffset, planeLayout.destinationStride ? planeLayout.destinationStride : 4 * inputFrame.width() });
        callback(WTFMove(planeLayouts));
        return;
    }

    // FIXME: Handle I420A, I422, I444, RGBX and BGRX formats.
    callback({ });
}

void VideoFrame::paintInContext(GraphicsContext & context, const FloatRect& destination, const ImageOrientation& destinationImageOrientation, bool shouldDiscardAlpha)
{
    auto image = GstSampleColorConverter::singleton().convertSampleToImage(downcast<VideoFrameGStreamer>(*this).sample());
    if (!image)
        return;

    auto imageRect = image->rect();
    auto source = destinationImageOrientation.usesWidthAsHeight() ? FloatRect(imageRect.location(), imageRect.size().transposedSize()) : imageRect;
    auto compositeOperator = !shouldDiscardAlpha && image->hasAlpha() ? CompositeOperator::SourceOver : CompositeOperator::Copy;
    context.drawImage(image->image(), destination, source, { compositeOperator, destinationImageOrientation });
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

RefPtr<VideoFrameGStreamer> VideoFrameGStreamer::resizeTo(const IntSize& destinationSize)
{
    auto* caps = gst_sample_get_caps(m_sample.get());

    const auto* structure = gst_caps_get_structure(caps, 0);
    int frameRateNumerator, frameRateDenominator;
    if (!gst_structure_get_fraction(structure, "framerate", &frameRateNumerator, &frameRateDenominator)) {
        frameRateNumerator = 1;
        frameRateDenominator = 1;
    }

    GstVideoInfo inputInfo;
    gst_video_info_from_caps(&inputInfo, caps);
    auto format = GST_VIDEO_INFO_FORMAT(&inputInfo);
    auto width = destinationSize.width();
    auto height = destinationSize.height();
    auto outputCaps = adoptGRef(gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, gst_video_format_to_string(format), "width", G_TYPE_INT, width,
        "height", G_TYPE_INT, height, "framerate", GST_TYPE_FRACTION, frameRateNumerator, frameRateDenominator, nullptr));
    GstVideoInfo outputInfo;
    gst_video_info_from_caps(&outputInfo, outputCaps.get());

    auto* buffer = gst_sample_get_buffer(m_sample.get());
    auto outputBuffer = adoptGRef(gst_buffer_new_allocate(nullptr, GST_VIDEO_INFO_SIZE(&outputInfo), nullptr));
    {
        GUniquePtr<GstVideoConverter> converter(gst_video_converter_new(&inputInfo, &outputInfo, nullptr));
        GstMappedFrame inputFrame(buffer, inputInfo, GST_MAP_READ);
        GstMappedFrame outputFrame(outputBuffer.get(), outputInfo, GST_MAP_WRITE);
        gst_video_converter_frame(converter.get(), inputFrame.get(), outputFrame.get());
    }

    double frameRate;
    gst_util_fraction_to_double(frameRateNumerator, frameRateDenominator, &frameRate);

    auto presentationTime = this->presentationTime();
    setBufferFields(outputBuffer.get(), presentationTime, frameRate);
    auto sample = adoptGRef(gst_sample_new(outputBuffer.get(), outputCaps.get(), nullptr, nullptr));
    return VideoFrameGStreamer::create(WTFMove(sample), destinationSize, presentationTime, rotation(), isMirrored());
}

RefPtr<JSC::Uint8ClampedArray> VideoFrameGStreamer::computeRGBAImageData() const
{
    auto* caps = gst_sample_get_caps(m_sample.get());
    GstVideoInfo inputInfo;
    if (!gst_video_info_from_caps(&inputInfo, caps))
        return nullptr;

    // We could check the input format is RGBA before attempting a conversion, but it is very
    // unlikely to pay off. The input format is likely to be BGRA (when the samples are created as a
    // result of mediastream captureStream) or some YUV format if the sample is from a video capture
    // device. This method is called only by internals during layout tests, it is thus not critical
    // to optimize this code path.

    auto outputCaps = adoptGRef(gst_caps_copy(caps));
    gst_caps_set_simple(outputCaps.get(), "format", G_TYPE_STRING, "RGBA", nullptr);

    GstVideoInfo outputInfo;
    if (!gst_video_info_from_caps(&outputInfo, outputCaps.get()))
        return nullptr;

    int width = GST_VIDEO_INFO_WIDTH(&inputInfo);
    int height = GST_VIDEO_INFO_HEIGHT(&inputInfo);
    unsigned byteLength = GST_VIDEO_INFO_SIZE(&inputInfo);
    auto bufferStorage = JSC::ArrayBuffer::create(width * height, 4);
    auto outputBuffer = adoptGRef(gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_NO_SHARE, bufferStorage->data(), byteLength, 0, byteLength, nullptr, [](gpointer) { }));
    GstMappedFrame outputFrame(outputBuffer.get(), outputInfo, GST_MAP_WRITE);

    GUniquePtr<GstVideoConverter> converter(gst_video_converter_new(&inputInfo, &outputInfo, nullptr));
    GstMappedFrame inputFrame(gst_sample_get_buffer(m_sample.get()), inputInfo, GST_MAP_READ);
    gst_video_converter_frame(converter.get(), inputFrame.get(), outputFrame.get());
    return JSC::Uint8ClampedArray::tryCreate(WTFMove(bufferStorage), 0, byteLength);
}

RefPtr<ImageGStreamer> VideoFrameGStreamer::convertToImage()
{
    return GstSampleColorConverter::singleton().convertSampleToImage(m_sample);
}

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
