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
#include "RemoteAudioSession.h"

#if ENABLE(GPU_PROCESS) && USE(AUDIO_SESSION)

#include "GPUConnectionToWebProcessMessages.h"
#include "GPUProcessProxy.h"
#include "RemoteAudioSessionMessages.h"
#include "RemoteAudioSessionProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/PlatformMediaSessionManager.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteAudioSession);

Ref<RemoteAudioSession> RemoteAudioSession::create(WebProcess& webProcess)
{
    return adoptRef(*new RemoteAudioSession(webProcess));
}

RemoteAudioSession::RemoteAudioSession(WebProcess& webProcess)
    : m_webProcess(webProcess)
{
    AudioSession::addInterruptionObserver(*this);
}

RemoteAudioSession::~RemoteAudioSession()
{
    if (auto gpuProcessConnection = m_gpuProcessConnection.get())
        gpuProcessConnection->messageReceiverMap().removeMessageReceiver(Messages::RemoteAudioSession::messageReceiverName());

    AudioSession::removeInterruptionObserver(*this);
}

void RemoteAudioSession::gpuProcessConnectionDidClose(GPUProcessConnection& connection)
{
    ASSERT(m_gpuProcessConnection.get() == &connection);
    m_gpuProcessConnection = nullptr;
    setActive(false);
    connection.messageReceiverMap().removeMessageReceiver(Messages::RemoteAudioSession::messageReceiverName());
}

IPC::Connection& RemoteAudioSession::ensureConnection()
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!gpuProcessConnection) {
        gpuProcessConnection = WebProcess::singleton().ensureGPUProcessConnection();
        m_gpuProcessConnection = gpuProcessConnection;
        gpuProcessConnection->addClient(*this);
        gpuProcessConnection->messageReceiverMap().addMessageReceiver(Messages::RemoteAudioSession::messageReceiverName(), *this);

        auto sendResult = protect(ensureConnection())->sendSync(Messages::GPUConnectionToWebProcess::EnsureAudioSession(), { });
        std::tie(m_configuration) = sendResult.takeReplyOr(RemoteAudioSessionConfiguration { });
    }
    return gpuProcessConnection->connection();
}

const RemoteAudioSessionConfiguration& RemoteAudioSession::configuration() const
{
    if (!m_configuration)
        const_cast<RemoteAudioSession*>(this)->ensureConnection();
    return *m_configuration;
}

RemoteAudioSessionConfiguration& RemoteAudioSession::configuration()
{
    if (!m_configuration)
        ensureConnection();
    return *m_configuration;
}

void RemoteAudioSession::setCategory(CategoryType type, Mode mode, RouteSharingPolicy policy)
{
#if PLATFORM(COCOA)
    if (type == m_category && mode == m_mode && policy == m_routeSharingPolicy && !m_isPlayingToBluetoothOverrideChanged)
        return;

    m_category = type;
    m_mode = mode;
    m_routeSharingPolicy = policy;
    m_isPlayingToBluetoothOverrideChanged = false;

    protect(ensureConnection())->send(Messages::RemoteAudioSessionProxy::SetCategory(type, mode, policy), { });
#else
    UNUSED_PARAM(type);
    UNUSED_PARAM(policy);
#endif
}

void RemoteAudioSession::setPreferredBufferSize(size_t size)
{
    configuration().preferredBufferSize = size;
    protect(ensureConnection())->send(Messages::RemoteAudioSessionProxy::SetPreferredBufferSize(size), { });
}

Ref<AudioSession::SetActivePromise> RemoteAudioSession::tryToSetActiveInternal(bool active)
{
    if (active && m_isInterruptedForTesting)
        return SetActivePromise::createAndReject();

    SetActivePromise::Producer producer;
    Ref<SetActivePromise> promise = producer.promise();

    // Coalesce same-state requests against the tail of the chain; different-state
    // requests append a new entry. We keep at most one IPC in flight at a time so
    // ordering is preserved even when the underlying activation becomes truly
    // non-blocking.
    if (!m_pendingActivationChain.isEmpty() && m_pendingActivationChain.last().active == active) {
        m_pendingActivationChain.last().waiters.append(WTF::move(producer));
        return promise;
    }

    PendingActivation entry { active, { } };
    entry.waiters.append(WTF::move(producer));
    m_pendingActivationChain.append(WTF::move(entry));

    // Optimistically reflect the requested state so JS observers (e.g.
    // internals.audioSessionActive()) see the new value without waiting for the
    // IPC round-trip. The base class whenSettled callback sets m_active again
    // before calling activeStateChanged(), so activeStateChanged() always sees
    // the correct state regardless of later optimistic updates from enqueued
    // deactivations. Reverted to !active on IPC failure in sendNextActivationIPC.
    setActive(active);

    sendNextActivationIPC();
    return promise;
}

