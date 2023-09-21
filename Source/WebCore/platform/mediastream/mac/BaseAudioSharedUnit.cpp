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
#include "BaseAudioSharedUnit.h"

#if ENABLE(MEDIA_STREAM)

#include "AudioSession.h"
#include "CaptureDeviceManager.h"
#include "CoreAudioCaptureSource.h"
#include "DeprecatedGlobalSettings.h"
#include "Logging.h"
#include "PlatformMediaSessionManager.h"
#include <wtf/FastMalloc.h>

namespace WebCore {

BaseAudioSharedUnit::BaseAudioSharedUnit()
    : m_sampleRate(AudioSession::sharedSession().sampleRate())
{
    RealtimeMediaSourceCenter::singleton().addDevicesChangedObserver(*this);
}

BaseAudioSharedUnit::~BaseAudioSharedUnit()
{
    RealtimeMediaSourceCenter::singleton().removeDevicesChangedObserver(*this);
}

void BaseAudioSharedUnit::addClient(CoreAudioCaptureSource& client)
{
    ASSERT(isMainThread());
    m_clients.add(&client);
    Locker locker { m_audioThreadClientsLock };
    m_audioThreadClients = copyToVector(m_clients);
}

void BaseAudioSharedUnit::removeClient(CoreAudioCaptureSource& client)
{
    ASSERT(isMainThread());
    m_clients.remove(&client);
    Locker locker { m_audioThreadClientsLock };
    m_audioThreadClients = copyToVector(m_clients);
}

void BaseAudioSharedUnit::clearClients()
{
    ASSERT(isMainThread());
    m_clients.clear();
    Locker locker { m_audioThreadClientsLock };
    m_audioThreadClients.clear();
}

void BaseAudioSharedUnit::forEachClient(const Function<void(CoreAudioCaptureSource&)>& apply) const
{
    ASSERT(isMainThread());
    for (auto& client : copyToVector(m_clients)) {
        // Make sure the client has not been destroyed.
        if (!m_clients.contains(client.get()))
            continue;
        apply(*client);
    }
}

const static OSStatus lowPriorityError1 = 560557684;
const static OSStatus lowPriorityError2 = 561017449;
void BaseAudioSharedUnit::startProducingData()
{
    ASSERT(isMainThread());

    if (m_suspended)
        resume();

    setIsProducingMicrophoneSamples(true);

    if (++m_producingCount != 1)
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
            RELEASE_LOG_ERROR(WebRTC, "BaseAudioSharedUnit::startProducingData failed due to not high enough priority, suspending unit");
            suspend();
        } else
            captureFailed();
    }
}

OSStatus BaseAudioSharedUnit::startUnit()
{
    forEachClient([](auto& client) {
        client.audioUnitWillStart();
    });
    ASSERT(!DeprecatedGlobalSettings::shouldManageAudioSessionCategory() || AudioSession::sharedSession().category() == AudioSession::CategoryType::PlayAndRecord);

#if PLATFORM(IOS_FAMILY)
    if (AudioSession::sharedSession().category() != AudioSession::CategoryType::PlayAndRecord) {
        RELEASE_LOG_ERROR(WebRTC, "BaseAudioSharedUnit::startUnit cannot call startInternal if category is not set to PlayAndRecord");
        return lowPriorityError2;
    }
#endif

    return startInternal();
}

void BaseAudioSharedUnit::prepareForNewCapture()
{
    m_volume = 1;
    resetSampleRate();

    if (!m_suspended)
        return;
    m_suspended = false;

    if (!m_producingCount)
        return;

    RELEASE_LOG_ERROR(WebRTC, "BaseAudioSharedUnit::prepareForNewCapture, notifying suspended sources of capture failure");
    captureFailed();
}

void BaseAudioSharedUnit::setCaptureDevice(String&& persistentID, uint32_t captureDeviceID)
{
    bool hasChanged = this->persistentID() != persistentID || this->captureDeviceID() != captureDeviceID;
    m_capturingDevice = { WTFMove(persistentID), captureDeviceID };

    if (hasChanged)
        captureDeviceChanged();
}

