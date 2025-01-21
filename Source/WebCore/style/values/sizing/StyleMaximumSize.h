/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "StyleSizing.h"

namespace WebCore {
namespace Style {

// <'max-width'>/<'max-height'> = none | <length-percentage [0,∞]> | min-content | max-content | fit-content(<length-percentage [0,∞]>) | <calc-size()> | stretch | fit-content | contain

// What is actually implemented is:

// <'max-width'>/<'max-height'> = none | <length-percentage [0,∞]> | min-content | max-content | fit-content | intrinsic | min-intrinsic | -webkit-fill-available

// MISSING:
//    fit-content(<length-percentage [0,∞]>)
//    <calc-size()>
//    stretch
//    contain

// NON-STANDARD:
//    intrinsic
//    min-intrinsic
//    -webkit-fill-available

// https://drafts.csswg.org/css-sizing-3/#max-size-properties
// https://drafts.csswg.org/css-sizing-4/#sizing-values (additional values added)
struct MaximumSize : SizeBase<WebCore::CSS::Keyword::None> {
    using Base = SizeBase<WebCore::CSS::Keyword::None>;
    using Base::Base;
    using Base::operator=;

    bool isNone() const { return holdsAlternative<WebCore::CSS::Keyword::None>(); }
};

// MARK: - From CSSValue

template<> struct CSSValueConversions<MaximumSize> {
    MaximumSize operator()(const CSSValue&, const BuilderState&);
};

// MARK: - Evaluation

template<> struct Evaluation<MaximumSize> {
    double operator()(const MaximumSize&, double);
    float operator()(const MaximumSize&, float);
    LayoutUnit operator()(const MaximumSize&, LayoutUnit);
};

LayoutUnit evaluateMinimum(const MaximumSize&, LayoutUnit maximumValue);

// MARK: - Blending

template<> struct Blending<MaximumSize> {
    auto canBlend(const MaximumSize&, const MaximumSize&) -> bool;
    auto blend(const MaximumSize&, const MaximumSize&, const BlendingContext&) -> MaximumSize;
};

// MARK: - TextStream

WTF::TextStream& operator<<(WTF::TextStream&, MaximumSize);

} // namespace Style

namespace CSS {

// MARK: - To CSSValue

template<> struct CSSValueCreation<Style::MaximumSize> {
    Ref<CSSValue> operator()(const Style::MaximumSize&, const RenderStyle&);
};

} // namespace CSS
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::MaximumSize> = true;
