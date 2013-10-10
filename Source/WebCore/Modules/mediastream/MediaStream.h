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

#ifndef MediaStream_h
#define MediaStream_h

#if ENABLE(MEDIA_STREAM)

#include "ContextDestructionObserver.h"
#include "EventTarget.h"
#include "ExceptionBase.h"
#include "MediaStreamDescriptor.h"
#include "MediaStreamTrack.h"
#include "ScriptWrappable.h"
#include "Timer.h"
#include "URLRegistry.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class MediaStreamCenter;

class MediaStream FINAL : public RefCounted<MediaStream>, public URLRegistrable, public ScriptWrappable, public MediaStreamDescriptorClient, public EventTargetWithInlineData, public ContextDestructionObserver {
public:
    static PassRefPtr<MediaStream> create(ScriptExecutionContext*);
    static PassRefPtr<MediaStream> create(ScriptExecutionContext*, PassRefPtr<MediaStream>);
    static PassRefPtr<MediaStream> create(ScriptExecutionContext*, const MediaStreamTrackVector&);
    static PassRefPtr<MediaStream> create(ScriptExecutionContext*, PassRefPtr<MediaStreamDescriptor>);
    virtual ~MediaStream();

    String id() const { return m_descriptor->id(); }

    void addTrack(PassRefPtr<MediaStreamTrack>, ExceptionCode&);
    void removeTrack(PassRefPtr<MediaStreamTrack>, ExceptionCode&);
    MediaStreamTrack* getTrackById(String);

    MediaStreamTrackVector getAudioTracks() const { return m_audioTracks; }
    MediaStreamTrackVector getVideoTracks() const { return m_videoTracks; }

    bool ended() const;
    void setEnded();
    PassRefPtr<MediaStream> clone();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(ended);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(addtrack);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(removetrack);

    MediaStreamDescriptor* descriptor() const { return m_descriptor.get(); }

    // EventTarget
    virtual EventTargetInterface eventTargetInterface() const FINAL { return MediaStreamEventTargetInterfaceType; }
    virtual ScriptExecutionContext* scriptExecutionContext() const FINAL { return ContextDestructionObserver::scriptExecutionContext(); }

    using RefCounted<MediaStream>::ref;
    using RefCounted<MediaStream>::deref;

    // URLRegistrable
    virtual URLRegistry& registry() const OVERRIDE;

protected:
    MediaStream(ScriptExecutionContext*, PassRefPtr<MediaStreamDescriptor>);

    // ContextDestructionObserver
    virtual void contextDestroyed() OVERRIDE FINAL;

private:
    // EventTarget
    virtual void refEventTarget() OVERRIDE FINAL { ref(); }
    virtual void derefEventTarget() OVERRIDE FINAL { deref(); }

    // MediaStreamDescriptorClient
    virtual void trackDidEnd() OVERRIDE FINAL;
    virtual void streamDidEnd() OVERRIDE FINAL;
    virtual void addRemoteSource(MediaStreamSource*) OVERRIDE FINAL;
    virtual void removeRemoteSource(MediaStreamSource*) OVERRIDE FINAL;

    void scheduleDispatchEvent(PassRefPtr<Event>);
    void scheduledEventTimerFired(Timer<MediaStream>*);

    void cloneMediaStreamTrackVector(MediaStreamTrackVector&, const MediaStreamTrackVector&);

    RefPtr<MediaStreamDescriptor> m_descriptor;
    MediaStreamTrackVector m_audioTracks;
    MediaStreamTrackVector m_videoTracks;

    Timer<MediaStream> m_scheduledEventTimer;
    Vector<RefPtr<Event>> m_scheduledEvents;
};

typedef Vector<RefPtr<MediaStream> > MediaStreamVector;

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaStream_h
