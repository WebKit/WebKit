/*
 * Copyright (C) 2020 Apple Inc.  All rights reserved.
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

#pragma once

#if ENABLE(GPU_PROCESS)

#include <WebCore/RenderingResourceIdentifier.h>
#include <wtf/HashMap.h>

namespace WebCore {
class ImageBuffer;
}

namespace WebKit {

class RemoteRenderingBackendProxy;

class RemoteResourceCacheProxy {
public:
    RemoteResourceCacheProxy(RemoteRenderingBackendProxy&);

    void cacheImageBuffer(WebCore::RenderingResourceIdentifier, WebCore::ImageBuffer*);
    WebCore::ImageBuffer* cachedImageBuffer(WebCore::RenderingResourceIdentifier);
    void releaseImageBuffer(WebCore::RenderingResourceIdentifier);

    bool lockRemoteImageBufferForRemoteClient(WebCore::ImageBuffer&, WebCore::RenderingResourceIdentifier remoteClientIdentifier);
    void unlockRemoteResourcesForRemoteClient(WebCore::RenderingResourceIdentifier remoteClientIdentifier);

private:
    using RemoteClientsHashSet = HashSet<WebCore::RenderingResourceIdentifier>;
    using RemoteResourceClientsHashMap = HashMap<WebCore::RenderingResourceIdentifier, RemoteClientsHashSet>;
    using RemoteImageBufferProxyHashMap = HashMap<WebCore::RenderingResourceIdentifier, WebCore::ImageBuffer*>;

    void lockRemoteResourceForRemoteClient(WebCore::RenderingResourceIdentifier, WebCore::RenderingResourceIdentifier remoteClientIdentifier);
    void releaseRemoteResource(WebCore::RenderingResourceIdentifier);

    RemoteImageBufferProxyHashMap m_imageBuffers;
    RemoteResourceClientsHashMap m_lockedRemoteResourceForRemoteClients;
    RemoteResourceClientsHashMap m_pendingUnlockRemoteResourceForRemoteClients;
    RemoteRenderingBackendProxy& m_remoteRenderingBackendProxy;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
