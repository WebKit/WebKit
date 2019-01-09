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
#include "NetworkSession.h"
#include "SessionTracker.h"
#include <WebCore/NetworkStorageSession.h>
#include <pal/SessionID.h>
#include <wtf/RunLoop.h>

namespace WebKit {
namespace NetworkCache {

using namespace WebCore;

SpeculativeLoad::SpeculativeLoad(Cache& cache, const GlobalFrameID& globalFrameID, const ResourceRequest& request, std::unique_ptr<NetworkCache::Entry> cacheEntryForValidation, RevalidationCompletionHandler&& completionHandler)
    : m_cache(cache)
    , m_globalFrameID(globalFrameID)
    , m_completionHandler(WTFMove(completionHandler))
    , m_originalRequest(request)
    , m_bufferedDataForCache(SharedBuffer::create())
    , m_cacheEntry(WTFMove(cacheEntryForValidation))
{
    ASSERT(!m_cacheEntry || m_cacheEntry->needsValidation());

    NetworkLoadParameters parameters;
    parameters.webPageID = globalFrameID.first;
    parameters.webFrameID = globalFrameID.second;
    parameters.sessionID = PAL::SessionID::defaultSessionID();
    parameters.storedCredentialsPolicy = StoredCredentialsPolicy::Use;
    parameters.contentSniffingPolicy = ContentSniffingPolicy::DoNotSniffContent;
    parameters.contentEncodingSniffingPolicy = ContentEncodingSniffingPolicy::Sniff;
    parameters.request = m_originalRequest;
    m_networkLoad = std::make_unique<NetworkLoad>(*this, WTFMove(parameters), *SessionTracker::networkSession(PAL::SessionID::defaultSessionID()));
}

SpeculativeLoad::~SpeculativeLoad()
{
    ASSERT(!m_networkLoad);
}

void SpeculativeLoad::willSendRedirectedRequest(ResourceRequest&& request, ResourceRequest&& redirectRequest, ResourceResponse&& redirectResponse)
{
    LOG(NetworkCacheSpeculativePreloading, "Speculative redirect %s -> %s", request.url().string().utf8().data(), redirectRequest.url().string().utf8().data());

    Optional<Seconds> maxAgeCap;
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (auto networkStorageSession = WebCore::NetworkStorageSession::storageSession(PAL::SessionID::defaultSessionID()))
        maxAgeCap = networkStorageSession->maxAgeCacheCap(request);
#endif
    m_cacheEntry = m_cache->storeRedirect(request, redirectResponse, redirectRequest, maxAgeCap);
    // Create a synthetic cache entry if we can't store.
    if (!m_cacheEntry)
        m_cacheEntry = m_cache->makeRedirectEntry(request, redirectResponse, redirectRequest);

    // Don't follow the redirect. The redirect target will be registered for speculative load when it is loaded.
    didComplete();
}

void SpeculativeLoad::didReceiveResponse(ResourceResponse&& receivedResponse, ResponseCompletionHandler&& completionHandler)
{
    m_response = receivedResponse;

    if (m_response.isMultipart())
        m_bufferedDataForCache = nullptr;

    bool validationSucceeded = m_response.httpStatusCode() == 304; // 304 Not Modified
    if (validationSucceeded && m_cacheEntry)
        m_cacheEntry = m_cache->update(m_originalRequest, m_globalFrameID, *m_cacheEntry, m_response);
    else
        m_cacheEntry = nullptr;

    completionHandler(PolicyAction::Use);
}

void SpeculativeLoad::didReceiveBuffer(Ref<SharedBuffer>&& buffer, int reportedEncodedDataLength)
{
    ASSERT(!m_cacheEntry);

    if (m_bufferedDataForCache) {
        // Prevent memory growth in case of streaming data.
        const size_t maximumCacheBufferSize = 10 * 1024 * 1024;
        if (m_bufferedDataForCache->size() + buffer->size() <= maximumCacheBufferSize)
            m_bufferedDataForCache->append(buffer.get());
        else
            m_bufferedDataForCache = nullptr;
    }
}

void SpeculativeLoad::didFinishLoading(const WebCore::NetworkLoadMetrics&)
{
    if (m_didComplete)
        return;
    if (!m_cacheEntry && m_bufferedDataForCache) {
        m_cacheEntry = m_cache->store(m_originalRequest, m_response, m_bufferedDataForCache.copyRef(), [](auto& mappedBody) { });
        // Create a synthetic cache entry if we can't store.
        if (!m_cacheEntry && isStatusCodeCacheableByDefault(m_response.httpStatusCode()))
            m_cacheEntry = m_cache->makeEntry(m_originalRequest, m_response, WTFMove(m_bufferedDataForCache));
    }

    didComplete();
}

void SpeculativeLoad::didFailLoading(const ResourceError&)
{
    if (m_didComplete)
        return;
    m_cacheEntry = nullptr;

    didComplete();
}

void SpeculativeLoad::didComplete()
{
    RELEASE_ASSERT(RunLoop::isMain());

    if (m_didComplete)
        return;
    m_didComplete = true;
    m_networkLoad = nullptr;

    // Make sure speculatively revalidated resources do not get validated by the NetworkResourceLoader again.
    if (m_cacheEntry)
        m_cacheEntry->setNeedsValidation(false);

    m_completionHandler(WTFMove(m_cacheEntry));
}

} // namespace NetworkCache
} // namespace WebKit

#endif // ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
