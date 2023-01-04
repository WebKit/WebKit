/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CoreAudioCaptureSource.h"

#if ENABLE(MEDIA_STREAM)

#include "AudioSampleBufferList.h"
#include "AudioSampleDataSource.h"
#include "AudioSession.h"
#include "CoreAudioCaptureDevice.h"
#include "CoreAudioCaptureDeviceManager.h"
#include "CoreAudioSharedUnit.h"
#include "Logging.h"
#include "PlatformMediaSessionManager.h"
#include "Timer.h"
#include "WebAudioSourceProviderCocoa.h"
#include <AudioToolbox/AudioConverter.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreMedia/CMSync.h>
#include <mach/mach_time.h>
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <pal/spi/cf/CoreAudioSPI.h>
#include <sys/time.h>
#include <wtf/Algorithms.h>
#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>

#if PLATFORM(IOS_FAMILY)
#include "AVAudioSessionCaptureDevice.h"
#include "AVAudioSessionCaptureDeviceManager.h"
#include "CoreAudioCaptureSourceIOS.h"
#endif

#include <pal/cf/AudioToolboxSoftLink.h>
#include <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

#if PLATFORM(MAC)
CoreAudioCaptureSourceFactory& CoreAudioCaptureSourceFactory::singleton()
{
    static NeverDestroyed<CoreAudioCaptureSourceFactory> factory;
    return factory.get();
}
#endif

static CaptureSourceOrError initializeCoreAudioCaptureSource(Ref<CoreAudioCaptureSource>&& source, const MediaConstraints* constraints)
{
    if (constraints) {
        if (auto result = source->applyConstraints(*constraints))
            return WTFMove(result->badConstraint);
    }
    return CaptureSourceOrError(WTFMove(source));
}

CaptureSourceOrError CoreAudioCaptureSource::create(String&& deviceID, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, PageIdentifier pageIdentifier)
{
    CoreAudioCaptureSourceFactory::singleton().setOverrideUnit(nullptr);

#if PLATFORM(MAC)
    auto device = CoreAudioCaptureDeviceManager::singleton().coreAudioDeviceWithUID(deviceID);
    if (!device)
        return { "No CoreAudioCaptureSource device"_s };

    auto source = adoptRef(*new CoreAudioCaptureSource(device.value(), device->deviceID(), WTFMove(hashSalts), nullptr, pageIdentifier));
#elif PLATFORM(IOS_FAMILY)
    auto device = AVAudioSessionCaptureDeviceManager::singleton().audioSessionDeviceWithUID(WTFMove(deviceID));
    if (!device)
        return { "No AVAudioSessionCaptureDevice device"_s };

    auto source = adoptRef(*new CoreAudioCaptureSource(device.value(), 0, WTFMove(hashSalts), nullptr, pageIdentifier));
#endif
    return initializeCoreAudioCaptureSource(WTFMove(source), constraints);
}

CaptureSourceOrError CoreAudioCaptureSource::createForTesting(String&& deviceID, AtomString&& label, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, BaseAudioSharedUnit& overrideUnit, PageIdentifier pageIdentifier)
{
    CoreAudioCaptureSourceFactory::singleton().setOverrideUnit(&overrideUnit);

    auto source = adoptRef(*new CoreAudioCaptureSource(CaptureDevice { WTFMove(deviceID), CaptureDevice::DeviceType::Microphone, WTFMove(label) }, 0, WTFMove(hashSalts), &overrideUnit, pageIdentifier));
    return initializeCoreAudioCaptureSource(WTFMove(source), constraints);
}

BaseAudioSharedUnit& CoreAudioCaptureSource::unit()
{
    return m_overrideUnit ? *m_overrideUnit : CoreAudioSharedUnit::singleton();
}

const BaseAudioSharedUnit& CoreAudioCaptureSource::unit() const
{
    return m_overrideUnit ? *m_overrideUnit : CoreAudioSharedUnit::singleton();
}

CoreAudioCaptureSourceFactory::CoreAudioCaptureSourceFactory()
{
    AudioSession::sharedSession().addInterruptionObserver(*this);
}

CoreAudioCaptureSourceFactory::~CoreAudioCaptureSourceFactory()
{
    AudioSession::sharedSession().removeInterruptionObserver(*this);
}

