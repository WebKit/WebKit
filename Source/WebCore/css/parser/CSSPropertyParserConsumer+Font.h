/*
 * Copyright (C) 2021 Metrological Group B.V.
 * Copyright (C) 2021 Igalia S.L.
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

#include "CSSPropertyParserConsumer+RawTypes.h"
#include "CSSPropertyParserConsumer+UnevaluatedCalc.h"
#include "CSSValueKeywords.h"
#include "SystemFontDatabase.h"
#include <optional>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class CSSParserTokenRange;
class CSSPrimitiveValue;
class CSSValue;
class CSSValueList;
class ScriptExecutionContext;

struct CSSParserContext;

enum CSSParserMode : uint8_t;
enum CSSValueID : uint16_t;

enum class FontTechnology : uint8_t;

namespace WebKitFontFamilyNames {
enum class FamilyNamesIndex;
}

namespace CSSPropertyParserHelpers {

// MARK: - Font

// normal | italic | oblique <angle [-90deg,90deg]>?
using UnresolvedFontStyleAngle = std::variant<AngleRaw, UnevaluatedCalc<AngleRaw>>;
using UnresolvedFontStyle = std::variant<CSSValueID, AngleRaw, UnevaluatedCalc<AngleRaw>>;

// normal | small-caps
using UnresolvedFontVariantCaps = CSSValueID;

// normal | bold | bolder | lighter | <number [1,1000]>
using UnresolvedFontWeightNumber = std::variant<NumberRaw, UnevaluatedCalc<NumberRaw>>;
using UnresolvedFontWeight = std::variant<CSSValueID, NumberRaw, UnevaluatedCalc<NumberRaw>>;

// normal | <percentage [0,∞]> | ultra-condensed | extra-condensed | condensed | semi-condensed | semi-expanded | expanded | extra-expanded | ultra-expanded
using UnresolvedFontStretchPercentage = std::variant<PercentageRaw, UnevaluatedCalc<PercentageRaw>>;
using UnresolvedFontStretch = std::variant<CSSValueID, PercentageRaw, UnevaluatedCalc<PercentageRaw>>;

// <absolute-size> | <relative-size> | <length-percentage [0,∞]>
using UnresolvedFontSize = std::variant<CSSValueID, LengthPercentageRaw, UnevaluatedCalc<LengthPercentageRaw>>;

// normal | <number [0,∞]> | <length-percentage [0,∞]>
using UnresolvedFontLineHeight = std::variant<CSSValueID, NumberRaw, UnevaluatedCalc<NumberRaw>, LengthPercentageRaw, UnevaluatedCalc<LengthPercentageRaw>>;

// [ <family-name> | <generic-family> ]#
using UnresolvedFontFamilyName = std::variant<CSSValueID, AtomString>;
using UnresolvedFontFamily = Vector<UnresolvedFontFamilyName>;

struct UnresolvedFont {
    UnresolvedFontStyle style;
    UnresolvedFontVariantCaps variantCaps;
    UnresolvedFontWeight weight;
    UnresolvedFontStretch stretch;
    UnresolvedFontSize size;
    UnresolvedFontLineHeight lineHeight;
    UnresolvedFontFamily family;
};

// MARK: 'font' (shorthand)
// https://drafts.csswg.org/css-fonts-4/#font-prop
std::optional<UnresolvedFont> parseUnresolvedFont(const String&, CSSParserMode);

// MARK: 'font-weight'
// https://drafts.csswg.org/css-fonts-4/#font-weight-prop
RefPtr<CSSValue> consumeFontWeight(CSSParserTokenRange&);

// MARK: 'font-style'
// https://drafts.csswg.org/css-fonts-4/#font-style-prop
RefPtr<CSSValue> consumeFontStyle(CSSParserTokenRange&, CSSParserMode);

// MARK: 'font-family'
// https://drafts.csswg.org/css-fonts-4/#font-family-prop
RefPtr<CSSValue> consumeFontFamily(CSSParserTokenRange&);
// Sub-production of 'font-family': <generic-family>
// https://drafts.csswg.org/css-fonts-4/#generic-family-name-syntax
const AtomString& genericFontFamily(CSSValueID);
WebKitFontFamilyNames::FamilyNamesIndex genericFontFamilyIndex(CSSValueID);

// MARK: 'font-variant-ligatures'
// https://drafts.csswg.org/css-fonts-4/#propdef-font-variant-ligatures
RefPtr<CSSValue> consumeFontVariantLigatures(CSSParserTokenRange&);

// MARK: 'font-variant-east-asian'
// https://drafts.csswg.org/css-fonts-4/#font-variant-east-asian-prop
RefPtr<CSSValue> consumeFontVariantEastAsian(CSSParserTokenRange&);

// MARK: 'font-variant-alternates'
// https://drafts.csswg.org/css-fonts-4/#font-variant-alternates-prop
RefPtr<CSSValue> consumeFontVariantAlternates(CSSParserTokenRange&);

// MARK: 'font-variant-numeric'
// https://drafts.csswg.org/css-fonts-4/#font-variant-numeric-prop
RefPtr<CSSValue> consumeFontVariantNumeric(CSSParserTokenRange&);

// MARK: 'font-size-adjust'
// https://drafts.csswg.org/css-fonts-4/#font-size-adjust-prop
RefPtr<CSSValue> consumeFontSizeAdjust(CSSParserTokenRange&);


// MARK: - @font-face descriptor consumers

// MARK: @font-face 'font-family'
// https://drafts.csswg.org/css-fonts-4/#font-family-desc
RefPtr<CSSValue> consumeFontFaceFontFamily(CSSParserTokenRange&);

// MARK: @font-face 'src'
// https://drafts.csswg.org/css-fonts-4/#src-desc
RefPtr<CSSValueList> parseFontFaceSrc(const String&, const CSSParserContext&);
RefPtr<CSSValueList> consumeFontFaceSrc(CSSParserTokenRange&, const CSSParserContext&);
// Sub-production of 'src: <font-tech>
// https://drafts.csswg.org/css-fonts-4/#font-tech-values
Vector<FontTechnology> consumeFontTech(CSSParserTokenRange&, bool singleValue = false);
// Sub-production of 'src': <font-format>
// https://drafts.csswg.org/css-fonts-4/#font-format-values
String consumeFontFormat(CSSParserTokenRange&, bool rejectStringValues = false);

// MARK: @font-face 'size-adjust'
// https://drafts.csswg.org/css-fonts-5/#size-adjust-desc
RefPtr<CSSValue> parseFontFaceSizeAdjust(const String&, ScriptExecutionContext&);

// MARK: @font-face 'unicode-range'
// https://drafts.csswg.org/css-fonts-4/#unicode-range-desc
RefPtr<CSSValueList> parseFontFaceUnicodeRange(const String&, ScriptExecutionContext&);
RefPtr<CSSValueList> consumeFontFaceUnicodeRange(CSSParserTokenRange&);

// MARK: @font-face 'font-display'
// https://drafts.csswg.org/css-fonts-4/#descdef-font-face-font-display
RefPtr<CSSValue> parseFontFaceDisplay(const String&, ScriptExecutionContext&);

// MARK: @font-face 'font-style'
// https://drafts.csswg.org/css-fonts-4/#descdef-font-face-font-style
RefPtr<CSSValue> parseFontFaceFontStyle(const String&, ScriptExecutionContext&);
RefPtr<CSSValue> consumeFontFaceFontStyle(CSSParserTokenRange&, CSSParserMode);

// MARK: @font-face 'font-feature-tag'
// https://drafts.csswg.org/css-fonts-4/#feature-tag-value
RefPtr<CSSValue> consumeFeatureTagValue(CSSParserTokenRange&);

// MARK: @font-face 'font-feature-settings'
// https://drafts.csswg.org/css-fonts-4/#descdef-font-face-font-feature-settings
RefPtr<CSSValue> parseFontFaceFeatureSettings(const String&, ScriptExecutionContext&);

// MARK: @font-face 'font-variation-settings'
// https://drafts.csswg.org/css-fonts-4/#descdef-font-face-font-variation-settings
#if ENABLE(VARIATION_FONTS)
RefPtr<CSSValue> consumeVariationTagValue(CSSParserTokenRange&);
#endif

// MARK: @font-face 'font-stretch'
// https://drafts.csswg.org/css-fonts-4/#font-stretch-desc
RefPtr<CSSValue> parseFontFaceFontStretch(const String&, ScriptExecutionContext&);
RefPtr<CSSValue> consumeFontFaceFontStretch(CSSParserTokenRange&);

// MARK: @font-face 'font-weight'
// https://drafts.csswg.org/css-fonts-4/#descdef-font-face-font-weight
RefPtr<CSSValue> parseFontFaceFontWeight(const String&, ScriptExecutionContext&);
RefPtr<CSSValue> consumeFontFaceFontWeight(CSSParserTokenRange&);

// MARK: - @font-palette-values descriptor consumers:

// MARK: @font-palette-values 'font-family'
RefPtr<CSSValue> consumeFontPaletteValuesFontFamily(CSSParserTokenRange&);

// MARK: @font-palette-values 'override-colors'
// https://drafts.csswg.org/css-fonts-4/#descdef-font-palette-values-override-colors
RefPtr<CSSValue> consumeFontPaletteValuesOverrideColors(CSSParserTokenRange&, const CSSParserContext&);

// MARK: - @font-feature-values descriptor consumers

// MARK: @font-feature-values 'prelude family name list'
// https://drafts.csswg.org/css-fonts-4/#font-feature-values-syntax
Vector<AtomString> consumeFontFeatureValuesPreludeFamilyNameList(CSSParserTokenRange&);

// Template and inline implementations are at the bottom of the file for readability.

inline bool isSystemFontShorthand(CSSValueID valueID)
{
    // This needs to stay in sync with SystemFontDatabase::FontShorthand.
    static_assert(CSSValueStatusBar - CSSValueCaption == static_cast<SystemFontDatabase::FontShorthandUnderlyingType>(SystemFontDatabase::FontShorthand::StatusBar));
    return valueID >= CSSValueCaption && valueID <= CSSValueStatusBar;
}

inline SystemFontDatabase::FontShorthand lowerFontShorthand(CSSValueID valueID)
{
    // This needs to stay in sync with SystemFontDatabase::FontShorthand.
    ASSERT(isSystemFontShorthand(valueID));
    return static_cast<SystemFontDatabase::FontShorthand>(valueID - CSSValueCaption);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
