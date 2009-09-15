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
    ASSERT(!m_frame || applicationCacheHost());
    ApplicationCacheHost* cacheHost = applicationCacheHost();
    if (cacheHost)
        cacheHost->setDOMApplicationCache(this);
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
        return ApplicationCacheHost::UNCACHED;
    return cacheHost->status();
}

void DOMApplicationCache::update(ExceptionCode& ec)
{
    ApplicationCacheHost* cacheHost = applicationCacheHost();
    if (!cacheHost || !cacheHost->update())
        ec = INVALID_STATE_ERR;
}

void DOMApplicationCache::swapCache(ExceptionCode& ec)
{
    ApplicationCacheHost* cacheHost = applicationCacheHost();
    if (!cacheHost || !cacheHost->swapCache())
        ec = INVALID_STATE_ERR;
}

ScriptExecutionContext* DOMApplicationCache::scriptExecutionContext() const
{
    ASSERT(m_frame);
    return m_frame->document();
}

void DOMApplicationCache::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> eventListener, bool)
{
    EventListenersMap::iterator iter = m_eventListeners.find(eventType);
    if (iter == m_eventListeners.end()) {
        ListenerVector listeners;
        listeners.append(eventListener);
        m_eventListeners.add(eventType, listeners);
    } else {
        ListenerVector& listeners = iter->second;
        for (ListenerVector::iterator listenerIter = listeners.begin(); listenerIter != listeners.end(); ++listenerIter) {
            if (**listenerIter == *eventListener)
                return;
        }
        
        listeners.append(eventListener);
        m_eventListeners.add(eventType, listeners);
    }    
}

void DOMApplicationCache::removeEventListener(const AtomicString& eventType, EventListener* eventListener, bool)
{
    EventListenersMap::iterator iter = m_eventListeners.find(eventType);
    if (iter == m_eventListeners.end())
        return;
    
    ListenerVector& listeners = iter->second;
    for (ListenerVector::const_iterator listenerIter = listeners.begin(); listenerIter != listeners.end(); ++listenerIter) {
        if (**listenerIter == *eventListener) {
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

void DOMApplicationCache::callListener(const AtomicString& eventType, EventListener* listener)
{
    ASSERT(m_frame);
    
    RefPtr<Event> event = Event::create(eventType, false, false);
    if (listener) {
        event->setTarget(this);
        event->setCurrentTarget(this);
        listener->handleEvent(event.get(), false);
    }
    
    ExceptionCode ec = 0;
    dispatchEvent(event.release(), ec);
    ASSERT(!ec);    
}

const AtomicString& DOMApplicationCache::toEventType(ApplicationCacheHost::EventID id)
{
    switch (id) {
    case ApplicationCacheHost::CHECKING_EVENT:
        return eventNames().checkingEvent;
    case ApplicationCacheHost::ERROR_EVENT:
        return eventNames().errorEvent;
    case ApplicationCacheHost::NOUPDATE_EVENT:
        return eventNames().noupdateEvent;
    case ApplicationCacheHost::DOWNLOADING_EVENT:
        return eventNames().downloadingEvent;
    case ApplicationCacheHost::PROGRESS_EVENT:
        return eventNames().progressEvent;
    case ApplicationCacheHost::UPDATEREADY_EVENT:
        return eventNames().updatereadyEvent;
    case ApplicationCacheHost::CACHED_EVENT:
        return eventNames().cachedEvent;
    case ApplicationCacheHost::OBSOLETE_EVENT:            
        return eventNames().obsoleteEvent;
    }
    ASSERT_NOT_REACHED();
    return eventNames().errorEvent;
}

ApplicationCacheHost::EventID DOMApplicationCache::toEventID(const AtomicString& eventType)
{
    if (eventType == eventNames().checkingEvent)
        return ApplicationCacheHost::CHECKING_EVENT;
    if (eventType == eventNames().errorEvent)
        return ApplicationCacheHost::ERROR_EVENT;
    if (eventType == eventNames().noupdateEvent)
        return ApplicationCacheHost::NOUPDATE_EVENT;
    if (eventType == eventNames().downloadingEvent)
        return ApplicationCacheHost::DOWNLOADING_EVENT;
    if (eventType == eventNames().progressEvent)
        return ApplicationCacheHost::PROGRESS_EVENT;
    if (eventType == eventNames().updatereadyEvent)
        return ApplicationCacheHost::UPDATEREADY_EVENT;
    if (eventType == eventNames().cachedEvent)
        return ApplicationCacheHost::CACHED_EVENT;
    if (eventType == eventNames().obsoleteEvent)
        return ApplicationCacheHost::OBSOLETE_EVENT;
  
    ASSERT_NOT_REACHED();
    return ApplicationCacheHost::ERROR_EVENT;
}

} // namespace WebCore

#endif // ENABLE(OFFLINE_WEB_APPLICATIONS)
