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
struct Sepia;
}

namespace Style {

// sepia() = sepia( [ <number [0,1(clamp upper)] > | <percentage [0,100(clamp upper)]> ]?@(default=1) )
// https://drafts.fxtf.org/filter-effects/#funcdef-filter-sepia
struct Sepia {
    using Parameter = Number<CSS::ClosedUnitRangeClampUpper>;

    Parameter value;

    static Sepia passthroughForInterpolation();

    constexpr bool requiresRepaintForCurrentColorChange() const { return false; }
    constexpr bool affectsOpacity() const { return false; }
    constexpr bool movesPixels() const { return false; }
    constexpr bool shouldBeRestrictedBySecurityOrigin() const { return false; }
    bool isIdentity() const { return value.isZero(); }

    bool transformColor(SRGBA<float>&) const;
    constexpr bool inverseTransformColor(SRGBA<float>&) const { return false; }

    bool operator==(const Sepia&) const = default;
};
using SepiaFunction = FunctionNotation<CSSValueSepia, Sepia>;
DEFINE_TYPE_WRAPPER_GET(Sepia, value);

// MARK: - Conversion

template<> struct ToCSS<Sepia> { auto operator()(const Sepia&, const RenderStyle&) -> CSS::Sepia; };
template<> struct ToStyle<CSS::Sepia> { auto operator()(const CSS::Sepia&, const BuilderState&) -> Sepia; };

// MARK: - Evaluation

template<> struct Evaluation<Sepia, Ref<FilterEffect>> { auto operator()(const Sepia&) -> Ref<FilterEffect>; };

// MARK: - Platform

template<> struct ToPlatform<Sepia> { auto operator()(const Sepia&) -> Ref<FilterOperation>; };

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::Style::Sepia, 1)
