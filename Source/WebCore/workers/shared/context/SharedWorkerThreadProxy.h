/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "WorkerDebuggerProxy.h"
#include "WorkerGlobalScopeProxy.h"
#include "WorkerLoaderProxy.h"
#include "WorkerObjectProxy.h"
#include "WorkerOptions.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class SharedWorker;
class SharedWorkerThread;

class SharedWorkerThreadProxy final : public ThreadSafeRefCounted<SharedWorkerThreadProxy>, public WorkerGlobalScopeProxy, public WorkerObjectProxy, public WorkerLoaderProxy, public WorkerDebuggerProxy {
public:
    template<typename... Args> static SharedWorkerThreadProxy& create(Args&&... args) { return *new SharedWorkerThreadProxy(std::forward<Args>(args)...); }

    SharedWorkerThread* thread() { return m_workerThread.get(); }

    void startWorkerGlobalScope(const URL& scriptURL, const String& name, const String& userAgent, bool isOnline, const ScriptBuffer& sourceCode, const ContentSecurityPolicyResponseHeaders&, bool shouldBypassMainWorldContentSecurityPolicy, const CrossOriginEmbedderPolicy&, MonotonicTime timeOrigin, ReferrerPolicy, WorkerType, FetchRequestCredentials, JSC::RuntimeFlags) final;
    void workerObjectDestroyed() final;
    bool hasPendingActivity() const final;
    void terminateWorkerGlobalScope() final;

private:
    explicit SharedWorkerThreadProxy(SharedWorker&);

    void workerGlobalScopeDestroyedInternal();

    void postMessageToWorkerGlobalScope(MessageWithMessagePorts&&) final;
    void postTaskToWorkerGlobalScope(Function<void(ScriptExecutionContext&)>&&) final;
    void notifyNetworkStateChange(bool isOnline) final;
    void suspendForBackForwardCache() final;
    void resumeForBackForwardCache() final;
    void postExceptionToWorkerObject(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL) final;
    void workerGlobalScopeDestroyed() final;
    void postMessageToWorkerObject(MessageWithMessagePorts&&) final;
    void confirmMessageFromWorkerObject(bool hasPendingActivity) final;
    void reportPendingActivity(bool hasPendingActivity) final;
    RefPtr<CacheStorageConnection> createCacheStorageConnection() final;
    RefPtr<RTCDataChannelRemoteHandlerConnection> createRTCDataChannelRemoteHandlerConnection() final;
    void postTaskToLoader(ScriptExecutionContext::Task&&) final;
    bool postTaskForModeToWorkerOrWorkletGlobalScope(ScriptExecutionContext::Task&&, const String& mode) final;
    void postMessageToDebugger(const String&) final;
    void setResourceCachingDisabledByWebInspector(bool) final;

    WeakPtr<SharedWorker> m_sharedWorker;
    RefPtr<SharedWorkerThread> m_workerThread;
    const RefPtr<ScriptExecutionContext> m_scriptExecutionContext;
    const String m_identifierForInspector;
    bool m_askedToTerminate { false };
    bool m_hasPendingActivity { false };
    bool m_mayBeDestroyed { false };
};

} // namespace WebCore
