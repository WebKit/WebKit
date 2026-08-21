/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/PublicSuffixStore.h>
#include <WebCore/QuirksData.h>
#include <WebCore/RegistrableDomain.h>
#include <array>
#include <initializer_list>
#include <optional>
#include <ranges>
#include <span>
#include <wtf/Assertions.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

enum class QuirkCondition : uint8_t {
    SmallScreen,
    TubularApp,
    LensApp,
};

WEBCORE_EXPORT bool evaluateQuirkCondition(QuirkCondition);

class QuirkMatchContext {
public:
    QuirkMatchContext(URL topURL, URL documentURL)
        : m_topURL(WTF::move(topURL))
        , m_documentURL(WTF::move(documentURL))
    {
    }

    const URL& topURL() const LIFETIME_BOUND { return m_topURL; }

    StringView host() const LIFETIME_BOUND { return m_topURL.host(); }

    const String& registrableDomain() const LIFETIME_BOUND
    {
        if (!m_registrableDomain)
            m_registrableDomain = RegistrableDomain { m_topURL }.string();
        return *m_registrableDomain;
    }

    const String& domainWithoutPublicSuffix() const LIFETIME_BOUND
    {
        if (!m_domainWithoutPublicSuffix)
            m_domainWithoutPublicSuffix = PublicSuffixStore::singleton().domainWithoutPublicSuffix(registrableDomain());
        return *m_domainWithoutPublicSuffix;
    }

    const String& documentDomain() const LIFETIME_BOUND
    {
        if (!m_documentDomain)
            m_documentDomain = RegistrableDomain { m_documentURL }.string();
        return *m_documentDomain;
    }

private:
    const URL m_topURL;
    const URL m_documentURL;
    mutable std::optional<String> m_registrableDomain;
    mutable std::optional<String> m_domainWithoutPublicSuffix;
    mutable std::optional<String> m_documentDomain;
};

// QuirkMatch represents a declarative description of which pages a quirk applies to
//
// Every match starts with exactly one static factory, which picks how the site
// is identified, and is then optionally narrowed by chained refinements. All
// conditions are ANDed together:
//
//     QuirkMatch::anyTopLevelDomain("apple"_s).pathStartsWith("/store"_s)
//
class QuirkMatch {
public:
    static constexpr QuirkMatch domain(ASCIILiteral domain)
    {
        return QuirkMatch { Kind::Domain, domain };
    }

    template<const auto& patterns> static constexpr QuirkMatch domains()
    {
        static_assert(std::size(patterns), "A quirk must match at least one domain.");
        return QuirkMatch { Kind::Domain, std::span<const ASCIILiteral> { patterns } };
    }

    static constexpr QuirkMatch host(ASCIILiteral host)
    {
        return QuirkMatch { Kind::Host, host };
    }

    static constexpr QuirkMatch hostOrSubdomainOf(ASCIILiteral host)
    {
        return QuirkMatch { Kind::HostOrSubdomainOf, host };
    }

    static constexpr QuirkMatch hostEndingWith(ASCIILiteral suffix)
    {
        return QuirkMatch { Kind::HostEndingWith, suffix };
    }

    static constexpr QuirkMatch anyTopLevelDomain(ASCIILiteral name)
    {
        return QuirkMatch { Kind::AnyTopLevelDomain, name };
    }

    constexpr QuirkMatch&& pathContains(ASCIILiteral substring) &&
    {
        return WTF::move(*this).setPathConstraint(PathConstraintKind::PathContains, substring);
    }

    constexpr QuirkMatch&& pathStartsWith(ASCIILiteral prefix) &&
    {
        return WTF::move(*this).setPathConstraint(PathConstraintKind::PathStartsWith, prefix);
    }

    constexpr QuirkMatch&& pathOrFragmentContains(ASCIILiteral substring) &&
    {
        return WTF::move(*this).setPathConstraint(PathConstraintKind::PathOrFragmentContains, substring);
    }

    constexpr QuirkMatch&& onlyIf(QuirkCondition condition) &&
    {
        m_condition = condition;
        return WTF::move(*this);
    }

    template<const auto& patterns> constexpr QuirkMatch&& documentDomainIsOneOf() &&
    {
        static_assert(std::size(patterns), "A quirk must match at least one document domain.");
        m_documentDomains = std::span<const ASCIILiteral> { patterns };
        return WTF::move(*this);
    }

    bool matches(const QuirkMatchContext& context) const
    {
        if (!matchesSite(context)) [[likely]]
            return false;

        if (m_pathConstraint && !matchesPathConstraint(context.topURL()))
            return false;

        if (m_condition && !evaluateQuirkCondition(*m_condition))
            return false;

        if (!m_documentDomains.empty() && !matchesDocumentDomain(context))
            return false;

        return true;
    }

private:
    enum class Kind : uint8_t {
        Domain,
        Host,
        HostOrSubdomainOf,
        HostEndingWith,
        AnyTopLevelDomain,
    };

