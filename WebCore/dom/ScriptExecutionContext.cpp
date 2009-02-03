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
#include "ScriptExecutionContext.h"

#include "ActiveDOMObject.h"
#include "Document.h"
#include "MessagePort.h"
#include "SecurityOrigin.h"
#include "WorkerContext.h"
#include "WorkerThread.h"
#include <wtf/MainThread.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

class ProcessMessagesSoonTask : public ScriptExecutionContext::Task {
public:
    static PassRefPtr<ProcessMessagesSoonTask> create()
    {
        return adoptRef(new ProcessMessagesSoonTask);
    }

    virtual void performTask(ScriptExecutionContext* context)
    {
        context->dispatchMessagePortEvents();
    }
};

ScriptExecutionContext::ScriptExecutionContext()
{
}

ScriptExecutionContext::~ScriptExecutionContext()
{
    HashMap<ActiveDOMObject*, void*>::iterator activeObjectsEnd = m_activeDOMObjects.end();
    for (HashMap<ActiveDOMObject*, void*>::iterator iter = m_activeDOMObjects.begin(); iter != activeObjectsEnd; ++iter) {
        ASSERT(iter->first->scriptExecutionContext() == this);
        iter->first->contextDestroyed();
    }

    HashSet<MessagePort*>::iterator messagePortsEnd = m_messagePorts.end();
    for (HashSet<MessagePort*>::iterator iter = m_messagePorts.begin(); iter != messagePortsEnd; ++iter) {
        ASSERT((*iter)->scriptExecutionContext() == this);
        (*iter)->contextDestroyed();
    }
}

void ScriptExecutionContext::processMessagePortMessagesSoon()
{
    postTask(ProcessMessagesSoonTask::create());
}

void ScriptExecutionContext::dispatchMessagePortEvents()
{
    RefPtr<ScriptExecutionContext> protect(this);

    // Make a frozen copy.
    Vector<MessagePort*> ports;
    copyToVector(m_messagePorts, ports);

    unsigned portCount = ports.size();
    for (unsigned i = 0; i < portCount; ++i) {
        MessagePort* port = ports[i];
        // The port may be destroyed, and another one created at the same address, but this is safe, as the worst that can happen
        // as a result is that dispatchMessages() will be called needlessly.
        if (m_messagePorts.contains(port) && port->queueIsOpen())
            port->dispatchMessages();
    }
}

void ScriptExecutionContext::createdMessagePort(MessagePort* port)
{
    ASSERT(port);
#if ENABLE(WORKERS)
    ASSERT((isDocument() && isMainThread())
        || (isWorkerContext() && currentThread() == static_cast<WorkerContext*>(this)->thread()->threadID()));
#endif

    m_messagePorts.add(port);
}

void ScriptExecutionContext::destroyedMessagePort(MessagePort* port)
{
    ASSERT(port);
#if ENABLE(WORKERS)
    ASSERT((isDocument() && isMainThread())
        || (isWorkerContext() && currentThread() == static_cast<WorkerContext*>(this)->thread()->threadID()));
#endif

    m_messagePorts.remove(port);
}

bool ScriptExecutionContext::canSuspendActiveDOMObjects()
{
    // No protection against m_activeDOMObjects changing during iteration: canSuspend() shouldn't execute arbitrary JS.
    HashMap<ActiveDOMObject*, void*>::iterator activeObjectsEnd = m_activeDOMObjects.end();
    for (HashMap<ActiveDOMObject*, void*>::iterator iter = m_activeDOMObjects.begin(); iter != activeObjectsEnd; ++iter) {
        ASSERT(iter->first->scriptExecutionContext() == this);
        if (!iter->first->canSuspend())
            return false;
    }
    return true;
}

void ScriptExecutionContext::suspendActiveDOMObjects()
{
    // No protection against m_activeDOMObjects changing during iteration: suspend() shouldn't execute arbitrary JS.
    HashMap<ActiveDOMObject*, void*>::iterator activeObjectsEnd = m_activeDOMObjects.end();
    for (HashMap<ActiveDOMObject*, void*>::iterator iter = m_activeDOMObjects.begin(); iter != activeObjectsEnd; ++iter) {
        ASSERT(iter->first->scriptExecutionContext() == this);
        iter->first->suspend();
    }
}

void ScriptExecutionContext::resumeActiveDOMObjects()
{
    // No protection against m_activeDOMObjects changing during iteration: resume() shouldn't execute arbitrary JS.
    HashMap<ActiveDOMObject*, void*>::iterator activeObjectsEnd = m_activeDOMObjects.end();
    for (HashMap<ActiveDOMObject*, void*>::iterator iter = m_activeDOMObjects.begin(); iter != activeObjectsEnd; ++iter) {
        ASSERT(iter->first->scriptExecutionContext() == this);
        iter->first->resume();
    }
}

void ScriptExecutionContext::stopActiveDOMObjects()
{
    // No protection against m_activeDOMObjects changing during iteration: stop() shouldn't execute arbitrary JS.
    HashMap<ActiveDOMObject*, void*>::iterator activeObjectsEnd = m_activeDOMObjects.end();
    for (HashMap<ActiveDOMObject*, void*>::iterator iter = m_activeDOMObjects.begin(); iter != activeObjectsEnd; ++iter) {
        ASSERT(iter->first->scriptExecutionContext() == this);
        iter->first->stop();
    }
}

void ScriptExecutionContext::createdActiveDOMObject(ActiveDOMObject* object, void* upcastPointer)
{
    ASSERT(object);
    ASSERT(upcastPointer);
    m_activeDOMObjects.add(object, upcastPointer);
}

void ScriptExecutionContext::destroyedActiveDOMObject(ActiveDOMObject* object)
{
    ASSERT(object);
    m_activeDOMObjects.remove(object);
}

void ScriptExecutionContext::setSecurityOrigin(PassRefPtr<SecurityOrigin> securityOrigin)
{
    m_securityOrigin = securityOrigin;
}

void ScriptExecutionContext::addTimeout(int timeoutId, DOMTimer* timer)
{
    ASSERT(!m_timeouts.contains(timeoutId));
    m_timeouts.set(timeoutId, timer);
}

void ScriptExecutionContext::removeTimeout(int timeoutId)
{
    m_timeouts.remove(timeoutId);
}

DOMTimer* ScriptExecutionContext::findTimeout(int timeoutId)
{
    return m_timeouts.get(timeoutId);
}

ScriptExecutionContext::Task::~Task()
{
}

} // namespace WebCore
