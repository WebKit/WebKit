/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "ScopedName.h"
#include "StyleValueTypes.h"

namespace WebCore {

class StyleProperties;

namespace Style {

// <single-position-try-fallback> = [ [<dashed-ident> || <try-tactic>] | <position-area> ]
struct PositionTryFallback {
    std::optional<ScopedName> positionTryRuleName { };

    enum class Tactic : uint8_t {
        FlipBlock,
        FlipInline,
        FlipStart
    };
    Vector<Tactic> tactics { };

    // A position-area fallback is mutually exclusive with the rest.
    RefPtr<const StyleProperties> positionAreaProperties { };

    ~PositionTryFallback();

    bool operator==(const PositionTryFallback&) const;
};

// <position-try-fallback-list> = <single-position-try-fallback>#
using PositionTryFallbackList = CommaSeparatedFixedVector<PositionTryFallback>;

// <'position-try-fallbacks'> = none | <position-try-fallback-list>
// https://drafts.csswg.org/css-anchor-position-1/#propdef-position-try-fallbacks
struct PositionTryFallbacks : ListOrNone<PositionTryFallbackList> { using ListOrNone<PositionTryFallbackList>::ListOrNone; };

// MARK: - Conversion

template<> struct CSSValueConversion<PositionTryFallback> { auto operator()(BuilderState&, const CSSValue&) -> PositionTryFallback; };
template<> struct CSSValueCreation<PositionTryFallback> { auto operator()(CSSValuePool&, const RenderStyle&, const PositionTryFallback&) -> Ref<CSSValue>; };

// MARK: - Serialization

template<> struct Serialize<PositionTryFallback> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const PositionTryFallback&); };

// MARK: - Logging

TextStream& operator<<(TextStream&, const PositionTryFallback&);

} // namespace Style
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::PositionTryFallbacks> = true;
