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
#include "VideoSampleMetadata.h"
#include <wtf/text/AtomString.h>

namespace WebCore {

class PixelBuffer;

class MediaSampleGStreamer : public MediaSample {
public:
    static Ref<MediaSampleGStreamer> create(GRefPtr<GstSample>&& sample, const FloatSize& presentationSize, const AtomString& trackId, VideoRotation videoRotation = VideoRotation::None, bool videoMirrored = false, std::optional<VideoSampleMetadata>&& metadata = std::nullopt)
    {
        return adoptRef(*new MediaSampleGStreamer(WTFMove(sample), presentationSize, trackId, videoRotation, videoMirrored, WTFMove(metadata)));
    }

    static Ref<MediaSampleGStreamer> createWrappedSample(const GRefPtr<GstSample>& sample, VideoRotation videoRotation = VideoRotation::None)
    {
        return adoptRef(*new MediaSampleGStreamer(sample, videoRotation));
    }

    static Ref<MediaSampleGStreamer> createFakeSample(GstCaps*, MediaTime pts, MediaTime dts, MediaTime duration, const FloatSize& presentationSize, const AtomString& trackId);
    static Ref<MediaSampleGStreamer> createImageSample(PixelBuffer&&, const IntSize& destinationSize = { }, double frameRate = 1, VideoRotation videoRotation = VideoRotation::None, bool videoMirrored = false, std::optional<VideoSampleMetadata>&& metadata = std::nullopt);

    void extendToTheBeginning();
    MediaTime presentationTime() const override { return m_pts; }
    MediaTime decodeTime() const override { return m_dts; }
    MediaTime duration() const override { return m_duration; }
    AtomString trackID() const override { return m_trackId; }
    size_t sizeInBytes() const override { return m_size; }
    FloatSize presentationSize() const override { return m_presentationSize; }
    void offsetTimestampsBy(const MediaTime&) override;
    void setTimestamps(const MediaTime&, const MediaTime&) override;
    bool isDivisable() const override { return false; }
    std::pair<RefPtr<MediaSample>, RefPtr<MediaSample>> divide(const MediaTime&, UseEndTime) override  { return { nullptr, nullptr }; }
    Ref<MediaSample> createNonDisplayingCopy() const override;
    SampleFlags flags() const override { return m_flags; }
    PlatformSample platformSample() const override;
    PlatformSample::Type platformSampleType() const override { return PlatformSample::GStreamerSampleType; }
    std::optional<ByteRange> byteRange() const override { return std::nullopt; }
    void dump(PrintStream&) const override;
    RefPtr<JSC::Uint8ClampedArray> getRGBAImageData() const final;
    VideoRotation videoRotation() const override { return m_videoRotation; }
    bool videoMirrored() const override { return m_videoMirrored; }

protected:
    MediaSampleGStreamer(GRefPtr<GstSample>&&, const FloatSize& presentationSize, const AtomString& trackId, VideoRotation = VideoRotation::None, bool videoMirrored = false, std::optional<VideoSampleMetadata>&& = std::nullopt);
    MediaSampleGStreamer(const GRefPtr<GstSample>&, VideoRotation = VideoRotation::None);
    virtual ~MediaSampleGStreamer() = default;

private:
    MediaSampleGStreamer(const FloatSize& presentationSize, const AtomString& trackId);

    void initializeFromBuffer();

    MediaTime m_pts;
    MediaTime m_dts;
    MediaTime m_duration;
    AtomString m_trackId;
    size_t m_size { 0 };
    GRefPtr<GstSample> m_sample;
    FloatSize m_presentationSize;
    MediaSample::SampleFlags m_flags { MediaSample::IsSync };
    VideoRotation m_videoRotation { VideoRotation::None };
    bool m_videoMirrored { false };
};

} // namespace WebCore.

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
