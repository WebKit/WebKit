/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#pragma once

#include "PublicSuffix.h"
#include <wtf/HashTraits.h>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class RegistrableDomain {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RegistrableDomain() = default;
    explicit RegistrableDomain(const URL& url)
#if ENABLE(PUBLIC_SUFFIX_LIST)
        : m_registrableDomain { topPrivatelyControlledDomain(url.host().toString()) }
#else
        : m_registrableDomain { url.host().toString() }
#endif
    {
        auto hostString = url.host().toString();
        if (hostString.isEmpty())
            m_registrableDomain = "nullOrigin"_s;
        else if (m_registrableDomain.isEmpty())
            m_registrableDomain = hostString;
    }

    explicit RegistrableDomain(const String& domain)
#if ENABLE(PUBLIC_SUFFIX_LIST)
        : m_registrableDomain { topPrivatelyControlledDomain(domain) }
#else
        : m_registrableDomain { domain }
#endif
    {
        if (domain.isEmpty())
            m_registrableDomain = "nullOrigin"_s;
        else if (m_registrableDomain.isEmpty())
            m_registrableDomain = domain;
    }

    bool isEmpty() const { return m_registrableDomain.isEmpty() || m_registrableDomain == "nullOrigin"_s; }
    const String& string() const { return m_registrableDomain; }

    bool operator!=(const RegistrableDomain& other) const { return m_registrableDomain != other.m_registrableDomain; }
    bool operator==(const RegistrableDomain& other) const { return m_registrableDomain == other.m_registrableDomain; }

    bool matches(const URL& url) const
    {
        auto host = url.host();
        if (!host.endsWith(m_registrableDomain))
            return false;
        if (host.length() == m_registrableDomain.length())
            return true;
        return host[host.length() - m_registrableDomain.length() - 1] == '.';
    }

    RegistrableDomain isolatedCopy() const { return RegistrableDomain { m_registrableDomain.isolatedCopy() }; }

    RegistrableDomain(WTF::HashTableDeletedValueType)
        : m_registrableDomain(WTF::HashTableDeletedValue) { }
    bool isHashTableDeletedValue() const { return m_registrableDomain.isHashTableDeletedValue(); }
    unsigned hash() const { return m_registrableDomain.hash(); }

    struct RegistrableDomainHash {
        static unsigned hash(const RegistrableDomain& registrableDomain) { return registrableDomain.m_registrableDomain.hash(); }
        static bool equal(const RegistrableDomain& a, const RegistrableDomain& b) { return a == b; }
        static const bool safeToCompareToEmptyOrDeleted = false;
    };
    
    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<RegistrableDomain> decode(Decoder&);
    
private:
    String m_registrableDomain;
};

template<class Encoder>
void RegistrableDomain::encode(Encoder& encoder) const
{
    encoder << m_registrableDomain;
}

template<class Decoder>
Optional<RegistrableDomain> RegistrableDomain::decode(Decoder& decoder)
{
    Optional<String> domain;
    decoder >> domain;
    if (!domain)
        return WTF::nullopt;

    RegistrableDomain registrableDomain;
    registrableDomain.m_registrableDomain = WTFMove(*domain);
    return registrableDomain;
}

} // namespace WebCore

namespace WTF {
template<> struct DefaultHash<WebCore::RegistrableDomain> {
    using Hash = WebCore::RegistrableDomain::RegistrableDomainHash;
};
template<> struct HashTraits<WebCore::RegistrableDomain> : SimpleClassHashTraits<WebCore::RegistrableDomain> { };
}