void RemoteAudioSession::sendNextActivationIPC()
{
    if (m_activationIPCInFlight || m_pendingActivationChain.isEmpty())
        return;

    bool active = m_pendingActivationChain.first().active;
    m_activationIPCInFlight = true;

    auto processResult = [this, protectedThis = Ref { *this }, active](bool succeeded) mutable {
        ASSERT(!m_pendingActivationChain.isEmpty() && m_pendingActivationChain.first().active == active);
        auto pending = m_pendingActivationChain.takeFirst();
        m_activationIPCInFlight = false;

        if (!succeeded)
            setActive(!active); // revert optimistic update
        sendNextActivationIPC();

        for (auto& waiter : pending.waiters) {
            if (succeeded)
                waiter.resolve();
            else
                waiter.reject();
        }
    };

    bool useAsyncIPC = WebProcess::singleton().sharedPreferencesForWebProcessValue().asyncAudioSessionActivationEnabled;
    if (useAsyncIPC) {
        protect(ensureConnection())->sendWithPromisedReply(
            Messages::RemoteAudioSessionProxy::TryToSetActive(active)
        )->whenSettled(RunLoop::mainSingleton(),
            [processResult = WTF::move(processResult)](auto&& result) mutable {
                processResult(result.has_value() && *result);
            });
    } else {
        auto sendResult = protect(ensureConnection())->sendSync(Messages::RemoteAudioSessionProxy::TryToSetActiveSync(active), { });
        auto [succeeded] = sendResult.takeReplyOr(false);
        processResult(succeeded);
    }
}

void RemoteAudioSession::addConfigurationChangeObserver(AudioSessionConfigurationChangeObserver& observer)
{
    m_configurationChangeObservers.add(observer);
}

void RemoteAudioSession::removeConfigurationChangeObserver(AudioSessionConfigurationChangeObserver& observer)
{
    m_configurationChangeObservers.remove(observer);
}

void RemoteAudioSession::setIsPlayingToBluetoothOverride(std::optional<bool> value)
{
    m_isPlayingToBluetoothOverrideChanged = true;
    protect(ensureConnection())->send(Messages::RemoteAudioSessionProxy::SetIsPlayingToBluetoothOverride(value), { });
}

AudioSession::CategoryType RemoteAudioSession::category() const
{
    return m_category;
}

AudioSession::Mode RemoteAudioSession::mode() const
{
    return m_mode;
}

void RemoteAudioSession::configurationChanged(RemoteAudioSessionConfiguration&& configuration)
{
    bool mutedStateChanged = !m_configuration || configuration.isMuted != (*m_configuration).isMuted;
    bool bufferSizeChanged = !m_configuration || configuration.bufferSize != (*m_configuration).bufferSize;
    bool sampleRateChanged = !m_configuration || configuration.sampleRate != (*m_configuration).sampleRate;
    bool routingContextUIDChanged = !m_configuration || configuration.routingContextUID != (*m_configuration).routingContextUID;

    m_configuration = WTF::move(configuration);

    m_configurationChangeObservers.forEach([&](auto& observer) {
        if (mutedStateChanged)
            observer.hardwareMutedStateDidChange(*this);

        if (bufferSizeChanged)
            observer.bufferSizeDidChange(*this);

        if (sampleRateChanged)
            observer.sampleRateDidChange(*this);

        if (routingContextUIDChanged)
            observer.routingContextUIDDidChange(*this);
    });

    if (!mutedStateChanged && !bufferSizeChanged && !sampleRateChanged && !routingContextUIDChanged)
        return;

    RefPtr protectedProcess = m_webProcess.get();
    if (!protectedProcess)
        return;

    if (!protectedProcess->sharedPreferencesForWebProcessValue().remoteMediaSessionManagerEnabled)
        return;

    protectedProcess->remoteAudioSessionConfigurationChanged(*m_configuration);
}

void RemoteAudioSession::beginInterruptionRemote()
{
    AudioSession::removeInterruptionObserver(*this);
    beginInterruption();
    AudioSession::addInterruptionObserver(*this);
}

void RemoteAudioSession::endInterruptionRemote(MayResume mayResume)
{
    AudioSession::removeInterruptionObserver(*this);
    endInterruption(mayResume);
    AudioSession::addInterruptionObserver(*this);
}

void RemoteAudioSession::beginAudioSessionInterruption()
{
    protect(ensureConnection())->send(Messages::RemoteAudioSessionProxy::BeginInterruptionRemote(), { });
}

void RemoteAudioSession::endAudioSessionInterruption(MayResume mayResume)
{
    protect(ensureConnection())->send(Messages::RemoteAudioSessionProxy::EndInterruptionRemote(mayResume), { });
}

void RemoteAudioSession::beginInterruptionForTesting()
{
    m_isInterruptedForTesting = true;
    protect(ensureConnection())->send(Messages::RemoteAudioSessionProxy::TriggerBeginInterruptionForTesting(), { });
}

void RemoteAudioSession::endInterruptionForTesting()
{
    if (!m_isInterruptedForTesting)
        return;

    m_isInterruptedForTesting = false;
    protect(ensureConnection())->send(Messages::RemoteAudioSessionProxy::TriggerEndInterruptionForTesting(), { });
}

void RemoteAudioSession::setSceneIdentifier(const String& sceneIdentifier)
{
    configuration().sceneIdentifier = sceneIdentifier;
    protect(ensureConnection())->send(Messages::RemoteAudioSessionProxy::SetSceneIdentifier(sceneIdentifier), { });
}

void RemoteAudioSession::setSoundStageSize(AudioSession::SoundStageSize size)
{
    configuration().soundStageSize = size;
    protect(ensureConnection())->send(Messages::RemoteAudioSessionProxy::SetSoundStageSize(size), { });
}

}

#endif
