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
#include "CSSPropertyParserConsumer+LengthPercentage.h"
#include "CSSPropertyParserConsumer+LengthPercentageDefinitions.h"
#include "CSSPropertyParserConsumer+List.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+Number.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+Percentage.h"
#include "CSSPropertyParserConsumer+PercentageDefinitions.h"
#include "CSSPropertyParserConsumer+URL.h"
#include "CSSPropertyParserOptions.h"
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

template<typename Result, typename... Ts> static Result forwardVariantTo(std::variant<Ts...>&& variant)
{
    return WTF::switchOn(WTFMove(variant), [](auto&& alternative) -> Result { return { WTFMove(alternative) }; });
}

template<typename T> static Ref<CSSPrimitiveValue> resolveToCSSPrimitiveValue(CSS::PrimitiveNumeric<T>&& primitive)
{
    return WTF::switchOn(WTFMove(primitive.value), [](auto&& alternative) { return CSSPrimitiveValueResolverBase::resolve(WTFMove(alternative), { }, { }); }).releaseNonNull();
}

static CSSParserMode parserMode(ScriptExecutionContext& context)
{
    auto* document = dynamicDowncast<Document>(context);
    return (document && document->inQuirksMode()) ? HTMLQuirksMode : HTMLStandardMode;
}

// MARK: - 'font-stretch'
// FIXME: Rename to 'font-width' to match spec. 'font-stretch' is now a legacy alias.

static std::optional<UnresolvedFontStretch> consumeFontStretchUnresolved(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <'font-stretch'> = normal | <percentage [0,∞]> | ultra-condensed | extra-condensed | condensed | semi-condensed | semi-expanded | expanded | extra-expanded | ultra-expanded
    // https://drafts.csswg.org/css-fonts-4/#propdef-font-width

    // FIXME: Add support for consuming the percentage value.
    // FIXME: Add a way to export this "raw" version from the generated CSSPropertyParsing.

    return consumeIdentRaw<CSSValueUltraCondensed, CSSValueExtraCondensed, CSSValueCondensed, CSSValueSemiCondensed, CSSValueNormal, CSSValueSemiExpanded, CSSValueExpanded, CSSValueExtraExpanded, CSSValueUltraExpanded>(range);
}

// MARK: - 'font-weight'

#if !ENABLE(VARIATION_FONTS)
static bool isIntegerAndDivisibleBy100(double value)
{
    ASSERT(value >= 1);
    ASSERT(value <= 1000);
    return static_cast<int>(value / 100) * 100 == value;
}

static std::optional<UnresolvedFontWeightNumber::Raw> validateFontWeightNumber(UnresolvedFontWeightNumber::Raw number)
{
    if (!isIntegerAndDivisibleBy100(number.value))
        return std::nullopt;
    return number;
}
#endif

static std::optional<UnresolvedFontWeightNumber> consumeFontWeightNumberUnresolved(CSSParserTokenRange& range, const CSSParserContext& context)
{
    auto rangeCopy = range;

    auto options = CSSPropertyParserOptions { };
    auto number = MetaConsumer<UnresolvedFontWeightNumber>::consume(rangeCopy, context, { }, options);
    if (!number)
        return std::nullopt;

#if !ENABLE(VARIATION_FONTS)
    // Additional validation is needed for the legacy path.
    auto result = WTF::switchOn(WTFMove(number->value),
        [](UnresolvedFontWeightNumber::Raw&& number) -> std::optional<UnresolvedFontWeightNumber> {
            if (auto validated = validateFontWeightNumber(WTFMove(number)))
                return { { WTFMove(*validated) } };
            return std::nullopt;
        },
        [](UnresolvedFontWeightNumber::Calc&& calc) -> std::optional<UnresolvedFontWeightNumber> {
            return { { WTFMove(calc) } };
        }
    );
    if (!number)
        return std::nullopt;
#else
    auto result = WTFMove(number);
#endif

    range = rangeCopy;
    return result;
}

static std::optional<UnresolvedFontWeight> consumeFontWeightUnresolved(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'font-weight'> = normal | bold | bolder | lighter | <number [1,1000]>
    // https://drafts.csswg.org/css-fonts-4/#font-weight-prop

    if (auto keyword = consumeIdentRaw<CSSValueNormal, CSSValueBold, CSSValueBolder, CSSValueLighter>(range))
        return { *keyword };
    if (auto fontWeightNumber = consumeFontWeightNumberUnresolved(range, context))
        return { WTFMove(*fontWeightNumber) };
    return std::nullopt;
}

