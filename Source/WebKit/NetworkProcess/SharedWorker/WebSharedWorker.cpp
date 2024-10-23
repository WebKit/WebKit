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
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WeakRef.h>

namespace WebKit {

static HashMap<WebCore::SharedWorkerIdentifier, WeakRef<WebSharedWorker>>& allWorkers()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<HashMap<WebCore::SharedWorkerIdentifier, WeakRef<WebSharedWorker>>> allWorkers;
    return allWorkers;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebSharedWorker);


Ref<WebSharedWorker> WebSharedWorker::create(WebSharedWorkerServer& server, const WebCore::SharedWorkerKey& key, const WebCore::WorkerOptions& options)
{
    return adoptRef(*new WebSharedWorker(server, key, options));
}

WebSharedWorker::WebSharedWorker(WebSharedWorkerServer& server, const WebCore::SharedWorkerKey& key, const WebCore::WorkerOptions& workerOptions)
    : m_server(server)
    , m_key(key)
    , m_workerOptions(workerOptions)
{
    ASSERT(!allWorkers().contains(identifier()));
    allWorkers().add(identifier(), *this);
}

WebSharedWorker::~WebSharedWorker()
{
    if (RefPtr connection = contextConnection()) {
        for (auto& sharedWorkerObject : m_sharedWorkerObjects)
            connection->removeSharedWorkerObject(sharedWorkerObject.identifier);
    }

    ASSERT(allWorkers().get(identifier()) == this);
    allWorkers().remove(identifier());
}

WebSharedWorker* WebSharedWorker::fromIdentifier(WebCore::SharedWorkerIdentifier identifier)
{
    return allWorkers().get(identifier);
}

WebCore::RegistrableDomain WebSharedWorker::topRegistrableDomain() const
{
    return WebCore::RegistrableDomain { m_key.origin.topOrigin };
}

void WebSharedWorker::setFetchResult(WebCore::WorkerFetchResult&& fetchResult)
{
    m_fetchResult = WTFMove(fetchResult);
}

void WebSharedWorker::didCreateContextConnection(WebSharedWorkerServerToContextConnection& contextConnection)
{
    for (auto& sharedWorkerObject : m_sharedWorkerObjects)
        contextConnection.addSharedWorkerObject(sharedWorkerObject.identifier);
    if (didFinishFetching())
        launch(contextConnection);
}

void WebSharedWorker::launch(WebSharedWorkerServerToContextConnection& connection)
{
    connection.launchSharedWorker(*this);
    if (m_isSuspended)
        connection.suspendSharedWorker(identifier());
}

void WebSharedWorker::addSharedWorkerObject(WebCore::SharedWorkerObjectIdentifier sharedWorkerObjectIdentifier, const WebCore::TransferredMessagePort& port)
{
    ASSERT(!m_sharedWorkerObjects.contains({ sharedWorkerObjectIdentifier, { false, port } }));
    m_sharedWorkerObjects.add({ sharedWorkerObjectIdentifier, { false, port } });
    if (RefPtr connection = contextConnection())
        connection->addSharedWorkerObject(sharedWorkerObjectIdentifier);

    resumeIfNeeded();
}

void WebSharedWorker::removeSharedWorkerObject(WebCore::SharedWorkerObjectIdentifier sharedWorkerObjectIdentifier)
{
    m_sharedWorkerObjects.remove({ sharedWorkerObjectIdentifier, { } });
    if (RefPtr connection = contextConnection())
        connection->removeSharedWorkerObject(sharedWorkerObjectIdentifier);

    suspendIfNeeded();
}

void WebSharedWorker::suspend(WebCore::SharedWorkerObjectIdentifier sharedWorkerObjectIdentifier)
{
    auto iterator = m_sharedWorkerObjects.find({ sharedWorkerObjectIdentifier, { } });
    if (iterator == m_sharedWorkerObjects.end())
        return;

    iterator->state.isSuspended = true;
    ASSERT(!m_isSuspended);
    suspendIfNeeded();
}

void WebSharedWorker::suspendIfNeeded()
{
    if (m_isSuspended)
        return;

    for (auto& object : m_sharedWorkerObjects) {
        if (!object.state.isSuspended)
            return;
    }

    m_isSuspended = true;
    if (RefPtr connection = contextConnection())
        connection->suspendSharedWorker(identifier());
}

void WebSharedWorker::resume(WebCore::SharedWorkerObjectIdentifier sharedWorkerObjectIdentifier)
{
    auto iterator = m_sharedWorkerObjects.find({ sharedWorkerObjectIdentifier, { } });
    if (iterator == m_sharedWorkerObjects.end())
        return;

    iterator->state.isSuspended = false;
    resumeIfNeeded();
}

void WebSharedWorker::resumeIfNeeded()
{
    if (!m_isSuspended)
        return;

    m_isSuspended = false;
    if (RefPtr connection = contextConnection())
        connection->resumeSharedWorker(identifier());
}

void WebSharedWorker::forEachSharedWorkerObject(const Function<void(WebCore::SharedWorkerObjectIdentifier, const WebCore::TransferredMessagePort&)>& apply) const
{
    for (auto& object : m_sharedWorkerObjects)
        apply(object.identifier, *object.state.port);
}

std::optional<WebCore::ProcessIdentifier> WebSharedWorker::firstSharedWorkerObjectProcess() const
{
    if (m_sharedWorkerObjects.isEmpty())
        return std::nullopt;
    return m_sharedWorkerObjects.first().identifier.processIdentifier();
}

WebSharedWorkerServerToContextConnection* WebSharedWorker::contextConnection() const
{
    if (!m_server)
        return nullptr;
    return m_server->contextConnectionForRegistrableDomain(topRegistrableDomain());
}

} // namespace WebKit
