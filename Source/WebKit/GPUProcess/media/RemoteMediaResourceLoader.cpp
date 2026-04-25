/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "RemoteMediaResourceLoader.h"

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "ArgumentCoders.h"
#include "Connection.h"
#include "RemoteMediaPlayerProxy.h"
#include "RemoteMediaResource.h"
#include "RemoteMediaResourceLoaderMessages.h"
#include "RemoteMediaResourceLoaderProxyMessages.h"
#include "SharedBufferReference.h"
#include <WebCore/ResourceError.h>
#include <WebCore/SharedMemory.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMediaResourceLoader);

using namespace WebCore;

Ref<RemoteMediaResourceLoader> RemoteMediaResourceLoader::create(RemoteMediaPlayerProxy& proxy, Ref<IPC::Connection>&& connection)
{
    auto loader = adoptRef(*new RemoteMediaResourceLoader(proxy, WTF::move(connection)));
    loader->initializeConnection();
    return loader;
}

RemoteMediaResourceLoader::RemoteMediaResourceLoader(RemoteMediaPlayerProxy& remoteMediaPlayerProxy, Ref<IPC::Connection>&& connection)
    : m_remoteMediaPlayerProxy(remoteMediaPlayerProxy)
    , m_connection(WTF::move(connection))
{
    ASSERT(isMainRunLoop());
}

RemoteMediaResourceLoader::~RemoteMediaResourceLoader()
{
    ensureOnMainRunLoop([remoteMediaPlayerProxy = std::exchange(m_remoteMediaPlayerProxy, nullptr), identifier = identifier()] {
        if (RefPtr proxy = remoteMediaPlayerProxy.get())
            proxy->destroyResourceLoader(identifier);
    });
}

void RemoteMediaResourceLoader::initializeConnection()
{
    m_connection->addWorkQueueMessageReceiver(Messages::RemoteMediaResourceLoader::messageReceiverName(), defaultQueue(), *this, identifier().toUInt64());
}

RefPtr<PlatformMediaResource> RemoteMediaResourceLoader::requestResource(ResourceRequest&& request, LoadOptions options)
{
    // This call is thread-safe.
    AtomicObjectIdentifier<RemoteMediaResourceIdentifierType> remoteMediaResourceIdentifier = RemoteMediaResourceIdentifier::generate();
    auto remoteMediaResource = RemoteMediaResource::create(*this, remoteMediaResourceIdentifier);
    addMediaResource(remoteMediaResourceIdentifier, remoteMediaResource);

    m_connection->send(Messages::RemoteMediaResourceLoaderProxy::RequestResource(remoteMediaResourceIdentifier, WTF::move(request), options), identifier());

    return remoteMediaResource;
}

void RemoteMediaResourceLoader::sendH2Ping(const URL& url, CompletionHandler<void(Expected<Seconds, ResourceError>&&)>&& completionHandler)
{
    m_connection->sendWithAsyncReply(Messages::RemoteMediaResourceLoaderProxy::SendH2Ping(url), WTF::move(completionHandler), identifier());
}

void RemoteMediaResourceLoader::addMediaResource(RemoteMediaResourceIdentifier remoteMediaResourceIdentifier, RemoteMediaResource& remoteMediaResource)
{
    Locker locker { m_lock };
    ASSERT(!m_remoteMediaResources.contains(remoteMediaResourceIdentifier));
    m_remoteMediaResources.add(remoteMediaResourceIdentifier, ThreadSafeWeakPtr { remoteMediaResource });
}

void RemoteMediaResourceLoader::removeMediaResource(RemoteMediaResourceIdentifier remoteMediaResourceIdentifier)
{
    RefPtr resource = resourceForId(remoteMediaResourceIdentifier);
    ASSERT(resource);
    if (!resource)
        return;

    m_connection->sendWithAsyncReply(Messages::RemoteMediaResourceLoaderProxy::RemoveResource(remoteMediaResourceIdentifier), [protectedThis = Ref { *this }, remoteMediaResourceIdentifier] {
        Locker locker { protectedThis->m_lock };
        ASSERT(protectedThis->m_remoteMediaResources.contains(remoteMediaResourceIdentifier));
        protectedThis->m_remoteMediaResources.remove(remoteMediaResourceIdentifier);
    }, identifier());
}

RefPtr<RemoteMediaResource> RemoteMediaResourceLoader::resourceForId(RemoteMediaResourceIdentifier identifier)
{
    Locker locker { m_lock };
    return m_remoteMediaResources.get(identifier).get();
}

void RemoteMediaResourceLoader::responseReceived(RemoteMediaResourceIdentifier identifier, const ResourceResponse& response, bool didPassAccessControlCheck, CompletionHandler<void(ShouldContinuePolicyCheck)>&& completionHandler)
{
    assertIsCurrent(defaultQueue());
    if (RefPtr resource = resourceForId(identifier))
        resource->responseReceived(response, didPassAccessControlCheck, WTF::move(completionHandler));
    else
        completionHandler(ShouldContinuePolicyCheck::No);
}

void RemoteMediaResourceLoader::redirectReceived(RemoteMediaResourceIdentifier identifier, ResourceRequest&& request, const ResourceResponse& response, CompletionHandler<void(WebCore::ResourceRequest&&)>&& completionHandler)
{
    assertIsCurrent(defaultQueue());
    if (RefPtr resource = resourceForId(identifier))
        resource->redirectReceived(WTF::move(request), response, WTF::move(completionHandler));
    else
        completionHandler({ });
}

void RemoteMediaResourceLoader::dataSent(RemoteMediaResourceIdentifier identifier, uint64_t bytesSent, uint64_t totalBytesToBeSent)
{
    assertIsCurrent(defaultQueue());
    if (RefPtr resource = resourceForId(identifier))
        resource->dataSent(bytesSent, totalBytesToBeSent);
}

void RemoteMediaResourceLoader::dataReceived(RemoteMediaResourceIdentifier identifier, IPC::SharedBufferReference&& buffer, CompletionHandler<void(std::optional<SharedMemory::Handle>&&)>&& completionHandler)
{
    assertIsCurrent(defaultQueue());
    RefPtr resource = resourceForId(identifier);
    if (!resource)
        return completionHandler(std::nullopt);

    RefPtr sharedMemory = buffer.sharedCopy();
    if (!sharedMemory)
        return completionHandler(std::nullopt);

    auto handle = sharedMemory->createHandle(SharedMemory::Protection::ReadOnly);
    if (!handle)
        return completionHandler(std::nullopt);

    resource->dataReceived(sharedMemory->createSharedBuffer(buffer.size()));
    completionHandler(WTF::move(handle));
}

void RemoteMediaResourceLoader::accessControlCheckFailed(RemoteMediaResourceIdentifier identifier, const ResourceError& error)
{
    assertIsCurrent(defaultQueue());
    if (RefPtr resource = resourceForId(identifier))
        resource->accessControlCheckFailed(error);
}

void RemoteMediaResourceLoader::loadFailed(RemoteMediaResourceIdentifier identifier, const ResourceError& error)
{
    assertIsCurrent(defaultQueue());
    if (RefPtr resource = resourceForId(identifier))
        resource->loadFailed(error);
}

void RemoteMediaResourceLoader::loadFinished(RemoteMediaResourceIdentifier identifier, const NetworkLoadMetrics& metrics)
{
    assertIsCurrent(defaultQueue());
    if (RefPtr resource = resourceForId(identifier))
        resource->loadFinished(metrics);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
