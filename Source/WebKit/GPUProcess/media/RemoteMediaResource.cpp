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
#include "RemoteMediaResource.h"

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "RemoteMediaPlayerProxy.h"
#include "RemoteMediaResourceLoader.h"
#include <WebCore/ResourceResponse.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMediaResource);

using namespace WebCore;

Ref<RemoteMediaResource> RemoteMediaResource::create(RemoteMediaResourceLoader& loader, RemoteMediaResourceIdentifier identifier)
{
    return adoptRef(*new RemoteMediaResource(loader, identifier));
}

RemoteMediaResource::RemoteMediaResource(RemoteMediaResourceLoader& loader, RemoteMediaResourceIdentifier identifier)
    : m_remoteMediaResourceLoader(loader)
    , m_id(identifier)
{
}

RemoteMediaResource::~RemoteMediaResource()
{
    ASSERT(m_shutdown);
}

void RemoteMediaResource::shutdown()
{
    // This call must be thread-safe, so protect against simultaneous calls
    // on multiple threads and dispatch to the loader's queue if necessary.
    if (m_shutdown.exchange(true))
        return;

    setClient(nullptr);

    RefPtr loader = m_remoteMediaResourceLoader.get();
    if (!loader)
        return;

    if (RemoteMediaResourceLoader::defaultQueue()->isCurrent()) {
        loader->removeMediaResource(m_id);
        return;
    }

    RemoteMediaResourceLoader::defaultQueue()->dispatchSync([loader = WTF::move(loader), id = m_id] {
        loader->removeMediaResource(id);
    });
}

bool RemoteMediaResource::didPassAccessControlCheck() const
{
    return m_didPassAccessControlCheck;
}

void RemoteMediaResource::responseReceived(const ResourceResponse& response, bool didPassAccessControlCheck, CompletionHandler<void(ShouldContinuePolicyCheck)>&& completionHandler)
{
    assertIsCurrent(RemoteMediaResourceLoader::defaultQueue());

    auto client = this->client();
    if (!client)
        return completionHandler(ShouldContinuePolicyCheck::No);

    m_didPassAccessControlCheck = didPassAccessControlCheck;
    client->responseReceived(*this, response, [protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)](auto shouldContinue) mutable {
        if (shouldContinue == ShouldContinuePolicyCheck::No)
            protectedThis->shutdown();

        completionHandler(shouldContinue);
    });
}

void RemoteMediaResource::redirectReceived(ResourceRequest&& request, const ResourceResponse& response, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    assertIsCurrent(RemoteMediaResourceLoader::defaultQueue());

    if (auto client = this->client())
        client->redirectReceived(*this, WTF::move(request), response, WTF::move(completionHandler));
}

void RemoteMediaResource::dataSent(uint64_t bytesSent, uint64_t totalBytesToBeSent)
{
    assertIsCurrent(RemoteMediaResourceLoader::defaultQueue());

    if (auto client = this->client())
        client->dataSent(*this, bytesSent, totalBytesToBeSent);
}

void RemoteMediaResource::dataReceived(const SharedBuffer& data)
{
    assertIsCurrent(RemoteMediaResourceLoader::defaultQueue());

    if (auto client = this->client())
        client->dataReceived(*this, data);
}

void RemoteMediaResource::accessControlCheckFailed(const ResourceError& error)
{
    assertIsCurrent(RemoteMediaResourceLoader::defaultQueue());

    m_didPassAccessControlCheck = false;
    if (auto client = this->client())
        client->accessControlCheckFailed(*this, error);
}

void RemoteMediaResource::loadFailed(const ResourceError& error)
{
    assertIsCurrent(RemoteMediaResourceLoader::defaultQueue());

    if (auto client = this->client())
        client->loadFailed(*this, error);
}

void RemoteMediaResource::loadFinished(const NetworkLoadMetrics& metrics)
{
    assertIsCurrent(RemoteMediaResourceLoader::defaultQueue());

    if (auto client = this->client())
        client->loadFinished(*this, metrics);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
