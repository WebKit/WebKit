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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#if ENABLE(VIDEO_TRACK)

#include "ContextDestructionObserver.h"
#include "EventTarget.h"
#include "GenericEventQueue.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class HTMLMediaElement;
class Element;
class TrackBase;

class TrackListBase : public RefCounted<TrackListBase>, public EventTargetWithInlineData, public ContextDestructionObserver {
    WTF_MAKE_ISO_ALLOCATED(TrackListBase);
public:
    virtual ~TrackListBase();

    virtual unsigned length() const;
    virtual bool contains(TrackBase&) const;
    virtual void remove(TrackBase&, bool scheduleEvent = true);

    // EventTarget
    EventTargetInterface eventTargetInterface() const override = 0;
    using RefCounted<TrackListBase>::ref;
    using RefCounted<TrackListBase>::deref;
    ScriptExecutionContext* scriptExecutionContext() const final { return ContextDestructionObserver::scriptExecutionContext(); }

    virtual void clearElement();
    Element* element() const;
    WeakPtr<HTMLMediaElement> mediaElement() const { return m_element; }

    // Needs to be public so tracks can call it
    void scheduleChangeEvent();
    bool isChangeEventScheduled() const;

    bool isAnyTrackEnabled() const;

protected:
    TrackListBase(WeakPtr<HTMLMediaElement>, ScriptExecutionContext*);

    void scheduleAddTrackEvent(Ref<TrackBase>&&);
    void scheduleRemoveTrackEvent(Ref<TrackBase>&&);

    Vector<RefPtr<TrackBase>> m_inbandTracks;

private:
    void scheduleTrackEvent(const AtomString& eventName, Ref<TrackBase>&&);

    // EventTarget
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    WeakPtr<HTMLMediaElement> m_element;

    UniqueRef<MainThreadGenericEventQueue> m_asyncEventQueue;
};

} // namespace WebCore

#endif // ENABLE(VIDEO_TRACK)
