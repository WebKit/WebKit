/*
 * Copyright (C) 2008-2017 Apple Inc. All Rights Reserved.
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
 */

#pragma once

#include "WorkerGlobalScopeProxy.h"
#include "WorkerDebuggerProxy.h"
#include "WorkerLoaderProxy.h"
#include "WorkerObjectProxy.h"
#include <wtf/MonotonicTime.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class DedicatedWorkerThread;
class WorkerInspectorProxy;
class WorkerUserGestureForwarder;

class WorkerMessagingProxy final : public ThreadSafeRefCounted<WorkerMessagingProxy>, public WorkerGlobalScopeProxy, public WorkerObjectProxy, public WorkerLoaderProxy, public WorkerDebuggerProxy {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit WorkerMessagingProxy(Worker&);
    virtual ~WorkerMessagingProxy();

private:
    // Implementations of WorkerGlobalScopeProxy.
    // (Only use these functions in the worker object thread.)
    void startWorkerGlobalScope(const URL& scriptURL, PAL::SessionID, const String& name, WorkerInitializationData&&, const ScriptBuffer& sourceCode, const ContentSecurityPolicyResponseHeaders&, bool shouldBypassMainWorldContentSecurityPolicy, const CrossOriginEmbedderPolicy&, MonotonicTime timeOrigin, ReferrerPolicy, WorkerType, FetchRequestCredentials, JSC::RuntimeFlags) final;
    void terminateWorkerGlobalScope() final;
    void postMessageToWorkerGlobalScope(MessageWithMessagePorts&&) final;
    void postTaskToWorkerGlobalScope(Function<void(ScriptExecutionContext&)>&&) final;
    void workerObjectDestroyed() final;
    void notifyNetworkStateChange(bool isOnline) final;
    void suspendForBackForwardCache() final;
    void resumeForBackForwardCache() final;

    // Implementation of WorkerObjectProxy.
    // (Only use these functions in the worker context thread.)
    void postMessageToWorkerObject(MessageWithMessagePorts&&) final;
    void postTaskToWorkerObject(Function<void(Worker&)>&&) final;
    void postExceptionToWorkerObject(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL) final;
    void workerGlobalScopeClosed() final;
    void workerGlobalScopeDestroyed() final;

    // Implementation of WorkerDebuggerProxy.
    // (Only use these functions in the worker context thread.)
    void postMessageToDebugger(const String&) final;
    void setResourceCachingDisabledByWebInspector(bool) final;

    // Implementation of WorkerLoaderProxy.
    // These functions are called on different threads to schedule loading
    // requests and to send callbacks back to WorkerGlobalScope.
    bool isWorkerMessagingProxy() const final { return true; }
    void postTaskToLoader(ScriptExecutionContext::Task&&) final;
    ScriptExecutionContextIdentifier loaderContextIdentifier() const final;
    RefPtr<CacheStorageConnection> createCacheStorageConnection() final;
    StorageConnection* storageConnection() final;
    RefPtr<RTCDataChannelRemoteHandlerConnection> createRTCDataChannelRemoteHandlerConnection() final;

    void workerThreadCreated(DedicatedWorkerThread&);

    bool askedToTerminate() const final { return m_askedToTerminate; }

    void workerGlobalScopeDestroyedInternal();
    Worker* workerObject() const { return m_workerObject; }

    RefPtr<ScriptExecutionContext> m_scriptExecutionContext;
    ScriptExecutionContextIdentifier m_loaderContextIdentifier;
    RefPtr<WorkerInspectorProxy> m_inspectorProxy;
    RefPtr<WorkerUserGestureForwarder> m_userGestureForwarder;
    Worker* m_workerObject;
    bool m_mayBeDestroyed { false };
    RefPtr<DedicatedWorkerThread> m_workerThread;

    bool m_askedToSuspend { false };
    bool m_askedToTerminate { false };

    Vector<std::unique_ptr<ScriptExecutionContext::Task>> m_queuedEarlyTasks; // Tasks are queued here until there's a thread object created.
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WorkerMessagingProxy)
    static bool isType(const WebCore::WorkerLoaderProxy& proxy) { return proxy.isWorkerMessagingProxy(); }
SPECIALIZE_TYPE_TRAITS_END()
