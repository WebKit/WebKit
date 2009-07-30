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

#include "config.h"
#include "DOMApplicationCache.h"

#if ENABLE(OFFLINE_WEB_APPLICATIONS)

#include "ApplicationCacheHost.h"
#include "DocumentLoader.h"
#include "Event.h"
#include "EventException.h"
#include "EventListener.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"

namespace WebCore {

DOMApplicationCache::DOMApplicationCache(Frame* frame)
    : m_frame(frame)
{
    ASSERT(applicationCacheHost());
    applicationCacheHost()->setDOMApplicationCache(this);
}

void DOMApplicationCache::disconnectFrame()
{
    ApplicationCacheHost* cacheHost = applicationCacheHost();
    if (cacheHost)
        cacheHost->setDOMApplicationCache(0);
    m_frame = 0;
}

ApplicationCacheHost* DOMApplicationCache::applicationCacheHost() const
{
    if (!m_frame || !m_frame->loader()->documentLoader())
        return 0;
    return m_frame->loader()->documentLoader()->applicationCacheHost();
}

unsigned short DOMApplicationCache::status() const
{
    ApplicationCacheHost* cacheHost = applicationCacheHost();
    if (!cacheHost)
        return UNCACHED;
    return cacheHost->status();
}

void DOMApplicationCache::update(ExceptionCode& ec)
{
    ApplicationCacheHost* cacheHost = applicationCacheHost();
    if (!cacheHost || !cacheHost->update())
        ec = INVALID_STATE_ERR;
}

bool DOMApplicationCache::swapCache()
{
    ApplicationCacheHost* cacheHost = applicationCacheHost();
    if (!cacheHost)
        return false;
    return cacheHost->swapCache();
}
    
void DOMApplicationCache::swapCache(ExceptionCode& ec)
{
    if (!swapCache())
        ec = INVALID_STATE_ERR;
}

ScriptExecutionContext* DOMApplicationCache::scriptExecutionContext() const
{
    return m_frame->document();
}

void DOMApplicationCache::addEventListener(const AtomicString& eventName, PassRefPtr<EventListener> eventListener, bool)
{
    EventListenersMap::iterator iter = m_eventListeners.find(eventName);
    if (iter == m_eventListeners.end()) {
        ListenerVector listeners;
        listeners.append(eventListener);
        m_eventListeners.add(eventName, listeners);
    } else {
        ListenerVector& listeners = iter->second;
        for (ListenerVector::iterator listenerIter = listeners.begin(); listenerIter != listeners.end(); ++listenerIter) {
            if (*listenerIter == eventListener)
                return;
        }
        
        listeners.append(eventListener);
        m_eventListeners.add(eventName, listeners);
    }    
}

void DOMApplicationCache::removeEventListener(const AtomicString& eventName, EventListener* eventListener, bool)
{
    EventListenersMap::iterator iter = m_eventListeners.find(eventName);
    if (iter == m_eventListeners.end())
        return;
    
    ListenerVector& listeners = iter->second;
    for (ListenerVector::const_iterator listenerIter = listeners.begin(); listenerIter != listeners.end(); ++listenerIter) {
        if (*listenerIter == eventListener) {
            listeners.remove(listenerIter - listeners.begin());
            return;
        }
    }
}

bool DOMApplicationCache::dispatchEvent(PassRefPtr<Event> event, ExceptionCode& ec)
{
    if (!event || event->type().isEmpty()) {
        ec = EventException::UNSPECIFIED_EVENT_TYPE_ERR;
        return true;
    }

    ListenerVector listenersCopy = m_eventListeners.get(event->type());
    for (ListenerVector::const_iterator listenerIter = listenersCopy.begin(); listenerIter != listenersCopy.end(); ++listenerIter) {
        event->setTarget(this);
        event->setCurrentTarget(this);
        listenerIter->get()->handleEvent(event.get(), false);
    }
    
    return !event->defaultPrevented();
}

void DOMApplicationCache::callListener(const AtomicString& eventName, EventListener* listener)
{
    ASSERT(m_frame);
    
    RefPtr<Event> event = Event::create(eventName, false, false);
    if (listener) {
        event->setTarget(this);
        event->setCurrentTarget(this);
        listener->handleEvent(event.get(), false);
    }
    
    ExceptionCode ec = 0;
    dispatchEvent(event.release(), ec);
    ASSERT(!ec);    
}

// static
const AtomicString& DOMApplicationCache::toEventName(EventType eventType)
{
    switch (eventType) {
    case CHECKING_EVENT:
        return eventNames().checkingEvent;
    case ERROR_EVENT:
        return eventNames().errorEvent;
    case NOUPDATE_EVENT:
        return eventNames().noupdateEvent;
    case DOWNLOADING_EVENT:
        return eventNames().downloadingEvent;
    case PROGRESS_EVENT:
        return eventNames().progressEvent;
    case UPDATEREADY_EVENT:
        return eventNames().updatereadyEvent;
    case CACHED_EVENT:
        return eventNames().cachedEvent;
    case OBSOLETE_EVENT:            
        return eventNames().obsoleteEvent;
    }
    ASSERT_NOT_REACHED();
    return eventNames().errorEvent;
}

// static
DOMApplicationCache::EventType DOMApplicationCache::toEventType(const AtomicString& eventName)
{
    if (eventName == eventNames().checkingEvent)
        return CHECKING_EVENT;
    if (eventName == eventNames().errorEvent)
        return ERROR_EVENT;
    if (eventName == eventNames().noupdateEvent)
        return NOUPDATE_EVENT;
    if (eventName == eventNames().downloadingEvent)
        return DOWNLOADING_EVENT;
    if (eventName == eventNames().progressEvent)
        return PROGRESS_EVENT;
    if (eventName == eventNames().updatereadyEvent)
        return UPDATEREADY_EVENT;
    if (eventName == eventNames().cachedEvent)
        return CACHED_EVENT;
    if (eventName == eventNames().obsoleteEvent)
        return OBSOLETE_EVENT;
  
    ASSERT_NOT_REACHED();
    return ERROR_EVENT;
}


} // namespace WebCore

#endif // ENABLE(OFFLINE_WEB_APPLICATIONS)
