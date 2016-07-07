/*
 * Copyright (C) 2008, 2016 Apple Inc. All Rights Reserved.
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

#pragma once

#include "WorkerRunLoop.h"
#include <memory>
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class ContentSecurityPolicyResponseHeaders;
class URL;
class NotificationClient;
class SecurityOrigin;
class SocketProvider;
class WorkerGlobalScope;
class WorkerLoaderProxy;
class WorkerReportingProxy;

enum WorkerThreadStartMode { DontPauseWorkerGlobalScopeOnStart, PauseWorkerGlobalScopeOnStart };

namespace IDBClient {
class IDBConnectionProxy;
}

struct WorkerThreadStartupData;

class WorkerThread : public RefCounted<WorkerThread> {
public:
    virtual ~WorkerThread();

    bool start();
    void stop();

    ThreadIdentifier threadID() const { return m_threadID; }
    WorkerRunLoop& runLoop() { return m_runLoop; }
    WorkerLoaderProxy& workerLoaderProxy() const { return m_workerLoaderProxy; }
    WorkerReportingProxy& workerReportingProxy() const { return m_workerReportingProxy; }

    // Number of active worker threads.
    WEBCORE_EXPORT static unsigned workerThreadCount();
    static void releaseFastMallocFreeMemoryInAllThreads();

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    NotificationClient* getNotificationClient() { return m_notificationClient; }
    void setNotificationClient(NotificationClient* client) { m_notificationClient = client; }
#endif

protected:
    WorkerThread(const URL&, const String& userAgent, const String& sourceCode, WorkerLoaderProxy&, WorkerReportingProxy&, WorkerThreadStartMode, const ContentSecurityPolicyResponseHeaders&, bool shouldBypassMainWorldContentSecurityPolicy, const SecurityOrigin* topOrigin, IDBClient::IDBConnectionProxy*, SocketProvider*);

    // Factory method for creating a new worker context for the thread.
    virtual Ref<WorkerGlobalScope> createWorkerGlobalScope(const URL&, const String& userAgent, const ContentSecurityPolicyResponseHeaders&, bool shouldBypassMainWorldContentSecurityPolicy, PassRefPtr<SecurityOrigin> topOrigin) = 0;

    // Executes the event loop for the worker thread. Derived classes can override to perform actions before/after entering the event loop.
    virtual void runEventLoop();

    WorkerGlobalScope* workerGlobalScope() { return m_workerGlobalScope.get(); }

    IDBClient::IDBConnectionProxy* idbConnectionProxy();
    SocketProvider* socketProvider();

private:
    // Static function executed as the core routine on the new thread. Passed a pointer to a WorkerThread object.
    static void workerThreadStart(void*);
    void workerThread();

    ThreadIdentifier m_threadID;
    WorkerRunLoop m_runLoop;
    WorkerLoaderProxy& m_workerLoaderProxy;
    WorkerReportingProxy& m_workerReportingProxy;

    RefPtr<WorkerGlobalScope> m_workerGlobalScope;
    Lock m_threadCreationMutex;

    std::unique_ptr<WorkerThreadStartupData> m_startupData;

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    NotificationClient* m_notificationClient;
#endif

#if ENABLE(INDEXED_DATABASE)
    RefPtr<IDBClient::IDBConnectionProxy> m_idbConnectionProxy;
#endif
#if ENABLE(WEB_SOCKETS)
    RefPtr<SocketProvider> m_socketProvider;
#endif
};

} // namespace WebCore

