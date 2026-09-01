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

#include <WebCore/QuirkNames.h>
#include <WebCore/QuirksData.h>
#include <WebCore/URLMatch.h>
#include <array>
#include <initializer_list>
#include <optional>
#include <span>
#include <wtf/Vector.h>
#include <wtf/text/ASCIILiteral.h>

namespace WebCore {

class Document;
class Node;

struct QuirkBehavior {
    constexpr QuirkBehavior(SiteSpecificQuirk quirk)
        : quirk(quirk)
    {
    }

    constexpr QuirkBehavior(SiteSpecificQuirk quirk, ASCIILiteral selector)
        : quirk(quirk)
        , selector(selector)
    {
    }

    SiteSpecificQuirk quirk;
    ASCIILiteral selector;
};

struct QuirkElementCondition {
    SiteSpecificQuirk quirk { };
    ASCIILiteral selector;
};

[[nodiscard]] WEBCORE_EXPORT bool quirkSelectorMatches(ASCIILiteral selector, const Node*);

class QuirkBehaviors {
public:
    constexpr QuirkBehaviors() = default;

    consteval QuirkBehaviors(std::initializer_list<QuirkBehavior> behaviors)
    {
        for (auto& behavior : behaviors) {
            RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(!m_bits.get(static_cast<size_t>(behavior.quirk)));
            m_bits.set(static_cast<size_t>(behavior.quirk));

            if (behavior.selector.isNull())
                continue;

            RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(m_conditionCount < maximumConditions);
            m_conditions[m_conditionCount++] = { behavior.quirk, behavior.selector };
        }
    }

    constexpr const QuirkBitSet& bits() const LIFETIME_BOUND { return m_bits; }

    constexpr std::span<const QuirkElementCondition> conditions() const LIFETIME_BOUND
    {
        return std::span { m_conditions }.first(m_conditionCount);
    }

    constexpr QuirkBitSet unconditionalBits() const
    {
        auto bits = m_bits;
        bits.exclude(conditionalBits());
        return bits;
    }

    constexpr QuirkBitSet conditionalBits() const
    {
        QuirkBitSet bits;
        for (auto& condition : conditions())
            bits.set(static_cast<size_t>(condition.quirk));
        return bits;
    }

    constexpr void exclude(const QuirkBitSet& quirks)
    {
        m_bits.exclude(quirks);

        size_t remaining = 0;
        for (size_t i = 0; i < m_conditionCount; ++i) {
            if (!quirks.get(static_cast<size_t>(m_conditions[i].quirk)))
                m_conditions[remaining++] = m_conditions[i];
        }
        m_conditionCount = remaining;
    }

private:
    // Every row pays for this capacity; raise it only when a row needs more.
    static constexpr size_t maximumConditions = 2;

    QuirkBitSet m_bits;
    std::array<QuirkElementCondition, maximumConditions> m_conditions { };
    size_t m_conditionCount { 0 };
};

enum class IsTopDocument : bool { No, Yes };

class QuirkURLMatch {
public:
    constexpr QuirkURLMatch(URLMatch match)
        : m_kind(Kind::TopURL)
        , m_match(match)
    {
    }

    static constexpr QuirkURLMatch embeddedDocument(URLMatch match)
    {
        return QuirkURLMatch { Kind::EmbeddedDocument, match };
    }

    static constexpr QuirkURLMatch embeddedDocumentInTopMatch(URLMatch topMatch, URLMatch documentMatch)
    {
        return QuirkURLMatch { Kind::EmbeddedDocumentInTopURL, documentMatch, topMatch };
    }

    [[nodiscard]] WEBCORE_EXPORT bool matches(const URLMatchContext& topContext, const URLMatchContext& documentContext, IsTopDocument) const;

private:
    enum class Kind : uint8_t { TopURL, EmbeddedDocument, EmbeddedDocumentInTopURL };

    constexpr QuirkURLMatch(Kind kind, URLMatch match, std::optional<URLMatch> topMatch = std::nullopt)
        : m_kind(kind)
        , m_match(match)
        , m_topMatch(topMatch)
    {
    }

    Kind m_kind;
    URLMatch m_match;
    std::optional<URLMatch> m_topMatch;
};

struct ResolvedQuirks {
    QuirksData data;
    Vector<QuirkElementCondition, 2> elementConditions;
};

struct Quirk {
    QuirkURLMatch match;
    QuirkBehaviors behaviors { };
    std::optional<QuirkSite> site { };
    bool availableWhen { true };

    void apply(ResolvedQuirks&) const;
};

WEBCORE_EXPORT ResolvedQuirks resolveSiteSpecificQuirks(const URL& topURL, const URL& documentURL, IsTopDocument);

WEBCORE_EXPORT QuirksData resolveTopURLQuirks(const URL&);

WEBCORE_EXPORT std::span<const Quirk> quirkTableForTesting();

[[nodiscard]] WEBCORE_EXPORT bool quirkSelectorParsesForTesting(ASCIILiteral, const Document&);

} // namespace WebCore
