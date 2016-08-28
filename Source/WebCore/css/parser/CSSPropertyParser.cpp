// Copyright 2015 The Chromium Authors. All rights reserved.
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

#include "config.h"
#include "CSSPropertyParser.h"

#include "CSSBasicShapes.h"
#include "CSSBorderImage.h"
#include "CSSContentDistributionValue.h"
#include "CSSCursorImageValue.h"
// FIXME-NEWPARSER #include "CSSCustomIdentValue.h"
#include "CSSFontFaceSrcValue.h"
// FIXME-NEWPARSER #include "CSSFontFamilyValue.h"
#include "CSSFontFeatureValue.h"
#include "CSSFunctionValue.h"
#include "CSSGridAutoRepeatValue.h"
#include "CSSGridLineNamesValue.h"
#include "CSSGridTemplateAreasValue.h"
#include "CSSInheritedValue.h"
#include "CSSInitialValue.h"
#include "CSSParserFastPaths.h"
#include "CSSParserIdioms.h"
// FIXME-NEWPARSER #include "CSSPathValue.h"
// FIXME-NEWPARSER #include "CSSPendingSubstitutionValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyParserHelpers.h"
// FIXME-NEWPARSER #include "CSSQuadValue.h"
#include "CSSReflectValue.h"
#include "CSSShadowValue.h"
// FIXME-NEWPARSER #include "CSSStringValue.h"
#include "CSSTimingFunctionValue.h"
// FIXME-NEWPARSER #include "CSSURIValue.h"
#include "CSSUnicodeRangeValue.h"
#include "CSSUnsetValue.h"
// FIXME-NEWPARSER #include "CSSValuePair.h"
// FIXME-NEWPARSER #include "CSSVariableReferenceValue.h"
// FIXME-NEWPARSER #include "CSSVariableParser.h"
#include "Counter.h"
#include "FontFace.h"
#include "HashTools.h"
#include "RenderTheme.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGPathUtilities.h"
#include "StylePropertyShorthand.h"
#include <memory>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static bool hasPrefix(const char* string, unsigned length, const char* prefix)
{
    for (unsigned i = 0; i < length; ++i) {
        if (!prefix[i])
            return true;
        if (string[i] != prefix[i])
            return false;
    }
    return false;
}

#if PLATFORM(IOS)
static void cssPropertyNameIOSAliasing(const char* propertyName, const char*& propertyNameAlias, unsigned& newLength)
{
    if (!strcmp(propertyName, "-webkit-hyphenate-locale")) {
        // Worked in iOS 4.2.
        static const char webkitLocale[] = "-webkit-locale";
        propertyNameAlias = webkitLocale;
        newLength = strlen(webkitLocale);
    }
}
#endif

template <typename CharacterType>
static CSSPropertyID cssPropertyID(const CharacterType* propertyName, unsigned length)
{
    char buffer[maxCSSPropertyNameLength + 1 + 1]; // 1 to turn "apple"/"khtml" into "webkit", 1 for null character
    
    for (unsigned i = 0; i != length; ++i) {
        CharacterType c = propertyName[i];
        if (!c || c >= 0x7F)
            return CSSPropertyInvalid; // illegal character
        buffer[i] = toASCIILower(c);
    }
    buffer[length] = '\0';
    
    const char* name = buffer;
    if (buffer[0] == '-') {
#if ENABLE(LEGACY_CSS_VENDOR_PREFIXES)
        // If the prefix is -apple- or -khtml-, change it to -webkit-.
        // This makes the string one character longer.
        if (RuntimeEnabledFeatures::sharedFeatures().legacyCSSVendorPrefixesEnabled()
            && (hasPrefix(buffer, length, "-apple-") || hasPrefix(buffer, length, "-khtml-"))) {
            memmove(buffer + 7, buffer + 6, length + 1 - 6);
            memcpy(buffer, "-webkit", 7);
            ++length;
        }
#endif
#if PLATFORM(IOS)
        cssPropertyNameIOSAliasing(buffer, name, length);
#endif
    }
    
    const Property* hashTableEntry = findProperty(name, length);
    return hashTableEntry ? static_cast<CSSPropertyID>(hashTableEntry->id) : CSSPropertyInvalid;
}

static bool isAppleLegacyCssValueKeyword(const char* valueKeyword, unsigned length)
{
    static const char applePrefix[] = "-apple-";
    static const char appleSystemPrefix[] = "-apple-system";
    static const char* appleWirelessPlaybackTargetActive = getValueName(CSSValueAppleWirelessPlaybackTargetActive);
    
    return hasPrefix(valueKeyword, length, applePrefix)
    && !hasPrefix(valueKeyword, length, appleSystemPrefix)
    && !WTF::equal(reinterpret_cast<const LChar*>(valueKeyword), reinterpret_cast<const LChar*>(appleWirelessPlaybackTargetActive), length);
}

template <typename CharacterType>
static CSSValueID cssValueKeywordID(const CharacterType* valueKeyword, unsigned length)
{
    char buffer[maxCSSValueKeywordLength + 1 + 1]; // 1 to turn "apple"/"khtml" into "webkit", 1 for null character
    
    for (unsigned i = 0; i != length; ++i) {
        CharacterType c = valueKeyword[i];
        if (!c || c >= 0x7F)
            return CSSValueInvalid; // illegal keyword.
        buffer[i] = WTF::toASCIILower(c);
    }
    buffer[length] = '\0';
    
    if (buffer[0] == '-') {
        // If the prefix is -apple- or -khtml-, change it to -webkit-.
        // This makes the string one character longer.
        // On iOS we don't want to change values starting with -apple-system to -webkit-system.
        // FIXME: Remove this mangling without breaking the web.
        if (isAppleLegacyCssValueKeyword(buffer, length) || hasPrefix(buffer, length, "-khtml-")) {
            memmove(buffer + 7, buffer + 6, length + 1 - 6);
            memcpy(buffer, "-webkit", 7);
            ++length;
        }
    }
    
    const Value* hashTableEntry = findValue(buffer, length);
    return hashTableEntry ? static_cast<CSSValueID>(hashTableEntry->id) : CSSValueInvalid;
}

CSSValueID cssValueKeywordID(StringView string)
{
    unsigned length = string.length();
    if (!length)
        return CSSValueInvalid;
    if (length > maxCSSValueKeywordLength)
        return CSSValueInvalid;
    
    return string.is8Bit() ? cssValueKeywordID(string.characters8(), length) : cssValueKeywordID(string.characters16(), length);
}

CSSPropertyID unresolvedCSSPropertyID(StringView string)
{
    unsigned length = string.length();
    
    if (!length)
        return CSSPropertyInvalid;
    if (length > maxCSSPropertyNameLength)
        return CSSPropertyInvalid;
    
    return string.is8Bit() ? cssPropertyID(string.characters8(), length) : cssPropertyID(string.characters16(), length);
}
    
