/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MockRealtimeAudioSource.h"

#if ENABLE(MEDIA_STREAM)
#include "CaptureDevice.h"
#include "Logging.h"
#include "MediaConstraints.h"
#include "MockRealtimeMediaSourceCenter.h"
#include "NotImplemented.h"
#include "RealtimeMediaSourceSettings.h"
#include <wtf/UUID.h>

namespace WebCore {

class MockRealtimeAudioSourceFactory : public RealtimeMediaSource::AudioCaptureFactory
{
public:
    CaptureSourceOrError createAudioCaptureSource(const CaptureDevice& device, const MediaConstraints* constraints) final
    {
        for (auto& mockDevice : MockRealtimeMediaSource::audioDevices()) {
            if (mockDevice.persistentId() == device.persistentId())
                return MockRealtimeAudioSource::create(mockDevice.persistentId(), mockDevice.label(), constraints);
        }
        return { };
    }
};

#if !PLATFORM(MAC) && !PLATFORM(IOS)
CaptureSourceOrError MockRealtimeAudioSource::create(const String& deviceID, const String& name, const MediaConstraints* constraints)
{
    auto source = adoptRef(*new MockRealtimeAudioSource(deviceID, name));
    if (constraints && source->applyConstraints(*constraints))
        return { };

    return CaptureSourceOrError(WTFMove(source));
}
#endif

static MockRealtimeAudioSourceFactory& mockAudioCaptureSourceFactory()
{
    static NeverDestroyed<MockRealtimeAudioSourceFactory> factory;
    return factory.get();
}

RealtimeMediaSource::AudioCaptureFactory& MockRealtimeAudioSource::factory()
{
    return mockAudioCaptureSourceFactory();
}

MockRealtimeAudioSource::MockRealtimeAudioSource(const String& deviceID, const String& name)
    : MockRealtimeMediaSource(deviceID, RealtimeMediaSource::Type::Audio, name)
    , m_timer(RunLoop::current(), this, &MockRealtimeAudioSource::tick)
{
}

MockRealtimeAudioSource::~MockRealtimeAudioSource()
{
#if PLATFORM(IOS)
    MockRealtimeMediaSourceCenter::audioCaptureSourceFactory().unsetActiveSource(*this);
#endif
}

void MockRealtimeAudioSource::updateSettings(RealtimeMediaSourceSettings& settings)
{
    settings.setVolume(volume());
    settings.setEchoCancellation(echoCancellation());
    settings.setSampleRate(sampleRate());
}

void MockRealtimeAudioSource::initializeCapabilities(RealtimeMediaSourceCapabilities& capabilities)
{
    capabilities.setVolume(CapabilityValueOrRange(0.0, 1.0));
    capabilities.setEchoCancellation(RealtimeMediaSourceCapabilities::EchoCancellation::ReadWrite);
    capabilities.setSampleRate(CapabilityValueOrRange(44100, 48000));
}

void MockRealtimeAudioSource::initializeSupportedConstraints(RealtimeMediaSourceSupportedConstraints& supportedConstraints)
{
    supportedConstraints.setSupportsVolume(true);
    supportedConstraints.setSupportsEchoCancellation(true);
    supportedConstraints.setSupportsSampleRate(true);
}

void MockRealtimeAudioSource::startProducingData()
{
#if PLATFORM(IOS)
    MockRealtimeMediaSourceCenter::audioCaptureSourceFactory().setActiveSource(*this);
#endif

    if (!sampleRate())
        setSampleRate(device() == MockRealtimeMediaSource::MockDevice::Microphone1 ? 44100 : 48000);

    m_startTime = monotonicallyIncreasingTime();
    m_timer.startRepeating(renderInterval());
}

void MockRealtimeAudioSource::stopProducingData()
{
    m_timer.stop();
    m_elapsedTime += monotonicallyIncreasingTime() - m_startTime;
    m_startTime = NAN;
}

double MockRealtimeAudioSource::elapsedTime()
{
    if (std::isnan(m_startTime))
        return m_elapsedTime;

    return m_elapsedTime + (monotonicallyIncreasingTime() - m_startTime);
}

void MockRealtimeAudioSource::tick()
{
    if (std::isnan(m_lastRenderTime))
        m_lastRenderTime = monotonicallyIncreasingTime();

    double now = monotonicallyIncreasingTime();

    if (m_delayUntil) {
        if (m_delayUntil < now)
            return;
        m_delayUntil = 0;
    }

    double delta = now - m_lastRenderTime;
    m_lastRenderTime = now;
    render(delta);
}

void MockRealtimeAudioSource::delaySamples(float delta)
{
    m_delayUntil = monotonicallyIncreasingTime() + delta;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
