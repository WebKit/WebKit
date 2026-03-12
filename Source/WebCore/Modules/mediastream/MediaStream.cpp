/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2012, 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
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

#include "ContextDestructionObserverInlines.h"
#include "DocumentPage.h"
#include "Event.h"
#include "EventNames.h"
#include "EventTargetInlines.h"
#include "Logging.h"
#include "MediaSessionManagerInterface.h"
#include "MediaStreamTrackEvent.h"
#include "Page.h"
#include "RealtimeMediaSource.h"
#include "ScriptWrappableInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaStream);

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

Ref<MediaStream> MediaStream::create(Document& document, Vector<Ref<MediaStreamTrack>>&& tracks, CheckDuplicate checkDuplicate)
{
    if (checkDuplicate == CheckDuplicate::Yes) {
        HashSet<MediaStreamTrack*> existingTracks;
        Vector<size_t> tracksToRemove;
        for (size_t counter = 0; counter < tracks.size(); ++counter) {
            if (!existingTracks.add(tracks[counter].ptr()).isNewEntry)
                tracksToRemove.append(counter);
        }
        for (auto iterator = tracksToRemove.rbegin(); iterator != tracksToRemove.rend(); ++iterator)
            tracks.removeAt(*iterator);
    }

    auto mediaStream = adoptRef(*new MediaStream(document, WTF::move(tracks)));
    mediaStream->suspendIfNeeded();
    return mediaStream;
}

Ref<MediaStream> MediaStream::create(Document& document, Ref<MediaStreamPrivate>&& streamPrivate, AllowEventTracks allowEventTracks)
{
    auto mediaStream = adoptRef(*new MediaStream(document, WTF::move(streamPrivate), allowEventTracks));
    mediaStream->suspendIfNeeded();
    return mediaStream;
}

static inline MediaStreamTrackPrivateVector createTrackPrivateVector(const Vector<Ref<MediaStreamTrack>>& tracks)
{
    return map(tracks, [](auto& track) { return Ref { track->privateTrack() }; });
}

MediaStream::MediaStream(Document& document, Vector<Ref<MediaStreamTrack>>&& tracks)
    : ActiveDOMObject(document)
    , m_private(MediaStreamPrivate::create(document.logger(), createTrackPrivateVector(tracks)))
    , m_tracks(WTF::move(tracks))
{
    // This constructor preserves MediaStreamTrack instances and must be used by calls originating
    // from the JavaScript MediaStream constructor.

    for (Ref track : m_tracks)
        track->setMediaStreamId(id());

    setIsActive(m_private->active());
    m_private->addObserver(*this);
}

MediaStream::MediaStream(Document& document, Ref<MediaStreamPrivate>&& streamPrivate, AllowEventTracks allowEventTracks)
    : ActiveDOMObject(document)
    , m_private(WTF::move(streamPrivate))
    , m_allowEventTracks(allowEventTracks)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    for (auto& trackPrivate : m_private->tracks()) {
        auto track = MediaStreamTrack::create(document, trackPrivate.get());
        track->setMediaStreamId(id());
        m_tracks.append(WTF::move(track));
    }

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

    RefPtr document = this->document();
    if (!document)
        return nullptr;

    Vector<Ref<MediaStreamTrack>> clonedTracks;
    clonedTracks.reserveInitialCapacity(m_tracks.size());
    for (Ref track : m_tracks) {
        if (auto clone = track->clone())
            clonedTracks.append(clone.releaseNonNull());
    }
    return MediaStream::create(*document, WTF::move(clonedTracks), CheckDuplicate::No);
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
    auto position = m_tracks.findIf([&](auto& track) {
        return track->id() == id;
    });
    if (position == notFound)
        return nullptr;

    return m_tracks[position].ptr();
}

MediaStreamTrack* MediaStream::getFirstAudioTrack() const
{
    for (auto& track : m_tracks) {
        if (track->isAudio())
            return track.ptr();
    }
    return nullptr;
}

MediaStreamTrack* MediaStream::getFirstVideoTrack() const
{
    for (auto& track : m_tracks) {
        if (track->isVideo())
            return track.ptr();
    }
    return nullptr;
}

