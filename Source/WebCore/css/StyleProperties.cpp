/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012, 2013 Apple Inc. All rights reserved.
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
#include "CSSPendingSubstitutionValue.h"
#include "CSSPropertyParser.h"
#include "CSSTokenizer.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "Color.h"
#include "Document.h"
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

static size_t sizeForImmutableStylePropertiesWithPropertyCount(unsigned count)
{
    return sizeof(ImmutableStyleProperties) - sizeof(void*) + sizeof(StylePropertyMetadata) * count + sizeof(PackedPtr<const CSSValue>) * count;
}

static bool isCSSWideValueKeyword(StringView value)
{
    return value == "initial"_s || value == "inherit"_s || value == "unset"_s || value == "revert"_s || value == "revert-layer"_s;
}

static bool isNoneValue(const RefPtr<CSSValue>& value)
{
    return value && value->isPrimitiveValue() && downcast<CSSPrimitiveValue>(value.get())->isValueID() && downcast<CSSPrimitiveValue>(value.get())->valueID() == CSSValueNone;
}

static bool isNormalValue(const RefPtr<CSSValue>& value)
{
    return value && value->isPrimitiveValue() && downcast<CSSPrimitiveValue>(value.get())->isValueID() && downcast<CSSPrimitiveValue>(value.get())->valueID() == CSSValueNormal;
}

static bool isValueID(const Ref<CSSValue>& value, CSSValueID id)
{
    return value->isPrimitiveValue() && downcast<CSSPrimitiveValue>(value.get()).isValueID() && downcast<CSSPrimitiveValue>(value.get()).valueID() == id;
}

static bool isValueID(const RefPtr<CSSValue>& value, CSSValueID id)
{
    return value && isValueID(*value, id);
}

static bool isValueIDIncludingList(const Ref<CSSValue>& value, CSSValueID id)
{
    if (is<CSSValueList>(value)) {
        if (downcast<CSSValueList>(value.get()).size() != 1)
            return false;
        return isValueID(downcast<CSSValueList>(value.get()).item(0), id);
    }
    return isValueID(value, id);
}

static bool isValueIDIncludingList(const RefPtr<CSSValue>& value, CSSValueID id)
{
    return value && isValueIDIncludingList(*value, id);
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
    PackedPtr<CSSValue>* valueArray = bitwise_cast<PackedPtr<CSSValue>*>(this->valueArray());
    for (unsigned i = 0; i < length; ++i) {
        metadataArray[i] = properties[i].metadata();
        auto* value = properties[i].value();
        valueArray[i] = value;
        value->ref();
    }
}

ImmutableStyleProperties::~ImmutableStyleProperties()
{
    PackedPtr<CSSValue>* valueArray = bitwise_cast<PackedPtr<CSSValue>*>(this->valueArray());
    for (unsigned i = 0; i < m_arraySize; ++i)
        valueArray[i]->deref();
}

MutableStyleProperties::MutableStyleProperties(const StyleProperties& other)
    : StyleProperties(other.cssParserMode(), MutablePropertiesType)
{
    if (is<MutableStyleProperties>(other))
        m_propertyVector = downcast<MutableStyleProperties>(other).m_propertyVector;
    else {
        const auto& immutableOther = downcast<ImmutableStyleProperties>(other);
        unsigned propertyCount = immutableOther.propertyCount();
        m_propertyVector.reserveInitialCapacity(propertyCount);
        for (unsigned i = 0; i < propertyCount; ++i)
            m_propertyVector.uncheckedAppend(immutableOther.propertyAt(i).toCSSProperty());
    }
}

bool StyleProperties::shorthandHasVariableReference(CSSPropertyID propertyID, String& shorthandValue) const
{
    auto shorthand = shorthandForProperty(propertyID);
    if (shorthand.length()) {
        size_t numSetFromShorthand = 0;
        // Checks for shorthand property if any of its longhand properties have set to a variable
        // or are all pending substitution
        for (size_t i = 0; i < shorthand.length(); i++) {
            auto cssPropertyValue =  getPropertyCSSValue(shorthand.properties()[i]);
            
            auto hasBeenSetFromLonghand = is<CSSVariableReferenceValue>(cssPropertyValue);
            auto hasBeenSetFromShorthand = is<CSSPendingSubstitutionValue>(cssPropertyValue);
            auto hasNotBeenSetFromRequestedShorthand = hasBeenSetFromShorthand && downcast<CSSPendingSubstitutionValue>(*cssPropertyValue).shorthandPropertyId() != propertyID;
            
            // Request for shorthand value should return empty string if any longhand values have been
            // set to a variable or if they were set to a variable by a different shorthand.
            if (hasBeenSetFromLonghand || hasNotBeenSetFromRequestedShorthand)
                return true;
            if (hasBeenSetFromShorthand)
                numSetFromShorthand += 1;
        }
        if (numSetFromShorthand) {
            if (numSetFromShorthand != shorthand.length())
                return true;
            shorthandValue = downcast<CSSPendingSubstitutionValue>(* getPropertyCSSValue(shorthand.properties()[0])).shorthandValue().cssText();
            return true;
        }
    }
    return false;
}

