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

#include "CSSComputedStyleDeclaration.h"
#include "CSSCustomPropertyValue.h"
#include "CSSParser.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "CSSVariableDependentValue.h"
#include "Document.h"
#include "PropertySetCSSStyleDeclaration.h"
#include "StylePropertyShorthand.h"
#include "StyleSheetContents.h"
#include <bitset>
#include <wtf/text/StringBuilder.h>

#ifndef NDEBUG
#include <stdio.h>
#include <wtf/ASCIICType.h>
#include <wtf/text/CString.h>
#endif

namespace WebCore {

static size_t sizeForImmutableStylePropertiesWithPropertyCount(unsigned count)
{
    return sizeof(ImmutableStyleProperties) - sizeof(void*) + sizeof(CSSValue*) * count + sizeof(StylePropertyMetadata) * count;
}

static bool isInitialOrInherit(const String& value)
{
    return value.length() == 7 && (value == "initial" || value == "inherit");
}

Ref<ImmutableStyleProperties> ImmutableStyleProperties::create(const CSSProperty* properties, unsigned count, CSSParserMode cssParserMode)
{
    void* slot = WTF::fastMalloc(sizeForImmutableStylePropertiesWithPropertyCount(count));
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
    : StyleProperties(cssParserMode)
{
}

MutableStyleProperties::MutableStyleProperties(const CSSProperty* properties, unsigned length)
    : StyleProperties(CSSStrictMode)
{
    m_propertyVector.reserveInitialCapacity(length);
    for (unsigned i = 0; i < length; ++i)
        m_propertyVector.uncheckedAppend(properties[i]);
}

MutableStyleProperties::~MutableStyleProperties()
{
}

ImmutableStyleProperties::ImmutableStyleProperties(const CSSProperty* properties, unsigned length, CSSParserMode cssParserMode)
    : StyleProperties(cssParserMode, length)
{
    StylePropertyMetadata* metadataArray = const_cast<StylePropertyMetadata*>(this->metadataArray());
    CSSValue** valueArray = const_cast<CSSValue**>(this->valueArray());
    for (unsigned i = 0; i < length; ++i) {
        metadataArray[i] = properties[i].metadata();
        valueArray[i] = properties[i].value();
        valueArray[i]->ref();
    }
}

ImmutableStyleProperties::~ImmutableStyleProperties()
{
    CSSValue** valueArray = const_cast<CSSValue**>(this->valueArray());
    for (unsigned i = 0; i < m_arraySize; ++i)
        valueArray[i]->deref();
}

MutableStyleProperties::MutableStyleProperties(const StyleProperties& other)
    : StyleProperties(other.cssParserMode())
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

String StyleProperties::getPropertyValue(CSSPropertyID propertyID) const
{
    RefPtr<CSSValue> value = getPropertyCSSValue(propertyID);
    if (value)
        return value->cssText();

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
    case CSSPropertyBackgroundRepeat:
        return getLayeredShorthandValue(backgroundRepeatShorthand());
    case CSSPropertyBackground:
        return getLayeredShorthandValue(backgroundShorthand());
    case CSSPropertyBorder:
        return borderPropertyValue(OmitUncommonValues);
    case CSSPropertyBorderTop:
        return getShorthandValue(borderTopShorthand());
    case CSSPropertyBorderRight:
        return getShorthandValue(borderRightShorthand());
    case CSSPropertyBorderBottom:
        return getShorthandValue(borderBottomShorthand());
    case CSSPropertyBorderLeft:
        return getShorthandValue(borderLeftShorthand());
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
    case CSSPropertyFlex:
        return getShorthandValue(flexShorthand());
    case CSSPropertyFlexFlow:
        return getShorthandValue(flexFlowShorthand());
#if ENABLE(CSS_GRID_LAYOUT)
    case CSSPropertyWebkitGridArea:
        return getShorthandValue(webkitGridAreaShorthand());
    case CSSPropertyWebkitGridTemplate:
        return getShorthandValue(webkitGridTemplateShorthand());
    case CSSPropertyWebkitGrid:
        return getShorthandValue(webkitGridShorthand());
    case CSSPropertyWebkitGridColumn:
        return getShorthandValue(webkitGridColumnShorthand());
    case CSSPropertyWebkitGridRow:
        return getShorthandValue(webkitGridRowShorthand());
#endif
    case CSSPropertyFont:
        return fontValue();
    case CSSPropertyMargin:
        return get4Values(marginShorthand());
    case CSSPropertyWebkitMarginCollapse:
        return getShorthandValue(webkitMarginCollapseShorthand());
    case CSSPropertyOverflow:
        return getCommonValue(overflowShorthand());
    case CSSPropertyPadding:
        return get4Values(paddingShorthand());
    case CSSPropertyTransition:
        return getLayeredShorthandValue(transitionShorthand());
    case CSSPropertyListStyle:
        return getShorthandValue(listStyleShorthand());
    case CSSPropertyWebkitMarquee:
        return getShorthandValue(webkitMarqueeShorthand());
    case CSSPropertyWebkitMaskPosition:
        return getLayeredShorthandValue(webkitMaskPositionShorthand());
    case CSSPropertyWebkitMaskRepeat:
        return getLayeredShorthandValue(webkitMaskRepeatShorthand());
    case CSSPropertyWebkitMask:
        return getLayeredShorthandValue(webkitMaskShorthand());
    case CSSPropertyWebkitTextEmphasis:
        return getShorthandValue(webkitTextEmphasisShorthand());
    case CSSPropertyWebkitTextStroke:
        return getShorthandValue(webkitTextStrokeShorthand());
    case CSSPropertyPerspectiveOrigin:
        return getShorthandValue(perspectiveOriginShorthand());
    case CSSPropertyTransformOrigin:
        return getShorthandValue(transformOriginShorthand());
    case CSSPropertyWebkitTransition:
        return getLayeredShorthandValue(webkitTransitionShorthand());
    case CSSPropertyWebkitAnimation:
        return getLayeredShorthandValue(webkitAnimationShorthand());
    case CSSPropertyMarker: {
        RefPtr<CSSValue> value = getPropertyCSSValueInternal(CSSPropertyMarkerStart);
        if (value)
            return value->cssText();
        return String();
    }
    case CSSPropertyBorderRadius:
        return get4Values(borderRadiusShorthand());
    default:
        return String();
    }
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
    RefPtr<CSSValue> horizontalValue = getPropertyCSSValueInternal(shorthand.properties()[0]);
    if (horizontalValue && horizontalValue->isVariableDependentValue())
        return horizontalValue->cssText();
    
    RefPtr<CSSValue> verticalValue = getPropertyCSSValueInternal(shorthand.properties()[1]);

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
    case CSSPropertyFontVariantCaps:
    case CSSPropertyFontWeight:
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

    String commonValue = fontSizeProperty.value()->cssText();
    StringBuilder result;
    appendFontLonghandValueIfExplicit(CSSPropertyFontStyle, result, commonValue);
    appendFontLonghandValueIfExplicit(CSSPropertyFontVariantCaps, result, commonValue);
    appendFontLonghandValueIfExplicit(CSSPropertyFontWeight, result, commonValue);
    if (!result.isEmpty())
        result.append(' ');
    result.append(fontSizeProperty.value()->cssText());
    appendFontLonghandValueIfExplicit(CSSPropertyLineHeight, result, commonValue);
    if (!result.isEmpty())
        result.append(' ');
    result.append(fontFamilyProperty.value()->cssText());
    if (isInitialOrInherit(commonValue))
        return commonValue;
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

    if (top.isInherited() && right.isInherited() && bottom.isInherited() && left.isInherited())
        return getValueName(CSSValueInherit);

    if (top.value()->isInitialValue() || right.value()->isInitialValue() || bottom.value()->isInitialValue() || left.value()->isInitialValue()) {
        if (top.value()->isInitialValue() && right.value()->isInitialValue() && bottom.value()->isInitialValue() && left.value()->isInitialValue() && !top.isImplicit()) {
            // All components are "initial" and "top" is not implicit.
            return getValueName(CSSValueInitial);
        }
        return String();
    }
    if (top.isImportant() != right.isImportant() || right.isImportant() != bottom.isImportant() || bottom.isImportant() != left.isImportant())
        return String();

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
    // Begin by collecting the properties into an array.
    Vector< RefPtr<CSSValue>> values(size);
    size_t numLayers = 0;

    for (unsigned i = 0; i < size; ++i) {
        values[i] = getPropertyCSSValueInternal(shorthand.properties()[i]);
        if (values[i]) {
            if (values[i]->isVariableDependentValue())
                return values[i]->cssText();
            if (values[i]->isBaseValueList())
                numLayers = std::max(downcast<CSSValueList>(*values[i]).length(), numLayers);
            else
                numLayers = std::max<size_t>(1U, numLayers);
        }
    }

    String commonValue;
    bool commonValueInitialized = false;

    // Now stitch the properties together. Implicit initial values are flagged as such and
    // can safely be omitted.
    for (size_t i = 0; i < numLayers; i++) {
        StringBuilder layerResult;
        bool useRepeatXShorthand = false;
        bool useRepeatYShorthand = false;
        bool useSingleWordShorthand = false;
        bool foundPositionYCSSProperty = false;
        for (unsigned j = 0; j < size; j++) {
            RefPtr<CSSValue> value;
            if (values[j]) {
                if (values[j]->isBaseValueList())
                    value = downcast<CSSValueList>(*values[j]).item(i);
                else {
                    value = values[j];

                    // Color only belongs in the last layer.
                    if (shorthand.properties()[j] == CSSPropertyBackgroundColor) {
                        if (i != numLayers - 1)
                            value = nullptr;
                    } else if (i) // Other singletons only belong in the first layer.
                        value = nullptr;
                }
            }

            // We need to report background-repeat as it was written in the CSS. If the property is implicit,
            // then it was written with only one value. Here we figure out which value that was so we can
            // report back correctly.
            if ((shorthand.properties()[j] == CSSPropertyBackgroundRepeatX && isPropertyImplicit(shorthand.properties()[j]))
                || (shorthand.properties()[j] == CSSPropertyWebkitMaskRepeatX && isPropertyImplicit(shorthand.properties()[j]))) {

                // BUG 49055: make sure the value was not reset in the layer check just above.
                if ((j < size - 1 && shorthand.properties()[j + 1] == CSSPropertyBackgroundRepeatY && value)
                    || (j < size - 1 && shorthand.properties()[j + 1] == CSSPropertyWebkitMaskRepeatY && value)) {
                    RefPtr<CSSValue> yValue;
                    RefPtr<CSSValue> nextValue = values[j + 1];
                    if (nextValue) {
                        if (is<CSSValueList>(*nextValue))
                            yValue = downcast<CSSValueList>(*nextValue).itemWithoutBoundsCheck(i);
                        else
                            yValue = nextValue;

                        if (!is<CSSPrimitiveValue>(*value) || !is<CSSPrimitiveValue>(*yValue))
                            continue;

                        CSSValueID xId = downcast<CSSPrimitiveValue>(*value).getValueID();
                        CSSValueID yId = downcast<CSSPrimitiveValue>(*yValue).getValueID();
                        if (xId != yId) {
                            if (xId == CSSValueRepeat && yId == CSSValueNoRepeat) {
                                useRepeatXShorthand = true;
                                ++j;
                            } else if (xId == CSSValueNoRepeat && yId == CSSValueRepeat) {
                                useRepeatYShorthand = true;
                                continue;
                            }
                        } else {
                            useSingleWordShorthand = true;
                            ++j;
                        }
                    }
                }
            }

            String valueText;
            if (value && !value->isImplicitInitialValue()) {
                if (!layerResult.isEmpty())
                    layerResult.append(' ');
                if (foundPositionYCSSProperty
                    && (shorthand.properties()[j] == CSSPropertyBackgroundSize || shorthand.properties()[j] == CSSPropertyWebkitMaskSize))
                    layerResult.appendLiteral("/ ");
                if (!foundPositionYCSSProperty
                    && (shorthand.properties()[j] == CSSPropertyBackgroundSize || shorthand.properties()[j] == CSSPropertyWebkitMaskSize))
                    continue;

                if (useRepeatXShorthand) {
                    useRepeatXShorthand = false;
                    layerResult.append(getValueName(CSSValueRepeatX));
                } else if (useRepeatYShorthand) {
                    useRepeatYShorthand = false;
                    layerResult.append(getValueName(CSSValueRepeatY));
                } else {
                    if (useSingleWordShorthand)
                        useSingleWordShorthand = false;
                    valueText = value->cssText();
                    layerResult.append(valueText);
                }

                if (shorthand.properties()[j] == CSSPropertyBackgroundPositionY
                    || shorthand.properties()[j] == CSSPropertyWebkitMaskPositionY) {
                    foundPositionYCSSProperty = true;

                    // background-position is a special case: if only the first offset is specified,
                    // the second one defaults to "center", not the same value.
                    if (commonValueInitialized && commonValue != "initial" && commonValue != "inherit")
                        commonValue = String();
                }
            }

            if (!commonValueInitialized) {
                commonValue = valueText;
                commonValueInitialized = true;
            } else if (!commonValue.isNull() && commonValue != valueText)
                commonValue = String();
        }

        if (!layerResult.isEmpty()) {
            if (!result.isEmpty())
                result.appendLiteral(", ");
            result.append(layerResult);
        }
    }

    if (isInitialOrInherit(commonValue))
        return commonValue;

    if (result.isEmpty())
        return String();
    return result.toString();
}

String StyleProperties::getShorthandValue(const StylePropertyShorthand& shorthand) const
{
    String commonValue;
    StringBuilder result;
    for (unsigned i = 0; i < shorthand.length(); ++i) {
        if (!isPropertyImplicit(shorthand.properties()[i])) {
            RefPtr<CSSValue> value = getPropertyCSSValueInternal(shorthand.properties()[i]);
            if (!value)
                return String();
            if (value->isVariableDependentValue())
                return value->cssText();
            String valueText = value->cssText();
            if (!i)
                commonValue = valueText;
            else if (!commonValue.isNull() && commonValue != valueText)
                commonValue = String();
            if (value->isInitialValue())
                continue;
            if (!result.isEmpty())
                result.append(' ');
            result.append(valueText);
        } else
            commonValue = String();
    }
    if (isInitialOrInherit(commonValue))
        return commonValue;
    if (result.isEmpty())
        return String();
    return result.toString();
}

// only returns a non-null value if all properties have the same, non-null value
String StyleProperties::getCommonValue(const StylePropertyShorthand& shorthand) const
{
    String res;
    bool lastPropertyWasImportant = false;
    for (unsigned i = 0; i < shorthand.length(); ++i) {
        RefPtr<CSSValue> value = getPropertyCSSValueInternal(shorthand.properties()[i]);
        // FIXME: CSSInitialValue::cssText should generate the right value.
        if (!value)
            return String();
        if (value->isVariableDependentValue())
            return value->cssText();
        String text = value->cssText();
        if (text.isNull())
            return String();
        if (res.isNull())
            res = text;
        else if (res != text)
            return String();

        bool currentPropertyIsImportant = propertyIsImportant(shorthand.properties()[i]);
        if (i && lastPropertyWasImportant != currentPropertyIsImportant)
            return String();
        lastPropertyWasImportant = currentPropertyIsImportant;
    }
    return res;
}

String StyleProperties::borderPropertyValue(CommonValueMode valueMode) const
{
    const StylePropertyShorthand properties[3] = { borderWidthShorthand(), borderStyleShorthand(), borderColorShorthand() };
    String commonValue;
    StringBuilder result;
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(properties); ++i) {
        String value = getCommonValue(properties[i]);
        if (value.isNull()) {
            if (valueMode == ReturnNullOnUncommonValues)
                return String();
            ASSERT(valueMode == OmitUncommonValues);
            continue;
        }
        if (!i)
            commonValue = value;
        else if (!commonValue.isNull() && commonValue != value)
            commonValue = String();
        if (value == "initial")
            continue;
        if (!result.isEmpty())
            result.append(' ');
        result.append(value);
    }
    if (isInitialOrInherit(commonValue))
        return commonValue;
    return result.isEmpty() ? String() : result.toString();
}

