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

#include "config.h"
#include "ServiceWorkerDebuggable.h"

#if ENABLE(REMOTE_INSPECTOR)
#if ENABLE(SERVICE_WORKER)

#include "ServiceWorkerInspectorProxy.h"
#include "ServiceWorkerThreadProxy.h"

namespace WebCore {

using namespace Inspector;

ServiceWorkerDebuggable::ServiceWorkerDebuggable(ServiceWorkerThreadProxy& serviceWorkerThreadProxy, const ServiceWorkerContextData& data)
    : m_serviceWorkerThreadProxy(serviceWorkerThreadProxy)
    , m_scopeURL(data.registration.scopeURL)
{
}

void ServiceWorkerDebuggable::connect(FrontendChannel& channel, bool, bool)
{
    m_serviceWorkerThreadProxy.inspectorProxy().connectToWorker(channel);
}

void ServiceWorkerDebuggable::disconnect(FrontendChannel& channel)
{
    m_serviceWorkerThreadProxy.inspectorProxy().disconnectFromWorker(channel);
}

void ServiceWorkerDebuggable::dispatchMessageFromRemote(const String& message)
{
    m_serviceWorkerThreadProxy.inspectorProxy().sendMessageToWorker(message);
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
#endif // ENABLE(REMOTE_INSPECTOR)
