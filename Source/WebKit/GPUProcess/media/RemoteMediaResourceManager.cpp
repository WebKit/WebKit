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
#include "RemoteMediaResourceManager.h"

#if ENABLE(GPU_PROCESS)

#include "Connection.h"
#include "RemoteMediaResource.h"
#include "RemoteMediaResourceIdentifier.h"
#include "SharedBufferReference.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/ResourceRequest.h>
#include <wtf/Scope.h>

namespace WebKit {

using namespace WebCore;

RemoteMediaResourceManager::RemoteMediaResourceManager()
{
}

RemoteMediaResourceManager::~RemoteMediaResourceManager()
{
}

void RemoteMediaResourceManager::addMediaResource(RemoteMediaResourceIdentifier remoteMediaResourceIdentifier, RemoteMediaResource& remoteMediaResource)
{
    ASSERT(!m_remoteMediaResources.contains(remoteMediaResourceIdentifier));
    m_remoteMediaResources.add(remoteMediaResourceIdentifier, &remoteMediaResource);
}

void RemoteMediaResourceManager::removeMediaResource(RemoteMediaResourceIdentifier remoteMediaResourceIdentifier)
{
    ASSERT(m_remoteMediaResources.contains(remoteMediaResourceIdentifier));
    m_remoteMediaResources.remove(remoteMediaResourceIdentifier);
}

void RemoteMediaResourceManager::responseReceived(RemoteMediaResourceIdentifier identifier, const ResourceResponse& response, bool didPassAccessControlCheck, CompletionHandler<void(ShouldContinuePolicyCheck)>&& completionHandler)
{
    auto* resource = m_remoteMediaResources.get(identifier);
    if (!resource) {
        completionHandler(ShouldContinuePolicyCheck::No);
        return;
    }

    resource->responseReceived(response, didPassAccessControlCheck, WTFMove(completionHandler));
}

void RemoteMediaResourceManager::redirectReceived(RemoteMediaResourceIdentifier identifier, ResourceRequest&& request, const ResourceResponse& response, CompletionHandler<void(WebCore::ResourceRequest&&)>&& completionHandler)
{
    auto* resource = m_remoteMediaResources.get(identifier);
    if (!resource) {
        completionHandler({ });
        return;
    }

    resource->redirectReceived(WTFMove(request), response, WTFMove(completionHandler));
}

void RemoteMediaResourceManager::dataSent(RemoteMediaResourceIdentifier identifier, uint64_t bytesSent, uint64_t totalBytesToBeSent)
{
    auto* resource = m_remoteMediaResources.get(identifier);
    if (!resource)
        return;

    resource->dataSent(bytesSent, totalBytesToBeSent);
}

void RemoteMediaResourceManager::dataReceived(RemoteMediaResourceIdentifier identifier, IPC::SharedBufferReference&& buffer, CompletionHandler<void(std::optional<SharedMemory::Handle>&&)>&& completionHandler)
{
    auto* resource = m_remoteMediaResources.get(identifier);
    if (!resource)
        return completionHandler(std::nullopt);

    auto sharedMemory = buffer.sharedCopy();
    if (!sharedMemory)
        return completionHandler(std::nullopt);

    auto handle = sharedMemory->createHandle(SharedMemory::Protection::ReadOnly);
    if (!handle)
        return completionHandler(std::nullopt);

    resource->dataReceived(sharedMemory->createSharedBuffer(buffer.size()));
    completionHandler(WTFMove(handle));
}

void RemoteMediaResourceManager::accessControlCheckFailed(RemoteMediaResourceIdentifier identifier, const ResourceError& error)
{
    auto* resource = m_remoteMediaResources.get(identifier);
    if (!resource)
        return;

    resource->accessControlCheckFailed(error);
}

void RemoteMediaResourceManager::loadFailed(RemoteMediaResourceIdentifier identifier, const ResourceError& error)
{
    auto* resource = m_remoteMediaResources.get(identifier);
    if (!resource)
        return;

    resource->loadFailed(error);
}

void RemoteMediaResourceManager::loadFinished(RemoteMediaResourceIdentifier identifier, const NetworkLoadMetrics& metrics)
{
    auto* resource = m_remoteMediaResources.get(identifier);
    if (!resource)
        return;

    resource->loadFinished(metrics);
}

} // namespace WebKit

#endif
