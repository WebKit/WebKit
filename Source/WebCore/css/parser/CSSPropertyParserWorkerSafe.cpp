// Copyright 2015 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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
#include "CSSFontStyleWithAngleValue.h"
#include "CSSImageValue.h"
#include "CSSParserFastPaths.h"
#include "CSSParserImpl.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParser.h"
#include "CSSPropertyParserHelpers.h"
#include "CSSTokenizer.h"
#include "CSSUnicodeRangeValue.h"
#include "Document.h"
#include "FontCustomPlatformData.h"
#include "ParsingUtilities.h"
#include "ScriptExecutionContext.h"
#include "StyleSheetContents.h"

#if ENABLE(VARIATION_FONTS)
#include "CSSFontStyleRangeValue.h"
#include "CSSFontVariationValue.h"
#endif

namespace WebCore {

namespace CSSPropertyParserHelpersWorkerSafe {

RefPtr<CSSValue> consumeFontFeatureSettings(CSSParserTokenRange&);
RefPtr<CSSPrimitiveValue> consumeFontStretch(CSSParserTokenRange&);

}

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
    auto* document = dynamicDowncast<Document>(context);
    return (document && document->inQuirksMode()) ? HTMLQuirksMode : HTMLStandardMode;
}

RefPtr<CSSValueList> CSSPropertyParserWorkerSafe::parseFontFaceSrc(const String& string, const CSSParserContext& context)
{
    CSSParserImpl parser(context, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();
    range.consumeWhitespace();
    if (range.atEnd())
        return nullptr;
    RefPtr parsedValue = CSSPropertyParserHelpersWorkerSafe::consumeFontFaceSrc(range, parser.context());
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
    RefPtr parsedValue =
#if ENABLE(VARIATION_FONTS)
        CSSPropertyParserHelpersWorkerSafe::consumeFontStyleRange(range, parserContext.mode);
#else
        CSSPropertyParserHelpersWorkerSafe::consumeFontStyle(range, parserContext.mode);
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
    RefPtr parsedValue =
#if ENABLE(VARIATION_FONTS)
        CSSPropertyParserHelpersWorkerSafe::consumeFontWeightAbsoluteRange(range);
#else
        CSSPropertyParserHelpersWorkerSafe::consumeFontWeightAbsolute(range);
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
    RefPtr parsedValue =
#if ENABLE(VARIATION_FONTS)
        CSSPropertyParserHelpersWorkerSafe::consumeFontStretchRange(range);
#else
        CSSPropertyParserHelpersWorkerSafe::consumeFontStretch(range);
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
    RefPtr parsedValue = CSSPropertyParserHelpersWorkerSafe::consumeFontFaceUnicodeRange(range);
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
    RefPtr parsedValue = CSSPropertyParserHelpersWorkerSafe::consumeFontFeatureSettings(range);
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
    RefPtr parsedValue = CSSPropertyParserHelpersWorkerSafe::consumeFontFaceFontDisplay(range);
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

RefPtr<CSSValue> CSSPropertyParserWorkerSafe::parseFontFaceSizeAdjust(const String& string, ScriptExecutionContext& context)
{
    CSSParserContext parserContext(parserMode(context));
    CSSParserImpl parser(parserContext, string);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();
    range.consumeWhitespace();
    if (range.atEnd())
        return nullptr;
    RefPtr parsedValue = CSSPropertyParserHelpers::consumePercent(range, ValueRange::NonNegative);
    if (!parsedValue || !range.atEnd())
        return nullptr;

    return parsedValue;
}

namespace CSSPropertyParserHelpersWorkerSafe {

static RefPtr<CSSFontFaceSrcResourceValue> consumeFontFaceSrcURI(CSSParserTokenRange& range, const CSSParserContext& context)
{
    StringView parsedURL = CSSPropertyParserHelpers::consumeURLRaw(range);
    String urlString = !parsedURL.is8Bit() && parsedURL.containsOnlyASCII() ? String::make8Bit(parsedURL.characters16(), parsedURL.length()) : parsedURL.toString();
    auto location = context.completeURL(urlString);
    if (location.resolvedURL.isNull())
        return nullptr;

    String format;
    Vector<FontTechnology> technologies;
    if (range.peek().functionId() == CSSValueFormat) {
        format = CSSPropertyParserHelpers::consumeFontFormat(range);
        if (format.isNull())
            return nullptr;
    }
    if (range.peek().functionId() == CSSValueTech) {
        technologies = CSSPropertyParserHelpers::consumeFontTech(range);
        if (technologies.isEmpty())
            return nullptr;
    }
    if (!range.atEnd())
        return nullptr;

    return CSSFontFaceSrcResourceValue::create(WTFMove(location), WTFMove(format), WTFMove(technologies), context.isContentOpaque ? LoadedFromOpaqueSource::Yes : LoadedFromOpaqueSource::No);
}

static RefPtr<CSSValue> consumeFontFaceSrcLocal(CSSParserTokenRange& range)
{
    CSSParserTokenRange args = CSSPropertyParserHelpers::consumeFunction(range);
    if (args.peek().type() == StringToken) {
        auto& arg = args.consumeIncludingWhitespace();
        if (!args.atEnd() || !range.atEnd())
            return nullptr;
        return CSSFontFaceSrcLocalValue::create(arg.value().toAtomString());
    }
    if (args.peek().type() == IdentToken) {
        AtomString familyName = CSSPropertyParserHelpers::concatenateFamilyName(args);
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

#if ENABLE(VARIATION_FONTS)

static RefPtr<CSSPrimitiveValue> consumeFontStyleAngle(CSSParserTokenRange& range, CSSParserMode mode)
{
    auto rangeAfterAngle = range;
    RefPtr angle = CSSPropertyParserHelpers::consumeAngle(rangeAfterAngle, mode);
    if (!angle)
        return nullptr;
    if (!angle->isCalculated() && !CSSPropertyParserHelpers::isFontStyleAngleInRange(angle->doubleValue(CSSUnitType::CSS_DEG)))
        return nullptr;
    range = rangeAfterAngle;
    return angle;
}

RefPtr<CSSValue> consumeFontStyleRange(CSSParserTokenRange& range, CSSParserMode mode)
{
    RefPtr keyword = CSSPropertyParserHelpers::consumeIdent<CSSValueNormal, CSSValueItalic, CSSValueOblique>(range);
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

RefPtr<CSSValue> consumeFontStyle(CSSParserTokenRange& range, CSSParserMode mode)
{
    RefPtr keyword = CSSPropertyParserHelpers::consumeIdent<CSSValueNormal, CSSValueItalic, CSSValueOblique>(range);
    if (!keyword)
        return nullptr;

#if ENABLE(VARIATION_FONTS)
    if (keyword->valueID() == CSSValueOblique && !range.atEnd()) {
        if (auto angle = consumeFontStyleAngle(range, mode))
            return CSSFontStyleWithAngleValue::create(angle.releaseNonNull());
    }
#else
    UNUSED_PARAM(mode);
#endif

    return keyword;
}

static RefPtr<CSSPrimitiveValue> consumeFontWeightAbsoluteKeywordValue(CSSParserTokenRange& range)
{
    return CSSPropertyParserHelpers::consumeIdent<CSSValueNormal, CSSValueBold>(range);
}

#if ENABLE(VARIATION_FONTS)

RefPtr<CSSValue> consumeFontWeightAbsoluteRange(CSSParserTokenRange& range)
{
    if (RefPtr result = consumeFontWeightAbsoluteKeywordValue(range))
        return result;
    RefPtr firstNumber = CSSPropertyParserHelpers::consumeFontWeightNumber(range);
    if (!firstNumber)
        return nullptr;
    if (range.atEnd())
        return firstNumber;
    RefPtr secondNumber = CSSPropertyParserHelpers::consumeFontWeightNumber(range);
    if (!secondNumber)
        return nullptr;
    return CSSValueList::createSpaceSeparated(firstNumber.releaseNonNull(), secondNumber.releaseNonNull());
}

#else

RefPtr<CSSPrimitiveValue> consumeFontWeightAbsolute(CSSParserTokenRange& range)
{
    if (RefPtr result = consumeFontWeightAbsoluteKeywordValue(range))
        return result;
    return CSSPropertyParserHelpers::consumeFontWeightNumber(range);
}

#endif

RefPtr<CSSPrimitiveValue> consumeFontStretchKeywordValue(CSSParserTokenRange& range)
{
    if (auto valueID = CSSPropertyParserHelpers::consumeFontStretchKeywordValueRaw(range))
        return CSSPrimitiveValue::create(*valueID);
    return nullptr;
}

RefPtr<CSSPrimitiveValue> consumeFontStretch(CSSParserTokenRange& range)
{
    if (RefPtr result = consumeFontStretchKeywordValue(range))
        return result;
#if ENABLE(VARIATION_FONTS)
    if (RefPtr percent = CSSPropertyParserHelpers::consumePercent(range, ValueRange::NonNegative))
        return percent;
#endif
    return nullptr;
}

#if ENABLE(VARIATION_FONTS)

RefPtr<CSSValue> consumeFontStretchRange(CSSParserTokenRange& range)
{
    if (RefPtr result = consumeFontStretchKeywordValue(range))
        return result;
    RefPtr firstPercent = CSSPropertyParserHelpers::consumePercent(range, ValueRange::NonNegative);
    if (!firstPercent)
        return nullptr;
    if (range.atEnd())
        return firstPercent;
    RefPtr secondPercent = CSSPropertyParserHelpers::consumePercent(range, ValueRange::NonNegative);
    if (!secondPercent)
        return nullptr;
    return CSSValueList::createSpaceSeparated(firstPercent.releaseNonNull(), secondPercent.releaseNonNull());
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
    } while (CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(range));
    return CSSValueList::createCommaSeparated(WTFMove(values));
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

    int tagValue = 1;
    if (!range.atEnd() && range.peek().type() != CommaToken) {
        // Feature tag values could follow: <integer> | on | off
        if (auto integer = CSSPropertyParserHelpers::consumeNonNegativeIntegerRaw(range))
            tagValue = *integer;
        else if (range.peek().id() == CSSValueOn || range.peek().id() == CSSValueOff)
            tagValue = range.consumeIncludingWhitespace().id() == CSSValueOn;
        else
            return nullptr;
    }
    return CSSFontFeatureValue::create(WTFMove(*tag), tagValue);
}

// FIXME: This function is identical to the one generated in CSSPropertyParsing, both equally safe; should come up with a way to not duplicate.
RefPtr<CSSValue> consumeFontFeatureSettings(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNormal)
        return CSSPropertyParserHelpers::consumeIdent(range);
    return CSSPropertyParserHelpers::consumeCommaSeparatedListWithoutSingleValueOptimization(range, consumeFeatureTagValue);
}

RefPtr<CSSPrimitiveValue> consumeFontFaceFontDisplay(CSSParserTokenRange& range)
{
    return CSSPropertyParserHelpers::consumeIdent<CSSValueAuto, CSSValueBlock, CSSValueSwap, CSSValueFallback, CSSValueOptional>(range);
}

#if ENABLE(VARIATION_FONTS)

RefPtr<CSSValue> consumeVariationTagValue(CSSParserTokenRange& range)
{
    // https://w3c.github.io/csswg-drafts/css-fonts/#font-variation-settings-def

    auto tag = consumeFontTag(range);
    if (!tag)
        return nullptr;
    
    auto tagValue = CSSPropertyParserHelpers::consumeNumberRaw(range);
    if (!tagValue)
        return nullptr;
    
    return CSSFontVariationValue::create(WTFMove(*tag), tagValue->value);
}

#endif // ENABLE(VARIATION_FONTS)

} // namespace CSSPropertyParserHelpersWorkerSafe

} // namespace WebCore
