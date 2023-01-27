/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "StyleProperties.h"

#include "CSSBorderImageWidthValue.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSCustomPropertyValue.h"
#include "CSSGridLineNamesValue.h"
#include "CSSGridTemplateAreasValue.h"
#include "CSSOffsetRotateValue.h"
#include "CSSParser.h"
#include "CSSParserIdioms.h"
#include "CSSPendingSubstitutionValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParser.h"
#include "CSSTokenizer.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "Color.h"
#include "Document.h"
#include "FontSelectionValueInlines.h"
#include "PropertySetCSSStyleDeclaration.h"
#include "Rect.h"
#include "StylePropertyShorthand.h"
#include "StylePropertyShorthandFunctions.h"
#include "StyleSheetContents.h"
#include <bitset>
#include <wtf/text/StringBuilder.h>

#ifndef NDEBUG
#include <stdio.h>
#include <wtf/text/CString.h>
#endif

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleProperties);
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(ImmutableStyleProperties);
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(MutableStyleProperties);

constexpr unsigned maxShorthandLength = 17; // FIXME: Generate this from CSSProperties.json.
constexpr unsigned maxShorthandsForLonghand = 4; // FIXME: Generate this from CSSProperties.json and use it for StylePropertyShorthandVector too.

static size_t sizeForImmutableStylePropertiesWithPropertyCount(unsigned count)
{
    return sizeof(ImmutableStyleProperties) - sizeof(void*) + sizeof(StylePropertyMetadata) * count + sizeof(SizeEfficientPtr<const CSSValue>) * count;
}

static bool isValueIDIncludingList(const CSSValue& value, CSSValueID id)
{
    if (is<CSSValueList>(value)) {
        auto& valueList = downcast<CSSValueList>(value);
        if (valueList.size() != 1)
            return false;
        auto* item = valueList.item(0);
        return item && isValueID(*item, id);
    }
    return isValueID(value, id);
}

Ref<ImmutableStyleProperties> ImmutableStyleProperties::create(const CSSProperty* properties, unsigned count, CSSParserMode cssParserMode)
{
    void* slot = ImmutableStylePropertiesMalloc::malloc(sizeForImmutableStylePropertiesWithPropertyCount(count));
    return adoptRef(*new (NotNull, slot) ImmutableStyleProperties(properties, count, cssParserMode));
}

Ref<ImmutableStyleProperties> StyleProperties::immutableCopyIfNeeded() const
{
    if (is<ImmutableStyleProperties>(*this))
        return downcast<ImmutableStyleProperties>(const_cast<StyleProperties&>(*this));
    const MutableStyleProperties& mutableThis = downcast<MutableStyleProperties>(*this);
    return ImmutableStyleProperties::create(mutableThis.m_propertyVector.data(), mutableThis.m_propertyVector.size(), cssParserMode());
}

MutableStyleProperties::MutableStyleProperties(CSSParserMode cssParserMode)
    : StyleProperties(cssParserMode, MutablePropertiesType)
{
}

MutableStyleProperties::MutableStyleProperties(Vector<CSSProperty>&& properties)
    : StyleProperties(HTMLStandardMode, MutablePropertiesType)
    , m_propertyVector(WTFMove(properties))
{
}

MutableStyleProperties::~MutableStyleProperties() = default;

ImmutableStyleProperties::ImmutableStyleProperties(const CSSProperty* properties, unsigned length, CSSParserMode cssParserMode)
    : StyleProperties(cssParserMode, length)
{
    StylePropertyMetadata* metadataArray = const_cast<StylePropertyMetadata*>(this->metadataArray());
    SizeEfficientPtr<CSSValue>* valueArray = bitwise_cast<SizeEfficientPtr<CSSValue>*>(this->valueArray());
    for (unsigned i = 0; i < length; ++i) {
        metadataArray[i] = properties[i].metadata();
        auto* value = properties[i].value();
        valueArray[i] = value;
        value->ref();
    }
}

ImmutableStyleProperties::~ImmutableStyleProperties()
{
    SizeEfficientPtr<CSSValue>* valueArray = bitwise_cast<SizeEfficientPtr<CSSValue>*>(this->valueArray());
    for (unsigned i = 0; i < m_arraySize; ++i)
        valueArray[i]->deref();
}

MutableStyleProperties::MutableStyleProperties(const StyleProperties& other)
    : StyleProperties(other.cssParserMode(), MutablePropertiesType)
{
    if (is<MutableStyleProperties>(other))
        m_propertyVector = downcast<MutableStyleProperties>(other).m_propertyVector;
    else
        m_propertyVector = map(downcast<ImmutableStyleProperties>(other), [] (auto property) { return property.toCSSProperty(); });
}

static inline CSSValueID valueIDExpandingInitialValuePlaceholder(CSSPropertyID property, const CSSValue& value)
{
    return value.isImplicitInitialValue() ? initialValueIDForLonghand(property) : valueID(value);
}

static inline std::optional<CSSValueID> valueIDExpandingInitialValuePlaceholder(CSSPropertyID property, const CSSValue* value)
{
    if (!value)
        return std::nullopt;
    return valueIDExpandingInitialValuePlaceholder(property, *value);
}

static String serializeLonghandValue(CSSPropertyID property, const CSSValue& value)
{
    switch (property) {
    case CSSPropertyFillOpacity:
    case CSSPropertyFloodOpacity:
    case CSSPropertyOpacity:
    case CSSPropertyStopOpacity:
    case CSSPropertyStrokeOpacity:
        // FIXME: Handle this when creating the CSSValue for opacity, to be consistent with other CSS value serialization quirks.
        // Opacity percentage values serialize as a fraction in the range 0-1, not "%".
        if (is<CSSPrimitiveValue>(value) && downcast<CSSPrimitiveValue>(value).isPercentage())
            return makeString(downcast<CSSPrimitiveValue>(value).doubleValue() / 100);
        FALLTHROUGH;
    default:
        return value.isImplicitInitialValue() ? initialValueTextForLonghand(property) : value.cssText();
    }
}

static String serializeLonghandValue(CSSPropertyID property, const CSSValue* value)
{
    return value ? serializeLonghandValue(property, *value) : String();
}

class ShorthandSerializer {
public:
    explicit ShorthandSerializer(const StyleProperties&, CSSPropertyID shorthandID);
    String serialize();

private:
    bool commonSerializationChecks(const StyleProperties&);

    String serializeLonghands() const;
    String serializeLonghands(unsigned lengthLimit, const char* separator = " ") const;
    String serializeLonghandsOmittingInitialValues() const;
    String serializeLonghandsOmittingTrailingInitialValue(const char* separator = " ") const;

    String serializeCommonValue() const;
    String serializeCommonValue(unsigned startIndex, unsigned count) const;
    String serializePair() const;
    String serializeQuad() const;

    String serializeLayered() const;

    String serializeAlignment() const;
    String serializeBorder(unsigned sectionLength) const;
    String serializeBorderImage() const;
    String serializeBorderRadius() const;
    String serializeBreakInside() const;
    String serializeColumnBreak() const;
    String serializeFont() const;
    String serializeFontSynthesis() const;
    String serializeFontVariant() const;
    String serializeGrid() const;
    String serializeGridArea() const;
    String serializeGridRowColumn() const;
    String serializeGridTemplate() const;
    String serializeOffset() const;
    String serializePageBreak() const;

    struct Longhand {
        CSSPropertyID property;
        CSSValue& value;
    };
    struct LonghandIteratorBase {
        void operator++() { ++index; }
        bool operator==(std::nullptr_t) const { return index >= serializer.length(); }
        bool operator!=(std::nullptr_t) const { return !(*this == nullptr); }
        const ShorthandSerializer& serializer;
        unsigned index { 0 };
    };
    struct LonghandIterator : LonghandIteratorBase {
        Longhand operator*() const { return { serializer.longhand(index) }; }
    };
    struct LonghandValueIterator : LonghandIteratorBase {
        CSSValue& operator*() const { return { serializer.longhandValue(index) }; }
    };
    template<typename IteratorType> struct LonghandRange {
        IteratorType begin() const { return { { serializer } }; }
        static constexpr std::nullptr_t end() { return nullptr; }
        unsigned size() const { return serializer.length(); }
        const ShorthandSerializer& serializer;
    };

    static bool isInitialValue(Longhand);
    static String serializeValue(Longhand);

    unsigned length() const { return m_shorthand.length(); }
    Longhand longhand(unsigned index) const { return { longhandProperty(index), longhandValue(index) }; }
    CSSPropertyID longhandProperty(unsigned index) const;
    CSSValue& longhandValue(unsigned index) const;

    unsigned longhandIndex(unsigned index, CSSPropertyID) const;

    LonghandRange<LonghandIterator> longhands() const { return { *this }; }
    LonghandRange<LonghandValueIterator> longhandValues() const { return { *this }; }

    CSSValueID longhandValueID(unsigned index) const;
    bool isLonghandValueID(unsigned index, CSSValueID valueID) const { return longhandValueID(index) == valueID; }
    bool isLonghandValueNone(unsigned index) const { return isLonghandValueID(index, CSSValueNone); }
    bool isLonghandInitialValue(unsigned index) const { return isInitialValue(longhand(index)); }
    String serializeLonghandValue(unsigned index) const;

    bool subsequentLonghandsHaveInitialValues(unsigned index) const;

