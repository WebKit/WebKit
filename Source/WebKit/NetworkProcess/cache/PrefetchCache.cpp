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
#include <wtf/NeverDestroyed.h>

namespace WebKit {

PrefetchCache::Entry::Entry(WebCore::ResourceResponse&& response, RefPtr<WebCore::SharedBuffer>&& buffer)
    : response(WTFMove(response)), buffer(WTFMove(buffer))
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
    m_sessionPrefetches.clear();
}

std::unique_ptr<PrefetchCache::Entry> PrefetchCache::take(PAL::SessionID sessionID, const URL& url)
{
    auto* resources = sessionPrefetchMap(sessionID);
    if (!resources)
        return nullptr;
    return resources->take(url);
}

static const Seconds expirationTimeout { 5_s };

void PrefetchCache::store(PAL::SessionID sessionID, const URL& requestUrl, WebCore::ResourceResponse&& response, RefPtr<WebCore::SharedBuffer>&& buffer)
{
    ASSERT(sessionID.isValid());
    m_sessionPrefetches.ensure(sessionID, [&] {
        return std::make_unique<PrefetchEntriesMap>();
    }).iterator->value->set(requestUrl, std::make_unique<PrefetchCache::Entry>(WTFMove(response), WTFMove(buffer)));
    m_sessionExpirationList.append(std::make_tuple(sessionID, requestUrl, WallTime::now()));
    if (!m_expirationTimer.isActive())
        m_expirationTimer.startOneShot(expirationTimeout);
}

auto PrefetchCache::sessionPrefetchMap(PAL::SessionID sessionID) const -> PrefetchEntriesMap*
{
    ASSERT(sessionID.isValid());
    return m_sessionPrefetches.get(sessionID);
}

void PrefetchCache::clearExpiredEntries()
{
    PAL::SessionID sessionID;
    URL requestUrl;
    WallTime timestamp;
    auto timeout = WallTime::now();
    while (!m_sessionExpirationList.isEmpty()) {
        std::tie(sessionID, requestUrl, timestamp) = m_sessionExpirationList.first();
        auto* resources = sessionPrefetchMap(sessionID);
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
