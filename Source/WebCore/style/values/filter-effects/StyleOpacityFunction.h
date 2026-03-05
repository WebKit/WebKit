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

#include <WebCore/ColorTypes.h>
#include <WebCore/StylePrimitiveNumericTypes.h>

namespace WebCore {

class FilterEffect;
class FilterOperation;

namespace CSS {
struct Opacity;
}

namespace Style {

// opacity() = opacity( [ <number [0,1(clamp upper)] > | <percentage [0,100(clamp upper)]> ]?@(default=1) )
// https://drafts.fxtf.org/filter-effects/#funcdef-filter-opacity
struct OpacityParameters {
    using Parameter = Number<CSS::ClosedUnitRangeClampUpper>;

    Parameter value;

    static OpacityParameters passthroughForInterpolation();

    constexpr bool requiresRepaintForCurrentColorChange() const { return false; }
    constexpr bool affectsOpacity() const { return true; }
    constexpr bool movesPixels() const { return false; }
    constexpr bool shouldBeRestrictedBySecurityOrigin() const { return false; }
    bool isIdentity() const { return value == 1; }

    bool transformColor(SRGBA<float>&) const;
    constexpr bool inverseTransformColor(SRGBA<float>&) const { return false; }

    bool operator==(const OpacityParameters&) const = default;
};
using OpacityFunction = FunctionNotation<CSSValueOpacity, OpacityParameters>;
DEFINE_TYPE_WRAPPER_GET(OpacityParameters, value);

// MARK: - Conversion

template<> struct ToCSS<OpacityParameters> { auto operator()(const OpacityParameters&, const RenderStyle&) -> CSS::Opacity; };
template<> struct ToStyle<CSS::Opacity> { auto operator()(const CSS::Opacity&, const BuilderState&) -> OpacityParameters; };

// MARK: - Blending

template<> struct Blending<OpacityParameters> {
    auto blend(const OpacityParameters&, const OpacityParameters&, const BlendingContext&) -> OpacityParameters;
};

// MARK: - Evaluation

template<> struct Evaluation<OpacityParameters, Ref<FilterEffect>> { auto operator()(const OpacityParameters&) -> Ref<FilterEffect>; };

// MARK: - Platform

template<> struct ToPlatform<OpacityParameters> { auto operator()(const OpacityParameters&) -> Ref<FilterOperation>; };

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::Style::OpacityParameters, 1)
