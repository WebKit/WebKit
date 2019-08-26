/*
 * Copyright (C) 2019 Igalia S.L.
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
#include "PrefetchCache.h"

#include <WebCore/HTTPHeaderNames.h>

namespace WebKit {

PrefetchCache::Entry::Entry(WebCore::ResourceResponse&& response, RefPtr<WebCore::SharedBuffer>&& buffer)
    : response(WTFMove(response)), buffer(WTFMove(buffer))
{
}

PrefetchCache::Entry::Entry(WebCore::ResourceResponse&& redirectResponse, WebCore::ResourceRequest&& redirectRequest)
    : response(WTFMove(redirectResponse)), redirectRequest(WTFMove(redirectRequest))
{
}

PrefetchCache::PrefetchCache()
    : m_expirationTimer(*this, &PrefetchCache::clearExpiredEntries)
{
}

PrefetchCache::~PrefetchCache()
{
}

void PrefetchCache::clear()
{
    m_expirationTimer.stop();
    m_sessionExpirationList.clear();
    if (m_sessionPrefetches)
        m_sessionPrefetches->clear();
}

std::unique_ptr<PrefetchCache::Entry> PrefetchCache::take(const URL& url)
{
    auto* resources = m_sessionPrefetches.get();
    if (!resources)
        return nullptr;
    m_sessionExpirationList.removeAllMatching([&url] (const auto& tuple) {
        return std::get<0>(tuple) == url;
    });
    return resources->take(url);
}

static const Seconds expirationTimeout { 5_s };

void PrefetchCache::store(const URL& requestUrl, WebCore::ResourceResponse&& response, RefPtr<WebCore::SharedBuffer>&& buffer)
{
    if (!m_sessionPrefetches)
        m_sessionPrefetches = makeUnique<PrefetchEntriesMap>();
    auto addResult = m_sessionPrefetches->add(requestUrl, makeUnique<PrefetchCache::Entry>(WTFMove(response), WTFMove(buffer)));
    // Limit prefetches for same url to 1.
    if (!addResult.isNewEntry)
        return;
    m_sessionExpirationList.append(std::make_tuple(requestUrl, WallTime::now()));
    if (!m_expirationTimer.isActive())
        m_expirationTimer.startOneShot(expirationTimeout);
}

void PrefetchCache::storeRedirect(const URL& requestUrl, WebCore::ResourceResponse&& redirectResponse, WebCore::ResourceRequest&& redirectRequest)
{
    if (!m_sessionPrefetches)
        m_sessionPrefetches = makeUnique<PrefetchEntriesMap>();
    redirectRequest.clearPurpose();
    m_sessionPrefetches->set(requestUrl, makeUnique<PrefetchCache::Entry>(WTFMove(redirectResponse), WTFMove(redirectRequest)));
    m_sessionExpirationList.append(std::make_tuple(requestUrl, WallTime::now()));
    if (!m_expirationTimer.isActive())
        m_expirationTimer.startOneShot(expirationTimeout);
}

void PrefetchCache::clearExpiredEntries()
{
    auto timeout = WallTime::now();
    while (!m_sessionExpirationList.isEmpty()) {
        auto [requestUrl, timestamp] = m_sessionExpirationList.first();
        auto* resources = m_sessionPrefetches.get();
        ASSERT(resources);
        ASSERT(resources->contains(requestUrl));
        auto elapsed = timeout - timestamp;
        if (elapsed > expirationTimeout) {
            resources->remove(requestUrl);
            m_sessionExpirationList.removeFirst();
        } else {
            m_expirationTimer.startOneShot(expirationTimeout - elapsed);
            break;
        }
    }
}

}
