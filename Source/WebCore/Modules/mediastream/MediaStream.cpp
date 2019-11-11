/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2012, 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#include "Frame.h"
#include "FrameLoader.h"
#include "Logging.h"
#include "MediaStreamTrackEvent.h"
#include "NetworkingContext.h"
#include "Page.h"
#include "RealtimeMediaSource.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/URL.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MediaStream);

Ref<MediaStream> MediaStream::create(Document& document)
{
    return MediaStream::create(document, MediaStreamPrivate::create(document.logger(), { }));
}

Ref<MediaStream> MediaStream::create(Document& document, MediaStream& stream)
{
    return adoptRef(*new MediaStream(document, stream.getTracks()));
}

Ref<MediaStream> MediaStream::create(Document& document, const MediaStreamTrackVector& tracks)
{
    return adoptRef(*new MediaStream(document, tracks));
}

Ref<MediaStream> MediaStream::create(Document& document, Ref<MediaStreamPrivate>&& streamPrivate)
{
    return adoptRef(*new MediaStream(document, WTFMove(streamPrivate)));
}

static inline MediaStreamTrackPrivateVector createTrackPrivateVector(const MediaStreamTrackVector& tracks)
{
    MediaStreamTrackPrivateVector trackPrivates;
    trackPrivates.reserveCapacity(tracks.size());
    for (auto& track : tracks)
        trackPrivates.append(&track->privateTrack());
    return trackPrivates;
}

MediaStream::MediaStream(Document& document, const MediaStreamTrackVector& tracks)
    : ActiveDOMObject(document)
    , m_private(MediaStreamPrivate::create(document.logger(), createTrackPrivateVector(tracks)))
{
    // This constructor preserves MediaStreamTrack instances and must be used by calls originating
    // from the JavaScript MediaStream constructor.

    for (auto& track : tracks) {
        track->addObserver(*this);
        m_trackSet.add(track->id(), track);
    }

    setIsActive(m_private->active());
    m_private->addObserver(*this);
    suspendIfNeeded();
}

MediaStream::MediaStream(Document& document, Ref<MediaStreamPrivate>&& streamPrivate)
    : ActiveDOMObject(document)
    , m_private(WTFMove(streamPrivate))
{
    ALWAYS_LOG(LOGIDENTIFIER);

    for (auto& trackPrivate : m_private->tracks()) {
        auto track = MediaStreamTrack::create(document, *trackPrivate);
        track->addObserver(*this);
        m_trackSet.add(track->id(), WTFMove(track));
    }

    setIsActive(m_private->active());
    m_private->addObserver(*this);
    suspendIfNeeded();
}

MediaStream::~MediaStream()
{
    // Set isActive to false immediately so any callbacks triggered by shutting down, e.g.
    // mediaState(), are short circuited.
    m_isActive = false;
    m_private->removeObserver(*this);
    for (auto& track : m_trackSet.values())
        track->removeObserver(*this);
    if (Document* document = this->document()) {
        if (m_isWaitingUntilMediaCanStart)
            document->removeMediaCanStartListener(*this);
    }
}

RefPtr<MediaStream> MediaStream::clone()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    MediaStreamTrackVector clonedTracks;
    clonedTracks.reserveInitialCapacity(m_trackSet.size());

    for (auto& track : m_trackSet.values())
        clonedTracks.uncheckedAppend(track->clone());

    return MediaStream::create(*document(), clonedTracks);
}

void MediaStream::addTrack(MediaStreamTrack& track)
{
    ALWAYS_LOG(LOGIDENTIFIER, track.logIdentifier());

    if (!internalAddTrack(track, StreamModifier::DomAPI))
        return;

    for (auto& observer : m_observers)
        observer->didAddOrRemoveTrack();
}

void MediaStream::removeTrack(MediaStreamTrack& track)
{
    ALWAYS_LOG(LOGIDENTIFIER, track.logIdentifier());

    if (!internalRemoveTrack(track.id(), StreamModifier::DomAPI))
        return;

    for (auto& observer : m_observers)
        observer->didAddOrRemoveTrack();
}

MediaStreamTrack* MediaStream::getTrackById(String id)
{
    auto it = m_trackSet.find(id);
    if (it != m_trackSet.end())
        return it->value.get();

    return nullptr;
}

