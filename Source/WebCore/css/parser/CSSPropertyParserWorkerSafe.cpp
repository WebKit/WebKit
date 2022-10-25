// Copyright 2015 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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

#include "CSSFontFaceSrcValue.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontStyleValue.h"
#include "CSSImageValue.h"
#include "CSSParserFastPaths.h"
#include "CSSParserImpl.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParser.h"
#include "CSSTokenizer.h"
#include "CSSUnicodeRangeValue.h"
#include "Document.h"
#include "ParsingUtilities.h"
#include "ScriptExecutionContext.h"
#include "StyleSheetContents.h"

#if ENABLE(VARIATION_FONTS)
#include "CSSFontStyleRangeValue.h"
#endif

namespace WebCore {

std::optional<CSSPropertyParserHelpers::FontRaw> CSSPropertyParserWorkerSafe::parseFont(const String& string, CSSParserMode mode)
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

static CSSParserMode parserMode(ScriptExecutionContext& context)
{
    return (is<Document>(context) && downcast<Document>(context).inQuirksMode()) ? HTMLQuirksMode : HTMLStandardMode;
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
    CSSParserContext parserContext(parserMode(context));
    CSSParserImpl parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();
    range.consumeWhitespace();
    if (range.atEnd())
        return nullptr;
    auto parsedValue =
#if ENABLE(VARIATION_FONTS)
        CSSPropertyParserHelpersWorkerSafe::consumeFontStyleRange(range, parserContext.mode, context.cssValuePool());
#else
        CSSPropertyParserHelpersWorkerSafe::consumeFontStyle(range, parserContext.mode, context.cssValuePool());
#endif
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

RefPtr<CSSValue> CSSPropertyParserWorkerSafe::parseFontFaceWeight(const String& string, ScriptExecutionContext& context)
{
    CSSParserContext parserContext(parserMode(context));
    CSSParserImpl parser(parserContext, string);
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
    CSSParserContext parserContext(parserMode(context));
    CSSParserImpl parser(parserContext, string);
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

RefPtr<CSSValueList> CSSPropertyParserWorkerSafe::parseFontFaceUnicodeRange(const String& string, ScriptExecutionContext& context)
{
    CSSParserContext parserContext(parserMode(context));
    CSSParserImpl parser(parserContext, string);
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
    CSSParserContext parserContext(parserMode(context));
    CSSParserImpl parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();
    range.consumeWhitespace();
    if (range.atEnd())
        return nullptr;
    auto parsedValue = CSSPropertyParserHelpersWorkerSafe::consumeFontFeatureSettings(range, context.cssValuePool());
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

RefPtr<CSSPrimitiveValue> CSSPropertyParserWorkerSafe::parseFontFaceDisplay(const String& string, ScriptExecutionContext& context)
{
    CSSParserContext parserContext(parserMode(context));
    CSSParserImpl parser(parserContext, string);
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

static RefPtr<CSSFontFaceSrcValue> consumeFontFaceSrcURI(CSSParserTokenRange& range, const CSSParserContext& context)
{
    auto location = context.completeURL(CSSPropertyParserHelpers::consumeUrlAsStringView(range).toString()).resolvedURL.string();
    if (location.isNull())
        return nullptr;

    String format;
    if (range.peek().functionId() == CSSValueFormat) {
        // https://drafts.csswg.org/css-fonts/#descdef-font-face-src
        // FIXME: The format should be a comma-separated list; at this time we support only one.
        // FIXME: We allow any identifier here and convert all to strings; specification calls for only certain identifiers.
        auto args = CSSPropertyParserHelpers::consumeFunction(range);
        auto& arg = args.consumeIncludingWhitespace();
        if ((arg.type() != StringToken && arg.type() != IdentToken) || !args.atEnd())
            return nullptr;
        format = arg.value().toString();
    }

    // FIXME: Change CSSFontFaceSrcValue::create to take format so we don't need a separate setFormat call.
    auto srcValue = CSSFontFaceSrcValue::create(location, context.isContentOpaque ? LoadedFromOpaqueSource::Yes : LoadedFromOpaqueSource::No);
    srcValue->setFormat(format);
    return srcValue;
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
        String familyName = CSSPropertyParserHelpers::concatenateFamilyName(args);
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

#if ENABLE(VARIATION_FONTS)

static RefPtr<CSSPrimitiveValue> consumeFontStyleAngle(CSSParserTokenRange& range, CSSParserMode mode, CSSValuePool& pool)
{
    auto rangeAfterAngle = range;
    auto angle = CSSPropertyParserHelpers::consumeAngleWorkerSafe(rangeAfterAngle, mode, pool);
    if (!angle)
        return nullptr;
    if (!angle->isCalculated() && !CSSPropertyParserHelpers::isFontStyleAngleInRange(angle->doubleValue(CSSUnitType::CSS_DEG)))
        return nullptr;
    range = rangeAfterAngle;
    return angle;
}

RefPtr<CSSFontStyleRangeValue> consumeFontStyleRange(CSSParserTokenRange& range, CSSParserMode mode, CSSValuePool& pool)
{
    auto keyword = CSSPropertyParserHelpers::consumeIdentWorkerSafe<CSSValueNormal, CSSValueItalic, CSSValueOblique>(range, pool);
    if (!keyword)
        return nullptr;

    if (keyword->valueID() != CSSValueOblique || range.atEnd())
        return CSSFontStyleRangeValue::create(keyword.releaseNonNull());

    auto rangeAfterAngles = range;
    auto firstAngle = consumeFontStyleAngle(rangeAfterAngles, mode, pool);
    if (!firstAngle)
        return nullptr;

    auto result = CSSValueList::createSpaceSeparated();
    result->append(firstAngle.releaseNonNull());

    if (!rangeAfterAngles.atEnd()) {
        auto secondAngle = consumeFontStyleAngle(rangeAfterAngles, mode, pool);
        if (!secondAngle)
            return nullptr;
        result->append(secondAngle.releaseNonNull());
    }

    range = rangeAfterAngles;
    return CSSFontStyleRangeValue::create(keyword.releaseNonNull(), WTFMove(result));
}

#endif

RefPtr<CSSFontStyleValue> consumeFontStyle(CSSParserTokenRange& range, CSSParserMode mode, CSSValuePool& pool)
{
    auto keyword = CSSPropertyParserHelpers::consumeIdentWorkerSafe<CSSValueNormal, CSSValueItalic, CSSValueOblique>(range, pool);
    if (!keyword)
        return nullptr;

#if ENABLE(VARIATION_FONTS)
    if (keyword->valueID() == CSSValueOblique && !range.atEnd()) {
        if (auto angle = consumeFontStyleAngle(range, mode, pool))
            return CSSFontStyleValue::create(keyword.releaseNonNull(), angle.releaseNonNull());
    }
#else
    UNUSED_PARAM(mode);
#endif

    return CSSFontStyleValue::create(keyword.releaseNonNull());
}

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
    if (!secondNumber)
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
    if (auto percent = CSSPropertyParserHelpers::consumePercentWorkerSafe(range, ValueRange::NonNegative, pool))
        return fontStretchIsWithinRange(percent->value<float>()) ? percent : nullptr;
#endif
    return nullptr;
}

#if ENABLE(VARIATION_FONTS)
RefPtr<CSSValue> consumeFontStretchRange(CSSParserTokenRange& range, CSSValuePool& pool)
{
    if (auto result = consumeFontStretchKeywordValue(range, pool))
        return result;
    auto firstPercent = CSSPropertyParserHelpers::consumePercentWorkerSafe(range, ValueRange::NonNegative, pool);
    if (!firstPercent || !fontStretchIsWithinRange(firstPercent->value<float>()))
        return nullptr;
    if (range.atEnd())
        return firstPercent;
    auto secondPercent = CSSPropertyParserHelpers::consumePercentWorkerSafe(range, ValueRange::NonNegative, pool);
    if (!secondPercent || !fontStretchIsWithinRange(secondPercent->value<float>()))
        return nullptr;
    auto result = CSSValueList::createSpaceSeparated();
    result->append(firstPercent.releaseNonNull());
    result->append(secondPercent.releaseNonNull());
    return RefPtr<CSSValue>(WTFMove(result));
}
#endif

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
    UChar32 start;
    UChar32 end;
};

static std::optional<UnicodeRange> consumeUnicodeRange(CSSParserTokenRange& range)
{
    return readCharactersForParsing(consumeUnicodeRangeString(range), [&](auto buffer) -> std::optional<UnicodeRange> {
        if (!skipExactly(buffer, '+'))
            return std::nullopt;
        UChar32 start = 0;
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
    auto values = CSSValueList::createCommaSeparated();
    do {
        auto unicodeRange = consumeUnicodeRange(range);
        range.consumeWhitespace();
        if (!unicodeRange || unicodeRange->end > UCHAR_MAX_VALUE || unicodeRange->start > unicodeRange->end)
            return nullptr;
        values->append(CSSUnicodeRangeValue::create(unicodeRange->start, unicodeRange->end));
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
        if (auto integer = CSSPropertyParserHelpers::consumeIntegerZeroAndGreaterRaw(range))
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
