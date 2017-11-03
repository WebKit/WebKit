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
#include "ServiceWorkerRegistrationData.h"
#include <wtf/Identified.h>

namespace WebCore {

class SWServer;
class SWServerWorker;
enum class ServiceWorkerRegistrationState;
struct ExceptionData;
struct ServiceWorkerFetchResult;

class SWServerRegistration : public ThreadSafeIdentified<SWServerRegistration> {
public:
    SWServerRegistration(SWServer&, const ServiceWorkerRegistrationKey&, ServiceWorkerUpdateViaCache, const URL& scopeURL, const URL& scriptURL);
    ~SWServerRegistration();

    const ServiceWorkerRegistrationKey& key() const { return m_registrationKey; }

    SWServerWorker* getNewestWorker();
    ServiceWorkerRegistrationData data() const;

    bool isUninstalling() const { return m_uninstalling; }
    void setIsUninstalling(bool value) { m_uninstalling = value; }

    void setLastUpdateTime(double time) { m_lastUpdateTime = time; }
    ServiceWorkerUpdateViaCache updateViaCache() const { return m_updateViaCache; }

    void setActiveServiceWorkerIdentifier(ServiceWorkerIdentifier identifier) { m_activeServiceWorkerIdentifier = identifier; }

    void updateRegistrationState(ServiceWorkerRegistrationState, SWServerWorker*);

    void addClientServiceWorkerRegistration(uint64_t connectionIdentifier, uint64_t clientRegistrationIdentifier);
    void removeClientServiceWorkerRegistration(uint64_t connectionIdentifier, uint64_t clientRegistrationIdentifier);

private:
    ServiceWorkerRegistrationKey m_registrationKey;
    ServiceWorkerUpdateViaCache m_updateViaCache;
    URL m_scopeURL;
    URL m_scriptURL;

    bool m_uninstalling { false };
    RefPtr<SWServerWorker> m_installingWorker;
    RefPtr<SWServerWorker> m_waitingWorker;
    RefPtr<SWServerWorker> m_activeWorker;

    std::optional<ServiceWorkerIdentifier> m_activeServiceWorkerIdentifier;

    double m_lastUpdateTime { 0 };
    
    HashMap<uint64_t, std::unique_ptr<HashSet<uint64_t>>> m_clientRegistrationsByConnection;
    SWServer& m_server;
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
