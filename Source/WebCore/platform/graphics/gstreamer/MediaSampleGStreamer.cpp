/*
 * Copyright (C) 2016 Metrological Group B.V.
 * Copyright (C) 2016, 2017, 2018 Igalia S.L
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include <algorithm>

#if ENABLE(VIDEO) && USE(GSTREAMER)

GST_DEBUG_CATEGORY(webkit_media_sample_debug);
#define GST_CAT_DEFAULT webkit_media_sample_debug

namespace WebCore {

static void ensureMediaSampleDebugCategoryInitialized()
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_media_sample_debug, "webkitmediasample", 0, "WebKit Media Sample");
    });
}

MediaSampleGStreamer::MediaSampleGStreamer(GRefPtr<GstSample>&& sample, const FloatSize& presentationSize, TrackID trackId)
    : m_pts(MediaTime::zeroTime())
    , m_dts(MediaTime::zeroTime())
    , m_duration(MediaTime::zeroTime())
    , m_trackId(trackId)
    , m_presentationSize(presentationSize)
{
    ensureMediaSampleDebugCategoryInitialized();
    ASSERT(sample);
    m_sample = WTF::move(sample);
    const GstClockTime minimumDuration = 1000; // 1 us
    auto* buffer = gst_sample_get_buffer(m_sample.get());
    RELEASE_ASSERT(buffer);

    if (GST_BUFFER_PTS_IS_VALID(buffer))
        m_pts = fromGstClockTime(GST_BUFFER_PTS(buffer));
    if (GST_BUFFER_DTS_IS_VALID(buffer) || GST_BUFFER_PTS_IS_VALID(buffer))
        m_dts = fromGstClockTime(GST_BUFFER_DTS_OR_PTS(buffer));
    if (GST_BUFFER_DURATION_IS_VALID(buffer)) {
        // Sometimes (albeit rarely, so far seen only at the end of a track)
        // frames have very small durations, so small that may be under the
        // precision we are working with and be truncated to zero.
        // SourceBuffer algorithms are not expecting frames with zero-duration,
        // so let's use something very small instead in those fringe cases.
        m_duration = fromGstClockTime(std::max(GST_BUFFER_DURATION(buffer), minimumDuration));
    } else {
        // Unfortunately, sometimes samples don't provide a duration. This can never happen in MP4 because of the way
        // the format is laid out, but it's pretty common in WebM.
        // The good part is that durations don't matter for playback, just for buffered ranges and coded frame deletion.
        // We want to pick something small enough to not cause unwanted frame deletion, but big enough to never be
        // mistaken for a rounding artifact.
        m_duration = fromGstClockTime(16666667); // 1/60 seconds
    }

    m_size = gst_buffer_get_size(buffer);

    if (GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT))
        m_flags = MediaSample::None;

    if (GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DECODE_ONLY))
        m_flags = static_cast<MediaSample::SampleFlags>(m_flags | MediaSample::IsNonDisplaying);
}

MediaSampleGStreamer::MediaSampleGStreamer(const FloatSize& presentationSize, TrackID trackId)
    : m_pts(MediaTime::zeroTime())
    , m_dts(MediaTime::zeroTime())
    , m_duration(MediaTime::zeroTime())
    , m_trackId(trackId)
    , m_presentationSize(presentationSize)
{
    ensureMediaSampleDebugCategoryInitialized();
}

Ref<MediaSampleGStreamer> MediaSampleGStreamer::createFakeSample(GstCaps*, const MediaTime& pts, const MediaTime& dts, const MediaTime& duration, const FloatSize& presentationSize, TrackID trackId)
{
    MediaSampleGStreamer* gstreamerMediaSample = new MediaSampleGStreamer(presentationSize, trackId);
    gstreamerMediaSample->m_pts = pts;
    gstreamerMediaSample->m_dts = dts;
    gstreamerMediaSample->m_duration = duration;
    gstreamerMediaSample->m_flags = MediaSample::IsNonDisplaying;
    return adoptRef(*gstreamerMediaSample);
}

void MediaSampleGStreamer::extendToTheBeginning()
{
    GST_TRACE("Extending to beginning");
    // Only to be used with the first sample, as a hack for lack of support for edit lists.
    // See AppendPipeline::appsinkNewSample()
    ASSERT(m_dts == MediaTime::zeroTime());
    m_duration += m_pts;
    m_pts = MediaTime::zeroTime();
}

void MediaSampleGStreamer::updateSampleTimestamps([[maybe_unused]] const String& debugMessage)
{
    RELEASE_ASSERT(m_sample);
    GRefPtr buffer = gst_sample_get_buffer(m_sample.get());
    RELEASE_ASSERT(buffer);

    GstClockTime newPts = 0;
    if (m_pts >= MediaTime::zeroTime())
        newPts = toGstClockTime(m_pts);
    else
        GST_WARNING("New PTS is negative: %s", m_pts.toString().ascii().data());

    GstClockTime newDts = 0;
    if (m_dts >= MediaTime::zeroTime())
        newDts = toGstClockTime(m_dts);
    else
        GST_WARNING("New DTS is negative: %s", m_dts.toString().ascii().data());

    GST_TRACE("%s, new PTS: %" GST_TIME_FORMAT " (prev: %" GST_TIME_FORMAT "), new DTS: %" GST_TIME_FORMAT " (prev: %" GST_TIME_FORMAT ")", debugMessage.ascii().data(),
        GST_TIME_ARGS(newPts), GST_TIME_ARGS(GST_BUFFER_PTS(buffer.get())),
        GST_TIME_ARGS(newDts), GST_TIME_ARGS(GST_BUFFER_DTS(buffer.get())));

    GRefPtr writableBuffer = adoptGRef(gst_buffer_make_writable(buffer.leakRef()));
    GST_BUFFER_PTS(writableBuffer.get()) = newPts;
    GST_BUFFER_DTS(writableBuffer.get()) = newDts;
    m_sample = adoptGRef(gst_sample_make_writable(m_sample.leakRef()));
    gst_sample_set_buffer(m_sample.get(), writableBuffer.get());
}

void MediaSampleGStreamer::setTimestamps(const MediaTime& presentationTime, const MediaTime& decodeTime)
{
    m_pts = presentationTime;
    m_dts = decodeTime;

    if (!m_sample)
        return;

    updateSampleTimestamps("Setting timestamps"_s);
}

void MediaSampleGStreamer::offsetTimestampsBy(const MediaTime& timestampOffset)
{
    if (!timestampOffset)
        return;

    m_pts += timestampOffset;
    m_dts += timestampOffset;

    if (!m_sample)
        return;

    auto debugMessage = emptyString();
#ifndef GST_DISABLE_GST_DEBUG
    if (gst_debug_category_get_threshold(GST_CAT_DEFAULT) >= GST_LEVEL_TRACE)
        debugMessage = makeString("Offsetting timestamps by "_s, timestampOffset.toString());
#endif
    updateSampleTimestamps(debugMessage);
}

PlatformSample MediaSampleGStreamer::platformSample() const
{
    return PlatformSample { m_sample.get() };
}

Ref<MediaSample> MediaSampleGStreamer::createNonDisplayingCopy() const
{
    if (!m_sample)
        return createFakeSample(nullptr, m_pts, m_dts, m_duration, m_presentationSize, m_trackId);

    GRefPtr sample = adoptGRef(gst_sample_copy(m_sample.get()));

    GRefPtr buffer = gst_sample_get_buffer(sample.get());
    RELEASE_ASSERT(buffer);
    GRefPtr writableBuffer = adoptGRef(gst_buffer_make_writable(buffer.leakRef()));

    GST_BUFFER_FLAG_SET(writableBuffer.get(), GST_BUFFER_FLAG_DECODE_ONLY);
    sample = adoptGRef(gst_sample_make_writable(sample.leakRef()));
    gst_sample_set_buffer(sample.get(), writableBuffer.get());

    return adoptRef(*new MediaSampleGStreamer(WTF::move(sample), m_presentationSize, m_trackId));
}

Ref<MediaSample> MediaSampleGStreamer::createCopyWithAdjustedStartTime(const MediaTime& offset) const
{
    MediaTime clampedOffset = std::max(MediaTime::zeroTime(), std::min(offset, m_duration));

    MediaTime newPresentationTime = m_pts + clampedOffset;
    MediaTime newDecodeTime = m_dts + clampedOffset;
    MediaTime adjustedDuration = m_duration - clampedOffset;

    if (!m_sample)
        return createFakeSample(nullptr, newPresentationTime, newDecodeTime, adjustedDuration, m_presentationSize, m_trackId);

    GRefPtr<GstSample> newSample = adoptGRef(gst_sample_copy(m_sample.get()));

    GRefPtr buffer = gst_sample_get_buffer(newSample.get());
    if (!buffer) [[unlikely]]
        return createFakeSample(nullptr, newPresentationTime, newDecodeTime, adjustedDuration, m_presentationSize, m_trackId);

    GRefPtr newBuffer = adoptGRef(gst_buffer_make_writable(buffer.leakRef()));
    GST_BUFFER_PTS(newBuffer.get()) = toGstClockTime(newPresentationTime);
    GST_BUFFER_DTS(newBuffer.get()) = toGstClockTime(newDecodeTime);
    GST_BUFFER_DURATION(newBuffer.get()) = toGstClockTime(adjustedDuration);
    newSample = adoptGRef(gst_sample_make_writable(newSample.leakRef()));
    gst_sample_set_buffer(newSample.get(), newBuffer.get());

    Ref copy = adoptRef(*new MediaSampleGStreamer(WTF::move(newSample), m_presentationSize, m_trackId));
    copy->m_flags = m_flags;
    return copy;
}

void MediaSampleGStreamer::dump(PrintStream& out) const
{
    out.print("{PTS(", presentationTime(), "), DTS(", decodeTime(), "), duration(", duration(), "), flags(");

    bool anyFlags = false;
    auto appendFlag = [&out, &anyFlags](ASCIILiteral flagName) {
        if (anyFlags)
            out.print(",");
        out.print(flagName);
        anyFlags = true;
    };

    if (flags() & MediaSample::IsSync)
        appendFlag("sync"_s);
    if (flags() & MediaSample::IsNonDisplaying)
        appendFlag("non-displaying"_s);
    if (flags() & MediaSample::HasAlpha)
        appendFlag("has-alpha"_s);
    if (flags() & ~(MediaSample::IsSync | MediaSample::IsNonDisplaying | MediaSample::HasAlpha))
        appendFlag("unknown-flag"_s);

    out.print("), trackId(", trackID(), "), presentationSize(", presentationSize().width(), "x", presentationSize().height(), ")}");
}

} // namespace WebCore.

#undef GST_CAT_DEFAULT

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
