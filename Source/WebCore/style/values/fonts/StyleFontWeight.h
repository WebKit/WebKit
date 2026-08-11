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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/FontSelectionAlgorithm.h>
#include <WebCore/StylePrimitiveNumericTypes.h>

namespace WebCore {
namespace Style {

// <'font-weight'> = normal | bold | bolder | lighter | <number [1,1000]>
// NOTE: Computed value is always resolved to a <number [1,1000]>.
// https://drafts.csswg.org/css-fonts-4/#propdef-font-weight
struct FontWeight {
    using Number = Style::Number<CSS::Range{1, 1000}>;

    FontWeight(CSS::Keyword::Normal) : m_platform { normalWeightValue() } { }
    FontWeight(CSS::Keyword::Bold) : m_platform { boldWeightValue() } { }
    FontWeight(Number number) : m_platform { FontSelectionValue::clampFloat(number.value) } { }

    FontWeight(FontSelectionValue platform) : m_platform { platform } { }

    bool isNormal() const { return m_platform == normalWeightValue(); }
    bool isBold() const { return m_platform == boldWeightValue(); }

    Number number() const { return Number { static_cast<float>(m_platform) }; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
        return visitor(number());
    }

    FontSelectionValue platform() const { return m_platform; }

    // NOTE: This is not if the value would compute to the keyword `bold`, but rather a more general whether the weight is large enough to be considered "bold" (see `WebCore::boldThreshold()`).
    bool isConsideredBold() const { return WebCore::isFontWeightBold(m_platform); }

    bool operator==(const FontWeight&) const = default;

private:
    FontSelectionValue m_platform;
};

// MARK: - Conversion

template<> struct CSSValueConversion<FontWeight> { auto operator()(BuilderState&, const CSSValue&) -> FontWeight; };

// MARK: - Blending

template<> struct Blending<FontWeight> {
    auto blend(const FontWeight&, const FontWeight&, const BlendingContext&) -> FontWeight;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::FontWeight)
