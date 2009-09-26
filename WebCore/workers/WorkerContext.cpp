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
#include "MessagePort.h"
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

#if ENABLE(NOTIFICATIONS)
#include "NotificationCenter.h"
#endif

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
    ASSERT(currentThread() == thread()->threadID());
#if ENABLE(NOTIFICATIONS)
    m_notifications.clear();
#endif
    // Notify proxy that we are going away. This can free the WorkerThread object, so do not access it after this.
    thread()->workerReportingProxy().workerContextDestroyed();
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
    // Notify parent that this context is closed. Parent is responsible for calling WorkerThread::stop().
    thread()->workerReportingProxy().workerContextClosed();
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

void WorkerContext::importScripts(const Vector<String>& urls, const String& callerURL, int callerLine, ExceptionCode& ec)
{
#if !ENABLE(INSPECTOR)
    UNUSED_PARAM(callerURL);
    UNUSED_PARAM(callerLine);
#endif
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
    Vector<KURL>::const_iterator end = completedURLs.end();

    for (Vector<KURL>::const_iterator it = completedURLs.begin(); it != end; ++it) {
        WorkerScriptLoader scriptLoader;
        scriptLoader.loadSynchronously(scriptExecutionContext(), *it, AllowCrossOriginRequests);

        // If the fetching attempt failed, throw a NETWORK_ERR exception and abort all these steps.
        if (scriptLoader.failed()) {
            ec = XMLHttpRequestException::NETWORK_ERR;
            return;
        }

        scriptExecutionContext()->scriptImported(scriptLoader.identifier(), scriptLoader.script());
#if ENABLE(INSPECTOR)
        scriptExecutionContext()->addMessage(InspectorControllerDestination, JSMessageSource, LogMessageType, LogMessageLevel, "Worker script imported: \"" + *it + "\".", callerLine, callerURL);
#endif

        ScriptValue exception;
        m_script->evaluate(ScriptSourceCode(scriptLoader.script(), *it), &exception);
        if (!exception.hasNoValue()) {
            m_script->setException(exception);
            return;
        }
    }
}

void WorkerContext::reportException(const String& errorMessage, int lineNumber, const String& sourceURL)
{
    bool errorHandled = false;
    if (onerror())
        errorHandled = onerror()->reportError(errorMessage, sourceURL, lineNumber);

    if (!errorHandled)
        thread()->workerReportingProxy().postExceptionToWorkerObject(errorMessage, lineNumber, sourceURL);
}

void WorkerContext::addMessage(MessageDestination destination, MessageSource source, MessageType type, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceURL)
{
    thread()->workerReportingProxy().postConsoleMessageToWorkerObject(destination, source, type, level, message, lineNumber, sourceURL);
}

#if ENABLE(NOTIFICATIONS)
NotificationCenter* WorkerContext::webkitNotifications() const
{
    if (!m_notifications)
        m_notifications = NotificationCenter::create(scriptExecutionContext(), m_thread->getNotificationPresenter());
    return m_notifications.get();
}
#endif

EventTargetData* WorkerContext::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* WorkerContext::ensureEventTargetData()
{
    return &m_eventTargetData;
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
