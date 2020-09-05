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
#include "CSSDeferredParser.h"
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
    return value == "initial" || value == "inherit" || value == "unset" || value == "revert";
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
    ASSERT(other.type() != DeferredPropertiesType);
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
    if (auto value = getPropertyCSSValue(propertyID)) {
        switch (propertyID) {
        case CSSPropertyFillOpacity:
        case CSSPropertyFloodOpacity:
        case CSSPropertyOpacity:
        case CSSPropertyStopOpacity:
        case CSSPropertyStrokeOpacity:
            // Opacity percentage values serialize as a fraction in the range 0-1, no "%".
            if (is<CSSPrimitiveValue>(*value) && downcast<CSSPrimitiveValue>(*value).isPercentage())
                return makeString(downcast<CSSPrimitiveValue>(*value).doubleValue() / 100);
            FALLTHROUGH;
        default:
            return value->cssText();
        }
    }

    {
        auto shorthand = shorthandForProperty(propertyID);
        if (shorthand.length() && is<CSSPendingSubstitutionValue>(getPropertyCSSValue(shorthand.properties()[0])))
            return String();
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
    case CSSPropertyBackgroundRepeat:
        return getLayeredShorthandValue(backgroundRepeatShorthand());
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
    case CSSPropertyGridArea:
        return getGridShorthandValue(gridAreaShorthand());
    case CSSPropertyGridTemplate:
        return getGridShorthandValue(gridTemplateShorthand());
    case CSSPropertyGrid:
        return getGridShorthandValue(gridShorthand());
    case CSSPropertyGridColumn:
        return getGridShorthandValue(gridColumnShorthand());
    case CSSPropertyGridRow:
        return getGridShorthandValue(gridRowShorthand());
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
    case CSSPropertyWebkitMarginCollapse:
        return getShorthandValue(webkitMarginCollapseShorthand());
    case CSSPropertyOverflow:
        return get2Values(overflowShorthand());
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
    case CSSPropertyMarker:
        if (auto value = getPropertyCSSValue(CSSPropertyMarkerStart))
            return value->cssText();
        return String();
    case CSSPropertyBorderRadius:
        return get4Values(borderRadiusShorthand());
    case CSSPropertyGap:
        return get2Values(gapShorthand());
#if ENABLE(CSS_SCROLL_SNAP)
    case CSSPropertyScrollSnapMargin:
        return get4Values(scrollSnapMarginShorthand());
    case CSSPropertyScrollPadding:
        return get4Values(scrollPaddingShorthand());
#endif
    default:
        return String();
    }
}

Optional<Color> StyleProperties::propertyAsColor(CSSPropertyID property) const
{
    auto colorValue = getPropertyCSSValue(property);
    if (!is<CSSPrimitiveValue>(colorValue))
        return WTF::nullopt;

    auto& primitiveColor = downcast<CSSPrimitiveValue>(*colorValue);
    return primitiveColor.isRGBColor() ? primitiveColor.color() : CSSParser::parseColor(colorValue->cssText());
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
    case CSSPropertyFontVariantCaps:
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

            // We need to report background-repeat as it was written in the CSS.
            // If the property is implicit, then it was written with only one value. Here we figure out which value that was so we can report back correctly.
            if (value && j < size - 1 && (property == CSSPropertyBackgroundRepeatX || property == CSSPropertyWebkitMaskRepeatX) && isPropertyImplicit(property)) {
                // Make sure the value was not reset in the layer check just above.
                auto nextProperty = shorthand.properties()[j + 1];
                if (nextProperty == CSSPropertyBackgroundRepeatY || nextProperty == CSSPropertyWebkitMaskRepeatY) {
                    if (auto yValue = values[j + 1]) {
                        if (is<CSSValueList>(*yValue))
                            yValue = downcast<CSSValueList>(*yValue).itemWithoutBoundsCheck(i);
                        if (!is<CSSPrimitiveValue>(*value) || !is<CSSPrimitiveValue>(*yValue))
                            continue;

                        auto xId = downcast<CSSPrimitiveValue>(*value).valueID();
                        auto yId = downcast<CSSPrimitiveValue>(*yValue).valueID();
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

                if (property == CSSPropertyBackgroundSize || property == CSSPropertyWebkitMaskSize) {
                    if (!foundPositionYCSSProperty)
                        continue;
                    layerResult.appendLiteral("/ ");
                }

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

                if (property == CSSPropertyBackgroundPositionY || property == CSSPropertyWebkitMaskPositionY)
                    foundPositionYCSSProperty = true;
            }

            if (commonValue.isNull())
                commonValue = valueText;
            else if (commonValue != valueText)
                commonValue = emptyString(); // Could use value here other than a CSS-wide value keyword or the null string.
        }

        if (!layerResult.isEmpty())
            result.append(result.isEmpty() ? "" : ", ", layerResult.toString());
    }

    if (isCSSWideValueKeyword(commonValue))
        return commonValue;

    return result.isEmpty() ? String() : result.toString();
}

