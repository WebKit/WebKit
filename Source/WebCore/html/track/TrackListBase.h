/*
 * Copyright (C) 2011, 2012 Apple Inc.  All rights reserved.
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

#ifndef TrackListBase_h
#define TrackListBase_h

#if ENABLE(VIDEO_TRACK)

#include "EventListener.h"
#include "EventTarget.h"
#include "GenericEventQueue.h"
#include "Timer.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class HTMLMediaElement;
class Element;
class TrackBase;

class TrackListBase : public RefCounted<TrackListBase>, public EventTargetWithInlineData {
public:
    virtual ~TrackListBase();

    virtual unsigned length() const;
    virtual bool contains(TrackBase*) const;
    virtual void remove(TrackBase*);

    // EventTarget
    virtual EventTargetInterface eventTargetInterface() const = 0;
    using RefCounted<TrackListBase>::ref;
    using RefCounted<TrackListBase>::deref;
    virtual ScriptExecutionContext* scriptExecutionContext() const override FINAL { return m_context; }

    DEFINE_ATTRIBUTE_EVENT_LISTENER(addtrack);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(change);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(removetrack);

    void clearElement() { m_element = 0; }
    Element* element() const;
    HTMLMediaElement* mediaElement() const { return m_element; }

    // Needs to be public so tracks can call it
    void scheduleChangeEvent();

    bool isAnyTrackEnabled() const;

protected:
    TrackListBase(HTMLMediaElement*, ScriptExecutionContext*);

    void scheduleAddTrackEvent(PassRefPtr<TrackBase>);
    void scheduleRemoveTrackEvent(PassRefPtr<TrackBase>);

    Vector<RefPtr<TrackBase>> m_inbandTracks;

private:
    void scheduleTrackEvent(const AtomicString& eventName, PassRefPtr<TrackBase>);

    // EventTarget
    virtual void refEventTarget() override FINAL { ref(); }
    virtual void derefEventTarget() override FINAL { deref(); }

    ScriptExecutionContext* m_context;
    HTMLMediaElement* m_element;

    GenericEventQueue m_asyncEventQueue;
};

} // namespace WebCore

#endif
#endif
