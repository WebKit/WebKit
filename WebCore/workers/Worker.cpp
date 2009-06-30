/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
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
 *
 */

#include "config.h"

#if ENABLE(WORKERS)

#include "Worker.h"

#include "DOMWindow.h"
#include "DocLoader.h"
#include "Document.h"
#include "EventException.h"
#include "EventListener.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "MessageEvent.h"
#include "SecurityOrigin.h"
#include "TextEncoding.h"
#include "WorkerContextProxy.h"
#include "WorkerScriptLoader.h"
#include "WorkerThread.h"
#include <wtf/MainThread.h>

namespace WebCore {

Worker::Worker(const String& url, ScriptExecutionContext* context, ExceptionCode& ec)
    : ActiveDOMObject(context, this)
    , m_contextProxy(WorkerContextProxy::create(this))
{
    m_scriptURL = context->completeURL(url);
    if (url.isEmpty() || !m_scriptURL.isValid()) {
        ec = SYNTAX_ERR;
        return;
    }

    if (!context->securityOrigin()->canAccess(SecurityOrigin::create(m_scriptURL).get())) {
        ec = SECURITY_ERR;
        return;
    }

    m_scriptLoader = new WorkerScriptLoader();
    m_scriptLoader->loadAsynchronously(scriptExecutionContext(), m_scriptURL, DenyCrossOriginRedirect, this);
    setPendingActivity(this);  // The worker context does not exist while loading, so we must ensure that the worker object is not collected, as well as its event listeners.
}

Worker::~Worker()
{
    ASSERT(isMainThread());
    ASSERT(scriptExecutionContext()); // The context is protected by worker context proxy, so it cannot be destroyed while a Worker exists.
    m_contextProxy->workerObjectDestroyed();
}

void Worker::postMessage(const String& message, ExceptionCode& ec)
{
    postMessage(message, 0, ec);
}

void Worker::postMessage(const String& message, MessagePort* messagePort, ExceptionCode& ec)
{
    // Disentangle the port in preparation for sending it to the remote context.
    OwnPtr<MessagePortChannel> channel = messagePort ? messagePort->disentangle(ec) : 0;
    if (ec)
        return;
    m_contextProxy->postMessageToWorkerContext(message, channel.release());
}

void Worker::terminate()
{
    m_contextProxy->terminateWorkerContext();
}

bool Worker::canSuspend() const
{
    // FIXME: It is not currently possible to suspend a worker, so pages with workers can not go into page cache.
    return false;
}

void Worker::stop()
{
    terminate();
}

bool Worker::hasPendingActivity() const
{
    return m_contextProxy->hasPendingActivity() || ActiveDOMObject::hasPendingActivity();
}

void Worker::notifyFinished()
{
    if (m_scriptLoader->failed())
        dispatchErrorEvent();
    else
        m_contextProxy->startWorkerContext(m_scriptURL, scriptExecutionContext()->userAgent(m_scriptURL), m_scriptLoader->script());

    m_scriptLoader = 0;

    unsetPendingActivity(this);
}

void Worker::dispatchErrorEvent()
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

void Worker::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> eventListener, bool)
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

void Worker::removeEventListener(const AtomicString& eventType, EventListener* eventListener, bool)
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

bool Worker::dispatchEvent(PassRefPtr<Event> event, ExceptionCode& ec)
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

void Worker::dispatchMessage(const String& message, PassRefPtr<MessagePort> port)
{
    RefPtr<Event> evt = MessageEvent::create(message, "", "", 0, port);

    if (m_onMessageListener.get()) {
        evt->setTarget(this);
        evt->setCurrentTarget(this);
        m_onMessageListener->handleEvent(evt.get(), false);
    }

    ExceptionCode ec = 0;
    dispatchEvent(evt.release(), ec);
    ASSERT(!ec);
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