String StyleProperties::getPropertyValue(CSSPropertyID propertyID) const
{
    if (auto value = getPropertyCSSValue(propertyID)) {
        switch (propertyID) {
        case CSSPropertyFillOpacity:
        case CSSPropertyFloodOpacity:
        case CSSPropertyOpacity:
        case CSSPropertyStopOpacity:
        case CSSPropertyStrokeOpacity:
            // Opacity percentage values serialize as a fraction in the range 0-1, not "%".
            if (is<CSSPrimitiveValue>(*value) && downcast<CSSPrimitiveValue>(*value).isPercentage())
                return makeString(downcast<CSSPrimitiveValue>(*value).doubleValue() / 100);
            FALLTHROUGH;
        default:
            return value->cssText();
        }
    }

    {
        auto shorthandValue = String();
        if (shorthandHasVariableReference(propertyID, shorthandValue))
            return shorthandValue;
    }

    // Shorthand and 4-values properties
    switch (propertyID) {
    case CSSPropertyAll:
        return getCommonValue(allShorthand());
    case CSSPropertyAnimation:
        return getLayeredShorthandValue(animationShorthand());
    case CSSPropertyBorderSpacing:
        return borderSpacingValue(borderSpacingShorthand());
    case CSSPropertyBackgroundPosition:
        return getLayeredShorthandValue(backgroundPositionShorthand());
    case CSSPropertyBackground:
        return getLayeredShorthandValue(backgroundShorthand());
    case CSSPropertyBorder:
        return borderPropertyValue(borderWidthShorthand(), borderStyleShorthand(), borderColorShorthand());
    case CSSPropertyBorderTop:
        return getShorthandValue(borderTopShorthand());
    case CSSPropertyBorderRight:
        return getShorthandValue(borderRightShorthand());
    case CSSPropertyBorderBottom:
        return getShorthandValue(borderBottomShorthand());
    case CSSPropertyBorderLeft:
        return getShorthandValue(borderLeftShorthand());
    case CSSPropertyBorderBlock:
        return borderPropertyValue(borderBlockWidthShorthand(), borderBlockStyleShorthand(), borderBlockColorShorthand());
    case CSSPropertyBorderBlockColor:
        return get2Values(borderBlockColorShorthand());
    case CSSPropertyBorderBlockStyle:
        return get2Values(borderBlockStyleShorthand());
    case CSSPropertyBorderBlockWidth:
        return get2Values(borderBlockWidthShorthand());
    case CSSPropertyBorderBlockStart:
        return getShorthandValue(borderBlockStartShorthand());
    case CSSPropertyBorderBlockEnd:
        return getShorthandValue(borderBlockEndShorthand());
    case CSSPropertyBorderImage:
    case CSSPropertyWebkitBorderImage:
        return borderImagePropertyValue(propertyID);
    case CSSPropertyBorderInline:
        return borderPropertyValue(borderInlineWidthShorthand(), borderInlineStyleShorthand(), borderInlineColorShorthand());
    case CSSPropertyBorderInlineColor:
        return get2Values(borderInlineColorShorthand());
    case CSSPropertyBorderInlineStyle:
        return get2Values(borderInlineStyleShorthand());
    case CSSPropertyBorderInlineWidth:
        return get2Values(borderInlineWidthShorthand());
    case CSSPropertyBorderInlineStart:
        return getShorthandValue(borderInlineStartShorthand());
    case CSSPropertyBorderInlineEnd:
        return getShorthandValue(borderInlineEndShorthand());
    case CSSPropertyOffset:
        return offsetValue();
    case CSSPropertyOutline:
        return getShorthandValue(outlineShorthand());
    case CSSPropertyBorderColor:
        return get4Values(borderColorShorthand());
    case CSSPropertyBorderWidth:
        return get4Values(borderWidthShorthand());
    case CSSPropertyBorderStyle:
        return get4Values(borderStyleShorthand());
    case CSSPropertyColumnRule:
        return getShorthandValue(columnRuleShorthand());
    case CSSPropertyColumns:
        return getShorthandValue(columnsShorthand());
    case CSSPropertyContainer:
        if (auto type = getPropertyCSSValue(CSSPropertyContainerType)) {
            if (isNormalValue(type)) {
                if (auto name = getPropertyCSSValue(CSSPropertyContainerName))
                    return name->cssText();
                return { };
            }
        }
        return getShorthandValue(containerShorthand(), " / ");
    case CSSPropertyFlex:
        return getShorthandValue(flexShorthand());
    case CSSPropertyFlexFlow:
        return getShorthandValue(flexFlowShorthand());
    case CSSPropertyGridArea:
        return getGridAreaShorthandValue();
    case CSSPropertyGridTemplate:
        return getGridTemplateValue();
    case CSSPropertyGrid:
        return getGridValue();
    case CSSPropertyGridColumn:
        return getGridRowColumnShorthandValue(gridColumnShorthand());
    case CSSPropertyGridRow:
        return getGridRowColumnShorthandValue(gridRowShorthand());
    case CSSPropertyPageBreakAfter:
        return pageBreakPropertyValue(pageBreakAfterShorthand());
    case CSSPropertyPageBreakBefore:
        return pageBreakPropertyValue(pageBreakBeforeShorthand());
    case CSSPropertyPageBreakInside:
        return pageBreakPropertyValue(pageBreakInsideShorthand());
    case CSSPropertyPlaceContent:
        return getAlignmentShorthandValue(placeContentShorthand());
    case CSSPropertyPlaceItems:
        return getAlignmentShorthandValue(placeItemsShorthand());
    case CSSPropertyPlaceSelf:
        return getAlignmentShorthandValue(placeSelfShorthand());
    case CSSPropertyFont:
        return fontValue();
    case CSSPropertyFontVariant:
        return fontVariantValue();
    case CSSPropertyFontSynthesis:
        return fontSynthesisValue();
    case CSSPropertyTextDecoration:
        if (auto line = getPropertyCSSValue(CSSPropertyTextDecorationLine))
            return line->cssText();
        return String();
    case CSSPropertyWebkitTextDecoration:
        return getShorthandValue(webkitTextDecorationShorthand());
    case CSSPropertyTextDecorationSkip:
        return textDecorationSkipValue();
    case CSSPropertyInset:
        return get4Values(insetShorthand());
    case CSSPropertyInsetBlock:
        return get2Values(insetBlockShorthand());
    case CSSPropertyInsetInline:
        return get2Values(insetInlineShorthand());
    case CSSPropertyMargin:
        return get4Values(marginShorthand());
    case CSSPropertyMarginBlock:
        return get2Values(marginBlockShorthand());
    case CSSPropertyMarginInline:
        return get2Values(marginInlineShorthand());
    case CSSPropertyOverflow:
        return get2Values(overflowShorthand());
    case CSSPropertyOverscrollBehavior:
        return get2Values(overscrollBehaviorShorthand());
    case CSSPropertyPadding:
        return get4Values(paddingShorthand());
    case CSSPropertyPaddingBlock:
        return get2Values(paddingBlockShorthand());
    case CSSPropertyPaddingInline:
        return get2Values(paddingInlineShorthand());
    case CSSPropertyTransition:
        return getLayeredShorthandValue(transitionShorthand());
    case CSSPropertyListStyle:
        return getShorthandValue(listStyleShorthand());
    case CSSPropertyMaskPosition:
        return getLayeredShorthandValue(maskPositionShorthand());
    case CSSPropertyWebkitMaskPosition:
        return getLayeredShorthandValue(webkitMaskPositionShorthand());
    case CSSPropertyMask:
    case CSSPropertyWebkitMask:
        return getLayeredShorthandValue(shorthandForProperty(propertyID));
    case CSSPropertyTextEmphasis:
        return getShorthandValue(textEmphasisShorthand());
    case CSSPropertyWebkitTextStroke:
        return getShorthandValue(webkitTextStrokeShorthand());
    case CSSPropertyPerspectiveOrigin:
        return getShorthandValue(perspectiveOriginShorthand());
    case CSSPropertyTransformOrigin:
        return getShorthandValue(transformOriginShorthand());
    case CSSPropertyMarker:
        if (auto value = getPropertyCSSValue(CSSPropertyMarkerStart))
            return value->cssText();
        return String();
    case CSSPropertyBorderRadius:
        return get4Values(borderRadiusShorthand());
    case CSSPropertyGap:
        return get2Values(gapShorthand());
    case CSSPropertyScrollMargin:
        return get4Values(scrollMarginShorthand());
    case CSSPropertyScrollMarginBlock:
        return get2Values(scrollMarginBlockShorthand());
    case CSSPropertyScrollMarginInline:
        return get2Values(scrollMarginInlineShorthand());
    case CSSPropertyScrollPadding:
        return get4Values(scrollPaddingShorthand());
    case CSSPropertyScrollPaddingBlock:
        return get2Values(scrollPaddingBlockShorthand());
    case CSSPropertyScrollPaddingInline:
        return get2Values(scrollPaddingInlineShorthand());
    case CSSPropertyWebkitTextOrientation:
        return getPropertyValue(CSSPropertyTextOrientation);
    case CSSPropertyWebkitBackgroundSize:
        return getPropertyValue(CSSPropertyBackgroundSize);
    case CSSPropertyContainIntrinsicSize:
        return get2Values(containIntrinsicSizeShorthand());
    default:
        return String();
    }
}

std::optional<Color> StyleProperties::propertyAsColor(CSSPropertyID property) const
{
    auto colorValue = getPropertyCSSValue(property);
    if (!is<CSSPrimitiveValue>(colorValue))
        return std::nullopt;

    auto& primitiveColor = downcast<CSSPrimitiveValue>(*colorValue);
    return primitiveColor.isRGBColor() ? primitiveColor.color() : CSSParser::parseColorWithoutContext(colorValue->cssText());
}

CSSValueID StyleProperties::propertyAsValueID(CSSPropertyID property) const
{
    auto cssValue = getPropertyCSSValue(property);
    return is<CSSPrimitiveValue>(cssValue) ? downcast<CSSPrimitiveValue>(*cssValue).valueID() : CSSValueInvalid;
}

String StyleProperties::getCustomPropertyValue(const String& propertyName) const
{
    RefPtr<CSSValue> value = getCustomPropertyCSSValue(propertyName);
    if (value)
        return value->cssText();
    return String();
}

String StyleProperties::borderSpacingValue(const StylePropertyShorthand& shorthand) const
{
    auto horizontalValue = getPropertyCSSValue(shorthand.properties()[0]);
    auto verticalValue = getPropertyCSSValue(shorthand.properties()[1]);

    // While standard border-spacing property does not allow specifying border-spacing-vertical without
    // specifying border-spacing-horizontal <http://www.w3.org/TR/CSS21/tables.html#separated-borders>,
    // -webkit-border-spacing-vertical can be set without -webkit-border-spacing-horizontal.
    if (!horizontalValue || !verticalValue)
        return String();

    String horizontalValueCSSText = horizontalValue->cssText();
    String verticalValueCSSText = verticalValue->cssText();
    if (horizontalValueCSSText == verticalValueCSSText)
        return horizontalValueCSSText;
    return horizontalValueCSSText + ' ' + verticalValueCSSText;
}

void StyleProperties::appendFontLonghandValueIfExplicit(CSSPropertyID propertyID, StringBuilder& result, String& commonValue) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return; // All longhands must have at least implicit values if "font" is specified.

    if (propertyAt(foundPropertyIndex).isImplicit()) {
        commonValue = String();
        return;
    }

    char prefix = '\0';
    switch (propertyID) {
    case CSSPropertyFontStyle:
        break; // No prefix.
    case CSSPropertyFontFamily:
    case CSSPropertyFontVariantAlternates:
    case CSSPropertyFontVariantCaps:
    case CSSPropertyFontVariantLigatures:
    case CSSPropertyFontVariantNumeric:
    case CSSPropertyFontVariantPosition:
    case CSSPropertyFontVariantEastAsian:
    case CSSPropertyFontWeight:
    case CSSPropertyFontStretch:
        prefix = ' ';
        break;
    case CSSPropertyLineHeight:
        prefix = '/';
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    if (prefix && !result.isEmpty())
        result.append(prefix);
    String value = propertyAt(foundPropertyIndex).value()->cssText();
    result.append(value);
    if (!commonValue.isNull() && commonValue != value)
        commonValue = String();
}

