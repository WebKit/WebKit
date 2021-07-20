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
#include "SecurityOriginData.h"
#include <wtf/HashTraits.h>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class RegistrableDomain {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RegistrableDomain() = default;

    explicit RegistrableDomain(const URL& url)
        : RegistrableDomain(registrableDomainFromHost(url.host().toString()))
    {
    }

    explicit RegistrableDomain(const SecurityOriginData& origin)
        : RegistrableDomain(registrableDomainFromHost(origin.host))
    {
    }

    bool isEmpty() const { return m_registrableDomain.isEmpty() || m_registrableDomain == "nullOrigin"_s; }
    String& string() { return m_registrableDomain; }
    const String& string() const { return m_registrableDomain; }

    bool operator!=(const RegistrableDomain& other) const { return m_registrableDomain != other.m_registrableDomain; }
    bool operator==(const RegistrableDomain& other) const { return m_registrableDomain == other.m_registrableDomain; }
    bool operator==(const char* other) const { return m_registrableDomain == other; }

    bool matches(const URL& url) const
    {
        return matches(url.host());
    }

    bool matches(const SecurityOriginData& origin) const
    {
        return matches(origin.host);
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

    static RegistrableDomain uncheckedCreateFromRegistrableDomainString(const String& domain)
    {
        return RegistrableDomain { String { domain } };
    }
    
    static RegistrableDomain uncheckedCreateFromHost(const String& host)
    {
#if ENABLE(PUBLIC_SUFFIX_LIST)
        auto registrableDomain = topPrivatelyControlledDomain(host);
        if (registrableDomain.isEmpty())
            return uncheckedCreateFromRegistrableDomainString(host);
        return RegistrableDomain { WTFMove(registrableDomain) };
#else
        return uncheckedCreateFromRegistrableDomainString(host);
#endif
    }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<RegistrableDomain> decode(Decoder&);

protected:

private:
    explicit RegistrableDomain(String&& domain)
        : m_registrableDomain { domain.isEmpty() ? "nullOrigin"_s : WTFMove(domain) }
    {
    }

    bool matches(StringView host) const
    {
        if (host.isEmpty() && m_registrableDomain == "nullOrigin"_s)
            return true;
        if (!host.endsWith(m_registrableDomain))
            return false;
        if (host.length() == m_registrableDomain.length())
            return true;
        return host[host.length() - m_registrableDomain.length() - 1] == '.';
    }

    static inline String registrableDomainFromHost(const String& host)
    {
#if ENABLE(PUBLIC_SUFFIX_LIST)
        auto domain = topPrivatelyControlledDomain(host);
#else
        auto domain = host;
#endif
        if (host.isEmpty())
            domain = "nullOrigin"_s;
        else if (domain.isEmpty())
            domain = host;
        return domain;
    }

    String m_registrableDomain;
};

template<class Encoder>
void RegistrableDomain::encode(Encoder& encoder) const
{
    encoder << m_registrableDomain;
}

template<class Decoder>
std::optional<RegistrableDomain> RegistrableDomain::decode(Decoder& decoder)
{
    std::optional<String> domain;
    decoder >> domain;
    if (!domain)
        return std::nullopt;

    RegistrableDomain registrableDomain;
    registrableDomain.m_registrableDomain = WTFMove(*domain);
    return registrableDomain;
}

inline bool areRegistrableDomainsEqual(const URL& a, const URL& b)
{
    return RegistrableDomain(a).matches(b);
}

} // namespace WebCore

namespace WTF {
template<> struct DefaultHash<WebCore::RegistrableDomain> : WebCore::RegistrableDomain::RegistrableDomainHash { };
template<> struct HashTraits<WebCore::RegistrableDomain> : SimpleClassHashTraits<WebCore::RegistrableDomain> { };

template<> class StringTypeAdapter<WebCore::RegistrableDomain, void> : public StringTypeAdapter<String, void> {
public:
    StringTypeAdapter(const WebCore::RegistrableDomain& domain)
        : StringTypeAdapter<String, void>(domain.string())
    { }
};

} // namespace WTF
