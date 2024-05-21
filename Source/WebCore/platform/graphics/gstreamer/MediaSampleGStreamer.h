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

#pragma once

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "FloatSize.h"
#include "GStreamerCommon.h"
#include "MediaSample.h"
#include "VideoFrameTimeMetadata.h"

namespace WebCore {

class MediaSampleGStreamer : public MediaSample {
public:
    static Ref<MediaSampleGStreamer> create(GRefPtr<GstSample>&& sample, const FloatSize& presentationSize, TrackID id)
    {
        return adoptRef(*new MediaSampleGStreamer(WTFMove(sample), presentationSize, id));
    }

    static Ref<MediaSampleGStreamer> createFakeSample(GstCaps*, const MediaTime& pts, const MediaTime& dts, const MediaTime& duration, const FloatSize& presentationSize, TrackID);

    void extendToTheBeginning();
    MediaTime presentationTime() const override { return m_pts; }
    MediaTime decodeTime() const override { return m_dts; }
    MediaTime duration() const override { return m_duration; }
    TrackID trackID() const override { return m_trackId; }
    size_t sizeInBytes() const override { return m_size; }
    FloatSize presentationSize() const override { return m_presentationSize; }
    void offsetTimestampsBy(const MediaTime&) override;
    void setTimestamps(const MediaTime&, const MediaTime&) override;
    Ref<MediaSample> createNonDisplayingCopy() const override;
    SampleFlags flags() const override { return m_flags; }
    PlatformSample platformSample() const override;
    PlatformSample::Type platformSampleType() const override { return PlatformSample::GStreamerSampleType; }
    void dump(PrintStream&) const override;

    const GRefPtr<GstSample>& sample() const { return m_sample; }

protected:
    MediaSampleGStreamer(GRefPtr<GstSample>&&, const FloatSize& presentationSize, TrackID);
    virtual ~MediaSampleGStreamer() = default;

private:
    MediaSampleGStreamer(const FloatSize& presentationSize, TrackID);

    MediaTime m_pts;
    MediaTime m_dts;
    MediaTime m_duration;
    TrackID m_trackId;
    size_t m_size { 0 };
    GRefPtr<GstSample> m_sample;
    FloatSize m_presentationSize;
    MediaSample::SampleFlags m_flags { MediaSample::IsSync };
};

} // namespace WebCore.

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
