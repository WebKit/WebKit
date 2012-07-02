/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef MediaStreamTrackList_h
#define MediaStreamTrackList_h

#if ENABLE(MEDIA_STREAM)

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "MediaStreamTrack.h"

namespace WebCore {

class MediaStreamTrackList : public RefCounted<MediaStreamTrackList>, public ActiveDOMObject, public EventTarget {
public:
    static PassRefPtr<MediaStreamTrackList> create(MediaStream*, const MediaStreamTrackVector&);
    virtual ~MediaStreamTrackList();

    void detachOwner();

    // DOM methods & attributes for MediaStreamTrackList
    unsigned length() const;
    MediaStreamTrack* item(unsigned index) const;

    void add(PassRefPtr<MediaStreamTrack>, ExceptionCode&);
    void remove(PassRefPtr<MediaStreamTrack>, ExceptionCode&);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(addtrack);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(removetrack);

    void remove(MediaStreamComponent*);

    // ActiveDOMObject
    virtual void stop() OVERRIDE;

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ScriptExecutionContext* scriptExecutionContext() const OVERRIDE;

    using RefCounted<MediaStreamTrackList>::ref;
    using RefCounted<MediaStreamTrackList>::deref;

private:
    MediaStreamTrackList(MediaStream*, const MediaStreamTrackVector&);

    // EventTarget
    virtual EventTargetData* eventTargetData() OVERRIDE;
    virtual EventTargetData* ensureEventTargetData() OVERRIDE;
    virtual void refEventTarget() OVERRIDE { ref(); }
    virtual void derefEventTarget() OVERRIDE { deref(); }
    EventTargetData m_eventTargetData;

    // m_owner can become zero.
    MediaStream* m_owner;

    MediaStreamTrackVector m_trackVector;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaStreamTrackList_h
