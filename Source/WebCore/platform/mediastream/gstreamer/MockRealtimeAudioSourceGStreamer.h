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

#include "GStreamerAudioData.h"
#include "GStreamerAudioStreamDescription.h"
#include "MockRealtimeAudioSource.h"

namespace WebCore {

class MockRealtimeAudioSourceGStreamer final : public MockRealtimeAudioSource {
public:
    static Ref<MockRealtimeAudioSource> createForMockAudioCapturer(String&& deviceID, AtomString&& name, MediaDeviceHashSalts&&);

    static const HashSet<MockRealtimeAudioSource*>& allMockRealtimeAudioSources();

    ~MockRealtimeAudioSourceGStreamer();

protected:
    void render(Seconds) final;

private:
    friend class MockRealtimeAudioSource;
    MockRealtimeAudioSourceGStreamer(String&& deviceID, AtomString&& name, MediaDeviceHashSalts&&);
    void reconfigure();
    void addHum(float amplitude, float frequency, float sampleRate, uint64_t start, float *p, uint64_t count);

    bool interrupted() const final { return m_isInterrupted; };
    void setInterruptedForTesting(bool) final;

    std::optional<GStreamerAudioStreamDescription> m_streamFormat;
    Vector<float> m_bipBopBuffer;
    uint32_t m_maximiumFrameCount;
    uint64_t m_samplesEmitted { 0 };
    uint64_t m_samplesRendered { 0 };
    bool m_isInterrupted { false };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
