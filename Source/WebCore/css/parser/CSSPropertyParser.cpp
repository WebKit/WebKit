// Copyright 2015 The Chromium Authors. All rights reserved.
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

#include "config.h"
#include "CSSPropertyParser.h"

#include "CSSBackgroundRepeatValue.h"
#include "CSSBasicShapes.h"
#include "CSSBorderImage.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSBorderImageWidthValue.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSContentDistributionValue.h"
#include "CSSCursorImageValue.h"
#include "CSSCustomPropertyValue.h"
#include "CSSFontFaceSrcValue.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontPaletteValuesOverrideColorsValue.h"
#include "CSSFontVariantAlternatesValue.h"
#if ENABLE(VARIATION_FONTS)
#include "CSSFontVariationValue.h"
#endif
#include "CSSFontStyleRangeValue.h"
#include "CSSFontStyleValue.h"
#include "CSSFunctionValue.h"
#include "CSSGridAutoRepeatValue.h"
#include "CSSGridIntegerRepeatValue.h"
#include "CSSGridLineNamesValue.h"
#include "CSSGridTemplateAreasValue.h"
#include "CSSLineBoxContainValue.h"
#include "CSSOffsetRotateValue.h"
#include "CSSParserFastPaths.h"
#include "CSSParserIdioms.h"
#include "CSSPendingSubstitutionValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSRayValue.h"
#include "CSSReflectValue.h"
#include "CSSShadowValue.h"
#include "CSSSubgridValue.h"
#include "CSSTimingFunctionValue.h"
#include "CSSUnicodeRangeValue.h"
#include "CSSVariableParser.h"
#include "CSSVariableReferenceValue.h"
#include "Counter.h"
#include "FontFace.h"
#include "HashTools.h"
// FIXME-NEWPARSER: CSSPrimitiveValue is a large class that holds many unrelated objects,
// switching behavior on the type of the object it is holding.
// Since CSSValue is already a class hierarchy, this adds an unnecessary second level to the hierarchy that complicates code.
// So we need to remove the various behaviors from CSSPrimitiveValue and split them into separate subclasses of CSSValue.
// FIXME-NEWPARSER: Replace Pair and Rect with actual CSSValue subclasses (CSSValuePair and CSSQuadValue).
#include "Pair.h"
#include "Rect.h"
#include "RenderTheme.h"
#include "SVGPathByteStream.h"
#include "SVGPathUtilities.h"
#include "StyleBuilder.h"
#include "StyleBuilderConverter.h"
#include "StylePropertyShorthand.h"
#include "StylePropertyShorthandFunctions.h"
#include "TimingFunction.h"
#include <bitset>
#include <memory>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

bool isCustomPropertyName(const String& propertyName)
{
    return propertyName.length() > 2 && propertyName.characterAt(0) == '-' && propertyName.characterAt(1) == '-';
}

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
    
    if (auto hashTableEntry = findProperty(buffer, length))
        return static_cast<CSSPropertyID>(hashTableEntry->id);

    return CSSPropertyInvalid;
}

static bool isAppleLegacyCssValueKeyword(const char* valueKeyword, unsigned length)
{
    static const char applePrefix[] = "-apple-";
    static const char appleSystemPrefix[] = "-apple-system";
    static const char applePayPrefix[] = "-apple-pay";

#if PLATFORM(COCOA)
    static const char* appleWirelessPlaybackTargetActive = getValueName(CSSValueAppleWirelessPlaybackTargetActive);
#endif

    return hasPrefix(valueKeyword, length, applePrefix)
    && !hasPrefix(valueKeyword, length, appleSystemPrefix)
    && !hasPrefix(valueKeyword, length, applePayPrefix)
#if PLATFORM(COCOA)
    && !equal(reinterpret_cast<const LChar*>(valueKeyword), reinterpret_cast<const LChar*>(appleWirelessPlaybackTargetActive), length)
#endif
    ;
}

template <typename CharacterType>
static CSSValueID cssValueKeywordID(const CharacterType* valueKeyword, unsigned length)
{
    char buffer[maxCSSValueKeywordLength + 1 + 1]; // 1 to turn "apple"/"khtml" into "webkit", 1 for null character
    
    for (unsigned i = 0; i != length; ++i) {
        CharacterType c = valueKeyword[i];
        if (!c || c >= 0x7F)
            return CSSValueInvalid; // illegal keyword.
        buffer[i] = toASCIILower(c);
    }
    buffer[length] = '\0';
    
    if (buffer[0] == '-') {
        // If the prefix is -apple- or -khtml-, change it to -webkit-.
        // This makes the string one character longer.
        // On iOS we don't want to change values starting with -apple-system to -webkit-system.
        // FIXME: Remove this mangling without breaking the web.
        if (isAppleLegacyCssValueKeyword(buffer, length)) {
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

CSSPropertyID cssPropertyID(StringView string)
{
    unsigned length = string.length();
    
    if (!length)
        return CSSPropertyInvalid;
    if (length > maxCSSPropertyNameLength)
        return CSSPropertyInvalid;
    
    return string.is8Bit() ? cssPropertyID(string.characters8(), length) : cssPropertyID(string.characters16(), length);
}
    
using namespace CSSPropertyParserHelpers;
using namespace CSSPropertyParserHelpersWorkerSafe;

CSSPropertyParser::CSSPropertyParser(const CSSParserTokenRange& range, const CSSParserContext& context, Vector<CSSProperty, 256>* parsedProperties, bool consumeWhitespace)
    : m_range(range)
    , m_context(context)
    , m_parsedProperties(parsedProperties)
{
    if (consumeWhitespace)
        m_range.consumeWhitespace();
}

void CSSPropertyParser::addProperty(CSSPropertyID property, CSSPropertyID currentShorthand, Ref<CSSValue>&& value, bool important, bool implicit)
{
    int shorthandIndex = 0;
    bool setFromShorthand = false;

    if (currentShorthand) {
        auto shorthands = matchingShorthandsForLonghand(property);
        setFromShorthand = true;
        if (shorthands.size() > 1)
            shorthandIndex = indexOfShorthandForLonghand(currentShorthand, shorthands);
    }

    // Allow anything to be set from a shorthand (e.g. the CSS all property always sets everything,
    // regardless of whether the longhands are enabled), and allow internal properties as we use
    // them to handle certain DOM-exposed values (e.g. -webkit-font-size-delta from
    // execCommand('FontSizeDelta')).
    ASSERT(isCSSPropertyExposed(property, &m_context.propertySettings) || setFromShorthand || isInternalCSSProperty(property));

    m_parsedProperties->append(CSSProperty(property, WTFMove(value), important, setFromShorthand, shorthandIndex, implicit));
}

void CSSPropertyParser::addPropertyWithImplicitDefault(CSSPropertyID property, CSSPropertyID currentShorthand, RefPtr<CSSValue>&& value, Ref<CSSValue>&& implicitDefault, bool important)
{
    if (value)
        addProperty(property, currentShorthand, value.releaseNonNull(), important, false);
    else
        addProperty(property, currentShorthand, WTFMove(implicitDefault), important, true);
}

void CSSPropertyParser::addExpandedPropertyForValue(CSSPropertyID property, Ref<CSSValue>&& value, bool important)
{
    for (auto longhand : shorthandForProperty(property))
        addProperty(longhand, property, value.copyRef(), important);
}

bool CSSPropertyParser::parseValue(CSSPropertyID propertyID, bool important, const CSSParserTokenRange& range, const CSSParserContext& context, ParsedPropertyVector& parsedProperties, StyleRuleType ruleType)
{
    int parsedPropertiesSize = parsedProperties.size();

    CSSPropertyParser parser(range, context, &parsedProperties);
    bool parseSuccess;

    if (ruleType == StyleRuleType::FontFace)
        parseSuccess = parser.parseFontFaceDescriptor(propertyID);
    else if (ruleType == StyleRuleType::FontPaletteValues)
        parseSuccess = parser.parseFontPaletteValuesDescriptor(propertyID);
    else if (ruleType == StyleRuleType::CounterStyle)
        parseSuccess = parser.parseCounterStyleDescriptor(propertyID, context);
    else if (ruleType == StyleRuleType::Keyframe)
        parseSuccess = parser.parseKeyframeDescriptor(propertyID, important);
    else
        parseSuccess = parser.parseValueStart(propertyID, important);

    if (!parseSuccess)
        parsedProperties.shrink(parsedPropertiesSize);

    return parseSuccess;
}

static RefPtr<CSSValue> maybeConsumeCSSWideKeyword(CSSParserTokenRange& range)
{
    CSSParserTokenRange rangeCopy = range;
    CSSValueID valueID = rangeCopy.consumeIncludingWhitespace().id();
    if (!rangeCopy.atEnd())
        return nullptr;

    if (!isCSSWideKeyword(valueID))
        return nullptr;

    range = rangeCopy;
    return CSSValuePool::singleton().createIdentifierValue(valueID);
}

RefPtr<CSSValue> CSSPropertyParser::parseSingleValue(CSSPropertyID property, const CSSParserTokenRange& range, const CSSParserContext& context)
{
    CSSPropertyParser parser(range, context, nullptr);
    if (auto value = maybeConsumeCSSWideKeyword(parser.m_range))
        return value;
    
    RefPtr<CSSValue> value = parser.parseSingleValue(property);
    if (!value || !parser.m_range.atEnd())
        return nullptr;
    return value;
}

bool CSSPropertyParser::canParseTypedCustomPropertyValue(const String& syntax, const CSSParserTokenRange& tokens, const CSSParserContext& context)
{
    CSSPropertyParser parser(tokens, context, nullptr);
    return parser.canParseTypedCustomPropertyValue(syntax);
}

RefPtr<CSSCustomPropertyValue> CSSPropertyParser::parseTypedCustomPropertyValue(const AtomString& name, const String& syntax, const CSSParserTokenRange& tokens, const Style::BuilderState& builderState, const CSSParserContext& context)
{
    CSSPropertyParser parser(tokens, context, nullptr, false);
    RefPtr<CSSCustomPropertyValue> value = parser.parseTypedCustomPropertyValue(name, syntax, builderState);
    if (!value || !parser.m_range.atEnd())
        return nullptr;
    return value;
}

void CSSPropertyParser::collectParsedCustomPropertyValueDependencies(const String& syntax, bool isRoot, HashSet<CSSPropertyID>& dependencies, const CSSParserTokenRange& tokens, const CSSParserContext& context)
{
    CSSPropertyParser parser(tokens, context, nullptr);
    parser.collectParsedCustomPropertyValueDependencies(syntax, isRoot, dependencies);
}

bool CSSPropertyParser::parseValueStart(CSSPropertyID propertyID, bool important)
{
    if (consumeCSSWideKeyword(propertyID, important))
        return true;

    CSSParserTokenRange originalRange = m_range;
    bool isShorthand = isShorthandCSSProperty(propertyID);

    if (isShorthand) {
        // Variable references will fail to parse here and will fall out to the variable ref parser below.
        if (parseShorthand(propertyID, important))
            return true;
    } else {
        RefPtr<CSSValue> parsedValue = parseSingleValue(propertyID);
        if (parsedValue && m_range.atEnd()) {
            addProperty(propertyID, CSSPropertyInvalid, *parsedValue, important);
            return true;
        }
    }

    if (CSSVariableParser::containsValidVariableReferences(originalRange, m_context)) {
        auto variable = CSSVariableReferenceValue::create(originalRange, m_context);
        if (isShorthand)
            addExpandedPropertyForValue(propertyID, CSSPendingSubstitutionValue::create(propertyID, WTFMove(variable)), important);
        else
            addProperty(propertyID, CSSPropertyInvalid, WTFMove(variable), important);
        return true;
    }

    return false;
}
 
bool CSSPropertyParser::consumeCSSWideKeyword(CSSPropertyID propertyID, bool important)
{
    CSSParserTokenRange rangeCopy = m_range;
    auto value = maybeConsumeCSSWideKeyword(rangeCopy);
    if (!value)
        return false;
    
    const StylePropertyShorthand& shorthand = shorthandForProperty(propertyID);
    if (!shorthand.length()) {
        if (CSSProperty::isDescriptorOnly(propertyID))
            return false;
        addProperty(propertyID, CSSPropertyInvalid, value.releaseNonNull(), important);
    } else
        addExpandedPropertyForValue(propertyID, value.releaseNonNull(), important);
    m_range = rangeCopy;
    return true;
}

bool CSSPropertyParser::consumeTransformOrigin(bool important)
{
    if (auto resultXY = consumeOneOrTwoValuedPositionCoordinates(m_range, m_context.mode, UnitlessQuirk::Forbid)) {
        m_range.consumeWhitespace();
        bool atEnd = m_range.atEnd();
        auto resultZ = consumeLength(m_range, m_context.mode, ValueRange::All);
        if ((!resultZ && !atEnd) || !m_range.atEnd())
            return false;
        addProperty(CSSPropertyTransformOriginX, CSSPropertyTransformOrigin, WTFMove(resultXY->x), important);
        addProperty(CSSPropertyTransformOriginY, CSSPropertyTransformOrigin, WTFMove(resultXY->y), important);
        addPropertyWithImplicitDefault(CSSPropertyTransformOriginZ, CSSPropertyTransformOrigin, resultZ, CSSValuePool::singleton().createValue(0, CSSUnitType::CSS_PX), important);
        
        return true;
    }
    return false;
}

bool CSSPropertyParser::consumePerspectiveOrigin(bool important)
{
    if (auto result = consumePositionCoordinates(m_range, m_context.mode, UnitlessQuirk::Forbid, PositionSyntax::Position)) {
        addProperty(CSSPropertyPerspectiveOriginX, CSSPropertyPerspectiveOrigin, WTFMove(result->x), important);
        addProperty(CSSPropertyPerspectiveOriginY, CSSPropertyPerspectiveOrigin, WTFMove(result->y), important);
        return true;
    }
    return false;
}

// Methods for consuming non-shorthand properties starts here.
static RefPtr<CSSValue> consumeDisplay(CSSParserTokenRange& range)
{
    // Parse single keyword values
    auto singleKeyword = consumeIdent<
        // <display-box>
        CSSValueContents,
        CSSValueNone,
        // <display-internal>
        CSSValueTableCaption,
        CSSValueTableCell,
        CSSValueTableColumnGroup,
        CSSValueTableColumn,
        CSSValueTableHeaderGroup,
        CSSValueTableFooterGroup,
        CSSValueTableRow,
        CSSValueTableRowGroup,
        // <display-legacy>
        CSSValueInlineBlock,
        CSSValueInlineFlex,
        CSSValueInlineGrid,
        CSSValueInlineTable,
        // Prefixed values
        CSSValueWebkitInlineBox,
        CSSValueWebkitBox,
        // No layout support for the full <display-listitem> syntax, so treat it as <display-legacy>
        CSSValueListItem
    >(range);

    if (singleKeyword)
        return singleKeyword;

    // Empty value, stop parsing
    if (range.atEnd())
        return nullptr;

    // Convert -webkit-flex/-webkit-inline-flex to flex/inline-flex
    CSSValueID nextValueID = range.peek().id();
    if (nextValueID == CSSValueWebkitInlineFlex || nextValueID == CSSValueWebkitFlex) {
        consumeIdent(range);
        return CSSValuePool::singleton().createValue(
            nextValueID == CSSValueWebkitInlineFlex ? CSSValueInlineFlex : CSSValueFlex);
    }

    // Parse [ <display-outside> || <display-inside> ]
    std::optional<CSSValueID> parsedDisplayOutside;
    std::optional<CSSValueID> parsedDisplayInside;
    while (!range.atEnd()) {
        auto nextValueID = range.peek().id();
        switch (nextValueID) {
        // <display-outside>
        case CSSValueBlock:
        case CSSValueInline:
            if (parsedDisplayOutside)
                return nullptr;
            parsedDisplayOutside = nextValueID;
            break;
        // <display-inside>
        case CSSValueFlex:
        case CSSValueFlow:
        case CSSValueFlowRoot:
        case CSSValueGrid:
        case CSSValueTable:
            if (parsedDisplayInside)
                return nullptr;
            parsedDisplayInside = nextValueID;
            break;
        default:
            return nullptr;
        }
        consumeIdent(range);
    }

    // Set defaults when one of the two values are unspecified
    CSSValueID displayOutside = parsedDisplayOutside.value_or(CSSValueBlock);
    CSSValueID displayInside = parsedDisplayInside.value_or(CSSValueFlow);

    auto selectShortValue = [&]() -> CSSValueID {
        if (displayOutside == CSSValueBlock) {
            // Alias display: flow to display: block
            return displayInside == CSSValueFlow ? CSSValueBlock : displayInside;
        }

        // Convert `display: inline <display-inside>` to the equivalent short value
        switch (displayInside) {
        case CSSValueFlex:
            return CSSValueInlineFlex;
        case CSSValueFlow:
            return CSSValueInline;
        case CSSValueFlowRoot:
            return CSSValueInlineBlock;
        case CSSValueGrid:
            return CSSValueInlineGrid;
        case CSSValueTable:
            return CSSValueInlineTable;
        default:
            ASSERT_NOT_REACHED();
            return CSSValueInline;
        }
    };

    return CSSValuePool::singleton().createValue(selectShortValue());
}

static RefPtr<CSSValue> consumeWillChange(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);

    RefPtr<CSSValueList> values = CSSValueList::createCommaSeparated();
    // Every comma-separated list of identifiers is a valid will-change value,
    // unless the list includes an explicitly disallowed identifier.
    while (!range.atEnd()) {
        switch (range.peek().id()) {
        case CSSValueContents:
        case CSSValueScrollPosition:
            values->append(consumeIdent(range).releaseNonNull());
            break;
        case CSSValueNone:
        case CSSValueAll:
        case CSSValueAuto:
            return nullptr;
        default:
            if (range.peek().type() != IdentToken)
                return nullptr;
            CSSPropertyID propertyID = cssPropertyID(range.peek().value());
            if (propertyID == CSSPropertyWillChange)
                return nullptr;
            if (!isCSSPropertyExposed(propertyID, &context.propertySettings))
                propertyID = CSSPropertyInvalid;
            if (propertyID != CSSPropertyInvalid) {
                values->append(CSSValuePool::singleton().createIdentifierValue(propertyID));
                range.consumeIncludingWhitespace();
                break;
            }
            if (auto customIdent = consumeCustomIdent(range)) {
                // Append properties we don't recognize, but that are legal, as strings.
                values->append(customIdent.releaseNonNull());
                break;
            }
            return nullptr;
        }

        // This is a comma separated list
        if (!range.atEnd() && !consumeCommaIncludingWhitespace(range))
            return nullptr;
    }

    return values;
}

#if ENABLE(VARIATION_FONTS)
static RefPtr<CSSValue> consumeFontVariationTag(CSSParserTokenRange& range)
{
    if (range.peek().type() != StringToken)
        return nullptr;
    
    auto string = range.consumeIncludingWhitespace().value();
    
    FontTag tag;
    if (string.length() != tag.size())
        return nullptr;
    for (unsigned i = 0; i < tag.size(); ++i) {
        // Limits the range of characters to 0x20-0x7E, following the tag name rules defiend in the OpenType specification.
        UChar character = string[i];
        if (character < 0x20 || character > 0x7E)
            return nullptr;
        tag[i] = character;
    }
    
    if (range.atEnd())
        return nullptr;

    auto tagValue = consumeNumberRaw(range);
    if (!tagValue)
        return nullptr;
    
    return CSSFontVariationValue::create(tag, tagValue->value);
}
    
static RefPtr<CSSValue> consumeFontVariationSettings(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);
    
    auto settings = CSSValueList::createCommaSeparated();
    do {
        RefPtr<CSSValue> variationValue = consumeFontVariationTag(range);
        if (!variationValue)
            return nullptr;
        settings->append(variationValue.releaseNonNull());
    } while (consumeCommaIncludingWhitespace(range));
    
    if (!settings->length())
        return nullptr;

    return settings;
}
#endif // ENABLE(VARIATION_FONTS)

static RefPtr<CSSValue> consumePage(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeCustomIdent(range);
}

static RefPtr<CSSValue> consumeQuotes(CSSParserTokenRange& range)
{
    auto id = range.peek().id();
    if (id == CSSValueNone || id == CSSValueAuto)
        return consumeIdent(range);
    RefPtr<CSSValueList> values = CSSValueList::createSpaceSeparated();
    while (!range.atEnd()) {
        RefPtr<CSSPrimitiveValue> parsedValue = consumeString(range);
        if (!parsedValue)
            return nullptr;
        values->append(parsedValue.releaseNonNull());
    }
    if (values->length() && values->length() % 2 == 0)
        return values;
    return nullptr;
}

class FontVariantLigaturesParser {
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
        m_result->append(consumeIdent(range).releaseNonNull());
        return ParseResult::ConsumedValue;
    }

    RefPtr<CSSValue> finalizeValue()
    {
        if (!m_result->length())
            return CSSValuePool::singleton().createIdentifierValue(CSSValueNormal);
        return WTFMove(m_result);
    }

private:
    bool m_sawCommonLigaturesValue;
    bool m_sawDiscretionaryLigaturesValue;
    bool m_sawHistoricalLigaturesValue;
    bool m_sawContextualLigaturesValue;
    RefPtr<CSSValueList> m_result;
};

static RefPtr<CSSValue> consumeFontVariantLigatures(CSSParserTokenRange& range)
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

static RefPtr<CSSValue> consumeFontVariantEastAsian(CSSParserTokenRange& range)
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

    RefPtr<CSSValueList> values = CSSValueList::createSpaceSeparated();
    switch (variant.value_or(FontVariantEastAsianVariant::Normal)) {
    case FontVariantEastAsianVariant::Normal:
        break;
    case FontVariantEastAsianVariant::Jis78:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueJis78));
        break;
    case FontVariantEastAsianVariant::Jis83:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueJis83));
        break;
    case FontVariantEastAsianVariant::Jis90:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueJis90));
        break;
    case FontVariantEastAsianVariant::Jis04:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueJis04));
        break;
    case FontVariantEastAsianVariant::Simplified:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueSimplified));
        break;
    case FontVariantEastAsianVariant::Traditional:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueTraditional));
        break;
    }

    switch (width.value_or(FontVariantEastAsianWidth::Normal)) {
    case FontVariantEastAsianWidth::Normal:
        break;
    case FontVariantEastAsianWidth::Full:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueFullWidth));
        break;
    case FontVariantEastAsianWidth::Proportional:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueProportionalWidth));
        break;
    }
        
    switch (ruby.value_or(FontVariantEastAsianRuby::Normal)) {
    case FontVariantEastAsianRuby::Normal:
        break;
    case FontVariantEastAsianRuby::Yes:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueRuby));
    }

    if (!values->length())
        return nullptr;

    return values;
}
    
static RefPtr<CSSPrimitiveValue> consumeFontVariantCaps(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueNormal, CSSValueSmallCaps, CSSValueAllSmallCaps,
        CSSValuePetiteCaps, CSSValueAllPetiteCaps,
        CSSValueUnicase, CSSValueTitlingCaps>(range);
}

static RefPtr<CSSValue> consumeFontVariantAlternates(CSSParserTokenRange& range)
{
    if (range.atEnd())
        return nullptr;

    if (range.peek().id() == CSSValueNormal) {
        consumeIdent<CSSValueNormal>(range);
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNormal);
    }

    auto result = FontVariantAlternates::Normal();

    auto parseSomethingWithoutError = [&range, &result]() {
        bool hasParsedSomething = false;
        auto parseAndSetArgument = [&range, &hasParsedSomething] (auto& value) {
            CSSParserTokenRange args = consumeFunction(range);
            auto ident = consumeCustomIdent(args);
            if (!args.atEnd())
                return false;
        
            if (!ident)
                return false;
        
            if (value)
                return false;
        
            hasParsedSomething = true;
            value = ident->stringValue();
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
                if (!parseAndSetArgument(result.valuesRef().styleset))
                    return false;
            } else if (token.functionId() == CSSValueCharacterVariant) {
                if (!parseAndSetArgument(result.valuesRef().characterVariant))
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

static RefPtr<CSSPrimitiveValue> consumeFontVariantPosition(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueNormal, CSSValueSub, CSSValueSuper>(range);
}

class FontVariantNumericParser {
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
        m_result->append(consumeIdent(range).releaseNonNull());
        return ParseResult::ConsumedValue;
    }

    RefPtr<CSSValue> finalizeValue()
    {
        if (!m_result->length())
            return CSSValuePool::singleton().createIdentifierValue(CSSValueNormal);
        return WTFMove(m_result);
    }


private:
    bool m_sawNumericFigureValue;
    bool m_sawNumericSpacingValue;
    bool m_sawNumericFractionValue;
    bool m_sawOrdinalValue;
    bool m_sawSlashedZeroValue;
    RefPtr<CSSValueList> m_result;
};

static RefPtr<CSSValue> consumeFontVariantNumeric(CSSParserTokenRange& range)
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

static RefPtr<CSSPrimitiveValue> consumeFontVariantCSS21(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueNormal, CSSValueSmallCaps>(range);
}

static RefPtr<CSSPrimitiveValue> consumeFontWeight(CSSParserTokenRange& range)
{
    if (auto result = consumeFontWeightRaw(range)) {
        return WTF::switchOn(*result, [] (CSSValueID valueID) {
            return CSSValuePool::singleton().createIdentifierValue(valueID);
        }, [] (double weightNumber) {
            return CSSValuePool::singleton().createValue(weightNumber, CSSUnitType::CSS_NUMBER);
        });
    }
    return nullptr;
}

static RefPtr<CSSPrimitiveValue> consumeFontPalette(CSSParserTokenRange& range)
{
    if (auto result = consumeIdent<CSSValueNormal, CSSValueLight, CSSValueDark>(range))
        return result;
    return consumeDashedIdent(range);
}

static RefPtr<CSSValue> consumeFamilyName(CSSParserTokenRange& range)
{
    auto familyName = consumeFamilyNameRaw(range);
    if (familyName.isNull())
        return nullptr;
    return CSSValuePool::singleton().createFontFamilyValue(familyName);
}

static RefPtr<CSSValue> consumeGenericFamily(CSSParserTokenRange& range)
{
    return consumeIdentRange(range, CSSValueSerif, CSSValueWebkitBody);
}

static RefPtr<CSSValueList> consumeFontFamily(CSSParserTokenRange& range)
{
    RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    do {
        if (auto parsedValue = consumeGenericFamily(range))
            list->append(parsedValue.releaseNonNull());
        else {
            if (auto parsedValue = consumeFamilyName(range))
                list->append(parsedValue.releaseNonNull());
            else
                return nullptr;
        }
    } while (consumeCommaIncludingWhitespace(range));
    return list;
}

static RefPtr<CSSValueList> consumeFontFamilyDescriptor(CSSParserTokenRange& range)
{
    // FIXME-NEWPARSER: https://bugs.webkit.org/show_bug.cgi?id=196381 For compatibility with the old parser, we have to make
    // a list here, even though the list always contains only a single family name.
    // Once the old parser is gone, we can delete this function, make the caller
    // use consumeFamilyName instead, and then patch the @font-face code to
    // not expect a list with a single name in it.
    RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    RefPtr<CSSValue> parsedValue = consumeFamilyName(range);
    if (parsedValue)
        list->append(parsedValue.releaseNonNull());
    
    if (!range.atEnd() || !list->length())
        return nullptr;

    return list;
}

bool CSSPropertyParser::consumeFontSynthesis(bool important)
{
    // none | [ weight || style || small-caps ]
    if (m_range.peek().id() == CSSValueNone) {
        addProperty(CSSPropertyFontSynthesisSmallCaps, CSSPropertyFontSynthesis, consumeIdent(m_range).releaseNonNull(), important);
        addProperty(CSSPropertyFontSynthesisStyle, CSSPropertyFontSynthesis, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);
        addProperty(CSSPropertyFontSynthesisWeight, CSSPropertyFontSynthesis, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);
        return m_range.atEnd();
    }

    bool foundWeight = false;
    bool foundStyle = false;
    bool foundSmallCaps = false;

    auto checkAndMarkExistence = [](bool* found) {
        if (*found)
            return false;
        return *found = true;
    };

    while (!m_range.atEnd()) {
        auto ident = consumeIdent<CSSValueWeight, CSSValueStyle, CSSValueSmallCaps>(m_range);
        if (!ident)
            return false;
        switch (ident->valueID()) {
        case CSSValueWeight:
            if (!checkAndMarkExistence(&foundWeight))
                return false;
            break;
        case CSSValueStyle:
            if (!checkAndMarkExistence(&foundStyle))
                return false;
            break;
        case CSSValueSmallCaps:
            if (!checkAndMarkExistence(&foundSmallCaps))
                return false;
            break;
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    }

    addProperty(CSSPropertyFontSynthesisWeight, CSSPropertyFontSynthesis, CSSValuePool::singleton().createIdentifierValue(foundWeight ? CSSValueAuto : CSSValueNone), important);
    addProperty(CSSPropertyFontSynthesisStyle, CSSPropertyFontSynthesis, CSSValuePool::singleton().createIdentifierValue(foundStyle ? CSSValueAuto : CSSValueNone), important);
    addProperty(CSSPropertyFontSynthesisSmallCaps, CSSPropertyFontSynthesis, CSSValuePool::singleton().createIdentifierValue(foundSmallCaps ? CSSValueAuto : CSSValueNone), important);
    
    return true;
}

static RefPtr<CSSValue> consumeFontSynthesisLonghand(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueNone, CSSValueAuto>(range);
}

static RefPtr<CSSValue> consumeLetterSpacing(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);
    
    return consumeLength(range, cssParserMode, ValueRange::All, UnitlessQuirk::Allow);
}

static RefPtr<CSSValue> consumeWordSpacing(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);
    
    return consumeLengthOrPercent(range, cssParserMode, ValueRange::All, UnitlessQuirk::Allow);
}
    
static RefPtr<CSSValue> consumeTabSize(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    auto tabSize = consumeNumber(range, ValueRange::NonNegative);
    if (tabSize)
        return tabSize;
    return consumeLength(range, cssParserMode, ValueRange::NonNegative);
}

