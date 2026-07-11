/*
 * Copyright (C) 2026 Shopify Inc. All rights reserved.
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
#include "EarlyHintsPreloadTask.h"

#include "EarlyHintsPreloadCache.h"
#include "NetworkLoad.h"
#include "NetworkLoadParameters.h"
#include "NetworkSession.h"
#include <WebCore/NetworkLoadMetrics.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceLoaderOptions.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SharedBuffer.h>

namespace WebKit {
using namespace WebCore;

static constexpr size_t maxPreloadBodySize { 5 * 1024 * 1024 };

Ref<EarlyHintsPreloadTask> EarlyHintsPreloadTask::create(NetworkSession& networkSession, NetworkLoadParameters&& parameters, NetworkCache::GlobalFrameID frameID, SecurityOriginData&& hintingOrigin, URL&& url, String&& destination, FetchOptions::Mode mode, StoredCredentialsPolicy storedCredentialsPolicy, String&& cachePartition)
{
    return adoptRef(*new EarlyHintsPreloadTask(networkSession, WTF::move(parameters), frameID, WTF::move(hintingOrigin), WTF::move(url), WTF::move(destination), mode, storedCredentialsPolicy, WTF::move(cachePartition)));
}

EarlyHintsPreloadTask::EarlyHintsPreloadTask(NetworkSession& networkSession, NetworkLoadParameters&& parameters, NetworkCache::GlobalFrameID frameID, SecurityOriginData&& hintingOrigin, URL&& url, String&& destination, FetchOptions::Mode mode, StoredCredentialsPolicy storedCredentialsPolicy, String&& cachePartition)
    : m_networkLoad(NetworkLoad::create(*this, WTF::move(parameters), networkSession))
    , m_session(&networkSession)
    , m_frameID(frameID)
    , m_hintingOrigin(WTF::move(hintingOrigin))
    , m_url(WTF::move(url))
    , m_destination(WTF::move(destination))
    , m_mode(mode)
    , m_storedCredentialsPolicy(storedCredentialsPolicy)
    , m_cachePartition(WTF::move(cachePartition))
{
}

EarlyHintsPreloadTask::~EarlyHintsPreloadTask() = default;

void EarlyHintsPreloadTask::start()
{
    m_selfKeepAlive = this;
    m_networkLoad->start();
}

void EarlyHintsPreloadTask::willSendRedirectedRequest(ResourceRequest&& request, ResourceRequest&& redirectRequest, ResourceResponse&&, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    // Unlike a normal load, this task has no NetworkLoadChecker to validate a cross-origin redirect
    // (CORS, credentials, tainting), so abandon the preload on one and let the consumer's own,
    // fully-checked load follow it. Same-origin redirects are safe to follow.
    if (!protocolHostAndPortAreEqual(request.url(), redirectRequest.url())) {
        complete();
        completionHandler({ });
        return;
    }
    completionHandler(WTF::move(redirectRequest));
}

void EarlyHintsPreloadTask::didReceiveResponse(ResourceResponse&& response, PrivateRelayed privateRelayed, ResponseCompletionHandler&& completionHandler)
{
    // Only a successful response is worth parking; otherwise abandon the preload.
    if (response.httpStatusCode() < 200 || response.httpStatusCode() >= 300) {
        completionHandler(PolicyAction::Ignore);
        complete();
        return;
    }
    m_response = WTF::move(response);
    m_privateRelayed = privateRelayed;
    completionHandler(PolicyAction::Use);
}

void EarlyHintsPreloadTask::didReceiveBuffer(const FragmentedSharedBuffer& buffer)
{
    // Abandon the preload if its body exceeds the cap.
    if (m_buffer.size() + buffer.size() > maxPreloadBodySize) {
        m_networkLoad->cancel();
        complete();
        return;
    }
    m_buffer.append(buffer);
}

void EarlyHintsPreloadTask::didFinishLoading(const NetworkLoadMetrics&)
{
    if (CheckedPtr session = m_session) {
        RefPtr<FragmentedSharedBuffer> body = m_buffer.takeBuffer();
        session->earlyHintsPreloadCache()->store(m_frameID, m_hintingOrigin, m_url, WTF::move(m_destination), m_mode, m_storedCredentialsPolicy, String { m_cachePartition }, WTF::move(m_response), m_privateRelayed, WTF::move(body));
    }
    complete();
}

void EarlyHintsPreloadTask::didFailLoading(const ResourceError&)
{
    complete();
}

void EarlyHintsPreloadTask::complete()
{
    if (!m_selfKeepAlive)
        return;
    m_networkLoad->clearClient();
    m_selfKeepAlive = nullptr;
}

} // namespace WebKit