    enum class PathConstraintKind : uint8_t {
        PathContains,
        PathStartsWith,
        PathOrFragmentContains,
    };

    class PatternList {
    public:
        constexpr explicit PatternList(ASCIILiteral pattern)
            : m_single(pattern)
        {
            ASSERT_UNDER_CONSTEXPR_CONTEXT(!pattern.isNull());
        }

        constexpr explicit PatternList(std::span<const ASCIILiteral> patterns)
            : m_multiple(patterns)
        {
            ASSERT_UNDER_CONSTEXPR_CONTEXT(!patterns.empty());
        }

        std::span<const ASCIILiteral> span() const LIFETIME_BOUND
        {
            return m_multiple.empty() ? singleElementSpan(m_single) : m_multiple;
        }

    private:
        ASCIILiteral m_single;
        std::span<const ASCIILiteral> m_multiple;
    };

    constexpr QuirkMatch(Kind kind, ASCIILiteral pattern)
        : m_kind(kind)
        , m_patterns(pattern)
    {
    }

    constexpr QuirkMatch(Kind kind, std::span<const ASCIILiteral> patterns)
        : m_kind(kind)
        , m_patterns(patterns)
    {
    }

    constexpr QuirkMatch&& setPathConstraint(PathConstraintKind kind, ASCIILiteral value) &&
    {
        m_pathConstraintKind = kind;
        m_pathConstraint = value;
        return WTF::move(*this);
    }

    template<typename Function> bool anyPattern(NOESCAPE Function&& match) const
    {
        return std::ranges::any_of(m_patterns.span(), match);
    }

    bool matchesSite(const QuirkMatchContext& context) const
    {
        switch (m_kind) {
        case Kind::Domain:
            return anyPattern([&](ASCIILiteral pattern) { return context.registrableDomain() == pattern; });
        case Kind::Host:
            return anyPattern([&](ASCIILiteral pattern) { return context.host() == pattern; });
        case Kind::HostOrSubdomainOf:
            return anyPattern([&](ASCIILiteral pattern) { return context.topURL().isMatchingDomain(pattern); });
        case Kind::HostEndingWith:
            return anyPattern([&](ASCIILiteral pattern) { return context.host().endsWithIgnoringASCIICase(pattern); });
        case Kind::AnyTopLevelDomain:
            return anyPattern([&](ASCIILiteral pattern) { return context.domainWithoutPublicSuffix() == pattern; });
        }

        ASSERT_NOT_REACHED();
        return false;
    }

    bool matchesPathConstraint(const URL& topURL) const
    {
        switch (m_pathConstraintKind) {
        case PathConstraintKind::PathContains:
            return topURL.path().contains(m_pathConstraint);
        case PathConstraintKind::PathStartsWith:
            return startsWithLettersIgnoringASCIICase(topURL.path(), m_pathConstraint);
        case PathConstraintKind::PathOrFragmentContains:
            return topURL.path().contains(m_pathConstraint) || topURL.fragmentIdentifier().contains(m_pathConstraint);
        }

        ASSERT_NOT_REACHED();
        return false;
    }

    bool matchesDocumentDomain(const QuirkMatchContext& context) const
    {
        for (auto pattern : m_documentDomains) {
            if (context.documentDomain() == pattern)
                return true;
        }
        return false;
    }

    Kind m_kind;
    PathConstraintKind m_pathConstraintKind { PathConstraintKind::PathContains };
    PatternList m_patterns;
    ASCIILiteral m_pathConstraint;
    std::span<const ASCIILiteral> m_documentDomains;
    std::optional<QuirkCondition> m_condition;
};

consteval QuirksData::QuirkBitSet quirkBehaviors(std::initializer_list<QuirksData::SiteSpecificQuirk> quirks)
{
    QuirksData::QuirkBitSet bits;
    for (auto quirk : quirks)
        bits.set(static_cast<size_t>(quirk));
    return bits;
}

struct Quirk {
    QuirkMatch match;
    QuirksData::QuirkBitSet behaviors { };
    std::optional<QuirkSite> site { };
    bool disablesElementFullscreen { false };

    void apply(QuirksData& quirksData) const
    {
        quirksData.activeQuirks.merge(behaviors);

        if (site)
            quirksData.addSite(*site);

        if (disablesElementFullscreen)
            quirksData.shouldDisableElementFullscreen = true;
    }
};

} // namespace WebCore
