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

#include "RemoteAudioSessionProxy.h"
#include <WebCore/AudioSession.h>
#include <wtf/HashCountedSet.h>

namespace WebKit {

using namespace WebCore;

static bool categoryCanMixWithOthers(AudioSession::CategoryType category)
{
    return category == AudioSession::AmbientSound;
}

RemoteAudioSessionProxyManager::RemoteAudioSessionProxyManager()
    : m_session(AudioSession::create())
{
}

RemoteAudioSessionProxyManager::~RemoteAudioSessionProxyManager() = default;

void RemoteAudioSessionProxyManager::addProxy(WeakPtr<RemoteAudioSessionProxy>&& proxy)
{
    auto id = proxy->processIdentifier();
    ASSERT(!m_proxies.contains(id));
    m_proxies.set(id, WTFMove(proxy));
}

void RemoteAudioSessionProxyManager::removeProxy(const ProcessIdentifier& id)
{
    ASSERT(m_proxies.contains(id));
    m_proxies.remove(id);
}

RemoteAudioSessionProxy* RemoteAudioSessionProxyManager::getProxy(const ProcessIdentifier& id)
{
    auto results = m_proxies.find(id);
    if (results != m_proxies.end())
        return results->value.get();
    return nullptr;
}

void RemoteAudioSessionProxyManager::setCategoryForProcess(const ProcessIdentifier& id, AudioSession::CategoryType category, RouteSharingPolicy policy)
{
    ASSERT(m_proxies.contains(id));
    auto proxy = getProxy(id);
    if (!proxy)
        return;

    if (proxy->category() == category && proxy->routeSharingPolicy() == policy)
        return;

    HashCountedSet<AudioSession::CategoryType, WTF::IntHash<AudioSession::CategoryType>, WTF::StrongEnumHashTraits<AudioSession::CategoryType>> categoryCounts;
    HashCountedSet<RouteSharingPolicy, WTF::IntHash<RouteSharingPolicy>, WTF::StrongEnumHashTraits<RouteSharingPolicy>> policyCounts;
    for (auto& otherProxy : m_proxies.values()) {
        categoryCounts.add(otherProxy->category());
        policyCounts.add(otherProxy->routeSharingPolicy());
    }

    if (categoryCounts.contains(AudioSession::PlayAndRecord))
        category = AudioSession::PlayAndRecord;
    else if (categoryCounts.contains(AudioSession::RecordAudio))
        category = AudioSession::RecordAudio;
    else if (categoryCounts.contains(AudioSession::MediaPlayback))
        category = AudioSession::MediaPlayback;
    else if (categoryCounts.contains(AudioSession::SoloAmbientSound))
        category = AudioSession::SoloAmbientSound;
    else if (categoryCounts.contains(AudioSession::AmbientSound))
        category = AudioSession::AmbientSound;
    else if (categoryCounts.contains(AudioSession::AudioProcessing))
        category = AudioSession::AudioProcessing;
    else
        category = AudioSession::None;

    if (policyCounts.contains(RouteSharingPolicy::LongFormVideo))
        policy = RouteSharingPolicy::LongFormVideo;
    else if (policyCounts.contains(RouteSharingPolicy::LongFormAudio))
        policy = RouteSharingPolicy::LongFormAudio;
    else if (policyCounts.contains(RouteSharingPolicy::Independent))
        policy = RouteSharingPolicy::Independent;
    else
        policy = RouteSharingPolicy::Default;

    m_session->setCategory(category, policy);
}

void RemoteAudioSessionProxyManager::setPreferredBufferSizeForProcess(const ProcessIdentifier& id, size_t preferredBufferSize)
{
    for (auto& otherProxy : m_proxies.values()) {
        if (!otherProxy)
            continue;
        if (otherProxy->preferredBufferSize() < preferredBufferSize)
            preferredBufferSize = otherProxy->preferredBufferSize();
    }

    m_session->setPreferredBufferSize(preferredBufferSize);
}

bool RemoteAudioSessionProxyManager::tryToSetActiveForProcess(const ProcessIdentifier& id, bool active)
{
    ASSERT(m_proxies.contains(id));
    auto proxy = getProxy(id);
    if (!proxy)
        return false;

    size_t activeProxyCount { 0 };
    for (auto& otherProxy : m_proxies.values()) {
        if (!otherProxy)
            continue;
        if (otherProxy->isActive())
            ++activeProxyCount;
    }

    if (!active && activeProxyCount > 1) {
        // This proxy wants to de-activate, but other proxies are still
        // active. No-op, and return deactivation was sucessful.
        return true;
    }

    if (!active) {
        // This proxy wants to de-activate, and is the last remaining active
        // proxy. Deactivate the session, and return whether that deactivation
        // was sucessful;
        return m_session->tryToSetActive(false);
    }

    if (active && !activeProxyCount) {
        // This proxy and only this proxy wants to become active. Activate
        // the session, and return whether that activation was successful.
        return m_session->tryToSetActive(active);
    }

    // If this proxy is Ambient, and the session is already active, this
    // proxy will mix with the active proxies. No-op, and return activation
    // was sucessful.
    if (categoryCanMixWithOthers(proxy->category()))
        return true;

#if PLATFORM(IOS_FAMILY)
    // Otherwise, this proxy wants to become active, but there are other
    // proxies who are already active. Walk over the proxies, and interrupt
    // those proxies whose cateogries indicate they cannot mix with others.
    for (auto& otherProxy : m_proxies.values()) {
        if (!otherProxy)
            continue;
        if (!otherProxy->isActive())
            continue;

        if (categoryCanMixWithOthers(otherProxy->category()))
            continue;

        otherProxy->beginInterruption(PlatformMediaSession::InterruptionType::SystemInterruption);
    }
#endif

    return true;

}

}

#endif
