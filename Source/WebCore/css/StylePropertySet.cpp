/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
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
#include "StylePropertySet.h"

#include "CSSParser.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "Document.h"
#include "PropertySetCSSStyleDeclaration.h"
#include "StylePropertyShorthand.h"
#include "StyleSheetContents.h"
#include <wtf/BitArray.h>
#include <wtf/MemoryInstrumentationVector.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(CSS_VARIABLES)
#include "CSSVariableValue.h"
#endif

#ifndef NDEBUG
#include <stdio.h>
#include <wtf/ASCIICType.h>
#include <wtf/text/CString.h>
#endif

using namespace std;

namespace WebCore {

typedef HashMap<const StylePropertySet*, OwnPtr<PropertySetCSSStyleDeclaration> > PropertySetCSSOMWrapperMap;
static PropertySetCSSOMWrapperMap& propertySetCSSOMWrapperMap()
{
    DEFINE_STATIC_LOCAL(PropertySetCSSOMWrapperMap, propertySetCSSOMWrapperMapInstance, ());
    return propertySetCSSOMWrapperMapInstance;
}

static size_t sizeForImmutableStylePropertySetWithPropertyCount(unsigned count)
{
    return sizeof(ImmutableStylePropertySet) - sizeof(void*) + sizeof(CSSValue*) * count + sizeof(StylePropertyMetadata) * count;
}

static bool isInitialOrInherit(const String& value)
{
    DEFINE_STATIC_LOCAL(String, initial, ("initial"));
    DEFINE_STATIC_LOCAL(String, inherit, ("inherit"));
    return value.length() == 7 && (value == initial || value == inherit);
}

PassRefPtr<StylePropertySet> StylePropertySet::createImmutable(const CSSProperty* properties, unsigned count, CSSParserMode cssParserMode)
{
    void* slot = WTF::fastMalloc(sizeForImmutableStylePropertySetWithPropertyCount(count));
    return adoptRef(new (slot) ImmutableStylePropertySet(properties, count, cssParserMode));
}

PassRefPtr<StylePropertySet> StylePropertySet::immutableCopyIfNeeded() const
{
    if (!isMutable())
        return const_cast<StylePropertySet*>(this);
    return createImmutable(mutablePropertyVector().data(), mutablePropertyVector().size(), cssParserMode());
}

MutableStylePropertySet::MutableStylePropertySet(const CSSProperty* properties, unsigned length)
    : StylePropertySet(CSSStrictMode)
{
    m_propertyVector.reserveInitialCapacity(length);
    for (unsigned i = 0; i < length; ++i)
        m_propertyVector.uncheckedAppend(properties[i]);
}

ImmutableStylePropertySet::ImmutableStylePropertySet(const CSSProperty* properties, unsigned length, CSSParserMode cssParserMode)
    : StylePropertySet(cssParserMode, length)
{
    StylePropertyMetadata* metadataArray = const_cast<StylePropertyMetadata*>(immutableMetadataArray());
    CSSValue** valueArray = const_cast<CSSValue**>(immutableValueArray());
    for (unsigned i = 0; i < length; ++i) {
        metadataArray[i] = properties[i].metadata();
        valueArray[i] = properties[i].value();
        valueArray[i]->ref();
    }
}

ImmutableStylePropertySet::~ImmutableStylePropertySet()
{
    CSSValue** valueArray = const_cast<CSSValue**>(immutableValueArray());
    for (unsigned i = 0; i < m_arraySize; ++i)
        valueArray[i]->deref();
}

MutableStylePropertySet::MutableStylePropertySet(const StylePropertySet& other)
    : StylePropertySet(other.cssParserMode())
{
    if (other.isMutable())
        m_propertyVector = static_cast<const MutableStylePropertySet&>(other).mutablePropertyVector();
    else {
        m_propertyVector.reserveInitialCapacity(other.propertyCount());
        for (unsigned i = 0; i < other.propertyCount(); ++i)
            m_propertyVector.uncheckedAppend(other.propertyAt(i).toCSSProperty());
    }
}

StylePropertySet::~StylePropertySet()
{
    ASSERT(!m_ownsCSSOMWrapper || propertySetCSSOMWrapperMap().contains(this));
    if (m_ownsCSSOMWrapper)
        propertySetCSSOMWrapperMap().remove(this);
}

String StylePropertySet::getPropertyValue(CSSPropertyID propertyID) const
{
    RefPtr<CSSValue> value = getPropertyCSSValue(propertyID);
    if (value)
        return value->cssText();

    // Shorthand and 4-values properties
    switch (propertyID) {
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
    case CSSPropertyWebkitFlex:
        return getShorthandValue(webkitFlexShorthand());
    case CSSPropertyWebkitFlexFlow:
        return getShorthandValue(webkitFlexFlowShorthand());
    case CSSPropertyFont:
        return fontValue();
    case CSSPropertyMargin:
        return get4Values(marginShorthand());
    case CSSPropertyOverflow:
        return getCommonValue(overflowShorthand());
    case CSSPropertyPadding:
        return get4Values(paddingShorthand());
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
    case CSSPropertyWebkitTransformOrigin:
        return getShorthandValue(webkitTransformOriginShorthand());
    case CSSPropertyWebkitTransition:
        return getLayeredShorthandValue(webkitTransitionShorthand());
    case CSSPropertyWebkitAnimation:
        return getLayeredShorthandValue(webkitAnimationShorthand());
#if ENABLE(CSS_EXCLUSIONS)
    case CSSPropertyWebkitWrap:
        return getShorthandValue(webkitWrapShorthand());
#endif
#if ENABLE(SVG)
    case CSSPropertyMarker: {
        RefPtr<CSSValue> value = getPropertyCSSValue(CSSPropertyMarkerStart);
        if (value)
            return value->cssText();
        return String();
    }
#endif
    case CSSPropertyBorderRadius:
        return get4Values(borderRadiusShorthand());
    default:
          return String();
    }
}

String StylePropertySet::borderSpacingValue(const StylePropertyShorthand& shorthand) const
{
    RefPtr<CSSValue> horizontalValue = getPropertyCSSValue(shorthand.properties()[0]);
    RefPtr<CSSValue> verticalValue = getPropertyCSSValue(shorthand.properties()[1]);

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

bool StylePropertySet::appendFontLonghandValueIfExplicit(CSSPropertyID propertyID, StringBuilder& result, String& commonValue) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return false; // All longhands must have at least implicit values if "font" is specified.

    if (propertyAt(foundPropertyIndex).isImplicit()) {
        commonValue = String();
        return true;
    }

    char prefix = '\0';
    switch (propertyID) {
    case CSSPropertyFontStyle:
        break; // No prefix.
    case CSSPropertyFontFamily:
    case CSSPropertyFontVariant:
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

    return true;
}

String StylePropertySet::fontValue() const
{
    int foundPropertyIndex = findPropertyIndex(CSSPropertyFontSize);
    if (foundPropertyIndex == -1)
        return emptyString();

    PropertyReference fontSizeProperty = propertyAt(foundPropertyIndex);
    if (fontSizeProperty.isImplicit())
        return emptyString();

    String commonValue = fontSizeProperty.value()->cssText();
    StringBuilder result;
    bool success = true;
    success &= appendFontLonghandValueIfExplicit(CSSPropertyFontStyle, result, commonValue);
    success &= appendFontLonghandValueIfExplicit(CSSPropertyFontVariant, result, commonValue);
    success &= appendFontLonghandValueIfExplicit(CSSPropertyFontWeight, result, commonValue);
    if (!result.isEmpty())
        result.append(' ');
    result.append(fontSizeProperty.value()->cssText());
    success &= appendFontLonghandValueIfExplicit(CSSPropertyLineHeight, result, commonValue);
    success &= appendFontLonghandValueIfExplicit(CSSPropertyFontFamily, result, commonValue);
    if (!success) {
        // An invalid "font" value has been built (should never happen, as at least implicit values
        // for mandatory longhands are always found in the style), report empty value instead.
        ASSERT_NOT_REACHED();
        return emptyString();
    }
    if (isInitialOrInherit(commonValue))
        return commonValue;
    return result.toString();
}

String StylePropertySet::get4Values(const StylePropertyShorthand& shorthand) const
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

    bool showLeft = right.value()->cssText() != left.value()->cssText();
    bool showBottom = (top.value()->cssText() != bottom.value()->cssText()) || showLeft;
    bool showRight = (top.value()->cssText() != right.value()->cssText()) || showBottom;

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

String StylePropertySet::getLayeredShorthandValue(const StylePropertyShorthand& shorthand) const
{
    StringBuilder result;

    const unsigned size = shorthand.length();
    // Begin by collecting the properties into an array.
    Vector< RefPtr<CSSValue> > values(size);
    size_t numLayers = 0;

    for (unsigned i = 0; i < size; ++i) {
        values[i] = getPropertyCSSValue(shorthand.properties()[i]);
        if (values[i]) {
            if (values[i]->isBaseValueList()) {
                CSSValueList* valueList = static_cast<CSSValueList*>(values[i].get());
                numLayers = max(valueList->length(), numLayers);
            } else
                numLayers = max<size_t>(1U, numLayers);
        }
    }

    String commonValue;
    bool commonValueInitialized = false;

    // Now stitch the properties together.  Implicit initial values are flagged as such and
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
                    value = static_cast<CSSValueList*>(values[j].get())->item(i);
                else {
                    value = values[j];

                    // Color only belongs in the last layer.
                    if (shorthand.properties()[j] == CSSPropertyBackgroundColor) {
                        if (i != numLayers - 1)
                            value = 0;
                    } else if (i != 0) // Other singletons only belong in the first layer.
                        value = 0;
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
                    if (nextValue->isValueList())
                        yValue = static_cast<CSSValueList*>(nextValue.get())->itemWithoutBoundsCheck(i);
                    else
                        yValue = nextValue;

                    int xId = static_cast<CSSPrimitiveValue*>(value.get())->getIdent();
                    int yId = static_cast<CSSPrimitiveValue*>(yValue.get())->getIdent();
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

String StylePropertySet::getShorthandValue(const StylePropertyShorthand& shorthand) const
{
    String commonValue;
    StringBuilder result;
    for (unsigned i = 0; i < shorthand.length(); ++i) {
        if (!isPropertyImplicit(shorthand.properties()[i])) {
            RefPtr<CSSValue> value = getPropertyCSSValue(shorthand.properties()[i]);
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
String StylePropertySet::getCommonValue(const StylePropertyShorthand& shorthand) const
{
    String res;
    bool lastPropertyWasImportant = false;
    for (unsigned i = 0; i < shorthand.length(); ++i) {
        RefPtr<CSSValue> value = getPropertyCSSValue(shorthand.properties()[i]);
        // FIXME: CSSInitialValue::cssText should generate the right value.
        if (!value)
            return String();
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

String StylePropertySet::borderPropertyValue(CommonValueMode valueMode) const
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

PassRefPtr<CSSValue> StylePropertySet::getPropertyCSSValue(CSSPropertyID propertyID) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return 0;
    return propertyAt(foundPropertyIndex).value();
}

bool StylePropertySet::removeShorthandProperty(CSSPropertyID propertyID)
{
    ASSERT(isMutable());
    StylePropertyShorthand shorthand = shorthandForProperty(propertyID);
    if (!shorthand.length())
        return false;
    return removePropertiesInSet(shorthand.properties(), shorthand.length());
}

bool StylePropertySet::removeProperty(CSSPropertyID propertyID, String* returnText)
{
    ASSERT(isMutable());
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
    mutablePropertyVector().remove(foundPropertyIndex);
    
    return true;
}

bool StylePropertySet::propertyIsImportant(CSSPropertyID propertyID) const
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

CSSPropertyID StylePropertySet::getPropertyShorthand(CSSPropertyID propertyID) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return CSSPropertyInvalid;
    return propertyAt(foundPropertyIndex).shorthandID();
}

bool StylePropertySet::isPropertyImplicit(CSSPropertyID propertyID) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return false;
    return propertyAt(foundPropertyIndex).isImplicit();
}

bool StylePropertySet::setProperty(CSSPropertyID propertyID, const String& value, bool important, StyleSheetContents* contextStyleSheet)
{
    ASSERT(isMutable());
    // Setting the value to an empty string just removes the property in both IE and Gecko.
    // Setting it to null seems to produce less consistent results, but we treat it just the same.
    if (value.isEmpty()) {
        removeProperty(propertyID);
        return true;
    }

    // When replacing an existing property value, this moves the property to the end of the list.
    // Firefox preserves the position, and MSIE moves the property to the beginning.
    return CSSParser::parseValue(this, propertyID, value, important, cssParserMode(), contextStyleSheet);
}

void StylePropertySet::setProperty(CSSPropertyID propertyID, PassRefPtr<CSSValue> prpValue, bool important)
{
    ASSERT(isMutable());
    StylePropertyShorthand shorthand = shorthandForProperty(propertyID);
    if (!shorthand.length()) {
        setProperty(CSSProperty(propertyID, prpValue, important));
        return;
    }

    removePropertiesInSet(shorthand.properties(), shorthand.length());

    RefPtr<CSSValue> value = prpValue;
    for (unsigned i = 0; i < shorthand.length(); ++i)
        mutablePropertyVector().append(CSSProperty(shorthand.properties()[i], value, important));
}

void StylePropertySet::setProperty(const CSSProperty& property, CSSProperty* slot)
{
    ASSERT(isMutable());
    if (!removeShorthandProperty(property.id())) {
        CSSProperty* toReplace = slot ? slot : findMutableCSSPropertyWithID(property.id());
        if (toReplace) {
            *toReplace = property;
            return;
        }
    }
    mutablePropertyVector().append(property);
}

bool StylePropertySet::setProperty(CSSPropertyID propertyID, int identifier, bool important)
{
    ASSERT(isMutable());
    setProperty(CSSProperty(propertyID, cssValuePool().createIdentifierValue(identifier), important));
    return true;
}

void StylePropertySet::parseDeclaration(const String& styleDeclaration, StyleSheetContents* contextStyleSheet)
{
    ASSERT(isMutable());

    mutablePropertyVector().clear();

    CSSParserContext context(cssParserMode());
    if (contextStyleSheet) {
        context = contextStyleSheet->parserContext();
        context.mode = cssParserMode();
    }
    CSSParser parser(context);
    parser.parseDeclaration(this, styleDeclaration, 0, contextStyleSheet);
}

void StylePropertySet::addParsedProperties(const Vector<CSSProperty>& properties)
{
    ASSERT(isMutable());
    mutablePropertyVector().reserveCapacity(mutablePropertyVector().size() + properties.size());
    for (unsigned i = 0; i < properties.size(); ++i)
        addParsedProperty(properties[i]);
}

void StylePropertySet::addParsedProperty(const CSSProperty& property)
{
    ASSERT(isMutable());
    // Only add properties that have no !important counterpart present
    if (!propertyIsImportant(property.id()) || property.isImportant())
        setProperty(property);
}

String StylePropertySet::asText() const
{
    StringBuilder result;

    int positionXPropertyIndex = -1;
    int positionYPropertyIndex = -1;
    int repeatXPropertyIndex = -1;
    int repeatYPropertyIndex = -1;

    BitArray<numCSSProperties> shorthandPropertyUsed;
    BitArray<numCSSProperties> shorthandPropertyAppeared;

    unsigned size = propertyCount();
    unsigned numDecls = 0;
    for (unsigned n = 0; n < size; ++n) {
        PropertyReference property = propertyAt(n);
        CSSPropertyID propertyID = property.id();
        CSSPropertyID shorthandPropertyID = CSSPropertyInvalid;
        CSSPropertyID borderFallbackShorthandProperty = CSSPropertyInvalid;
        String value;

        switch (propertyID) {
#if ENABLE(CSS_VARIABLES)
        case CSSPropertyVariable:
            if (numDecls++)
                result.append(' ');
            result.append(property.cssText());
            continue;
#endif
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
        case CSSPropertyBorderTopStyle:
        case CSSPropertyBorderRightStyle:
        case CSSPropertyBorderBottomStyle:
        case CSSPropertyBorderLeftStyle:
            if (!borderFallbackShorthandProperty)
                borderFallbackShorthandProperty = CSSPropertyBorderStyle;
        case CSSPropertyBorderTopColor:
        case CSSPropertyBorderRightColor:
        case CSSPropertyBorderBottomColor:
        case CSSPropertyBorderLeftColor:
            if (!borderFallbackShorthandProperty)
                borderFallbackShorthandProperty = CSSPropertyBorderColor;

            // FIXME: Deal with cases where only some of border-(top|right|bottom|left) are specified.
            if (!shorthandPropertyAppeared.get(CSSPropertyBorder - firstCSSProperty)) {
                value = borderPropertyValue(ReturnNullOnUncommonValues);
                if (value.isNull())
                    shorthandPropertyAppeared.set(CSSPropertyBorder - firstCSSProperty);
                else
                    shorthandPropertyID = CSSPropertyBorder;
            } else if (shorthandPropertyUsed.get(CSSPropertyBorder - firstCSSProperty))
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
        case CSSPropertyFontVariant:
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
        case CSSPropertyWebkitAnimationName:
        case CSSPropertyWebkitAnimationDuration:
        case CSSPropertyWebkitAnimationTimingFunction:
        case CSSPropertyWebkitAnimationDelay:
        case CSSPropertyWebkitAnimationIterationCount:
        case CSSPropertyWebkitAnimationDirection:
        case CSSPropertyWebkitAnimationFillMode:
            shorthandPropertyID = CSSPropertyWebkitAnimation;
            break;
        case CSSPropertyWebkitFlexDirection:
        case CSSPropertyWebkitFlexWrap:
            shorthandPropertyID = CSSPropertyWebkitFlexFlow;
            break;
        case CSSPropertyWebkitFlexBasis:
        case CSSPropertyWebkitFlexGrow:
        case CSSPropertyWebkitFlexShrink:
            shorthandPropertyID = CSSPropertyWebkitFlex;
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
        case CSSPropertyWebkitTransformOriginX:
        case CSSPropertyWebkitTransformOriginY:
        case CSSPropertyWebkitTransformOriginZ:
            shorthandPropertyID = CSSPropertyWebkitTransformOrigin;
            break;
        case CSSPropertyWebkitTransitionProperty:
        case CSSPropertyWebkitTransitionDuration:
        case CSSPropertyWebkitTransitionTimingFunction:
        case CSSPropertyWebkitTransitionDelay:
            shorthandPropertyID = CSSPropertyWebkitTransition;
            break;
#if ENABLE(CSS_EXCLUSIONS)
        case CSSPropertyWebkitWrapFlow:
        case CSSPropertyWebkitShapeMargin:
        case CSSPropertyWebkitShapePadding:
            shorthandPropertyID = CSSPropertyWebkitWrap;
            break;
#endif
        default:
            break;
        }

        unsigned shortPropertyIndex = shorthandPropertyID - firstCSSProperty;
        if (shorthandPropertyID) {
            if (shorthandPropertyUsed.get(shortPropertyIndex))
                continue;
            if (!shorthandPropertyAppeared.get(shortPropertyIndex) && value.isNull())
                value = getPropertyValue(shorthandPropertyID);
            shorthandPropertyAppeared.set(shortPropertyIndex);
        }

        if (!value.isNull()) {
            propertyID = shorthandPropertyID;
            shorthandPropertyUsed.set(shortPropertyIndex);
        } else
            value = property.value()->cssText();

        if (value == "initial" && !CSSProperty::isInheritedProperty(propertyID))
            continue;

        if (numDecls++)
            result.append(' ');
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

void StylePropertySet::mergeAndOverrideOnConflict(const StylePropertySet* other)
{
    ASSERT(isMutable());
    unsigned size = other->propertyCount();
    for (unsigned n = 0; n < size; ++n) {
        PropertyReference toMerge = other->propertyAt(n);
        CSSProperty* old = findMutableCSSPropertyWithID(toMerge.id());
        if (old)
            setProperty(toMerge.toCSSProperty(), old);
        else
            mutablePropertyVector().append(toMerge.toCSSProperty());
    }
}

void StylePropertySet::addSubresourceStyleURLs(ListHashSet<KURL>& urls, StyleSheetContents* contextStyleSheet) const
{
    unsigned size = propertyCount();
    for (unsigned i = 0; i < size; ++i)
        propertyAt(i).value()->addSubresourceStyleURLs(urls, contextStyleSheet);
}

bool StylePropertySet::hasFailedOrCanceledSubresources() const
{
    unsigned size = propertyCount();
    for (unsigned i = 0; i < size; ++i) {
        if (propertyAt(i).value()->hasFailedOrCanceledSubresources())
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
    CSSPropertyWebkitColumnCount,
    CSSPropertyWebkitColumnGap,
    CSSPropertyWebkitColumnRuleColor,
    CSSPropertyWebkitColumnRuleStyle,
    CSSPropertyWebkitColumnRuleWidth,
    CSSPropertyWebkitColumnBreakBefore,
    CSSPropertyWebkitColumnBreakAfter,
    CSSPropertyWebkitColumnBreakInside,
    CSSPropertyWebkitColumnWidth,
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
#endif // CSS3_TEXT
    CSSPropertyTextIndent,
    CSSPropertyWidows
};

void StylePropertySet::clear()
{
    ASSERT(isMutable());
    mutablePropertyVector().clear();
}

const unsigned numBlockProperties = WTF_ARRAY_LENGTH(blockProperties);

PassRefPtr<StylePropertySet> StylePropertySet::copyBlockProperties() const
{
    return copyPropertiesInSet(blockProperties, numBlockProperties);
}

void StylePropertySet::removeBlockProperties()
{
    removePropertiesInSet(blockProperties, numBlockProperties);
}

bool StylePropertySet::removePropertiesInSet(const CSSPropertyID* set, unsigned length)
{
    ASSERT(isMutable());
    if (mutablePropertyVector().isEmpty())
        return false;

    // FIXME: This is always used with static sets and in that case constructing the hash repeatedly is pretty pointless.
    HashSet<CSSPropertyID> toRemove;
    for (unsigned i = 0; i < length; ++i)
        toRemove.add(set[i]);

    Vector<CSSProperty> newProperties;
    newProperties.reserveInitialCapacity(mutablePropertyVector().size());

    unsigned size = mutablePropertyVector().size();
    for (unsigned n = 0; n < size; ++n) {
        const CSSProperty& property = mutablePropertyVector().at(n);
        // Not quite sure if the isImportant test is needed but it matches the existing behavior.
        if (!property.isImportant()) {
            if (toRemove.contains(property.id()))
                continue;
        }
        newProperties.append(property);
    }

    bool changed = newProperties.size() != mutablePropertyVector().size();
    mutablePropertyVector() = newProperties;
    return changed;
}

int StylePropertySet::findPropertyIndex(CSSPropertyID propertyID) const
{
    for (int n = propertyCount() - 1 ; n >= 0; --n) {
        if (propertyID == propertyAt(n).id())
            return n;
    }
    return -1;
}

CSSProperty* StylePropertySet::findMutableCSSPropertyWithID(CSSPropertyID propertyID)
{
    ASSERT(isMutable());
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return 0;
    return &mutablePropertyVector().at(foundPropertyIndex);
}
    
bool StylePropertySet::propertyMatches(const PropertyReference& property) const
{
    int foundPropertyIndex = findPropertyIndex(property.id());
    if (foundPropertyIndex == -1)
        return false;
    return propertyAt(foundPropertyIndex).value()->cssText() == property.value()->cssText();
}
    
void StylePropertySet::removeEquivalentProperties(const StylePropertySet* style)
{
    ASSERT(isMutable());
    Vector<CSSPropertyID> propertiesToRemove;
    unsigned size = mutablePropertyVector().size();
    for (unsigned i = 0; i < size; ++i) {
        PropertyReference property = propertyAt(i);
        if (style->propertyMatches(property))
            propertiesToRemove.append(property.id());
    }    
    // FIXME: This should use mass removal.
    for (unsigned i = 0; i < propertiesToRemove.size(); ++i)
        removeProperty(propertiesToRemove[i]);
}

void StylePropertySet::removeEquivalentProperties(const CSSStyleDeclaration* style)
{
    ASSERT(isMutable());
    Vector<CSSPropertyID> propertiesToRemove;
    unsigned size = mutablePropertyVector().size();
    for (unsigned i = 0; i < size; ++i) {
        PropertyReference property = propertyAt(i);
        if (style->cssPropertyMatches(property))
            propertiesToRemove.append(property.id());
    }    
    // FIXME: This should use mass removal.
    for (unsigned i = 0; i < propertiesToRemove.size(); ++i)
        removeProperty(propertiesToRemove[i]);
}

PassRefPtr<StylePropertySet> StylePropertySet::copy() const
{
    return adoptRef(new MutableStylePropertySet(*this));
}

PassRefPtr<StylePropertySet> StylePropertySet::copyPropertiesInSet(const CSSPropertyID* set, unsigned length) const
{
    Vector<CSSProperty, 256> list;
    list.reserveInitialCapacity(length);
    for (unsigned i = 0; i < length; ++i) {
        RefPtr<CSSValue> value = getPropertyCSSValue(set[i]);
        if (value)
            list.append(CSSProperty(set[i], value.release(), false));
    }
    return StylePropertySet::create(list.data(), list.size());
}

PropertySetCSSStyleDeclaration* StylePropertySet::cssStyleDeclaration()
{
    if (!m_ownsCSSOMWrapper)
        return 0;
    ASSERT(isMutable());
    return propertySetCSSOMWrapperMap().get(this);
}

CSSStyleDeclaration* StylePropertySet::ensureCSSStyleDeclaration()
{
    ASSERT(isMutable());

    if (m_ownsCSSOMWrapper) {
        ASSERT(!static_cast<CSSStyleDeclaration*>(propertySetCSSOMWrapperMap().get(this))->parentRule());
        ASSERT(!propertySetCSSOMWrapperMap().get(this)->parentElement());
        return propertySetCSSOMWrapperMap().get(this);
    }
    m_ownsCSSOMWrapper = true;
    PropertySetCSSStyleDeclaration* cssomWrapper = new PropertySetCSSStyleDeclaration(const_cast<StylePropertySet*>(this));
    propertySetCSSOMWrapperMap().add(this, adoptPtr(cssomWrapper));
    return cssomWrapper;
}

CSSStyleDeclaration* StylePropertySet::ensureInlineCSSStyleDeclaration(const StyledElement* parentElement)
{
    ASSERT(isMutable());

    if (m_ownsCSSOMWrapper) {
        ASSERT(propertySetCSSOMWrapperMap().get(this)->parentElement() == parentElement);
        return propertySetCSSOMWrapperMap().get(this);
    }
    m_ownsCSSOMWrapper = true;
    PropertySetCSSStyleDeclaration* cssomWrapper = new InlineCSSStyleDeclaration(const_cast<StylePropertySet*>(this), const_cast<StyledElement*>(parentElement));
    propertySetCSSOMWrapperMap().add(this, adoptPtr(cssomWrapper));
    return cssomWrapper;
}

unsigned StylePropertySet::averageSizeInBytes()
{
    // Please update this if the storage scheme changes so that this longer reflects the actual size.
    return sizeForImmutableStylePropertySetWithPropertyCount(4);
}

void StylePropertySet::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    size_t actualSize = m_isMutable ? sizeof(StylePropertySet) : sizeForImmutableStylePropertySetWithPropertyCount(m_arraySize);
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::CSS, actualSize);
    if (m_isMutable)
        info.addMember(mutablePropertyVector());
    else {
        for (unsigned i = 0; i < propertyCount(); ++i)
            info.addMember(propertyAt(i).value());
    }
}

// See the function above if you need to update this.
struct SameSizeAsStylePropertySet : public RefCounted<SameSizeAsStylePropertySet> {
    unsigned bitfield;
};
COMPILE_ASSERT(sizeof(StylePropertySet) == sizeof(SameSizeAsStylePropertySet), style_property_set_should_stay_small);

#ifndef NDEBUG
void StylePropertySet::showStyle()
{
    fprintf(stderr, "%s\n", asText().ascii().data());
}
#endif

PassRefPtr<StylePropertySet> StylePropertySet::create(CSSParserMode cssParserMode)
{
    return adoptRef(new MutableStylePropertySet(cssParserMode));
}

PassRefPtr<StylePropertySet> StylePropertySet::create(const CSSProperty* properties, unsigned count)
{
    return adoptRef(new MutableStylePropertySet(properties, count));
}

String StylePropertySet::PropertyReference::cssName() const
{
#if ENABLE(CSS_VARIABLES)
    if (id() == CSSPropertyVariable) {
        ASSERT(propertyValue()->isVariableValue());
        return "-webkit-var-" + static_cast<const CSSVariableValue*>(propertyValue())->name();
    }
#endif
    return getPropertyNameString(id());
}

String StylePropertySet::PropertyReference::cssText() const
{
    StringBuilder result;
    result.append(cssName());
    result.appendLiteral(": ");
    result.append(propertyValue()->cssText());
    if (isImportant())
        result.appendLiteral(" !important");
    result.append(';');
    return result.toString();
}


} // namespace WebCore
