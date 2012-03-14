/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef TextTrackList_h
#define TextTrackList_h

#if ENABLE(VIDEO_TRACK)

#include "EventListener.h"
#include "EventTarget.h"
#include "Timer.h"
#include <algorithm>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class HTMLMediaElement;
class TextTrack;
class TextTrackList;

class TextTrackList : public RefCounted<TextTrackList>, public EventTarget {
public:
    static PassRefPtr<TextTrackList> create(HTMLMediaElement* owner, ScriptExecutionContext* context)
    {
        return adoptRef(new TextTrackList(owner, context));
    }
    ~TextTrackList();

    unsigned length() const;
    unsigned getTrackIndex(TextTrack*);

    TextTrack* item(unsigned index);
    void append(PassRefPtr<TextTrack>);
    void remove(TextTrack*);

    // EventTarget
    virtual const AtomicString& interfaceName() const;
    using RefCounted<TextTrackList>::ref;
    using RefCounted<TextTrackList>::deref;
    virtual ScriptExecutionContext* scriptExecutionContext() const { return m_context; }

    DEFINE_ATTRIBUTE_EVENT_LISTENER(addtrack);

    void clearOwner() { m_owner = 0; }
    Node* owner() const;
    
    bool isFiringEventListeners() { return m_dispatchingEvents; }

private:
    TextTrackList(HTMLMediaElement*, ScriptExecutionContext*);

    // EventTarget
    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }
    virtual EventTargetData* eventTargetData() { return &m_eventTargetData; }
    virtual EventTargetData* ensureEventTargetData() { return &m_eventTargetData; }

    void scheduleAddTrackEvent(PassRefPtr<TextTrack>);
    void asyncEventTimerFired(Timer<TextTrackList>*);

    ScriptExecutionContext* m_context;
    HTMLMediaElement* m_owner;

    Vector<RefPtr<Event> > m_pendingEvents;
    Timer<TextTrackList> m_pendingEventTimer;

    EventTargetData m_eventTargetData;
    Vector<RefPtr<TextTrack> > m_addTrackTracks;
    Vector<RefPtr<TextTrack> > m_elementTracks;
    
    int m_dispatchingEvents;
};

} // namespace WebCore

#endif
#endif
