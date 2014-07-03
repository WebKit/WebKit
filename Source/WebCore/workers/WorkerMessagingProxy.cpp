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
 *
 */

#include "config.h"
#include "WorkerMessagingProxy.h"

#include "ContentSecurityPolicy.h"
#include "DOMWindow.h"
#include "DedicatedWorkerGlobalScope.h"
#include "DedicatedWorkerThread.h"
#include "Document.h"
#include "ErrorEvent.h"
#include "Event.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "InspectorInstrumentation.h"
#include "MessageEvent.h"
#include "PageGroup.h"
#include "ScriptExecutionContext.h"
#include "Worker.h"
#include "WorkerDebuggerAgent.h"
#include "WorkerInspectorController.h"
#include <inspector/InspectorAgentBase.h>
#include <inspector/ScriptCallStack.h>
#include <runtime/ConsoleTypes.h>
#include <wtf/MainThread.h>

namespace WebCore {

WorkerGlobalScopeProxy* WorkerGlobalScopeProxy::create(Worker* worker)
{
    return new WorkerMessagingProxy(worker);
}

WorkerMessagingProxy::WorkerMessagingProxy(Worker* workerObject)
    : m_scriptExecutionContext(workerObject->scriptExecutionContext())
    , m_workerObject(workerObject)
    , m_mayBeDestroyed(false)
    , m_unconfirmedMessageCount(0)
    , m_workerThreadHadPendingActivity(false)
    , m_askedToTerminate(false)
#if ENABLE(INSPECTOR)
    , m_pageInspector(0)
#endif
{
    ASSERT(m_workerObject);
    ASSERT((m_scriptExecutionContext->isDocument() && isMainThread())
           || (m_scriptExecutionContext->isWorkerGlobalScope() && currentThread() == toWorkerGlobalScope(*m_scriptExecutionContext).thread().threadID()));
}

WorkerMessagingProxy::~WorkerMessagingProxy()
{
    ASSERT(!m_workerObject);
    ASSERT((m_scriptExecutionContext->isDocument() && isMainThread())
           || (m_scriptExecutionContext->isWorkerGlobalScope() && currentThread() == toWorkerGlobalScope(*m_scriptExecutionContext).thread().threadID()));
}

void WorkerMessagingProxy::startWorkerGlobalScope(const URL& scriptURL, const String& userAgent, const String& sourceCode, WorkerThreadStartMode startMode)
{
    // FIXME: This need to be revisited when we support nested worker one day
    ASSERT_WITH_SECURITY_IMPLICATION(m_scriptExecutionContext->isDocument());
    Document* document = static_cast<Document*>(m_scriptExecutionContext.get());
    GroupSettings* settings = 0;
    if (document->page())
        settings = &document->page()->group().groupSettings();
    RefPtr<DedicatedWorkerThread> thread = DedicatedWorkerThread::create(scriptURL, userAgent, settings, sourceCode, *this, *this, startMode, document->contentSecurityPolicy()->deprecatedHeader(), document->contentSecurityPolicy()->deprecatedHeaderType(), document->topOrigin());
    workerThreadCreated(thread);
    thread->start();
    InspectorInstrumentation::didStartWorkerGlobalScope(m_scriptExecutionContext.get(), this, scriptURL);
}

void WorkerMessagingProxy::postMessageToWorkerObject(PassRefPtr<SerializedScriptValue> message, std::unique_ptr<MessagePortChannelArray> channels)
{
    MessagePortChannelArray* channelsPtr = channels.release();
    m_scriptExecutionContext->postTask([=] (ScriptExecutionContext& context) {
        Worker* workerObject = this->workerObject();
        if (!workerObject || askedToTerminate())
            return;

        std::unique_ptr<MessagePortArray> ports = MessagePort::entanglePorts(context, std::unique_ptr<MessagePortChannelArray>(channelsPtr));
        workerObject->dispatchEvent(MessageEvent::create(WTF::move(ports), message));
    });
}

void WorkerMessagingProxy::postMessageToWorkerGlobalScope(PassRefPtr<SerializedScriptValue> message, std::unique_ptr<MessagePortChannelArray> channels)
{
    if (m_askedToTerminate)
        return;

    MessagePortChannelArray* channelsPtr = channels.release();
    ScriptExecutionContext::Task task([=] (ScriptExecutionContext& scriptContext) {
        ASSERT_WITH_SECURITY_IMPLICATION(scriptContext.isWorkerGlobalScope());
        DedicatedWorkerGlobalScope& context = static_cast<DedicatedWorkerGlobalScope&>(scriptContext);
        std::unique_ptr<MessagePortArray> ports = MessagePort::entanglePorts(scriptContext, std::unique_ptr<MessagePortChannelArray>(channelsPtr));
        context.dispatchEvent(MessageEvent::create(WTF::move(ports), message));
        context.thread().workerObjectProxy().confirmMessageFromWorkerObject(context.hasPendingActivity());
    });

    if (m_workerThread) {
        ++m_unconfirmedMessageCount;
        m_workerThread->runLoop().postTask(WTF::move(task));
    } else
        m_queuedEarlyTasks.append(std::make_unique<ScriptExecutionContext::Task>(WTF::move(task)));
}

void WorkerMessagingProxy::postTaskToLoader(ScriptExecutionContext::Task task)
{
    // FIXME: In case of nested workers, this should go directly to the root Document context.
    ASSERT(m_scriptExecutionContext->isDocument());
    m_scriptExecutionContext->postTask(WTF::move(task));
}

bool WorkerMessagingProxy::postTaskForModeToWorkerGlobalScope(ScriptExecutionContext::Task task, const String& mode)
{
    if (m_askedToTerminate)
        return false;

    ASSERT(m_workerThread);
    m_workerThread->runLoop().postTaskForMode(WTF::move(task), mode);
    return true;
}

void WorkerMessagingProxy::postExceptionToWorkerObject(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL)
{
    String errorMessageCopy = errorMessage.isolatedCopy();
    String sourceURLCopy = sourceURL.isolatedCopy();
    m_scriptExecutionContext->postTask([=] (ScriptExecutionContext& context) {
        Worker* workerObject = this->workerObject();
        if (!workerObject)
            return;

        // We don't bother checking the askedToTerminate() flag here, because exceptions should *always* be reported even if the thread is terminated.
        // This is intentionally different than the behavior in MessageWorkerTask, because terminated workers no longer deliver messages (section 4.6 of the WebWorker spec), but they do report exceptions.

        bool errorHandled = !workerObject->dispatchEvent(ErrorEvent::create(errorMessageCopy, sourceURLCopy, lineNumber, columnNumber));
        if (!errorHandled)
            context.reportException(errorMessageCopy, lineNumber, columnNumber, sourceURLCopy, 0);
    });
}

void WorkerMessagingProxy::postConsoleMessageToWorkerObject(MessageSource source, MessageLevel level, const String& message, int lineNumber, int columnNumber, const String& sourceURL)
{
    String messageCopy = message.isolatedCopy();
    String sourceURLCopy = sourceURL.isolatedCopy();
    m_scriptExecutionContext->postTask([=] (ScriptExecutionContext& context) {
        if (askedToTerminate())
            return;
        context.addConsoleMessage(source, level, messageCopy, sourceURLCopy, lineNumber, columnNumber);
    });
}

void WorkerMessagingProxy::workerThreadCreated(PassRefPtr<DedicatedWorkerThread> workerThread)
{
    m_workerThread = workerThread;

    if (m_askedToTerminate) {
        // Worker.terminate() could be called from JS before the thread was created.
        m_workerThread->stop();
    } else {
        ASSERT(!m_unconfirmedMessageCount);
        m_unconfirmedMessageCount = m_queuedEarlyTasks.size();
        m_workerThreadHadPendingActivity = true; // Worker initialization means a pending activity.

        auto queuedEarlyTasks = WTF::move(m_queuedEarlyTasks);
        for (auto& task : queuedEarlyTasks)
            m_workerThread->runLoop().postTask(WTF::move(*task));
    }
}

void WorkerMessagingProxy::workerObjectDestroyed()
{
    m_workerObject = 0;
    m_scriptExecutionContext->postTask([this] (ScriptExecutionContext&) {
        m_mayBeDestroyed = true;
        if (m_workerThread)
            terminateWorkerGlobalScope();
        else
            workerGlobalScopeDestroyedInternal();
    });
}

void WorkerMessagingProxy::notifyNetworkStateChange(bool isOnline)
{
    if (m_askedToTerminate)
        return;

    if (!m_workerThread)
        return;

    m_workerThread->runLoop().postTask([=] (ScriptExecutionContext& context) {
        toWorkerGlobalScope(&context)->dispatchEvent(Event::create(isOnline ? eventNames().onlineEvent : eventNames().offlineEvent, false, false));
    });
}

#if ENABLE(INSPECTOR)
void WorkerMessagingProxy::connectToInspector(WorkerGlobalScopeProxy::PageInspector* pageInspector)
{
    if (m_askedToTerminate)
        return;
    ASSERT(!m_pageInspector);
    m_pageInspector = pageInspector;
    m_workerThread->runLoop().postTaskForMode([] (ScriptExecutionContext& context) {
        toWorkerGlobalScope(&context)->workerInspectorController().connectFrontend();
    }, WorkerDebuggerAgent::debuggerTaskMode);
}

void WorkerMessagingProxy::disconnectFromInspector()
{
    m_pageInspector = 0;
    if (m_askedToTerminate)
        return;
    m_workerThread->runLoop().postTaskForMode([] (ScriptExecutionContext& context) {
        toWorkerGlobalScope(&context)->workerInspectorController().disconnectFrontend(Inspector::InspectorDisconnectReason::InspectorDestroyed);
    }, WorkerDebuggerAgent::debuggerTaskMode);
}

void WorkerMessagingProxy::sendMessageToInspector(const String& message)
{
    if (m_askedToTerminate)
        return;
    String messageCopy = message.isolatedCopy();
    m_workerThread->runLoop().postTaskForMode([messageCopy] (ScriptExecutionContext& context) {
        toWorkerGlobalScope(&context)->workerInspectorController().dispatchMessageFromFrontend(messageCopy);
    }, WorkerDebuggerAgent::debuggerTaskMode);
    WorkerDebuggerAgent::interruptAndDispatchInspectorCommands(m_workerThread.get());
}
#endif

void WorkerMessagingProxy::workerGlobalScopeDestroyed()
{
    m_scriptExecutionContext->postTask([this] (ScriptExecutionContext&) {
        workerGlobalScopeDestroyedInternal();
    });
    // Will execute workerGlobalScopeDestroyedInternal() on context's thread.
}

void WorkerMessagingProxy::workerGlobalScopeClosed()
{
    // Executes terminateWorkerGlobalScope() on parent context's thread.
    m_scriptExecutionContext->postTask([this] (ScriptExecutionContext&) {
        terminateWorkerGlobalScope();
    });
}

void WorkerMessagingProxy::workerGlobalScopeDestroyedInternal()
{
    // WorkerGlobalScopeDestroyedTask is always the last to be performed, so the proxy is not needed for communication
    // in either side any more. However, the Worker object may still exist, and it assumes that the proxy exists, too.
    m_askedToTerminate = true;
    m_workerThread = 0;

    InspectorInstrumentation::workerGlobalScopeTerminated(m_scriptExecutionContext.get(), this);

    if (m_mayBeDestroyed)
        delete this;
}

void WorkerMessagingProxy::terminateWorkerGlobalScope()
{
    if (m_askedToTerminate)
        return;
    m_askedToTerminate = true;

    if (m_workerThread)
        m_workerThread->stop();

    InspectorInstrumentation::workerGlobalScopeTerminated(m_scriptExecutionContext.get(), this);
}

#if ENABLE(INSPECTOR)
void WorkerMessagingProxy::postMessageToPageInspector(const String& message)
{
    String messageCopy = message.isolatedCopy();
    m_scriptExecutionContext->postTask([=] (ScriptExecutionContext&) {
        m_pageInspector->dispatchMessageFromWorker(messageCopy);
    });
}
#endif

void WorkerMessagingProxy::confirmMessageFromWorkerObject(bool hasPendingActivity)
{
    m_scriptExecutionContext->postTask([=] (ScriptExecutionContext&) {
        reportPendingActivityInternal(true, hasPendingActivity);
    });
    // Will execute reportPendingActivityInternal() on context's thread.
}

void WorkerMessagingProxy::reportPendingActivity(bool hasPendingActivity)
{
    m_scriptExecutionContext->postTask([=] (ScriptExecutionContext&) {
        reportPendingActivityInternal(false, hasPendingActivity);
    });
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
