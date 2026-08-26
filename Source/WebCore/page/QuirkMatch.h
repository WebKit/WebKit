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

#include <WebCore/QuirksData.h>
#include <array>
#include <initializer_list>
#include <optional>
#include <span>
#include <wtf/Assertions.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

enum class QuirkEnvironment : uint8_t {
    SmallScreen,
    TubularApp,
    LensApp,
};

WEBCORE_EXPORT bool evaluateQuirkEnvironment(QuirkEnvironment);

enum class IsTopDocument : bool { No, Yes };

class QuirkMatchContext {
public:
    QuirkMatchContext(URL topURL, URL documentURL, IsTopDocument isTopDocument)
        : m_topURL(WTF::move(topURL))
        , m_documentURL(WTF::move(documentURL))
        , m_isTopDocument(isTopDocument)
    {
    }

    const URL& topURL() const LIFETIME_BOUND { return m_topURL; }

    bool isTopDocument() const { return m_isTopDocument == IsTopDocument::Yes; }

    StringView topHost() const LIFETIME_BOUND { return m_topURL.host(); }

    WEBCORE_EXPORT const String& topRegistrableDomain() const LIFETIME_BOUND;

    WEBCORE_EXPORT const String& topDomainWithoutPublicSuffix() const LIFETIME_BOUND;

    WEBCORE_EXPORT const String& documentRegistrableDomain() const LIFETIME_BOUND;

private:
    const URL m_topURL;
    const URL m_documentURL;
    const IsTopDocument m_isTopDocument;
    mutable std::optional<String> m_topRegistrableDomain;
    mutable std::optional<String> m_topDomainWithoutPublicSuffix;
    mutable std::optional<String> m_documentRegistrableDomain;
};

class QuirkPatternList {
public:
    constexpr QuirkPatternList() = default;

    constexpr QuirkPatternList(ASCIILiteral pattern)
        : m_single(pattern)
    {
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(!pattern.isNull());
    }

    template<size_t size> constexpr QuirkPatternList(const std::array<ASCIILiteral, size>& patterns LIFETIME_BOUND)
        : m_multiple(patterns)
    {
        static_assert(size, "A quirk pattern list must name at least one pattern.");
    }

    constexpr bool isEmpty() const { return m_single.isNull() && m_multiple.empty(); }

    bool contains(StringView value) const
    {
        return containsMatching([&](ASCIILiteral pattern) { return value == pattern; });
    }

    bool containsMatching(NOESCAPE const Invocable<bool(ASCIILiteral)> auto& predicate) const
    {
        for (auto pattern : span()) {
            if (predicate(pattern))
                return true;
        }
        return false;
    }

private:
    constexpr std::span<const ASCIILiteral> span() const LIFETIME_BOUND
    {
        if (!m_multiple.empty())
            return m_multiple;
        if (m_single.isNull())
            return { };
        return singleElementSpan(m_single);
    }

    ASCIILiteral m_single;
    std::span<const ASCIILiteral> m_multiple;
};

namespace QuirkRefinement {

struct PathContains {
    ASCIILiteral substring;
};

struct PathStartsWith {
    ASCIILiteral prefix;
};

struct PathOrFragmentContains {
    ASCIILiteral substring;
};

struct DocumentDomainIs {
    QuirkPatternList domains;
};

struct HostIs {
    QuirkPatternList hosts;
};

struct EnvironmentIs {
    QuirkEnvironment environment;
};

struct Embedded { };

constexpr PathContains pathContains(ASCIILiteral substring)
{
    return { substring };
}

constexpr PathStartsWith pathStartsWith(ASCIILiteral prefix)
{
    return { prefix };
}

constexpr PathOrFragmentContains pathOrFragmentContains(ASCIILiteral substring)
{
    return { substring };
}

constexpr DocumentDomainIs documentDomainIs(QuirkPatternList domains)
{
    return { domains };
}

constexpr HostIs hostIs(QuirkPatternList hosts)
{
    return { hosts };
}

constexpr Embedded embedded()
{
    return { };
}

constexpr EnvironmentIs smallScreen()
{
    return { QuirkEnvironment::SmallScreen };
}

constexpr EnvironmentIs tubularApp()
{
    return { QuirkEnvironment::TubularApp };
}

constexpr EnvironmentIs lensApp()
{
    return { QuirkEnvironment::LensApp };
}

} // namespace QuirkRefinement

// QuirkMatch represents a declarative description of which pages a quirk applies to
//
// Every match starts with exactly one static factory, which picks how the site is
// identified. Each takes a single pattern or a list of them:
//
//     domain()             the registrable domain, so "youtube.com" also covers
//                          player.youtube.com but not youtube.co.uk
//     host()               the exact host, so "docs.google.com" covers nothing else
//     hostOrSubdomainOf()  the host or any subdomain of it, respecting label boundaries,
//                          for HTTP-family URLs only
//     anyTopLevelDomain()  the domain under every public suffix, so "amazon" covers
//                          amazon.com and amazon.co.uk
//     anySite()            every page with a host, for quirks keyed only on refinements
//
// when() then narrows the match with QuirkRefinement refinements, all of which are ANDed
// together:
//
//     QuirkMatch::anyTopLevelDomain("apple"_s).when(pathStartsWith("/store"_s))
//
// exceptWhen() takes the same refinements to carve matching pages back out, so a
// refinement reads identically in either position and its scope is bounded by the call:
//
//     QuirkMatch::domain("wix.com"_s).exceptWhen(pathStartsWith("/website/templates/"_s))
//
class QuirkMatch {
public:
    static constexpr QuirkMatch domain(QuirkPatternList domains)
    {
        return QuirkMatch { Kind::Domain, domains };
    }

