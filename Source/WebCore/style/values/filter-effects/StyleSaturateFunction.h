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
struct Saturate;
}

namespace Style {

// saturate() = saturate( [ <number [0,∞]> | <percentage [0,∞]> ]?@(default=1) )
// https://drafts.fxtf.org/filter-effects/#funcdef-filter-saturate
struct Saturate {
    using Parameter = Number<CSS::Nonnegative>;

    Parameter value;

    static Saturate passthroughForInterpolation();

    constexpr bool requiresRepaintForCurrentColorChange() const { return false; }
    constexpr bool affectsOpacity() const { return false; }
    constexpr bool movesPixels() const { return false; }
    constexpr bool shouldBeRestrictedBySecurityOrigin() const { return false; }
    bool isIdentity() const { return value == 1; }

    bool transformColor(SRGBA<float>&) const;
    constexpr bool inverseTransformColor(SRGBA<float>&) const { return false; }

    bool operator==(const Saturate&) const = default;
};
using SaturateFunction = FunctionNotation<CSSValueSaturate, Saturate>;
DEFINE_TYPE_WRAPPER_GET(Saturate, value);

// MARK: - Conversion

template<> struct ToCSS<Saturate> { auto operator()(const Saturate&, const RenderStyle&) -> CSS::Saturate; };
template<> struct ToStyle<CSS::Saturate> { auto operator()(const CSS::Saturate&, const BuilderState&) -> Saturate; };

// MARK: - Blending

template<> struct Blending<Saturate> {
    auto blend(const Saturate&, const Saturate&, const BlendingContext&) -> Saturate;
};

// MARK: - Evaluation

template<> struct Evaluation<Saturate, Ref<FilterEffect>> { auto operator()(const Saturate&) -> Ref<FilterEffect>; };

// MARK: - Platform

template<> struct ToPlatform<Saturate> { auto operator()(const Saturate&) -> Ref<FilterOperation>; };

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::Style::Saturate, 1)
