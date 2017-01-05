/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO_TRACK)

#include "HTMLMediaElement.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

const AtomicString& AudioTrack::alternativeKeyword()
{
    static NeverDestroyed<AtomicString> alternative("alternative", AtomicString::ConstructFromLiteral);
    return alternative;
}

const AtomicString& AudioTrack::descriptionKeyword()
{
    static NeverDestroyed<AtomicString> description("description", AtomicString::ConstructFromLiteral);
    return description;
}

const AtomicString& AudioTrack::mainKeyword()
{
    static NeverDestroyed<AtomicString> main("main", AtomicString::ConstructFromLiteral);
    return main;
}

const AtomicString& AudioTrack::mainDescKeyword()
{
    static NeverDestroyed<AtomicString> mainDesc("main-desc", AtomicString::ConstructFromLiteral);
    return mainDesc;
}

const AtomicString& AudioTrack::translationKeyword()
{
    static NeverDestroyed<AtomicString> translation("translation", AtomicString::ConstructFromLiteral);
    return translation;
}

const AtomicString& AudioTrack::commentaryKeyword()
{
    static NeverDestroyed<AtomicString> commentary("commentary", AtomicString::ConstructFromLiteral);
    return commentary;
}

AudioTrack::AudioTrack(AudioTrackClient& client, AudioTrackPrivate& trackPrivate)
    : MediaTrackBase(MediaTrackBase::AudioTrack, trackPrivate.id(), trackPrivate.label(), trackPrivate.language())
    , m_enabled(trackPrivate.enabled())
    , m_client(&client)
    , m_private(trackPrivate)
{
    m_private->setClient(this);
    updateKindFromPrivate();
}

AudioTrack::~AudioTrack()
{
    m_private->setClient(nullptr);
}

void AudioTrack::setPrivate(AudioTrackPrivate& trackPrivate)
{
    if (m_private.ptr() == &trackPrivate)
        return;

    m_private->setClient(nullptr);
    m_private = trackPrivate;
    m_private->setEnabled(m_enabled);
    m_private->setClient(this);

    updateKindFromPrivate();
}

bool AudioTrack::isValidKind(const AtomicString& value) const
{
    return value == alternativeKeyword()
        || value == commentaryKeyword()
        || value == descriptionKeyword()
        || value == mainKeyword()
        || value == mainDescKeyword()
        || value == translationKeyword();
}

void AudioTrack::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_private->setEnabled(enabled);
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

    if (m_client)
        m_client->audioTrackEnabledChanged(*this);
}

void AudioTrack::idChanged(const AtomicString& id)
{
    setId(id);
}

void AudioTrack::labelChanged(const AtomicString& label)
{
    setLabel(label);
}

void AudioTrack::languageChanged(const AtomicString& language)
{
    setLanguage(language);
}

void AudioTrack::willRemove()
{
    mediaElement()->removeAudioTrack(*this);
}

void AudioTrack::updateKindFromPrivate()
{
    switch (m_private->kind()) {
    case AudioTrackPrivate::Alternative:
        setKind(AudioTrack::alternativeKeyword());
        break;
    case AudioTrackPrivate::Description:
        setKind(AudioTrack::descriptionKeyword());
        break;
    case AudioTrackPrivate::Main:
        setKind(AudioTrack::mainKeyword());
        break;
    case AudioTrackPrivate::MainDesc:
        setKind(AudioTrack::mainDescKeyword());
        break;
    case AudioTrackPrivate::Translation:
        setKind(AudioTrack::translationKeyword());
        break;
    case AudioTrackPrivate::Commentary:
        setKind(AudioTrack::commentaryKeyword());
        break;
    case AudioTrackPrivate::None:
        setKind(emptyString());
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

} // namespace WebCore

#endif
