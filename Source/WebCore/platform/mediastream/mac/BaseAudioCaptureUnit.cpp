/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "BaseAudioCaptureUnit.h"

#if ENABLE(MEDIA_STREAM)

#include "AudioSession.h"
#include "CaptureDeviceManager.h"
#include "CoreAudioCaptureSource.h"
#include "DeprecatedGlobalSettings.h"
#include "Logging.h"
#include "PlatformMediaSessionManager.h"
#include <algorithm>

namespace WebCore {

constexpr Seconds voiceActivityThrottlingDuration = 5_s;

BaseAudioCaptureUnit::BaseAudioCaptureUnit(CanEnableEchoCancellation canEnableEchoCancellation)
    : m_canEnableEchoCancellation(canEnableEchoCancellation == CanEnableEchoCancellation::Yes)
    , m_enableEchoCancellation(m_canEnableEchoCancellation)
    , m_sampleRate(AudioSession::singleton().sampleRate())
{
    RealtimeMediaSourceCenter::singleton().addDevicesChangedObserver(*this);
}

BaseAudioCaptureUnit::~BaseAudioCaptureUnit()
{
    RealtimeMediaSourceCenter::singleton().removeDevicesChangedObserver(*this);
}

#if !PLATFORM(MAC)
void BaseAudioCaptureUnit::setEnableEchoCancellation(bool enableEchoCancellation)
{
    ASSERT(m_canEnableEchoCancellation || (!enableEchoCancellation && !m_enableEchoCancellation));
    m_enableEchoCancellation = enableEchoCancellation;
}
#endif

void BaseAudioCaptureUnit::addClient(CoreAudioCaptureSource& client)
{
    ASSERT(isMainThread());
    m_clients.add(client);
    Locker locker { m_audioThreadClientsLock };
    m_audioThreadClients = m_clients.weakValues();
}

void BaseAudioCaptureUnit::removeClient(CoreAudioCaptureSource& client)
{
    ASSERT(isMainThread());
    m_clients.remove(client);
    {
        Locker locker { m_audioThreadClientsLock };
        m_audioThreadClients = m_clients.weakValues();
    }

    if (!shouldContinueRunning())
        stopRunning();
}

void BaseAudioCaptureUnit::clearClients()
{
    ASSERT(isMainThread());
    m_clients.clear();
    Locker locker { m_audioThreadClientsLock };
    m_audioThreadClients.clear();
}

void BaseAudioCaptureUnit::forEachClient(NOESCAPE const Function<void(CoreAudioCaptureSource&)>& apply) const
{
    ASSERT(isMainThread());
    m_clients.forEach(apply);
}

const static OSStatus lowPriorityError1 = 560557684;
const static OSStatus lowPriorityError2 = 561017449;

#if ASSERT_ENABLED
static bool s_isBaseAudioCaptureUnitAllowedToStart = false;

void BaseAudioCaptureUnit::allowStarting()
{
    s_isBaseAudioCaptureUnitAllowedToStart = true;
}
#endif

void BaseAudioCaptureUnit::startProducingData()
{
    ASSERT(isMainThread());
    ASSERT(s_isBaseAudioCaptureUnitAllowedToStart);

    if (m_suspended)
        resume();

    setIsProducingMicrophoneSamples(true);

    if (++m_producingCount != 1)
        return;

#if PLATFORM(MAC)
    prewarmAudioUnitCreation([weakThis = WeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->continueStartProducingData();
    });
#else
    continueStartProducingData();
#endif
}

void BaseAudioCaptureUnit::continueStartProducingData()
{
    if (!m_producingCount || m_clients.isEmptyIgnoringNullReferences())
        return;

    if (isProducingData())
        return;

    if (hasAudioUnit()) {
        cleanupAudioUnit();
        ASSERT(!hasAudioUnit());
    }
    auto error = startUnit();
    if (error) {
        if (error == lowPriorityError1 || error == lowPriorityError2) {
            RELEASE_LOG_ERROR(WebRTC, "BaseAudioCaptureUnit::startProducingData failed due to not high enough priority, suspending unit");
            suspend();
        } else
            captureFailed();
    }
}

OSStatus BaseAudioCaptureUnit::startUnit()
{
    forEachClient([](auto& client) {
        client.audioUnitWillStart();
    });
    ASSERT(!DeprecatedGlobalSettings::shouldManageAudioSessionCategory() || AudioSession::singleton().category() == AudioSession::CategoryType::PlayAndRecord);

#if PLATFORM(IOS_FAMILY)
    if (AudioSession::singleton().category() != AudioSession::CategoryType::PlayAndRecord) {
        RELEASE_LOG_ERROR(WebRTC, "BaseAudioCaptureUnit::startUnit cannot call startInternal if category is not set to PlayAndRecord");
        return lowPriorityError2;
    }
#endif

    return startInternal();
}

void BaseAudioCaptureUnit::prepareForNewCapture()
{
    m_volume = 1;
    resetSampleRate();

    if (!m_suspended)
        return;
    m_suspended = false;

    if (!m_producingCount)
        return;

    RELEASE_LOG_ERROR(WebRTC, "BaseAudioCaptureUnit::prepareForNewCapture, notifying suspended sources of capture failure");
    captureFailed();
}

void BaseAudioCaptureUnit::setCaptureDevice(String&& persistentID, uint32_t captureDeviceID, bool isDefault)
{
    bool hasChanged = this->persistentID() != persistentID || this->captureDeviceID() != captureDeviceID || m_isCapturingWithDefaultMicrophone != isDefault;
    if (hasChanged)
        willChangeCaptureDeviceTo(persistentID);

    m_capturingDevice = { WTF::move(persistentID), captureDeviceID };
    m_isCapturingWithDefaultMicrophone = isDefault;

    if (hasChanged)
        captureDeviceChanged();
}

void BaseAudioCaptureUnit::devicesChanged()
{
    Ref protectedThis { *this };

    if (!hasAudioUnit())
        return;

    auto devices = RealtimeMediaSourceCenter::singleton().audioCaptureFactory().audioCaptureDeviceManager().captureDevices();
    auto persistentID = this->persistentID();
    if (persistentID.isEmpty())
        return;

    if (std::ranges::any_of(devices, [&persistentID](auto& device) { return persistentID == device.persistentId(); })) {
        validateOutputDevice(m_outputDeviceID);
        return;
    }

    if (devices.size() && m_isCapturingWithDefaultMicrophone && migrateToNewDefaultDevice(devices[0])) {
        RELEASE_LOG_ERROR(WebRTC, "BaseAudioCaptureUnit::devicesChanged - migrating to new default device");
        return;
    }

    RELEASE_LOG_ERROR(WebRTC, "BaseAudioCaptureUnit::devicesChanged - failing capture, capturing device is missing");
    captureFailed();
}

void BaseAudioCaptureUnit::captureFailed()
{
    RELEASE_LOG_ERROR(WebRTC, "BaseAudioCaptureUnit::captureFailed");
    forEachClient([](auto& client) {
        client.captureFailed();
    });

    m_producingCount = 0;

    clearClients();

    stopRunning();
}

void BaseAudioCaptureUnit::stopProducingData()
{
    ASSERT(isMainThread());
    ASSERT(m_producingCount);
#if PLATFORM(MAC)
    if (!m_suspended) {
        prewarmAudioUnitCreation([weakThis = WeakPtr { *this }] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->continueStopProducingData();
        });
        return;
    }
#endif
    continueStopProducingData();
}

