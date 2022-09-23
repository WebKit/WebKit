/*
 * Copyright (C) 2011, 2014-2015 Apple Inc. All rights reserved.
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
#include "NetworkCacheCoders.h"

#include "NetworkCacheKey.h"
#include "NetworkCacheSubresourcesEntry.h"
#include <wtf/persistence/PersistentCoders.h>

namespace WTF::Persistence {

void Coder<WebKit::NetworkCache::Key>::encode(WTF::Persistence::Encoder& encoder, const WebKit::NetworkCache::Key& instance)
{
    encoder << instance.partition();
    encoder << instance.type();
    encoder << instance.identifier();
    encoder << instance.range();
    encoder << instance.hash();
    encoder << instance.partitionHash();
}

std::optional<WebKit::NetworkCache::Key> Coder<WebKit::NetworkCache::Key>::decode(WTF::Persistence::Decoder& decoder)
{
    WebKit::NetworkCache::Key key;

    std::optional<String> partition;
    decoder >> partition;
    if (!partition)
        return std::nullopt;
    key.m_partition = WTFMove(*partition);

    std::optional<String> type;
    decoder >> type;
    if (!type)
        return std::nullopt;
    key.m_type = WTFMove(*type);

    std::optional<String> identifier;
    decoder >> identifier;
    if (!identifier)
        return std::nullopt;
    key.m_identifier = WTFMove(*identifier);

    std::optional<String> range;
    decoder >> range;
    if (!range)
        return std::nullopt;
    key.m_range = WTFMove(*range);

    std::optional<WebKit::NetworkCache::Key::HashType> hash;
    decoder >> hash;
    if (!hash)
        return std::nullopt;
    key.m_hash = WTFMove(*hash);

    std::optional<WebKit::NetworkCache::Key::HashType> partitionHash;
    decoder >> partitionHash;
    if (!partitionHash)
        return std::nullopt;
    key.m_partitionHash = WTFMove(*partitionHash);

    return { WTFMove(key) };
}

#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
void Coder<WebKit::NetworkCache::SubresourceInfo>::encode(WTF::Persistence::Encoder& encoder, const WebKit::NetworkCache::SubresourceInfo& instance)
{
    encoder << instance.key();
    encoder << instance.lastSeen();
    encoder << instance.firstSeen();
    encoder << instance.isTransient();

    // Do not bother serializing other data members of transient resources as they are empty.
    if (instance.isTransient())
        return;

    encoder << instance.isSameSite();
    encoder << instance.isAppInitiated();
    encoder << instance.firstPartyForCookies();
    encoder << instance.requestHeaders();
    encoder << instance.priority();
}

std::optional<WebKit::NetworkCache::SubresourceInfo> Coder<WebKit::NetworkCache::SubresourceInfo>::decode(WTF::Persistence::Decoder& decoder)
{
    std::optional<WebKit::NetworkCache::Key> key;
    decoder >> key;
    if (!key)
        return std::nullopt;

    std::optional<WallTime> lastSeen;
    decoder >> lastSeen;
    if (!lastSeen)
        return std::nullopt;

    std::optional<WallTime> firstSeen;
    decoder >> firstSeen;
    if (!firstSeen)
        return std::nullopt;

    std::optional<bool> isTransient;
    decoder >> isTransient;
    if (!isTransient)
        return std::nullopt;

    if (*isTransient)
        return WebKit::NetworkCache::SubresourceInfo(WTFMove(*key), *lastSeen, *firstSeen);

    std::optional<bool> isSameSite;
    decoder >> isSameSite;
    if (!isSameSite)
        return std::nullopt;

    std::optional<bool> isAppInitiated;
    decoder >> isAppInitiated;
    if (!isAppInitiated)
        return std::nullopt;

    std::optional<URL> firstPartyForCookies;
    decoder >> firstPartyForCookies;
    if (!firstPartyForCookies)
        return std::nullopt;

    std::optional<WebCore::HTTPHeaderMap> requestHeaders;
    decoder >> requestHeaders;
    if (!requestHeaders)
        return std::nullopt;

    std::optional<WebCore::ResourceLoadPriority> priority;
    decoder >> priority;
    if (!priority)
        return std::nullopt;

    return WebKit::NetworkCache::SubresourceInfo(WTFMove(*key), *lastSeen, *firstSeen, *isSameSite, *isAppInitiated, WTFMove(*firstPartyForCookies), WTFMove(*requestHeaders), *priority);
}
#endif // ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)

}
