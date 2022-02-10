/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WebSharedWorker.h"

#include "WebSharedWorkerServer.h"
#include "WebSharedWorkerServerToContextConnection.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>

namespace WebKit {

static HashMap<WebCore::SharedWorkerIdentifier, WebSharedWorker*>& allWorkers()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<HashMap<WebCore::SharedWorkerIdentifier, WebSharedWorker*>> allWorkers;
    return allWorkers;
}

WebSharedWorker::WebSharedWorker(WebSharedWorkerServer& server, const WebCore::SharedWorkerKey& key, const WebCore::WorkerOptions& workerOptions)
    : m_server(server)
    , m_identifier(WebCore::SharedWorkerIdentifier::generate())
    , m_key(key)
    , m_workerOptions(workerOptions)
{
    ASSERT(!allWorkers().contains(m_identifier));
    allWorkers().add(m_identifier, this);
}

WebSharedWorker::~WebSharedWorker()
{
    if (auto* connection = contextConnection()) {
        for (auto& sharedWorkerObjectIdentifier : m_sharedWorkerObjects.keys())
            connection->removeSharedWorkerObject(sharedWorkerObjectIdentifier);
    }

    ASSERT(allWorkers().get(m_identifier) == this);
    allWorkers().remove(m_identifier);
}

WebSharedWorker* WebSharedWorker::fromIdentifier(WebCore::SharedWorkerIdentifier identifier)
{
    return allWorkers().get(identifier);
}

WebCore::RegistrableDomain WebSharedWorker::registrableDomain() const
{
    return WebCore::RegistrableDomain { url() };
}

void WebSharedWorker::didCreateContextConnection(WebSharedWorkerServerToContextConnection& contextConnection)
{
    for (auto& sharedWorkerObjectIdentifier : m_sharedWorkerObjects.keys())
        contextConnection.addSharedWorkerObject(sharedWorkerObjectIdentifier);
}

void WebSharedWorker::addSharedWorkerObject(WebCore::SharedWorkerObjectIdentifier sharedWorkerObjectIdentifier, const WebCore::TransferredMessagePort& port)
{
    m_sharedWorkerObjects.add(sharedWorkerObjectIdentifier, port);
    if (auto* connection = contextConnection())
        connection->addSharedWorkerObject(sharedWorkerObjectIdentifier);
}

void WebSharedWorker::removeSharedWorkerObject(WebCore::SharedWorkerObjectIdentifier sharedWorkerObjectIdentifier)
{
    m_sharedWorkerObjects.remove(sharedWorkerObjectIdentifier);
    if (auto* connection = contextConnection())
        connection->removeSharedWorkerObject(sharedWorkerObjectIdentifier);
}

void WebSharedWorker::forEachSharedWorkerObject(const Function<void(WebCore::SharedWorkerObjectIdentifier, const WebCore::TransferredMessagePort&)>& apply) const
{
    for (auto& [sharedWorkerObjectIdentifier, port] : m_sharedWorkerObjects)
        apply(sharedWorkerObjectIdentifier, port);
}

WebSharedWorkerServerToContextConnection* WebSharedWorker::contextConnection() const
{
    return m_server.contextConnectionForRegistrableDomain(registrableDomain());
}

} // namespace WebKit
