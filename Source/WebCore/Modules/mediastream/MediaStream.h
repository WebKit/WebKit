/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "MediaStreamPrivate.h"
#include "MediaStreamTrack.h"
#include "ScriptWrappable.h"
#include "Timer.h"
#include "URLRegistry.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class MediaStreamCenter;

class MediaStream final : public RefCounted<MediaStream>, public URLRegistrable, public ScriptWrappable, public MediaStreamPrivateClient, public EventTargetWithInlineData, public ContextDestructionObserver {
public:
    class Observer {
    public:
        virtual void didAddOrRemoveTrack() = 0;
    };

    static PassRefPtr<MediaStream> create(ScriptExecutionContext&);
    static PassRefPtr<MediaStream> create(ScriptExecutionContext&, PassRefPtr<MediaStream>);
    static PassRefPtr<MediaStream> create(ScriptExecutionContext&, const Vector<RefPtr<MediaStreamTrack>>&);
    static PassRefPtr<MediaStream> create(ScriptExecutionContext&, PassRefPtr<MediaStreamPrivate>);
    virtual ~MediaStream();

    String id() const { return m_private->id(); }

    void addTrack(PassRefPtr<MediaStreamTrack>, ExceptionCode&);
    void removeTrack(PassRefPtr<MediaStreamTrack>, ExceptionCode&);
    MediaStreamTrack* getTrackById(String);

    Vector<RefPtr<MediaStreamTrack>> getAudioTracks() const { return m_audioTracks; }
    Vector<RefPtr<MediaStreamTrack>> getVideoTracks() const { return m_videoTracks; }
    Vector<RefPtr<MediaStreamTrack>> getTracks();

    PassRefPtr<MediaStream> clone();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(addtrack);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(removetrack);

    bool active() const;
    void setActive(bool);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(active);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(inactive);

    MediaStreamPrivate* privateStream() const { return m_private.get(); }

    // EventTarget
    virtual EventTargetInterface eventTargetInterface() const final { return MediaStreamEventTargetInterfaceType; }
    virtual ScriptExecutionContext* scriptExecutionContext() const final { return ContextDestructionObserver::scriptExecutionContext(); }

    using RefCounted<MediaStream>::ref;
    using RefCounted<MediaStream>::deref;

    // URLRegistrable
    virtual URLRegistry& registry() const override;

    void addObserver(Observer*);
    void removeObserver(Observer*);

protected:
    MediaStream(ScriptExecutionContext&, PassRefPtr<MediaStreamPrivate>);

    // ContextDestructionObserver
    virtual void contextDestroyed() override final;

private:
    // EventTarget
    virtual void refEventTarget() override final { ref(); }
    virtual void derefEventTarget() override final { deref(); }

    // MediaStreamPrivateClient
    virtual void trackDidEnd() override final;
    virtual void setStreamIsActive(bool) override final;
    virtual void addRemoteSource(MediaStreamSource*) override final;
    virtual void removeRemoteSource(MediaStreamSource*) override final;
    virtual void addRemoteTrack(MediaStreamTrackPrivate*) override final;
    virtual void removeRemoteTrack(MediaStreamTrackPrivate*) override final;

    bool removeTrack(PassRefPtr<MediaStreamTrack>);
    bool addTrack(PassRefPtr<MediaStreamTrack>);

    bool haveTrackWithSource(PassRefPtr<MediaStreamSource>);

    void scheduleDispatchEvent(PassRefPtr<Event>);
    void scheduledEventTimerFired(Timer<MediaStream>*);

    void cloneMediaStreamTrackVector(Vector<RefPtr<MediaStreamTrack>>&, const Vector<RefPtr<MediaStreamTrack>>&);

    Vector<RefPtr<MediaStreamTrack>>* trackVectorForType(MediaStreamSource::Type);

    RefPtr<MediaStreamPrivate> m_private;
    Vector<RefPtr<MediaStreamTrack>> m_audioTracks;
    Vector<RefPtr<MediaStreamTrack>> m_videoTracks;

    Timer<MediaStream> m_scheduledEventTimer;
    Vector<RefPtr<Event>> m_scheduledEvents;

    Vector<Observer*> m_observers;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaStream_h
