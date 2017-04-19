/*
 * Copyright (C) 2008-2017 Apple Inc. All Rights Reserved.
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
#include "EventNames.h"
#include "MessageEvent.h"
#include "ScriptExecutionContext.h"
#include "Worker.h"
#include "WorkerInspectorProxy.h"
#include <inspector/ScriptCallStack.h>
#include <runtime/ConsoleTypes.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>

namespace WebCore {

WorkerGlobalScopeProxy& WorkerGlobalScopeProxy::create(Worker& worker)
{
    return *new WorkerMessagingProxy(worker);
}

WorkerMessagingProxy::WorkerMessagingProxy(Worker& workerObject)
    : m_scriptExecutionContext(workerObject.scriptExecutionContext())
    , m_inspectorProxy(std::make_unique<WorkerInspectorProxy>(workerObject.identifier()))
    , m_workerObject(&workerObject)
{
    ASSERT((is<Document>(*m_scriptExecutionContext) && isMainThread())
        || (is<WorkerGlobalScope>(*m_scriptExecutionContext) && currentThread() == downcast<WorkerGlobalScope>(*m_scriptExecutionContext).thread().threadID()));

    // Nobody outside this class ref counts this object. The original ref
    // is balanced by the deref in workerGlobalScopeDestroyedInternal.
}

WorkerMessagingProxy::~WorkerMessagingProxy()
{
    ASSERT(!m_workerObject);
    ASSERT((is<Document>(*m_scriptExecutionContext) && isMainThread())
        || (is<WorkerGlobalScope>(*m_scriptExecutionContext) && currentThread() == downcast<WorkerGlobalScope>(*m_scriptExecutionContext).thread().threadID()));
}

void WorkerMessagingProxy::startWorkerGlobalScope(const URL& scriptURL, const String& userAgent, const String& sourceCode, const ContentSecurityPolicyResponseHeaders& contentSecurityPolicyResponseHeaders, bool shouldBypassMainWorldContentSecurityPolicy, MonotonicTime timeOrigin, JSC::RuntimeFlags runtimeFlags)
{
    // FIXME: This need to be revisited when we support nested worker one day
    ASSERT(m_scriptExecutionContext);
    Document& document = downcast<Document>(*m_scriptExecutionContext);
    WorkerThreadStartMode startMode = m_inspectorProxy->workerStartMode(*m_scriptExecutionContext.get());
    String identifier = m_inspectorProxy->identifier();

#if ENABLE(INDEXED_DATABASE)
    IDBClient::IDBConnectionProxy* proxy = document.idbConnectionProxy();
#else
    IDBClient::IDBConnectionProxy* proxy = nullptr;
#endif

#if ENABLE(WEB_SOCKETS)
    SocketProvider* socketProvider = document.socketProvider();
#else
    SocketProvider* socketProvider = nullptr;
#endif

    auto thread = DedicatedWorkerThread::create(scriptURL, identifier, userAgent, sourceCode, *this, *this, startMode, contentSecurityPolicyResponseHeaders, shouldBypassMainWorldContentSecurityPolicy, document.topOrigin(), timeOrigin, proxy, socketProvider, runtimeFlags);

    workerThreadCreated(thread.get());
    thread->start();

    m_inspectorProxy->workerStarted(m_scriptExecutionContext.get(), thread.ptr(), scriptURL);
}

void WorkerMessagingProxy::postMessageToWorkerObject(RefPtr<SerializedScriptValue>&& message, std::unique_ptr<MessagePortChannelArray> channels)
{
    m_scriptExecutionContext->postTask([this, channels = WTFMove(channels), message = WTFMove(message)] (ScriptExecutionContext& context) mutable {
        Worker* workerObject = this->workerObject();
        if (!workerObject || askedToTerminate())
            return;

        auto ports = MessagePort::entanglePorts(context, WTFMove(channels));
        workerObject->dispatchEvent(MessageEvent::create(WTFMove(ports), WTFMove(message)));
    });
}

void WorkerMessagingProxy::postMessageToWorkerGlobalScope(RefPtr<SerializedScriptValue>&& message, std::unique_ptr<MessagePortChannelArray> channels)
{
    if (m_askedToTerminate)
        return;

    ScriptExecutionContext::Task task([channels = WTFMove(channels), message = WTFMove(message)] (ScriptExecutionContext& scriptContext) mutable {
        ASSERT_WITH_SECURITY_IMPLICATION(scriptContext.isWorkerGlobalScope());
        auto& context = static_cast<DedicatedWorkerGlobalScope&>(scriptContext);
        auto ports = MessagePort::entanglePorts(scriptContext, WTFMove(channels));
        context.dispatchEvent(MessageEvent::create(WTFMove(ports), WTFMove(message)));
        context.thread().workerObjectProxy().confirmMessageFromWorkerObject(context.hasPendingActivity());
    });

    if (m_workerThread) {
        ++m_unconfirmedMessageCount;
        m_workerThread->runLoop().postTask(WTFMove(task));
    } else
        m_queuedEarlyTasks.append(std::make_unique<ScriptExecutionContext::Task>(WTFMove(task)));
}

void WorkerMessagingProxy::postTaskToLoader(ScriptExecutionContext::Task&& task)
{
    // FIXME: In case of nested workers, this should go directly to the root Document context.
    ASSERT(m_scriptExecutionContext->isDocument());
    m_scriptExecutionContext->postTask(WTFMove(task));
}

bool WorkerMessagingProxy::postTaskForModeToWorkerGlobalScope(ScriptExecutionContext::Task&& task, const String& mode)
{
    if (m_askedToTerminate)
        return false;

    ASSERT(m_workerThread);
    m_workerThread->runLoop().postTaskForMode(WTFMove(task), mode);
    return true;
}

void WorkerMessagingProxy::postExceptionToWorkerObject(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL)
{
    m_scriptExecutionContext->postTask([this, errorMessage = errorMessage.isolatedCopy(), sourceURL = sourceURL.isolatedCopy(), lineNumber, columnNumber] (ScriptExecutionContext& context) {
        Worker* workerObject = this->workerObject();
        if (!workerObject)
            return;

        // We don't bother checking the askedToTerminate() flag here, because exceptions should *always* be reported even if the thread is terminated.
        // This is intentionally different than the behavior in MessageWorkerTask, because terminated workers no longer deliver messages (section 4.6 of the WebWorker spec), but they do report exceptions.

        bool errorHandled = !workerObject->dispatchEvent(ErrorEvent::create(errorMessage, sourceURL, lineNumber, columnNumber, { }));
        if (!errorHandled)
            context.reportException(errorMessage, lineNumber, columnNumber, sourceURL, nullptr, nullptr);
    });
}

void WorkerMessagingProxy::postMessageToPageInspector(const String& message)
{
    RunLoop::main().dispatch([this, protectedThis = makeRef(*this), message = message.isolatedCopy()] {
        if (!m_mayBeDestroyed)
            m_inspectorProxy->sendMessageFromWorkerToFrontend(message);
    });
}

void WorkerMessagingProxy::workerThreadCreated(DedicatedWorkerThread& workerThread)
{
    m_workerThread = &workerThread;

    if (m_askedToTerminate) {
        // Worker.terminate() could be called from JS before the thread was created.
        m_workerThread->stop();
    } else {
        ASSERT(!m_unconfirmedMessageCount);
        m_unconfirmedMessageCount = m_queuedEarlyTasks.size();
        m_workerThreadHadPendingActivity = true; // Worker initialization means a pending activity.

        auto queuedEarlyTasks = WTFMove(m_queuedEarlyTasks);
        for (auto& task : queuedEarlyTasks)
            m_workerThread->runLoop().postTask(WTFMove(*task));
    }
}

void WorkerMessagingProxy::workerObjectDestroyed()
{
    m_workerObject = nullptr;
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

    m_workerThread->runLoop().postTask([isOnline] (ScriptExecutionContext& context) {
        downcast<WorkerGlobalScope>(context).dispatchEvent(Event::create(isOnline ? eventNames().onlineEvent : eventNames().offlineEvent, false, false));
    });
}

void WorkerMessagingProxy::workerGlobalScopeDestroyed()
{
    m_scriptExecutionContext->postTask([this] (ScriptExecutionContext&) {
        workerGlobalScopeDestroyedInternal();
    });
}

void WorkerMessagingProxy::workerGlobalScopeClosed()
{
    m_scriptExecutionContext->postTask([this] (ScriptExecutionContext&) {
        terminateWorkerGlobalScope();
    });
}

void WorkerMessagingProxy::workerGlobalScopeDestroyedInternal()
{
    // This is always the last task to be performed, so the proxy is not needed for communication
    // in either side any more. However, the Worker object may still exist, and it assumes that the proxy exists, too.
    m_askedToTerminate = true;
    m_workerThread = nullptr;

    m_inspectorProxy->workerTerminated();

    // This balances the original ref in construction.
    if (m_mayBeDestroyed)
        deref();
}

void WorkerMessagingProxy::terminateWorkerGlobalScope()
{
    if (m_askedToTerminate)
        return;
    m_askedToTerminate = true;

    m_inspectorProxy->workerTerminated();

    if (m_workerThread)
        m_workerThread->stop();
}

void WorkerMessagingProxy::confirmMessageFromWorkerObject(bool hasPendingActivity)
{
    m_scriptExecutionContext->postTask([this, hasPendingActivity] (ScriptExecutionContext&) {
        reportPendingActivityInternal(true, hasPendingActivity);
    });
}

void WorkerMessagingProxy::reportPendingActivity(bool hasPendingActivity)
{
    m_scriptExecutionContext->postTask([this, hasPendingActivity] (ScriptExecutionContext&) {
        reportPendingActivityInternal(false, hasPendingActivity);
    });
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
