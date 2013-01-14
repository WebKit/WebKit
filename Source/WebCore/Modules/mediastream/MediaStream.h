/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
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
#include "MediaStreamDescriptor.h"
#include "MediaStreamTrackList.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class MediaStream : public RefCounted<MediaStream>, public MediaStreamDescriptorClient, public EventTarget, public ContextDestructionObserver {
public:
    static PassRefPtr<MediaStream> create(ScriptExecutionContext*);
    static PassRefPtr<MediaStream> create(ScriptExecutionContext*, PassRefPtr<MediaStream>);
    static PassRefPtr<MediaStream> create(ScriptExecutionContext*, const MediaStreamTrackVector&);
    static PassRefPtr<MediaStream> create(ScriptExecutionContext*, PassRefPtr<MediaStreamDescriptor>);
    virtual ~MediaStream();

    // DEPRECATED
    String label() const { return m_descriptor->id(); }

    String id() const { return m_descriptor->id(); }

    MediaStreamTrackList* audioTracks() { return m_audioTracks.get(); }
    MediaStreamTrackList* videoTracks() { return m_videoTracks.get(); }

    bool ended() const;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(ended);

    // MediaStreamDescriptorClient
    virtual void streamEnded() OVERRIDE;

    virtual bool isLocal() const { return false; }

    MediaStreamDescriptor* descriptor() const { return m_descriptor.get(); }

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ScriptExecutionContext* scriptExecutionContext() const OVERRIDE;

    using RefCounted<MediaStream>::ref;
    using RefCounted<MediaStream>::deref;

protected:
    MediaStream(ScriptExecutionContext*, PassRefPtr<MediaStreamDescriptor>);

    // EventTarget
    virtual EventTargetData* eventTargetData() OVERRIDE;
    virtual EventTargetData* ensureEventTargetData() OVERRIDE;

private:
    // EventTarget
    virtual void refEventTarget() OVERRIDE { ref(); }
    virtual void derefEventTarget() OVERRIDE { deref(); }

    // MediaStreamDescriptorClient
    virtual void addTrack(MediaStreamComponent*) OVERRIDE;
    virtual void removeTrack(MediaStreamComponent*) OVERRIDE;

    EventTargetData m_eventTargetData;

    RefPtr<MediaStreamTrackList> m_audioTracks;
    RefPtr<MediaStreamTrackList> m_videoTracks;
    RefPtr<MediaStreamDescriptor> m_descriptor;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaStream_h