void BaseAudioSharedUnit::devicesChanged()
{
    auto devices = RealtimeMediaSourceCenter::singleton().audioCaptureFactory().audioCaptureDeviceManager().captureDevices();
    auto persistentID = this->persistentID();
    if (persistentID.isEmpty())
        return;

    if (WTF::anyOf(devices, [&persistentID] (auto& device) { return persistentID == device.persistentId(); })) {
        validateOutputDevice(m_outputDeviceID);
        return;
    }

    RELEASE_LOG_ERROR(WebRTC, "BaseAudioSharedUnit::devicesChanged - failing capture, capturing device is missing");
    captureFailed();
}

void BaseAudioSharedUnit::captureFailed()
{
    RELEASE_LOG_ERROR(WebRTC, "BaseAudioSharedUnit::captureFailed");
    forEachClient([](auto& client) {
        client.captureFailed();
    });

    m_producingCount = 0;

    clearClients();

    stopInternal();
    cleanupAudioUnit();
}

void BaseAudioSharedUnit::stopProducingData()
{
    ASSERT(isMainThread());
    ASSERT(m_producingCount);

    if (m_producingCount && --m_producingCount)
        return;

    if (m_isRenderingAudio) {
        setIsProducingMicrophoneSamples(false);
        return;
    }

    stopInternal();
    cleanupAudioUnit();

    auto callbacks = std::exchange(m_whenNotRunningCallbacks, { });
    for (auto& callback : callbacks)
        callback();
}

void BaseAudioSharedUnit::setIsProducingMicrophoneSamples(bool value)
{
    m_isProducingMicrophoneSamples = value;
    isProducingMicrophoneSamplesChanged();
}

void BaseAudioSharedUnit::setIsRenderingAudio(bool value)
{
    m_isRenderingAudio = value;
    if (m_isRenderingAudio || m_producingCount)
        return;

    stopInternal();
    cleanupAudioUnit();
}

void BaseAudioSharedUnit::reconfigure()
{
    ASSERT(isMainThread());
    if (m_suspended) {
        m_needsReconfiguration = true;
        return;
    }
    reconfigureAudioUnit();
}

OSStatus BaseAudioSharedUnit::resume()
{
    ASSERT(isMainThread());
    if (!m_suspended)
        return 0;

    ASSERT(!isProducingData());

    RELEASE_LOG_INFO(WebRTC, "BaseAudioSharedUnit::resume");

    m_suspended = false;

    if (m_needsReconfiguration) {
        m_needsReconfiguration = false;
        reconfigure();
    }

    ASSERT(!m_producingCount);

    callOnMainThread([weakThis = WeakPtr { this }] {
        if (!weakThis || weakThis->m_suspended)
            return;

        weakThis->forEachClient([](auto& client) {
            if (client.canResumeAfterInterruption())
                client.setMuted(false);
        });
    });

    return 0;
}

OSStatus BaseAudioSharedUnit::suspend()
{
    ASSERT(isMainThread());

    RELEASE_LOG_INFO(WebRTC, "BaseAudioSharedUnit::suspend");

    m_suspended = true;
    stopInternal();

    forEachClient([](auto& client) {
        client.setCanResumeAfterInterruption(client.isProducingData());
        client.setMuted(true);
    });

    ASSERT(!m_producingCount);

    return 0;
}

void BaseAudioSharedUnit::audioSamplesAvailable(const MediaTime& time, const PlatformAudioData& data, const AudioStreamDescription& description, size_t numberOfFrames)
{
    // We hold the lock here since adding/removing clients can only happen in main thread.
    Locker locker { m_audioThreadClientsLock };

    // For performance reasons, we forbid heap allocations while doing rendering on the capture audio thread.
    ForbidMallocUseForCurrentThreadScope forbidMallocUse;

    for (auto& client : m_audioThreadClients) {
        if (client.get()->isProducingData())
            client.get()->audioSamplesAvailable(time, data, description, numberOfFrames);
    }
}

void BaseAudioSharedUnit::whenAudioCaptureUnitIsNotRunning(Function<void()>&& callback)
{
    if (!isProducingData()) {
        callback();
        return;
    }
    m_whenNotRunningCallbacks.append(WTFMove(callback));
}

void BaseAudioSharedUnit::handleNewCurrentMicrophoneDevice(CaptureDevice&& device)
{
    forEachClient([&device](auto& client) {
        client.handleNewCurrentMicrophoneDevice(device);
    });
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
