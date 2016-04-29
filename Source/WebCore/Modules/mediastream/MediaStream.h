/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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

#ifndef MediaStream_h
#define MediaStream_h

#if ENABLE(MEDIA_STREAM)

#include "ContextDestructionObserver.h"
#include "EventTarget.h"
#include "ExceptionBase.h"
#include "MediaProducer.h"
#include "MediaStreamPrivate.h"
#include "MediaStreamTrack.h"
#include "ScriptWrappable.h"
#include "Timer.h"
#include "URLRegistry.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Document;

class MediaStream final : public URLRegistrable, public EventTargetWithInlineData, public ContextDestructionObserver, public MediaStreamTrack::Observer, public MediaStreamPrivate::Observer, private MediaProducer, public RefCounted<MediaStream> {
public:
    class Observer {
    public:
        virtual ~Observer() { }
        virtual void didAddOrRemoveTrack() = 0;
    };

    static Ref<MediaStream> create(ScriptExecutionContext&);
    static Ref<MediaStream> create(ScriptExecutionContext&, MediaStream&);
    static Ref<MediaStream> create(ScriptExecutionContext&, const MediaStreamTrackVector&);
    static Ref<MediaStream> create(ScriptExecutionContext&, RefPtr<MediaStreamPrivate>&&);
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
    bool muted() const { return m_isMuted; }

    MediaStreamPrivate* privateStream() const { return m_private.get(); }

    // EventTarget
    EventTargetInterface eventTargetInterface() const final { return MediaStreamEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ContextDestructionObserver::scriptExecutionContext(); }

    using RefCounted<MediaStream>::ref;
    using RefCounted<MediaStream>::deref;

    // URLRegistrable
    URLRegistry& registry() const override;

    void addObserver(Observer*);
    void removeObserver(Observer*);

protected:
    MediaStream(ScriptExecutionContext&, const MediaStreamTrackVector&);
    MediaStream(ScriptExecutionContext&, RefPtr<MediaStreamPrivate>&&);

    // ContextDestructionObserver
    void contextDestroyed() final;

private:
    enum class StreamModifier { DomAPI, Platform };

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

    // MediaProducer
    void pageMutedStateDidChange() final;
    MediaProducer::MediaStateFlags mediaState() const final;

    bool internalAddTrack(Ref<MediaStreamTrack>&&, StreamModifier);
    bool internalRemoveTrack(const String&, StreamModifier);

    void scheduleActiveStateChange();
    void activityEventTimerFired();
    void setIsActive(bool);
    void statusDidChange();

    Document* document() const;

    MediaStreamTrackVector trackVectorForType(RealtimeMediaSource::Type) const;

    RefPtr<MediaStreamPrivate> m_private;

    HashMap<String, RefPtr<MediaStreamTrack>> m_trackSet;

    Timer m_activityEventTimer;
    Vector<Ref<Event>> m_scheduledActivityEvents;

    Vector<Observer*> m_observers;

    bool m_isActive { false };
    bool m_isMuted { true };
    bool m_externallyMuted { false };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaStream_h
