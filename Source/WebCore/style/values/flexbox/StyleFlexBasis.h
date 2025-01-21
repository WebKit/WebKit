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

struct PreferredSize;

// <'flex-basis'> = content | <'width'>
// https://drafts.csswg.org/css-flexbox-1/#flex-basis-property
struct FlexBasis : SizeBase<WebCore::CSS::Keyword::Auto, WebCore::CSS::Keyword::Content> {
    using Base = SizeBase<WebCore::CSS::Keyword::Auto, WebCore::CSS::Keyword::Content>;
    using Base::Base;
    using Base::operator=;

    // FIXME: Implement.
    // Transforms `content` to `max-content`.
    Style::PreferredSize asPreferredSize() const;

    bool isAuto() const { return holdsAlternative<WebCore::CSS::Keyword::Auto>(); }
    bool isContent() const { return holdsAlternative<WebCore::CSS::Keyword::Content>(); }
};

// MARK: - From CSSValue

template<> struct CSSValueConversions<FlexBasis> {
    FlexBasis operator()(const CSSValue&, const BuilderState&);
};

// MARK: - Evaluation

template<> struct Evaluation<FlexBasis> {
    double operator()(const FlexBasis&, double);
    float operator()(const FlexBasis&, float);
    LayoutUnit operator()(const FlexBasis&, LayoutUnit);
};

LayoutUnit evaluateMinimum(const FlexBasis&, LayoutUnit maximumValue);

// MARK: - Blending

template<> struct Blending<FlexBasis> {
    auto canBlend(const FlexBasis&, const FlexBasis&) -> bool;
    auto blend(const FlexBasis&, const FlexBasis&, const BlendingContext&) -> FlexBasis;
};

// MARK: - TextStream

WTF::TextStream& operator<<(WTF::TextStream&, FlexBasis);

} // namespace Style

namespace CSS {

// MARK: - To CSSValue

template<> struct CSSValueCreation<Style::FlexBasis> {
    Ref<CSSValue> operator()(const Style::FlexBasis&, const RenderStyle&);
};

} // namespace CSS
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::FlexBasis> = true;