    StylePropertyShorthand m_shorthand;
    RefPtr<CSSValue> m_longhandValues[maxShorthandLength];
    String m_result;
    bool m_gridTemplateAreasWasSetFromShorthand { false };
    bool m_commonSerializationChecksSuppliedResult;
};

bool ShorthandSerializer::commonSerializationChecks(const StyleProperties& properties)
{
    ASSERT(length());
    ASSERT(length() <= maxShorthandLength || m_shorthand.id() == CSSPropertyAll);

    std::optional<CSSValueID> specialKeyword;
    bool allSpecialKeywords = true;
    std::optional<bool> importance;
    std::optional<CSSPendingSubstitutionValue*> firstValueFromShorthand;
    String commonValue;
    for (unsigned i = 0; i < length(); ++i) {
        auto longhand = longhandProperty(i);

        int propertyIndex = properties.findPropertyIndex(longhand);
        if (propertyIndex == -1)
            return true;
        auto property = properties.propertyAt(propertyIndex);

        // Don't serialize if longhands have different importance.
        bool isImportant = property.isImportant();
        if (importance.value_or(isImportant) != isImportant)
            return true;
        importance = isImportant;

        // Record one bit of data besides the property values that's needed for serializatin.
        // FIXME: Remove this.
        if (longhand == CSSPropertyGridTemplateAreas && property.toCSSProperty().isSetFromShorthand())
            m_gridTemplateAreasWasSetFromShorthand = true;

        auto value = property.value();

        // Don't serialize if longhands have different CSS-wide keywords.
        if (!isCSSWideKeyword(valueID(*value)) || value->isImplicitInitialValue()) {
            if (specialKeyword)
                return true;
            allSpecialKeywords = false;
        } else {
            if (!allSpecialKeywords)
                return true;
            auto keyword = valueID(value);
            if (specialKeyword.value_or(keyword) != keyword)
                return true;
            specialKeyword = keyword;
            continue;
        }

        // Don't serialize if any longhand was set to a variable.
        if (is<CSSVariableReferenceValue>(value))
            return true;

        // Don't serialize if any longhand was set by a different shorthand.
        auto* valueFromShorthand = dynamicDowncast<CSSPendingSubstitutionValue>(value);
        if (valueFromShorthand && valueFromShorthand->shorthandPropertyId() != m_shorthand.id())
            return true;

        // Don't serialize if longhands are inconsistent about whether they were set by the shorthand.
        if (!firstValueFromShorthand)
            firstValueFromShorthand = valueFromShorthand;
        else {
            bool wasSetByShorthand = valueFromShorthand;
            bool firstWasSetByShorthand = *firstValueFromShorthand;
            if (firstWasSetByShorthand != wasSetByShorthand)
                return true;
        }

        if (m_shorthand.id() != CSSPropertyAll)
            m_longhandValues[i] = WTFMove(value);
    }
    if (specialKeyword) {
        m_result = nameString(*specialKeyword);
        return true;
    }
    if (*firstValueFromShorthand) {
        m_result = (*firstValueFromShorthand)->shorthandValue().cssText();
        return true;
    }
    return false;
}

inline String StyleProperties::serializeLonghandValue(CSSPropertyID propertyID) const
{
    return WebCore::serializeLonghandValue(propertyID, getPropertyCSSValue(propertyID).get());
}

inline String StyleProperties::serializeShorthandValue(CSSPropertyID propertyID) const
{
    return ShorthandSerializer(*this, propertyID).serialize();
}

String StyleProperties::getPropertyValue(CSSPropertyID propertyID) const
{
    return isLonghand(propertyID) ? serializeLonghandValue(propertyID) : serializeShorthandValue(propertyID);
}

inline ShorthandSerializer::ShorthandSerializer(const StyleProperties& properties, CSSPropertyID shorthandID)
    : m_shorthand(shorthandForProperty(shorthandID))
    , m_commonSerializationChecksSuppliedResult(commonSerializationChecks(properties))
{
}

inline CSSPropertyID ShorthandSerializer::longhandProperty(unsigned index) const
{
    ASSERT(index < length());
    return m_shorthand.properties()[index];
}

inline CSSValue& ShorthandSerializer::longhandValue(unsigned index) const
{
    ASSERT(index < length());
    return *m_longhandValues[index];
}

inline String ShorthandSerializer::serializeValue(Longhand longhand)
{
    return WebCore::serializeLonghandValue(longhand.property, longhand.value);
}

inline bool ShorthandSerializer::isInitialValue(Longhand longhand)
{
    return isInitialValueForLonghand(longhand.property, longhand.value);
}

inline unsigned ShorthandSerializer::longhandIndex(unsigned index, CSSPropertyID longhand) const
{
    ASSERT_UNUSED(longhand, longhandProperty(index) == longhand);
    return index;
}

inline CSSValueID ShorthandSerializer::longhandValueID(unsigned index) const
{
    return valueIDExpandingInitialValuePlaceholder(longhandProperty(index), longhandValue(index));
}

inline String ShorthandSerializer::serializeLonghandValue(unsigned index) const
{
    return serializeValue(longhand(index));
}

String ShorthandSerializer::serialize()
{
    if (m_commonSerializationChecksSuppliedResult)
        return WTFMove(m_result);

    switch (m_shorthand.id()) {
    case CSSPropertyAll:
        return String();
    case CSSPropertyAnimation:
    case CSSPropertyBackground:
    case CSSPropertyBackgroundPosition:
    case CSSPropertyMask:
    case CSSPropertyMaskPosition:
    case CSSPropertyTransition:
    case CSSPropertyWebkitMask:
    case CSSPropertyWebkitMaskPosition:
        return serializeLayered();
    case CSSPropertyBorder:
        return serializeBorder(4);
    case CSSPropertyBorderBlock:
    case CSSPropertyBorderInline:
        return serializeBorder(2);
    case CSSPropertyBorderBlockColor:
    case CSSPropertyBorderBlockStyle:
    case CSSPropertyBorderBlockWidth:
    case CSSPropertyBorderInlineColor:
    case CSSPropertyBorderInlineStyle:
    case CSSPropertyBorderInlineWidth:
    case CSSPropertyBorderSpacing:
    case CSSPropertyContainIntrinsicSize:
    case CSSPropertyGap:
    case CSSPropertyInsetBlock:
    case CSSPropertyInsetInline:
    case CSSPropertyMarginBlock:
    case CSSPropertyMarginInline:
    case CSSPropertyOverflow:
    case CSSPropertyOverscrollBehavior:
    case CSSPropertyPaddingBlock:
    case CSSPropertyPaddingInline:
    case CSSPropertyPlaceContent:
    case CSSPropertyPlaceItems:
    case CSSPropertyPlaceSelf:
    case CSSPropertyScrollMarginBlock:
    case CSSPropertyScrollMarginInline:
    case CSSPropertyScrollPaddingBlock:
    case CSSPropertyScrollPaddingInline:
        return serializePair();
    case CSSPropertyBorderBlockEnd:
    case CSSPropertyBorderBlockStart:
    case CSSPropertyBorderBottom:
    case CSSPropertyBorderInlineEnd:
    case CSSPropertyBorderInlineStart:
    case CSSPropertyBorderLeft:
    case CSSPropertyBorderRight:
    case CSSPropertyBorderTop:
    case CSSPropertyColumnRule:
    case CSSPropertyColumns:
    case CSSPropertyFlexFlow:
    case CSSPropertyListStyle:
    case CSSPropertyOutline:
    case CSSPropertyTextEmphasis:
    case CSSPropertyWebkitTextDecoration:
    case CSSPropertyWebkitTextStroke:
        return serializeLonghandsOmittingInitialValues();
    case CSSPropertyBorderColor:
    case CSSPropertyBorderStyle:
    case CSSPropertyBorderWidth:
    case CSSPropertyInset:
    case CSSPropertyMargin:
    case CSSPropertyPadding:
    case CSSPropertyScrollMargin:
    case CSSPropertyScrollPadding:
        return serializeQuad();
    case CSSPropertyBorderImage:
    case CSSPropertyWebkitBorderImage:
    case CSSPropertyWebkitMaskBoxImage:
        return serializeBorderImage();
    case CSSPropertyBorderRadius:
    case CSSPropertyWebkitBorderRadius:
        return serializeBorderRadius();
    case CSSPropertyContainer:
        return serializeLonghandsOmittingTrailingInitialValue(" / ");
    case CSSPropertyFlex:
    case CSSPropertyPerspectiveOrigin:
        return serializeLonghands();
    case CSSPropertyFont:
        return serializeFont();
    case CSSPropertyFontVariant:
        return serializeFontVariant();
    case CSSPropertyFontSynthesis:
        return serializeFontSynthesis();
    case CSSPropertyGrid:
        return serializeGrid();
    case CSSPropertyGridArea:
        return serializeGridArea();
    case CSSPropertyGridColumn:
    case CSSPropertyGridRow:
        return serializeGridRowColumn();
    case CSSPropertyGridTemplate:
        return serializeGridTemplate();
    case CSSPropertyMarker:
        return serializeCommonValue();
    case CSSPropertyOffset:
        return serializeOffset();
    case CSSPropertyPageBreakAfter:
    case CSSPropertyPageBreakBefore:
        return serializePageBreak();
    case CSSPropertyPageBreakInside:
    case CSSPropertyWebkitColumnBreakInside:
        return serializeBreakInside();
    case CSSPropertyTextDecorationSkip:
    case CSSPropertyTextDecoration:
    case CSSPropertyWebkitBackgroundSize:
    case CSSPropertyWebkitPerspective:
    case CSSPropertyWebkitTextOrientation:
        return serializeLonghandValue(0);
    case CSSPropertyTransformOrigin:
        return serializeLonghandsOmittingTrailingInitialValue();
    case CSSPropertyWebkitColumnBreakAfter:
    case CSSPropertyWebkitColumnBreakBefore:
        return serializeColumnBreak();
    default:
        ASSERT_NOT_REACHED();
        return String();
    }
}

