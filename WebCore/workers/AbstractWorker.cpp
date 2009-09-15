/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WORKERS)

#include "AbstractWorker.h"

#include "ErrorEvent.h"
#include "Event.h"
#include "EventException.h"
#include "EventNames.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"

namespace WebCore {

AbstractWorker::AbstractWorker(ScriptExecutionContext* context)
    : ActiveDOMObject(context, this)
{
}

AbstractWorker::~AbstractWorker()
{
}

void AbstractWorker::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> eventListener, bool)
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

void AbstractWorker::removeEventListener(const AtomicString& eventType, EventListener* eventListener, bool)
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

bool AbstractWorker::dispatchEvent(PassRefPtr<Event> event, ExceptionCode& ec)
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

void AbstractWorker::dispatchLoadErrorEvent()
{
    RefPtr<Event> evt = Event::create(eventNames().errorEvent, false, true);
    if (m_onErrorListener) {
        evt->setTarget(this);
        evt->setCurrentTarget(this);
        m_onErrorListener->handleEvent(evt.get(), true);
    }

    ExceptionCode ec = 0;
    dispatchEvent(evt.release(), ec);
    ASSERT(!ec);
}

bool AbstractWorker::dispatchScriptErrorEvent(const String& message, const String& sourceURL, int lineNumber)
{
    bool handled = false;
    RefPtr<ErrorEvent> event = ErrorEvent::create(message, sourceURL, static_cast<unsigned>(lineNumber));
    if (m_onErrorListener) {
        event->setTarget(this);
        event->setCurrentTarget(this);
        m_onErrorListener->handleEvent(event.get(), true);
        if (event->defaultPrevented())
            handled = true;
    }

    ExceptionCode ec = 0;
    handled = !dispatchEvent(event.release(), ec);
    ASSERT(!ec);

    return handled;
}

KURL AbstractWorker::resolveURL(const String& url, ExceptionCode& ec)
{
    if (url.isEmpty()) {
        ec = SYNTAX_ERR;
        return KURL();
    }

    // FIXME: This should use the dynamic global scope (bug #27887)
    KURL scriptURL = scriptExecutionContext()->completeURL(url);
    if (!scriptURL.isValid()) {
        ec = SYNTAX_ERR;
        return KURL();
    }

    if (!scriptExecutionContext()->securityOrigin()->canAccess(SecurityOrigin::create(scriptURL).get())) {
        ec = SECURITY_ERR;
        return KURL();
    }
    return scriptURL;
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
