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

#include <WebCore/StyleValueTypes.h>
#include <WebCore/TextFlags.h>

namespace WebCore {
namespace Style {

// <'font-variant-numeric'> = normal | [ <numeric-figure-values> || <numeric-spacing-values> || <numeric-fraction-values> || ordinal || slashed-zero ]
// https://drafts.csswg.org/css-fonts-4/#propdef-font-variant-numeric
struct FontVariantNumeric {
    using Platform = WebCore::FontVariantNumericValues;

    constexpr FontVariantNumeric(CSS::Keyword::Normal)
        : m_platform { }
    {
    }

    constexpr FontVariantNumeric(Platform value)
        : m_platform { value }
    {
    }

    constexpr Platform platform() const { return m_platform; }

    constexpr bool isNormal() const
    {
        return m_platform.figure == FontVariantNumericFigure::Normal
            && m_platform.spacing == FontVariantNumericSpacing::Normal
            && m_platform.fraction == FontVariantNumericFraction::Normal
            && m_platform.ordinal == FontVariantNumericOrdinal::Normal
            && m_platform.slashedZero == FontVariantNumericSlashedZero::Normal;
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isNormal())
            return visitor(CSS::Keyword::Normal { });

        auto figureValue = [](auto value) -> std::optional<Variant<CSS::Keyword::LiningNums, CSS::Keyword::OldstyleNums>> {
            using enum FontVariantNumericFigure;
            switch (value) {
            case Normal:                return std::nullopt;
            case LiningNumbers:         return CSS::Keyword::LiningNums { };
            case OldStyleNumbers:       return CSS::Keyword::OldstyleNums { };
            }
            RELEASE_ASSERT_NOT_REACHED();
        };
        auto spacingValue = [](auto value) -> std::optional<Variant<CSS::Keyword::ProportionalNums, CSS::Keyword::TabularNums>> {
            using enum FontVariantNumericSpacing;
            switch (value) {
            case Normal:                return std::nullopt;
            case ProportionalNumbers:   return CSS::Keyword::ProportionalNums { };
            case TabularNumbers:        return CSS::Keyword::TabularNums { };
            }
            RELEASE_ASSERT_NOT_REACHED();
        };
        auto fractionValue = [](auto value) -> std::optional<Variant<CSS::Keyword::DiagonalFractions, CSS::Keyword::StackedFractions>> {
            using enum FontVariantNumericFraction;
            switch (value) {
            case Normal:                return std::nullopt;
            case DiagonalFractions:     return CSS::Keyword::DiagonalFractions { };
            case StackedFractions:      return CSS::Keyword::StackedFractions { };
            }
            RELEASE_ASSERT_NOT_REACHED();
        };
        auto ordinalValue = [](auto value) -> std::optional<CSS::Keyword::Ordinal> {
            using enum FontVariantNumericOrdinal;
            switch (value) {
            case Normal:                return std::nullopt;
            case Yes:                   return CSS::Keyword::Ordinal { };
            }
            RELEASE_ASSERT_NOT_REACHED();
        };
        auto slashedZeroValue = [](auto value) -> std::optional<CSS::Keyword::SlashedZero> {
            using enum FontVariantNumericSlashedZero;
            switch (value) {
            case Normal:                return std::nullopt;
            case Yes:                   return CSS::Keyword::SlashedZero { };
            }
            RELEASE_ASSERT_NOT_REACHED();
        };

        return visitor(SpaceSeparatedTuple {
            figureValue(m_platform.figure),
            spacingValue(m_platform.spacing),
            fractionValue(m_platform.fraction),
            ordinalValue(m_platform.ordinal),
            slashedZeroValue(m_platform.slashedZero),
        });
    }

    constexpr bool operator==(const FontVariantNumeric&) const = default;

private:
    Platform m_platform;
};

// MARK: - Conversion

template<> struct CSSValueConversion<FontVariantNumeric> { auto operator()(BuilderState&, const CSSValue&) -> FontVariantNumeric; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::FontVariantNumeric)
