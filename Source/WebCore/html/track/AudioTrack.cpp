/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AudioTrack.h"

#if ENABLE(VIDEO)

#include "AudioTrackClient.h"
#include "AudioTrackConfiguration.h"
#include "AudioTrackList.h"
#include "AudioTrackPrivate.h"
#include "CommonAtomStrings.h"
#include "ScriptExecutionContext.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

const AtomString& AudioTrack::descriptionKeyword()
{
    static MainThreadNeverDestroyed<const AtomString> description("description"_s);
    return description;
}

const AtomString& AudioTrack::mainDescKeyword()
{
    static MainThreadNeverDestroyed<const AtomString> mainDesc("main-desc"_s);
    return mainDesc;
}

const AtomString& AudioTrack::translationKeyword()
{
    static MainThreadNeverDestroyed<const AtomString> translation("translation"_s);
    return translation;
}

AudioTrack::AudioTrack(ScriptExecutionContext* context, AudioTrackPrivate& trackPrivate)
    : MediaTrackBase(context, MediaTrackBase::AudioTrack, trackPrivate.trackUID(), trackPrivate.id(), trackPrivate.label(), trackPrivate.language())
    , m_private(trackPrivate)
    , m_enabled(trackPrivate.enabled())
    , m_configuration(AudioTrackConfiguration::create())
{
    m_private->setClient(*this);
    updateKindFromPrivate();
    updateConfigurationFromPrivate();
}

AudioTrack::~AudioTrack()
{
    m_private->clearClient();
}

void AudioTrack::setPrivate(AudioTrackPrivate& trackPrivate)
{
    if (m_private.ptr() == &trackPrivate)
        return;

    m_private->clearClient();
    m_private = trackPrivate;
    m_private->setEnabled(m_enabled);
    m_private->setClient(*this);
#if !RELEASE_LOG_DISABLED
    m_private->setLogger(logger(), logIdentifier());
#endif

    updateKindFromPrivate();
    updateConfigurationFromPrivate();
    setId(m_private->id());
}

void AudioTrack::setLanguage(const AtomString& language)
{
    TrackBase::setLanguage(language);

    m_clients.forEach([&] (auto& client) {
        client.audioTrackLanguageChanged(*this);
    });
}

bool AudioTrack::isValidKind(const AtomString& value) const
{
    return value == alternativeAtom()
        || value == commentaryAtom()
        || value == descriptionKeyword()
        || value == mainAtom()
        || value == mainDescKeyword()
        || value == translationKeyword();
}

void AudioTrack::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_private->setEnabled(enabled);
    m_clients.forEach([this] (auto& client) {
        client.audioTrackEnabledChanged(*this);
    });
}

void AudioTrack::addClient(AudioTrackClient& client)
{
    ASSERT(!m_clients.contains(client));
    m_clients.add(client);
}

void AudioTrack::clearClient(AudioTrackClient& client)
{
    ASSERT(m_clients.contains(client));
    m_clients.remove(client);
}

size_t AudioTrack::inbandTrackIndex() const
{
    return m_private->trackIndex();
}

void AudioTrack::enabledChanged(bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_enabled = enabled;

    m_clients.forEach([this] (auto& client) {
        client.audioTrackEnabledChanged(*this);
    });
}

void AudioTrack::configurationChanged(const PlatformAudioTrackConfiguration& configuration)
{
    m_configuration->setState(configuration);
}

void AudioTrack::idChanged(TrackID id)
{
    setId(id);
    m_clients.forEach([this] (auto& client) {
        client.audioTrackIdChanged(*this);
    });
}

void AudioTrack::labelChanged(const AtomString& label)
{
    setLabel(label);
    m_clients.forEach([this] (auto& client) {
        client.audioTrackLabelChanged(*this);
    });
}

void AudioTrack::languageChanged(const AtomString& language)
{
    setLanguage(language);
}

void AudioTrack::willRemove()
{
    m_clients.forEach([this] (auto& client) {
        client.willRemoveAudioTrack(*this);
    });
}

void AudioTrack::updateKindFromPrivate()
{
    switch (m_private->kind()) {
    case AudioTrackPrivate::Kind::Alternative:
        setKind(alternativeAtom());
        break;
    case AudioTrackPrivate::Kind::Description:
        setKind(AudioTrack::descriptionKeyword());
        break;
    case AudioTrackPrivate::Kind::Main:
        setKind(mainAtom());
        break;
    case AudioTrackPrivate::Kind::MainDesc:
        setKind(AudioTrack::mainDescKeyword());
        break;
    case AudioTrackPrivate::Kind::Translation:
        setKind(AudioTrack::translationKeyword());
        break;
    case AudioTrackPrivate::Kind::Commentary:
        setKind(commentaryAtom());
        break;
    case AudioTrackPrivate::Kind::None:
        setKind(emptyAtom());
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

void AudioTrack::updateConfigurationFromPrivate()
{
    m_configuration->setState(m_private->configuration());
}

#if !RELEASE_LOG_DISABLED
void AudioTrack::setLogger(const Logger& logger, const void* logIdentifier)
{
    TrackBase::setLogger(logger, logIdentifier);
    m_private->setLogger(logger, this->logIdentifier());
}
#endif

} // namespace WebCore

#endif
