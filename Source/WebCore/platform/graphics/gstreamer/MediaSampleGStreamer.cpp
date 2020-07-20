/*
 * Copyright (C) 2016 Metrological Group B.V.
 * Copyright (C) 2016, 2017, 2018 Igalia S.L
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
#include "MediaSampleGStreamer.h"

#include "GStreamerCommon.h"

#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/TypedArrayInlines.h>
#include <algorithm>

#if ENABLE(VIDEO) && USE(GSTREAMER)

namespace WebCore {

MediaSampleGStreamer::MediaSampleGStreamer(GRefPtr<GstSample>&& sample, const FloatSize& presentationSize, const AtomString& trackId)
    : m_pts(MediaTime::zeroTime())
    , m_dts(MediaTime::zeroTime())
    , m_duration(MediaTime::zeroTime())
    , m_trackId(trackId)
    , m_presentationSize(presentationSize)
{
    const GstClockTime minimumDuration = 1000; // 1 us
    ASSERT(sample);
    GstBuffer* buffer = gst_sample_get_buffer(sample.get());
    RELEASE_ASSERT(buffer);

    auto createMediaTime =
        [](GstClockTime time) -> MediaTime {
            return MediaTime(GST_TIME_AS_USECONDS(time), G_USEC_PER_SEC);
        };

    if (GST_BUFFER_PTS_IS_VALID(buffer))
        m_pts = createMediaTime(GST_BUFFER_PTS(buffer));
    if (GST_BUFFER_DTS_IS_VALID(buffer) || GST_BUFFER_PTS_IS_VALID(buffer))
        m_dts = createMediaTime(GST_BUFFER_DTS_OR_PTS(buffer));
    if (GST_BUFFER_DURATION_IS_VALID(buffer)) {
        // Sometimes (albeit rarely, so far seen only at the end of a track)
        // frames have very small durations, so small that may be under the
        // precision we are working with and be truncated to zero.
        // SourceBuffer algorithms are not expecting frames with zero-duration,
        // so let's use something very small instead in those fringe cases.
        m_duration = createMediaTime(std::max(GST_BUFFER_DURATION(buffer), minimumDuration));
    } else {
        // Unfortunately, sometimes samples don't provide a duration. This can never happen in MP4 because of the way
        // the format is laid out, but it's pretty common in WebM.
        // The good part is that durations don't matter for playback, just for buffered ranges and coded frame deletion.
        // We want to pick something small enough to not cause unwanted frame deletion, but big enough to never be
        // mistaken for a rounding artifact.
        m_duration = createMediaTime(16666667); // 1/60 seconds
    }

    m_size = gst_buffer_get_size(buffer);
    m_sample = sample;

    if (GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT))
        m_flags = MediaSample::None;

    if (GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DECODE_ONLY))
        m_flags = static_cast<MediaSample::SampleFlags>(m_flags | MediaSample::IsNonDisplaying);
}

MediaSampleGStreamer::MediaSampleGStreamer(const FloatSize& presentationSize, const AtomString& trackId)
    : m_pts(MediaTime::zeroTime())
    , m_dts(MediaTime::zeroTime())
    , m_duration(MediaTime::zeroTime())
    , m_trackId(trackId)
    , m_presentationSize(presentationSize)
{
}

Ref<MediaSampleGStreamer> MediaSampleGStreamer::createFakeSample(GstCaps*, MediaTime pts, MediaTime dts, MediaTime duration, const FloatSize& presentationSize, const AtomString& trackId)
{
    MediaSampleGStreamer* gstreamerMediaSample = new MediaSampleGStreamer(presentationSize, trackId);
    gstreamerMediaSample->m_pts = pts;
    gstreamerMediaSample->m_dts = dts;
    gstreamerMediaSample->m_duration = duration;
    gstreamerMediaSample->m_flags = MediaSample::IsNonDisplaying;
    return adoptRef(*gstreamerMediaSample);
}

Ref<MediaSampleGStreamer> MediaSampleGStreamer::createImageSample(Vector<uint8_t>&& bgraData, unsigned width, unsigned height)
{
    size_t size = bgraData.sizeInBytes();
    auto buffer = adoptGRef(gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY, bgraData.data(), size, 0, size, bgraData.releaseBuffer().leakPtr(), [](gpointer data) {
        WTF::VectorMalloc::free(data);
    }));
    gst_buffer_add_video_meta(buffer.get(), GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_FORMAT_BGRA, width, height);
    auto caps = adoptGRef(gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "BGRA", "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, "framerate", GST_TYPE_FRACTION, 1, 1, nullptr));
    auto sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
    return create(WTFMove(sample), FloatSize(width, height), { });
}

RefPtr<JSC::Uint8ClampedArray> MediaSampleGStreamer::getRGBAImageData() const
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

void MediaSampleGStreamer::extendToTheBeginning()
{
    // Only to be used with the first sample, as a hack for lack of support for edit lists.
    // See AppendPipeline::appsinkNewSample()
    ASSERT(m_dts == MediaTime::zeroTime());
    m_duration += m_pts;
    m_pts = MediaTime::zeroTime();
}

void MediaSampleGStreamer::offsetTimestampsBy(const MediaTime& timestampOffset)
{
    if (!timestampOffset)
        return;
    m_pts += timestampOffset;
    m_dts += timestampOffset;
    if (auto* buffer = gst_sample_get_buffer(m_sample.get())) {
        GST_BUFFER_PTS(buffer) = toGstClockTime(m_pts);
        GST_BUFFER_DTS(buffer) = toGstClockTime(m_dts);
    }
}

PlatformSample MediaSampleGStreamer::platformSample()
{
    PlatformSample sample = { PlatformSample::GStreamerSampleType, { .gstSample = m_sample.get() } };
    return sample;
}

Ref<MediaSample> MediaSampleGStreamer::createNonDisplayingCopy() const
{
    if (!m_sample)
        return createFakeSample(nullptr, m_pts, m_dts, m_duration, m_presentationSize, m_trackId);

    GstBuffer* buffer = gst_sample_get_buffer(m_sample.get());
    GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_DECODE_ONLY);

    GstCaps* caps = gst_sample_get_caps(m_sample.get());
    GstSegment* segment = gst_sample_get_segment(m_sample.get());
    const GstStructure* originalInfo = gst_sample_get_info(m_sample.get());
    GstStructure* info = originalInfo ? gst_structure_copy(originalInfo) : nullptr;
    GRefPtr<GstSample> sample = adoptGRef(gst_sample_new(buffer, caps, segment, info));

    return adoptRef(*new MediaSampleGStreamer(sample.get(), m_presentationSize, m_trackId));
}

void MediaSampleGStreamer::dump(PrintStream& out) const
{
    out.print("{PTS(", presentationTime(), "), DTS(", decodeTime(), "), duration(", duration(), "), flags(");

    bool anyFlags = false;
    auto appendFlag = [&out, &anyFlags](const char* flagName) {
        if (anyFlags)
            out.print(",");
        out.print(flagName);
        anyFlags = true;
    };

    if (flags() & MediaSample::IsSync)
        appendFlag("sync");
    if (flags() & MediaSample::IsNonDisplaying)
        appendFlag("non-displaying");
    if (flags() & MediaSample::HasAlpha)
        appendFlag("has-alpha");
    if (flags() & ~(MediaSample::IsSync | MediaSample::IsNonDisplaying | MediaSample::HasAlpha))
        appendFlag("unknown-flag");

    out.print("), trackId(", trackID().string(), "), presentationSize(", presentationSize().width(), "x", presentationSize().height(), ")}");
}

} // namespace WebCore.

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
