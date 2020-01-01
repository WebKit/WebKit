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

#if ENABLE(GPU_PROCESS)

#include "RemoteMediaPlayerProxy.h"
#include "RemoteMediaResourceManager.h"
#include <WebCore/ResourceResponse.h>

namespace WebKit {

using namespace WebCore;

Ref<RemoteMediaResource> RemoteMediaResource::create(RemoteMediaResourceManager& remoteMediaResourceManager, RemoteMediaPlayerProxy& remoteMediaPlayerProxy, RemoteMediaResourceIdentifier id)
{
    return adoptRef(*new RemoteMediaResource(remoteMediaResourceManager, remoteMediaPlayerProxy, id));
}

RemoteMediaResource::RemoteMediaResource(RemoteMediaResourceManager& remoteMediaResourceManager, RemoteMediaPlayerProxy& remoteMediaPlayerProxy, RemoteMediaResourceIdentifier id)
    : m_remoteMediaResourceManager(remoteMediaResourceManager)
    , m_remoteMediaPlayerProxy(makeWeakPtr(remoteMediaPlayerProxy))
    , m_id(id)
{
}

RemoteMediaResource::~RemoteMediaResource()
{
    ASSERT(isMainThread());
    stop();
    m_remoteMediaResourceManager.removeMediaResource(m_id);
}

void RemoteMediaResource::stop()
{
    if (m_remoteMediaPlayerProxy)
        m_remoteMediaPlayerProxy->removeResource(m_id);
}

bool RemoteMediaResource::didPassAccessControlCheck() const
{
    return m_didPassAccessControlCheck;
}

void RemoteMediaResource::responseReceived(const ResourceResponse& response, bool didPassAccessControlCheck, CompletionHandler<void(PolicyChecker::ShouldContinue)>&& completionHandler)
{
    if (!m_client)
        return;

    m_didPassAccessControlCheck = didPassAccessControlCheck;
    m_client->responseReceived(*this, response, [protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](auto shouldContinue) mutable {
        ASSERT(isMainThread());
        if (shouldContinue == PolicyChecker::ShouldContinue::No)
            protectedThis->stop();

        completionHandler(shouldContinue);
    });
}

void RemoteMediaResource::redirectReceived(ResourceRequest&& request, const ResourceResponse& response, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    if (m_client)
        m_client->redirectReceived(*this, WTFMove(request), response, WTFMove(completionHandler));
}

void RemoteMediaResource::dataSent(uint64_t bytesSent, uint64_t totalBytesToBeSent)
{
    if (m_client)
        m_client->dataSent(*this, bytesSent, totalBytesToBeSent);
}

void RemoteMediaResource::dataReceived(const char* data, int64_t length)
{
    if (m_client)
        m_client->dataReceived(*this, data, length);
}

void RemoteMediaResource::accessControlCheckFailed(const ResourceError& error)
{
    m_didPassAccessControlCheck = false;
    if (m_client)
        m_client->accessControlCheckFailed(*this, error);
}

void RemoteMediaResource::loadFailed(const ResourceError& error)
{
    if (m_client)
        m_client->loadFailed(*this, error);
}

void RemoteMediaResource::loadFinished()
{
    if (m_client)
        m_client->loadFinished(*this);
}

} // namespace WebKit

#endif
