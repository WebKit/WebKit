/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2012, 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2013-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaStream.h"

#if ENABLE(MEDIA_STREAM)

#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "Logging.h"
#include "MediaStreamTrackEvent.h"
#include "Page.h"
#include "RealtimeMediaSource.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(MediaStream);

Ref<MediaStream> MediaStream::create(Document& document)
{
    return MediaStream::create(document, MediaStreamPrivate::create(document.logger(), { }));
}

Ref<MediaStream> MediaStream::create(Document& document, MediaStream& stream)
{
    auto mediaStream = adoptRef(*new MediaStream(document, stream.getTracks()));
    mediaStream->suspendIfNeeded();
    return mediaStream;
}

Ref<MediaStream> MediaStream::create(Document& document, const Vector<Ref<MediaStreamTrack>>& tracks)
{
    auto mediaStream = adoptRef(*new MediaStream(document, tracks));
    mediaStream->suspendIfNeeded();
    return mediaStream;
}

Ref<MediaStream> MediaStream::create(Document& document, Ref<MediaStreamPrivate>&& streamPrivate)
{
    auto mediaStream = adoptRef(*new MediaStream(document, WTFMove(streamPrivate)));
    mediaStream->suspendIfNeeded();
    return mediaStream;
}

static inline MediaStreamTrackPrivateVector createTrackPrivateVector(const Vector<Ref<MediaStreamTrack>>& tracks)
{
    return map(tracks, [](auto& track) { return Ref { track->privateTrack() }; });
}

MediaStream::MediaStream(Document& document, const Vector<Ref<MediaStreamTrack>>& tracks)
    : ActiveDOMObject(document)
    , m_private(MediaStreamPrivate::create(document.logger(), createTrackPrivateVector(tracks)))
{
    // This constructor preserves MediaStreamTrack instances and must be used by calls originating
    // from the JavaScript MediaStream constructor.

    for (auto& track : tracks)
        m_trackMap.add(track->id(), track);

    setIsActive(m_private->active());
    m_private->addObserver(*this);
}

MediaStream::MediaStream(Document& document, Ref<MediaStreamPrivate>&& streamPrivate)
    : ActiveDOMObject(document)
    , m_private(WTFMove(streamPrivate))
{
    ALWAYS_LOG(LOGIDENTIFIER);

    for (auto& trackPrivate : m_private->tracks())
        m_trackMap.add(trackPrivate->id(), MediaStreamTrack::create(document, trackPrivate.get()));

    setIsActive(m_private->active());
    m_private->addObserver(*this);
}

MediaStream::~MediaStream()
{
    // Set isActive to false immediately so any callbacks triggered by shutting down, e.g.
    // mediaState(), are short circuited.
    m_isActive = false;
    m_private->removeObserver(*this);
    if (WeakPtr document = this->document()) {
        if (m_isWaitingUntilMediaCanStart)
            document->removeMediaCanStartListener(*this);
    }
}

RefPtr<MediaStream> MediaStream::clone()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    Vector<Ref<MediaStreamTrack>> clonedTracks;
    clonedTracks.reserveInitialCapacity(m_trackMap.size());
    for (auto& track : m_trackMap.values()) {
        if (auto clone = track->clone())
            clonedTracks.append(clone.releaseNonNull());
    }
    return MediaStream::create(*document(), WTFMove(clonedTracks));
}

void MediaStream::addTrack(MediaStreamTrack& track)
{
    ALWAYS_LOG(LOGIDENTIFIER, track.logIdentifier());
    if (getTrackById(track.privateTrack().id()))
        return;

    internalAddTrack(track);
    m_private->addTrack(track.privateTrack());
}

void MediaStream::removeTrack(MediaStreamTrack& track)
{
    ALWAYS_LOG(LOGIDENTIFIER, track.logIdentifier());
    if (auto taken = internalTakeTrack(track.id())) {
        ASSERT(taken.get() == &track);
        m_private->removeTrack(track.privateTrack());
    }
}

MediaStreamTrack* MediaStream::getTrackById(String id)
{
    auto iterator = m_trackMap.find(id);
    if (iterator != m_trackMap.end())
        return iterator->value.ptr();

    return nullptr;
}

MediaStreamTrack* MediaStream::getFirstAudioTrack() const
{
    for (auto& track : m_trackMap.values()) {
        if (track->isAudio())
            return track.ptr();
    }
    return nullptr;
}

MediaStreamTrack* MediaStream::getFirstVideoTrack() const
{
    for (auto& track : m_trackMap.values()) {
        if (track->isVideo())
            return track.ptr();
    }
    return nullptr;
}

MediaStreamTrackVector MediaStream::getAudioTracks() const
{
    return filteredTracks([] (auto& track) mutable {
        return track.isAudio();
    });
}

MediaStreamTrackVector MediaStream::getVideoTracks() const
{
    return filteredTracks([] (auto& track) mutable {
        return track.isVideo();
    });
}