#if ENABLE(TEXT_AUTOSIZING)
static RefPtr<CSSValue> consumeTextSizeAdjust(CSSParserTokenRange& range, CSSParserMode /* cssParserMode */)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    return consumePercent(range, ValueRange::NonNegative);
}
#endif

static RefPtr<CSSValue> consumeFontSize(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless = UnitlessQuirk::Forbid)
{
    if (range.peek().id() >= CSSValueXxSmall && range.peek().id() <= CSSValueLarger)
        return consumeIdent(range);
    return consumeLengthOrPercent(range, cssParserMode, ValueRange::NonNegative, unitless);
}

static RefPtr<CSSPrimitiveValue> consumeLineHeight(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);

    RefPtr<CSSPrimitiveValue> lineHeight = consumeNumber(range, ValueRange::NonNegative);
    if (lineHeight)
        return lineHeight;
    return consumeLengthOrPercent(range, cssParserMode, ValueRange::NonNegative);
}

template<typename... Args>
static Ref<CSSPrimitiveValue> createPrimitiveValuePair(Args&&... args)
{
    return CSSValuePool::singleton().createValue(Pair::create(std::forward<Args>(args)...));
}


static RefPtr<CSSValue> consumeCounter(CSSParserTokenRange& range, int defaultValue)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    do {
        RefPtr<CSSPrimitiveValue> counterName = consumeCustomIdent(range);
        if (!counterName)
            return nullptr;
        int i = defaultValue;
        if (auto counterValue = consumeIntegerRaw(range))
            i = *counterValue;
        list->append(createPrimitiveValuePair(counterName.releaseNonNull(), CSSPrimitiveValue::create(i, CSSUnitType::CSS_INTEGER), Pair::IdenticalValueEncoding::Coalesce));
    } while (!range.atEnd());
    return list;
}

static RefPtr<CSSValue> consumePageSize(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueA3, CSSValueA4, CSSValueA5, CSSValueB4, CSSValueB5, CSSValueLedger, CSSValueLegal, CSSValueLetter>(range);
}

static RefPtr<CSSValueList> consumeSize(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    RefPtr<CSSValueList> result = CSSValueList::createSpaceSeparated();

    if (range.peek().id() == CSSValueAuto) {
        result->append(consumeIdent(range).releaseNonNull());
        return result;
    }

    if (RefPtr<CSSValue> width = consumeLength(range, cssParserMode, ValueRange::NonNegative)) {
        RefPtr<CSSValue> height = consumeLength(range, cssParserMode, ValueRange::NonNegative);
        result->append(width.releaseNonNull());
        if (height)
            result->append(height.releaseNonNull());
        return result;
    }

    RefPtr<CSSValue> pageSize = consumePageSize(range);
    RefPtr<CSSValue> orientation = consumeIdent<CSSValuePortrait, CSSValueLandscape>(range);
    if (!pageSize)
        pageSize = consumePageSize(range);

    if (!orientation && !pageSize)
        return nullptr;
    if (pageSize)
        result->append(pageSize.releaseNonNull());
    if (orientation)
        result->append(orientation.releaseNonNull());
    return result;
}

static RefPtr<CSSValue> consumeTextIndent(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    // [ <length> | <percentage> ] && hanging? && each-line?
    RefPtr<CSSValue> lengthOrPercentage;
    RefPtr<CSSPrimitiveValue> eachLine;
    RefPtr<CSSPrimitiveValue> hanging;

    do {
        if (!lengthOrPercentage) {
            if (RefPtr<CSSValue> textIndent = consumeLengthOrPercent(range, cssParserMode, ValueRange::All, UnitlessQuirk::Allow)) {
                lengthOrPercentage = textIndent;
                continue;
            }
        }

        CSSValueID id = range.peek().id();
        if (!eachLine && id == CSSValueEachLine) {
            eachLine = consumeIdent(range);
            continue;
        }

        if (!hanging && id == CSSValueHanging) {
            hanging = consumeIdent(range);
            continue;
        }

        return nullptr;
    } while (!range.atEnd());

    if (!lengthOrPercentage)
        return nullptr;

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    list->append(*lengthOrPercentage);
    if (hanging)
        list->append(hanging.releaseNonNull());
    if (eachLine)
        list->append(eachLine.releaseNonNull());

    return list;
}

static RefPtr<CSSValue> consumeScrollPadding(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeLengthOrPercent(range, cssParserMode, ValueRange::NonNegative);
}

static bool validWidthOrHeightKeyword(CSSValueID id, const CSSParserContext& /*context*/)
{
    if (id == CSSValueIntrinsic || id == CSSValueMinIntrinsic || id == CSSValueMinContent || id == CSSValueWebkitMinContent || id == CSSValueMaxContent || id == CSSValueWebkitMaxContent || id == CSSValueWebkitFillAvailable || id == CSSValueFitContent || id == CSSValueWebkitFitContent) {
        return true;
    }
    return false;
}

static RefPtr<CSSValue> consumeMaxWidthOrHeight(CSSParserTokenRange& range, const CSSParserContext& context, UnitlessQuirk unitless = UnitlessQuirk::Forbid)
{
    if (range.peek().id() == CSSValueNone || validWidthOrHeightKeyword(range.peek().id(), context))
        return consumeIdent(range);
    return consumeLengthOrPercent(range, context.mode, ValueRange::NonNegative, unitless);
}

static RefPtr<CSSValue> consumeWidthOrHeight(CSSParserTokenRange& range, const CSSParserContext& context, UnitlessQuirk unitless = UnitlessQuirk::Forbid)
{
    if (range.peek().id() == CSSValueAuto || validWidthOrHeightKeyword(range.peek().id(), context))
        return consumeIdent(range);
    return consumeLengthOrPercent(range, context.mode, ValueRange::NonNegative, unitless);
}

static RefPtr<CSSValue> consumeMarginOrOffset(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeLengthOrPercent(range, cssParserMode, ValueRange::All, unitless);
}

static RefPtr<CSSPrimitiveValue> consumeClipComponent(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeLength(range, cssParserMode, ValueRange::All, UnitlessQuirk::Allow);
}

static RefPtr<CSSValue> consumeClip(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);

    if (range.peek().functionId() != CSSValueRect)
        return nullptr;

    CSSParserTokenRange args = consumeFunction(range);
    // rect(t, r, b, l) || rect(t r b l)
    RefPtr<CSSPrimitiveValue> top = consumeClipComponent(args, cssParserMode);
    if (!top)
        return nullptr;
    bool needsComma = consumeCommaIncludingWhitespace(args);
    RefPtr<CSSPrimitiveValue> right = consumeClipComponent(args, cssParserMode);
    if (!right || (needsComma && !consumeCommaIncludingWhitespace(args)))
        return nullptr;
    RefPtr<CSSPrimitiveValue> bottom = consumeClipComponent(args, cssParserMode);
    if (!bottom || (needsComma && !consumeCommaIncludingWhitespace(args)))
        return nullptr;
    RefPtr<CSSPrimitiveValue> left = consumeClipComponent(args, cssParserMode);
    if (!left || !args.atEnd())
        return nullptr;
    
    auto rect = Rect::create();
    rect->setLeft(left.releaseNonNull());
    rect->setTop(top.releaseNonNull());
    rect->setRight(right.releaseNonNull());
    rect->setBottom(bottom.releaseNonNull());
    return CSSValuePool::singleton().createValue(WTFMove(rect));
}

static RefPtr<CSSValue> consumeTouchAction(CSSParserTokenRange& range)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueNone || id == CSSValueAuto || id == CSSValueManipulation)
        return consumeIdent(range);

    auto list = CSSValueList::createSpaceSeparated();
    while (true) {
        auto ident = consumeIdent<CSSValuePanX, CSSValuePanY, CSSValuePinchZoom>(range);
        if (!ident)
            break;
        if (list->hasValue(ident.get()))
            return nullptr;
        list->append(ident.releaseNonNull());
    }

    if (!list->length())
        return nullptr;
    return list;
}

static RefPtr<CSSPrimitiveValue> consumeLineClamp(CSSParserTokenRange& range)
{
    if (auto clampValue = consumePercent(range, ValueRange::NonNegative))
        return clampValue;
    // When specifying number of lines, don't allow 0 as a valid value.
    return consumePositiveInteger(range);
}

static RefPtr<CSSValue> consumeAutoOrString(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeString(range);
}

static RefPtr<CSSValue> consumeHyphenateLimit(CSSParserTokenRange& range, CSSValueID valueID)
{
    if (range.peek().id() == valueID)
        return consumeIdent(range);
    return consumeNumber(range, ValueRange::NonNegative);
}

static RefPtr<CSSValue> consumeColumnWidth(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    // Always parse lengths in strict mode here, since it would be ambiguous otherwise when used in
    // the 'columns' shorthand property.
    RefPtr<CSSPrimitiveValue> columnWidth = consumeLength(range, HTMLStandardMode, ValueRange::NonNegative);
    if (!columnWidth || columnWidth->isZero().value_or(false))
        return nullptr;

    return columnWidth;
}

static RefPtr<CSSValue> consumeColumnCount(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumePositiveInteger(range);
}

static RefPtr<CSSValue> consumeGapLength(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);
    return consumeLengthOrPercent(range, cssParserMode, ValueRange::NonNegative);
}

static RefPtr<CSSValue> consumeColumnSpan(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueAll, CSSValueNone>(range);
}

static RefPtr<CSSValue> consumeZoom(CSSParserTokenRange& range, const CSSParserContext& /*context*/)
{
    const CSSParserToken& token = range.peek();
    RefPtr<CSSPrimitiveValue> zoom;
    if (token.type() == IdentToken)
        zoom = consumeIdent<CSSValueNormal, CSSValueReset, CSSValueDocument>(range);
    else {
        zoom = consumePercent(range, ValueRange::NonNegative);
        if (!zoom)
            zoom = consumeNumber(range, ValueRange::NonNegative);
    }
    return zoom;
}

static RefPtr<CSSValue> consumeAnimationIterationCount(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueInfinite)
        return consumeIdent(range);
    return consumeNumber(range, ValueRange::NonNegative);
}

static RefPtr<CSSValue> consumeAnimationName(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    if (range.peek().type() == StringToken) {
        const CSSParserToken& token = range.consumeIncludingWhitespace();
        if (equalLettersIgnoringASCIICase(token.value(), "none"_s))
            return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);
        return CSSValuePool::singleton().createValue(token.value().toString(), CSSUnitType::CSS_STRING);
    }

    return consumeCustomIdent(range);
}

static RefPtr<CSSValue> consumeTransitionProperty(CSSParserTokenRange& range)
{
    const CSSParserToken& token = range.peek();
    if (token.type() != IdentToken)
        return nullptr;
    if (token.id() == CSSValueNone)
        return consumeIdent(range);

    if (CSSPropertyID property = token.parseAsCSSPropertyID()) {
        range.consumeIncludingWhitespace();
        return CSSValuePool::singleton().createIdentifierValue(property);
    }
    return consumeCustomIdent(range);
}

    
static RefPtr<CSSValue> consumeSteps(CSSParserTokenRange& range)
{
    // https://drafts.csswg.org/css-easing-1/#funcdef-step-easing-function-steps

    ASSERT(range.peek().functionId() == CSSValueSteps);
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);
    
    auto stepsValue = consumePositiveIntegerRaw(args);
    if (!stepsValue)
        return nullptr;
    
    std::optional<StepsTimingFunction::StepPosition> stepPosition;
    if (consumeCommaIncludingWhitespace(args)) {
        switch (args.consumeIncludingWhitespace().id()) {
        case CSSValueJumpStart:
            stepPosition = StepsTimingFunction::StepPosition::JumpStart;
            break;

        case CSSValueJumpEnd:
            stepPosition = StepsTimingFunction::StepPosition::JumpEnd;
            break;

        case CSSValueJumpNone:
            stepPosition = StepsTimingFunction::StepPosition::JumpNone;
            break;

        case CSSValueJumpBoth:
            stepPosition = StepsTimingFunction::StepPosition::JumpBoth;
            break;

        case CSSValueStart:
            stepPosition = StepsTimingFunction::StepPosition::Start;
            break;

        case CSSValueEnd:
            stepPosition = StepsTimingFunction::StepPosition::End;
            break;

        default:
            return nullptr;
        }
    }
    
    if (!args.atEnd())
        return nullptr;

    if (*stepsValue == 1 && stepPosition == StepsTimingFunction::StepPosition::JumpNone)
        return nullptr;

    range = rangeCopy;
    return CSSStepsTimingFunctionValue::create(*stepsValue, stepPosition);
}

static RefPtr<CSSValue> consumeCubicBezier(CSSParserTokenRange& range)
{
    ASSERT(range.peek().functionId() == CSSValueCubicBezier);
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);

    auto x1 = consumeNumberRaw(args);
    if (!x1 || x1->value < 0 || x1->value > 1)
        return nullptr;
    
    if (!consumeCommaIncludingWhitespace(args))
        return nullptr;

    auto y1 = consumeNumberRaw(args);
    if (!y1)
        return nullptr;

    if (!consumeCommaIncludingWhitespace(args))
        return nullptr;

    auto x2 = consumeNumberRaw(args);
    if (!x2 || x2->value < 0 || x2->value > 1)
        return nullptr;

    if (!consumeCommaIncludingWhitespace(args))
        return nullptr;

    auto y2 = consumeNumberRaw(args);
    if (!y2)
        return nullptr;

    if (!args.atEnd())
        return nullptr;

    range = rangeCopy;
    return CSSCubicBezierTimingFunctionValue::create(x1->value, y1->value, x2->value, y2->value);
}

static RefPtr<CSSValue> consumeSpringFunction(CSSParserTokenRange& range)
{
    ASSERT(range.peek().functionId() == CSSValueSpring);
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);

    // Mass must be greater than 0.
    auto mass = consumeNumberRaw(args);
    if (!mass || mass->value <= 0)
        return nullptr;
    
    // Stiffness must be greater than 0.
    auto stiffness = consumeNumberRaw(args);
    if (!stiffness || stiffness->value <= 0)
        return nullptr;
    
    // Damping coefficient must be greater than or equal to 0.
    auto damping = consumeNumberRaw(args);
    if (!damping || damping->value < 0)
        return nullptr;
    
    // Initial velocity may have any value.
    auto initialVelocity = consumeNumberRaw(args);
    if (!initialVelocity)
        return nullptr;

    if (!args.atEnd())
        return nullptr;

    range = rangeCopy;

    return CSSSpringTimingFunctionValue::create(mass->value, stiffness->value, damping->value, initialVelocity->value);
}

static RefPtr<CSSValue> consumeAnimationTimingFunction(CSSParserTokenRange& range, const CSSParserContext& context)
{
    switch (range.peek().id()) {
    case CSSValueLinear:
    case CSSValueEase:
    case CSSValueEaseIn:
    case CSSValueEaseOut:
    case CSSValueEaseInOut:
        return consumeIdent(range);

    case CSSValueStepStart:
        range.consumeIncludingWhitespace();
        return CSSStepsTimingFunctionValue::create(1, StepsTimingFunction::StepPosition::Start);

    case CSSValueStepEnd:
        range.consumeIncludingWhitespace();
        return CSSStepsTimingFunctionValue::create(1, StepsTimingFunction::StepPosition::End);

    default:
        break;
    }

    CSSValueID function = range.peek().functionId();
    if (function == CSSValueCubicBezier)
        return consumeCubicBezier(range);
    if (function == CSSValueSteps)
        return consumeSteps(range);
    if (context.springTimingFunctionEnabled && function == CSSValueSpring)
        return consumeSpringFunction(range);
    return nullptr;
}

static RefPtr<CSSValue> consumeAnimationValue(CSSPropertyID property, CSSParserTokenRange& range, const CSSParserContext& context)
{
    switch (property) {
    case CSSPropertyAnimationDelay:
    case CSSPropertyTransitionDelay:
        return consumeTime(range, context.mode, ValueRange::All, UnitlessQuirk::Forbid);
    case CSSPropertyAnimationDirection:
        return consumeIdent<CSSValueNormal, CSSValueAlternate, CSSValueReverse, CSSValueAlternateReverse>(range);
    case CSSPropertyAnimationDuration:
    case CSSPropertyTransitionDuration:
        return consumeTime(range, context.mode, ValueRange::NonNegative, UnitlessQuirk::Forbid);
    case CSSPropertyAnimationFillMode:
        return consumeIdent<CSSValueNone, CSSValueForwards, CSSValueBackwards, CSSValueBoth>(range);
    case CSSPropertyAnimationIterationCount:
        return consumeAnimationIterationCount(range);
    case CSSPropertyAnimationName:
        return consumeAnimationName(range);
    case CSSPropertyAnimationPlayState:
        return consumeIdent<CSSValueRunning, CSSValuePaused>(range);
    case CSSPropertyAnimationComposition:
        return consumeIdent<CSSValueAccumulate, CSSValueAdd, CSSValueReplace>(range);
    case CSSPropertyTransitionProperty:
        return consumeTransitionProperty(range);
    case CSSPropertyAnimationTimingFunction:
    case CSSPropertyTransitionTimingFunction:
        return consumeAnimationTimingFunction(range, context);
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
        if (value->isPrimitiveValue() && downcast<CSSPrimitiveValue>(value.get()).isValueID()
            && downcast<CSSPrimitiveValue>(value.get()).valueID() == CSSValueNone)
            return false;
    }
    return true;
}

static RefPtr<CSSValue> consumeAnimationPropertyList(CSSPropertyID property, CSSParserTokenRange& range, const CSSParserContext& context)
{
    RefPtr<CSSValueList> list;
    RefPtr<CSSValue> singleton;
    do {
        RefPtr<CSSValue> currentValue = consumeAnimationValue(property, range, context);
        if (!currentValue)
            return nullptr;
        
        if (singleton && !list) {
            list = CSSValueList::createCommaSeparated();
            list->append(singleton.releaseNonNull());
        }
        
        if (list)
            list->append(currentValue.releaseNonNull());
        else
            singleton = WTFMove(currentValue);
        
    } while (consumeCommaIncludingWhitespace(range));

    if (list) {
        if (!isValidAnimationPropertyList(property, *list))
            return nullptr;
    
        ASSERT(list->length());
        return list;
    }
    
    return singleton;
}

bool CSSPropertyParser::consumeAnimationShorthand(const StylePropertyShorthand& shorthand, bool important)
{
    const unsigned longhandCount = shorthand.length();
    RefPtr<CSSValueList> longhands[8];
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

                if (auto value = consumeAnimationValue(shorthand.properties()[i], m_range, m_context)) {
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
                ComputedStyleExtractor::addValueForAnimationPropertyToList(*longhands[i], shorthand.properties()[i], nullptr);
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

static RefPtr<CSSValue> consumeZIndex(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeInteger(range);
}

static RefPtr<CSSValue> consumeShadow(CSSParserTokenRange& range, const CSSParserContext& context, bool isBoxShadowProperty)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    RefPtr<CSSValueList> shadowValueList = CSSValueList::createCommaSeparated();
    do {
        if (auto shadowValue = consumeSingleShadow(range, context, isBoxShadowProperty, isBoxShadowProperty))
            shadowValueList->append(shadowValue.releaseNonNull());
        else
            return nullptr;
    } while (consumeCommaIncludingWhitespace(range));
    return shadowValueList;
}

static RefPtr<CSSValue> consumeTextDecorationLine(CSSParserTokenRange& range)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueNone)
        return consumeIdent(range);

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    while (true) {
        RefPtr<CSSPrimitiveValue> ident = consumeIdent<CSSValueBlink, CSSValueUnderline, CSSValueOverline, CSSValueLineThrough>(range);
        if (!ident)
            break;
        if (list->hasValue(ident.get()))
            return nullptr;
        list->append(ident.releaseNonNull());
    }

    if (!list->length())
        return nullptr;
    return list;
}

bool CSSPropertyParser::consumeTextDecorationSkip(bool important)
{
    if (RefPtr<CSSPrimitiveValue> skip = consumeIdent<CSSValueNone, CSSValueAuto, CSSValueInk>(m_range)) {
        switch (skip->valueID()) {
        case CSSValueNone:
            addProperty(CSSPropertyTextDecorationSkipInk, CSSPropertyTextDecorationSkip, skip.releaseNonNull(), important);
            return m_range.atEnd();
        case CSSValueAuto:
            addProperty(CSSPropertyTextDecorationSkipInk, CSSPropertyTextDecorationSkip, skip.releaseNonNull(), important);
            return m_range.atEnd();
        case CSSValueInk:
            addProperty(CSSPropertyTextDecorationSkipInk, CSSPropertyTextDecorationSkip, CSSValuePool::singleton().createIdentifierValue(CSSValueAuto), important);
            return m_range.atEnd();
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    }
    return false;
}

static RefPtr<CSSValue> consumeTextEmphasisStyle(CSSParserTokenRange& range)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueNone)
        return consumeIdent(range);

    if (RefPtr<CSSValue> textEmphasisStyle = consumeString(range))
        return textEmphasisStyle;

    RefPtr<CSSPrimitiveValue> fill = consumeIdent<CSSValueFilled, CSSValueOpen>(range);
    RefPtr<CSSPrimitiveValue> shape = consumeIdent<CSSValueDot, CSSValueCircle, CSSValueDoubleCircle, CSSValueTriangle, CSSValueSesame>(range);
    if (!fill)
        fill = consumeIdent<CSSValueFilled, CSSValueOpen>(range);
    if (fill && shape) {
        RefPtr<CSSValueList> parsedValues = CSSValueList::createSpaceSeparated();
        parsedValues->append(fill.releaseNonNull());
        parsedValues->append(shape.releaseNonNull());
        return parsedValues;
    }
    if (fill)
        return fill;
    if (shape)
        return shape;
    return nullptr;
}

static RefPtr<CSSPrimitiveValue> consumeColorWithAuto(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeColor(range, context);
}

static RefPtr<CSSValue> consumeOutlineColor(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // Allow the special focus color even in HTML Standard parsing mode.
    if (range.peek().id() == CSSValueWebkitFocusRingColor)
        return consumeIdent(range);
    return consumeColor(range, context);
}

static RefPtr<CSSPrimitiveValue> consumeLineWidth(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueThin || id == CSSValueMedium || id == CSSValueThick)
        return consumeIdent(range);
    return consumeLength(range, cssParserMode, ValueRange::NonNegative, unitless);
}

static RefPtr<CSSPrimitiveValue> consumeBorderWidth(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless)
{
    return consumeLineWidth(range, cssParserMode, unitless);
}

static RefPtr<CSSPrimitiveValue> consumeTextStrokeWidth(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    return consumeLineWidth(range, cssParserMode, UnitlessQuirk::Forbid);
}

static RefPtr<CSSPrimitiveValue> consumeColumnRuleWidth(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    return consumeLineWidth(range, cssParserMode, UnitlessQuirk::Forbid);
}

static bool consumeTranslate3d(CSSParserTokenRange& args, CSSParserMode cssParserMode, RefPtr<CSSFunctionValue>& transformValue)
{
    unsigned numberOfArguments = 2;
    RefPtr<CSSValue> parsedValue;
    do {
        parsedValue = consumeLengthOrPercent(args, cssParserMode, ValueRange::All);
        if (!parsedValue)
            return false;
        transformValue->append(*parsedValue);
        if (!consumeCommaIncludingWhitespace(args))
            return false;
    } while (--numberOfArguments);
    parsedValue = consumeLength(args, cssParserMode, ValueRange::All);
    if (!parsedValue)
        return false;
    transformValue->append(*parsedValue);
    return true;
}

static bool consumeNumbers(CSSParserTokenRange& args, RefPtr<CSSFunctionValue>& transformValue, unsigned numberOfArguments)
{
    do {
        RefPtr<CSSPrimitiveValue> parsedValue = consumeNumber(args, ValueRange::All);
        if (!parsedValue)
            return false;
        transformValue->append(parsedValue.releaseNonNull());
        if (--numberOfArguments && !consumeCommaIncludingWhitespace(args))
            return false;
    } while (numberOfArguments);
    return true;
}

static bool consumeNumbersOrPercents(CSSParserTokenRange& args, RefPtr<CSSFunctionValue>& transformValue, unsigned numberOfArguments)
{
    auto parseNumberAndAppend = [&] {
        auto parsedValue = consumeNumberOrPercent(args, ValueRange::All);
        if (!parsedValue)
            return false;

        transformValue->append(parsedValue.releaseNonNull());
        --numberOfArguments;
        return true;
    };

    if (!parseNumberAndAppend())
        return false;

    while (numberOfArguments) {
        if (!consumeCommaIncludingWhitespace(args))
            return false;

        if (!parseNumberAndAppend())
            return false;
    }

    return true;
}

static bool consumePerspective(CSSParserTokenRange& args, CSSParserMode cssParserMode, RefPtr<CSSFunctionValue>& transformValue)
{
    if (args.peek().id() == CSSValueNone) {
        transformValue->append(consumeIdent(args).releaseNonNull());
        return true;
    }

    if (auto parsedValue = consumeLength(args, cssParserMode, ValueRange::NonNegative)) {
        transformValue->append(parsedValue.releaseNonNull());
        return true;
    }

    if (auto perspective = consumeNumberRaw(args, ValueRange::NonNegative)) {
        transformValue->append(CSSPrimitiveValue::create(perspective->value, CSSUnitType::CSS_PX));
        return true;
    }

    return false;
}

