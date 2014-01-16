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

#ifndef MediaStreamTrack_h
#define MediaStreamTrack_h

#if ENABLE(MEDIA_STREAM)

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "MediaStreamSource.h"
#include "MediaStreamTrackPrivate.h"
#include "ScriptWrappable.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Dictionary;
class MediaConstraintsImpl;
class MediaSourceStates;
class MediaStreamTrackSourcesCallback;
class MediaStreamCapabilities;
class MediaTrackConstraints;

class MediaStreamTrack : public RefCounted<MediaStreamTrack>, public ScriptWrappable, public ActiveDOMObject, public EventTargetWithInlineData, public MediaStreamTrackPrivateClient {
public:
    class Observer {
    public:
        virtual ~Observer() { }
        virtual void trackDidEnd() = 0;
    };

    virtual ~MediaStreamTrack();

    virtual const AtomicString& kind() const = 0;
    const String& id() const;
    const String& label() const;

    bool enabled() const;
    void setEnabled(bool);

    bool muted() const;
    bool readonly() const;
    bool remote() const;
    bool stopped() const;

    const AtomicString& readyState() const;

    static void getSources(ScriptExecutionContext*, PassRefPtr<MediaStreamTrackSourcesCallback>, ExceptionCode&);

    RefPtr<MediaTrackConstraints> constraints() const;
    RefPtr<MediaSourceStates> states() const;
    RefPtr<MediaStreamCapabilities> capabilities() const;
    void applyConstraints(const Dictionary&);
    void applyConstraints(PassRefPtr<MediaConstraints>);

    RefPtr<MediaStreamTrack> clone();
    void stopProducingData();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(mute);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(unmute);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(started);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(ended);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(overconstrained);

    MediaStreamSource* source() const { return m_privateTrack->source(); }
    MediaStreamTrackPrivate& privateTrack() { return m_privateTrack.get(); }

    bool ended() const;

    void addObserver(Observer*);
    void removeObserver(Observer*);

    // EventTarget
    virtual EventTargetInterface eventTargetInterface() const override final { return MediaStreamTrackEventTargetInterfaceType; }
    virtual ScriptExecutionContext* scriptExecutionContext() const override final { return ActiveDOMObject::scriptExecutionContext(); }

    using RefCounted<MediaStreamTrack>::ref;
    using RefCounted<MediaStreamTrack>::deref;

protected:
    explicit MediaStreamTrack(MediaStreamTrack&);
    MediaStreamTrack(ScriptExecutionContext&, MediaStreamTrackPrivate&, const Dictionary*);

    void setSource(PassRefPtr<MediaStreamSource>);

private:

    void configureTrackRendering();
    void trackDidEnd();
    void scheduleEventDispatch(PassRefPtr<Event>);
    void dispatchQueuedEvents();

    // ActiveDOMObject
    virtual void stop() override final;

    // EventTarget
    virtual void refEventTarget() override final { ref(); }
    virtual void derefEventTarget() override final { deref(); }

    // MediaStreamTrackPrivateClient
    void trackReadyStateChanged();
    void trackMutedChanged();
    void trackEnabledChanged();

    Vector<RefPtr<Event>> m_scheduledEvents;

    RefPtr<MediaConstraintsImpl> m_constraints;
    Mutex m_mutex;

    Vector<Observer*> m_observers;

    Ref<MediaStreamTrackPrivate> m_privateTrack;
    bool m_eventDispatchScheduled;

    bool m_stoppingTrack;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaStreamTrack_h
