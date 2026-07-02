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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/CSSGridTrackBreadth.h>
#include <WebCore/CSSPrimitiveNumericTypes.h>

namespace WebCore {
namespace CSS {

// <minmax()> = minmax( <inflexible-breadth> , <track-breadth> )
//            | minmax( <fixed-breadth>      , <track-breadth> )
//            | minmax( <inflexible-breadth> , <fixed-breadth> )
struct GridMinMaxFunctionParameters {
    GridTrackBreadth min;
    GridTrackBreadth max;

    bool operator==(const GridMinMaxFunctionParameters&) const = default;
};
using GridMinMaxFunction = FunctionNotation<CSSValueMinmax, GridMinMaxFunctionParameters>;

template<size_t I> const auto& get(const GridMinMaxFunctionParameters& value)
{
    if constexpr (!I)
        return value.min;
    else if constexpr (I == 1)
        return value.max;
}

// <fit-content()> = fit-content( <length-percentage [0,∞]> )
struct GridFitContentFunctionParameters {
    LengthPercentage<Nonnegative> value;

    bool operator==(const GridFitContentFunctionParameters&) const = default;
};
using GridFitContentFunction = FunctionNotation<CSSValueFitContent, GridFitContentFunctionParameters>;
DEFINE_TYPE_WRAPPER_GET(GridFitContentFunctionParameters, value);


// <track-size>          = <track-breadth> | minmax( <inflexible-breadth> , <track-breadth> ) | fit-content( <length-percentage [0,∞]> )
// <fixed-size>          = <fixed-breadth> | minmax( <fixed-breadth> , <track-breadth> ) | minmax( <inflexible-breadth> , <fixed-breadth> )
// https://drafts.csswg.org/css-grid/#typedef-track-size
struct GridTrackSize {
    GridTrackSize(GridTrackBreadth&& breadth) : m_value { WTF::move(breadth) } { }
    GridTrackSize(GridMinMaxFunction&& function) : m_value { WTF::move(function) } { }
    GridTrackSize(GridFitContentFunction&& function) : m_value { WTF::move(function) } { }

    bool isBreadth() const { return WTF::holdsAlternative<GridTrackBreadth>(m_value); }
    bool isMinMaxFunction() const { return WTF::holdsAlternative<GridMinMaxFunction>(m_value); }
    bool isFitContentFunction() const { return WTF::holdsAlternative<GridFitContentFunction>(m_value); }

    bool isAuto() const
    {
        if (auto* breadth = std::get_if<GridTrackBreadth>(&m_value))
            return breadth->isAuto();
        return false;
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const GridTrackSize&) const = default;

private:
    Variant<GridTrackBreadth, GridMinMaxFunction, GridFitContentFunction> m_value;
};

} // namespace CSS
} // namespace WebCore

DEFINE_COMMA_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::GridMinMaxFunctionParameters, 2)
DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::CSS::GridFitContentFunctionParameters)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::CSS::GridTrackSize)
