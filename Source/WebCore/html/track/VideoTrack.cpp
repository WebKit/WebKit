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
#include "VideoTrack.h"

#if ENABLE(VIDEO_TRACK)

#include "HTMLMediaElement.h"
#include "VideoTrackList.h"
#include <wtf/NeverDestroyed.h>

#if ENABLE(MEDIA_SOURCE)
#include "SourceBuffer.h"
#endif

namespace WebCore {

const AtomString& VideoTrack::alternativeKeyword()
{
    static NeverDestroyed<const AtomString> alternative("alternative", AtomString::ConstructFromLiteral);
    return alternative;
}

const AtomString& VideoTrack::captionsKeyword()
{
    static NeverDestroyed<const AtomString> captions("captions", AtomString::ConstructFromLiteral);
    return captions;
}

const AtomString& VideoTrack::mainKeyword()
{
    static NeverDestroyed<const AtomString> captions("main", AtomString::ConstructFromLiteral);
    return captions;
}

const AtomString& VideoTrack::signKeyword()
{
    static NeverDestroyed<const AtomString> sign("sign", AtomString::ConstructFromLiteral);
    return sign;
}

const AtomString& VideoTrack::subtitlesKeyword()
{
    static NeverDestroyed<const AtomString> subtitles("subtitles", AtomString::ConstructFromLiteral);
    return subtitles;
}

const AtomString& VideoTrack::commentaryKeyword()
{
    static NeverDestroyed<const AtomString> commentary("commentary", AtomString::ConstructFromLiteral);
    return commentary;
}

VideoTrack::VideoTrack(VideoTrackClient& client, VideoTrackPrivate& trackPrivate)
    : MediaTrackBase(MediaTrackBase::VideoTrack, trackPrivate.id(), trackPrivate.label(), trackPrivate.language())
    , m_client(&client)
    , m_private(trackPrivate)
    , m_selected(trackPrivate.selected())
{
#if !RELEASE_LOG_DISABLED
    m_private->setLogger(logger(), logIdentifier());
#endif
    m_private->setClient(this);
    updateKindFromPrivate();
}

VideoTrack::~VideoTrack()
{
    m_private->setClient(nullptr);
}

void VideoTrack::setPrivate(VideoTrackPrivate& trackPrivate)
{
    if (m_private.ptr() == &trackPrivate)
        return;

    m_private->setClient(nullptr);
    m_private = trackPrivate;
    m_private->setClient(this);
#if !RELEASE_LOG_DISABLED
    m_private->setLogger(logger(), logIdentifier());
#endif

    m_private->setSelected(m_selected);
    updateKindFromPrivate();
}

bool VideoTrack::isValidKind(const AtomString& value) const
{
    return value == alternativeKeyword()
        || value == commentaryKeyword()
        || value == captionsKeyword()
        || value == mainKeyword()
        || value == signKeyword()
        || value == subtitlesKeyword();
}

void VideoTrack::setSelected(const bool selected)
{
    if (m_selected == selected)
        return;

    m_selected = selected;
    m_private->setSelected(selected);

    if (m_client)
        m_client->videoTrackSelectedChanged(*this);
}

size_t VideoTrack::inbandTrackIndex()
{
    return m_private->trackIndex();
}

void VideoTrack::selectedChanged(bool selected)
{
    setSelected(selected);
}

void VideoTrack::idChanged(const AtomString& id)
{
    setId(id);
}

void VideoTrack::labelChanged(const AtomString& label)
{
    setLabel(label);
}

void VideoTrack::languageChanged(const AtomString& language)
{
    setLanguage(language);
}

void VideoTrack::willRemove()
{
    auto element = makeRefPtr(mediaElement().get());
    if (!element)
        return;
    element->removeVideoTrack(*this);
}

#if ENABLE(MEDIA_SOURCE)

void VideoTrack::setKind(const AtomString& kind)
{
    // 10.1 kind, on setting:
    // 1. If the value being assigned to this attribute does not match one of the video track kinds,
    // then abort these steps.
    if (!isValidKind(kind))
        return;

    // 2. Update this attribute to the new value.
    setKindInternal(kind);

    // 3. If the sourceBuffer attribute on this track is not null, then queue a task to fire a simple
    // event named change at sourceBuffer.videoTracks.
    if (m_sourceBuffer)
        m_sourceBuffer->videoTracks().scheduleChangeEvent();

    // 4. Queue a task to fire a simple event named change at the VideoTrackList object referenced by
    // the videoTracks attribute on the HTMLMediaElement.
    mediaElement()->ensureVideoTracks().scheduleChangeEvent();
}

void VideoTrack::setLanguage(const AtomString& language)
{
    // 10.1 language, on setting:
    // 1. If the value being assigned to this attribute is not an empty string or a BCP 47 language
    // tag[BCP47], then abort these steps.
    // BCP 47 validation is done in TrackBase::setLanguage() which is
    // shared between all tracks that support setting language.

    // 2. Update this attribute to the new value.
    MediaTrackBase::setLanguage(language);

    // 3. If the sourceBuffer attribute on this track is not null, then queue a task to fire a simple
    // event named change at sourceBuffer.videoTracks.
    if (m_sourceBuffer)
        m_sourceBuffer->videoTracks().scheduleChangeEvent();

    // 4. Queue a task to fire a simple event named change at the VideoTrackList object referenced by
    // the videoTracks attribute on the HTMLMediaElement.
    if (mediaElement())
        mediaElement()->ensureVideoTracks().scheduleChangeEvent();
}

#endif

void VideoTrack::updateKindFromPrivate()
{
    switch (m_private->kind()) {
    case VideoTrackPrivate::Alternative:
        setKindInternal(VideoTrack::alternativeKeyword());
        return;
    case VideoTrackPrivate::Captions:
        setKindInternal(VideoTrack::captionsKeyword());
        return;
    case VideoTrackPrivate::Main:
        setKindInternal(VideoTrack::mainKeyword());
        return;
    case VideoTrackPrivate::Sign:
        setKindInternal(VideoTrack::signKeyword());
        return;
    case VideoTrackPrivate::Subtitles:
        setKindInternal(VideoTrack::subtitlesKeyword());
        return;
    case VideoTrackPrivate::Commentary:
        setKindInternal(VideoTrack::commentaryKeyword());
        return;
    case VideoTrackPrivate::None:
        setKindInternal(emptyString());
        return;
    }
    ASSERT_NOT_REACHED();
}

void VideoTrack::setMediaElement(WeakPtr<HTMLMediaElement> element)
{
    TrackBase::setMediaElement(element);
#if !RELEASE_LOG_DISABLED
    m_private->setLogger(logger(), logIdentifier());
#endif
}

} // namespace WebCore

#endif