RefPtr<CSSValue> consumeFontWeight(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (auto keyword = consumeIdentRaw<CSSValueNormal, CSSValueBold, CSSValueBolder, CSSValueLighter>(range))
        return CSSPrimitiveValue::create(*keyword);
    if (auto fontWeightNumber = consumeFontWeightNumberUnresolved(range, context))
        return resolveToCSSPrimitiveValue(WTFMove(*fontWeightNumber));
    return nullptr;
}

// MARK: - 'font-style'

#if ENABLE(VARIATION_FONTS)

static std::optional<UnresolvedFontStyleObliqueAngle> consumeFontStyleAngleUnresolved(CSSParserTokenRange& range, const CSSParserContext& context)
{
    auto rangeCopy = range;

    auto options = CSSPropertyParserOptions { .parserMode = context.mode };
    auto angle = MetaConsumer<UnresolvedFontStyleObliqueAngle>::consume(rangeCopy, context, { }, options);
    if (!angle)
        return std::nullopt;

    range = rangeCopy;
    return angle;
}

#endif

static std::optional<UnresolvedFontStyle> consumeFontStyleUnresolved(CSSParserTokenRange& range, const CSSParserContext& context)
{
    UNUSED_PARAM(context);
    auto keyword = consumeIdentRaw<CSSValueNormal, CSSValueItalic, CSSValueOblique>(range);
    if (!keyword)
        return std::nullopt;

#if ENABLE(VARIATION_FONTS)
    if (*keyword == CSSValueOblique && !range.atEnd()) {
        if (auto angle = consumeFontStyleAngleUnresolved(range, context))
            return { { WTFMove(*angle) } };
    }
#endif

    return { { *keyword } };
}

RefPtr<CSSValue> consumeFontStyle(CSSParserTokenRange& range, const CSSParserContext& context)
{
    UNUSED_PARAM(context);
    auto keyword = consumeIdentRaw<CSSValueNormal, CSSValueItalic, CSSValueOblique>(range);
    if (!keyword)
        return nullptr;

#if ENABLE(VARIATION_FONTS)
    if (*keyword == CSSValueOblique && !range.atEnd()) {
        if (auto angle = consumeFontStyleAngleUnresolved(range, context))
            return CSSFontStyleWithAngleValue::create(resolveToCSSPrimitiveValue(WTFMove(*angle)));
    }
#endif

    return CSSPrimitiveValue::create(*keyword);
}

// MARK: - 'font-family'

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

static AtomString consumeFamilyNameUnresolved(CSSParserTokenRange& range)
{
    if (range.peek().type() == StringToken)
        return range.consumeIncludingWhitespace().value().toAtomString();
    if (range.peek().type() != IdentToken)
        return nullAtom();
    return concatenateFamilyName(range);
}

static std::optional<CSSValueID> consumeGenericFamilyUnresolved(CSSParserTokenRange& range)
{
    return consumeIdentRaw<CSSValueSerif, CSSValueSansSerif, CSSValueCursive, CSSValueFantasy, CSSValueMonospace, CSSValueWebkitBody, CSSValueWebkitPictograph, CSSValueSystemUi>(range);
}

static RefPtr<CSSValue> consumeGenericFamily(CSSParserTokenRange& range)
{
    if (auto familyName = consumeGenericFamilyUnresolved(range)) {
        // FIXME: Remove special case for system-ui.
        if (*familyName == CSSValueSystemUi)
            return CSSValuePool::singleton().createFontFamilyValue(nameString(*familyName));
        return CSSPrimitiveValue::create(*familyName);
    }
    return nullptr;
}

static std::optional<UnresolvedFontFamily> consumeFontFamilyUnresolved(CSSParserTokenRange& range)
{
    // <'font-family'> = [ <family-name> | <generic-family> ]#
    // https://drafts.csswg.org/css-fonts-4/#font-family-prop

    UnresolvedFontFamily list;
    do {
        if (auto ident = consumeGenericFamilyUnresolved(range))
            list.append({ *ident });
        else {
            auto familyName = consumeFamilyNameUnresolved(range);
            if (familyName.isNull())
                return std::nullopt;
            list.append({ familyName });
        }
    } while (consumeCommaIncludingWhitespace(range));
    return list;
}

