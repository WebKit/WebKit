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

#include "EventTarget.h"
#include "MediaStreamDescriptor.h"
#include "MediaStreamTrackList.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class ScriptExecutionContext;

class MediaStream : public RefCounted<MediaStream>, public MediaStreamDescriptorOwner, public EventTarget {
public:
    // Must match the constants in the .idl file.
    enum ReadyState {
        LIVE = 1,
        ENDED = 2
    };

    static PassRefPtr<MediaStream> create(ScriptExecutionContext*, PassRefPtr<MediaStreamDescriptor>);
    static PassRefPtr<MediaStream> create(ScriptExecutionContext*, PassRefPtr<MediaStreamTrackList>);
    virtual ~MediaStream();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(ended);

    ReadyState readyState() const;
    String label() const { return m_descriptor->label(); }

    MediaStreamTrackList* tracks() { return m_tracks.get(); }

    void streamEnded();

    MediaStreamDescriptor* descriptor() const { return m_descriptor.get(); }

    // EventTarget
    virtual const AtomicString& interfaceName() const;
    virtual ScriptExecutionContext* scriptExecutionContext() const;

    using RefCounted<MediaStream>::ref;
    using RefCounted<MediaStream>::deref;

protected:
    MediaStream(ScriptExecutionContext*, PassRefPtr<MediaStreamDescriptor>);

    // EventTarget implementation.
    virtual EventTargetData* eventTargetData();
    virtual EventTargetData* ensureEventTargetData();

private:
    // EventTarget implementation.
    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }

    EventTargetData m_eventTargetData;

    RefPtr<ScriptExecutionContext> m_scriptExecutionContext;

    RefPtr<MediaStreamTrackList> m_tracks;
    RefPtr<MediaStreamDescriptor> m_descriptor;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaStream_h