std::optional<Color> StyleProperties::propertyAsColor(CSSPropertyID property) const
{
    auto colorValue = getPropertyCSSValue(property);
    if (!is<CSSPrimitiveValue>(colorValue))
        return std::nullopt;

    auto& primitiveColor = downcast<CSSPrimitiveValue>(*colorValue);
    return primitiveColor.isRGBColor() ? primitiveColor.color()
        : CSSParser::parseColorWithoutContext(WebCore::serializeLonghandValue(property, *colorValue));
}

std::optional<CSSValueID> StyleProperties::propertyAsValueID(CSSPropertyID property) const
{
    return valueIDExpandingInitialValuePlaceholder(property, getPropertyCSSValue(property).get());
}

String StyleProperties::getCustomPropertyValue(const String& propertyName) const
{
    RefPtr<CSSValue> value = getCustomPropertyCSSValue(propertyName);
    if (value)
        return value->cssText();
    return String();
}

static std::optional<CSSValueID> fontStretchKeyword(double value)
{
    // If the numeric value does not fit in the fixed point FontSelectionValue, don't convert it to a keyword even if it rounds to a keyword value.
    float valueAsFloat = value;
    FontSelectionValue valueAsFontSelectionValue { valueAsFloat };
    float valueAsFloatAfterRoundTrip = valueAsFontSelectionValue;
    if (value != valueAsFloatAfterRoundTrip)
        return std::nullopt;
    return fontStretchKeyword(valueAsFontSelectionValue);
}

String ShorthandSerializer::serializeFont() const
{
    // If all properties are set to the same system font shorthand, serialize as that.
    // If some but not all properties are, the font shorthand can't represent that, serialize as empty string.
    std::optional<CSSValueID> specialKeyword;
    bool allSpecialKeywords = true;
    for (auto& longhandValue : longhandValues()) {
        auto keyword = valueID(longhandValue);
        if (!CSSPropertyParserHelpers::isSystemFontShorthand(keyword))
            allSpecialKeywords = false;
        else {
            if (specialKeyword.value_or(keyword) != keyword)
                return String();
            specialKeyword = keyword;
        }
    }
    if (specialKeyword)
        return allSpecialKeywords ? nameString(*specialKeyword) : String();

    auto styleIndex = longhandIndex(0, CSSPropertyFontStyle);
    auto capsIndex = longhandIndex(1, CSSPropertyFontVariantCaps);
    auto weightIndex = longhandIndex(2, CSSPropertyFontWeight);
    auto stretchIndex = longhandIndex(3, CSSPropertyFontStretch);
    auto sizeIndex = longhandIndex(4, CSSPropertyFontSize);
    auto lineHeightIndex = longhandIndex(5, CSSPropertyLineHeight);
    auto familyIndex = longhandIndex(6, CSSPropertyFontFamily);

    // Properties after font-family are reset but not represented by the shorthand.
    // If any is not the initial value, serialize as empty string.
    if (!subsequentLonghandsHaveInitialValues(familyIndex + 1))
        return String();

    // Only two values of variant-caps can be serialized in the font shorthand.
    // If the value is anything else, serialize as empty string.
    auto capsKeyword = longhandValueID(capsIndex);
    if (capsKeyword != CSSValueNormal && capsKeyword != CSSValueSmallCaps)
        return String();

    // Font stretch values can only be serialized in the font shorthand as keywords, since percentages are also valid font sizes.
    // If a font stretch percentage can be expressed as a keyword, then do that.
    auto stretchKeyword = longhandValueID(stretchIndex);
    if (stretchKeyword == CSSValueInvalid) {
        auto& stretchValue = downcast<CSSPrimitiveValue>(longhandValue(stretchIndex));
        if (stretchValue.isCalculated() || !stretchValue.isPercentage())
            return String();
        auto keyword = fontStretchKeyword(stretchValue.doubleValue());
        if (!keyword)
            return String();
        stretchKeyword = *keyword;
    }

    bool includeStyle = !isLonghandInitialValue(styleIndex);
    bool includeCaps = capsKeyword != CSSValueNormal;
    bool includeWeight = !isLonghandInitialValue(weightIndex);
    bool includeStretch = stretchKeyword != CSSValueNormal;
    bool includeLineHeight = !isLonghandInitialValue(lineHeightIndex);

    auto style = includeStyle ? serializeLonghandValue(styleIndex) : String();
    auto capsSeparator = includeCaps && includeStyle ? " " : "";
    auto caps = includeCaps ? nameLiteral(capsKeyword) : ""_s;
    auto weightSeparator = includeWeight && (includeStyle || includeCaps) ? " " : "";
    auto weight = includeWeight ? serializeLonghandValue(weightIndex) : String();
    auto stretchSeparator = includeStretch && (includeStyle || includeCaps || includeWeight) ? " " : "";
    auto stretch = includeStretch ? nameLiteral(stretchKeyword) : ""_s;
    auto sizeSeparator = includeStyle || includeCaps || includeWeight || includeStretch ? " " : "";
    auto lineHeightSeparator = includeLineHeight ? " / " : "";
    auto lineHeight = includeLineHeight ? serializeLonghandValue(lineHeightIndex) : String();

    return makeString(style,
        capsSeparator, caps,
        weightSeparator, weight,
        stretchSeparator, stretch,
        sizeSeparator, serializeLonghandValue(sizeIndex),
        lineHeightSeparator, lineHeight,
        ' ', serializeLonghandValue(familyIndex));
}

String ShorthandSerializer::serializeOffset() const
{
    auto positionIndex = longhandIndex(0, CSSPropertyOffsetPosition);
    auto pathIndex = longhandIndex(1, CSSPropertyOffsetPath);
    auto distanceIndex = longhandIndex(2, CSSPropertyOffsetDistance);
    auto rotateIndex = longhandIndex(3, CSSPropertyOffsetRotate);
    auto anchorIndex = longhandIndex(4, CSSPropertyOffsetAnchor);

    bool includeDistance = !isLonghandInitialValue(distanceIndex);
    bool includeRotate = !isLonghandInitialValue(rotateIndex);
    bool includePath = includeDistance || includeRotate || !isLonghandInitialValue(pathIndex);
    bool includePosition = !includePath || !isLonghandInitialValue(positionIndex);
    bool includeAnchor = !isLonghandInitialValue(anchorIndex);

    if (!includeDistance && !includeRotate && !includeAnchor) {
        if (includePosition && includePath)
            return serializeLonghands(2);
        if (includePosition)
            return serializeLonghandValue(positionIndex);
        ASSERT(includePath);
        return serializeLonghandValue(pathIndex);
    }

    auto position = includePosition ? serializeLonghandValue(positionIndex) : String();
    auto pathSeparator = includePosition && includePath ? " " : "";
    auto path = includePath ? serializeLonghandValue(pathIndex) : String();
    auto distanceSeparator = includeDistance ? " " : "";
    auto distance = includeDistance ? serializeLonghandValue(distanceIndex) : String();
    auto rotateSeparator = includeRotate ? " " : "";
    auto rotate = includeRotate ? serializeLonghandValue(rotateIndex) : String();
    auto anchorSeparator = includeAnchor ? " / " : "";
    auto anchor = includeAnchor ? serializeLonghandValue(anchorIndex) : String();

    return makeString(position,
        pathSeparator, path,
        distanceSeparator, distance,
        rotateSeparator, rotate,
        anchorSeparator, anchor);
}

String ShorthandSerializer::serializeFontVariant() const
{
    for (auto& value : longhandValues()) {
        if (CSSPropertyParserHelpers::isSystemFontShorthand(valueID(value)))
            return String();
    }
    if (isLonghandValueNone(longhandIndex(0, CSSPropertyFontVariantLigatures))) {
        for (auto longhand : longhands()) {
            // font-variant cannot represent "font-variant-ligatures: none" along with any other non-normal longhands.
            if (longhand.property != CSSPropertyFontVariantLigatures && !isInitialValue(longhand))
                return String();
        }
    }
    return serializeLonghandsOmittingInitialValues();
}

String ShorthandSerializer::serializeFontSynthesis() const
{
    // font-synthesis: none | [ weight || style || small-caps ]
    ASSERT(length() == 3);

    unsigned bits = !isLonghandValueNone(longhandIndex(0, CSSPropertyFontSynthesisWeight)) << 2
        | !isLonghandValueNone(longhandIndex(1, CSSPropertyFontSynthesisStyle)) << 1
        | !isLonghandValueNone(longhandIndex(2, CSSPropertyFontSynthesisSmallCaps));

    switch (bits) {
    case 0b000: return nameString(CSSValueNone);
    case 0b001: return nameString(CSSValueSmallCaps);
    case 0b010: return nameString(CSSValueStyle);
    case 0b011: return "style small-caps"_s;
    case 0b100: return nameString(CSSValueWeight);
    case 0b101: return "weight small-caps"_s;
    case 0b110: return "weight style"_s;
    case 0b111: return "weight style small-caps"_s;
    }

    ASSERT_NOT_REACHED();
    return String();
}

