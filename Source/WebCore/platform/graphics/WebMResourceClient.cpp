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
#include "WebMResourceClient.h"

#if ENABLE(ALTERNATE_WEBM_PLAYER)

#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"

namespace WebCore {

RefPtr<WebMResourceClient> WebMResourceClient::create(WebMResourceClientParent& parent, PlatformMediaResourceLoader& loader, ResourceRequest&& request)
{
    auto resource = loader.requestResource(WTFMove(request), PlatformMediaResourceLoader::LoadOption::DisallowCaching);
    if (!resource)
        return nullptr;
    auto* resourcePointer = resource.get();
    auto client = adoptRef(*new WebMResourceClient { parent, resource.releaseNonNull() });
    auto result = client.copyRef();
    resourcePointer->setClient(WTFMove(client));
    return result;
}

WebMResourceClient::WebMResourceClient(WebMResourceClientParent& parent, Ref<PlatformMediaResource>&& resource)
    : m_parent(parent)
    , m_resource(WTFMove(resource))
{
}

void WebMResourceClient::stop()
{
    if (!m_resource)
        return;

    auto resource = WTFMove(m_resource);
    resource->stop();
    resource->setClient(nullptr);
}

void WebMResourceClient::dataReceived(PlatformMediaResource&, const SharedBuffer& buffer)
{
    if (!m_parent)
        return;
    
    m_buffer.append(buffer);
    m_parent->dataReceived(buffer);
}

void WebMResourceClient::loadFailed(PlatformMediaResource&, const ResourceError& error)
{
    if (!m_parent)
        return;
    
    m_parent->loadFailed(error);
}

void WebMResourceClient::loadFinished(PlatformMediaResource&, const NetworkLoadMetrics&)
{
    if (!m_parent)
        return;
    
    m_parent->loadFinished(*m_buffer.get());
}

} // namespace WebCore

#endif // ENABLE(ALTERNATE_WEBM_PLAYER)