String StyleProperties::getGridShorthandValue(const StylePropertyShorthand& shorthand) const
{
    return getShorthandValue(shorthand, " / ");
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
        if (value == "initial")
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
    // FIXME: Remove this isGlobalKeyword check after we do this consistently for all shorthands in getPropertyValue.
    if (value->isGlobalKeyword())
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
    if (!isEnabledCSSProperty(propertyID))
        return false;

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

    String syntax = "*";
    auto* registered = document ? document->getCSSRegisteredCustomPropertySet().get(propertyName) : nullptr;

    if (registered)
        syntax = registered->syntax;

    CSSTokenizer tokenizer(value);
    if (!CSSPropertyParser::canParseTypedCustomPropertyValue(syntax, tokenizer.tokenRange(), parserContext))
        return false;

    // When replacing an existing property value, this moves the property to the end of the list.
    // Firefox preserves the position, and MSIE moves the property to the beginning.
    return CSSParser::parseCustomPropertyValue(*this, propertyName, value, important, parserContext) == CSSParser::ParseResult::Changed;
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
            return true;
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
        CSSPropertyID borderBlockFallbackShorthandProperty = CSSPropertyInvalid;
        CSSPropertyID borderInlineFallbackShorthandProperty = CSSPropertyInvalid;
        String value;
        auto serializeBorderShorthand = [&] (const CSSPropertyID borderProperty, const CSSPropertyID fallbackProperty) {
            // FIXME: Deal with cases where only some of border sides are specified.
            ASSERT(borderProperty - firstCSSProperty < static_cast<CSSPropertyID>(shorthandPropertyAppeared.size()));
            if (!shorthandPropertyAppeared[borderProperty - firstCSSProperty] && isEnabledCSSProperty(borderProperty)) {
                value = getPropertyValue(borderProperty);
                if (value.isNull())
                    shorthandPropertyAppeared.set(borderProperty - firstCSSProperty);
                else
                    shorthandPropertyID = borderProperty;
            } else if (shorthandPropertyUsed[borderProperty - firstCSSProperty])
                shorthandPropertyID = borderProperty;
            if (!shorthandPropertyID)
                shorthandPropertyID = fallbackProperty;
        };
        
        if (is<CSSPendingSubstitutionValue>(property.value())) {
            auto& substitutionValue = downcast<CSSPendingSubstitutionValue>(*property.value());
            shorthandPropertyID = substitutionValue.shorthandPropertyId();
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
                serializeBorderShorthand(CSSPropertyBorder, borderFallbackShorthandProperty);
                break;
            case CSSPropertyBorderBlockStartWidth:
            case CSSPropertyBorderBlockEndWidth:
                if (!borderBlockFallbackShorthandProperty)
                    borderBlockFallbackShorthandProperty = CSSPropertyBorderBlockWidth;
                FALLTHROUGH;
            case CSSPropertyBorderBlockStartStyle:
            case CSSPropertyBorderBlockEndStyle:
                if (!borderBlockFallbackShorthandProperty)
                    borderBlockFallbackShorthandProperty = CSSPropertyBorderBlockStyle;
                FALLTHROUGH;
            case CSSPropertyBorderBlockStartColor:
            case CSSPropertyBorderBlockEndColor:
                if (!borderBlockFallbackShorthandProperty)
                    borderBlockFallbackShorthandProperty = CSSPropertyBorderBlockColor;
                serializeBorderShorthand(CSSPropertyBorderBlock, borderBlockFallbackShorthandProperty);
                break;
            case CSSPropertyBorderInlineStartWidth:
            case CSSPropertyBorderInlineEndWidth:
                if (!borderInlineFallbackShorthandProperty)
                    borderInlineFallbackShorthandProperty = CSSPropertyBorderInlineWidth;
                FALLTHROUGH;
            case CSSPropertyBorderInlineStartStyle:
            case CSSPropertyBorderInlineEndStyle:
                if (!borderInlineFallbackShorthandProperty)
                    borderInlineFallbackShorthandProperty = CSSPropertyBorderInlineStyle;
                FALLTHROUGH;
            case CSSPropertyBorderInlineStartColor:
            case CSSPropertyBorderInlineEndColor:
                if (!borderInlineFallbackShorthandProperty)
                    borderInlineFallbackShorthandProperty = CSSPropertyBorderInlineColor;
                serializeBorderShorthand(CSSPropertyBorderInline, borderInlineFallbackShorthandProperty);
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
            case CSSPropertyTop:
            case CSSPropertyRight:
            case CSSPropertyBottom:
            case CSSPropertyLeft:
                shorthandPropertyID = CSSPropertyInset;
                break;
            case CSSPropertyInsetBlockStart:
            case CSSPropertyInsetBlockEnd:
                shorthandPropertyID = CSSPropertyInsetBlock;
                break;
            case CSSPropertyInsetInlineStart:
            case CSSPropertyInsetInlineEnd:
                shorthandPropertyID = CSSPropertyInsetInline;
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
            case CSSPropertyMarginBlockStart:
            case CSSPropertyMarginBlockEnd:
                shorthandPropertyID = CSSPropertyMarginBlock;
                break;
            case CSSPropertyMarginInlineStart:
            case CSSPropertyMarginInlineEnd:
                shorthandPropertyID = CSSPropertyMarginInline;
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
            case CSSPropertyPaddingBlockStart:
            case CSSPropertyPaddingBlockEnd:
                shorthandPropertyID = CSSPropertyPaddingBlock;
                break;
            case CSSPropertyPaddingInlineStart:
            case CSSPropertyPaddingInlineEnd:
                shorthandPropertyID = CSSPropertyPaddingInline;
                break;
#if ENABLE(CSS_SCROLL_SNAP)
            case CSSPropertyScrollPaddingTop:
            case CSSPropertyScrollPaddingRight:
            case CSSPropertyScrollPaddingBottom:
            case CSSPropertyScrollPaddingLeft:
                shorthandPropertyID = CSSPropertyScrollPadding;
                break;
            case CSSPropertyScrollSnapMarginTop:
            case CSSPropertyScrollSnapMarginRight:
            case CSSPropertyScrollSnapMarginBottom:
            case CSSPropertyScrollSnapMarginLeft:
                shorthandPropertyID = CSSPropertyScrollSnapMargin;
                break;
#endif
            case CSSPropertyTransitionProperty:
            case CSSPropertyTransitionDuration:
            case CSSPropertyTransitionTimingFunction:
            case CSSPropertyTransitionDelay:
                shorthandPropertyID = CSSPropertyTransition;
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
            default:
                break;
            }
        }

        unsigned shortPropertyIndex = shorthandPropertyID - firstCSSProperty;
        if (shorthandPropertyID && isEnabledCSSProperty(shorthandPropertyID)) {
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

        result.append(": ", value, property.isImportant() ? " !important" : "", ';');
    }

    // FIXME: This is a not-so-nice way to turn x/y positions into single background-position/repeat in output.
    // In 2007 we decided this was required because background-position/repeat-x/y are non-standard properties and WebKit generated output would not work in Firefox (<rdar://problem/5143183>).
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
            if (value != "initial") {
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
    appendPositionOrProperty(repeatXPropertyIndex, repeatYPropertyIndex, "background-repeat", backgroundRepeatShorthand());

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

bool StyleProperties::traverseSubresources(const WTF::Function<bool (const CachedResource&)>& handler) const
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
    
Ref<DeferredStyleProperties> DeferredStyleProperties::create(const CSSParserTokenRange& tokenRange, CSSDeferredParser& parser)
{
    return adoptRef(*new DeferredStyleProperties(tokenRange, parser));
}

DeferredStyleProperties::DeferredStyleProperties(const CSSParserTokenRange& range, CSSDeferredParser& parser)
    : StylePropertiesBase(parser.mode(), DeferredPropertiesType)
    , m_parser(parser)
{
    size_t length = range.end() - range.begin();
    m_tokens.reserveCapacity(length);
    m_tokens.append(range.begin(), length);
}
    
DeferredStyleProperties::~DeferredStyleProperties() = default;

Ref<ImmutableStyleProperties> DeferredStyleProperties::parseDeferredProperties()
{
    return m_parser->parseDeclaration(m_tokens);
}

} // namespace WebCore