MediaStreamTrackVector MediaStream::getAudioTracks() const
{
    return trackVectorForType(RealtimeMediaSource::Type::Audio);
}

MediaStreamTrackVector MediaStream::getVideoTracks() const
{
    return trackVectorForType(RealtimeMediaSource::Type::Video);
}

MediaStreamTrackVector MediaStream::getTracks() const
{
    return copyToVector(m_trackSet.values());
}

void MediaStream::trackDidEnd()
{
    m_private->updateActiveState(MediaStreamPrivate::NotifyClientOption::Notify);
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

    if (!getTrackById(trackPrivate.id()))
        internalAddTrack(MediaStreamTrack::create(*context, trackPrivate), StreamModifier::Platform);
}

void MediaStream::didRemoveTrack(MediaStreamTrackPrivate& trackPrivate)
{
    internalRemoveTrack(trackPrivate.id(), StreamModifier::Platform);
}

void MediaStream::addTrackFromPlatform(Ref<MediaStreamTrack>&& track)
{
    ALWAYS_LOG(LOGIDENTIFIER, track->logIdentifier());

    auto* privateTrack = &track->privateTrack();
    internalAddTrack(WTFMove(track), StreamModifier::Platform);
    m_private->addTrack(privateTrack, MediaStreamPrivate::NotifyClientOption::Notify);
}

bool MediaStream::internalAddTrack(Ref<MediaStreamTrack>&& trackToAdd, StreamModifier streamModifier)
{
    auto result = m_trackSet.add(trackToAdd->id(), WTFMove(trackToAdd));
    if (!result.isNewEntry)
        return false;

    ASSERT(result.iterator->value);
    auto& track = *result.iterator->value;
    track.addObserver(*this);

    updateActiveState();

    if (streamModifier == StreamModifier::DomAPI)
        m_private->addTrack(&track.privateTrack(), MediaStreamPrivate::NotifyClientOption::DontNotify);
    else
        dispatchEvent(MediaStreamTrackEvent::create(eventNames().addtrackEvent, Event::CanBubble::No, Event::IsCancelable::No, &track));

    return true;
}

bool MediaStream::internalRemoveTrack(const String& trackId, StreamModifier streamModifier)
{
    auto track = m_trackSet.take(trackId);
    if (!track)
        return false;

    track->removeObserver(*this);

    updateActiveState();

    if (streamModifier == StreamModifier::DomAPI)
        m_private->removeTrack(track->privateTrack(), MediaStreamPrivate::NotifyClientOption::DontNotify);
    else
        dispatchEvent(MediaStreamTrackEvent::create(eventNames().removetrackEvent, Event::CanBubble::No, Event::IsCancelable::No, WTFMove(track)));

    return true;
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

MediaProducer::MediaStateFlags MediaStream::mediaState() const
{
    MediaProducer::MediaStateFlags state = MediaProducer::IsNotPlaying;

    if (!m_isActive || !document() || !document()->page())
        return state;

    for (const auto& track : m_trackSet.values())
        state |= track->mediaState();

    return state;
}

void MediaStream::statusDidChange()
{
    if (auto* document = this->document()) {
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
    for (auto& track : m_trackSet.values()) {
        if (!track->ended()) {
            active = true;
            break;
        }
    }

    if (m_isActive == active)
        return;

    setIsActive(active);
}

MediaStreamTrackVector MediaStream::trackVectorForType(RealtimeMediaSource::Type filterType) const
{
    MediaStreamTrackVector tracks;
    for (auto& track : m_trackSet.values()) {
        if (track->source().type() == filterType)
            tracks.append(track);
    }

    return tracks;
}

void MediaStream::addObserver(MediaStream::Observer* observer)
{
    if (m_observers.find(observer) == notFound)
        m_observers.append(observer);
}

void MediaStream::removeObserver(MediaStream::Observer* observer)
{
    size_t pos = m_observers.find(observer);
    if (pos != notFound)
        m_observers.remove(pos);
}

Document* MediaStream::document() const
{
    return downcast<Document>(scriptExecutionContext());
}

void MediaStream::stop()
{
    m_isActive = false;
}

const char* MediaStream::activeDOMObjectName() const
{
    return "MediaStream";
}

bool MediaStream::hasPendingActivity() const
{
    return m_isActive;
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaStream::logChannel() const
{
    return LogWebRTC;
}
#endif
    
} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
