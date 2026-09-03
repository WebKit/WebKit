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

#include <array>
#include <optional>
#include <span>
#include <wtf/Assertions.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

// URLMatch is a declarative description of a set of URLs
//
// Every match starts with exactly one static factory, which picks how the URL is
// identified. Each takes a single pattern or a list of them:
//
//     domain()             the registrable domain, so "youtube.com" also covers
//                          player.youtube.com but not youtube.co.uk
//     host()               the exact host, so "docs.google.com" covers nothing else
//     hostOrSubdomainOf()  the host or any subdomain of it, respecting label boundaries,
//                          for HTTP-family URLs only
//     anyTopLevelDomain()  the domain under every public suffix, so "amazon" covers
//                          amazon.com and amazon.co.uk
//     anyURL()             every URL with a host, for matches keyed only on refinements
//
// when() then narrows the match with URLRefinement refinements, all of which are ANDed
// together:
//
//     URLMatch::anyTopLevelDomain("apple"_s).when(pathStartsWith("/store"_s))
//
// exceptWhen() takes the same refinements to carve matching URLs back out, so a
// refinement reads identically in either position and its scope is bounded by the call:
//
//     URLMatch::domain("wix.com"_s).exceptWhen(pathStartsWith("/website/templates/"_s))
//

namespace WebCore {

enum class URLEnvironment : uint8_t {
    SmallScreen,
    TubularApp,
    LensApp,
};

WEBCORE_EXPORT bool evaluateURLEnvironment(URLEnvironment);

class URLMatchContext {
public:
    URLMatchContext(URL url)
        : m_url(WTF::move(url))
    {
    }

    const URL& url() const LIFETIME_BOUND { return m_url; }

    StringView host() const LIFETIME_BOUND { return m_url.host(); }

    WEBCORE_EXPORT const String& domainWithoutPublicSuffix() const LIFETIME_BOUND;

    WEBCORE_EXPORT const String& registrableDomain() const LIFETIME_BOUND;

private:
    const URL m_url;
    mutable std::optional<String> m_registrableDomain;
    mutable std::optional<String> m_domainWithoutPublicSuffix;
};

class URLPatternList {
public:
    constexpr URLPatternList() = default;

    constexpr URLPatternList(ASCIILiteral pattern)
        : m_single(pattern)
    {
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(!pattern.isNull());
    }

