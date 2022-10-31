/*
 * Copyright (C) 2015-2022 Apple Inc. All rights reserved.
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
#include "AudioSession.h"
#include "CaptureDevice.h"
#include "Logging.h"
#include "MediaConstraints.h"
#include "MockRealtimeMediaSourceCenter.h"
#include "NotImplemented.h"
#include "PlatformMediaSessionManager.h"
#include "RealtimeMediaSourceSettings.h"
#include <wtf/UUID.h>

#if PLATFORM(COCOA)
#include "MockAudioSharedUnit.h"
#endif

#if USE(GSTREAMER)
#include "MockRealtimeAudioSourceGStreamer.h"
#endif

namespace WebCore {

#if !PLATFORM(MAC) && !PLATFORM(IOS_FAMILY) && !USE(GSTREAMER)
CaptureSourceOrError MockRealtimeAudioSource::create(String&& deviceID, String&& name, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, PageIdentifier)
{
#ifndef NDEBUG
    auto device = MockRealtimeMediaSourceCenter::mockDeviceWithPersistentID(deviceID);
    ASSERT(device);
    if (!device)
        return { "No mock microphone device"_s };
#endif

    auto source = adoptRef(*new MockRealtimeAudioSource(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalts)));
    if (constraints && source->applyConstraints(*constraints))
        return { };

    return CaptureSourceOrError(WTFMove(source));
}
#endif

MockRealtimeAudioSource::MockRealtimeAudioSource(String&& deviceID, AtomString&& name, MediaDeviceHashSalts&& hashSalts, PageIdentifier pageIdentifier)
    : RealtimeMediaSource(CaptureDevice { WTFMove(deviceID), CaptureDevice::DeviceType::Microphone, WTFMove(name) }, WTFMove(hashSalts), pageIdentifier)
    , m_workQueue(WorkQueue::create("MockRealtimeAudioSource Render Queue"))
    , m_timer(RunLoop::current(), this, &MockRealtimeAudioSource::tick)
{
    auto device = MockRealtimeMediaSourceCenter::mockDeviceWithPersistentID(persistentID());
    ASSERT(device);
    m_device = *device;

    setSampleRate(std::get<MockMicrophoneProperties>(m_device.properties).defaultSampleRate);
    initializeEchoCancellation(true);
}

MockRealtimeAudioSource::~MockRealtimeAudioSource()
{
}

const RealtimeMediaSourceSettings& MockRealtimeAudioSource::settings()
{
    if (!m_currentSettings) {
        RealtimeMediaSourceSettings settings;
        settings.setDeviceId(hashedId());
        settings.setVolume(volume());
        settings.setEchoCancellation(echoCancellation());
        settings.setSampleRate(sampleRate());
        settings.setLabel(AtomString { name() });

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

void MockRealtimeAudioSource::setChannelCount(unsigned channelCount)
{
    if (channelCount > 2)
        return;

    m_channelCount = channelCount;
    settingsDidChange(RealtimeMediaSourceSettings::Flag::SampleRate);
}

const RealtimeMediaSourceCapabilities& MockRealtimeAudioSource::capabilities()
{
    if (!m_capabilities) {
        RealtimeMediaSourceCapabilities capabilities(settings().supportedConstraints());

        capabilities.setDeviceId(hashedId());
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
#if PLATFORM(IOS_FAMILY)
    PlatformMediaSessionManager::sharedManager().sessionCanProduceAudioChanged();
    ASSERT(AudioSession::sharedSession().category() == AudioSession::CategoryType::PlayAndRecord);
#endif

    if (!sampleRate())
        setSampleRate(std::get<MockMicrophoneProperties>(m_device.properties).defaultSampleRate);

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

    m_workQueue->dispatch([this, delta, protectedThis = Ref { *this }] {
        render(delta);
    });
}

void MockRealtimeAudioSource::delaySamples(Seconds delta)
{
    m_delayUntil = MonotonicTime::now() + delta;
}

void MockRealtimeAudioSource::setIsInterrupted(bool isInterrupted)
{
    UNUSED_PARAM(isInterrupted);
#if PLATFORM(COCOA)
    if (isInterrupted)
        MockAudioSharedUnit::singleton().suspend();
    else
        MockAudioSharedUnit::singleton().resume();
#elif USE(GSTREAMER)
    for (auto* source : MockRealtimeAudioSourceGStreamer::allMockRealtimeAudioSources())
        source->setInterruptedForTesting(isInterrupted);
#endif
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
