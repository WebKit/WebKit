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

#include "GStreamerCommon.h"
#include "PixelBuffer.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/TypedArrayInlines.h>

#if ENABLE(VIDEO) && USE(GSTREAMER)

namespace WebCore {

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
    gst_buffer_add_video_meta(buffer.get(), GST_VIDEO_FRAME_FLAG_NONE, format, width, height);

    if (metadata)
        webkitGstBufferSetVideoFrameTimeMetadata(buffer.get(), *metadata);

    int frameRateNumerator, frameRateDenominator;
    gst_util_double_to_fraction(frameRate, &frameRateNumerator, &frameRateDenominator);

    auto caps = adoptGRef(gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, formatName, "width", G_TYPE_INT, width,
        "height", G_TYPE_INT, height, "framerate", GST_TYPE_FRACTION, frameRateNumerator, frameRateDenominator, nullptr));
    auto sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));

    // Optionally resize the video frame to fit destinationSize. This code path is used mostly by
    // the mock realtime video source when the gUM constraints specifically required exact width
    // and/or height values.
    if (!destinationSize.isZero()) {
        GstVideoInfo inputInfo;
        gst_video_info_from_caps(&inputInfo, caps.get());

        width = destinationSize.width();
        height = destinationSize.height();
        auto outputCaps = adoptGRef(gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, formatName, "width", G_TYPE_INT, width,
            "height", G_TYPE_INT, height, "framerate", GST_TYPE_FRACTION, frameRateNumerator, frameRateDenominator, nullptr));
        GstVideoInfo outputInfo;
        gst_video_info_from_caps(&outputInfo, outputCaps.get());

        auto outputBuffer = adoptGRef(gst_buffer_new_allocate(nullptr, GST_VIDEO_INFO_SIZE(&outputInfo), nullptr));
        gst_buffer_add_video_meta(outputBuffer.get(), GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_FORMAT_BGRA, width, height);
        GUniquePtr<GstVideoConverter> converter(gst_video_converter_new(&inputInfo, &outputInfo, nullptr));
        GstMappedFrame inputFrame(gst_sample_get_buffer(sample.get()), inputInfo, GST_MAP_READ);
        GstMappedFrame outputFrame(outputBuffer.get(), outputInfo, GST_MAP_WRITE);
        gst_video_converter_frame(converter.get(), inputFrame.get(), outputFrame.get());
        sample = adoptGRef(gst_sample_new(outputBuffer.get(), outputCaps.get(), nullptr, nullptr));
    }

    return adoptRef(*new VideoFrameGStreamer(WTFMove(sample), FloatSize(width, height), presentationTime, videoRotation, videoMirrored, WTFMove(metadata)));
}

VideoFrameGStreamer::VideoFrameGStreamer(GRefPtr<GstSample>&& sample, const FloatSize& presentationSize, const MediaTime& presentationTime, Rotation videoRotation, bool videoMirrored, std::optional<VideoFrameTimeMetadata>&& metadata)
    : VideoFrame(presentationTime, videoMirrored, videoRotation)
    , m_sample(WTFMove(sample))
    , m_presentationSize(presentationSize)
{
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
    gst_buffer_add_video_meta(outputBuffer.get(), GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_FORMAT_RGBA, width, height);
    GstMappedFrame outputFrame(outputBuffer.get(), outputInfo, GST_MAP_WRITE);

    GUniquePtr<GstVideoConverter> converter(gst_video_converter_new(&inputInfo, &outputInfo, nullptr));
    GstMappedFrame inputFrame(gst_sample_get_buffer(m_sample.get()), inputInfo, GST_MAP_READ);
    gst_video_converter_frame(converter.get(), inputFrame.get(), outputFrame.get());
    return JSC::Uint8ClampedArray::tryCreate(WTFMove(bufferStorage), 0, byteLength);
}

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
