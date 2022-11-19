// Copyright 2016 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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
#include "GridArea.h"
#include "Length.h"
#include "Pair.h"
#include "StyleColor.h"
#include "SystemFontDatabase.h"
#include <variant>
#include <wtf/OptionSet.h>
#include <wtf/Vector.h>

namespace WebCore {

class CSSGridLineNamesValue;

namespace WebKitFontFamilyNames {
enum class FamilyNamesIndex;
}

enum class BoxOrient : uint8_t;

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

enum class NegativePercentagePolicy : bool { Forbid, Allow };

enum class UnitlessQuirk { Allow, Forbid };
enum class UnitlessZeroQuirk { Allow, Forbid };

struct NoneRaw { };

struct NumberRaw {
    double value;
};

struct PercentRaw {
    double value;
};

struct AngleRaw {
    CSSUnitType type;
    double value;
};

struct LengthRaw {
    CSSUnitType type;
    double value;
};

using LengthOrPercentRaw = std::variant<LengthRaw, PercentRaw>;

std::optional<int> consumeIntegerRaw(CSSParserTokenRange&);
RefPtr<CSSPrimitiveValue> consumeInteger(CSSParserTokenRange&);
std::optional<int> consumeIntegerZeroAndGreaterRaw(CSSParserTokenRange&);
RefPtr<CSSPrimitiveValue> consumeIntegerZeroAndGreater(CSSParserTokenRange&);
std::optional<unsigned> consumePositiveIntegerRaw(CSSParserTokenRange&);
RefPtr<CSSPrimitiveValue> consumePositiveInteger(CSSParserTokenRange&);
std::optional<NumberRaw> consumeNumberRaw(CSSParserTokenRange&, ValueRange = ValueRange::All);
RefPtr<CSSPrimitiveValue> consumeNumber(CSSParserTokenRange&, ValueRange);
RefPtr<CSSPrimitiveValue> consumeNumberOrPercent(CSSParserTokenRange&, ValueRange);
RefPtr<CSSPrimitiveValue> consumeFontWeightNumberWorkerSafe(CSSParserTokenRange&, CSSValuePool&);
RefPtr<CSSPrimitiveValue> consumeLength(CSSParserTokenRange&, CSSParserMode, ValueRange, UnitlessQuirk = UnitlessQuirk::Forbid);
RefPtr<CSSPrimitiveValue> consumePercent(CSSParserTokenRange&, ValueRange);
RefPtr<CSSPrimitiveValue> consumePercentWorkerSafe(CSSParserTokenRange&, ValueRange, CSSValuePool&);
RefPtr<CSSPrimitiveValue> consumeLengthOrPercent(CSSParserTokenRange&, CSSParserMode, ValueRange, UnitlessQuirk = UnitlessQuirk::Forbid, NegativePercentagePolicy = NegativePercentagePolicy::Forbid);
RefPtr<CSSPrimitiveValue> consumeAngle(CSSParserTokenRange&, CSSParserMode, UnitlessQuirk = UnitlessQuirk::Forbid, UnitlessZeroQuirk = UnitlessZeroQuirk::Forbid);
RefPtr<CSSPrimitiveValue> consumeAngleWorkerSafe(CSSParserTokenRange&, CSSParserMode, CSSValuePool&, UnitlessQuirk = UnitlessQuirk::Forbid, UnitlessZeroQuirk = UnitlessZeroQuirk::Forbid);
RefPtr<CSSPrimitiveValue> consumeTime(CSSParserTokenRange&, CSSParserMode, ValueRange, UnitlessQuirk = UnitlessQuirk::Forbid);
RefPtr<CSSPrimitiveValue> consumeResolution(CSSParserTokenRange&);

std::optional<CSSValueID> consumeIdentRaw(CSSParserTokenRange&);
RefPtr<CSSPrimitiveValue> consumeIdent(CSSParserTokenRange&);
RefPtr<CSSPrimitiveValue> consumeIdentWorkerSafe(CSSParserTokenRange&, CSSValuePool&);
RefPtr<CSSPrimitiveValue> consumeIdentRange(CSSParserTokenRange&, CSSValueID lower, CSSValueID upper);
template<CSSValueID, CSSValueID...> inline bool identMatches(CSSValueID id);
template<CSSValueID... allowedIdents> std::optional<CSSValueID> consumeIdentRaw(CSSParserTokenRange&);
template<CSSValueID... allowedIdents> RefPtr<CSSPrimitiveValue> consumeIdent(CSSParserTokenRange&);
template<CSSValueID... allowedIdents> RefPtr<CSSPrimitiveValue> consumeIdentWorkerSafe(CSSParserTokenRange&, CSSValuePool&);
template<typename Predicate, typename... Args> std::optional<CSSValueID> consumeIdentRaw(CSSParserTokenRange&, Predicate&&, Args&&...);
template<typename Predicate, typename... Args> RefPtr<CSSPrimitiveValue> consumeIdent(CSSParserTokenRange&, Predicate&&, Args&&...);
template<typename Predicate, typename... Args> RefPtr<CSSPrimitiveValue> consumeIdentWorkerSafe(CSSParserTokenRange&, CSSValuePool&, Predicate&&, Args&&...);

RefPtr<CSSPrimitiveValue> consumeCustomIdent(CSSParserTokenRange&, bool shouldLowercase = false);
RefPtr<CSSPrimitiveValue> consumeDashedIdent(CSSParserTokenRange&, bool shouldLowercase = false);
RefPtr<CSSPrimitiveValue> consumeString(CSSParserTokenRange&);
StringView consumeURLRaw(CSSParserTokenRange&);
RefPtr<CSSPrimitiveValue> consumeURL(CSSParserTokenRange&);

Color consumeColorWorkerSafe(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSPrimitiveValue> consumeColor(CSSParserTokenRange&, const CSSParserContext&, bool acceptQuirkyColors = false, OptionSet<StyleColor::CSSColorType> = { StyleColor::CSSColorType::Absolute, StyleColor::CSSColorType::Current, StyleColor::CSSColorType::System });

enum class PositionSyntax {
    Position, // <position>
    BackgroundPosition // <bg-position>
};

struct PositionCoordinates {
    Ref<CSSPrimitiveValue> x;
    Ref<CSSPrimitiveValue> y;
};

RefPtr<CSSPrimitiveValue> consumePosition(CSSParserTokenRange&, CSSParserMode, UnitlessQuirk, PositionSyntax);
RefPtr<CSSPrimitiveValue> consumeSingleAxisPosition(CSSParserTokenRange&, CSSParserMode, BoxOrient);
std::optional<PositionCoordinates> consumePositionCoordinates(CSSParserTokenRange&, CSSParserMode, UnitlessQuirk, PositionSyntax, NegativePercentagePolicy = NegativePercentagePolicy::Forbid);
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
using FontWeightRaw = std::variant<CSSValueID, double>;
using FontSizeRaw = std::variant<CSSValueID, CSSPropertyParserHelpers::LengthOrPercentRaw>;
using LineHeightRaw = std::variant<CSSValueID, double, CSSPropertyParserHelpers::LengthOrPercentRaw>;
using FontFamilyRaw = std::variant<CSSValueID, AtomString>;

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
RefPtr<CSSPrimitiveValue> consumeSingleContainerName(CSSParserTokenRange&);

std::optional<FontWeightRaw> consumeFontWeightRaw(CSSParserTokenRange&);
std::optional<CSSValueID> consumeFontStretchKeywordValueRaw(CSSParserTokenRange&);
std::optional<FontStyleRaw> consumeFontStyleRaw(CSSParserTokenRange&, CSSParserMode);
AtomString concatenateFamilyName(CSSParserTokenRange&);
AtomString consumeFamilyNameRaw(CSSParserTokenRange&);
// https://drafts.csswg.org/css-fonts-4/#family-name-value
Vector<AtomString> consumeFamilyNameList(CSSParserTokenRange&);
std::optional<FontRaw> consumeFontRaw(CSSParserTokenRange&, CSSParserMode);
const AtomString& genericFontFamily(CSSValueID);
WebKitFontFamilyNames::FamilyNamesIndex genericFontFamilyIndex(CSSValueID);

bool isFontStyleAngleInRange(double angleInDegrees);

RefPtr<CSSValueList> consumeAspectRatioValue(CSSParserTokenRange&);
RefPtr<CSSValue> consumeAspectRatio(CSSParserTokenRange&);

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

template<typename Predicate, typename... Args> std::optional<CSSValueID> consumeIdentRaw(CSSParserTokenRange& range, Predicate&& predicate, Args&&... args)
{
    if (auto keyword = range.peek().id(); predicate(keyword, std::forward<Args>(args)...)) {
        range.consumeIncludingWhitespace();
        return keyword;
    }
    return std::nullopt;
}

template<typename Predicate, typename... Args> RefPtr<CSSPrimitiveValue> consumeIdent(CSSParserTokenRange& range, Predicate&& predicate, Args&&... args)
{
    return consumeIdentWorkerSafe(range, CSSValuePool::singleton(), std::forward<Predicate>(predicate), std::forward<Args>(args)...);
}

template<typename Predicate, typename... Args> RefPtr<CSSPrimitiveValue> consumeIdentWorkerSafe(CSSParserTokenRange& range, CSSValuePool& cssValuePool, Predicate&& predicate, Args&&... args)
{
    if (auto keyword = range.peek().id(); predicate(keyword, std::forward<Args>(args)...)) {
        range.consumeIncludingWhitespace();
        return cssValuePool.createIdentifierValue(keyword);
    }
    return nullptr;
}

inline bool isFontStyleAngleInRange(double angleInDegrees)
{
    return angleInDegrees >= -90 && angleInDegrees <= 90;
}

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

template<typename... Args>
Ref<CSSPrimitiveValue> createPrimitiveValuePair(Args&&... args)
{
    return CSSValuePool::singleton().createValue(Pair::create(std::forward<Args>(args)...));
}

RefPtr<CSSValue> consumeAutoOrString(CSSParserTokenRange&);
RefPtr<CSSValue> consumeAutoOrColor(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeAutoOrPosition(CSSParserTokenRange&, CSSParserMode, UnitlessQuirk, PositionSyntax);
RefPtr<CSSValue> consumeNoneOrURL(CSSParserTokenRange&);

bool isFlexBasisIdent(CSSValueID);
bool isBaselineKeyword(CSSValueID);
bool isContentPositionKeyword(CSSValueID);
bool isContentPositionOrLeftOrRightKeyword(CSSValueID);
bool isSelfPositionKeyword(CSSValueID);
bool isSelfPositionOrLeftOrRightKeyword(CSSValueID);

using IsPositionKeyword = bool (*)(CSSValueID);

RefPtr<CSSValue> consumeDisplay(CSSParserTokenRange&);
RefPtr<CSSValue> consumeWillChange(CSSParserTokenRange&, const CSSParserContext&);
#if ENABLE(VARIATION_FONTS)
RefPtr<CSSValue> consumeFontVariationSettings(CSSParserTokenRange&);
#endif
RefPtr<CSSValue> consumePage(CSSParserTokenRange&);
RefPtr<CSSValue> consumeQuotes(CSSParserTokenRange&);
RefPtr<CSSValue> consumeFontVariantLigatures(CSSParserTokenRange&);
RefPtr<CSSValue> consumeFontVariantEastAsian(CSSParserTokenRange&);
RefPtr<CSSValue> consumeFontVariantCaps(CSSParserTokenRange&);
RefPtr<CSSValue> consumeFontVariantAlternates(CSSParserTokenRange&);
RefPtr<CSSValue> consumeFontVariantPosition(CSSParserTokenRange&);
RefPtr<CSSValue> consumeFontVariantNumeric(CSSParserTokenRange&);
RefPtr<CSSValue> consumeFontWeight(CSSParserTokenRange&);
RefPtr<CSSValue> consumeFontPalette(CSSParserTokenRange&);
RefPtr<CSSValue> consumeFamilyName(CSSParserTokenRange&);
RefPtr<CSSValue> consumeFontFamily(CSSParserTokenRange&);
RefPtr<CSSValue> consumeFontFamilyDescriptor(CSSParserTokenRange&);
RefPtr<CSSValue> consumeLetterSpacing(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeWordSpacing(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeTabSize(CSSParserTokenRange&, CSSParserMode);
#if ENABLE(TEXT_AUTOSIZING)
RefPtr<CSSValue> consumeTextSizeAdjust(CSSParserTokenRange&);
#endif
RefPtr<CSSValue> consumeFontSize(CSSParserTokenRange&, CSSParserMode, UnitlessQuirk = UnitlessQuirk::Forbid);
RefPtr<CSSValue> consumeFontSizeAdjust(CSSParserTokenRange&);
RefPtr<CSSValue> consumeLineHeight(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeCounterIncrement(CSSParserTokenRange&);
RefPtr<CSSValue> consumeCounterReset(CSSParserTokenRange&);
RefPtr<CSSValue> consumeSize(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeTextIndent(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeScrollPadding(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeMaxWidthOrHeight(CSSParserTokenRange&, const CSSParserContext&, UnitlessQuirk = UnitlessQuirk::Forbid);
RefPtr<CSSValue> consumeWidthOrHeight(CSSParserTokenRange&, const CSSParserContext&, UnitlessQuirk = UnitlessQuirk::Forbid);
RefPtr<CSSValue> consumeMarginOrOffset(CSSParserTokenRange&, CSSParserMode, UnitlessQuirk);
RefPtr<CSSValue> consumeMarginSide(CSSParserTokenRange&, CSSPropertyID currentShorthand, CSSParserMode);
RefPtr<CSSValue> consumeSide(CSSParserTokenRange&, CSSPropertyID currentShorthand, CSSParserMode);
RefPtr<CSSValue> consumeClip(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeTouchAction(CSSParserTokenRange&);
RefPtr<CSSValue> consumeWebkitLineClamp(CSSParserTokenRange&);
RefPtr<CSSValue> consumeHyphenateLimit(CSSParserTokenRange&, CSSValueID);
RefPtr<CSSValue> consumeColumnWidth(CSSParserTokenRange&);
RefPtr<CSSValue> consumeColumnCount(CSSParserTokenRange&);
RefPtr<CSSValue> consumeGapLength(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeZoom(CSSParserTokenRange&);
RefPtr<CSSValue> consumeAnimationIterationCount(CSSParserTokenRange&);
RefPtr<CSSValue> consumeAnimationName(CSSParserTokenRange&);
RefPtr<CSSValue> consumeTransitionProperty(CSSParserTokenRange&);
RefPtr<CSSValue> consumeSteps(CSSParserTokenRange&);
RefPtr<CSSValue> consumeCubicBezier(CSSParserTokenRange&);
RefPtr<CSSValue> consumeSpringFunction(CSSParserTokenRange&);
RefPtr<CSSValue> consumeAnimationTimingFunction(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeAnimationValue(CSSPropertyID, CSSParserTokenRange&, const CSSParserContext&);
bool isValidAnimationPropertyList(CSSPropertyID, const CSSValueList&);
RefPtr<CSSValue> consumeAnimationPropertyList(CSSPropertyID, CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeZIndex(CSSParserTokenRange&);
RefPtr<CSSValue> consumeShadow(CSSParserTokenRange&, const CSSParserContext&, bool isBoxShadowProperty);
RefPtr<CSSValue> consumeTextShadow(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeBoxShadow(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeWebkitBoxShadow(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeTextDecorationLine(CSSParserTokenRange&);
RefPtr<CSSValue> consumeTextEmphasisStyle(CSSParserTokenRange&);
RefPtr<CSSValue> consumeOutlineColor(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeLineWidth(CSSParserTokenRange&, CSSParserMode, UnitlessQuirk);
RefPtr<CSSValue> consumeBorderWidth(CSSParserTokenRange&, CSSPropertyID currentShorthand, const CSSParserContext&);
RefPtr<CSSValue> consumeBorderColor(CSSParserTokenRange&, CSSPropertyID currentShorthand, const CSSParserContext&);
RefPtr<CSSValue> consumeTextStrokeWidth(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeColumnRuleWidth(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeOpacity(CSSParserTokenRange&);
RefPtr<CSSValue> consumeTransformValue(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeTransform(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeTranslate(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeScale(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeRotate(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumePositionX(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumePositionY(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumePaintStroke(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeGlyphOrientationHorizontal(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeGlyphOrientationVertical(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumePaintOrder(CSSParserTokenRange&);
RefPtr<CSSValue> consumeFlexBasis(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeKerning(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeStrokeDasharray(CSSParserTokenRange&);
RefPtr<CSSValue> consumeBaselineShift(CSSParserTokenRange&);
RefPtr<CSSValue> consumeRxOrRy(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeCursor(CSSParserTokenRange&, const CSSParserContext&, bool inQuirksMode);
RefPtr<CSSValue> consumeAttr(CSSParserTokenRange args, const CSSParserContext&);
RefPtr<CSSValue> consumeCounterContent(CSSParserTokenRange args, bool counters);
RefPtr<CSSValue> consumeContent(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumePerspective(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeScrollSnapAlign(CSSParserTokenRange&);
RefPtr<CSSValue> consumeScrollSnapType(CSSParserTokenRange&);
RefPtr<CSSValue> consumeOverscrollBehavior(CSSParserTokenRange&);
RefPtr<CSSValue> consumeBorderRadiusCorner(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeTextUnderlineOffset(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeTextDecorationThickness(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeVerticalAlign(CSSParserTokenRange&, CSSParserMode);
bool consumeRadii(RefPtr<CSSPrimitiveValue> horizontalRadii[4], RefPtr<CSSPrimitiveValue> verticalRadii[4], CSSParserTokenRange&, CSSParserMode, bool useLegacyParsing);
enum class ConsumeRay { Include, Exclude };
RefPtr<CSSValue> consumePathOperation(CSSParserTokenRange&, const CSSParserContext&, ConsumeRay);
RefPtr<CSSValue> consumeShapeOutside(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeBaselineKeyword(CSSParserTokenRange&);
RefPtr<CSSValue> consumeContentDistributionOverflowPosition(CSSParserTokenRange&, IsPositionKeyword);
RefPtr<CSSValue> consumeJustifyContent(CSSParserTokenRange&);
RefPtr<CSSValue> consumeBorderImageRepeat(CSSParserTokenRange&);
RefPtr<CSSValue> consumeBorderImageSlice(CSSPropertyID, CSSParserTokenRange&);
RefPtr<CSSValue> consumeBorderImageOutset(CSSParserTokenRange&);
RefPtr<CSSValue> consumeBorderImageWidth(CSSPropertyID, CSSParserTokenRange&);
bool consumeBorderImageComponents(CSSPropertyID, CSSParserTokenRange&, const CSSParserContext&, RefPtr<CSSValue>&, RefPtr<CSSValue>&, RefPtr<CSSValue>&, RefPtr<CSSValue>&, RefPtr<CSSValue>&);
RefPtr<CSSValue> consumeWebkitBorderImage(CSSPropertyID, CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeReflect(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeBackgroundBlendMode(CSSParserTokenRange&);
RefPtr<CSSValue> consumeBackgroundAttachment(CSSParserTokenRange&);
RefPtr<CSSValue> consumeBackgroundBox(CSSParserTokenRange&);
RefPtr<CSSValue> consumeMaskClip(CSSParserTokenRange&);
RefPtr<CSSValue> consumeBackgroundClip(CSSParserTokenRange&);
RefPtr<CSSValue> consumePrefixedMaskComposite(CSSParserTokenRange&);
RefPtr<CSSValue> consumeMaskComposite(CSSParserTokenRange&);
RefPtr<CSSValue> consumeWebkitMaskSourceType(CSSParserTokenRange&);
RefPtr<CSSValue> consumeWebkitMaskMode(CSSParserTokenRange&);
RefPtr<CSSValue> consumeBackgroundSize(CSSPropertyID, CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeGridAutoFlow(CSSParserTokenRange&);
RefPtr<CSSValue> consumeRepeatStyle(CSSParserTokenRange&);
RefPtr<CSSValue> consumeBackgroundComponent(CSSPropertyID, CSSParserTokenRange&, const CSSParserContext&);
void addBackgroundValue(RefPtr<CSSValue>&, Ref<CSSValue>&&);
RefPtr<CSSValue> consumeCommaSeparatedBackgroundComponent(CSSPropertyID, CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeSelfPositionOverflowPosition(CSSParserTokenRange&, IsPositionKeyword);
RefPtr<CSSValue> consumeAlignItems(CSSParserTokenRange&);
RefPtr<CSSValue> consumeJustifyItems(CSSParserTokenRange&);
RefPtr<CSSValue> consumeGridLine(CSSParserTokenRange&);
bool parseGridTemplateAreasRow(StringView gridRowNames, NamedGridAreaMap&, const size_t rowCount, size_t& columnCount);
RefPtr<CSSValue> consumeGridTrackSize(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSGridLineNamesValue> consumeGridLineNames(CSSParserTokenRange&, CSSGridLineNamesValue* = nullptr, bool allowEmpty = false);
enum TrackListType { GridTemplate, GridTemplateNoRepeat, GridAuto };
RefPtr<CSSValue> consumeGridTrackList(CSSParserTokenRange&, const CSSParserContext&, TrackListType);
RefPtr<CSSValue> consumeGridTemplatesRowsOrColumns(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeGridTemplateAreas(CSSParserTokenRange&);
RefPtr<CSSValue> consumeLineBoxContain(CSSParserTokenRange&);
RefPtr<CSSValue> consumeWebkitLineGrid(CSSParserTokenRange&);
RefPtr<CSSValue> consumeContainerName(CSSParserTokenRange&);
RefPtr<CSSValue> consumeWebkitInitialLetter(CSSParserTokenRange&);
RefPtr<CSSValue> consumeSpeakAs(CSSParserTokenRange&);
RefPtr<CSSValue> consumeHangingPunctuation(CSSParserTokenRange&);
RefPtr<CSSValue> consumeWebkitMarqueeIncrement(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeWebkitMarqueeRepetition(CSSParserTokenRange&);
RefPtr<CSSValue> consumeWebkitMarqueeSpeed(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSValue> consumeAlt(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeContain(CSSParserTokenRange&);
RefPtr<CSSValue> consumeContainIntrinsicSize(CSSParserTokenRange&);
RefPtr<CSSValue> consumeTextEmphasisPosition(CSSParserTokenRange&);
#if ENABLE(DARK_MODE_CSS)
RefPtr<CSSValue> consumeColorScheme(CSSParserTokenRange&);
#endif
RefPtr<CSSValue> consumeListStyleType(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeOffsetRotate(CSSParserTokenRange&, CSSParserMode);

} // namespace CSSPropertyParserHelpers

} // namespace WebCore
