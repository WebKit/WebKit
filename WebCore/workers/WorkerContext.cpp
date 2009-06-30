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

#include "WorkerContext.h"

#include "ActiveDOMObject.h"
#include "DOMTimer.h"
#include "DOMWindow.h"
#include "Event.h"
#include "EventException.h"
#include "MessageEvent.h"
#include "NotImplemented.h"
#include "ScriptSourceCode.h"
#include "ScriptValue.h"
#include "SecurityOrigin.h"
#include "WorkerLocation.h"
#include "WorkerNavigator.h"
#include "WorkerObjectProxy.h"
#include "WorkerScriptLoader.h"
#include "WorkerThread.h"
#include "WorkerThreadableLoader.h"
#include "XMLHttpRequestException.h"
#include <wtf/RefPtr.h>

namespace WebCore {

WorkerContext::WorkerContext(const KURL& url, const String& userAgent, WorkerThread* thread)
    : m_url(url)
    , m_userAgent(userAgent)
    , m_script(new WorkerScriptController(this))
    , m_thread(thread)
    , m_closing(false)
{
    setSecurityOrigin(SecurityOrigin::create(url));
}

WorkerContext::~WorkerContext()
{
    ASSERT(currentThread() == m_thread->threadID());

    m_thread->workerObjectProxy().workerContextDestroyed();
}

ScriptExecutionContext* WorkerContext::scriptExecutionContext() const
{
    return const_cast<WorkerContext*>(this);
}

const KURL& WorkerContext::virtualURL() const
{
    return m_url;
}

KURL WorkerContext::virtualCompleteURL(const String& url) const
{
    return completeURL(url);
}

KURL WorkerContext::completeURL(const String& url) const
{
    // Always return a null URL when passed a null string.
    // FIXME: Should we change the KURL constructor to have this behavior?
    if (url.isNull())
        return KURL();
    // Always use UTF-8 in Workers.
    return KURL(m_url, url);
}

String WorkerContext::userAgent(const KURL&) const
{
    return m_userAgent;
}

WorkerLocation* WorkerContext::location() const
{
    if (!m_location)
        m_location = WorkerLocation::create(m_url);
    return m_location.get();
}

void WorkerContext::close()
{
    if (m_closing)
        return;

    m_closing = true;
    m_thread->stop();
}

WorkerNavigator* WorkerContext::navigator() const
{
    if (!m_navigator)
        m_navigator = WorkerNavigator::create(m_userAgent);
    return m_navigator.get();
}

bool WorkerContext::hasPendingActivity() const
{
    ActiveDOMObjectsMap& activeObjects = activeDOMObjects();
    ActiveDOMObjectsMap::const_iterator activeObjectsEnd = activeObjects.end();
    for (ActiveDOMObjectsMap::const_iterator iter = activeObjects.begin(); iter != activeObjectsEnd; ++iter) {
        if (iter->first->hasPendingActivity())
            return true;
    }

    // Keep the worker active as long as there is a MessagePort with pending activity or that is remotely entangled.
    HashSet<MessagePort*>::const_iterator messagePortsEnd = messagePorts().end();
    for (HashSet<MessagePort*>::const_iterator iter = messagePorts().begin(); iter != messagePortsEnd; ++iter) {
        if ((*iter)->hasPendingActivity() || ((*iter)->isEntangled() && !(*iter)->locallyEntangledPort()))
            return true;
    }

    return false;
}

void WorkerContext::reportException(const String& errorMessage, int lineNumber, const String& sourceURL)
{
    m_thread->workerObjectProxy().postExceptionToWorkerObject(errorMessage, lineNumber, sourceURL);
}

void WorkerContext::addMessage(MessageDestination destination, MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceURL)
{
    m_thread->workerObjectProxy().postConsoleMessageToWorkerObject(destination, source, level, message, lineNumber, sourceURL);
}

void WorkerContext::resourceRetrievedByXMLHttpRequest(unsigned long, const ScriptString&)
{
    // FIXME: The implementation is pending the fixes in https://bugs.webkit.org/show_bug.cgi?id=23175
    notImplemented();
}

void WorkerContext::scriptImported(unsigned long, const String&)
{
    // FIXME: The implementation is pending the fixes in https://bugs.webkit.org/show_bug.cgi?id=23175
    notImplemented();
}

void WorkerContext::postMessage(const String& message, ExceptionCode& ec)
{
    postMessage(message, 0, ec);
}

void WorkerContext::postMessage(const String& message, MessagePort* port, ExceptionCode& ec)
{
    if (m_closing)
        return;
    // Disentangle the port in preparation for sending it to the remote context.
    OwnPtr<MessagePortChannel> channel = port ? port->disentangle(ec) : 0;
    if (ec)
        return;
    m_thread->workerObjectProxy().postMessageToWorkerObject(message, channel.release());
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

void WorkerContext::removeEventListener(const AtomicString& eventType, EventListener* eventListener, bool)
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

void WorkerContext::postTask(PassRefPtr<Task> task)
{
    thread()->runLoop().postTask(task);
}

int WorkerContext::setTimeout(ScheduledAction* action, int timeout)
{
    return DOMTimer::install(scriptExecutionContext(), action, timeout, true);
}

void WorkerContext::clearTimeout(int timeoutId)
{
    DOMTimer::removeById(scriptExecutionContext(), timeoutId);
}

int WorkerContext::setInterval(ScheduledAction* action, int timeout)
{
    return DOMTimer::install(scriptExecutionContext(), action, timeout, false);
}

void WorkerContext::clearInterval(int timeoutId)
{
    DOMTimer::removeById(scriptExecutionContext(), timeoutId);
}

void WorkerContext::dispatchMessage(const String& message, PassRefPtr<MessagePort> port)
{
    // Since close() stops the thread event loop, this should not ever get called while closing.
    ASSERT(!m_closing);
    RefPtr<Event> evt = MessageEvent::create(message, "", "", 0, port);

    if (m_onmessageListener.get()) {
        evt->setTarget(this);
        evt->setCurrentTarget(this);
        m_onmessageListener->handleEvent(evt.get(), false);
    }

    ExceptionCode ec = 0;
    dispatchEvent(evt.release(), ec);
    ASSERT(!ec);
}

void WorkerContext::importScripts(const Vector<String>& urls, const String& callerURL, int callerLine, ExceptionCode& ec)
{
    ec = 0;
    Vector<String>::const_iterator urlsEnd = urls.end();
    Vector<KURL> completedURLs;
    for (Vector<String>::const_iterator it = urls.begin(); it != urlsEnd; ++it) {
        const KURL& url = scriptExecutionContext()->completeURL(*it);
        if (!url.isValid()) {
            ec = SYNTAX_ERR;
            return;
        }
        completedURLs.append(url);
    }
    String securityOrigin = scriptExecutionContext()->securityOrigin()->toString();
    Vector<KURL>::const_iterator end = completedURLs.end();

    for (Vector<KURL>::const_iterator it = completedURLs.begin(); it != end; ++it) {
        WorkerScriptLoader scriptLoader;
        scriptLoader.loadSynchronously(scriptExecutionContext(), *it, AllowCrossOriginRedirect);

        // If the fetching attempt failed, throw a NETWORK_ERR exception and abort all these steps.
        if (scriptLoader.failed()) {
            ec = XMLHttpRequestException::NETWORK_ERR;
            return;
        }

        scriptExecutionContext()->scriptImported(scriptLoader.identifier(), scriptLoader.script());
        scriptExecutionContext()->addMessage(InspectorControllerDestination, JSMessageSource, LogMessageLevel, "Worker script imported: \"" + *it + "\".", callerLine, callerURL);

        ScriptValue exception;
        m_script->evaluate(ScriptSourceCode(scriptLoader.script(), *it), &exception);
        if (!exception.hasNoValue()) {
            m_script->setException(exception);
            return;
        }
    }
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
