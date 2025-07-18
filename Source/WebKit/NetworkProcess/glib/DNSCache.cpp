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
#include "DNSCache.h"

#include <wtf/glib/RunLoopSourcePriority.h>

namespace WebKit {

static const Seconds expireInterval = 60_s;
static const unsigned maxCacheSize = 400;

Ref<DNSCache> DNSCache::create()
{
    return adoptRef(*new DNSCache);
}

DNSCache::DNSCache()
    : m_expiredTimer(RunLoop::mainSingleton(), this, &DNSCache::removeExpiredResponsesFired)
{
    m_expiredTimer.setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
}

DNSCache::DNSCacheMap& DNSCache::mapForType(Type type)
{
    switch (type) {
    case Type::Default:
        return m_dnsMap;
    case Type::IPv4Only:
        return m_ipv4Map;
    case Type::IPv6Only:
        return m_ipv6Map;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return m_dnsMap;
}

std::optional<Vector<GRefPtr<GInetAddress>>> DNSCache::lookup(const CString& host, Type type)
{
    Locker locker { m_lock };
    auto& map = mapForType(type);
    auto it = map.find(host);
    if (it == map.end())
        return std::nullopt;

    auto& response = it->value;
    if (response.expirationTime <= MonotonicTime::now()) {
        map.remove(it);
        return std::nullopt;
    }

    return response.addressList;
}

void DNSCache::update(const CString& host, Vector<GRefPtr<GInetAddress>>&& addressList, Type type)
{
    Locker locker { m_lock };
    auto& map = mapForType(type);
    CachedResponse response = { WTFMove(addressList), MonotonicTime::now() + expireInterval };
    auto addResult = map.set(host, WTFMove(response));
    if (addResult.isNewEntry)
        pruneResponsesInMap(map);
    m_expiredTimer.startOneShot(expireInterval);
}

void DNSCache::removeExpiredResponsesInMap(DNSCacheMap& map)
{
    map.removeIf([now = MonotonicTime::now()](auto& entry) {
        return entry.value.expirationTime <= now;
    });
}

void DNSCache::pruneResponsesInMap(DNSCacheMap& map)
{
    if (map.size() <= maxCacheSize)
        return;

    // First try to remove expired responses.
    removeExpiredResponsesInMap(map);
    if (map.size() <= maxCacheSize)
        return;

    Vector<CString> keys = copyToVector(map.keys());
    std::sort(keys.begin(), keys.end(), [&map](const CString& a, const CString& b) {
        return map.get(a).expirationTime < map.get(b).expirationTime;
    });

    unsigned responsesToRemoveCount = keys.size() - maxCacheSize;
    for (unsigned i = 0; i < responsesToRemoveCount; ++i)
        map.remove(keys[i]);
}

void DNSCache::removeExpiredResponsesFired()
{
    Locker locker { m_lock };
    removeExpiredResponsesInMap(m_dnsMap);
    removeExpiredResponsesInMap(m_ipv4Map);
    removeExpiredResponsesInMap(m_ipv6Map);
}

void DNSCache::clear()
{
    Locker locker { m_lock };
    m_dnsMap.clear();
    m_ipv4Map.clear();
    m_ipv6Map.clear();
}

} // namespace WebKit
