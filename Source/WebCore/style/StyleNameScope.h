/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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

#include <WebCore/StyleScopeOrdinal.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <name-scope> = none | all | <dashed-ident>#
// Used by:
// <'timeline-scope'>
// https://drafts.csswg.org/scroll-animations-1/#propdef-timeline-scope
// <'anchor-scope'>
// https://drafts.csswg.org/css-anchor-position-1/#propdef-anchor-scope

struct NameScope {
    enum class Type : uint8_t { None, All, Ident };

    NameScope(CSS::Keyword::None)
    {
    }

    NameScope(CSS::Keyword::All, ScopeOrdinal scopeOrdinal)
        : type { Type::All }
        , scopeOrdinal { scopeOrdinal }
    {
    }

    NameScope(CommaSeparatedListHashSet<CustomIdentifier>&& names, ScopeOrdinal scopeOrdinal)
        : type { Type::Ident }
        , names { WTFMove(names) }
        , scopeOrdinal { scopeOrdinal }
    {
    }

    Type type { Type::None };
    CommaSeparatedListHashSet<CustomIdentifier> names;
    ScopeOrdinal scopeOrdinal { ScopeOrdinal::Element };

    template<typename... F> constexpr decltype(auto) switchOn(F&&...) const;

    inline bool operator==(const NameScope&) const;
};

inline bool NameScope::operator==(const NameScope& other) const
{
    return type == other.type
        && scopeOrdinal == other.scopeOrdinal
        // Two name lists are equal if they contain the same values in the same order.
        // FIXME: This is not symmetrical in the case that other.names.isEmpty() is true, but names.isEmpty() is false.
        && (names.isEmpty() || std::ranges::equal(names, other.names));
}

template<typename... F> constexpr decltype(auto) NameScope::switchOn(F&&... f) const
{
    auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

    switch (type) {
    case NameScope::Type::None:
        return visitor(CSS::Keyword::None { });
    case NameScope::Type::All:
        return visitor(CSS::Keyword::All { });
    case NameScope::Type::Ident:
        if (names.isEmpty())
            return visitor(CSS::Keyword::None { });
        return visitor(names);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

// MARK: - Conversion

template<> struct CSSValueConversion<NameScope> { auto operator()(BuilderState&, const CSSValue&) -> NameScope; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::NameScope)
