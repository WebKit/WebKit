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
#include <WebCore/StylePrimitiveNumericTypes.h>

namespace WebCore {

class FilterEffect;
class FilterOperation;

namespace CSS {
struct Blur;
}

namespace Style {

struct ZoomFactor;

// blur() = blur( <length [0,∞]>?@(default=0px) )
// https://drafts.fxtf.org/filter-effects/#funcdef-filter-blur
struct Blur {
    using Parameter = Length<CSS::Nonnegative>;

    Parameter value;

    static Blur passthroughForInterpolation();

    constexpr bool requiresRepaintForCurrentColorChange() const { return false; }
    constexpr bool affectsOpacity() const { return true; }
    constexpr bool movesPixels() const { return true; }
    constexpr bool shouldBeRestrictedBySecurityOrigin() const { return false; }
    bool isIdentity() const { return value.isZero(); }

    IntOutsets calculateOutsets(ZoomFactor) const;

    bool operator==(const Blur&) const = default;
};
using BlurFunction = FunctionNotation<CSSValueBlur, Blur>;
DEFINE_TYPE_WRAPPER_GET(Blur, value);

// MARK: - Conversion

template<> struct ToCSS<Blur> { auto operator()(const Blur&, const RenderStyle&) -> CSS::Blur; };
template<> struct ToStyle<CSS::Blur> { auto operator()(const CSS::Blur&, const BuilderState&) -> Blur; };

// MARK: - Evaluation

template<> struct Evaluation<Blur, Ref<FilterEffect>> { auto operator()(const Blur&, const RenderStyle&) -> Ref<FilterEffect>; };

// MARK: - Platform

template<> struct ToPlatform<Blur> { auto operator()(const Blur&, const RenderStyle&) -> Ref<FilterOperation>; };

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::Style::Blur, 1)
