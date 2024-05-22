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
    Locker locker { m_HostTopPrivatelyControlledDomainCacheLock };
    m_hostTopPrivatelyControlledDomainCache.clear();
}

bool PublicSuffixStore::isPublicSuffix(StringView domain) const
{
    return platformIsPublicSuffix(domain);
}

PublicSuffix PublicSuffixStore::publicSuffix(const URL& url) const
{
    if (!url.isValid())
        return { };

    auto host = url.host();
    if (URL::hostIsIPAddress(host))
        return { };

    size_t separatorPosition;
    for (unsigned labelStart = 0; (separatorPosition = host.find('.', labelStart)) != notFound; labelStart = separatorPosition + 1) {
        auto candidate = host.substring(separatorPosition + 1);
        if (isPublicSuffix(candidate))
            return PublicSuffix::fromRawString(candidate.toString());
    }

    return { };
}

String PublicSuffixStore::topPrivatelyControlledDomain(StringView host) const
{
    // FIXME: if host is a URL, we could drop these checks.
    if (host.isEmpty())
        return { };

    if (!host.containsOnlyASCII())
        return host.toString();

    Locker locker { m_HostTopPrivatelyControlledDomainCacheLock };
    auto result = m_hostTopPrivatelyControlledDomainCache.ensure<ASCIICaseInsensitiveStringViewHashTranslator>(host, [&] {
        const auto lowercaseHost = host.convertToASCIILowercase();
        if (lowercaseHost == "localhost"_s || URL::hostIsIPAddress(lowercaseHost))
            return lowercaseHost;

        return platformTopPrivatelyControlledDomain(lowercaseHost);
    }).iterator->value.isolatedCopy();

    constexpr auto maxHostTopPrivatelyControlledDomainCache = 128;
    if (m_hostTopPrivatelyControlledDomainCache.size() > maxHostTopPrivatelyControlledDomainCache)
        m_hostTopPrivatelyControlledDomainCache.remove(m_hostTopPrivatelyControlledDomainCache.random());

    return result;
}

} // namespace WebCore