String ShorthandSerializer::serializePair() const
{
    ASSERT(length() == 2);
    auto first = serializeLonghandValue(0);
    auto second = serializeLonghandValue(1);
    if (first != second)
        return makeString(first, ' ', second);
    return first;
}

String ShorthandSerializer::serializeQuad() const
{
    ASSERT(length() == 4);
    auto top = serializeLonghandValue(0);
    auto right = serializeLonghandValue(1);
    auto bottom = serializeLonghandValue(2);
    auto left = serializeLonghandValue(3);
    if (left != right)
        return makeString(top, ' ', right, ' ', bottom, ' ', left);
    if (bottom != top)
        return makeString(top, ' ', right, ' ', bottom);
    if (right != top)
        return makeString(top, ' ', right);
    return top;
}

class LayerValues {
public:
    explicit LayerValues(const StylePropertyShorthand& shorthand)
        : m_shorthand(shorthand)
    {
        ASSERT(m_shorthand.length() <= maxShorthandLength);
    }

    void set(unsigned index, CSSValue* value, bool skipSerializing = false)
    {
        ASSERT(index < m_shorthand.length());
        m_skipSerializing[index] = skipSerializing
            || !value || isInitialValueForLonghand(m_shorthand.properties()[index], *value);
        m_values[index] = value;
    }

    bool& skip(unsigned index)
    {
        ASSERT(index < m_shorthand.length());
        return m_skipSerializing[index];
    }

    std::optional<CSSValueID> valueID(unsigned index) const
    {
        ASSERT(index < m_shorthand.length());
        return valueIDExpandingInitialValuePlaceholder(m_shorthand.properties()[index], m_values[index].get());
    }

    CSSValueID valueIDIncludingCustomIdent(unsigned index) const
    {
        auto* value = dynamicDowncast<CSSPrimitiveValue>(m_values[index].get());
        if (value && value->isCustomIdent())
            return cssValueKeywordID(value->stringValue());
        return valueID(index).value_or(CSSValueInvalid);
    }

    bool equalValueIDs(unsigned indexA, unsigned indexB) const
    {
        auto valueA = valueID(indexA);
        auto valueB = valueID(indexB);
        return valueA && valueB && *valueA == *valueB;
    }

    bool isValueID(unsigned index) const
    {
        auto result = valueID(index);
        return result && *result != CSSValueInvalid;
    }

    bool isPair(unsigned index) const
    {
        // This returns false for implicit initial values that are pairs, which is OK for now.
        ASSERT(index < m_shorthand.length());
        auto value = dynamicDowncast<CSSPrimitiveValue>(m_values[index].get());
        return value && value->isPair();
    }

    void serialize(StringBuilder& builder) const
    {
        // If all are skipped, then serialize the first.
        auto begin = std::begin(m_skipSerializing);
        auto end = begin + m_shorthand.length();
        bool allSkipped = std::find(begin, end, false) == end;

        auto separator = builder.isEmpty() ? ""_s : ", "_s;
        for (unsigned j = 0; j < m_shorthand.length(); j++) {
            if (allSkipped ? j : m_skipSerializing[j])
                continue;
            auto longhand = m_shorthand.properties()[j];
            if (longhand == CSSPropertyBackgroundSize || longhand == CSSPropertyMaskSize)
                separator = " / "_s;
            if (auto& value = m_values[j])
                builder.append(separator, serializeLonghandValue(longhand, *value));
            else
                builder.append(separator, initialValueTextForLonghand(longhand));
            separator = " "_s;
        }
    }

private:
    const StylePropertyShorthand& m_shorthand;
    bool m_skipSerializing[maxShorthandLength] { };
    RefPtr<CSSValue> m_values[maxShorthandLength];
};

String ShorthandSerializer::serializeLayered() const
{
    size_t numLayers = 1;
    for (auto& value : longhandValues()) {
        if (value.isBaseValueList())
            numLayers = std::max(downcast<CSSValueList>(value).length(), numLayers);
    }

    StringBuilder result;
    for (size_t i = 0; i < numLayers; i++) {
        LayerValues layerValues { m_shorthand };

        for (unsigned j = 0; j < length(); j++) {
            auto& value = longhandValue(j);
            if (value.isBaseValueList())
                layerValues.set(j, downcast<CSSValueList>(value).item(i));
            else {
                // Color is only in the last layer. Other singletons are only in the first.
                auto singletonLayer = longhandProperty(j) == CSSPropertyBackgroundColor ? numLayers - 1 : 0;
                layerValues.set(j, &value, i != singletonLayer);
            }
        }

        for (unsigned j = 0; j < length(); j++) {
            auto longhand = longhandProperty(j);

            // A single box value sets both background-origin and background-clip.
            // A single geometry-box value sets both mask-origin and mask-clip.
            // A single geometry-box value sets both mask-origin and -webkit-mask-clip.
            if (longhand == CSSPropertyBackgroundClip || longhand == CSSPropertyMaskClip || longhand == CSSPropertyWebkitMaskClip) {
                // The previous property is origin.
                ASSERT(j >= 1);
                ASSERT(longhandProperty(j - 1) == CSSPropertyBackgroundOrigin
                    || longhandProperty(j - 1) == CSSPropertyMaskOrigin);
                if (layerValues.equalValueIDs(j - 1, j)) {
                    // If the two are the same, one value sets both.
                    if (!layerValues.skip(j - 1) && !layerValues.skip(j))
                        layerValues.skip(j) = true;
                } else if (!layerValues.skip(j - 1) || !layerValues.skip(j)) {
                    // If the two are different, both need to be serialized, except in the special case of no-clip.
                    if (layerValues.valueID(j) != CSSValueNoClip) {
                        layerValues.skip(j - 1) = false;
                        layerValues.skip(j) = false;
                    }
                }
            }

            if (layerValues.skip(j))
                continue;

            // The syntax for background-size means that if it is present, background-position must be too.
            // The syntax for mask-size means that if it is present, mask-position must be too.
            if (longhand == CSSPropertyBackgroundSize || longhand == CSSPropertyMaskSize) {
                // The previous properties are X and Y.
                ASSERT(j >= 2);
                ASSERT(longhandProperty(j - 2) == CSSPropertyBackgroundPositionX
                    || longhandProperty(j - 2) == CSSPropertyWebkitMaskPositionX);
                ASSERT(longhandProperty(j - 1) == CSSPropertyBackgroundPositionY
                    || longhandProperty(j - 1) == CSSPropertyWebkitMaskPositionY);
                layerValues.skip(j - 2) = false;
                layerValues.skip(j - 1) = false;
            }

            // A single background-position value (identifier or numeric) sets the other value to center.
            // A single mask-position value (identifier or numeric) sets the other value to center.
            // Order matters when one is numeric, but not when both are identifiers.
            if (longhand == CSSPropertyBackgroundPositionY || longhand == CSSPropertyWebkitMaskPositionY) {
                // The previous property is X.
                ASSERT(j >= 1);
                ASSERT(longhandProperty(j - 1) == CSSPropertyBackgroundPositionX
                    || longhandProperty(j - 1) == CSSPropertyWebkitMaskPositionX);
                if (layerValues.valueID(j - 1) == CSSValueCenter && layerValues.isValueID(j))
                    layerValues.skip(j - 1) = true;
                else if (layerValues.valueID(j) == CSSValueCenter && !layerValues.isPair(j - 1)) {
                    layerValues.skip(j - 1) = false;
                    layerValues.skip(j) = true;
                }
            }

            // The first value in each animation shorthand that can be parsed as a time is assigned to
            // animation-duration, and the second is assigned to animation-delay, so we must serialize
            // both if we are serializing animation-delay.
            if (longhand == CSSPropertyAnimationDelay)
                layerValues.skip(longhandIndex(0, CSSPropertyAnimationDuration)) = false;
        }

        // In the animation shorthand, if the value of animation-name could be parsed as one of
        // the other longhands that longhand must be serialized to avoid ambiguity.
        if (m_shorthand.id() == CSSPropertyAnimation) {
            auto animationTimingFunctionIndex = longhandIndex(1, CSSPropertyAnimationTimingFunction);
            auto animationIterationCountIndex = longhandIndex(3, CSSPropertyAnimationIterationCount);
            auto animationDirectionIndex = longhandIndex(4, CSSPropertyAnimationDirection);
            auto animationFillModeIndex = longhandIndex(5, CSSPropertyAnimationFillMode);
            auto animationPlayStateIndex = longhandIndex(6, CSSPropertyAnimationPlayState);
            auto animationNameIndex = longhandIndex(7, CSSPropertyAnimationName);
            if (!layerValues.skip(animationNameIndex)) {
                switch (layerValues.valueIDIncludingCustomIdent(animationNameIndex)) {
                case CSSValueAlternate:
                case CSSValueAlternateReverse:
                case CSSValueNormal:
                case CSSValueReverse:
                    layerValues.skip(animationDirectionIndex) = false;
                    break;
                case CSSValueBackwards:
                case CSSValueBoth:
                case CSSValueForwards:
                    layerValues.skip(animationFillModeIndex) = false;
                    break;
                case CSSValueEase:
                case CSSValueEaseIn:
                case CSSValueEaseInOut:
                case CSSValueEaseOut:
                case CSSValueLinear:
                case CSSValueStepEnd:
                case CSSValueStepStart:
                    layerValues.skip(animationTimingFunctionIndex) = false;
                    break;
                case CSSValueInfinite:
                    layerValues.skip(animationIterationCountIndex) = false;
                    break;
                case CSSValuePaused:
                case CSSValueRunning:
                    layerValues.skip(animationPlayStateIndex) = false;
                    break;
                default:
                    break;
                }
            }
        }

        layerValues.serialize(result);
    }

    return result.toString();
}

