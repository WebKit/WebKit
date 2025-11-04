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
            return CaptureSourceOrError(CaptureSourceError { result->invalidConstraint });
    }
    return CaptureSourceOrError(WTFMove(source));
}

CaptureSourceOrError CoreAudioCaptureSource::create(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, std::optional<PageIdentifier> pageIdentifier)
{
#if PLATFORM(MAC)
    auto coreAudioDevice = CoreAudioCaptureDeviceManager::singleton().coreAudioDeviceWithUID(device.persistentId());
    if (!coreAudioDevice)
        return CaptureSourceOrError({ "No CoreAudioCaptureSource device"_s, MediaAccessDenialReason::PermissionDenied });

    auto source = adoptRef(*new CoreAudioCaptureSource(device, coreAudioDevice->deviceID(), WTFMove(hashSalts), pageIdentifier));
#elif PLATFORM(IOS_FAMILY)
    auto coreAudioDevice = AVAudioSessionCaptureDeviceManager::singleton().audioSessionDeviceWithUID(device.persistentId());
    if (!coreAudioDevice)
        return CaptureSourceOrError({ "No AVAudioSessionCaptureDevice device"_s, MediaAccessDenialReason::PermissionDenied });

    auto source = adoptRef(*new CoreAudioCaptureSource(device, 0, WTFMove(hashSalts), pageIdentifier));
#endif
    return initializeCoreAudioCaptureSource(WTFMove(source), constraints);
}

CaptureSourceOrError CoreAudioCaptureSource::createForTesting(String&& deviceID, AtomString&& label, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, std::optional<PageIdentifier> pageIdentifier, std::optional<bool> echoCancellation)
{
    auto source = adoptRef(*new CoreAudioCaptureSource(CaptureDevice { WTFMove(deviceID), CaptureDevice::DeviceType::Microphone, WTFMove(label) }, 0, WTFMove(hashSalts), pageIdentifier));
    if (echoCancellation) {
        source->m_echoCancellationCapability = *echoCancellation;
        source->initializeEchoCancellation(*echoCancellation);
    }

    return initializeCoreAudioCaptureSource(WTFMove(source), constraints);
}

CoreAudioCaptureSourceFactory::CoreAudioCaptureSourceFactory()
{
    AudioSession::addInterruptionObserver(*this);
}

CoreAudioCaptureSourceFactory::~CoreAudioCaptureSourceFactory()
{
    AudioSession::removeInterruptionObserver(*this);
}

void CoreAudioCaptureSourceFactory::beginInterruption()
{
    ensureOnMainThread([] {
        CoreAudioSharedUnit::forEach([](auto& unit) {
            unit.suspend();
        });
    });
}

void CoreAudioCaptureSourceFactory::endInterruption()
{
    ensureOnMainThread([] {
        CoreAudioSharedUnit::forEach([](auto& unit) {
            unit.resume();
        });
    });
}

void CoreAudioCaptureSourceFactory::scheduleReconfiguration()
{
    ensureOnMainThread([] {
        CoreAudioSharedUnit::forEach([](auto& unit) {
            unit.reconfigure();
        });
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

void CoreAudioCaptureSourceFactory::enableMutedSpeechActivityEventListener(Function<void()>&& callback)
{
    CoreAudioSharedUnit::defaultSingleton().enableMutedSpeechActivityEventListener(WTFMove(callback));
}

void CoreAudioCaptureSourceFactory::disableMutedSpeechActivityEventListener()
{
    CoreAudioSharedUnit::defaultSingleton().disableMutedSpeechActivityEventListener();
}

void CoreAudioCaptureSourceFactory::registerSpeakerSamplesProducer(CoreAudioSpeakerSamplesProducer& producer)
{
    CoreAudioSharedUnit::defaultSingleton().registerSpeakerSamplesProducer(producer);
}

void CoreAudioCaptureSourceFactory::unregisterSpeakerSamplesProducer(CoreAudioSpeakerSamplesProducer& producer)
{
    CoreAudioSharedUnit::defaultSingleton().unregisterSpeakerSamplesProducer(producer);
}

bool CoreAudioCaptureSourceFactory::shouldAudioCaptureUnitRenderAudio()
{
#if PLATFORM(IOS_FAMILY)
    return CoreAudioSharedUnit::defaultSingleton().isRunning();
#else
    return CoreAudioSharedUnit::defaultSingleton().isRunning() && CoreAudioSharedUnit::defaultSingleton().canRenderAudio();
#endif // PLATFORM(IOS_FAMILY)
}

CoreAudioCaptureSource::CoreAudioCaptureSource(const CaptureDevice& device, uint32_t captureDeviceID, MediaDeviceHashSalts&& hashSalts, std::optional<PageIdentifier> pageIdentifier)
    : RealtimeMediaSource(device, WTFMove(hashSalts), pageIdentifier)
    , m_captureDeviceID(captureDeviceID)
    , m_unit(CoreAudioSharedUnit::defaultSingleton())
{
    // We ensure that we unsuspend ourselves on the constructor as a capture source
    // is created when getUserMedia grants access which only happens when the process is foregrounded.
    // We also reset unit capture values to default.
    Ref unit = m_unit;
    unit->prepareForNewCapture();

    initializeEchoCancellation(unit->enableEchoCancellation());
    initializeSampleRate(unit->sampleRate());
    initializeVolume(unit->volume());
}

Ref<CoreAudioSharedUnit> CoreAudioCaptureSource::protectedUnit()
{
    return m_unit;
}

Ref<const CoreAudioSharedUnit> CoreAudioCaptureSource::protectedUnit() const
{
    return m_unit;
}

void CoreAudioCaptureSource::initializeToStartProducingData()
{
    if (m_isReadyToStart)
        return;

    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, "is Default ", captureDevice().isDefault());
    m_isReadyToStart = true;

    Ref unit = m_unit;
    unit->setCaptureDevice(String { persistentID() }, m_captureDeviceID, captureDevice().isDefault());

    bool shouldReconfigure = echoCancellation() != unit->enableEchoCancellation() || sampleRate() != unit->sampleRate() || volume() != unit->volume();
    unit->setEnableEchoCancellation(echoCancellation());
    unit->setSampleRate(sampleRate());
    unit->setVolume(volume());

    unit->addClient(*this);

    if (shouldReconfigure)
        unit->reconfigure();

    m_currentSettings = std::nullopt;
}

CoreAudioCaptureSource::~CoreAudioCaptureSource()
{
    protectedUnit()->removeClient(*this);
}

void CoreAudioCaptureSource::startProducingData()
{
    m_canResumeAfterInterruption = true;
    initializeToStartProducingData();
    protectedUnit()->startProducingData();
    m_currentSettings = { };
}

void CoreAudioCaptureSource::stopProducingData()
{
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER);
    protectedUnit()->stopProducingData();
}

void CoreAudioCaptureSource::endProducingData()
{
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER);

    Ref unit = m_unit;
    unit->removeClient(*this);
    if (isProducingData())
        unit->stopProducingData();
}