void BaseAudioCaptureUnit::continueStopProducingData()
{
    if (m_producingCount && --m_producingCount)
        return;

    if (shouldContinueRunning()) {
        setIsProducingMicrophoneSamples(false);
        return;
    }

    stopRunning();
}

void BaseAudioCaptureUnit::setIsProducingMicrophoneSamples(bool value)
{
    m_isProducingMicrophoneSamples = value;
    isProducingMicrophoneSamplesChanged();
}

void BaseAudioCaptureUnit::setIsRenderingAudio(bool value)
{
    m_isRenderingAudio = value;
    if (!shouldContinueRunning())
        stopRunning();
}

void BaseAudioCaptureUnit::stopRunning()
{
    stopInternal();
    cleanupAudioUnit();
}

void BaseAudioCaptureUnit::reconfigure()
{
    ASSERT(isMainThread());
    if (m_suspended) {
        m_needsReconfiguration = true;
        return;
    }
    reconfigureAudioUnit();
}

OSStatus BaseAudioCaptureUnit::resume()
{
    ASSERT(isMainThread());
    if (!m_suspended)
        return 0;

    ASSERT(!isProducingData());

    RELEASE_LOG_INFO(WebRTC, "BaseAudioCaptureUnit::resume");

    m_suspended = false;

    if (m_needsReconfiguration) {
        m_needsReconfiguration = false;
        reconfigure();
    }

    ASSERT(!m_producingCount);

    callOnMainThread([weakThis = WeakPtr { this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || protectedThis->m_suspended)
            return;

        protectedThis->forEachClient([](auto& client) {
            if (client.canResumeAfterInterruption())
                client.setMuted(false);
        });
    });

    return 0;
}

OSStatus BaseAudioCaptureUnit::suspend()
{
    ASSERT(isMainThread());

    RELEASE_LOG_INFO(WebRTC, "BaseAudioCaptureUnit::suspend");

    m_suspended = true;
    stopInternal();

    forEachClient([](auto& client) {
        client.setCanResumeAfterInterruption(client.isProducingData());
        client.setMuted(true);
    });

    m_producingCount = 0;

    return 0;
}

void BaseAudioCaptureUnit::audioSamplesAvailable(const MediaTime& time, const PlatformAudioData& data, const AudioStreamDescription& description, size_t numberOfFrames)
{
    // We hold the lock here since adding/removing clients can only happen in main thread.
    Locker locker { m_audioThreadClientsLock };

    // For performance reasons, we forbid heap allocations while doing rendering on the capture audio thread.
    ForbidMallocUseForCurrentThreadScope forbidMallocUse;

    for (auto& weakClient : m_audioThreadClients) {
        RefPtr client = weakClient.get();
        if (!client)
            continue;
        if (client->isProducingData())
            client->audioSamplesAvailable(time, data, description, numberOfFrames);
    }
}

void BaseAudioCaptureUnit::handleNewCurrentMicrophoneDevice(const CaptureDevice& device)
{
    forEachClient([&device](auto& client) {
        client.handleNewCurrentMicrophoneDevice(device);
    });
}

void BaseAudioCaptureUnit::voiceActivityDetected()
{
    if (!m_voiceActivityThrottleTimer)
        m_voiceActivityThrottleTimer = makeUnique<Timer>([] { });

    if (m_voiceActivityThrottleTimer->isActive() || !m_voiceActivityCallback)
        return;

    RELEASE_LOG_INFO(WebRTC, "BaseAudioCaptureUnit::voiceActivityDetected");

    m_voiceActivityCallback();
    m_voiceActivityThrottleTimer->startOneShot(voiceActivityThrottlingDuration);
}

void BaseAudioCaptureUnit::disableVoiceActivityThrottleTimerForTesting()
{
    if (m_voiceActivityThrottleTimer)
        m_voiceActivityThrottleTimer->stop();
}


} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
