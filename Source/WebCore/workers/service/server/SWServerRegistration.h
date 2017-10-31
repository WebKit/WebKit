/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(SERVICE_WORKER)

#include "SWServer.h"
#include "ServiceWorkerJobData.h"
#include "ServiceWorkerRegistrationData.h"
#include "Timer.h"
#include <wtf/Deque.h>
#include <wtf/Identified.h>

namespace WebCore {

class SWServer;
class SWServerWorker;
struct ExceptionData;
struct ServiceWorkerFetchResult;

class SWServerRegistration : public ThreadSafeIdentified<SWServerRegistration> {
public:
    explicit SWServerRegistration(SWServer&, const ServiceWorkerRegistrationKey&);
    SWServerRegistration(const SWServerRegistration&) = delete;
    ~SWServerRegistration();

    void enqueueJob(const ServiceWorkerJobData&);
    void scriptFetchFinished(SWServer::Connection&, const ServiceWorkerFetchResult&);
    void scriptContextFailedToStart(SWServer::Connection&, const String& workerID, const String& message);
    void scriptContextStarted(SWServer::Connection&, uint64_t identifier, const String& workerID);
    
    ServiceWorkerRegistrationData data() const;

private:
    void jobTimerFired();
    void startNextJob();
    void rejectCurrentJob(const ExceptionData&);
    void resolveCurrentRegistrationJob(const ServiceWorkerRegistrationData&);
    void resolveCurrentUnregistrationJob(bool unregistrationResult);
    void startScriptFetchForCurrentJob();
    void finishCurrentJob();

    void runRegisterJob(const ServiceWorkerJobData&);
    void runUnregisterJob(const ServiceWorkerJobData&);
    void runUpdateJob(const ServiceWorkerJobData&);

    void rejectWithExceptionOnMainThread(const ExceptionData&);
    void resolveWithRegistrationOnMainThread();
    void resolveWithUnregistrationResultOnMainThread(bool);
    void startScriptFetchFromMainThread();
    bool isEmpty();
    SWServerWorker* getNewestWorker();

    Deque<ServiceWorkerJobData> m_jobQueue;
    std::unique_ptr<ServiceWorkerJobData> m_currentJob;

    bool m_uninstalling { false };
    std::unique_ptr<SWServerWorker> m_installingWorker;
    std::unique_ptr<SWServerWorker> m_waitingWorker;
    std::unique_ptr<SWServerWorker> m_activeWorker;
    URL m_scopeURL;
    URL m_scriptURL;
    std::optional<ServiceWorkerUpdateViaCache> m_updateViaCache;
    
    double m_lastUpdateTime { 0 };

    Timer m_jobTimer;
    SWServer& m_server;
    ServiceWorkerRegistrationKey m_registrationKey;
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
