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

#include "ContentSecurityPolicyResponseHeaders.h"
#include "WorkerOrWorkletThread.h"
#include <JavaScriptCore/RuntimeFlags.h>
#include <memory>
#include <wtf/URL.h>

namespace WebCore {

class NotificationClient;
class SecurityOrigin;
class SocketProvider;
class WorkerGlobalScope;
class WorkerLoaderProxy;
class WorkerDebuggerProxy;
class WorkerReportingProxy;

enum class WorkerThreadStartMode {
    Normal,
    WaitForInspector,
};

namespace IDBClient {
class IDBConnectionProxy;
}

struct WorkerThreadStartupData;

struct WorkerParameters {
public:
    URL scriptURL;
    String name;
    String identifier;
    String userAgent;
    bool isOnline;
    ContentSecurityPolicyResponseHeaders contentSecurityPolicyResponseHeaders;
    bool shouldBypassMainWorldContentSecurityPolicy;
    MonotonicTime timeOrigin;
    ReferrerPolicy referrerPolicy;
    Settings::Values settingsValues;

    WorkerParameters isolatedCopy() const;
};

class WorkerThread : public WorkerOrWorkletThread {
public:
    virtual ~WorkerThread();

    WorkerLoaderProxy& workerLoaderProxy() final { return m_workerLoaderProxy; }
    WorkerDebuggerProxy* workerDebuggerProxy() const final { return &m_workerDebuggerProxy; }
    WorkerReportingProxy& workerReportingProxy() const { return m_workerReportingProxy; }

    // Number of active worker threads.
    WEBCORE_EXPORT static unsigned workerThreadCount();

#if ENABLE(NOTIFICATIONS)
    NotificationClient* getNotificationClient() { return m_notificationClient; }
    void setNotificationClient(NotificationClient* client) { m_notificationClient = client; }
#endif
    
    JSC::RuntimeFlags runtimeFlags() const { return m_runtimeFlags; }

protected:
    WorkerThread(const WorkerParameters&, const String& sourceCode, WorkerLoaderProxy&, WorkerDebuggerProxy&, WorkerReportingProxy&, WorkerThreadStartMode, const SecurityOrigin& topOrigin, IDBClient::IDBConnectionProxy*, SocketProvider*, JSC::RuntimeFlags);

    // Factory method for creating a new worker context for the thread.
    virtual Ref<WorkerGlobalScope> createWorkerGlobalScope(const WorkerParameters&, Ref<SecurityOrigin>&&, Ref<SecurityOrigin>&& topOrigin) = 0;

    WorkerGlobalScope* globalScope();

    IDBClient::IDBConnectionProxy* idbConnectionProxy();
    SocketProvider* socketProvider();

private:
    virtual bool isServiceWorkerThread() const { return false; }

    virtual void finishedEvaluatingScript() { }

    // WorkerOrWorkletThread.
    Ref<WTF::Thread> createThread() final;
    Ref<WorkerOrWorkletGlobalScope> createGlobalScope() final;
    void evaluateScriptIfNecessary(String& exceptionMessage) final;
    bool shouldWaitForWebInspectorOnStartup() const final;

    WorkerLoaderProxy& m_workerLoaderProxy;
    WorkerDebuggerProxy& m_workerDebuggerProxy;
    WorkerReportingProxy& m_workerReportingProxy;
    JSC::RuntimeFlags m_runtimeFlags;

    std::unique_ptr<WorkerThreadStartupData> m_startupData;

#if ENABLE(NOTIFICATIONS)
    NotificationClient* m_notificationClient { nullptr };
#endif

#if ENABLE(INDEXED_DATABASE)
    RefPtr<IDBClient::IDBConnectionProxy> m_idbConnectionProxy;
#endif
    RefPtr<SocketProvider> m_socketProvider;
};

} // namespace WebCore