std::optional<CSSValueID> StyleProperties::isSingleFontShorthand() const
{
    // Intentionally don't check font-stretch here, because it isn't set by the font shorthand in CSSPropertyParser::consumeSystemFont().

    auto sizePropertyIndex = findPropertyIndex(CSSPropertyFontSize);
    auto familyPropertyIndex = findPropertyIndex(CSSPropertyFontFamily);
    auto stylePropertyIndex = findPropertyIndex(CSSPropertyFontStyle);
    auto variantCapsPropertyIndex = findPropertyIndex(CSSPropertyFontVariantCaps);
    auto weightPropertyIndex = findPropertyIndex(CSSPropertyFontWeight);
    auto lineHeightPropertyIndex = findPropertyIndex(CSSPropertyLineHeight);

    if (sizePropertyIndex == -1
        || familyPropertyIndex == -1
        || stylePropertyIndex == -1
        || variantCapsPropertyIndex == -1
        || weightPropertyIndex == -1
        || lineHeightPropertyIndex == -1)
        return std::nullopt;

    auto sizeProperty = propertyAt(sizePropertyIndex);
    auto familyProperty = propertyAt(familyPropertyIndex);
    auto styleProperty = propertyAt(stylePropertyIndex);
    auto variantCapsProperty = propertyAt(variantCapsPropertyIndex);
    auto weightProperty = propertyAt(weightPropertyIndex);
    auto lineHeightProperty = propertyAt(lineHeightPropertyIndex);

    if (sizeProperty.isImplicit()
        || familyProperty.isImplicit()
        || styleProperty.isImplicit()
        || variantCapsProperty.isImplicit()
        || weightProperty.isImplicit()
        || lineHeightProperty.isImplicit())
        return std::nullopt;

    auto* sizeValue = sizeProperty.value();
    auto* familyValue = familyProperty.value();
    auto* styleValue = styleProperty.value();
    auto* variantCapsValue = variantCapsProperty.value();
    auto* weightValue = weightProperty.value();
    auto* lineHeightValue = lineHeightProperty.value();

    if (!is<CSSPrimitiveValue>(sizeValue)
        || !is<CSSPrimitiveValue>(familyValue)
        || !is<CSSPrimitiveValue>(styleValue)
        || !is<CSSPrimitiveValue>(variantCapsValue)
        || !is<CSSPrimitiveValue>(weightValue)
        || !is<CSSPrimitiveValue>(lineHeightValue))
        return std::nullopt;

    auto& sizePrimitiveValue = downcast<CSSPrimitiveValue>(*sizeValue);
    auto& familyPrimitiveValue = downcast<CSSPrimitiveValue>(*familyValue);
    auto& stylePrimitiveValue = downcast<CSSPrimitiveValue>(*styleValue);
    auto& variantCapsPrimitiveValue = downcast<CSSPrimitiveValue>(*variantCapsValue);
    auto& weightPrimitiveValue = downcast<CSSPrimitiveValue>(*weightValue);
    auto& lineHeightPrimitiveValue = downcast<CSSPrimitiveValue>(*lineHeightValue);

    auto sizeValueID = sizePrimitiveValue.valueID();
    auto familyValueID = familyPrimitiveValue.valueID();
    auto styleValueID = stylePrimitiveValue.valueID();
    auto variantCapsValueID = variantCapsPrimitiveValue.valueID();
    auto weightValueID = weightPrimitiveValue.valueID();
    auto lineHeightValueID = lineHeightPrimitiveValue.valueID();

    if (sizeValueID != familyValueID
        || sizeValueID != styleValueID
        || sizeValueID != variantCapsValueID
        || sizeValueID != weightValueID
        || sizeValueID != lineHeightValueID)
        return std::nullopt;

    if (sizeValueID == CSSValueInvalid)
        return std::nullopt;

    return sizeValueID;
}

String StyleProperties::fontValue() const
{
    int fontSizePropertyIndex = findPropertyIndex(CSSPropertyFontSize);
    int fontFamilyPropertyIndex = findPropertyIndex(CSSPropertyFontFamily);
    if (fontSizePropertyIndex == -1 || fontFamilyPropertyIndex == -1)
        return emptyString();

    PropertyReference fontSizeProperty = propertyAt(fontSizePropertyIndex);
    PropertyReference fontFamilyProperty = propertyAt(fontFamilyPropertyIndex);
    if (fontSizeProperty.isImplicit() || fontFamilyProperty.isImplicit())
        return emptyString();

    if (auto shorthand = isSingleFontShorthand())
        return getValueNameAtomString(shorthand.value());

    String commonValue = fontSizeProperty.value()->cssText();
    StringBuilder result;
    appendFontLonghandValueIfExplicit(CSSPropertyFontStyle, result, commonValue);
    appendFontLonghandValueIfExplicit(CSSPropertyFontVariantCaps, result, commonValue);
    appendFontLonghandValueIfExplicit(CSSPropertyFontWeight, result, commonValue);
    appendFontLonghandValueIfExplicit(CSSPropertyFontStretch, result, commonValue);
    if (!result.isEmpty())
        result.append(' ');
    result.append(fontSizeProperty.value()->cssText());
    appendFontLonghandValueIfExplicit(CSSPropertyLineHeight, result, commonValue);
    if (!result.isEmpty())
        result.append(' ');
    result.append(fontFamilyProperty.value()->cssText());
    if (isCSSWideValueKeyword(commonValue))
        return commonValue;
    return result.toString();
}

String StyleProperties::offsetValue() const
{
    StringBuilder result;

    auto offsetPositionIndex = findPropertyIndex(CSSPropertyOffsetPosition);
    auto offsetPathIndex = findPropertyIndex(CSSPropertyOffsetPath);

    // Either offset-position and offset-path must be specified.
    if (offsetPositionIndex == -1 && offsetPathIndex == -1)
        return String();

    if (offsetPositionIndex != -1) {
        auto offsetPosition = propertyAt(offsetPositionIndex);
        if (!offsetPosition.isImplicit()) {
            if (!offsetPosition.value())
                return String();

            result.append(offsetPosition.value()->cssText());
        }
    }

    if (offsetPathIndex != -1) {
        auto offsetPath = propertyAt(offsetPathIndex);
        if (!offsetPath.isImplicit()) {
            if (!offsetPath.value())
                return String();

            if (!result.isEmpty())
                result.append(' ');
            result.append(offsetPath.value()->cssText());
        }
    }

    // At this point, result is not empty because either offset-position or offset-path
    // must be present.

    auto offsetDistanceIndex = findPropertyIndex(CSSPropertyOffsetDistance);
    if (offsetDistanceIndex != -1) {
        auto offsetDistance = propertyAt(offsetDistanceIndex);
        if (!offsetDistance.isImplicit()) {
            auto offsetDistanceValue = offsetDistance.value();
            if (!offsetDistanceValue || !is<CSSPrimitiveValue>(offsetDistanceValue))
                return String();
            // Only include offset-distance if the distance is non-zero.
            // isZero() returns std::nullopt if offsetDistanceValue is a calculated value, in which case
            // we use value_or() to override to false.
            if (!(downcast<CSSPrimitiveValue>(offsetDistanceValue)->isZero().value_or(false))) {
                result.append(' ');
                result.append(downcast<CSSPrimitiveValue>(offsetDistanceValue)->cssText());
            }
        }
    }

    auto offsetRotateIndex = findPropertyIndex(CSSPropertyOffsetRotate);
    if (offsetRotateIndex != -1) {
        auto offsetRotate = propertyAt(offsetRotateIndex);
        if (!offsetRotate.isImplicit()) {
            auto offsetRotateValue = offsetRotate.value();
            if (!offsetRotateValue || !is<CSSOffsetRotateValue>(offsetRotateValue))
                return String();

            if (!(downcast<CSSOffsetRotateValue>(offsetRotateValue)->isInitialValue())) {
                result.append(' ');
                result.append(downcast<CSSOffsetRotateValue>(offsetRotateValue)->cssText());
            }
        }
    }

    auto offsetAnchorIndex = findPropertyIndex(CSSPropertyOffsetAnchor);
    if (offsetAnchorIndex != -1) {
        auto offsetAnchor = propertyAt(offsetAnchorIndex);
        if (!offsetAnchor.isImplicit()) {
            auto offsetAnchorValue = offsetAnchor.value();
            if (!offsetAnchorValue)
                return String();

            if (!is<CSSPrimitiveValue>(offsetAnchorValue) || !downcast<CSSPrimitiveValue>(offsetAnchorValue)->isValueID()
                || downcast<CSSPrimitiveValue>(offsetAnchorValue)->valueID() != CSSValueAuto) {
                result.append(" / ");
                result.append(offsetAnchorValue->cssText());
            }
        }
    }

    return result.toString();
}

String StyleProperties::textDecorationSkipValue() const
{
    int textDecorationSkipInkPropertyIndex = findPropertyIndex(CSSPropertyTextDecorationSkipInk);
    if (textDecorationSkipInkPropertyIndex == -1)
        return emptyString();
    PropertyReference textDecorationSkipInkProperty = propertyAt(textDecorationSkipInkPropertyIndex);
    if (textDecorationSkipInkProperty.isImplicit())
        return emptyString();
    return textDecorationSkipInkProperty.value()->cssText();
}

String StyleProperties::fontVariantValue() const
{
    String commonValue;
    StringBuilder result;
    appendFontLonghandValueIfExplicit(CSSPropertyFontVariantLigatures, result, commonValue);
    if (isCSSWideValueKeyword(result.toString()))
        return result.toString();
    appendFontLonghandValueIfExplicit(CSSPropertyFontVariantCaps, result, commonValue);
    appendFontLonghandValueIfExplicit(CSSPropertyFontVariantAlternates, result, commonValue);
    appendFontLonghandValueIfExplicit(CSSPropertyFontVariantNumeric, result, commonValue);
    appendFontLonghandValueIfExplicit(CSSPropertyFontVariantEastAsian, result, commonValue);
    appendFontLonghandValueIfExplicit(CSSPropertyFontVariantPosition, result, commonValue);
    return result.toString();
}

