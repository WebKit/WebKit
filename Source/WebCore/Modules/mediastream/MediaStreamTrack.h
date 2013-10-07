/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

class MediaStreamTrack : public RefCounted<MediaStreamTrack>, public ScriptWrappable, public ActiveDOMObject, public EventTargetWithInlineData, public MediaStreamSource::Observer {
public:
    virtual ~MediaStreamTrack();

    virtual const AtomicString& kind() const = 0;
    const String& id() const;
    const String& label() const;

    bool enabled() const;
    void setEnabled(bool);

    bool muted() const;
    bool readonly() const;
    bool remote() const;

    const AtomicString& readyState() const;

    static void getSources(ScriptExecutionContext*, PassRefPtr<MediaStreamTrackSourcesCallback>, ExceptionCode&);

    RefPtr<MediaTrackConstraints> constraints() const;
    RefPtr<MediaSourceStates> states() const;
    RefPtr<MediaStreamCapabilities> capabilities() const;
    void applyConstraints(const Dictionary&);

    RefPtr<MediaStreamTrack> clone();
    void stopProducingData();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(mute);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(unmute);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(started);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(ended);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(overconstrained);

    MediaStreamSource* source() const { return m_source.get(); }
    void setSource(MediaStreamSource*);

    bool ended() const;

    // EventTarget
    virtual EventTargetInterface eventTargetInterface() const OVERRIDE FINAL { return MediaStreamTrackEventTargetInterfaceType; }
    virtual ScriptExecutionContext* scriptExecutionContext() const OVERRIDE FINAL { return ActiveDOMObject::scriptExecutionContext(); }

    using RefCounted<MediaStreamTrack>::ref;
    using RefCounted<MediaStreamTrack>::deref;

protected:
    explicit MediaStreamTrack(MediaStreamTrack*);
    MediaStreamTrack(ScriptExecutionContext*, MediaStreamSource*, const Dictionary*);

private:

    void configureTrackRendering();
    void trackDidEnd();
    void scheduleEventDispatch(PassRefPtr<Event>);
    void dispatchQueuedEvents();

    // ActiveDOMObject
    virtual void stop() OVERRIDE FINAL;

    // EventTarget
    virtual void refEventTarget() OVERRIDE FINAL { ref(); }
    virtual void derefEventTarget() OVERRIDE FINAL { deref(); }

    // MediaStreamSourceObserver
    virtual void sourceStateChanged() OVERRIDE FINAL;
    virtual void sourceMutedChanged() OVERRIDE FINAL;
    virtual void sourceEnabledChanged() OVERRIDE FINAL;
    virtual bool stopped() OVERRIDE FINAL;

    Vector<RefPtr<Event>> m_scheduledEvents;

    RefPtr<MediaStreamSource> m_source;
    RefPtr<MediaConstraintsImpl> m_constraints;
    MediaStreamSource::ReadyState m_readyState;
    mutable String m_id;
    Mutex m_mutex;

    bool m_stopped;
    bool m_enabled;
    bool m_muted;
    bool m_eventDispatchScheduled;
};

typedef Vector<RefPtr<MediaStreamTrack> > MediaStreamTrackVector;

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaStreamTrack_h
