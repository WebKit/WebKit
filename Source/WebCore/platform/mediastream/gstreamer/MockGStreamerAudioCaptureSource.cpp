/*
 * Copyright (C) 2018 Metrological Group B.V.
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

#if ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
#include "MockGStreamerAudioCaptureSource.h"

#include "GStreamerAudioStreamDescription.h"
#include "MockRealtimeAudioSource.h"

#include <gst/app/gstappsrc.h>

namespace WebCore {

static const double s_Tau = 2 * M_PI;
static const double s_BipBopDuration = 0.07;
static const double s_BipBopVolume = 0.5;
static const double s_BipFrequency = 1500;
static const double s_BopFrequency = 500;
static const double s_HumFrequency = 150;
static const double s_HumVolume = 0.1;

class WrappedMockRealtimeAudioSource : public MockRealtimeAudioSource {
public:
    WrappedMockRealtimeAudioSource(String&& deviceID, String&& name, String&& hashSalt)
        : MockRealtimeAudioSource(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalt))
        , m_src(nullptr)
    {
    }

    void start(GRefPtr<GstElement> src)
    {
        m_src = src;
        if (m_streamFormat)
            gst_app_src_set_caps(GST_APP_SRC(m_src.get()), m_streamFormat->caps());
        MockRealtimeAudioSource::start();
    }

    void addHum(float amplitude, float frequency, float sampleRate, uint64_t start, float *p, uint64_t count)
    {
        float humPeriod = sampleRate / frequency;
        for (uint64_t i = start, end = start + count; i < end; ++i) {
            float a = amplitude * sin(i * s_Tau / humPeriod);
            a += *p;
            *p++ = a;
        }
    }

    void render(Seconds delta)
    {
        ASSERT(m_src);

        uint32_t totalFrameCount = GST_ROUND_UP_16(static_cast<size_t>(delta.seconds() * sampleRate()));
        uint32_t frameCount = std::min(totalFrameCount, m_maximiumFrameCount);
        while (frameCount) {
            uint32_t bipBopStart = m_samplesRendered % m_bipBopBuffer.size();
            uint32_t bipBopRemain = m_bipBopBuffer.size() - bipBopStart;
            uint32_t bipBopCount = std::min(frameCount, bipBopRemain);

            GstBuffer* buffer = gst_buffer_new_allocate(nullptr, bipBopCount * m_streamFormat->bytesPerFrame(), nullptr);
            {
                GstMappedBuffer map(buffer, GST_MAP_WRITE);

                if (!muted()) {
                    memcpy(map.data(), &m_bipBopBuffer[bipBopStart], sizeof(float) * bipBopCount);
                    addHum(s_HumVolume, s_HumFrequency, sampleRate(), m_samplesRendered, (float*)map.data(), bipBopCount);
                } else
                    memset(map.data(), 0, sizeof(float) * bipBopCount);
            }

            gst_app_src_push_buffer(GST_APP_SRC(m_src.get()), buffer);
            m_samplesRendered += bipBopCount;
            totalFrameCount -= bipBopCount;
            frameCount = std::min(totalFrameCount, m_maximiumFrameCount);
        }
    }

    void settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag> settings)
    {
        if (settings.contains(RealtimeMediaSourceSettings::Flag::SampleRate)) {
            GstAudioInfo info;
            auto rate = sampleRate();
            size_t sampleCount = 2 * rate;

            m_maximiumFrameCount = WTF::roundUpToPowerOfTwo(renderInterval().seconds() * sampleRate());
            gst_audio_info_set_format(&info, GST_AUDIO_FORMAT_F32LE, rate, 1, nullptr);
            m_streamFormat = GStreamerAudioStreamDescription(info);

            if (m_src)
                gst_app_src_set_caps(GST_APP_SRC(m_src.get()), m_streamFormat->caps());

            m_bipBopBuffer.grow(sampleCount);
            m_bipBopBuffer.fill(0);

            size_t bipBopSampleCount = ceil(s_BipBopDuration * rate);
            size_t bipStart = 0;
            size_t bopStart = rate;

            addHum(s_BipBopVolume, s_BipFrequency, rate, 0, static_cast<float*>(m_bipBopBuffer.data() + bipStart), bipBopSampleCount);
            addHum(s_BipBopVolume, s_BopFrequency, rate, 0, static_cast<float*>(m_bipBopBuffer.data() + bopStart), bipBopSampleCount);
        }

        MockRealtimeAudioSource::settingsDidChange(settings);
    }

    GRefPtr<GstElement> m_src;
    std::optional<GStreamerAudioStreamDescription> m_streamFormat;
    Vector<float> m_bipBopBuffer;
    uint32_t m_maximiumFrameCount;
    uint64_t m_samplesEmitted { 0 };
    uint64_t m_samplesRendered { 0 };
};

CaptureSourceOrError MockRealtimeAudioSource::create(String&& deviceID,
    String&& name, String&& hashSalt, const MediaConstraints* constraints)
{
    auto source = adoptRef(*new MockGStreamerAudioCaptureSource(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalt)));

    if (constraints && source->applyConstraints(*constraints))
        return { };

    return CaptureSourceOrError(WTFMove(source));
}

std::optional<std::pair<String, String>> MockGStreamerAudioCaptureSource::applyConstraints(const MediaConstraints& constraints)
{
    m_wrappedSource->applyConstraints(constraints);
    return GStreamerAudioCaptureSource::applyConstraints(constraints);
}

void MockGStreamerAudioCaptureSource::applyConstraints(const MediaConstraints& constraints, SuccessHandler&& successHandler, FailureHandler&& failureHandler)
{
    m_wrappedSource->applyConstraints(constraints, WTFMove(successHandler), WTFMove(failureHandler));
}

MockGStreamerAudioCaptureSource::MockGStreamerAudioCaptureSource(String&& deviceID, String&& name, String&& hashSalt)
    : GStreamerAudioCaptureSource(String { deviceID }, String { name }, String { hashSalt })
    , m_wrappedSource(std::make_unique<WrappedMockRealtimeAudioSource>(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalt)))
{
    m_wrappedSource->addObserver(*this);
}

MockGStreamerAudioCaptureSource::~MockGStreamerAudioCaptureSource()
{
    m_wrappedSource->removeObserver(*this);
}

void MockGStreamerAudioCaptureSource::stopProducingData()
{
    m_wrappedSource->stop();

    GStreamerAudioCaptureSource::stopProducingData();
}

void MockGStreamerAudioCaptureSource::startProducingData()
{
    GStreamerAudioCaptureSource::startProducingData();
    static_cast<WrappedMockRealtimeAudioSource*>(m_wrappedSource.get())->start(capturer()->source());
}

const RealtimeMediaSourceSettings& MockGStreamerAudioCaptureSource::settings()
{
    return m_wrappedSource->settings();
}

const RealtimeMediaSourceCapabilities& MockGStreamerAudioCaptureSource::capabilities()
{
    m_capabilities = m_wrappedSource->capabilities();
    m_currentSettings = m_wrappedSource->settings();
    return m_wrappedSource->capabilities();
}

void MockGStreamerAudioCaptureSource::captureFailed()
{
    stop();
    RealtimeMediaSource::captureFailed();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