String StyleProperties::fontSynthesisValue() const
{
    StringBuilder result;

    auto getExplicitLonghandValue = [&](CSSPropertyID propertyID) -> CSSValue* {
        auto foundPropertyIndex = findPropertyIndex(propertyID);
        if (foundPropertyIndex == -1)
            return nullptr;

        auto property = propertyAt(foundPropertyIndex);
        if (property.isImplicit())
            return nullptr;

        return property.value();
    };

    // font-synthesis: none | [ weight || style || small-caps ]
    auto weightValue = getExplicitLonghandValue(CSSPropertyFontSynthesisWeight);
    auto styleValue = getExplicitLonghandValue(CSSPropertyFontSynthesisStyle);
    auto capsValue = getExplicitLonghandValue(CSSPropertyFontSynthesisSmallCaps);

    auto valueID = [&](CSSValue *value) {
        if (!value || !is<CSSPrimitiveValue>(value))
            return CSSValueInvalid;
        return downcast<CSSPrimitiveValue>(value)->valueID();
    };

    auto weightValueID = valueID(weightValue);
    auto styleValueID = valueID(styleValue);
    auto capsValueID = valueID(capsValue);

    if (weightValueID != CSSValueInvalid && weightValueID == styleValueID && weightValueID == capsValueID) {
        // Handle `none` or CSS wide-keywords.
        if (weightValue->isCSSWideKeyword() || weightValueID == CSSValueNone)
            return weightValue->cssText();
    }

    // If one of the longhands is a CSS-wide keyword but not all of them are, this is not a valid shorthand.
    if ((weightValue && weightValue->isCSSWideKeyword()) || (styleValue && styleValue->isCSSWideKeyword()) || (capsValue && capsValue->isCSSWideKeyword()))
        return String();

    auto appendWithPrefixIfNeeded = [&](ASCIILiteral word) {
        if (!result.isEmpty())
            result.append(' ');
        result.append(word);
    };

    if (weightValueID == CSSValueAuto)
        appendWithPrefixIfNeeded("weight"_s);
    if (styleValueID == CSSValueAuto)
        appendWithPrefixIfNeeded("style"_s);
    if (capsValueID == CSSValueAuto)
        appendWithPrefixIfNeeded("small-caps"_s);

    return result.toString();
}

String StyleProperties::get2Values(const StylePropertyShorthand& shorthand) const
{
    // Assume the properties are in the usual order start, end.
    int startValueIndex = findPropertyIndex(shorthand.properties()[0]);
    int endValueIndex = findPropertyIndex(shorthand.properties()[1]);

    if (startValueIndex == -1 || endValueIndex == -1)
        return { };

    auto start = propertyAt(startValueIndex);
    auto end = propertyAt(endValueIndex);

    // All 2 properties must be specified.
    if (!start.value() || !end.value())
        return { };

    // Important flags must be the same
    if (start.isImportant() != end.isImportant())
        return { };

    if (start.isInherited() && end.isInherited())
        return getValueName(CSSValueInherit);

    if (start.value()->isInitialValue() || end.value()->isInitialValue()) {
        if (start.value()->isInitialValue() && end.value()->isInitialValue() && !start.isImplicit())
            return getValueName(CSSValueInitial);
        return { };
    }

    StringBuilder result;
    result.append(start.value()->cssText());
    if (!start.value()->equals(*end.value())) {
        result.append(' ');
        result.append(end.value()->cssText());
    }
    return result.toString();
}

String StyleProperties::get4Values(const StylePropertyShorthand& shorthand) const
{
    // Assume the properties are in the usual order top, right, bottom, left.
    int topValueIndex = findPropertyIndex(shorthand.properties()[0]);
    int rightValueIndex = findPropertyIndex(shorthand.properties()[1]);
    int bottomValueIndex = findPropertyIndex(shorthand.properties()[2]);
    int leftValueIndex = findPropertyIndex(shorthand.properties()[3]);

    if (topValueIndex == -1 || rightValueIndex == -1 || bottomValueIndex == -1 || leftValueIndex == -1)
        return String();

    PropertyReference top = propertyAt(topValueIndex);
    PropertyReference right = propertyAt(rightValueIndex);
    PropertyReference bottom = propertyAt(bottomValueIndex);
    PropertyReference left = propertyAt(leftValueIndex);

    // All 4 properties must be specified.
    if (!top.value() || !right.value() || !bottom.value() || !left.value())
        return String();

    // Important flags must be the same
    if (top.isImportant() != right.isImportant() || right.isImportant() != bottom.isImportant() || bottom.isImportant() != left.isImportant())
        return String();

    if (top.isInherited() && right.isInherited() && bottom.isInherited() && left.isInherited())
        return getValueName(CSSValueInherit);

    if (top.value()->isInitialValue() || right.value()->isInitialValue() || bottom.value()->isInitialValue() || left.value()->isInitialValue()) {
        if (top.value()->isInitialValue() && right.value()->isInitialValue() && bottom.value()->isInitialValue() && left.value()->isInitialValue() && !top.isImplicit()) {
            // All components are "initial" and "top" is not implicit.
            return getValueName(CSSValueInitial);
        }
        return String();
    }

    bool showLeft = !right.value()->equals(*left.value());
    bool showBottom = !top.value()->equals(*bottom.value()) || showLeft;
    bool showRight = !top.value()->equals(*right.value()) || showBottom;

    StringBuilder result;
    result.append(top.value()->cssText());
    if (showRight) {
        result.append(' ');
        result.append(right.value()->cssText());
    }
    if (showBottom) {
        result.append(' ');
        result.append(bottom.value()->cssText());
    }
    if (showLeft) {
        result.append(' ');
        result.append(left.value()->cssText());
    }
    return result.toString();
}

String StyleProperties::getLayeredShorthandValue(const StylePropertyShorthand& shorthand) const
{
    StringBuilder result;

    const unsigned size = shorthand.length();
    Vector<RefPtr<CSSValue>> values(size);
    size_t numLayers = 0;

    for (unsigned i = 0; i < size; ++i) {
        values[i] = getPropertyCSSValue(shorthand.properties()[i]);
        if (!values[i]) {
            // We don't have all longhand properties defined as required for the shorthand
            // property and thus should not serialize to a shorthand value. See spec at
            // https://www.w3.org/TR/cssom-1/#serialize-a-css-declaration-block
            return String();
        }
        if (values[i]->isBaseValueList())
            numLayers = std::max(downcast<CSSValueList>(*values[i]).length(), numLayers);
        else
            numLayers = std::max<size_t>(1U, numLayers);
    }

    String commonValue;

    // Now stitch the properties together.
    // Implicit initial values are flagged as such and can safely be omitted.
    for (size_t i = 0; i < numLayers; i++) {
        StringBuilder layerResult;
        bool useRepeatXShorthand = false;
        bool useRepeatYShorthand = false;
        bool useSingleWordShorthand = false;
        bool foundPositionYCSSProperty = false;
        for (unsigned j = 0; j < size; j++) {
            auto property = shorthand.properties()[j];

            auto value = values[j];
            if (value) {
                if (value->isBaseValueList())
                    value = downcast<CSSValueList>(*value).item(i);
                else {
                    // Color only belongs in the last layer.
                    if (property == CSSPropertyBackgroundColor) {
                        if (i != numLayers - 1)
                            value = nullptr;
                    } else if (i) // Other singletons only belong in the first layer.
                        value = nullptr;
                }
            }

            auto canOmitValue = [&]() {
                if (shorthand.id() == CSSPropertyMask) {
                    if (property == CSSPropertyMaskClip) {
                        // If the mask-clip value is the same as the value for mask-origin (the previous value),
                        // then we can skip serializing it, as one value sets both properties.
                        ASSERT(j > 0);
                        ASSERT(shorthand.properties()[j - 1] == CSSPropertyMaskOrigin);
                        auto originValue = values[j - 1];
                        if (is<CSSValueList>(*originValue))
                            originValue = downcast<CSSValueList>(*originValue).item(i);
                        if (!is<CSSPrimitiveValue>(*value) || (originValue && !is<CSSPrimitiveValue>(*originValue)))
                            return false;

                        auto maskId = downcast<CSSPrimitiveValue>(*value).valueID();
                        auto originId = originValue ? downcast<CSSPrimitiveValue>(*originValue).valueID() : CSSValueInitial;
                        return maskId == originId && (!isCSSWideValueKeyword(StringView { getValueName(maskId) }) || value->isImplicitInitialValue());
                    }
                    if (property == CSSPropertyMaskOrigin) {
                        // We can skip serializing mask-origin if it's the initial value, but only if we're also going to skip serializing
                        // the mask-clip as well (otherwise the single value for mask-clip would be assumed to be setting the value for both).
                        ASSERT(j + 1 < size);
                        ASSERT(shorthand.properties()[j + 1] == CSSPropertyMaskClip);
                        auto clipValue = values[j + 1];
                        if (is<CSSValueList>(*clipValue))
                            clipValue = downcast<CSSValueList>(*clipValue).item(i);
                        return value->isImplicitInitialValue() && (!clipValue || clipValue->isImplicitInitialValue());
                    }
                }

                return value->isImplicitInitialValue();
            };

            String valueText;
            if (!value && shorthand.id() == CSSPropertyMask)
                value = CSSValuePool::singleton().createImplicitInitialValue();
            if (value && !canOmitValue()) {
                if (!layerResult.isEmpty())
                    layerResult.append(' ');

                if (property == CSSPropertyBackgroundSize || property == CSSPropertyMaskSize) {
                    if (!foundPositionYCSSProperty)
                        continue;
                    layerResult.append("/ ");
                }

                if (useRepeatXShorthand) {
                    useRepeatXShorthand = false;
                    layerResult.append(getValueName(CSSValueRepeatX));
                } else if (useRepeatYShorthand) {
                    useRepeatYShorthand = false;
                    layerResult.append(getValueName(CSSValueRepeatY));
                } else if (shorthand.id() == CSSPropertyMask && property == CSSPropertyMaskOrigin && value->isImplicitInitialValue()) {
                    // If we're about to write the value for mask-origin, but it's an implicit initial value that's just a placeholder
                    // for a 'real' mask-clip value, then write the actual value not 'initial'.
                    layerResult.append(getValueName(CSSValueBorderBox));
                } else {
                    if (useSingleWordShorthand)
                        useSingleWordShorthand = false;
                    valueText = value->cssText();
                    layerResult.append(valueText);
                }

                if (property == CSSPropertyBackgroundPositionY || property == CSSPropertyWebkitMaskPositionY)
                    foundPositionYCSSProperty = true;
            }

            if (commonValue.isNull())
                commonValue = valueText;
            else if (commonValue != valueText)
                commonValue = emptyString(); // Could use value here other than a CSS-wide value keyword or the null string.
        }

        if (shorthand.id() == CSSPropertyMask && layerResult.isEmpty())
            layerResult.append(getValueName(CSSValueNone));

        if (!layerResult.isEmpty())
            result.append(result.isEmpty() ? "" : ", ", layerResult.toString());
    }

    if (isCSSWideValueKeyword(commonValue))
        return commonValue;

    return result.isEmpty() ? String() : result.toString();
}

