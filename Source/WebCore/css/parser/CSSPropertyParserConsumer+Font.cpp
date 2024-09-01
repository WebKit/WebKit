/*
 * Copyright (C) 2016 The Chromium Authors. All rights reserved
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

#include "config.h"
#include "CSSPropertyParserConsumer+Font.h"

#include "CSSCalcSymbolTable.h"
#include "CSSFontFaceSrcValue.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontPaletteValuesOverrideColorsValue.h"
#include "CSSFontStyleWithAngleValue.h"
#include "CSSFontVariantAlternatesValue.h"
#include "CSSFontVariantLigaturesParser.h"
#include "CSSFontVariantNumericParser.h"
#include "CSSParserIdioms.h"
#include "CSSParserImpl.h"
#include "CSSParserToken.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyParserConsumer+Angle.h"
#include "CSSPropertyParserConsumer+AngleDefinitions.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+Color.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+Integer.h"
#include "CSSPropertyParserConsumer+IntegerDefinitions.h"
#include "CSSPropertyParserConsumer+Length.h"
#include "CSSPropertyParserConsumer+LengthDefinitions.h"
#include "CSSPropertyParserConsumer+List.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+Number.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+Percent.h"
#include "CSSPropertyParserConsumer+PercentDefinitions.h"
#include "CSSPropertyParserConsumer+RawResolver.h"
#include "CSSPropertyParserConsumer+URL.h"
#include "CSSPropertyParsing.h"
#include "CSSUnicodeRangeValue.h"
#include "CSSValuePair.h"
#include "Document.h"
#include "FontCustomPlatformData.h"
#include "FontFace.h"
#include "ParsingUtilities.h"
#include "WebKitFontFamilyNames.h"

#if ENABLE(VARIATION_FONTS)
#include "CSSFontStyleRangeValue.h"
#include "CSSFontVariationValue.h"
#endif

namespace WebCore {
namespace CSSPropertyParserHelpers {

static bool isFontStyleAngleInRange(double angleInDegrees)
{
    return angleInDegrees >= -90 && angleInDegrees <= 90;
}

static CSSParserMode parserMode(ScriptExecutionContext& context)
{
    auto* document = dynamicDowncast<Document>(context);
    return (document && document->inQuirksMode()) ? HTMLQuirksMode : HTMLStandardMode;
}

// MARK: - Non-primitive consumers.

static std::optional<double> consumeFontWeightNumberRaw(CSSParserTokenRange& range)
{
#if !ENABLE(VARIATION_FONTS)
    auto isIntegerAndDivisibleBy100 = [](double value) {
        ASSERT(value >= 1);
        ASSERT(value <= 1000);
        return static_cast<int>(value / 100) * 100 == value;
    };
#endif

    auto& token = range.peek();
    switch (token.type()) {
    case FunctionToken: {
        // "[For calc()], the used value resulting from an expression must be clamped to the range allowed in the target context."
        auto unevaluatedCalc = NumberKnownTokenTypeFunctionConsumer::consume(range, { }, { });
        if (!unevaluatedCalc)
            return std::nullopt;
        auto result = RawResolver<NumberRaw>::resolve(*unevaluatedCalc, { }, { });
        if (!result)
            return std::nullopt;
#if !ENABLE(VARIATION_FONTS)
        if (!(result->value >= 1 && result->value <= 1000) || !isIntegerAndDivisibleBy100(result->value))
            return std::nullopt;
#endif
        return std::clamp(result->value, 1.0, 1000.0);
    }

    case NumberToken: {
        auto result = token.numericValue();
        if (!(result >= 1 && result <= 1000))
            return std::nullopt;
#if !ENABLE(VARIATION_FONTS)
        if (token.numericValueType() != IntegerValueType || !isIntegerAndDivisibleBy100(result))
            return std::nullopt;
#endif
        range.consumeIncludingWhitespace();
        return result;
    }

    default:
        return std::nullopt;
    }
}

static RefPtr<CSSPrimitiveValue> consumeFontWeightNumber(CSSParserTokenRange& range)
{
    if (auto result = consumeFontWeightNumberRaw(range))
        return CSSPrimitiveValue::create(*result);
    return nullptr;
}

static std::optional<FontWeightRaw> consumeFontWeightRaw(CSSParserTokenRange& range)
{
    if (auto result = consumeIdentRaw<CSSValueNormal, CSSValueBold, CSSValueBolder, CSSValueLighter>(range))
        return { *result };
    if (auto result = consumeFontWeightNumberRaw(range))
        return { *result };
    return std::nullopt;
}

static std::optional<CSSValueID> consumeFontStretchKeywordValueRaw(CSSParserTokenRange& range)
{
    return consumeIdentRaw<CSSValueUltraCondensed, CSSValueExtraCondensed, CSSValueCondensed, CSSValueSemiCondensed, CSSValueNormal, CSSValueSemiExpanded, CSSValueExpanded, CSSValueExtraExpanded, CSSValueUltraExpanded>(range);
}

static std::optional<FontStyleRaw> consumeFontStyleRaw(CSSParserTokenRange& range, CSSParserMode parserMode)
{
    UNUSED_PARAM(parserMode);

#if ENABLE(VARIATION_FONTS)
    CSSParserTokenRange rangeBeforeKeyword = range;
#endif

    auto keyword = consumeIdentRaw<CSSValueNormal, CSSValueItalic, CSSValueOblique>(range);
    if (!keyword)
        return std::nullopt;

#if ENABLE(VARIATION_FONTS)
    if (*keyword == CSSValueOblique && !range.atEnd()) {
        // FIXME: This angle does specify that unitless 0 is allowed - see https://drafts.csswg.org/css-fonts-4/#valdef-font-style-oblique-angle
        if (auto angle = consumeAngleRaw(range, parserMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow)) {
            if (isFontStyleAngleInRange(CSSPrimitiveValue::computeDegrees(angle->type, angle->value)))
                return { { CSSValueOblique, WTFMove(angle) } };
            // Must not consume anything on error.
            range = rangeBeforeKeyword;
            return std::nullopt;
        }
    }
#endif

    return { { *keyword, std::nullopt } };
}

static AtomString concatenateFamilyName(CSSParserTokenRange& range)
{
    StringBuilder builder;
    bool addedSpace = false;
    const CSSParserToken& firstToken = range.peek();
    while (range.peek().type() == IdentToken) {
        if (!builder.isEmpty()) {
            builder.append(' ');
            addedSpace = true;
        }
        builder.append(range.consumeIncludingWhitespace().value());
    }
    // <family-name> can't contain unquoted generic families
    if (!addedSpace && (!isValidCustomIdentifier(firstToken.id()) || !genericFontFamily(firstToken.id()).isNull()))
        return nullAtom();
    return builder.toAtomString();
}

static AtomString consumeFamilyNameRaw(CSSParserTokenRange& range)
{
    if (range.peek().type() == StringToken)
        return range.consumeIncludingWhitespace().value().toAtomString();
    if (range.peek().type() != IdentToken)
        return nullAtom();
    return concatenateFamilyName(range);
}

static std::optional<CSSValueID> consumeGenericFamilyRaw(CSSParserTokenRange& range)
{
    return consumeIdentRaw<CSSValueSerif, CSSValueSansSerif, CSSValueCursive, CSSValueFantasy, CSSValueMonospace, CSSValueWebkitBody, CSSValueWebkitPictograph, CSSValueSystemUi>(range);
}

static RefPtr<CSSValue> consumeGenericFamily(CSSParserTokenRange& range)
{
    if (auto familyName = consumeGenericFamilyRaw(range)) {
        // FIXME: Remove special case for system-ui.
        if (*familyName == CSSValueSystemUi)
            return CSSValuePool::singleton().createFontFamilyValue(nameString(*familyName));
        return CSSPrimitiveValue::create(*familyName);
    }
    return nullptr;
}

static std::optional<Vector<FontFamilyRaw>> consumeFontFamilyRaw(CSSParserTokenRange& range)
{
    Vector<FontFamilyRaw> list;
    do {
        if (auto ident = consumeGenericFamilyRaw(range))
            list.append({ *ident });
        else {
            auto familyName = consumeFamilyNameRaw(range);
            if (familyName.isNull())
                return std::nullopt;
            list.append({ familyName });
        }
    } while (consumeCommaIncludingWhitespace(range));
    return list;
}

static std::optional<FontSizeRaw> consumeFontSizeRaw(CSSParserTokenRange& range, CSSParserMode parserMode)
{
    // -webkit-xxx-large is a parse-time alias.
    if (range.peek().id() == CSSValueWebkitXxxLarge) {
        if (auto ident = consumeIdentRaw(range); ident && ident == CSSValueWebkitXxxLarge)
            return { CSSValueXxxLarge };
        return std::nullopt;
    }

    if (range.peek().id() >= CSSValueXxSmall && range.peek().id() <= CSSValueLarger) {
        if (auto ident = consumeIdentRaw(range))
            return { *ident };
        return std::nullopt;
    }

    if (auto result = consumeLengthOrPercentRaw(range, parserMode))
        return { *result };

    return std::nullopt;
}

static std::optional<LineHeightRaw> consumeLineHeightRaw(CSSParserTokenRange& range, CSSParserMode parserMode)
{
    if (range.peek().id() == CSSValueNormal) {
        if (auto ident = consumeIdentRaw(range))
            return { *ident };
        return std::nullopt;
    }

    if (auto number = consumeNumberRaw(range, ValueRange::NonNegative))
        return { number->value };

    if (auto lengthOrPercent = consumeLengthOrPercentRaw(range, parserMode))
        return { *lengthOrPercent };

    return std::nullopt;
}

// MARK: 'font' (shorthand)

static std::optional<FontRaw> consumeFontRaw(CSSParserTokenRange& range, CSSParserMode parserMode)
{
    FontRaw result;

    for (unsigned i = 0; i < 4 && !range.atEnd(); ++i) {
        if (consumeIdentRaw<CSSValueNormal>(range))
            continue;
        if (!result.style && (result.style = consumeFontStyleRaw(range, parserMode)))
            continue;
        if (!result.variantCaps && (result.variantCaps = consumeIdentRaw<CSSValueSmallCaps>(range)))
            continue;
        if (!result.weight && (result.weight = consumeFontWeightRaw(range)))
            continue;
        if (!result.stretch && (result.stretch = consumeFontStretchKeywordValueRaw(range)))
            continue;
        break;
    }

    if (range.atEnd())
        return std::nullopt;

    // Now a font size _must_ come.
    if (auto size = consumeFontSizeRaw(range, parserMode))
        result.size = *size;
    else
        return std::nullopt;

    if (range.atEnd())
        return std::nullopt;

    if (consumeSlashIncludingWhitespace(range)) {
        if (!(result.lineHeight = consumeLineHeightRaw(range, parserMode)))
            return std::nullopt;
    }

    // Font family must come now.
    if (auto family = consumeFontFamilyRaw(range))
        result.family = *family;
    else
        return std::nullopt;

    if (!range.atEnd())
        return std::nullopt;

    return result;
}

std::optional<FontRaw> parseFont(const String& string, CSSParserMode mode)
{
    CSSTokenizer tokenizer(string);
    CSSParserTokenRange range(tokenizer.tokenRange());
    range.consumeWhitespace();

    return consumeFontRaw(range, mode);
}

// MARK: 'font-variant-ligatures'

RefPtr<CSSValue> consumeFontVariantLigatures(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNormal || range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    CSSFontVariantLigaturesParser ligaturesParser;
    do {
        if (ligaturesParser.consumeLigature(range) != CSSFontVariantLigaturesParser::ParseResult::ConsumedValue)
            return nullptr;
    } while (!range.atEnd());

    return ligaturesParser.finalizeValue();
}

// MARK: 'font-variant-east-asian'

RefPtr<CSSValue> consumeFontVariantEastAsian(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);

    std::optional<FontVariantEastAsianVariant> variant;
    std::optional<FontVariantEastAsianWidth> width;
    std::optional<FontVariantEastAsianRuby> ruby;

    auto parseSomethingWithoutError = [&range, &variant, &width, &ruby] () {
        bool hasParsedSomething = false;

        while (true) {
            if (range.peek().type() != IdentToken)
                return hasParsedSomething;

            switch (range.peek().id()) {
            case CSSValueJis78:
                if (variant)
                    return false;
                variant = FontVariantEastAsianVariant::Jis78;
                break;
            case CSSValueJis83:
                if (variant)
                    return false;
                variant = FontVariantEastAsianVariant::Jis83;
                break;
            case CSSValueJis90:
                if (variant)
                    return false;
                variant = FontVariantEastAsianVariant::Jis90;
                break;
            case CSSValueJis04:
                if (variant)
                    return false;
                variant = FontVariantEastAsianVariant::Jis04;
                break;
            case CSSValueSimplified:
                if (variant)
                    return false;
                variant = FontVariantEastAsianVariant::Simplified;
                break;
            case CSSValueTraditional:
                if (variant)
                    return false;
                variant = FontVariantEastAsianVariant::Traditional;
                break;
            case CSSValueFullWidth:
                if (width)
                    return false;
                width = FontVariantEastAsianWidth::Full;
                break;
            case CSSValueProportionalWidth:
                if (width)
                    return false;
                width = FontVariantEastAsianWidth::Proportional;
                break;
            case CSSValueRuby:
                if (ruby)
                    return false;
                ruby = FontVariantEastAsianRuby::Yes;
                break;
            default:
                return hasParsedSomething;
            }

            range.consumeIncludingWhitespace();
            hasParsedSomething = true;
        }
    };

    if (!parseSomethingWithoutError())
        return nullptr;

    CSSValueListBuilder values;
    switch (variant.value_or(FontVariantEastAsianVariant::Normal)) {
    case FontVariantEastAsianVariant::Normal:
        break;
    case FontVariantEastAsianVariant::Jis78:
        values.append(CSSPrimitiveValue::create(CSSValueJis78));
        break;
    case FontVariantEastAsianVariant::Jis83:
        values.append(CSSPrimitiveValue::create(CSSValueJis83));
        break;
    case FontVariantEastAsianVariant::Jis90:
        values.append(CSSPrimitiveValue::create(CSSValueJis90));
        break;
    case FontVariantEastAsianVariant::Jis04:
        values.append(CSSPrimitiveValue::create(CSSValueJis04));
        break;
    case FontVariantEastAsianVariant::Simplified:
        values.append(CSSPrimitiveValue::create(CSSValueSimplified));
        break;
    case FontVariantEastAsianVariant::Traditional:
        values.append(CSSPrimitiveValue::create(CSSValueTraditional));
        break;
    }

    switch (width.value_or(FontVariantEastAsianWidth::Normal)) {
    case FontVariantEastAsianWidth::Normal:
        break;
    case FontVariantEastAsianWidth::Full:
        values.append(CSSPrimitiveValue::create(CSSValueFullWidth));
        break;
    case FontVariantEastAsianWidth::Proportional:
        values.append(CSSPrimitiveValue::create(CSSValueProportionalWidth));
        break;
    }

    switch (ruby.value_or(FontVariantEastAsianRuby::Normal)) {
    case FontVariantEastAsianRuby::Normal:
        break;
    case FontVariantEastAsianRuby::Yes:
        values.append(CSSPrimitiveValue::create(CSSValueRuby));
    }

    if (values.isEmpty())
        return nullptr;

    return CSSValueList::createSpaceSeparated(WTFMove(values));
}

// MARK: 'font-variant-alternates'

RefPtr<CSSValue> consumeFontVariantAlternates(CSSParserTokenRange& range)
{
    if (range.atEnd())
        return nullptr;

    if (range.peek().id() == CSSValueNormal) {
        consumeIdent<CSSValueNormal>(range);
        return CSSPrimitiveValue::create(CSSValueNormal);
    }

    auto result = FontVariantAlternates::Normal();

    auto parseSomethingWithoutError = [&range, &result]() {
        bool hasParsedSomething = false;
        auto parseAndSetArgument = [&range, &hasParsedSomething] (String& value) {
            if (!value.isNull())
                return false;

            CSSParserTokenRange args = consumeFunction(range);
            auto ident = consumeCustomIdent(args);
            if (!args.atEnd())
                return false;

            if (!ident)
                return false;

            hasParsedSomething = true;
            value = ident->stringValue();
            return true;
        };
        auto parseListAndSetArgument = [&range, &hasParsedSomething] (Vector<String>& value) {
            if (!value.isEmpty())
                return false;

            CSSParserTokenRange args = consumeFunction(range);
            do {
                auto ident = consumeCustomIdent(args);
                if (!ident)
                    return false;
                value.append(ident->stringValue());
            } while (consumeCommaIncludingWhitespace(args));

            if (!args.atEnd())
                return false;

            hasParsedSomething = true;
            return true;
        };
        while (true) {
            const CSSParserToken& token = range.peek();
            if (token.id() == CSSValueHistoricalForms) {
                consumeIdent<CSSValueHistoricalForms>(range);

                if (result.valuesRef().historicalForms)
                    return false;

                if (result.isNormal())
                    result.setValues();

                hasParsedSomething = true;
                result.valuesRef().historicalForms = true;
            } else if (token.functionId() == CSSValueSwash) {
                if (!parseAndSetArgument(result.valuesRef().swash))
                    return false;
            } else if (token.functionId() == CSSValueStylistic) {
                if (!parseAndSetArgument(result.valuesRef().stylistic))
                    return false;
            } else if (token.functionId() == CSSValueStyleset) {
                if (!parseListAndSetArgument(result.valuesRef().styleset))
                    return false;
            } else if (token.functionId() == CSSValueCharacterVariant) {
                if (!parseListAndSetArgument(result.valuesRef().characterVariant))
                    return false;
            } else if (token.functionId() == CSSValueOrnaments) {
                if (!parseAndSetArgument(result.valuesRef().ornaments))
                    return false;
            } else if (token.functionId() == CSSValueAnnotation) {
                if (!parseAndSetArgument(result.valuesRef().annotation))
                    return false;
            } else
                return hasParsedSomething;
        }
    };

    if (parseSomethingWithoutError())
        return CSSFontVariantAlternatesValue::create(WTFMove(result));

    return nullptr;
}

// MARK: 'font-variant-numeric'

RefPtr<CSSValue> consumeFontVariantNumeric(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);

    CSSFontVariantNumericParser numericParser;
    do {
        if (numericParser.consumeNumeric(range) != CSSFontVariantNumericParser::ParseResult::ConsumedValue)
            return nullptr;
    } while (!range.atEnd());

    return numericParser.finalizeValue();
}

// MARK: 'font-size-adjust'

RefPtr<CSSValue> consumeFontSizeAdjust(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNone || range.peek().id() == CSSValueFromFont)
        return consumeIdent(range);

    auto metric = consumeIdent<CSSValueExHeight, CSSValueCapHeight, CSSValueChWidth, CSSValueIcWidth, CSSValueIcHeight>(range);
    auto value = consumeNumber(range, ValueRange::NonNegative);
    if (!value)
        value = consumeIdent<CSSValueFromFont>(range);

    if (!value || !metric || metric->valueID() == CSSValueExHeight)
        return value;

    return CSSValuePair::create(metric.releaseNonNull(), value.releaseNonNull());
}

// MARK: 'font-family'

static RefPtr<CSSValue> consumeFamilyName(CSSParserTokenRange& range)
{
    // // https://drafts.csswg.org/css-fonts-4/#family-name-syntax

    auto familyName = consumeFamilyNameRaw(range);
    if (familyName.isNull())
        return nullptr;
    return CSSValuePool::singleton().createFontFamilyValue(familyName);
}

RefPtr<CSSValue> consumeFontFamily(CSSParserTokenRange& range)
{
    return consumeCommaSeparatedListWithoutSingleValueOptimization(range, [] (auto& range) -> RefPtr<CSSValue> {
        if (auto parsedValue = consumeGenericFamily(range))
            return parsedValue;
        return consumeFamilyName(range);
    });
}

const AtomString& genericFontFamily(CSSValueID ident)
{
    switch (ident) {
    case CSSValueSerif:
        return WebKitFontFamilyNames::serifFamily.get();
    case CSSValueSansSerif:
        return WebKitFontFamilyNames::sansSerifFamily.get();
    case CSSValueCursive:
        return WebKitFontFamilyNames::cursiveFamily.get();
    case CSSValueFantasy:
        return WebKitFontFamilyNames::fantasyFamily.get();
    case CSSValueMonospace:
        return WebKitFontFamilyNames::monospaceFamily.get();
    case CSSValueWebkitPictograph:
        return WebKitFontFamilyNames::pictographFamily.get();
    case CSSValueSystemUi:
        return WebKitFontFamilyNames::systemUiFamily.get();
    default:
        return nullAtom();
    }
}

WebKitFontFamilyNames::FamilyNamesIndex genericFontFamilyIndex(CSSValueID ident)
{
    switch (ident) {
    case CSSValueSerif:
        return WebKitFontFamilyNames::FamilyNamesIndex::SerifFamily;
    case CSSValueSansSerif:
        return WebKitFontFamilyNames::FamilyNamesIndex::SansSerifFamily;
    case CSSValueCursive:
        return WebKitFontFamilyNames::FamilyNamesIndex::CursiveFamily;
    case CSSValueFantasy:
        return WebKitFontFamilyNames::FamilyNamesIndex::FantasyFamily;
    case CSSValueMonospace:
        return WebKitFontFamilyNames::FamilyNamesIndex::MonospaceFamily;
    case CSSValueWebkitPictograph:
        return WebKitFontFamilyNames::FamilyNamesIndex::PictographFamily;
    case CSSValueSystemUi:
        return WebKitFontFamilyNames::FamilyNamesIndex::SystemUiFamily;
    default:
        ASSERT_NOT_REACHED();
        return WebKitFontFamilyNames::FamilyNamesIndex::StandardFamily;
    }
}

// MARK: 'font-style'

#if ENABLE(VARIATION_FONTS)

static RefPtr<CSSPrimitiveValue> consumeFontStyleAngle(CSSParserTokenRange& range, CSSParserMode mode)
{
    auto rangeAfterAngle = range;
    RefPtr angle = consumeAngle(rangeAfterAngle, mode);
    if (!angle)
        return nullptr;
    if (!angle->isCalculated() && !isFontStyleAngleInRange(angle->resolveAsAngleNoConversionDataRequired()))
        return nullptr;
    range = rangeAfterAngle;
    return angle;
}

#endif

RefPtr<CSSValue> consumeFontStyle(CSSParserTokenRange& range, CSSParserMode mode)
{
    UNUSED_PARAM(mode);

    RefPtr keyword = consumeIdent<CSSValueNormal, CSSValueItalic, CSSValueOblique>(range);
    if (!keyword)
        return nullptr;

#if ENABLE(VARIATION_FONTS)
    if (keyword->valueID() == CSSValueOblique && !range.atEnd()) {
        if (auto angle = consumeFontStyleAngle(range, mode))
            return CSSFontStyleWithAngleValue::create(angle.releaseNonNull());
    }
#endif

    return keyword;
}

#if ENABLE(VARIATION_FONTS)

RefPtr<CSSValue> consumeFontStyleRange(CSSParserTokenRange& range, CSSParserMode mode)
{
    RefPtr keyword = consumeIdent<CSSValueNormal, CSSValueItalic, CSSValueOblique>(range);
    if (!keyword)
        return nullptr;

    if (keyword->valueID() != CSSValueOblique || range.atEnd())
        return CSSFontStyleRangeValue::create(keyword.releaseNonNull());

    auto rangeAfterAngles = range;
    auto firstAngle = consumeFontStyleAngle(rangeAfterAngles, mode);
    if (!firstAngle)
        return nullptr;

    RefPtr<CSSValueList> angleList;
    if (rangeAfterAngles.atEnd())
        angleList = CSSValueList::createSpaceSeparated(firstAngle.releaseNonNull());
    else {
        RefPtr secondAngle = consumeFontStyleAngle(rangeAfterAngles, mode);
        if (!secondAngle)
            return nullptr;
        angleList = CSSValueList::createSpaceSeparated(firstAngle.releaseNonNull(), secondAngle.releaseNonNull());
    }

    range = rangeAfterAngles;
    return CSSFontStyleRangeValue::create(keyword.releaseNonNull(), angleList.releaseNonNull());
}
#endif

// MARK: 'font-stretch'

#if ENABLE(VARIATION_FONTS)

RefPtr<CSSValue> consumeFontStretchRange(CSSParserTokenRange& range)
{
    if (RefPtr result = CSSPropertyParsing::consumeFontStretchAbsolute(range))
        return result;
    RefPtr firstPercent = consumePercent(range, ValueRange::NonNegative);
    if (!firstPercent)
        return nullptr;
    if (range.atEnd())
        return firstPercent;
    RefPtr secondPercent = consumePercent(range, ValueRange::NonNegative);
    if (!secondPercent)
        return nullptr;
    return CSSValueList::createSpaceSeparated(firstPercent.releaseNonNull(), secondPercent.releaseNonNull());
}

#else

RefPtr<CSSValue> consumeFontStretch(CSSParserTokenRange& range)
{
    if (RefPtr result = CSSPropertyParsing::consumeFontStretchAbsolute(range))
        return result;
    if (RefPtr percent = consumePercent(range, ValueRange::NonNegative))
        return percent;
    return nullptr;
}

#endif

// MARK: 'font-weight'

RefPtr<CSSValue> consumeFontWeight(CSSParserTokenRange& range)
{
    if (auto result = consumeFontWeightRaw(range)) {
        return WTF::switchOn(*result, [] (CSSValueID valueID) {
            return CSSPrimitiveValue::create(valueID);
        }, [] (double weightNumber) {
            return CSSPrimitiveValue::create(weightNumber);
        });
    }
    return nullptr;
}

static RefPtr<CSSPrimitiveValue> consumeFontWeightAbsoluteKeywordValue(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueNormal, CSSValueBold>(range);
}

#if ENABLE(VARIATION_FONTS)

RefPtr<CSSValue> consumeFontWeightAbsoluteRange(CSSParserTokenRange& range)
{
    if (RefPtr result = consumeFontWeightAbsoluteKeywordValue(range))
        return result;
    RefPtr firstNumber = consumeFontWeightNumber(range);
    if (!firstNumber)
        return nullptr;
    if (range.atEnd())
        return firstNumber;
    RefPtr secondNumber = consumeFontWeightNumber(range);
    if (!secondNumber)
        return nullptr;
    return CSSValueList::createSpaceSeparated(firstNumber.releaseNonNull(), secondNumber.releaseNonNull());
}

#else

RefPtr<CSSPrimitiveValue> consumeFontWeightAbsolute(CSSParserTokenRange& range)
{
    if (RefPtr result = consumeFontWeightAbsoluteKeywordValue(range))
        return result;
    return consumeFontWeightNumber(range);
}

#endif


// MARK: - @-rule descriptor consumers:

// MARK: - @font-face

// MARK: @font-face 'font-family'
RefPtr<CSSValue> consumeFontFaceFontFamily(CSSParserTokenRange& range)
{
    auto name = consumeFamilyName(range);
    if (!name || !range.atEnd())
        return nullptr;

    // FIXME-NEWPARSER: https://bugs.webkit.org/show_bug.cgi?id=196381 For compatibility with the old parser, we have to make
    // a list here, even though the list always contains only a single family name.
    // Once the old parser is gone, we can delete this function, make the caller
    // use consumeFamilyName instead, and then patch the @font-face code to
    // not expect a list with a single name in it.
    return CSSValueList::createCommaSeparated(name.releaseNonNull());
}

// MARK: @font-face 'src'

Vector<FontTechnology> consumeFontTech(CSSParserTokenRange& range, bool singleValue)
{
    Vector<FontTechnology> technologies;
    auto args = consumeFunction(range);
    do {
        auto& arg = args.consumeIncludingWhitespace();
        if (arg.type() != IdentToken)
            return { };
        auto technology = fromCSSValueID<FontTechnology>(arg.id());
        if (technology == FontTechnology::Invalid)
            return { };
        technologies.append(technology);
    } while (consumeCommaIncludingWhitespace(args) && !singleValue);
    if (!args.atEnd())
        return { };
    return technologies;
}

static bool isFontFormatKeywordValid(CSSValueID id)
{
    switch (id) {
    case CSSValueCollection:
    case CSSValueEmbeddedOpentype:
    case CSSValueOpentype:
    case CSSValueSvg:
    case CSSValueTruetype:
    case CSSValueWoff:
    case CSSValueWoff2:
        return true;
    default:
        return false;
    }
}

String consumeFontFormat(CSSParserTokenRange& range, bool rejectStringValues)
{
    // https://drafts.csswg.org/css-fonts/#descdef-font-face-src
    // FIXME: We allow any identifier here and convert to strings; specification calls for certain keywords and legacy compatibility strings.
    auto args = consumeFunction(range);
    auto& arg = args.consumeIncludingWhitespace();
    if (!args.atEnd())
        return nullString();
    if (arg.type() == IdentToken && isFontFormatKeywordValid(arg.id()))
        return arg.value().toString();
    if (arg.type() == StringToken && !rejectStringValues)
        return arg.value().toString();
    return nullString();
}

static RefPtr<CSSFontFaceSrcResourceValue> consumeFontFaceSrcURI(CSSParserTokenRange& range, const CSSParserContext& context)
{
    StringView parsedURL = consumeURLRaw(range);
    String urlString = !parsedURL.is8Bit() && parsedURL.containsOnlyASCII() ? String::make8Bit(parsedURL.span16()) : parsedURL.toString();
    auto location = context.completeURL(urlString);
    if (location.resolvedURL.isNull())
        return nullptr;

    String format;
    Vector<FontTechnology> technologies;
    if (range.peek().functionId() == CSSValueFormat) {
        format = consumeFontFormat(range);
        if (format.isNull())
            return nullptr;
    }
    if (range.peek().functionId() == CSSValueTech) {
        technologies = consumeFontTech(range);
        if (technologies.isEmpty())
            return nullptr;
    }
    if (!range.atEnd())
        return nullptr;

    return CSSFontFaceSrcResourceValue::create(WTFMove(location), WTFMove(format), WTFMove(technologies), context.isContentOpaque ? LoadedFromOpaqueSource::Yes : LoadedFromOpaqueSource::No);
}

static RefPtr<CSSValue> consumeFontFaceSrcLocal(CSSParserTokenRange& range)
{
    CSSParserTokenRange args = consumeFunction(range);
    if (args.peek().type() == StringToken) {
        auto& arg = args.consumeIncludingWhitespace();
        if (!args.atEnd() || !range.atEnd())
            return nullptr;
        return CSSFontFaceSrcLocalValue::create(arg.value().toAtomString());
    }
    if (args.peek().type() == IdentToken) {
        AtomString familyName = concatenateFamilyName(args);
        if (familyName.isNull() || !args.atEnd() || !range.atEnd())
            return nullptr;
        return CSSFontFaceSrcLocalValue::create(WTFMove(familyName));
    }
    return nullptr;
}

RefPtr<CSSValueList> consumeFontFaceSrc(CSSParserTokenRange& range, const CSSParserContext& context)
{
    CSSValueListBuilder values;
    auto consumeSrcListComponent = [&](CSSParserTokenRange& range) -> RefPtr<CSSValue> {
        const CSSParserToken& token = range.peek();
        if (token.type() == CSSParserTokenType::UrlToken || token.functionId() == CSSValueUrl)
            return consumeFontFaceSrcURI(range, context);
        if (token.functionId() == CSSValueLocal)
            return consumeFontFaceSrcLocal(range);
        return nullptr;
    };
    while (!range.atEnd()) {
        auto begin = range.begin();
        while (!range.atEnd() && range.peek().type() != CSSParserTokenType::CommaToken)
            range.consumeComponentValue();
        auto subrange = range.makeSubRange(begin, &range.peek());
        if (RefPtr parsedValue = consumeSrcListComponent(subrange))
            values.append(parsedValue.releaseNonNull());
        if (!range.atEnd())
            range.consumeIncludingWhitespace();
    }
    if (values.isEmpty())
        return nullptr;
    return CSSValueList::createCommaSeparated(WTFMove(values));
}

RefPtr<CSSValueList> parseFontFaceSrc(const String& string, const CSSParserContext& context)
{
    CSSParserImpl parser(context, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();
    range.consumeWhitespace();
    if (range.atEnd())
        return nullptr;
    RefPtr parsedValue = consumeFontFaceSrc(range, parser.context());
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

// MARK: @font-face 'size-adjust'

RefPtr<CSSValue> parseFontFaceSizeAdjust(const String& string, ScriptExecutionContext& context)
{
    CSSParserContext parserContext(parserMode(context));
    CSSParserImpl parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();
    range.consumeWhitespace();
    if (range.atEnd())
        return nullptr;
    RefPtr parsedValue = consumePercent(range, ValueRange::NonNegative);
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

// MARK: @font-face 'unicode-range'

static bool consumeOptionalDelimiter(CSSParserTokenRange& range, UChar value)
{
    if (!(range.peek().type() == DelimiterToken && range.peek().delimiter() == value))
        return false;
    range.consume();
    return true;
}

static StringView consumeIdentifier(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken)
        return { };
    return range.consume().value();
}

static bool consumeAndAppendOptionalNumber(StringBuilder& builder, CSSParserTokenRange& range, CSSParserTokenType type = NumberToken)
{
    if (range.peek().type() != type)
        return false;
    auto originalText = range.consume().originalText();
    if (originalText.isNull())
        return false;
    builder.append(originalText);
    return true;
}

static bool consumeAndAppendOptionalDelimiter(StringBuilder& builder, CSSParserTokenRange& range, UChar value)
{
    if (!consumeOptionalDelimiter(range, value))
        return false;
    builder.append(value);
    return true;
}

static void consumeAndAppendOptionalQuestionMarks(StringBuilder& builder, CSSParserTokenRange& range)
{
    while (consumeAndAppendOptionalDelimiter(builder, range, '?')) { }
}

static String consumeUnicodeRangeString(CSSParserTokenRange& range)
{
    if (!equalLettersIgnoringASCIICase(consumeIdentifier(range), "u"_s))
        return { };
    StringBuilder builder;
    if (consumeAndAppendOptionalNumber(builder, range, DimensionToken))
        consumeAndAppendOptionalQuestionMarks(builder, range);
    else if (consumeAndAppendOptionalNumber(builder, range)) {
        if (!(consumeAndAppendOptionalNumber(builder, range, DimensionToken) || consumeAndAppendOptionalNumber(builder, range)))
            consumeAndAppendOptionalQuestionMarks(builder, range);
    } else if (consumeOptionalDelimiter(range, '+')) {
        builder.append('+');
        if (auto identifier = consumeIdentifier(range); !identifier.isNull())
            builder.append(identifier);
        else if (!consumeAndAppendOptionalDelimiter(builder, range, '?'))
            return { };
        consumeAndAppendOptionalQuestionMarks(builder, range);
    } else
        return { };
    return builder.toString();
}

struct UnicodeRange {
    char32_t start;
    char32_t end;
};

static std::optional<UnicodeRange> consumeUnicodeRange(CSSParserTokenRange& range)
{
    return readCharactersForParsing(consumeUnicodeRangeString(range), [&](auto buffer) -> std::optional<UnicodeRange> {
        if (!skipExactly(buffer, '+'))
            return std::nullopt;
        char32_t start = 0;
        unsigned hexDigitCount = 0;
        while (buffer.hasCharactersRemaining() && isASCIIHexDigit(*buffer)) {
            if (++hexDigitCount > 6)
                return std::nullopt;
            start <<= 4;
            start |= toASCIIHexValue(*buffer++);
        }
        auto end = start;
        while (skipExactly(buffer, '?')) {
            if (++hexDigitCount > 6)
                return std::nullopt;
            start <<= 4;
            end <<= 4;
            end |= 0xF;
        }
        if (!hexDigitCount)
            return std::nullopt;
        if (start == end && buffer.hasCharactersRemaining()) {
            if (!skipExactly(buffer, '-'))
                return std::nullopt;
            end = 0;
            hexDigitCount = 0;
            while (buffer.hasCharactersRemaining() && isASCIIHexDigit(*buffer)) {
                if (++hexDigitCount > 6)
                    return std::nullopt;
                end <<= 4;
                end |= toASCIIHexValue(*buffer++);
            }
            if (!hexDigitCount)
                return std::nullopt;
        }
        if (buffer.hasCharactersRemaining())
            return std::nullopt;
        return { { start, end } };
    });
}

RefPtr<CSSValueList> consumeFontFaceUnicodeRange(CSSParserTokenRange& range)
{
    CSSValueListBuilder values;
    do {
        auto unicodeRange = consumeUnicodeRange(range);
        range.consumeWhitespace();
        if (!unicodeRange || unicodeRange->end > UCHAR_MAX_VALUE || unicodeRange->start > unicodeRange->end)
            return nullptr;
        values.append(CSSUnicodeRangeValue::create(unicodeRange->start, unicodeRange->end));
    } while (consumeCommaIncludingWhitespace(range));
    return CSSValueList::createCommaSeparated(WTFMove(values));
}


RefPtr<CSSValueList> parseFontFaceUnicodeRange(const String& string, ScriptExecutionContext& context)
{
    CSSParserContext parserContext(parserMode(context));
    CSSParserImpl parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();
    range.consumeWhitespace();
    if (range.atEnd())
        return nullptr;
    RefPtr parsedValue = consumeFontFaceUnicodeRange(range);
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

// MARK: @font-face 'font-display'

RefPtr<CSSPrimitiveValue> parseFontFaceDisplay(const String& string, ScriptExecutionContext& context)
{
    CSSParserContext parserContext(parserMode(context));
    CSSParserImpl parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();
    range.consumeWhitespace();
    if (range.atEnd())
        return nullptr;
    RefPtr parsedValue = consumeFontFaceFontDisplay(range);
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

RefPtr<CSSPrimitiveValue> consumeFontFaceFontDisplay(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueAuto, CSSValueBlock, CSSValueSwap, CSSValueFallback, CSSValueOptional>(range);
}

// MARK: @font-face 'font-style'

RefPtr<CSSValue> parseFontFaceStyle(const String& string, ScriptExecutionContext& context)
{
    CSSParserContext parserContext(parserMode(context));
    CSSParserImpl parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();
    range.consumeWhitespace();
    if (range.atEnd())
        return nullptr;
    RefPtr parsedValue =
#if ENABLE(VARIATION_FONTS)
        consumeFontStyleRange(range, parserContext.mode);
#else
        consumeFontStyle(range, parserContext.mode);
#endif
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

// MARK: @font-face 'font-feature-settings' / @font-face 'font-variation-settings'

RefPtr<CSSValue> parseFontFaceFeatureSettings(const String& string, ScriptExecutionContext& context)
{
    CSSParserContext parserContext(parserMode(context));
    CSSParserImpl parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();
    range.consumeWhitespace();
    if (range.atEnd())
        return nullptr;
    RefPtr parsedValue = CSSPropertyParsing::consumeFontFeatureSettings(range);
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}


static std::optional<FontTag> consumeFontTag(CSSParserTokenRange& range)
{
    FontTag tag;

    auto token = range.peek();
    if (token.type() != StringToken)
        return std::nullopt;
    if (token.value().length() != tag.size())
        return std::nullopt;

    for (unsigned i = 0; i < tag.size(); ++i) {
        // Limits the range of characters to 0x20-0x7E, following the tag name rules defiend in the OpenType specification.
        auto character = token.value()[i];
        if (character < 0x20 || character > 0x7E)
            return std::nullopt;

        tag[i] = character;
    }

    range.consumeIncludingWhitespace();

    return { tag };
}

RefPtr<CSSValue> consumeFeatureTagValue(CSSParserTokenRange& range)
{
    // <feature-tag-value> = <string> [ <integer> | on | off ]?

    // FIXME: The specification states "The <string> is a case-sensitive OpenType feature tag."
    // so we probably should not be lowercasing it at parse time.
    auto tag = consumeFontTag(range);
    if (!tag)
        return nullptr;

    RefPtr<CSSPrimitiveValue> tagValue;
    if (!range.atEnd() && range.peek().type() != CommaToken) {
        // Feature tag values could follow: <integer> | on | off
        if (auto integer = consumeNonNegativeInteger(range))
            tagValue = WTFMove(integer);
        else if (range.peek().id() == CSSValueOn || range.peek().id() == CSSValueOff)
            tagValue = CSSPrimitiveValue::createInteger(range.consumeIncludingWhitespace().id() == CSSValueOn ? 1 : 0);
        else
            return nullptr;
    } else
        tagValue = CSSPrimitiveValue::createInteger(1);

    return CSSFontFeatureValue::create(WTFMove(*tag), tagValue.releaseNonNull());
}


#if ENABLE(VARIATION_FONTS)

RefPtr<CSSValue> consumeVariationTagValue(CSSParserTokenRange& range)
{
    // https://w3c.github.io/csswg-drafts/css-fonts/#font-variation-settings-def

    auto tag = consumeFontTag(range);
    if (!tag)
        return nullptr;

    auto tagValue = consumeNumberRaw(range);
    if (!tagValue)
        return nullptr;

    return CSSFontVariationValue::create(WTFMove(*tag), tagValue->value);
}

#endif

// MARK: @font-face 'font-stretch'

RefPtr<CSSValue> parseFontFaceStretch(const String& string, ScriptExecutionContext& context)
{
    CSSParserContext parserContext(parserMode(context));
    CSSParserImpl parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();
    range.consumeWhitespace();
    if (range.atEnd())
        return nullptr;
    RefPtr parsedValue =
#if ENABLE(VARIATION_FONTS)
        consumeFontStretchRange(range);
#else
        consumeFontStretch(range);
#endif
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

// MARK: @font-face 'font-weight'

RefPtr<CSSValue> parseFontFaceWeight(const String& string, ScriptExecutionContext& context)
{
    CSSParserContext parserContext(parserMode(context));
    CSSParserImpl parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();
    range.consumeWhitespace();
    if (range.atEnd())
        return nullptr;
    RefPtr parsedValue =
#if ENABLE(VARIATION_FONTS)
        consumeFontWeightAbsoluteRange(range);
#else
        consumeFontWeightAbsolute(range);
#endif
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

// MARK: - @font-palette-values

RefPtr<CSSValue> consumeFontPaletteValuesFontFamily(CSSParserTokenRange& range)
{
    return consumeCommaSeparatedListWithoutSingleValueOptimization(range, [] (auto& range) {
        return consumeFamilyName(range);
    });
}

RefPtr<CSSValue> consumeFontPaletteValuesOverrideColors(CSSParserTokenRange& range, const CSSParserContext& context)
{
    auto list = consumeCommaSeparatedListWithoutSingleValueOptimization(range, [](auto& range, auto& context) -> RefPtr<CSSValue> {
        auto key = consumeNonNegativeInteger(range);
        if (!key)
            return nullptr;
        auto color = consumeColor(range, context, { .allowedColorTypes = StyleColor::CSSColorType::Absolute });
        if (!color)
            return nullptr;
        return CSSFontPaletteValuesOverrideColorsValue::create(key.releaseNonNull(), color.releaseNonNull());
    }, context);
    if (!range.atEnd() || !list || !list->length())
        return nullptr;
    return list;
}

// MARK: - @font-feature-values

Vector<AtomString> consumeFontFeatureValuesFamilyNameList(CSSParserTokenRange& range)
{
    Vector<AtomString> result;
    do {
        auto name = consumeFamilyNameRaw(range);
        if (name.isNull())
            return { };

        result.append(name);
    } while (consumeCommaIncludingWhitespace(range));
    return result;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
