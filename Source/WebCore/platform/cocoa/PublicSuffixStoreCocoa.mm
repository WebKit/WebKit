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

#import "config.h"
#import "PublicSuffixStore.h"

#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/URLHelpers.h>
#import <wtf/cocoa/NSURLExtras.h>
#import <wtf/text/StringHash.h>

namespace WebCore {

static bool isPublicSuffixCF(StringView domain)
{
    if (auto host = WTF::URLHelpers::mapHostName(domain.toStringWithoutCopying(), nullptr))
        return !host->isNull() ? _CFHostIsDomainTopLevel(host->createCFString().get()) : _CFHostIsDomainTopLevel(domain.createCFStringWithoutCopying().get());
    return false;
}

bool PublicSuffixStore::platformIsPublicSuffix(StringView domain) const
{
    {
        Locker locker { m_publicSuffixCacheLock };
        if (m_publicSuffixCache && m_publicSuffixCache->contains<ASCIICaseInsensitiveStringViewHashTranslator>(domain))
            return true;
    }

    return isPublicSuffixCF(domain);
}

String PublicSuffixStore::platformTopPrivatelyControlledDomain(StringView host) const
{
    size_t separatorPosition;
    for (unsigned labelStart = 0; (separatorPosition = host.find('.', labelStart)) != notFound; labelStart = separatorPosition + 1) {
        if (isPublicSuffix(host.substring(separatorPosition + 1)))
            return host.substring(labelStart).toString();
    }

    return { };
}

void PublicSuffixStore::enablePublicSuffixCache(CanAcceptCustomPublicSuffix canAcceptCustomPublicSuffix)
{
    ASSERT(isMainThread());

    m_canAcceptCustomPublicSuffix = canAcceptCustomPublicSuffix == CanAcceptCustomPublicSuffix::Yes;

    Locker locker { m_publicSuffixCacheLock };
    ASSERT(!m_publicSuffixCache);
    m_publicSuffixCache = HashSet<String, ASCIICaseInsensitiveHash> { };
}

void PublicSuffixStore::addPublicSuffix(const String& publicSuffix)
{
    ASSERT(isMainThread());
    if (publicSuffix.isEmpty())
        return;

    if (LIKELY(!m_canAcceptCustomPublicSuffix))
        RELEASE_ASSERT(isPublicSuffixCF(publicSuffix));

    Locker locker { m_publicSuffixCacheLock };
    ASSERT(m_publicSuffixCache);
    m_publicSuffixCache->add(crossThreadCopy(publicSuffix));
}

} // namespace WebCore
