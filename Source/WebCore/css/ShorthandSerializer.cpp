/**
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ShorthandSerializer.h"

#include "CSSBorderImageWidthValue.h"
#include "CSSGridLineNamesValue.h"
#include "CSSGridTemplateAreasValue.h"
#include "CSSParserIdioms.h"
#include "CSSPendingSubstitutionValue.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "CSSValuePair.h"
#include "CSSVariableReferenceValue.h"
#include "ComputedStyleExtractor.h"
#include "FontSelectionValueInlines.h"
#include "Quad.h"
#include "StylePropertiesInlines.h"
#include "StylePropertyShorthand.h"

namespace WebCore {

constexpr unsigned maxShorthandLength = 18; // FIXME: Generate this from CSSProperties.json.

class ShorthandSerializer {
public:
    template<typename PropertiesType> explicit ShorthandSerializer(const PropertiesType&, CSSPropertyID shorthandID);
    String serialize();

private:
    struct Longhand {
        CSSPropertyID property;
        CSSValue& value;
    };
    struct LonghandIteratorBase {
        void operator++() { ++index; }
        bool operator==(std::nullptr_t) const { return index >= serializer.length(); }
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

    bool commonSerializationChecks(const ComputedStyleExtractor&);
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
    String serializeCoordinatingListPropertyGroup() const;

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
    String serializeTextWrap() const;
    String serializeWhiteSpace() const;

    StylePropertyShorthand m_shorthand;
    RefPtr<CSSValue> m_longhandValues[maxShorthandLength];
    String m_result;
    bool m_commonSerializationChecksSuppliedResult { false };
};

template<typename PropertiesType>
inline ShorthandSerializer::ShorthandSerializer(const PropertiesType& properties, CSSPropertyID shorthandID)
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
    return WebCore::longhandValueID(longhandProperty(index), longhandValue(index));
}

inline String ShorthandSerializer::serializeLonghandValue(unsigned index) const
{
    return serializeValue(longhand(index));
}

bool ShorthandSerializer::subsequentLonghandsHaveInitialValues(unsigned startIndex) const
{
    for (unsigned i = startIndex; i < length(); ++i) {
        if (!isLonghandInitialValue(i))
            return false;
    }
    return true;
}

bool ShorthandSerializer::commonSerializationChecks(const ComputedStyleExtractor& properties)
{
    ASSERT(length() && length() <= maxShorthandLength);

    ASSERT(m_shorthand.id() != CSSPropertyAll);

    for (unsigned i = 0; i < length(); ++i) {
        auto longhandValue = properties.propertyValue(longhandProperty(i));
        if (!longhandValue) {
            m_result = emptyString();
            return true;
        }
        m_longhandValues[i] = longhandValue;
    }

    return false;
}

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
    case CSSPropertyMaskBorder:
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
    case CSSPropertyTextWrap:
        return serializeTextWrap();
    case CSSPropertyWebkitColumnBreakAfter:
    case CSSPropertyWebkitColumnBreakBefore:
        return serializeColumnBreak();
    case CSSPropertyWhiteSpace:
        return serializeWhiteSpace();
    case CSSPropertyScrollTimeline:
    case CSSPropertyViewTimeline:
        return serializeCoordinatingListPropertyGroup();
    default:
        ASSERT_NOT_REACHED();
        return String();
    }
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

inline String ShorthandSerializer::serializeCommonValue() const
{
    return serializeCommonValue(0, length());
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
    return Quad::serialize(serializeLonghandValue(0), serializeLonghandValue(1), serializeLonghandValue(2), serializeLonghandValue(3));
}

class LayerValues {
public:
    explicit LayerValues(const StylePropertyShorthand& shorthand)
        : m_shorthand(shorthand)
    {
        ASSERT(m_shorthand.length() <= maxShorthandLength);
    }

    void set(unsigned index, const CSSValue* value, bool skipSerializing = false)
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
        return longhandValueID(m_shorthand.properties()[index], m_values[index].get());
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
        auto* value = m_values[index].get();
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
    RefPtr<const CSSValue> m_values[maxShorthandLength];
};

String ShorthandSerializer::serializeCoordinatingListPropertyGroup() const
{
    ASSERT(length() > 1);

    // https://drafts.csswg.org/css-values-4/#linked-properties

    // First, figure out the number of items in the coordinating list base property,
    // which we'll need to match for all coordinated longhands, thus possibly trimming
    // or expanding.
    unsigned numberOfItemsForCoordinatingListBaseProperty = 1;
    if (auto* valueList = dynamicDowncast<CSSValueList>(longhandValue(0)))
        numberOfItemsForCoordinatingListBaseProperty = std::max(valueList->length(), numberOfItemsForCoordinatingListBaseProperty);

    // Now go through all longhands and ensure we repeat items earlier in the list
    // should there not be a specified value.
    StringBuilder result;
    for (unsigned listItemIndex = 0; listItemIndex < numberOfItemsForCoordinatingListBaseProperty; ++listItemIndex) {
        LayerValues layerValues { m_shorthand };
        for (unsigned longhandIndex = 0; longhandIndex < length(); ++longhandIndex) {
            auto& value = longhandValue(longhandIndex);
            if (auto* valueList = dynamicDowncast<CSSValueList>(value)) {
                auto* valueInList = [&]() -> const CSSValue* {
                    if (auto* specifiedValue = valueList->item(listItemIndex))
                        return specifiedValue;
                    if (auto numberOfItemsInList = valueList->size())
                        return valueList->item(listItemIndex % numberOfItemsInList);
                    return nullptr;
                }();
                layerValues.set(longhandIndex, valueInList);
            } else
                layerValues.set(longhandIndex, &value);
        }
        // The coordinating list base property must never be skipped.
        layerValues.skip(0) = false;
        layerValues.serialize(result);
    }
    return result.toString();
}

String ShorthandSerializer::serializeLayered() const
{
    unsigned numLayers = 1;
    for (auto& value : longhandValues()) {
        if (auto* valueList = dynamicDowncast<CSSValueList>(value))
            numLayers = std::max(valueList->length(), numLayers);
    }

    StringBuilder result;
    for (unsigned i = 0; i < numLayers; i++) {
        LayerValues layerValues { m_shorthand };

        for (unsigned j = 0; j < length(); j++) {
            auto& value = longhandValue(j);
            if (auto* valueList = dynamicDowncast<CSSValueList>(value))
                layerValues.set(j, valueList->item(i));
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

            // A single background-position value (identifier or numeric) sets the other value to center.
            // A single mask-position value (identifier or numeric) sets the other value to center.
            // Order matters when one is numeric, but not when both are identifiers.
            if (longhand == CSSPropertyBackgroundPositionY || longhand == CSSPropertyWebkitMaskPositionY) {
                // The previous property is X.
                ASSERT(j >= 1);
                ASSERT(longhandProperty(j - 1) == CSSPropertyBackgroundPositionX || longhandProperty(j - 1) == CSSPropertyWebkitMaskPositionX);
                if (length() == 2) {
                    ASSERT(j == 1);
                    layerValues.skip(0) = false;
                    layerValues.skip(1) = false;
                } else {
                    // Always serialize positions to at least 2 values.
                    // https://drafts.csswg.org/css-values-4/#position-serialization
                    if (!layerValues.skip(j - 1))
                        layerValues.skip(j) = false;
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

String ShorthandSerializer::serializeBorderImage() const
{
    ASSERT(length() == 5);
    StringBuilder result;
    bool omittedSlice = false;
    bool omittedWidth = false;
    auto separator = "";
    for (auto longhand : longhands()) {
        if (isInitialValue(longhand)) {
            if (longhand.property == CSSPropertyBorderImageSlice || longhand.property == CSSPropertyMaskBorderSlice)
                omittedSlice = true;
            else if (longhand.property == CSSPropertyBorderImageWidth || longhand.property == CSSPropertyMaskBorderWidth)
                omittedWidth = true;
            continue;
        }
        if (omittedSlice && (longhand.property == CSSPropertyBorderImageWidth || longhand.property == CSSPropertyBorderImageOutset || longhand.property == CSSPropertyMaskBorderWidth || longhand.property == CSSPropertyMaskBorderOutset))
            return String();

        String valueText;

        // -webkit-border-image has a legacy behavior that makes fixed border slices also set the border widths.
        if (auto* width = dynamicDowncast<CSSBorderImageWidthValue>(longhand.value)) {
            auto& widths = width->widths();
            bool overridesBorderWidths = m_shorthand.id() == CSSPropertyWebkitBorderImage && (widths.top().isLength() || widths.right().isLength() || widths.bottom().isLength() || widths.left().isLength());
            if (overridesBorderWidths != width->overridesBorderWidths())
                return String();
            valueText = widths.cssText();
        } else
            valueText = serializeValue(longhand);

        // Append separator and text.
        if (longhand.property == CSSPropertyBorderImageWidth || longhand.property == CSSPropertyMaskBorderWidth)
            separator = " / ";
        else if (longhand.property == CSSPropertyBorderImageOutset || longhand.property == CSSPropertyMaskBorderOutset)
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
    RefPtr<const CSSValue> horizontalRadii[4];
    RefPtr<const CSSValue> verticalRadii[4];
    for (unsigned i = 0; i < 4; ++i) {
        auto& value = longhandValue(i);
        horizontalRadii[i] = &value.first();
        verticalRadii[i] = &value.second();
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

static bool isValueIDIncludingList(const CSSValue& value, CSSValueID id)
{
    if (auto* valueList = dynamicDowncast<CSSValueList>(value)) {
        if (valueList->size() != 1)
            return false;
        auto* item = valueList->item(0);
        return item && isValueID(*item, id);
    }
    return isValueID(value, id);
}

static bool gridAutoFlowContains(CSSValue& autoFlow, CSSValueID id)
{
    if (auto* valueList = dynamicDowncast<CSSValueList>(autoFlow)) {
        for (auto& currentValue : *valueList) {
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
    auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
    return primitiveValue && primitiveValue->isCustomIdent();
}

static bool canOmitTrailingGridAreaValue(CSSValue& value, CSSValue& trailing)
{
    if (isCustomIdentValue(value))
        return isCustomIdentValue(trailing) && value.cssText() == trailing.cssText();
    return isValueID(trailing, CSSValueAuto);
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

String ShorthandSerializer::serializeGridRowColumn() const
{
    ASSERT(length() == 2);
    return serializeLonghands(canOmitTrailingGridAreaValue(longhandValue(0), longhandValue(1)) ? 1 : 2, " / ");
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

    // Depending on the values of grid-template-rows and grid-template-columns, we may not
    // be able to completely represent them in this version of the grid-template shorthand.
    // We need to make sure that those values map to a value the syntax supports
    auto isValidTrackSize = [&] (const CSSValue& value) {
        auto valueID = value.valueID();
        if (CSSPropertyParserHelpers::identMatches<CSSValueFitContent, CSSValueMinmax>(valueID) || CSSPropertyParserHelpers::isGridBreadthIdent(valueID))
            return true;
        if (const auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
            return primitiveValue->isLength() || primitiveValue->isPercentage() || primitiveValue->isCalculated() || primitiveValue->isFlex();
        return false;
    };
    auto isValidExplicitTrackList = [&] (const CSSValue& value) {
        const auto* values = dynamicDowncast<CSSValueList>(value);
        if (!values)
            return isValidTrackSize(value);

        auto hasAtLeastOneTrackSize = false;
        for (const auto& value : *values) {
            if (isValidTrackSize(value))
                hasAtLeastOneTrackSize = true;
            else if (!value.isGridLineNamesValue())
                return false;
        }
        return hasAtLeastOneTrackSize;
    };

    // For the rows we need to return early if the value of grid-template-rows is none because otherwise
    // we will iteratively build up that portion of the shorthand and check to see if the value is a valid
    // <track-size> at the appropriate position in the shorthand. For the columns we must check the entire
    // value of grid-template-columns beforehand because we append the value as a whole to the shorthand
    if (isLonghandValueNone(rowsIndex) || (!isLonghandValueNone(columnsIndex) && !isValidExplicitTrackList(longhandValue(columnsIndex))))
        return String();

    StringBuilder result;
    unsigned row = 0;
    for (auto& currentValue : downcast<CSSValueList>(longhandValue(rowsIndex))) {
        if (!result.isEmpty())
            result.append(' ');
        if (auto lineNames = dynamicDowncast<CSSGridLineNamesValue>(currentValue))
            result.append(lineNames->customCSSText());
        else {
            result.append('"', areasValue->stringForRow(row), '"');
            if (!isValidTrackSize(currentValue))
                return String();
            if (!isValueID(currentValue, CSSValueAuto))
                result.append(' ', currentValue.cssText());
            row++;
        }
    }
    if (!isLonghandValueNone(columnsIndex))
        result.append(" / ", serializeLonghandValue(columnsIndex));
    return result.toString();
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

String ShorthandSerializer::serializeTextWrap() const
{
    auto mode = longhandValueID(0);
    auto style = longhandValueID(1);

    if (style == CSSValueAuto)
        return nameString(mode);
    if (mode == CSSValueWrap)
        return nameString(style);

    return makeString(nameLiteral(mode), ' ', nameLiteral(style));
}

String ShorthandSerializer::serializeWhiteSpace() const
{
    auto whiteSpaceCollapse = longhandValueID(0);
    auto textWrapMode = longhandValueID(1);

    // Convert to backwards-compatible keywords if possible.
    if (whiteSpaceCollapse == CSSValueCollapse && textWrapMode == CSSValueWrap)
        return nameString(CSSValueNormal);
    if (whiteSpaceCollapse == CSSValuePreserve && textWrapMode == CSSValueNowrap)
        return nameString(CSSValuePre);
    if (whiteSpaceCollapse == CSSValuePreserve && textWrapMode == CSSValueWrap)
        return nameString(CSSValuePreWrap);
    if (whiteSpaceCollapse == CSSValuePreserveBreaks && textWrapMode == CSSValueWrap)
        return nameString(CSSValuePreLine);

    // Omit default longhand values.
    if (whiteSpaceCollapse == CSSValueCollapse)
        return nameString(textWrapMode);
    if (textWrapMode == CSSValueWrap)
        return nameString(whiteSpaceCollapse);

    return makeString(nameLiteral(whiteSpaceCollapse), ' ', nameLiteral(textWrapMode));
}

String serializeShorthandValue(const StyleProperties& properties, CSSPropertyID shorthand)
{
    return ShorthandSerializer(properties, shorthand).serialize();
}

String serializeShorthandValue(const ComputedStyleExtractor& extractor, CSSPropertyID shorthand)
{
    return ShorthandSerializer(extractor, shorthand).serialize();
}

}
