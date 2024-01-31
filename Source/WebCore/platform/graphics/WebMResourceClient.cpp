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
    auto client = adoptRef(*new WebMResourceClient { parent, Ref { *resource } });
    auto result = client.copyRef();
    resource->setClient(WTFMove(client));
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
    resource->shutdown();
}

void WebMResourceClient::dataReceived(PlatformMediaResource&, const SharedBuffer& buffer)
{
    if (RefPtr parent = m_parent.get())
        parent->dataReceived(buffer);
}

void WebMResourceClient::loadFailed(PlatformMediaResource&, const ResourceError& error)
{
    if (RefPtr parent = m_parent.get())
        parent->loadFailed(error);
}

void WebMResourceClient::loadFinished(PlatformMediaResource&, const NetworkLoadMetrics&)
{
    if (RefPtr parent = m_parent.get())
        parent->loadFinished();
}

} // namespace WebCore

#endif // ENABLE(ALTERNATE_WEBM_PLAYER)