String StyleProperties::getGridTemplateValue() const
{
    StringBuilder result;
    int areasIndex = findPropertyIndex(CSSPropertyGridTemplateAreas);
    if (areasIndex == -1)
        return String();
    auto rows = getPropertyCSSValue(CSSPropertyGridTemplateRows);
    if (!rows)
        return String();
    auto columns = getPropertyCSSValue(CSSPropertyGridTemplateColumns);
    if (!columns)
        return String();

    auto areas = propertyAt(areasIndex);
    if (!is<CSSGridTemplateAreasValue>(areas.value())) {
        String rowsText = rows->cssText();
        result.append(rowsText);

        String columnsText = columns->cssText();
        // If the values are identical, and either a css wide keyword
        // or 'none', then we can just output it once.
        if (columnsText != rowsText || (!isCSSWideValueKeyword(columnsText) && !isValueID(columns, CSSValueNone))) {
            result.append(" / ");
            result.append(columnsText);
        }
        return result.toString();
    }
    // We only want to try serializing the interleaved areas/templates
    // format if it was set from this shorthand, since that automatically
    // excludes values that can't be represented in this format (subgrid,
    // and the repeat() function).
    if (!areas.toCSSProperty().isSetFromShorthand())
        return String();

    ASSERT(is<CSSValueList>(rows));
    ASSERT(is<CSSGridTemplateAreasValue>(areas.value()));
    auto& areasValue = downcast<CSSGridTemplateAreasValue>(*areas.value());
    bool first = true;
    unsigned row = 0;
    for (auto& currentValue : downcast<CSSValueList>(*rows)) {
        if (!first)
            result.append(" ");
        first = false;

        if (is<CSSGridLineNamesValue>(currentValue))
            result.append(currentValue->cssText());
        else {
            result.append("\"");
            result.append(areasValue.stringForRow(row));
            result.append("\"");

            if (!isValueID(currentValue, CSSValueAuto)) {
                result.append(" ");
                result.append(currentValue->cssText());
            }
            row++;
        }
    }

    String columnsText = columns->cssText();
    if (!isNoneValue(columns)) {
        result.append(" / ");
        result.append(columnsText);
    }

    return result.toString();
}

static bool gridAutoFlowContains(const RefPtr<CSSValue>& autoFlow, CSSValueID id)
{
    if (is<CSSValueList>(autoFlow)) {
        for (auto& currentValue : downcast<CSSValueList>(*autoFlow)) {
            if (isValueID(currentValue, id))
                return true;
        }
        return false;
    }
    return isValueID(autoFlow, id);
}

String StyleProperties::getGridValue() const
{
    auto autoColumns = getPropertyCSSValue(CSSPropertyGridAutoColumns);
    auto autoRows = getPropertyCSSValue(CSSPropertyGridAutoRows);
    auto autoFlow = getPropertyCSSValue(CSSPropertyGridAutoFlow);
    if (!autoColumns || !autoRows || !autoFlow)
        return String();

    if (isValueIDIncludingList(autoColumns, CSSValueAuto) && isValueIDIncludingList(autoRows, CSSValueAuto) && isValueIDIncludingList(autoFlow, CSSValueRow))
        return getGridTemplateValue();

    if (!isValueID(getPropertyCSSValue(CSSPropertyGridTemplateAreas), CSSValueNone))
        return String();

    auto rows = getPropertyCSSValue(CSSPropertyGridTemplateRows);
    auto columns = getPropertyCSSValue(CSSPropertyGridTemplateColumns);
    if (!rows || !columns)
        return String();

    StringBuilder result;

    if (gridAutoFlowContains(autoFlow, CSSValueColumn)) {
        if (!isValueIDIncludingList(autoRows, CSSValueAuto) || !isValueIDIncludingList(columns, CSSValueNone))
            return String();

        result.append(rows->cssText());
        result.append(" / auto-flow");
        if (gridAutoFlowContains(autoFlow, CSSValueDense))
            result.append(" dense");

        if (!isValueIDIncludingList(autoColumns, CSSValueAuto)) {
            result.append(" ");
            result.append(autoColumns->cssText());
        }

        return result.toString();
    }

    if (!gridAutoFlowContains(autoFlow, CSSValueRow) && !gridAutoFlowContains(autoFlow, CSSValueDense))
        return String();

    if (!isValueIDIncludingList(autoColumns, CSSValueAuto) || !isValueIDIncludingList(rows, CSSValueNone))
        return String();

    result.append("auto-flow");
    if (gridAutoFlowContains(autoFlow, CSSValueDense))
        result.append(" dense");

    if (!isValueIDIncludingList(autoRows, CSSValueAuto)) {
        result.append(" ");
        result.append(autoRows->cssText());
    }

    result.append(" / ");
    result.append(columns->cssText());

    return result.toString();
}

