/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "PublicSuffixStore.h"

#include <wtf/CrossThreadCopier.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

PublicSuffixStore& PublicSuffixStore::singleton()
{
    static LazyNeverDestroyed<PublicSuffixStore> store;
    static std::once_flag flag;
    std::call_once(flag, [&] {
        store.construct();
    });
    return store.get();
}

void PublicSuffixStore::clearHostTopPrivatelyControlledDomainCache()
{
    Locker locker { m_hostRegistrableDomainCacheLock };
    m_hostRegistrableDomainCache.clear();
}

bool PublicSuffixStore::isPublicSuffix(StringView domain) const
{
    return platformIsPublicSuffix(domain);
}

String PublicSuffixStore::publicSuffix(const URL& url) const
{
    if (!url.isValid())
        return { };

    auto host = url.host();
    if (host.isEmpty() || URL::hostIsIPAddress(host))
        return { };

    size_t separatorPosition;
    for (unsigned labelStart = 0; (separatorPosition = host.find('.', labelStart)) != notFound; labelStart = separatorPosition + 1) {
        auto candidate = host.substring(separatorPosition + 1);
        if (isPublicSuffix(candidate))
            return candidate.toString();
    }

    return { };
}

String PublicSuffixStore::registrableDomain(const URL& url) const
{
    if (!url.isValid())
        return { };

    auto host = url.host();
    if (host.isEmpty())
        return { };

    if (!host.containsOnlyASCII())
        return host.toString();

    Locker locker { m_hostRegistrableDomainCacheLock };
    auto hostString = crossThreadCopy(host.toString());
    auto& result = m_hostRegistrableDomainCache.ensure(hostString, [&] {
        if (hostString == "localhost"_s || URL::hostIsIPAddress(hostString))
            return hostString;

        return platformRegistrableDomain(hostString);
    }).iterator->value;

    constexpr auto maxHostRegistrableDomainCacheSize = 128;
    if (m_hostRegistrableDomainCache.size() > maxHostRegistrableDomainCacheSize)
        m_hostRegistrableDomainCache.remove(m_hostRegistrableDomainCache.random());

    return crossThreadCopy(result);
}

} // namespace WebCore