static RefPtr<CSSValue> consumeTransformValue(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    CSSValueID functionId = range.peek().functionId();
    if (functionId == CSSValueInvalid)
        return nullptr;
    CSSParserTokenRange args = consumeFunction(range);
    if (args.atEnd())
        return nullptr;
    
    RefPtr<CSSFunctionValue> transformValue = CSSFunctionValue::create(functionId);
    RefPtr<CSSValue> parsedValue;
    switch (functionId) {
    case CSSValueRotate:
    case CSSValueRotateX:
    case CSSValueRotateY:
    case CSSValueRotateZ:
    case CSSValueSkewX:
    case CSSValueSkewY:
    case CSSValueSkew:
        parsedValue = consumeAngle(args, cssParserMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow);
        if (!parsedValue)
            return nullptr;
        if (functionId == CSSValueSkew && consumeCommaIncludingWhitespace(args)) {
            transformValue->append(*parsedValue);
            parsedValue = consumeAngle(args, cssParserMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow);
            if (!parsedValue)
                return nullptr;
        }
        break;
    case CSSValueScaleX:
    case CSSValueScaleY:
    case CSSValueScaleZ:
    case CSSValueScale:
        parsedValue = consumeNumberOrPercent(args, ValueRange::All);
        if (!parsedValue)
            return nullptr;
        if (functionId == CSSValueScale && consumeCommaIncludingWhitespace(args)) {
            transformValue->append(*parsedValue);
            parsedValue = consumeNumberOrPercent(args, ValueRange::All);
            if (!parsedValue)
                return nullptr;
        }
        break;
    case CSSValuePerspective:
        if (!consumePerspective(args, cssParserMode, transformValue))
            return nullptr;
        break;
    case CSSValueTranslateX:
    case CSSValueTranslateY:
    case CSSValueTranslate:
        parsedValue = consumeLengthOrPercent(args, cssParserMode, ValueRange::All);
        if (!parsedValue)
            return nullptr;
        if (functionId == CSSValueTranslate && consumeCommaIncludingWhitespace(args)) {
            transformValue->append(*parsedValue);
            parsedValue = consumeLengthOrPercent(args, cssParserMode, ValueRange::All);
            if (!parsedValue)
                return nullptr;
            if (is<CSSPrimitiveValue>(parsedValue)) {
                auto isZero = downcast<CSSPrimitiveValue>(*parsedValue).isZero();
                if (isZero && *isZero)
                    parsedValue = nullptr;
            }
        }
        break;
    case CSSValueTranslateZ:
        parsedValue = consumeLength(args, cssParserMode, ValueRange::All);
        break;
    case CSSValueMatrix:
    case CSSValueMatrix3d:
        if (!consumeNumbers(args, transformValue, (functionId == CSSValueMatrix3d) ? 16 : 6))
            return nullptr;
        break;
    case CSSValueScale3d:
        if (!consumeNumbersOrPercents(args, transformValue, 3))
            return nullptr;
        break;
    case CSSValueRotate3d:
        if (!consumeNumbers(args, transformValue, 3) || !consumeCommaIncludingWhitespace(args))
            return nullptr;
        parsedValue = consumeAngle(args, cssParserMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow);
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

static RefPtr<CSSValue> consumeTransform(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    do {
        RefPtr<CSSValue> parsedTransformValue = consumeTransformValue(range, cssParserMode);
        if (!parsedTransformValue)
            return nullptr;
        list->append(parsedTransformValue.releaseNonNull());
    } while (!range.atEnd());

    return list;
}

static RefPtr<CSSValue> consumeTranslate(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();

    // https://drafts.csswg.org/css-transforms-2/#propdef-translate
    //
    // The translate property accepts 1-3 values, each specifying a translation against one axis, in the order X, Y, then Z.
    // If only one or two values are given, this specifies a 2d translation, equivalent to the translate() function. If the second
    // value is missing, it defaults to 0px. If three values are given, this specifies a 3d translation, equivalent to the
    // translate3d() function.

    RefPtr<CSSValue> x = consumeLengthOrPercent(range, cssParserMode, ValueRange::All);
    if (!x)
        return list;

    // If we got this far we have a valid x value, so we can directly add it to the list.
    list->append(*x);

    range.consumeWhitespace();
    RefPtr<CSSValue> y = consumeLengthOrPercent(range, cssParserMode, ValueRange::All);
    if (!y)
        return list;

    // If we have a calc() or non-zero y value, we can directly add it to the list. We only
    // want to add a zero y value if a non-zero z value is specified.
    // Always include 0% in serialization per-spec.
    if (is<CSSPrimitiveValue>(y)) {
        auto& yPrimitiveValue = downcast<CSSPrimitiveValue>(*y);
        if (yPrimitiveValue.isCalculated() || yPrimitiveValue.isPercentage() || !*yPrimitiveValue.isZero())
            list->append(*y);
    }

    range.consumeWhitespace();
    RefPtr<CSSValue> z = consumeLength(range, cssParserMode, ValueRange::All);

    if (is<CSSPrimitiveValue>(z)) {
        auto& zPrimitiveValue = downcast<CSSPrimitiveValue>(*z);
        // If the z value is a zero value and not a percent value, we have nothing left to add to the list.
        if (!zPrimitiveValue.isCalculated() && !zPrimitiveValue.isPercentage() && *zPrimitiveValue.isZero())
            return list;
        // Add the zero value for y if we did not already add a y value.
        if (list->length() == 1)
            list->append(*y);
        list->append(*z);
    }

    return list;
}

static RefPtr<CSSValue> consumeScale(CSSParserTokenRange& range, CSSParserMode)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    // https://www.w3.org/TR/css-transforms-2/#propdef-scale
    //
    // The scale property accepts 1-3 values, each specifying a scale along one axis, in order X, Y, then Z.
    //
    // If only the X value is given, the Y value defaults to the same value.
    //
    // If one or two values are given, this specifies a 2d scaling, equivalent to the scale() function.
    // If three values are given, this specifies a 3d scaling, equivalent to the scale3d() function.

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();

    RefPtr<CSSValue> x = consumeNumberOrPercent(range, ValueRange::All);
    if (!x)
        return list;
    list->append(*x);
    range.consumeWhitespace();

    RefPtr<CSSValue> y = consumeNumberOrPercent(range, ValueRange::All);
    if (!y)
        return list;

    // If the x and y values are the same, the y value is not needed.
    if (downcast<CSSPrimitiveValue>(*x).doubleValue() != downcast<CSSPrimitiveValue>(*y).doubleValue())
        list->append(*y);
    range.consumeWhitespace();

    RefPtr<CSSValue> z = consumeNumberOrPercent(range, ValueRange::All);
    if (!z)
        return list;
    if (downcast<CSSPrimitiveValue>(*z).doubleValue() != 1.0) {
        // We only need to append the z value if it's set to something other than 1.
        // In case y was not added yet, because it was equal to x, we must append it
        // prior to appending z.
        if (list->length() == 1)
            list->append(*y);
        list->append(*z);
    }

    return list;
}

static RefPtr<CSSValue> consumeRotate(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();

    // https://www.w3.org/TR/css-transforms-2/#propdef-rotate
    //
    // The rotate property accepts an angle to rotate an element, and optionally an axis to rotate it around.
    //
    // If the axis is omitted, this specifies a 2d rotation, equivalent to the rotate() function.
    //
    // Otherwise, it specifies a 3d rotation: if x, y, or z is given, it specifies a rotation around that axis,
    // equivalent to the rotateX()/etc 3d transform functions. Alternately, the axis can be specified explicitly
    // by giving three numbers representing the x, y, and z components of an origin-centered vector, equivalent
    // to the rotate3d() function.

    RefPtr<CSSValue> angle;
    RefPtr<CSSValue> axisIdentifier;

    while (!range.atEnd()) {
        // First, attempt to parse a number, which might be in a series of 3 specifying the rotation axis.
        RefPtr<CSSValue> parsedValue = consumeNumber(range, ValueRange::All);
        if (parsedValue) {
            // If we've encountered an axis identifier, then this valus is invalid.
            if (axisIdentifier)
                return nullptr;
            list->append(*parsedValue);
            range.consumeWhitespace();
            continue;
        }

        // Then, attempt to parse an angle. We try this as a fallback rather than the first option because
        // a unitless 0 angle would be consumed as an angle.
        parsedValue = consumeAngle(range, cssParserMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow);
        if (parsedValue) {
            // If we had already parsed an angle or numbers but not 3 in a row, this value is invalid.
            if (angle || (list->length() && list->length() != 3))
                return nullptr;
            angle = parsedValue;
            range.consumeWhitespace();
            continue;
        }

        // Finally, attempt to parse one of the axis identifiers.
        parsedValue = consumeIdent<CSSValueX, CSSValueY, CSSValueZ>(range);
        // If we failed to find one of those identifiers or one was already specified, or we'd previously
        // encountered numbers to specify a rotation axis, then this value is invalid.
        if (!parsedValue || axisIdentifier || list->length())
            return nullptr;
        axisIdentifier = parsedValue;
        range.consumeWhitespace();
    }

    // We must have an angle to have a valid value.
    if (!angle)
        return nullptr;

    if (list->length() == 3) {
        // The first valid case is if we have 3 items in the list, meaning we parsed three consecutive number values
        // to specify the rotation axis. In that case, we must not also have encountered an axis identifier.
        ASSERT(!axisIdentifier);

        // No we must check the values since if we have a vector in the x, y or z axis alone we must serialize to the
        // matching identifier.
        auto x = downcast<CSSPrimitiveValue>(*list->itemWithoutBoundsCheck(0)).doubleValue();
        auto y = downcast<CSSPrimitiveValue>(*list->itemWithoutBoundsCheck(1)).doubleValue();
        auto z = downcast<CSSPrimitiveValue>(*list->itemWithoutBoundsCheck(2)).doubleValue();

        if (x && !y && !z) {
            list = CSSValueList::createSpaceSeparated();
            list->append(CSSPrimitiveValue::createIdentifier(CSSValueX));
        } else if (!x && y && !z) {
            list = CSSValueList::createSpaceSeparated();
            list->append(CSSPrimitiveValue::createIdentifier(CSSValueY));
        } else if (!x && !y && z)
            list = CSSValueList::createSpaceSeparated();

        // Finally, we must append the angle.
        list->append(*angle);
    } else if (!list->length()) {
        // The second valid case is if we have no item in the list, meaning we have either an optional rotation axis
        // using an identifier. In that case, we must add the axis identifier is specified and then add the angle.
        if (is<CSSPrimitiveValue>(axisIdentifier) && downcast<CSSPrimitiveValue>(*axisIdentifier).valueID() != CSSValueZ)
            list->append(*axisIdentifier);
        list->append(*angle);
    } else
        return nullptr;

    return list;
}

static RefPtr<CSSPrimitiveValue> consumePositionX(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    return consumeSingleAxisPosition(range, cssParserMode, BoxOrient::Horizontal);
}

static RefPtr<CSSPrimitiveValue> consumePositionY(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    return consumeSingleAxisPosition(range, cssParserMode, BoxOrient::Vertical);
}

static RefPtr<CSSValue> consumePaintStroke(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    RefPtr<CSSPrimitiveValue> url = consumeUrl(range);
    if (url) {
        RefPtr<CSSValue> parsedValue;
        if (range.peek().id() == CSSValueNone)
            parsedValue = consumeIdent(range);
        else
            parsedValue = consumeColor(range, context);
        if (parsedValue) {
            RefPtr<CSSValueList> values = CSSValueList::createSpaceSeparated();
            values->append(url.releaseNonNull());
            values->append(parsedValue.releaseNonNull());
            return values;
        }
        return url;
    }
    return consumeColor(range, context);
}

static RefPtr<CSSValue> consumeGlyphOrientation(CSSParserTokenRange& range, CSSParserMode mode, CSSPropertyID property)
{
    if (range.peek().id() == CSSValueAuto) {
        if (property == CSSPropertyGlyphOrientationVertical)
            return consumeIdent(range);
        return nullptr;
    }
    
    return consumeAngle(range, mode, UnitlessQuirk::Allow, UnitlessZeroQuirk::Allow);
}

static RefPtr<CSSValue> consumePaintOrder(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);

    Vector<CSSValueID, 3> paintTypeList;
    RefPtr<CSSPrimitiveValue> fill;
    RefPtr<CSSPrimitiveValue> stroke;
    RefPtr<CSSPrimitiveValue> markers;
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
    RefPtr<CSSValueList> paintOrderList = CSSValueList::createSpaceSeparated();
    switch (firstPaintOrderType) {
    case CSSValueFill:
    case CSSValueStroke:
        paintOrderList->append(firstPaintOrderType == CSSValueFill ? fill.releaseNonNull() : stroke.releaseNonNull());
        if (paintTypeList.size() > 1) {
            if (paintTypeList.at(1) == CSSValueMarkers)
                paintOrderList->append(markers.releaseNonNull());
        }
        break;
    case CSSValueMarkers:
        paintOrderList->append(markers.releaseNonNull());
        if (paintTypeList.size() > 1) {
            if (paintTypeList.at(1) == CSSValueStroke)
                paintOrderList->append(stroke.releaseNonNull());
        }
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    return paintOrderList;
}

static RefPtr<CSSValue> consumeNoneOrURI(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    return consumeUrl(range);
}

static bool isFlexBasisIdent(const WebCore::CSSValueID id, const CSSParserContext& context)
{
    return identMatches<CSSValueAuto, CSSValueContent>(id) || validWidthOrHeightKeyword(id, context);
}

static RefPtr<CSSValue> consumeFlexBasis(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (isFlexBasisIdent(range.peek().id(), context))
        return consumeIdent(range);
    return consumeLengthOrPercent(range, context.mode, ValueRange::NonNegative);
}

static RefPtr<CSSValue> consumeKerning(CSSParserTokenRange& range, CSSParserMode mode)
{
    RefPtr<CSSValue> result = consumeIdent<CSSValueAuto, CSSValueNormal>(range);
    if (result)
        return result;
    return consumeLength(range, mode, ValueRange::All, UnitlessQuirk::Allow);
}

static RefPtr<CSSValue> consumeStrokeDasharray(CSSParserTokenRange& range)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueNone)
        return consumeIdent(range);

    RefPtr<CSSValueList> dashes = CSSValueList::createCommaSeparated();
    do {
        RefPtr<CSSPrimitiveValue> dash = consumeLengthOrPercent(range, SVGAttributeMode, ValueRange::NonNegative);
        if (!dash || (consumeCommaIncludingWhitespace(range) && range.atEnd()))
            return nullptr;
        dashes->append(dash.releaseNonNull());
    } while (!range.atEnd());
    return dashes;
}

static RefPtr<CSSPrimitiveValue> consumeBaselineShift(CSSParserTokenRange& range)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueBaseline || id == CSSValueSub || id == CSSValueSuper)
        return consumeIdent(range);
    return consumeLengthOrPercent(range, SVGAttributeMode, ValueRange::All);
}

static RefPtr<CSSPrimitiveValue> consumeRxOrRy(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeLengthOrPercent(range, cssParserMode, ValueRange::NonNegative, UnitlessQuirk::Forbid);
}

static RefPtr<CSSValue> consumeCursor(CSSParserTokenRange& range, const CSSParserContext& context, bool inQuirksMode)
{
    RefPtr<CSSValueList> list;
    while (auto image = consumeImage(range, context, { AllowedImageType::URLFunction, AllowedImageType::ImageSet })) {
        std::optional<IntPoint> hotSpot;
        if (auto x = consumeNumberRaw(range)) {
            auto y = consumeNumberRaw(range);
            if (!y)
                return nullptr;
            // FIXME: Should we clamp or round instead of just casting from double to int?
            hotSpot = IntPoint { static_cast<int>(x->value), static_cast<int>(y->value) };
        }

        if (!list)
            list = CSSValueList::createCommaSeparated();

        list->append(CSSCursorImageValue::create(image.releaseNonNull(), hotSpot, context.isContentOpaque ? LoadedFromOpaqueSource::Yes : LoadedFromOpaqueSource::No));
        if (!consumeCommaIncludingWhitespace(range))
            return nullptr;
    }

    CSSValueID id = range.peek().id();
    RefPtr<CSSValue> cursorType;
    if (id == CSSValueHand) {
        if (!inQuirksMode) // Non-standard behavior
            return nullptr;
        cursorType = CSSValuePool::singleton().createIdentifierValue(CSSValuePointer);
        range.consumeIncludingWhitespace();
    } else if ((id >= CSSValueAuto && id <= CSSValueWebkitZoomOut) || id == CSSValueCopy || id == CSSValueNone) {
        cursorType = consumeIdent(range);
    } else {
        return nullptr;
    }

    if (!list)
        return cursorType;
    list->append(cursorType.releaseNonNull());
    return list;
}

static RefPtr<CSSValue> consumeAttr(CSSParserTokenRange args, CSSParserContext context)
{
    if (args.peek().type() != IdentToken)
        return nullptr;
    
    CSSParserToken token = args.consumeIncludingWhitespace();
    AtomString attrName;
    if (context.isHTMLDocument)
        attrName = token.value().convertToASCIILowercaseAtom();
    else
        attrName = token.value().toAtomString();

    if (!args.atEnd())
        return nullptr;

    // FIXME: Consider moving to a CSSFunctionValue with a custom-ident rather than a special CSS_ATTR primitive value.
    return CSSValuePool::singleton().createValue(attrName, CSSUnitType::CSS_ATTR);
}

static RefPtr<CSSValue> consumeCounterContent(CSSParserTokenRange args, bool counters)
{
    RefPtr<CSSPrimitiveValue> identifier = consumeCustomIdent(args);
    if (!identifier)
        return nullptr;

    RefPtr<CSSPrimitiveValue> separator;
    if (!counters)
        separator = CSSPrimitiveValue::create(String(), CSSUnitType::CSS_STRING);
    else {
        if (!consumeCommaIncludingWhitespace(args) || args.peek().type() != StringToken)
            return nullptr;
        separator = CSSPrimitiveValue::create(args.consumeIncludingWhitespace().value().toString(), CSSUnitType::CSS_STRING);
    }

    RefPtr<CSSPrimitiveValue> listStyle;
    if (consumeCommaIncludingWhitespace(args)) {
        CSSValueID id = args.peek().id();
        if ((id != CSSValueNone && !isPredefinedCounterStyle(id)))
            return nullptr;
        listStyle = consumeIdent(args);
    } else
        listStyle = CSSValuePool::singleton().createIdentifierValue(CSSValueDecimal);

    if (!args.atEnd())
        return nullptr;
    
    // FIXME-NEWPARSER: Should just have a CSSCounterValue.
    return CSSValuePool::singleton().createValue(Counter::create(identifier.releaseNonNull(), listStyle.releaseNonNull(), separator.releaseNonNull()));
}

static RefPtr<CSSValue> consumeContent(CSSParserTokenRange& range, CSSParserContext context)
{
    if (identMatches<CSSValueNone, CSSValueNormal>(range.peek().id()))
        return consumeIdent(range);

    RefPtr<CSSValueList> values = CSSValueList::createSpaceSeparated();

    do {
        RefPtr<CSSValue> parsedValue = consumeImage(range, context);
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
        values->append(parsedValue.releaseNonNull());
    } while (!range.atEnd());

    return values;
}

static RefPtr<CSSPrimitiveValue> consumePerspective(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    if (auto parsedValue = consumeLength(range, cssParserMode, ValueRange::All)) {
        if (!parsedValue->isNegative().value_or(false))
            return parsedValue;
    }

    return nullptr;
}

bool CSSPropertyParser::consumePrefixedPerspective(bool important)
{
    if (auto value = consumePerspective(m_range, m_context.mode)) {
        addProperty(CSSPropertyPerspective, CSSPropertyWebkitPerspective, value.releaseNonNull(), important);
        return m_range.atEnd();
    }

    if (auto perspective = consumeNumberRaw(m_range)) {
        if (perspective->value < 0)
            return false;
        auto value = CSSPrimitiveValue::create(perspective->value, CSSUnitType::CSS_PX);
        addProperty(CSSPropertyPerspective, CSSPropertyWebkitPerspective, WTFMove(value), important);
        return m_range.atEnd();
    }

    return false;
}

static RefPtr<CSSValueList> consumeScrollSnapAlign(CSSParserTokenRange& range)
{
    auto firstValue = consumeIdent<CSSValueNone, CSSValueStart, CSSValueCenter, CSSValueEnd>(range);
    if (!firstValue)
        return nullptr;

    auto secondValue = consumeIdent<CSSValueNone, CSSValueStart, CSSValueCenter, CSSValueEnd>(range);
    bool shouldAddSecondValue = secondValue && !secondValue->equals(*firstValue);

    RefPtr<CSSValueList> alignmentValue = CSSValueList::createSpaceSeparated();
    alignmentValue->append(firstValue.releaseNonNull());

    // Only add the second value if it differs from the first so that we produce the canonical
    // serialization of this CSSValueList.
    if (shouldAddSecondValue)
        alignmentValue->append(secondValue.releaseNonNull());

    return alignmentValue;
}

static RefPtr<CSSValueList> consumeScrollSnapType(CSSParserTokenRange& range)
{
    RefPtr<CSSValueList> typeValue = CSSValueList::createSpaceSeparated();

    auto firstValue = consumeIdent<CSSValueNone, CSSValueX, CSSValueY, CSSValueBlock, CSSValueInline, CSSValueBoth>(range);
    if (!firstValue)
        return nullptr;
    typeValue->append(firstValue.releaseNonNull());

    // We only add the second value if it is not the initial value as described in specification
    // so that serialization of this CSSValueList produces the canonical serialization.
    auto secondValue = consumeIdent<CSSValueProximity, CSSValueMandatory>(range);
    if (secondValue && secondValue->valueID() != CSSValueProximity)
        typeValue->append(secondValue.releaseNonNull());

    return typeValue;
}

static RefPtr<CSSPrimitiveValue> consumeScrollBehavior(CSSParserTokenRange& range)
{
    auto valueID = range.peek().id();
    if (valueID != CSSValueAuto && valueID != CSSValueSmooth)
        return nullptr;
    return consumeIdent(range);
}

static RefPtr<CSSValue> consumeOverscrollBehavior(CSSParserTokenRange& range)
{
    auto valueID = range.peek().id();
    if (valueID != CSSValueAuto && valueID != CSSValueNone && valueID != CSSValueContain)
        return nullptr;
    return consumeIdent(range);
}

static RefPtr<CSSValue> consumeOverflowAnchor(CSSParserTokenRange& range)
{
    auto valueID = range.peek().id();
    if (valueID != CSSValueAuto && valueID != CSSValueNone)
        return nullptr;
    return consumeIdent(range);
}

static RefPtr<CSSValue> consumeBorderRadiusCorner(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    RefPtr<CSSPrimitiveValue> parsedValue1 = consumeLengthOrPercent(range, cssParserMode, ValueRange::NonNegative);
    if (!parsedValue1)
        return nullptr;
    RefPtr<CSSPrimitiveValue> parsedValue2 = consumeLengthOrPercent(range, cssParserMode, ValueRange::NonNegative);
    if (!parsedValue2)
        parsedValue2 = parsedValue1;
    return createPrimitiveValuePair(parsedValue1.releaseNonNull(), parsedValue2.releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce);
}

static RefPtr<CSSValue> consumeTextUnderlineOffset(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (auto value = consumeIdent<CSSValueAuto>(range))
        return value;
    return consumeLength(range, cssParserMode, ValueRange::All);
}

static RefPtr<CSSValue> consumeTextDecorationThickness(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (auto value = consumeIdent<CSSValueAuto, CSSValueFromFont>(range))
        return value;
    return consumeLength(range, cssParserMode, ValueRange::All);
}

static RefPtr<CSSPrimitiveValue> consumeVerticalAlign(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    RefPtr<CSSPrimitiveValue> parsedValue = consumeIdentRange(range, CSSValueBaseline, CSSValueWebkitBaselineMiddle);
    if (!parsedValue)
        parsedValue = consumeLengthOrPercent(range, cssParserMode, ValueRange::All, UnitlessQuirk::Allow);
    return parsedValue;
}

static RefPtr<CSSPrimitiveValue> consumeShapeRadius(CSSParserTokenRange& args, CSSParserMode cssParserMode)
{
    if (identMatches<CSSValueClosestSide, CSSValueFarthestSide>(args.peek().id()))
        return consumeIdent(args);
    return consumeLengthOrPercent(args, cssParserMode, ValueRange::NonNegative);
}

static RefPtr<CSSBasicShapeCircle> consumeBasicShapeCircle(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // spec: https://drafts.csswg.org/css-shapes/#supported-basic-shapes
    // circle( [<shape-radius>]? [at <position>]? )
    RefPtr<CSSBasicShapeCircle> shape = CSSBasicShapeCircle::create();
    if (RefPtr<CSSPrimitiveValue> radius = consumeShapeRadius(args, context.mode))
        shape->setRadius(radius.releaseNonNull());
    if (consumeIdent<CSSValueAt>(args)) {
        auto centerCoordinates = consumePositionCoordinates(args, context.mode, UnitlessQuirk::Forbid, PositionSyntax::Position);
        if (!centerCoordinates)
            return nullptr;
        shape->setCenterX(WTFMove(centerCoordinates->x));
        shape->setCenterY(WTFMove(centerCoordinates->y));
    }
    return shape;
}

static RefPtr<CSSBasicShapeEllipse> consumeBasicShapeEllipse(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // spec: https://drafts.csswg.org/css-shapes/#supported-basic-shapes
    // ellipse( [<shape-radius>{2}]? [at <position>]? )
    auto shape = CSSBasicShapeEllipse::create();
    if (auto radiusX = consumeShapeRadius(args, context.mode)) {
        auto radiusY = consumeShapeRadius(args, context.mode);
        if (!radiusY)
            return nullptr;
        shape->setRadiusX(radiusX.releaseNonNull());
        shape->setRadiusY(radiusY.releaseNonNull());
    }
    if (consumeIdent<CSSValueAt>(args)) {
        auto centerCoordinates = consumePositionCoordinates(args, context.mode, UnitlessQuirk::Forbid, PositionSyntax::Position);
        if (!centerCoordinates)
            return nullptr;
        shape->setCenterX(WTFMove(centerCoordinates->x));
        shape->setCenterY(WTFMove(centerCoordinates->y));
    }
    return shape;
}

static RefPtr<CSSBasicShapePolygon> consumeBasicShapePolygon(CSSParserTokenRange& args, const CSSParserContext& context)
{
    RefPtr<CSSBasicShapePolygon> shape = CSSBasicShapePolygon::create();
    if (identMatches<CSSValueEvenodd, CSSValueNonzero>(args.peek().id())) {
        shape->setWindRule(args.consumeIncludingWhitespace().id() == CSSValueEvenodd ? WindRule::EvenOdd : WindRule::NonZero);
        if (!consumeCommaIncludingWhitespace(args))
            return nullptr;
    }

    do {
        RefPtr<CSSPrimitiveValue> xLength = consumeLengthOrPercent(args, context.mode, ValueRange::All);
        if (!xLength)
            return nullptr;
        RefPtr<CSSPrimitiveValue> yLength = consumeLengthOrPercent(args, context.mode, ValueRange::All);
        if (!yLength)
            return nullptr;
        shape->appendPoint(xLength.releaseNonNull(), yLength.releaseNonNull());
    } while (consumeCommaIncludingWhitespace(args));
    return shape;
}

static RefPtr<CSSBasicShapePath> consumeBasicShapePath(CSSParserTokenRange& args)
{
    WindRule windRule = WindRule::NonZero;
    if (identMatches<CSSValueEvenodd, CSSValueNonzero>(args.peek().id())) {
        windRule = args.consumeIncludingWhitespace().id() == CSSValueEvenodd ? WindRule::EvenOdd : WindRule::NonZero;
        if (!consumeCommaIncludingWhitespace(args))
            return nullptr;
    }

    if (args.peek().type() != StringToken)
        return nullptr;
    
    auto byteStream = makeUnique<SVGPathByteStream>();
    if (!buildSVGPathByteStreamFromString(args.consumeIncludingWhitespace().value().toString(), *byteStream, UnalteredParsing))
        return nullptr;
    
    auto shape = CSSBasicShapePath::create(WTFMove(byteStream));
    shape->setWindRule(windRule);
    
    return shape;
}

static void complete4Sides(RefPtr<CSSPrimitiveValue> side[4])
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

static bool consumeRadii(RefPtr<CSSPrimitiveValue> horizontalRadii[4], RefPtr<CSSPrimitiveValue> verticalRadii[4], CSSParserTokenRange& range, CSSParserMode cssParserMode, bool useLegacyParsing)
{
    unsigned i = 0;
    for (; i < 4 && !range.atEnd() && range.peek().type() != DelimiterToken; ++i) {
        horizontalRadii[i] = consumeLengthOrPercent(range, cssParserMode, ValueRange::NonNegative);
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
            verticalRadii[i] = consumeLengthOrPercent(range, cssParserMode, ValueRange::NonNegative);
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

static RefPtr<CSSBasicShapeInset> consumeBasicShapeInset(CSSParserTokenRange& args, const CSSParserContext& context)
{
    RefPtr<CSSBasicShapeInset> shape = CSSBasicShapeInset::create();
    RefPtr<CSSPrimitiveValue> top = consumeLengthOrPercent(args, context.mode, ValueRange::All);
    if (!top)
        return nullptr;
    RefPtr<CSSPrimitiveValue> right = consumeLengthOrPercent(args, context.mode, ValueRange::All);
    RefPtr<CSSPrimitiveValue> bottom;
    RefPtr<CSSPrimitiveValue> left;
    if (right) {
        bottom = consumeLengthOrPercent(args, context.mode, ValueRange::All);
        if (bottom)
            left = consumeLengthOrPercent(args, context.mode, ValueRange::All);
    }
    if (left)
        shape->updateShapeSize4Values(top.releaseNonNull(), right.releaseNonNull(), bottom.releaseNonNull(), left.releaseNonNull());
    else if (bottom)
        shape->updateShapeSize3Values(top.releaseNonNull(), right.releaseNonNull(), bottom.releaseNonNull());
    else if (right)
        shape->updateShapeSize2Values(top.releaseNonNull(), right.releaseNonNull());
    else
        shape->updateShapeSize1Value(top.releaseNonNull());

    if (consumeIdent<CSSValueRound>(args)) {
        RefPtr<CSSPrimitiveValue> horizontalRadii[4] = { nullptr };
        RefPtr<CSSPrimitiveValue> verticalRadii[4] = { nullptr };
        if (!consumeRadii(horizontalRadii, verticalRadii, args, context.mode, false))
            return nullptr;
        shape->setTopLeftRadius(createPrimitiveValuePair(horizontalRadii[0].releaseNonNull(), verticalRadii[0].releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce));
        shape->setTopRightRadius(createPrimitiveValuePair(horizontalRadii[1].releaseNonNull(), verticalRadii[1].releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce));
        shape->setBottomRightRadius(createPrimitiveValuePair(horizontalRadii[2].releaseNonNull(), verticalRadii[2].releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce));
        shape->setBottomLeftRadius(createPrimitiveValuePair(horizontalRadii[3].releaseNonNull(), verticalRadii[3].releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce));
    }
    return shape;
}

static RefPtr<CSSPrimitiveValue> consumeBasicShape(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().type() != FunctionToken)
        return nullptr;
    CSSValueID id = range.peek().functionId();
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);
    
    // FIXME-NEWPARSER: CSSBasicShape should be a CSSValue, and shapes should not be primitive values.
    RefPtr<CSSBasicShape> shape;
    if (id == CSSValueCircle)
        shape = consumeBasicShapeCircle(args, context);
    else if (id == CSSValueEllipse)
        shape = consumeBasicShapeEllipse(args, context);
    else if (id == CSSValuePolygon)
        shape = consumeBasicShapePolygon(args, context);
    else if (id == CSSValueInset)
        shape = consumeBasicShapeInset(args, context);
    else if (id == CSSValuePath)
        shape = consumeBasicShapePath(args);
    if (!shape)
        return nullptr;
    range = rangeCopy;
    
    if (!args.atEnd())
        return nullptr;

    return CSSValuePool::singleton().createValue(shape.releaseNonNull());
}

static RefPtr<CSSValue> consumeBasicShapeOrBox(CSSParserTokenRange& range, const CSSParserContext& context)
{
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    bool shapeFound = false;
    bool boxFound = false;
    while (!range.atEnd() && !(shapeFound && boxFound)) {
        RefPtr<CSSValue> componentValue;
        if (range.peek().type() == FunctionToken && !shapeFound) {
            componentValue = consumeBasicShape(range, context);
            shapeFound = true;
        } else if (range.peek().type() == IdentToken && !boxFound) {
            componentValue = consumeIdent<CSSValueContentBox, CSSValuePaddingBox, CSSValueBorderBox, CSSValueMarginBox, CSSValueFillBox, CSSValueStrokeBox, CSSValueViewBox>(range);
            boxFound = true;
        }
        if (!componentValue)
            break;
        list->append(componentValue.releaseNonNull());
    }
    
    if (!list->length())
        return nullptr;
    
    return list;
}

// Parses the ray() definition as defined in https://drafts.fxtf.org/motion-1/#funcdef-offset-path-ray
// ray( [ <angle> && <size> && contain? ] )
static RefPtr<CSSRayValue> consumeRayShape(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().type() != FunctionToken || range.peek().functionId() != CSSValueRay)
        return nullptr;

    CSSParserTokenRange args = consumeFunction(range);

    RefPtr<CSSPrimitiveValue> angle;
    RefPtr<CSSPrimitiveValue> size;
    bool isContaining = false;
    while (!args.atEnd()) {
        if (!angle && (angle = consumeAngle(args, context.mode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid)))
            continue;

        if (!size && (size = consumeIdent<CSSValueClosestSide, CSSValueClosestCorner, CSSValueFarthestSide, CSSValueFarthestCorner, CSSValueSides>(args)))
            continue;

        if (!isContaining && (isContaining = consumeIdent<CSSValueContain>(args)))
            continue;

        return nullptr;
    }

    // <angle> and <size> must be present.
    if (!angle || !size)
        return nullptr;

    return CSSRayValue::create(angle.releaseNonNull(), size.releaseNonNull(), isContaining);
}

enum class ConsumeRay { Include, Exclude };

// Consumes shapes accepted by clip-path and offset-path.
static RefPtr<CSSValue> consumePathOperation(CSSParserTokenRange& range, const CSSParserContext& context, ConsumeRay consumeRay)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    if (RefPtr<CSSPrimitiveValue> url = consumeUrl(range))
        return url;

    if (consumeRay == ConsumeRay::Include) {
        if (auto ray = consumeRayShape(range, context))
            return ray;
    }

    return consumeBasicShapeOrBox(range, context);
}

