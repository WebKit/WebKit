/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#include "AudioSession.h"

#if USE(AUDIO_SESSION)

#include "Logging.h"
#include "NotImplemented.h"
#include <wtf/NeverDestroyed.h>

#if PLATFORM(MAC)
#include "AudioSessionMac.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include "AudioSessionIOS.h"
#endif

namespace WebCore {

bool AudioSession::s_shouldManageAudioSessionCategory { false };

static std::optional<UniqueRef<AudioSession>>& sharedAudioSession()
{
    static NeverDestroyed<std::optional<UniqueRef<AudioSession>>> session;
    return session.get();
}

static WeakHashSet<AudioSession::ChangedObserver>& audioSessionChangedObservers()
{
    static NeverDestroyed<WeakHashSet<AudioSession::ChangedObserver>> observers;
    return observers;
}

UniqueRef<AudioSession> AudioSession::create()
{
#if PLATFORM(MAC)
    return makeUniqueRef<AudioSessionMac>();
#elif PLATFORM(IOS_FAMILY)
    return makeUniqueRef<AudioSessionIOS>();
#else
    return makeUniqueRef<AudioSession>();
#endif
}

AudioSession::AudioSession() = default;
AudioSession::~AudioSession() = default;

AudioSession& AudioSession::sharedSession()
{
    if (!sharedAudioSession())
        setSharedSession(AudioSession::create());
    return *sharedAudioSession();
}

void AudioSession::setSharedSession(UniqueRef<AudioSession>&& session)
{
    sharedAudioSession() = WTFMove(session);

    audioSessionChangedObservers().forEach([] (auto& observer) {
        observer(*sharedAudioSession());
    });
}

void AudioSession::addAudioSessionChangedObserver(const ChangedObserver& observer)
{
    ASSERT(!audioSessionChangedObservers().contains(observer));
    audioSessionChangedObservers().add(observer);

    if (sharedAudioSession())
        observer(*sharedAudioSession());
}

bool AudioSession::tryToSetActive(bool active)
{
    bool previousIsActive = isActive();
    if (!tryToSetActiveInternal(active))
        return false;

    bool hasActiveChanged = previousIsActive != isActive();
    m_active = active;
    if (m_isInterrupted && m_active) {
        callOnMainThread([hasActiveChanged] {
            auto& session = sharedSession();
            if (session.m_isInterrupted && session.m_active)
                session.endInterruption(MayResume::Yes);
            if (hasActiveChanged)
                session.activeStateChanged();
        });
    } else if (hasActiveChanged)
        activeStateChanged();

    return true;
}

void AudioSession::addInterruptionObserver(InterruptionObserver& observer)
{
    m_interruptionObservers.add(observer);
}

void AudioSession::removeInterruptionObserver(InterruptionObserver& observer)
{
    m_interruptionObservers.remove(observer);
}

void AudioSession::beginInterruption()
{
    if (m_isInterrupted) {
        RELEASE_LOG_ERROR(WebRTC, "AudioSession::beginInterruption but session is already interrupted!");
        return;
    }
    m_isInterrupted = true;
    for (auto& observer : m_interruptionObservers)
        observer.beginAudioSessionInterruption();
}

void AudioSession::endInterruption(MayResume mayResume)
{
    if (!m_isInterrupted) {
        RELEASE_LOG_ERROR(WebRTC, "AudioSession::endInterruption but session is already uninterrupted!");
        return;
    }
    m_isInterrupted = false;

    for (auto& observer : m_interruptionObservers)
        observer.endAudioSessionInterruption(mayResume);
}

void AudioSession::activeStateChanged()
{
    for (auto& observer : m_interruptionObservers)
        observer.activeStateChanged();
}

void AudioSession::setCategory(CategoryType, RouteSharingPolicy)
{
    notImplemented();
}

void AudioSession::setCategoryOverride(CategoryType category)
{
    if (m_categoryOverride == category)
        return;

    m_categoryOverride = category;
    if (category != CategoryType::None)
        setCategory(category, RouteSharingPolicy::Default);
}

AudioSession::CategoryType AudioSession::categoryOverride() const
{
    return m_categoryOverride;
}

AudioSession::CategoryType AudioSession::category() const
{
    notImplemented();
    return AudioSession::CategoryType::None;
}

float AudioSession::sampleRate() const
{
    notImplemented();
    return 0;
}

size_t AudioSession::bufferSize() const
{
    notImplemented();
    return 0;
}

size_t AudioSession::numberOfOutputChannels() const
{
    notImplemented();
    return 0;
}

size_t AudioSession::maximumNumberOfOutputChannels() const
{
    notImplemented();
    return 0;
}

bool AudioSession::tryToSetActiveInternal(bool)
{
    notImplemented();
    return true;
}

size_t AudioSession::preferredBufferSize() const
{
    notImplemented();
    return 0;
}

void AudioSession::setPreferredBufferSize(size_t)
{
    notImplemented();
}

RouteSharingPolicy AudioSession::routeSharingPolicy() const
{
    return RouteSharingPolicy::Default;
}

String AudioSession::routingContextUID() const
{
    return emptyString();
}

void AudioSession::audioOutputDeviceChanged()
{
    notImplemented();
}

void AudioSession::addConfigurationChangeObserver(ConfigurationChangeObserver&)
{
    notImplemented();
}

void AudioSession::removeConfigurationChangeObserver(ConfigurationChangeObserver&)
{
    notImplemented();
}

void AudioSession::setIsPlayingToBluetoothOverride(std::optional<bool>)
{
    notImplemented();
}

String convertEnumerationToString(RouteSharingPolicy enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("Default"),
        MAKE_STATIC_STRING_IMPL("LongFormAudio"),
        MAKE_STATIC_STRING_IMPL("Independent"),
        MAKE_STATIC_STRING_IMPL("LongFormVideo"),
    };
    static_assert(!static_cast<size_t>(RouteSharingPolicy::Default), "RouteSharingPolicy::Default is not 0 as expected");
    static_assert(static_cast<size_t>(RouteSharingPolicy::LongFormAudio) == 1, "RouteSharingPolicy::LongFormAudio is not 1 as expected");
    static_assert(static_cast<size_t>(RouteSharingPolicy::Independent) == 2, "RouteSharingPolicy::Independent is not 2 as expected");
    static_assert(static_cast<size_t>(RouteSharingPolicy::LongFormVideo) == 3, "RouteSharingPolicy::LongFormVideo is not 3 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

String convertEnumerationToString(AudioSession::CategoryType enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("None"),
        MAKE_STATIC_STRING_IMPL("AmbientSound"),
        MAKE_STATIC_STRING_IMPL("SoloAmbientSound"),
        MAKE_STATIC_STRING_IMPL("MediaPlayback"),
        MAKE_STATIC_STRING_IMPL("RecordAudio"),
        MAKE_STATIC_STRING_IMPL("PlayAndRecord"),
        MAKE_STATIC_STRING_IMPL("AudioProcessing"),
    };
    static_assert(!static_cast<size_t>(AudioSession::CategoryType::None), "AudioSession::CategoryType::None is not 0 as expected");
    static_assert(static_cast<size_t>(AudioSession::CategoryType::AmbientSound) == 1, "AudioSession::CategoryType::AmbientSound is not 1 as expected");
    static_assert(static_cast<size_t>(AudioSession::CategoryType::SoloAmbientSound) == 2, "AudioSession::CategoryType::SoloAmbientSound is not 2 as expected");
    static_assert(static_cast<size_t>(AudioSession::CategoryType::MediaPlayback) == 3, "AudioSession::CategoryType::MediaPlayback is not 3 as expected");
    static_assert(static_cast<size_t>(AudioSession::CategoryType::RecordAudio) == 4, "AudioSession::CategoryType::RecordAudio is not 4 as expected");
    static_assert(static_cast<size_t>(AudioSession::CategoryType::PlayAndRecord) == 5, "AudioSession::CategoryType::PlayAndRecord is not 5 as expected");
    static_assert(static_cast<size_t>(AudioSession::CategoryType::AudioProcessing) == 6, "AudioSession::CategoryType::AudioProcessing is not 6 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

String convertEnumerationToString(AudioSessionRoutingArbitrationClient::RoutingArbitrationError enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("None"),
        MAKE_STATIC_STRING_IMPL("Failed"),
        MAKE_STATIC_STRING_IMPL("Cancelled"),
    };
    static_assert(!static_cast<size_t>(AudioSessionRoutingArbitrationClient::RoutingArbitrationError::None), "AudioSessionRoutingArbitrationClient::RoutingArbitrationError::None is not 0 as expected");
    static_assert(static_cast<size_t>(AudioSessionRoutingArbitrationClient::RoutingArbitrationError::Failed), "AudioSessionRoutingArbitrationClient::RoutingArbitrationError::Failed is not 1 as expected");
    static_assert(static_cast<size_t>(AudioSessionRoutingArbitrationClient::RoutingArbitrationError::Cancelled), "AudioSessionRoutingArbitrationClient::RoutingArbitrationError::Cancelled is not 2 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

String convertEnumerationToString(AudioSessionRoutingArbitrationClient::DefaultRouteChanged enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("No"),
        MAKE_STATIC_STRING_IMPL("Yes"),
    };
    static_assert(!static_cast<bool>(AudioSessionRoutingArbitrationClient::DefaultRouteChanged::No), "AudioSessionRoutingArbitrationClient::DefaultRouteChanged::No is not false as expected");
    static_assert(static_cast<bool>(AudioSessionRoutingArbitrationClient::DefaultRouteChanged::Yes), "AudioSessionRoutingArbitrationClient::DefaultRouteChanged::Yes is not true as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

}

#endif // USE(AUDIO_SESSION)
