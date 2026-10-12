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
#include "CSSColorInterpolationMethod.h"
#include "CSSFontFaceSrcValue.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontPaletteMix.h"
#include "CSSFontPaletteValue.h"
#include "CSSFontStyleWithAngleValue.h"
#include "CSSFontVariantLigaturesParser.h"
#include "CSSFontVariantNumericParser.h"
#include "CSSParser.h"
#include "CSSParserIdioms.h"
#include "CSSParserToken.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+AngleDefinitions.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+Color.h"
#include "CSSPropertyParserConsumer+ColorInterpolationMethod.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+IntegerDefinitions.h"
#include "CSSPropertyParserConsumer+KeywordDefinitions.h"
#include "CSSPropertyParserConsumer+LengthPercentageDefinitions.h"
#include "CSSPropertyParserConsumer+List.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+PercentageDefinitions.h"
#include "CSSPropertyParserConsumer+URL.h"
#include "CSSPropertyParserOptions.h"
#include "CSSPropertyParserState.h"
#include "CSSPropertyParsing.h"
#include "CSSUnicodeRangeValue.h"
#include "CSSValuePair.h"
#include "Document.h"
#include "FontCustomPlatformData.h"
#include "FontFace.h"
#include "StyleKeyword+Mappings.h"
#include "WebKitFontFamilyNames.h"
#include <wtf/Function.h>
#include <wtf/text/ParsingUtilities.h>

#if ENABLE(VARIATION_FONTS)
#include "CSSFontStyleRangeValue.h"
#include "CSSFontVariationValue.h"
#endif

namespace WebCore {
namespace CSSPropertyParserHelpers {

static CSSParserMode NODELETE parserMode(ScriptExecutionContext& context)
{
    auto* document = dynamicDowncast<Document>(context);
    return (document && document->inQuirksMode()) ? HTMLQuirksMode : HTMLStandardMode;
}

// MARK: - 'font-width'

static std::optional<UnresolvedFontWidth> consumeFontWidthUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState&)
{
    // <'font-width'> = normal | <percentage [0,∞]> | ultra-condensed | extra-condensed | condensed | semi-condensed | semi-expanded | expanded | extra-expanded | ultra-expanded
    // https://drafts.csswg.org/css-fonts-4/#propdef-font-width

    // FIXME: Add support for consuming the percentage value.
    // FIXME: Add a way to export this "raw" version from the generated CSSPropertyParsing.

    return consumeIdentRaw<CSSValueID::UltraCondensed, CSSValueID::ExtraCondensed, CSSValueID::Condensed, CSSValueID::SemiCondensed, CSSValueID::Normal, CSSValueID::SemiExpanded, CSSValueID::Expanded, CSSValueID::ExtraExpanded, CSSValueID::UltraExpanded>(range);
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

static std::optional<UnresolvedFontWeightNumber> consumeFontWeightNumberUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    auto rangeCopy = range;
    auto number = MetaConsumer<UnresolvedFontWeightNumber>::consume(rangeCopy, state);
    if (!number)
        return std::nullopt;

#if !ENABLE(VARIATION_FONTS)
    // Additional validation is needed for the legacy path.
    auto result = WTF::switchOn(WTF::move(*number),
        [](UnresolvedFontWeightNumber::Raw&& number) -> std::optional<UnresolvedFontWeightNumber> {
            if (auto validated = validateFontWeightNumber(WTF::move(number)))
                return { { WTF::move(*validated) } };
            return std::nullopt;
        },
        [](UnresolvedFontWeightNumber::Calc&& calc) -> std::optional<UnresolvedFontWeightNumber> {
            return { { WTF::move(calc) } };
        }
    );
    if (!number)
        return std::nullopt;
#else
    auto result = WTF::move(number);
#endif

    range = rangeCopy;
    return result;
}

static std::optional<UnresolvedFontWeight> consumeFontWeightUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'font-weight'> = normal | bold | bolder | lighter | <number [1,1000]>
    // https://drafts.csswg.org/css-fonts-4/#font-weight-prop

    if (auto keyword = consumeIdentRaw<CSSValueID::Normal, CSSValueID::Bold, CSSValueID::Bolder, CSSValueID::Lighter>(range))
        return { *keyword };
    if (auto fontWeightNumber = consumeFontWeightNumberUnresolved(range, state))
        return { WTF::move(*fontWeightNumber) };
    return std::nullopt;
}

// MARK: - 'font-style'

#if ENABLE(VARIATION_FONTS)