PassRefPtr<CSSValue> StyleProperties::getPropertyCSSValue(CSSPropertyID propertyID) const
{
    PassRefPtr<CSSValue> value = getPropertyCSSValueInternal(propertyID);
    if (value && value->isVariableDependentValue()) {
        auto& dependentValue = downcast<CSSVariableDependentValue>(*value);
        if (dependentValue.propertyID() != propertyID)
            return CSSCustomPropertyValue::createInvalid(); // Have to return special "pending-substitution" value.
    }
    return value;
}

PassRefPtr<CSSValue> StyleProperties::getPropertyCSSValueInternal(CSSPropertyID propertyID) const
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

    bool ret = removePropertiesInSet(shorthand.properties(), shorthand.length());

    CSSPropertyID prefixingVariant = prefixingVariantForPropertyId(propertyID);
    if (prefixingVariant == propertyID)
        return ret;

    StylePropertyShorthand shorthandPrefixingVariant = shorthandForProperty(prefixingVariant);
    return removePropertiesInSet(shorthandPrefixingVariant.properties(), shorthandPrefixingVariant.length());
}

bool MutableStyleProperties::removeProperty(CSSPropertyID propertyID, String* returnText)
{
    if (removeShorthandProperty(propertyID)) {
        // FIXME: Return an equivalent shorthand when possible.
        if (returnText)
            *returnText = "";
        return true;
    }

    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1) {
        if (returnText)
            *returnText = "";
        return false;
    }

    if (returnText)
        *returnText = propertyAt(foundPropertyIndex).value()->cssText();

    // A more efficient removal strategy would involve marking entries as empty
    // and sweeping them when the vector grows too big.
    m_propertyVector.remove(foundPropertyIndex);

    removePrefixedOrUnprefixedProperty(propertyID);

    return true;
}

