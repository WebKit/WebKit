/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2020 Igalia S.L.
 * Author: Thibault Saunier <tsaunier@igalia.com>
 * Author: Alejandro G. Castro  <alex@igalia.com>
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

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)

#include "GStreamerAudioCapturer.h"
#include "GStreamerAudioData.h"
#include "GStreamerAudioStreamDescription.h"
#include "MockRealtimeAudioSource.h"

namespace WebCore {

class MockRealtimeAudioSourceGStreamer final : public MockRealtimeAudioSource, GStreamerCapturerObserver {
public:
    static Ref<MockRealtimeAudioSource> createForMockAudioCapturer(String&& deviceID, AtomString&& name, MediaDeviceHashSalts&&);

    static const HashSet<MockRealtimeAudioSource*>& allMockRealtimeAudioSources();

    ~MockRealtimeAudioSourceGStreamer();

    // GStreamerCapturerObserver
    void captureEnded() final;

    std::pair<GstClockTime, GstClockTime> queryCaptureLatency() const final;

protected:
    void render(Seconds) final;
    void settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag>) final;

private:
    friend class MockRealtimeAudioSource;
    MockRealtimeAudioSourceGStreamer(String&& deviceID, AtomString&& name, MediaDeviceHashSalts&&);
    void reconfigure();
    void addHum(float amplitude, float frequency, float sampleRate, uint64_t start, float *p, uint64_t count);

    void startProducingData() final;
    void stopProducingData() final;

    bool interrupted() const final { return m_isInterrupted; };
    void setInterruptedForTesting(bool) final;

    std::optional<GStreamerAudioStreamDescription> m_streamFormat;
    GRefPtr<GstCaps> m_caps;
    Vector<float> m_bipBopBuffer;
    uint32_t m_maximiumFrameCount;
    uint64_t m_samplesEmitted { 0 };
    uint64_t m_samplesRendered { 0 };
    bool m_isInterrupted { false };
    RefPtr<GStreamerAudioCapturer> m_capturer;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