static RefPtr<CSSValue> consumeShapeOutside(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (RefPtr<CSSValue> imageValue = consumeImageOrNone(range, context))
        return imageValue;
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    if (RefPtr<CSSValue> boxValue = consumeIdent<CSSValueContentBox, CSSValuePaddingBox, CSSValueBorderBox, CSSValueMarginBox>(range))
        list->append(boxValue.releaseNonNull());
    if (RefPtr<CSSPrimitiveValue> shapeValue = consumeBasicShape(range, context)) {
        if (shapeValue->shapeValue()->type() == CSSBasicShapeCircle::CSSBasicShapePathType)
            return nullptr;
        list->append(shapeValue.releaseNonNull());
        if (list->length() < 2) {
            if (RefPtr<CSSValue> boxValue = consumeIdent<CSSValueContentBox, CSSValuePaddingBox, CSSValueBorderBox, CSSValueMarginBox>(range))
                list->append(boxValue.releaseNonNull());
        }
    }
    if (!list->length())
        return nullptr;
    return list;
}

static bool isAuto(CSSValueID id)
{
    return identMatches<CSSValueAuto>(id);
}

static bool isNormalOrStretch(CSSValueID id)
{
    return identMatches<CSSValueNormal, CSSValueStretch>(id);
}

static bool isLeftOrRightKeyword(CSSValueID id)
{
    return identMatches<CSSValueLeft, CSSValueRight>(id);
}

static bool isContentDistributionKeyword(CSSValueID id)
{
    return identMatches<CSSValueSpaceBetween, CSSValueSpaceAround, CSSValueSpaceEvenly, CSSValueStretch>(id);
}

static bool isContentPositionKeyword(CSSValueID id)
{
    return identMatches<CSSValueStart, CSSValueEnd, CSSValueCenter, CSSValueFlexStart, CSSValueFlexEnd>(id);
}

static bool isContentPositionOrLeftOrRightKeyword(CSSValueID id)
{
    return isContentPositionKeyword(id) || isLeftOrRightKeyword(id);
}

static bool isOverflowKeyword(CSSValueID id)
{
    return CSSPropertyParserHelpers::identMatches<CSSValueUnsafe, CSSValueSafe>(id);
}

static bool isBaselineKeyword(CSSValueID id)
{
    return identMatches<CSSValueFirst, CSSValueLast, CSSValueBaseline>(id);
}

static RefPtr<CSSPrimitiveValue> consumeOverflowPositionKeyword(CSSParserTokenRange& range)
{
    return isOverflowKeyword(range.peek().id()) ? consumeIdent(range) : nullptr;
}

static CSSValueID getBaselineKeyword(RefPtr<CSSValue> value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(*value);
    if (primitiveValue.pairValue()) {
        ASSERT(primitiveValue.pairValue()->first()->valueID() == CSSValueLast);
        ASSERT(primitiveValue.pairValue()->second()->valueID() == CSSValueBaseline);
        return CSSValueLastBaseline;
    }
    ASSERT(primitiveValue.valueID() == CSSValueBaseline);
    return CSSValueBaseline;
}

static RefPtr<CSSValue> consumeBaselineKeyword(CSSParserTokenRange& range)
{
    RefPtr<CSSPrimitiveValue> preference = consumeIdent<CSSValueFirst, CSSValueLast>(range);
    RefPtr<CSSPrimitiveValue> baseline = consumeIdent<CSSValueBaseline>(range);
    if (!baseline)
        return nullptr;
    if (preference && preference->valueID() == CSSValueLast)
        return createPrimitiveValuePair(preference.releaseNonNull(), baseline.releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce);
    return baseline;
}

using IsPositionKeyword = bool (*)(CSSValueID);

static RefPtr<CSSValue> consumeContentDistributionOverflowPosition(CSSParserTokenRange& range, IsPositionKeyword isPositionKeyword)
{
    ASSERT(isPositionKeyword);
    CSSValueID id = range.peek().id();
    if (identMatches<CSSValueNormal>(id))
        return CSSContentDistributionValue::create(CSSValueInvalid, range.consumeIncludingWhitespace().id(), CSSValueInvalid);

    if (isBaselineKeyword(id)) {
        RefPtr<CSSValue> baseline = consumeBaselineKeyword(range);
        if (!baseline)
            return nullptr;
        return CSSContentDistributionValue::create(CSSValueInvalid, getBaselineKeyword(baseline), CSSValueInvalid);
    }

    if (isContentDistributionKeyword(id))
        return CSSContentDistributionValue::create(range.consumeIncludingWhitespace().id(), CSSValueInvalid, CSSValueInvalid);

    CSSValueID overflow = isOverflowKeyword(id) ? range.consumeIncludingWhitespace().id() : CSSValueInvalid;
    if (isPositionKeyword(range.peek().id()))
        return CSSContentDistributionValue::create(CSSValueInvalid, range.consumeIncludingWhitespace().id(), overflow);

    return nullptr;
}

static RefPtr<CSSPrimitiveValue> consumeBorderImageRepeatKeyword(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueStretch, CSSValueRepeat, CSSValueSpace, CSSValueRound>(range);
}

static RefPtr<CSSValue> consumeBorderImageRepeat(CSSParserTokenRange& range)
{
    RefPtr<CSSPrimitiveValue> horizontal = consumeBorderImageRepeatKeyword(range);
    if (!horizontal)
        return nullptr;
    RefPtr<CSSPrimitiveValue> vertical = consumeBorderImageRepeatKeyword(range);
    if (!vertical)
        vertical = horizontal;
    return createPrimitiveValuePair(horizontal.releaseNonNull(), vertical.releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce);
}

static RefPtr<CSSValue> consumeBorderImageSlice(CSSPropertyID property, CSSParserTokenRange& range)
{
    bool fill = consumeIdent<CSSValueFill>(range);
    RefPtr<CSSPrimitiveValue> slices[4] = { nullptr };

    for (size_t index = 0; index < 4; ++index) {
        RefPtr<CSSPrimitiveValue> value = consumePercent(range, ValueRange::NonNegative);
        if (!value)
            value = consumeNumber(range, ValueRange::NonNegative);
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
    
    // Now build a rect value to hold all four of our primitive values.
    // FIXME-NEWPARSER: Should just have a CSSQuadValue.
    auto quad = Quad::create();
    quad->setTop(slices[0].releaseNonNull());
    quad->setRight(slices[1].releaseNonNull());
    quad->setBottom(slices[2].releaseNonNull());
    quad->setLeft(slices[3].releaseNonNull());
    
    // Make our new border image value now.
    return CSSBorderImageSliceValue::create(CSSValuePool::singleton().createValue(WTFMove(quad)), fill);
}

static RefPtr<CSSValue> consumeBorderImageOutset(CSSParserTokenRange& range)
{
    RefPtr<CSSPrimitiveValue> outsets[4] = { nullptr };

    RefPtr<CSSPrimitiveValue> value;
    for (size_t index = 0; index < 4; ++index) {
        value = consumeNumber(range, ValueRange::NonNegative);
        if (!value)
            value = consumeLength(range, HTMLStandardMode, ValueRange::NonNegative);
        if (!value)
            break;
        outsets[index] = value;
    }
    if (!outsets[0])
        return nullptr;
    complete4Sides(outsets);
    
    // FIXME-NEWPARSER: Should just have a CSSQuadValue.
    auto quad = Quad::create();
    quad->setTop(outsets[0].releaseNonNull());
    quad->setRight(outsets[1].releaseNonNull());
    quad->setBottom(outsets[2].releaseNonNull());
    quad->setLeft(outsets[3].releaseNonNull());
    
    return CSSValuePool::singleton().createValue(WTFMove(quad));
}

static RefPtr<CSSValue> consumeBorderImageWidth(CSSPropertyID property, CSSParserTokenRange& range)
{
    RefPtr<CSSPrimitiveValue> widths[4];

    RefPtr<CSSPrimitiveValue> value;
    for (size_t index = 0; index < 4; ++index) {
        value = consumeNumber(range, ValueRange::NonNegative);
        if (!value)
            value = consumeLengthOrPercent(range, HTMLStandardMode, ValueRange::NonNegative, UnitlessQuirk::Forbid);
        if (!value)
            value = consumeIdent<CSSValueAuto>(range);
        if (!value)
            break;
        widths[index] = value;
    }
    if (!widths[0])
        return nullptr;
    complete4Sides(widths);

    // -webkit-border-image has a legacy behavior that makes fixed border slices also set the border widths.
    bool overridesBorderWidths = property == CSSPropertyWebkitBorderImage && (widths[0]->isLength() || widths[1]->isLength() || widths[2]->isLength() || widths[3]->isLength());

    // FIXME-NEWPARSER: Should just have a CSSQuadValue.
    auto quad = Quad::create();
    quad->setTop(widths[0].releaseNonNull());
    quad->setRight(widths[1].releaseNonNull());
    quad->setBottom(widths[2].releaseNonNull());
    quad->setLeft(widths[3].releaseNonNull());

    return CSSBorderImageWidthValue::create(CSSValuePool::singleton().createValue(WTFMove(quad)), overridesBorderWidths);
}

static bool consumeBorderImageComponents(CSSPropertyID property, CSSParserTokenRange& range, const CSSParserContext& context, RefPtr<CSSValue>& source,
    RefPtr<CSSValue>& slice, RefPtr<CSSValue>& width, RefPtr<CSSValue>& outset, RefPtr<CSSValue>& repeat)
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
                    width = consumeBorderImageWidth(property, range);
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

static RefPtr<CSSValue> consumeWebkitBorderImage(CSSPropertyID property, CSSParserTokenRange& range, const CSSParserContext& context)
{
    RefPtr<CSSValue> source;
    RefPtr<CSSValue> slice;
    RefPtr<CSSValue> width;
    RefPtr<CSSValue> outset;
    RefPtr<CSSValue> repeat;
    if (consumeBorderImageComponents(property, range, context, source, slice, width, outset, repeat))
        return createBorderImageValue(WTFMove(source), WTFMove(slice), WTFMove(width), WTFMove(outset), WTFMove(repeat));
    return nullptr;
}

static RefPtr<CSSValue> consumeReflect(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    
    RefPtr<CSSPrimitiveValue> direction = consumeIdent<CSSValueAbove, CSSValueBelow, CSSValueLeft, CSSValueRight>(range);
    if (!direction)
        return nullptr;

    RefPtr<CSSPrimitiveValue> offset;
    if (range.atEnd())
        offset = CSSValuePool::singleton().createValue(0, CSSUnitType::CSS_PX);
    else {
        offset = consumeLengthOrPercent(range, context.mode, ValueRange::All, UnitlessQuirk::Forbid);
        if (!offset)
            return nullptr;
    }

    RefPtr<CSSValue> mask;
    if (!range.atEnd()) {
        mask = consumeWebkitBorderImage(CSSPropertyWebkitBoxReflect, range, context);
        if (!mask)
            return nullptr;
    }
    return CSSReflectValue::create(direction.releaseNonNull(), offset.releaseNonNull(), WTFMove(mask));
}

static RefPtr<CSSPrimitiveValue> consumeBackgroundBlendMode(CSSParserTokenRange& range)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueNormal || id == CSSValueOverlay || (id >= CSSValueMultiply && id <= CSSValueLuminosity))
        return consumeIdent(range);
    return nullptr;
}

static RefPtr<CSSPrimitiveValue> consumeBackgroundAttachment(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueScroll, CSSValueFixed, CSSValueLocal>(range);
}

static RefPtr<CSSPrimitiveValue> consumeBackgroundBox(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueBorderBox, CSSValuePaddingBox, CSSValueContentBox>(range);
}

static RefPtr<CSSPrimitiveValue> consumeMaskClip(CSSParserTokenRange& range)
{
    // TODO: Also handle fill-box, stroke-box and view-box.
    return consumeIdent<CSSValueBorderBox, CSSValuePaddingBox, CSSValueContentBox, CSSValueNoClip>(range);
}

static RefPtr<CSSPrimitiveValue> consumeBackgroundClip(CSSParserTokenRange& range)
{
    if (auto value = consumeBackgroundBox(range))
        return value;
    return consumeIdent<CSSValueText, CSSValueWebkitText>(range);
}

static RefPtr<CSSPrimitiveValue> consumePrefixedMaskComposite(CSSParserTokenRange& range)
{
    return consumeIdentRange(range, CSSValueClear, CSSValuePlusLighter);
}

static RefPtr<CSSPrimitiveValue> consumeMaskComposite(CSSParserTokenRange& range)
{
    return consumeIdentRange(range, CSSValueAdd, CSSValueExclude);
}

static RefPtr<CSSPrimitiveValue> consumeWebkitMaskSourceType(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueAuto, CSSValueAlpha, CSSValueLuminance>(range);
}

static RefPtr<CSSPrimitiveValue> consumeWebkitMaskMode(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueAlpha, CSSValueLuminance, CSSValueMatchSource>(range);
}

static RefPtr<CSSPrimitiveValue> consumePrefixedBackgroundBox(CSSPropertyID property, CSSParserTokenRange& range, const CSSParserContext& /*context*/)
{
    // The values 'border', 'padding' and 'content' are deprecated and do not apply to the version of the property that has the -webkit- prefix removed.
    if (RefPtr<CSSPrimitiveValue> value = consumeIdentRange(range, CSSValueBorder, CSSValuePaddingBox))
        return value;
    if (range.peek().id() == CSSValueWebkitText || ((property == CSSPropertyWebkitBackgroundClip || property == CSSPropertyWebkitMaskClip) && range.peek().id() == CSSValueText))
        return consumeIdent(range);
    return nullptr;
}

static RefPtr<CSSPrimitiveValue> consumeBackgroundSize(CSSPropertyID property, CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (identMatches<CSSValueContain, CSSValueCover>(range.peek().id()))
        return consumeIdent(range);

    auto identicalValueEncoding = Pair::IdenticalValueEncoding::DoNotCoalesce;

    // FIXME: We're allowing the unitless quirk on this property because our
    // tests assume that. Other browser engines don't allow it though.
    RefPtr<CSSPrimitiveValue> horizontal = consumeIdent<CSSValueAuto>(range);
    if (horizontal)
        identicalValueEncoding = Pair::IdenticalValueEncoding::Coalesce;
    else
        horizontal = consumeLengthOrPercent(range, cssParserMode, ValueRange::NonNegative, UnitlessQuirk::Allow);

    if (!horizontal)
        return nullptr;

    RefPtr<CSSPrimitiveValue> vertical;
    if (!range.atEnd()) {
        vertical = consumeIdent<CSSValueAuto>(range);
        if (!vertical)
            vertical = consumeLengthOrPercent(range, cssParserMode, ValueRange::NonNegative, UnitlessQuirk::Allow);
    }

    if (!vertical) {
        if (property == CSSPropertyWebkitBackgroundSize) {
            // Legacy syntax: "-webkit-background-size: 10px" is equivalent to "background-size: 10px 10px".
            vertical = horizontal;
        } else if (property == CSSPropertyBackgroundSize)
            vertical = CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
        else
            return horizontal;
    }

    return createPrimitiveValuePair(horizontal.releaseNonNull(), vertical.releaseNonNull(), identicalValueEncoding);
}

static RefPtr<CSSValueList> consumeGridAutoFlow(CSSParserTokenRange& range)
{
    RefPtr<CSSPrimitiveValue> rowOrColumnValue = consumeIdent<CSSValueRow, CSSValueColumn>(range);
    RefPtr<CSSPrimitiveValue> denseAlgorithm = consumeIdent<CSSValueDense>(range);
    if (!rowOrColumnValue) {
        rowOrColumnValue = consumeIdent<CSSValueRow, CSSValueColumn>(range);
        if (!rowOrColumnValue && !denseAlgorithm)
            return nullptr;
    }
    RefPtr<CSSValueList> parsedValues = CSSValueList::createSpaceSeparated();
    if (rowOrColumnValue) {
        CSSValueID value = rowOrColumnValue->valueID();
        if (value == CSSValueID::CSSValueColumn || (value == CSSValueID::CSSValueRow && !denseAlgorithm))
            parsedValues->append(rowOrColumnValue.releaseNonNull());
    }
    if (denseAlgorithm)
        parsedValues->append(denseAlgorithm.releaseNonNull());
    return parsedValues;
}

static bool consumeRepeatStyleComponent(CSSParserTokenRange& range, RefPtr<CSSPrimitiveValue>& value1, RefPtr<CSSPrimitiveValue>& value2)
{
    if (consumeIdent<CSSValueRepeatX>(range)) {
        value1 = CSSValuePool::singleton().createIdentifierValue(CSSValueRepeat);
        value2 = CSSValuePool::singleton().createIdentifierValue(CSSValueNoRepeat);
        return true;
    }

    if (consumeIdent<CSSValueRepeatY>(range)) {
        value1 = CSSValuePool::singleton().createIdentifierValue(CSSValueNoRepeat);
        value2 = CSSValuePool::singleton().createIdentifierValue(CSSValueRepeat);
        return true;
    }

    value1 = consumeIdent<CSSValueRepeat, CSSValueNoRepeat, CSSValueRound, CSSValueSpace>(range);
    if (!value1)
        return false;

    value2 = consumeIdent<CSSValueRepeat, CSSValueNoRepeat, CSSValueRound, CSSValueSpace>(range);
    if (!value2)
        value2 = value1;

    return true;
}

static RefPtr<CSSValue> consumeRepeatStyle(CSSParserTokenRange& range)
{
    RefPtr<CSSPrimitiveValue> repeatX;
    RefPtr<CSSPrimitiveValue> repeatY;
    if (!consumeRepeatStyleComponent(range, repeatX, repeatY))
        return nullptr;

    ASSERT(repeatX);
    ASSERT(repeatY);
    return CSSBackgroundRepeatValue::create(repeatX.releaseNonNull(), repeatY.releaseNonNull());
}

static RefPtr<CSSValue> consumeBackgroundComponent(CSSPropertyID property, CSSParserTokenRange& range, const CSSParserContext& context)
{
    switch (property) {
    case CSSPropertyBackgroundClip:
        return consumeBackgroundClip(range);
    case CSSPropertyBackgroundBlendMode:
        return consumeBackgroundBlendMode(range);
    case CSSPropertyBackgroundAttachment:
        return consumeBackgroundAttachment(range);
    case CSSPropertyBackgroundOrigin:
        return consumeBackgroundBox(range);
    case CSSPropertyMaskComposite:
        return consumeMaskComposite(range);
    case CSSPropertyWebkitMaskComposite:
        return consumePrefixedMaskComposite(range);
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyWebkitMaskClip:
    case CSSPropertyMaskOrigin:
        return consumePrefixedBackgroundBox(property, range, context);
    case CSSPropertyMaskClip:
        return consumeMaskClip(range);
    case CSSPropertyBackgroundImage:
    case CSSPropertyMaskImage:
        return consumeImageOrNone(range, context);
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyMaskRepeat:
        return consumeRepeatStyle(range);
    case CSSPropertyMaskMode:
        return consumeWebkitMaskMode(range);
    case CSSPropertyWebkitMaskSourceType:
        return consumeWebkitMaskSourceType(range);
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyWebkitMaskPositionX:
        return consumePositionX(range, context.mode);
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyWebkitMaskPositionY:
        return consumePositionY(range, context.mode);
    case CSSPropertyBackgroundSize:
    case CSSPropertyWebkitBackgroundSize:
    case CSSPropertyMaskSize:
        return consumeBackgroundSize(property, range, context.mode);
    case CSSPropertyBackgroundColor:
        return consumeColor(range, context);
    default:
        break;
    };
    return nullptr;
}

static void addBackgroundValue(RefPtr<CSSValue>& list, Ref<CSSValue>&& value)
{
    if (list) {
        if (!list->isBaseValueList()) {
            RefPtr<CSSValue> firstValue = list;
            list = CSSValueList::createCommaSeparated();
            downcast<CSSValueList>(*list).append(firstValue.releaseNonNull());
        }
        downcast<CSSValueList>(*list).append(WTFMove(value));
    } else {
        // To conserve memory we don't actually wrap a single value in a list.
        list = WTFMove(value);
    }
}

static RefPtr<CSSValue> consumeCommaSeparatedBackgroundComponent(CSSPropertyID property, CSSParserTokenRange& range, const CSSParserContext& context)
{
    RefPtr<CSSValue> result;
    do {
        RefPtr<CSSValue> value = consumeBackgroundComponent(property, range, context);
        if (!value)
            return nullptr;
        addBackgroundValue(result, value.releaseNonNull());
    } while (consumeCommaIncludingWhitespace(range));
    return result;
}

static bool isSelfPositionKeyword(CSSValueID id)
{
    return identMatches<CSSValueStart, CSSValueEnd, CSSValueCenter, CSSValueSelfStart, CSSValueSelfEnd, CSSValueFlexStart, CSSValueFlexEnd>(id);
}

static bool isSelfPositionOrLeftOrRightKeyword(CSSValueID id)
{
    return isSelfPositionKeyword(id) || isLeftOrRightKeyword(id);
}

static RefPtr<CSSValue> consumeSelfPositionOverflowPosition(CSSParserTokenRange& range, IsPositionKeyword isPositionKeyword)
{
    ASSERT(isPositionKeyword);
    CSSValueID id = range.peek().id();
    if (isAuto(id) || isNormalOrStretch(id))
        return consumeIdent(range);

    if (isBaselineKeyword(id))
        return consumeBaselineKeyword(range);

    RefPtr<CSSPrimitiveValue> overflowPosition = consumeOverflowPositionKeyword(range);
    if (!isPositionKeyword(range.peek().id()))
        return nullptr;
    RefPtr<CSSPrimitiveValue> selfPosition = consumeIdent(range);
    if (overflowPosition)
        return createPrimitiveValuePair(overflowPosition.releaseNonNull(), selfPosition.releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce);
    return selfPosition;
}

static RefPtr<CSSValue> consumeAlignItems(CSSParserTokenRange& range)
{
    // align-items property does not allow the 'auto' value.
    if (identMatches<CSSValueAuto>(range.peek().id()))
        return nullptr;
    return consumeSelfPositionOverflowPosition(range, isSelfPositionKeyword);
}

static RefPtr<CSSValue> consumeJustifyItems(CSSParserTokenRange& range)
{
    // justify-items property does not allow the 'auto' value.
    if (identMatches<CSSValueAuto>(range.peek().id()))
        return nullptr;
    CSSParserTokenRange rangeCopy = range;
    RefPtr<CSSPrimitiveValue> legacy = consumeIdent<CSSValueLegacy>(rangeCopy);
    RefPtr<CSSPrimitiveValue> positionKeyword = consumeIdent<CSSValueCenter, CSSValueLeft, CSSValueRight>(rangeCopy);
    if (!legacy)
        legacy = consumeIdent<CSSValueLegacy>(rangeCopy);
    if (legacy) {
        range = rangeCopy;
        if (positionKeyword)
            return createPrimitiveValuePair(legacy.releaseNonNull(), positionKeyword.releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce);
        return legacy;
    }
    return consumeSelfPositionOverflowPosition(range, isSelfPositionOrLeftOrRightKeyword);
}

static RefPtr<CSSValue> consumeFitContent(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);
    RefPtr<CSSPrimitiveValue> length = consumeLengthOrPercent(args, cssParserMode, ValueRange::NonNegative, UnitlessQuirk::Allow);
    if (!length || !args.atEnd())
        return nullptr;
    range = rangeCopy;
    RefPtr<CSSFunctionValue> result = CSSFunctionValue::create(CSSValueFitContent);
    result->append(length.releaseNonNull());
    return result;
}

static RefPtr<CSSPrimitiveValue> consumeCustomIdentForGridLine(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto || range.peek().id() == CSSValueSpan)
        return nullptr;
    return consumeCustomIdent(range);
}

static RefPtr<CSSValue> consumeGridLine(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);

    RefPtr<CSSPrimitiveValue> spanValue;
    RefPtr<CSSPrimitiveValue> gridLineName;
    RefPtr<CSSPrimitiveValue> numericValue = consumeInteger(range);
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
    if (spanValue && numericValue && numericValue->intValue() < 0)
        return nullptr; // Negative numbers are not allowed for span.
    if (numericValue && numericValue->intValue() == 0)
        return nullptr; // An <integer> value of zero makes the declaration invalid.

    RefPtr<CSSValueList> values = CSSValueList::createSpaceSeparated();
    if (spanValue)
        values->append(spanValue.releaseNonNull());
    if (numericValue)
        values->append(numericValue.releaseNonNull());
    if (gridLineName)
        values->append(gridLineName.releaseNonNull());
    ASSERT(values->length());
    return values;
}

static bool isGridTrackFixedSized(const CSSPrimitiveValue& primitiveValue)
{
    CSSValueID valueID = primitiveValue.valueID();
    if (valueID == CSSValueMinContent || valueID == CSSValueWebkitMinContent || valueID == CSSValueMaxContent || valueID == CSSValueWebkitMaxContent || valueID == CSSValueAuto || primitiveValue.isFlex())
        return false;

    return true;
}

static bool isGridTrackFixedSized(const CSSValue& value)
{
    if (value.isPrimitiveValue())
        return isGridTrackFixedSized(downcast<CSSPrimitiveValue>(value));

    ASSERT(value.isFunctionValue());
    auto& function = downcast<CSSFunctionValue>(value);
    if (function.name() == CSSValueFitContent || function.length() < 2)
        return false;

    const CSSValue* minPrimitiveValue = downcast<CSSPrimitiveValue>(function.item(0));
    const CSSValue* maxPrimitiveValue = downcast<CSSPrimitiveValue>(function.item(1));
    return isGridTrackFixedSized(*minPrimitiveValue) || isGridTrackFixedSized(*maxPrimitiveValue);
}

static Vector<String> parseGridTemplateAreasColumnNames(StringView gridRowNames)
{
    ASSERT(!gridRowNames.isEmpty());
    Vector<String> columnNames;
    StringBuilder areaName;
    for (auto character : gridRowNames.codeUnits()) {
        if (isCSSSpace(character)) {
            if (!areaName.isEmpty()) {
                columnNames.append(areaName.toString());
                areaName.clear();
            }
            continue;
        }
        if (character == '.') {
            if (areaName == "."_s)
                continue;
            if (!areaName.isEmpty()) {
                columnNames.append(areaName.toString());
                areaName.clear();
            }
        } else {
            if (!isNameCodePoint(character))
                return Vector<String>();
            if (areaName == "."_s) {
                columnNames.append(areaName.toString());
                areaName.clear();
            }
        }

        areaName.append(character);
    }

    if (!areaName.isEmpty())
        columnNames.append(areaName.toString());

    return columnNames;
}

static bool parseGridTemplateAreasRow(StringView gridRowNames, NamedGridAreaMap& gridAreaMap, const size_t rowCount, size_t& columnCount)
{
    if (gridRowNames.isAllSpecialCharacters<isCSSSpace>())
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
        if (gridAreaName == "."_s)
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

static RefPtr<CSSPrimitiveValue> consumeGridBreadth(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    const CSSParserToken& token = range.peek();
    if (identMatches<CSSValueMinContent, CSSValueWebkitMinContent, CSSValueMaxContent, CSSValueWebkitMaxContent, CSSValueAuto>(token.id()))
        return consumeIdent(range);
    if (token.type() == DimensionToken && token.unitType() == CSSUnitType::CSS_FR) {
        if (range.peek().numericValue() < 0)
            return nullptr;
        return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().numericValue(), CSSUnitType::CSS_FR);
    }
    return consumeLengthOrPercent(range, cssParserMode, ValueRange::NonNegative);
}

static RefPtr<CSSValue> consumeGridTrackSize(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    const CSSParserToken& token = range.peek();
    if (identMatches<CSSValueAuto>(token.id()))
        return consumeIdent(range);

    if (token.functionId() == CSSValueMinmax) {
        CSSParserTokenRange rangeCopy = range;
        CSSParserTokenRange args = consumeFunction(rangeCopy);
        RefPtr<CSSPrimitiveValue> minTrackBreadth = consumeGridBreadth(args, cssParserMode);
        if (!minTrackBreadth || minTrackBreadth->isFlex() || !consumeCommaIncludingWhitespace(args))
            return nullptr;
        RefPtr<CSSPrimitiveValue> maxTrackBreadth = consumeGridBreadth(args, cssParserMode);
        if (!maxTrackBreadth || !args.atEnd())
            return nullptr;
        range = rangeCopy;
        RefPtr<CSSFunctionValue> result = CSSFunctionValue::create(CSSValueMinmax);
        result->append(minTrackBreadth.releaseNonNull());
        result->append(maxTrackBreadth.releaseNonNull());
        return result;
    }

    if (token.functionId() == CSSValueFitContent)
        return consumeFitContent(range, cssParserMode);

    return consumeGridBreadth(range, cssParserMode);
}

// Appends to the passed in CSSGridLineNamesValue if any, otherwise creates a new one. Returns nullptr if an empty list is consumed.
static RefPtr<CSSGridLineNamesValue> consumeGridLineNames(CSSParserTokenRange& range, CSSGridLineNamesValue* lineNames = nullptr, bool allowEmpty = false)
{
    CSSParserTokenRange rangeCopy = range;
    if (rangeCopy.consumeIncludingWhitespace().type() != LeftBracketToken)
        return nullptr;
    
    RefPtr<CSSGridLineNamesValue> result = lineNames;
    if (!result)
        result = CSSGridLineNamesValue::create();
    while (RefPtr<CSSPrimitiveValue> lineName = consumeCustomIdentForGridLine(rangeCopy))
        result->append(lineName.releaseNonNull());
    if (rangeCopy.consumeIncludingWhitespace().type() != RightBracketToken)
        return nullptr;
    range = rangeCopy;
    return (result->length() || allowEmpty) ? result : nullptr;
}