bool MutableStyleProperties::removeCustomProperty(const String& propertyName, String* returnText)
{
    int foundPropertyIndex = findCustomPropertyIndex(propertyName);
    if (foundPropertyIndex == -1) {
        if (returnText)
            *returnText = "";
        return false;
    }

    if (returnText)
        *returnText = propertyAt(foundPropertyIndex).value()->cssText();

    // A more efficient removal strategy would involve marking entries as empty
    // and sweeping them when the vector grows too big.
    m_propertyVector.remove(foundPropertyIndex);

    return true;
}

void MutableStyleProperties::removePrefixedOrUnprefixedProperty(CSSPropertyID propertyID)
{
    int foundPropertyIndex = findPropertyIndex(prefixingVariantForPropertyId(propertyID));
    if (foundPropertyIndex == -1)
        return;
    m_propertyVector.remove(foundPropertyIndex);
}

bool StyleProperties::propertyIsImportant(CSSPropertyID propertyID) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex != -1)
        return propertyAt(foundPropertyIndex).isImportant();

    StylePropertyShorthand shorthand = shorthandForProperty(propertyID);
    if (!shorthand.length())
        return false;

    for (unsigned i = 0; i < shorthand.length(); ++i) {
        if (!propertyIsImportant(shorthand.properties()[i]))
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

bool MutableStyleProperties::setProperty(CSSPropertyID propertyID, const String& value, bool important, StyleSheetContents* contextStyleSheet)
{
    // Setting the value to an empty string just removes the property in both IE and Gecko.
    // Setting it to null seems to produce less consistent results, but we treat it just the same.
    if (value.isEmpty())
        return removeProperty(propertyID);

    // When replacing an existing property value, this moves the property to the end of the list.
    // Firefox preserves the position, and MSIE moves the property to the beginning.
    return CSSParser::parseValue(this, propertyID, value, important, cssParserMode(), contextStyleSheet) == CSSParser::ParseResult::Changed;
}

bool MutableStyleProperties::setCustomProperty(const String& propertyName, const String& value, bool important, StyleSheetContents* contextStyleSheet)
{
    // Setting the value to an empty string just removes the property in both IE and Gecko.
    // Setting it to null seems to produce less consistent results, but we treat it just the same.
    if (value.isEmpty())
        return removeCustomProperty(propertyName);

    // When replacing an existing property value, this moves the property to the end of the list.
    // Firefox preserves the position, and MSIE moves the property to the beginning.
    return CSSParser::parseCustomPropertyValue(this, propertyName, value, important, cssParserMode(), contextStyleSheet) == CSSParser::ParseResult::Changed;
}

void MutableStyleProperties::setProperty(CSSPropertyID propertyID, PassRefPtr<CSSValue> prpValue, bool important)
{
    StylePropertyShorthand shorthand = shorthandForProperty(propertyID);
    if (!shorthand.length()) {
        setProperty(CSSProperty(propertyID, prpValue, important));
        return;
    }

    removePropertiesInSet(shorthand.properties(), shorthand.length());

    RefPtr<CSSValue> value = prpValue;
    for (unsigned i = 0; i < shorthand.length(); ++i)
        m_propertyVector.append(CSSProperty(shorthand.properties()[i], value, important));
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
            if (*toReplace == property)
                return false;

            *toReplace = property;
            setPrefixingVariantProperty(property);
            return true;
        }
    }

    return appendPrefixingVariantProperty(property);
}

