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
#include <initializer_list>
#include <optional>

namespace WebCore {

class QuirkBehaviors {
public:
    constexpr QuirkBehaviors() = default;

    consteval QuirkBehaviors(std::initializer_list<SiteSpecificQuirk> quirks)
    {
        for (auto quirk : quirks)
            m_bits.set(static_cast<size_t>(quirk));
    }

    constexpr const QuirkBitSet& bits() const LIFETIME_BOUND { return m_bits; }

    constexpr void exclude(const QuirkBitSet& quirks) { m_bits.exclude(quirks); }

private:
    QuirkBitSet m_bits;
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

struct Quirk {
    QuirkURLMatch match;
    QuirkBehaviors behaviors { };
    std::optional<QuirkSite> site { };
    bool availableWhen { true };

    void apply(QuirksData&) const;
};

WEBCORE_EXPORT QuirksData resolveSiteSpecificQuirks(const URL& topURL, const URL& documentURL, IsTopDocument);

// For callers with no Document, which therefore only see top-URL quirks.
WEBCORE_EXPORT QuirksData resolveTopURLQuirks(const URL&);

} // namespace WebCore
