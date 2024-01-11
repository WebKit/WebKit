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
#include "VideoTrack.h"

#if ENABLE(VIDEO)

#include "CommonAtomStrings.h"
#include "ScriptExecutionContext.h"
#include "VideoTrackClient.h"
#include "VideoTrackConfiguration.h"
#include "VideoTrackList.h"
#include "VideoTrackPrivate.h"
#include <wtf/NeverDestroyed.h>

#if ENABLE(MEDIA_SOURCE)
#include "SourceBuffer.h"
#endif

namespace WebCore {

const AtomString& VideoTrack::signKeyword()
{
    static MainThreadNeverDestroyed<const AtomString> sign("sign"_s);
    return sign;
}

VideoTrack::VideoTrack(ScriptExecutionContext* context, VideoTrackPrivate& trackPrivate)
    : MediaTrackBase(context, MediaTrackBase::VideoTrack, trackPrivate.trackUID(), trackPrivate.id(), trackPrivate.label(), trackPrivate.language())
    , m_private(trackPrivate)
    , m_configuration(VideoTrackConfiguration::create())
    , m_selected(trackPrivate.selected())
{
    m_private->setClient(*this);
    updateKindFromPrivate();
    updateConfigurationFromPrivate();
}

VideoTrack::~VideoTrack()
{
    m_private->clearClient();
}

void VideoTrack::setPrivate(VideoTrackPrivate& trackPrivate)
{
    if (m_private.ptr() == &trackPrivate)
        return;

    m_private->clearClient();
    m_private = trackPrivate;
    m_private->setClient(*this);
#if !RELEASE_LOG_DISABLED
    m_private->setLogger(logger(), logIdentifier());
#endif

    m_private->setSelected(m_selected);
    updateKindFromPrivate();
    updateConfigurationFromPrivate();
    setId(m_private->id());
}

bool VideoTrack::isValidKind(const AtomString& value) const
{
    return value == alternativeAtom()
        || value == commentaryAtom()
        || value == captionsAtom()
        || value == mainAtom()
        || value == signKeyword()
        || value == subtitlesAtom();
}

void VideoTrack::setSelected(const bool selected)
{
    if (m_selected == selected)
        return;

    m_selected = selected;
    m_private->setSelected(selected);

    m_clients.forEach([this] (auto& client) {
        client.videoTrackSelectedChanged(*this);
    });
}

void VideoTrack::addClient(VideoTrackClient& client)
{
    ASSERT(!m_clients.contains(client));
    m_clients.add(client);
}

void VideoTrack::clearClient(VideoTrackClient& client)
{
    ASSERT(m_clients.contains(client));
    m_clients.remove(client);
}

size_t VideoTrack::inbandTrackIndex()
{
    return m_private->trackIndex();
}

void VideoTrack::selectedChanged(bool selected)
{
    setSelected(selected);
    m_clients.forEach([this] (auto& client) {
        client.videoTrackSelectedChanged(*this);
    });
}

void VideoTrack::configurationChanged(const PlatformVideoTrackConfiguration& configuration)
{
    m_configuration->setState(configuration);
}

void VideoTrack::idChanged(TrackID id)
{
    setId(id);
    m_clients.forEach([this] (auto& client) {
        client.videoTrackIdChanged(*this);
    });
}

void VideoTrack::labelChanged(const AtomString& label)
{
    setLabel(label);
    m_clients.forEach([this] (auto& client) {
        client.videoTrackLabelChanged(*this);
    });
}

void VideoTrack::languageChanged(const AtomString& language)
{
    setLanguage(language);
}

void VideoTrack::willRemove()
{
    m_clients.forEach([this] (auto& client) {
        client.willRemoveVideoTrack(*this);
    });
}

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
    // 4. Queue a task to fire a simple event named change at the VideoTrackList object referenced by
    // the videoTracks attribute on the HTMLMediaElement.
    m_clients.forEach([this] (auto& client) {
        client.videoTrackKindChanged(*this);
    });
}

void VideoTrack::setLanguage(const AtomString& language)
{
    // 10.1 language, on setting:
    // 1. If the value being assigned to this attribute is not an empty string or a BCP 47 language
    // tag[BCP47], then abort these steps.
    // BCP 47 validation is done in TrackBase::setLanguage() which is
    // shared between all tracks that support setting language.

    // 2. Update this attribute to the new value.
    TrackBase::setLanguage(language);

    // 3. If the sourceBuffer attribute on this track is not null, then queue a task to fire a simple
    // event named change at sourceBuffer.videoTracks.
    // 4. Queue a task to fire a simple event named change at the VideoTrackList object referenced by
    // the videoTracks attribute on the HTMLMediaElement.
    m_clients.forEach([&] (auto& client) {
        client.videoTrackLanguageChanged(*this);
    });
}

void VideoTrack::updateKindFromPrivate()
{
    switch (m_private->kind()) {
    case VideoTrackPrivate::Kind::Alternative:
        setKind(alternativeAtom());
        return;
    case VideoTrackPrivate::Kind::Captions:
        setKind(captionsAtom());
        return;
    case VideoTrackPrivate::Kind::Main:
        setKind(mainAtom());
        return;
    case VideoTrackPrivate::Kind::Sign:
        setKind(VideoTrack::signKeyword());
        return;
    case VideoTrackPrivate::Kind::Subtitles:
        setKind(subtitlesAtom());
        return;
    case VideoTrackPrivate::Kind::Commentary:
        setKind(commentaryAtom());
        return;
    case VideoTrackPrivate::Kind::None:
        setKind(emptyAtom());
        return;
    }
    ASSERT_NOT_REACHED();
}

void VideoTrack::updateConfigurationFromPrivate()
{
    m_configuration->setState(m_private->configuration());
}

#if !RELEASE_LOG_DISABLED
void VideoTrack::setLogger(const Logger& logger, const void* logIdentifier)
{
    TrackBase::setLogger(logger, logIdentifier);
    m_private->setLogger(logger, this->logIdentifier());
}
#endif

} // namespace WebCore

#endif
