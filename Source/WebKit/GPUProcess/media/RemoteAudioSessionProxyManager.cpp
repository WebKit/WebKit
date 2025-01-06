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
#include "RemoteAudioSessionProxyManager.h"

#if ENABLE(GPU_PROCESS) && USE(AUDIO_SESSION)

#include "GPUProcess.h"
#include "GPUProcessConnectionMessages.h"
#include "RemoteAudioSessionProxy.h"
#include <WebCore/AudioSession.h>
#include <WebCore/CoreAudioCaptureSource.h>
#include <WebCore/PlatformMediaSessionManager.h>
#include <wtf/HashCountedSet.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace WebCore;

static bool categoryCanMixWithOthers(AudioSession::CategoryType category)
{
    return category == AudioSession::CategoryType::AmbientSound;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteAudioSessionProxyManager);

RemoteAudioSessionProxyManager::RemoteAudioSessionProxyManager(GPUProcess& gpuProcess)
    : m_gpuProcess(gpuProcess)
{
    Ref session = AudioSession::sharedSession();
    session->addInterruptionObserver(*this);
    session->addConfigurationChangeObserver(*this);
}

RemoteAudioSessionProxyManager::~RemoteAudioSessionProxyManager()
{
    Ref session = AudioSession::sharedSession();
    session->removeInterruptionObserver(*this);
    session->removeConfigurationChangeObserver(*this);
}

void RemoteAudioSessionProxyManager::addProxy(RemoteAudioSessionProxy& proxy, std::optional<audit_token_t> auditToken)
{
    ASSERT(!m_proxies.contains(proxy));
    m_proxies.add(proxy);
    updateCategory();

    if (auditToken)
        AudioSession::protectedSharedSession()->setHostProcessAttribution(*auditToken);
}

void RemoteAudioSessionProxyManager::removeProxy(RemoteAudioSessionProxy& proxy)
{
    ASSERT(m_proxies.contains(proxy));
    m_proxies.remove(proxy);
    updateCategory();
}

void RemoteAudioSessionProxyManager::updateCategory()
{
    HashCountedSet<AudioSession::CategoryType, WTF::IntHash<AudioSession::CategoryType>, WTF::StrongEnumHashTraits<AudioSession::CategoryType>> categoryCounts;
    HashCountedSet<AudioSession::Mode, WTF::IntHash<AudioSession::Mode>, WTF::StrongEnumHashTraits<AudioSession::Mode>> modeCounts;
    HashCountedSet<RouteSharingPolicy, WTF::IntHash<RouteSharingPolicy>, WTF::StrongEnumHashTraits<RouteSharingPolicy>> policyCounts;
    for (Ref otherProxy : m_proxies) {
        categoryCounts.add(otherProxy->category());
        modeCounts.add(otherProxy->mode());
        policyCounts.add(otherProxy->routeSharingPolicy());
    }

    AudioSession::CategoryType category = AudioSession::CategoryType::None;
    if (categoryCounts.contains(AudioSession::CategoryType::PlayAndRecord))
        category = AudioSession::CategoryType::PlayAndRecord;
    else if (categoryCounts.contains(AudioSession::CategoryType::RecordAudio))
        category = AudioSession::CategoryType::RecordAudio;
    else if (categoryCounts.contains(AudioSession::CategoryType::MediaPlayback))
        category = AudioSession::CategoryType::MediaPlayback;
    else if (categoryCounts.contains(AudioSession::CategoryType::SoloAmbientSound))
        category = AudioSession::CategoryType::SoloAmbientSound;
    else if (categoryCounts.contains(AudioSession::CategoryType::AmbientSound))
        category = AudioSession::CategoryType::AmbientSound;
    else if (categoryCounts.contains(AudioSession::CategoryType::AudioProcessing))
        category = AudioSession::CategoryType::AudioProcessing;

    AudioSession::Mode mode = AudioSession::Mode::Default;
    if (modeCounts.contains(AudioSession::Mode::MoviePlayback))
        mode = AudioSession::Mode::MoviePlayback;
    else if (modeCounts.contains(AudioSession::Mode::VideoChat))
        mode = AudioSession::Mode::VideoChat;

    RouteSharingPolicy policy = RouteSharingPolicy::Default;
    if (policyCounts.contains(RouteSharingPolicy::LongFormVideo))
        policy = RouteSharingPolicy::LongFormVideo;
    else if (policyCounts.contains(RouteSharingPolicy::LongFormAudio))
        policy = RouteSharingPolicy::LongFormAudio;
    else if (policyCounts.contains(RouteSharingPolicy::Independent))
        ASSERT_NOT_REACHED();

    AudioSession::protectedSharedSession()->setCategory(category, mode, policy);
}

void RemoteAudioSessionProxyManager::updatePreferredBufferSizeForProcess()
{
    size_t preferredBufferSize = std::numeric_limits<size_t>::max();
    for (Ref proxy : m_proxies) {
        if (proxy->preferredBufferSize() && proxy->preferredBufferSize() < preferredBufferSize)
            preferredBufferSize = proxy->preferredBufferSize();
    }

    if (preferredBufferSize != std::numeric_limits<size_t>::max())
        AudioSession::protectedSharedSession()->setPreferredBufferSize(preferredBufferSize);
}

