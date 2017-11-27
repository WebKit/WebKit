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

#include "ServiceWorkerData.h"
#include "ServiceWorkerIdentifier.h"
#include "ServiceWorkerRegistrationKey.h"
#include "ServiceWorkerTypes.h"
#include "URL.h"
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class SWServer;
class SWServerRegistration;
enum class WorkerType;
struct ServiceWorkerJobDataIdentifier;

class SWServerWorker : public ThreadSafeRefCounted<SWServerWorker> {
public:
    template <typename... Args> static Ref<SWServerWorker> create(Args&&... args)
    {
        return adoptRef(*new SWServerWorker(std::forward<Args>(args)...));
    }
    
    SWServerWorker(const SWServerWorker&) = delete;
    WEBCORE_EXPORT ~SWServerWorker();

    void terminate();

    SWServer& server();
    const ServiceWorkerRegistrationKey& registrationKey() const { return m_registrationKey; }
    const URL& scriptURL() const { return m_data.scriptURL; }
    const String& script() const { return m_script; }
    WorkerType type() const { return m_data.type; }

    ServiceWorkerIdentifier identifier() const { return m_data.identifier; }
    SWServerToContextConnectionIdentifier contextConnectionIdentifier() const { return m_contextConnectionIdentifier; }

    ServiceWorkerState state() const { return m_data.state; }
    void setState(ServiceWorkerState state) { m_data.state = state; }

    bool hasPendingEvents() const { return m_hasPendingEvents; }
    void setHasPendingEvents(bool value) { m_hasPendingEvents = value; }

    void scriptContextFailedToStart(const ServiceWorkerJobDataIdentifier&, const String& message);
    void scriptContextStarted(const ServiceWorkerJobDataIdentifier&);
    void didFinishInstall(const ServiceWorkerJobDataIdentifier&, bool wasSuccessful);
    void didFinishActivation();
    void contextTerminated();

    WEBCORE_EXPORT static SWServerWorker* existingWorkerForIdentifier(ServiceWorkerIdentifier);

    const ServiceWorkerData& data() const { return m_data; }

private:
    SWServerWorker(SWServer&, SWServerRegistration&, SWServerToContextConnectionIdentifier, const URL&, const String& script, WorkerType, ServiceWorkerIdentifier);

    SWServer& m_server;
    ServiceWorkerRegistrationKey m_registrationKey;
    SWServerToContextConnectionIdentifier m_contextConnectionIdentifier;
    ServiceWorkerData m_data;
    String m_script;
    bool m_hasPendingEvents { false };
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