String StyleProperties::getGridShorthandValue(const StylePropertyShorthand& shorthand) const
{
    return getShorthandValue(shorthand, " / ");
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

String StyleProperties::getGridRowColumnShorthandValue(const StylePropertyShorthand& shorthand) const
{
    auto start = getPropertyCSSValue(shorthand.properties()[0]);
    auto end = getPropertyCSSValue(shorthand.properties()[1]);

    if (!start || !end)
        return String();

    StringBuilder result;
    result.append(start->cssText());
    if (!canOmitTrailingGridAreaValue(*start, *end)) {
        result.append(" / ");
        result.append(end->cssText());
    }

    return result.toString();
}

String StyleProperties::getGridAreaShorthandValue() const
{
    RefPtr<CSSValue> values[4];
    values[0] = getPropertyCSSValue(CSSPropertyGridRowStart);
    values[1] = getPropertyCSSValue(CSSPropertyGridColumnStart);
    values[2] = getPropertyCSSValue(CSSPropertyGridRowEnd);
    values[3] = getPropertyCSSValue(CSSPropertyGridColumnEnd);

    if (!values[0] || !values[1] || !values[2] || !values[3])
        return String();

    StringBuilder result;
    result.append(values[0]->cssText());

    unsigned trailingValues = 3;
    if (canOmitTrailingGridAreaValue(*values[1], *values[3])) {
        trailingValues--;
        if (canOmitTrailingGridAreaValue(*values[0], *values[2])) {
            trailingValues--;
            if (canOmitTrailingGridAreaValue(*values[0], *values[1]))
                trailingValues--;
        }
    }

    for (unsigned i = 1; i <= trailingValues; ++i) {
        result.append(" / ");
        result.append(values[i]->cssText());
    }

    return result.toString();
}

String StyleProperties::getShorthandValue(const StylePropertyShorthand& shorthand, const char* separator) const
{
    String commonValue;
    StringBuilder result;
    for (unsigned i = 0; i < shorthand.length(); ++i) {
        if (!isPropertyImplicit(shorthand.properties()[i])) {
            auto value = getPropertyCSSValue(shorthand.properties()[i]);
            if (!value)
                return String();
            String valueText = value->cssText();
            if (!i)
                commonValue = valueText;
            else if (!commonValue.isNull() && commonValue != valueText)
                commonValue = String();
            if (value->isInitialValue())
                continue;
            if (!result.isEmpty())
                result.append(separator);
            result.append(valueText);
        } else
            commonValue = String();
    }
    if (isCSSWideValueKeyword(commonValue))
        return commonValue;
    if (result.isEmpty())
        return String();
    return result.toString();
}

// Returns a non-null value if all properties have the same value.
String StyleProperties::getCommonValue(const StylePropertyShorthand& shorthand) const
{
    String result;
    bool lastPropertyWasImportant = false;
    for (unsigned i = 0; i < shorthand.length(); ++i) {
        auto value = getPropertyCSSValue(shorthand.properties()[i]);
        if (!value)
            return String();
        // FIXME: CSSInitialValue::cssText should generate the right value.
        String text = value->cssText();
        if (text.isNull())
            return String();
        if (result.isNull())
            result = text;
        else if (result != text)
            return String();
        bool currentPropertyIsImportant = propertyIsImportant(shorthand.properties()[i]);
        if (i && lastPropertyWasImportant != currentPropertyIsImportant)
            return String();
        lastPropertyWasImportant = currentPropertyIsImportant;
    }
    return result;
}

String StyleProperties::getAlignmentShorthandValue(const StylePropertyShorthand& shorthand) const
{
    String value = getCommonValue(shorthand);
    if (value.isNull() || value.isEmpty())
        return getShorthandValue(shorthand);
    return value;
}

String StyleProperties::borderImagePropertyValue(CSSPropertyID propertyID) const
{
    const StylePropertyShorthand& shorthand = borderImageShorthand();
    StringBuilder result;
    bool lastPropertyWasImportant = false;
    bool omittedSlice = false;
    bool omittedWidth = false;
    String commonWideValueText;
    auto separator = "";
    for (unsigned i = 0; i < shorthand.length(); ++i) {
        // All longhands should have the same importance.
        auto longhand = shorthand.properties()[i];
        bool currentPropertyIsImportant = propertyIsImportant(longhand);
        if (i && lastPropertyWasImportant != currentPropertyIsImportant)
            return String();
        lastPropertyWasImportant = currentPropertyIsImportant;

        // All longhands should be present.
        auto value = getPropertyCSSValue(longhand);
        if (!value)
            return String();

        // Omit implicit initial values. However, border-image-width and border-image-outset require border-image-slice.
        if (value->isInitialValue() && isPropertyImplicit(longhand)) {
            if (longhand == CSSPropertyBorderImageSlice)
                omittedSlice = true;
            else if (longhand == CSSPropertyBorderImageWidth)
                omittedWidth = true;
            continue;
        }
        if (omittedSlice && (longhand == CSSPropertyBorderImageWidth || longhand == CSSPropertyBorderImageOutset))
            return String();

        // -webkit-border-image has a legacy behavior that makes fixed border slices also set the border widths.
        if (is<CSSBorderImageWidthValue>(value.get())) {
            auto* borderImageWidth = downcast<CSSBorderImageWidthValue>(value.get());
            Quad* widths = borderImageWidth->widths();
            bool overridesBorderWidths = propertyID == CSSPropertyWebkitBorderImage && widths && (widths->top()->isLength() || widths->right()->isLength() || widths->bottom()->isLength() || widths->left()->isLength());
            if (overridesBorderWidths != borderImageWidth->m_overridesBorderWidths)
                return String();
            value = borderImageWidth->m_widths;
        }

        // If a longhand is set to a css-wide keyword, the others should be the same.
        String valueText = value->cssText();
        if (isCSSWideValueKeyword(valueText)) {
            if (!i)
                commonWideValueText = valueText;
            else if (commonWideValueText != valueText)
                return String();
            continue;
        }
        if (!commonWideValueText.isNull())
            return String();

        // Append separator and text.
        if (longhand == CSSPropertyBorderImageWidth)
            separator = " / ";
        else if (longhand == CSSPropertyBorderImageOutset)
            separator = omittedWidth ? " / / " : " / ";
        result.append(separator, valueText);
        separator = " ";
    }
    if (!commonWideValueText.isNull())
        return commonWideValueText;
    return result.toString();
}

String StyleProperties::borderPropertyValue(const StylePropertyShorthand& width, const StylePropertyShorthand& style, const StylePropertyShorthand& color) const
{
    const StylePropertyShorthand properties[3] = { width, style, color };
    String commonValue;
    StringBuilder result;
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(properties); ++i) {
        String value = getCommonValue(properties[i]);
        if (value.isNull())
            return String();
        if (!i)
            commonValue = value;
        else if (commonValue != value)
            commonValue = String();
        if (value == "initial"_s)
            continue;
        if (!result.isEmpty())
            result.append(' ');
        result.append(value);
    }
    if (isCSSWideValueKeyword(commonValue))
        return commonValue;
    return result.toString();
}

String StyleProperties::pageBreakPropertyValue(const StylePropertyShorthand& shorthand) const
{
    auto value = getPropertyCSSValue(shorthand.properties()[0]);
    if (!value)
        return String();
    // FIXME: Remove this isCSSWideKeyword check after we do this consistently for all shorthands in getPropertyValue.
    if (value->isCSSWideKeyword())
        return value->cssText();
    
    if (!is<CSSPrimitiveValue>(*value))
        return String();
    
    CSSValueID valueId = downcast<CSSPrimitiveValue>(*value).valueID();
    switch (valueId) {
    case CSSValuePage:
        return "always"_s;
    case CSSValueAuto:
    case CSSValueAvoid:
    case CSSValueLeft:
    case CSSValueRight:
        return value->cssText();
    default:
        return String();
    }
}

RefPtr<CSSValue> StyleProperties::getPropertyCSSValue(CSSPropertyID propertyID) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return nullptr;
    return propertyAt(foundPropertyIndex).value();
}

RefPtr<CSSValue> StyleProperties::getCustomPropertyCSSValue(const String& propertyName) const
{
    int foundPropertyIndex = findCustomPropertyIndex(propertyName);
    if (foundPropertyIndex == -1)
        return nullptr;
    return propertyAt(foundPropertyIndex).value();
}

bool MutableStyleProperties::removeShorthandProperty(CSSPropertyID propertyID)
{
    StylePropertyShorthand shorthand = shorthandForProperty(propertyID);
    if (!shorthand.length())
        return false;

    return removePropertiesInSet(shorthand.properties(), shorthand.length());
}

bool MutableStyleProperties::removeProperty(CSSPropertyID propertyID, String* returnText)
{
    if (removeShorthandProperty(propertyID)) {
        // FIXME: Return an equivalent shorthand when possible.
        if (returnText)
            *returnText = emptyString();
        return true;
    }

    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1) {
        if (returnText)
            *returnText = emptyString();
        return false;
    }

    if (returnText)
        *returnText = propertyAt(foundPropertyIndex).value()->cssText();

    // A more efficient removal strategy would involve marking entries as empty
    // and sweeping them when the vector grows too big.
    m_propertyVector.remove(foundPropertyIndex);

    return true;
}

bool MutableStyleProperties::removeCustomProperty(const String& propertyName, String* returnText)
{
    int foundPropertyIndex = findCustomPropertyIndex(propertyName);
    if (foundPropertyIndex == -1) {
        if (returnText)
            *returnText = emptyString();
        return false;
    }

    if (returnText)
        *returnText = propertyAt(foundPropertyIndex).value()->cssText();

    // A more efficient removal strategy would involve marking entries as empty
    // and sweeping them when the vector grows too big.
    m_propertyVector.remove(foundPropertyIndex);

    return true;
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
    return getPropertyNameString(propertyAt(foundPropertyIndex).shorthandID());
}

bool StyleProperties::isPropertyImplicit(CSSPropertyID propertyID) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return false;
    return propertyAt(foundPropertyIndex).isImplicit();
}