String ShorthandSerializer::serializeGridTemplate() const
{
    ASSERT(length() >= 3);

    auto rowsIndex = longhandIndex(0, CSSPropertyGridTemplateRows);
    auto columnsIndex = longhandIndex(1, CSSPropertyGridTemplateColumns);
    auto areasIndex = longhandIndex(2, CSSPropertyGridTemplateAreas);

    auto* areasValue = dynamicDowncast<CSSGridTemplateAreasValue>(longhandValue(areasIndex));
    if (!areasValue) {
        if (isLonghandValueNone(rowsIndex) && isLonghandValueNone(columnsIndex))
            return nameString(CSSValueNone);
        return serializeLonghands(2, " / ");
    }

    // FIXME: We must remove the check below and instead check that values can be represented.
    // We only want to try serializing the interleaved areas/templates
    // format if it was set from this shorthand, since that automatically
    // excludes values that can't be represented in this format (subgrid,
    // and the repeat() function).
    if (!m_gridTemplateAreasWasSetFromShorthand)
        return String();

    StringBuilder result;
    unsigned row = 0;
    for (auto& currentValue : downcast<CSSValueList>(longhandValue(rowsIndex))) {
        if (!result.isEmpty())
            result.append(' ');
        if (auto lineNames = dynamicDowncast<CSSGridLineNamesValue>(currentValue.get()))
            result.append(lineNames->customCSSText());
        else {
            result.append('"', areasValue->stringForRow(row), '"');
            if (!isValueID(currentValue, CSSValueAuto))
                result.append(' ', currentValue->cssText());
            row++;
        }
    }
    if (!isLonghandValueNone(columnsIndex))
        result.append(" / ", serializeLonghandValue(columnsIndex));
    return result.toString();
}

static bool gridAutoFlowContains(CSSValue& autoFlow, CSSValueID id)
{
    if (is<CSSValueList>(autoFlow)) {
        for (auto& currentValue : downcast<CSSValueList>(autoFlow)) {
            if (isValueID(currentValue, id))
                return true;
        }
        return false;
    }
    return isValueID(autoFlow, id);
}

String ShorthandSerializer::serializeGrid() const
{
    ASSERT(length() == 6);

    auto rowsIndex = longhandIndex(0, CSSPropertyGridTemplateRows);
    auto columnsIndex = longhandIndex(1, CSSPropertyGridTemplateColumns);
    auto areasIndex = longhandIndex(2, CSSPropertyGridTemplateAreas);
    auto autoFlowIndex = longhandIndex(3, CSSPropertyGridAutoFlow);
    auto autoRowsIndex = longhandIndex(4, CSSPropertyGridAutoRows);
    auto autoColumnsIndex = longhandIndex(5, CSSPropertyGridAutoColumns);

    auto& autoColumns = longhandValue(autoColumnsIndex);
    auto& autoRows = longhandValue(autoRowsIndex);
    auto& autoFlow = longhandValue(autoFlowIndex);

    if (isValueIDIncludingList(autoColumns, CSSValueAuto) && isValueIDIncludingList(autoRows, CSSValueAuto) && isValueIDIncludingList(autoFlow, CSSValueRow))
        return serializeGridTemplate();

    if (!isLonghandValueNone(areasIndex))
        return String();

    auto& rows = longhandValue(rowsIndex);
    auto& columns = longhandValue(columnsIndex);

    bool autoFlowContainsDense = gridAutoFlowContains(autoFlow, CSSValueDense);
    auto dense = autoFlowContainsDense ? " dense" : "";

    if (gridAutoFlowContains(autoFlow, CSSValueColumn)) {
        if (!isValueIDIncludingList(autoRows, CSSValueAuto) || !isValueIDIncludingList(columns, CSSValueNone))
            return String();

        if (isValueIDIncludingList(autoColumns, CSSValueAuto))
            return makeString(serializeLonghandValue(rowsIndex), " / auto-flow", dense);
        return makeString(serializeLonghandValue(rowsIndex), " / auto-flow", dense, ' ', serializeLonghandValue(autoColumnsIndex));
    }

    if (!gridAutoFlowContains(autoFlow, CSSValueRow) && !autoFlowContainsDense)
        return String();
    if (!isValueIDIncludingList(autoColumns, CSSValueAuto) || !isValueIDIncludingList(rows, CSSValueNone))
        return String();

    if (isValueIDIncludingList(autoRows, CSSValueAuto))
        return makeString("auto-flow", dense, " / ", serializeLonghandValue(columnsIndex));
    return makeString("auto-flow", dense, " ", serializeLonghandValue(autoRowsIndex), " / ", serializeLonghandValue(columnsIndex));
}

static bool isCustomIdentValue(const CSSValue& value)
{
    return is<CSSPrimitiveValue>(value) && downcast<CSSPrimitiveValue>(value).isCustomIdent();
}

static bool canOmitTrailingGridAreaValue(CSSValue& value, CSSValue& trailing)
{
    if (isCustomIdentValue(value))
        return isCustomIdentValue(trailing) && value.cssText() == trailing.cssText();
    return isValueID(trailing, CSSValueAuto);
}

String ShorthandSerializer::serializeGridRowColumn() const
{
    ASSERT(length() == 2);
    return serializeLonghands(canOmitTrailingGridAreaValue(longhandValue(0), longhandValue(1)) ? 1 : 2, " / ");
}

String ShorthandSerializer::serializeGridArea() const
{
    ASSERT(length() == 4);
    unsigned longhandsToSerialize = 4;
    if (canOmitTrailingGridAreaValue(longhandValue(1), longhandValue(3))) {
        --longhandsToSerialize;
        if (canOmitTrailingGridAreaValue(longhandValue(0), longhandValue(2))) {
            --longhandsToSerialize;
            if (canOmitTrailingGridAreaValue(longhandValue(0), longhandValue(1)))
                --longhandsToSerialize;
        }
    }
    return serializeLonghands(longhandsToSerialize, " / ");
}

String ShorthandSerializer::serializeLonghands() const
{
    return serializeLonghands(length());
}

String ShorthandSerializer::serializeLonghands(unsigned lengthLimit, const char* separator) const
{
    ASSERT(lengthLimit <= length());
    switch (lengthLimit) {
    case 1:
        return serializeLonghandValue(0);
    case 2:
        return makeString(serializeLonghandValue(0), separator, serializeLonghandValue(1));
    case 3:
        return makeString(serializeLonghandValue(0), separator, serializeLonghandValue(1), separator, serializeLonghandValue(2));
    case 4:
        return makeString(serializeLonghandValue(0), separator, serializeLonghandValue(1), separator, serializeLonghandValue(2), separator, serializeLonghandValue(3));
    default:
        StringBuilder result;
        auto prefix = "";
        for (unsigned i = 0; i < lengthLimit; ++i)
            result.append(std::exchange(prefix, separator), serializeLonghandValue(i));
        return result.toString();
    }
}

String ShorthandSerializer::serializeLonghandsOmittingInitialValues() const
{
    StringBuilder result;
    auto prefix = "";
    for (auto longhand : longhands()) {
        if (!isInitialValue(longhand))
            result.append(std::exchange(prefix, " "), serializeValue(longhand));
    }
    return result.isEmpty() ? serializeLonghandValue(0) : result.toString();
}

String ShorthandSerializer::serializeLonghandsOmittingTrailingInitialValue(const char* separator) const
{
    ASSERT(length() >= 2);
    return serializeLonghands(length() - isLonghandInitialValue(length() - 1), separator);
}

String ShorthandSerializer::serializeCommonValue(unsigned startIndex, unsigned count) const
{
    String result;
    for (unsigned i = 0; i < count; ++i) {
        String text = serializeLonghandValue(startIndex + i);
        if (result.isNull())
            result = text;
        else if (result != text)
            return String();
    }
    return result;
}

inline String ShorthandSerializer::serializeCommonValue() const
{
    return serializeCommonValue(0, length());
}

String ShorthandSerializer::serializeBorderImage() const
{
    ASSERT(length() == 5);
    StringBuilder result;
    bool omittedSlice = false;
    bool omittedWidth = false;
    auto separator = "";
    for (auto longhand : longhands()) {
        if (isInitialValue(longhand)) {
            if (longhand.property == CSSPropertyBorderImageSlice || longhand.property == CSSPropertyWebkitMaskBoxImageSlice)
                omittedSlice = true;
            else if (longhand.property == CSSPropertyBorderImageWidth || longhand.property == CSSPropertyWebkitMaskBoxImageWidth)
                omittedWidth = true;
            continue;
        }
        if (omittedSlice && (longhand.property == CSSPropertyBorderImageWidth || longhand.property == CSSPropertyBorderImageOutset || longhand.property == CSSPropertyWebkitMaskBoxImageWidth || longhand.property == CSSPropertyWebkitMaskBoxImageOutset))
            return String();

        String valueText;

        // -webkit-border-image has a legacy behavior that makes fixed border slices also set the border widths.
        if (auto* width = dynamicDowncast<CSSBorderImageWidthValue>(longhand.value)) {
            Quad& widths = width->widths();
            bool overridesBorderWidths = m_shorthand.id() == CSSPropertyWebkitBorderImage && (widths.top()->isLength() || widths.right()->isLength() || widths.bottom()->isLength() || widths.left()->isLength());
            if (overridesBorderWidths != width->m_overridesBorderWidths)
                return String();
            valueText = widths.cssText();
        } else
            valueText = serializeValue(longhand);

        // Append separator and text.
        if (longhand.property == CSSPropertyBorderImageWidth || longhand.property == CSSPropertyWebkitMaskBoxImageWidth)
            separator = " / ";
        else if (longhand.property == CSSPropertyBorderImageOutset || longhand.property == CSSPropertyWebkitMaskBoxImageOutset)
            separator = omittedWidth ? " / / " : " / ";
        result.append(separator, valueText);
        separator = " ";
    }
    if (result.isEmpty())
        return nameString(CSSValueNone);
    return result.toString();
}