static bool consumeGridTrackRepeatFunction(CSSParserTokenRange& range, CSSParserMode cssParserMode, CSSValueList& list, bool& isAutoRepeat, bool& allTracksAreFixedSized)
{
    CSSParserTokenRange args = consumeFunction(range);
    // The number of repetitions for <auto-repeat> is not important at parsing level
    // because it will be computed later, let's set it to 1.
    size_t repetitions = 1;
    isAutoRepeat = identMatches<CSSValueAutoFill, CSSValueAutoFit>(args.peek().id());
    RefPtr<CSSValueList> repeatedValues;
    if (isAutoRepeat)
        repeatedValues = CSSGridAutoRepeatValue::create(args.consumeIncludingWhitespace().id());
    else {
        auto repetition = consumePositiveIntegerRaw(args);
        if (!repetition)
            return false;
        repetitions = clampTo<size_t>(static_cast<size_t>(*repetition), 0, GridPosition::max());
        repeatedValues = CSSValueList::createSpaceSeparated();
    }
    if (!consumeCommaIncludingWhitespace(args))
        return false;
    RefPtr<CSSGridLineNamesValue> lineNames = consumeGridLineNames(args);
    if (lineNames)
        repeatedValues->append(lineNames.releaseNonNull());

    size_t numberOfTracks = 0;
    while (!args.atEnd()) {
        RefPtr<CSSValue> trackSize = consumeGridTrackSize(args, cssParserMode);
        if (!trackSize)
            return false;
        if (allTracksAreFixedSized)
            allTracksAreFixedSized = isGridTrackFixedSized(*trackSize);
        repeatedValues->append(trackSize.releaseNonNull());
        ++numberOfTracks;
        lineNames = consumeGridLineNames(args);
        if (lineNames)
            repeatedValues->append(lineNames.releaseNonNull());
    }
    // We should have found at least one <track-size> or else it is not a valid <track-list>.
    if (!numberOfTracks)
        return false;

    if (isAutoRepeat)
        list.append(repeatedValues.releaseNonNull());
    else {
        // We clamp the repetitions to a multiple of the repeat() track list's size, while staying below the max grid size.
        repetitions = std::min(repetitions, GridPosition::max() / numberOfTracks);
        RefPtr<CSSValueList> integerRepeatedValues = CSSGridIntegerRepeatValue::create(repetitions);
        for (size_t i = 0; i < repeatedValues->length(); ++i)
            integerRepeatedValues->append(*repeatedValues->itemWithoutBoundsCheck(i));
        list.append(integerRepeatedValues.releaseNonNull());
    }
    return true;
}

static bool consumeSubgridNameRepeatFunction(CSSParserTokenRange& range, CSSValueList& list, bool& isAutoRepeat)
{
    CSSParserTokenRange args = consumeFunction(range);
    size_t repetitions = 1;
    isAutoRepeat = identMatches<CSSValueAutoFill>(args.peek().id());
    RefPtr<CSSValueList> repeatedValues;
    if (isAutoRepeat)
        repeatedValues = CSSGridAutoRepeatValue::create(args.consumeIncludingWhitespace().id());
    else {
        auto repetition = consumePositiveIntegerRaw(args);
        if (!repetition)
            return false;
        repetitions = clampTo<size_t>(static_cast<size_t>(*repetition), 0, GridPosition::max());
        repeatedValues = CSSGridIntegerRepeatValue::create(repetitions);
    }
    if (!consumeCommaIncludingWhitespace(args))
        return false;

    do {
        auto lineNames = consumeGridLineNames(args, nullptr, true);
        if (!lineNames)
            return false;
        repeatedValues->append(lineNames.releaseNonNull());
    } while (!args.atEnd());

    list.append(repeatedValues.releaseNonNull());
    return true;
}

enum TrackListType { GridTemplate, GridTemplateNoRepeat, GridAuto };

static RefPtr<CSSValue> consumeGridTrackList(CSSParserTokenRange& range, const CSSParserContext& context, TrackListType trackListType)
{
    bool seenAutoRepeat = false;
    if (trackListType == GridTemplate && context.subgridEnabled && range.peek().id() == CSSValueSubgrid) {
        consumeIdent(range);
        auto values = CSSSubgridValue::create();
        while (!range.atEnd() && range.peek().type() != DelimiterToken) {
            if (range.peek().functionId() == CSSValueRepeat) {
                bool isAutoRepeat;
                if (!consumeSubgridNameRepeatFunction(range, values, isAutoRepeat))
                    return nullptr;
                if (isAutoRepeat && seenAutoRepeat)
                    return nullptr;
                seenAutoRepeat = seenAutoRepeat || isAutoRepeat;
            } else if (auto value = consumeGridLineNames(range, nullptr, true))
                values->append(value.releaseNonNull());
            else
                return nullptr;
        }
        return values;
    }
    bool allowGridLineNames = trackListType != GridAuto;
    RefPtr<CSSValueList> values = CSSValueList::createSpaceSeparated();
    if (!allowGridLineNames && range.peek().type() == LeftBracketToken)
        return nullptr;
    RefPtr<CSSGridLineNamesValue> lineNames = consumeGridLineNames(range);
    if (lineNames)
        values->append(lineNames.releaseNonNull());
    
    bool allowRepeat = trackListType == GridTemplate;
    bool allTracksAreFixedSized = true;
    do {
        bool isAutoRepeat;
        if (range.peek().functionId() == CSSValueRepeat) {
            if (!allowRepeat)
                return nullptr;
            if (!consumeGridTrackRepeatFunction(range, context.mode, *values, isAutoRepeat, allTracksAreFixedSized))
                return nullptr;
            if (isAutoRepeat && seenAutoRepeat)
                return nullptr;
            seenAutoRepeat = seenAutoRepeat || isAutoRepeat;
        } else if (RefPtr<CSSValue> value = consumeGridTrackSize(range, context.mode)) {
            if (allTracksAreFixedSized)
                allTracksAreFixedSized = isGridTrackFixedSized(*value);
            values->append(value.releaseNonNull());
        } else {
            return nullptr;
        }
        if (seenAutoRepeat && !allTracksAreFixedSized)
            return nullptr;
        if (!allowGridLineNames && range.peek().type() == LeftBracketToken)
            return nullptr;
        lineNames = consumeGridLineNames(range);
        if (lineNames)
            values->append(lineNames.releaseNonNull());
    } while (!range.atEnd() && range.peek().type() != DelimiterToken);
    return values;
}

static RefPtr<CSSValue> consumeGridTemplatesRowsOrColumns(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    return consumeGridTrackList(range, context, GridTemplate);
}

static RefPtr<CSSValue> consumeGridTemplateAreas(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    NamedGridAreaMap gridAreaMap;
    size_t rowCount = 0;
    size_t columnCount = 0;

    while (range.peek().type() == StringToken) {
        if (!parseGridTemplateAreasRow(range.consumeIncludingWhitespace().value(), gridAreaMap, rowCount, columnCount))
            return nullptr;
        ++rowCount;
    }

    if (rowCount == 0)
        return nullptr;
    ASSERT(columnCount);
    return CSSGridTemplateAreasValue::create(gridAreaMap, rowCount, columnCount);
}

static RefPtr<CSSValue> consumeLineBoxContain(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    OptionSet<LineBoxContain> lineBoxContain;
    
    while (range.peek().type() == IdentToken) {
        auto id = range.peek().id();
        if (id == CSSValueBlock) {
            if (lineBoxContain.contains(LineBoxContain::Block))
                return nullptr;
            lineBoxContain.add(LineBoxContain::Block);
        } else if (id == CSSValueInline) {
            if (lineBoxContain.contains(LineBoxContain::Inline))
                return nullptr;
            lineBoxContain.add(LineBoxContain::Inline);
        } else if (id == CSSValueFont) {
            if (lineBoxContain.contains(LineBoxContain::Font))
                return nullptr;
            lineBoxContain.add(LineBoxContain::Font);
        } else if (id == CSSValueGlyphs) {
            if (lineBoxContain.contains(LineBoxContain::Glyphs))
                return nullptr;
            lineBoxContain.add(LineBoxContain::Glyphs);
        } else if (id == CSSValueReplaced) {
            if (lineBoxContain.contains(LineBoxContain::Replaced))
                return nullptr;
            lineBoxContain.add(LineBoxContain::Replaced);
        } else if (id == CSSValueInlineBox) {
            if (lineBoxContain.contains(LineBoxContain::InlineBox))
                return nullptr;
            lineBoxContain.add(LineBoxContain::InlineBox);
        } else if (id == CSSValueInitialLetter) {
            if (lineBoxContain.contains(LineBoxContain::InitialLetter))
                return nullptr;
            lineBoxContain.add(LineBoxContain::InitialLetter);
        } else
            return nullptr;
        range.consumeIncludingWhitespace();
    }
    
    if (!lineBoxContain)
        return nullptr;
    
    return CSSLineBoxContainValue::create(lineBoxContain);
}

static RefPtr<CSSValue> consumeLineGrid(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    return consumeCustomIdent(range);
}

static RefPtr<CSSValue> consumeContainerName(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    auto list = CSSValueList::createSpaceSeparated();
    do {
        auto name = consumeSingleContainerName(range);
        if (!name)
            return list;
        list->append(name.releaseNonNull());
    } while (!range.atEnd());

    return list;
}

static RefPtr<CSSValue> consumeInitialLetter(CSSParserTokenRange& range)
{
    RefPtr<CSSValue> ident = consumeIdent<CSSValueNormal>(range);
    if (ident)
        return ident;
    
    RefPtr<CSSPrimitiveValue> height = consumeNumber(range, ValueRange::NonNegative);
    if (!height)
        return nullptr;
    
    RefPtr<CSSPrimitiveValue> position;
    if (!range.atEnd()) {
        position = consumeNumber(range, ValueRange::NonNegative);
        if (!position || !range.atEnd())
            return nullptr;
    } else
        position = height.copyRef();
    
    return createPrimitiveValuePair(position.releaseNonNull(), WTFMove(height));
}

static RefPtr<CSSValue> consumeSpeakAs(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    
    bool seenNormal = false;
    bool seenSpellOut = false;
    bool seenLiteralPunctuation = false;
    bool seenNoPunctuation = false;

    // normal | spell-out || digits || [ literal-punctuation | no-punctuation ]
    while (!range.atEnd()) {
        CSSValueID valueID = range.peek().id();
        if ((valueID == CSSValueNormal && seenSpellOut)
            || (valueID == CSSValueSpellOut && seenNormal)
            || (valueID == CSSValueLiteralPunctuation && seenNoPunctuation)
            || (valueID == CSSValueNoPunctuation && seenLiteralPunctuation))
            return nullptr;
        RefPtr<CSSValue> ident = consumeIdent<CSSValueNormal, CSSValueSpellOut, CSSValueDigits, CSSValueLiteralPunctuation, CSSValueNoPunctuation>(range);
        if (!ident)
            return nullptr;
        switch (valueID) {
        case CSSValueNormal:
            seenNormal = true;
            break;
        case CSSValueSpellOut:
            seenSpellOut = true;
            break;
        case CSSValueLiteralPunctuation:
            seenLiteralPunctuation = true;
            break;
        case CSSValueNoPunctuation:
            seenNoPunctuation = true;
            break;
        default:
            break;
        }
        list->append(ident.releaseNonNull());
    }
    
    return list->length() ? list : nullptr;
}
    
static RefPtr<CSSValue> consumeHangingPunctuation(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();

    bool seenForceEnd = false;
    bool seenAllowEnd = false;
    bool seenFirst = false;
    bool seenLast = false;

    while (!range.atEnd()) {
        CSSValueID valueID = range.peek().id();
        if ((valueID == CSSValueFirst && seenFirst)
            || (valueID == CSSValueLast && seenLast)
            || (valueID == CSSValueAllowEnd && (seenAllowEnd || seenForceEnd))
            || (valueID == CSSValueForceEnd && (seenAllowEnd || seenForceEnd)))
            return nullptr;
        RefPtr<CSSValue> ident = consumeIdent<CSSValueAllowEnd, CSSValueForceEnd, CSSValueFirst, CSSValueLast>(range);
        if (!ident)
            return nullptr;
        switch (valueID) {
        case CSSValueAllowEnd:
            seenAllowEnd = true;
            break;
        case CSSValueForceEnd:
            seenForceEnd = true;
            break;
        case CSSValueFirst:
            seenFirst = true;
            break;
        case CSSValueLast:
            seenLast = true;
            break;
        default:
            break;
        }
        list->append(ident.releaseNonNull());
    }
    
    return list->length() ? list : nullptr;
}

static RefPtr<CSSValue> consumeWebkitMarqueeIncrement(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    return consumeLengthOrPercent(range, cssParserMode, ValueRange::All, UnitlessQuirk::Allow);
}

static RefPtr<CSSValue> consumeWebkitMarqueeRepetition(CSSParserTokenRange& range)
{
    return consumeNumber(range, ValueRange::NonNegative);
}

static RefPtr<CSSValue> consumeWebkitMarqueeSpeed(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    return consumeTime(range, cssParserMode, ValueRange::NonNegative, UnitlessQuirk::Allow);
}

static RefPtr<CSSValue> consumeAlt(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().type() == StringToken)
        return consumeString(range);
    
    if (range.peek().functionId() != CSSValueAttr)
        return nullptr;
    
    return consumeAttr(consumeFunction(range), context);
}

static RefPtr<CSSValue> consumeAspectRatio(CSSParserTokenRange& range)
{
    RefPtr<CSSPrimitiveValue> autoValue;
    if (range.peek().type() == IdentToken)
        autoValue = consumeIdent<CSSValueAuto>(range);

    if (range.atEnd())
        return autoValue;

    auto ratioList = consumeAspectRatioValue(range);
    if (!ratioList)
        return nullptr;

    if (!autoValue)
        autoValue = consumeIdent<CSSValueAuto>(range);

    if (!autoValue)
        return ratioList;

    auto list = CSSValueList::createSpaceSeparated();
    list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueAuto));
    list->append(ratioList.releaseNonNull());

    return list;
}

static RefPtr<CSSValue> consumeContain(CSSParserTokenRange& range)
{
    if (auto singleValue = consumeIdent<CSSValueNone, CSSValueStrict, CSSValueContent>(range))
        return singleValue;
    auto list = CSSValueList::createSpaceSeparated();
    RefPtr<CSSPrimitiveValue> size, inlineSize, layout, paint, style;
    while (!range.atEnd()) {
        switch (range.peek().id()) {
        case CSSValueSize:
            if (size)
                return nullptr;
            size = consumeIdent(range);
            break;
        case CSSValueInlineSize:
            if (inlineSize || size)
                return nullptr;
            inlineSize = consumeIdent(range);
            break;
        case CSSValueLayout:
            if (layout)
                return nullptr;
            layout = consumeIdent(range);
            break;
        case CSSValuePaint:
            if (paint)
                return nullptr;
            paint = consumeIdent(range);
            break;
        case CSSValueStyle:
            if (style)
                return nullptr;
            style = consumeIdent(range);
            break;
        default:
            return nullptr;
        }
    }
    if (size)
        list->append(size.releaseNonNull());
    if (inlineSize)
        list->append(inlineSize.releaseNonNull());
    if (layout)
        list->append(layout.releaseNonNull());
    if (style)
        list->append(style.releaseNonNull());
    if (paint)
        list->append(paint.releaseNonNull());
    if (!list->length())
        return nullptr;
    return RefPtr<CSSValue>(WTFMove(list));
}

static RefPtr<CSSValue> consumeContainIntrinsicSize(CSSParserTokenRange& range)
{
    RefPtr<CSSPrimitiveValue> autoValue;
    if (range.peek().type() == IdentToken) {
        switch (range.peek().id()) {
        case CSSValueNone:
            return consumeIdent<CSSValueNone>(range);
        case CSSValueAuto:
            autoValue = consumeIdent<CSSValueAuto>(range);
            break;
        default:
            return nullptr;
        }
    }

    if (range.atEnd())
        return nullptr;

    auto lengthValue = consumeLength(range, HTMLStandardMode, ValueRange::NonNegative);
    if (!lengthValue)
        return nullptr;

    if (!autoValue)
        return lengthValue;

    auto list = CSSValueList::createSpaceSeparated();
    list->append(autoValue.releaseNonNull());
    list->append(lengthValue.releaseNonNull());

    return list;
}

static RefPtr<CSSValue> consumeContentVisibility(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueVisible, CSSValueAuto, CSSValueHidden>(range);
}

static RefPtr<CSSValue> consumeTextEmphasisPosition(CSSParserTokenRange& range)
{
    bool foundOverOrUnder = false;
    CSSValueID overUnderValueID = CSSValueOver;
    bool foundLeftOrRight = false;
    CSSValueID leftRightValueID = CSSValueRight;
    while (!range.atEnd()) {
        switch (range.peek().id()) {
        case CSSValueOver:
            if (foundOverOrUnder)
                return nullptr;
            foundOverOrUnder = true;
            overUnderValueID = CSSValueOver;
            break;
        case CSSValueUnder:
            if (foundOverOrUnder)
                return nullptr;
            foundOverOrUnder = true;
            overUnderValueID = CSSValueUnder;
            break;
        case CSSValueLeft:
            if (foundLeftOrRight)
                return nullptr;
            foundLeftOrRight = true;
            leftRightValueID = CSSValueLeft;
            break;
        case CSSValueRight:
            if (foundLeftOrRight)
                return nullptr;
            foundLeftOrRight = true;
            leftRightValueID = CSSValueRight;
            break;
        default:
            return nullptr;
        }
        
        range.consumeIncludingWhitespace();
    }
    if (!foundOverOrUnder)
        return nullptr;
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    list->append(CSSValuePool::singleton().createIdentifierValue(overUnderValueID));
    if (foundLeftOrRight)
        list->append(CSSValuePool::singleton().createIdentifierValue(leftRightValueID));
    return list;
}

#if ENABLE(DARK_MODE_CSS)

static RefPtr<CSSValue> consumeColorScheme(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);

    Vector<CSSValueID, 3> identifiers;

    while (!range.atEnd()) {
        if (range.peek().type() != IdentToken)
            return nullptr;

        CSSValueID id = range.peek().id();

        switch (id) {
        case CSSValueNormal:
            // `normal` is only allowed as a single value, and was handled earlier.
            // Don't allow it in the list.
            return nullptr;

        case CSSValueOnly:
        case CSSValueLight:
        case CSSValueDark:
            if (!identifiers.appendIfNotContains(id))
                return nullptr;
            break;

        default:
            // Unknown identifiers are allowed and ignored.
            break;
        }

        range.consumeIncludingWhitespace();
    }

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    for (auto id : identifiers)
        list->append(CSSValuePool::singleton().createIdentifierValue(id));
    return list;
}

#endif

static RefPtr<CSSPrimitiveValue> consumePositionOrAuto(CSSParserTokenRange& range, CSSParserMode parserMode, UnitlessQuirk unitless, PositionSyntax positionSyntax)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);

    return consumePosition(range, parserMode, unitless, positionSyntax);
}

static RefPtr<CSSOffsetRotateValue> consumeOffsetRotate(CSSParserTokenRange& range, CSSParserMode mode)
{
    RefPtr<CSSPrimitiveValue> modifier;
    RefPtr<CSSPrimitiveValue> angle;

    auto rangeCopy = range;

    // Attempt to parse the first token as the modifier (auto / reverse keyword). If
    // successful, parse the second token as the angle. If not, try to parse the other
    // way around.
    if ((modifier = consumeIdent<CSSValueAuto, CSSValueReverse>(rangeCopy)))
        angle = consumeAngle(rangeCopy, mode);
    else {
        angle = consumeAngle(rangeCopy, mode);
        modifier = consumeIdent<CSSValueAuto, CSSValueReverse>(rangeCopy);
    }

    if (!angle && !modifier)
        return nullptr;

    range = rangeCopy;

    return CSSOffsetRotateValue::create(WTFMove(modifier), WTFMove(angle));
}

