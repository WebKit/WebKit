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

#include "WorkerMessagingProxy.h"

#include "DOMWindow.h"
#include "Document.h"
#include "MessageEvent.h"
#include "Worker.h"
#include "WorkerContext.h"
#include "WorkerTask.h"
#include "WorkerThread.h"

namespace WebCore {

class MessageWorkerContextTask : public WorkerTask {
public:
    static PassRefPtr<MessageWorkerContextTask> create(const String& message)
    {
        return adoptRef(new MessageWorkerContextTask(message));
    }

private:
    MessageWorkerContextTask(const String& message)
        : m_message(message.copy())
    {
    }

    virtual void performTask(WorkerContext* context)
    {
        RefPtr<Event> evt = MessageEvent::create(m_message, "", "", 0, 0);

        if (context->onmessage()) {
            evt->setTarget(context);
            evt->setCurrentTarget(context);
            context->onmessage()->handleEvent(evt.get(), false);
        }

        ExceptionCode ec = 0;
        context->dispatchEvent(evt.release(), ec);
        ASSERT(!ec);

        context->thread()->messagingProxy()->confirmWorkerThreadMessage(context->hasPendingActivity());
    }

private:
    String m_message;
};

class MessageWorkerTask : public ScriptExecutionContext::Task {
public:
    static PassRefPtr<MessageWorkerTask> create(const String& message, WorkerMessagingProxy* messagingProxy)
    {
        return adoptRef(new MessageWorkerTask(message, messagingProxy));
    }

private:
    MessageWorkerTask(const String& message, WorkerMessagingProxy* messagingProxy)
        : m_message(message.copy())
        , m_messagingProxy(messagingProxy)
    {
    }

    virtual void performTask(ScriptExecutionContext*)
    {
        Worker* workerObject = m_messagingProxy->workerObject();
        if (!workerObject)
            return;

        RefPtr<Event> evt = MessageEvent::create(m_message, "", "", 0, 0);

        if (workerObject->onmessage()) {
            evt->setTarget(workerObject);
            evt->setCurrentTarget(workerObject);
            workerObject->onmessage()->handleEvent(evt.get(), false);
        }

        ExceptionCode ec = 0;
        workerObject->dispatchEvent(evt.release(), ec);
        ASSERT(!ec);
    }

private:
    String m_message;
    WorkerMessagingProxy* m_messagingProxy;
};

class WorkerExceptionTask : public ScriptExecutionContext::Task {
public:
    static PassRefPtr<WorkerExceptionTask> create(const String& errorMessage, int lineNumber, const String& sourceURL)
    {
        return adoptRef(new WorkerExceptionTask(errorMessage, lineNumber, sourceURL));
    }

private:
    WorkerExceptionTask(const String& errorMessage, int lineNumber, const String& sourceURL)
        : m_errorMessage(errorMessage.copy())
        , m_lineNumber(lineNumber)
        , m_sourceURL(sourceURL.copy())
    {
    }

    virtual void performTask(ScriptExecutionContext* context)
    {
        context->reportException(m_errorMessage, m_lineNumber, m_sourceURL);
    }

private:
    String m_errorMessage;
    int m_lineNumber;
    String m_sourceURL;
};

class WorkerContextDestroyedTask : public ScriptExecutionContext::Task {
public:
    static PassRefPtr<WorkerContextDestroyedTask> create(WorkerMessagingProxy* messagingProxy)
    {
        return adoptRef(new WorkerContextDestroyedTask(messagingProxy));
    }

private:
    WorkerContextDestroyedTask(WorkerMessagingProxy* messagingProxy)
        : m_messagingProxy(messagingProxy)
    {
    }

    virtual void performTask(ScriptExecutionContext* context)
    {
        m_messagingProxy->workerContextDestroyedInternal();
    }

private:
    WorkerMessagingProxy* m_messagingProxy;
};

class WorkerThreadActivityReportTask : public ScriptExecutionContext::Task {
public:
    static PassRefPtr<WorkerThreadActivityReportTask> create(WorkerMessagingProxy* messagingProxy, bool confirmingMessage, bool hasPendingActivity)
    {
        return adoptRef(new WorkerThreadActivityReportTask(messagingProxy, confirmingMessage, hasPendingActivity));
    }

private:
    WorkerThreadActivityReportTask(WorkerMessagingProxy* messagingProxy, bool confirmingMessage, bool hasPendingActivity)
        : m_messagingProxy(messagingProxy)
        , m_confirmingMessage(confirmingMessage)
        , m_hasPendingActivity(hasPendingActivity)
    {
    }