static std::optional<UnresolvedFontStyleObliqueAngle> consumeFontStyleAngleUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    auto rangeCopy = range;

    auto angle = MetaConsumer<UnresolvedFontStyleObliqueAngle>::consume(rangeCopy, state);
    if (!angle)
        return std::nullopt;

    range = rangeCopy;
    return angle;
}

#endif

static std::optional<UnresolvedFontStyle> consumeFontStyleUnresolved(CSSParserTokenRange& range, [[maybe_unused]] CSS::PropertyParserState& state)
{
    auto keyword = consumeIdentRaw<CSSValueID::Normal, CSSValueID::Italic, CSSValueID::Oblique>(range);
    if (!keyword)
        return std::nullopt;

#if ENABLE(VARIATION_FONTS)
    if (*keyword == CSSValueID::Oblique && !range.atEnd()) {
        if (auto angle = consumeFontStyleAngleUnresolved(range, state))
            return { { WTF::move(*angle) } };
    }
#endif

    return { { *keyword } };
}

RefPtr<CSSValue> consumeFontStyle(CSSParserTokenRange& range, [[maybe_unused]] CSS::PropertyParserState& state)
{
    auto keyword = consumeIdentRaw<CSSValueID::Normal, CSSValueID::Italic, CSSValueID::Oblique>(range);
    if (!keyword)
        return nullptr;

#if ENABLE(VARIATION_FONTS)
    if (*keyword == CSSValueID::Oblique && !range.atEnd()) {
        if (auto angle = consumeFontStyleAngleUnresolved(range, state))
            return CSSFontStyleWithAngleValue::create(WTF::move(*angle));
    }
#endif

    return CSSKeywordValue::create(*keyword);
}

// MARK: - 'font-family'

const AtomString& genericFontFamily(CSSValueID ident)
{
    switch (ident) {
    case CSSValueID::Serif:
        return WebKitFontFamilyNames::serifFamily.get();
    case CSSValueID::SansSerif:
        return WebKitFontFamilyNames::sansSerifFamily.get();
    case CSSValueID::Cursive:
        return WebKitFontFamilyNames::cursiveFamily.get();
    case CSSValueID::Fantasy:
        return WebKitFontFamilyNames::fantasyFamily.get();
    case CSSValueID::Monospace:
        return WebKitFontFamilyNames::monospaceFamily.get();
    case CSSValueID::WebkitPictograph:
        return WebKitFontFamilyNames::pictographFamily.get();
    case CSSValueID::SystemUi:
        return WebKitFontFamilyNames::systemUiFamily.get();
    case CSSValueID::Math:
        return WebKitFontFamilyNames::mathFamily.get();
    default:
        return nullAtom();
    }
}

WebKitFontFamilyNames::FamilyNamesIndex genericFontFamilyIndex(CSSValueID ident)
{
    switch (ident) {
    case CSSValueID::Serif:
        return WebKitFontFamilyNames::FamilyNamesIndex::SerifFamily;
    case CSSValueID::SansSerif:
        return WebKitFontFamilyNames::FamilyNamesIndex::SansSerifFamily;
    case CSSValueID::Cursive:
        return WebKitFontFamilyNames::FamilyNamesIndex::CursiveFamily;
    case CSSValueID::Fantasy:
        return WebKitFontFamilyNames::FamilyNamesIndex::FantasyFamily;
    case CSSValueID::Monospace:
        return WebKitFontFamilyNames::FamilyNamesIndex::MonospaceFamily;
    case CSSValueID::WebkitPictograph:
        return WebKitFontFamilyNames::FamilyNamesIndex::PictographFamily;
    case CSSValueID::SystemUi:
        return WebKitFontFamilyNames::FamilyNamesIndex::SystemUiFamily;
    case CSSValueID::Math:
        return WebKitFontFamilyNames::FamilyNamesIndex::MathFamily;
    default:
        ASSERT_NOT_REACHED();
        return WebKitFontFamilyNames::FamilyNamesIndex::StandardFamily;
    }
}

