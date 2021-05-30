// Copyright 2016 The Chromium Authors. All rights reserved.
// Copyright (C) 2016 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include "CSSFunctionValue.h"
#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSShadowValue.h"
#include "CSSValuePool.h"
#include "Length.h" // For ValueRange
#include <wtf/OptionSet.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>

namespace WebCore {

namespace WebKitFontFamilyNames {
enum class FamilyNamesIndex;
}

// When these functions are successful, they will consume all the relevant
// tokens from the range and also consume any whitespace which follows. When
// the start of the range doesn't match the type we're looking for, the range
// will not be modified.
namespace CSSPropertyParserHelpers {

// FIXME: These should probably just be consumeComma and consumeSlash.
bool consumeCommaIncludingWhitespace(CSSParserTokenRange&);
bool consumeSlashIncludingWhitespace(CSSParserTokenRange&);
// consumeFunction expects the range starts with a FunctionToken.
CSSParserTokenRange consumeFunction(CSSParserTokenRange&);

enum class UnitlessQuirk { Allow, Forbid };
enum class UnitlessZeroQuirk { Allow, Forbid };
enum class AllowXResolutionUnit { Allow, Forbid };

struct AngleRaw {
    CSSUnitType type;
    double value;
};

struct LengthRaw {
    CSSUnitType type;
    double value;
};

using LengthOrPercentRaw = Variant<LengthRaw, double>;

std::optional<int> consumeIntegerRaw(CSSParserTokenRange&, double minimumValue = -std::numeric_limits<double>::max());
RefPtr<CSSPrimitiveValue> consumeInteger(CSSParserTokenRange&, double minimumValue = -std::numeric_limits<double>::max());
std::optional<unsigned> consumePositiveIntegerRaw(CSSParserTokenRange&);
RefPtr<CSSPrimitiveValue> consumePositiveInteger(CSSParserTokenRange&);
std::optional<double> consumeNumberRaw(CSSParserTokenRange&, ValueRange = ValueRange::All);
RefPtr<CSSPrimitiveValue> consumeNumber(CSSParserTokenRange&, ValueRange);
std::optional<double> consumeFontWeightNumberRaw(CSSParserTokenRange&);
RefPtr<CSSPrimitiveValue> consumeFontWeightNumber(CSSParserTokenRange&);
RefPtr<CSSPrimitiveValue> consumeFontWeightNumberWorkerSafe(CSSParserTokenRange&, CSSValuePool&);
std::optional<LengthRaw> consumeLengthRaw(CSSParserTokenRange&, CSSParserMode, ValueRange, UnitlessQuirk = UnitlessQuirk::Forbid);
RefPtr<CSSPrimitiveValue> consumeLength(CSSParserTokenRange&, CSSParserMode, ValueRange, UnitlessQuirk = UnitlessQuirk::Forbid);
std::optional<double> consumePercentRaw(CSSParserTokenRange&, ValueRange = ValueRange::All);
RefPtr<CSSPrimitiveValue> consumePercent(CSSParserTokenRange&, ValueRange);
RefPtr<CSSPrimitiveValue> consumePercentWorkerSafe(CSSParserTokenRange&, ValueRange, CSSValuePool&);
std::optional<LengthOrPercentRaw> consumeLengthOrPercentRaw(CSSParserTokenRange&, CSSParserMode, ValueRange, UnitlessQuirk = UnitlessQuirk::Forbid);
RefPtr<CSSPrimitiveValue> consumeLengthOrPercent(CSSParserTokenRange&, CSSParserMode, ValueRange, UnitlessQuirk = UnitlessQuirk::Forbid);
std::optional<AngleRaw> consumeAngleRaw(CSSParserTokenRange&, CSSParserMode, UnitlessQuirk = UnitlessQuirk::Forbid, UnitlessZeroQuirk = UnitlessZeroQuirk::Forbid);
RefPtr<CSSPrimitiveValue> consumeAngle(CSSParserTokenRange&, CSSParserMode, UnitlessQuirk = UnitlessQuirk::Forbid, UnitlessZeroQuirk = UnitlessZeroQuirk::Forbid);
RefPtr<CSSPrimitiveValue> consumeAngleWorkerSafe(CSSParserTokenRange&, CSSParserMode, CSSValuePool&, UnitlessQuirk = UnitlessQuirk::Forbid, UnitlessZeroQuirk = UnitlessZeroQuirk::Forbid);
RefPtr<CSSPrimitiveValue> consumeTime(CSSParserTokenRange&, CSSParserMode, ValueRange, UnitlessQuirk = UnitlessQuirk::Forbid);
RefPtr<CSSPrimitiveValue> consumeResolution(CSSParserTokenRange&, AllowXResolutionUnit = AllowXResolutionUnit::Forbid);

std::optional<CSSValueID> consumeIdentRaw(CSSParserTokenRange&);
RefPtr<CSSPrimitiveValue> consumeIdent(CSSParserTokenRange&);
RefPtr<CSSPrimitiveValue> consumeIdentWorkerSafe(CSSParserTokenRange&, CSSValuePool&);
std::optional<CSSValueID> consumeIdentRangeRaw(CSSParserTokenRange&, CSSValueID lower, CSSValueID upper);
RefPtr<CSSPrimitiveValue> consumeIdentRange(CSSParserTokenRange&, CSSValueID lower, CSSValueID upper);
template<CSSValueID, CSSValueID...> inline bool identMatches(CSSValueID id);
template<CSSValueID... allowedIdents> std::optional<CSSValueID> consumeIdentRaw(CSSParserTokenRange&);
template<CSSValueID... allowedIdents> RefPtr<CSSPrimitiveValue> consumeIdent(CSSParserTokenRange&);
template<CSSValueID... allowedIdents> RefPtr<CSSPrimitiveValue> consumeIdentWorkerSafe(CSSParserTokenRange&, CSSValuePool&);

RefPtr<CSSPrimitiveValue> consumeCustomIdent(CSSParserTokenRange&, bool shouldLowercase = false);
RefPtr<CSSPrimitiveValue> consumeString(CSSParserTokenRange&);
StringView consumeUrlAsStringView(CSSParserTokenRange&);
RefPtr<CSSPrimitiveValue> consumeUrl(CSSParserTokenRange&);

Color consumeColorWorkerSafe(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSPrimitiveValue> consumeColor(CSSParserTokenRange&, const CSSParserContext&, bool acceptQuirkyColors = false);

enum class PositionSyntax {
    Position, // <position>
    BackgroundPosition // <bg-position>
};

struct PositionCoordinates {
    Ref<CSSPrimitiveValue> x;
    Ref<CSSPrimitiveValue> y;
};

RefPtr<CSSPrimitiveValue> consumePosition(CSSParserTokenRange&, CSSParserMode, UnitlessQuirk, PositionSyntax);
std::optional<PositionCoordinates> consumePositionCoordinates(CSSParserTokenRange&, CSSParserMode, UnitlessQuirk, PositionSyntax);
std::optional<PositionCoordinates> consumeOneOrTwoValuedPositionCoordinates(CSSParserTokenRange&, CSSParserMode, UnitlessQuirk);

enum class AllowedImageType : uint8_t {
    URLFunction = 1 << 0,
    RawStringAsURL = 1 << 1,
    ImageSet = 1 << 2,
    GeneratedImage = 1 << 3
};

RefPtr<CSSValue> consumeImage(CSSParserTokenRange&, const CSSParserContext&, OptionSet<AllowedImageType> = { AllowedImageType::URLFunction, AllowedImageType::ImageSet, AllowedImageType::GeneratedImage });
RefPtr<CSSValue> consumeImageOrNone(CSSParserTokenRange&, const CSSParserContext&);

enum class AllowedFilterFunctions {
    PixelFilters,
    ColorFilters
};

RefPtr<CSSValue> consumeFilter(CSSParserTokenRange&, const CSSParserContext&, AllowedFilterFunctions);
RefPtr<CSSShadowValue> consumeSingleShadow(CSSParserTokenRange&, const CSSParserContext&, bool allowInset, bool allowSpread);

struct FontStyleRaw {
    CSSValueID style;
    std::optional<AngleRaw> angle;
};
using FontWeightRaw = Variant<CSSValueID, double>;
using FontSizeRaw = Variant<CSSValueID, CSSPropertyParserHelpers::LengthOrPercentRaw>;
using LineHeightRaw = Variant<CSSValueID, double, CSSPropertyParserHelpers::LengthOrPercentRaw>;
using FontFamilyRaw = Variant<CSSValueID, String>;

struct FontRaw {
    std::optional<FontStyleRaw> style;
    std::optional<CSSValueID> variantCaps;
    std::optional<FontWeightRaw> weight;
    std::optional<CSSValueID> stretch;
    FontSizeRaw size;
    std::optional<LineHeightRaw> lineHeight;
    Vector<FontFamilyRaw> family;
};

bool isPredefinedCounterStyle(CSSValueID);
RefPtr<CSSPrimitiveValue> consumeCounterStyleName(CSSParserTokenRange&);
AtomString consumeCounterStyleNameInPrelude(CSSParserTokenRange&);

std::optional<CSSValueID> consumeFontVariantCSS21Raw(CSSParserTokenRange&);
std::optional<CSSValueID> consumeFontWeightKeywordValueRaw(CSSParserTokenRange&);
std::optional<FontWeightRaw> consumeFontWeightRaw(CSSParserTokenRange&);
std::optional<CSSValueID> consumeFontStretchKeywordValueRaw(CSSParserTokenRange&);
std::optional<CSSValueID> consumeFontStyleKeywordValueRaw(CSSParserTokenRange&);
std::optional<FontStyleRaw> consumeFontStyleRaw(CSSParserTokenRange&, CSSParserMode);
String concatenateFamilyName(CSSParserTokenRange&);
String consumeFamilyNameRaw(CSSParserTokenRange&);
std::optional<CSSValueID> consumeGenericFamilyRaw(CSSParserTokenRange&);
std::optional<Vector<FontFamilyRaw>> consumeFontFamilyRaw(CSSParserTokenRange&);
std::optional<FontSizeRaw> consumeFontSizeRaw(CSSParserTokenRange&, CSSParserMode, UnitlessQuirk = UnitlessQuirk::Forbid);
std::optional<LineHeightRaw> consumeLineHeightRaw(CSSParserTokenRange&, CSSParserMode);
std::optional<FontRaw> consumeFontRaw(CSSParserTokenRange&, CSSParserMode);
const AtomString& genericFontFamily(CSSValueID);
WebKitFontFamilyNames::FamilyNamesIndex genericFontFamilyIndex(CSSValueID);

bool isFontStyleAngleInRange(double angleInDegrees);

// Template and inline implementations are at the bottom of the file for readability.

template<typename... emptyBaseCase> inline bool identMatches(CSSValueID) { return false; }
template<CSSValueID head, CSSValueID... tail> inline bool identMatches(CSSValueID id)
{
    return id == head || identMatches<tail...>(id);
}

template<CSSValueID... names> std::optional<CSSValueID> consumeIdentRaw(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken || !identMatches<names...>(range.peek().id()))
        return std::nullopt;
    return range.consumeIncludingWhitespace().id();
}

template<CSSValueID... names> RefPtr<CSSPrimitiveValue> consumeIdent(CSSParserTokenRange& range)
{
    return consumeIdentWorkerSafe<names...>(range, CSSValuePool::singleton());
}

template<CSSValueID... names> RefPtr<CSSPrimitiveValue> consumeIdentWorkerSafe(CSSParserTokenRange& range, CSSValuePool& cssValuePool)
{
    if (range.peek().type() != IdentToken || !identMatches<names...>(range.peek().id()))
        return nullptr;
    return cssValuePool.createIdentifierValue(range.consumeIncludingWhitespace().id());
}

inline bool isFontStyleAngleInRange(double angleInDegrees)
{
    return angleInDegrees > -90 && angleInDegrees < 90;
}

} // namespace CSSPropertyParserHelpers

} // namespace WebCore
