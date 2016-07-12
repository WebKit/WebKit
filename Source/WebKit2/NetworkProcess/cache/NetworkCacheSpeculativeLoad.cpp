/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
#include "NetworkCacheSpeculativeLoad.h"

#include "Logging.h"
#include "NetworkCache.h"
#include "NetworkLoad.h"
#include <WebCore/SessionID.h>
#include <wtf/CurrentTime.h>
#include <wtf/RunLoop.h>

namespace WebKit {
namespace NetworkCache {

using namespace WebCore;

SpeculativeLoad::SpeculativeLoad(const GlobalFrameID& frameID, const ResourceRequest& request, std::unique_ptr<NetworkCache::Entry> cacheEntryForValidation, RevalidationCompletionHandler&& completionHandler)
    : m_frameID(frameID)
    , m_completionHandler(WTFMove(completionHandler))
    , m_originalRequest(request)
    , m_bufferedDataForCache(SharedBuffer::create())
    , m_cacheEntryForValidation(WTFMove(cacheEntryForValidation))
{
    ASSERT(m_cacheEntryForValidation);
    ASSERT(m_cacheEntryForValidation->needsValidation());

    NetworkLoadParameters parameters;
    parameters.sessionID = SessionID::defaultSessionID();
    parameters.allowStoredCredentials = AllowStoredCredentials;
    parameters.contentSniffingPolicy = DoNotSniffContent;
    parameters.request = m_originalRequest;
#if USE(NETWORK_SESSION)
    m_networkLoad = std::make_unique<NetworkLoad>(*this, WTFMove(parameters), NetworkSession::defaultSession());
#else
    m_networkLoad = std::make_unique<NetworkLoad>(*this, WTFMove(parameters));
#endif
}

SpeculativeLoad::~SpeculativeLoad()
{
    ASSERT(!m_networkLoad);
}

void SpeculativeLoad::willSendRedirectedRequest(ResourceRequest&&, ResourceRequest&& redirectRequest, ResourceResponse&& redirectResponse)
{
    LOG(NetworkCacheSpeculativePreloading, "(NetworkProcess) Speculative revalidation for %s hit a redirect, aborting the load.", redirectResponse.url().string().utf8().data());
    // We drop speculative revalidations if they redirect for now as we would need to notify WebCore of such redirects.
    abort();
}

auto SpeculativeLoad::didReceiveResponse(ResourceResponse&& receivedResponse) -> ShouldContinueDidReceiveResponse
{
    m_response = receivedResponse;

    if (m_response.isMultipart())
        m_bufferedDataForCache = nullptr;

    ASSERT(m_cacheEntryForValidation);

    bool validationSucceeded = m_response.httpStatusCode() == 304; // 304 Not Modified
    if (validationSucceeded)
        m_cacheEntryForValidation = NetworkCache::singleton().update(m_originalRequest, m_frameID, *m_cacheEntryForValidation, m_response);
    else
        m_cacheEntryForValidation = nullptr;

    return ShouldContinueDidReceiveResponse::Yes;
}

void SpeculativeLoad::didReceiveBuffer(Ref<SharedBuffer>&& buffer, int reportedEncodedDataLength)
{
    ASSERT(!m_cacheEntryForValidation);

    if (m_bufferedDataForCache) {
        // Prevent memory growth in case of streaming data.
        const size_t maximumCacheBufferSize = 10 * 1024 * 1024;
        if (m_bufferedDataForCache->size() + buffer->size() <= maximumCacheBufferSize)
            m_bufferedDataForCache->append(buffer.get());
        else
            m_bufferedDataForCache = nullptr;
    }
}

void SpeculativeLoad::didFinishLoading(double finishTime)
{
    if (!m_cacheEntryForValidation && m_bufferedDataForCache)
        m_cacheEntryForValidation = NetworkCache::singleton().store(m_originalRequest, m_response, WTFMove(m_bufferedDataForCache), [](auto& mappedBody) { });

    didComplete();
}

void SpeculativeLoad::didFailLoading(const ResourceError&)
{
    m_cacheEntryForValidation = nullptr;

    didComplete();
}

void SpeculativeLoad::abort()
{
    if (m_networkLoad)
        m_networkLoad->cancel();

    m_cacheEntryForValidation = nullptr;
    didComplete();
}

void SpeculativeLoad::didComplete()
{
    RELEASE_ASSERT(RunLoop::isMain());

    m_networkLoad = nullptr;

    // Make sure speculatively revalidated resources do not get validated by the NetworkResourceLoader again.
    if (m_cacheEntryForValidation)
        m_cacheEntryForValidation->setNeedsValidation(false);

    m_completionHandler(WTFMove(m_cacheEntryForValidation));
}

} // namespace NetworkCache
} // namespace WebKit

#endif // ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
