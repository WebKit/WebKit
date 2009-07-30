/*
 * Copyright (C) 2008, 2009 Apple Inc. All Rights Reserved.
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

#ifndef DOMApplicationCache_h
#define DOMApplicationCache_h

#if ENABLE(OFFLINE_WEB_APPLICATIONS)

#include "AtomicStringHash.h"
#include "EventTarget.h"
#include "EventListener.h"
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class ApplicationCacheHost;
class AtomicStringImpl;
class Frame;
class KURL;
class String;
    
class DOMApplicationCache : public RefCounted<DOMApplicationCache>, public EventTarget {
public:
    static PassRefPtr<DOMApplicationCache> create(Frame* frame) { return adoptRef(new DOMApplicationCache(frame)); }
    ~DOMApplicationCache() { ASSERT(!m_frame); }

    void disconnectFrame();

    enum Status {
        UNCACHED = 0,
        IDLE = 1,
        CHECKING = 2,
        DOWNLOADING = 3,
        UPDATEREADY = 4,
        OBSOLETE = 5
    };

    enum EventType {
        CHECKING_EVENT = 0,
        ERROR_EVENT,
        NOUPDATE_EVENT,
        DOWNLOADING_EVENT,
        PROGRESS_EVENT,
        UPDATEREADY_EVENT,
        CACHED_EVENT,
        OBSOLETE_EVENT
    };

    unsigned short status() const;
    void update(ExceptionCode&);
    void swapCache(ExceptionCode&);

    // Event listener attributes by EventType

    void setAttributeEventListener(EventType type, PassRefPtr<EventListener> eventListener) { m_attributeEventListeners[type] = eventListener; }
    EventListener* getAttributeEventListener(EventType type) const { return m_attributeEventListeners[type].get(); }
    void clearAttributeEventListener(EventType type) { m_attributeEventListeners[type] = 0; }
    void callEventListener(EventType type) { callListener(toEventName(type), getAttributeEventListener(type)); }

    // EventTarget impl

    virtual void addEventListener(const AtomicString& eventName, PassRefPtr<EventListener>, bool useCapture);
    virtual void removeEventListener(const AtomicString& eventName, EventListener*, bool useCapture);
    virtual bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&);
    typedef Vector<RefPtr<EventListener> > ListenerVector;
    typedef HashMap<AtomicString, ListenerVector> EventListenersMap;
    EventListenersMap& eventListeners() { return m_eventListeners; }

    using RefCounted<DOMApplicationCache>::ref;
    using RefCounted<DOMApplicationCache>::deref;

    // Explicitly named attribute event listener helpers

    void setOnchecking(PassRefPtr<EventListener> listener) { setAttributeEventListener(CHECKING_EVENT, listener); }
    EventListener* onchecking() const { return getAttributeEventListener(CHECKING_EVENT); }

    void setOnerror(PassRefPtr<EventListener> listener) { setAttributeEventListener(ERROR_EVENT, listener);}
    EventListener* onerror() const { return getAttributeEventListener(ERROR_EVENT); }

    void setOnnoupdate(PassRefPtr<EventListener> listener) { setAttributeEventListener(NOUPDATE_EVENT, listener); }
    EventListener* onnoupdate() const { return getAttributeEventListener(NOUPDATE_EVENT); }

    void setOndownloading(PassRefPtr<EventListener> listener) { setAttributeEventListener(DOWNLOADING_EVENT, listener); }
    EventListener* ondownloading() const { return getAttributeEventListener(DOWNLOADING_EVENT); }

    void setOnprogress(PassRefPtr<EventListener> listener) { setAttributeEventListener(PROGRESS_EVENT, listener); }
    EventListener* onprogress() const { return getAttributeEventListener(PROGRESS_EVENT); }

    void setOnupdateready(PassRefPtr<EventListener> listener) { setAttributeEventListener(UPDATEREADY_EVENT, listener); }
    EventListener* onupdateready() const { return getAttributeEventListener(UPDATEREADY_EVENT); }

    void setOncached(PassRefPtr<EventListener> listener) { setAttributeEventListener(CACHED_EVENT, listener); }
    EventListener* oncached() const { return getAttributeEventListener(CACHED_EVENT); }

    void setOnobsolete(PassRefPtr<EventListener> listener) { setAttributeEventListener(OBSOLETE_EVENT, listener); }
    EventListener* onobsolete() const { return getAttributeEventListener(OBSOLETE_EVENT); }

    virtual ScriptExecutionContext* scriptExecutionContext() const;
    DOMApplicationCache* toDOMApplicationCache() { return this; }

    static const AtomicString& toEventName(EventType eventType);
    static EventType toEventType(const AtomicString& eventName);

private:
    DOMApplicationCache(Frame*);

    void callListener(const AtomicString& eventName, EventListener*);
    
    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }

    ApplicationCacheHost* applicationCacheHost() const;
    bool swapCache();
    
    RefPtr<EventListener> m_attributeEventListeners[OBSOLETE_EVENT + 1];

    EventListenersMap m_eventListeners;

    Frame* m_frame;
};

} // namespace WebCore

#endif // ENABLE(OFFLINE_WEB_APPLICATIONS)

#endif // DOMApplicationCache_h
