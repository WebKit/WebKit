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
    GRefPtr<GstSample> convertSample(const GRefPtr<GstSample>&, const GRefPtr<GstCaps>&);

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

GRefPtr<GstSample> GstSampleColorConverter::convertSample(const GRefPtr<GstSample>& sample, const GRefPtr<GstCaps>& outputCaps)
{
    auto* buffer = gst_sample_get_buffer(sample.get());
    if (UNLIKELY(!GST_IS_BUFFER(buffer)))
        return nullptr;

    auto* caps = gst_sample_get_caps(sample.get());

    GstVideoInfo videoInfo;
    gst_video_info_init(&videoInfo);
    if (!gst_video_info_from_caps(&videoInfo, caps))
        return nullptr;

    GST_DEBUG("Converting video frame from %" GST_PTR_FORMAT " to %" GST_PTR_FORMAT, caps, outputCaps.get());

    auto colorConvert = [&]() -> GRefPtr<GstBuffer> {
        GstVideoInfo outputInfo;
        gst_video_info_init(&outputInfo);
        if (!gst_video_info_from_caps(&outputInfo, outputCaps.get()))
            return nullptr;

        if (!m_colorConvertOutputCaps || !gst_caps_is_equal(m_colorConvertOutputCaps.get(), outputCaps.get())
            || !m_colorConvertInputCaps || !gst_caps_is_equal(m_colorConvertInputCaps.get(), caps)) {
            m_colorConvert.reset(gst_video_converter_new(&videoInfo, &outputInfo, nullptr));
            m_colorConvertInputCaps = adoptGRef(gst_caps_copy(caps));
            m_colorConvertOutputCaps = outputCaps;
        }

        auto outputBuffer = adoptGRef(gst_buffer_new_allocate(nullptr, GST_VIDEO_INFO_SIZE(&outputInfo), nullptr));
        GstMappedFrame inputFrame(buffer, videoInfo, GST_MAP_READ);
        GstMappedFrame outputFrame(outputBuffer.get(), outputInfo, GST_MAP_WRITE);
        gst_video_converter_frame(m_colorConvert.get(), inputFrame.get(), outputFrame.get());
        return outputBuffer;
    };

    GRefPtr<GstBuffer> outputBuffer;
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
            outputBuffer = adoptGRef(gst_gl_color_convert_perform(m_glColorConvert.get(), buffer));
        } else
            outputBuffer = colorConvert();
    } else
#endif
        outputBuffer = colorConvert();

    if (UNLIKELY(!GST_IS_BUFFER(outputBuffer.get())))
        return nullptr;

    const auto* info = gst_sample_get_info(sample.get());
    auto convertedSample = adoptGRef(gst_sample_new(outputBuffer.get(), m_colorConvertOutputCaps.get(),
        gst_sample_get_segment(sample.get()), info ? gst_structure_copy(info) : nullptr));
    return convertedSample;
}

RefPtr<ImageGStreamer> GstSampleColorConverter::convertSampleToImage(const GRefPtr<GstSample>& sample)
{
    GstVideoInfo videoInfo;
    gst_video_info_init(&videoInfo);
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

static inline void setBufferFields(GstBuffer* buffer, const MediaTime& presentationTime, double frameRate)
{
    GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_LIVE);
    GST_BUFFER_DTS(buffer) = GST_BUFFER_PTS(buffer) = toGstClockTime(presentationTime);
    GST_BUFFER_DURATION(buffer) = toGstClockTime(1_s / frameRate);
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

VideoFrameGStreamer::VideoFrameGStreamer(GRefPtr<GstSample>&& sample, const FloatSize& presentationSize, const MediaTime& presentationTime, Rotation videoRotation, bool videoMirrored, std::optional<VideoFrameTimeMetadata>&& metadata)
    : VideoFrame(presentationTime, videoMirrored, videoRotation)
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

VideoFrameGStreamer::VideoFrameGStreamer(const GRefPtr<GstSample>& sample, const MediaTime& presentationTime, Rotation videoRotation)
    : VideoFrame(presentationTime, false, videoRotation)
    , m_sample(sample)
{
    ensureDebugCategoryInitialized();
}

void VideoFrame::paintInContext(GraphicsContext& context, const FloatRect& destination, const ImageOrientation& destinationImageOrientation, bool shouldDiscardAlpha)
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

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