String ShorthandSerializer::serializeBorderRadius() const
{
    ASSERT(length() == 4);
    RefPtr<CSSPrimitiveValue> horizontalRadii[4];
    RefPtr<CSSPrimitiveValue> verticalRadii[4];
    for (unsigned i = 0; i < 4; ++i) {
        auto pair = downcast<CSSPrimitiveValue>(longhandValue(i)).pairValue();
        horizontalRadii[i] = pair->first();
        verticalRadii[i] = pair->second();
    }

    bool serializeBoth = false;
    for (unsigned i = 0; i < 4; ++i) {
        if (!horizontalRadii[i]->equals(*verticalRadii[i])) {
            serializeBoth = true;
            break;
        }
    }

    StringBuilder result;
    auto serializeRadii = [&](const auto (&r)[4]) {
        if (!r[3]->equals(*r[1]))
            result.append(r[0]->cssText(), ' ', r[1]->cssText(), ' ', r[2]->cssText(), ' ', r[3]->cssText());
        else if (!r[2]->equals(*r[0]) || (m_shorthand.id() == CSSPropertyWebkitBorderRadius && !serializeBoth && !r[1]->equals(*r[0])))
            result.append(r[0]->cssText(), ' ', r[1]->cssText(), ' ', r[2]->cssText());
        else if (!r[1]->equals(*r[0]))
            result.append(r[0]->cssText(), ' ', r[1]->cssText());
        else
            result.append(r[0]->cssText());
    };
    serializeRadii(horizontalRadii);
    if (serializeBoth) {
        result.append(" / ");
        serializeRadii(verticalRadii);
    }
    return result.toString();
}

String ShorthandSerializer::serializeBorder(unsigned sectionLength) const
{
    ASSERT(3 * sectionLength <= length());

    bool mustSerializeAsEmptyString = false;
    auto serializeSection = [&](unsigned index, CSSValueID defaultValue) {
        auto value = serializeCommonValue(index * sectionLength, sectionLength);
        if (value.isNull())
            mustSerializeAsEmptyString = true;
        else if (value == nameLiteral(defaultValue))
            value = String();
        return value;
    };
    auto width = serializeSection(0, CSSValueMedium); // widths
    auto style = serializeSection(1, CSSValueNone); // styles
    auto color = serializeSection(2, CSSValueCurrentcolor); // colors
    if (mustSerializeAsEmptyString || !subsequentLonghandsHaveInitialValues(3 * sectionLength))
        return String();

    unsigned bits = !width.isNull() << 2 | !style.isNull() << 1 | !color.isNull();
    switch (bits) {
    case 0b000: return nameString(CSSValueMedium);
    case 0b001: return color;
    case 0b010: return style;
    case 0b011: return makeString(style, ' ', color);
    case 0b100: return width;
    case 0b101: return makeString(width, ' ', color);
    case 0b110: return makeString(width, ' ', style);
    case 0b111: return makeString(width, ' ', style, ' ', color);
    }

    ASSERT_NOT_REACHED();
    return String();
}

String ShorthandSerializer::serializeBreakInside() const
{
    auto keyword = longhandValueID(0);
    switch (keyword) {
    case CSSValueAuto:
    case CSSValueAvoid:
        return nameString(keyword);
    default:
        return String();
    }
}

String ShorthandSerializer::serializePageBreak() const
{
    auto keyword = longhandValueID(0);
    switch (keyword) {
    case CSSValuePage:
        return nameString(CSSValueAlways);
    case CSSValueAuto:
    case CSSValueAvoid:
    case CSSValueLeft:
    case CSSValueRight:
        return nameString(keyword);
    default:
        return String();
    }
}

String ShorthandSerializer::serializeColumnBreak() const
{
    switch (longhandValueID(0)) {
    case CSSValueColumn:
        return nameString(CSSValueAlways);
    case CSSValueAvoidColumn:
        return nameString(CSSValueAvoid);
    case CSSValueAuto:
        return nameString(CSSValueAuto);
    default:
        return String();
    }
}

bool ShorthandSerializer::subsequentLonghandsHaveInitialValues(unsigned startIndex) const
{
    for (unsigned i = startIndex; i < length(); ++i) {
        if (!isLonghandInitialValue(i))
            return false;
    }
    return true;
}

RefPtr<CSSValue> StyleProperties::getPropertyCSSValue(CSSPropertyID propertyID) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return nullptr;
    auto property = propertyAt(foundPropertyIndex);
    auto value = property.value();
    // System fonts are represented as CSSPrimitiveValue for various font subproperties, but these must serialize as the empty string.
    // It might be better to implement this as a special CSSValue type instead of turning them into null here.
    if (property.shorthandID() == CSSPropertyFont && CSSPropertyParserHelpers::isSystemFontShorthand(valueID(value)))
        return nullptr;
    return value;
}

RefPtr<CSSValue> StyleProperties::getCustomPropertyCSSValue(const String& propertyName) const
{
    int foundPropertyIndex = findCustomPropertyIndex(propertyName);
    if (foundPropertyIndex == -1)
        return nullptr;
    return propertyAt(foundPropertyIndex).value();
}

inline bool MutableStyleProperties::removeShorthandProperty(CSSPropertyID propertyID, String* returnText)
{
    // FIXME: Use serializeShorthandValue here to return the value of the removed shorthand as we do when removing a longhand.
    if (returnText)
        *returnText = String();
    auto shorthand = shorthandForProperty(propertyID);
    return removePropertiesInSet(shorthand.properties(), shorthand.length());
}

bool MutableStyleProperties::removePropertyAtIndex(int index, String* returnText)
{
    if (index == -1) {
        if (returnText)
            *returnText = String();
        return false;
    }

    if (returnText) {
        auto property = propertyAt(index);
        *returnText = WebCore::serializeLonghandValue(property.id(), *property.value());
    }

    // A more efficient removal strategy would involve marking entries as empty
    // and sweeping them when the vector grows too big.
    m_propertyVector.remove(index);
    return true;
}

inline bool MutableStyleProperties::removeLonghandProperty(CSSPropertyID propertyID, String* returnText)
{
    return removePropertyAtIndex(findPropertyIndex(propertyID), returnText);
}

bool MutableStyleProperties::removeProperty(CSSPropertyID propertyID, String* returnText)
{
    return isLonghand(propertyID) ? removeLonghandProperty(propertyID, returnText) : removeShorthandProperty(propertyID, returnText);
}

bool MutableStyleProperties::removeCustomProperty(const String& propertyName, String* returnText)
{
    return removePropertyAtIndex(findCustomPropertyIndex(propertyName), returnText);
}

bool StyleProperties::propertyIsImportant(CSSPropertyID propertyID) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex != -1)
        return propertyAt(foundPropertyIndex).isImportant();

    auto shorthand = shorthandForProperty(propertyID);
    if (!shorthand.length())
        return false;

    for (auto longhand : shorthand) {
        if (!propertyIsImportant(longhand))
            return false;
    }
    return true;
}

bool StyleProperties::customPropertyIsImportant(const String& propertyName) const
{
    int foundPropertyIndex = findCustomPropertyIndex(propertyName);
    if (foundPropertyIndex != -1)
        return propertyAt(foundPropertyIndex).isImportant();
    return false;
}

String StyleProperties::getPropertyShorthand(CSSPropertyID propertyID) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return String();
    return nameString(propertyAt(foundPropertyIndex).shorthandID());
}

bool StyleProperties::isPropertyImplicit(CSSPropertyID propertyID) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return false;
    return propertyAt(foundPropertyIndex).isImplicit();
}

bool MutableStyleProperties::setProperty(CSSPropertyID propertyID, const String& value, bool important, CSSParserContext parserContext, bool* didFailParsing)
{
    if (!isExposed(propertyID, &parserContext.propertySettings) && !isInternal(propertyID)) {
        // Allow internal properties as we use them to handle certain DOM-exposed values
        // (e.g. -webkit-font-size-delta from execCommand('FontSizeDelta')).
        ASSERT_NOT_REACHED();
        return false;
    }

    // Setting the value to an empty string just removes the property in both IE and Gecko.
    // Setting it to null seems to produce less consistent results, but we treat it just the same.
    if (value.isEmpty())
        return removeProperty(propertyID);

    parserContext.mode = cssParserMode();

    // When replacing an existing property value, this moves the property to the end of the list.
    // Firefox preserves the position, and MSIE moves the property to the beginning.
    auto parseResult = CSSParser::parseValue(*this, propertyID, value, important, parserContext);
    if (didFailParsing)
        *didFailParsing = parseResult == CSSParser::ParseResult::Error;
    return parseResult == CSSParser::ParseResult::Changed;
}