RefPtr<CSSValue> CSSPropertyParser::parseSingleValue(CSSPropertyID property, CSSPropertyID currentShorthand)
{
    if (CSSParserFastPaths::isKeywordPropertyID(property)) {
        if (CSSParserFastPaths::isValidKeywordPropertyAndValue(property, m_range.peek().id(), m_context))
            return consumeIdent(m_range);

        // Some properties need to fall back onto the regular parser.
        if (!CSSParserFastPaths::isPartialKeywordPropertyID(property))
            return nullptr;
    }

    if (!isCSSPropertyExposed(property, &m_context.propertySettings) && !isInternalCSSProperty(property)) {
        // Allow internal properties as we use them to parse several internal-only-shorthands (e.g. background-repeat),
        // and to handle certain DOM-exposed values (e.g. -webkit-font-size-delta from execCommand('FontSizeDelta')).
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    switch (property) {
    case CSSPropertyDisplay:
        return consumeDisplay(m_range);
    case CSSPropertyWillChange:
        return consumeWillChange(m_range, m_context);
    case CSSPropertyPage:
        return consumePage(m_range);
    case CSSPropertyQuotes:
        return consumeQuotes(m_range);
    case CSSPropertyFontVariantAlternates:
        return consumeFontVariantAlternates(m_range);
    case CSSPropertyFontVariantCaps:
        return consumeFontVariantCaps(m_range);
    case CSSPropertyFontVariantLigatures:
        return consumeFontVariantLigatures(m_range);
    case CSSPropertyFontVariantNumeric:
        return consumeFontVariantNumeric(m_range);
    case CSSPropertyFontVariantEastAsian:
        return consumeFontVariantEastAsian(m_range);
    case CSSPropertyFontFeatureSettings:
        return consumeFontFeatureSettings(m_range, CSSValuePool::singleton());
    case CSSPropertyFontFamily:
        return consumeFontFamily(m_range);
    case CSSPropertyFontWeight:
        return consumeFontWeight(m_range);
    case CSSPropertyFontPalette:
        return consumeFontPalette(m_range);
    case CSSPropertyFontStretch:
        return consumeFontStretch(m_range, CSSValuePool::singleton());
    case CSSPropertyFontStyle:
        return consumeFontStyle(m_range, m_context.mode, CSSValuePool::singleton());
    case CSSPropertyFontSynthesisWeight:
    case CSSPropertyFontSynthesisStyle:
    case CSSPropertyFontSynthesisSmallCaps:
        return consumeFontSynthesisLonghand(m_range);
#if ENABLE(VARIATION_FONTS)
    case CSSPropertyFontVariationSettings:
        return consumeFontVariationSettings(m_range);
#endif
    case CSSPropertyLetterSpacing:
        return consumeLetterSpacing(m_range, m_context.mode);
    case CSSPropertyWordSpacing:
        return consumeWordSpacing(m_range, m_context.mode);
    case CSSPropertyTabSize:
        return consumeTabSize(m_range, m_context.mode);
#if ENABLE(TEXT_AUTOSIZING)
    case CSSPropertyWebkitTextSizeAdjust:
        return consumeTextSizeAdjust(m_range, m_context.mode);
#endif
    case CSSPropertyFontSize:
        return consumeFontSize(m_range, m_context.mode, UnitlessQuirk::Allow);
    case CSSPropertyLineHeight:
        return consumeLineHeight(m_range, m_context.mode);
    case CSSPropertyWebkitBorderHorizontalSpacing:
    case CSSPropertyWebkitBorderVerticalSpacing:
        return consumeLength(m_range, m_context.mode, ValueRange::NonNegative);
    case CSSPropertyCounterIncrement:
    case CSSPropertyCounterReset:
        return consumeCounter(m_range, property == CSSPropertyCounterIncrement ? 1 : 0);
    case CSSPropertySize:
        return consumeSize(m_range, m_context.mode);
    case CSSPropertyTextIndent:
        return consumeTextIndent(m_range, m_context.mode);
    case CSSPropertyMaxWidth:
    case CSSPropertyMaxHeight:
        return consumeMaxWidthOrHeight(m_range, m_context, UnitlessQuirk::Allow);
    case CSSPropertyMaxInlineSize:
    case CSSPropertyMaxBlockSize:
        return consumeMaxWidthOrHeight(m_range, m_context);
    case CSSPropertyMinWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyWidth:
    case CSSPropertyHeight:
        return consumeWidthOrHeight(m_range, m_context, UnitlessQuirk::Allow);
    case CSSPropertyMinInlineSize:
    case CSSPropertyMinBlockSize:
    case CSSPropertyInlineSize:
    case CSSPropertyBlockSize:
        return consumeWidthOrHeight(m_range, m_context);
    case CSSPropertyMarginTop:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
    case CSSPropertyBottom:
    case CSSPropertyLeft:
    case CSSPropertyRight:
    case CSSPropertyTop: {
        UnitlessQuirk unitless = currentShorthand != CSSPropertyInset ? UnitlessQuirk::Allow : UnitlessQuirk::Forbid;
        return consumeMarginOrOffset(m_range, m_context.mode, unitless);
    }
    case CSSPropertyInsetInlineStart:
    case CSSPropertyInsetInlineEnd:
    case CSSPropertyInsetBlockStart:
    case CSSPropertyInsetBlockEnd:
    case CSSPropertyMarginInlineStart:
    case CSSPropertyMarginInlineEnd:
    case CSSPropertyMarginBlockStart:
    case CSSPropertyMarginBlockEnd:
        return consumeMarginOrOffset(m_range, m_context.mode, UnitlessQuirk::Forbid);
    case CSSPropertyPaddingTop:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
        return consumeLengthOrPercent(m_range, m_context.mode, ValueRange::NonNegative, UnitlessQuirk::Allow);
    case CSSPropertyPaddingInlineStart:
    case CSSPropertyPaddingInlineEnd:
    case CSSPropertyPaddingBlockStart:
    case CSSPropertyPaddingBlockEnd:
        return consumeLengthOrPercent(m_range, m_context.mode, ValueRange::NonNegative, UnitlessQuirk::Forbid);
    case CSSPropertyScrollMarginBottom:
    case CSSPropertyScrollMarginLeft:
    case CSSPropertyScrollMarginRight:
    case CSSPropertyScrollMarginTop:
    case CSSPropertyScrollMarginInlineStart:
    case CSSPropertyScrollMarginInlineEnd:
    case CSSPropertyScrollMarginBlockStart:
    case CSSPropertyScrollMarginBlockEnd:
        return consumeLength(m_range, m_context.mode, ValueRange::All);
    case CSSPropertyScrollPaddingBottom:
    case CSSPropertyScrollPaddingLeft:
    case CSSPropertyScrollPaddingRight:
    case CSSPropertyScrollPaddingTop:
    case CSSPropertyScrollPaddingInlineStart:
    case CSSPropertyScrollPaddingInlineEnd:
    case CSSPropertyScrollPaddingBlockStart:
    case CSSPropertyScrollPaddingBlockEnd:
        return consumeScrollPadding(m_range, m_context.mode);
    case CSSPropertyScrollSnapAlign:
        return consumeScrollSnapAlign(m_range);
    case CSSPropertyScrollSnapStop:
        return consumeIdent<CSSValueAlways, CSSValueNormal>(m_range);
    case CSSPropertyScrollSnapType:
        return consumeScrollSnapType(m_range);
    case CSSPropertyScrollBehavior:
        return consumeScrollBehavior(m_range);
    case CSSPropertyOverscrollBehaviorBlock:
    case CSSPropertyOverscrollBehaviorInline:
    case CSSPropertyOverscrollBehaviorX:
    case CSSPropertyOverscrollBehaviorY:
        return consumeOverscrollBehavior(m_range);
    case CSSPropertyClip:
        return consumeClip(m_range, m_context.mode);
    case CSSPropertyTouchAction:
        return consumeTouchAction(m_range);
    case CSSPropertyObjectPosition:
        return consumePosition(m_range, m_context.mode, UnitlessQuirk::Forbid, PositionSyntax::Position);
    case CSSPropertyWebkitLineClamp:
        return consumeLineClamp(m_range);
    case CSSPropertyWebkitFontSizeDelta:
        return consumeLength(m_range, m_context.mode, ValueRange::All, UnitlessQuirk::Allow);
    case CSSPropertyWebkitHyphenateCharacter:
    case CSSPropertyWebkitLocale:
        return consumeAutoOrString(m_range);
    case CSSPropertyWebkitHyphenateLimitBefore:
    case CSSPropertyWebkitHyphenateLimitAfter:
        return consumeHyphenateLimit(m_range, CSSValueAuto);
    case CSSPropertyWebkitHyphenateLimitLines:
        return consumeHyphenateLimit(m_range, CSSValueNoLimit);
    case CSSPropertyColumnWidth:
        return consumeColumnWidth(m_range);
    case CSSPropertyColumnCount:
        return consumeColumnCount(m_range);
    case CSSPropertyColumnGap:
        return consumeGapLength(m_range, m_context.mode);
    case CSSPropertyRowGap:
        return consumeGapLength(m_range, m_context.mode);
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
    case CSSPropertyAnimationComposition:
    case CSSPropertyTransitionProperty:
    case CSSPropertyAnimationTimingFunction:
    case CSSPropertyTransitionTimingFunction:
        return consumeAnimationPropertyList(property, m_range, m_context);
    case CSSPropertyShapeMargin:
        return consumeLengthOrPercent(m_range, m_context.mode, ValueRange::NonNegative);
    case CSSPropertyShapeImageThreshold:
        return consumeNumber(m_range, ValueRange::All);
    case CSSPropertyWebkitBoxOrdinalGroup:
    case CSSPropertyOrphans:
    case CSSPropertyWidows:
        return consumePositiveInteger(m_range);
    case CSSPropertyTextDecorationColor:
        return consumeColor(m_range, m_context);
    case CSSPropertyWebkitTextStrokeWidth:
        return consumeTextStrokeWidth(m_range, m_context.mode);
    case CSSPropertyWebkitTextFillColor:
#if ENABLE(TOUCH_EVENTS)
    case CSSPropertyWebkitTapHighlightColor:
#endif
    case CSSPropertyTextEmphasisColor:
    case CSSPropertyBorderInlineStartColor:
    case CSSPropertyBorderInlineEndColor:
    case CSSPropertyBorderBlockStartColor:
    case CSSPropertyBorderBlockEndColor:
    case CSSPropertyWebkitTextStrokeColor:
    case CSSPropertyStrokeColor:
    case CSSPropertyStopColor:
    case CSSPropertyFloodColor:
    case CSSPropertyLightingColor:
    case CSSPropertyColumnRuleColor:
        return consumeColor(m_range, m_context);
    case CSSPropertyAccentColor: {
        return consumeColorWithAuto(m_range, m_context);
    }
    case CSSPropertyCaretColor:
        return consumeColorWithAuto(m_range, m_context);
    case CSSPropertyColor:
    case CSSPropertyBackgroundColor:
        return consumeColor(m_range, m_context, inQuirksMode());
    case CSSPropertyBorderInlineStartWidth:
    case CSSPropertyBorderInlineEndWidth:
    case CSSPropertyBorderBlockStartWidth:
    case CSSPropertyBorderBlockEndWidth:
        return consumeBorderWidth(m_range, m_context.mode, UnitlessQuirk::Forbid);
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor: {
        bool allowQuirkyColors = inQuirksMode()
            && (currentShorthand == CSSPropertyInvalid || currentShorthand == CSSPropertyBorderColor);
        return consumeColor(m_range, m_context, allowQuirkyColors);
    }
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderTopWidth: {
        bool allowQuirkyLengths = inQuirksMode()
            && (currentShorthand == CSSPropertyInvalid || currentShorthand == CSSPropertyBorderWidth);
        UnitlessQuirk unitless = allowQuirkyLengths ? UnitlessQuirk::Allow : UnitlessQuirk::Forbid;
        return consumeBorderWidth(m_range, m_context.mode, unitless);
    }
    case CSSPropertyZIndex:
        return consumeZIndex(m_range);
    case CSSPropertyTextShadow: // CSS2 property, dropped in CSS2.1, back in CSS3, so treat as CSS3
    case CSSPropertyBoxShadow:
    case CSSPropertyWebkitBoxShadow:
        return consumeShadow(m_range, m_context, property == CSSPropertyBoxShadow || property == CSSPropertyWebkitBoxShadow);
    case CSSPropertyFilter:
#if ENABLE(FILTERS_LEVEL_2)
    case CSSPropertyWebkitBackdropFilter:
#endif
        return consumeFilter(m_range, m_context, AllowedFilterFunctions::PixelFilters);
    case CSSPropertyAppleColorFilter:
        return consumeFilter(m_range, m_context, AllowedFilterFunctions::ColorFilters);
    case CSSPropertyWebkitTextDecorationsInEffect:
    case CSSPropertyTextDecorationLine:
        return consumeTextDecorationLine(m_range);
    case CSSPropertyTextEmphasisStyle:
        return consumeTextEmphasisStyle(m_range);
    case CSSPropertyOutlineColor:
        return consumeOutlineColor(m_range, m_context.mode);
    case CSSPropertyOutlineOffset:
        return consumeLength(m_range, m_context.mode, ValueRange::All);
    case CSSPropertyOutlineWidth:
        return consumeLineWidth(m_range, m_context.mode, UnitlessQuirk::Forbid);
    case CSSPropertyTransform:
        return consumeTransform(m_range, m_context.mode);
    case CSSPropertyTransformBox:
        return consumeIdent<CSSValueBorderBox, CSSValueViewBox, CSSValueFillBox, CSSValueStrokeBox, CSSValueContentBox>(m_range);
    case CSSPropertyTransformOriginX:
    case CSSPropertyPerspectiveOriginX:
        return consumePositionX(m_range, m_context.mode);
    case CSSPropertyTransformOriginY:
    case CSSPropertyPerspectiveOriginY:
        return consumePositionY(m_range, m_context.mode);
    case CSSPropertyTransformOriginZ:
        return consumeLength(m_range, m_context.mode, ValueRange::All);
    case CSSPropertyTranslate:
        return consumeTranslate(m_range, m_context.mode);
    case CSSPropertyScale:
        return consumeScale(m_range, m_context.mode);
    case CSSPropertyRotate:
        return consumeRotate(m_range, m_context.mode);
    case CSSPropertyFill:
    case CSSPropertyStroke:
        return consumePaintStroke(m_range, m_context);
    case CSSPropertyGlyphOrientationVertical:
    case CSSPropertyGlyphOrientationHorizontal:
        return consumeGlyphOrientation(m_range, m_context.mode, property);
    case CSSPropertyPaintOrder:
        return consumePaintOrder(m_range);
    case CSSPropertyMarkerStart:
    case CSSPropertyMarkerMid:
    case CSSPropertyMarkerEnd:
        return consumeNoneOrURI(m_range);
    case CSSPropertyFlexBasis:
        return consumeFlexBasis(m_range, m_context);
    case CSSPropertyFlexGrow:
    case CSSPropertyFlexShrink:
        return consumeNumber(m_range, ValueRange::NonNegative);
    case CSSPropertyStrokeDasharray:
        return consumeStrokeDasharray(m_range);
    case CSSPropertyColumnRuleWidth:
        return consumeColumnRuleWidth(m_range, m_context.mode);
    case CSSPropertyStrokeOpacity:
    case CSSPropertyFillOpacity:
    case CSSPropertyStopOpacity:
    case CSSPropertyFloodOpacity:
    case CSSPropertyOpacity:
        if (auto parsedValue = consumeNumber(m_range, ValueRange::All))
            return parsedValue;
        return consumePercent(m_range, ValueRange::All);
    case CSSPropertyOffsetPath:
        return consumePathOperation(m_range, m_context, ConsumeRay::Include);
    case CSSPropertyOffsetDistance:
        return consumeLengthOrPercent(m_range, m_context.mode, ValueRange::All, UnitlessQuirk::Forbid);
    case CSSPropertyOffsetPosition:
    case CSSPropertyOffsetAnchor:
        return consumePositionOrAuto(m_range, m_context.mode, UnitlessQuirk::Forbid, PositionSyntax::Position);
    case CSSPropertyOffsetRotate:
        return consumeOffsetRotate(m_range, m_context.mode);
    case CSSPropertyWebkitBoxFlex:
        return consumeNumber(m_range, ValueRange::All);
    case CSSPropertyBaselineShift:
        return consumeBaselineShift(m_range);
    case CSSPropertyKerning:
        return consumeKerning(m_range, m_context.mode);
    case CSSPropertyStrokeMiterlimit:
        return consumeNumber(m_range, ValueRange::NonNegative);
    case CSSPropertyStrokeWidth:
    case CSSPropertyStrokeDashoffset:
    case CSSPropertyCx:
    case CSSPropertyCy:
    case CSSPropertyX:
    case CSSPropertyY:
        return consumeLengthOrPercent(m_range, SVGAttributeMode, ValueRange::All, UnitlessQuirk::Forbid);
    case CSSPropertyR:
        return consumeLengthOrPercent(m_range, m_context.mode, ValueRange::NonNegative, UnitlessQuirk::Forbid);
    case CSSPropertyRx:
    case CSSPropertyRy:
        return consumeRxOrRy(m_range, m_context.mode);
    case CSSPropertyCursor:
        return consumeCursor(m_range, m_context, inQuirksMode());
    case CSSPropertyContent:
        return consumeContent(m_range, m_context);
    case CSSPropertyListStyleImage:
    case CSSPropertyBorderImageSource:
    case CSSPropertyWebkitMaskBoxImageSource:
        return consumeImageOrNone(m_range, m_context);
    case CSSPropertyPerspective:
        return consumePerspective(m_range, m_context.mode);
    case CSSPropertyBorderTopRightRadius:
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderBottomLeftRadius:
    case CSSPropertyBorderBottomRightRadius:
    case CSSPropertyBorderStartStartRadius:
    case CSSPropertyBorderStartEndRadius:
    case CSSPropertyBorderEndStartRadius:
    case CSSPropertyBorderEndEndRadius:
        return consumeBorderRadiusCorner(m_range, m_context.mode);
    case CSSPropertyWebkitBoxFlexGroup:
        return consumeIntegerZeroAndGreater(m_range);
    case CSSPropertyOrder:
        return consumeInteger(m_range);
    case CSSPropertyTextUnderlinePosition:
        // auto | [ [ under | from-font ] || [ left | right ] ], but we only support auto | under | from-font for now
        return consumeIdent<CSSValueAuto, CSSValueUnder, CSSValueFromFont>(m_range);
    case CSSPropertyTextUnderlineOffset:
        return consumeTextUnderlineOffset(m_range, m_context.mode);
    case CSSPropertyTextDecorationThickness:
        return consumeTextDecorationThickness(m_range, m_context.mode);
    case CSSPropertyVerticalAlign:
        return consumeVerticalAlign(m_range, m_context.mode);
    case CSSPropertyShapeOutside:
        return consumeShapeOutside(m_range, m_context);
    case CSSPropertyClipPath:
        return consumePathOperation(m_range, m_context, ConsumeRay::Exclude);
    case CSSPropertyJustifyContent:
        // justify-content property does not allow the <baseline-position> values.
        if (isBaselineKeyword(m_range.peek().id()))
            return nullptr;
        return consumeContentDistributionOverflowPosition(m_range, isContentPositionOrLeftOrRightKeyword);
    case CSSPropertyAlignContent:
        return consumeContentDistributionOverflowPosition(m_range, isContentPositionKeyword);
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
        return consumeBorderImageWidth(property, m_range);
    case CSSPropertyWebkitMaskBoxImage:
        return consumeWebkitBorderImage(property, m_range, m_context);
    case CSSPropertyWebkitBoxReflect:
        return consumeReflect(m_range, m_context);
    case CSSPropertyWebkitLineBoxContain:
        return consumeLineBoxContain(m_range);
    case CSSPropertyBackgroundAttachment:
    case CSSPropertyBackgroundBlendMode:
    case CSSPropertyBackgroundClip:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyBackgroundSize:
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyMaskClip:
    case CSSPropertyWebkitMaskClip:
    case CSSPropertyMaskComposite:
    case CSSPropertyWebkitMaskComposite:
    case CSSPropertyMaskImage:
    case CSSPropertyMaskOrigin:
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
    case CSSPropertyMaskRepeat:
    case CSSPropertyMaskSize:
    case CSSPropertyMaskMode:
    case CSSPropertyWebkitMaskSourceType:
        return consumeCommaSeparatedBackgroundComponent(property, m_range, m_context);
    case CSSPropertyAlignItems:
        return consumeAlignItems(m_range);
    case CSSPropertyJustifySelf:
        return consumeSelfPositionOverflowPosition(m_range, isSelfPositionOrLeftOrRightKeyword);
    case CSSPropertyAlignSelf:
        return consumeSelfPositionOverflowPosition(m_range, isSelfPositionKeyword);
    case CSSPropertyJustifyItems:
        return consumeJustifyItems(m_range);
    case CSSPropertyGridColumnEnd:
    case CSSPropertyGridColumnStart:
    case CSSPropertyGridRowEnd:
    case CSSPropertyGridRowStart:
        return consumeGridLine(m_range);
    case CSSPropertyGridAutoColumns:
    case CSSPropertyGridAutoRows:
        return consumeGridTrackList(m_range, m_context, GridAuto);
    case CSSPropertyGridTemplateColumns:
    case CSSPropertyGridTemplateRows:
        return consumeGridTemplatesRowsOrColumns(m_range, m_context);
    case CSSPropertyGridTemplateAreas:
        return consumeGridTemplateAreas(m_range);
    case CSSPropertyGridAutoFlow:
        return consumeGridAutoFlow(m_range);
    case CSSPropertyWebkitLineGrid:
        return consumeLineGrid(m_range);
    case CSSPropertyWebkitInitialLetter:
        return consumeInitialLetter(m_range);
    case CSSPropertySpeakAs:
        return consumeSpeakAs(m_range);
    case CSSPropertyHangingPunctuation:
        return consumeHangingPunctuation(m_range);
    case CSSPropertyWebkitMarqueeIncrement:
        return consumeWebkitMarqueeIncrement(m_range, m_context.mode);
    case CSSPropertyWebkitMarqueeRepetition:
        return consumeWebkitMarqueeRepetition(m_range);
    case CSSPropertyWebkitMarqueeSpeed:
        return consumeWebkitMarqueeSpeed(m_range, m_context.mode);
    case CSSPropertyAlt:
        return consumeAlt(m_range, m_context);
    case CSSPropertyAspectRatio:
        return consumeAspectRatio(m_range);
    case CSSPropertyContain:
        return consumeContain(m_range);
    case CSSPropertyContentVisibility:
        return consumeContentVisibility(m_range);
    case CSSPropertyTextEmphasisPosition:
        return consumeTextEmphasisPosition(m_range);
#if ENABLE(DARK_MODE_CSS)
    case CSSPropertyColorScheme:
        return consumeColorScheme(m_range);
#endif
    case CSSPropertyListStyleType:
        // All the keyword values for the list-style-type property are handled by the CSSParserFastPaths.
        return consumeString(m_range);
    case CSSPropertyContainerName:
        return consumeContainerName(m_range);
    case CSSPropertyContainIntrinsicHeight:
    case CSSPropertyContainIntrinsicWidth:
    case CSSPropertyContainIntrinsicBlockSize:
    case CSSPropertyContainIntrinsicInlineSize:
        return consumeContainIntrinsicSize(m_range);
    case CSSPropertyOverflowAnchor:
        return consumeOverflowAnchor(m_range);
    default:
        return nullptr;
    }
}

bool CSSPropertyParser::canParseTypedCustomPropertyValue(const String& syntax)
{
    if (syntax != "*"_s) {
        m_range.consumeWhitespace();

        // First check for keywords
        if (isCSSWideKeyword(m_range.peek().id()))
            return true;

        auto localRange = m_range;
        while (!localRange.atEnd()) {
            auto id = localRange.consume().functionId();
            if (id == CSSValueVar || id == CSSValueEnv)
                return true; // For variables, we just permit everything
        }

        auto primitiveVal = consumeWidthOrHeight(m_range, m_context);
        if (primitiveVal && primitiveVal->isPrimitiveValue() && m_range.atEnd())
            return true;
        return false;
    }

    return true;
}

void CSSPropertyParser::collectParsedCustomPropertyValueDependencies(const String& syntax, bool isRoot, HashSet<CSSPropertyID>& dependencies)
{
    if (syntax != "*"_s) {
        m_range.consumeWhitespace();
        auto primitiveVal = consumeWidthOrHeight(m_range, m_context);
        if (!m_range.atEnd())
            return;
        if (primitiveVal && primitiveVal->isPrimitiveValue()) {
            primitiveVal->collectDirectComputationalDependencies(dependencies);
            if (isRoot)
                primitiveVal->collectDirectRootComputationalDependencies(dependencies);
        }
    }
}

RefPtr<CSSCustomPropertyValue> CSSPropertyParser::parseTypedCustomPropertyValue(const AtomString& name, const String& syntax, const Style::BuilderState& builderState)
{
    if (syntax != "*"_s) {
        m_range.consumeWhitespace();
        auto primitiveVal = consumeWidthOrHeight(m_range, m_context);
        if (primitiveVal && primitiveVal->isPrimitiveValue() && downcast<CSSPrimitiveValue>(*primitiveVal).isLength()) {
            auto length = Style::BuilderConverter::convertLength(builderState, *primitiveVal);
            if (!length.isCalculated() && !length.isUndefined())
                return CSSCustomPropertyValue::createSyntaxLength(name, WTFMove(length));
        }
    } else {
        auto propertyValue = CSSCustomPropertyValue::createSyntaxAll(name, CSSVariableData::create(m_range));
        while (!m_range.atEnd())
            m_range.consume();
        return { WTFMove(propertyValue) };
    }

    return nullptr;
}

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-system
static RefPtr<CSSPrimitiveValue> consumeCounterStyleSystem(CSSParserTokenRange& range)
{
    if (auto ident = consumeIdent<CSSValueCyclic, CSSValueNumeric, CSSValueAlphabetic, CSSValueSymbolic, CSSValueAdditive>(range))
        return ident;

    if (auto ident = consumeIdent<CSSValueFixed>(range)) {
        if (range.atEnd())
            return ident;
        // If we have the `fixed` keyword but the range is not at the end, the next token must be a integer.
        // If it's not, this value is invalid.
        auto firstSymbolValue = consumeInteger(range);
        if (!firstSymbolValue)
            return nullptr;
        return createPrimitiveValuePair(ident.releaseNonNull(), firstSymbolValue.releaseNonNull());
    }

    if (auto ident = consumeIdent<CSSValueExtends>(range)) {
        // There must be a `<counter-style-name>` following the `extends` keyword. If there isn't, this value is invalid.
        auto parsedCounterStyleName = consumeCounterStyleName(range);
        if (!parsedCounterStyleName)
            return nullptr;
        return createPrimitiveValuePair(ident.releaseNonNull(), parsedCounterStyleName.releaseNonNull());
    }
    return nullptr;
}

// https://www.w3.org/TR/css-counter-styles-3/#typedef-symbol
static RefPtr<CSSValue> consumeCounterStyleSymbol(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (auto string = consumeString(range))
        return string;
    if (auto customIdent = consumeCustomIdent(range))
        return customIdent;
    // There are inherent difficulties in supporting <image> symbols in @counter-styles, so gate them behind a
    // flag for now. https://bugs.webkit.org/show_bug.cgi?id=167645
    if (context.counterStyleAtRuleImageSymbolsEnabled) {
        if (auto image = consumeImage(range, context, { AllowedImageType::URLFunction, AllowedImageType::GeneratedImage }))
            return image;
    }
    return nullptr;
}

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-negative
static RefPtr<CSSValue> consumeCounterStyleNegative(CSSParserTokenRange& range, const CSSParserContext& context)
{
    auto prependValue = consumeCounterStyleSymbol(range, context);
    if (!prependValue)
        return nullptr;
    if (range.atEnd())
        return prependValue;

    auto appendValue = consumeCounterStyleSymbol(range, context);
    if (!appendValue || !range.atEnd())
        return nullptr;

    RefPtr<CSSValueList> values = CSSValueList::createSpaceSeparated();
    values->append(prependValue.releaseNonNull());
    values->append(appendValue.releaseNonNull());
    return values;
}

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-range
static RefPtr<CSSPrimitiveValue> consumeCounterStyleRangeBound(CSSParserTokenRange& range)
{
    if (auto infinite = consumeIdent<CSSValueInfinite>(range))
        return infinite;
    if (auto integer = consumeInteger(range))
        return integer;
    return nullptr;
}

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-range
static RefPtr<CSSValue> consumeCounterStyleRange(CSSParserTokenRange& range)
{
    if (auto autoValue = consumeIdent<CSSValueAuto>(range))
        return autoValue;

    auto rangeList = CSSValueList::createCommaSeparated();
    do {
        auto lowerBound = consumeCounterStyleRangeBound(range);
        if (!lowerBound)
            return nullptr;
        auto upperBound = consumeCounterStyleRangeBound(range);
        if (!upperBound)
            return nullptr;

        // If the lower bound of any range is higher than the upper bound, the entire descriptor is invalid and must be
        // ignored.
        if (lowerBound->isInteger() && upperBound->isInteger() && lowerBound->intValue() > upperBound->intValue())
            return nullptr;
        rangeList->append(createPrimitiveValuePair(lowerBound.releaseNonNull(), upperBound.releaseNonNull(), Pair::IdenticalValueEncoding::DoNotCoalesce));
    } while (consumeCommaIncludingWhitespace(range));
    if (!range.atEnd() || !rangeList->length())
        return nullptr;
    return rangeList;
}

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-pad
static RefPtr<CSSValue> consumeCounterStylePad(CSSParserTokenRange& range, const CSSParserContext& context)
{
    RefPtr<CSSValue> integer;
    RefPtr<CSSValue> symbol;
    while (!integer || !symbol) {
        if (!integer) {
            integer = consumeIntegerZeroAndGreater(range);
            if (integer)
                continue;
        }
        if (!symbol) {
            symbol = consumeCounterStyleSymbol(range, context);
            if (symbol)
                continue;
        }
        return nullptr;
    }
    if (!range.atEnd())
        return nullptr;
    auto values = CSSValueList::createSpaceSeparated();
    values->append(integer.releaseNonNull());
    values->append(symbol.releaseNonNull());
    return values;
}

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-symbols
static RefPtr<CSSValue> consumeCounterStyleSymbols(CSSParserTokenRange& range, const CSSParserContext& context)
{
    auto symbols = CSSValueList::createSpaceSeparated();
    while (!range.atEnd()) {
        auto symbol = consumeCounterStyleSymbol(range, context);
        if (!symbol)
            return nullptr;
        symbols->append(symbol.releaseNonNull());
    }
    if (!symbols->length())
        return nullptr;
    return symbols;
}

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-symbols
static RefPtr<CSSValue> consumeCounterStyleAdditiveSymbols(CSSParserTokenRange& range, const CSSParserContext& context)
{
    auto values = CSSValueList::createCommaSeparated();
    std::optional<int> lastWeight;
    do {
        auto integer = consumeIntegerZeroAndGreater(range);
        auto symbol = consumeCounterStyleSymbol(range, context);
        if (!integer) {
            if (!symbol)
                return nullptr;
            integer = consumeIntegerZeroAndGreater(range);
            if (!integer)
                return nullptr;
        }

        // Additive tuples must be specified in order of strictly descending weight.
        auto weight = integer->intValue();
        if (lastWeight && !(weight < lastWeight))
            return nullptr;
        lastWeight = weight;

        auto pair = CSSValueList::createSpaceSeparated();
        pair->append(integer.releaseNonNull());
        pair->append(symbol.releaseNonNull());
        values->append(WTFMove(pair));
    } while (consumeCommaIncludingWhitespace(range));
    if (!range.atEnd() || !values->length())
        return nullptr;
    return values;
}

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-speak-as
static RefPtr<CSSValue> consumeCounterStyleSpeakAs(CSSParserTokenRange& range)
{
    if (auto speakAsIdent = consumeIdent<CSSValueAuto, CSSValueBullets, CSSValueNumbers, CSSValueWords, CSSValueSpellOut>(range))
        return speakAsIdent;
    return consumeCounterStyleName(range);
}

RefPtr<CSSValue> CSSPropertyParser::parseCounterStyleDescriptor(CSSPropertyID propId, CSSParserTokenRange& range, const CSSParserContext& context)
{
    ASSERT(context.propertySettings.cssCounterStyleAtRulesEnabled);
    ASSERT(isCSSPropertyExposed(propId, &context.propertySettings));

    switch (propId) {
    case CSSPropertySystem:
        return consumeCounterStyleSystem(range);
    case CSSPropertyNegative:
        return consumeCounterStyleNegative(range, context);
    case CSSPropertyPrefix:
    case CSSPropertySuffix:
        return consumeCounterStyleSymbol(range, context);
    case CSSPropertyRange:
        return consumeCounterStyleRange(range);
    case CSSPropertyPad:
        return consumeCounterStylePad(range, context);
    case CSSPropertyFallback:
        return consumeCounterStyleName(range);
    case CSSPropertySymbols:
        return consumeCounterStyleSymbols(range, context);
    case CSSPropertyAdditiveSymbols:
        return consumeCounterStyleAdditiveSymbols(range, context);
    case CSSPropertySpeakAs:
        return consumeCounterStyleSpeakAs(range);
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

bool CSSPropertyParser::parseCounterStyleDescriptor(CSSPropertyID propId, const CSSParserContext& context)
{
    auto parsedValue = parseCounterStyleDescriptor(propId, m_range, context);
    if (!parsedValue || !m_range.atEnd())
        return false;

    addProperty(propId, CSSPropertyInvalid, *parsedValue, false);
    return true;
}

bool CSSPropertyParser::parseFontFaceDescriptor(CSSPropertyID propId)
{
    ASSERT(isCSSPropertyExposed(propId, &m_context.propertySettings));

    RefPtr<CSSValue> parsedValue;
    switch (propId) {
    case CSSPropertyFontFamily:
        parsedValue = consumeFontFamilyDescriptor(m_range);
        break;
    case CSSPropertySrc: // This is a list of urls or local references.
        parsedValue = consumeFontFaceSrc(m_range, m_context);
        break;
    case CSSPropertyUnicodeRange:
        parsedValue = consumeFontFaceUnicodeRange(m_range);
        break;
    case CSSPropertyFontDisplay:
        parsedValue = consumeFontFaceFontDisplay(m_range, CSSValuePool::singleton());
        break;
    case CSSPropertyFontWeight:
#if ENABLE(VARIATION_FONTS)
        parsedValue = consumeFontWeightAbsoluteRange(m_range, CSSValuePool::singleton());
#else
        parsedValue = consumeFontWeightAbsolute(m_range, CSSValuePool::singleton());
#endif
        break;
    case CSSPropertyFontStretch:
#if ENABLE(VARIATION_FONTS)
        parsedValue = consumeFontStretchRange(m_range, CSSValuePool::singleton());
#else
        parsedValue = consumeFontStretch(m_range, CSSValuePool::singleton());
#endif
        break;
    case CSSPropertyFontStyle:
#if ENABLE(VARIATION_FONTS)
        parsedValue = consumeFontStyleRange(m_range, m_context.mode, CSSValuePool::singleton());
#else
        parsedValue = consumeFontStyle(m_range, m_context.mode, CSSValuePool::singleton());
#endif
        break;
    case CSSPropertyFontVariantCaps:
        parsedValue = consumeFontVariantCaps(m_range);
        break;
    case CSSPropertyFontVariantLigatures:
        parsedValue = consumeFontVariantLigatures(m_range);
        break;
    case CSSPropertyFontVariantNumeric:
        parsedValue = consumeFontVariantNumeric(m_range);
        break;
    case CSSPropertyFontVariantEastAsian:
        parsedValue = consumeFontVariantEastAsian(m_range);
        break;
    case CSSPropertyFontVariantAlternates:
        parsedValue = consumeFontVariantAlternates(m_range);
        break;
    case CSSPropertyFontVariantPosition:
        parsedValue = consumeFontVariantPosition(m_range);
        break;
    case CSSPropertyFontVariant:
        return consumeFontVariantShorthand(false);
    case CSSPropertyFontFeatureSettings:
        parsedValue = consumeFontFeatureSettings(m_range, CSSValuePool::singleton());
        break;
    default:
        break;
    }

    if (!parsedValue || !m_range.atEnd())
        return false;

    addProperty(propId, CSSPropertyInvalid, *parsedValue, false);
    return true;
}

bool CSSPropertyParser::parseKeyframeDescriptor(CSSPropertyID propertyID, bool important)
{
    // https://www.w3.org/TR/css-animations-1/#keyframes
    // The <declaration-list> inside of <keyframe-block> accepts any CSS property except those
    // defined in this specification, but does accept the animation-timing-function property and
    // interprets it specially.
    switch (propertyID) {
    case CSSPropertyAnimation:
    case CSSPropertyAnimationDelay:
    case CSSPropertyAnimationDirection:
    case CSSPropertyAnimationDuration:
    case CSSPropertyAnimationFillMode:
    case CSSPropertyAnimationIterationCount:
    case CSSPropertyAnimationName:
    case CSSPropertyAnimationPlayState:
        return false;
    default:
        return parseValueStart(propertyID, important);
    }
}

static RefPtr<CSSPrimitiveValue> consumeBasePaletteDescriptor(CSSParserTokenRange& range)
{
    if (auto result = consumeIdent<CSSValueLight, CSSValueDark>(range))
        return result;
    return consumeIntegerZeroAndGreater(range);
}

static RefPtr<CSSValueList> consumeOverrideColorsDescriptor(CSSParserTokenRange& range, const CSSParserContext& context)
{
    RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    do {
        auto key = consumeIntegerZeroAndGreater(range);
        if (!key)
            return nullptr;

        auto color = consumeColor(range, context, false, { StyleColor::CSSColorType::Absolute });
        if (!color)
            return nullptr;

        RefPtr<CSSValue> value = CSSFontPaletteValuesOverrideColorsValue::create(key.releaseNonNull(), color.releaseNonNull());
        list->append(value.releaseNonNull());
    } while (consumeCommaIncludingWhitespace(range));
    
    if (!range.atEnd() || !list->length())
        return nullptr;

    return list;
}

bool CSSPropertyParser::parseFontPaletteValuesDescriptor(CSSPropertyID propId)
{
    ASSERT(isCSSPropertyExposed(propId, &m_context.propertySettings));

    RefPtr<CSSValue> parsedValue;
    switch (propId) {
    case CSSPropertyFontFamily:
        parsedValue = consumeFamilyName(m_range);
        break;
    case CSSPropertyBasePalette:
        parsedValue = consumeBasePaletteDescriptor(m_range);
        break;
    case CSSPropertyOverrideColors:
        parsedValue = consumeOverrideColorsDescriptor(m_range, m_context);
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
    ASSERT(CSSPropertyParserHelpers::isSystemFontShorthand(systemFontID));
    if (!m_range.atEnd())
        return false;

    // It's illegal to look up properties (weight, size, etc.) of the system font here,
    // because those values can change (e.g. accessibility font sizes, or accessibility bold).
    // Parsing (correctly) doesn't re-run in response to updateStyleAfterChangeInEnvironment().
    // Instead, we stuff sentinel values into the outputted CSSValues, which are later replaced by
    // real system font values inside Style::BuilderCustom and Style::BuilderConverter.
    
    addProperty(CSSPropertyFontStyle, CSSPropertyFont, CSSValuePool::singleton().createIdentifierValue(systemFontID), important);
    addProperty(CSSPropertyFontWeight, CSSPropertyFont, CSSValuePool::singleton().createIdentifierValue(systemFontID), important);
    addProperty(CSSPropertyFontSize, CSSPropertyFont, CSSValuePool::singleton().createIdentifierValue(systemFontID), important);
    addProperty(CSSPropertyFontFamily, CSSPropertyFont, CSSValuePool::singleton().createIdentifierValue(systemFontID), important);
    addProperty(CSSPropertyFontVariantCaps, CSSPropertyFont, CSSValuePool::singleton().createIdentifierValue(systemFontID), important);
    addProperty(CSSPropertyLineHeight, CSSPropertyFont, CSSValuePool::singleton().createIdentifierValue(systemFontID), important);

    // FIXME_NEWPARSER: What about FontVariantNumeric and FontVariantLigatures?

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
    RefPtr<CSSFontStyleValue> fontStyle;
    RefPtr<CSSPrimitiveValue> fontVariantCaps;
    RefPtr<CSSPrimitiveValue> fontWeight;
    RefPtr<CSSPrimitiveValue> fontStretch;

    while (!m_range.atEnd()) {
        CSSValueID id = m_range.peek().id();
        if (!fontStyle) {
            fontStyle = consumeFontStyle(m_range, m_context.mode, CSSValuePool::singleton());
            if (fontStyle)
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
        if (!fontStretch) {
            fontStretch = consumeFontStretchKeywordValue(m_range, CSSValuePool::singleton());
            if (fontStretch)
                continue;
        }
        break;
    }

    if (m_range.atEnd())
        return false;

    auto& valuePool = CSSValuePool::singleton();

    addPropertyWithImplicitDefault(CSSPropertyFontStyle, CSSPropertyFont, fontStyle, CSSFontStyleValue::create(valuePool.createIdentifierValue(CSSValueNormal)), important);
    addPropertyWithImplicitDefault(CSSPropertyFontVariantCaps, CSSPropertyFont, fontVariantCaps, valuePool.createIdentifierValue(CSSValueNormal), important);
/*  
    // FIXME-NEWPARSER: What do we do with these? They aren't part of our fontShorthand().
    addProperty(CSSPropertyFontVariantLigatures, CSSPropertyFont, CSSValuePool::singleton().createIdentifierValue(CSSValueNormal), important, true);
    addProperty(CSSPropertyFontVariantNumeric, CSSPropertyFont, CSSValuePool::singleton().createIdentifierValue(CSSValueNormal), important, true);
*/

    addPropertyWithImplicitDefault(CSSPropertyFontWeight, CSSPropertyFont, fontWeight, valuePool.createIdentifierValue(CSSValueNormal), important);
    addPropertyWithImplicitDefault(CSSPropertyFontStretch, CSSPropertyFont, fontStretch, valuePool.createIdentifierValue(CSSValueNormal), important);

    // Now a font size _must_ come.
    RefPtr<CSSValue> fontSize = consumeFontSize(m_range, m_context.mode);
    if (!fontSize || m_range.atEnd())
        return false;

    addProperty(CSSPropertyFontSize, CSSPropertyFont, fontSize.releaseNonNull(), important);

    RefPtr<CSSPrimitiveValue> lineHeight;
    if (consumeSlashIncludingWhitespace(m_range)) {
        lineHeight = consumeLineHeight(m_range, m_context.mode);
        if (!lineHeight)
            return false;
    }
    addPropertyWithImplicitDefault(CSSPropertyLineHeight, CSSPropertyFont, lineHeight, valuePool.createIdentifierValue(CSSValueNormal), important);

    // Font family must come now.
    RefPtr<CSSValue> parsedFamilyValue = consumeFontFamily(m_range);
    if (!parsedFamilyValue)
        return false;

    addProperty(CSSPropertyFontFamily, CSSPropertyFont, parsedFamilyValue.releaseNonNull(), important);

    return m_range.atEnd();
}

bool CSSPropertyParser::consumeFontVariantShorthand(bool important)
{
    if (identMatches<CSSValueNormal, CSSValueNone>(m_range.peek().id())) {
        addProperty(CSSPropertyFontVariantLigatures, CSSPropertyFontVariant, consumeIdent(m_range).releaseNonNull(), important);
        addProperty(CSSPropertyFontVariantNumeric, CSSPropertyFontVariant, CSSValuePool::singleton().createIdentifierValue(CSSValueNormal), important, true);
        addProperty(CSSPropertyFontVariantCaps, CSSPropertyFontVariant, CSSValuePool::singleton().createIdentifierValue(CSSValueNormal), important, true);
        addProperty(CSSPropertyFontVariantAlternates, CSSPropertyFontVariant, CSSValuePool::singleton().createIdentifierValue(CSSValueNormal), important, true);
        addProperty(CSSPropertyFontVariantEastAsian, CSSPropertyFontVariant, CSSValuePool::singleton().createIdentifierValue(CSSValueNormal), important, true);
        addProperty(CSSPropertyFontVariantPosition, CSSPropertyFontVariant, CSSValuePool::singleton().createIdentifierValue(CSSValueNormal), important, true);
        return m_range.atEnd();
    }

    RefPtr<CSSPrimitiveValue> capsValue;
    RefPtr<CSSValue> alternatesValue;
    RefPtr<CSSPrimitiveValue> positionValue;

    RefPtr<CSSValue> eastAsianValue;
    FontVariantLigaturesParser ligaturesParser;
    FontVariantNumericParser numericParser;
    bool implicitLigatures = true;
    bool implicitNumeric = true;
    do {
        if (!capsValue) {
            capsValue = consumeFontVariantCaps(m_range);
            if (capsValue)
                continue;
        }
        
        if (!positionValue) {
            positionValue = consumeFontVariantPosition(m_range);
            if (positionValue)
                continue;
        }

        if (!alternatesValue) {
            alternatesValue = consumeFontVariantAlternates(m_range);
            if (alternatesValue)
                continue;
        }

        FontVariantLigaturesParser::ParseResult ligaturesParseResult = ligaturesParser.consumeLigature(m_range);
        FontVariantNumericParser::ParseResult numericParseResult = numericParser.consumeNumeric(m_range);
        if (ligaturesParseResult == FontVariantLigaturesParser::ParseResult::ConsumedValue) {
            implicitLigatures = false;
            continue;
        }
        if (numericParseResult == FontVariantNumericParser::ParseResult::ConsumedValue) {
            implicitNumeric = false;
            continue;
        }

        if (ligaturesParseResult == FontVariantLigaturesParser::ParseResult::DisallowedValue
            || numericParseResult == FontVariantNumericParser::ParseResult::DisallowedValue)
            return false;

        if (!eastAsianValue) {
            eastAsianValue = consumeFontVariantEastAsian(m_range);
            if (eastAsianValue)
                continue;
        }

        // Saw some value that didn't match anything else.
        return false;

    } while (!m_range.atEnd());

    addProperty(CSSPropertyFontVariantLigatures, CSSPropertyFontVariant, ligaturesParser.finalizeValue().releaseNonNull(), important, implicitLigatures);
    addProperty(CSSPropertyFontVariantNumeric, CSSPropertyFontVariant, numericParser.finalizeValue().releaseNonNull(), important, implicitNumeric);

    auto& valuePool = CSSValuePool::singleton();
    addPropertyWithImplicitDefault(CSSPropertyFontVariantCaps, CSSPropertyFontVariant, capsValue, valuePool.createIdentifierValue(CSSValueNormal), important);
    addPropertyWithImplicitDefault(CSSPropertyFontVariantAlternates, CSSPropertyFontVariant, WTFMove(alternatesValue), valuePool.createIdentifierValue(CSSValueNormal), important);
    addPropertyWithImplicitDefault(CSSPropertyFontVariantPosition, CSSPropertyFontVariant, positionValue, valuePool.createIdentifierValue(CSSValueNormal), important);
    addPropertyWithImplicitDefault(CSSPropertyFontVariantEastAsian, CSSPropertyFontVariant, WTFMove(eastAsianValue), valuePool.createIdentifierValue(CSSValueNormal), important);

    return true;
}

bool CSSPropertyParser::consumeBorderSpacing(bool important)
{
    RefPtr<CSSValue> horizontalSpacing = consumeLength(m_range, m_context.mode, ValueRange::NonNegative, UnitlessQuirk::Allow);
    if (!horizontalSpacing)
        return false;
    RefPtr<CSSValue> verticalSpacing = horizontalSpacing;
    if (!m_range.atEnd())
        verticalSpacing = consumeLength(m_range, m_context.mode, ValueRange::NonNegative, UnitlessQuirk::Allow);
    if (!verticalSpacing || !m_range.atEnd())
        return false;
    addProperty(CSSPropertyWebkitBorderHorizontalSpacing, CSSPropertyBorderSpacing, horizontalSpacing.releaseNonNull(), important);
    addProperty(CSSPropertyWebkitBorderVerticalSpacing, CSSPropertyBorderSpacing, verticalSpacing.releaseNonNull(), important);
    return true;
}

bool CSSPropertyParser::consumeColumns(bool important)
{
    RefPtr<CSSValue> columnWidth;
    RefPtr<CSSValue> columnCount;
    bool hasPendingExplicitAuto = false;
    
    for (unsigned propertiesParsed = 0; propertiesParsed < 2 && !m_range.atEnd(); ++propertiesParsed) {
        if (!propertiesParsed && m_range.peek().id() == CSSValueAuto) {
            // 'auto' is a valid value for any of the two longhands, and at this point
            // we don't know which one(s) it is meant for. We need to see if there are other values first.
            consumeIdent(m_range);
            hasPendingExplicitAuto = true;
        } else {
            if (!columnWidth) {
                if ((columnWidth = consumeColumnWidth(m_range)))
                    continue;
            }
            if (!columnCount) {
                if ((columnCount = consumeColumnCount(m_range)))
                    continue;
            }
            // If we didn't find at least one match, this is an invalid shorthand and we have to ignore it.
            return false;
        }
    }
    
    if (!m_range.atEnd())
        return false;

    // Any unassigned property at this point will become implicit 'auto'.
    if (columnWidth)
        addProperty(CSSPropertyColumnWidth, CSSPropertyInvalid, columnWidth.releaseNonNull(), important);
    else {
        addProperty(CSSPropertyColumnWidth, CSSPropertyInvalid, CSSValuePool::singleton().createIdentifierValue(CSSValueAuto), important, !hasPendingExplicitAuto /* implicit */);
        hasPendingExplicitAuto = false;
    }
    
    if (columnCount)
        addProperty(CSSPropertyColumnCount, CSSPropertyInvalid, columnCount.releaseNonNull(), important);
    else
        addProperty(CSSPropertyColumnCount, CSSPropertyInvalid, CSSValuePool::singleton().createIdentifierValue(CSSValueAuto), important, !hasPendingExplicitAuto /* implicit */);
    
    return true;
}

bool CSSPropertyParser::consumeShorthandGreedily(const StylePropertyShorthand& shorthand, bool important)
{
    ASSERT(shorthand.length() <= 6); // Existing shorthands have at most 6 longhands.
    RefPtr<CSSValue> longhands[6];
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
        addPropertyWithImplicitDefault(shorthandProperties[i], shorthand.id(), WTFMove(longhands[i]), CSSValuePool::singleton().createImplicitInitialValue(), important);
    }
    return true;
}

bool CSSPropertyParser::consumeFlex(bool important)
{
    static const double unsetValue = -1;
    double flexGrow = unsetValue;
    double flexShrink = unsetValue;
    RefPtr<CSSPrimitiveValue> flexBasis;

    if (m_range.peek().id() == CSSValueNone) {
        flexGrow = 0;
        flexShrink = 0;
        flexBasis = CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
        m_range.consumeIncludingWhitespace();
    } else {
        unsigned index = 0;
        while (!m_range.atEnd() && index++ < 3) {
            if (auto number = consumeNumberRaw(m_range)) {
                if (number->value < 0)
                    return false;
                if (flexGrow == unsetValue)
                    flexGrow = number->value;
                else if (flexShrink == unsetValue)
                    flexShrink = number->value;
                else if (!number->value) // flex only allows a basis of 0 (sans units) if flex-grow and flex-shrink values have already been set.
                    flexBasis = CSSPrimitiveValue::create(0, CSSUnitType::CSS_PX);
                else
                    return false;
            } else if (!flexBasis) {
                if (isFlexBasisIdent(m_range.peek().id(), m_context))
                    flexBasis = consumeIdent(m_range);
                if (!flexBasis)
                    flexBasis = consumeLengthOrPercent(m_range, m_context.mode, ValueRange::NonNegative);
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
        
        // FIXME: Using % here is a hack to work around intrinsic sizing implementation being
        // a mess (e.g., turned off for nested column flexboxes, failing to relayout properly even
        // if turned back on for nested columns, etc.). We have layout test coverage of both
        // scenarios.
        if (!flexBasis)
            flexBasis = CSSPrimitiveValue::create(0, CSSUnitType::CSS_PERCENTAGE);
    }

    if (!m_range.atEnd())
        return false;
    addProperty(CSSPropertyFlexGrow, CSSPropertyFlex, CSSPrimitiveValue::create(clampTo<float>(flexGrow), CSSUnitType::CSS_NUMBER), important);
    addProperty(CSSPropertyFlexShrink, CSSPropertyFlex, CSSPrimitiveValue::create(clampTo<float>(flexShrink), CSSUnitType::CSS_NUMBER), important);
    addProperty(CSSPropertyFlexBasis, CSSPropertyFlex, flexBasis.releaseNonNull(), important);
    return true;
}

bool CSSPropertyParser::consumeBorder(RefPtr<CSSValue>& width, RefPtr<CSSValue>& style, RefPtr<CSSValue>& color)
{
    while (!width || !style || !color) {
        if (!width) {
            width = consumeLineWidth(m_range, m_context.mode, UnitlessQuirk::Forbid);
            if (width)
                continue;
        }
        if (!style) {
            style = parseSingleValue(CSSPropertyBorderLeftStyle, CSSPropertyBorder);
            if (style)
                continue;
        }
        if (!color) {
            color = consumeColor(m_range, m_context);
            if (color)
                continue;
        }
        break;
    }

    if (!width && !style && !color)
        return false;

    if (!width)
        width = CSSValuePool::singleton().createImplicitInitialValue();
    if (!style)
        style = CSSValuePool::singleton().createImplicitInitialValue();
    if (!color)
        color = CSSValuePool::singleton().createImplicitInitialValue();

    return m_range.atEnd();
}

bool CSSPropertyParser::consume2ValueShorthand(const StylePropertyShorthand& shorthand, bool important)
{
    ASSERT(shorthand.length() == 2);
    const CSSPropertyID* longhands = shorthand.properties();
    RefPtr<CSSValue> start = parseSingleValue(longhands[0], shorthand.id());
    if (!start)
        return false;

    RefPtr<CSSValue> end = parseSingleValue(longhands[1], shorthand.id());
    bool endImplicit = !end;
    if (endImplicit)
        end = start;
    addProperty(longhands[0], shorthand.id(), start.releaseNonNull(), important);
    addProperty(longhands[1], shorthand.id(), end.releaseNonNull(), important, endImplicit);

    return m_range.atEnd();
}

bool CSSPropertyParser::consume4ValueShorthand(const StylePropertyShorthand& shorthand, bool important)
{
    ASSERT(shorthand.length() == 4);
    const CSSPropertyID* longhands = shorthand.properties();
    RefPtr<CSSValue> top = parseSingleValue(longhands[0], shorthand.id());
    if (!top)
        return false;

    RefPtr<CSSValue> right = parseSingleValue(longhands[1], shorthand.id());
    RefPtr<CSSValue> bottom;
    RefPtr<CSSValue> left;
    if (right) {
        bottom = parseSingleValue(longhands[2], shorthand.id());
        if (bottom)
            left = parseSingleValue(longhands[3], shorthand.id());
    }

    bool rightImplicit = !right;
    bool bottomImplicit = !bottom;
    bool leftImplicit = !left;

    if (!right)
        right = top;
    if (!bottom)
        bottom = top;
    if (!left)
        left = right;

    addProperty(longhands[0], shorthand.id(), top.releaseNonNull(), important);
    addProperty(longhands[1], shorthand.id(), right.releaseNonNull(), important, rightImplicit);
    addProperty(longhands[2], shorthand.id(), bottom.releaseNonNull(), important, bottomImplicit);
    addProperty(longhands[3], shorthand.id(), left.releaseNonNull(), important, leftImplicit);

    return m_range.atEnd();
}

bool CSSPropertyParser::consumeBorderImage(CSSPropertyID property, bool important)
{
    RefPtr<CSSValue> source;
    RefPtr<CSSValue> slice;
    RefPtr<CSSValue> width;
    RefPtr<CSSValue> outset;
    RefPtr<CSSValue> repeat;
    
    if (consumeBorderImageComponents(property, m_range, m_context, source, slice, width, outset, repeat)) {
        auto& valuePool = CSSValuePool::singleton();
        switch (property) {
        case CSSPropertyWebkitMaskBoxImage:
            addPropertyWithImplicitDefault(CSSPropertyWebkitMaskBoxImageSource, property, WTFMove(source), valuePool.createImplicitInitialValue(), important);
            addPropertyWithImplicitDefault(CSSPropertyWebkitMaskBoxImageSlice, property, WTFMove(slice), valuePool.createImplicitInitialValue(), important);
            addPropertyWithImplicitDefault(CSSPropertyWebkitMaskBoxImageWidth, property, WTFMove(width), valuePool.createImplicitInitialValue(), important);
            addPropertyWithImplicitDefault(CSSPropertyWebkitMaskBoxImageOutset, property, WTFMove(outset), valuePool.createImplicitInitialValue(), important);
            addPropertyWithImplicitDefault(CSSPropertyWebkitMaskBoxImageRepeat, property, WTFMove(repeat), valuePool.createImplicitInitialValue(), important);
            return true;
        case CSSPropertyBorderImage:
        case CSSPropertyWebkitBorderImage:
            addPropertyWithImplicitDefault(CSSPropertyBorderImageSource, property, WTFMove(source), valuePool.createImplicitInitialValue(), important);
            addPropertyWithImplicitDefault(CSSPropertyBorderImageSlice, property, WTFMove(slice), valuePool.createImplicitInitialValue(), important);
            addPropertyWithImplicitDefault(CSSPropertyBorderImageWidth, property, WTFMove(width), valuePool.createImplicitInitialValue(), important);
            addPropertyWithImplicitDefault(CSSPropertyBorderImageOutset, property, WTFMove(outset), valuePool.createImplicitInitialValue(), important);
            addPropertyWithImplicitDefault(CSSPropertyBorderImageRepeat, property, WTFMove(repeat), valuePool.createImplicitInitialValue(), important);
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
    if (value == CSSValueAuto)
        return value;
    if (value == CSSValueAvoid)
        return CSSValueAvoidColumn;
    return CSSValueInvalid;
}

static inline CSSValueID mapFromColumnRegionOrPageBreakInside(CSSValueID value)
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
    RefPtr<CSSPrimitiveValue> keyword = consumeIdent(m_range);
    if (!keyword)
        return false;
    if (!m_range.atEnd())
        return false;
    CSSValueID value = keyword->valueID();
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
        value = mapFromColumnRegionOrPageBreakInside(value);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    if (value == CSSValueInvalid)
        return false;

    CSSPropertyID genericBreakProperty = mapFromLegacyBreakProperty(property);
    addProperty(genericBreakProperty, property, CSSValuePool::singleton().createIdentifierValue(value), important);
    return true;
}

bool CSSPropertyParser::consumeLegacyTextOrientation(bool important)
{
    // -webkit-text-orientation is a legacy shorthand for text-orientation.
    // The only difference is that it accepts 'sideways-right', which is mapped into 'sideways'.
    RefPtr<CSSPrimitiveValue> keyword;
    auto valueID = m_range.peek().id();
    if (valueID == CSSValueSidewaysRight) {
        keyword = CSSValuePool::singleton().createIdentifierValue(CSSValueSideways);
        consumeIdentRaw(m_range);
    } else if (CSSParserFastPaths::isValidKeywordPropertyAndValue(CSSPropertyTextOrientation, valueID, m_context))
        keyword = consumeIdent(m_range);
    if (!keyword || !m_range.atEnd())
        return false;
    addProperty(CSSPropertyTextOrientation, CSSPropertyWebkitTextOrientation, keyword.releaseNonNull(), important);
    return true;
}

static bool consumeBackgroundPosition(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID property, RefPtr<CSSValue>& resultX, RefPtr<CSSValue>& resultY)
{
    do {
        auto position = consumePositionCoordinates(range, context.mode, UnitlessQuirk::Allow, property == CSSPropertyMaskPosition ? PositionSyntax::Position : PositionSyntax::BackgroundPosition, NegativePercentagePolicy::Allow);
        if (!position)
            return false;
        addBackgroundValue(resultX, WTFMove(position->x));
        addBackgroundValue(resultY, WTFMove(position->y));
    } while (consumeCommaIncludingWhitespace(range));
    return true;
}

// Note: consumeBackgroundShorthand assumes y properties (for example background-position-y) follow
// the x properties in the shorthand array.
bool CSSPropertyParser::consumeBackgroundShorthand(const StylePropertyShorthand& shorthand, bool important)
{
    const unsigned longhandCount = shorthand.length();
    RefPtr<CSSValue> longhands[10];
    ASSERT(longhandCount <= 10);

    bool implicit = false;
    do {
        bool parsedLonghand[10] = { false };
        RefPtr<CSSValue> originValue;
        do {
            bool foundProperty = false;
            for (size_t i = 0; i < longhandCount; ++i) {
                if (parsedLonghand[i])
                    continue;

                RefPtr<CSSValue> value;
                RefPtr<CSSValue> valueY;
                CSSPropertyID property = shorthand.properties()[i];
                if (property == CSSPropertyBackgroundPositionX || property == CSSPropertyWebkitMaskPositionX) {
                    CSSParserTokenRange rangeCopy = m_range;
                    auto position = consumePositionCoordinates(rangeCopy, m_context.mode, UnitlessQuirk::Forbid, PositionSyntax::BackgroundPosition);
                    if (!position)
                        continue;
                    value = WTFMove(position->x);
                    valueY = WTFMove(position->y);
                    m_range = rangeCopy;
                } else if (property == CSSPropertyBackgroundSize || property == CSSPropertyMaskSize) {
                    if (!consumeSlashIncludingWhitespace(m_range))
                        continue;
                    value = consumeBackgroundSize(property, m_range, m_context.mode);
                    if (!value || !parsedLonghand[i - 1]) // Position must have been parsed in the current layer.
                        return false;
                } else if (property == CSSPropertyBackgroundPositionY || property == CSSPropertyWebkitMaskPositionY) {
                    continue;
                } else {
                    value = consumeBackgroundComponent(property, m_range, m_context);
                }
                if (value) {
                    if (property == CSSPropertyBackgroundOrigin || property == CSSPropertyMaskOrigin)
                        originValue = value;
                    parsedLonghand[i] = true;
                    foundProperty = true;
                    addBackgroundValue(longhands[i], value.releaseNonNull());
                    if (valueY) {
                        parsedLonghand[i + 1] = true;
                        addBackgroundValue(longhands[i + 1], valueY.releaseNonNull());
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
            if ((property == CSSPropertyBackgroundClip || property == CSSPropertyMaskClip || property == CSSPropertyWebkitMaskClip) && !parsedLonghand[i] && originValue) {
                addBackgroundValue(longhands[i], originValue.releaseNonNull());
                continue;
            }
            if (!parsedLonghand[i])
                addBackgroundValue(longhands[i], CSSValuePool::singleton().createImplicitInitialValue());
        }
    } while (consumeCommaIncludingWhitespace(m_range));
    if (!m_range.atEnd())
        return false;

    for (size_t i = 0; i < longhandCount; ++i) {
        CSSPropertyID property = shorthand.properties()[i];
        if (property == CSSPropertyBackgroundSize && longhands[i] && m_context.useLegacyBackgroundSizeShorthandBehavior)
            continue;
        addProperty(property, shorthand.id(), *longhands[i], important, implicit);
    }
    return true;
}

bool CSSPropertyParser::consumeOverflowShorthand(bool important)
{
    CSSValueID xValueID = m_range.consumeIncludingWhitespace().id();
    if (!CSSParserFastPaths::isValidKeywordPropertyAndValue(CSSPropertyOverflowY, xValueID, m_context))
        return false;

    CSSValueID yValueID;
    if (m_range.atEnd()) {
        yValueID = xValueID;

        // FIXME: -webkit-paged-x or -webkit-paged-y only apply to overflow-y. If this value has been
        // set using the shorthand, then for now overflow-x will default to auto, but once we implement
        // pagination controls, it should default to hidden. If the overflow-y value is anything but
        // paged-x or paged-y, then overflow-x and overflow-y should have the same value.
        if (xValueID == CSSValueWebkitPagedX || xValueID == CSSValueWebkitPagedY)
            xValueID = CSSValueAuto;
    } else 
        yValueID = m_range.consumeIncludingWhitespace().id();

    if (!CSSParserFastPaths::isValidKeywordPropertyAndValue(CSSPropertyOverflowY, yValueID, m_context))
        return false;
    if (!m_range.atEnd())
        return false;

    addProperty(CSSPropertyOverflowX, CSSPropertyOverflow, CSSValuePool::singleton().createIdentifierValue(xValueID), important);
    addProperty(CSSPropertyOverflowY, CSSPropertyOverflow, CSSValuePool::singleton().createIdentifierValue(yValueID), important);
    return true;
}

static bool isCustomIdentValue(const CSSValue& value)
{
    return is<CSSPrimitiveValue>(value) && downcast<CSSPrimitiveValue>(value).isCustomIdent();
}

bool CSSPropertyParser::consumeGridItemPositionShorthand(CSSPropertyID shorthandId, bool important)
{
    const StylePropertyShorthand& shorthand = shorthandForProperty(shorthandId);
    ASSERT(shorthand.length() == 2);
    RefPtr<CSSValue> startValue = consumeGridLine(m_range);
    if (!startValue)
        return false;

    RefPtr<CSSValue> endValue;
    if (consumeSlashIncludingWhitespace(m_range)) {
        endValue = consumeGridLine(m_range);
        if (!endValue)
            return false;
    } else {
        endValue = isCustomIdentValue(*startValue) ? startValue : CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
    }
    if (!m_range.atEnd())
        return false;
    addProperty(shorthand.properties()[0], shorthandId, startValue.releaseNonNull(), important);
    addProperty(shorthand.properties()[1], shorthandId, endValue.releaseNonNull(), important);
    return true;
}

bool CSSPropertyParser::consumeGridAreaShorthand(bool important)
{
    RefPtr<CSSValue> rowStartValue = consumeGridLine(m_range);
    if (!rowStartValue)
        return false;
    RefPtr<CSSValue> columnStartValue;
    RefPtr<CSSValue> rowEndValue;
    RefPtr<CSSValue> columnEndValue;
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
        columnStartValue = isCustomIdentValue(*rowStartValue) ? rowStartValue : CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
    if (!rowEndValue)
        rowEndValue = isCustomIdentValue(*rowStartValue) ? rowStartValue : CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
    if (!columnEndValue)
        columnEndValue = isCustomIdentValue(*columnStartValue) ? columnStartValue : CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);

    addProperty(CSSPropertyGridRowStart, CSSPropertyGridArea, rowStartValue.releaseNonNull(), important);
    addProperty(CSSPropertyGridColumnStart, CSSPropertyGridArea, columnStartValue.releaseNonNull(), important);
    addProperty(CSSPropertyGridRowEnd, CSSPropertyGridArea, rowEndValue.releaseNonNull(), important);
    addProperty(CSSPropertyGridColumnEnd, CSSPropertyGridArea, columnEndValue.releaseNonNull(), important);
    return true;
}

bool CSSPropertyParser::consumeGridTemplateRowsAndAreasAndColumns(CSSPropertyID shorthandId, bool important)
{
    NamedGridAreaMap gridAreaMap;
    size_t rowCount = 0;
    size_t columnCount = 0;
    RefPtr<CSSValueList> templateRows = CSSValueList::createSpaceSeparated();

    // Persists between loop iterations so we can use the same value for
    // consecutive <line-names> values
    RefPtr<CSSGridLineNamesValue> lineNames;

    do {
        // Handle leading <custom-ident>*.
        bool hasPreviousLineNames = lineNames;
        lineNames = consumeGridLineNames(m_range, lineNames.get());
        if (lineNames && !hasPreviousLineNames)
            templateRows->append(lineNames.releaseNonNull());

        // Handle a template-area's row.
        if (m_range.peek().type() != StringToken || !parseGridTemplateAreasRow(m_range.consumeIncludingWhitespace().value(), gridAreaMap, rowCount, columnCount))
            return false;
        ++rowCount;

        // Handle template-rows's track-size.
        RefPtr<CSSValue> value = consumeGridTrackSize(m_range, m_context.mode);
        if (!value)
            value = CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
        templateRows->append(*value);

        // This will handle the trailing/leading <custom-ident>* in the grammar.
        lineNames = consumeGridLineNames(m_range);
        if (lineNames)
            templateRows->append(*lineNames);
    } while (!m_range.atEnd() && !(m_range.peek().type() == DelimiterToken && m_range.peek().delimiter() == '/'));

    RefPtr<CSSValue> columnsValue;
    if (!m_range.atEnd()) {
        if (!consumeSlashIncludingWhitespace(m_range))
            return false;
        columnsValue = consumeGridTrackList(m_range, m_context, GridTemplateNoRepeat);
        if (!columnsValue || !m_range.atEnd())
            return false;
    } else {
        columnsValue = CSSValuePool::singleton().createIdentifierValue(CSSValueNone);
    }
    addProperty(CSSPropertyGridTemplateRows, shorthandId, templateRows.releaseNonNull(), important);
    addProperty(CSSPropertyGridTemplateColumns, shorthandId, columnsValue.releaseNonNull(), important);
    addProperty(CSSPropertyGridTemplateAreas, shorthandId, CSSGridTemplateAreasValue::create(gridAreaMap, rowCount, columnCount), important);
    return true;
}

bool CSSPropertyParser::consumeGridTemplateShorthand(CSSPropertyID shorthandId, bool important)
{
    CSSParserTokenRange rangeCopy = m_range;
    RefPtr<CSSValue> rowsValue = consumeIdent<CSSValueNone>(m_range);

    // 1- 'none' case.
    if (rowsValue && m_range.atEnd()) {
        addProperty(CSSPropertyGridTemplateRows, shorthandId, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);
        addProperty(CSSPropertyGridTemplateColumns, shorthandId, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);
        addProperty(CSSPropertyGridTemplateAreas, shorthandId, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);
        return true;
    }

    // 2- <grid-template-rows> / <grid-template-columns>
    if (!rowsValue)
        rowsValue = consumeGridTrackList(m_range, m_context, GridTemplate);

    if (rowsValue) {
        if (!consumeSlashIncludingWhitespace(m_range))
            return false;
        RefPtr<CSSValue> columnsValue = consumeGridTemplatesRowsOrColumns(m_range, m_context);
        if (!columnsValue || !m_range.atEnd())
            return false;

        addProperty(CSSPropertyGridTemplateRows, shorthandId, rowsValue.releaseNonNull(), important);
        addProperty(CSSPropertyGridTemplateColumns, shorthandId, columnsValue.releaseNonNull(), important);
        addProperty(CSSPropertyGridTemplateAreas, shorthandId, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);
        return true;
    }

    // 3- [ <line-names>? <string> <track-size>? <line-names>? ]+ [ / <track-list> ]?
    m_range = rangeCopy;
    return consumeGridTemplateRowsAndAreasAndColumns(shorthandId, important);
}

static RefPtr<CSSValue> consumeImplicitGridAutoFlow(CSSParserTokenRange& range, Ref<CSSPrimitiveValue>&& flowDirection)
{
    // [ auto-flow && dense? ]
    if (range.atEnd())
        return nullptr;
    RefPtr<CSSValue> denseIdent;
    if (range.peek().id() == CSSValueAutoFlow) {
        range.consumeIncludingWhitespace();
        denseIdent = consumeIdent<CSSValueDense>(range);
    } else {
        // Dense case
        if (range.peek().id() != CSSValueDense)
            return nullptr;
        denseIdent = consumeIdent<CSSValueDense>(range);
        if (!denseIdent || !consumeIdent<CSSValueAutoFlow>(range))
            return nullptr;
    }
    auto list = CSSValueList::createSpaceSeparated();
    if (flowDirection->valueID() == CSSValueColumn || !denseIdent)
        list->append(WTFMove(flowDirection));
    if (denseIdent)
        list->append(denseIdent.releaseNonNull());
    
    return list;
}

bool CSSPropertyParser::consumeGridShorthand(bool important)
{
    ASSERT(shorthandForProperty(CSSPropertyGrid).length() == 6);

    CSSParserTokenRange rangeCopy = m_range;

    // 1- <grid-template>
    if (consumeGridTemplateShorthand(CSSPropertyGrid, important)) {
        // It can only be specified the explicit or the implicit grid properties in a single grid declaration.
        // The sub-properties not specified are set to their initial value, as normal for shorthands.
        addProperty(CSSPropertyGridAutoFlow, CSSPropertyGrid, CSSValuePool::singleton().createIdentifierValue(CSSValueRow), important);
        addProperty(CSSPropertyGridAutoColumns, CSSPropertyGrid, CSSValuePool::singleton().createIdentifierValue(CSSValueAuto), important);
        addProperty(CSSPropertyGridAutoRows, CSSPropertyGrid, CSSValuePool::singleton().createIdentifierValue(CSSValueAuto), important);

        return true;
    }

    m_range = rangeCopy;

    RefPtr<CSSValue> autoColumnsValue;
    RefPtr<CSSValue> autoRowsValue;
    RefPtr<CSSValue> templateRows;
    RefPtr<CSSValue> templateColumns;
    RefPtr<CSSValue> gridAutoFlow;
    
    if (m_range.peek().id() == CSSValueAutoFlow || m_range.peek().id() == CSSValueDense) {
        // 2- [ auto-flow && dense? ] <grid-auto-rows>? / <grid-template-columns>
        gridAutoFlow = consumeImplicitGridAutoFlow(m_range, CSSValuePool::singleton().createIdentifierValue(CSSValueRow));
        if (!gridAutoFlow || m_range.atEnd())
            return false;
        if (consumeSlashIncludingWhitespace(m_range))
            autoRowsValue = CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
        else {
            autoRowsValue = consumeGridTrackList(m_range, m_context, GridAuto);
            if (!autoRowsValue)
                return false;
            if (!consumeSlashIncludingWhitespace(m_range))
                return false;
        }
        if (m_range.atEnd())
            return false;
        templateColumns = consumeGridTemplatesRowsOrColumns(m_range, m_context);
        if (!templateColumns)
            return false;
        templateRows = CSSValuePool::singleton().createIdentifierValue(CSSValueNone);
        autoColumnsValue = CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
    } else {
        // 3- <grid-template-rows> / [ auto-flow && dense? ] <grid-auto-columns>?
        templateRows = consumeGridTemplatesRowsOrColumns(m_range, m_context.mode);
        if (!templateRows)
            return false;
        if (!consumeSlashIncludingWhitespace(m_range) || m_range.atEnd())
            return false;
        gridAutoFlow = consumeImplicitGridAutoFlow(m_range, CSSValuePool::singleton().createIdentifierValue(CSSValueColumn));
        if (!gridAutoFlow)
            return false;
        if (m_range.atEnd())
            autoColumnsValue = CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
        else {
            autoColumnsValue = consumeGridTrackList(m_range, m_context, GridAuto);
            if (!autoColumnsValue)
                return false;
        }
        templateColumns = CSSValuePool::singleton().createIdentifierValue(CSSValueNone);
        autoRowsValue = CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
    }
    
    if (!m_range.atEnd())
        return false;
    
    // It can only be specified the explicit or the implicit grid properties in a single grid declaration.
    // The sub-properties not specified are set to their initial value, as normal for shorthands.
    addProperty(CSSPropertyGridTemplateColumns, CSSPropertyGrid, templateColumns.releaseNonNull(), important);
    addProperty(CSSPropertyGridTemplateRows, CSSPropertyGrid, templateRows.releaseNonNull(), important);
    addProperty(CSSPropertyGridTemplateAreas, CSSPropertyGrid, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);
    addProperty(CSSPropertyGridAutoFlow, CSSPropertyGrid, gridAutoFlow.releaseNonNull(), important);
    addProperty(CSSPropertyGridAutoColumns, CSSPropertyGrid, autoColumnsValue.releaseNonNull(), important);
    addProperty(CSSPropertyGridAutoRows, CSSPropertyGrid, autoRowsValue.releaseNonNull(), important);
    
    return true;
}

bool CSSPropertyParser::consumePlaceContentShorthand(bool important)
{
    ASSERT(shorthandForProperty(CSSPropertyPlaceContent).length() == 2);

    if (m_range.atEnd())
        return false;

    CSSParserTokenRange rangeCopy = m_range;
    bool isBaseline = isBaselineKeyword(m_range.peek().id());
    RefPtr<CSSValue> alignContentValue = consumeContentDistributionOverflowPosition(m_range, isContentPositionKeyword);
    if (!alignContentValue)
        return false;

    // justify-content property does not allow the <baseline-position> values.
    if (m_range.atEnd() && isBaseline)
        return false;
    if (isBaselineKeyword(m_range.peek().id()))
        return false;

    if (m_range.atEnd())
        m_range = rangeCopy;
    RefPtr<CSSValue> justifyContentValue = consumeContentDistributionOverflowPosition(m_range, isContentPositionOrLeftOrRightKeyword);
    if (!justifyContentValue)
        return false;
    if (!m_range.atEnd())
        return false;

    addProperty(CSSPropertyAlignContent, CSSPropertyPlaceContent, alignContentValue.releaseNonNull(), important);
    addProperty(CSSPropertyJustifyContent, CSSPropertyPlaceContent, justifyContentValue.releaseNonNull(), important);
    return true;
}

bool CSSPropertyParser::consumePlaceItemsShorthand(bool important)
{
    ASSERT(shorthandForProperty(CSSPropertyPlaceItems).length() == 2);

    CSSParserTokenRange rangeCopy = m_range;
    RefPtr<CSSValue> alignItemsValue = consumeAlignItems(m_range);
    if (!alignItemsValue)
        return false;

    if (m_range.atEnd())
        m_range = rangeCopy;
    RefPtr<CSSValue> justifyItemsValue = consumeJustifyItems(m_range);
    if (!justifyItemsValue)
        return false;

    if (!m_range.atEnd())
        return false;

    addProperty(CSSPropertyAlignItems, CSSPropertyPlaceItems, alignItemsValue.releaseNonNull(), important);
    addProperty(CSSPropertyJustifyItems, CSSPropertyPlaceItems, justifyItemsValue.releaseNonNull(), important);
    return true;
}

bool CSSPropertyParser::consumePlaceSelfShorthand(bool important)
{
    ASSERT(shorthandForProperty(CSSPropertyPlaceSelf).length() == 2);

    CSSParserTokenRange rangeCopy = m_range;
    RefPtr<CSSValue> alignSelfValue = consumeSelfPositionOverflowPosition(m_range, isSelfPositionKeyword);
    if (!alignSelfValue)
        return false;

    if (m_range.atEnd())
        m_range = rangeCopy;
    RefPtr<CSSValue> justifySelfValue = consumeSelfPositionOverflowPosition(m_range, isSelfPositionOrLeftOrRightKeyword);
    if (!justifySelfValue)
        return false;

    if (!m_range.atEnd())
        return false;

    addProperty(CSSPropertyAlignSelf, CSSPropertyPlaceSelf, alignSelfValue.releaseNonNull(), important);
    addProperty(CSSPropertyJustifySelf, CSSPropertyPlaceSelf, justifySelfValue.releaseNonNull(), important);
    return true;
}

bool CSSPropertyParser::consumeOverscrollBehaviorShorthand(bool important)
{
    ASSERT(shorthandForProperty(CSSPropertyOverscrollBehavior).length() == 2);

    if (m_range.atEnd())
        return false;

    RefPtr<CSSValue> overscrollBehaviorX = consumeOverscrollBehavior(m_range);
    if (!overscrollBehaviorX)
        return false;

    RefPtr<CSSValue> overscrollBehaviorY;
    m_range.consumeWhitespace();
    if (m_range.atEnd())
        overscrollBehaviorY = overscrollBehaviorX;
    else {
        overscrollBehaviorY = consumeOverscrollBehavior(m_range);
        m_range.consumeWhitespace();
        if (!m_range.atEnd())
            return false;
    }

    addProperty(CSSPropertyOverscrollBehaviorX, CSSPropertyOverscrollBehavior, *overscrollBehaviorX, important);
    addProperty(CSSPropertyOverscrollBehaviorY, CSSPropertyOverscrollBehavior, *overscrollBehaviorY, important);
    return true;
}

bool CSSPropertyParser::consumeContainerShorthand(bool important)
{
    auto name = consumeContainerName(m_range);
    if (!name)
        return false;

    bool sawSlash = false;

    auto consumeSlashType = [&]() -> RefPtr<CSSValue> {
        if (m_range.atEnd())
            return nullptr;
        if (!consumeSlashIncludingWhitespace(m_range))
            return nullptr;
        sawSlash = true;
        return parseSingleValue(CSSPropertyContainerType);
    };

    auto type = consumeSlashType();

    if (!m_range.atEnd() || (sawSlash && !type))
        return false;

    addProperty(CSSPropertyContainerName, CSSPropertyContainer, name.releaseNonNull(), important);
    addPropertyWithImplicitDefault(CSSPropertyContainerType, CSSPropertyContainer, WTFMove(type), CSSValuePool::singleton().createImplicitInitialValue(), important);
    return true;
}

bool CSSPropertyParser::consumeContainIntrinsicSizeShorthand(bool important)
{
    ASSERT(shorthandForProperty(CSSPropertyContainIntrinsicSize).length() == 2);
    ASSERT(isCSSPropertyExposed(CSSPropertyContainIntrinsicSize, &m_context.propertySettings));

    if (m_range.atEnd())
        return false;

    RefPtr<CSSValue> containIntrinsicWidth = consumeContainIntrinsicSize(m_range);
    if (!containIntrinsicWidth)
        return false;

    RefPtr<CSSValue> containIntrinsicHeight;
    m_range.consumeWhitespace();
    if (m_range.atEnd())
        containIntrinsicHeight = containIntrinsicWidth;
    else {
        containIntrinsicHeight = consumeContainIntrinsicSize(m_range);
        m_range.consumeWhitespace();
        if (!m_range.atEnd() || !containIntrinsicHeight)
            return false;
    }

    addProperty(CSSPropertyContainIntrinsicWidth, CSSPropertyContainIntrinsicSize, *containIntrinsicWidth, important);
    addProperty(CSSPropertyContainIntrinsicHeight, CSSPropertyContainIntrinsicSize, *containIntrinsicHeight, important);
    return true;
}

bool CSSPropertyParser::consumeOffset(bool important)
{
    auto& valuePool = CSSValuePool::singleton();

    // The offset shorthand is defined as:
    // [ <'offset-position'>?
    //   [ <'offset-path'>
    //     [ <'offset-distance'> || <'offset-rotate'> ]?
    //   ]?
    // ]!
    // [ / <'offset-anchor'> ]?

    // Parse out offset-position.
    auto offsetPosition = parseSingleValue(CSSPropertyOffsetPosition, CSSPropertyOffset);

    // Parse out offset-path.
    auto offsetPath = parseSingleValue(CSSPropertyOffsetPath, CSSPropertyOffset);

    // Either one of offset-position and offset-path must be present.
    if (!offsetPosition && !offsetPath)
        return false;

    // Only parse offset-distance and offset-rotate if offset-path is specified.
    RefPtr<CSSValue> offsetDistance;
    RefPtr<CSSValue> offsetRotate;
    if (offsetPath) {
        // Try to parse offset-distance first. If successful, parse the following offset-rotate.
        // Otherwise, parse in the reverse order.
        if ((offsetDistance = parseSingleValue(CSSPropertyOffsetDistance, CSSPropertyOffset)))
            offsetRotate = parseSingleValue(CSSPropertyOffsetRotate, CSSPropertyOffset);
        else {
            offsetRotate = parseSingleValue(CSSPropertyOffsetRotate, CSSPropertyOffset);
            offsetDistance = parseSingleValue(CSSPropertyOffsetDistance, CSSPropertyOffset);
        }
    }

    // Parse out offset-anchor. Only parse if the prefix slash is present.
    RefPtr<CSSValue> offsetAnchor;
    if (consumeSlashIncludingWhitespace(m_range)) {
        // offset-anchor must follow the slash.
        if (!(offsetAnchor = parseSingleValue(CSSPropertyOffsetAnchor, CSSPropertyOffset)))
            return false;
    }

    addPropertyWithImplicitDefault(CSSPropertyOffsetPath, CSSPropertyOffset, WTFMove(offsetPath), valuePool.createIdentifierValue(CSSValueNone), important);
    addPropertyWithImplicitDefault(CSSPropertyOffsetDistance, CSSPropertyOffset, WTFMove(offsetDistance), valuePool.createValue(0.0, CSSUnitType::CSS_PX), important);
    addPropertyWithImplicitDefault(CSSPropertyOffsetPosition, CSSPropertyOffset, WTFMove(offsetPosition), valuePool.createIdentifierValue(CSSValueAuto), important);
    addPropertyWithImplicitDefault(CSSPropertyOffsetAnchor, CSSPropertyOffset, WTFMove(offsetAnchor), valuePool.createIdentifierValue(CSSValueAuto), important);
    addPropertyWithImplicitDefault(CSSPropertyOffsetRotate, CSSPropertyOffset, WTFMove(offsetRotate), CSSOffsetRotateValue::initialValue(), important);

    return m_range.atEnd();
}

bool CSSPropertyParser::consumeListStyleShorthand(bool important)
{
    auto& valuePool = CSSValuePool::singleton();
    RefPtr<CSSValue> parsedPosition;
    RefPtr<CSSValue> parsedImage;
    RefPtr<CSSValue> parsedType;
    unsigned noneCount = 0;

    while (!m_range.atEnd()) {
        if (m_range.peek().id() == CSSValueNone) {
            ++noneCount;
            consumeIdent(m_range);
            continue;
        }
        if (!parsedPosition) {
            if (auto position = parseSingleValue(CSSPropertyListStylePosition, CSSPropertyListStyle)) {
                parsedPosition = position;
                continue;
            }
        }
        if (!parsedImage) {
            if (auto image = parseSingleValue(CSSPropertyListStyleImage, CSSPropertyListStyle)) {
                parsedImage = image;
                continue;
            }
        }
        if (!parsedType) {
            if (auto type = parseSingleValue(CSSPropertyListStyleType, CSSPropertyListStyle)) {
                parsedType = type;
                continue;
            }
        }
        return false;
    }

    if (noneCount > (static_cast<unsigned>(!parsedImage + !parsedType)))
        return false;

    // Use the implicit initial value for list-style-image, to serialize to "none" instead of "none none".
    if (noneCount == 2) {
        parsedImage = valuePool.createImplicitInitialValue();
        parsedType = valuePool.createIdentifierValue(CSSValueNone);
    } else if (noneCount == 1) {
        if (!parsedImage)
            parsedImage = parsedType ? valuePool.createIdentifierValue(CSSValueNone) : valuePool.createImplicitInitialValue();
        if (!parsedType)
            parsedType = valuePool.createIdentifierValue(CSSValueNone);
    }

    addPropertyWithImplicitDefault(CSSPropertyListStylePosition, CSSPropertyListStyle, WTFMove(parsedPosition), valuePool.createImplicitInitialValue(), important);
    addPropertyWithImplicitDefault(CSSPropertyListStyleImage, CSSPropertyListStyle, WTFMove(parsedImage), valuePool.createImplicitInitialValue(), important);
    addPropertyWithImplicitDefault(CSSPropertyListStyleType, CSSPropertyListStyle, WTFMove(parsedType), valuePool.createImplicitInitialValue(), important);
    return m_range.atEnd();
}

bool CSSPropertyParser::parseShorthand(CSSPropertyID property, bool important)
{
    switch (property) {
    case CSSPropertyOverflow:
        return consumeOverflowShorthand(important);
    case CSSPropertyOverscrollBehavior:
        return consumeOverscrollBehaviorShorthand(important);
    case CSSPropertyFont: {
        const CSSParserToken& token = m_range.peek();
        if (CSSPropertyParserHelpers::isSystemFontShorthand(token.id()))
            return consumeSystemFont(important);
        return consumeFont(important);
    }
    case CSSPropertyFontVariant:
        return consumeFontVariantShorthand(important);
    case CSSPropertyFontSynthesis:
        return consumeFontSynthesis(important);
    case CSSPropertyBorderSpacing:
        return consumeBorderSpacing(important);
    case CSSPropertyColumns:
        return consumeColumns(important);
    case CSSPropertyAnimation:
        return consumeAnimationShorthand(animationShorthandForParsing(), important);
    case CSSPropertyTransition:
        return consumeAnimationShorthand(transitionShorthandForParsing(), important);
    case CSSPropertyTextDecoration: {
        auto line = consumeTextDecorationLine(m_range);
        if (!line || !m_range.atEnd())
            return false;
        addProperty(CSSPropertyTextDecorationLine, property, line.releaseNonNull(), important);
        return true;
    }
    case CSSPropertyWebkitTextDecoration:
        // FIXME-NEWPARSER: We need to unprefix -line/-style/-color ASAP and get rid
        // of -webkit-text-decoration completely.
        return consumeShorthandGreedily(webkitTextDecorationShorthand(), important);
    case CSSPropertyInset:
        return consume4ValueShorthand(insetShorthand(), important);
    case CSSPropertyInsetBlock:
        return consume2ValueShorthand(insetBlockShorthand(), important);
    case CSSPropertyInsetInline:
        return consume2ValueShorthand(insetInlineShorthand(), important);
    case CSSPropertyMargin:
        return consume4ValueShorthand(marginShorthand(), important);
    case CSSPropertyMarginBlock:
        return consume2ValueShorthand(marginBlockShorthand(), important);
    case CSSPropertyMarginInline:
        return consume2ValueShorthand(marginInlineShorthand(), important);
    case CSSPropertyPadding:
        return consume4ValueShorthand(paddingShorthand(), important);
    case CSSPropertyPaddingBlock:
        return consume2ValueShorthand(paddingBlockShorthand(), important);
    case CSSPropertyPaddingInline:
        return consume2ValueShorthand(paddingInlineShorthand(), important);
    case CSSPropertyScrollMargin:
        return consume4ValueShorthand(scrollMarginShorthand(), important);
    case CSSPropertyScrollMarginBlock:
        return consume2ValueShorthand(scrollMarginBlockShorthand(), important);
    case CSSPropertyScrollMarginInline:
        return consume2ValueShorthand(scrollMarginInlineShorthand(), important);
    case CSSPropertyScrollPadding:
        return consume4ValueShorthand(scrollPaddingShorthand(), important);
    case CSSPropertyScrollPaddingBlock:
        return consume2ValueShorthand(scrollPaddingBlockShorthand(), important);
    case CSSPropertyScrollPaddingInline:
        return consume2ValueShorthand(scrollPaddingInlineShorthand(), important);
    case CSSPropertyTextEmphasis:
        return consumeShorthandGreedily(textEmphasisShorthand(), important);
    case CSSPropertyOutline:
        return consumeShorthandGreedily(outlineShorthand(), important);
    case CSSPropertyOffset:
        return consumeOffset(important);
    case CSSPropertyBorderInline: {
        RefPtr<CSSValue> width;
        RefPtr<CSSValue> style;
        RefPtr<CSSValue> color;
        if (!consumeBorder(width, style, color))
            return false;

        addExpandedPropertyForValue(CSSPropertyBorderInlineWidth, width.releaseNonNull(), important);
        addExpandedPropertyForValue(CSSPropertyBorderInlineStyle, style.releaseNonNull(), important);
        addExpandedPropertyForValue(CSSPropertyBorderInlineColor, color.releaseNonNull(), important);
        return true;
    }
    case CSSPropertyBorderInlineColor:
        return consume2ValueShorthand(borderInlineColorShorthand(), important);
    case CSSPropertyBorderInlineStyle:
        return consume2ValueShorthand(borderInlineStyleShorthand(), important);
    case CSSPropertyBorderInlineWidth:
        return consume2ValueShorthand(borderInlineWidthShorthand(), important);
    case CSSPropertyBorderInlineStart:
        return consumeShorthandGreedily(borderInlineStartShorthand(), important);
    case CSSPropertyBorderInlineEnd:
        return consumeShorthandGreedily(borderInlineEndShorthand(), important);
    case CSSPropertyBorderBlock: {
        RefPtr<CSSValue> width;
        RefPtr<CSSValue> style;
        RefPtr<CSSValue> color;
        if (!consumeBorder(width, style, color))
            return false;

        addExpandedPropertyForValue(CSSPropertyBorderBlockWidth, width.releaseNonNull(), important);
        addExpandedPropertyForValue(CSSPropertyBorderBlockStyle, style.releaseNonNull(), important);
        addExpandedPropertyForValue(CSSPropertyBorderBlockColor, color.releaseNonNull(), important);
        return true;
    }
    case CSSPropertyBorderBlockColor:
        return consume2ValueShorthand(borderBlockColorShorthand(), important);
    case CSSPropertyBorderBlockStyle:
        return consume2ValueShorthand(borderBlockStyleShorthand(), important);
    case CSSPropertyBorderBlockWidth:
        return consume2ValueShorthand(borderBlockWidthShorthand(), important);
    case CSSPropertyBorderBlockStart:
        return consumeShorthandGreedily(borderBlockStartShorthand(), important);
    case CSSPropertyBorderBlockEnd:
        return consumeShorthandGreedily(borderBlockEndShorthand(), important);
    case CSSPropertyWebkitTextStroke:
        return consumeShorthandGreedily(webkitTextStrokeShorthand(), important);
    case CSSPropertyMarker: {
        RefPtr<CSSValue> marker = parseSingleValue(CSSPropertyMarkerStart);
        if (!marker || !m_range.atEnd())
            return false;
        auto markerRef = marker.releaseNonNull();
        addProperty(CSSPropertyMarkerStart, CSSPropertyMarker, markerRef.copyRef(), important);
        addProperty(CSSPropertyMarkerMid, CSSPropertyMarker, markerRef.copyRef(), important);
        addProperty(CSSPropertyMarkerEnd, CSSPropertyMarker, markerRef.copyRef(), important);
        return true;
    }
    case CSSPropertyFlex:
        return consumeFlex(important);
    case CSSPropertyFlexFlow:
        return consumeShorthandGreedily(flexFlowShorthand(), important);
    case CSSPropertyColumnRule:
        return consumeShorthandGreedily(columnRuleShorthand(), important);
    case CSSPropertyListStyle:
        return consumeListStyleShorthand(important);
    case CSSPropertyBorderRadius:
    case CSSPropertyWebkitBorderRadius: {
        RefPtr<CSSPrimitiveValue> horizontalRadii[4];
        RefPtr<CSSPrimitiveValue> verticalRadii[4];
        if (!consumeRadii(horizontalRadii, verticalRadii, m_range, m_context.mode, property == CSSPropertyWebkitBorderRadius))
            return false;
        addProperty(CSSPropertyBorderTopLeftRadius, CSSPropertyBorderRadius, createPrimitiveValuePair(horizontalRadii[0].releaseNonNull(), verticalRadii[0].releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce), important);
        addProperty(CSSPropertyBorderTopRightRadius, CSSPropertyBorderRadius, createPrimitiveValuePair(horizontalRadii[1].releaseNonNull(), verticalRadii[1].releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce), important);
        addProperty(CSSPropertyBorderBottomRightRadius, CSSPropertyBorderRadius, createPrimitiveValuePair(horizontalRadii[2].releaseNonNull(), verticalRadii[2].releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce), important);
        addProperty(CSSPropertyBorderBottomLeftRadius, CSSPropertyBorderRadius, createPrimitiveValuePair(horizontalRadii[3].releaseNonNull(), verticalRadii[3].releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce), important);
        return true;
    }
    case CSSPropertyBorderColor:
        return consume4ValueShorthand(borderColorShorthand(), important);
    case CSSPropertyBorderStyle:
        return consume4ValueShorthand(borderStyleShorthand(), important);
    case CSSPropertyBorderWidth:
        return consume4ValueShorthand(borderWidthShorthand(), important);
    case CSSPropertyBorderTop:
        return consumeShorthandGreedily(borderTopShorthand(), important);
    case CSSPropertyBorderRight:
        return consumeShorthandGreedily(borderRightShorthand(), important);
    case CSSPropertyBorderBottom:
        return consumeShorthandGreedily(borderBottomShorthand(), important);
    case CSSPropertyBorderLeft:
        return consumeShorthandGreedily(borderLeftShorthand(), important);
    case CSSPropertyBorder: {
        RefPtr<CSSValue> width;
        RefPtr<CSSValue> style;
        RefPtr<CSSValue> color;
        if (!consumeBorder(width, style, color))
            return false;

        addExpandedPropertyForValue(CSSPropertyBorderWidth, width.releaseNonNull(), important);
        addExpandedPropertyForValue(CSSPropertyBorderStyle, style.releaseNonNull(), important);
        addExpandedPropertyForValue(CSSPropertyBorderColor, color.releaseNonNull(), important);
        addExpandedPropertyForValue(CSSPropertyBorderImage, CSSValuePool::singleton().createImplicitInitialValue(), important);
        return true;
    }
    case CSSPropertyBorderImage:
    case CSSPropertyWebkitBorderImage:
        return consumeBorderImage(property, important);
    case CSSPropertyPageBreakAfter:
    case CSSPropertyPageBreakBefore:
    case CSSPropertyPageBreakInside:
    case CSSPropertyWebkitColumnBreakAfter:
    case CSSPropertyWebkitColumnBreakBefore:
    case CSSPropertyWebkitColumnBreakInside:
        return consumeLegacyBreakProperty(property, important);
    case CSSPropertyWebkitTextOrientation:
        return consumeLegacyTextOrientation(important);
    case CSSPropertyMaskPosition:
    case CSSPropertyWebkitMaskPosition:
    case CSSPropertyBackgroundPosition: {
        RefPtr<CSSValue> resultX;
        RefPtr<CSSValue> resultY;
        if (!consumeBackgroundPosition(m_range, m_context, property, resultX, resultY) || !m_range.atEnd())
            return false;
        addProperty(property == CSSPropertyBackgroundPosition ? CSSPropertyBackgroundPositionX : CSSPropertyWebkitMaskPositionX, property, resultX.releaseNonNull(), important);
        addProperty(property == CSSPropertyBackgroundPosition ? CSSPropertyBackgroundPositionY : CSSPropertyWebkitMaskPositionY, property, resultY.releaseNonNull(), important);
        return true;
    }
    case CSSPropertyBackground:
        return consumeBackgroundShorthand(backgroundShorthand(), important);
    case CSSPropertyWebkitBackgroundSize: {
        auto backgroundSize = consumeCommaSeparatedBackgroundComponent(CSSPropertyWebkitBackgroundSize, m_range, m_context.mode);
        if (!backgroundSize || !m_range.atEnd())
            return false;
        addProperty(CSSPropertyBackgroundSize, CSSPropertyWebkitBackgroundSize, backgroundSize.releaseNonNull(), important);
        return true;
    }
    case CSSPropertyMask:
    case CSSPropertyWebkitMask:
        return consumeBackgroundShorthand(shorthandForProperty(property), important);
    case CSSPropertyTransformOrigin:
        return consumeTransformOrigin(important);
    case CSSPropertyPerspectiveOrigin:
        return consumePerspectiveOrigin(important);
    case CSSPropertyWebkitPerspective:
        return consumePrefixedPerspective(important);
    case CSSPropertyGap: {
        RefPtr<CSSValue> rowGap = consumeGapLength(m_range, m_context.mode);
        RefPtr<CSSValue> columnGap = consumeGapLength(m_range, m_context.mode);
        if (!rowGap || !m_range.atEnd())
            return false;
        if (!columnGap)
            columnGap = rowGap;
        addProperty(CSSPropertyRowGap, CSSPropertyGap, rowGap.releaseNonNull(), important);
        addProperty(CSSPropertyColumnGap, CSSPropertyGap, columnGap.releaseNonNull(), important);
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
    case CSSPropertyPlaceContent:
        return consumePlaceContentShorthand(important);
    case CSSPropertyPlaceItems:
        return consumePlaceItemsShorthand(important);
    case CSSPropertyPlaceSelf:
        return consumePlaceSelfShorthand(important);
    case CSSPropertyTextDecorationSkip:
        return consumeTextDecorationSkip(important);
    case CSSPropertyContainer:
        return consumeContainerShorthand(important);
    case CSSPropertyContainIntrinsicSize:
        return consumeContainIntrinsicSizeShorthand(important);
    default:
        return false;
    }
}

} // namespace WebCore
