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

#include "config.h"
#include "ServiceWorkerGlobalScope.h"

#if ENABLE(SERVICE_WORKER)

#include "ServiceWorkerClients.h"
#include "ServiceWorkerThread.h"

namespace WebCore {

ServiceWorkerGlobalScope::ServiceWorkerGlobalScope(uint64_t serverConnectionIdentifier, const ServiceWorkerContextData& data, const URL& url, const String& identifier, const String& userAgent, ServiceWorkerThread& thread, bool shouldBypassMainWorldContentSecurityPolicy, Ref<SecurityOrigin>&& topOrigin, MonotonicTime timeOrigin, IDBClient::IDBConnectionProxy* connectionProxy, SocketProvider* socketProvider, PAL::SessionID sessionID)
    : WorkerGlobalScope(url, identifier, userAgent, thread, shouldBypassMainWorldContentSecurityPolicy, WTFMove(topOrigin), timeOrigin, connectionProxy, socketProvider, sessionID)
    , m_serverConnectionIdentifier(serverConnectionIdentifier)
    , m_contextData(crossThreadCopy(data))
    , m_clients(ServiceWorkerClients::create(*this))
{
}

ServiceWorkerGlobalScope::~ServiceWorkerGlobalScope() = default;

ServiceWorkerRegistration* ServiceWorkerGlobalScope::registration()
{
    // FIXME: implement this.
    return nullptr;
}

void ServiceWorkerGlobalScope::skipWaiting(Ref<DeferredPromise>&&)
{
}

EventTargetInterface ServiceWorkerGlobalScope::eventTargetInterface() const
{
    return ServiceWorkerGlobalScopeEventTargetInterfaceType;
}

ServiceWorkerThread& ServiceWorkerGlobalScope::thread()
{
    return static_cast<ServiceWorkerThread&>(WorkerGlobalScope::thread());
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