static AtomString concatenateFamilyName(CSSParserTokenRange& range, bool allowNumericTokens = false)
{
    StringBuilder builder;
    bool addedSpace = false;
    const CSSParserToken& firstToken = range.peek();
    // In legacyFontFaceAttributeMode (the <font face> attribute), a numeric token is allowed as
    // part of a literal family name, e.g. "Bodoni 72" (its original text is preserved in value()).
    while (range.peek().type() == IdentToken || (allowNumericTokens && range.peek().type() == NumberToken)) {
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

static AtomString consumeFamilyNameUnresolved(CSSParserTokenRange& range, bool allowNumericTokens = false)
{
    if (range.peek().type() == StringToken)
        return range.consumeIncludingWhitespace().value().toAtomString();
    if (range.peek().type() != IdentToken)
        return nullAtom();
    return concatenateFamilyName(range, allowNumericTokens);
}

static std::optional<CSSValueID> consumeGenericFamilyUnresolved(CSSParserTokenRange& range)
{
    return consumeIdentRaw<CSSValueID::Serif, CSSValueID::SansSerif, CSSValueID::Cursive, CSSValueID::Fantasy, CSSValueID::Monospace, CSSValueID::WebkitBody, CSSValueID::WebkitPictograph, CSSValueID::SystemUi, CSSValueID::Math>(range);
}

static RefPtr<CSSValue> consumeGenericFamily(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (auto familyName = consumeGenericFamilyUnresolved(range)) {
        // FIXME: Remove special case for system-ui.
        if (*familyName == CSSValueID::SystemUi)
            return state.pool.createFontFamilyNameValue(nameLiteral(*familyName));
        return CSSKeywordValue::create(*familyName);
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

RefPtr<CSSValue> consumeFamilyName(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // https://drafts.csswg.org/css-fonts-4/#family-name-syntax

    auto familyName = consumeFamilyNameUnresolved(range, state.context.legacyFontFaceAttributeMode);
    if (familyName.isNull())
        return nullptr;
    return state.pool.createFontFamilyNameValue(familyName);
}

RefPtr<CSSValue> consumeFontFamily(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'font-family'> = [ <family-name> | <generic-family> ]#
    // https://drafts.csswg.org/css-fonts-4/#font-family-prop

    return consumeListSeparatedBy<',', OneOrMore>(range, [&](auto& range) -> RefPtr<CSSValue> {
        if (auto parsedValue = consumeGenericFamily(range, state))
            return parsedValue;
        return consumeFamilyName(range, state);
    });
}

// MARK: - 'font-size'

static std::optional<UnresolvedFontSize> consumeFontSizeUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'font-size> = <absolute-size> | <relative-size> | <length-percentage [0,∞]> | math
    // https://drafts.csswg.org/css-fonts-4/#propdef-font-size

    // Additionally supports non-standard productions: <-webkit-absolute-size> | <-webkit-relative-size>

    // FIXME: Add a way to export this "raw" version from the generated CSSPropertyParsing.

    // -webkit-xxx-large is a parse-time alias.
    if (range.peek().id() == CSSValueID::WebkitXxxLarge) {
        if (auto ident = consumeIdentRaw(range); ident && ident == CSSValueID::WebkitXxxLarge)
            return { CSSValueID::XxxLarge };
        return std::nullopt;
    }

    if ((range.peek().id() >= CSSValueID::XxSmall && range.peek().id() <= CSSValueID::Larger) || range.peek().id() == CSSValueID::Math) {
        if (auto ident = consumeIdentRaw(range))
            return { *ident };
        return std::nullopt;
    }

    auto rangeCopy = range;

    auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<CSS::Nonnegative>>::consume(rangeCopy, state);
    if (!lengthPercentage)
        return std::nullopt;

    range = rangeCopy;
    return { WTF::move(*lengthPercentage) };
}

// MARK: - 'line-height'

static std::optional<UnresolvedFontLineHeight> consumeLineHeightUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'line-height'> = normal | <number [0,∞]> | <length-percentage [0,∞]>
    // https://www.w3.org/TR/css-inline-3/#line-height-property

    using Consumer = MetaConsumer<
        CSS::Keyword::Normal,
        CSS::Number<CSS::Nonnegative>,
        CSS::LengthPercentage<CSS::Nonnegative>
    >;

    return Consumer::consume(range, state,
        [&](CSS::Keyword::Normal) { return UnresolvedFontLineHeight { CSSValueID::Normal }; },
        [&](auto&& value) { return UnresolvedFontLineHeight { WTF::move(value) }; }
    );
}

// MARK: 'font' (shorthand)

static std::optional<UnresolvedFont> consumeUnresolvedFont(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    std::optional<UnresolvedFontStyle> fontStyle;
    std::optional<UnresolvedFontVariantCaps> fontVariantCaps;
    std::optional<UnresolvedFontWeight> fontWeight;
    std::optional<UnresolvedFontWidth> fontWidth;
    std::optional<UnresolvedFontSize> fontSize;
    std::optional<UnresolvedFontLineHeight> fontLineHeight;
    std::optional<UnresolvedFontFamily> fontFamily;

    // Optional font-style, font-variant, font-weight and font-width in any order.
    for (unsigned i = 0; i < 4 && !range.atEnd(); ++i) {
        if (consumeIdentRaw<CSSValueID::Normal>(range))
            continue;
        if (!fontStyle && (fontStyle = consumeFontStyleUnresolved(range, state)))
            continue;
        if (!fontVariantCaps && (fontVariantCaps = consumeIdentRaw<CSSValueID::SmallCaps>(range)))
            continue;
        if (!fontWeight && (fontWeight = consumeFontWeightUnresolved(range, state)))
            continue;
        if (!fontWidth && (fontWidth = consumeFontWidthUnresolved(range, state)))
            continue;
        break;
    }

    if (range.atEnd())
        return std::nullopt;

    // Required 'font-size'.
    fontSize = consumeFontSizeUnresolved(range, state);
    if (!fontSize)
        return std::nullopt;

    if (range.atEnd())
        return std::nullopt;

    if (consumeSlashIncludingWhitespace(range)) {
        // If a slash is consumed, a 'line-height' is required.
        fontLineHeight = consumeLineHeightUnresolved(range, state);
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
        .style = fontStyle.value_or(CSSValueID::Normal),
        .variantCaps = fontVariantCaps.value_or(CSSValueID::Normal),
        .weight = fontWeight.value_or(CSSValueID::Normal),
        .width = fontWidth.value_or(CSSValueID::Normal),
        .size = WTF::move(*fontSize),
        .lineHeight = fontLineHeight.value_or(CSSValueID::Normal),
        .family = WTF::move(*fontFamily),
    };
}

std::optional<UnresolvedFont> parseUnresolvedFont(StringView string, ScriptExecutionContext& context, std::optional<CSSParserMode> parserModeOverride)
{
    auto parserContext = CSSParserContext(parserModeOverride ? *parserModeOverride : parserMode(context));
    auto tokenizer = CSSTokenizer(string);
    auto range = tokenizer.tokenRange();

    range.consumeWhitespace();

    auto state = CSS::PropertyParserState { .context = parserContext, .pool = context.cssValuePool() };
    return consumeUnresolvedFont(range, state);
}

// MARK: 'font-size-adjust'

RefPtr<CSSValue> consumeFontSizeAdjust(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (range.peek().id() == CSSValueID::None || range.peek().id() == CSSValueID::FromFont)
        return consumeIdent(range);

    auto metric = consumeIdent<CSSValueID::ExHeight, CSSValueID::CapHeight, CSSValueID::ChWidth, CSSValueID::IcWidth, CSSValueID::IcHeight>(range);
    RefPtr<CSSValue> value = CSSPrimitiveValueResolver<CSS::Number<CSS::Nonnegative>>::consumeAndResolve(range, state);
    if (!value)
        value = consumeIdent<CSSValueID::FromFont>(range);

    if (!value || !metric || metric->valueID() == CSSValueID::ExHeight)
        return value;

    return CSSValuePair::create(metric.releaseNonNull(), value.releaseNonNull());
}

// MARK: 'font-palette'

static std::optional<CSS::FontPaletteMixParameters::Component> consumeFontPaletteMixComponent(CSSParserTokenRange& args, CSS::PropertyParserState& state)
{
    // <font-palette-mix-component> = <'font-palette'> && <percentage [0,100]>?
    // https://drafts.csswg.org/css-fonts-4/#funcdef-palette-mix (subset)

    auto percentage = MetaConsumer<CSS::FontPaletteMixParameters::Component::Percentage>::consume(args, state);

    auto palette = consumeFontPaletteUnresolved(args, state);
    if (!palette)
        return std::nullopt;

    if (!percentage)
        percentage = MetaConsumer<CSS::FontPaletteMixParameters::Component::Percentage>::consume(args, state);

    return CSS::FontPaletteMixParameters::Component {
        .palette = WTF::move(*palette),
        .percentage = WTF::move(percentage)
    };
}

static std::optional<CSS::FontPaletteMixFunction> consumeFontPaletteMixFunctionUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <palette-mix()> = palette-mix( <color-interpolation-method>? , [ <'font-palette'> && <percentage [0,100]>? ]# )
    // https://drafts.csswg.org/css-fonts-4/#funcdef-palette-mix
    ASSERT(range.peek().functionId() == CSSValueID::PaletteMix);

    if (!state.context.cssFontPaletteMixFunctionEnabled)
        return std::nullopt;

    auto args = consumeFunction(range);

    std::optional<CSS::ColorInterpolationMethod> colorInterpolationMethod = CSS::defaultInterpolationMethodForPaletteMix;
    if (args.peek().id() == CSSValueID::In) {
        colorInterpolationMethod = consumeColorInterpolationMethod(args, state);
        if (!colorInterpolationMethod)
            return std::nullopt;

        if (!consumeCommaIncludingWhitespace(args))
            return std::nullopt;
    }

    CommaSeparatedVector<CSS::FontPaletteMixParameters::Component> components;
    do {
        auto component = consumeFontPaletteMixComponent(args, state);
        if (!component)
            return std::nullopt;
        components.value.append(WTF::move(*component));
    } while (consumeCommaIncludingWhitespace(args));

    if (!args.atEnd())
        return std::nullopt;

    return CSS::FontPaletteMixFunction {
        CSS::FontPaletteMixFunctionValue {
            .parameters = CSS::FontPaletteMixParameters {
                .colorInterpolationMethod = WTF::move(*colorInterpolationMethod),
                .components = WTF::move(components),
            }
        }
    };
}

std::optional<CSS::FontPalette> consumeFontPaletteUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'font-palette'> = normal | light | dark | <palette-identifier> | <palette-mix()>
    // https://drafts.csswg.org/css-fonts-4/#propdef-font-palette

    switch (range.peek().id()) {
    case CSSValueID::Invalid:
        break;
    case CSSValueID::Normal:
        range.consumeIncludingWhitespace();
        return CSS::FontPalette { CSS::Keyword::Normal { } };
    case CSSValueID::Light:
        range.consumeIncludingWhitespace();
        return CSS::FontPalette { CSS::Keyword::Light { } };
    case CSSValueID::Dark:
        range.consumeIncludingWhitespace();
        return CSS::FontPalette { CSS::Keyword::Dark { } };
    default:
        return std::nullopt;
    }

    switch (range.peek().functionId()) {
    case CSSValueID::Invalid:
        break;
    case CSSValueID::PaletteMix:
        return consumeFontPaletteMixFunctionUnresolved(range, state);
    default:
        return std::nullopt;
    }

    auto paletteIdentifier = consumeUnresolvedDashedIdent(range, state);
    if (!paletteIdentifier)
        return std::nullopt;

    return CSS::FontPalette { WTF::move(*paletteIdentifier) };
}

RefPtr<CSSValue> consumeFontPalette(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'font-palette'> = normal | light | dark | <palette-identifier> | <palette-mix()>
    // https://drafts.csswg.org/css-fonts-4/#propdef-font-palette

    if (auto unresolved = consumeFontPaletteUnresolved(range, state))
        return CSSFontPaletteValue::create(WTF::move(*unresolved));
    return nullptr;
}

// MARK: - @-rule descriptor consumers:

// MARK: - @font-face

// MARK: @font-face 'src'

Vector<FontTechnology> consumeFontTech(CSSParserTokenRange& range, CSS::PropertyParserState&, bool singleValue)
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

static bool NODELETE isFontFormatKeywordValid(CSSValueID id)
{
    switch (id) {
    case CSSValueID::Collection:
    case CSSValueID::EmbeddedOpentype:
    case CSSValueID::Opentype:
    case CSSValueID::Svg:
    case CSSValueID::Truetype:
    case CSSValueID::Woff:
    case CSSValueID::Woff2:
        return true;
    default:
        return false;
    }
}

String consumeFontFormat(CSSParserTokenRange& range, CSS::PropertyParserState&, bool rejectStringValues)
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

static RefPtr<CSSFontFaceSrcResourceValue> consumeFontFaceSrcURI(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <font-src-uri>       = <url> [ format( <font-format> ) ]? [ tech( <font-tech># ) ]?

    // <font-format>        = <string> | collection | embedded-opentype | opentype | svg | truetype | woff | woff2
    // <font-tech>          = <font-features-tech> | <color-font-tech> | variations | palettes | incremental
    // <font-features-tech> = features-opentype | features-aat | features-graphite
    // <color-font-tech>    = color-COLRv0 | color-COLRv1 | color-SVG | color-sbix | color-CBDT

    // https://drafts.csswg.org/css-fonts-4/#typedef-font-src

    auto location = consumeURLRaw(range, state, { });
    if (!location)
        return nullptr;

    String format;
    Vector<FontTechnology> technologies;
    if (range.peek().functionId() == CSSValueID::Format) {
        format = consumeFontFormat(range, state);
        if (format.isNull())
            return nullptr;
    }
    if (range.peek().functionId() == CSSValueID::Tech) {
        technologies = consumeFontTech(range, state);
        if (technologies.isEmpty())
            return nullptr;
    }
    if (!range.atEnd())
        return nullptr;

    return CSSFontFaceSrcResourceValue::create(WTF::move(*location), WTF::move(format), WTF::move(technologies));
}

static RefPtr<CSSValue> consumeFontFaceSrcLocal(CSSParserTokenRange& range, CSS::PropertyParserState&)
{
    // <font-src-local>     = local( <family-name> )
    // https://drafts.csswg.org/css-fonts-4/#typedef-font-src

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
        return CSSFontFaceSrcLocalValue::create(WTF::move(familyName));
    }
    return nullptr;
}

RefPtr<CSSValueList> consumeFontFaceSrc(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <font-src-list>      = <font-src>#

    // <font-src>           = <font-src-uri> | <font-src-local>
    // <font-src-uri>       = <url> [ format( <font-format> ) ]? [ tech( <font-tech># ) ]?
    // <font-src-local>     = local( <family-name> )

    // <font-format>        = <string> | collection | embedded-opentype | opentype | svg | truetype | woff | woff2
    // <font-tech>          = <font-features-tech> | <color-font-tech> | variations | palettes | incremental
    // <font-features-tech> = features-opentype | features-aat | features-graphite
    // <color-font-tech>    = color-COLRv0 | color-COLRv1 | color-SVG | color-sbix | color-CBDT

    // https://drafts.csswg.org/css-fonts-4/#typedef-font-src

    CSSValueListBuilder values;
    auto consumeSrcListComponent = [&](CSSParserTokenRange& range) -> RefPtr<CSSValue> {
        const CSSParserToken& token = range.peek();
        if (token.type() == CSSParserTokenType::UrlToken || token.functionId() == CSSValueID::Url)
            return consumeFontFaceSrcURI(range, state);
        if (token.functionId() == CSSValueID::Local)
            return consumeFontFaceSrcLocal(range, state);
        return nullptr;
    };
    while (!range.atEnd()) {
        auto begin = range;
        while (!range.atEnd() && range.peek().type() != CSSParserTokenType::CommaToken)
            range.consumeComponentValue();
        auto subrange = begin.rangeUntil(range);
        if (RefPtr parsedValue = consumeSrcListComponent(subrange))
            values.append(parsedValue.releaseNonNull());
        if (!range.atEnd())
            range.consumeIncludingWhitespace();
    }
    if (values.isEmpty())
        return nullptr;
    return CSSValueList::createCommaSeparated(WTF::move(values));
}

RefPtr<CSSValueList> parseFontFaceSrc(StringView string, ScriptExecutionContext& context)
{
    RefPtr document = dynamicDowncast<Document>(context);
    CSSParserContext parserContext = document ? CSSParserContext(*document) : CSSParserContext(HTMLStandardMode);
    CSSParser parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();

    range.consumeWhitespace();

    if (range.atEnd())
        return nullptr;

    auto state = CSS::PropertyParserState { .context = parserContext, .pool = context.cssValuePool() };
    auto parsedValue = consumeFontFaceSrc(range, state);
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

// MARK: @font-face 'size-adjust'

RefPtr<CSSValue> parseFontFaceSizeAdjust(StringView string, ScriptExecutionContext& context)
{
    // <'size-adjust'> = <percentage [0,∞]>
    // https://www.w3.org/TR/css-fonts-5/#descdef-font-face-size-adjust

    CSSParserContext parserContext(parserMode(context));
    CSSParser parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();

    range.consumeWhitespace();

    if (range.atEnd())
        return nullptr;

    auto state = CSS::PropertyParserState { .context = parserContext, .pool = context.cssValuePool() };
    auto parsedValue = CSSPropertyParsing::consumeFontFaceSizeAdjust(range, state);
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

static RefPtr<CSSValue> parseFontFaceMetricOverride(StringView string, ScriptExecutionContext& context,
    NOESCAPE const Function<RefPtr<CSSValue>(CSSParserTokenRange&, CSS::PropertyParserState&)>& consumeMetricOverride)
{
    // <font-metrics-override> = normal | <percentage [0,∞]>
    // https://drafts.csswg.org/css-fonts-4/#font-metrics-override-desc

    CSSParserContext parserContext(parserMode(context));
    CSSParser parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();

    range.consumeWhitespace();

    if (range.atEnd())
        return nullptr;

    auto state = CSS::PropertyParserState { .context = parserContext, .pool = context.cssValuePool() };
    auto parsedValue = consumeMetricOverride(range, state);
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

RefPtr<CSSValue> parseFontFaceAscentOverride(StringView string, ScriptExecutionContext& context)
{
    return parseFontFaceMetricOverride(string, context, CSSPropertyParsing::consumeFontFaceAscentOverride);
}

RefPtr<CSSValue> parseFontFaceDescentOverride(StringView string, ScriptExecutionContext& context)
{
    return parseFontFaceMetricOverride(string, context, CSSPropertyParsing::consumeFontFaceDescentOverride);
}

RefPtr<CSSValue> parseFontFaceLineGapOverride(StringView string, ScriptExecutionContext& context)
{
    return parseFontFaceMetricOverride(string, context, CSSPropertyParsing::consumeFontFaceLineGapOverride);
}

// MARK: @font-face 'unicode-range'

RefPtr<CSSValueList> parseFontFaceUnicodeRange(StringView string, ScriptExecutionContext& context)
{
    // <'unicode-range'> = <unicode-range-token>#
    // https://drafts.csswg.org/css-fonts/#descdef-font-face-unicode-range

    CSSParserContext parserContext(parserMode(context));
    CSSParser parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();

    range.consumeWhitespace();

    if (range.atEnd())
        return nullptr;

    RefPtr parsedValue = CSSPropertyParsing::consumeFontFaceUnicodeRange(range);
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return dynamicDowncast<CSSValueList>(*parsedValue);
}

// MARK: @font-face 'font-display'

RefPtr<CSSValue> parseFontFaceDisplay(StringView string, ScriptExecutionContext& context)
{
    // <'font-display'> = auto | block | swap | fallback | optional
    // https://drafts.csswg.org/css-fonts/#descdef-font-face-font-display

    CSSParserContext parserContext(parserMode(context));
    CSSParser parser(parserContext, string);
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

RefPtr<CSSValue> parseFontFaceFontStyle(StringView string, ScriptExecutionContext& context)
{
    // <'font-style'> = auto | normal | italic | oblique [ <angle [-90deg,90deg]>{1,2} ]?
    // https://drafts.csswg.org/css-fonts/#descdef-font-face-font-style

    // FIXME: Missing support for "auto" identifier.

    CSSParserContext parserContext(parserMode(context));
    CSSParser parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();

    range.consumeWhitespace();

    if (range.atEnd())
        return nullptr;

    auto state = CSS::PropertyParserState { .context = parserContext, .pool = context.cssValuePool() };
    auto parsedValue = consumeFontFaceFontStyle(range, state);
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

#if ENABLE(VARIATION_FONTS)

std::optional<CSS::FontStyleRange> consumeUnresolvedFontFaceFontStyle(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'font-style'> auto | normal | italic | oblique [ <angle [-90deg,90deg]>{1,2} ]?
    // https://drafts.csswg.org/css-fonts-4/#descdef-font-face-font-style

    // FIXME: Missing support for "auto" identifier.

    switch (range.peek().id()) {
    case CSSValueID::Normal:
        range.consumeIncludingWhitespace();
        return CSS::FontStyleRange { CSS::Keyword::Normal { } };
    case CSSValueID::Italic:
        range.consumeIncludingWhitespace();
        return CSS::FontStyleRange { CSS::Keyword::Italic { } };
    case CSSValueID::Oblique: {
        auto rangeCopy = range;

        rangeCopy.consumeIncludingWhitespace();

        if (rangeCopy.atEnd()) {
            range = rangeCopy;
            return CSS::FontStyleRange { CSS::FontStyleRange::Oblique { std::nullopt } };
        }

        auto firstAngle = consumeFontStyleAngleUnresolved(rangeCopy, state);
        if (!firstAngle)
            return std::nullopt;

        using Oblique = CSS::FontStyleRange::Oblique;

        if (rangeCopy.atEnd()) {
            range = rangeCopy;
            if (firstAngle->isKnownZero())
                return CSS::FontStyleRange { CSS::Keyword::Normal { } };
            return CSS::FontStyleRange { Oblique { Oblique::Angles { *firstAngle, *firstAngle } } };
        }

        auto secondAngle = consumeFontStyleAngleUnresolved(rangeCopy, state);
        if (!secondAngle)
            return std::nullopt;

        range = rangeCopy;
        if (firstAngle->isKnownZero() && secondAngle->isKnownZero())
            return CSS::FontStyleRange { CSS::Keyword::Normal { } };
        return CSS::FontStyleRange { Oblique { Oblique::Angles { *firstAngle, *secondAngle } } };
    }

    default:
        return std::nullopt;
    }
}

RefPtr<CSSValue> consumeFontFaceFontStyle(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (auto unresolved = consumeUnresolvedFontFaceFontStyle(range, state))
        return CSSFontStyleRangeValue::create(WTF::move(*unresolved));
    return nullptr;
}

#else

RefPtr<CSSValue> consumeFontFaceFontStyle(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'font-style'> = <font-style>
    // NOTE: This is the pre-variation fonts definition.

    return consumeFontStyle(range, state);
}

#endif

// MARK: @font-face 'font-feature-settings'

static std::optional<FontTag> NODELETE consumeFontOpenTypeTag(CSSParserTokenRange& range)
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

RefPtr<CSSValue> consumeFeatureTagValue(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <feature-tag-value> = <opentype-tag> [ <integer [0,∞]> | on | off ]?
    // https://drafts.csswg.org/css-fonts/#feature-tag-value

    using namespace CSS::Literals;

    auto tag = consumeFontOpenTypeTag(range);
    if (!tag)
        return nullptr;

    std::optional<CSS::Integer<CSS::Nonnegative>> tagValue;
    if (!range.atEnd() && range.peek().type() != CommaToken) {
        // Feature tag values could follow: <integer [0,∞]> | on | off
        if (auto integer = MetaConsumer<CSS::Integer<CSS::Nonnegative>>::consume(range, state))
            tagValue = WTF::move(integer);
        else if (range.peek().id() == CSSValueID::On || range.peek().id() == CSSValueID::Off)
            tagValue = range.consumeIncludingWhitespace().id() == CSSValueID::On ? 1_css_integer : 0_css_integer;
        else
            return nullptr;
    } else
        tagValue = 1_css_integer;

    return CSSFontFeatureValue::create(WTF::move(*tag), WTF::move(*tagValue));
}

RefPtr<CSSValue> parseFontFaceFeatureSettings(StringView string, ScriptExecutionContext& context)
{
    // <'font-feature-settings'> = normal | <feature-tag-value>#
    // https://drafts.csswg.org/css-fonts/#descdef-font-face-font-feature-settings

    CSSParserContext parserContext(parserMode(context));
    CSSParser parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();

    range.consumeWhitespace();

    if (range.atEnd())
        return nullptr;

    auto state = CSS::PropertyParserState { .context = parserContext, .pool = context.cssValuePool() };
    auto parsedValue = CSSPropertyParsing::consumeFontFeatureSettings(range, state);
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

// MARK: @font-face 'font-variation-settings'

#if ENABLE(VARIATION_FONTS)

RefPtr<CSSValue> consumeVariationTagValue(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <variation-tag-value> = <opentype-tag> <number>
    // https://drafts.csswg.org/css-fonts/#font-variation-settings-def

    auto tag = consumeFontOpenTypeTag(range);
    if (!tag)
        return nullptr;

    auto tagValue = CSSPrimitiveValueResolver<CSS::Number<>>::consumeAndResolve(range, state);
    if (!tagValue)
        return nullptr;

    return CSSFontVariationValue::create(WTF::move(*tag), tagValue.releaseNonNull());
}

#endif

// MARK: @font-face 'font-width'

RefPtr<CSSValue> parseFontFaceFontWidth(StringView string, ScriptExecutionContext& context)
{
    // <font-width> = auto | <'font-width'>{1,2}
    // https://drafts.csswg.org/css-fonts-4/#descdef-font-face-font-width

    CSSParserContext parserContext(parserMode(context));
    CSSParser parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();

    range.consumeWhitespace();

    if (range.atEnd())
        return nullptr;

    auto state = CSS::PropertyParserState { .context = parserContext, .pool = context.cssValuePool() };
    auto parsedValue = CSSPropertyParsing::consumeFontFaceFontWidth(range, state);
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

// MARK: @font-face 'font-weight'

RefPtr<CSSValue> parseFontFaceFontWeight(StringView string, ScriptExecutionContext& context)
{
    // <'font-weight'> = auto | <font-weight-absolute>{1,2}
    // https://drafts.csswg.org/css-fonts-4/#descdef-font-face-font-weight

    CSSParserContext parserContext(parserMode(context));
    CSSParser parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();

    range.consumeWhitespace();

    if (range.atEnd())
        return nullptr;

    auto state = CSS::PropertyParserState { .context = parserContext, .pool = context.cssValuePool() };
    auto parsedValue = CSSPropertyParsing::consumeFontFaceFontWeight(range, state);
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
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