static RefPtr<CSSValue> consumeFamilyName(CSSParserTokenRange& range)
{
    // // https://drafts.csswg.org/css-fonts-4/#family-name-syntax

    auto familyName = consumeFamilyNameUnresolved(range);
    if (familyName.isNull())
        return nullptr;
    return CSSValuePool::singleton().createFontFamilyValue(familyName);
}

RefPtr<CSSValue> consumeFontFamily(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <'font-family'> = [ <family-name> | <generic-family> ]#
    // https://drafts.csswg.org/css-fonts-4/#font-family-prop

    return consumeCommaSeparatedListWithoutSingleValueOptimization(range, [] (auto& range) -> RefPtr<CSSValue> {
        if (auto parsedValue = consumeGenericFamily(range))
            return parsedValue;
        return consumeFamilyName(range);
    });
}

// MARK: - 'font-size'

static std::optional<UnresolvedFontSize> consumeFontSizeUnresolved(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'font-size> = <absolute-size> | <relative-size> | <length-percentage [0,∞]> | math
    // https://drafts.csswg.org/css-fonts-4/#propdef-font-size

    // Additionally supports non-standard productions: <-webkit-absolute-size> | <-webkit-relative-size>

    // FIXME: Add support for 'math' keyword.
    // FIXME: Add a way to export this "raw" version from the generated CSSPropertyParsing.

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

    auto rangeCopy = range;

    auto options = CSSPropertyParserOptions { .parserMode = context.mode };
    auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<CSS::Nonnegative>>::consume(rangeCopy, context, { }, options);
    if (!lengthPercentage)
        return std::nullopt;

    range = rangeCopy;
    return { WTFMove(*lengthPercentage) };
}

// MARK: - 'line-height'

static std::optional<UnresolvedFontLineHeight> consumeLineHeightUnresolved(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'line-height'> = normal | <number [0,∞]> | <length-percentage [0,∞]>
    // https://www.w3.org/TR/css-inline-3/#line-height-property

    if (range.peek().id() == CSSValueNormal) {
        if (auto ident = consumeIdentRaw(range))
            return { { *ident } };
        return std::nullopt;
    }

    auto rangeCopy = range;

    auto options = CSSPropertyParserOptions { .parserMode = context.mode };
    auto lineHeight = MetaConsumer<CSS::Number<CSS::Nonnegative>, CSS::LengthPercentage<CSS::Nonnegative>>::consume(rangeCopy, context, { }, options);
    if (!lineHeight)
        return std::nullopt;

    range = rangeCopy;
    return forwardVariantTo<UnresolvedFontLineHeight>(WTFMove(*lineHeight));
}

// MARK: 'font' (shorthand)

static std::optional<UnresolvedFont> consumeUnresolvedFont(CSSParserTokenRange& range, const CSSParserContext& context)
{
    std::optional<UnresolvedFontStyle> fontStyle;
    std::optional<UnresolvedFontVariantCaps> fontVariantCaps;
    std::optional<UnresolvedFontWeight> fontWeight;
    std::optional<UnresolvedFontStretch> fontStretch;
    std::optional<UnresolvedFontSize> fontSize;
    std::optional<UnresolvedFontLineHeight> fontLineHeight;
    std::optional<UnresolvedFontFamily> fontFamily;

    // Optional font-style, font-variant, font-weight and font-stretch in any order.
    for (unsigned i = 0; i < 4 && !range.atEnd(); ++i) {
        if (consumeIdentRaw<CSSValueNormal>(range))
            continue;
        if (!fontStyle && (fontStyle = consumeFontStyleUnresolved(range, context)))
            continue;
        if (!fontVariantCaps && (fontVariantCaps = consumeIdentRaw<CSSValueSmallCaps>(range)))
            continue;
        if (!fontWeight && (fontWeight = consumeFontWeightUnresolved(range, context)))
            continue;
        if (!fontStretch && (fontStretch = consumeFontStretchUnresolved(range, context)))
            continue;
        break;
    }

    if (range.atEnd())
        return std::nullopt;

    // Required 'font-size'.
    fontSize = consumeFontSizeUnresolved(range, context);
    if (!fontSize)
        return std::nullopt;

    if (range.atEnd())
        return std::nullopt;

    if (consumeSlashIncludingWhitespace(range)) {
        // If a slash is consumed, a 'line-height' is required.
        fontLineHeight = consumeLineHeightUnresolved(range, context);
        if (!fontLineHeight)
            return std::nullopt;
    }

    // Required 'font-family'.
    fontFamily = consumeFontFamilyUnresolved(range);
    if (!fontFamily)
        return std::nullopt;

    if (!range.atEnd())
        return std::nullopt;

    return UnresolvedFont {
        .style = fontStyle.value_or(CSSValueNormal),
        .variantCaps = fontVariantCaps.value_or(CSSValueNormal),
        .weight = fontWeight.value_or(CSSValueNormal),
        .stretch = fontStretch.value_or(CSSValueNormal),
        .size = WTFMove(*fontSize),
        .lineHeight = fontLineHeight.value_or(CSSValueNormal),
        .family = WTFMove(*fontFamily),
    };
}

