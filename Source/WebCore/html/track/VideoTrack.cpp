/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 * Copyright (C) 2011, 2012, 2013 Apple Inc.  All rights reserved.
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

#if ENABLE(VIDEO_TRACK)

#include "VideoTrack.h"

#include "Event.h"
#include "HTMLMediaElement.h"
#include "VideoTrackList.h"

#if ENABLE(MEDIA_SOURCE)
#include "SourceBuffer.h"
#endif

namespace WebCore {

const AtomicString& VideoTrack::alternativeKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, alternative, ("alternative", AtomicString::ConstructFromLiteral));
    return alternative;
}

const AtomicString& VideoTrack::captionsKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, captions, ("captions", AtomicString::ConstructFromLiteral));
    return captions;
}

const AtomicString& VideoTrack::mainKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, captions, ("main", AtomicString::ConstructFromLiteral));
    return captions;
}

const AtomicString& VideoTrack::signKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, sign, ("sign", AtomicString::ConstructFromLiteral));
    return sign;
}

const AtomicString& VideoTrack::subtitlesKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, subtitles, ("subtitles", AtomicString::ConstructFromLiteral));
    return subtitles;
}

const AtomicString& VideoTrack::commentaryKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, commentary, ("commentary", AtomicString::ConstructFromLiteral));
    return commentary;
}

VideoTrack::VideoTrack(VideoTrackClient* client, PassRefPtr<VideoTrackPrivate> trackPrivate)
    : TrackBase(TrackBase::VideoTrack, trackPrivate->id(), trackPrivate->label(), trackPrivate->language())
    , m_selected(trackPrivate->selected())
    , m_client(client)
    , m_private(trackPrivate)
{
    m_private->setClient(this);

    switch (m_private->kind()) {
    case VideoTrackPrivate::Alternative:
        setKindInternal(VideoTrack::alternativeKeyword());
        break;
    case VideoTrackPrivate::Captions:
        setKindInternal(VideoTrack::captionsKeyword());
        break;
    case VideoTrackPrivate::Main:
        setKindInternal(VideoTrack::mainKeyword());
        break;
    case VideoTrackPrivate::Sign:
        setKindInternal(VideoTrack::signKeyword());
        break;
    case VideoTrackPrivate::Subtitles:
        setKindInternal(VideoTrack::subtitlesKeyword());
        break;
    case VideoTrackPrivate::Commentary:
        setKindInternal(VideoTrack::commentaryKeyword());
        break;
    case VideoTrackPrivate::None:
        setKindInternal(emptyString());
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

VideoTrack::~VideoTrack()
{
    m_private->setClient(0);
}

bool VideoTrack::isValidKind(const AtomicString& value) const
{
    if (value == alternativeKeyword())
        return true;
    if (value == captionsKeyword())
        return true;
    if (value == mainKeyword())
        return true;
    if (value == signKeyword())
        return true;
    if (value == subtitlesKeyword())
        return true;
    if (value == commentaryKeyword())
        return true;

    return false;
}

void VideoTrack::setSelected(const bool selected)
{
    if (m_selected == selected)
        return;

    m_selected = selected;
    m_private->setSelected(selected);

    if (m_client)
        m_client->videoTrackSelectedChanged(this);
}

size_t VideoTrack::inbandTrackIndex()
{
    ASSERT(m_private);
    return m_private->trackIndex();
}

void VideoTrack::selectedChanged(VideoTrackPrivate* trackPrivate, bool selected)
{
    ASSERT_UNUSED(trackPrivate, trackPrivate == m_private);
    setSelected(selected);
}

void VideoTrack::idChanged(TrackPrivateBase* trackPrivate, const AtomicString& id)
{
    ASSERT_UNUSED(trackPrivate, trackPrivate == m_private);
    setId(id);
}

void VideoTrack::labelChanged(TrackPrivateBase* trackPrivate, const AtomicString& label)
{
    ASSERT_UNUSED(trackPrivate, trackPrivate == m_private);
    setLabel(label);
}

void VideoTrack::languageChanged(TrackPrivateBase* trackPrivate, const AtomicString& language)
{
    ASSERT_UNUSED(trackPrivate, trackPrivate == m_private);
    setLanguage(language);
}

void VideoTrack::willRemove(TrackPrivateBase* trackPrivate)
{
    ASSERT_UNUSED(trackPrivate, trackPrivate == m_private);
    mediaElement()->removeVideoTrack(this);
}

#if ENABLE(MEDIA_SOURCE)
void VideoTrack::setKind(const AtomicString& kind)
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
        m_sourceBuffer->videoTracks()->scheduleChangeEvent();

    // 4. Queue a task to fire a simple event named change at the VideoTrackList object referenced by
    // the videoTracks attribute on the HTMLMediaElement.
    mediaElement()->videoTracks()->scheduleChangeEvent();
}

void VideoTrack::setLanguage(const AtomicString& language)
{
    // 10.1 language, on setting:
    // 1. If the value being assigned to this attribute is not an empty string or a BCP 47 language
    // tag[BCP47], then abort these steps.
    // FIXME(123926): Validate the BCP47-ness of langague.

    // 2. Update this attribute to the new value.
    TrackBase::setLanguage(language);

    // 3. If the sourceBuffer attribute on this track is not null, then queue a task to fire a simple
    // event named change at sourceBuffer.videoTracks.
    if (m_sourceBuffer)
        m_sourceBuffer->videoTracks()->scheduleChangeEvent();

    // 4. Queue a task to fire a simple event named change at the VideoTrackList object referenced by
    // the videoTracks attribute on the HTMLMediaElement.
    mediaElement()->videoTracks()->scheduleChangeEvent();
}
#endif

} // namespace WebCore

#endif