// FIXME-NEWPARSER
// Comment out property parsing for now.
/*
using namespace CSSPropertyParserHelpers;


CSSPropertyParser::CSSPropertyParser(const CSSParserTokenRange& range,
    const CSSParserContext& context, Vector<CSSProperty, 256>* parsedProperties)
    : m_range(range)
    , m_context(context)
    , m_parsedProperties(parsedProperties)
{
    m_range.consumeWhitespace();
}

void CSSPropertyParser::addProperty(CSSPropertyID property, CSSPropertyID currentShorthand, const CSSValue& value, bool important, bool implicit)
{
    ASSERT(!isPropertyAlias(property));

    int shorthandIndex = 0;
    bool setFromShorthand = false;

    if (currentShorthand) {
        Vector<StylePropertyShorthand, 4> shorthands;
        getMatchingShorthandsForLonghand(property, &shorthands);
        setFromShorthand = true;
        if (shorthands.size() > 1)
            shorthandIndex = indexOfShorthandForLonghand(currentShorthand, shorthands);
    }

    m_parsedProperties->append(CSSProperty(property, value, important, setFromShorthand, shorthandIndex, implicit));
}

void CSSPropertyParser::addExpandedPropertyForValue(CSSPropertyID property, const CSSValue& value, bool important)
{
    const StylePropertyShorthand& shorthand = shorthandForProperty(property);
    unsigned shorthandLength = shorthand.length();
    ASSERT(shorthandLength);
    const CSSPropertyID* longhands = shorthand.properties();
    for (unsigned i = 0; i < shorthandLength; ++i)
        addProperty(longhands[i], property, value, important);
}

bool CSSPropertyParser::parseValue(CSSPropertyID unresolvedProperty, bool important,
    const CSSParserTokenRange& range, const CSSParserContext& context,
    Vector<CSSProperty, 256>& parsedProperties, StyleRule::Type ruleType)
{
    int parsedPropertiesSize = parsedProperties.size();

    CSSPropertyParser parser(range, context, &parsedProperties);
    CSSPropertyID resolvedProperty = resolveCSSPropertyID(unresolvedProperty);
    bool parseSuccess;

    if (ruleType == StyleRule::Viewport) {
        parseSuccess = (RuntimeEnabledFeatures::cssViewportEnabled() || isUASheetBehavior(context.mode()))
            && parser.parseViewportDescriptor(resolvedProperty, important);
    } else if (ruleType == StyleRule::FontFace) {
        parseSuccess = parser.parseFontFaceDescriptor(resolvedProperty);
    } else {
        parseSuccess = parser.parseValueStart(unresolvedProperty, important);
    }

    // This doesn't count UA style sheets
    if (parseSuccess && context.useCounter())
        context.useCounter()->count(context.mode(), unresolvedProperty);

    if (!parseSuccess)
        parsedProperties.shrink(parsedPropertiesSize);

    return parseSuccess;
}

const CSSValue* CSSPropertyParser::parseSingleValue(
    CSSPropertyID property, const CSSParserTokenRange& range, const CSSParserContext& context)
{
    CSSPropertyParser parser(range, context, nullptr);
    const CSSValue* value = parser.parseSingleValue(property);
    if (!value || !parser.m_range.atEnd())
        return nullptr;
    return value;
}

bool CSSPropertyParser::parseValueStart(CSSPropertyID unresolvedProperty, bool important)
{
    if (consumeCSSWideKeyword(unresolvedProperty, important))
        return true;

    CSSParserTokenRange originalRange = m_range;
    CSSPropertyID propertyId = resolveCSSPropertyID(unresolvedProperty);
    bool isShorthand = isShorthandProperty(propertyId);

    if (isShorthand) {
        // Variable references will fail to parse here and will fall out to the variable ref parser below.
        if (parseShorthand(unresolvedProperty, important))
            return true;
    } else {
        if (const CSSValue* parsedValue = parseSingleValue(unresolvedProperty)) {
            if (m_range.atEnd()) {
                addProperty(propertyId, CSSPropertyInvalid, *parsedValue, important);
                return true;
            }
        }
    }

    if (RuntimeEnabledFeatures::cssVariablesEnabled() && CSSVariableParser::containsValidVariableReferences(originalRange)) {
        CSSVariableReferenceValue* variable = CSSVariableReferenceValue::create(CSSVariableData::create(originalRange));

        if (isShorthand) {
            const CSSPendingSubstitutionValue& pendingValue = *CSSPendingSubstitutionValue::create(propertyId, variable);
            addExpandedPropertyForValue(propertyId, pendingValue, important);
        } else {
            addProperty(propertyId, CSSPropertyInvalid, *variable, important);
        }
        return true;
    }

    return false;
}
 
bool CSSPropertyParser::consumeCSSWideKeyword(CSSPropertyID unresolvedProperty, bool important)
{
    CSSParserTokenRange rangeCopy = m_range;
    CSSValueID id = rangeCopy.consumeIncludingWhitespace().id();
    if (!rangeCopy.atEnd())
        return false;

    CSSValue* value = nullptr;
    if (id == CSSValueInitial)
        value = CSSInitialValue::create();
    else if (id == CSSValueInherit)
        value = CSSInheritedValue::create();
    else if (id == CSSValueUnset)
        value = CSSUnsetValue::create();
    else
        return false;

    CSSPropertyID property = resolveCSSPropertyID(unresolvedProperty);
    const StylePropertyShorthand& shorthand = shorthandForProperty(property);
    if (!shorthand.length()) {
        if (CSSPropertyMetadata::isDescriptorOnly(unresolvedProperty))
            return false;
        addProperty(property, CSSPropertyInvalid, *value, important);
    } else {
        addExpandedPropertyForValue(property, *value, important);
    }
    m_range = rangeCopy;
    return true;
}

static CSSValueList* consumeTransformOrigin(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless)
{
    CSSValue* resultX = nullptr;
    CSSValue* resultY = nullptr;
    if (consumeOneOrTwoValuedPosition(range, cssParserMode, unitless, resultX, resultY)) {
        CSSValueList* list = CSSValueList::createSpaceSeparated();
        list->append(*resultX);
        list->append(*resultY);
        CSSValue* resultZ = consumeLength(range, cssParserMode, ValueRangeAll);
        if (!resultZ)
            resultZ = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Pixels);
        list->append(*resultZ);
        return list;
    }
    return nullptr;
}

// Methods for consuming non-shorthand properties starts here.
static CSSValue* consumeWillChange(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);

    CSSValueList* values = CSSValueList::createCommaSeparated();
    // Every comma-separated list of identifiers is a valid will-change value,
    // unless the list includes an explicitly disallowed identifier.
    while (true) {
        if (range.peek().type() != IdentToken)
            return nullptr;
        CSSPropertyID unresolvedProperty = unresolvedCSSPropertyID(range.peek().value());
        if (unresolvedProperty) {
            ASSERT(CSSPropertyMetadata::isEnabledProperty(unresolvedProperty));
            // Now "all" is used by both CSSValue and CSSPropertyValue.
            // Need to return nullptr when currentValue is CSSPropertyAll.
            if (unresolvedProperty == CSSPropertyWillChange || unresolvedProperty == CSSPropertyAll)
                return nullptr;
            values->append(*CSSCustomIdentValue::create(unresolvedProperty));
            range.consumeIncludingWhitespace();
        } else {
            switch (range.peek().id()) {
            case CSSValueNone:
            case CSSValueAll:
            case CSSValueAuto:
            case CSSValueDefault:
            case CSSValueInitial:
            case CSSValueInherit:
                return nullptr;
            case CSSValueContents:
            case CSSValueScrollPosition:
                values->append(*consumeIdent(range));
                break;
            default:
                range.consumeIncludingWhitespace();
                break;
            }
        }

        if (range.atEnd())
            break;
        if (!consumeCommaIncludingWhitespace(range))
            return nullptr;
    }

    return values;
}

static CSSFontFeatureValue* consumeFontFeatureTag(CSSParserTokenRange& range)
{
    // Feature tag name consists of 4-letter characters.
    static const unsigned tagNameLength = 4;

    const CSSParserToken& token = range.consumeIncludingWhitespace();
    // Feature tag name comes first
    if (token.type() != StringToken)
        return nullptr;
    if (token.value().length() != tagNameLength)
        return nullptr;
    AtomicString tag = token.value().toAtomicString();
    for (unsigned i = 0; i < tagNameLength; ++i) {
        // Limits the range of characters to 0x20-0x7E, following the tag name rules defiend in the OpenType specification.
        UChar character = tag[i];
        if (character < 0x20 || character > 0x7E)
            return nullptr;
    }

    int tagValue = 1;
    // Feature tag values could follow: <integer> | on | off
    if (range.peek().type() == NumberToken && range.peek().numericValueType() == IntegerValueType && range.peek().numericValue() >= 0) {
        tagValue = clampTo<int>(range.consumeIncludingWhitespace().numericValue());
        if (tagValue < 0)
            return nullptr;
    } else if (range.peek().id() == CSSValueOn || range.peek().id() == CSSValueOff) {
        tagValue = range.consumeIncludingWhitespace().id() == CSSValueOn;
    }
    return CSSFontFeatureValue::create(tag, tagValue);
}

static CSSValue* consumeFontFeatureSettings(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);
    CSSValueList* settings = CSSValueList::createCommaSeparated();
    do {
        CSSFontFeatureValue* fontFeatureValue = consumeFontFeatureTag(range);
        if (!fontFeatureValue)
            return nullptr;
        settings->append(*fontFeatureValue);
    } while (consumeCommaIncludingWhitespace(range));
    return settings;
}

static CSSValue* consumePage(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeCustomIdent(range);
}

static CSSValue* consumeQuotes(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    CSSValueList* values = CSSValueList::createSpaceSeparated();
    while (!range.atEnd()) {
        CSSStringValue* parsedValue = consumeString(range);
        if (!parsedValue)
            return nullptr;
        values->append(*parsedValue);
    }
    if (values->length() && values->length() % 2 == 0)
        return values;
    return nullptr;
}

static CSSValue* consumeWebkitHighlight(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    return consumeString(range);
}

class FontVariantLigaturesParser {
    STACK_ALLOCATED();

public:
    FontVariantLigaturesParser()
        : m_sawCommonLigaturesValue(false)
        , m_sawDiscretionaryLigaturesValue(false)
        , m_sawHistoricalLigaturesValue(false)
        , m_sawContextualLigaturesValue(false)
        , m_result(CSSValueList::createSpaceSeparated())
    {
    }

    enum class ParseResult {
        ConsumedValue,
        DisallowedValue,
        UnknownValue
    };

    ParseResult consumeLigature(CSSParserTokenRange& range)
    {
        CSSValueID valueID = range.peek().id();
        switch (valueID) {
        case CSSValueNoCommonLigatures:
        case CSSValueCommonLigatures:
            if (m_sawCommonLigaturesValue)
                return ParseResult::DisallowedValue;
            m_sawCommonLigaturesValue = true;
            break;
        case CSSValueNoDiscretionaryLigatures:
        case CSSValueDiscretionaryLigatures:
            if (m_sawDiscretionaryLigaturesValue)
                return ParseResult::DisallowedValue;
            m_sawDiscretionaryLigaturesValue = true;
            break;
        case CSSValueNoHistoricalLigatures:
        case CSSValueHistoricalLigatures:
            if (m_sawHistoricalLigaturesValue)
                return ParseResult::DisallowedValue;
            m_sawHistoricalLigaturesValue = true;
            break;
        case CSSValueNoContextual:
        case CSSValueContextual:
            if (m_sawContextualLigaturesValue)
                return ParseResult::DisallowedValue;
            m_sawContextualLigaturesValue = true;
            break;
        default:
            return ParseResult::UnknownValue;
        }
        m_result->append(*consumeIdent(range));
        return ParseResult::ConsumedValue;
    }

    CSSValue* finalizeValue()
    {
        if (!m_result->length())
            return CSSPrimitiveValue::createIdentifier(CSSValueNormal);
        return m_result.release();
    }

private:
    bool m_sawCommonLigaturesValue;
    bool m_sawDiscretionaryLigaturesValue;
    bool m_sawHistoricalLigaturesValue;
    bool m_sawContextualLigaturesValue;
    Member<CSSValueList> m_result;
};

static CSSValue* consumeFontVariantLigatures(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNormal || range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    FontVariantLigaturesParser ligaturesParser;
    do {
        if (ligaturesParser.consumeLigature(range) !=
            FontVariantLigaturesParser::ParseResult::ConsumedValue)
            return nullptr;
    } while (!range.atEnd());

    return ligaturesParser.finalizeValue();
}

static CSSPrimitiveValue* consumeFontVariantCaps(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueNormal, CSSValueSmallCaps, CSSValueAllSmallCaps,
        CSSValuePetiteCaps, CSSValueAllPetiteCaps,
        CSSValueUnicase, CSSValueTitlingCaps>(range);
}

class FontVariantNumericParser {
    STACK_ALLOCATED();

public:
    FontVariantNumericParser()
        : m_sawNumericFigureValue(false)
        , m_sawNumericSpacingValue(false)
        , m_sawNumericFractionValue(false)
        , m_sawOrdinalValue(false)
        , m_sawSlashedZeroValue(false)
        , m_result(CSSValueList::createSpaceSeparated())
    {
    }

    enum class ParseResult {
        ConsumedValue,
        DisallowedValue,
        UnknownValue
    };

    ParseResult consumeNumeric(CSSParserTokenRange& range)
    {
        CSSValueID valueID = range.peek().id();
        switch (valueID) {
        case CSSValueLiningNums:
        case CSSValueOldstyleNums:
            if (m_sawNumericFigureValue)
                return ParseResult::DisallowedValue;
            m_sawNumericFigureValue = true;
            break;
        case CSSValueProportionalNums:
        case CSSValueTabularNums:
            if (m_sawNumericSpacingValue)
                return ParseResult::DisallowedValue;
            m_sawNumericSpacingValue = true;
            break;
        case CSSValueDiagonalFractions:
        case CSSValueStackedFractions:
            if (m_sawNumericFractionValue)
                return ParseResult::DisallowedValue;
            m_sawNumericFractionValue = true;
            break;
        case CSSValueOrdinal:
            if (m_sawOrdinalValue)
                return ParseResult::DisallowedValue;
            m_sawOrdinalValue = true;
            break;
        case CSSValueSlashedZero:
            if (m_sawSlashedZeroValue)
                return ParseResult::DisallowedValue;
            m_sawSlashedZeroValue = true;
            break;
        default:
            return ParseResult::UnknownValue;
        }
        m_result->append(*consumeIdent(range));
        return ParseResult::ConsumedValue;
    }

    CSSValue* finalizeValue()
    {
        if (!m_result->length())
            return CSSPrimitiveValue::createIdentifier(CSSValueNormal);
        return m_result.release();
    }


private:
    bool m_sawNumericFigureValue;
    bool m_sawNumericSpacingValue;
    bool m_sawNumericFractionValue;
    bool m_sawOrdinalValue;
    bool m_sawSlashedZeroValue;
    Member<CSSValueList> m_result;
};

static CSSValue* consumeFontVariantNumeric(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);

    FontVariantNumericParser numericParser;
    do {
        if (numericParser.consumeNumeric(range) !=
            FontVariantNumericParser::ParseResult::ConsumedValue)
            return nullptr;
    } while (!range.atEnd());

    return numericParser.finalizeValue();
}

static CSSPrimitiveValue* consumeFontVariantCSS21(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueNormal, CSSValueSmallCaps>(range);
}

static CSSValue* consumeFontVariantList(CSSParserTokenRange& range)
{
    CSSValueList* values = CSSValueList::createCommaSeparated();
    do {
        if (range.peek().id() == CSSValueAll) {
            // FIXME: CSSPropertyParser::parseFontVariant() implements
            // the old css3 draft:
            // http://www.w3.org/TR/2002/WD-css3-webfonts-20020802/#font-variant
            // 'all' is only allowed in @font-face and with no other values.
            if (values->length())
                return nullptr;
            return consumeIdent(range);
        }
        CSSPrimitiveValue* fontVariant = consumeFontVariantCSS21(range);
        if (fontVariant)
            values->append(*fontVariant);
    } while (consumeCommaIncludingWhitespace(range));

    if (values->length())
        return values;

    return nullptr;
}

static CSSPrimitiveValue* consumeFontWeight(CSSParserTokenRange& range)
{
    const CSSParserToken& token = range.peek();
    if (token.id() >= CSSValueNormal && token.id() <= CSSValueLighter)
        return consumeIdent(range);
    if (token.type() != NumberToken || token.numericValueType() != IntegerValueType)
        return nullptr;
    int weight = static_cast<int>(token.numericValue());
    if ((weight % 100) || weight < 100 || weight > 900)
        return nullptr;
    range.consumeIncludingWhitespace();
    return CSSPrimitiveValue::createIdentifier(static_cast<CSSValueID>(CSSValue100 + weight / 100 - 1));
}

static String concatenateFamilyName(CSSParserTokenRange& range)
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
    if (!addedSpace && isCSSWideKeyword(firstToken.id()))
        return String();
    return builder.toString();
}

static CSSValue* consumeFamilyName(CSSParserTokenRange& range)
{
    if (range.peek().type() == StringToken)
        return CSSFontFamilyValue::create(range.consumeIncludingWhitespace().value().toString());
    if (range.peek().type() != IdentToken)
        return nullptr;
    String familyName = concatenateFamilyName(range);
    if (familyName.isNull())
        return nullptr;
    return CSSFontFamilyValue::create(familyName);
}

static CSSValue* consumeGenericFamily(CSSParserTokenRange& range)
{
    return consumeIdentRange(range, CSSValueSerif, CSSValueWebkitBody);
}

static CSSValueList* consumeFontFamily(CSSParserTokenRange& range)
{
    CSSValueList* list = CSSValueList::createCommaSeparated();
    do {
        CSSValue* parsedValue = consumeGenericFamily(range);
        if (parsedValue) {
            list->append(*parsedValue);
        } else {
            parsedValue = consumeFamilyName(range);
            if (parsedValue) {
                list->append(*parsedValue);
            } else {
                return nullptr;
            }
        }
    } while (consumeCommaIncludingWhitespace(range));
    return list;
}

static CSSValue* consumeSpacing(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);
    // FIXME: allow <percentage>s in word-spacing.
    return consumeLength(range, cssParserMode, ValueRangeAll, UnitlessQuirk::Allow);
}

static CSSValue* consumeTabSize(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    CSSPrimitiveValue* parsedValue = consumeInteger(range, 0);
    if (parsedValue)
        return parsedValue;
    return consumeLength(range, cssParserMode, ValueRangeNonNegative);
}

static CSSValue* consumeTextSizeAdjust(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    return consumePercent(range, ValueRangeNonNegative);
}

static CSSValue* consumeFontSize(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless = UnitlessQuirk::Forbid)
{
    if (range.peek().id() >= CSSValueXxSmall && range.peek().id() <= CSSValueLarger)
        return consumeIdent(range);
    return consumeLengthOrPercent(range, cssParserMode, ValueRangeNonNegative, unitless);
}

static CSSPrimitiveValue* consumeLineHeight(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);

    CSSPrimitiveValue* lineHeight = consumeNumber(range, ValueRangeNonNegative);
    if (lineHeight)
        return lineHeight;
    return consumeLengthOrPercent(range, cssParserMode, ValueRangeNonNegative);
}

static CSSValueList* consumeRotation(CSSParserTokenRange& range)
{
    ASSERT(RuntimeEnabledFeatures::cssIndependentTransformPropertiesEnabled());
    CSSValueList* list = CSSValueList::createSpaceSeparated();

    CSSValue* rotation = consumeAngle(range);
    if (!rotation)
        return nullptr;
    list->append(*rotation);

    if (range.atEnd())
        return list;

    for (unsigned i = 0; i < 3; i++) { // 3 dimensions of rotation
        CSSValue* dimension = consumeNumber(range, ValueRangeAll);
        if (!dimension)
            return nullptr;
        list->append(*dimension);
    }

    return list;
}

static CSSValueList* consumeScale(CSSParserTokenRange& range)
{
    ASSERT(RuntimeEnabledFeatures::cssIndependentTransformPropertiesEnabled());

    CSSValue* scale = consumeNumber(range, ValueRangeAll);
    if (!scale)
        return nullptr;
    CSSValueList* list = CSSValueList::createSpaceSeparated();
    list->append(*scale);
    scale = consumeNumber(range, ValueRangeAll);
    if (scale) {
        list->append(*scale);
        scale = consumeNumber(range, ValueRangeAll);
        if (scale)
            list->append(*scale);
    }

    return list;
}

static CSSValueList* consumeTranslate(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    ASSERT(RuntimeEnabledFeatures::cssIndependentTransformPropertiesEnabled());
    CSSValue* translate = consumeLengthOrPercent(range, cssParserMode, ValueRangeAll);
    if (!translate)
        return nullptr;
    CSSValueList* list = CSSValueList::createSpaceSeparated();
    list->append(*translate);
    translate = consumeLengthOrPercent(range, cssParserMode, ValueRangeAll);
    if (translate) {
        list->append(*translate);
        translate = consumeLength(range, cssParserMode, ValueRangeAll);
        if (translate)
            list->append(*translate);
    }

    return list;
}

static CSSValue* consumeCounter(CSSParserTokenRange& range, int defaultValue)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    CSSValueList* list = CSSValueList::createSpaceSeparated();
    do {
        CSSCustomIdentValue* counterName = consumeCustomIdent(range);
        if (!counterName)
            return nullptr;
        int i = defaultValue;
        if (CSSPrimitiveValue* counterValue = consumeInteger(range))
            i = clampTo<int>(counterValue->getDoubleValue());
        list->append(*CSSValuePair::create(counterName,
            CSSPrimitiveValue::create(i, CSSPrimitiveValue::UnitType::Integer),
            CSSValuePair::DropIdenticalValues));
    } while (!range.atEnd());
    return list;
}

static CSSValue* consumePageSize(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueA3, CSSValueA4, CSSValueA5, CSSValueB4, CSSValueB5, CSSValueLedger, CSSValueLegal, CSSValueLetter>(range);
}

static CSSValueList* consumeSize(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    CSSValueList* result = CSSValueList::createSpaceSeparated();

    if (range.peek().id() == CSSValueAuto) {
        result->append(*consumeIdent(range));
        return result;
    }

    if (CSSValue* width = consumeLength(range, cssParserMode, ValueRangeNonNegative)) {
        CSSValue* height = consumeLength(range, cssParserMode, ValueRangeNonNegative);
        result->append(*width);
        if (height)
            result->append(*height);
        return result;
    }

    CSSValue* pageSize = consumePageSize(range);
    CSSValue* orientation = consumeIdent<CSSValuePortrait, CSSValueLandscape>(range);
    if (!pageSize)
        pageSize = consumePageSize(range);

    if (!orientation && !pageSize)
        return nullptr;
    if (pageSize)
        result->append(*pageSize);
    if (orientation)
        result->append(*orientation);
    return result;
}

static CSSValue* consumeSnapHeight(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    CSSPrimitiveValue* unit = consumeLength(range, cssParserMode, ValueRangeNonNegative);
    if (!unit)
        return nullptr;
    CSSValueList* list = CSSValueList::createSpaceSeparated();
    list->append(*unit);

    if (CSSPrimitiveValue* position = consumePositiveInteger(range)) {
        if (position->getIntValue() > 100)
            return nullptr;
        list->append(*position);
    }

    return list;
}

static CSSValue* consumeTextIndent(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    // [ <length> | <percentage> ] && hanging? && each-line?
    // Keywords only allowed when css3Text is enabled.
    CSSValueList* list = CSSValueList::createSpaceSeparated();

    bool hasLengthOrPercentage = false;
    bool hasEachLine = false;
    bool hasHanging = false;

    do {
        if (!hasLengthOrPercentage) {
            if (CSSValue* textIndent = consumeLengthOrPercent(range, cssParserMode, ValueRangeAll, UnitlessQuirk::Allow)) {
                list->append(*textIndent);
                hasLengthOrPercentage = true;
                continue;
            }
        }

        if (RuntimeEnabledFeatures::css3TextEnabled()) {
            CSSValueID id = range.peek().id();
            if (!hasEachLine && id == CSSValueEachLine) {
                list->append(*consumeIdent(range));
                hasEachLine = true;
                continue;
            }
            if (!hasHanging && id == CSSValueHanging) {
                list->append(*consumeIdent(range));
                hasHanging = true;
                continue;
            }
        }
        return nullptr;
    } while (!range.atEnd());

    if (!hasLengthOrPercentage)
        return nullptr;

    return list;
}

static bool validWidthOrHeightKeyword(CSSValueID id, const CSSParserContext& context)
{
    if (id == CSSValueWebkitMinContent || id == CSSValueWebkitMaxContent || id == CSSValueWebkitFillAvailable || id == CSSValueWebkitFitContent
        || id == CSSValueMinContent || id == CSSValueMaxContent || id == CSSValueFitContent) {
        if (context.useCounter()) {
            switch (id) {
            case CSSValueWebkitMinContent:
                context.useCounter()->count(UseCounter::CSSValuePrefixedMinContent);
                break;
            case CSSValueWebkitMaxContent:
                context.useCounter()->count(UseCounter::CSSValuePrefixedMaxContent);
                break;
            case CSSValueWebkitFillAvailable:
                context.useCounter()->count(UseCounter::CSSValuePrefixedFillAvailable);
                break;
            case CSSValueWebkitFitContent:
                context.useCounter()->count(UseCounter::CSSValuePrefixedFitContent);
                break;
            default:
                break;
            }
        }
        return true;
    }
    return false;
}

static CSSValue* consumeMaxWidthOrHeight(CSSParserTokenRange& range, const CSSParserContext& context, UnitlessQuirk unitless = UnitlessQuirk::Forbid)
{
    if (range.peek().id() == CSSValueNone || validWidthOrHeightKeyword(range.peek().id(), context))
        return consumeIdent(range);
    return consumeLengthOrPercent(range, context.mode(), ValueRangeNonNegative, unitless);
}

static CSSValue* consumeWidthOrHeight(CSSParserTokenRange& range, const CSSParserContext& context, UnitlessQuirk unitless = UnitlessQuirk::Forbid)
{
    if (range.peek().id() == CSSValueAuto || validWidthOrHeightKeyword(range.peek().id(), context))
        return consumeIdent(range);
    return consumeLengthOrPercent(range, context.mode(), ValueRangeNonNegative, unitless);
}

static CSSValue* consumeMarginOrOffset(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeLengthOrPercent(range, cssParserMode, ValueRangeAll, unitless);
}

static CSSPrimitiveValue* consumeClipComponent(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeLength(range, cssParserMode, ValueRangeAll, UnitlessQuirk::Allow);
}

static CSSValue* consumeClip(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);

    if (range.peek().functionId() != CSSValueRect)
        return nullptr;

    CSSParserTokenRange args = consumeFunction(range);
    // rect(t, r, b, l) || rect(t r b l)
    CSSPrimitiveValue* top = consumeClipComponent(args, cssParserMode);
    if (!top)
        return nullptr;
    bool needsComma = consumeCommaIncludingWhitespace(args);
    CSSPrimitiveValue* right = consumeClipComponent(args, cssParserMode);
    if (!right || (needsComma && !consumeCommaIncludingWhitespace(args)))
        return nullptr;
    CSSPrimitiveValue* bottom = consumeClipComponent(args, cssParserMode);
    if (!bottom || (needsComma && !consumeCommaIncludingWhitespace(args)))
        return nullptr;
    CSSPrimitiveValue* left = consumeClipComponent(args, cssParserMode);
    if (!left || !args.atEnd())
        return nullptr;
    return CSSQuadValue::create(top, right, bottom, left, CSSQuadValue::SerializeAsRect);
}

static bool consumePan(CSSParserTokenRange& range, CSSValue*& panX, CSSValue*& panY)
{
    CSSValueID id = range.peek().id();
    if ((id == CSSValuePanX || id == CSSValuePanRight || id == CSSValuePanLeft) && !panX) {
        if (id != CSSValuePanX && !RuntimeEnabledFeatures::cssTouchActionPanDirectionsEnabled())
            return false;
        panX = consumeIdent(range);
    } else if ((id == CSSValuePanY || id == CSSValuePanDown || id == CSSValuePanUp) && !panY) {
        if (id != CSSValuePanY && !RuntimeEnabledFeatures::cssTouchActionPanDirectionsEnabled())
            return false;
        panY = consumeIdent(range);
    } else {
        return false;
    }
    return true;
}

static CSSValue* consumeTouchAction(CSSParserTokenRange& range)
{
    CSSValueList* list = CSSValueList::createSpaceSeparated();
    CSSValueID id = range.peek().id();
    if (id == CSSValueAuto || id == CSSValueNone || id == CSSValueManipulation) {
        list->append(*consumeIdent(range));
        return list;
    }

    CSSValue* panX = nullptr;
    CSSValue* panY = nullptr;
    if (!consumePan(range, panX, panY))
        return nullptr;
    if (!range.atEnd() && !consumePan(range, panX, panY))
        return nullptr;

    if (panX)
        list->append(*panX);
    if (panY)
        list->append(*panY);
    return list;
}

static CSSPrimitiveValue* consumeLineClamp(CSSParserTokenRange& range)
{
    if (range.peek().type() != PercentageToken && range.peek().type() != NumberToken)
        return nullptr;
    CSSPrimitiveValue* clampValue = consumePercent(range, ValueRangeNonNegative);
    if (clampValue)
        return clampValue;
    // When specifying number of lines, don't allow 0 as a valid value.
    return consumePositiveInteger(range);
}

static CSSValue* consumeLocale(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeString(range);
}

static CSSValue* consumeColumnWidth(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    // Always parse lengths in strict mode here, since it would be ambiguous otherwise when used in
    // the 'columns' shorthand property.
    CSSPrimitiveValue* columnWidth = consumeLength(range, HTMLStandardMode, ValueRangeNonNegative);
    if (!columnWidth || (!columnWidth->isCalculated() && columnWidth->getDoubleValue() == 0))
        return nullptr;
    return columnWidth;
}

static CSSValue* consumeColumnCount(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumePositiveInteger(range);
}

static CSSValue* consumeColumnGap(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);
    return consumeLength(range, cssParserMode, ValueRangeNonNegative);
}

static CSSValue* consumeColumnSpan(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueAll, CSSValueNone>(range);
}

static CSSValue* consumeZoom(CSSParserTokenRange& range, const CSSParserContext& context)
{
    const CSSParserToken& token = range.peek();
    CSSPrimitiveValue* zoom = nullptr;
    if (token.type() == IdentToken) {
        zoom = consumeIdent<CSSValueNormal, CSSValueReset, CSSValueDocument>(range);
    } else {
        zoom = consumePercent(range, ValueRangeNonNegative);
        if (!zoom)
            zoom = consumeNumber(range, ValueRangeNonNegative);
    }
    if (zoom && context.useCounter()
        && !(token.id() == CSSValueNormal
            || (token.type() == NumberToken && zoom->getDoubleValue() == 1)
            || (token.type() == PercentageToken && zoom->getDoubleValue() == 100)))
        context.useCounter()->count(UseCounter::CSSZoomNotEqualToOne);
    return zoom;
}

static CSSValue* consumeAnimationIterationCount(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueInfinite)
        return consumeIdent(range);
    return consumeNumber(range, ValueRangeNonNegative);
}

static CSSValue* consumeAnimationName(CSSParserTokenRange& range, const CSSParserContext& context, bool allowQuotedName)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    if (allowQuotedName && range.peek().type() == StringToken) {
        // Legacy support for strings in prefixed animations.
        if (context.useCounter())
            context.useCounter()->count(UseCounter::QuotedAnimationName);

        const CSSParserToken& token = range.consumeIncludingWhitespace();
        if (equalIgnoringASCIICase(token.value(), "none"))
            return CSSPrimitiveValue::createIdentifier(CSSValueNone);
        return CSSCustomIdentValue::create(token.value().toString());
    }

    return consumeCustomIdent(range);
}

static CSSValue* consumeTransitionProperty(CSSParserTokenRange& range)
{
    const CSSParserToken& token = range.peek();
    if (token.type() != IdentToken)
        return nullptr;
    if (token.id() == CSSValueNone)
        return consumeIdent(range);

    if (CSSPropertyID property = token.parseAsUnresolvedCSSPropertyID()) {
        ASSERT(CSSPropertyMetadata::isEnabledProperty(property));
        range.consumeIncludingWhitespace();
        return CSSCustomIdentValue::create(property);
    }
    return consumeCustomIdent(range);
}

static CSSValue* consumeSteps(CSSParserTokenRange& range)
{
    ASSERT(range.peek().functionId() == CSSValueSteps);
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);

    CSSPrimitiveValue* steps = consumePositiveInteger(args);
    if (!steps)
        return nullptr;

    StepsTimingFunction::StepPosition position = StepsTimingFunction::StepPosition::END;
    if (consumeCommaIncludingWhitespace(args)) {
        switch (args.consumeIncludingWhitespace().id()) {
        case CSSValueMiddle:
            if (!RuntimeEnabledFeatures::webAnimationsAPIEnabled())
                return nullptr;
            position = StepsTimingFunction::StepPosition::MIDDLE;
            break;
        case CSSValueStart:
            position = StepsTimingFunction::StepPosition::START;
            break;
        case CSSValueEnd:
            position = StepsTimingFunction::StepPosition::END;
            break;
        default:
            return nullptr;
        }
    }

    if (!args.atEnd())
        return nullptr;

    range = rangeCopy;
    return CSSStepsTimingFunctionValue::create(steps->getIntValue(), position);
}

static CSSValue* consumeCubicBezier(CSSParserTokenRange& range)
{
    ASSERT(range.peek().functionId() == CSSValueCubicBezier);
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);

    double x1, y1, x2, y2;
    if (consumeNumberRaw(args, x1)
        && x1 >= 0 && x1 <= 1
        && consumeCommaIncludingWhitespace(args)
        && consumeNumberRaw(args, y1)
        && consumeCommaIncludingWhitespace(args)
        && consumeNumberRaw(args, x2)
        && x2 >= 0 && x2 <= 1
        && consumeCommaIncludingWhitespace(args)
        && consumeNumberRaw(args, y2)
        && args.atEnd()) {
        range = rangeCopy;
        return CSSCubicBezierTimingFunctionValue::create(x1, y1, x2, y2);
    }

    return nullptr;
}

static CSSValue* consumeAnimationTimingFunction(CSSParserTokenRange& range)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueEase || id == CSSValueLinear || id == CSSValueEaseIn
        || id == CSSValueEaseOut || id == CSSValueEaseInOut || id == CSSValueStepStart
        || id == CSSValueStepEnd || id == CSSValueStepMiddle)
        return consumeIdent(range);

    CSSValueID function = range.peek().functionId();
    if (function == CSSValueSteps)
        return consumeSteps(range);
    if (function == CSSValueCubicBezier)
        return consumeCubicBezier(range);
    return nullptr;
}

static CSSValue* consumeAnimationValue(CSSPropertyID property, CSSParserTokenRange& range, const CSSParserContext& context, bool useLegacyParsing)
{
    switch (property) {
    case CSSPropertyAnimationDelay:
    case CSSPropertyTransitionDelay:
        return consumeTime(range, ValueRangeAll);
    case CSSPropertyAnimationDirection:
        return consumeIdent<CSSValueNormal, CSSValueAlternate, CSSValueReverse, CSSValueAlternateReverse>(range);
    case CSSPropertyAnimationDuration:
    case CSSPropertyTransitionDuration:
        return consumeTime(range, ValueRangeNonNegative);
    case CSSPropertyAnimationFillMode:
        return consumeIdent<CSSValueNone, CSSValueForwards, CSSValueBackwards, CSSValueBoth>(range);
    case CSSPropertyAnimationIterationCount:
        return consumeAnimationIterationCount(range);
    case CSSPropertyAnimationName:
        return consumeAnimationName(range, context, useLegacyParsing);
    case CSSPropertyAnimationPlayState:
        return consumeIdent<CSSValueRunning, CSSValuePaused>(range);
    case CSSPropertyTransitionProperty:
        return consumeTransitionProperty(range);
    case CSSPropertyAnimationTimingFunction:
    case CSSPropertyTransitionTimingFunction:
        return consumeAnimationTimingFunction(range);
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

static bool isValidAnimationPropertyList(CSSPropertyID property, const CSSValueList& valueList)
{
    if (property != CSSPropertyTransitionProperty || valueList.length() < 2)
        return true;
    for (auto& value : valueList) {
        if (value->isPrimitiveValue() && toCSSPrimitiveValue(*value).isValueID()
            && toCSSPrimitiveValue(*value).getValueID() == CSSValueNone)
            return false;
    }
    return true;
}

static CSSValueList* consumeAnimationPropertyList(CSSPropertyID property, CSSParserTokenRange& range, const CSSParserContext& context, bool useLegacyParsing)
{
    CSSValueList* list = CSSValueList::createCommaSeparated();
    do {
        CSSValue* value = consumeAnimationValue(property, range, context, useLegacyParsing);
        if (!value)
            return nullptr;
        list->append(*value);
    } while (consumeCommaIncludingWhitespace(range));
    if (!isValidAnimationPropertyList(property, *list))
        return nullptr;
    ASSERT(list->length());
    return list;
}

bool CSSPropertyParser::consumeAnimationShorthand(const StylePropertyShorthand& shorthand, bool useLegacyParsing, bool important)
{
    const unsigned longhandCount = shorthand.length();
    CSSValueList* longhands[8];
    ASSERT(longhandCount <= 8);
    for (size_t i = 0; i < longhandCount; ++i)
        longhands[i] = CSSValueList::createCommaSeparated();

    do {
        bool parsedLonghand[8] = { false };
        do {
            bool foundProperty = false;
            for (size_t i = 0; i < longhandCount; ++i) {
                if (parsedLonghand[i])
                    continue;

                if (CSSValue* value = consumeAnimationValue(shorthand.properties()[i], m_range, m_context, useLegacyParsing)) {
                    parsedLonghand[i] = true;
                    foundProperty = true;
                    longhands[i]->append(*value);
                    break;
                }
            }
            if (!foundProperty)
                return false;
        } while (!m_range.atEnd() && m_range.peek().type() != CommaToken);

        // FIXME: This will make invalid longhands, see crbug.com/386459
        for (size_t i = 0; i < longhandCount; ++i) {
            if (!parsedLonghand[i])
                longhands[i]->append(*CSSInitialValue::createLegacyImplicit());
            parsedLonghand[i] = false;
        }
    } while (consumeCommaIncludingWhitespace(m_range));

    for (size_t i = 0; i < longhandCount; ++i) {
        if (!isValidAnimationPropertyList(shorthand.properties()[i], *longhands[i]))
            return false;
    }

    for (size_t i = 0; i < longhandCount; ++i)
        addProperty(shorthand.properties()[i], shorthand.id(), *longhands[i], important);

    return m_range.atEnd();
}

static CSSValue* consumeZIndex(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeInteger(range);
}

static CSSShadowValue* parseSingleShadow(CSSParserTokenRange& range, CSSParserMode cssParserMode, bool allowInset, bool allowSpread)
{
    CSSPrimitiveValue* style = nullptr;
    CSSValue* color = nullptr;

    if (range.atEnd())
        return nullptr;
    if (range.peek().id() == CSSValueInset) {
        if (!allowInset)
            return nullptr;
        style = consumeIdent(range);
    }
    color = consumeColor(range, cssParserMode);

    CSSPrimitiveValue* horizontalOffset = consumeLength(range, cssParserMode, ValueRangeAll);
    if (!horizontalOffset)
        return nullptr;

    CSSPrimitiveValue* verticalOffset = consumeLength(range, cssParserMode, ValueRangeAll);
    if (!verticalOffset)
        return nullptr;

    CSSPrimitiveValue* blurRadius = consumeLength(range, cssParserMode, ValueRangeAll);
    CSSPrimitiveValue* spreadDistance = nullptr;
    if (blurRadius) {
        // Blur radius must be non-negative.
        if (blurRadius->getDoubleValue() < 0)
            return nullptr;
        if (allowSpread)
            spreadDistance = consumeLength(range, cssParserMode, ValueRangeAll);
    }

    if (!range.atEnd()) {
        if (!color)
            color = consumeColor(range, cssParserMode);
        if (range.peek().id() == CSSValueInset) {
            if (!allowInset || style)
                return nullptr;
            style = consumeIdent(range);
        }
    }
    return CSSShadowValue::create(horizontalOffset, verticalOffset, blurRadius,
        spreadDistance, style, color);
}

static CSSValue* consumeShadow(CSSParserTokenRange& range, CSSParserMode cssParserMode, bool isBoxShadowProperty)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    CSSValueList* shadowValueList = CSSValueList::createCommaSeparated();
    do {
        if (CSSShadowValue* shadowValue = parseSingleShadow(range, cssParserMode, isBoxShadowProperty, isBoxShadowProperty))
            shadowValueList->append(*shadowValue);
        else
            return nullptr;
    } while (consumeCommaIncludingWhitespace(range));
    return shadowValueList;
}

static CSSFunctionValue* consumeFilterFunction(CSSParserTokenRange& range, const CSSParserContext& context)
{
    CSSValueID filterType = range.peek().functionId();
    if (filterType < CSSValueInvert || filterType > CSSValueDropShadow)
        return nullptr;
    CSSParserTokenRange args = consumeFunction(range);
    CSSFunctionValue* filterValue = CSSFunctionValue::create(filterType);
    CSSValue* parsedValue = nullptr;

    if (filterType == CSSValueDropShadow) {
        parsedValue = parseSingleShadow(args, context.mode(), false, false);
    } else {
        if (args.atEnd()) {
            if (context.useCounter())
                context.useCounter()->count(UseCounter::CSSFilterFunctionNoArguments);
            return filterValue;
        }
        if (filterType == CSSValueBrightness) {
            // FIXME (crbug.com/397061): Support calc expressions like calc(10% + 0.5)
            parsedValue = consumePercent(args, ValueRangeAll);
            if (!parsedValue)
                parsedValue = consumeNumber(args, ValueRangeAll);
        } else if (filterType == CSSValueHueRotate) {
            parsedValue = consumeAngle(args);
        } else if (filterType == CSSValueBlur) {
            parsedValue = consumeLength(args, HTMLStandardMode, ValueRangeNonNegative);
        } else {
            // FIXME (crbug.com/397061): Support calc expressions like calc(10% + 0.5)
            parsedValue = consumePercent(args, ValueRangeNonNegative);
            if (!parsedValue)
                parsedValue = consumeNumber(args, ValueRangeNonNegative);
            if (parsedValue && filterType != CSSValueSaturate && filterType != CSSValueContrast) {
                bool isPercentage = toCSSPrimitiveValue(parsedValue)->isPercentage();
                double maxAllowed = isPercentage ? 100.0 : 1.0;
                if (toCSSPrimitiveValue(parsedValue)->getDoubleValue() > maxAllowed) {
                    parsedValue = CSSPrimitiveValue::create(
                        maxAllowed,
                        isPercentage ? CSSPrimitiveValue::UnitType::Percentage : CSSPrimitiveValue::UnitType::Number);
                }
            }
        }
    }
    if (!parsedValue || !args.atEnd())
        return nullptr;
    filterValue->append(*parsedValue);
    return filterValue;
}

static CSSValue* consumeFilter(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    CSSValueList* list = CSSValueList::createSpaceSeparated();
    do {
        CSSValue* filterValue = consumeUrl(range);
        if (!filterValue) {
            filterValue = consumeFilterFunction(range, context);
            if (!filterValue)
                return nullptr;
        }
        list->append(*filterValue);
    } while (!range.atEnd());
    return list;
}

static CSSValue* consumeTextDecorationLine(CSSParserTokenRange& range)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueNone)
        return consumeIdent(range);

    CSSValueList* list = CSSValueList::createSpaceSeparated();
    while (true) {
        CSSPrimitiveValue* ident = consumeIdent<CSSValueBlink, CSSValueUnderline, CSSValueOverline, CSSValueLineThrough>(range);
        if (!ident)
            break;
        if (list->hasValue(*ident))
            return nullptr;
        list->append(*ident);
    }

    if (!list->length())
        return nullptr;
    return list;
}

// none | strict | content | [ layout || style || paint || size ]
static CSSValue* consumeContain(CSSParserTokenRange& range)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueNone)
        return consumeIdent(range);

    CSSValueList* list = CSSValueList::createSpaceSeparated();
    if (id == CSSValueStrict || id == CSSValueContent) {
        list->append(*consumeIdent(range));
        return list;
    }
    while (true) {
        CSSPrimitiveValue* ident = consumeIdent<CSSValuePaint, CSSValueLayout, CSSValueStyle, CSSValueSize>(range);
        if (!ident)
            break;
        if (list->hasValue(*ident))
            return nullptr;
        list->append(*ident);
    }

    if (!list->length())
        return nullptr;
    return list;
}

static CSSValue* consumePath(CSSParserTokenRange& range)
{
    // FIXME: Add support for <url>, <basic-shape>, <geometry-box>.
    if (range.peek().functionId() != CSSValuePath)
        return nullptr;

    CSSParserTokenRange functionRange = range;
    CSSParserTokenRange functionArgs = consumeFunction(functionRange);

    if (functionArgs.peek().type() != StringToken)
        return nullptr;
    String pathString = functionArgs.consumeIncludingWhitespace().value().toString();

    std::unique_ptr<SVGPathByteStream> byteStream = SVGPathByteStream::create();
    if (buildByteStreamFromString(pathString, *byteStream) != SVGParseStatus::NoError
        || !functionArgs.atEnd())
        return nullptr;

    range = functionRange;
    if (byteStream->isEmpty())
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);
    return CSSPathValue::create(std::move(byteStream));
}

static CSSValue* consumePathOrNone(CSSParserTokenRange& range)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueNone)
        return consumeIdent(range);

    return consumePath(range);
}

static CSSValue* consumeMotionRotation(CSSParserTokenRange& range)
{
    CSSValue* angle = consumeAngle(range);
    CSSValue* keyword = consumeIdent<CSSValueAuto, CSSValueReverse>(range);
    if (!angle && !keyword)
        return nullptr;

    if (!angle)
        angle = consumeAngle(range);

    CSSValueList* list = CSSValueList::createSpaceSeparated();
    if (keyword)
        list->append(*keyword);
    if (angle)
        list->append(*angle);
    return list;
}

static CSSValue* consumeTextEmphasisStyle(CSSParserTokenRange& range)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueNone)
        return consumeIdent(range);

    if (CSSValue* textEmphasisStyle = consumeString(range))
        return textEmphasisStyle;

    CSSPrimitiveValue* fill = consumeIdent<CSSValueFilled, CSSValueOpen>(range);
    CSSPrimitiveValue* shape = consumeIdent<CSSValueDot, CSSValueCircle, CSSValueDoubleCircle, CSSValueTriangle, CSSValueSesame>(range);
    if (!fill)
        fill = consumeIdent<CSSValueFilled, CSSValueOpen>(range);
    if (fill && shape) {
        CSSValueList* parsedValues = CSSValueList::createSpaceSeparated();
        parsedValues->append(*fill);
        parsedValues->append(*shape);
        return parsedValues;
    }
    if (fill)
        return fill;
    if (shape)
        return shape;
    return nullptr;
}

static CSSValue* consumeOutlineColor(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    // Allow the special focus color even in HTML Standard parsing mode.
    if (range.peek().id() == CSSValueWebkitFocusRingColor)
        return consumeIdent(range);
    return consumeColor(range, cssParserMode);
}

static CSSPrimitiveValue* consumeLineWidth(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueThin || id == CSSValueMedium || id == CSSValueThick)
        return consumeIdent(range);
    return consumeLength(range, cssParserMode, ValueRangeNonNegative, unitless);
}

static CSSPrimitiveValue* consumeBorderWidth(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless)
{
    return consumeLineWidth(range, cssParserMode, unitless);
}

static CSSPrimitiveValue* consumeTextStrokeWidth(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    return consumeLineWidth(range, cssParserMode, UnitlessQuirk::Forbid);
}

static CSSPrimitiveValue* consumeColumnRuleWidth(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    return consumeLineWidth(range, cssParserMode, UnitlessQuirk::Forbid);
}

static bool consumeTranslate3d(CSSParserTokenRange& args, CSSParserMode cssParserMode, CSSFunctionValue*& transformValue)
{
    unsigned numberOfArguments = 2;
    CSSValue* parsedValue = nullptr;
    do {
        parsedValue = consumeLengthOrPercent(args, cssParserMode, ValueRangeAll);
        if (!parsedValue)
            return false;
        transformValue->append(*parsedValue);
        if (!consumeCommaIncludingWhitespace(args))
            return false;
    } while (--numberOfArguments);
    parsedValue = consumeLength(args, cssParserMode, ValueRangeAll);
    if (!parsedValue)
        return false;
    transformValue->append(*parsedValue);
    return true;
}

static bool consumeNumbers(CSSParserTokenRange& args, CSSFunctionValue*& transformValue, unsigned numberOfArguments)
{
    do {
        CSSValue* parsedValue = consumeNumber(args, ValueRangeAll);
        if (!parsedValue)
            return false;
        transformValue->append(*parsedValue);
        if (--numberOfArguments && !consumeCommaIncludingWhitespace(args))
            return false;
    } while (numberOfArguments);
    return true;
}

static bool consumePerspective(CSSParserTokenRange& args, CSSParserMode cssParserMode, CSSFunctionValue*& transformValue, bool useLegacyParsing)
{
    CSSPrimitiveValue* parsedValue = consumeLength(args, cssParserMode, ValueRangeNonNegative);
    if (!parsedValue && useLegacyParsing) {
        double perspective;
        if (!consumeNumberRaw(args, perspective) || perspective < 0)
            return false;
        parsedValue = CSSPrimitiveValue::create(perspective, CSSPrimitiveValue::UnitType::Pixels);
    }
    if (!parsedValue)
        return false;
    transformValue->append(*parsedValue);
    return true;
}

static CSSValue* consumeTransformValue(CSSParserTokenRange& range, CSSParserMode cssParserMode, bool useLegacyParsing)
{
    CSSValueID functionId = range.peek().functionId();
    if (functionId == CSSValueInvalid)
        return nullptr;
    CSSParserTokenRange args = consumeFunction(range);
    if (args.atEnd())
        return nullptr;
    CSSFunctionValue* transformValue = CSSFunctionValue::create(functionId);
    CSSValue* parsedValue = nullptr;
    switch (functionId) {
    case CSSValueRotate:
    case CSSValueRotateX:
    case CSSValueRotateY:
    case CSSValueRotateZ:
    case CSSValueSkewX:
    case CSSValueSkewY:
    case CSSValueSkew:
        parsedValue = consumeAngle(args);
        if (!parsedValue)
            return nullptr;
        if (functionId == CSSValueSkew && consumeCommaIncludingWhitespace(args)) {
            transformValue->append(*parsedValue);
            parsedValue = consumeAngle(args);
            if (!parsedValue)
                return nullptr;
        }
        break;
    case CSSValueScaleX:
    case CSSValueScaleY:
    case CSSValueScaleZ:
    case CSSValueScale:
        parsedValue = consumeNumber(args, ValueRangeAll);
        if (!parsedValue)
            return nullptr;
        if (functionId == CSSValueScale && consumeCommaIncludingWhitespace(args)) {
            transformValue->append(*parsedValue);
            parsedValue = consumeNumber(args, ValueRangeAll);
            if (!parsedValue)
                return nullptr;
        }
        break;
    case CSSValuePerspective:
        if (!consumePerspective(args, cssParserMode, transformValue, useLegacyParsing))
            return nullptr;
        break;
    case CSSValueTranslateX:
    case CSSValueTranslateY:
    case CSSValueTranslate:
        parsedValue = consumeLengthOrPercent(args, cssParserMode, ValueRangeAll);
        if (!parsedValue)
            return nullptr;
        if (functionId == CSSValueTranslate && consumeCommaIncludingWhitespace(args)) {
            transformValue->append(*parsedValue);
            parsedValue = consumeLengthOrPercent(args, cssParserMode, ValueRangeAll);
            if (!parsedValue)
                return nullptr;
        }
        break;
    case CSSValueTranslateZ:
        parsedValue = consumeLength(args, cssParserMode, ValueRangeAll);
        break;
    case CSSValueMatrix:
    case CSSValueMatrix3d:
        if (!consumeNumbers(args, transformValue, (functionId == CSSValueMatrix3d) ? 16 : 6))
            return nullptr;
        break;
    case CSSValueScale3d:
        if (!consumeNumbers(args, transformValue, 3))
            return nullptr;
        break;
    case CSSValueRotate3d:
        if (!consumeNumbers(args, transformValue, 3) || !consumeCommaIncludingWhitespace(args))
            return nullptr;
        parsedValue = consumeAngle(args);
        if (!parsedValue)
            return nullptr;
        break;
    case CSSValueTranslate3d:
        if (!consumeTranslate3d(args, cssParserMode, transformValue))
            return nullptr;
        break;
    default:
        return nullptr;
    }
    if (parsedValue)
        transformValue->append(*parsedValue);
    if (!args.atEnd())
        return nullptr;
    return transformValue;
}

static CSSValue* consumeTransform(CSSParserTokenRange& range, CSSParserMode cssParserMode, bool useLegacyParsing)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    CSSValueList* list = CSSValueList::createSpaceSeparated();
    do {
        CSSValue* parsedTransformValue = consumeTransformValue(range, cssParserMode, useLegacyParsing);
        if (!parsedTransformValue)
            return nullptr;
        list->append(*parsedTransformValue);
    } while (!range.atEnd());

    return list;
}

template <CSSValueID start, CSSValueID end>
static CSSValue* consumePositionLonghand(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().type() == IdentToken) {
        CSSValueID id = range.peek().id();
        int percent;
        if (id == start)
            percent = 0;
        else if (id == CSSValueCenter)
            percent = 50;
        else if (id == end)
            percent = 100;
        else
            return nullptr;
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(percent, CSSPrimitiveValue::UnitType::Percentage);
    }
    return consumeLengthOrPercent(range, cssParserMode, ValueRangeAll);
}

static CSSValue* consumePositionX(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    return consumePositionLonghand<CSSValueLeft, CSSValueRight>(range, cssParserMode);
}

static CSSValue* consumePositionY(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    return consumePositionLonghand<CSSValueTop, CSSValueBottom>(range, cssParserMode);
}

static CSSValue* consumePaintStroke(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    CSSURIValue* url = consumeUrl(range);
    if (url) {
        CSSValue* parsedValue = nullptr;
        if (range.peek().id() == CSSValueNone)
            parsedValue = consumeIdent(range);
        else
            parsedValue = consumeColor(range, cssParserMode);
        if (parsedValue) {
            CSSValueList* values = CSSValueList::createSpaceSeparated();
            values->append(*url);
            values->append(*parsedValue);
            return values;
        }
        return url;
    }
    return consumeColor(range, cssParserMode);
}

static CSSValue* consumePaintOrder(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);

    Vector<CSSValueID, 3> paintTypeList;
    CSSPrimitiveValue* fill = nullptr;
    CSSPrimitiveValue* stroke = nullptr;
    CSSPrimitiveValue* markers = nullptr;
    do {
        CSSValueID id = range.peek().id();
        if (id == CSSValueFill && !fill)
            fill = consumeIdent(range);
        else if (id == CSSValueStroke && !stroke)
            stroke = consumeIdent(range);
        else if (id == CSSValueMarkers && !markers)
            markers = consumeIdent(range);
        else
            return nullptr;
        paintTypeList.append(id);
    } while (!range.atEnd());

    // After parsing we serialize the paint-order list. Since it is not possible to
    // pop a last list items from CSSValueList without bigger cost, we create the
    // list after parsing.
    CSSValueID firstPaintOrderType = paintTypeList.at(0);
    CSSValueList* paintOrderList = CSSValueList::createSpaceSeparated();
    switch (firstPaintOrderType) {
    case CSSValueFill:
    case CSSValueStroke:
        paintOrderList->append(firstPaintOrderType == CSSValueFill ? *fill : *stroke);
        if (paintTypeList.size() > 1) {
            if (paintTypeList.at(1) == CSSValueMarkers)
                paintOrderList->append(*markers);
        }
        break;
    case CSSValueMarkers:
        paintOrderList->append(*markers);
        if (paintTypeList.size() > 1) {
            if (paintTypeList.at(1) == CSSValueStroke)
                paintOrderList->append(*stroke);
        }
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    return paintOrderList;
}

static CSSValue* consumeNoneOrURI(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    return consumeUrl(range);
}

static CSSValue* consumeFlexBasis(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    // FIXME: Support intrinsic dimensions too.
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeLengthOrPercent(range, cssParserMode, ValueRangeNonNegative);
}

static CSSValue* consumeStrokeDasharray(CSSParserTokenRange& range)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueNone)
        return consumeIdent(range);

    CSSValueList* dashes = CSSValueList::createCommaSeparated();
    do {
        CSSPrimitiveValue* dash = consumeLengthOrPercent(range, SVGAttributeMode, ValueRangeNonNegative);
        if (!dash || (consumeCommaIncludingWhitespace(range) && range.atEnd()))
            return nullptr;
        dashes->append(*dash);
    } while (!range.atEnd());
    return dashes;
}

static CSSPrimitiveValue* consumeBaselineShift(CSSParserTokenRange& range)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueBaseline || id == CSSValueSub || id == CSSValueSuper)
        return consumeIdent(range);
    return consumeLengthOrPercent(range, SVGAttributeMode, ValueRangeAll);
}

static CSSPrimitiveValue* consumeRxOrRy(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeLengthOrPercent(range, SVGAttributeMode, ValueRangeAll, UnitlessQuirk::Forbid);
}

static CSSValue* consumeCursor(CSSParserTokenRange& range, const CSSParserContext& context, bool inQuirksMode)
{
    CSSValueList* list = nullptr;
    while (CSSValue* image = consumeImage(range, context, ConsumeGeneratedImage::Forbid)) {
        double num;
        IntPoint hotSpot(-1, -1);
        bool hotSpotSpecified = false;
        if (consumeNumberRaw(range, num)) {
            hotSpot.setX(int(num));
            if (!consumeNumberRaw(range, num))
                return nullptr;
            hotSpot.setY(int(num));
            hotSpotSpecified = true;
        }

        if (!list)
            list = CSSValueList::createCommaSeparated();

        list->append(*CSSCursorImageValue::create(image, hotSpotSpecified, hotSpot));
        if (!consumeCommaIncludingWhitespace(range))
            return nullptr;
    }

    CSSValueID id = range.peek().id();
    if (!range.atEnd() && context.useCounter()) {
        if (id == CSSValueWebkitZoomIn)
            context.useCounter()->count(UseCounter::PrefixedCursorZoomIn);
        else if (id == CSSValueWebkitZoomOut)
            context.useCounter()->count(UseCounter::PrefixedCursorZoomOut);
    }
    CSSValue* cursorType = nullptr;
    if (id == CSSValueHand) {
        if (!inQuirksMode) // Non-standard behavior
            return nullptr;
        cursorType = CSSPrimitiveValue::createIdentifier(CSSValuePointer);
        range.consumeIncludingWhitespace();
    } else if ((id >= CSSValueAuto && id <= CSSValueWebkitZoomOut) || id == CSSValueCopy || id == CSSValueNone) {
        cursorType = consumeIdent(range);
    } else {
        return nullptr;
    }

    if (!list)
        return cursorType;
    list->append(*cursorType);
    return list;
}

static CSSValue* consumeAttr(CSSParserTokenRange args, CSSParserContext context)
{
    if (args.peek().type() != IdentToken)
        return nullptr;

    String attrName = args.consumeIncludingWhitespace().value().toString();
    if (!args.atEnd())
        return nullptr;

    if (context.isHTMLDocument())
        attrName = attrName.lower();

    CSSFunctionValue* attrValue = CSSFunctionValue::create(CSSValueAttr);
    attrValue->append(*CSSCustomIdentValue::create(attrName));
    return attrValue;
}

static CSSValue* consumeCounterContent(CSSParserTokenRange args, bool counters)
{
    CSSCustomIdentValue* identifier = consumeCustomIdent(args);
    if (!identifier)
        return nullptr;

    CSSStringValue* separator = nullptr;
    if (!counters) {
        separator = CSSStringValue::create(String());
    } else {
        if (!consumeCommaIncludingWhitespace(args) || args.peek().type() != StringToken)
            return nullptr;
        separator = CSSStringValue::create(args.consumeIncludingWhitespace().value().toString());
    }

    CSSPrimitiveValue* listStyle = nullptr;
    if (consumeCommaIncludingWhitespace(args)) {
        CSSValueID id = args.peek().id();
        if ((id != CSSValueNone && (id < CSSValueDisc || id > CSSValueKatakanaIroha)))
            return nullptr;
        listStyle = consumeIdent(args);
    } else {
        listStyle = CSSPrimitiveValue::createIdentifier(CSSValueDecimal);
    }

    if (!args.atEnd())
        return nullptr;
    return CSSCounterValue::create(identifier, listStyle, separator);
}

static CSSValue* consumeContent(CSSParserTokenRange& range, CSSParserContext context)
{
    if (identMatches<CSSValueNone, CSSValueNormal>(range.peek().id()))
        return consumeIdent(range);

    CSSValueList* values = CSSValueList::createSpaceSeparated();

    do {
        CSSValue* parsedValue = consumeImage(range, context);
        if (!parsedValue)
            parsedValue = consumeIdent<CSSValueOpenQuote, CSSValueCloseQuote, CSSValueNoOpenQuote, CSSValueNoCloseQuote>(range);
        if (!parsedValue)
            parsedValue = consumeString(range);
        if (!parsedValue) {
            if (range.peek().functionId() == CSSValueAttr)
                parsedValue = consumeAttr(consumeFunction(range), context);
            else if (range.peek().functionId() == CSSValueCounter)
                parsedValue = consumeCounterContent(consumeFunction(range), false);
            else if (range.peek().functionId() == CSSValueCounters)
                parsedValue = consumeCounterContent(consumeFunction(range), true);
            if (!parsedValue)
                return nullptr;
        }
        values->append(*parsedValue);
    } while (!range.atEnd());

    return values;
}

static CSSPrimitiveValue* consumePerspective(CSSParserTokenRange& range, CSSParserMode cssParserMode, CSSPropertyID unresolvedProperty)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    CSSPrimitiveValue* parsedValue = consumeLength(range, cssParserMode, ValueRangeAll);
    if (!parsedValue && (unresolvedProperty == CSSPropertyAliasWebkitPerspective)) {
        double perspective;
        if (!consumeNumberRaw(range, perspective))
            return nullptr;
        parsedValue = CSSPrimitiveValue::create(perspective, CSSPrimitiveValue::UnitType::Pixels);
    }
    if (parsedValue && (parsedValue->isCalculated() || parsedValue->getDoubleValue() > 0))
        return parsedValue;
    return nullptr;
}

static CSSValueList* consumePositionList(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    CSSValueList* positions = CSSValueList::createCommaSeparated();
    do {
        CSSValue* position = consumePosition(range, cssParserMode, UnitlessQuirk::Forbid);
        if (!position)
            return nullptr;
        positions->append(*position);
    } while (consumeCommaIncludingWhitespace(range));
    return positions;
}

static CSSValue* consumeScrollSnapCoordinate(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    return consumePositionList(range, cssParserMode);
}

static CSSValue* consumeScrollSnapPoints(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    if (range.peek().functionId() == CSSValueRepeat) {
        CSSParserTokenRange args = consumeFunction(range);
        CSSPrimitiveValue* parsedValue = consumeLengthOrPercent(args, cssParserMode, ValueRangeNonNegative);
        if (args.atEnd() && parsedValue && (parsedValue->isCalculated() || parsedValue->getDoubleValue() > 0)) {
            CSSFunctionValue* result = CSSFunctionValue::create(CSSValueRepeat);
            result->append(*parsedValue);
            return result;
        }
    }
    return nullptr;
}

static CSSValue* consumeBorderRadiusCorner(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    CSSValue* parsedValue1 = consumeLengthOrPercent(range, cssParserMode, ValueRangeNonNegative);
    if (!parsedValue1)
        return nullptr;
    CSSValue* parsedValue2 = consumeLengthOrPercent(range, cssParserMode, ValueRangeNonNegative);
    if (!parsedValue2)
        parsedValue2 = parsedValue1;
    return CSSValuePair::create(parsedValue1, parsedValue2, CSSValuePair::DropIdenticalValues);
}

static CSSPrimitiveValue* consumeVerticalAlign(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    CSSPrimitiveValue* parsedValue = consumeIdentRange(range, CSSValueBaseline, CSSValueWebkitBaselineMiddle);
    if (!parsedValue)
        parsedValue = consumeLengthOrPercent(range, cssParserMode, ValueRangeAll, UnitlessQuirk::Allow);
    return parsedValue;
}

static CSSPrimitiveValue* consumeShapeRadius(CSSParserTokenRange& args, CSSParserMode cssParserMode)
{
    if (identMatches<CSSValueClosestSide, CSSValueFarthestSide>(args.peek().id()))
        return consumeIdent(args);
    return consumeLengthOrPercent(args, cssParserMode, ValueRangeNonNegative);
}

static CSSBasicShapeCircleValue* consumeBasicShapeCircle(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // spec: https://drafts.csswg.org/css-shapes/#supported-basic-shapes
    // circle( [<shape-radius>]? [at <position>]? )
    CSSBasicShapeCircleValue* shape = CSSBasicShapeCircleValue::create();
    if (CSSPrimitiveValue* radius = consumeShapeRadius(args, context.mode()))
        shape->setRadius(radius);
    if (consumeIdent<CSSValueAt>(args)) {
        CSSValue* centerX = nullptr;
        CSSValue* centerY = nullptr;
        if (!consumePosition(args, context.mode(), UnitlessQuirk::Forbid, centerX, centerY))
            return nullptr;
        shape->setCenterX(centerX);
        shape->setCenterY(centerY);
    }
    return shape;
}

static CSSBasicShapeEllipseValue* consumeBasicShapeEllipse(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // spec: https://drafts.csswg.org/css-shapes/#supported-basic-shapes
    // ellipse( [<shape-radius>{2}]? [at <position>]? )
    CSSBasicShapeEllipseValue* shape = CSSBasicShapeEllipseValue::create();
    if (CSSPrimitiveValue* radiusX = consumeShapeRadius(args, context.mode())) {
        shape->setRadiusX(radiusX);
        if (CSSPrimitiveValue* radiusY = consumeShapeRadius(args, context.mode()))
            shape->setRadiusY(radiusY);
    }
    if (consumeIdent<CSSValueAt>(args)) {
        CSSValue* centerX = nullptr;
        CSSValue* centerY = nullptr;
        if (!consumePosition(args, context.mode(), UnitlessQuirk::Forbid, centerX, centerY))
            return nullptr;
        shape->setCenterX(centerX);
        shape->setCenterY(centerY);
    }
    return shape;
}

static CSSBasicShapePolygonValue* consumeBasicShapePolygon(CSSParserTokenRange& args, const CSSParserContext& context)
{
    CSSBasicShapePolygonValue* shape = CSSBasicShapePolygonValue::create();
    if (identMatches<CSSValueEvenodd, CSSValueNonzero>(args.peek().id())) {
        shape->setWindRule(args.consumeIncludingWhitespace().id() == CSSValueEvenodd ? RULE_EVENODD : RULE_NONZERO);
        if (!consumeCommaIncludingWhitespace(args))
            return nullptr;
    }

    do {
        CSSPrimitiveValue* xLength = consumeLengthOrPercent(args, context.mode(), ValueRangeAll);
        if (!xLength)
            return nullptr;
        CSSPrimitiveValue* yLength = consumeLengthOrPercent(args, context.mode(), ValueRangeAll);
        if (!yLength)
            return nullptr;
        shape->appendPoint(xLength, yLength);
    } while (consumeCommaIncludingWhitespace(args));
    return shape;
}

static void complete4Sides(CSSPrimitiveValue* side[4])
{
    if (side[3])
        return;
    if (!side[2]) {
        if (!side[1])
            side[1] = side[0];
        side[2] = side[0];
    }
    side[3] = side[1];
}

static bool consumeRadii(CSSPrimitiveValue* horizontalRadii[4], CSSPrimitiveValue* verticalRadii[4], CSSParserTokenRange& range, CSSParserMode cssParserMode, bool useLegacyParsing)
{
    unsigned i = 0;
    for (; i < 4 && !range.atEnd() && range.peek().type() != DelimiterToken; ++i) {
        horizontalRadii[i] = consumeLengthOrPercent(range, cssParserMode, ValueRangeNonNegative);
        if (!horizontalRadii[i])
            return false;
    }
    if (!horizontalRadii[0])
        return false;
    if (range.atEnd()) {
        // Legacy syntax: -webkit-border-radius: l1 l2; is equivalent to border-radius: l1 / l2;
        if (useLegacyParsing && i == 2) {
            verticalRadii[0] = horizontalRadii[1];
            horizontalRadii[1] = nullptr;
        } else {
            complete4Sides(horizontalRadii);
            for (unsigned i = 0; i < 4; ++i)
                verticalRadii[i] = horizontalRadii[i];
            return true;
        }
    } else {
        if (!consumeSlashIncludingWhitespace(range))
            return false;
        for (i = 0; i < 4 && !range.atEnd(); ++i) {
            verticalRadii[i] = consumeLengthOrPercent(range, cssParserMode, ValueRangeNonNegative);
            if (!verticalRadii[i])
                return false;
        }
        if (!verticalRadii[0] || !range.atEnd())
            return false;
    }
    complete4Sides(horizontalRadii);
    complete4Sides(verticalRadii);
    return true;
}

static CSSBasicShapeInsetValue* consumeBasicShapeInset(CSSParserTokenRange& args, const CSSParserContext& context)
{
    CSSBasicShapeInsetValue* shape = CSSBasicShapeInsetValue::create();
    CSSPrimitiveValue* top = consumeLengthOrPercent(args, context.mode(), ValueRangeAll);
    if (!top)
        return nullptr;
    CSSPrimitiveValue* right = consumeLengthOrPercent(args, context.mode(), ValueRangeAll);
    CSSPrimitiveValue* bottom = nullptr;
    CSSPrimitiveValue* left = nullptr;
    if (right) {
        bottom = consumeLengthOrPercent(args, context.mode(), ValueRangeAll);
        if (bottom)
            left = consumeLengthOrPercent(args, context.mode(), ValueRangeAll);
    }
    if (left)
        shape->updateShapeSize4Values(top, right, bottom, left);
    else if (bottom)
        shape->updateShapeSize3Values(top, right, bottom);
    else if (right)
        shape->updateShapeSize2Values(top, right);
    else
        shape->updateShapeSize1Value(top);

    if (consumeIdent<CSSValueRound>(args)) {
        CSSPrimitiveValue* horizontalRadii[4] = { 0 };
        CSSPrimitiveValue* verticalRadii[4] = { 0 };
        if (!consumeRadii(horizontalRadii, verticalRadii, args, context.mode(), false))
            return nullptr;
        shape->setTopLeftRadius(CSSValuePair::create(horizontalRadii[0], verticalRadii[0], CSSValuePair::DropIdenticalValues));
        shape->setTopRightRadius(CSSValuePair::create(horizontalRadii[1], verticalRadii[1], CSSValuePair::DropIdenticalValues));
        shape->setBottomRightRadius(CSSValuePair::create(horizontalRadii[2], verticalRadii[2], CSSValuePair::DropIdenticalValues));
        shape->setBottomLeftRadius(CSSValuePair::create(horizontalRadii[3], verticalRadii[3], CSSValuePair::DropIdenticalValues));
    }
    return shape;
}

static CSSValue* consumeBasicShape(CSSParserTokenRange& range, const CSSParserContext& context)
{
    CSSValue* shape = nullptr;
    if (range.peek().type() != FunctionToken)
        return nullptr;
    CSSValueID id = range.peek().functionId();
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);
    if (id == CSSValueCircle)
        shape = consumeBasicShapeCircle(args, context);
    else if (id == CSSValueEllipse)
        shape = consumeBasicShapeEllipse(args, context);
    else if (id == CSSValuePolygon)
        shape = consumeBasicShapePolygon(args, context);
    else if (id == CSSValueInset)
        shape = consumeBasicShapeInset(args, context);
    if (!shape || !args.atEnd())
        return nullptr;
    range = rangeCopy;
    return shape;
}

static CSSValue* consumeWebkitClipPath(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    if (CSSURIValue* url = consumeUrl(range))
        return url;
    return consumeBasicShape(range, context);
}

static CSSValue* consumeShapeOutside(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (CSSValue* imageValue = consumeImageOrNone(range, context))
        return imageValue;
    CSSValueList* list = CSSValueList::createSpaceSeparated();
    if (CSSValue* boxValue = consumeIdent<CSSValueContentBox, CSSValuePaddingBox, CSSValueBorderBox, CSSValueMarginBox>(range))
        list->append(*boxValue);
    if (CSSValue* shapeValue = consumeBasicShape(range, context)) {
        list->append(*shapeValue);
        if (list->length() < 2) {
            if (CSSValue* boxValue = consumeIdent<CSSValueContentBox, CSSValuePaddingBox, CSSValueBorderBox, CSSValueMarginBox>(range))
                list->append(*boxValue);
        }
    }
    if (!list->length())
        return nullptr;
    return list;
}

static CSSValue* consumeContentDistributionOverflowPosition(CSSParserTokenRange& range)
{
    if (identMatches<CSSValueNormal, CSSValueBaseline, CSSValueLastBaseline>(range.peek().id()))
        return CSSContentDistributionValue::create(CSSValueInvalid, range.consumeIncludingWhitespace().id(), CSSValueInvalid);

    CSSValueID distribution = CSSValueInvalid;
    CSSValueID position = CSSValueInvalid;
    CSSValueID overflow = CSSValueInvalid;
    do {
        CSSValueID id = range.peek().id();
        if (identMatches<CSSValueSpaceBetween, CSSValueSpaceAround, CSSValueSpaceEvenly, CSSValueStretch>(id)) {
            if (distribution != CSSValueInvalid)
                return nullptr;
            distribution = id;
        } else if (identMatches<CSSValueStart, CSSValueEnd, CSSValueCenter, CSSValueFlexStart, CSSValueFlexEnd, CSSValueLeft, CSSValueRight>(id)) {
            if (position != CSSValueInvalid)
                return nullptr;
            position = id;
        } else if (identMatches<CSSValueUnsafe, CSSValueSafe>(id)) {
            if (overflow != CSSValueInvalid)
                return nullptr;
            overflow = id;
        } else {
            return nullptr;
        }
        range.consumeIncludingWhitespace();
    } while (!range.atEnd());

    // The grammar states that we should have at least <content-distribution> or <content-position>.
    if (position == CSSValueInvalid && distribution == CSSValueInvalid)
        return nullptr;

    // The grammar states that <overflow-position> must be associated to <content-position>.
    if (overflow != CSSValueInvalid && position == CSSValueInvalid)
        return nullptr;

    return CSSContentDistributionValue::create(distribution, position, overflow);
}

static CSSPrimitiveValue* consumeBorderImageRepeatKeyword(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueStretch, CSSValueRepeat, CSSValueSpace, CSSValueRound>(range);
}

static CSSValue* consumeBorderImageRepeat(CSSParserTokenRange& range)
{
    CSSPrimitiveValue* horizontal = consumeBorderImageRepeatKeyword(range);
    if (!horizontal)
        return nullptr;
    CSSPrimitiveValue* vertical = consumeBorderImageRepeatKeyword(range);
    if (!vertical)
        vertical = horizontal;
    return CSSValuePair::create(horizontal, vertical, CSSValuePair::DropIdenticalValues);
}

static CSSValue* consumeBorderImageSlice(CSSPropertyID property, CSSParserTokenRange& range)
{
    bool fill = consumeIdent<CSSValueFill>(range);
    CSSPrimitiveValue* slices[4] = { 0 };

    for (size_t index = 0; index < 4; ++index) {
        CSSPrimitiveValue* value = consumePercent(range, ValueRangeNonNegative);
        if (!value)
            value = consumeNumber(range, ValueRangeNonNegative);
        if (!value)
            break;
        slices[index] = value;
    }
    if (!slices[0])
        return nullptr;
    if (consumeIdent<CSSValueFill>(range)) {
        if (fill)
            return nullptr;
        fill = true;
    }
    complete4Sides(slices);
    // FIXME: For backwards compatibility, -webkit-border-image, -webkit-mask-box-image and -webkit-box-reflect have to do a fill by default.
    // FIXME: What do we do with -webkit-box-reflect and -webkit-mask-box-image? Probably just have to leave them filling...
    if (property == CSSPropertyWebkitBorderImage || property == CSSPropertyWebkitMaskBoxImage || property == CSSPropertyWebkitBoxReflect)
        fill = true;
    return CSSBorderImageSliceValue::create(CSSQuadValue::create(slices[0], slices[1], slices[2], slices[3], CSSQuadValue::SerializeAsQuad), fill);
}

static CSSValue* consumeBorderImageOutset(CSSParserTokenRange& range)
{
    CSSPrimitiveValue* outsets[4] = { 0 };

    CSSPrimitiveValue* value = nullptr;
    for (size_t index = 0; index < 4; ++index) {
        value = consumeNumber(range, ValueRangeNonNegative);
        if (!value)
            value = consumeLength(range, HTMLStandardMode, ValueRangeNonNegative);
        if (!value)
            break;
        outsets[index] = value;
    }
    if (!outsets[0])
        return nullptr;
    complete4Sides(outsets);
    return CSSQuadValue::create(outsets[0], outsets[1], outsets[2], outsets[3], CSSQuadValue::SerializeAsQuad);
}

static CSSValue* consumeBorderImageWidth(CSSParserTokenRange& range)
{
    CSSPrimitiveValue* widths[4] = { 0 };

    CSSPrimitiveValue* value = nullptr;
    for (size_t index = 0; index < 4; ++index) {
        value = consumeNumber(range, ValueRangeNonNegative);
        if (!value)
            value = consumeLengthOrPercent(range, HTMLStandardMode, ValueRangeNonNegative, UnitlessQuirk::Forbid);
        if (!value)
            value = consumeIdent<CSSValueAuto>(range);
        if (!value)
            break;
        widths[index] = value;
    }
    if (!widths[0])
        return nullptr;
    complete4Sides(widths);
    return CSSQuadValue::create(widths[0], widths[1], widths[2], widths[3], CSSQuadValue::SerializeAsQuad);
}

static bool consumeBorderImageComponents(CSSPropertyID property, CSSParserTokenRange& range, const CSSParserContext& context, CSSValue*& source,
    CSSValue*& slice, CSSValue*& width, CSSValue*& outset, CSSValue*& repeat)
{
    do {
        if (!source) {
            source = consumeImageOrNone(range, context);
            if (source)
                continue;
        }
        if (!repeat) {
            repeat = consumeBorderImageRepeat(range);
            if (repeat)
                continue;
        }
        if (!slice) {
            slice = consumeBorderImageSlice(property, range);
            if (slice) {
                ASSERT(!width && !outset);
                if (consumeSlashIncludingWhitespace(range)) {
                    width = consumeBorderImageWidth(range);
                    if (consumeSlashIncludingWhitespace(range)) {
                        outset = consumeBorderImageOutset(range);
                        if (!outset)
                            return false;
                    } else if (!width) {
                        return false;
                    }
                }
            } else {
                return false;
            }
        } else {
            return false;
        }
    } while (!range.atEnd());
    return true;
}

static CSSValue* consumeWebkitBorderImage(CSSPropertyID property, CSSParserTokenRange& range, const CSSParserContext& context)
{
    CSSValue* source = nullptr;
    CSSValue* slice = nullptr;
    CSSValue* width = nullptr;
    CSSValue* outset = nullptr;
    CSSValue* repeat = nullptr;
    if (consumeBorderImageComponents(property, range, context, source, slice, width, outset, repeat))
        return createBorderImageValue(source, slice, width, outset, repeat);
    return nullptr;
}

static CSSValue* consumeReflect(CSSParserTokenRange& range, const CSSParserContext& context)
{
    CSSPrimitiveValue* direction = consumeIdent<CSSValueAbove, CSSValueBelow, CSSValueLeft, CSSValueRight>(range);
    if (!direction)
        return nullptr;

    CSSPrimitiveValue* offset = nullptr;
    if (range.atEnd()) {
        offset = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Pixels);
    } else {
        offset = consumeLengthOrPercent(range, context.mode(), ValueRangeAll, UnitlessQuirk::Forbid);
        if (!offset)
            return nullptr;
    }

    CSSValue* mask = nullptr;
    if (!range.atEnd()) {
        mask = consumeWebkitBorderImage(CSSPropertyWebkitBoxReflect, range, context);
        if (!mask)
            return nullptr;
    }
    return CSSReflectValue::create(direction, offset, mask);
}

static CSSValue* consumeFontSizeAdjust(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    return consumeNumber(range, ValueRangeNonNegative);
}

static CSSValue* consumeImageOrientation(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueFromImage)
        return consumeIdent(range);
    if (range.peek().type() != NumberToken) {
        CSSPrimitiveValue* angle = consumeAngle(range);
        if (angle && angle->getDoubleValue() == 0)
            return angle;
    }
    return nullptr;
}

static CSSValue* consumeBackgroundBlendMode(CSSParserTokenRange& range)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueNormal || id == CSSValueOverlay || (id >= CSSValueMultiply && id <= CSSValueLuminosity))
        return consumeIdent(range);
    return nullptr;
}

static CSSValue* consumeBackgroundAttachment(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueScroll, CSSValueFixed, CSSValueLocal>(range);
}

static CSSValue* consumeBackgroundBox(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueBorderBox, CSSValuePaddingBox, CSSValueContentBox>(range);
}

static CSSValue* consumeBackgroundComposite(CSSParserTokenRange& range)
{
    return consumeIdentRange(range, CSSValueClear, CSSValuePlusLighter);
}

static CSSValue* consumeMaskSourceType(CSSParserTokenRange& range)
{
    ASSERT(RuntimeEnabledFeatures::cssMaskSourceTypeEnabled());
    return consumeIdent<CSSValueAuto, CSSValueAlpha, CSSValueLuminance>(range);
}

static CSSValue* consumePrefixedBackgroundBox(CSSPropertyID property, CSSParserTokenRange& range, const CSSParserContext& context)
{
    // The values 'border', 'padding' and 'content' are deprecated and do not apply to the version of the property that has the -webkit- prefix removed.
    if (CSSValue* value = consumeIdentRange(range, CSSValueBorder, CSSValuePaddingBox))
        return value;
    if ((property == CSSPropertyWebkitBackgroundClip || property == CSSPropertyWebkitMaskClip) && range.peek().id() == CSSValueText)
        return consumeIdent(range);
    return nullptr;
}

static CSSValue* consumeBackgroundSize(CSSPropertyID unresolvedProperty, CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (identMatches<CSSValueContain, CSSValueCover>(range.peek().id()))
        return consumeIdent(range);

    CSSPrimitiveValue* horizontal = consumeIdent<CSSValueAuto>(range);
    if (!horizontal)
        horizontal = consumeLengthOrPercent(range, cssParserMode, ValueRangeAll, UnitlessQuirk::Forbid);

    CSSPrimitiveValue* vertical = nullptr;
    if (!range.atEnd()) {
        if (range.peek().id() == CSSValueAuto) // `auto' is the default
            range.consumeIncludingWhitespace();
        else
            vertical = consumeLengthOrPercent(range, cssParserMode, ValueRangeAll, UnitlessQuirk::Forbid);
    } else if (unresolvedProperty == CSSPropertyAliasWebkitBackgroundSize) {
        // Legacy syntax: "-webkit-background-size: 10px" is equivalent to "background-size: 10px 10px".
        vertical = horizontal;
    }
    if (!vertical)
        return horizontal;
    return CSSValuePair::create(horizontal, vertical, CSSValuePair::KeepIdenticalValues);
}

static CSSValueList* consumeGridAutoFlow(CSSParserTokenRange& range)
{
    CSSPrimitiveValue* rowOrColumnValue = consumeIdent<CSSValueRow, CSSValueColumn>(range);
    CSSPrimitiveValue* denseAlgorithm = consumeIdent<CSSValueDense>(range);
    if (!rowOrColumnValue) {
        rowOrColumnValue = consumeIdent<CSSValueRow, CSSValueColumn>(range);
        if (!rowOrColumnValue && !denseAlgorithm)
            return nullptr;
    }
    CSSValueList* parsedValues = CSSValueList::createSpaceSeparated();
    if (rowOrColumnValue)
        parsedValues->append(*rowOrColumnValue);
    if (denseAlgorithm)
        parsedValues->append(*denseAlgorithm);
    return parsedValues;
}

static CSSValue* consumeBackgroundComponent(CSSPropertyID unresolvedProperty, CSSParserTokenRange& range, const CSSParserContext& context)
{
    switch (unresolvedProperty) {
    case CSSPropertyBackgroundClip:
        return consumeBackgroundBox(range);
    case CSSPropertyBackgroundBlendMode:
        return consumeBackgroundBlendMode(range);
    case CSSPropertyBackgroundAttachment:
        return consumeBackgroundAttachment(range);
    case CSSPropertyBackgroundOrigin:
        return consumeBackgroundBox(range);
    case CSSPropertyWebkitMaskComposite:
        return consumeBackgroundComposite(range);
    case CSSPropertyMaskSourceType:
        return consumeMaskSourceType(range);
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyWebkitMaskClip:
    case CSSPropertyWebkitMaskOrigin:
        return consumePrefixedBackgroundBox(unresolvedProperty, range, context);
    case CSSPropertyBackgroundImage:
    case CSSPropertyWebkitMaskImage:
        return consumeImageOrNone(range, context);
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyWebkitMaskPositionX:
        return consumePositionX(range, context.mode());
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyWebkitMaskPositionY:
        return consumePositionY(range, context.mode());
    case CSSPropertyBackgroundSize:
    case CSSPropertyAliasWebkitBackgroundSize:
    case CSSPropertyWebkitMaskSize:
        return consumeBackgroundSize(unresolvedProperty, range, context.mode());
    case CSSPropertyBackgroundColor:
        return consumeColor(range, context.mode());
    default:
        break;
    };
    return nullptr;
}

static void addBackgroundValue(CSSValue*& list, CSSValue* value)
{
    if (list) {
        if (!list->isBaseValueList()) {
            CSSValue* firstValue = list;
            list = CSSValueList::createCommaSeparated();
            toCSSValueList(list)->append(*firstValue);
        }
        toCSSValueList(list)->append(*value);
    } else {
        // To conserve memory we don't actually wrap a single value in a list.
        list = value;
    }
}

static CSSValue* consumeCommaSeparatedBackgroundComponent(CSSPropertyID unresolvedProperty, CSSParserTokenRange& range, const CSSParserContext& context)
{
    CSSValue* result = nullptr;
    do {
        CSSValue* value = consumeBackgroundComponent(unresolvedProperty, range, context);
        if (!value)
            return nullptr;
        addBackgroundValue(result, value);
    } while (consumeCommaIncludingWhitespace(range));
    return result;
}

static CSSPrimitiveValue* consumeSelfPositionKeyword(CSSParserTokenRange& range)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueStart || id == CSSValueEnd || id == CSSValueCenter
        || id == CSSValueSelfStart || id == CSSValueSelfEnd || id == CSSValueFlexStart
        || id == CSSValueFlexEnd || id == CSSValueLeft || id == CSSValueRight)
        return consumeIdent(range);
    return nullptr;
}

static CSSValue* consumeSelfPositionOverflowPosition(CSSParserTokenRange& range)
{
    if (identMatches<CSSValueAuto, CSSValueNormal, CSSValueStretch, CSSValueBaseline, CSSValueLastBaseline>(range.peek().id()))
        return consumeIdent(range);

    CSSPrimitiveValue* overflowPosition = consumeIdent<CSSValueUnsafe, CSSValueSafe>(range);
    CSSPrimitiveValue* selfPosition = consumeSelfPositionKeyword(range);
    if (!selfPosition)
        return nullptr;
    if (!overflowPosition)
        overflowPosition = consumeIdent<CSSValueUnsafe, CSSValueSafe>(range);
    if (overflowPosition)
        return CSSValuePair::create(selfPosition, overflowPosition, CSSValuePair::DropIdenticalValues);
    return selfPosition;
}

static CSSValue* consumeAlignItems(CSSParserTokenRange& range)
{
    // align-items property does not allow the 'auto' value.
    if (identMatches<CSSValueAuto>(range.peek().id()))
        return nullptr;
    return consumeSelfPositionOverflowPosition(range);
}

static CSSValue* consumeJustifyItems(CSSParserTokenRange& range)
{
    CSSParserTokenRange rangeCopy = range;
    CSSPrimitiveValue* legacy = consumeIdent<CSSValueLegacy>(rangeCopy);
    CSSPrimitiveValue* positionKeyword = consumeIdent<CSSValueCenter, CSSValueLeft, CSSValueRight>(rangeCopy);
    if (!legacy)
        legacy = consumeIdent<CSSValueLegacy>(rangeCopy);
    if (legacy && positionKeyword) {
        range = rangeCopy;
        return CSSValuePair::create(legacy, positionKeyword, CSSValuePair::DropIdenticalValues);
    }
    return consumeSelfPositionOverflowPosition(range);
}

static CSSCustomIdentValue* consumeCustomIdentForGridLine(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto || range.peek().id() == CSSValueSpan)
        return nullptr;
    return consumeCustomIdent(range);
}

static CSSValue* consumeGridLine(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);

    CSSPrimitiveValue* spanValue = nullptr;
    CSSCustomIdentValue* gridLineName = nullptr;
    CSSPrimitiveValue* numericValue = consumeInteger(range);
    if (numericValue) {
        gridLineName = consumeCustomIdentForGridLine(range);
        spanValue = consumeIdent<CSSValueSpan>(range);
    } else {
        spanValue = consumeIdent<CSSValueSpan>(range);
        if (spanValue) {
            numericValue = consumeInteger(range);
            gridLineName = consumeCustomIdentForGridLine(range);
            if (!numericValue)
                numericValue = consumeInteger(range);
        } else {
            gridLineName = consumeCustomIdentForGridLine(range);
            if (gridLineName) {
                numericValue = consumeInteger(range);
                spanValue = consumeIdent<CSSValueSpan>(range);
                if (!spanValue && !numericValue)
                    return gridLineName;
            } else {
                return nullptr;
            }
        }
    }

    if (spanValue && !numericValue && !gridLineName)
        return nullptr; // "span" keyword alone is invalid.
    if (spanValue && numericValue && numericValue->getIntValue() < 0)
        return nullptr; // Negative numbers are not allowed for span.
    if (numericValue && numericValue->getIntValue() == 0)
        return nullptr; // An <integer> value of zero makes the declaration invalid.

    CSSValueList* values = CSSValueList::createSpaceSeparated();
    if (spanValue)
        values->append(*spanValue);
    if (numericValue)
        values->append(*numericValue);
    if (gridLineName)
        values->append(*gridLineName);
    ASSERT(values->length());
    return values;
}

static bool isGridTrackFixedSized(const CSSPrimitiveValue& primitiveValue)
{
    CSSValueID valueID = primitiveValue.getValueID();
    if (valueID == CSSValueMinContent || valueID == CSSValueMaxContent || valueID == CSSValueAuto || primitiveValue.isFlex())
        return false;

    return true;
}

static bool isGridTrackFixedSized(const CSSValue& value)
{
    if (value.isPrimitiveValue())
        return isGridTrackFixedSized(toCSSPrimitiveValue(value));

    const CSSPrimitiveValue& minPrimitiveValue = toCSSPrimitiveValue(toCSSFunctionValue(value).item(0));
    const CSSPrimitiveValue& maxPrimitiveValue = toCSSPrimitiveValue(toCSSFunctionValue(value).item(1));
    return isGridTrackFixedSized(minPrimitiveValue) || isGridTrackFixedSized(maxPrimitiveValue);
}

static Vector<String> parseGridTemplateAreasColumnNames(const String& gridRowNames)
{
    ASSERT(!gridRowNames.isEmpty());
    Vector<String> columnNames;
    // Using StringImpl to avoid checks and indirection in every call to String::operator[].
    StringImpl& text = *gridRowNames.impl();

    StringBuilder areaName;
    for (unsigned i = 0; i < text.length(); ++i) {
        if (isCSSSpace(text[i])) {
            if (!areaName.isEmpty()) {
                columnNames.append(areaName.toString());
                areaName.clear();
            }
            continue;
        }
        if (text[i] == '.') {
            if (areaName == ".")
                continue;
            if (!areaName.isEmpty()) {
                columnNames.append(areaName.toString());
                areaName.clear();
            }
        } else {
            if (!isNameCodePoint(text[i]))
                return Vector<String>();
            if (areaName == ".") {
                columnNames.append(areaName.toString());
                areaName.clear();
            }
        }

        areaName.append(text[i]);
    }

    if (!areaName.isEmpty())
        columnNames.append(areaName.toString());

    return columnNames;
}

static bool parseGridTemplateAreasRow(const String& gridRowNames, NamedGridAreaMap& gridAreaMap, const size_t rowCount, size_t& columnCount)
{
    if (gridRowNames.isEmpty() || gridRowNames.containsOnlyWhitespace())
        return false;

    Vector<String> columnNames = parseGridTemplateAreasColumnNames(gridRowNames);
    if (rowCount == 0) {
        columnCount = columnNames.size();
        if (columnCount == 0)
            return false;
    } else if (columnCount != columnNames.size()) {
        // The declaration is invalid if all the rows don't have the number of columns.
        return false;
    }

    for (size_t currentColumn = 0; currentColumn < columnCount; ++currentColumn) {
        const String& gridAreaName = columnNames[currentColumn];

        // Unamed areas are always valid (we consider them to be 1x1).
        if (gridAreaName == ".")
            continue;

        size_t lookAheadColumn = currentColumn + 1;
        while (lookAheadColumn < columnCount && columnNames[lookAheadColumn] == gridAreaName)
            lookAheadColumn++;

        NamedGridAreaMap::iterator gridAreaIt = gridAreaMap.find(gridAreaName);
        if (gridAreaIt == gridAreaMap.end()) {
            gridAreaMap.add(gridAreaName, GridArea(GridSpan::translatedDefiniteGridSpan(rowCount, rowCount + 1), GridSpan::translatedDefiniteGridSpan(currentColumn, lookAheadColumn)));
        } else {
            GridArea& gridArea = gridAreaIt->value;

            // The following checks test that the grid area is a single filled-in rectangle.
            // 1. The new row is adjacent to the previously parsed row.
            if (rowCount != gridArea.rows.endLine())
                return false;

            // 2. The new area starts at the same position as the previously parsed area.
            if (currentColumn != gridArea.columns.startLine())
                return false;

            // 3. The new area ends at the same position as the previously parsed area.
            if (lookAheadColumn != gridArea.columns.endLine())
                return false;

            gridArea.rows = GridSpan::translatedDefiniteGridSpan(gridArea.rows.startLine(), gridArea.rows.endLine() + 1);
        }
        currentColumn = lookAheadColumn - 1;
    }

    return true;
}

static CSSPrimitiveValue* consumeGridBreadth(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    const CSSParserToken& token = range.peek();
    if (identMatches<CSSValueMinContent, CSSValueMaxContent, CSSValueAuto>(token.id()))
        return consumeIdent(range);
    if (token.type() == DimensionToken && token.unitType() == CSSPrimitiveValue::UnitType::Fraction) {
        if (range.peek().numericValue() < 0)
            return nullptr;
        return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().numericValue(), CSSPrimitiveValue::UnitType::Fraction);
    }
    return consumeLengthOrPercent(range, cssParserMode, ValueRangeNonNegative, UnitlessQuirk::Allow);
}

static CSSValue* consumeGridTrackSize(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    const CSSParserToken& token = range.peek();
    if (identMatches<CSSValueAuto>(token.id()))
        return consumeIdent(range);

    if (token.functionId() == CSSValueMinmax) {
        CSSParserTokenRange rangeCopy = range;
        CSSParserTokenRange args = consumeFunction(rangeCopy);
        CSSPrimitiveValue* minTrackBreadth = consumeGridBreadth(args, cssParserMode);
        if (!minTrackBreadth || minTrackBreadth->isFlex() || !consumeCommaIncludingWhitespace(args))
            return nullptr;
        CSSPrimitiveValue* maxTrackBreadth = consumeGridBreadth(args, cssParserMode);
        if (!maxTrackBreadth || !args.atEnd())
            return nullptr;
        range = rangeCopy;
        CSSFunctionValue* result = CSSFunctionValue::create(CSSValueMinmax);
        result->append(*minTrackBreadth);
        result->append(*maxTrackBreadth);
        return result;
    }
    return consumeGridBreadth(range, cssParserMode);
}

// Appends to the passed in CSSGridLineNamesValue if any, otherwise creates a new one.
static CSSGridLineNamesValue* consumeGridLineNames(CSSParserTokenRange& range, CSSGridLineNamesValue* lineNames = nullptr)
{
    CSSParserTokenRange rangeCopy = range;
    if (rangeCopy.consumeIncludingWhitespace().type() != LeftBracketToken)
        return nullptr;
    if (!lineNames)
        lineNames = CSSGridLineNamesValue::create();
    while (CSSCustomIdentValue* lineName = consumeCustomIdentForGridLine(rangeCopy))
        lineNames->append(*lineName);
    if (rangeCopy.consumeIncludingWhitespace().type() != RightBracketToken)
        return nullptr;
    range = rangeCopy;
    return lineNames;
}

static bool consumeGridTrackRepeatFunction(CSSParserTokenRange& range, CSSParserMode cssParserMode, CSSValueList& list, bool& isAutoRepeat, bool& allTracksAreFixedSized)
{
    CSSParserTokenRange args = consumeFunction(range);
    // The number of repetitions for <auto-repeat> is not important at parsing level
    // because it will be computed later, let's set it to 1.
    size_t repetitions = 1;
    isAutoRepeat = identMatches<CSSValueAutoFill, CSSValueAutoFit>(args.peek().id());
    CSSValueList* repeatedValues;
    if (isAutoRepeat) {
        repeatedValues = CSSGridAutoRepeatValue::create(args.consumeIncludingWhitespace().id());
    } else {
        // FIXME: a consumeIntegerRaw would be more efficient here.
        CSSPrimitiveValue* repetition = consumePositiveInteger(args);
        if (!repetition)
            return false;
        repetitions = clampTo<size_t>(repetition->getDoubleValue(), 0, kGridMaxTracks);
        repeatedValues = CSSValueList::createSpaceSeparated();
    }
    if (!consumeCommaIncludingWhitespace(args))
        return false;
    CSSGridLineNamesValue* lineNames = consumeGridLineNames(args);
    if (lineNames)
        repeatedValues->append(*lineNames);

    size_t numberOfTracks = 0;
    while (!args.atEnd()) {
        CSSValue* trackSize = consumeGridTrackSize(args, cssParserMode);
        if (!trackSize)
            return false;
        if (allTracksAreFixedSized)
            allTracksAreFixedSized = isGridTrackFixedSized(*trackSize);
        repeatedValues->append(*trackSize);
        ++numberOfTracks;
        lineNames = consumeGridLineNames(args);
        if (lineNames)
            repeatedValues->append(*lineNames);
    }
    // We should have found at least one <track-size> or else it is not a valid <track-list>.
    if (!numberOfTracks)
        return false;

    if (isAutoRepeat) {
        list.append(*repeatedValues);
    } else {
        // We clamp the repetitions to a multiple of the repeat() track list's size, while staying below the max grid size.
        repetitions = std::min(repetitions, kGridMaxTracks / numberOfTracks);
        for (size_t i = 0; i < repetitions; ++i) {
            for (size_t j = 0; j < repeatedValues->length(); ++j)
                list.append(repeatedValues->item(j));
        }
    }
    return true;
}

enum TrackListType { GridTemplate, GridTemplateNoRepeat, GridAuto };

static CSSValue* consumeGridTrackList(CSSParserTokenRange& range, CSSParserMode cssParserMode, TrackListType trackListType)
{
    bool allowGridLineNames = trackListType != GridAuto;
    CSSValueList* values = CSSValueList::createSpaceSeparated();
    CSSGridLineNamesValue* lineNames = consumeGridLineNames(range);
    if (lineNames) {
        if (!allowGridLineNames)
            return nullptr;
        values->append(*lineNames);
    }

    bool allowRepeat = trackListType == GridTemplate;
    bool seenAutoRepeat = false;
    bool allTracksAreFixedSized = true;
    do {
        bool isAutoRepeat;
        if (range.peek().functionId() == CSSValueRepeat) {
            if (!allowRepeat)
                return nullptr;
            if (!consumeGridTrackRepeatFunction(range, cssParserMode, *values, isAutoRepeat, allTracksAreFixedSized))
                return nullptr;
            if (isAutoRepeat && seenAutoRepeat)
                return nullptr;
            seenAutoRepeat = seenAutoRepeat || isAutoRepeat;
        } else if (CSSValue* value = consumeGridTrackSize(range, cssParserMode)) {
            if (allTracksAreFixedSized)
                allTracksAreFixedSized = isGridTrackFixedSized(*value);
            values->append(*value);
        } else {
            return nullptr;
        }
        if (seenAutoRepeat && !allTracksAreFixedSized)
            return nullptr;
        lineNames = consumeGridLineNames(range);
        if (lineNames) {
            if (!allowGridLineNames)
                return nullptr;
            values->append(*lineNames);
        }
    } while (!range.atEnd() && range.peek().type() != DelimiterToken);
    return values;
}

static CSSValue* consumeGridTemplatesRowsOrColumns(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    return consumeGridTrackList(range, cssParserMode, GridTemplate);
}

static CSSValue* consumeGridTemplateAreas(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    NamedGridAreaMap gridAreaMap;
    size_t rowCount = 0;
    size_t columnCount = 0;

    while (range.peek().type() == StringToken) {
        if (!parseGridTemplateAreasRow(range.consumeIncludingWhitespace().value().toString(), gridAreaMap, rowCount, columnCount))
            return nullptr;
        ++rowCount;
    }

    if (rowCount == 0)
        return nullptr;
    ASSERT(columnCount);
    return CSSGridTemplateAreasValue::create(gridAreaMap, rowCount, columnCount);
}

static void countKeywordOnlyPropertyUsage(CSSPropertyID property, UseCounter* counter, CSSValueID valueID)
{
    if (!counter)
        return;
    switch (property) {
    case CSSPropertyWebkitAppearance:
        if (valueID == CSSValueNone) {
            counter->count(UseCounter::CSSValueAppearanceNone);
        } else {
            counter->count(UseCounter::CSSValueAppearanceNotNone);
            if (valueID == CSSValueButton)
                counter->count(UseCounter::CSSValueAppearanceButton);
            else if (valueID == CSSValueCaret)
                counter->count(UseCounter::CSSValueAppearanceCaret);
            else if (valueID == CSSValueCheckbox)
                counter->count(UseCounter::CSSValueAppearanceCheckbox);
            else if (valueID == CSSValueMenulist)
                counter->count(UseCounter::CSSValueAppearanceMenulist);
            else if (valueID == CSSValueMenulistButton)
                counter->count(UseCounter::CSSValueAppearanceMenulistButton);
            else if (valueID == CSSValueListbox)
                counter->count(UseCounter::CSSValueAppearanceListbox);
            else if (valueID == CSSValueRadio)
                counter->count(UseCounter::CSSValueAppearanceRadio);
            else if (valueID == CSSValueSearchfield)
                counter->count(UseCounter::CSSValueAppearanceSearchField);
            else if (valueID == CSSValueTextfield)
                counter->count(UseCounter::CSSValueAppearanceTextField);
            else
                counter->count(UseCounter::CSSValueAppearanceOthers);
        }
        break;

    default:
        break;
    }
}

const CSSValue* CSSPropertyParser::parseSingleValue(CSSPropertyID unresolvedProperty, CSSPropertyID currentShorthand)
{
    CSSPropertyID property = resolveCSSPropertyID(unresolvedProperty);
    if (CSSParserFastPaths::isKeywordPropertyID(property)) {
        if (!CSSParserFastPaths::isValidKeywordPropertyAndValue(property, m_range.peek().id(), m_context.mode()))
            return nullptr;
        countKeywordOnlyPropertyUsage(property, m_context.useCounter(), m_range.peek().id());
        return consumeIdent(m_range);
    }
    switch (property) {
    case CSSPropertyWillChange:
        return consumeWillChange(m_range);
    case CSSPropertyPage:
        return consumePage(m_range);
    case CSSPropertyQuotes:
        return consumeQuotes(m_range);
    case CSSPropertyWebkitHighlight:
        return consumeWebkitHighlight(m_range);
    case CSSPropertyFontVariantCaps:
        return consumeFontVariantCaps(m_range);
    case CSSPropertyFontVariantLigatures:
        return consumeFontVariantLigatures(m_range);
    case CSSPropertyFontVariantNumeric:
        return consumeFontVariantNumeric(m_range);
    case CSSPropertyFontFeatureSettings:
        return consumeFontFeatureSettings(m_range);
    case CSSPropertyFontFamily:
        return consumeFontFamily(m_range);
    case CSSPropertyFontWeight:
        return consumeFontWeight(m_range);
    case CSSPropertyLetterSpacing:
    case CSSPropertyWordSpacing:
        return consumeSpacing(m_range, m_context.mode());
    case CSSPropertyTabSize:
        return consumeTabSize(m_range, m_context.mode());
    case CSSPropertyTextSizeAdjust:
        return consumeTextSizeAdjust(m_range, m_context.mode());
    case CSSPropertyFontSize:
        return consumeFontSize(m_range, m_context.mode(), UnitlessQuirk::Allow);
    case CSSPropertyLineHeight:
        return consumeLineHeight(m_range, m_context.mode());
    case CSSPropertyRotate:
        return consumeRotation(m_range);
    case CSSPropertyScale:
        return consumeScale(m_range);
    case CSSPropertyTranslate:
        return consumeTranslate(m_range, m_context.mode());
    case CSSPropertyWebkitBorderHorizontalSpacing:
    case CSSPropertyWebkitBorderVerticalSpacing:
        return consumeLength(m_range, m_context.mode(), ValueRangeNonNegative);
    case CSSPropertyCounterIncrement:
    case CSSPropertyCounterReset:
        return consumeCounter(m_range, property == CSSPropertyCounterIncrement ? 1 : 0);
    case CSSPropertySize:
        return consumeSize(m_range, m_context.mode());
    case CSSPropertySnapHeight:
        return consumeSnapHeight(m_range, m_context.mode());
    case CSSPropertyTextIndent:
        return consumeTextIndent(m_range, m_context.mode());
    case CSSPropertyMaxWidth:
    case CSSPropertyMaxHeight:
        return consumeMaxWidthOrHeight(m_range, m_context, UnitlessQuirk::Allow);
    case CSSPropertyWebkitMaxLogicalWidth:
    case CSSPropertyWebkitMaxLogicalHeight:
        return consumeMaxWidthOrHeight(m_range, m_context);
    case CSSPropertyMinWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyWidth:
    case CSSPropertyHeight:
        return consumeWidthOrHeight(m_range, m_context, UnitlessQuirk::Allow);
    case CSSPropertyWebkitMinLogicalWidth:
    case CSSPropertyWebkitMinLogicalHeight:
    case CSSPropertyWebkitLogicalWidth:
    case CSSPropertyWebkitLogicalHeight:
        return consumeWidthOrHeight(m_range, m_context);
    case CSSPropertyMarginTop:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
    case CSSPropertyBottom:
    case CSSPropertyLeft:
    case CSSPropertyRight:
    case CSSPropertyTop:
        return consumeMarginOrOffset(m_range, m_context.mode(), UnitlessQuirk::Allow);
    case CSSPropertyWebkitMarginStart:
    case CSSPropertyWebkitMarginEnd:
    case CSSPropertyWebkitMarginBefore:
    case CSSPropertyWebkitMarginAfter:
        return consumeMarginOrOffset(m_range, m_context.mode(), UnitlessQuirk::Forbid);
    case CSSPropertyPaddingTop:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
        return consumeLengthOrPercent(m_range, m_context.mode(), ValueRangeNonNegative, UnitlessQuirk::Allow);
    case CSSPropertyWebkitPaddingStart:
    case CSSPropertyWebkitPaddingEnd:
    case CSSPropertyWebkitPaddingBefore:
    case CSSPropertyWebkitPaddingAfter:
        return consumeLengthOrPercent(m_range, m_context.mode(), ValueRangeNonNegative, UnitlessQuirk::Forbid);
    case CSSPropertyClip:
        return consumeClip(m_range, m_context.mode());
    case CSSPropertyTouchAction:
        return consumeTouchAction(m_range);
    case CSSPropertyScrollSnapDestination:
    case CSSPropertyObjectPosition:
    case CSSPropertyPerspectiveOrigin:
        return consumePosition(m_range, m_context.mode(), UnitlessQuirk::Forbid);
    case CSSPropertyWebkitLineClamp:
        return consumeLineClamp(m_range);
    case CSSPropertyWebkitFontSizeDelta:
        return consumeLength(m_range, m_context.mode(), ValueRangeAll, UnitlessQuirk::Allow);
    case CSSPropertyWebkitHyphenateCharacter:
    case CSSPropertyWebkitLocale:
        return consumeLocale(m_range);
    case CSSPropertyColumnWidth:
        return consumeColumnWidth(m_range);
    case CSSPropertyColumnCount:
        return consumeColumnCount(m_range);
    case CSSPropertyColumnGap:
        return consumeColumnGap(m_range, m_context.mode());
    case CSSPropertyColumnSpan:
        return consumeColumnSpan(m_range);
    case CSSPropertyZoom:
        return consumeZoom(m_range, m_context);
    case CSSPropertyAnimationDelay:
    case CSSPropertyTransitionDelay:
    case CSSPropertyAnimationDirection:
    case CSSPropertyAnimationDuration:
    case CSSPropertyTransitionDuration:
    case CSSPropertyAnimationFillMode:
    case CSSPropertyAnimationIterationCount:
    case CSSPropertyAnimationName:
    case CSSPropertyAnimationPlayState:
    case CSSPropertyTransitionProperty:
    case CSSPropertyAnimationTimingFunction:
    case CSSPropertyTransitionTimingFunction:
        return consumeAnimationPropertyList(property, m_range, m_context, unresolvedProperty == CSSPropertyAliasWebkitAnimationName);
    case CSSPropertyGridColumnGap:
    case CSSPropertyGridRowGap:
        return consumeLength(m_range, m_context.mode(), ValueRangeNonNegative);
    case CSSPropertyShapeMargin:
        return consumeLengthOrPercent(m_range, m_context.mode(), ValueRangeNonNegative);
    case CSSPropertyShapeImageThreshold:
        return consumeNumber(m_range, ValueRangeAll);
    case CSSPropertyWebkitBoxOrdinalGroup:
    case CSSPropertyOrphans:
    case CSSPropertyWidows:
        return consumePositiveInteger(m_range);
    case CSSPropertyTextDecorationColor:
        ASSERT(RuntimeEnabledFeatures::css3TextDecorationsEnabled());
        return consumeColor(m_range, m_context.mode());
    case CSSPropertyWebkitTextStrokeWidth:
        return consumeTextStrokeWidth(m_range, m_context.mode());
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTapHighlightColor:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitBorderStartColor:
    case CSSPropertyWebkitBorderEndColor:
    case CSSPropertyWebkitBorderBeforeColor:
    case CSSPropertyWebkitBorderAfterColor:
    case CSSPropertyWebkitTextStrokeColor:
    case CSSPropertyStopColor:
    case CSSPropertyFloodColor:
    case CSSPropertyLightingColor:
    case CSSPropertyColumnRuleColor:
        return consumeColor(m_range, m_context.mode());
    case CSSPropertyColor:
    case CSSPropertyBackgroundColor:
        return consumeColor(m_range, m_context.mode(), inQuirksMode());
    case CSSPropertyWebkitBorderStartWidth:
    case CSSPropertyWebkitBorderEndWidth:
    case CSSPropertyWebkitBorderBeforeWidth:
    case CSSPropertyWebkitBorderAfterWidth:
        return consumeBorderWidth(m_range, m_context.mode(), UnitlessQuirk::Forbid);
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor: {
        bool allowQuirkyColors = inQuirksMode()
            && (currentShorthand == CSSPropertyInvalid || currentShorthand == CSSPropertyBorderColor);
        return consumeColor(m_range, m_context.mode(), allowQuirkyColors);
    }
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderTopWidth: {
        bool allowQuirkyLengths = inQuirksMode()
            && (currentShorthand == CSSPropertyInvalid || currentShorthand == CSSPropertyBorderWidth);
        UnitlessQuirk unitless = allowQuirkyLengths ? UnitlessQuirk::Allow : UnitlessQuirk::Forbid;
        return consumeBorderWidth(m_range, m_context.mode(), unitless);
    }
    case CSSPropertyZIndex:
        return consumeZIndex(m_range);
    case CSSPropertyTextShadow: // CSS2 property, dropped in CSS2.1, back in CSS3, so treat as CSS3
    case CSSPropertyBoxShadow:
        return consumeShadow(m_range, m_context.mode(), property == CSSPropertyBoxShadow);
    case CSSPropertyFilter:
    case CSSPropertyBackdropFilter:
        return consumeFilter(m_range, m_context);
    case CSSPropertyTextDecoration:
        ASSERT(!RuntimeEnabledFeatures::css3TextDecorationsEnabled());
        // fallthrough
    case CSSPropertyWebkitTextDecorationsInEffect:
    case CSSPropertyTextDecorationLine:
        return consumeTextDecorationLine(m_range);
    case CSSPropertyD:
    case CSSPropertyMotionPath:
        return consumePathOrNone(m_range);
    case CSSPropertyMotionOffset:
        return consumeLengthOrPercent(m_range, m_context.mode(), ValueRangeAll);
    case CSSPropertyMotionRotation:
        return consumeMotionRotation(m_range);
    case CSSPropertyWebkitTextEmphasisStyle:
        return consumeTextEmphasisStyle(m_range);
    case CSSPropertyOutlineColor:
        return consumeOutlineColor(m_range, m_context.mode());
    case CSSPropertyOutlineOffset:
        return consumeLength(m_range, m_context.mode(), ValueRangeAll);
    case CSSPropertyOutlineWidth:
        return consumeLineWidth(m_range, m_context.mode(), UnitlessQuirk::Forbid);
    case CSSPropertyTransform:
        return consumeTransform(m_range, m_context.mode(), unresolvedProperty == CSSPropertyAliasWebkitTransform);
    case CSSPropertyWebkitTransformOriginX:
    case CSSPropertyWebkitPerspectiveOriginX:
        return consumePositionX(m_range, m_context.mode());
    case CSSPropertyWebkitTransformOriginY:
    case CSSPropertyWebkitPerspectiveOriginY:
        return consumePositionY(m_range, m_context.mode());
    case CSSPropertyWebkitTransformOriginZ:
        return consumeLength(m_range, m_context.mode(), ValueRangeAll);
    case CSSPropertyFill:
    case CSSPropertyStroke:
        return consumePaintStroke(m_range, m_context.mode());
    case CSSPropertyPaintOrder:
        return consumePaintOrder(m_range);
    case CSSPropertyMarkerStart:
    case CSSPropertyMarkerMid:
    case CSSPropertyMarkerEnd:
    case CSSPropertyClipPath:
    case CSSPropertyMask:
        return consumeNoneOrURI(m_range);
    case CSSPropertyFlexBasis:
        return consumeFlexBasis(m_range, m_context.mode());
    case CSSPropertyFlexGrow:
    case CSSPropertyFlexShrink:
        return consumeNumber(m_range, ValueRangeNonNegative);
    case CSSPropertyStrokeDasharray:
        return consumeStrokeDasharray(m_range);
    case CSSPropertyColumnRuleWidth:
        return consumeColumnRuleWidth(m_range, m_context.mode());
    case CSSPropertyStrokeOpacity:
    case CSSPropertyFillOpacity:
    case CSSPropertyStopOpacity:
    case CSSPropertyFloodOpacity:
    case CSSPropertyOpacity:
    case CSSPropertyWebkitBoxFlex:
        return consumeNumber(m_range, ValueRangeAll);
    case CSSPropertyBaselineShift:
        return consumeBaselineShift(m_range);
    case CSSPropertyStrokeMiterlimit:
        return consumeNumber(m_range, ValueRangeNonNegative);
    case CSSPropertyStrokeWidth:
    case CSSPropertyStrokeDashoffset:
    case CSSPropertyCx:
    case CSSPropertyCy:
    case CSSPropertyX:
    case CSSPropertyY:
    case CSSPropertyR:
        return consumeLengthOrPercent(m_range, SVGAttributeMode, ValueRangeAll, UnitlessQuirk::Forbid);
    case CSSPropertyRx:
    case CSSPropertyRy:
        return consumeRxOrRy(m_range);
    case CSSPropertyCursor:
        return consumeCursor(m_range, m_context, inQuirksMode());
    case CSSPropertyContain:
        return consumeContain(m_range);
    case CSSPropertyTransformOrigin:
        return consumeTransformOrigin(m_range, m_context.mode(), UnitlessQuirk::Forbid);
    case CSSPropertyContent:
        return consumeContent(m_range, m_context);
    case CSSPropertyListStyleImage:
    case CSSPropertyBorderImageSource:
    case CSSPropertyWebkitMaskBoxImageSource:
        return consumeImageOrNone(m_range, m_context);
    case CSSPropertyPerspective:
        return consumePerspective(m_range, m_context.mode(), unresolvedProperty);
    case CSSPropertyScrollSnapCoordinate:
        return consumeScrollSnapCoordinate(m_range, m_context.mode());
    case CSSPropertyScrollSnapPointsX:
    case CSSPropertyScrollSnapPointsY:
        return consumeScrollSnapPoints(m_range, m_context.mode());
    case CSSPropertyBorderTopRightRadius:
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderBottomLeftRadius:
    case CSSPropertyBorderBottomRightRadius:
        return consumeBorderRadiusCorner(m_range, m_context.mode());
    case CSSPropertyWebkitBoxFlexGroup:
        return consumeInteger(m_range, 0);
    case CSSPropertyOrder:
        return consumeInteger(m_range);
    case CSSPropertyTextUnderlinePosition:
        // auto | [ under || [ left | right ] ], but we only support auto | under for now
        ASSERT(RuntimeEnabledFeatures::css3TextDecorationsEnabled());
        return consumeIdent<CSSValueAuto, CSSValueUnder>(m_range);
    case CSSPropertyVerticalAlign:
        return consumeVerticalAlign(m_range, m_context.mode());
    case CSSPropertyShapeOutside:
        return consumeShapeOutside(m_range, m_context);
    case CSSPropertyWebkitClipPath:
        return consumeWebkitClipPath(m_range, m_context);
    case CSSPropertyJustifyContent:
    case CSSPropertyAlignContent:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        return consumeContentDistributionOverflowPosition(m_range);
    case CSSPropertyBorderImageRepeat:
    case CSSPropertyWebkitMaskBoxImageRepeat:
        return consumeBorderImageRepeat(m_range);
    case CSSPropertyBorderImageSlice:
    case CSSPropertyWebkitMaskBoxImageSlice:
        return consumeBorderImageSlice(property, m_range);
    case CSSPropertyBorderImageOutset:
    case CSSPropertyWebkitMaskBoxImageOutset:
        return consumeBorderImageOutset(m_range);
    case CSSPropertyBorderImageWidth:
    case CSSPropertyWebkitMaskBoxImageWidth:
        return consumeBorderImageWidth(m_range);
    case CSSPropertyWebkitBorderImage:
        return consumeWebkitBorderImage(property, m_range, m_context);
    case CSSPropertyWebkitBoxReflect:
        return consumeReflect(m_range, m_context);
    case CSSPropertyFontSizeAdjust:
        ASSERT(RuntimeEnabledFeatures::cssFontSizeAdjustEnabled());
        return consumeFontSizeAdjust(m_range);
    case CSSPropertyImageOrientation:
        ASSERT(RuntimeEnabledFeatures::imageOrientationEnabled());
        return consumeImageOrientation(m_range);
    case CSSPropertyBackgroundAttachment:
    case CSSPropertyBackgroundBlendMode:
    case CSSPropertyBackgroundClip:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyBackgroundSize:
    case CSSPropertyMaskSourceType:
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyWebkitMaskClip:
    case CSSPropertyWebkitMaskComposite:
    case CSSPropertyWebkitMaskImage:
    case CSSPropertyWebkitMaskOrigin:
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
    case CSSPropertyWebkitMaskSize:
        return consumeCommaSeparatedBackgroundComponent(unresolvedProperty, m_range, m_context);
    case CSSPropertyWebkitMaskRepeatX:
    case CSSPropertyWebkitMaskRepeatY:
        return nullptr;
    case CSSPropertyAlignItems:
        DCHECK(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        return consumeAlignItems(m_range);
    case CSSPropertyJustifySelf:
    case CSSPropertyAlignSelf:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        return consumeSelfPositionOverflowPosition(m_range);
    case CSSPropertyJustifyItems:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        return consumeJustifyItems(m_range);
    case CSSPropertyGridColumnEnd:
    case CSSPropertyGridColumnStart:
    case CSSPropertyGridRowEnd:
    case CSSPropertyGridRowStart:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        return consumeGridLine(m_range);
    case CSSPropertyGridAutoColumns:
    case CSSPropertyGridAutoRows:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        return consumeGridTrackList(m_range, m_context.mode(), GridAuto);
    case CSSPropertyGridTemplateColumns:
    case CSSPropertyGridTemplateRows:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        return consumeGridTemplatesRowsOrColumns(m_range, m_context.mode());
    case CSSPropertyGridTemplateAreas:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        return consumeGridTemplateAreas(m_range);
    case CSSPropertyGridAutoFlow:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        return consumeGridAutoFlow(m_range);
    default:
        return nullptr;
    }
}

static CSSPrimitiveValue* consumeFontDisplay(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueAuto, CSSValueBlock, CSSValueSwap, CSSValueFallback, CSSValueOptional>(range);
}

static CSSValueList* consumeFontFaceUnicodeRange(CSSParserTokenRange& range)
{
    CSSValueList* values = CSSValueList::createCommaSeparated();

    do {
        const CSSParserToken& token = range.consumeIncludingWhitespace();
        if (token.type() != UnicodeRangeToken)
            return nullptr;

        UChar32 start = token.unicodeRangeStart();
        UChar32 end = token.unicodeRangeEnd();
        if (start > end)
            return nullptr;
        values->append(*CSSUnicodeRangeValue::create(start, end));
    } while (consumeCommaIncludingWhitespace(range));

    return values;
}

static CSSValue* consumeFontFaceSrcURI(CSSParserTokenRange& range, const CSSParserContext& context)
{
    String url = consumeUrlAsStringView(range).toString();
    if (url.isNull())
        return nullptr;
    CSSFontFaceSrcValue* uriValue(CSSFontFaceSrcValue::create(url, context.completeURL(url), context.shouldCheckContentSecurityPolicy()));
    uriValue->setReferrer(context.referrer());

    if (range.peek().functionId() != CSSValueFormat)
        return uriValue;

    // FIXME: https://drafts.csswg.org/css-fonts says that format() contains a comma-separated list of strings,
    // but CSSFontFaceSrcValue stores only one format. Allowing one format for now.
    CSSParserTokenRange args = consumeFunction(range);
    const CSSParserToken& arg = args.consumeIncludingWhitespace();
    if ((arg.type() != StringToken) || !args.atEnd())
        return nullptr;
    uriValue->setFormat(arg.value().toString());
    return uriValue;
}

static CSSValue* consumeFontFaceSrcLocal(CSSParserTokenRange& range, const CSSParserContext& context)
{
    CSSParserTokenRange args = consumeFunction(range);
    ContentSecurityPolicyDisposition shouldCheckContentSecurityPolicy = context.shouldCheckContentSecurityPolicy();
    if (args.peek().type() == StringToken) {
        const CSSParserToken& arg = args.consumeIncludingWhitespace();
        if (!args.atEnd())
            return nullptr;
        return CSSFontFaceSrcValue::createLocal(arg.value().toString(), shouldCheckContentSecurityPolicy);
    }
    if (args.peek().type() == IdentToken) {
        String familyName = concatenateFamilyName(args);
        if (!args.atEnd())
            return nullptr;
        return CSSFontFaceSrcValue::createLocal(familyName, shouldCheckContentSecurityPolicy);
    }
    return nullptr;
}

static CSSValueList* consumeFontFaceSrc(CSSParserTokenRange& range, const CSSParserContext& context)
{
    CSSValueList* values = CSSValueList::createCommaSeparated();

    do {
        const CSSParserToken& token = range.peek();
        CSSValue* parsedValue = nullptr;
        if (token.functionId() == CSSValueLocal)
            parsedValue = consumeFontFaceSrcLocal(range, context);
        else
            parsedValue = consumeFontFaceSrcURI(range, context);
        if (!parsedValue)
            return nullptr;
        values->append(*parsedValue);
    } while (consumeCommaIncludingWhitespace(range));
    return values;
}

bool CSSPropertyParser::parseFontFaceDescriptor(CSSPropertyID propId)
{
    CSSValue* parsedValue = nullptr;
    switch (propId) {
    case CSSPropertyFontFamily:
        if (consumeGenericFamily(m_range))
            return false;
        parsedValue = consumeFamilyName(m_range);
        break;
    case CSSPropertySrc: // This is a list of urls or local references.
        parsedValue = consumeFontFaceSrc(m_range, m_context);
        break;
    case CSSPropertyUnicodeRange:
        parsedValue = consumeFontFaceUnicodeRange(m_range);
        break;
    case CSSPropertyFontDisplay:
        parsedValue = consumeFontDisplay(m_range);
        break;
    case CSSPropertyFontStretch:
    case CSSPropertyFontStyle: {
        CSSValueID id = m_range.consumeIncludingWhitespace().id();
        if (!CSSParserFastPaths::isValidKeywordPropertyAndValue(propId, id, m_context.mode()))
            return false;
        parsedValue = CSSPrimitiveValue::createIdentifier(id);
        break;
    }
    case CSSPropertyFontVariant:
        parsedValue = consumeFontVariantList(m_range);
        break;
    case CSSPropertyFontWeight:
        parsedValue = consumeFontWeight(m_range);
        break;
    case CSSPropertyFontFeatureSettings:
        parsedValue = consumeFontFeatureSettings(m_range);
        break;
    default:
        break;
    }

    if (!parsedValue || !m_range.atEnd())
        return false;

    addProperty(propId, CSSPropertyInvalid, *parsedValue, false);
    return true;
}

bool CSSPropertyParser::consumeSystemFont(bool important)
{
    CSSValueID systemFontID = m_range.consumeIncludingWhitespace().id();
    ASSERT(systemFontID >= CSSValueCaption && systemFontID <= CSSValueStatusBar);
    if (!m_range.atEnd())
        return false;

    FontStyle fontStyle = FontStyleNormal;
    FontWeight fontWeight = FontWeightNormal;
    float fontSize = 0;
    AtomicString fontFamily;
    LayoutTheme::theme().systemFont(systemFontID, fontStyle, fontWeight, fontSize, fontFamily);

    addProperty(CSSPropertyFontStyle, CSSPropertyFont, *CSSPrimitiveValue::createIdentifier(fontStyle == FontStyleItalic ? CSSValueItalic : CSSValueNormal), important);
    addProperty(CSSPropertyFontWeight, CSSPropertyFont, *CSSPrimitiveValue::create(fontWeight), important);
    addProperty(CSSPropertyFontSize, CSSPropertyFont, *CSSPrimitiveValue::create(fontSize, CSSPrimitiveValue::UnitType::Pixels), important);
    CSSValueList* fontFamilyList = CSSValueList::createCommaSeparated();
    fontFamilyList->append(*CSSFontFamilyValue::create(fontFamily));
    addProperty(CSSPropertyFontFamily, CSSPropertyFont, *fontFamilyList, important);

    addProperty(CSSPropertyFontStretch, CSSPropertyFont, *CSSPrimitiveValue::createIdentifier(CSSValueNormal), important);
    addProperty(CSSPropertyFontVariantCaps, CSSPropertyFont, *CSSPrimitiveValue::createIdentifier(CSSValueNormal), important);
    addProperty(CSSPropertyFontVariantLigatures, CSSPropertyFont, *CSSPrimitiveValue::createIdentifier(CSSValueNormal), important);
    addProperty(CSSPropertyFontVariantNumeric, CSSPropertyFont, *CSSPrimitiveValue::createIdentifier(CSSValueNormal), important);
    addProperty(CSSPropertyLineHeight, CSSPropertyFont, *CSSPrimitiveValue::createIdentifier(CSSValueNormal), important);
    return true;
}

bool CSSPropertyParser::consumeFont(bool important)
{
    // Let's check if there is an inherit or initial somewhere in the shorthand.
    CSSParserTokenRange range = m_range;
    while (!range.atEnd()) {
        CSSValueID id = range.consumeIncludingWhitespace().id();
        if (id == CSSValueInherit || id == CSSValueInitial)
            return false;
    }
    // Optional font-style, font-variant, font-stretch and font-weight.
    CSSPrimitiveValue* fontStyle = nullptr;
    CSSPrimitiveValue* fontVariantCaps = nullptr;
    CSSPrimitiveValue* fontWeight = nullptr;
    CSSPrimitiveValue* fontStretch = nullptr;
    while (!m_range.atEnd()) {
        CSSValueID id = m_range.peek().id();
        if (!fontStyle && CSSParserFastPaths::isValidKeywordPropertyAndValue(CSSPropertyFontStyle, id, m_context.mode())) {
            fontStyle = consumeIdent(m_range);
            continue;
        }
        if (!fontVariantCaps && (id == CSSValueNormal || id == CSSValueSmallCaps)) {
            // Font variant in the shorthand is particular, it only accepts normal or small-caps.
            // See https://drafts.csswg.org/css-fonts/#propdef-font
            fontVariantCaps = consumeFontVariantCSS21(m_range);
            if (fontVariantCaps)
                continue;
        }
        if (!fontWeight) {
            fontWeight = consumeFontWeight(m_range);
            if (fontWeight)
                continue;
        }
        if (!fontStretch && CSSParserFastPaths::isValidKeywordPropertyAndValue(CSSPropertyFontStretch, id, m_context.mode()))
            fontStretch = consumeIdent(m_range);
        else
            break;
    }

    if (m_range.atEnd())
        return false;

    addProperty(CSSPropertyFontStyle, CSSPropertyFont, fontStyle ? *fontStyle : *CSSPrimitiveValue::createIdentifier(CSSValueNormal), important);
    addProperty(CSSPropertyFontVariantCaps, CSSPropertyFont, fontVariantCaps ? *fontVariantCaps : *CSSPrimitiveValue::createIdentifier(CSSValueNormal), important);
    addProperty(CSSPropertyFontVariantLigatures, CSSPropertyFont, *CSSPrimitiveValue::createIdentifier(CSSValueNormal), important);
    addProperty(CSSPropertyFontVariantNumeric, CSSPropertyFont, *CSSPrimitiveValue::createIdentifier(CSSValueNormal), important);

    addProperty(CSSPropertyFontWeight, CSSPropertyFont, fontWeight ? *fontWeight : *CSSPrimitiveValue::createIdentifier(CSSValueNormal), important);
    addProperty(CSSPropertyFontStretch, CSSPropertyFont, fontStretch ? *fontStretch : *CSSPrimitiveValue::createIdentifier(CSSValueNormal), important);

    // Now a font size _must_ come.
    CSSValue* fontSize = consumeFontSize(m_range, m_context.mode());
    if (!fontSize || m_range.atEnd())
        return false;

    addProperty(CSSPropertyFontSize, CSSPropertyFont, *fontSize, important);

    if (consumeSlashIncludingWhitespace(m_range)) {
        CSSPrimitiveValue* lineHeight = consumeLineHeight(m_range, m_context.mode());
        if (!lineHeight)
            return false;
        addProperty(CSSPropertyLineHeight, CSSPropertyFont, *lineHeight, important);
    } else {
        addProperty(CSSPropertyLineHeight, CSSPropertyFont, *CSSPrimitiveValue::createIdentifier(CSSValueNormal), important);
    }

    // Font family must come now.
    CSSValue* parsedFamilyValue = consumeFontFamily(m_range);
    if (!parsedFamilyValue)
        return false;

    addProperty(CSSPropertyFontFamily, CSSPropertyFont, *parsedFamilyValue, important);

    // FIXME: http://www.w3.org/TR/2011/WD-css3-fonts-20110324/#font-prop requires that
    // "font-stretch", "font-size-adjust", and "font-kerning" be reset to their initial values
    // but we don't seem to support them at the moment. They should also be added here once implemented.
    return m_range.atEnd();
}

bool CSSPropertyParser::consumeFontVariantShorthand(bool important)
{
    if (identMatches<CSSValueNormal, CSSValueNone>(m_range.peek().id())) {
        addProperty(CSSPropertyFontVariantLigatures, CSSPropertyFontVariant, *consumeIdent(m_range), important);
        addProperty(CSSPropertyFontVariantCaps, CSSPropertyFontVariant, *CSSPrimitiveValue::createIdentifier(CSSValueNormal), important);
        return m_range.atEnd();
    }

    CSSPrimitiveValue* capsValue = nullptr;
    FontVariantLigaturesParser ligaturesParser;
    FontVariantNumericParser numericParser;
    do {
        FontVariantLigaturesParser::ParseResult ligaturesParseResult = ligaturesParser.consumeLigature(m_range);
        FontVariantNumericParser::ParseResult numericParseResult = numericParser.consumeNumeric(m_range);
        if (ligaturesParseResult == FontVariantLigaturesParser::ParseResult::ConsumedValue
            || numericParseResult == FontVariantNumericParser::ParseResult::ConsumedValue)
            continue;

        if (ligaturesParseResult == FontVariantLigaturesParser::ParseResult::DisallowedValue
            || numericParseResult == FontVariantNumericParser::ParseResult::DisallowedValue)
            return false;

        CSSValueID id = m_range.peek().id();
        switch (id) {
        case CSSValueSmallCaps:
        case CSSValueAllSmallCaps:
        case CSSValuePetiteCaps:
        case CSSValueAllPetiteCaps:
        case CSSValueUnicase:
        case CSSValueTitlingCaps:
            // Only one caps value permitted in font-variant grammar.
            if (capsValue)
                return false;
            capsValue = consumeIdent(m_range);
            break;
        default:
            return false;
        }
    } while (!m_range.atEnd());

    addProperty(CSSPropertyFontVariantLigatures, CSSPropertyFontVariant, *ligaturesParser.finalizeValue(), important);
    addProperty(CSSPropertyFontVariantNumeric, CSSPropertyFontVariant, *numericParser.finalizeValue(), important);
    addProperty(CSSPropertyFontVariantCaps, CSSPropertyFontVariant, capsValue ? *capsValue : *CSSPrimitiveValue::createIdentifier(CSSValueNormal), important);
    return true;
}

bool CSSPropertyParser::consumeBorderSpacing(bool important)
{
    CSSValue* horizontalSpacing = consumeLength(m_range, m_context.mode(), ValueRangeNonNegative, UnitlessQuirk::Allow);
    if (!horizontalSpacing)
        return false;
    CSSValue* verticalSpacing = horizontalSpacing;
    if (!m_range.atEnd())
        verticalSpacing = consumeLength(m_range, m_context.mode(), ValueRangeNonNegative, UnitlessQuirk::Allow);
    if (!verticalSpacing || !m_range.atEnd())
        return false;
    addProperty(CSSPropertyWebkitBorderHorizontalSpacing, CSSPropertyBorderSpacing, *horizontalSpacing, important);
    addProperty(CSSPropertyWebkitBorderVerticalSpacing, CSSPropertyBorderSpacing, *verticalSpacing, important);
    return true;
}

static CSSValue* consumeSingleViewportDescriptor(CSSParserTokenRange& range, CSSPropertyID propId, CSSParserMode cssParserMode)
{
    CSSValueID id = range.peek().id();
    switch (propId) {
    case CSSPropertyMinWidth:
    case CSSPropertyMaxWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMaxHeight:
        if (id == CSSValueAuto || id == CSSValueInternalExtendToZoom)
            return consumeIdent(range);
        return consumeLengthOrPercent(range, cssParserMode, ValueRangeNonNegative);
    case CSSPropertyMinZoom:
    case CSSPropertyMaxZoom:
    case CSSPropertyZoom: {
        if (id == CSSValueAuto)
            return consumeIdent(range);
        CSSValue* parsedValue = consumeNumber(range, ValueRangeNonNegative);
        if (parsedValue)
            return parsedValue;
        return consumePercent(range, ValueRangeNonNegative);
    }
    case CSSPropertyUserZoom:
        return consumeIdent<CSSValueZoom, CSSValueFixed>(range);
    case CSSPropertyOrientation:
        return consumeIdent<CSSValueAuto, CSSValuePortrait, CSSValueLandscape>(range);
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

bool CSSPropertyParser::parseViewportDescriptor(CSSPropertyID propId, bool important)
{
    ASSERT(RuntimeEnabledFeatures::cssViewportEnabled() || isUASheetBehavior(m_context.mode()));

    switch (propId) {
    case CSSPropertyWidth: {
        CSSValue* minWidth = consumeSingleViewportDescriptor(m_range, CSSPropertyMinWidth, m_context.mode());
        if (!minWidth)
            return false;
        CSSValue* maxWidth = minWidth;
        if (!m_range.atEnd())
            maxWidth = consumeSingleViewportDescriptor(m_range, CSSPropertyMaxWidth, m_context.mode());
        if (!maxWidth || !m_range.atEnd())
            return false;
        addProperty(CSSPropertyMinWidth, CSSPropertyInvalid, *minWidth, important);
        addProperty(CSSPropertyMaxWidth, CSSPropertyInvalid, *maxWidth, important);
        return true;
    }
    case CSSPropertyHeight: {
        CSSValue* minHeight = consumeSingleViewportDescriptor(m_range, CSSPropertyMinHeight, m_context.mode());
        if (!minHeight)
            return false;
        CSSValue* maxHeight = minHeight;
        if (!m_range.atEnd())
            maxHeight = consumeSingleViewportDescriptor(m_range, CSSPropertyMaxHeight, m_context.mode());
        if (!maxHeight || !m_range.atEnd())
            return false;
        addProperty(CSSPropertyMinHeight, CSSPropertyInvalid, *minHeight, important);
        addProperty(CSSPropertyMaxHeight, CSSPropertyInvalid, *maxHeight, important);
        return true;
    }
    case CSSPropertyMinWidth:
    case CSSPropertyMaxWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMaxHeight:
    case CSSPropertyMinZoom:
    case CSSPropertyMaxZoom:
    case CSSPropertyZoom:
    case CSSPropertyUserZoom:
    case CSSPropertyOrientation: {
        CSSValue* parsedValue = consumeSingleViewportDescriptor(m_range, propId, m_context.mode());
        if (!parsedValue || !m_range.atEnd())
            return false;
        addProperty(propId, CSSPropertyInvalid, *parsedValue, important);
        return true;
    }
    default:
        return false;
    }
}

static bool consumeColumnWidthOrCount(CSSParserTokenRange& range, CSSValue*& columnWidth, CSSValue*& columnCount)
{
    if (range.peek().id() == CSSValueAuto) {
        consumeIdent(range);
        return true;
    }
    if (!columnWidth) {
        columnWidth = consumeColumnWidth(range);
        if (columnWidth)
            return true;
    }
    if (!columnCount)
        columnCount = consumeColumnCount(range);
    return columnCount;
}

bool CSSPropertyParser::consumeColumns(bool important)
{
    CSSValue* columnWidth = nullptr;
    CSSValue* columnCount = nullptr;
    if (!consumeColumnWidthOrCount(m_range, columnWidth, columnCount))
        return false;
    consumeColumnWidthOrCount(m_range, columnWidth, columnCount);
    if (!m_range.atEnd())
        return false;
    if (!columnWidth)
        columnWidth = CSSPrimitiveValue::createIdentifier(CSSValueAuto);
    if (!columnCount)
        columnCount = CSSPrimitiveValue::createIdentifier(CSSValueAuto);
    addProperty(CSSPropertyColumnWidth, CSSPropertyInvalid, *columnWidth, important);
    addProperty(CSSPropertyColumnCount, CSSPropertyInvalid, *columnCount, important);
    return true;
}

bool CSSPropertyParser::consumeShorthandGreedily(const StylePropertyShorthand& shorthand, bool important)
{
    ASSERT(shorthand.length() <= 6); // Existing shorthands have at most 6 longhands.
    const CSSValue* longhands[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    const CSSPropertyID* shorthandProperties = shorthand.properties();
    do {
        bool foundLonghand = false;
        for (size_t i = 0; !foundLonghand && i < shorthand.length(); ++i) {
            if (longhands[i])
                continue;
            longhands[i] = parseSingleValue(shorthandProperties[i], shorthand.id());
            if (longhands[i])
                foundLonghand = true;
        }
        if (!foundLonghand)
            return false;
    } while (!m_range.atEnd());

    for (size_t i = 0; i < shorthand.length(); ++i) {
        if (longhands[i])
            addProperty(shorthandProperties[i], shorthand.id(), *longhands[i], important);
        else
            addProperty(shorthandProperties[i], shorthand.id(), *CSSInitialValue::createLegacyImplicit(), important);
    }
    return true;
}

bool CSSPropertyParser::consumeFlex(bool important)
{
    static const double unsetValue = -1;
    double flexGrow = unsetValue;
    double flexShrink = unsetValue;
    CSSPrimitiveValue* flexBasis = nullptr;

    if (m_range.peek().id() == CSSValueNone) {
        flexGrow = 0;
        flexShrink = 0;
        flexBasis = CSSPrimitiveValue::createIdentifier(CSSValueAuto);
        m_range.consumeIncludingWhitespace();
    } else {
        unsigned index = 0;
        while (!m_range.atEnd() && index++ < 3) {
            double num;
            if (consumeNumberRaw(m_range, num)) {
                if (num < 0)
                    return false;
                if (flexGrow == unsetValue)
                    flexGrow = num;
                else if (flexShrink == unsetValue)
                    flexShrink = num;
                else if (!num) // flex only allows a basis of 0 (sans units) if flex-grow and flex-shrink values have already been set.
                    flexBasis = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Pixels);
                else
                    return false;
            } else if (!flexBasis) {
                if (m_range.peek().id() == CSSValueAuto)
                    flexBasis = consumeIdent(m_range);
                if (!flexBasis)
                    flexBasis = consumeLengthOrPercent(m_range, m_context.mode(), ValueRangeNonNegative);
                if (index == 2 && !m_range.atEnd())
                    return false;
            }
        }
        if (index == 0)
            return false;
        if (flexGrow == unsetValue)
            flexGrow = 1;
        if (flexShrink == unsetValue)
            flexShrink = 1;
        if (!flexBasis)
            flexBasis = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Percentage);
    }

    if (!m_range.atEnd())
        return false;
    addProperty(CSSPropertyFlexGrow, CSSPropertyFlex, *CSSPrimitiveValue::create(clampTo<float>(flexGrow), CSSPrimitiveValue::UnitType::Number), important);
    addProperty(CSSPropertyFlexShrink, CSSPropertyFlex, *CSSPrimitiveValue::create(clampTo<float>(flexShrink), CSSPrimitiveValue::UnitType::Number), important);
    addProperty(CSSPropertyFlexBasis, CSSPropertyFlex, *flexBasis, important);
    return true;
}

bool CSSPropertyParser::consumeBorder(bool important)
{
    CSSValue* width = nullptr;
    const CSSValue* style = nullptr;
    CSSValue* color = nullptr;

    while (!width || !style || !color) {
        if (!width) {
            width = consumeLineWidth(m_range, m_context.mode(), UnitlessQuirk::Forbid);
            if (width)
                continue;
        }
        if (!style) {
            style = parseSingleValue(CSSPropertyBorderLeftStyle, CSSPropertyBorder);
            if (style)
                continue;
        }
        if (!color) {
            color = consumeColor(m_range, m_context.mode());
            if (color)
                continue;
        }
        break;
    }

    if (!width && !style && !color)
        return false;

    if (!width)
        width = CSSInitialValue::createLegacyImplicit();
    if (!style)
        style = CSSInitialValue::createLegacyImplicit();
    if (!color)
        color = CSSInitialValue::createLegacyImplicit();

    addExpandedPropertyForValue(CSSPropertyBorderWidth, *width, important);
    addExpandedPropertyForValue(CSSPropertyBorderStyle, *style, important);
    addExpandedPropertyForValue(CSSPropertyBorderColor, *color, important);
    addExpandedPropertyForValue(CSSPropertyBorderImage, *CSSInitialValue::createLegacyImplicit(), important);

    return m_range.atEnd();
}

bool CSSPropertyParser::consume4Values(const StylePropertyShorthand& shorthand, bool important)
{
    ASSERT(shorthand.length() == 4);
    const CSSPropertyID* longhands = shorthand.properties();
    const CSSValue* top = parseSingleValue(longhands[0], shorthand.id());
    if (!top)
        return false;

    const CSSValue* right = parseSingleValue(longhands[1], shorthand.id());
    const CSSValue* bottom = nullptr;
    const CSSValue* left = nullptr;
    if (right) {
        bottom = parseSingleValue(longhands[2], shorthand.id());
        if (bottom)
            left = parseSingleValue(longhands[3], shorthand.id());
    }

    if (!right)
        right = top;
    if (!bottom)
        bottom = top;
    if (!left)
        left = right;

    addProperty(longhands[0], shorthand.id(), *top, important);
    addProperty(longhands[1], shorthand.id(), *right, important);
    addProperty(longhands[2], shorthand.id(), *bottom, important);
    addProperty(longhands[3], shorthand.id(), *left, important);

    return m_range.atEnd();
}

bool CSSPropertyParser::consumeBorderImage(CSSPropertyID property, bool important)
{
    CSSValue* source = nullptr;
    CSSValue* slice = nullptr;
    CSSValue* width = nullptr;
    CSSValue* outset = nullptr;
    CSSValue* repeat = nullptr;
    if (consumeBorderImageComponents(property, m_range, m_context, source, slice, width, outset, repeat)) {
        switch (property) {
        case CSSPropertyWebkitMaskBoxImage:
            addProperty(CSSPropertyWebkitMaskBoxImageSource, CSSPropertyWebkitMaskBoxImage, source ? *source : *CSSInitialValue::createLegacyImplicit(), important);
            addProperty(CSSPropertyWebkitMaskBoxImageSlice, CSSPropertyWebkitMaskBoxImage, slice ? *slice : *CSSInitialValue::createLegacyImplicit(), important);
            addProperty(CSSPropertyWebkitMaskBoxImageWidth, CSSPropertyWebkitMaskBoxImage, width ? *width : *CSSInitialValue::createLegacyImplicit(), important);
            addProperty(CSSPropertyWebkitMaskBoxImageOutset, CSSPropertyWebkitMaskBoxImage, outset ? *outset : *CSSInitialValue::createLegacyImplicit(), important);
            addProperty(CSSPropertyWebkitMaskBoxImageRepeat, CSSPropertyWebkitMaskBoxImage, repeat ? *repeat : *CSSInitialValue::createLegacyImplicit(), important);
            return true;
        case CSSPropertyBorderImage:
            addProperty(CSSPropertyBorderImageSource, CSSPropertyBorderImage, source ? *source : *CSSInitialValue::createLegacyImplicit(), important);
            addProperty(CSSPropertyBorderImageSlice, CSSPropertyBorderImage, slice ? *slice : *CSSInitialValue::createLegacyImplicit(), important);
            addProperty(CSSPropertyBorderImageWidth, CSSPropertyBorderImage, width ? *width : *CSSInitialValue::createLegacyImplicit(), important);
            addProperty(CSSPropertyBorderImageOutset, CSSPropertyBorderImage, outset ? *outset : *CSSInitialValue::createLegacyImplicit(), important);
            addProperty(CSSPropertyBorderImageRepeat, CSSPropertyBorderImage, repeat ? *repeat : *CSSInitialValue::createLegacyImplicit(), important);
            return true;
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    }
    return false;
}

static inline CSSValueID mapFromPageBreakBetween(CSSValueID value)
{
    if (value == CSSValueAlways)
        return CSSValuePage;
    if (value == CSSValueAuto || value == CSSValueAvoid || value == CSSValueLeft || value == CSSValueRight)
        return value;
    return CSSValueInvalid;
}

static inline CSSValueID mapFromColumnBreakBetween(CSSValueID value)
{
    if (value == CSSValueAlways)
        return CSSValueColumn;
    if (value == CSSValueAuto || value == CSSValueAvoid)
        return value;
    return CSSValueInvalid;
}

static inline CSSValueID mapFromColumnOrPageBreakInside(CSSValueID value)
{
    if (value == CSSValueAuto || value == CSSValueAvoid)
        return value;
    return CSSValueInvalid;
}

static inline CSSPropertyID mapFromLegacyBreakProperty(CSSPropertyID property)
{
    if (property == CSSPropertyPageBreakAfter || property == CSSPropertyWebkitColumnBreakAfter)
        return CSSPropertyBreakAfter;
    if (property == CSSPropertyPageBreakBefore || property == CSSPropertyWebkitColumnBreakBefore)
        return CSSPropertyBreakBefore;
    ASSERT(property == CSSPropertyPageBreakInside || property == CSSPropertyWebkitColumnBreakInside);
    return CSSPropertyBreakInside;
}

bool CSSPropertyParser::consumeLegacyBreakProperty(CSSPropertyID property, bool important)
{
    // The fragmentation spec says that page-break-(after|before|inside) are to be treated as
    // shorthands for their break-(after|before|inside) counterparts. We'll do the same for the
    // non-standard properties -webkit-column-break-(after|before|inside).
    CSSPrimitiveValue* keyword = consumeIdent(m_range);
    if (!keyword)
        return false;
    if (!m_range.atEnd())
        return false;
    CSSValueID value = keyword->getValueID();
    switch (property) {
    case CSSPropertyPageBreakAfter:
    case CSSPropertyPageBreakBefore:
        value = mapFromPageBreakBetween(value);
        break;
    case CSSPropertyWebkitColumnBreakAfter:
    case CSSPropertyWebkitColumnBreakBefore:
        value = mapFromColumnBreakBetween(value);
        break;
    case CSSPropertyPageBreakInside:
    case CSSPropertyWebkitColumnBreakInside:
        value = mapFromColumnOrPageBreakInside(value);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    if (value == CSSValueInvalid)
        return false;

    CSSPropertyID genericBreakProperty = mapFromLegacyBreakProperty(property);
    addProperty(genericBreakProperty, property, *CSSPrimitiveValue::createIdentifier(value), important);
    return true;
}

static bool consumeBackgroundPosition(CSSParserTokenRange& range, const CSSParserContext& context, UnitlessQuirk unitless, CSSValue*& resultX, CSSValue*& resultY)
{
    do {
        CSSValue* positionX = nullptr;
        CSSValue* positionY = nullptr;
        if (!consumePosition(range, context.mode(), unitless, positionX, positionY))
            return false;
        addBackgroundValue(resultX, positionX);
        addBackgroundValue(resultY, positionY);
    } while (consumeCommaIncludingWhitespace(range));
    return true;
}

static bool consumeRepeatStyleComponent(CSSParserTokenRange& range, CSSValue*& value1, CSSValue*& value2, bool& implicit)
{
    if (consumeIdent<CSSValueRepeatX>(range)) {
        value1 = CSSPrimitiveValue::createIdentifier(CSSValueRepeat);
        value2 = CSSPrimitiveValue::createIdentifier(CSSValueNoRepeat);
        implicit = true;
        return true;
    }
    if (consumeIdent<CSSValueRepeatY>(range)) {
        value1 = CSSPrimitiveValue::createIdentifier(CSSValueNoRepeat);
        value2 = CSSPrimitiveValue::createIdentifier(CSSValueRepeat);
        implicit = true;
        return true;
    }
    value1 = consumeIdent<CSSValueRepeat, CSSValueNoRepeat, CSSValueRound, CSSValueSpace>(range);
    if (!value1)
        return false;

    value2 = consumeIdent<CSSValueRepeat, CSSValueNoRepeat, CSSValueRound, CSSValueSpace>(range);
    if (!value2) {
        value2 = value1;
        implicit = true;
    }
    return true;
}

static bool consumeRepeatStyle(CSSParserTokenRange& range, CSSValue*& resultX, CSSValue*& resultY, bool& implicit)
{
    do {
        CSSValue* repeatX = nullptr;
        CSSValue* repeatY = nullptr;
        if (!consumeRepeatStyleComponent(range, repeatX, repeatY, implicit))
            return false;
        addBackgroundValue(resultX, repeatX);
        addBackgroundValue(resultY, repeatY);
    } while (consumeCommaIncludingWhitespace(range));
    return true;
}

// Note: consumeBackgroundShorthand assumes y properties (for example background-position-y) follow
// the x properties in the shorthand array.
bool CSSPropertyParser::consumeBackgroundShorthand(const StylePropertyShorthand& shorthand, bool important)
{
    const unsigned longhandCount = shorthand.length();
    CSSValue* longhands[10] = { 0 };
    ASSERT(longhandCount <= 10);

    bool implicit = false;
    do {
        bool parsedLonghand[10] = { false };
        CSSValue* originValue = nullptr;
        do {
            bool foundProperty = false;
            for (size_t i = 0; i < longhandCount; ++i) {
                if (parsedLonghand[i])
                    continue;

                CSSValue* value = nullptr;
                CSSValue* valueY = nullptr;
                CSSPropertyID property = shorthand.properties()[i];
                if (property == CSSPropertyBackgroundRepeatX || property == CSSPropertyWebkitMaskRepeatX) {
                    consumeRepeatStyleComponent(m_range, value, valueY, implicit);
                } else if (property == CSSPropertyBackgroundPositionX || property == CSSPropertyWebkitMaskPositionX) {
                    CSSParserTokenRange rangeCopy = m_range;
                    if (!consumePosition(rangeCopy, m_context.mode(), UnitlessQuirk::Forbid, value, valueY))
                        continue;
                    m_range = rangeCopy;
                } else if (property == CSSPropertyBackgroundSize || property == CSSPropertyWebkitMaskSize) {
                    if (!consumeSlashIncludingWhitespace(m_range))
                        continue;
                    value = consumeBackgroundSize(property, m_range, m_context.mode());
                    if (!value || !parsedLonghand[i - 1]) // Position must have been parsed in the current layer.
                        return false;
                } else if (property == CSSPropertyBackgroundPositionY || property == CSSPropertyBackgroundRepeatY
                    || property == CSSPropertyWebkitMaskPositionY || property == CSSPropertyWebkitMaskRepeatY) {
                    continue;
                } else {
                    value = consumeBackgroundComponent(property, m_range, m_context);
                }
                if (value) {
                    if (property == CSSPropertyBackgroundOrigin || property == CSSPropertyWebkitMaskOrigin)
                        originValue = value;
                    parsedLonghand[i] = true;
                    foundProperty = true;
                    addBackgroundValue(longhands[i], value);
                    if (valueY) {
                        parsedLonghand[i + 1] = true;
                        addBackgroundValue(longhands[i + 1], valueY);
                    }
                }
            }
            if (!foundProperty)
                return false;
        } while (!m_range.atEnd() && m_range.peek().type() != CommaToken);

        // FIXME: This will make invalid longhands, see crbug.com/386459
        for (size_t i = 0; i < longhandCount; ++i) {
            CSSPropertyID property = shorthand.properties()[i];
            if (property == CSSPropertyBackgroundColor && !m_range.atEnd()) {
                if (parsedLonghand[i])
                    return false; // Colors are only allowed in the last layer.
                continue;
            }
            if ((property == CSSPropertyBackgroundClip || property == CSSPropertyWebkitMaskClip) && !parsedLonghand[i] && originValue) {
                addBackgroundValue(longhands[i], originValue);
                continue;
            }
            if (!parsedLonghand[i])
                addBackgroundValue(longhands[i], CSSInitialValue::createLegacyImplicit());
        }
    } while (consumeCommaIncludingWhitespace(m_range));
    if (!m_range.atEnd())
        return false;

    for (size_t i = 0; i < longhandCount; ++i) {
        CSSPropertyID property = shorthand.properties()[i];
        if (property == CSSPropertyBackgroundSize && longhands[i] && m_context.useLegacyBackgroundSizeShorthandBehavior())
            continue;
        addProperty(property, shorthand.id(), *longhands[i], important, implicit);
    }
    return true;
}

bool CSSPropertyParser::consumeGridItemPositionShorthand(CSSPropertyID shorthandId, bool important)
{
    ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
    const StylePropertyShorthand& shorthand = shorthandForProperty(shorthandId);
    ASSERT(shorthand.length() == 2);
    CSSValue* startValue = consumeGridLine(m_range);
    if (!startValue)
        return false;

    CSSValue* endValue = nullptr;
    if (consumeSlashIncludingWhitespace(m_range)) {
        endValue = consumeGridLine(m_range);
        if (!endValue)
            return false;
    } else {
        endValue = startValue->isCustomIdentValue() ? startValue : CSSPrimitiveValue::createIdentifier(CSSValueAuto);
    }
    if (!m_range.atEnd())
        return false;
    addProperty(shorthand.properties()[0], shorthandId, *startValue, important);
    addProperty(shorthand.properties()[1], shorthandId, *endValue, important);
    return true;
}

bool CSSPropertyParser::consumeGridAreaShorthand(bool important)
{
    ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
    ASSERT(gridAreaShorthand().length() == 4);
    CSSValue* rowStartValue = consumeGridLine(m_range);
    if (!rowStartValue)
        return false;
    CSSValue* columnStartValue = nullptr;
    CSSValue* rowEndValue = nullptr;
    CSSValue* columnEndValue = nullptr;
    if (consumeSlashIncludingWhitespace(m_range)) {
        columnStartValue = consumeGridLine(m_range);
        if (!columnStartValue)
            return false;
        if (consumeSlashIncludingWhitespace(m_range)) {
            rowEndValue = consumeGridLine(m_range);
            if (!rowEndValue)
                return false;
            if (consumeSlashIncludingWhitespace(m_range)) {
                columnEndValue = consumeGridLine(m_range);
                if (!columnEndValue)
                    return false;
            }
        }
    }
    if (!m_range.atEnd())
        return false;
    if (!columnStartValue)
        columnStartValue = rowStartValue->isCustomIdentValue() ? rowStartValue : CSSPrimitiveValue::createIdentifier(CSSValueAuto);
    if (!rowEndValue)
        rowEndValue = rowStartValue->isCustomIdentValue() ? rowStartValue : CSSPrimitiveValue::createIdentifier(CSSValueAuto);
    if (!columnEndValue)
        columnEndValue = columnStartValue->isCustomIdentValue() ? columnStartValue : CSSPrimitiveValue::createIdentifier(CSSValueAuto);

    addProperty(CSSPropertyGridRowStart, CSSPropertyGridArea, *rowStartValue, important);
    addProperty(CSSPropertyGridColumnStart, CSSPropertyGridArea, *columnStartValue, important);
    addProperty(CSSPropertyGridRowEnd, CSSPropertyGridArea, *rowEndValue, important);
    addProperty(CSSPropertyGridColumnEnd, CSSPropertyGridArea, *columnEndValue, important);
    return true;
}

bool CSSPropertyParser::consumeGridTemplateRowsAndAreasAndColumns(CSSPropertyID shorthandId, bool important)
{
    NamedGridAreaMap gridAreaMap;
    size_t rowCount = 0;
    size_t columnCount = 0;
    CSSValueList* templateRows = CSSValueList::createSpaceSeparated();

    // Persists between loop iterations so we can use the same value for
    // consecutive <line-names> values
    CSSGridLineNamesValue* lineNames = nullptr;

    do {
        // Handle leading <custom-ident>*.
        bool hasPreviousLineNames = lineNames;
        lineNames = consumeGridLineNames(m_range, lineNames);
        if (lineNames && !hasPreviousLineNames)
            templateRows->append(*lineNames);

        // Handle a template-area's row.
        if (m_range.peek().type() != StringToken || !parseGridTemplateAreasRow(m_range.consumeIncludingWhitespace().value().toString(), gridAreaMap, rowCount, columnCount))
            return false;
        ++rowCount;

        // Handle template-rows's track-size.
        CSSValue* value = consumeGridTrackSize(m_range, m_context.mode());
        if (!value)
            value = CSSPrimitiveValue::createIdentifier(CSSValueAuto);
        templateRows->append(*value);

        // This will handle the trailing/leading <custom-ident>* in the grammar.
        lineNames = consumeGridLineNames(m_range);
        if (lineNames)
            templateRows->append(*lineNames);
    } while (!m_range.atEnd() && !(m_range.peek().type() == DelimiterToken && m_range.peek().delimiter() == '/'));

    CSSValue* columnsValue = nullptr;
    if (!m_range.atEnd()) {
        if (!consumeSlashIncludingWhitespace(m_range))
            return false;
        columnsValue = consumeGridTrackList(m_range, m_context.mode(), GridTemplateNoRepeat);
        if (!columnsValue || !m_range.atEnd())
            return false;
    } else {
        columnsValue = CSSPrimitiveValue::createIdentifier(CSSValueNone);
    }
    addProperty(CSSPropertyGridTemplateRows, shorthandId, *templateRows, important);
    addProperty(CSSPropertyGridTemplateColumns, shorthandId, *columnsValue, important);
    addProperty(CSSPropertyGridTemplateAreas, shorthandId, *CSSGridTemplateAreasValue::create(gridAreaMap, rowCount, columnCount), important);
    return true;
}

bool CSSPropertyParser::consumeGridTemplateShorthand(CSSPropertyID shorthandId, bool important)
{
    ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
    ASSERT(gridTemplateShorthand().length() == 3);

    CSSParserTokenRange rangeCopy = m_range;
    CSSValue* rowsValue = consumeIdent<CSSValueNone>(m_range);

    // 1- 'none' case.
    if (rowsValue && m_range.atEnd()) {
        addProperty(CSSPropertyGridTemplateRows, shorthandId, *CSSPrimitiveValue::createIdentifier(CSSValueNone), important);
        addProperty(CSSPropertyGridTemplateColumns, shorthandId, *CSSPrimitiveValue::createIdentifier(CSSValueNone), important);
        addProperty(CSSPropertyGridTemplateAreas, shorthandId, *CSSPrimitiveValue::createIdentifier(CSSValueNone), important);
        return true;
    }

    // 2- <grid-template-rows> / <grid-template-columns>
    if (!rowsValue)
        rowsValue = consumeGridTrackList(m_range, m_context.mode(), GridTemplate);

    if (rowsValue) {
        if (!consumeSlashIncludingWhitespace(m_range))
            return false;
        CSSValue* columnsValue = consumeGridTemplatesRowsOrColumns(m_range, m_context.mode());
        if (!columnsValue || !m_range.atEnd())
            return false;

        addProperty(CSSPropertyGridTemplateRows, shorthandId, *rowsValue, important);
        addProperty(CSSPropertyGridTemplateColumns, shorthandId, *columnsValue, important);
        addProperty(CSSPropertyGridTemplateAreas, shorthandId, *CSSPrimitiveValue::createIdentifier(CSSValueNone), important);
        return true;
    }

    // 3- [ <line-names>? <string> <track-size>? <line-names>? ]+ [ / <track-list> ]?
    m_range = rangeCopy;
    return consumeGridTemplateRowsAndAreasAndColumns(shorthandId, important);
}

bool CSSPropertyParser::consumeGridShorthand(bool important)
{
    ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
    ASSERT(shorthandForProperty(CSSPropertyGrid).length() == 8);

    CSSParserTokenRange rangeCopy = m_range;

    // 1- <grid-template>
    if (consumeGridTemplateShorthand(CSSPropertyGrid, important)) {
        // It can only be specified the explicit or the implicit grid properties in a single grid declaration.
        // The sub-properties not specified are set to their initial value, as normal for shorthands.
        addProperty(CSSPropertyGridAutoFlow, CSSPropertyGrid, *CSSInitialValue::createLegacyImplicit(), important);
        addProperty(CSSPropertyGridAutoColumns, CSSPropertyGrid, *CSSInitialValue::createLegacyImplicit(), important);
        addProperty(CSSPropertyGridAutoRows, CSSPropertyGrid, *CSSInitialValue::createLegacyImplicit(), important);
        addProperty(CSSPropertyGridColumnGap, CSSPropertyGrid, *CSSInitialValue::createLegacyImplicit(), important);
        addProperty(CSSPropertyGridRowGap, CSSPropertyGrid, *CSSInitialValue::createLegacyImplicit(), important);
        return true;
    }

    m_range = rangeCopy;

    // 2- <grid-auto-flow> [ <grid-auto-rows> [ / <grid-auto-columns> ]? ]
    CSSValueList* gridAutoFlow = consumeGridAutoFlow(m_range);
    if (!gridAutoFlow)
        return false;

    CSSValue* autoColumnsValue = nullptr;
    CSSValue* autoRowsValue = nullptr;

    if (!m_range.atEnd()) {
        autoRowsValue = consumeGridTrackList(m_range, m_context.mode(), GridAuto);
        if (!autoRowsValue)
            return false;
        if (consumeSlashIncludingWhitespace(m_range)) {
            autoColumnsValue = consumeGridTrackList(m_range, m_context.mode(), GridAuto);
            if (!autoColumnsValue)
                return false;
        }
        if (!m_range.atEnd())
            return false;
    } else {
        // Other omitted values are set to their initial values.
        autoColumnsValue = CSSInitialValue::createLegacyImplicit();
        autoRowsValue = CSSInitialValue::createLegacyImplicit();
    }

    // if <grid-auto-columns> value is omitted, it is set to the value specified for grid-auto-rows.
    if (!autoColumnsValue)
        autoColumnsValue = autoRowsValue;

    // It can only be specified the explicit or the implicit grid properties in a single grid declaration.
    // The sub-properties not specified are set to their initial value, as normal for shorthands.
    addProperty(CSSPropertyGridTemplateColumns, CSSPropertyGrid, *CSSInitialValue::createLegacyImplicit(), important);
    addProperty(CSSPropertyGridTemplateRows, CSSPropertyGrid, *CSSInitialValue::createLegacyImplicit(), important);
    addProperty(CSSPropertyGridTemplateAreas, CSSPropertyGrid, *CSSInitialValue::createLegacyImplicit(), important);
    addProperty(CSSPropertyGridAutoFlow, CSSPropertyGrid, *gridAutoFlow, important);
    addProperty(CSSPropertyGridAutoColumns, CSSPropertyGrid, *autoColumnsValue, important);
    addProperty(CSSPropertyGridAutoRows, CSSPropertyGrid, *autoRowsValue, important);
    addProperty(CSSPropertyGridColumnGap, CSSPropertyGrid, *CSSInitialValue::createLegacyImplicit(), important);
    addProperty(CSSPropertyGridRowGap, CSSPropertyGrid, *CSSInitialValue::createLegacyImplicit(), important);
    return true;
}

bool CSSPropertyParser::parseShorthand(CSSPropertyID unresolvedProperty, bool important)
{
    CSSPropertyID property = resolveCSSPropertyID(unresolvedProperty);

    switch (property) {
    case CSSPropertyWebkitMarginCollapse: {
        CSSValueID id = m_range.consumeIncludingWhitespace().id();
        if (!CSSParserFastPaths::isValidKeywordPropertyAndValue(CSSPropertyWebkitMarginBeforeCollapse, id, m_context.mode()))
            return false;
        CSSValue* beforeCollapse = CSSPrimitiveValue::createIdentifier(id);
        addProperty(CSSPropertyWebkitMarginBeforeCollapse, CSSPropertyWebkitMarginCollapse, *beforeCollapse, important);
        if (m_range.atEnd()) {
            addProperty(CSSPropertyWebkitMarginAfterCollapse, CSSPropertyWebkitMarginCollapse, *beforeCollapse, important);
            return true;
        }
        id = m_range.consumeIncludingWhitespace().id();
        if (!CSSParserFastPaths::isValidKeywordPropertyAndValue(CSSPropertyWebkitMarginAfterCollapse, id, m_context.mode()))
            return false;
        addProperty(CSSPropertyWebkitMarginAfterCollapse, CSSPropertyWebkitMarginCollapse, *CSSPrimitiveValue::createIdentifier(id), important);
        return true;
    }
    case CSSPropertyOverflow: {
        CSSValueID id = m_range.consumeIncludingWhitespace().id();
        if (!CSSParserFastPaths::isValidKeywordPropertyAndValue(CSSPropertyOverflowY, id, m_context.mode()))
            return false;
        if (!m_range.atEnd())
            return false;
        CSSValue* overflowYValue = CSSPrimitiveValue::createIdentifier(id);

        CSSValue* overflowXValue = nullptr;

        // FIXME: -webkit-paged-x or -webkit-paged-y only apply to overflow-y. If this value has been
        // set using the shorthand, then for now overflow-x will default to auto, but once we implement
        // pagination controls, it should default to hidden. If the overflow-y value is anything but
        // paged-x or paged-y, then overflow-x and overflow-y should have the same value.
        if (id == CSSValueWebkitPagedX || id == CSSValueWebkitPagedY)
            overflowXValue = CSSPrimitiveValue::createIdentifier(CSSValueAuto);
        else
            overflowXValue = overflowYValue;
        addProperty(CSSPropertyOverflowX, CSSPropertyOverflow, *overflowXValue, important);
        addProperty(CSSPropertyOverflowY, CSSPropertyOverflow, *overflowYValue, important);
        return true;
    }
    case CSSPropertyFont: {
        const CSSParserToken& token = m_range.peek();
        if (token.id() >= CSSValueCaption && token.id() <= CSSValueStatusBar)
            return consumeSystemFont(important);
        return consumeFont(important);
    }
    case CSSPropertyFontVariant:
        return consumeFontVariantShorthand(important);
    case CSSPropertyBorderSpacing:
        return consumeBorderSpacing(important);
    case CSSPropertyColumns:
        return consumeColumns(important);
    case CSSPropertyAnimation:
        return consumeAnimationShorthand(animationShorthandForParsing(), unresolvedProperty == CSSPropertyAliasWebkitAnimation, important);
    case CSSPropertyTransition:
        return consumeAnimationShorthand(transitionShorthandForParsing(), false, important);
    case CSSPropertyTextDecoration:
        ASSERT(RuntimeEnabledFeatures::css3TextDecorationsEnabled());
        return consumeShorthandGreedily(textDecorationShorthand(), important);
    case CSSPropertyMargin:
        return consume4Values(marginShorthand(), important);
    case CSSPropertyPadding:
        return consume4Values(paddingShorthand(), important);
    case CSSPropertyMotion:
        return consumeShorthandGreedily(motionShorthand(), important);
    case CSSPropertyWebkitTextEmphasis:
        return consumeShorthandGreedily(webkitTextEmphasisShorthand(), important);
    case CSSPropertyOutline:
        return consumeShorthandGreedily(outlineShorthand(), important);
    case CSSPropertyWebkitBorderStart:
        return consumeShorthandGreedily(webkitBorderStartShorthand(), important);
    case CSSPropertyWebkitBorderEnd:
        return consumeShorthandGreedily(webkitBorderEndShorthand(), important);
    case CSSPropertyWebkitBorderBefore:
        return consumeShorthandGreedily(webkitBorderBeforeShorthand(), important);
    case CSSPropertyWebkitBorderAfter:
        return consumeShorthandGreedily(webkitBorderAfterShorthand(), important);
    case CSSPropertyWebkitTextStroke:
        return consumeShorthandGreedily(webkitTextStrokeShorthand(), important);
    case CSSPropertyMarker: {
        const CSSValue* marker = parseSingleValue(CSSPropertyMarkerStart);
        if (!marker || !m_range.atEnd())
            return false;
        addProperty(CSSPropertyMarkerStart, CSSPropertyMarker, *marker, important);
        addProperty(CSSPropertyMarkerMid, CSSPropertyMarker, *marker, important);
        addProperty(CSSPropertyMarkerEnd, CSSPropertyMarker, *marker, important);
        return true;
    }
    case CSSPropertyFlex:
        return consumeFlex(important);
    case CSSPropertyFlexFlow:
        return consumeShorthandGreedily(flexFlowShorthand(), important);
    case CSSPropertyColumnRule:
        return consumeShorthandGreedily(columnRuleShorthand(), important);
    case CSSPropertyListStyle:
        return consumeShorthandGreedily(listStyleShorthand(), important);
    case CSSPropertyBorderRadius: {
        CSSPrimitiveValue* horizontalRadii[4] = { 0 };
        CSSPrimitiveValue* verticalRadii[4] = { 0 };
        if (!consumeRadii(horizontalRadii, verticalRadii, m_range, m_context.mode(), unresolvedProperty == CSSPropertyAliasWebkitBorderRadius))
            return false;
        addProperty(CSSPropertyBorderTopLeftRadius, CSSPropertyBorderRadius, *CSSValuePair::create(horizontalRadii[0], verticalRadii[0], CSSValuePair::DropIdenticalValues), important);
        addProperty(CSSPropertyBorderTopRightRadius, CSSPropertyBorderRadius, *CSSValuePair::create(horizontalRadii[1], verticalRadii[1], CSSValuePair::DropIdenticalValues), important);
        addProperty(CSSPropertyBorderBottomRightRadius, CSSPropertyBorderRadius, *CSSValuePair::create(horizontalRadii[2], verticalRadii[2], CSSValuePair::DropIdenticalValues), important);
        addProperty(CSSPropertyBorderBottomLeftRadius, CSSPropertyBorderRadius, *CSSValuePair::create(horizontalRadii[3], verticalRadii[3], CSSValuePair::DropIdenticalValues), important);
        return true;
    }
    case CSSPropertyBorderColor:
        return consume4Values(borderColorShorthand(), important);
    case CSSPropertyBorderStyle:
        return consume4Values(borderStyleShorthand(), important);
    case CSSPropertyBorderWidth:
        return consume4Values(borderWidthShorthand(), important);
    case CSSPropertyBorderTop:
        return consumeShorthandGreedily(borderTopShorthand(), important);
    case CSSPropertyBorderRight:
        return consumeShorthandGreedily(borderRightShorthand(), important);
    case CSSPropertyBorderBottom:
        return consumeShorthandGreedily(borderBottomShorthand(), important);
    case CSSPropertyBorderLeft:
        return consumeShorthandGreedily(borderLeftShorthand(), important);
    case CSSPropertyBorder:
        return consumeBorder(important);
    case CSSPropertyBorderImage:
    case CSSPropertyWebkitMaskBoxImage:
        return consumeBorderImage(property, important);
    case CSSPropertyPageBreakAfter:
    case CSSPropertyPageBreakBefore:
    case CSSPropertyPageBreakInside:
    case CSSPropertyWebkitColumnBreakAfter:
    case CSSPropertyWebkitColumnBreakBefore:
    case CSSPropertyWebkitColumnBreakInside:
        return consumeLegacyBreakProperty(property, important);
    case CSSPropertyWebkitMaskPosition:
    case CSSPropertyBackgroundPosition: {
        CSSValue* resultX = nullptr;
        CSSValue* resultY = nullptr;
        if (!consumeBackgroundPosition(m_range, m_context, UnitlessQuirk::Allow, resultX, resultY) || !m_range.atEnd())
            return false;
        addProperty(property == CSSPropertyBackgroundPosition ? CSSPropertyBackgroundPositionX : CSSPropertyWebkitMaskPositionX, property, *resultX, important);
        addProperty(property == CSSPropertyBackgroundPosition ? CSSPropertyBackgroundPositionY : CSSPropertyWebkitMaskPositionY, property, *resultY, important);
        return true;
    }
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyWebkitMaskRepeat: {
        CSSValue* resultX = nullptr;
        CSSValue* resultY = nullptr;
        bool implicit = false;
        if (!consumeRepeatStyle(m_range, resultX, resultY, implicit) || !m_range.atEnd())
            return false;
        addProperty(property == CSSPropertyBackgroundRepeat ? CSSPropertyBackgroundRepeatX : CSSPropertyWebkitMaskRepeatX, property, *resultX, important, implicit);
        addProperty(property == CSSPropertyBackgroundRepeat ? CSSPropertyBackgroundRepeatY : CSSPropertyWebkitMaskRepeatY, property, *resultY, important, implicit);
        return true;
    }
    case CSSPropertyBackground:
        return consumeBackgroundShorthand(backgroundShorthand(), important);
    case CSSPropertyWebkitMask:
        return consumeBackgroundShorthand(webkitMaskShorthand(), important);
    case CSSPropertyGridGap: {
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled() && shorthandForProperty(CSSPropertyGridGap).length() == 2);
        CSSValue* rowGap = consumeLength(m_range, m_context.mode(), ValueRangeNonNegative);
        CSSValue* columnGap = consumeLength(m_range, m_context.mode(), ValueRangeNonNegative);
        if (!rowGap || !m_range.atEnd())
            return false;
        if (!columnGap)
            columnGap = rowGap;
        addProperty(CSSPropertyGridRowGap, CSSPropertyGridGap, *rowGap, important);
        addProperty(CSSPropertyGridColumnGap, CSSPropertyGridGap, *columnGap, important);
        return true;
    }
    case CSSPropertyGridColumn:
    case CSSPropertyGridRow:
        return consumeGridItemPositionShorthand(property, important);
    case CSSPropertyGridArea:
        return consumeGridAreaShorthand(important);
    case CSSPropertyGridTemplate:
        return consumeGridTemplateShorthand(CSSPropertyGridTemplate, important);
    case CSSPropertyGrid:
        return consumeGridShorthand(important);
    default:
        return false;
    }
}
*/

} // namespace WebCore