std::optional<UnresolvedFont> parseUnresolvedFont(const String& string, const CSSParserContext& context)
{
    CSSTokenizer tokenizer(string);
    CSSParserTokenRange range(tokenizer.tokenRange());
    range.consumeWhitespace();

    return consumeUnresolvedFont(range, context);
}

// MARK: 'font-variant-ligatures'

RefPtr<CSSValue> consumeFontVariantLigatures(CSSParserTokenRange& range, const CSSParserContext&)
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

RefPtr<CSSValue> consumeFontVariantEastAsian(CSSParserTokenRange& range, const CSSParserContext&)
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

RefPtr<CSSValue> consumeFontVariantAlternates(CSSParserTokenRange& range, const CSSParserContext&)
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

RefPtr<CSSValue> consumeFontVariantNumeric(CSSParserTokenRange& range, const CSSParserContext&)
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

RefPtr<CSSValue> consumeFontSizeAdjust(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueNone || range.peek().id() == CSSValueFromFont)
        return consumeIdent(range);

    auto metric = consumeIdent<CSSValueExHeight, CSSValueCapHeight, CSSValueChWidth, CSSValueIcWidth, CSSValueIcHeight>(range);
    auto value = consumeNumber(range, context, ValueRange::NonNegative);
    if (!value)
        value = consumeIdent<CSSValueFromFont>(range);

    if (!value || !metric || metric->valueID() == CSSValueExHeight)
        return value;

    return CSSValuePair::create(metric.releaseNonNull(), value.releaseNonNull());
}

// MARK: - @-rule descriptor consumers:

// MARK: - @font-face

// MARK: @font-face 'font-family'
RefPtr<CSSValue> consumeFontFaceFontFamily(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <'font-family'> = <family-name>
    // https://drafts.csswg.org/css-fonts-4/#descdef-font-face-font-family

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

Vector<FontTechnology> consumeFontTech(CSSParserTokenRange& range, const CSSParserContext&, bool singleValue)
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

String consumeFontFormat(CSSParserTokenRange& range, const CSSParserContext&, bool rejectStringValues)
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
        format = consumeFontFormat(range, context);
        if (format.isNull())
            return nullptr;
    }
    if (range.peek().functionId() == CSSValueTech) {
        technologies = consumeFontTech(range, context);
        if (technologies.isEmpty())
            return nullptr;
    }
    if (!range.atEnd())
        return nullptr;

    return CSSFontFaceSrcResourceValue::create(WTFMove(location), WTFMove(format), WTFMove(technologies), context.isContentOpaque ? LoadedFromOpaqueSource::Yes : LoadedFromOpaqueSource::No);
}

static RefPtr<CSSValue> consumeFontFaceSrcLocal(CSSParserTokenRange& range, const CSSParserContext&)
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
            return consumeFontFaceSrcLocal(range, context);
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
    // <'size-adjust'> = <percentage [0,∞]>
    // https://www.w3.org/TR/css-fonts-5/#descdef-font-face-size-adjust

    CSSParserContext parserContext(parserMode(context));
    CSSParserImpl parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();

    range.consumeWhitespace();

    if (range.atEnd())
        return nullptr;

    RefPtr parsedValue = consumePercentage(range, parserContext, ValueRange::NonNegative);
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

