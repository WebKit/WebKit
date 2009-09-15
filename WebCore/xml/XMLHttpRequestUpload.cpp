/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
#include "XMLHttpRequestUpload.h"

#include "AtomicString.h"
#include "Event.h"
#include "EventException.h"
#include "EventNames.h"
#include "XMLHttpRequest.h"
#include "XMLHttpRequestProgressEvent.h"
#include <wtf/Assertions.h>

namespace WebCore {

XMLHttpRequestUpload::XMLHttpRequestUpload(XMLHttpRequest* xmlHttpRequest)
    : m_xmlHttpRequest(xmlHttpRequest)
{
}

bool XMLHttpRequestUpload::hasListeners() const
{
    return m_onAbortListener || m_onErrorListener || m_onLoadListener || m_onLoadStartListener || m_onProgressListener || !m_eventListeners.isEmpty();
}

ScriptExecutionContext* XMLHttpRequestUpload::scriptExecutionContext() const
{
    XMLHttpRequest* xmlHttpRequest = associatedXMLHttpRequest();
    if (!xmlHttpRequest)
        return 0;
    return xmlHttpRequest->scriptExecutionContext();
}

void XMLHttpRequestUpload::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> eventListener, bool)
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

void XMLHttpRequestUpload::removeEventListener(const AtomicString& eventType, EventListener* eventListener, bool)
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

bool XMLHttpRequestUpload::dispatchEvent(PassRefPtr<Event> evt, ExceptionCode& ec)
{
    // FIXME: check for other error conditions enumerated in the spec.
    if (!evt || evt->type().isEmpty()) {
        ec = EventException::UNSPECIFIED_EVENT_TYPE_ERR;
        return true;
    }

    ListenerVector listenersCopy = m_eventListeners.get(evt->type());
    for (ListenerVector::const_iterator listenerIter = listenersCopy.begin(); listenerIter != listenersCopy.end(); ++listenerIter) {
        evt->setTarget(this);
        evt->setCurrentTarget(this);
        listenerIter->get()->handleEvent(evt.get(), false);
    }

    return !evt->defaultPrevented();
}

void XMLHttpRequestUpload::dispatchXMLHttpRequestProgressEvent(EventListener* listener, const AtomicString& type, bool lengthComputable, unsigned loaded, unsigned total)
{
    RefPtr<XMLHttpRequestProgressEvent> evt = XMLHttpRequestProgressEvent::create(type, lengthComputable, loaded, total);
    if (listener) {
        evt->setTarget(this);
        evt->setCurrentTarget(this);
        listener->handleEvent(evt.get(), false);
    }

    ExceptionCode ec = 0;
    dispatchEvent(evt.release(), ec);
    ASSERT(!ec);
}

void XMLHttpRequestUpload::dispatchAbortEvent()
{
    dispatchXMLHttpRequestProgressEvent(m_onAbortListener.get(), eventNames().abortEvent, false, 0, 0);
}

void XMLHttpRequestUpload::dispatchErrorEvent()
{
    dispatchXMLHttpRequestProgressEvent(m_onErrorListener.get(), eventNames().errorEvent, false, 0, 0);
}

void XMLHttpRequestUpload::dispatchLoadEvent()
{
    dispatchXMLHttpRequestProgressEvent(m_onLoadListener.get(), eventNames().loadEvent, false, 0, 0);
}

void XMLHttpRequestUpload::dispatchLoadStartEvent()
{
    dispatchXMLHttpRequestProgressEvent(m_onLoadStartListener.get(), eventNames().loadstartEvent, false, 0, 0);
}

void XMLHttpRequestUpload::dispatchProgressEvent(long long bytesSent, long long totalBytesToBeSent)
{
    dispatchXMLHttpRequestProgressEvent(m_onProgressListener.get(), eventNames().progressEvent, true, static_cast<unsigned>(bytesSent), static_cast<unsigned>(totalBytesToBeSent));
}

} // namespace WebCore
