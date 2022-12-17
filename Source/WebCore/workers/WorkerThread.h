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
#include "CrossOriginEmbedderPolicy.h"
#include "FetchRequestCredentials.h"
#include "NotificationPermission.h"
#include "ScriptExecutionContextIdentifier.h"
#include "ServiceWorkerRegistrationData.h"
#include "WorkerClient.h"
#include "WorkerOrWorkletThread.h"
#include "WorkerRunLoop.h"
#include "WorkerType.h"
#include <JavaScriptCore/RuntimeFlags.h>
#include <memory>
#include <pal/SessionID.h>
#include <wtf/URL.h>

namespace WebCore {

class NotificationClient;
class Page;
class ScriptBuffer;
class SecurityOrigin;
class SocketProvider;
class WorkerBadgeProxy;
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
    URL ownerURL;
    String name;
    String inspectorIdentifier;
    String userAgent;
    bool isOnline;
    ContentSecurityPolicyResponseHeaders contentSecurityPolicyResponseHeaders;
    bool shouldBypassMainWorldContentSecurityPolicy;
    CrossOriginEmbedderPolicy crossOriginEmbedderPolicy;
    MonotonicTime timeOrigin;
    ReferrerPolicy referrerPolicy;
    WorkerType workerType;
    FetchRequestCredentials credentials;
    Settings::Values settingsValues;
    WorkerThreadMode workerThreadMode { WorkerThreadMode::CreateNewThread };
    PAL::SessionID sessionID;
#if ENABLE(SERVICE_WORKER)
    std::optional<ServiceWorkerData> serviceWorkerData;
#endif
    ScriptExecutionContextIdentifier clientIdentifier;

    WorkerParameters isolatedCopy() const;
};

class WorkerThread : public WorkerOrWorkletThread {
public:
    virtual ~WorkerThread();

    WorkerBadgeProxy& workerBadgeProxy() const { return m_workerBadgeProxy; }
    WorkerDebuggerProxy* workerDebuggerProxy() const final { return &m_workerDebuggerProxy; }
    WorkerLoaderProxy& workerLoaderProxy() final { return m_workerLoaderProxy; }
    WorkerReportingProxy& workerReportingProxy() const { return m_workerReportingProxy; }

    // Number of active worker threads.
    WEBCORE_EXPORT static unsigned workerThreadCount();

#if ENABLE(NOTIFICATIONS)
    NotificationClient* getNotificationClient() { return m_notificationClient; }
    void setNotificationClient(NotificationClient* client) { m_notificationClient = client; }
#endif
    
    JSC::RuntimeFlags runtimeFlags() const { return m_runtimeFlags; }
    bool isInStaticScriptEvaluation() const { return m_isInStaticScriptEvaluation; }

    void setWorkerClient(std::unique_ptr<WorkerClient>&& client) { m_workerClient = WTFMove(client); }
    WorkerClient* workerClient() { return m_workerClient.get(); }
protected:
    WorkerThread(const WorkerParameters&, const ScriptBuffer& sourceCode, WorkerLoaderProxy&, WorkerDebuggerProxy&, WorkerReportingProxy&, WorkerBadgeProxy&, WorkerThreadStartMode, const SecurityOrigin& topOrigin, IDBClient::IDBConnectionProxy*, SocketProvider*, JSC::RuntimeFlags);

    // Factory method for creating a new worker context for the thread.
    virtual Ref<WorkerGlobalScope> createWorkerGlobalScope(const WorkerParameters&, Ref<SecurityOrigin>&&, Ref<SecurityOrigin>&& topOrigin) = 0;

    WorkerGlobalScope* globalScope();

    IDBClient::IDBConnectionProxy* idbConnectionProxy();
    SocketProvider* socketProvider();

    std::unique_ptr<WorkerClient> m_workerClient;
private:
    virtual ASCIILiteral threadName() const = 0;

    virtual void finishedEvaluatingScript() { }

    // WorkerOrWorkletThread.
    Ref<Thread> createThread() final;
    RefPtr<WorkerOrWorkletGlobalScope> createGlobalScope() final;
    void evaluateScriptIfNecessary(String& exceptionMessage) final;
    bool shouldWaitForWebInspectorOnStartup() const final;

    WorkerLoaderProxy& m_workerLoaderProxy;
    WorkerDebuggerProxy& m_workerDebuggerProxy;
    WorkerReportingProxy& m_workerReportingProxy;
    WorkerBadgeProxy& m_workerBadgeProxy;
    JSC::RuntimeFlags m_runtimeFlags;

    std::unique_ptr<WorkerThreadStartupData> m_startupData;

#if ENABLE(NOTIFICATIONS)
    NotificationClient* m_notificationClient { nullptr };
#endif

    RefPtr<IDBClient::IDBConnectionProxy> m_idbConnectionProxy;
    RefPtr<SocketProvider> m_socketProvider;
    bool m_isInStaticScriptEvaluation { false };
};

} // namespace WebCore
