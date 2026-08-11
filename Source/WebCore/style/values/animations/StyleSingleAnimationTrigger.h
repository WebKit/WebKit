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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleCustomIdent.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// https://drafts.csswg.org/animation-triggers-1/#typedef-animation-action
// <animation-action> = none | play | play-once | play-forwards | play-backwards | pause | reset | replay
using AnimationAction = Variant<CSS::Keyword::None, CSS::Keyword::Play, CSS::Keyword::PlayOnce, CSS::Keyword::PlayForwards, CSS::Keyword::PlayBackwards, CSS::Keyword::Pause, CSS::Keyword::Reset, CSS::Keyword::Replay>;

// <named-animation-trigger> = [ <dashed-ident> <animation-action>+ ]
struct NamedAnimationTrigger {
    CustomIdent name;
    SpaceSeparatedFixedVector<AnimationAction> actions;

    bool operator==(const NamedAnimationTrigger&) const = default;
};

template<size_t I> const auto& get(const NamedAnimationTrigger& value)
{
    if constexpr (!I)
        return value.name;
    else if constexpr (I == 1)
        return value.actions;
}

// <named-animation-triggers> = [ <dashed-ident> <animation-action>+ ]+
using NamedAnimationTriggers = SpaceSeparatedFixedVector<NamedAnimationTrigger>;

// <single-animation-trigger> = none | [ <dashed-ident> <animation-action>+ ]+
// https://drafts.csswg.org/animation-triggers-1/#propdef-animation-trigger
struct SingleAnimationTrigger : ListOrNone<NamedAnimationTriggers> {
    using ListOrNone<NamedAnimationTriggers>::ListOrNone;
};

// MARK: - Conversion

template<> struct CSSValueConversion<NamedAnimationTrigger> { auto operator()(BuilderState&, const CSSValue&) -> NamedAnimationTrigger; };
template<> struct CSSValueConversion<AnimationAction> { auto operator()(BuilderState&, const CSSValue&) -> AnimationAction; };

} // namespace Style
} // namespace WebCore

DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::NamedAnimationTrigger, 2)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::SingleAnimationTrigger)
