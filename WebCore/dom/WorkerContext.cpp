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

#include "WorkerContext.h"

#include "ActiveDOMObject.h"
#include "DOMWindow.h"
#include "Event.h"
#include "EventException.h"
#include "SecurityOrigin.h"
#include "WorkerLocation.h"
#include "WorkerMessagingProxy.h"
#include "WorkerTask.h"
#include "WorkerThread.h"

namespace WebCore {

WorkerContext::WorkerContext(const KURL& url, WorkerThread* thread)
    : m_url(url)
    , m_location(WorkerLocation::create(url))
    , m_script(new WorkerScriptController(this))
    , m_thread(thread)
{
    setSecurityOrigin(SecurityOrigin::create(url));
}

WorkerContext::~WorkerContext()
{
    ASSERT(currentThread() == m_thread->threadID());

    m_thread->messagingProxy()->workerContextDestroyed();
}

ScriptExecutionContext* WorkerContext::scriptExecutionContext() const
{
    return const_cast<WorkerContext*>(this);
}


const KURL& WorkerContext::virtualURL() const
{
    return m_url;
}

KURL WorkerContext::completeURL(const String& url) const
{
    // Always return a null URL when passed a null string.
    // FIXME: Should we change the KURL constructor to have this behavior?
    if (url.isNull())
        return KURL();
    // FIXME: does this need to provide a charset, like Document::completeURL does?
    return KURL(m_location->url(), url);
}

bool WorkerContext::hasPendingActivity() const
{
    ActiveDOMObjectsMap& activeObjects = activeDOMObjects();
    ActiveDOMObjectsMap::const_iterator activeObjectsEnd = activeObjects.end();
    for (ActiveDOMObjectsMap::const_iterator iter = activeObjects.begin(); iter != activeObjectsEnd; ++iter) {
        if (iter->first->hasPendingActivity())
            return true;
    }
    return false;
}

void WorkerContext::reportException(const String& errorMessage, int lineNumber, const String& sourceURL)
{
    m_thread->messagingProxy()->postWorkerException(errorMessage, lineNumber, sourceURL);
}

void WorkerContext::postMessage(const String& message)
{
    m_thread->messagingProxy()->postMessageToWorkerObject(message);
}

void WorkerContext::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> eventListener, bool)
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

void WorkerContext::removeEventListener(const AtomicString& eventType, EventListener* eventListener, bool useCapture)
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

bool WorkerContext::dispatchEvent(PassRefPtr<Event> event, ExceptionCode& ec)
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

class ScriptExecutionContextTaskWorkerTask : public WorkerTask {
public:
    static PassRefPtr<ScriptExecutionContextTaskWorkerTask> create(PassRefPtr<ScriptExecutionContext::Task> task)
    {
        return adoptRef(new ScriptExecutionContextTaskWorkerTask(task));
    }

private:
    ScriptExecutionContextTaskWorkerTask(PassRefPtr<ScriptExecutionContext::Task> task)
        : m_task(task)
    {
    }

    virtual void performTask(WorkerContext* context)
    {
        m_task->performTask(context);
    }

    RefPtr<ScriptExecutionContext::Task> m_task;
};

void WorkerContext::postTask(PassRefPtr<Task> task)
{
    thread()->messageQueue().append(ScriptExecutionContextTaskWorkerTask::create(task));
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
