/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include "CSSImageOrNone.h"
#include "CSSPrimitiveNumericTypes.h"
#include "CSSValueTypes.h"

namespace WebCore {
namespace CSS {

// https://drafts.csswg.org/css-images-4/#funcdef-cross-fade

// MARK: - -webkit-cross-fade()

// <-webkit-cross-fade()> = -webkit-cross-fade( [ <image> | none ] , [ <image> | none ] , [ <number [0,1]> | <percentage [0,100]> ] )
// Non-standard legacy syntax.
struct WebkitCrossfade {
    using Progress = NumberOrPercentageResolvedToNumber<ClosedUnitRangeClampBoth, ClosedPercentageRangeClampBoth>;

    ImageOrNone from;
    ImageOrNone to;
    Progress progress;

    bool operator==(const WebkitCrossfade&) const = default;
};
template<size_t I> const auto& get(const WebkitCrossfade& value)
{
    if constexpr (!I)
        return value.from;
    else if constexpr (I == 1)
        return value.to;
    else if constexpr (I == 2)
        return value.progress;
}
using WebkitCrossfadeFunction = FunctionNotation<CSSValueID::WebkitCrossFade, WebkitCrossfade>;

// MARK: - cross-fade()

// <cross-fade-component> = [ <image> | <color> ] && <percentage [0,100]>?
// FIXME: Add support for <color> parameter.
struct CrossfadeComponent {
    using Percentage = CSS::Percentage<ClosedPercentageRange>;

    ImageWrapper image;
    std::optional<Percentage> percentage;

    bool operator==(const CrossfadeComponent&) const = default;
};
template<size_t I> const auto& get(const CrossfadeComponent& value)
{
    if constexpr (!I)
        return value.image;
    else if constexpr (I == 1)
        return value.percentage;
}

// <cross-fade()> = cross-fade( <cross-fade-component># )
struct Crossfade {
    using Component = CrossfadeComponent;

    CommaSeparatedVector<Component> components;

    bool operator==(const Crossfade&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(Crossfade, components);
using CrossfadeFunction = FunctionNotation<CSSValueID::CrossFade, Crossfade>;

} // namespace CSS
} // namespace WebCore

DEFINE_COMMA_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::WebkitCrossfade, 3)
DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::CrossfadeComponent, 2)
DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::CSS::Crossfade)