static unsigned getIndexInShorthandVectorForPrefixingVariant(const CSSProperty& property, CSSPropertyID prefixingVariant)
{
    if (!property.isSetFromShorthand())
        return 0;

    CSSPropertyID prefixedShorthand = prefixingVariantForPropertyId(property.shorthandID());
    return indexOfShorthandForLonghand(prefixedShorthand, matchingShorthandsForLonghand(prefixingVariant));
}

bool MutableStyleProperties::appendPrefixingVariantProperty(const CSSProperty& property)
{
    m_propertyVector.append(property);
    CSSPropertyID prefixingVariant = prefixingVariantForPropertyId(property.id());
    if (prefixingVariant == property.id())
        return true;

    m_propertyVector.append(CSSProperty(prefixingVariant, property.value(), property.isImportant(), property.isSetFromShorthand(), getIndexInShorthandVectorForPrefixingVariant(property, prefixingVariant), property.metadata().m_implicit));
    return true;
}

void MutableStyleProperties::setPrefixingVariantProperty(const CSSProperty& property)
{
    CSSPropertyID prefixingVariant = prefixingVariantForPropertyId(property.id());
    CSSProperty* toReplace = findCSSPropertyWithID(prefixingVariant);
    if (toReplace && prefixingVariant != property.id())
        *toReplace = CSSProperty(prefixingVariant, property.value(), property.isImportant(), property.isSetFromShorthand(), getIndexInShorthandVectorForPrefixingVariant(property, prefixingVariant), property.metadata().m_implicit);
}

