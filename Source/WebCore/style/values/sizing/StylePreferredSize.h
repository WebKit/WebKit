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

// <'width'>/<'height'> = auto | <length-percentage [0,∞]> | min-content | max-content | fit-content(<length-percentage [0,∞]>) | <calc-size()> | stretch | fit-content | contain

// What is actually implemented is:

// <'width'>/<'height'> = auto | <length-percentage [0,∞]> | min-content | max-content | fit-content | intrinsic | min-intrinsic | -webkit-fill-available

// MISSING:
//    fit-content(<length-percentage [0,∞]>)
//    <calc-size()>
//    stretch
//    contain

// NON-STANDARD:
//    intrinsic
//    min-intrinsic
//    -webkit-fill-available

// https://drafts.csswg.org/css-sizing-3/#preferred-size-properties
// https://drafts.csswg.org/css-sizing-4/#sizing-values (additional values added)
struct PreferredSize : SizeBase<WebCore::CSS::Keyword::Auto> {
    using Base = SizeBase<WebCore::CSS::Keyword::Auto>;
    using Base::Base;
    using Base::operator=;

    bool isAuto() const { return holdsAlternative<WebCore::CSS::Keyword::Auto>(); }
};

// MARK: - From CSSValue

template<> struct CSSValueConversions<PreferredSize> {
    PreferredSize operator()(const CSSValue&, const BuilderState&);
};

// MARK: - Evaluation

template<> struct Evaluation<PreferredSize> {
    double operator()(const PreferredSize&, double);
    float operator()(const PreferredSize&, float);
    LayoutUnit operator()(const PreferredSize&, LayoutUnit);
};

LayoutUnit evaluateMinimum(const PreferredSize&, LayoutUnit maximumValue);

// MARK: - Blending

template<> struct Blending<PreferredSize> {
    auto canBlend(const PreferredSize&, const PreferredSize&) -> bool;
    auto blend(const PreferredSize&, const PreferredSize&, const BlendingContext&) -> PreferredSize;
};

// MARK: - TextStream

WTF::TextStream& operator<<(WTF::TextStream&, PreferredSize);

} // namespace Style

namespace CSS {

// MARK: - To CSSValue

template<> struct CSSValueCreation<Style::PreferredSize> {
    Ref<CSSValue> operator()(const Style::PreferredSize&, const RenderStyle&);
};

} // namespace CSS
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::PreferredSize> = true;
