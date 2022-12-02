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

#include "config.h"

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
#include "MockRealtimeAudioSourceGStreamer.h"

#include "MockRealtimeMediaSourceCenter.h"

namespace WebCore {

static const double s_Tau = 2 * M_PI;
static const double s_BipBopDuration = 0.07;
static const double s_BipBopVolume = 0.5;
static const double s_BipFrequency = 1500;
static const double s_BopFrequency = 500;
static const double s_HumFrequency = 150;
static const double s_HumVolume = 0.1;
static const double s_NoiseFrequency = 3000;
static const double s_NoiseVolume = 0.05;

static HashSet<MockRealtimeAudioSource*>& allMockRealtimeAudioSourcesStorage()
{
    static MainThreadNeverDestroyed<HashSet<MockRealtimeAudioSource*>> audioSources;
    return audioSources;
}

const HashSet<MockRealtimeAudioSource*>& MockRealtimeAudioSourceGStreamer::allMockRealtimeAudioSources()
{
    return allMockRealtimeAudioSourcesStorage();
}

CaptureSourceOrError MockRealtimeAudioSource::create(String&& deviceID, AtomString&& name, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, PageIdentifier)
{
#ifndef NDEBUG
    auto device = MockRealtimeMediaSourceCenter::mockDeviceWithPersistentID(deviceID);
    ASSERT(device);
    if (!device)
        return { "No mock microphone device"_s };
#endif

    auto source = adoptRef(*new MockRealtimeAudioSourceGStreamer(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalts)));
    if (constraints) {
        if (auto error = source->applyConstraints(*constraints))
            return WTFMove(error->message);
    }

    return CaptureSourceOrError(WTFMove(source));
}

Ref<MockRealtimeAudioSource> MockRealtimeAudioSourceGStreamer::createForMockAudioCapturer(String&& deviceID, AtomString&& name, MediaDeviceHashSalts&& hashSalts)
{
    return adoptRef(*new MockRealtimeAudioSourceGStreamer(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalts)));
}

MockRealtimeAudioSourceGStreamer::MockRealtimeAudioSourceGStreamer(String&& deviceID, AtomString&& name, MediaDeviceHashSalts&& hashSalts)
    : MockRealtimeAudioSource(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalts), { })
{
    ensureGStreamerInitialized();
    allMockRealtimeAudioSourcesStorage().add(this);
}

MockRealtimeAudioSourceGStreamer::~MockRealtimeAudioSourceGStreamer()
{
    allMockRealtimeAudioSourcesStorage().remove(this);
}

void MockRealtimeAudioSourceGStreamer::render(Seconds delta)
{
    if (!m_bipBopBuffer.size())
        reconfigure();

    uint32_t totalFrameCount = GST_ROUND_UP_16(static_cast<size_t>(delta.seconds() * sampleRate()));
    uint32_t frameCount = std::min(totalFrameCount, m_maximiumFrameCount);

    while (frameCount) {
        uint32_t bipBopStart = m_samplesRendered % m_bipBopBuffer.size();
        uint32_t bipBopRemain = m_bipBopBuffer.size() - bipBopStart;
        uint32_t bipBopCount = std::min(frameCount, bipBopRemain);

        ASSERT(m_streamFormat);
        const auto& info = m_streamFormat->getInfo();
        GRefPtr<GstBuffer> buffer = adoptGRef(gst_buffer_new_allocate(nullptr, bipBopCount * m_streamFormat->bytesPerFrame(), nullptr));
        {
            GstMappedBuffer map(buffer.get(), GST_MAP_WRITE);

            if (muted())
                webkitGstAudioFormatFillSilence(info.finfo, map.data(), map.size());
            else {
                memcpy(map.data(), &m_bipBopBuffer[bipBopStart], sizeof(float) * bipBopCount);
                addHum(s_HumVolume, s_HumFrequency, sampleRate(), m_samplesRendered, reinterpret_cast<float*>(map.data()), bipBopCount);
            }
        }

        m_samplesRendered += bipBopCount;
        totalFrameCount -= bipBopCount;
        frameCount = std::min(totalFrameCount, m_maximiumFrameCount);

        MediaTime presentationTime((m_samplesRendered * G_USEC_PER_SEC) / sampleRate(), G_USEC_PER_SEC);
        GST_BUFFER_PTS(buffer.get()) = toGstClockTime(presentationTime);
        GST_BUFFER_FLAG_SET(buffer.get(), GST_BUFFER_FLAG_LIVE);

        auto caps = adoptGRef(gst_audio_info_to_caps(&info));
        auto sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
        GStreamerAudioData data(WTFMove(sample), info);
        audioSamplesAvailable(presentationTime, data, *m_streamFormat, bipBopCount);
    }
}

void MockRealtimeAudioSourceGStreamer::addHum(float amplitude, float frequency, float sampleRate, uint64_t start, float *p, uint64_t count)
{
    float humPeriod = sampleRate / frequency;
    for (uint64_t i = start, end = start + count; i < end; ++i) {
        float a = amplitude * sin(i * s_Tau / humPeriod);
        a += *p;
        *p++ = a;
    }
}

void MockRealtimeAudioSourceGStreamer::reconfigure()
{
    GstAudioInfo info;
    auto rate = sampleRate();
    size_t sampleCount = 2 * rate;

    m_maximiumFrameCount = WTF::roundUpToPowerOfTwo(renderInterval().seconds() * sampleRate());
    gst_audio_info_set_format(&info, GST_AUDIO_FORMAT_F32LE, rate, 1, nullptr);
    m_streamFormat = GStreamerAudioStreamDescription(info);

    m_bipBopBuffer.resize(sampleCount);
    m_bipBopBuffer.fill(0);

    size_t bipBopSampleCount = ceil(s_BipBopDuration * rate);
    size_t bipStart = 0;
    size_t bopStart = rate;

    addHum(s_BipBopVolume, s_BipFrequency, rate, 0, m_bipBopBuffer.data() + bipStart, bipBopSampleCount);
    addHum(s_BipBopVolume, s_BopFrequency, rate, 0, m_bipBopBuffer.data() + bopStart, bipBopSampleCount);
    if (!echoCancellation())
        addHum(s_NoiseVolume, s_NoiseFrequency, rate, 0, m_bipBopBuffer.data(), sampleCount);
}

void MockRealtimeAudioSourceGStreamer::setInterruptedForTesting(bool isInterrupted)
{
    m_isInterrupted = isInterrupted;
    MockRealtimeAudioSource::setInterruptedForTesting(isInterrupted);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