bool MutableStyleProperties::setProperty(CSSPropertyID propertyID, CSSValueID identifier, bool important)
{
    return setProperty(CSSProperty(propertyID, CSSValuePool::singleton().createIdentifierValue(identifier), important));
}

bool MutableStyleProperties::setProperty(CSSPropertyID propertyID, CSSPropertyID identifier, bool important)
{
    return setProperty(CSSProperty(propertyID, CSSValuePool::singleton().createIdentifierValue(identifier), important));
}

bool MutableStyleProperties::parseDeclaration(const String& styleDeclaration, StyleSheetContents* contextStyleSheet)
{
    auto oldProperties = WTFMove(m_propertyVector);
    m_propertyVector.clear();

    CSSParserContext context(cssParserMode());
    if (contextStyleSheet) {
        context = contextStyleSheet->parserContext();
        context.mode = cssParserMode();
    }
    CSSParser parser(context);
    parser.parseDeclaration(this, styleDeclaration, 0, contextStyleSheet);

    // We could do better. Just changing property order does not require style invalidation.
    return oldProperties != m_propertyVector;
}

bool MutableStyleProperties::addParsedProperties(const CSSParser::ParsedPropertyVector& properties)
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
    
    // Only add properties that have no !important counterpart present
    if (!propertyIsImportant(property.id()) || property.isImportant())
        return setProperty(property);
    
    return false;
}

