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
struct Contrast;
}

namespace Style {

// contrast() = contrast( [ <number [0,∞]> | <percentage [0,∞]> ]?@(default=1) )
// https://drafts.fxtf.org/filter-effects/#funcdef-filter-contrast
struct Contrast {
    using Parameter = Number<CSS::Nonnegative>;

    Parameter value;

    static Contrast passthroughForInterpolation();

    constexpr bool requiresRepaintForCurrentColorChange() const { return false; }
    constexpr bool affectsOpacity() const { return false; }
    constexpr bool movesPixels() const { return false; }
    constexpr bool shouldBeRestrictedBySecurityOrigin() const { return false; }
    bool isIdentity() const { return value == 1; }

    bool transformColor(SRGBA<float>&) const;
    constexpr bool inverseTransformColor(SRGBA<float>&) const { return false; }

    bool operator==(const Contrast&) const = default;
};
using ContrastFunction = FunctionNotation<CSSValueContrast, Contrast>;
DEFINE_TYPE_WRAPPER_GET(Contrast, value);

// MARK: - Conversion

template<> struct ToCSS<Contrast> { auto operator()(const Contrast&, const RenderStyle&) -> CSS::Contrast; };
template<> struct ToStyle<CSS::Contrast> { auto operator()(const CSS::Contrast&, const BuilderState&) -> Contrast; };

// MARK: - Blending

template<> struct Blending<Contrast> {
    auto blend(const Contrast&, const Contrast&, const BlendingContext&) -> Contrast;
};

// MARK: - Evaluation

template<> struct Evaluation<Contrast, Ref<FilterEffect>> { auto operator()(const Contrast&) -> Ref<FilterEffect>; };

// MARK: - Platform

template<> struct ToPlatform<Contrast> { auto operator()(const Contrast&) -> Ref<FilterOperation>; };

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::Style::Contrast, 1)