bool MutableStyleProperties::setProperty(CSSPropertyID propertyID, const String& value, bool important, CSSParserContext parserContext)
{
    if (!isCSSPropertyExposed(propertyID, &parserContext.propertySettings) && !isInternalCSSProperty(propertyID)) {
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
    return CSSParser::parseValue(*this, propertyID, value, important, parserContext) == CSSParser::ParseResult::Changed;
}

bool MutableStyleProperties::setProperty(CSSPropertyID propertyID, const String& value, bool important)
{
    CSSParserContext parserContext(cssParserMode());
    return setProperty(propertyID, value, important, parserContext);
}

bool MutableStyleProperties::setCustomProperty(const Document* document, const String& propertyName, const String& value, bool important, CSSParserContext parserContext)
{
    // Setting the value to an empty string just removes the property in both IE and Gecko.
    // Setting it to null seems to produce less consistent results, but we treat it just the same.
    if (value.isEmpty())
        return removeCustomProperty(propertyName);

    parserContext.mode = cssParserMode();

    String syntax = "*"_s;
    auto* registered = document ? document->getCSSRegisteredCustomPropertySet().get(propertyName) : nullptr;

    if (registered)
        syntax = registered->syntax;

    CSSTokenizer tokenizer(value);
    if (!CSSPropertyParser::canParseTypedCustomPropertyValue(syntax, tokenizer.tokenRange(), parserContext))
        return false;

    // When replacing an existing property value, this moves the property to the end of the list.
    // Firefox preserves the position, and MSIE moves the property to the beginning.
    return CSSParser::parseCustomPropertyValue(*this, AtomString { propertyName }, value, important, parserContext) == CSSParser::ParseResult::Changed;
}

void MutableStyleProperties::setProperty(CSSPropertyID propertyID, RefPtr<CSSValue>&& value, bool important)
{
    StylePropertyShorthand shorthand = shorthandForProperty(propertyID);
    if (!shorthand.length()) {
        setProperty(CSSProperty(propertyID, WTFMove(value), important));
        return;
    }

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
    if (!removeShorthandProperty(property.id())) {
        CSSProperty* toReplace = slot;
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
            toReplace = nullptr;
        }
    }

    m_propertyVector.append(property);
    return true;
}

bool MutableStyleProperties::setProperty(CSSPropertyID propertyID, CSSValueID identifier, bool important)
{
    return setProperty(CSSProperty(propertyID, CSSValuePool::singleton().createIdentifierValue(identifier), important));
}

