/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RemoteAudioSessionProxy.h"

#if ENABLE(GPU_PROCESS) && USE(AUDIO_SESSION)

#include "GPUConnectionToWebProcess.h"
#include "GPUProcess.h"
#include "Logging.h"
#include "RemoteAudioSessionMessages.h"
#include "RemoteAudioSessionProxyManager.h"
#include "RemoteAudioSessionProxyMessages.h"
#include <WebCore/AudioSession.h>
#include <WebCore/AVAudioSessionCaptureDeviceManager.h>
#include <wtf/TZoneMalloc.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, protectedConnection().get())

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteAudioSessionProxy);

RemoteAudioSessionProxy::RemoteAudioSessionProxy(GPUConnectionToWebProcess& gpuConnection)
    : m_gpuConnection(gpuConnection)
{
}

RemoteAudioSessionProxy::~RemoteAudioSessionProxy() = default;

RefPtr<GPUConnectionToWebProcess> RemoteAudioSessionProxy::gpuConnectionToWebProcess() const
{
    return m_gpuConnection.get();
}

WebCore::ProcessIdentifier RemoteAudioSessionProxy::processIdentifier()
{
    return m_gpuConnection.get()->webProcessIdentifier();
}

RemoteAudioSessionConfiguration RemoteAudioSessionProxy::configuration()
{
    Ref session = protectedAudioSessionManager()->session();
    return {
        session->routingContextUID(),
        session->sampleRate(),
        session->bufferSize(),
        session->numberOfOutputChannels(),
        session->maximumNumberOfOutputChannels(),
        session->preferredBufferSize(),
        session->outputLatency(),
        session->isMuted(),
        m_active,
        m_sceneIdentifier,
        m_soundStageSize,
        session->categoryOverride(),
    };
}

void RemoteAudioSessionProxy::setCategory(AudioSession::CategoryType category, AudioSession::Mode mode, RouteSharingPolicy policy)
{
    if (m_category == category && m_mode == mode && m_routeSharingPolicy == policy && !m_isPlayingToBluetoothOverrideChanged)
        return;

    m_category = category;
    m_mode = mode;
    m_routeSharingPolicy = policy;
    m_isPlayingToBluetoothOverrideChanged = false;
    protectedAudioSessionManager()->updateCategory();
}

void RemoteAudioSessionProxy::setPreferredBufferSize(uint64_t size)
{
    m_preferredBufferSize = size;
    protectedAudioSessionManager()->updatePreferredBufferSizeForProcess();
}

void RemoteAudioSessionProxy::tryToSetActive(bool active, SetActiveCompletion&& completion)
{
    Ref manager = audioSessionManager();
    auto success = manager->tryToSetActiveForProcess(*this, active);
    bool hasActiveChanged = success && m_active != active;
    if (success) {
        m_active = active;
        if (m_active)
            m_isInterrupted = false;

#if ENABLE(MEDIA_STREAM) && PLATFORM(IOS_FAMILY)
        if (m_active)
            AVAudioSessionCaptureDeviceManager::singleton().setPreferredSpeakerID(m_speakerID);
#endif
    }

    completion(success);

    if (hasActiveChanged)
        configurationChanged();

    manager->updatePresentingProcesses();
    manager->updateSpatialExperience();
}

void RemoteAudioSessionProxy::setIsPlayingToBluetoothOverride(std::optional<bool>&& value)
{
    m_isPlayingToBluetoothOverrideChanged = true;
    protectedAudioSessionManager()->protectedSession()->setIsPlayingToBluetoothOverride(WTFMove(value));
}

void RemoteAudioSessionProxy::configurationChanged()
{
    protectedConnection()->send(Messages::RemoteAudioSession::ConfigurationChanged(configuration()), { });
}

void RemoteAudioSessionProxy::beginInterruption()
{
    m_isInterrupted = true;
    protectedConnection()->send(Messages::RemoteAudioSession::BeginInterruptionRemote(), { });
}

void RemoteAudioSessionProxy::endInterruption(AudioSession::MayResume mayResume)
{
    m_isInterrupted = false;
    protectedConnection()->send(Messages::RemoteAudioSession::EndInterruptionRemote(mayResume), { });
}

void RemoteAudioSessionProxy::beginInterruptionRemote()
{
    protectedAudioSessionManager()->beginInterruptionRemote();
}

void RemoteAudioSessionProxy::endInterruptionRemote(AudioSession::MayResume mayResume)
{
    protectedAudioSessionManager()->endInterruptionRemote(mayResume);
}

void RemoteAudioSessionProxy::setSceneIdentifier(const String& sceneIdentifier)
{
    m_sceneIdentifier = sceneIdentifier;
    protectedAudioSessionManager()->updateSpatialExperience();
}

void RemoteAudioSessionProxy::setSoundStageSize(AudioSession::SoundStageSize size)
{
    m_soundStageSize = size;
    protectedAudioSessionManager()->updateSpatialExperience();
}

RemoteAudioSessionProxyManager& RemoteAudioSessionProxy::audioSessionManager()
{
    return m_gpuConnection.get()->gpuProcess().audioSessionManager();
}

Ref<RemoteAudioSessionProxyManager> RemoteAudioSessionProxy::protectedAudioSessionManager()
{
    return audioSessionManager();
}

Ref<IPC::Connection> RemoteAudioSessionProxy::protectedConnection() const
{
    return m_gpuConnection.get()->connection();
}

void RemoteAudioSessionProxy::triggerBeginInterruptionForTesting()
{
    AudioSession::singleton().beginInterruptionForTesting();
}

void RemoteAudioSessionProxy::triggerEndInterruptionForTesting()
{
    AudioSession::singleton().endInterruptionForTesting();
}

std::optional<SharedPreferencesForWebProcess> RemoteAudioSessionProxy::sharedPreferencesForWebProcess() const
{
    if (RefPtr gpuConnectionToWebProcess = m_gpuConnection.get())
        return gpuConnectionToWebProcess->sharedPreferencesForWebProcess();

    return std::nullopt;
}

#if PLATFORM(IOS_FAMILY)
void RemoteAudioSessionProxy::setPreferredSpeakerID(const String& speakerID)
{
    if (m_speakerID == speakerID)
        return;
    
    m_speakerID = speakerID;
    if (!m_active)
        return;

#if ENABLE(MEDIA_STREAM)
    AVAudioSessionCaptureDeviceManager::singleton().setPreferredSpeakerID(m_speakerID);
#endif
}
#endif

} // namespace WebKit

#undef MESSAGE_CHECK

#endif