    template<size_t size> constexpr URLPatternList(const std::array<ASCIILiteral, size>& patterns LIFETIME_BOUND)
        : m_multiple(patterns)
    {
        static_assert(size, "A URL pattern list must name at least one pattern.");
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

namespace URLRefinement {

struct PathContains {
    ASCIILiteral substring;
};

struct PathStartsWith {
    ASCIILiteral prefix;
};

struct PathIs {
    ASCIILiteral path;
};

struct PathOrFragmentContains {
    ASCIILiteral substring;
};

struct QueryContains {
    ASCIILiteral substring;
};

struct HostIs {
    URLPatternList hosts;
};

struct EnvironmentIs {
    URLEnvironment environment;
};

constexpr PathContains pathContains(ASCIILiteral substring)
{
    return { substring };
}

constexpr PathStartsWith pathStartsWith(ASCIILiteral prefix)
{
    return { prefix };
}

constexpr PathIs pathIs(ASCIILiteral path)
{
    return { path };
}

constexpr PathOrFragmentContains pathOrFragmentContains(ASCIILiteral substring)
{
    return { substring };
}

constexpr QueryContains queryContains(ASCIILiteral substring)
{
    return { substring };
}

constexpr HostIs hostIs(URLPatternList hosts)
{
    return { hosts };
}

constexpr EnvironmentIs smallScreen()
{
    return { URLEnvironment::SmallScreen };
}

constexpr EnvironmentIs tubularApp()
{
    return { URLEnvironment::TubularApp };
}

constexpr EnvironmentIs lensApp()
{
    return { URLEnvironment::LensApp };
}

} // namespace URLRefinement

class URLMatch {
public:
    static constexpr URLMatch domain(URLPatternList domains)
    {
        return URLMatch { Kind::Domain, domains };
    }

    static constexpr URLMatch host(URLPatternList hosts)
    {
        return URLMatch { Kind::Host, hosts };
    }

    static constexpr URLMatch hostOrSubdomainOf(URLPatternList hosts)
    {
        return URLMatch { Kind::HostOrSubdomainOf, hosts };
    }

    static constexpr URLMatch anyTopLevelDomain(URLPatternList names)
    {
        return URLMatch { Kind::AnyTopLevelDomain, names };
    }

    static constexpr URLMatch anyURL()
    {
        return URLMatch { Kind::Any };
    }

    template<typename... Refinements> constexpr URLMatch when(Refinements... refinements) &&
    {
        static_assert(sizeof...(refinements), "when() must name at least one refinement to match.");

        (applyRefinement(m_refinements, refinements), ...);
        return WTF::move(*this);
    }

    template<typename... Refinements> constexpr URLMatch exceptWhen(Refinements... refinements) &&
    {
        static_assert(sizeof...(refinements), "exceptWhen() must name at least one refinement to exclude.");
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(!m_exception);

        RefinementSet exception;
        (applyRefinement(exception, refinements), ...);
        m_exception = exception;
        return WTF::move(*this);
    }

    WEBCORE_EXPORT bool matches(const URLMatchContext&) const;

private:
    enum class Kind : uint8_t {
        Domain,
        Host,
        HostOrSubdomainOf,
        AnyTopLevelDomain,
        Any,
    };

    enum class PathComparison : uint8_t {
        PathContains,
        PathStartsWith,
        PathIs,
        PathOrFragmentContains,
    };

    struct RefinementSet {
        PathComparison pathComparison { PathComparison::PathContains };
        ASCIILiteral pathPattern;
        ASCIILiteral queryPattern;
        std::optional<URLEnvironment> environment;
        URLPatternList hosts;

        bool matches(const URLMatchContext&) const;

    private:
        bool matchesPathPattern(const URL&) const;
    };

    static constexpr void setPathPattern(RefinementSet& set, PathComparison comparison, ASCIILiteral pattern)
    {
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(set.pathPattern.isNull());
        set.pathComparison = comparison;
        set.pathPattern = pattern;
    }

    static constexpr void applyRefinement(RefinementSet& set, URLRefinement::PathContains refinement)
    {
        setPathPattern(set, PathComparison::PathContains, refinement.substring);
    }

    static constexpr void applyRefinement(RefinementSet& set, URLRefinement::PathStartsWith refinement)
    {
        setPathPattern(set, PathComparison::PathStartsWith, refinement.prefix);
    }

    static constexpr void applyRefinement(RefinementSet& set, URLRefinement::PathIs refinement)
    {
        setPathPattern(set, PathComparison::PathIs, refinement.path);
    }

    static constexpr void applyRefinement(RefinementSet& set, URLRefinement::PathOrFragmentContains refinement)
    {
        setPathPattern(set, PathComparison::PathOrFragmentContains, refinement.substring);
    }

    static constexpr void applyRefinement(RefinementSet& set, URLRefinement::QueryContains refinement)
    {
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(set.queryPattern.isNull());
        set.queryPattern = refinement.substring;
    }

    static constexpr void applyRefinement(RefinementSet& set, URLRefinement::HostIs refinement)
    {
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(set.hosts.isEmpty());
        set.hosts = refinement.hosts;
    }

    static constexpr void applyRefinement(RefinementSet& set, URLRefinement::EnvironmentIs refinement)
    {
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(!set.environment);
        set.environment = refinement.environment;
    }

    constexpr explicit URLMatch(Kind kind)
        : m_kind(kind)
    {
    }

    constexpr URLMatch(Kind kind, URLPatternList patterns)
        : m_kind(kind)
        , m_patterns(patterns)
    {
    }

    bool matchesURL(const URLMatchContext&) const;

    Kind m_kind;
    URLPatternList m_patterns;
    RefinementSet m_refinements;
    std::optional<RefinementSet> m_exception;
};

} // namespace WebCore