String StyleProperties::asText() const
{
    StringBuilder result;

    int positionXPropertyIndex = -1;
    int positionYPropertyIndex = -1;
    int repeatXPropertyIndex = -1;
    int repeatYPropertyIndex = -1;

    std::bitset<numCSSProperties> shorthandPropertyUsed;
    std::bitset<numCSSProperties> shorthandPropertyAppeared;

    unsigned size = propertyCount();
    unsigned numDecls = 0;
    for (unsigned n = 0; n < size; ++n) {
        PropertyReference property = propertyAt(n);
        CSSPropertyID propertyID = property.id();
        CSSPropertyID shorthandPropertyID = CSSPropertyInvalid;
        CSSPropertyID borderFallbackShorthandProperty = CSSPropertyInvalid;
        String value;
        
        if (property.value() && property.value()->isVariableDependentValue()) {
            auto& dependentValue = downcast<CSSVariableDependentValue>(*property.value());
            if (dependentValue.propertyID() != propertyID)
                shorthandPropertyID = dependentValue.propertyID();
        } else {
            switch (propertyID) {
            case CSSPropertyAnimationName:
            case CSSPropertyAnimationDuration:
            case CSSPropertyAnimationTimingFunction:
            case CSSPropertyAnimationDelay:
            case CSSPropertyAnimationIterationCount:
            case CSSPropertyAnimationDirection:
            case CSSPropertyAnimationFillMode:
                shorthandPropertyID = CSSPropertyAnimation;
                break;
            case CSSPropertyBackgroundPositionX:
                positionXPropertyIndex = n;
                continue;
            case CSSPropertyBackgroundPositionY:
                positionYPropertyIndex = n;
                continue;
            case CSSPropertyBackgroundRepeatX:
                repeatXPropertyIndex = n;
                continue;
            case CSSPropertyBackgroundRepeatY:
                repeatYPropertyIndex = n;
                continue;
            case CSSPropertyBorderTopWidth:
            case CSSPropertyBorderRightWidth:
            case CSSPropertyBorderBottomWidth:
            case CSSPropertyBorderLeftWidth:
                if (!borderFallbackShorthandProperty)
                    borderFallbackShorthandProperty = CSSPropertyBorderWidth;
                FALLTHROUGH;
            case CSSPropertyBorderTopStyle:
            case CSSPropertyBorderRightStyle:
            case CSSPropertyBorderBottomStyle:
            case CSSPropertyBorderLeftStyle:
                if (!borderFallbackShorthandProperty)
                    borderFallbackShorthandProperty = CSSPropertyBorderStyle;
                FALLTHROUGH;
            case CSSPropertyBorderTopColor:
            case CSSPropertyBorderRightColor:
            case CSSPropertyBorderBottomColor:
            case CSSPropertyBorderLeftColor:
                if (!borderFallbackShorthandProperty)
                    borderFallbackShorthandProperty = CSSPropertyBorderColor;

                // FIXME: Deal with cases where only some of border-(top|right|bottom|left) are specified.
                ASSERT(CSSPropertyBorder - firstCSSProperty < shorthandPropertyAppeared.size());
                if (!shorthandPropertyAppeared[CSSPropertyBorder - firstCSSProperty]) {
                    value = borderPropertyValue(ReturnNullOnUncommonValues);
                    if (value.isNull())
                        shorthandPropertyAppeared.set(CSSPropertyBorder - firstCSSProperty);
                    else
                        shorthandPropertyID = CSSPropertyBorder;
                } else if (shorthandPropertyUsed[CSSPropertyBorder - firstCSSProperty])
                    shorthandPropertyID = CSSPropertyBorder;
                if (!shorthandPropertyID)
                    shorthandPropertyID = borderFallbackShorthandProperty;
                break;
            case CSSPropertyWebkitBorderHorizontalSpacing:
            case CSSPropertyWebkitBorderVerticalSpacing:
                shorthandPropertyID = CSSPropertyBorderSpacing;
                break;
            case CSSPropertyFontFamily:
            case CSSPropertyLineHeight:
            case CSSPropertyFontSize:
            case CSSPropertyFontStyle:
            case CSSPropertyFontVariantCaps:
            case CSSPropertyFontWeight:
                // Don't use CSSPropertyFont because old UAs can't recognize them but are important for editing.
                break;
            case CSSPropertyListStyleType:
            case CSSPropertyListStylePosition:
            case CSSPropertyListStyleImage:
                shorthandPropertyID = CSSPropertyListStyle;
                break;
            case CSSPropertyMarginTop:
            case CSSPropertyMarginRight:
            case CSSPropertyMarginBottom:
            case CSSPropertyMarginLeft:
                shorthandPropertyID = CSSPropertyMargin;
                break;
            case CSSPropertyOutlineWidth:
            case CSSPropertyOutlineStyle:
            case CSSPropertyOutlineColor:
                shorthandPropertyID = CSSPropertyOutline;
                break;
            case CSSPropertyOverflowX:
            case CSSPropertyOverflowY:
                shorthandPropertyID = CSSPropertyOverflow;
                break;
            case CSSPropertyPaddingTop:
            case CSSPropertyPaddingRight:
            case CSSPropertyPaddingBottom:
            case CSSPropertyPaddingLeft:
                shorthandPropertyID = CSSPropertyPadding;
                break;
            case CSSPropertyTransitionProperty:
            case CSSPropertyTransitionDuration:
            case CSSPropertyTransitionTimingFunction:
            case CSSPropertyTransitionDelay:
                shorthandPropertyID = CSSPropertyTransition;
                break;
            case CSSPropertyWebkitAnimationName:
            case CSSPropertyWebkitAnimationDuration:
            case CSSPropertyWebkitAnimationTimingFunction:
            case CSSPropertyWebkitAnimationDelay:
            case CSSPropertyWebkitAnimationIterationCount:
            case CSSPropertyWebkitAnimationDirection:
            case CSSPropertyWebkitAnimationFillMode:
                shorthandPropertyID = CSSPropertyWebkitAnimation;
                break;
            case CSSPropertyFlexDirection:
            case CSSPropertyFlexWrap:
                shorthandPropertyID = CSSPropertyFlexFlow;
                break;
            case CSSPropertyFlexBasis:
            case CSSPropertyFlexGrow:
            case CSSPropertyFlexShrink:
                shorthandPropertyID = CSSPropertyFlex;
                break;
            case CSSPropertyWebkitMaskPositionX:
            case CSSPropertyWebkitMaskPositionY:
            case CSSPropertyWebkitMaskRepeatX:
            case CSSPropertyWebkitMaskRepeatY:
            case CSSPropertyWebkitMaskImage:
            case CSSPropertyWebkitMaskRepeat:
            case CSSPropertyWebkitMaskPosition:
            case CSSPropertyWebkitMaskClip:
            case CSSPropertyWebkitMaskOrigin:
                shorthandPropertyID = CSSPropertyWebkitMask;
                break;
            case CSSPropertyPerspectiveOriginX:
            case CSSPropertyPerspectiveOriginY:
                shorthandPropertyID = CSSPropertyPerspectiveOrigin;
                break;
            case CSSPropertyTransformOriginX:
            case CSSPropertyTransformOriginY:
            case CSSPropertyTransformOriginZ:
                shorthandPropertyID = CSSPropertyTransformOrigin;
                break;
            case CSSPropertyWebkitTransitionProperty:
            case CSSPropertyWebkitTransitionDuration:
            case CSSPropertyWebkitTransitionTimingFunction:
            case CSSPropertyWebkitTransitionDelay:
                shorthandPropertyID = CSSPropertyWebkitTransition;
                break;
            default:
                break;
            }
        }

        unsigned shortPropertyIndex = shorthandPropertyID - firstCSSProperty;
        if (shorthandPropertyID) {
            ASSERT(shortPropertyIndex < shorthandPropertyUsed.size());
            if (shorthandPropertyUsed[shortPropertyIndex])
                continue;
            if (!shorthandPropertyAppeared[shortPropertyIndex] && value.isNull())
                value = getPropertyValue(shorthandPropertyID);
            shorthandPropertyAppeared.set(shortPropertyIndex);
        }

        if (!value.isNull()) {
            propertyID = shorthandPropertyID;
            shorthandPropertyUsed.set(shortPropertyIndex);
        } else
            value = property.value()->cssText();

        if (propertyID != CSSPropertyCustom && value == "initial" && !CSSProperty::isInheritedProperty(propertyID))
            continue;

        if (numDecls++)
            result.append(' ');

        if (propertyID == CSSPropertyCustom)
            result.append(downcast<CSSCustomPropertyValue>(*property.value()).name());
        else
            result.append(getPropertyName(propertyID));

        result.appendLiteral(": ");
        result.append(value);
        if (property.isImportant())
            result.appendLiteral(" !important");
        result.append(';');
    }

    // FIXME: This is a not-so-nice way to turn x/y positions into single background-position in output.
    // It is required because background-position-x/y are non-standard properties and WebKit generated output
    // would not work in Firefox (<rdar://problem/5143183>)
    // It would be a better solution if background-position was CSS_PAIR.
    if (positionXPropertyIndex != -1 && positionYPropertyIndex != -1 && propertyAt(positionXPropertyIndex).isImportant() == propertyAt(positionYPropertyIndex).isImportant()) {
        PropertyReference positionXProperty = propertyAt(positionXPropertyIndex);
        PropertyReference positionYProperty = propertyAt(positionYPropertyIndex);

        if (numDecls++)
            result.append(' ');
        result.appendLiteral("background-position: ");
        if (positionXProperty.value()->isValueList() || positionYProperty.value()->isValueList())
            result.append(getLayeredShorthandValue(backgroundPositionShorthand()));
        else {
            result.append(positionXProperty.value()->cssText());
            result.append(' ');
            result.append(positionYProperty.value()->cssText());
        }
        if (positionXProperty.isImportant())
            result.appendLiteral(" !important");
        result.append(';');
    } else {
        if (positionXPropertyIndex != -1) {
            if (numDecls++)
                result.append(' ');
            result.append(propertyAt(positionXPropertyIndex).cssText());
        }
        if (positionYPropertyIndex != -1) {
            if (numDecls++)
                result.append(' ');
            result.append(propertyAt(positionYPropertyIndex).cssText());
        }
    }

    // FIXME: We need to do the same for background-repeat.
    if (repeatXPropertyIndex != -1 && repeatYPropertyIndex != -1 && propertyAt(repeatXPropertyIndex).isImportant() == propertyAt(repeatYPropertyIndex).isImportant()) {
        PropertyReference repeatXProperty = propertyAt(repeatXPropertyIndex);
        PropertyReference repeatYProperty = propertyAt(repeatYPropertyIndex);

        if (numDecls++)
            result.append(' ');
        result.appendLiteral("background-repeat: ");
        if (repeatXProperty.value()->isValueList() || repeatYProperty.value()->isValueList())
            result.append(getLayeredShorthandValue(backgroundRepeatShorthand()));
        else {
            result.append(repeatXProperty.value()->cssText());
            result.append(' ');
            result.append(repeatYProperty.value()->cssText());
        }
        if (repeatXProperty.isImportant())
            result.appendLiteral(" !important");
        result.append(';');
    } else {
        if (repeatXPropertyIndex != -1) {
            if (numDecls++)
                result.append(' ');
            result.append(propertyAt(repeatXPropertyIndex).cssText());
        }
        if (repeatYPropertyIndex != -1) {
            if (numDecls++)
                result.append(' ');
            result.append(propertyAt(repeatYPropertyIndex).cssText());
        }
    }

    ASSERT(!numDecls ^ !result.isEmpty());
    return result.toString();
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

void StyleProperties::addSubresourceStyleURLs(ListHashSet<URL>& urls, StyleSheetContents* contextStyleSheet) const
{
    unsigned size = propertyCount();
    for (unsigned i = 0; i < size; ++i)
        propertyAt(i).value()->addSubresourceStyleURLs(urls, contextStyleSheet);
}

bool StyleProperties::traverseSubresources(const std::function<bool (const CachedResource&)>& handler) const
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
    CSSPropertyWebkitAspectRatio,
    CSSPropertyColumnCount,
    CSSPropertyColumnGap,
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
#if ENABLE(CSS_REGIONS)
    CSSPropertyWebkitRegionBreakAfter,
    CSSPropertyWebkitRegionBreakBefore,
    CSSPropertyWebkitRegionBreakInside,
#endif
    CSSPropertyTextAlign,
#if ENABLE(CSS3_TEXT)
    CSSPropertyWebkitTextAlignLast,
    CSSPropertyWebkitTextJustify,
#endif // CSS3_TEXT
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
        // Not quite sure if the isImportant test is needed but it matches the existing behavior.
        return !property.isImportant() && toRemove.contains(property.id());
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
    // Convert the propertyID into an uint16_t to compare it with the metadata's m_propertyID to avoid
    // the compiler converting it to an int multiple times in the loop.
    for (int n = m_arraySize - 1 ; n >= 0; --n) {
        if (metadataArray()[n].m_propertyID == CSSPropertyCustom) {
            // We found a custom property. See if the name matches.
            if (!valueArray()[n])
                continue;
            if (downcast<CSSCustomPropertyValue>(*valueArray()[n]).name() == propertyName)
                return n;
        }
    }

    return -1;
}

