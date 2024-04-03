/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, (&connection()))

namespace WebKit {

using namespace WebCore;

UniqueRef<RemoteAudioSessionProxy> RemoteAudioSessionProxy::create(GPUConnectionToWebProcess& gpuConnection)
{
    return makeUniqueRef<RemoteAudioSessionProxy>(gpuConnection);
}

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
    auto& session = audioSessionManager().session();
    return {
        session.routingContextUID(),
        session.sampleRate(),
        session.bufferSize(),
        session.numberOfOutputChannels(),
        session.maximumNumberOfOutputChannels(),
        session.preferredBufferSize(),
        session.isMuted(),
        m_active
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
    audioSessionManager().updateCategory();
}

void RemoteAudioSessionProxy::setPreferredBufferSize(uint64_t size)
{
    m_preferredBufferSize = size;
    audioSessionManager().updatePreferredBufferSizeForProcess();
}

void RemoteAudioSessionProxy::tryToSetActive(bool active, SetActiveCompletion&& completion)
{
    auto success = audioSessionManager().tryToSetActiveForProcess(*this, active);
    bool hasActiveChanged = success && m_active != active;
    if (success) {
        m_active = active;
        if (m_active)
            m_isInterrupted = false;
    }

    completion(success);

    if (hasActiveChanged)
        configurationChanged();

    audioSessionManager().updatePresentingProcesses();
}

void RemoteAudioSessionProxy::setIsPlayingToBluetoothOverride(std::optional<bool>&& value)
{
    m_isPlayingToBluetoothOverrideChanged = true;
    audioSessionManager().session().setIsPlayingToBluetoothOverride(WTFMove(value));
}

void RemoteAudioSessionProxy::configurationChanged()
{
    connection().send(Messages::RemoteAudioSession::ConfigurationChanged(configuration()), { });
}

void RemoteAudioSessionProxy::beginInterruption()
{
    m_isInterrupted = true;
    connection().send(Messages::RemoteAudioSession::BeginInterruptionRemote(), { });
}

void RemoteAudioSessionProxy::endInterruption(AudioSession::MayResume mayResume)
{
    m_isInterrupted = false;
    connection().send(Messages::RemoteAudioSession::EndInterruptionRemote(mayResume), { });
}

void RemoteAudioSessionProxy::beginInterruptionRemote()
{
    audioSessionManager().beginInterruptionRemote();
}

void RemoteAudioSessionProxy::endInterruptionRemote(AudioSession::MayResume mayResume)
{
    audioSessionManager().endInterruptionRemote(mayResume);
}

RemoteAudioSessionProxyManager& RemoteAudioSessionProxy::audioSessionManager()
{
    return m_gpuConnection.get()->gpuProcess().audioSessionManager();
}

bool RemoteAudioSessionProxy::allowTestOnlyIPC()
{
    if (auto connection = m_gpuConnection.get())
        return connection->allowTestOnlyIPC();
    return false;
}

IPC::Connection& RemoteAudioSessionProxy::connection()
{
    return m_gpuConnection.get()->connection();
}

void RemoteAudioSessionProxy::triggerBeginInterruptionForTesting()
{
    MESSAGE_CHECK(m_gpuConnection.get()->allowTestOnlyIPC());
    AudioSession::sharedSession().beginInterruptionForTesting();
}

void RemoteAudioSessionProxy::triggerEndInterruptionForTesting()
{
    MESSAGE_CHECK(m_gpuConnection.get()->allowTestOnlyIPC());
    AudioSession::sharedSession().endInterruptionForTesting();
}

}

#endif
