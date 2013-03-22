/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WorkerStorageQuotaCallbacksBridge.h"

#include "CrossThreadTask.h"
#include "WebCommonWorkerClient.h"
#include "WebStorageQuotaCallbacks.h"
#include "WebStorageQuotaCallbacksImpl.h"
#include "WebWorkerBase.h"
#include "WorkerContext.h"
#include "WorkerLoaderProxy.h"

using namespace WebCore;

namespace WebKit {

// FIXME: Replace WebFrame parameter in queryStorageUsageAndQuota() with WebString and move the method to Platform so that we can remove all these complexity for Worker.
class MainThreadStorageQuotaCallbacks : public WebStorageQuotaCallbacks {
public:
    // Callbacks are self-destructed and we always return leaked pointer here.
    static MainThreadStorageQuotaCallbacks* createLeakedPtr(PassRefPtr<WorkerStorageQuotaCallbacksBridge> bridge, const String& mode)
    {
        OwnPtr<MainThreadStorageQuotaCallbacks> callbacks = adoptPtr(new MainThreadStorageQuotaCallbacks(bridge, mode));
        return callbacks.leakPtr();
    }

    virtual ~MainThreadStorageQuotaCallbacks()
    {
    }

    virtual void didQueryStorageUsageAndQuota(unsigned long long usageInBytes, unsigned long long quotaInBytes)
    {
        m_bridge->didQueryStorageUsageAndQuotaOnMainThread(usageInBytes, quotaInBytes, m_mode);
        delete this;
    }

    virtual void didFail(WebStorageQuotaError error)
    {
        m_bridge->didFailOnMainThread(error, m_mode);
        delete this;
    }

    virtual void didGrantStorageQuota(unsigned long long grantedQuotaInBytes)
    {
        ASSERT_NOT_REACHED();
    }

private:
    MainThreadStorageQuotaCallbacks(PassRefPtr<WorkerStorageQuotaCallbacksBridge> bridge, const String& mode)
        : m_bridge(bridge)
        , m_mode(mode)
    {
        ASSERT(m_bridge);
    }

    RefPtr<WorkerStorageQuotaCallbacksBridge> m_bridge;
    const String m_mode;
};

// FIXME: Replace WebFrame parameter in queryStorageUsageAndQuota() with WebString and move the method to Platform so that we can remove all these complexity for Worker."
// Observes the worker context. By keeping this separate, it is easier to verify
// that it only gets deleted on the worker context thread which is verified by ~Observer.
class WorkerStorageQuotaContextObserver : public WebCore::WorkerContext::Observer {
public:
    static PassOwnPtr<WorkerStorageQuotaContextObserver> create(WorkerContext* context, PassRefPtr<WorkerStorageQuotaCallbacksBridge> bridge)
    {
        return adoptPtr(new WorkerStorageQuotaContextObserver(context, bridge));
    }

    // WorkerContext::Observer method.
    virtual void notifyStop()
    {
        m_bridge->stop();
    }

private:
    WorkerStorageQuotaContextObserver(WorkerContext* context, PassRefPtr<WorkerStorageQuotaCallbacksBridge> bridge)
        : WebCore::WorkerContext::Observer(context)
        , m_bridge(bridge)
    {
    }

