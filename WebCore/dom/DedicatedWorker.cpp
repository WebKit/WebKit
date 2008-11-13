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

#include "DedicatedWorker.h"

#include "CachedScript.h"
#include "DOMWindow.h"
#include "DocLoader.h"
#include "Document.h"
#include "Event.h"
#include "EventException.h"
#include "EventListener.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "FrameLoader.h"
#include "MessageEvent.h"
#include "MessagePort.h"
#include "SecurityOrigin.h"
#include "Timer.h"
#include "WorkerContext.h"
#include "WorkerTask.h"
#include "WorkerThread.h"
#include <wtf/MainThread.h>

namespace WebCore {

class WorkerThreadScriptLoadTimer : public TimerBase {
public:
    WorkerThreadScriptLoadTimer(PassRefPtr<DedicatedWorker> worker)
        : m_worker(worker)
    {
        ASSERT(m_worker);
    }

private:
    virtual void fired()
    {
        m_worker->startLoad();
        delete this;
    }

    RefPtr<DedicatedWorker> m_worker;
};

class WorkerConnectTask : public WorkerTask {
public:
    WorkerConnectTask(const String& message, PassRefPtr<MessagePort> port)
        : m_message(message.copy())
        , m_port(port)
    {
    }

    virtual void performTask(WorkerContext* context)
    {
        m_port->attachToContext(context);

        RefPtr<Event> evt = MessageEvent::create(m_message, "", "", 0, m_port);

        if (context->onconnect()) {
            evt->setTarget(context);
            evt->setCurrentTarget(context);
            context->onconnect()->handleEvent(evt.get(), false);
        }

        ExceptionCode ec = 0;
        context->dispatchEvent(evt.release(), ec);
        ASSERT(!ec);
    }

private:
    String m_message;
    RefPtr<MessagePort> m_port;
};

DedicatedWorker::DedicatedWorker(const String& url, Document* document, ExceptionCode& ec)
    : ActiveDOMObject(document, this)
    , m_thread(0)
{
    m_scriptURL = document->completeURL(url);
    if (!m_scriptURL.isValid()) {
        ec = SYNTAX_ERR;
        return;
    }

    if (!document->securityOrigin()->canAccess(SecurityOrigin::create(m_scriptURL).get())) {
        ec = SECURITY_ERR;
        return;
    }

    WorkerThreadScriptLoadTimer* timer = new WorkerThreadScriptLoadTimer(this);
    timer->startOneShot(0);
}

DedicatedWorker::~DedicatedWorker()
{
    ASSERT(isMainThread());
}

Document* DedicatedWorker::document() const
{
    ASSERT(scriptExecutionContext()->isDocument());
    return static_cast<Document*>(scriptExecutionContext());
}

PassRefPtr<MessagePort> DedicatedWorker::connect(ScriptExecutionContext* scriptExecutionContext, const String& message)
{
    RefPtr<MessagePort> port = MessagePort::create(scriptExecutionContext);
    RefPtr<MessagePort> otherPort = MessagePort::create(0);
    MessagePort::entangle(port.get(), otherPort.get());
    if (m_thread)
        m_thread->messageQueue().append(new WorkerConnectTask(message, otherPort));
    else
        m_queuedEarlyTasks.append(new WorkerConnectTask(message, otherPort));

    return port;
}

void DedicatedWorker::close()
{
    // Not implemented.
}

void DedicatedWorker::postMessage(const String& /*message*/, MessagePort* /*port*/)
{
    // Not implemented.
}

void DedicatedWorker::startLoad()
{
    // The document may have been destroyed already.
    // FIXME: Should we explicitly check whether the document has been detached, and cancel loading in that case?
    if (!document())
        return;

    m_cachedScript = document()->docLoader()->requestScript(m_scriptURL, document()->charset());

    if (m_cachedScript) {
        setPendingActivity(this);  // The worker context does not exist while loading, so we much ensure that the worker object is not collected, as well as its event listeners.
        m_cachedScript->addClient(this);
        return;
    }

    dispatchErrorEvent();
}

void DedicatedWorker::notifyFinished(CachedResource* resource)
{
    ASSERT(resource == m_cachedScript.get());
    if (m_cachedScript->errorOccurred()) {
        dispatchErrorEvent();
        unsetPendingActivity(this);
    } else  {
        // FIXME: When can we unsetPendingActivity()? Currently, worker objects are never collected, and threads never exit.

        RefPtr<WorkerThread> thread = WorkerThread::create(m_scriptURL, m_cachedScript->script(), this);
        m_thread = thread.get();

        unsigned taskCount = m_queuedEarlyTasks.size();
        for (unsigned i = 0; i < taskCount; ++i)
            m_thread->messageQueue().append(m_queuedEarlyTasks[i]);

        m_thread->start();
    }

    m_queuedEarlyTasks.clear();

    m_cachedScript->removeClient(this);
    m_cachedScript = 0;
}

void DedicatedWorker::dispatchErrorEvent()
{
    if (m_onErrorListener) {
        RefPtr<Event> evt = Event::create(eventNames().errorEvent, false, true);
        // DedicatedWorker is not an EventTarget, so target and currentTarget remain null.
        m_onErrorListener->handleEvent(evt.release().get(), true);
    }
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
