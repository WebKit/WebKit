/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/ImmutableStyleProperties.h>
#include <WebCore/ScopedName.h>
#include <WebCore/StylePositionTryFallbackTactic.h>
#include <WebCore/StyleValueTypes.h>
#include <optional>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {
namespace Style {

// <position-try-fallback> = [ [<dashed-ident> || <try-tactic>] | <position-area> ]
// https://drafts.csswg.org/css-anchor-position-1/#propdef-position-try-fallbacks
struct PositionTryFallback {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(PositionTryFallback);

    using Tactic = PositionTryFallbackTactic;

    struct RuleAndTactics {
        Markable<ScopedName> rule { };
        ListOrNullopt<SpaceSeparatedVector<Tactic>> tactics { };
        bool operator==(const RuleAndTactics&) const = default;
    };
    struct PositionArea {
        RefPtr<const ImmutableStyleProperties> properties { };
        bool operator==(const PositionArea&) const;
    };

    // Only one of these is valid at a time.
    // FIXME: Use a Variant<PositionTryFallback::RuleAndTactics, PositionTryFallback::PositionArea> to enforce this invariant, and then make PositionArea use a Ref instead of a RefPtr.
    RuleAndTactics ruleAndTactics { };
    PositionArea positionArea { };

    bool isPositionArea() const { return !!positionArea.properties; }
    bool isRuleAndTactics() const { return !positionArea.properties; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
        if (isPositionArea())
            return visitor(positionArea);
        return visitor(ruleAndTactics);
    }

    bool operator==(const PositionTryFallback&) const;
};

template<size_t I> const auto& get(const PositionTryFallback::RuleAndTactics& value)
{
    if constexpr (!I)
        return value.rule;
    else if constexpr (I == 1)
        return value.tactics;
}

// MARK: - Conversion

template<> struct CSSValueConversion<PositionTryFallback> { auto operator()(BuilderState&, const CSSValue&) -> PositionTryFallback; };

// `PositionTryFallback::PositionArea` is special-cased to handle `ImmutableStyleProperties`.
template<> struct CSSValueCreation<PositionTryFallback::PositionArea> { auto operator()(CSSValuePool&, const RenderStyle&, const PositionTryFallback::PositionArea&) -> Ref<CSSValue>; };

// MARK: - Serialization

// `PositionTryFallback::PositionArea` is special-cased to handle `ImmutableStyleProperties`.
template<> struct Serialize<PositionTryFallback::PositionArea> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const PositionTryFallback::PositionArea&); };

// MARK: - Logging

// `PositionTryFallback::PositionArea` is special-cased to handle `ImmutableStyleProperties`.
TextStream& operator<<(TextStream&, const PositionTryFallback::PositionArea&);

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::PositionTryFallback)
DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::PositionTryFallback::RuleAndTactics, 2)