const RealtimeMediaSourceCapabilities& CoreAudioCaptureSource::capabilities()
{
    if (!m_capabilities) {
        RealtimeMediaSourceCapabilities capabilities(settings().supportedConstraints());
        capabilities.setDeviceId(hashedId());
        capabilities.setGroupId(hashedGroupId());
        capabilities.setEchoCancellation(m_echoCancellationCapability ? (*m_echoCancellationCapability ? RealtimeMediaSourceCapabilities::EchoCancellation::On : RealtimeMediaSourceCapabilities::EchoCancellation::Off) : RealtimeMediaSourceCapabilities::EchoCancellation::OnOrOff);
        capabilities.setVolume({ 0.0, 1.0 });
        capabilities.setSampleRate(m_unit->sampleRateCapacities());
        m_capabilities = WTFMove(capabilities);
    }
    return m_capabilities.value();
}

const RealtimeMediaSourceSettings& CoreAudioCaptureSource::settings()
{
    if (!m_currentSettings) {
        Ref unit = m_unit.get();

        RealtimeMediaSourceSettings settings;
        settings.setVolume(volume());
        settings.setSampleRate(unit->isRenderingAudio() ? unit->actualSampleRate() : sampleRate());
        settings.setDeviceId(hashedId());
        settings.setGroupId(hashedGroupId());
        settings.setLabel(name());
        settings.setEchoCancellation(echoCancellation());

        RealtimeMediaSourceSupportedConstraints supportedConstraints;
        supportedConstraints.setSupportsDeviceId(true);
        supportedConstraints.setSupportsGroupId(true);
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
    if (!m_isReadyToStart || m_echoCancellationChanging) {
        m_currentSettings = std::nullopt;
        return;
    }

    bool shouldReconfigure = false;
    if (settings.contains(RealtimeMediaSourceSettings::Flag::EchoCancellation)) {
        protectedUnit()->setEnableEchoCancellation(echoCancellation());
        shouldReconfigure = true;
    }
    if (settings.contains(RealtimeMediaSourceSettings::Flag::SampleRate)) {
        protectedUnit()->setSampleRate(sampleRate());
        shouldReconfigure = true;
    }
    if (shouldReconfigure)
        protectedUnit()->reconfigure();

    m_currentSettings = std::nullopt;
}

bool CoreAudioCaptureSource::interrupted() const
{
    return protectedUnit()->isSuspended() || RealtimeMediaSource::interrupted();
}

void CoreAudioCaptureSource::delaySamples(Seconds seconds)
{
    protectedUnit()->delaySamples(seconds);
}

#if PLATFORM(IOS_FAMILY)
void CoreAudioCaptureSource::setIsInBackground(bool value)
{
    if (isProducingData())
        CoreAudioSharedUnit::setIsInBackground(value);
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
    
    RELEASE_LOG_INFO(WebRTC, "CoreAudioCaptureSource switching from '%s' to '%s'", name().utf8().data(), device.label().utf8().data());
    
    setName(AtomString { device.label() });
    setPersistentId(device.persistentId());
    
    m_currentSettings = { };
    m_capabilities = { };

    forEachObserver([](auto& observer) {
        observer.sourceConfigurationChanged();
    });
}

void CoreAudioCaptureSource::echoCancellationChanged()
{
    if (!isProducingData() || echoCancellation() == m_unit->enableEchoCancellation())
        return;

    m_echoCancellationChanging = true;
    setEchoCancellation(m_unit->enableEchoCancellation());
    m_echoCancellationChanging = false;

    configurationChanged();
}

const AudioStreamDescription* CoreAudioCaptureSource::audioStreamDescription() const
{
    if (!m_unit->microphoneProcFormat())
        return nullptr;
    return &m_unit->microphoneProcFormat().value();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