    RefPtr<WorkerStorageQuotaCallbacksBridge> m_bridge;
};

void WorkerStorageQuotaCallbacksBridge::stop()
{
    ASSERT(m_workerContext->isContextThread());
    {
        MutexLocker locker(m_loaderProxyMutex);
        m_workerLoaderProxy = 0;
    }

    if (m_callbacksOnWorkerThread)
        m_callbacksOnWorkerThread->didFail(WebStorageQuotaErrorAbort);

    cleanUpAfterCallback();
}

void WorkerStorageQuotaCallbacksBridge::cleanUpAfterCallback()
{
    ASSERT(m_workerContext->isContextThread());

    m_callbacksOnWorkerThread = 0;
    if (m_workerContextObserver) {
        WorkerStorageQuotaContextObserver* observer = m_workerContextObserver;
        m_workerContextObserver = 0;
        // The next line may delete this.
        delete observer;
    }
}

WorkerStorageQuotaCallbacksBridge::WorkerStorageQuotaCallbacksBridge(WebCore::WorkerLoaderProxy* workerLoaderProxy, WebCore::ScriptExecutionContext* workerContext, WebStorageQuotaCallbacksImpl* callbacks)
    : m_workerLoaderProxy(workerLoaderProxy)
    , m_workerContext(workerContext)
    , m_workerContextObserver(WorkerStorageQuotaContextObserver::create(static_cast<WorkerContext*>(m_workerContext), this).leakPtr())
    , m_callbacksOnWorkerThread(callbacks)
{
    ASSERT(m_workerContext->isContextThread());
}

WorkerStorageQuotaCallbacksBridge::~WorkerStorageQuotaCallbacksBridge()
{
    // One way or another, the bridge should be stopped before it is destroyed.
    ASSERT(!m_callbacksOnWorkerThread);
}

void WorkerStorageQuotaCallbacksBridge::postQueryUsageAndQuotaToMainThread(WebCommonWorkerClient* commonClient, WebStorageQuotaType storageType, const String& mode)
{
    dispatchTaskToMainThread(createCallbackTask(&queryUsageAndQuotaOnMainThread, AllowCrossThreadAccess(commonClient), storageType, this, mode));
}

void WorkerStorageQuotaCallbacksBridge::queryUsageAndQuotaOnMainThread(ScriptExecutionContext*, WebCommonWorkerClient* commonClient, WebStorageQuotaType storageType, PassRefPtr<WorkerStorageQuotaCallbacksBridge> bridge, const String& mode)
{
    if (!commonClient)
        bridge->didFailOnMainThread(WebStorageQuotaErrorAbort, mode);
    else
        commonClient->queryUsageAndQuota(storageType, MainThreadStorageQuotaCallbacks::createLeakedPtr(bridge, mode));
}

void WorkerStorageQuotaCallbacksBridge::didFailOnMainThread(WebStorageQuotaError error, const String& mode)
{
    mayPostTaskToWorker(createCallbackTask(&didFailOnWorkerThread, this, error), mode);
}

void WorkerStorageQuotaCallbacksBridge::didQueryStorageUsageAndQuotaOnMainThread(unsigned long long usageInBytes, unsigned long long quotaInBytes, const String& mode)
{
    mayPostTaskToWorker(createCallbackTask(&didQueryStorageUsageAndQuotaOnWorkerThread, this, usageInBytes, quotaInBytes), mode);
}

void WorkerStorageQuotaCallbacksBridge::didFailOnWorkerThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerStorageQuotaCallbacksBridge> bridge, WebStorageQuotaError error)
{
    bridge->m_callbacksOnWorkerThread->didFail(error);
}

void WorkerStorageQuotaCallbacksBridge::didQueryStorageUsageAndQuotaOnWorkerThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerStorageQuotaCallbacksBridge> bridge, unsigned long long usageInBytes, unsigned long long quotaInBytes)
{
    bridge->m_callbacksOnWorkerThread->didQueryStorageUsageAndQuota(usageInBytes, quotaInBytes);
}

void WorkerStorageQuotaCallbacksBridge::runTaskOnMainThread(WebCore::ScriptExecutionContext* scriptExecutionContext, PassRefPtr<WorkerStorageQuotaCallbacksBridge> bridge, PassOwnPtr<WebCore::ScriptExecutionContext::Task> taskToRun)
{
    ASSERT(isMainThread());
    taskToRun->performTask(scriptExecutionContext);
}

void WorkerStorageQuotaCallbacksBridge::runTaskOnWorkerThread(WebCore::ScriptExecutionContext* scriptExecutionContext, PassRefPtr<WorkerStorageQuotaCallbacksBridge> bridge, PassOwnPtr<WebCore::ScriptExecutionContext::Task> taskToRun)
{
    ASSERT(bridge);
    if (!bridge->m_callbacksOnWorkerThread)
        return;
    ASSERT(bridge->m_workerContext);
    ASSERT(bridge->m_workerContext->isContextThread());
    ASSERT(taskToRun);
    taskToRun->performTask(scriptExecutionContext);

    // taskToRun does the callback.
    bridge->cleanUpAfterCallback();

    // WorkerStorageQuotaCallbacksBridge may be deleted here when bridge goes out of scope.
}

void WorkerStorageQuotaCallbacksBridge::dispatchTaskToMainThread(PassOwnPtr<WebCore::ScriptExecutionContext::Task> task)
{
    ASSERT(m_workerLoaderProxy);
    ASSERT(m_workerContext->isContextThread());
    WebWorkerBase::dispatchTaskToMainThread(createCallbackTask(&runTaskOnMainThread, RefPtr<WorkerStorageQuotaCallbacksBridge>(this).release(), task));
}

void WorkerStorageQuotaCallbacksBridge::mayPostTaskToWorker(PassOwnPtr<ScriptExecutionContext::Task> task, const String& mode)
{
    // Relies on its caller (MainThreadStorageQuotaCallbacks:did*) to keep WorkerStorageQuotaCallbacksBridge alive.
    ASSERT(isMainThread());

    MutexLocker locker(m_loaderProxyMutex);
    if (m_workerLoaderProxy)
        m_workerLoaderProxy->postTaskForModeToWorkerContext(createCallbackTask(&runTaskOnWorkerThread, this, task), mode);
}

} // namespace WebCore