bool MutableStyleProperties::setProperty(CSSPropertyID propertyID, const String& value, bool important, bool* didFailParsing)
{
    CSSParserContext parserContext(cssParserMode());
    return setProperty(propertyID, value, important, parserContext, didFailParsing);
}

bool MutableStyleProperties::setCustomProperty(const String& propertyName, const String& value, bool important, CSSParserContext parserContext)
{
    // Setting the value to an empty string just removes the property in both IE and Gecko.
    // Setting it to null seems to produce less consistent results, but we treat it just the same.
    if (value.isEmpty())
        return removeCustomProperty(propertyName);

    parserContext.mode = cssParserMode();

    auto propertyNameAtom = AtomString { propertyName };

    // When replacing an existing property value, this moves the property to the end of the list.
    // Firefox preserves the position, and MSIE moves the property to the beginning.
    return CSSParser::parseCustomPropertyValue(*this, propertyNameAtom, value, important, parserContext) == CSSParser::ParseResult::Changed;
}

void MutableStyleProperties::setProperty(CSSPropertyID propertyID, RefPtr<CSSValue>&& value, bool important)
{
    if (isLonghand(propertyID)) {
        setProperty(CSSProperty(propertyID, WTFMove(value), important));
        return;
    }
    auto shorthand = shorthandForProperty(propertyID);
    removePropertiesInSet(shorthand.properties(), shorthand.length());
    for (auto longhand : shorthand)
        m_propertyVector.append(CSSProperty(longhand, value.copyRef(), important));
}

bool MutableStyleProperties::canUpdateInPlace(const CSSProperty& property, CSSProperty* toReplace) const
{
    // If the property is in a logical property group, we can't just update the value in-place,
    // because afterwards there might be another property of the same group but different mapping logic.
    // In that case the latter might override the former, so setProperty would have no effect.
    CSSPropertyID id = property.id();
    if (CSSProperty::isInLogicalPropertyGroup(id)) {
        ASSERT(toReplace >= m_propertyVector.begin());
        ASSERT(toReplace < m_propertyVector.end());
        for (CSSProperty* it = toReplace + 1; it != m_propertyVector.end(); ++it) {
            if (CSSProperty::areInSameLogicalPropertyGroupWithDifferentMappingLogic(id, it->id()))
                return false;
        }
    }
    return true;
}

bool MutableStyleProperties::setProperty(const CSSProperty& property, CSSProperty* slot)
{
    ASSERT(property.id() == CSSPropertyCustom || isLonghand(property.id()));
    auto* toReplace = slot;
    if (!slot) {
        if (property.id() == CSSPropertyCustom) {
            if (property.value())
                toReplace = findCustomCSSPropertyWithName(downcast<CSSCustomPropertyValue>(*property.value()).name());
        } else
            toReplace = findCSSPropertyWithID(property.id());
    }
    if (toReplace) {
        if (canUpdateInPlace(property, toReplace)) {
            if (*toReplace == property)
                return false;
            *toReplace = property;
            return true;
        }
        m_propertyVector.remove(toReplace - m_propertyVector.begin());
    }
    m_propertyVector.append(property);
    return true;
}

bool MutableStyleProperties::setProperty(CSSPropertyID propertyID, CSSValueID identifier, bool important)
{
    ASSERT(isLonghand(propertyID));
    return setProperty(CSSProperty(propertyID, CSSPrimitiveValue::create(identifier), important));
}

bool MutableStyleProperties::parseDeclaration(const String& styleDeclaration, CSSParserContext context)
{
    auto oldProperties = WTFMove(m_propertyVector);
    m_propertyVector.clear();

    context.mode = cssParserMode();

    CSSParser parser(context);
    parser.parseDeclaration(*this, styleDeclaration);

    // We could do better. Just changing property order does not require style invalidation.
    return oldProperties != m_propertyVector;
}

bool MutableStyleProperties::addParsedProperties(const ParsedPropertyVector& properties)
{
    bool anyChanged = false;
    m_propertyVector.reserveCapacity(m_propertyVector.size() + properties.size());
    for (const auto& property : properties) {
        if (addParsedProperty(property))
            anyChanged = true;
    }

    return anyChanged;
}

bool MutableStyleProperties::addParsedProperty(const CSSProperty& property)
{
    if (property.id() == CSSPropertyCustom) {
        if ((property.value() && !customPropertyIsImportant(downcast<CSSCustomPropertyValue>(*property.value()).name())) || property.isImportant())
            return setProperty(property);
        return false;
    }
    return setProperty(property);
}

String StyleProperties::asText() const
{
    return asTextInternal().toString();
}

AtomString StyleProperties::asTextAtom() const
{
    return asTextInternal().toAtomString();
}

static constexpr bool canUseShorthandForLonghand(CSSPropertyID shorthandID, CSSPropertyID longhandID)
{
    ASSERT(isShorthandCSSProperty(shorthandID));
    ASSERT(isLonghand(longhandID));
    switch (shorthandID) {
    // We are not yet using the CSSPropertyFont shorthand here because our editing code is currently incompatible.
    case CSSPropertyFont:
        return false;

    // Avoid legacy shorthands according to https://www.w3.org/TR/css-cascade-5/#legacy-shorthand
    case CSSPropertyPageBreakAfter:
    case CSSPropertyPageBreakBefore:
    case CSSPropertyPageBreakInside:
    case CSSPropertyWebkitBackgroundSize:
    case CSSPropertyWebkitBorderRadius:
    case CSSPropertyWebkitColumnBreakAfter:
    case CSSPropertyWebkitColumnBreakBefore:
    case CSSPropertyWebkitColumnBreakInside:
    case CSSPropertyWebkitMaskPosition:
    case CSSPropertyWebkitPerspective:
    case CSSPropertyWebkitTextDecoration:
    case CSSPropertyWebkitTextOrientation:
        return false;

    // No other browser currently supports text-decoration-skip, so it's currently more web
    // compatible to avoid collapsing text-decoration-skip-ink, its only longhand.
    case CSSPropertyTextDecorationSkip:
        return false;

    // FIXME: -webkit-mask is a legacy shorthand but it's used to serialize -webkit-mask-clip,
    // which should be a legacy shorthand of mask-clip, but it's implemented as a longhand.
    case CSSPropertyWebkitMask:
        return longhandID == CSSPropertyWebkitMaskClip;

    // FIXME: more mask nonsense.
    case CSSPropertyMask:
        return longhandID != CSSPropertyMaskComposite && longhandID != CSSPropertyMaskMode && longhandID != CSSPropertyMaskSize;

    // FIXME: If font-variant-ligatures is none, this depends on the value of the longhand.
    case CSSPropertyFontVariant:
    // FIXME: These shorthands are avoided for unknown legacy reasons, probably shouldn't be avoided.
    case CSSPropertyBorderBlockEnd:
    case CSSPropertyBorderBlockStart:
    case CSSPropertyBorderBottom:
    case CSSPropertyBorderInlineEnd:
    case CSSPropertyBorderInlineStart:
    case CSSPropertyBorderLeft:
    case CSSPropertyBorderRight:
    case CSSPropertyBorderTop:
    case CSSPropertyColumnRule:
    case CSSPropertyColumns:
    case CSSPropertyContainer:
    case CSSPropertyFontSynthesis:
    case CSSPropertyGap:
    case CSSPropertyGridArea:
    case CSSPropertyGridColumn:
    case CSSPropertyGridRow:
    case CSSPropertyMaskPosition:
    case CSSPropertyOffset:
    case CSSPropertyPlaceContent:
    case CSSPropertyPlaceItems:
    case CSSPropertyPlaceSelf:
    case CSSPropertyTextEmphasis:
    case CSSPropertyWebkitTextStroke:
        return false;

    default:
        return true;
    }
}

StringBuilder StyleProperties::asTextInternal() const
{
    StringBuilder result;

    constexpr unsigned shorthandPropertyCount = lastShorthandProperty - firstShorthandProperty + 1;
    std::bitset<shorthandPropertyCount> shorthandPropertyUsed;
    std::bitset<shorthandPropertyCount> shorthandPropertyAppeared;

    unsigned numDecls = 0;
    for (auto property : *this) {
        auto propertyID = property.id();
        ASSERT(isLonghand(propertyID) || propertyID == CSSPropertyCustom);
        Vector<CSSPropertyID, maxShorthandsForLonghand> shorthands;

        if (is<CSSPendingSubstitutionValue>(property.value())) {
            auto& substitutionValue = downcast<CSSPendingSubstitutionValue>(*property.value());
            shorthands.append(substitutionValue.shorthandPropertyId());
        } else {
            for (auto& shorthand : matchingShorthandsForLonghand(propertyID)) {
                if (canUseShorthandForLonghand(shorthand.id(), propertyID))
                    shorthands.append(shorthand.id());
            }
        }

        String value;
        bool alreadyUsedShorthand = false;
        for (auto& shorthandPropertyID : shorthands) {
            ASSERT(isShorthandCSSProperty(shorthandPropertyID));
            unsigned shorthandPropertyIndex = shorthandPropertyID - firstShorthandProperty;

            ASSERT(shorthandPropertyIndex < shorthandPropertyUsed.size());
            if (shorthandPropertyUsed[shorthandPropertyIndex]) {
                alreadyUsedShorthand = true;
                break;
            }
            if (shorthandPropertyAppeared[shorthandPropertyIndex])
                continue;
            shorthandPropertyAppeared.set(shorthandPropertyIndex);

            value = serializeShorthandValue(shorthandPropertyID);
            if (!value.isNull()) {
                propertyID = shorthandPropertyID;
                shorthandPropertyUsed.set(shorthandPropertyIndex);
                break;
            }
        }
        if (alreadyUsedShorthand)
            continue;

        if (value.isNull())
            value = WebCore::serializeLonghandValue(propertyID, *property.value());

        if (numDecls++)
            result.append(' ');

        if (propertyID == CSSPropertyCustom)
            result.append(downcast<CSSCustomPropertyValue>(*property.value()).name());
        else
            result.append(nameLiteral(propertyID));

        result.append(": ", value, property.isImportant() ? " !important" : "", ';');
    }

    ASSERT(!numDecls ^ !result.isEmpty());
    return result;
}