MediaStreamTrackVector MediaStream::getTracks() const
{
    return copyToVector(m_trackMap.values());
}

void MediaStream::activeStatusChanged()
{
    updateActiveState();
}

void MediaStream::didAddTrack(MediaStreamTrackPrivate& trackPrivate)
{
    ScriptExecutionContext* context = scriptExecutionContext();
    if (!context)
        return;

    if (getTrackById(trackPrivate.id()))
        return;

    auto track = MediaStreamTrack::create(*context, trackPrivate);
    internalAddTrack(track.copyRef());
    dispatchEvent(MediaStreamTrackEvent::create(eventNames().addtrackEvent, Event::CanBubble::No, Event::IsCancelable::No, WTFMove(track)));
}

void MediaStream::didRemoveTrack(MediaStreamTrackPrivate& trackPrivate)
{
    if (auto track = internalTakeTrack(trackPrivate.id()))
        dispatchEvent(MediaStreamTrackEvent::create(eventNames().removetrackEvent, Event::CanBubble::No, Event::IsCancelable::No, WTFMove(track)));
}

void MediaStream::addTrackFromPlatform(Ref<MediaStreamTrack>&& track)
{
    ALWAYS_LOG(LOGIDENTIFIER, track->logIdentifier());

    auto& privateTrack = track->privateTrack();
    internalAddTrack(track.copyRef());
    m_private->addTrack(privateTrack);
    dispatchEvent(MediaStreamTrackEvent::create(eventNames().addtrackEvent, Event::CanBubble::No, Event::IsCancelable::No, WTFMove(track)));
}

void MediaStream::internalAddTrack(Ref<MediaStreamTrack>&& trackToAdd)
{
    ASSERT(!m_trackMap.contains(trackToAdd->id()));
    m_trackMap.add(trackToAdd->id(), WTFMove(trackToAdd));
    updateActiveState();
}

RefPtr<MediaStreamTrack> MediaStream::internalTakeTrack(const String& trackId)
{
    auto track = m_trackMap.take(trackId);
    if (track)
        updateActiveState();

    return track;
}

void MediaStream::setIsActive(bool active)
{
    if (m_isActive == active)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, active);

    m_isActive = active;
    statusDidChange();
}

void MediaStream::mediaCanStart(Document& document)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    ASSERT_UNUSED(document, &document == this->document());
    ASSERT(m_isWaitingUntilMediaCanStart);
    if (m_isWaitingUntilMediaCanStart) {
        m_isWaitingUntilMediaCanStart = false;
        startProducingData();
    }
}

void MediaStream::startProducingData()
{
    Document* document = this->document();
    if (!document || !document->page())
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    // If we can't start a load right away, start it later.
    if (!document->page()->canStartMedia()) {
        ALWAYS_LOG(LOGIDENTIFIER, "not allowed to start in background, waiting");
        if (m_isWaitingUntilMediaCanStart)
            return;

        m_isWaitingUntilMediaCanStart = true;
        document->addMediaCanStartListener(*this);
        return;
    }

    if (m_isProducingData)
        return;
    m_isProducingData = true;
    m_private->startProducingData();
}

void MediaStream::stopProducingData()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (!m_isProducingData)
        return;

    m_isProducingData = false;
    m_private->stopProducingData();
}

MediaProducerMediaStateFlags MediaStream::mediaState() const
{
    MediaProducerMediaStateFlags state;

    if (!m_isActive || !document() || !document()->page())
        return state;

    for (const auto& track : m_trackMap.values())
        state.add(track->mediaState());

    return state;
}

void MediaStream::statusDidChange()
{
    if (RefPtr document = this->document()) {
        if (!m_isActive)
            return;
        document->updateIsPlayingMedia();
    }
}

void MediaStream::characteristicsChanged()
{
    auto state = mediaState();
    if (m_state != state) {
        m_state = state;
        statusDidChange();
    }
}

void MediaStream::updateActiveState()
{
    bool active = false;
    for (auto& track : m_trackMap.values()) {
        if (!track->ended()) {
            active = true;
            break;
        }
    }

    if (m_isActive == active)
        return;

    setIsActive(active);
}

MediaStreamTrackVector MediaStream::filteredTracks(const Function<bool(const MediaStreamTrack&)>& filter) const
{
    MediaStreamTrackVector tracks;
    for (auto& track : m_trackMap.values()) {
        if (filter(track))
            tracks.append(track);
    }

    return tracks;
}

Document* MediaStream::document() const
{
    return downcast<Document>(scriptExecutionContext());
}

void MediaStream::stop()
{
    m_isActive = false;
}

bool MediaStream::virtualHasPendingActivity() const
{
    return m_isActive;
}

Ref<MediaStreamPrivate> MediaStream::protectedPrivateStream()
{
    return m_private;
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaStream::logChannel() const
{
    return LogWebRTC;
}
#endif
    
} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