void RemoteAudioSessionProxyManager::updateSpatialExperience()
{
    String sceneIdentifier;
    std::optional<AudioSession::SoundStageSize> maxSize;
    for (Ref proxy : m_proxies) {
        if (!proxy->isActive())
            continue;

        if (!maxSize || proxy->soundStageSize() > *maxSize) {
            maxSize = proxy->soundStageSize();
            sceneIdentifier = proxy->sceneIdentifier();
        }
    }

    Ref session = AudioSession::sharedSession();
    session->setSceneIdentifier(sceneIdentifier);
    session->setSoundStageSize(maxSize.value_or(AudioSession::SoundStageSize::Automatic));
}

bool RemoteAudioSessionProxyManager::hasOtherActiveProxyThan(RemoteAudioSessionProxy& proxyToExclude)
{
    for (Ref proxy : m_proxies) {
        if (proxy->isActive() && proxy.ptr() != &proxyToExclude)
            return true;
    }
    return false;
}

bool RemoteAudioSessionProxyManager::hasActiveNotInterruptedProxy()
{
    for (Ref proxy : m_proxies) {
        if (proxy->isActive() && !proxy->isInterrupted())
            return true;
    }
    return false;
}

bool RemoteAudioSessionProxyManager::tryToSetActiveForProcess(RemoteAudioSessionProxy& proxy, bool active)
{
    ASSERT(m_proxies.contains(proxy));

    if (!active) {
        if (hasOtherActiveProxyThan(proxy)) {
            // This proxy wants to de-activate, but other proxies are still
            // active. No-op, and return deactivation was sucessful.
            return true;
        }

        // This proxy wants to de-activate, and is the last remaining active
        // proxy. Deactivate the session, and return whether that deactivation
        // was sucessful.
        return AudioSession::protectedSharedSession()->tryToSetActive(false);
    }

    if (!hasActiveNotInterruptedProxy()) {
        // This proxy and only this proxy wants to become active. Activate
        // the session, and return whether that activation was successful.
        return AudioSession::protectedSharedSession()->tryToSetActive(active);
    }

    // If this proxy is Ambient, and the session is already active, this
    // proxy will mix with the active proxies. No-op, and return activation
    // was sucessful.
    if (categoryCanMixWithOthers(proxy.category()))
        return true;

#if PLATFORM(IOS_FAMILY)
    // Otherwise, this proxy wants to become active, but there are other
    // proxies who are already active. Walk over the proxies, and interrupt
    // those proxies whose categories indicate they cannot mix with others.
    for (auto& otherProxy : m_proxies) {
        if (otherProxy.processIdentifier() == proxy.processIdentifier())
            continue;

        if (!otherProxy.isActive())
            continue;

        if (categoryCanMixWithOthers(otherProxy.category()))
            continue;

        otherProxy.beginInterruption();
    }
#endif
    return true;
}

void RemoteAudioSessionProxyManager::updatePresentingProcesses()
{
#if ENABLE(EXTENSION_CAPABILITIES)
    if (PlatformMediaSessionManager::mediaCapabilityGrantsEnabled())
        return;
#endif

    Vector<audit_token_t> presentingProcesses;

    if (auto token = m_gpuProcess->protectedParentProcessConnection()->getAuditToken())
        presentingProcesses.append(*token);

    // AVAudioSession will take out an assertion on all the "presenting applications"
    // when it moves to a "playing" state. But it's possible that (e.g.) multiple
    // applications may be using SafariViewService simultaneously. So only include
    // tokens from those proxies whose sessions are currently "active". Only their
    // presenting applications will be kept from becoming "suspended" during playback.
    m_proxies.forEach([&](auto& proxy) {
        if (!proxy.isActive())
            return;
        for (auto& token : proxy.gpuConnectionToWebProcess()->presentingApplicationAuditTokens().values())
            presentingProcesses.append(token.auditToken());
    });
    AudioSession::protectedSharedSession()->setPresentingProcesses(WTFMove(presentingProcesses));
}

void RemoteAudioSessionProxyManager::beginInterruptionRemote()
{
    Ref session = this->session();
    // Temporarily remove as an observer to avoid a spurious IPC back to the web process.
    session->removeInterruptionObserver(*this);
    session->beginInterruption();
    session->addInterruptionObserver(*this);
}

void RemoteAudioSessionProxyManager::endInterruptionRemote(AudioSession::MayResume mayResume)
{
    Ref session = this->session();
    // Temporarily remove as an observer to avoid a spurious IPC back to the web process.
    session->removeInterruptionObserver(*this);
    session->endInterruption(mayResume);
    session->addInterruptionObserver(*this);
}

void RemoteAudioSessionProxyManager::beginAudioSessionInterruption()
{
    m_proxies.forEach([](auto& proxy) {
        if (proxy.isActive())
            proxy.beginInterruption();
    });
}

void RemoteAudioSessionProxyManager::endAudioSessionInterruption(AudioSession::MayResume mayResume)
{
    m_proxies.forEach([mayResume](auto& proxy) {
        if (proxy.isActive())
            proxy.endInterruption(mayResume);
    });
}

void RemoteAudioSessionProxyManager::hardwareMutedStateDidChange(const AudioSession& session)
{
    configurationDidChange(session);
}

void RemoteAudioSessionProxyManager::bufferSizeDidChange(const AudioSession& session)
{
    configurationDidChange(session);
}

void RemoteAudioSessionProxyManager::sampleRateDidChange(const AudioSession& session)
{
    configurationDidChange(session);
}

void RemoteAudioSessionProxyManager::configurationDidChange(const WebCore::AudioSession&)
{
    m_proxies.forEach([](auto& proxy) {
        proxy.configurationChanged();
    });
}

}

#endif