    virtual void performTask(ScriptExecutionContext* context)
    {
        m_messagingProxy->reportWorkerThreadActivityInternal(m_confirmingMessage, m_hasPendingActivity);
    }

private:
    WorkerMessagingProxy* m_messagingProxy;
    bool m_confirmingMessage;
    bool m_hasPendingActivity;
};


WorkerMessagingProxy::WorkerMessagingProxy(PassRefPtr<ScriptExecutionContext> scriptExecutionContext, Worker* workerObject)
    : m_scriptExecutionContext(scriptExecutionContext)
    , m_workerObject(workerObject)
    , m_unconfirmedMessageCount(1) // Worker initialization counts as a pending message.
    , m_workerThreadHadPendingActivity(false)
{
    ASSERT(m_workerObject);
    ASSERT((m_scriptExecutionContext->isDocument() && isMainThread())
        || (m_scriptExecutionContext->isWorkerContext() && currentThread() == static_cast<WorkerContext*>(m_scriptExecutionContext.get())->thread()->threadID()));
}

WorkerMessagingProxy::~WorkerMessagingProxy()
{
    ASSERT(!m_workerObject);
    ASSERT((m_scriptExecutionContext->isDocument() && isMainThread())
        || (m_scriptExecutionContext->isWorkerContext() && currentThread() == static_cast<WorkerContext*>(m_scriptExecutionContext.get())->thread()->threadID()));
}

void WorkerMessagingProxy::postMessageToWorkerObject(const String& message)
{
    m_scriptExecutionContext->postTask(MessageWorkerTask::create(message, this));
}

void WorkerMessagingProxy::postMessageToWorkerContext(const String& message)
{
    ++m_unconfirmedMessageCount;
    if (m_workerThread)
        m_workerThread->messageQueue().append(MessageWorkerContextTask::create(message));
    else
        m_queuedEarlyTasks.append(MessageWorkerContextTask::create(message));
}

void WorkerMessagingProxy::postWorkerException(const String& errorMessage, int lineNumber, const String& sourceURL)
{
    m_scriptExecutionContext->postTask(WorkerExceptionTask::create(errorMessage, lineNumber, sourceURL));
}

void WorkerMessagingProxy::workerThreadCreated(PassRefPtr<WorkerThread> workerThread)
{
    m_workerThread = workerThread;

    unsigned taskCount = m_queuedEarlyTasks.size();
    for (unsigned i = 0; i < taskCount; ++i)
        m_workerThread->messageQueue().append(m_queuedEarlyTasks[i]);
    m_queuedEarlyTasks.clear();
}

void WorkerMessagingProxy::workerObjectDestroyed()
{
    m_workerObject = 0;
    if (m_workerThread)
        m_workerThread->stop();
    else
        workerContextDestroyedInternal(); // It never existed, just do our cleanup.
}

void WorkerMessagingProxy::workerContextDestroyed()
{
    m_scriptExecutionContext->postTask(WorkerContextDestroyedTask::create(this));
    // Will execute workerContextDestroyedInternal() on context's thread.
}

void WorkerMessagingProxy::workerContextDestroyedInternal()
{
    ASSERT(!m_workerObject);
    delete this;
}

void WorkerMessagingProxy::confirmWorkerThreadMessage(bool hasPendingActivity)
{
    m_scriptExecutionContext->postTask(WorkerThreadActivityReportTask::create(this, true, hasPendingActivity));
    // Will execute reportWorkerThreadActivityInternal() on context's thread.
}

void WorkerMessagingProxy::reportWorkerThreadActivity(bool hasPendingActivity)
{
    m_scriptExecutionContext->postTask(WorkerThreadActivityReportTask::create(this, false, hasPendingActivity));
    // Will execute reportWorkerThreadActivityInternal() on context's thread.
}

void WorkerMessagingProxy::reportWorkerThreadActivityInternal(bool confirmingMessage, bool hasPendingActivity)
{
    if (confirmingMessage) {
        ASSERT(m_unconfirmedMessageCount);
        --m_unconfirmedMessageCount;
    }

    m_workerThreadHadPendingActivity = hasPendingActivity;
}

bool WorkerMessagingProxy::workerThreadHasPendingActivity() const
{
    return m_unconfirmedMessageCount || m_workerThreadHadPendingActivity;
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
