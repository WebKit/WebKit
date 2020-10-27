/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "RemoteResourceCacheProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteRenderingBackendProxy.h"

namespace WebKit {
using namespace WebCore;

RemoteResourceCacheProxy::RemoteResourceCacheProxy(RemoteRenderingBackendProxy& remoteRenderingBackendProxy)
    : m_remoteRenderingBackendProxy(remoteRenderingBackendProxy)
{
}

void RemoteResourceCacheProxy::cacheImageBuffer(RenderingResourceIdentifier renderingResourceIdentifier, WebCore::ImageBuffer* imageBuffer)
{
    auto addResult = m_imageBuffers.add(renderingResourceIdentifier, imageBuffer);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

ImageBuffer* RemoteResourceCacheProxy::cachedImageBuffer(RenderingResourceIdentifier renderingResourceIdentifier)
{
    return m_imageBuffers.get(renderingResourceIdentifier);
}

void RemoteResourceCacheProxy::releaseImageBuffer(RenderingResourceIdentifier renderingResourceIdentifier)
{
    bool found = m_imageBuffers.remove(renderingResourceIdentifier);
    ASSERT_UNUSED(found, found);

    // Tell the GPU process to remove this resource unless it's pending replaying back.
    releaseRemoteResource(renderingResourceIdentifier);
}

bool RemoteResourceCacheProxy::lockRemoteImageBufferForRemoteClient(ImageBuffer& imageBuffer, RenderingResourceIdentifier remoteClientIdentifier)
{
    if (!imageBuffer.renderingResourceIdentifier())
        return false;

    if (cachedImageBuffer(imageBuffer.renderingResourceIdentifier()) != &imageBuffer) {
        ASSERT_NOT_REACHED();
        return false;
    }

    lockRemoteResourceForRemoteClient(imageBuffer.renderingResourceIdentifier(), remoteClientIdentifier);
    return true;
}

void RemoteResourceCacheProxy::lockRemoteResourceForRemoteClient(RenderingResourceIdentifier renderingResourceIdentifier, RenderingResourceIdentifier remoteClientIdentifier)
{
    // Add the "resource -> client" dependency in m_lockedRemoteResourceForRemoteClients.
    auto& clients = m_lockedRemoteResourceForRemoteClients.ensure(renderingResourceIdentifier, [&] {
        return RemoteClientsHashSet();
    }).iterator->value;

    clients.add(remoteClientIdentifier);
}

void RemoteResourceCacheProxy::releaseRemoteResource(RenderingResourceIdentifier renderingResourceIdentifier)
{
    ASSERT(!m_pendingUnlockRemoteResourceForRemoteClients.contains(renderingResourceIdentifier));

    // If we have recorded resource -> client dependency before in m_lockedRemoteResourceForRemoteClients, move this
    // dependency to m_pendingUnlockRemoteResourceForRemoteClients. Otherwise remove the resource in the GPU Process.
    if (m_lockedRemoteResourceForRemoteClients.contains(renderingResourceIdentifier))
        m_pendingUnlockRemoteResourceForRemoteClients.add(renderingResourceIdentifier, m_lockedRemoteResourceForRemoteClients.take(renderingResourceIdentifier));
    else
        m_remoteRenderingBackendProxy.releaseRemoteResource(renderingResourceIdentifier);
}

void RemoteResourceCacheProxy::unlockRemoteResourcesForRemoteClient(RenderingResourceIdentifier remoteClientIdentifier)
{
    auto removeRemoteClient = [&](RemoteResourceClientsHashMap& remoteResourceForRemoteClients, RenderingResourceIdentifier remoteClientIdentifier) {
        Vector<RenderingResourceIdentifier> remoteResourceClientsToBeRemoved;

        // Remove all the resource -> client dependencies from remoteResourceClientsHashMap.
        for (auto& pair : remoteResourceForRemoteClients) {
            pair.value.remove(remoteClientIdentifier);
            if (pair.value.isEmpty())
                remoteResourceClientsToBeRemoved.append(pair.key);
        }

        // Return a list of the unreferenced resources.
        return remoteResourceClientsToBeRemoved;
    };

    // Remove all the resource -> client dependencies from m_lockedRemoteResourceForRemoteClients.
    for (auto renderingResourceIdentifier : removeRemoteClient(m_lockedRemoteResourceForRemoteClients, remoteClientIdentifier))
        m_lockedRemoteResourceForRemoteClients.remove(renderingResourceIdentifier);

    // Remove all the resource -> client dependencies from m_pendingUnlockRemoteResourceForRemoteClients.
    for (auto renderingResourceIdentifier : removeRemoteClient(m_pendingUnlockRemoteResourceForRemoteClients, remoteClientIdentifier)) {
        // Remove the unreferenced resources from m_remoteResourceClientsPendingReleaseHashMap.
        m_pendingUnlockRemoteResourceForRemoteClients.remove(renderingResourceIdentifier);

        // Tell the GPU process to remove this resource.
        m_remoteRenderingBackendProxy.releaseRemoteResource(renderingResourceIdentifier);
    }
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