MediaStreamTrackVector MediaStream::getAudioTracks() const
{
    MediaStreamTrackVector tracks;
    for (Ref track : m_tracks) {
        if (track->isAudio())
            tracks.append(WTF::move(track));
    }
    return tracks;
}

MediaStreamTrackVector MediaStream::getVideoTracks() const
{
    MediaStreamTrackVector tracks;
    for (Ref track : m_tracks) {
        if (track->isVideo())
            tracks.append(WTF::move(track));
    }
    return tracks;
}

MediaStreamTrackVector MediaStream::getTracks() const
{
    return m_tracks;
}

void MediaStream::activeStatusChanged()
{
    updateActiveState();
}

void MediaStream::didAddTrack(MediaStreamTrackPrivate& trackPrivate)
{
    RefPtr context = scriptExecutionContext();
    if (!context)
        return;

    if (getTrackById(trackPrivate.id()))
        return;

    Ref track = MediaStreamTrack::create(*context, trackPrivate);
    internalAddTrack(track.copyRef());
    ASSERT(m_allowEventTracks == AllowEventTracks::Yes);
    dispatchEvent(MediaStreamTrackEvent::create(eventNames().addtrackEvent, Event::CanBubble::No, Event::IsCancelable::No, WTF::move(track)));
}

void MediaStream::didRemoveTrack(MediaStreamTrackPrivate& trackPrivate)
{
    if (RefPtr track = internalTakeTrack(trackPrivate.id())) {
        ASSERT(m_allowEventTracks == AllowEventTracks::Yes);
        dispatchEvent(MediaStreamTrackEvent::create(eventNames().removetrackEvent, Event::CanBubble::No, Event::IsCancelable::No, track.releaseNonNull()));
    }
}

void MediaStream::addTrackFromPlatform(Ref<MediaStreamTrack>&& track)
{
    ALWAYS_LOG(LOGIDENTIFIER, track->logIdentifier());
    ASSERT(m_allowEventTracks == AllowEventTracks::Yes);

    auto& privateTrack = track->privateTrack();
    internalAddTrack(track.copyRef());
    m_private->addTrack(privateTrack);
    dispatchEvent(MediaStreamTrackEvent::create(eventNames().addtrackEvent, Event::CanBubble::No, Event::IsCancelable::No, WTF::move(track)));
}

void MediaStream::internalAddTrack(Ref<MediaStreamTrack>&& track)
{
    ASSERT(!getTrackById(track->id()));
    m_tracks.append(WTF::move(track));
    updateActiveState();
}

RefPtr<MediaStreamTrack> MediaStream::internalTakeTrack(const String& trackId)
{
    auto position = m_tracks.findIf([&](auto& track) {
        return track->id() == trackId;
    });
    if (position == notFound)
        return nullptr;

    RefPtr track = m_tracks[position].get();
    m_tracks.removeAt(position);

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
    RefPtr document = this->document();
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

    if (!getAudioTracks().isEmpty()) {
        if (RefPtr manager = mediaSessionManager())
            manager->sessionCanProduceAudioChanged();
    }
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

    for (Ref track : m_tracks)
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
    for (Ref track : m_tracks) {
        if (!track->ended()) {
            active = true;
            break;
        }
    }

    if (m_isActive == active)
        return;

    setIsActive(active);
}

Document* MediaStream::document() const
{
    return downcast<Document>(scriptExecutionContext());
}

void MediaStream::inactivate()
{
    m_isActive = false;
}

bool MediaStream::virtualHasPendingActivity() const
{
    return m_isActive && m_allowEventTracks == AllowEventTracks::Yes && hasEventListeners();
}

ScriptExecutionContext* MediaStream::scriptExecutionContext() const
{
    return ContextDestructionObserver::scriptExecutionContext();
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaStream::logChannel() const
{
    return LogWebRTC;
}
#endif

RefPtr<MediaSessionManagerInterface> MediaStream::mediaSessionManager() const
{
    RefPtr document = dynamicDowncast<Document>(scriptExecutionContext());
    if (!document)
        return nullptr;

    RefPtr page = document->page();
    if (!page)
        return nullptr;

    return page->mediaSessionManager();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
