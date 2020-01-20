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
#include "DataReference.h"
#include "RemoteMediaResource.h"
#include "RemoteMediaResourceIdentifier.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/ResourceRequest.h>

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

void RemoteMediaResourceManager::responseReceived(RemoteMediaResourceIdentifier id, const ResourceResponse& response, bool didPassAccessControlCheck, CompletionHandler<void(PolicyChecker::ShouldContinue)>&& completionHandler)
{
    auto* resource = m_remoteMediaResources.get(id);
    if (!resource || !resource->ready()) {
        completionHandler(PolicyChecker::ShouldContinue::No);
        return;
    }

    m_remoteMediaResources.get(id)->responseReceived(response, didPassAccessControlCheck, WTFMove(completionHandler));
}

void RemoteMediaResourceManager::redirectReceived(RemoteMediaResourceIdentifier id, ResourceRequest&& request, const ResourceResponse& response, CompletionHandler<void(WebCore::ResourceRequest&&)>&& completionHandler)
{
    auto* resource = m_remoteMediaResources.get(id);
    if (!resource || !resource->ready()) {
        completionHandler({ });
        return;
    }

    m_remoteMediaResources.get(id)->redirectReceived(WTFMove(request), response, WTFMove(completionHandler));
}

void RemoteMediaResourceManager::dataSent(RemoteMediaResourceIdentifier id, uint64_t bytesSent, uint64_t totalBytesToBeSent)
{
    auto* resource = m_remoteMediaResources.get(id);
    if (!resource || !resource->ready())
        return;

    m_remoteMediaResources.get(id)->dataSent(bytesSent, totalBytesToBeSent);
}

void RemoteMediaResourceManager::dataReceived(RemoteMediaResourceIdentifier id, const IPC::DataReference& data)
{
    auto* resource = m_remoteMediaResources.get(id);
    if (!resource || !resource->ready())
        return;

    m_remoteMediaResources.get(id)->dataReceived(reinterpret_cast<const char*>(data.data()), data.size());
}

void RemoteMediaResourceManager::accessControlCheckFailed(RemoteMediaResourceIdentifier id, const ResourceError& error)
{
    auto* resource = m_remoteMediaResources.get(id);
    if (!resource || !resource->ready())
        return;

    m_remoteMediaResources.get(id)->accessControlCheckFailed(error);
}

void RemoteMediaResourceManager::loadFailed(RemoteMediaResourceIdentifier id, const ResourceError& error)
{
    auto* resource = m_remoteMediaResources.get(id);
    if (!resource || !resource->ready())
        return;

    m_remoteMediaResources.get(id)->loadFailed(error);
}

void RemoteMediaResourceManager::loadFinished(RemoteMediaResourceIdentifier id)
{
    auto* resource = m_remoteMediaResources.get(id);
    if (!resource || !resource->ready())
        return;

    m_remoteMediaResources.get(id)->loadFinished();
}

} // namespace WebKit

#endif
