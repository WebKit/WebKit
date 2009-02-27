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

#include "WorkerMessagingProxy.h"

#include "DOMWindow.h"
#include "Document.h"
#include "GenericWorkerTask.h"
#include "MessageEvent.h"
#include "ScriptExecutionContext.h"
#include "Worker.h"
#include "WorkerContext.h"
#include "WorkerThread.h"

namespace WebCore {

class MessageWorkerContextTask : public ScriptExecutionContext::Task {
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

    virtual void performTask(ScriptExecutionContext* scriptContext)
    {
        ASSERT(scriptContext->isWorkerContext());
        WorkerContext* context = static_cast<WorkerContext*>(scriptContext);

        context->dispatchMessage(m_message);

        context->thread()->workerObjectProxy()->confirmMessageFromWorkerObject(context->hasPendingActivity());
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
        if (!workerObject || m_messagingProxy->askedToTerminate())
            return;

        workerObject->dispatchMessage(m_message);
    }

private:
    String m_message;
    WorkerMessagingProxy* m_messagingProxy;
};

class WorkerExceptionTask : public ScriptExecutionContext::Task {
public:
    static PassRefPtr<WorkerExceptionTask> create(const String& errorMessage, int lineNumber, const String& sourceURL, WorkerMessagingProxy* messagingProxy)
    {
        return adoptRef(new WorkerExceptionTask(errorMessage, lineNumber, sourceURL, messagingProxy));
    }

private:
    WorkerExceptionTask(const String& errorMessage, int lineNumber, const String& sourceURL, WorkerMessagingProxy* messagingProxy)
        : m_errorMessage(errorMessage.copy())
        , m_lineNumber(lineNumber)
        , m_sourceURL(sourceURL.copy())
        , m_messagingProxy(messagingProxy)
    {
    }

    virtual void performTask(ScriptExecutionContext* context)
    {
        if (!m_messagingProxy->askedToTerminate())
            context->reportException(m_errorMessage, m_lineNumber, m_sourceURL);
    }

    String m_errorMessage;
    int m_lineNumber;
    String m_sourceURL;
    WorkerMessagingProxy* m_messagingProxy;
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

    virtual void performTask(ScriptExecutionContext*)
    {
        m_messagingProxy->workerContextDestroyedInternal();
    }

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

    virtual void performTask(ScriptExecutionContext*)
    {
        m_messagingProxy->reportPendingActivityInternal(m_confirmingMessage, m_hasPendingActivity);
    }