int MutableStyleProperties::findCustomPropertyIndex(const String& propertyName) const
{
    // Convert the propertyID into an uint16_t to compare it with the metadata's m_propertyID to avoid
    // the compiler converting it to an int multiple times in the loop.
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
    Vector<CSSProperty, 256> list;
    list.reserveInitialCapacity(length);
    for (unsigned i = 0; i < length; ++i) {
        RefPtr<CSSValue> value = getPropertyCSSValueInternal(set[i]);
        if (value)
            list.append(CSSProperty(set[i], value.release(), false));
    }
    return MutableStyleProperties::create(list.data(), list.size());
}

PropertySetCSSStyleDeclaration* MutableStyleProperties::cssStyleDeclaration()
{
    return m_cssomWrapper.get();
}

CSSStyleDeclaration* MutableStyleProperties::ensureCSSStyleDeclaration()
{
    if (m_cssomWrapper) {
        ASSERT(!static_cast<CSSStyleDeclaration*>(m_cssomWrapper.get())->parentRule());
        ASSERT(!m_cssomWrapper->parentElement());
        return m_cssomWrapper.get();
    }
    m_cssomWrapper = std::make_unique<PropertySetCSSStyleDeclaration>(this);
    return m_cssomWrapper.get();
}

CSSStyleDeclaration* MutableStyleProperties::ensureInlineCSSStyleDeclaration(StyledElement* parentElement)
{
    if (m_cssomWrapper) {
        ASSERT(m_cssomWrapper->parentElement() == parentElement);
        return m_cssomWrapper.get();
    }
    m_cssomWrapper = std::make_unique<InlineCSSStyleDeclaration>(this, parentElement);
    return m_cssomWrapper.get();
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
COMPILE_ASSERT(sizeof(StyleProperties) == sizeof(SameSizeAsStyleProperties), style_property_set_should_stay_small);

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

Ref<MutableStyleProperties> MutableStyleProperties::create(const CSSProperty* properties, unsigned count)
{
    return adoptRef(*new MutableStyleProperties(properties, count));
}

String StyleProperties::PropertyReference::cssName() const
{
    if (id() == CSSPropertyCustom)
        return downcast<CSSCustomPropertyValue>(*value()).name();
    return getPropertyNameString(id());
}

String StyleProperties::PropertyReference::cssText() const
{
    StringBuilder result;
    result.append(cssName());
    result.appendLiteral(": ");
    result.append(m_value->cssText());
    if (isImportant())
        result.appendLiteral(" !important");
    result.append(';');
    return result.toString();
}


} // namespace WebCore
