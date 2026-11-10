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
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AudioTrack);

const RefPtr<AtomStringImpl>& AudioTrack::descriptionKeyword()
{
    static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> description("description"_s);
    return description;
}

const RefPtr<AtomStringImpl>& AudioTrack::mainDescKeyword()
{
    static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> mainDesc("main-desc"_s);
    return mainDesc;
}

const RefPtr<AtomStringImpl>& AudioTrack::translationKeyword()
{
    static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> translation("translation"_s);
    return translation;
}

AudioTrack::AudioTrack(ScriptExecutionContext* context, AudioTrackPrivate& trackPrivate)
    : MediaTrackBase(context, MediaTrackBase::AudioTrack, trackPrivate.trackUID(), trackPrivate.id(), trackPrivate.label(), trackPrivate.language())
    , m_private(trackPrivate)
    , m_enabled(trackPrivate.enabled())
    , m_configuration(AudioTrackConfiguration::create())
{
    addClientToTrackPrivateBase(*this, trackPrivate);

    updateKindFromPrivate();
    updateConfigurationFromPrivate();
}

AudioTrack::~AudioTrack()
{
    removeClientFromTrackPrivateBase(protect(m_private));
}

void AudioTrack::setPrivate(AudioTrackPrivate& trackPrivate)
{
    if (m_private.ptr() == &trackPrivate)
        return;

    removeClientFromTrackPrivateBase(protect(m_private));
    m_private = trackPrivate;
    trackPrivate.setEnabled(m_enabled);
    addClientToTrackPrivateBase(*this, trackPrivate);

#if !RELEASE_LOG_DISABLED
    trackPrivate.setLogger(protect(logger()), logIdentifier());
#endif

    updateKindFromPrivate();
    updateConfigurationFromPrivate();
    setId(trackPrivate.id());
}

void AudioTrack::setLanguage(AtomStringImpl* language)
{
    TrackBase::setLanguage(language);

    m_clients.forEach([&] (auto& client) {
        client.audioTrackLanguageChanged(*this);
    });
}

bool AudioTrack::isValidKind(AtomStringImpl* value) const
{
    return equal(value, "alternative"_s)
        || equal(value, "commentary"_s)
        || equal(value, "description"_s)
        || equal(value, "main"_s)
        || equal(value, "main-desc"_s)
        || equal(value, "translation"_s);
}

void AudioTrack::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    protect(m_private)->setEnabled(enabled);
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
    return protect(m_private)->trackIndex();
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
    if (m_configuration->updateState(configuration) == AudioTrackConfiguration::StateChanged::No)
        return;
    m_clients.forEach([this] (auto& client) {
        client.audioTrackConfigurationChanged(*this);
    });
}

void AudioTrack::idChanged(TrackID id)
{
    setId(id);
    m_clients.forEach([this] (auto& client) {
        client.audioTrackIdChanged(*this);
    });
}

void AudioTrack::labelChanged(StringImpl* label)
{
    setLabel(AtomStringImpl::add(isolatedCopy(label)).get());
    m_clients.forEach([this] (auto& client) {
        client.audioTrackLabelChanged(*this);
    });
}

void AudioTrack::languageChanged(StringImpl* language)
{
    setLanguage(AtomStringImpl::add(isolatedCopy(language)).get());
}

void AudioTrack::willRemove()
{
    m_clients.forEach([this] (auto& client) {
        client.willRemoveAudioTrack(*this);
    });
}

void AudioTrack::updateKindFromPrivate()
{
    switch (protect(m_private)->kind()) {
    case AudioTrackPrivate::Kind::Alternative:
        setKind("alternative"_atom);
        break;
    case AudioTrackPrivate::Kind::Description:
        setKind("description"_atom);
        break;
    case AudioTrackPrivate::Kind::Main:
        setKind("main"_atom);
        break;
    case AudioTrackPrivate::Kind::MainDesc:
        setKind("main-desc"_atom);
        break;
    case AudioTrackPrivate::Kind::Translation:
        setKind("translation"_atom);
        break;
    case AudioTrackPrivate::Kind::Commentary:
        setKind("commentary"_atom);
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
    configurationChanged(m_private->configuration());
}

#if !RELEASE_LOG_DISABLED
void AudioTrack::setLogger(const Logger& logger, uint64_t logIdentifier)
{
    TrackBase::setLogger(logger, logIdentifier);
    protect(m_private)->setLogger(logger, this->logIdentifier());
}
#endif

} // namespace WebCore

#endif