    static constexpr QuirkMatch host(QuirkPatternList hosts)
    {
        return QuirkMatch { Kind::Host, hosts };
    }

    static constexpr QuirkMatch hostOrSubdomainOf(QuirkPatternList hosts)
    {
        return QuirkMatch { Kind::HostOrSubdomainOf, hosts };
    }

    static constexpr QuirkMatch anyTopLevelDomain(QuirkPatternList names)
    {
        return QuirkMatch { Kind::AnyTopLevelDomain, names };
    }

    static constexpr QuirkMatch anySite()
    {
        return QuirkMatch { Kind::AnySite };
    }

    template<typename... Refinements> constexpr QuirkMatch when(Refinements... refinements) &&
    {
        static_assert(sizeof...(refinements), "when() must name at least one refinement to match.");

        (applyRefinement(m_refinements, refinements), ...);
        return WTF::move(*this);
    }

    template<typename... Refinements> constexpr QuirkMatch exceptWhen(Refinements... refinements) &&
    {
        static_assert(sizeof...(refinements), "exceptWhen() must name at least one refinement to exclude.");
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(!m_exception);

        RefinementSet exception;
        (applyRefinement(exception, refinements), ...);
        m_exception = exception;
        return WTF::move(*this);
    }

    WEBCORE_EXPORT bool matches(const QuirkMatchContext&) const;

private:
    enum class Kind : uint8_t {
        Domain,
        Host,
        HostOrSubdomainOf,
        AnyTopLevelDomain,
        AnySite,
    };

    enum class PathComparison : uint8_t {
        PathContains,
        PathStartsWith,
        PathOrFragmentContains,
    };

    struct RefinementSet {
        PathComparison pathComparison { PathComparison::PathContains };
        ASCIILiteral pathPattern;
        std::optional<QuirkEnvironment> environment;
        QuirkPatternList documentDomains;
        QuirkPatternList hosts;
        bool requiresEmbeddedDocument { false };

        bool matches(const QuirkMatchContext&) const;

    private:
        bool matchesPathPattern(const URL& topURL) const;
    };

    static constexpr void setPathPattern(RefinementSet& set, PathComparison comparison, ASCIILiteral pattern)
    {
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(set.pathPattern.isNull());
        set.pathComparison = comparison;
        set.pathPattern = pattern;
    }

    static constexpr void applyRefinement(RefinementSet& set, QuirkRefinement::PathContains refinement)
    {
        setPathPattern(set, PathComparison::PathContains, refinement.substring);
    }

    static constexpr void applyRefinement(RefinementSet& set, QuirkRefinement::PathStartsWith refinement)
    {
        setPathPattern(set, PathComparison::PathStartsWith, refinement.prefix);
    }

    static constexpr void applyRefinement(RefinementSet& set, QuirkRefinement::PathOrFragmentContains refinement)
    {
        setPathPattern(set, PathComparison::PathOrFragmentContains, refinement.substring);
    }

    static constexpr void applyRefinement(RefinementSet& set, QuirkRefinement::DocumentDomainIs refinement)
    {
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(set.documentDomains.isEmpty());
        set.documentDomains = refinement.domains;
    }

    static constexpr void applyRefinement(RefinementSet& set, QuirkRefinement::HostIs refinement)
    {
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(set.hosts.isEmpty());
        set.hosts = refinement.hosts;
    }

    static constexpr void applyRefinement(RefinementSet& set, QuirkRefinement::EnvironmentIs refinement)
    {
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(!set.environment);
        set.environment = refinement.environment;
    }

    static constexpr void applyRefinement(RefinementSet& set, QuirkRefinement::Embedded)
    {
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(!set.requiresEmbeddedDocument);
        set.requiresEmbeddedDocument = true;
    }

    constexpr explicit QuirkMatch(Kind kind)
        : m_kind(kind)
    {
    }

    constexpr QuirkMatch(Kind kind, QuirkPatternList patterns)
        : m_kind(kind)
        , m_patterns(patterns)
    {
    }

    bool matchesSite(const QuirkMatchContext&) const;

    Kind m_kind;
    QuirkPatternList m_patterns;
    RefinementSet m_refinements;
    std::optional<RefinementSet> m_exception;
};

class QuirkBehaviors {
public:
    constexpr QuirkBehaviors() = default;

    consteval QuirkBehaviors(std::initializer_list<QuirksData::SiteSpecificQuirk> quirks)
    {
        for (auto quirk : quirks)
            m_bits.set(static_cast<size_t>(quirk));
    }

    constexpr const QuirksData::QuirkBitSet& bits() const LIFETIME_BOUND { return m_bits; }

private:
    QuirksData::QuirkBitSet m_bits;
};

struct Quirk {
    QuirkMatch match;
    QuirkBehaviors behaviors { };
    std::optional<QuirkSite> site { };

    void apply(QuirksData&) const;
};

WEBCORE_EXPORT QuirksData resolveSiteSpecificQuirks(const QuirkMatchContext&);

} // namespace WebCore
