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

enum class IsTopDocument : bool { No, Yes };

class QuirkMatchContext {
public:
    QuirkMatchContext(URL topURL, URL documentURL, IsTopDocument isTopDocument = IsTopDocument::Yes)
        : m_topURL(WTF::move(topURL))
        , m_documentURL(WTF::move(documentURL))
        , m_isTopDocument(isTopDocument)
    {
    }

    const URL& topURL() const LIFETIME_BOUND { return m_topURL; }

    bool isTopDocument() const { return m_isTopDocument == IsTopDocument::Yes; }

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
    const IsTopDocument m_isTopDocument;
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

    static constexpr QuirkMatch anySite()
    {
        return QuirkMatch { Kind::AnySite };
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
        currentRefinements().condition = condition;
        return WTF::move(*this);
    }

    constexpr QuirkMatch&& documentDomainIs(ASCIILiteral pattern) &&
    {
        currentRefinements().documentDomains = PatternList { pattern };
        return WTF::move(*this);
    }

    template<const auto& patterns> constexpr QuirkMatch&& documentDomainIsOneOf() &&
    {
        static_assert(std::size(patterns), "A quirk must match at least one document domain.");
        currentRefinements().documentDomains = PatternList { std::span<const ASCIILiteral> { patterns } };
        return WTF::move(*this);
    }

    template<const auto& patterns> constexpr QuirkMatch&& hostIsOneOf() &&
    {
        static_assert(std::size(patterns), "A quirk must match at least one host.");
        currentRefinements().hosts = std::span<const ASCIILiteral> { patterns };
        return WTF::move(*this);
    }

    constexpr QuirkMatch&& onlyIfEmbedded() &&
    {
        currentRefinements().requiresEmbeddedDocument = true;
        return WTF::move(*this);
    }

    constexpr QuirkMatch&& exceptWhen() &&
    {
        m_refiningException = true;
        return WTF::move(*this);
    }

    bool matches(const QuirkMatchContext& context) const
    {
        if (!matchesSite(context)) [[likely]]
            return false;

        if (!matchesRefinements(m_refinements, context))
            return false;

        if (!m_exception.isEmpty() && matchesRefinements(m_exception, context))
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
        AnySite,
    };

    enum class PathConstraintKind : uint8_t {
        PathContains,
        PathStartsWith,
        PathOrFragmentContains,
    };

    class PatternList {
    public:
        constexpr PatternList() = default;

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
            if (!m_multiple.empty())
                return m_multiple;
            if (m_single.isNull())
                return { };
            return singleElementSpan(m_single);
        }

        constexpr bool isEmpty() const { return m_multiple.empty() && m_single.isNull(); }

    private:
        ASCIILiteral m_single;
        std::span<const ASCIILiteral> m_multiple;
    };

    struct Refinements {
        PathConstraintKind pathConstraintKind { PathConstraintKind::PathContains };
        ASCIILiteral pathConstraint;
        std::optional<QuirkCondition> condition;
        PatternList documentDomains;
        std::span<const ASCIILiteral> hosts;
        bool requiresEmbeddedDocument { false };

        constexpr bool isEmpty() const
        {
            return !requiresEmbeddedDocument && pathConstraint.isNull() && !condition && documentDomains.isEmpty() && hosts.empty();
        }
    };

    constexpr explicit QuirkMatch(Kind kind)
        : m_kind(kind)
    {
    }

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

    constexpr Refinements& currentRefinements() LIFETIME_BOUND
    {
        return m_refiningException ? m_exception : m_refinements;
    }

    constexpr QuirkMatch&& setPathConstraint(PathConstraintKind kind, ASCIILiteral value) &&
    {
        auto& refinements = currentRefinements();
        refinements.pathConstraintKind = kind;
        refinements.pathConstraint = value;
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
        case Kind::AnySite:
            return !context.host().isEmpty();
        }

        ASSERT_NOT_REACHED();
        return false;
    }

    bool matchesRefinements(const Refinements& refinements, const QuirkMatchContext& context) const
    {
        if (!refinements.pathConstraint.isNull() && !matchesPathConstraint(refinements, context.topURL()))
            return false;

        if (refinements.condition && !evaluateQuirkCondition(*refinements.condition))
            return false;

        if (refinements.requiresEmbeddedDocument && context.isTopDocument())
            return false;

        if (!refinements.documentDomains.isEmpty() && !anyOf(refinements.documentDomains.span(), context.documentDomain()))
            return false;

        if (!refinements.hosts.empty() && !anyOf(refinements.hosts, context.host()))
            return false;

        return true;
    }

    template<typename StringType> static bool anyOf(std::span<const ASCIILiteral> patterns, const StringType& value)
    {
        return std::ranges::any_of(patterns, [&](ASCIILiteral pattern) { return value == pattern; });
    }

    bool matchesPathConstraint(const Refinements& refinements, const URL& topURL) const
    {
        switch (refinements.pathConstraintKind) {
        case PathConstraintKind::PathContains:
            return topURL.path().contains(refinements.pathConstraint);
        case PathConstraintKind::PathStartsWith:
            return startsWithLettersIgnoringASCIICase(topURL.path(), refinements.pathConstraint);
        case PathConstraintKind::PathOrFragmentContains:
            return topURL.path().contains(refinements.pathConstraint) || topURL.fragmentIdentifier().contains(refinements.pathConstraint);
        }

        ASSERT_NOT_REACHED();
        return false;
    }

    Kind m_kind;
    PatternList m_patterns;
    Refinements m_refinements;
    Refinements m_exception;
    bool m_refiningException { false };
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

    void apply(QuirksData& quirksData) const
    {
        quirksData.activeQuirks.merge(behaviors);

        if (site)
            quirksData.addSite(*site);
    }
};

WEBCORE_EXPORT QuirksData resolveSiteSpecificQuirks(const QuirkMatchContext&);

} // namespace WebCore
