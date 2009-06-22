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

#include "ApplicationCache.h"
#include "ApplicationCacheGroup.h"
#include "ApplicationCacheResource.h"
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
}

void DOMApplicationCache::disconnectFrame()
{
    m_frame = 0;
}

ApplicationCache* DOMApplicationCache::associatedCache() const
{
    if (!m_frame)
        return 0;
 
    return m_frame->loader()->documentLoader()->applicationCache();
}

unsigned short DOMApplicationCache::status() const
{
    ApplicationCache* cache = associatedCache();    
    if (!cache)
        return UNCACHED;

    switch (cache->group()->updateStatus()) {
        case ApplicationCacheGroup::Checking:
            return CHECKING;
        case ApplicationCacheGroup::Downloading:
            return DOWNLOADING;
        case ApplicationCacheGroup::Idle: {
            if (cache->group()->isObsolete())
                return OBSOLETE;
            if (cache != cache->group()->newestCache())
                return UPDATEREADY;
            return IDLE;
        }
    }

    ASSERT_NOT_REACHED();
    return 0;
}

void DOMApplicationCache::update(ExceptionCode& ec)
{
    ApplicationCache* cache = associatedCache();
    if (!cache) {
        ec = INVALID_STATE_ERR;
        return;
    }
    
    cache->group()->update(m_frame, ApplicationCacheUpdateWithoutBrowsingContext);
}

bool DOMApplicationCache::swapCache()
{
    if (!m_frame)
        return false;
    
    ApplicationCache* cache = m_frame->loader()->documentLoader()->applicationCache();
    if (!cache)
        return false;

    // If the group of application caches to which cache belongs has the lifecycle status obsolete, unassociate document from cache.
    if (cache->group()->isObsolete()) {
        cache->group()->disassociateDocumentLoader(m_frame->loader()->documentLoader());
        return true;
    }

    // If there is no newer cache, raise an INVALID_STATE_ERR exception.
    ApplicationCache* newestCache = cache->group()->newestCache();
    if (cache == newestCache)
        return false;
    
    ASSERT(cache->group() == newestCache->group());
    m_frame->loader()->documentLoader()->setApplicationCache(newestCache);
    
    return true;
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
            if (*listenerIter == eventListener)
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

void DOMApplicationCache::callCheckingListener()
{
    callListener(eventNames().checkingEvent, m_onCheckingListener.get());
}

void DOMApplicationCache::callErrorListener()
{
    callListener(eventNames().errorEvent, m_onErrorListener.get());
}

void DOMApplicationCache::callNoUpdateListener()
{
    callListener(eventNames().noupdateEvent, m_onNoUpdateListener.get());
}

void DOMApplicationCache::callDownloadingListener()
{
    callListener(eventNames().downloadingEvent, m_onDownloadingListener.get());
}

void DOMApplicationCache::callProgressListener()
{
    callListener(eventNames().progressEvent, m_onProgressListener.get());
}

void DOMApplicationCache::callUpdateReadyListener()
{
    callListener(eventNames().updatereadyEvent, m_onUpdateReadyListener.get());
}

void DOMApplicationCache::callCachedListener()
{
    callListener(eventNames().cachedEvent, m_onCachedListener.get());
}

void DOMApplicationCache::callObsoleteListener()
{
    callListener(eventNames().obsoleteEvent, m_onObsoleteListener.get());
}

} // namespace WebCore

#endif // ENABLE(OFFLINE_WEB_APPLICATIONS)