bool MutableStyleProperties::setProperty(CSSPropertyID propertyID, CSSPropertyID identifier, bool important)
{
    return setProperty(CSSProperty(propertyID, CSSValuePool::singleton().createIdentifierValue(identifier), important));
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

StringBuilder StyleProperties::asTextInternal() const
{
    StringBuilder result;

    int positionXPropertyIndex = -1;
    int positionYPropertyIndex = -1;

    constexpr unsigned shorthandPropertyCount = lastShorthandProperty - firstShorthandProperty + 1;
    std::bitset<shorthandPropertyCount> shorthandPropertyUsed;
    std::bitset<shorthandPropertyCount> shorthandPropertyAppeared;

    unsigned size = propertyCount();
    unsigned numDecls = 0;
    for (unsigned n = 0; n < size; ++n) {
        PropertyReference property = propertyAt(n);
        CSSPropertyID propertyID = property.id();
        Vector<CSSPropertyID> shorthands;
        String value;

        if (is<CSSPendingSubstitutionValue>(property.value())) {
            auto& substitutionValue = downcast<CSSPendingSubstitutionValue>(*property.value());
            shorthands.append(substitutionValue.shorthandPropertyId());
            value = substitutionValue.shorthandValue().cssText();
        } else {
            switch (propertyID) {
            case CSSPropertyAnimationName:
            case CSSPropertyAnimationDuration:
            case CSSPropertyAnimationTimingFunction:
            case CSSPropertyAnimationDelay:
            case CSSPropertyAnimationIterationCount:
            case CSSPropertyAnimationDirection:
            case CSSPropertyAnimationFillMode:
            case CSSPropertyAnimationPlayState:
                shorthands.append(CSSPropertyAnimation);
                break;
            case CSSPropertyBackgroundPositionX:
                positionXPropertyIndex = n;
                continue;
            case CSSPropertyBackgroundPositionY:
                positionYPropertyIndex = n;
                continue;
            case CSSPropertyBorderTopWidth:
            case CSSPropertyBorderRightWidth:
            case CSSPropertyBorderBottomWidth:
            case CSSPropertyBorderLeftWidth:
                // FIXME: Deal with cases where only some of border sides are specified.
                shorthands.append(CSSPropertyBorder);
                shorthands.append(CSSPropertyBorderWidth);
                    break;
            case CSSPropertyBorderTopStyle:
            case CSSPropertyBorderRightStyle:
            case CSSPropertyBorderBottomStyle:
            case CSSPropertyBorderLeftStyle:
                shorthands.append(CSSPropertyBorder);
                shorthands.append(CSSPropertyBorderStyle);
                break;
            case CSSPropertyBorderTopColor:
            case CSSPropertyBorderRightColor:
            case CSSPropertyBorderBottomColor:
            case CSSPropertyBorderLeftColor:
                shorthands.append(CSSPropertyBorder);
                shorthands.append(CSSPropertyBorderColor);
                break;
            case CSSPropertyBorderBlockStartWidth:
            case CSSPropertyBorderBlockEndWidth:
                shorthands.append(CSSPropertyBorderBlock);
                shorthands.append(CSSPropertyBorderBlockWidth);
                break;
            case CSSPropertyBorderBlockStartStyle:
            case CSSPropertyBorderBlockEndStyle:
                shorthands.append(CSSPropertyBorderBlock);
                shorthands.append(CSSPropertyBorderBlockStyle);
                break;
            case CSSPropertyBorderBlockStartColor:
            case CSSPropertyBorderBlockEndColor:
                shorthands.append(CSSPropertyBorderBlock);
                shorthands.append(CSSPropertyBorderBlockColor);
                break;
            case CSSPropertyBorderInlineStartWidth:
            case CSSPropertyBorderInlineEndWidth:
                shorthands.append(CSSPropertyBorderInline);
                shorthands.append(CSSPropertyBorderInlineWidth);
                break;
            case CSSPropertyBorderInlineStartStyle:
            case CSSPropertyBorderInlineEndStyle:
                shorthands.append(CSSPropertyBorderInline);
                shorthands.append(CSSPropertyBorderInlineStyle);
                break;
            case CSSPropertyBorderTopLeftRadius:
            case CSSPropertyBorderTopRightRadius:
            case CSSPropertyBorderBottomRightRadius:
            case CSSPropertyBorderBottomLeftRadius:
                shorthands.append(CSSPropertyBorderRadius);
                break;
            case CSSPropertyBorderInlineStartColor:
            case CSSPropertyBorderInlineEndColor:
                shorthands.append(CSSPropertyBorderInline);
                shorthands.append(CSSPropertyBorderInlineColor);
                break;
            case CSSPropertyWebkitBorderHorizontalSpacing:
            case CSSPropertyWebkitBorderVerticalSpacing:
                shorthands.append(CSSPropertyBorderSpacing);
                break;
            case CSSPropertyBorderImageSource:
            case CSSPropertyBorderImageSlice:
            case CSSPropertyBorderImageWidth:
            case CSSPropertyBorderImageOutset:
            case CSSPropertyBorderImageRepeat:
                shorthands.append(CSSPropertyBorderImage);
                shorthands.append(CSSPropertyWebkitBorderImage);
                break;
            case CSSPropertyFontFamily:
            case CSSPropertyLineHeight:
            case CSSPropertyFontSize:
            case CSSPropertyFontStyle:
            case CSSPropertyFontVariantCaps:
            case CSSPropertyFontWeight:
                // Don't use CSSPropertyFont because old UAs can't recognize them but are important for editing.
                break;
            case CSSPropertyTop:
            case CSSPropertyRight:
            case CSSPropertyBottom:
            case CSSPropertyLeft:
                shorthands.append(CSSPropertyInset);
                break;
            case CSSPropertyInsetBlockStart:
            case CSSPropertyInsetBlockEnd:
                shorthands.append(CSSPropertyInsetBlock);
                break;
            case CSSPropertyInsetInlineStart:
            case CSSPropertyInsetInlineEnd:
                shorthands.append(CSSPropertyInsetInline);
                break;
            case CSSPropertyListStyleType:
            case CSSPropertyListStylePosition:
            case CSSPropertyListStyleImage:
                shorthands.append(CSSPropertyListStyle);
                break;
            case CSSPropertyMarginTop:
            case CSSPropertyMarginRight:
            case CSSPropertyMarginBottom:
            case CSSPropertyMarginLeft:
                shorthands.append(CSSPropertyMargin);
                break;
            case CSSPropertyMarginBlockStart:
            case CSSPropertyMarginBlockEnd:
                shorthands.append(CSSPropertyMarginBlock);
                break;
            case CSSPropertyMarginInlineStart:
            case CSSPropertyMarginInlineEnd:
                shorthands.append(CSSPropertyMarginInline);
                break;
            case CSSPropertyOutlineWidth:
            case CSSPropertyOutlineStyle:
            case CSSPropertyOutlineColor:
                shorthands.append(CSSPropertyOutline);
                break;
            case CSSPropertyOverflowX:
            case CSSPropertyOverflowY:
                shorthands.append(CSSPropertyOverflow);
                break;
            case CSSPropertyOverscrollBehaviorX:
            case CSSPropertyOverscrollBehaviorY:
                shorthands.append(CSSPropertyOverscrollBehavior);
                break;
            case CSSPropertyPaddingTop:
            case CSSPropertyPaddingRight:
            case CSSPropertyPaddingBottom:
            case CSSPropertyPaddingLeft:
                shorthands.append(CSSPropertyPadding);
                break;
            case CSSPropertyPaddingBlockStart:
            case CSSPropertyPaddingBlockEnd:
                shorthands.append(CSSPropertyPaddingBlock);
                break;
            case CSSPropertyPaddingInlineStart:
            case CSSPropertyPaddingInlineEnd:
                shorthands.append(CSSPropertyPaddingInline);
                break;
            case CSSPropertyScrollMarginTop:
            case CSSPropertyScrollMarginRight:
            case CSSPropertyScrollMarginBottom:
            case CSSPropertyScrollMarginLeft:
                shorthands.append(CSSPropertyScrollMargin);
                break;
            case CSSPropertyScrollMarginBlockStart:
            case CSSPropertyScrollMarginBlockEnd:
                shorthands.append(CSSPropertyScrollMarginBlock);
                break;
            case CSSPropertyScrollMarginInlineStart:
            case CSSPropertyScrollMarginInlineEnd:
                shorthands.append(CSSPropertyScrollMarginInline);
                break;
            case CSSPropertyScrollPaddingTop:
            case CSSPropertyScrollPaddingRight:
            case CSSPropertyScrollPaddingBottom:
            case CSSPropertyScrollPaddingLeft:
                shorthands.append(CSSPropertyScrollPadding);
                break;
            case CSSPropertyScrollPaddingBlockStart:
            case CSSPropertyScrollPaddingBlockEnd:
                shorthands.append(CSSPropertyScrollPaddingBlock);
                break;
            case CSSPropertyScrollPaddingInlineStart:
            case CSSPropertyScrollPaddingInlineEnd:
                shorthands.append(CSSPropertyScrollPaddingInline);
                break;
            case CSSPropertyTextDecorationLine:
                shorthands.append(CSSPropertyTextDecoration);
                break;
            case CSSPropertyTransitionProperty:
            case CSSPropertyTransitionDuration:
            case CSSPropertyTransitionTimingFunction:
            case CSSPropertyTransitionDelay:
                shorthands.append(CSSPropertyTransition);
                break;
            case CSSPropertyFlexDirection:
            case CSSPropertyFlexWrap:
                shorthands.append(CSSPropertyFlexFlow);
                break;
            case CSSPropertyFlexBasis:
            case CSSPropertyFlexGrow:
            case CSSPropertyFlexShrink:
                shorthands.append(CSSPropertyFlex);
                break;
            case CSSPropertyWebkitMaskPositionX:
            case CSSPropertyWebkitMaskPositionY:
            case CSSPropertyMaskImage:
            case CSSPropertyMaskRepeat:
            case CSSPropertyMaskPosition:
            case CSSPropertyMaskClip:
            case CSSPropertyMaskOrigin:
                shorthands.append(CSSPropertyMask);
                break;
            case CSSPropertyWebkitMaskClip:
            case CSSPropertyWebkitMaskPosition:
                // TODO: A lot of the above properties can be both prefixed and unprefixed?
                shorthands.append(CSSPropertyWebkitMask);
                break;
            case CSSPropertyPerspectiveOriginX:
            case CSSPropertyPerspectiveOriginY:
                shorthands.append(CSSPropertyPerspectiveOrigin);
                break;
            case CSSPropertyTransformOriginX:
            case CSSPropertyTransformOriginY:
            case CSSPropertyTransformOriginZ:
                shorthands.append(CSSPropertyTransformOrigin);
                break;
            case CSSPropertyContainIntrinsicHeight:
            case CSSPropertyContainIntrinsicWidth:
                shorthands.append(CSSPropertyContainIntrinsicSize);
                break;
            case CSSPropertyGridTemplateRows:
            case CSSPropertyGridTemplateColumns:
            case CSSPropertyGridTemplateAreas:
                shorthands.append(CSSPropertyGrid);
                shorthands.append(CSSPropertyGridTemplate);
                break;
            case CSSPropertyGridAutoFlow:
            case CSSPropertyGridAutoRows:
            case CSSPropertyGridAutoColumns:
                shorthands.append(CSSPropertyGrid);
                break;
            default:
                break;
            }
        }

        bool alreadyUsedShorthand = false;
        for (auto& shorthandPropertyID : shorthands) {
            ASSERT(isShorthandCSSProperty(shorthandPropertyID));
            unsigned shorthandPropertyIndex = shorthandPropertyID - firstShorthandProperty;

            ASSERT(shorthandPropertyIndex < shorthandPropertyUsed.size());
            if (shorthandPropertyUsed[shorthandPropertyIndex]) {
                alreadyUsedShorthand = true;
                break;
            }
            if (!shorthandPropertyAppeared[shorthandPropertyIndex] && value.isNull())
                value = getPropertyValue(shorthandPropertyID);
            shorthandPropertyAppeared.set(shorthandPropertyIndex);

            if (!value.isNull()) {
                propertyID = shorthandPropertyID;
                shorthandPropertyUsed.set(shorthandPropertyIndex);
                break;
            }
        }
        if (alreadyUsedShorthand)
            continue;

        if (value.isNull())
            value = property.value()->cssText();

        if (propertyID != CSSPropertyCustom && value == "initial"_s && !CSSProperty::isInheritedProperty(propertyID))
            continue;

        if (numDecls++)
            result.append(' ');

        if (propertyID == CSSPropertyCustom)
            result.append(downcast<CSSCustomPropertyValue>(*property.value()).name());
        else
            result.append(getPropertyName(propertyID));

        result.append(": ", value, property.isImportant() ? " !important" : "", ';');
    }

    // FIXME: This is a not-so-nice way to turn x/y positions into single background-position/repeat in output.
    // In 2007 we decided this was required because background-position-x/y are non-standard properties and WebKit generated output would not work in Firefox (<rdar://problem/5143183>).
    // FIXME: This can probably be cleaned up now that background-position-x/y are standardized.
    auto appendPositionOrProperty = [&] (int xIndex, int yIndex, const char* name, const StylePropertyShorthand& shorthand) {
        if (xIndex != -1 && yIndex != -1 && propertyAt(xIndex).isImportant() == propertyAt(yIndex).isImportant()) {
            String value;
            auto xProperty = propertyAt(xIndex);
            auto yProperty = propertyAt(yIndex);
            if (xProperty.value()->isValueList() || yProperty.value()->isValueList())
                value = getLayeredShorthandValue(shorthand);
            else {
                auto x = xProperty.value()->cssText();
                auto y = yProperty.value()->cssText();
                if (x == y && isCSSWideValueKeyword(x))
                    value = x;
                else
                    value = makeString(x, ' ', y);
            }
            if (value != "initial"_s) {
                result.append(numDecls ? " " : "", name, ": ", value, xProperty.isImportant() ? " !important" : "", ';');
                ++numDecls;
            }
        } else {
            if (xIndex != -1) {
                if (numDecls++)
                    result.append(' ');
                result.append(propertyAt(xIndex).cssText());
            }
            if (yIndex != -1) {
                if (numDecls++)
                    result.append(' ');
                result.append(propertyAt(yIndex).cssText());
            }
        }
    };

    appendPositionOrProperty(positionXPropertyIndex, positionYPropertyIndex, "background-position", backgroundPositionShorthand());

    ASSERT(!numDecls ^ !result.isEmpty());
    return result;
}

bool StyleProperties::hasCSSOMWrapper() const
{
    return is<MutableStyleProperties>(*this) && downcast<MutableStyleProperties>(*this).m_cssomWrapper;
}

void MutableStyleProperties::mergeAndOverrideOnConflict(const StyleProperties& other)
{
    unsigned size = other.propertyCount();
    for (unsigned i = 0; i < size; ++i)
        addParsedProperty(other.propertyAt(i).toCSSProperty());
}

bool StyleProperties::traverseSubresources(const Function<bool(const CachedResource&)>& handler) const
{
    unsigned size = propertyCount();
    for (unsigned i = 0; i < size; ++i) {
        if (propertyAt(i).value()->traverseSubresources(handler))
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

const unsigned numBlockProperties = WTF_ARRAY_LENGTH(blockProperties);

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
    for (unsigned i = 0; i < length; ++i)
        toRemove.add(set[i]);

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

int ImmutableStyleProperties::findCustomPropertyIndex(const String& propertyName) const
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

int MutableStyleProperties::findCustomPropertyIndex(const String& propertyName) const
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
        return 0;
    return &m_propertyVector.at(foundPropertyIndex);
}

CSSProperty* MutableStyleProperties::findCustomCSSPropertyWithName(const String& propertyName)
{
    int foundPropertyIndex = findCustomPropertyIndex(propertyName);
    if (foundPropertyIndex == -1)
        return 0;
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

String StyleProperties::PropertyReference::cssName() const
{
    if (id() == CSSPropertyCustom)
        return downcast<CSSCustomPropertyValue>(*value()).name();
    return getPropertyNameString(id());
}

String StyleProperties::PropertyReference::cssText() const
{
    return makeString(cssName(), ": ", m_value->cssText(), isImportant() ? " !important" : "", ';');
}

} // namespace WebCore
