/*
 * Copyright (C) 2019-2025 Apple Inc. All rights reserved.
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
#include "RemoteMediaResourceProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "RemoteMediaResourceLoaderProxy.h"
#include "SharedBufferReference.h"
#include <wtf/CompletionHandler.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMediaResourceProxy);

RemoteMediaResourceProxy::RemoteMediaResourceProxy(Ref<RemoteMediaResourceLoaderProxy>&& loader, WebCore::PlatformMediaResource& platformMediaResource, RemoteMediaResourceIdentifier identifier)
    : m_loader(WTF::move(loader))
    , m_platformMediaResource(platformMediaResource)
    , m_id(identifier)
{
}

RemoteMediaResourceProxy::~RemoteMediaResourceProxy() = default;

Ref<WebCore::PlatformMediaResource> RemoteMediaResourceProxy::mediaResource() const
{
    return m_platformMediaResource.get();
}

void RemoteMediaResourceProxy::responseReceived(WebCore::PlatformMediaResource&, const WebCore::ResourceResponse& response, CompletionHandler<void(WebCore::ShouldContinuePolicyCheck)>&& completionHandler)
{
    m_loader->responseReceived(m_id, response, mediaResource()->didPassAccessControlCheck(), WTF::move(completionHandler));
}

void RemoteMediaResourceProxy::redirectReceived(WebCore::PlatformMediaResource&, WebCore::ResourceRequest&& request, const WebCore::ResourceResponse& response, CompletionHandler<void(WebCore::ResourceRequest&&)>&& completionHandler)
{
    m_loader->redirectReceived(m_id, WTF::move(request), response, WTF::move(completionHandler));
}

bool RemoteMediaResourceProxy::shouldCacheResponse(WebCore::PlatformMediaResource&, const WebCore::ResourceResponse&)
{
    // TODO: need to check WebCoreNSURLSessionDataTaskClient::shouldCacheResponse()
    return false;
}

void RemoteMediaResourceProxy::dataSent(WebCore::PlatformMediaResource&, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    m_loader->dataSent(m_id, bytesSent, totalBytesToBeSent);
}

void RemoteMediaResourceProxy::dataReceived(WebCore::PlatformMediaResource&, const WebCore::SharedBuffer& buffer)
{
    m_loader->dataReceived(m_id, buffer);
}

void RemoteMediaResourceProxy::accessControlCheckFailed(WebCore::PlatformMediaResource&, const WebCore::ResourceError& error)
{
    m_loader->accessControlCheckFailed(m_id, error);
}

void RemoteMediaResourceProxy::loadFailed(WebCore::PlatformMediaResource&, const WebCore::ResourceError& error)
{
    m_loader->loadFailed(m_id, error);
}

void RemoteMediaResourceProxy::loadFinished(WebCore::PlatformMediaResource&, const WebCore::NetworkLoadMetrics& metrics)
{
    m_loader->loadFinished(m_id, metrics);
}

}

#endif // ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
