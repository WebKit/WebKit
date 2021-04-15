// Copyright 2015 The Chromium Authors. All rights reserved.
// Copyright (C) 2016 Apple Inc. All rights reserved.
// Copyright (C) 2021 Metrological Group B.V.
// Copyright (C) 2021 Igalia S.L.
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

#include "config.h"
#include "CSSPropertyParserWorkerSafe.h"

#include "CSSFontStyleValue.h"
#include "CSSParserFastPaths.h"
#include "CSSParserImpl.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParser.h"
#include "CSSPropertyParserHelpers.h"
#include "CSSTokenizer.h"
#include "ScriptExecutionContext.h"

#if ENABLE(VARIATION_FONTS)
#include "CSSFontStyleRangeValue.h"
#endif

namespace WebCore {

Optional<FontRaw> CSSPropertyParserWorkerSafe::parseFont(const String& string, CSSParserMode mode)
{
    CSSTokenizer tokenizer(string);
    CSSParserTokenRange range(tokenizer.tokenRange());
    range.consumeWhitespace();

    return CSSPropertyParserHelpers::consumeFontRaw(range, mode);
}

Color CSSPropertyParserWorkerSafe::parseColor(const String& string)
{
    if (auto color = CSSParserFastPaths::parseSimpleColor(string))
        return *color;

    CSSTokenizer tokenizer(string);
    CSSParserTokenRange range(tokenizer.tokenRange());
    range.consumeWhitespace();

    return CSSPropertyParserHelpers::consumeColorWorkerSafe(range, CSSParserContext(HTMLStandardMode));
}

static CSSParserContext parserContext(ScriptExecutionContext& context)
{
    if (!is<Document>(context))
        return HTMLStandardMode;
    return downcast<Document>(context).inQuirksMode() ? HTMLQuirksMode : HTMLStandardMode;
}

RefPtr<CSSValueList> CSSPropertyParserWorkerSafe::parseFontFaceSrc(const String& string, const CSSParserContext& context)
{
    CSSParserImpl parser(context, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();
    range.consumeWhitespace();
    if (range.atEnd())
        return nullptr;
    auto parsedValue = CSSPropertyParserHelpersWorkerSafe::consumeFontFaceSrc(range, parser.context());
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

RefPtr<CSSValue> CSSPropertyParserWorkerSafe::parseFontFaceStyle(const String& string, ScriptExecutionContext& context)
{
    CSSParserImpl parser(parserContext(context), string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();
    range.consumeWhitespace();
    if (range.atEnd())
        return nullptr;
    auto parsedValue =
#if ENABLE(VARIATION_FONTS)
        CSSPropertyParserHelpersWorkerSafe::consumeFontStyleRange(range, parser.context().mode, context.cssValuePool());
#else
        CSSPropertyParserHelpersWorkerSafe::consumeFontStyle(range, parser.context().mode, context.cssValuePool());
#endif
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

RefPtr<CSSValue> CSSPropertyParserWorkerSafe::parseFontFaceWeight(const String& string, ScriptExecutionContext& context)
{
    CSSParserImpl parser(parserContext(context), string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();
    range.consumeWhitespace();
    if (range.atEnd())
        return nullptr;
    auto parsedValue =
#if ENABLE(VARIATION_FONTS)
        CSSPropertyParserHelpersWorkerSafe::consumeFontWeightAbsoluteRange(range, context.cssValuePool());
#else
        CSSPropertyParserHelpersWorkerSafe::consumeFontWeightAbsolute(range, context.cssValuePool());
#endif
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

RefPtr<CSSValue> CSSPropertyParserWorkerSafe::parseFontFaceStretch(const String& string, ScriptExecutionContext& context)
{
    CSSParserImpl parser(parserContext(context), string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();
    range.consumeWhitespace();
    if (range.atEnd())
        return nullptr;
    auto parsedValue =
#if ENABLE(VARIATION_FONTS)
        CSSPropertyParserHelpersWorkerSafe::consumeFontStretchRange(range, context.cssValuePool());
#else
        CSSPropertyParserHelpersWorkerSafe::consumeFontStretch(range, context.cssValuePool());
#endif
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

RefPtr<CSSValue> CSSPropertyParserWorkerSafe::parseFontFaceUnicodeRange(const String& string, ScriptExecutionContext& context)
{
    CSSParserImpl parser(parserContext(context), string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();
    range.consumeWhitespace();
    if (range.atEnd())
        return nullptr;
    auto parsedValue = CSSPropertyParserHelpersWorkerSafe::consumeFontFaceUnicodeRange(range);
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

RefPtr<CSSValue> CSSPropertyParserWorkerSafe::parseFontFaceFeatureSettings(const String& string, ScriptExecutionContext& context)
{
    CSSParserImpl parser(parserContext(context), string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();
    range.consumeWhitespace();
    if (range.atEnd())
        return nullptr;
    auto parsedValue = CSSPropertyParserHelpersWorkerSafe::consumeFontFeatureSettings(range, context.cssValuePool());
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

RefPtr<CSSValue> CSSPropertyParserWorkerSafe::parseFontFaceDisplay(const String& string, ScriptExecutionContext& context)
{
    CSSParserImpl parser(parserContext(context), string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();
    range.consumeWhitespace();
    if (range.atEnd())
        return nullptr;
    auto parsedValue = CSSPropertyParserHelpersWorkerSafe::consumeFontFaceFontDisplay(range, context.cssValuePool());
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

namespace CSSPropertyParserHelpersWorkerSafe {

static RefPtr<CSSValue> consumeFontFaceSrcURI(CSSParserTokenRange& range, const CSSParserContext& context)
{
    String url = CSSPropertyParserHelpers::consumeUrlAsStringView(range).toString();
    if (url.isNull())
        return nullptr;

    RefPtr<CSSFontFaceSrcValue> uriValue = CSSFontFaceSrcValue::create(context.completeURL(url).string(), context.isContentOpaque ? LoadedFromOpaqueSource::Yes : LoadedFromOpaqueSource::No);

    if (range.peek().functionId() != CSSValueFormat)
        return uriValue;

    // FIXME: https://drafts.csswg.org/css-fonts says that format() contains a comma-separated list of strings,
    // but CSSFontFaceSrcValue stores only one format. Allowing one format for now.
    // FIXME: We're allowing the format to be an identifier as well as a string, because the old
    // parser did. It's not clear if we need to continue to support this behavior, but we have lots of
    // layout tests that rely on it.
    CSSParserTokenRange args = CSSPropertyParserHelpers::consumeFunction(range);
    const CSSParserToken& arg = args.consumeIncludingWhitespace();
    if ((arg.type() != StringToken && arg.type() != IdentToken) || !args.atEnd())
        return nullptr;
    uriValue->setFormat(arg.value().toString());
    return uriValue;
}

static RefPtr<CSSValue> consumeFontFaceSrcLocal(CSSParserTokenRange& range)
{
    CSSParserTokenRange args = CSSPropertyParserHelpers::consumeFunction(range);
    if (args.peek().type() == StringToken) {
        const CSSParserToken& arg = args.consumeIncludingWhitespace();
        if (!args.atEnd())
            return nullptr;
        return CSSFontFaceSrcValue::createLocal(arg.value().toString());
    }
    if (args.peek().type() == IdentToken) {
        String familyName = concatenateFamilyName(args);
        if (!args.atEnd())
            return nullptr;
        return CSSFontFaceSrcValue::createLocal(familyName);
    }
    return nullptr;
}

RefPtr<CSSValueList> consumeFontFaceSrc(CSSParserTokenRange& range, const CSSParserContext& context)
{
    RefPtr<CSSValueList> values = CSSValueList::createCommaSeparated();

    do {
        const CSSParserToken& token = range.peek();
        RefPtr<CSSValue> parsedValue;
        if (token.functionId() == CSSValueLocal)
            parsedValue = consumeFontFaceSrcLocal(range);
        else
            parsedValue = consumeFontFaceSrcURI(range, context);
        if (!parsedValue)
            return nullptr;
        values->append(parsedValue.releaseNonNull());
    } while (CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(range));
    return values;
}

RefPtr<CSSFontStyleValue> consumeFontStyle(CSSParserTokenRange& range, CSSParserMode cssParserMode, CSSValuePool& pool)
{
    if (auto result = CSSPropertyParserHelpers::consumeFontStyleRaw(range, cssParserMode)) {
#if ENABLE(VARIATION_FONTS)
        if (result->style == CSSValueOblique && result->angle) {
            return CSSFontStyleValue::create(pool.createIdentifierValue(CSSValueOblique),
                pool.createValue(result->angle->value, result->angle->type));
        }
#endif
        return CSSFontStyleValue::create(pool.createIdentifierValue(result->style));
    }
    return nullptr;
}

#if ENABLE(VARIATION_FONTS)
static RefPtr<CSSPrimitiveValue> consumeFontStyleKeywordValue(CSSParserTokenRange& range, CSSValuePool& pool)
{
    return CSSPropertyParserHelpers::consumeIdentWorkerSafe<CSSValueNormal, CSSValueItalic, CSSValueOblique>(range, pool);
}

RefPtr<CSSFontStyleRangeValue> consumeFontStyleRange(CSSParserTokenRange& range, CSSParserMode cssParserMode, CSSValuePool& pool)
{
    auto keyword = consumeFontStyleKeywordValue(range, pool);
    if (!keyword)
        return nullptr;

    if (keyword->valueID() != CSSValueOblique || range.atEnd())
        return CSSFontStyleRangeValue::create(keyword.releaseNonNull());

    // FIXME: This should probably not allow the unitless zero.
    if (auto firstAngle = CSSPropertyParserHelpers::consumeAngleWorkerSafe(range, cssParserMode, pool, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow)) {
        auto firstAngleInDegrees = firstAngle->doubleValue(CSSUnitType::CSS_DEG);
        if (!isFontStyleAngleInRange(firstAngleInDegrees))
            return nullptr;
        if (range.atEnd()) {
            auto result = CSSValueList::createSpaceSeparated();
            result->append(firstAngle.releaseNonNull());
            return CSSFontStyleRangeValue::create(keyword.releaseNonNull(), WTFMove(result));
        }

        // FIXME: This should probably not allow the unitless zero.
        auto secondAngle = CSSPropertyParserHelpers::consumeAngleWorkerSafe(range, cssParserMode, pool, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow);
        if (!secondAngle)
            return nullptr;
        auto secondAngleInDegrees = secondAngle->doubleValue(CSSUnitType::CSS_DEG);
        if (!isFontStyleAngleInRange(secondAngleInDegrees) || firstAngleInDegrees > secondAngleInDegrees)
            return nullptr;
        auto result = CSSValueList::createSpaceSeparated();
        result->append(firstAngle.releaseNonNull());
        result->append(secondAngle.releaseNonNull());
        return CSSFontStyleRangeValue::create(keyword.releaseNonNull(), WTFMove(result));
    }

    return nullptr;
}
#endif

static RefPtr<CSSPrimitiveValue> consumeFontWeightAbsoluteKeywordValue(CSSParserTokenRange& range, CSSValuePool& pool)
{
    return CSSPropertyParserHelpers::consumeIdentWorkerSafe<CSSValueNormal, CSSValueBold>(range, pool);
}

#if ENABLE(VARIATION_FONTS)
RefPtr<CSSValue> consumeFontWeightAbsoluteRange(CSSParserTokenRange& range, CSSValuePool& pool)
{
    if (auto result = consumeFontWeightAbsoluteKeywordValue(range, pool))
        return result;
    auto firstNumber = CSSPropertyParserHelpers::consumeFontWeightNumberWorkerSafe(range, pool);
    if (!firstNumber)
        return nullptr;
    if (range.atEnd())
        return firstNumber;
    auto secondNumber = CSSPropertyParserHelpers::consumeFontWeightNumberWorkerSafe(range, pool);
    if (!secondNumber || firstNumber->floatValue() > secondNumber->floatValue())
        return nullptr;
    auto result = CSSValueList::createSpaceSeparated();
    result->append(firstNumber.releaseNonNull());
    result->append(secondNumber.releaseNonNull());
    return RefPtr<CSSValue>(WTFMove(result));
}
#else
RefPtr<CSSPrimitiveValue> consumeFontWeightAbsolute(CSSParserTokenRange& range, CSSValuePool& pool)
{
    if (auto result = consumeFontWeightAbsoluteKeywordValue(range, pool))
        return result;
    return CSSPropertyParserHelpers::consumeFontWeightNumberWorkerSafe(range, pool);
}
#endif

RefPtr<CSSPrimitiveValue> consumeFontStretchKeywordValue(CSSParserTokenRange& range, CSSValuePool& pool)
{
    if (auto valueID = CSSPropertyParserHelpers::consumeFontStretchKeywordValueRaw(range))
        return pool.createIdentifierValue(*valueID);
    return nullptr;
}

#if ENABLE(VARIATION_FONTS)
static bool fontStretchIsWithinRange(float stretch)
{
    return stretch >= 0;
}
#endif

RefPtr<CSSPrimitiveValue> consumeFontStretch(CSSParserTokenRange& range, CSSValuePool& pool)
{
    if (auto result = consumeFontStretchKeywordValue(range, pool))
        return result;
#if ENABLE(VARIATION_FONTS)
    if (auto percent = CSSPropertyParserHelpers::consumePercentWorkerSafe(range, ValueRangeNonNegative, pool))
        return fontStretchIsWithinRange(percent->value<float>()) ? percent : nullptr;
#endif
    return nullptr;
}

#if ENABLE(VARIATION_FONTS)
RefPtr<CSSValue> consumeFontStretchRange(CSSParserTokenRange& range, CSSValuePool& pool)
{
    if (auto result = consumeFontStretchKeywordValue(range, pool))
        return result;
    auto firstPercent = CSSPropertyParserHelpers::consumePercentWorkerSafe(range, ValueRangeNonNegative, pool);
    if (!firstPercent || !fontStretchIsWithinRange(firstPercent->value<float>()))
        return nullptr;
    if (range.atEnd())
        return firstPercent;
    auto secondPercent = CSSPropertyParserHelpers::consumePercentWorkerSafe(range, ValueRangeNonNegative, pool);
    if (!secondPercent || !fontStretchIsWithinRange(secondPercent->value<float>()) || firstPercent->floatValue() > secondPercent->floatValue())
        return nullptr;
    auto result = CSSValueList::createSpaceSeparated();
    result->append(firstPercent.releaseNonNull());
    result->append(secondPercent.releaseNonNull());
    return RefPtr<CSSValue>(WTFMove(result));
}
#endif

RefPtr<CSSValueList> consumeFontFaceUnicodeRange(CSSParserTokenRange& range)
{
    RefPtr<CSSValueList> values = CSSValueList::createCommaSeparated();

    do {
        const CSSParserToken& token = range.consumeIncludingWhitespace();
        if (token.type() != UnicodeRangeToken)
            return nullptr;

        UChar32 start = token.unicodeRangeStart();
        UChar32 end = token.unicodeRangeEnd();
        if (start > end)
            return nullptr;
        values->append(CSSUnicodeRangeValue::create(start, end));
    } while (CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(range));

    return values;
}

static RefPtr<CSSFontFeatureValue> consumeFontFeatureTag(CSSParserTokenRange& range)
{
    // Feature tag name consists of 4-letter characters.
    static const unsigned tagNameLength = 4;

    const CSSParserToken& token = range.consumeIncludingWhitespace();
    // Feature tag name comes first
    if (token.type() != StringToken)
        return nullptr;
    if (token.value().length() != tagNameLength)
        return nullptr;
    
    FontTag tag;
    for (unsigned i = 0; i < tag.size(); ++i) {
        // Limits the range of characters to 0x20-0x7E, following the tag name rules defiend in the OpenType specification.
        UChar character = token.value()[i];
        if (character < 0x20 || character > 0x7E)
            return nullptr;
        tag[i] = toASCIILower(character);
    }

    int tagValue = 1;
    if (!range.atEnd() && range.peek().type() != CommaToken) {
        // Feature tag values could follow: <integer> | on | off
        if (auto integer = CSSPropertyParserHelpers::consumeIntegerRaw(range, 0))
            tagValue = *integer;
        else if (range.peek().id() == CSSValueOn || range.peek().id() == CSSValueOff)
            tagValue = range.consumeIncludingWhitespace().id() == CSSValueOn;
        else
            return nullptr;
    }
    return CSSFontFeatureValue::create(WTFMove(tag), tagValue);
}

RefPtr<CSSValue> consumeFontFeatureSettings(CSSParserTokenRange& range, CSSValuePool& pool)
{
    if (range.peek().id() == CSSValueNormal)
        return CSSPropertyParserHelpers::consumeIdentWorkerSafe(range, pool);
    RefPtr<CSSValueList> settings = CSSValueList::createCommaSeparated();
    do {
        RefPtr<CSSFontFeatureValue> fontFeatureValue = consumeFontFeatureTag(range);
        if (!fontFeatureValue)
            return nullptr;
        settings->append(fontFeatureValue.releaseNonNull());
    } while (CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(range));
    return settings;
}

RefPtr<CSSPrimitiveValue> consumeFontFaceFontDisplay(CSSParserTokenRange& range, CSSValuePool& pool)
{
    return CSSPropertyParserHelpers::consumeIdentWorkerSafe<CSSValueAuto, CSSValueBlock, CSSValueSwap, CSSValueFallback, CSSValueOptional>(range, pool);
}

} // namespace CSSPropertyParserHelpersWorkerSafe

} // namespace WebCore
