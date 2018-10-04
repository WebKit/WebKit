/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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

#if !PLATFORM(MAC) && !PLATFORM(IOS) && !(USE(GSTREAMER) && USE(LIBWEBRTC))
CaptureSourceOrError MockRealtimeAudioSource::create(const String& deviceID, const String& name, const MediaConstraints* constraints)
{
    auto device = MockRealtimeMediaSourceCenter::mockDeviceWithPersistentID(deviceID);
    ASSERT(device);
    if (!device)
        return { };

    auto source = adoptRef(*new MockRealtimeAudioSource(WTFMove(device), deviceID, name));
    if (constraints && source->applyConstraints(*constraints))
        return { };

    return CaptureSourceOrError(WTFMove(source));
}
#endif

MockRealtimeAudioSource::MockRealtimeAudioSource(const String& deviceID, const String& name)
    : RealtimeMediaSource(deviceID, RealtimeMediaSource::Type::Audio, name)
    , m_timer(RunLoop::current(), this, &MockRealtimeAudioSource::tick)
{
    auto device = MockRealtimeMediaSourceCenter::mockDeviceWithPersistentID(deviceID);
    ASSERT(device);
    m_device = *device;
}

MockRealtimeAudioSource::~MockRealtimeAudioSource()
{
#if PLATFORM(IOS)
    RealtimeMediaSourceCenter::singleton().audioFactory().unsetActiveSource(*this);
#endif
}

const RealtimeMediaSourceSettings& MockRealtimeAudioSource::settings()
{
    if (!m_currentSettings) {
        RealtimeMediaSourceSettings settings;
        settings.setDeviceId(id());
        settings.setVolume(volume());
        settings.setEchoCancellation(echoCancellation());
        settings.setSampleRate(sampleRate());

        RealtimeMediaSourceSupportedConstraints supportedConstraints;
        supportedConstraints.setSupportsDeviceId(true);
        supportedConstraints.setSupportsVolume(true);
        supportedConstraints.setSupportsEchoCancellation(true);
        supportedConstraints.setSupportsSampleRate(true);
        settings.setSupportedConstraints(supportedConstraints);

        m_currentSettings = WTFMove(settings);
    }
    return m_currentSettings.value();
}

const RealtimeMediaSourceCapabilities& MockRealtimeAudioSource::capabilities()
{
    if (!m_capabilities) {
        RealtimeMediaSourceCapabilities capabilities(settings().supportedConstraints());

        capabilities.setDeviceId(id());
        capabilities.setVolume(CapabilityValueOrRange(0.0, 1.0));
        capabilities.setEchoCancellation(RealtimeMediaSourceCapabilities::EchoCancellation::ReadWrite);
        capabilities.setSampleRate(CapabilityValueOrRange(44100, 48000));

        m_capabilities = WTFMove(capabilities);
    }
    return m_capabilities.value();
}

void MockRealtimeAudioSource::settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag>)
{
    m_currentSettings = std::nullopt;
}

void MockRealtimeAudioSource::startProducingData()
{
#if PLATFORM(IOS)
    RealtimeMediaSourceCenter::singleton().audioFactory().setActiveSource(*this);
#endif

    if (!sampleRate())
        setSampleRate(WTF::get<MockMicrophoneProperties>(m_device.properties).defaultSampleRate);

    m_startTime = MonotonicTime::now();
    m_timer.startRepeating(renderInterval());
}

void MockRealtimeAudioSource::stopProducingData()
{
    m_timer.stop();
    m_startTime = MonotonicTime::nan();
}

void MockRealtimeAudioSource::tick()
{
    if (std::isnan(m_lastRenderTime))
        m_lastRenderTime = MonotonicTime::now();

    MonotonicTime now = MonotonicTime::now();

    if (m_delayUntil) {
        if (m_delayUntil < now)
            return;
        m_delayUntil = MonotonicTime();
    }

    Seconds delta = now - m_lastRenderTime;
    m_lastRenderTime = now;
    render(delta);
}

void MockRealtimeAudioSource::delaySamples(Seconds delta)
{
    m_delayUntil = MonotonicTime::now() + delta;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