BaseAudioSharedUnit& CoreAudioCaptureSourceFactory::unit()
{
    return m_overrideUnit ? *m_overrideUnit : CoreAudioSharedUnit::singleton();
}

void CoreAudioCaptureSourceFactory::beginInterruption()
{
    ensureOnMainThread([] {
        CoreAudioCaptureSourceFactory::singleton().unit().suspend();
    });
}

void CoreAudioCaptureSourceFactory::endInterruption()
{
    ensureOnMainThread([] {
        CoreAudioCaptureSourceFactory::singleton().unit().resume();
    });
}

void CoreAudioCaptureSourceFactory::scheduleReconfiguration()
{
    ensureOnMainThread([] {
        CoreAudioSharedUnit::singleton().reconfigure();
    });
}

AudioCaptureFactory& CoreAudioCaptureSource::factory()
{
    return CoreAudioCaptureSourceFactory::singleton();
}

CaptureDeviceManager& CoreAudioCaptureSourceFactory::audioCaptureDeviceManager()
{
#if PLATFORM(MAC)
    return CoreAudioCaptureDeviceManager::singleton();
#else
    return AVAudioSessionCaptureDeviceManager::singleton();
#endif
}

const Vector<CaptureDevice>& CoreAudioCaptureSourceFactory::speakerDevices() const
{
#if PLATFORM(MAC)
    return CoreAudioCaptureDeviceManager::singleton().speakerDevices();
#else
    return AVAudioSessionCaptureDeviceManager::singleton().speakerDevices();
#endif
}

void CoreAudioCaptureSourceFactory::devicesChanged(const Vector<CaptureDevice>& devices)
{
    CoreAudioSharedUnit::unit().devicesChanged(devices);
}

void CoreAudioCaptureSourceFactory::registerSpeakerSamplesProducer(CoreAudioSpeakerSamplesProducer& producer)
{
    CoreAudioSharedUnit::unit().registerSpeakerSamplesProducer(producer);
}

void CoreAudioCaptureSourceFactory::unregisterSpeakerSamplesProducer(CoreAudioSpeakerSamplesProducer& producer)
{
    CoreAudioSharedUnit::unit().unregisterSpeakerSamplesProducer(producer);
}

bool CoreAudioCaptureSourceFactory::isAudioCaptureUnitRunning()
{
    return CoreAudioSharedUnit::unit().isRunning();
}

void CoreAudioCaptureSourceFactory::whenAudioCaptureUnitIsNotRunning(Function<void()>&& callback)
{
    return CoreAudioSharedUnit::unit().whenAudioCaptureUnitIsNotRunning(WTFMove(callback));
}

bool CoreAudioCaptureSourceFactory::shouldAudioCaptureUnitRenderAudio()
{
    auto& unit = CoreAudioSharedUnit::unit();
#if PLATFORM(IOS_FAMILY)
    return unit.isRunning();
#else
    return unit.isRunning() && unit.isUsingVPIO();
#endif // PLATFORM(IOS_FAMILY)
}

CoreAudioCaptureSource::CoreAudioCaptureSource(const CaptureDevice& device, uint32_t captureDeviceID, MediaDeviceHashSalts&& hashSalts, BaseAudioSharedUnit* overrideUnit, PageIdentifier pageIdentifier)
    : RealtimeMediaSource(device, WTFMove(hashSalts), pageIdentifier)
    , m_captureDeviceID(captureDeviceID)
    , m_overrideUnit(overrideUnit)
{
    auto& unit = this->unit();

    // We ensure that we unsuspend ourselves on the constructor as a capture source
    // is created when getUserMedia grants access which only happens when the process is foregrounded.
    // We also reset unit capture values to default.
    unit.prepareForNewCapture();

    initializeEchoCancellation(unit.enableEchoCancellation());
    initializeSampleRate(unit.sampleRate());
    initializeVolume(unit.volume());
}

void CoreAudioCaptureSource::initializeToStartProducingData()
{
    if (m_isReadyToStart)
        return;

    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER);
    m_isReadyToStart = true;

    auto& unit = this->unit();
    unit.setCaptureDevice(String { persistentID() }, m_captureDeviceID);

    bool shouldReconfigure = echoCancellation() != unit.enableEchoCancellation() || sampleRate() != unit.sampleRate() || volume() != unit.volume();
    unit.setEnableEchoCancellation(echoCancellation());
    unit.setSampleRate(sampleRate());
    unit.setVolume(volume());

    unit.addClient(*this);

    if (shouldReconfigure)
        unit.reconfigure();

    m_currentSettings = std::nullopt;
}