    WorkerMessagingProxy* m_messagingProxy;
    bool m_confirmingMessage;
    bool m_hasPendingActivity;
};


#if !PLATFORM(CHROMIUM)
WorkerContextProxy* WorkerContextProxy::create(Worker* worker)
{
    return new WorkerMessagingProxy(worker);
}
#endif

WorkerMessagingProxy::WorkerMessagingProxy(Worker* workerObject)
    : m_scriptExecutionContext(workerObject->scriptExecutionContext())
    , m_workerObject(workerObject)
    , m_unconfirmedMessageCount(0)
    , m_workerThreadHadPendingActivity(false)
    , m_askedToTerminate(false)
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

void WorkerMessagingProxy::startWorkerContext(const KURL& scriptURL, const String& userAgent, const String& sourceCode)
{
    RefPtr<WorkerThread> thread = WorkerThread::create(scriptURL, userAgent, sourceCode, this);
    workerThreadCreated(thread);
    thread->start();
}

void WorkerMessagingProxy::postMessageToWorkerObject(const String& message)
{
    m_scriptExecutionContext->postTask(MessageWorkerTask::create(message, this));
}

void WorkerMessagingProxy::postMessageToWorkerContext(const String& message)
{
    if (m_askedToTerminate)
        return;

    if (m_workerThread) {
        ++m_unconfirmedMessageCount;
        m_workerThread->runLoop().postTask(MessageWorkerContextTask::create(message));
    } else
        m_queuedEarlyTasks.append(MessageWorkerContextTask::create(message));
}

void WorkerMessagingProxy::postTaskToWorkerContext(PassRefPtr<ScriptExecutionContext::Task> task)
{
    postTaskForModeToWorkerContext(task, WorkerRunLoop::defaultMode());
}

void WorkerMessagingProxy::postTaskForModeToWorkerContext(PassRefPtr<ScriptExecutionContext::Task> task, const String& mode)
{
    if (m_askedToTerminate)
        return;

    ASSERT(m_workerThread);
    m_workerThread->runLoop().postTaskForMode(task, mode);
}

void WorkerMessagingProxy::postTaskToWorkerObject(PassRefPtr<ScriptExecutionContext::Task> task)
{
    m_scriptExecutionContext->postTask(task);
}

void WorkerMessagingProxy::postExceptionToWorkerObject(const String& errorMessage, int lineNumber, const String& sourceURL)
{
    m_scriptExecutionContext->postTask(WorkerExceptionTask::create(errorMessage, lineNumber, sourceURL, this));
}
    
static void postConsoleMessageTask(ScriptExecutionContext* context, WorkerMessagingProxy* messagingProxy, MessageDestination destination, MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceURL)
{
    if (messagingProxy->askedToTerminate())
        return;
    context->addMessage(destination, source, level, message, lineNumber, sourceURL);
}

void WorkerMessagingProxy::postConsoleMessageToWorkerObject(MessageDestination destination, MessageSource source, MessageLevel level, const String& message, int lineNumber, const String& sourceURL)
{
    m_scriptExecutionContext->postTask(createCallbackTask(&postConsoleMessageTask, this, destination, source, level, message, lineNumber, sourceURL));
}

void WorkerMessagingProxy::workerThreadCreated(PassRefPtr<WorkerThread> workerThread)
{
    m_workerThread = workerThread;

    if (m_askedToTerminate) {
        // Worker.terminate() could be called from JS before the thread was created.
        m_workerThread->stop();
    } else {
        unsigned taskCount = m_queuedEarlyTasks.size();
        ASSERT(!m_unconfirmedMessageCount);
        m_unconfirmedMessageCount = taskCount;
        m_workerThreadHadPendingActivity = true; // Worker initialization means a pending activity.

        for (unsigned i = 0; i < taskCount; ++i)
            m_workerThread->runLoop().postTask(m_queuedEarlyTasks[i]);
        m_queuedEarlyTasks.clear();
    }
}

void WorkerMessagingProxy::workerObjectDestroyed()
{
    m_workerObject = 0;
    if (m_workerThread)
        terminateWorkerContext();
    else
        workerContextDestroyedInternal();
}

void WorkerMessagingProxy::workerContextDestroyed()
{
    m_scriptExecutionContext->postTask(WorkerContextDestroyedTask::create(this));
    // Will execute workerContextDestroyedInternal() on context's thread.
}

void WorkerMessagingProxy::workerContextDestroyedInternal()
{
    // WorkerContextDestroyedTask is always the last to be performed, so the proxy is not needed for communication
    // in either side any more. However, the Worker object may still exist, and it assumes that the proxy exists, too.
    m_workerThread = 0;
    if (!m_workerObject)
        delete this;
}

void WorkerMessagingProxy::terminateWorkerContext()
{
    if (m_askedToTerminate)
        return;
    m_askedToTerminate = true;

    if (m_workerThread)
        m_workerThread->stop();
}

void WorkerMessagingProxy::confirmMessageFromWorkerObject(bool hasPendingActivity)
{
    m_scriptExecutionContext->postTask(WorkerThreadActivityReportTask::create(this, true, hasPendingActivity));
    // Will execute reportPendingActivityInternal() on context's thread.
}

void WorkerMessagingProxy::reportPendingActivity(bool hasPendingActivity)
{
    m_scriptExecutionContext->postTask(WorkerThreadActivityReportTask::create(this, false, hasPendingActivity));
    // Will execute reportPendingActivityInternal() on context's thread.
}

void WorkerMessagingProxy::reportPendingActivityInternal(bool confirmingMessage, bool hasPendingActivity)
{
    if (confirmingMessage && !m_askedToTerminate) {
        ASSERT(m_unconfirmedMessageCount);
        --m_unconfirmedMessageCount;
    }

    m_workerThreadHadPendingActivity = hasPendingActivity;
}

bool WorkerMessagingProxy::hasPendingActivity() const
{
    return (m_unconfirmedMessageCount || m_workerThreadHadPendingActivity) && !m_askedToTerminate;
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
