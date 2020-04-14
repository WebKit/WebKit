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

#include "NotImplemented.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static UniqueRef<AudioSession>& sharedAudioSession()
{
    static NeverDestroyed<UniqueRef<AudioSession>> session = AudioSession::create();
    return session.get();
}

UniqueRef<AudioSession> AudioSession::create()
{
    return makeUniqueRef<AudioSession>();
}

AudioSession& AudioSession::sharedSession()
{
    return sharedAudioSession();
}

void AudioSession::setSharedSession(UniqueRef<AudioSession>&& session)
{
    sharedAudioSession() = WTFMove(session);
}

bool AudioSession::tryToSetActive(bool active)
{
    if (!tryToSetActiveInternal(active))
        return false;

    m_active = active;
    return true;
}

#if !PLATFORM(IOS_FAMILY)
void AudioSession::addInterruptionObserver(InterruptionObserver&)
{
}

void AudioSession::removeInterruptionObserver(InterruptionObserver&)
{
}

void AudioSession::beginInterruption()
{
}

void AudioSession::endInterruption(MayResume)
{
}
#endif

#if !PLATFORM(COCOA)
class AudioSessionPrivate {
    WTF_MAKE_FAST_ALLOCATED;
};

AudioSession::AudioSession()
    : m_private(nullptr)
{
    notImplemented();
}

AudioSession::~AudioSession() = default;

void AudioSession::setCategory(CategoryType, RouteSharingPolicy)
{
    notImplemented();
}

AudioSession::CategoryType AudioSession::categoryOverride() const
{
    notImplemented();
    return None;
}

void AudioSession::setCategoryOverride(CategoryType)
{
    notImplemented();
}

AudioSession::CategoryType AudioSession::category() const
{
    notImplemented();
    return None;
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

#endif // !PLATFORM(COCOA)

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
    ASSERT(static_cast<size_t>(enumerationValue) < WTF_ARRAY_LENGTH(values));
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
    ASSERT(static_cast<size_t>(enumerationValue) < WTF_ARRAY_LENGTH(values));
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
    ASSERT(static_cast<size_t>(enumerationValue) < WTF_ARRAY_LENGTH(values));
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
    ASSERT(static_cast<size_t>(enumerationValue) < WTF_ARRAY_LENGTH(values));
    return values[static_cast<size_t>(enumerationValue)];
}

}

#endif // USE(AUDIO_SESSION)
