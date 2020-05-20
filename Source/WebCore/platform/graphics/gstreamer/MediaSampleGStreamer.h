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

#pragma once

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "FloatSize.h"
#include "GStreamerCommon.h"
#include "MediaSample.h"
#include <wtf/text/AtomString.h>

namespace WebCore {

class MediaSampleGStreamer : public MediaSample {
public:
    static Ref<MediaSampleGStreamer> create(GRefPtr<GstSample>&& sample, const FloatSize& presentationSize, const AtomString& trackId)
    {
        return adoptRef(*new MediaSampleGStreamer(WTFMove(sample), presentationSize, trackId));
    }

    static Ref<MediaSampleGStreamer> createFakeSample(GstCaps*, MediaTime pts, MediaTime dts, MediaTime duration, const FloatSize& presentationSize, const AtomString& trackId);

    void extendToTheBeginning();
    MediaTime presentationTime() const override { return m_pts; }
    MediaTime decodeTime() const override { return m_dts; }
    MediaTime duration() const override { return m_duration; }
    AtomString trackID() const override { return m_trackId; }
    void setTrackID(const String& trackId) override { m_trackId = trackId; }
    size_t sizeInBytes() const override { return m_size; }
    FloatSize presentationSize() const override { return m_presentationSize; }
    void offsetTimestampsBy(const MediaTime&) override;
    void setTimestamps(const MediaTime&, const MediaTime&) override { }
    bool isDivisable() const override { return false; }
    std::pair<RefPtr<MediaSample>, RefPtr<MediaSample>> divide(const MediaTime&) override  { return { nullptr, nullptr }; }
    Ref<MediaSample> createNonDisplayingCopy() const override;
    SampleFlags flags() const override { return m_flags; }
    PlatformSample platformSample() override;
    void dump(PrintStream&) const override;

protected:
    MediaSampleGStreamer(GRefPtr<GstSample>&&, const FloatSize& presentationSize, const AtomString& trackId);
    virtual ~MediaSampleGStreamer() = default;

private:
    MediaSampleGStreamer(const FloatSize& presentationSize, const AtomString& trackId);

    MediaTime m_pts;
    MediaTime m_dts;
    MediaTime m_duration;
    AtomString m_trackId;
    size_t m_size { 0 };
    GRefPtr<GstSample> m_sample;
    FloatSize m_presentationSize;
    MediaSample::SampleFlags m_flags { MediaSample::IsSync };
};

} // namespace WebCore.

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
