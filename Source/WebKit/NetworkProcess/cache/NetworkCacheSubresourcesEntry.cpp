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
#include "NetworkCacheSubresourcesEntry.h"

#include "Logging.h"
#include "NetworkCacheCoders.h"
#include <WebCore/RegistrableDomain.h>

namespace WebKit {
namespace NetworkCache {

void SubresourceInfo::encode(WTF::Persistence::Encoder& encoder) const
{
    encoder << m_key;
    encoder << m_lastSeen;
    encoder << m_firstSeen;
    encoder << m_isTransient;

    // Do not bother serializing other data members of transient resources as they are empty.
    if (m_isTransient)
        return;

    encoder << m_isSameSite;
    encoder << m_firstPartyForCookies;
    encoder << m_requestHeaders;
    encoder << m_priority;
}

std::optional<SubresourceInfo> SubresourceInfo::decode(WTF::Persistence::Decoder& decoder)
{
    SubresourceInfo info;

    std::optional<Key> key;
    decoder >> key;
    if (!key)
        return std::nullopt;
    info.m_key = WTFMove(*key);

    std::optional<WallTime> lastSeen;
    decoder >> lastSeen;
    if (!lastSeen)
        return std::nullopt;
    info.m_lastSeen = WTFMove(*lastSeen);

    std::optional<WallTime> firstSeen;
    decoder >> firstSeen;
    if (!firstSeen)
        return std::nullopt;
    info.m_firstSeen = WTFMove(*firstSeen);

    std::optional<bool> isTransient;
    decoder >> isTransient;
    if (!isTransient)
        return std::nullopt;
    info.m_isTransient = WTFMove(*isTransient);

    if (info.m_isTransient)
        return { WTFMove(info) };

    std::optional<bool> isSameSite;
    decoder >> isSameSite;
    if (!isSameSite)
        return std::nullopt;
    info.m_isSameSite = WTFMove(*isSameSite);

    std::optional<URL> firstPartyForCookies;
    decoder >> firstPartyForCookies;
    if (!firstPartyForCookies)
        return std::nullopt;
    info.m_firstPartyForCookies = WTFMove(*firstPartyForCookies);

    std::optional<WebCore::HTTPHeaderMap> requestHeaders;
    decoder >> requestHeaders;
    if (!requestHeaders)
        return std::nullopt;
    info.m_requestHeaders = WTFMove(*requestHeaders);

    std::optional<WebCore::ResourceLoadPriority> priority;
    decoder >> priority;
    if (!priority)
        return std::nullopt;
    info.m_priority = WTFMove(*priority);
    
    return { WTFMove(info) };
}

bool SubresourceInfo::isFirstParty() const
{
    WebCore::RegistrableDomain firstPartyDomain { m_firstPartyForCookies };
    return firstPartyDomain.matches(URL(URL(), key().identifier()));
}

Storage::Record SubresourcesEntry::encodeAsStorageRecord() const
{
    WTF::Persistence::Encoder encoder;
    encoder << m_subresources;

    encoder.encodeChecksum();

    return { m_key, m_timeStamp, { encoder.buffer(), encoder.bufferSize() }, { }, { }};
}

std::unique_ptr<SubresourcesEntry> SubresourcesEntry::decodeStorageRecord(const Storage::Record& storageEntry)
{
    auto entry = makeUnique<SubresourcesEntry>(storageEntry);

    WTF::Persistence::Decoder decoder(storageEntry.header.span());
    std::optional<Vector<SubresourceInfo>> subresources;
    decoder >> subresources;
    if (!subresources)
        return nullptr;
    entry->m_subresources = WTFMove(*subresources);

    if (!decoder.verifyChecksum()) {
        LOG(NetworkCache, "(NetworkProcess) checksum verification failure\n");
        return nullptr;
    }

    return entry;
}

SubresourcesEntry::SubresourcesEntry(const Storage::Record& storageEntry)
    : m_key(storageEntry.key)
    , m_timeStamp(storageEntry.timeStamp)
{
    ASSERT(m_key.type() == "SubResources");
}

SubresourceInfo::SubresourceInfo(const Key& key, const WebCore::ResourceRequest& request, const SubresourceInfo* previousInfo)
    : m_key(key)
    , m_lastSeen(WallTime::now())
    , m_firstSeen(previousInfo ? previousInfo->firstSeen() : m_lastSeen)
    , m_isTransient(!previousInfo)
    , m_isSameSite(request.isSameSite())
    , m_firstPartyForCookies(request.firstPartyForCookies())
    , m_requestHeaders(request.httpHeaderFields())
    , m_priority(request.priority())
{
}

static Vector<SubresourceInfo> makeSubresourceInfoVector(const Vector<std::unique_ptr<SubresourceLoad>>& subresourceLoads, Vector<SubresourceInfo>* previousSubresources)
{
    Vector<SubresourceInfo> result;
    result.reserveInitialCapacity(subresourceLoads.size());
    
    HashMap<Key, unsigned> previousMap;
    if (previousSubresources) {
        for (unsigned i = 0; i < previousSubresources->size(); ++i)
            previousMap.add(previousSubresources->at(i).key(), i);
    }

    HashSet<Key> deduplicationSet;
    for (auto& load : subresourceLoads) {
        if (!deduplicationSet.add(load->key).isNewEntry)
            continue;
        
        SubresourceInfo* previousInfo = nullptr;
        if (previousSubresources) {
            auto it = previousMap.find(load->key);
            if (it != previousMap.end())
                previousInfo = &(*previousSubresources)[it->value];
        }
        
        result.uncheckedAppend({ load->key, load->request, previousInfo });
        
        // FIXME: We should really consider all resources seen for the first time transient.
        if (!previousSubresources)
            result.last().setNonTransient();
    }

    return result;
}

SubresourcesEntry::SubresourcesEntry(Key&& key, const Vector<std::unique_ptr<SubresourceLoad>>& subresourceLoads)
    : m_key(WTFMove(key))
    , m_timeStamp(WallTime::now())
    , m_subresources(makeSubresourceInfoVector(subresourceLoads, nullptr))
{
    ASSERT(m_key.type() == "SubResources");
}
    
void SubresourcesEntry::updateSubresourceLoads(const Vector<std::unique_ptr<SubresourceLoad>>& subresourceLoads)
{
    m_subresources = makeSubresourceInfoVector(subresourceLoads, &m_subresources);
}

} // namespace WebKit
} // namespace NetworkCache

#endif // ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
