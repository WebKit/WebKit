/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "MediaCanStartListener.h"
#include "MediaProducer.h"
#include "MediaStreamPrivate.h"
#include "MediaStreamTrack.h"
#include "PlatformMediaSession.h"
#include "ScriptWrappable.h"
#include "Timer.h"
#include "URLRegistry.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Document;

class MediaStream final
    : public URLRegistrable
    , public EventTargetWithInlineData
    , public ActiveDOMObject
    , public MediaStreamTrack::Observer
    , public MediaStreamPrivate::Observer
    , private MediaCanStartListener
    , private PlatformMediaSessionClient
    , public RefCounted<MediaStream> {
public:
    class Observer {
    public:
        virtual ~Observer() { }
        virtual void didAddOrRemoveTrack() = 0;
    };

    static Ref<MediaStream> create(ScriptExecutionContext&);
    static Ref<MediaStream> create(ScriptExecutionContext&, MediaStream&);
    static Ref<MediaStream> create(ScriptExecutionContext&, const MediaStreamTrackVector&);
    static Ref<MediaStream> create(ScriptExecutionContext&, Ref<MediaStreamPrivate>&&);
    virtual ~MediaStream();

    String id() const { return m_private->id(); }

    void addTrack(MediaStreamTrack&);
    void removeTrack(MediaStreamTrack&);
    MediaStreamTrack* getTrackById(String);

    MediaStreamTrackVector getAudioTracks() const;
    MediaStreamTrackVector getVideoTracks() const;
    MediaStreamTrackVector getTracks() const;

    RefPtr<MediaStream> clone();

    bool active() const { return m_isActive; }
    bool muted() const { return m_private->muted(); }

    MediaStreamPrivate& privateStream() { return m_private.get(); }

    void startProducingData();
    void stopProducingData();

    void endCaptureTracks();

    // EventTarget
    EventTargetInterface eventTargetInterface() const final { return MediaStreamEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ContextDestructionObserver::scriptExecutionContext(); }

    using RefCounted<MediaStream>::ref;
    using RefCounted<MediaStream>::deref;

    // URLRegistrable
    URLRegistry& registry() const override;

    void addObserver(Observer*);
    void removeObserver(Observer*);

    void addTrackFromPlatform(Ref<MediaStreamTrack>&&);

    Document* document() const;

    // ActiveDOMObject API.
    bool hasPendingActivity() const final;

    enum class StreamModifier { DomAPI, Platform };
    bool internalAddTrack(Ref<MediaStreamTrack>&&, StreamModifier);
    WEBCORE_EXPORT bool internalRemoveTrack(const String&, StreamModifier);

protected:
    MediaStream(ScriptExecutionContext&, const MediaStreamTrackVector&);
    MediaStream(ScriptExecutionContext&, Ref<MediaStreamPrivate>&&);

private:

    // EventTarget
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    // MediaStreamTrack::Observer
    void trackDidEnd() final;

    // MediaStreamPrivate::Observer
    void activeStatusChanged() final;
    void didAddTrack(MediaStreamTrackPrivate&) final;
    void didRemoveTrack(MediaStreamTrackPrivate&) final;
    void characteristicsChanged() final;

    MediaProducer::MediaStateFlags mediaState() const;

    // MediaCanStartListener
    void mediaCanStart(Document&) final;

    // PlatformMediaSessionClient
    PlatformMediaSession::MediaType mediaType() const final;
    PlatformMediaSession::MediaType presentationType() const final;
    PlatformMediaSession::CharacteristicsFlags characteristics() const final;
    void mayResumePlayback(bool shouldResume) final;
    void suspendPlayback() final;
    bool canReceiveRemoteControlCommands() const final { return false; }
    void didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType, const PlatformMediaSession::RemoteCommandArgument*) final { }
    bool supportsSeeking() const final { return false; }
    bool shouldOverrideBackgroundPlaybackRestriction(PlatformMediaSession::InterruptionType) const final { return false; }
    String sourceApplicationIdentifier() const final;
    bool canProduceAudio() const final;
    const Document* hostingDocument() const final { return document(); }
    bool processingUserGestureForMedia() const final;

    // ActiveDOMObject API.
    void stop() final;
    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;

    void scheduleActiveStateChange();
    void activityEventTimerFired();
    void setIsActive(bool);
    void statusDidChange();

    MediaStreamTrackVector trackVectorForType(RealtimeMediaSource::Type) const;

    Ref<MediaStreamPrivate> m_private;

    HashMap<String, RefPtr<MediaStreamTrack>> m_trackSet;

    Timer m_activityEventTimer;
    Vector<Ref<Event>> m_scheduledActivityEvents;

    Vector<Observer*> m_observers;
    std::unique_ptr<PlatformMediaSession> m_mediaSession;

    MediaProducer::MediaStateFlags m_state { MediaProducer::IsNotPlaying };

    bool m_isActive { false };
    bool m_isProducingData { false };
    bool m_isWaitingUntilMediaCanStart { false };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
