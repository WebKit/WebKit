/*
 * Copyright (C) 2016 Metrological Group B.V.
 * Copyright (C) 2016 Igalia S.L
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
#include "GStreamerMediaSample.h"

#include "GStreamerUtilities.h"

#if ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(MEDIA_SOURCE)

namespace WebCore {

GStreamerMediaSample::GStreamerMediaSample(GstSample* sample, const FloatSize& presentationSize, const AtomicString& trackId)
    : MediaSample()
    , m_pts(MediaTime::zeroTime())
    , m_dts(MediaTime::zeroTime())
    , m_duration(MediaTime::zeroTime())
    , m_trackId(trackId)
    , m_size(0)
    , m_presentationSize(presentationSize)
    , m_flags(MediaSample::IsSync)
{

    if (!sample)
        return;

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    if (!buffer)
        return;

    auto createMediaTime =
        [](GstClockTime time) -> MediaTime {
            return MediaTime(GST_TIME_AS_USECONDS(time), G_USEC_PER_SEC);
        };

    if (GST_BUFFER_PTS_IS_VALID(buffer))
        m_pts = createMediaTime(GST_BUFFER_PTS(buffer));
    if (GST_BUFFER_DTS_IS_VALID(buffer))
        m_dts = createMediaTime(GST_BUFFER_DTS(buffer));
    if (GST_BUFFER_DURATION_IS_VALID(buffer))
        m_duration = createMediaTime(GST_BUFFER_DURATION(buffer));

    m_size = gst_buffer_get_size(buffer);
    m_sample = sample;

    if (GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT))
        m_flags = MediaSample::None;

    if (GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DECODE_ONLY))
        m_flags = static_cast<MediaSample::SampleFlags>(m_flags | MediaSample::IsNonDisplaying);
}

Ref<GStreamerMediaSample> GStreamerMediaSample::createFakeSample(GstCaps*, MediaTime pts, MediaTime dts, MediaTime duration, const FloatSize& presentationSize, const AtomicString& trackId)
{
    GStreamerMediaSample* gstreamerMediaSample = new GStreamerMediaSample(nullptr, presentationSize, trackId);
    gstreamerMediaSample->m_pts = pts;
    gstreamerMediaSample->m_dts = dts;
    gstreamerMediaSample->m_duration = duration;
    gstreamerMediaSample->m_flags = MediaSample::IsNonDisplaying;
    return adoptRef(*gstreamerMediaSample);
}

void GStreamerMediaSample::applyPtsOffset(MediaTime timestampOffset)
{
    if (m_pts > timestampOffset) {
        m_duration = m_duration + (m_pts - timestampOffset);
        m_pts = timestampOffset;
    }
}

void GStreamerMediaSample::offsetTimestampsBy(const MediaTime& timestampOffset)
{
    if (!timestampOffset)
        return;
    m_pts += timestampOffset;
    m_dts += timestampOffset;
    GstBuffer* buffer = gst_sample_get_buffer(m_sample.get());
    if (buffer) {
        GST_BUFFER_PTS(buffer) = toGstClockTime(m_pts.toFloat());
        GST_BUFFER_DTS(buffer) = toGstClockTime(m_dts.toFloat());
    }
}

Ref<MediaSample> GStreamerMediaSample::createNonDisplayingCopy() const
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

    return adoptRef(*new GStreamerMediaSample(sample.get(), m_presentationSize, m_trackId));
}

} // namespace WebCore.

#endif // USE(GSTREAMER)
