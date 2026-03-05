/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "RemoteMediaResourceLoaderProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "ArgumentCoders.h"
#include "Connection.h"
#include "RemoteMediaResourceIdentifier.h"
#include "RemoteMediaResourceLoaderMessages.h"
#include "RemoteMediaResourceLoaderProxyMessages.h"
#include "RemoteMediaResourceProxy.h"
#include "SharedBufferReference.h"
#include <WebCore/ResourceError.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMediaResourceLoaderProxy);

Ref<RemoteMediaResourceLoaderProxy> RemoteMediaResourceLoaderProxy::create(Ref<IPC::Connection>&& connection, WebCore::PlatformMediaResourceLoader& platformLoader, RemoteMediaResourceLoaderIdentifier identifier)
{
    auto loader = adoptRef(*new RemoteMediaResourceLoaderProxy(WTF::move(connection), platformLoader, identifier));
    loader->initializeConnection();
    return loader;
}

RemoteMediaResourceLoaderProxy::RemoteMediaResourceLoaderProxy(Ref<IPC::Connection>&& connection, WebCore::PlatformMediaResourceLoader& platformLoader, RemoteMediaResourceLoaderIdentifier identifier)
    : m_connection { WTF::move(connection) }
    , m_platformLoader { platformLoader }
    , m_identifier { identifier }
{
}

RemoteMediaResourceLoaderProxy::~RemoteMediaResourceLoaderProxy()
{
    m_connection->removeWorkQueueMessageReceiver(Messages::RemoteMediaResourceLoaderProxy::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteMediaResourceLoaderProxy::initializeConnection()
{
    m_connection->addWorkQueueMessageReceiver(Messages::RemoteMediaResourceLoaderProxy::messageReceiverName(), defaultQueue(), *this, m_identifier.toUInt64());
}

void RemoteMediaResourceLoaderProxy::requestResource(RemoteMediaResourceIdentifier id, WebCore::ResourceRequest&& request, WebCore::PlatformMediaResourceLoader::LoadOptions options)
{
    assertIsCurrent(defaultQueue());
    ASSERT(!m_mediaResources.contains(id));

    if (auto resource = m_platformLoader->requestResource(WTF::move(request), options)) {
        resource->setClient(adoptRef(*new RemoteMediaResourceProxy(*this, *resource, id)));
        m_mediaResources.add(id, WTF::move(resource));
        return;
    }

    // FIXME: Get the error from MediaResourceLoader::requestResource.
    m_connection->send(Messages::RemoteMediaResourceLoader::LoadFailed(id, { WebCore::ResourceError::Type::Cancellation }), m_identifier.toUInt64());
}

void RemoteMediaResourceLoaderProxy::sendH2Ping(const URL& url, CompletionHandler<void(Expected<WTF::Seconds, WebCore::ResourceError>&&)>&& completionHandler)
{
    assertIsCurrent(defaultQueue());
    m_platformLoader->sendH2Ping(url, WTF::move(completionHandler));
}

void RemoteMediaResourceLoaderProxy::removeResource(RemoteMediaResourceIdentifier id, CompletionHandler<void()>&& completionHandler)
{
    assertIsCurrent(defaultQueue());

    // The client(RemoteMediaResourceProxy) will be destroyed as well
    if (auto resource = m_mediaResources.take(id))
        resource->shutdown();

    completionHandler();
}

void RemoteMediaResourceLoaderProxy::responseReceived(RemoteMediaResourceIdentifier id, const WebCore::ResourceResponse& response, bool didPassAccessControlCheck, CompletionHandler<void(WebCore::ShouldContinuePolicyCheck)>&& completionHandler)
{
    m_connection->sendWithAsyncReply(Messages::RemoteMediaResourceLoader::ResponseReceived(id, response, didPassAccessControlCheck), WTF::move(completionHandler), m_identifier.toUInt64());
}

void RemoteMediaResourceLoaderProxy::redirectReceived(RemoteMediaResourceIdentifier id, WebCore::ResourceRequest&& request, const WebCore::ResourceResponse& response, CompletionHandler<void(WebCore::ResourceRequest&&)>&& completionHandler)
{
    m_connection->sendWithAsyncReply(Messages::RemoteMediaResourceLoader::RedirectReceived(id, WTF::move(request), response), WTF::move(completionHandler), m_identifier.toUInt64());
}

void RemoteMediaResourceLoaderProxy::dataSent(RemoteMediaResourceIdentifier id, uint64_t bytesSent, uint64_t totalBytesToBeSent)
{
    m_connection->send(Messages::RemoteMediaResourceLoader::DataSent(id, bytesSent, totalBytesToBeSent), m_identifier.toUInt64());
}

void RemoteMediaResourceLoaderProxy::dataReceived(RemoteMediaResourceIdentifier id, const WebCore::SharedBuffer& buffer)
{
    m_connection->sendWithAsyncReply(Messages::RemoteMediaResourceLoader::DataReceived(id, IPC::SharedBufferReference { buffer }), [] (auto&& bufferHandle) {
        // Take ownership of shared memory and mark it as media-related memory.
        if (bufferHandle)
            bufferHandle->takeOwnershipOfMemory(WebCore::MemoryLedger::Media);
    }, m_identifier.toUInt64());
}

void RemoteMediaResourceLoaderProxy::accessControlCheckFailed(RemoteMediaResourceIdentifier id, const WebCore::ResourceError& error)
{
    m_connection->send(Messages::RemoteMediaResourceLoader::AccessControlCheckFailed(id, error), m_identifier.toUInt64());
}

void RemoteMediaResourceLoaderProxy::loadFailed(RemoteMediaResourceIdentifier id, const WebCore::ResourceError& error)
{
    m_connection->send(Messages::RemoteMediaResourceLoader::LoadFailed(id, error), m_identifier.toUInt64());
}

void RemoteMediaResourceLoaderProxy::loadFinished(RemoteMediaResourceIdentifier id, const WebCore::NetworkLoadMetrics& metrics)
{
    m_connection->send(Messages::RemoteMediaResourceLoader::LoadFinished(id, metrics), m_identifier.toUInt64());
}

}

#endif