CoreAudioCaptureSource::~CoreAudioCaptureSource()
{
    unit().removeClient(*this);
}

void CoreAudioCaptureSource::startProducingData()
{
    m_canResumeAfterInterruption = true;
    initializeToStartProducingData();
    unit().startProducingData();
    m_currentSettings = { };
}

void CoreAudioCaptureSource::stopProducingData()
{
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER);
    unit().stopProducingData();
}

const RealtimeMediaSourceCapabilities& CoreAudioCaptureSource::capabilities()
{
    if (!m_capabilities) {
        RealtimeMediaSourceCapabilities capabilities(settings().supportedConstraints());
        capabilities.setDeviceId(hashedId());
        capabilities.setEchoCancellation(RealtimeMediaSourceCapabilities::EchoCancellation::ReadWrite);
        capabilities.setVolume(CapabilityValueOrRange(0.0, 1.0));
        capabilities.setSampleRate(unit().sampleRateCapacities());
        m_capabilities = WTFMove(capabilities);
    }
    return m_capabilities.value();
}

const RealtimeMediaSourceSettings& CoreAudioCaptureSource::settings()
{
    if (!m_currentSettings) {
        RealtimeMediaSourceSettings settings;
        settings.setVolume(volume());
        settings.setSampleRate(unit().isRenderingAudio() ? unit().actualSampleRate() : sampleRate());
        settings.setDeviceId(hashedId());
        settings.setLabel(name());
        settings.setEchoCancellation(echoCancellation());

        RealtimeMediaSourceSupportedConstraints supportedConstraints;
        supportedConstraints.setSupportsDeviceId(true);
        supportedConstraints.setSupportsEchoCancellation(true);
        supportedConstraints.setSupportsVolume(true);
        supportedConstraints.setSupportsSampleRate(true);
        settings.setSupportedConstraints(supportedConstraints);

        m_currentSettings = WTFMove(settings);
    }
    return m_currentSettings.value();
}

void CoreAudioCaptureSource::settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag> settings)
{
    if (!m_isReadyToStart) {
        m_currentSettings = std::nullopt;
        return;
    }

    bool shouldReconfigure = false;
    if (settings.contains(RealtimeMediaSourceSettings::Flag::EchoCancellation)) {
        unit().setEnableEchoCancellation(echoCancellation());
        shouldReconfigure = true;
    }
    if (settings.contains(RealtimeMediaSourceSettings::Flag::SampleRate)) {
        unit().setSampleRate(sampleRate());
        shouldReconfigure = true;
    }
    if (shouldReconfigure)
        unit().reconfigure();

    m_currentSettings = std::nullopt;
}

bool CoreAudioCaptureSource::interrupted() const
{
    return unit().isSuspended() ? true : RealtimeMediaSource::interrupted();
}

void CoreAudioCaptureSource::delaySamples(Seconds seconds)
{
    unit().delaySamples(seconds);
}

#if PLATFORM(IOS_FAMILY)
void CoreAudioCaptureSource::setIsInBackground(bool value)
{
    if (isProducingData())
        CoreAudioSharedUnit::unit().setIsInBackground(value);
}
#endif

void CoreAudioCaptureSource::audioUnitWillStart()
{
    forEachObserver([](auto& observer) {
        observer.audioUnitWillStart();
    });
}

void CoreAudioCaptureSource::handleNewCurrentMicrophoneDevice(const CaptureDevice& device)
{
    if (!isProducingData() || persistentID() == device.persistentId())
        return;
    
    RELEASE_LOG_INFO(WebRTC, "CoreAudioCaptureSource switching from '%s' to '%s'", name().string().utf8().data(), device.label().utf8().data());
    
    setName(AtomString { device.label() });
    setPersistentId(device.persistentId());
    
    m_currentSettings = { };
    m_capabilities = { };

    forEachObserver([](auto& observer) {
        observer.sourceConfigurationChanged();
    });
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
