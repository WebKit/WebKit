/*
 * Copyright (C) 2024-2026 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/BoxExtents.h>
#include <WebCore/StyleColor.h>
#include <WebCore/StylePrimitiveNumericTypes.h>

namespace WebCore {

class FilterEffect;
class FilterOperation;

namespace CSS {
struct DropShadow;
}

namespace Style {

struct ZoomFactor;

// drop-shadow() = drop-shadow( [ <color>?@(default=currentColor) && [<length>{2} <length [0,∞]>?@(default=0px)] ] )
// https://drafts.fxtf.org/filter-effects/#funcdef-filter-drop-shadow
struct DropShadow {
    Color color;
    SpaceSeparatedPoint<Length<CSS::AllUnzoomed>> location;
    Length<CSS::NonnegativeUnzoomed> stdDeviation;

    static DropShadow passthroughForInterpolation();

    bool requiresRepaintForCurrentColorChange() const { return color.containsCurrentColor(); }
    constexpr bool affectsOpacity() const { return true; }
    constexpr bool movesPixels() const { return true; }
    constexpr bool shouldBeRestrictedBySecurityOrigin() const { return false; }
    bool isIdentity() const { return isZero(stdDeviation) && isZero(location); }

    IntOutsets calculateOutsets(ZoomFactor) const;

    bool operator==(const DropShadow&) const = default;
};
using DropShadowFunction = FunctionNotation<CSSValueDropShadow, DropShadow>;

template<size_t I> const auto& get(const DropShadow& value)
{
    if constexpr (!I)
        return value.color;
    else if constexpr (I == 1)
        return value.location;
    else if constexpr (I == 2)
        return value.stdDeviation;
}

// MARK: - Conversion

template<> struct ToCSS<DropShadow> { auto operator()(const DropShadow&, const RenderStyle&) -> CSS::DropShadow; };
template<> struct ToStyle<CSS::DropShadow> { auto operator()(const CSS::DropShadow&, const BuilderState&) -> DropShadow; };

// MARK: - Evaluation

template<> struct Evaluation<DropShadow, Ref<FilterEffect>> { auto operator()(const DropShadow&, const RenderStyle&) -> Ref<FilterEffect>; };

// MARK: - Platform

template<> struct ToPlatform<DropShadow> { auto operator()(const DropShadow&, const RenderStyle&) -> Ref<FilterOperation>; };

} // namespace Style
} // namespace WebCore

DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::DropShadow, 3)