bool StyleProperties::hasCSSOMWrapper() const
{
    return is<MutableStyleProperties>(*this) && downcast<MutableStyleProperties>(*this).m_cssomWrapper;
}

void MutableStyleProperties::mergeAndOverrideOnConflict(const StyleProperties& other)
{
    for (auto property : other)
        addParsedProperty(property.toCSSProperty());
}

bool StyleProperties::traverseSubresources(const Function<bool(const CachedResource&)>& handler) const
{
    for (auto property : *this) {
        if (property.value()->traverseSubresources(handler))
            return true;
    }
    return false;
}

// This is the list of properties we want to copy in the copyBlockProperties() function.
// It is the list of CSS properties that apply specially to block-level elements.
static const CSSPropertyID blockProperties[] = {
    CSSPropertyOrphans,
    CSSPropertyOverflow, // This can be also be applied to replaced elements
    CSSPropertyColumnCount,
    CSSPropertyColumnGap,
    CSSPropertyRowGap,
    CSSPropertyColumnRuleColor,
    CSSPropertyColumnRuleStyle,
    CSSPropertyColumnRuleWidth,
    CSSPropertyWebkitColumnBreakBefore,
    CSSPropertyWebkitColumnBreakAfter,
    CSSPropertyWebkitColumnBreakInside,
    CSSPropertyColumnWidth,
    CSSPropertyPageBreakAfter,
    CSSPropertyPageBreakBefore,
    CSSPropertyPageBreakInside,
    CSSPropertyTextAlign,
    CSSPropertyTextAlignLast,
    CSSPropertyTextJustify,
    CSSPropertyTextIndent,
    CSSPropertyWidows
};

void MutableStyleProperties::clear()
{
    m_propertyVector.clear();
}

const unsigned numBlockProperties = std::size(blockProperties);

Ref<MutableStyleProperties> StyleProperties::copyBlockProperties() const
{
    return copyPropertiesInSet(blockProperties, numBlockProperties);
}

void MutableStyleProperties::removeBlockProperties()
{
    removePropertiesInSet(blockProperties, numBlockProperties);
}

bool MutableStyleProperties::removePropertiesInSet(const CSSPropertyID* set, unsigned length)
{
    if (m_propertyVector.isEmpty())
        return false;

    // FIXME: This is always used with static sets and in that case constructing the hash repeatedly is pretty pointless.
    HashSet<CSSPropertyID> toRemove;
    toRemove.add(set, set + length);

    return m_propertyVector.removeAllMatching([&toRemove] (const CSSProperty& property) {
        return toRemove.contains(property.id());
    }) > 0;
}

int ImmutableStyleProperties::findPropertyIndex(CSSPropertyID propertyID) const
{
    // Convert here propertyID into an uint16_t to compare it with the metadata's m_propertyID to avoid
    // the compiler converting it to an int multiple times in the loop.
    uint16_t id = static_cast<uint16_t>(propertyID);
    for (int n = m_arraySize - 1 ; n >= 0; --n) {
        if (metadataArray()[n].m_propertyID == id)
            return n;
    }

    return -1;
}

int MutableStyleProperties::findPropertyIndex(CSSPropertyID propertyID) const
{
    // Convert here propertyID into an uint16_t to compare it with the metadata's m_propertyID to avoid
    // the compiler converting it to an int multiple times in the loop.
    uint16_t id = static_cast<uint16_t>(propertyID);
    for (int n = m_propertyVector.size() - 1 ; n >= 0; --n) {
        if (m_propertyVector.at(n).metadata().m_propertyID == id)
            return n;
    }

    return -1;
}

int ImmutableStyleProperties::findCustomPropertyIndex(StringView propertyName) const
{
    for (int n = m_arraySize - 1 ; n >= 0; --n) {
        if (metadataArray()[n].m_propertyID == CSSPropertyCustom) {
            // We found a custom property. See if the name matches.
            auto* value = valueArray()[n].get();
            if (!value)
                continue;
            if (downcast<CSSCustomPropertyValue>(*value).name() == propertyName)
                return n;
        }
    }

    return -1;
}

int MutableStyleProperties::findCustomPropertyIndex(StringView propertyName) const
{
    for (int n = m_propertyVector.size() - 1 ; n >= 0; --n) {
        if (m_propertyVector.at(n).metadata().m_propertyID == CSSPropertyCustom) {
            // We found a custom property. See if the name matches.
            if (!m_propertyVector.at(n).value())
                continue;
            if (downcast<CSSCustomPropertyValue>(*m_propertyVector.at(n).value()).name() == propertyName)
                return n;
        }
    }

    return -1;
}

CSSProperty* MutableStyleProperties::findCSSPropertyWithID(CSSPropertyID propertyID)
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return nullptr;
    return &m_propertyVector.at(foundPropertyIndex);
}

CSSProperty* MutableStyleProperties::findCustomCSSPropertyWithName(const String& propertyName)
{
    int foundPropertyIndex = findCustomPropertyIndex(propertyName);
    if (foundPropertyIndex == -1)
        return nullptr;
    return &m_propertyVector.at(foundPropertyIndex);
}

bool StyleProperties::propertyMatches(CSSPropertyID propertyID, const CSSValue* propertyValue) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return false;
    return propertyAt(foundPropertyIndex).value()->equals(*propertyValue);
}

Ref<MutableStyleProperties> StyleProperties::mutableCopy() const
{
    return adoptRef(*new MutableStyleProperties(*this));
}

Ref<MutableStyleProperties> StyleProperties::copyPropertiesInSet(const CSSPropertyID* set, unsigned length) const
{
    Vector<CSSProperty> list;
    list.reserveInitialCapacity(length);
    for (unsigned i = 0; i < length; ++i) {
        if (auto value = getPropertyCSSValue(set[i]))
            list.uncheckedAppend(CSSProperty(set[i], WTFMove(value), false));
    }
    list.shrinkToFit();
    return MutableStyleProperties::create(WTFMove(list));
}

PropertySetCSSStyleDeclaration* MutableStyleProperties::cssStyleDeclaration()
{
    return m_cssomWrapper.get();
}

CSSStyleDeclaration& MutableStyleProperties::ensureCSSStyleDeclaration()
{
    if (m_cssomWrapper) {
        ASSERT(!static_cast<CSSStyleDeclaration*>(m_cssomWrapper.get())->parentRule());
        ASSERT(!m_cssomWrapper->parentElement());
        return *m_cssomWrapper;
    }
    m_cssomWrapper = makeUnique<PropertySetCSSStyleDeclaration>(*this);
    return *m_cssomWrapper;
}

CSSStyleDeclaration& MutableStyleProperties::ensureInlineCSSStyleDeclaration(StyledElement& parentElement)
{
    if (m_cssomWrapper) {
        ASSERT(m_cssomWrapper->parentElement() == &parentElement);
        return *m_cssomWrapper;
    }
    m_cssomWrapper = makeUnique<InlineCSSStyleDeclaration>(*this, parentElement);
    return *m_cssomWrapper;
}

unsigned StyleProperties::averageSizeInBytes()
{
    // Please update this if the storage scheme changes so that this longer reflects the actual size.
    return sizeForImmutableStylePropertiesWithPropertyCount(4);
}

// See the function above if you need to update this.
struct SameSizeAsStyleProperties : public RefCounted<SameSizeAsStyleProperties> {
    unsigned bitfield;
};
static_assert(sizeof(StyleProperties) == sizeof(SameSizeAsStyleProperties), "style property set should stay small");

#ifndef NDEBUG
void StyleProperties::showStyle()
{
    fprintf(stderr, "%s\n", asText().ascii().data());
}
#endif

Ref<MutableStyleProperties> MutableStyleProperties::create(CSSParserMode cssParserMode)
{
    return adoptRef(*new MutableStyleProperties(cssParserMode));
}

Ref<MutableStyleProperties> MutableStyleProperties::create(Vector<CSSProperty>&& properties)
{
    return adoptRef(*new MutableStyleProperties(WTFMove(properties)));
}

Ref<MutableStyleProperties> MutableStyleProperties::createEmpty()
{
    return adoptRef(*new MutableStyleProperties({ }));
}

String StyleProperties::PropertyReference::cssName() const
{
    if (id() == CSSPropertyCustom)
        return downcast<CSSCustomPropertyValue>(*value()).name();
    return nameString(id());
}

String StyleProperties::PropertyReference::cssText() const
{
    return makeString(cssName(), ": ", WebCore::serializeLonghandValue(id(), *m_value), isImportant() ? " !important;" : ";");
}

} // namespace WebCore