RefPtr<CSSValueList> consumeFontFaceUnicodeRange(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <'unicode-range'> = <urange>#
    // https://drafts.csswg.org/css-fonts/#descdef-font-face-unicode-range

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
    // <'unicode-range'> = <urange>#
    // https://drafts.csswg.org/css-fonts/#descdef-font-face-unicode-range

    CSSParserContext parserContext(parserMode(context));
    CSSParserImpl parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();

    range.consumeWhitespace();

    if (range.atEnd())
        return nullptr;

    RefPtr parsedValue = consumeFontFaceUnicodeRange(range, parserContext);
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

// MARK: @font-face 'font-display'

RefPtr<CSSValue> parseFontFaceDisplay(const String& string, ScriptExecutionContext& context)
{
    // <'font-display'> = auto | block | swap | fallback | optional
    // https://drafts.csswg.org/css-fonts/#descdef-font-face-font-display

    CSSParserContext parserContext(parserMode(context));
    CSSParserImpl parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();

    range.consumeWhitespace();

    if (range.atEnd())
        return nullptr;

    RefPtr parsedValue = CSSPropertyParsing::consumeFontFaceFontDisplay(range);
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

// MARK: @font-face 'font-style'

RefPtr<CSSValue> parseFontFaceFontStyle(const String& string, ScriptExecutionContext& context)
{
    // <'font-style'> = auto | normal | italic | oblique [ <angle [-90deg,90deg]>{1,2} ]?
    // https://drafts.csswg.org/css-fonts/#descdef-font-face-font-style

    // FIXME: Missing support for "auto" identifier.

    CSSParserContext parserContext(parserMode(context));
    CSSParserImpl parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();

    range.consumeWhitespace();

    if (range.atEnd())
        return nullptr;

    RefPtr parsedValue = consumeFontFaceFontStyle(range, parserContext);
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

#if ENABLE(VARIATION_FONTS)

RefPtr<CSSValue> consumeFontFaceFontStyle(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'font-style'> auto | normal | italic | oblique [ <angle [-90deg,90deg]>{1,2} ]?
    // https://drafts.csswg.org/css-fonts-4/#descdef-font-face-font-style

    // FIXME: Missing support for "auto" identifier.

    auto keyword = consumeIdentRaw<CSSValueNormal, CSSValueItalic, CSSValueOblique>(range);
    if (!keyword)
        return nullptr;

    if (*keyword != CSSValueOblique || range.atEnd())
        return CSSFontStyleRangeValue::create(CSSPrimitiveValue::create(*keyword));

    auto rangeAfterAngles = range;

    auto firstAngle = consumeFontStyleAngleUnresolved(rangeAfterAngles, context);
    if (!firstAngle)
        return nullptr;

    if (rangeAfterAngles.atEnd()) {
        range = rangeAfterAngles;
        return CSSFontStyleRangeValue::create(
            CSSPrimitiveValue::create(*keyword),
            CSSValueList::createSpaceSeparated(
                resolveToCSSPrimitiveValue(WTFMove(*firstAngle))
            )
        );
    }

    auto secondAngle = consumeFontStyleAngleUnresolved(rangeAfterAngles, context);
    if (!secondAngle)
        return nullptr;

    range = rangeAfterAngles;
    return CSSFontStyleRangeValue::create(
        CSSPrimitiveValue::create(*keyword),
        CSSValueList::createSpaceSeparated(
            resolveToCSSPrimitiveValue(WTFMove(*firstAngle)),
            resolveToCSSPrimitiveValue(WTFMove(*secondAngle))
        )
    );
}

#else

RefPtr<CSSValue> consumeFontFaceFontStyle(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'font-style'> = <font-style>
    // NOTE: This is the pre-variation fonts definition.

    return consumeFontStyle(range, context);
}

#endif

// MARK: 'opentype-tag'

static std::optional<FontTag> consumeFontOpenTypeTag(CSSParserTokenRange& range)
{
    // <opentype-tag> = <string>
    // https://drafts.csswg.org/css-fonts/#typedef-opentype-tag

    FontTag tag;

    auto token = range.peek();
    if (token.type() != StringToken)
        return std::nullopt;
    if (token.value().length() != tag.size())
        return std::nullopt;

    for (unsigned i = 0; i < tag.size(); ++i) {
        // Limits the range of characters to 0x20-0x7E, following the tag name rules defined in the OpenType specification.
        auto character = token.value()[i];
        if (character < 0x20 || character > 0x7E)
            return std::nullopt;

        tag[i] = character;
    }

    range.consumeIncludingWhitespace();

    return { tag };
}

// MARK: @font-face 'feature-tag-value'

RefPtr<CSSValue> consumeFeatureTagValue(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <feature-tag-value> = <opentype-tag> [ <integer [0,∞]> | on | off ]?
    // https://drafts.csswg.org/css-fonts/#feature-tag-value

    auto tag = consumeFontOpenTypeTag(range);
    if (!tag)
        return nullptr;

    RefPtr<CSSPrimitiveValue> tagValue;
    if (!range.atEnd() && range.peek().type() != CommaToken) {
        // Feature tag values could follow: <integer [0,∞]> | on | off
        if (auto integer = consumeNonNegativeInteger(range, context))
            tagValue = WTFMove(integer);
        else if (range.peek().id() == CSSValueOn || range.peek().id() == CSSValueOff)
            tagValue = CSSPrimitiveValue::createInteger(range.consumeIncludingWhitespace().id() == CSSValueOn ? 1 : 0);
        else
            return nullptr;
    } else
        tagValue = CSSPrimitiveValue::createInteger(1);

    return CSSFontFeatureValue::create(WTFMove(*tag), tagValue.releaseNonNull());
}

// MARK: @font-face 'font-feature-settings'

RefPtr<CSSValue> parseFontFaceFeatureSettings(const String& string, ScriptExecutionContext& context)
{
    // <'font-feature-settings'> = normal | <feature-tag-value>#
    // https://drafts.csswg.org/css-fonts/#descdef-font-face-font-feature-settings

    CSSParserContext parserContext(parserMode(context));
    CSSParserImpl parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();

    range.consumeWhitespace();

    if (range.atEnd())
        return nullptr;

    RefPtr parsedValue = CSSPropertyParsing::consumeFontFeatureSettings(range, parserContext);
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

// MARK: @font-face 'font-variation-settings'

#if ENABLE(VARIATION_FONTS)

RefPtr<CSSValue> consumeVariationTagValue(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <variation-tag-value> = <opentype-tag> <number>
    // https://drafts.csswg.org/css-fonts/#font-variation-settings-def

    auto tag = consumeFontOpenTypeTag(range);
    if (!tag)
        return nullptr;

    auto tagValue = consumeNumber(range, context);
    if (!tagValue)
        return nullptr;

    return CSSFontVariationValue::create(WTFMove(*tag), tagValue.releaseNonNull());
}

#endif

// MARK: @font-face 'font-stretch'
// FIXME: Rename to 'font-width' to match spec. 'font-stretch' is now a legacy alias.

RefPtr<CSSValue> parseFontFaceFontStretch(const String& string, ScriptExecutionContext& context)
{
    // <font-stretch> = auto | <'font-stretch'>{1,2}
    // https://drafts.csswg.org/css-fonts-4/#descdef-font-face-font-width

    CSSParserContext parserContext(parserMode(context));
    CSSParserImpl parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();

    range.consumeWhitespace();

    if (range.atEnd())
        return nullptr;

    RefPtr parsedValue = consumeFontFaceFontStretch(range, parserContext);
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

#if ENABLE(VARIATION_FONTS)

RefPtr<CSSValue> consumeFontFaceFontStretch(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <font-stretch> = auto | <'font-stretch'>{1,2}
    // https://drafts.csswg.org/css-fonts-4/#descdef-font-face-font-width

    // FIXME: Missing support for "auto" identifier.
    // FIXME: Both stretch values should allow keyword identifiers, not just the first.

    if (RefPtr result = CSSPropertyParsing::consumeFontStretchAbsolute(range))
        return result;

    RefPtr firstPercent = consumePercentage(range, context, ValueRange::NonNegative);
    if (!firstPercent)
        return nullptr;

    if (range.atEnd())
        return firstPercent;

    RefPtr secondPercent = consumePercentage(range, context, ValueRange::NonNegative);
    if (!secondPercent)
        return nullptr;

    return CSSValueList::createSpaceSeparated(
        firstPercent.releaseNonNull(),
        secondPercent.releaseNonNull()
    );
}

#else

RefPtr<CSSValue> consumeFontFaceFontStretch(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <font-stretch> = auto | <'font-stretch'>
    // NOTE: This is the pre-variation fonts definition.

    if (RefPtr result = CSSPropertyParsing::consumeFontStretchAbsolute(range))
        return result;
    if (RefPtr percent = consumePercentage(range, context, ValueRange::NonNegative))
        return percent;
    return nullptr;
}

#endif

// MARK: @font-face 'font-weight'

RefPtr<CSSValue> parseFontFaceFontWeight(const String& string, ScriptExecutionContext& context)
{
    // <'font-weight'> = auto | <font-weight-absolute>{1,2}
    // https://drafts.csswg.org/css-fonts-4/#descdef-font-face-font-weight

    CSSParserContext parserContext(parserMode(context));
    CSSParserImpl parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();

    range.consumeWhitespace();

    if (range.atEnd())
        return nullptr;

    RefPtr parsedValue = consumeFontFaceFontWeight(range, parserContext);
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

#if ENABLE(VARIATION_FONTS)

RefPtr<CSSValue> consumeFontFaceFontWeight(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'font-weight'> = auto | <font-weight-absolute>{1,2}
    // https://drafts.csswg.org/css-fonts-4/#descdef-font-face-font-weight

    // FIXME: Missing support for "auto" identifier.
    // FIXME: Both weight values should allow "normal" and "bold" identifiers, not just the first.

    if (auto keyword = consumeIdentRaw<CSSValueNormal, CSSValueBold>(range))
        return CSSPrimitiveValue::create(*keyword);

    auto firstNumber = consumeFontWeightNumberUnresolved(range, context);
    if (!firstNumber)
        return nullptr;

    if (range.atEnd())
        return resolveToCSSPrimitiveValue(WTFMove(*firstNumber));

    auto secondNumber = consumeFontWeightNumberUnresolved(range, context);
    if (!secondNumber)
        return nullptr;

    return CSSValueList::createSpaceSeparated(
        resolveToCSSPrimitiveValue(WTFMove(*firstNumber)),
        resolveToCSSPrimitiveValue(WTFMove(*secondNumber))
    );
}

#else

RefPtr<CSSValue> consumeFontFaceFontWeight(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'font-weight'> = <font-weight-absolute>
    // NOTE: This is the pre-variation fonts definition.

    if (auto keyword = consumeIdentRaw<CSSValueNormal, CSSValueBold>(range))
        return CSSPrimitiveValue::create(*keyword);
    if (auto fontWeightNumber = consumeFontWeightNumberUnresolved(range, context))
        return resolveToCSSPrimitiveValue(WTFMove(*fontWeightNumber));
    return nullptr;
}

#endif

// MARK: - @font-palette-values

// MARK: @font-palette-values 'font-family'

RefPtr<CSSValue> consumeFontPaletteValuesFontFamily(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <'font-family'> = <family-name>#
    // https://drafts.csswg.org/css-fonts/#descdef-font-palette-values-font-family

    return consumeCommaSeparatedListWithoutSingleValueOptimization(range, [](auto& range) {
        return consumeFamilyName(range);
    });
}

// MARK: @font-palette-values 'override-colors'

RefPtr<CSSValue> consumeFontPaletteValuesOverrideColors(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'override-colors'> = [ <integer [0,∞]> <color> ]#
    // https://drafts.csswg.org/css-fonts/#descdef-font-palette-values-override-colors

    auto list = consumeCommaSeparatedListWithoutSingleValueOptimization(range, [](auto& range, auto& context) -> RefPtr<CSSValue> {
        auto key = consumeNonNegativeInteger(range, context);
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

// MARK: @font-feature-values 'prelude family name list'

Vector<AtomString> consumeFontFeatureValuesPreludeFamilyNameList(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <prelude-family-name-list> = <family-name>#
    // https://drafts.csswg.org/css-fonts/#font-feature-values-syntax

    Vector<AtomString> result;
    do {
        auto name = consumeFamilyNameUnresolved(range);
        if (name.isNull())
            return { };

        result.append(name);
    } while (consumeCommaIncludingWhitespace(range));
    return result;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
