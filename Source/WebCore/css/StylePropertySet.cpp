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
#include "CSSPropertyLonghand.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "Document.h"
#include "PropertySetCSSStyleDeclaration.h"
#include <wtf/text/StringBuilder.h>

using namespace std;

namespace WebCore {

typedef HashMap<const StylePropertySet*, OwnPtr<PropertySetCSSStyleDeclaration> > PropertySetCSSOMWrapperMap;
static PropertySetCSSOMWrapperMap& propertySetCSSOMWrapperMap()
{
    DEFINE_STATIC_LOCAL(PropertySetCSSOMWrapperMap, propertySetCSSOMWrapperMapInstance, ());
    return propertySetCSSOMWrapperMapInstance;
}

StylePropertySet::StylePropertySet()
    : m_strictParsing(false)
    , m_hasCSSOMWrapper(false)
{
}

StylePropertySet::StylePropertySet(const Vector<CSSProperty>& properties)
    : m_properties(properties)
    , m_strictParsing(true)
    , m_hasCSSOMWrapper(false)
{
    m_properties.shrinkToFit();
}

StylePropertySet::StylePropertySet(const CSSProperty* properties, int numProperties, bool useStrictParsing)
    : m_strictParsing(useStrictParsing)
    , m_hasCSSOMWrapper(false)
{
    // FIXME: This logic belongs in CSSParser.

    m_properties.reserveInitialCapacity(numProperties);
    HashMap<int, bool> candidates;
    for (int i = 0; i < numProperties; ++i) {
        const CSSProperty& property = properties[i];
        bool important = property.isImportant();

        HashMap<int, bool>::iterator it = candidates.find(property.id());
        if (it != candidates.end()) {
            if (!important && it->second)
                continue;
            removeProperty(property.id());
        }

        m_properties.append(property);
        candidates.set(property.id(), important);
    }
}

StylePropertySet::~StylePropertySet()
{
    ASSERT(!m_hasCSSOMWrapper || propertySetCSSOMWrapperMap().contains(this));
    if (m_hasCSSOMWrapper)
        propertySetCSSOMWrapperMap().remove(this);
}

void StylePropertySet::copyPropertiesFrom(const StylePropertySet& other)
{
    m_properties = other.m_properties;
}

String StylePropertySet::getPropertyValue(int propertyID) const
{
    RefPtr<CSSValue> value = getPropertyCSSValue(propertyID);
    if (value)
        return value->cssText();

    // Shorthand and 4-values properties
    switch (propertyID) {
        case CSSPropertyBorderSpacing: {
            const int properties[2] = { CSSPropertyWebkitBorderHorizontalSpacing, CSSPropertyWebkitBorderVerticalSpacing };
            return borderSpacingValue(properties);
        }
        case CSSPropertyBackgroundPosition: {
            // FIXME: Is this correct? The code in cssparser.cpp is confusing
            const int properties[2] = { CSSPropertyBackgroundPositionX, CSSPropertyBackgroundPositionY };
            return getLayeredShorthandValue(properties);
        }
        case CSSPropertyBackgroundRepeat: {
            const int properties[2] = { CSSPropertyBackgroundRepeatX, CSSPropertyBackgroundRepeatY };
            return getLayeredShorthandValue(properties);
        }
        case CSSPropertyBackground: {
            const int properties[9] = { CSSPropertyBackgroundColor,
                                        CSSPropertyBackgroundImage,
                                        CSSPropertyBackgroundRepeatX,
                                        CSSPropertyBackgroundRepeatY,
                                        CSSPropertyBackgroundAttachment,
                                        CSSPropertyBackgroundPositionX,
                                        CSSPropertyBackgroundPositionY,
                                        CSSPropertyBackgroundClip,
                                        CSSPropertyBackgroundOrigin };
            return getLayeredShorthandValue(properties);
        }
        case CSSPropertyBorder: {
            const int properties[3][4] = {{ CSSPropertyBorderTopWidth,
                                            CSSPropertyBorderRightWidth,
                                            CSSPropertyBorderBottomWidth,
                                            CSSPropertyBorderLeftWidth },
                                          { CSSPropertyBorderTopStyle,
                                            CSSPropertyBorderRightStyle,
                                            CSSPropertyBorderBottomStyle,
                                            CSSPropertyBorderLeftStyle },
                                          { CSSPropertyBorderTopColor,
                                            CSSPropertyBorderRightColor,
                                            CSSPropertyBorderBottomColor,
                                            CSSPropertyBorderLeftColor }};
            String res;
            for (size_t i = 0; i < WTF_ARRAY_LENGTH(properties); ++i) {
                String value = getCommonValue(properties[i]);
                if (!value.isNull()) {
                    if (!res.isNull())
                        res += " ";
                    res += value;
                }
            }
            return res;
        }
        case CSSPropertyBorderTop: {
            const int properties[3] = { CSSPropertyBorderTopWidth, CSSPropertyBorderTopStyle,
                                        CSSPropertyBorderTopColor};
            return getShorthandValue(properties);
        }
        case CSSPropertyBorderRight: {
            const int properties[3] = { CSSPropertyBorderRightWidth, CSSPropertyBorderRightStyle,
                                        CSSPropertyBorderRightColor};
            return getShorthandValue(properties);
        }
        case CSSPropertyBorderBottom: {
            const int properties[3] = { CSSPropertyBorderBottomWidth, CSSPropertyBorderBottomStyle,
                                        CSSPropertyBorderBottomColor};
            return getShorthandValue(properties);
        }
        case CSSPropertyBorderLeft: {
            const int properties[3] = { CSSPropertyBorderLeftWidth, CSSPropertyBorderLeftStyle,
                                        CSSPropertyBorderLeftColor};
            return getShorthandValue(properties);
        }
        case CSSPropertyOutline: {
            const int properties[3] = { CSSPropertyOutlineWidth, CSSPropertyOutlineStyle,
                                        CSSPropertyOutlineColor };
            return getShorthandValue(properties);
        }
        case CSSPropertyBorderColor: {
            const int properties[4] = { CSSPropertyBorderTopColor, CSSPropertyBorderRightColor,
                                        CSSPropertyBorderBottomColor, CSSPropertyBorderLeftColor };
            return get4Values(properties);
        }
        case CSSPropertyBorderWidth: {
            const int properties[4] = { CSSPropertyBorderTopWidth, CSSPropertyBorderRightWidth,
                                        CSSPropertyBorderBottomWidth, CSSPropertyBorderLeftWidth };
            return get4Values(properties);
        }
        case CSSPropertyBorderStyle: {
            const int properties[4] = { CSSPropertyBorderTopStyle, CSSPropertyBorderRightStyle,
                                        CSSPropertyBorderBottomStyle, CSSPropertyBorderLeftStyle };
            return get4Values(properties);
        }
        case CSSPropertyWebkitFlexFlow: {
            const int properties[] = { CSSPropertyWebkitFlexDirection, CSSPropertyWebkitFlexWrap };
            return getShorthandValue(properties);
        }
        case CSSPropertyFont:
            return fontValue();
        case CSSPropertyMargin: {
            const int properties[4] = { CSSPropertyMarginTop, CSSPropertyMarginRight,
                                        CSSPropertyMarginBottom, CSSPropertyMarginLeft };
            return get4Values(properties);
        }
        case CSSPropertyOverflow: {
            const int properties[2] = { CSSPropertyOverflowX, CSSPropertyOverflowY };
            return getCommonValue(properties);
        }
        case CSSPropertyPadding: {
            const int properties[4] = { CSSPropertyPaddingTop, CSSPropertyPaddingRight,
                                        CSSPropertyPaddingBottom, CSSPropertyPaddingLeft };
            return get4Values(properties);
        }
        case CSSPropertyListStyle: {
            const int properties[3] = { CSSPropertyListStyleType, CSSPropertyListStylePosition,
                                        CSSPropertyListStyleImage };
            return getShorthandValue(properties);
        }
        case CSSPropertyWebkitMaskPosition: {
            // FIXME: Is this correct? The code in cssparser.cpp is confusing
            const int properties[2] = { CSSPropertyWebkitMaskPositionX, CSSPropertyWebkitMaskPositionY };
            return getLayeredShorthandValue(properties);
        }
        case CSSPropertyWebkitMaskRepeat: {
            const int properties[2] = { CSSPropertyWebkitMaskRepeatX, CSSPropertyWebkitMaskRepeatY };
            return getLayeredShorthandValue(properties);
        }
        case CSSPropertyWebkitMask: {
            const int properties[] = { CSSPropertyWebkitMaskImage, CSSPropertyWebkitMaskRepeat,
                                       CSSPropertyWebkitMaskAttachment, CSSPropertyWebkitMaskPosition, CSSPropertyWebkitMaskClip,
                                       CSSPropertyWebkitMaskOrigin };
            return getLayeredShorthandValue(properties);
        }
        case CSSPropertyWebkitTransformOrigin: {
            const int properties[3] = { CSSPropertyWebkitTransformOriginX,
                                        CSSPropertyWebkitTransformOriginY,
                                        CSSPropertyWebkitTransformOriginZ };
            return getShorthandValue(properties);
        }
        case CSSPropertyWebkitTransition: {
            const int properties[4] = { CSSPropertyWebkitTransitionProperty, CSSPropertyWebkitTransitionDuration,
                                        CSSPropertyWebkitTransitionTimingFunction, CSSPropertyWebkitTransitionDelay };
            return getLayeredShorthandValue(properties);
        }
        case CSSPropertyWebkitAnimation: {
            const int properties[7] = { CSSPropertyWebkitAnimationName, CSSPropertyWebkitAnimationDuration,
                                        CSSPropertyWebkitAnimationTimingFunction, CSSPropertyWebkitAnimationDelay,
                                        CSSPropertyWebkitAnimationIterationCount, CSSPropertyWebkitAnimationDirection,
                                        CSSPropertyWebkitAnimationFillMode };
            return getLayeredShorthandValue(properties);
        }
        case CSSPropertyWebkitWrap: {
            const int properties[3] = { CSSPropertyWebkitWrapFlow, CSSPropertyWebkitWrapMargin,
                CSSPropertyWebkitWrapPadding };
            return getShorthandValue(properties);
        }
#if ENABLE(SVG)
        case CSSPropertyMarker: {
            RefPtr<CSSValue> value = getPropertyCSSValue(CSSPropertyMarkerStart);
            if (value)
                return value->cssText();
        }
#endif
    }
    return String();
}

String StylePropertySet::borderSpacingValue(const int properties[2]) const
{
    RefPtr<CSSValue> horizontalValue = getPropertyCSSValue(properties[0]);
    RefPtr<CSSValue> verticalValue = getPropertyCSSValue(properties[1]);

    if (!horizontalValue)
        return String();
    ASSERT(verticalValue); // By <http://www.w3.org/TR/CSS21/tables.html#separated-borders>.

    String horizontalValueCSSText = horizontalValue->cssText();
    String verticalValueCSSText = verticalValue->cssText();
    if (horizontalValueCSSText == verticalValueCSSText)
        return horizontalValueCSSText;
    return horizontalValueCSSText + ' ' + verticalValueCSSText;
}

bool StylePropertySet::appendFontLonghandValueIfExplicit(int propertyId, StringBuilder& result) const
{
    const CSSProperty* property = findPropertyWithId(propertyId);
    if (!property)
        return false; // All longhands must have at least implicit values if "font" is specified.
    if (property->isImplicit())
        return true;

    char prefix = '\0';
    switch (propertyId) {
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
    result.append(property->value()->cssText());

    return true;
}

String StylePropertySet::fontValue() const
{
    const CSSProperty* fontSizeProperty = findPropertyWithId(CSSPropertyFontSize);
    if (!fontSizeProperty || fontSizeProperty->isImplicit())
        return emptyString();

    StringBuilder result;
    bool success = true;
    success &= appendFontLonghandValueIfExplicit(CSSPropertyFontStyle, result);
    success &= appendFontLonghandValueIfExplicit(CSSPropertyFontVariant, result);
    success &= appendFontLonghandValueIfExplicit(CSSPropertyFontWeight, result);
    if (!result.isEmpty())
        result.append(' ');
    result.append(fontSizeProperty->value()->cssText());
    success &= appendFontLonghandValueIfExplicit(CSSPropertyLineHeight, result);
    success &= appendFontLonghandValueIfExplicit(CSSPropertyFontFamily, result);
    if (!success) {
        // An invalid "font" value has been built (should never happen, as at least implicit values
        // for mandatory longhands are always found in the style), report empty value instead.
        ASSERT_NOT_REACHED();
        return emptyString();
    }
    return result.toString();
}

String StylePropertySet::get4Values(const int* properties) const
{
    // Assume the properties are in the usual order top, right, bottom, left.
    RefPtr<CSSValue> topValue = getPropertyCSSValue(properties[0]);
    RefPtr<CSSValue> rightValue = getPropertyCSSValue(properties[1]);
    RefPtr<CSSValue> bottomValue = getPropertyCSSValue(properties[2]);
    RefPtr<CSSValue> leftValue = getPropertyCSSValue(properties[3]);

    // All 4 properties must be specified.
    if (!topValue || !rightValue || !bottomValue || !leftValue)
        return String();

    bool showLeft = rightValue->cssText() != leftValue->cssText();
    bool showBottom = (topValue->cssText() != bottomValue->cssText()) || showLeft;
    bool showRight = (topValue->cssText() != rightValue->cssText()) || showBottom;

    String res = topValue->cssText();
    if (showRight)
        res += " " + rightValue->cssText();
    if (showBottom)
        res += " " + bottomValue->cssText();
    if (showLeft)
        res += " " + leftValue->cssText();

    return res;
}

String StylePropertySet::getLayeredShorthandValue(const int* properties, size_t size) const
{
    String res;

    // Begin by collecting the properties into an array.
    Vector< RefPtr<CSSValue> > values(size);
    size_t numLayers = 0;

    for (size_t i = 0; i < size; ++i) {
        values[i] = getPropertyCSSValue(properties[i]);
        if (values[i]) {
            if (values[i]->isValueList()) {
                CSSValueList* valueList = static_cast<CSSValueList*>(values[i].get());
                numLayers = max(valueList->length(), numLayers);
            } else
                numLayers = max<size_t>(1U, numLayers);
        }
    }

    // Now stitch the properties together.  Implicit initial values are flagged as such and
    // can safely be omitted.
    for (size_t i = 0; i < numLayers; i++) {
        String layerRes;
        bool useRepeatXShorthand = false;
        bool useRepeatYShorthand = false;
        bool useSingleWordShorthand = false;
        for (size_t j = 0; j < size; j++) {
            RefPtr<CSSValue> value;
            if (values[j]) {
                if (values[j]->isValueList())
                    value = static_cast<CSSValueList*>(values[j].get())->item(i);
                else {
                    value = values[j];

                    // Color only belongs in the last layer.
                    if (properties[j] == CSSPropertyBackgroundColor) {
                        if (i != numLayers - 1)
                            value = 0;
                    } else if (i != 0) // Other singletons only belong in the first layer.
                        value = 0;
                }
            }

            // We need to report background-repeat as it was written in the CSS. If the property is implicit,
            // then it was written with only one value. Here we figure out which value that was so we can
            // report back correctly.
            if (properties[j] == CSSPropertyBackgroundRepeatX && isPropertyImplicit(properties[j])) {

                // BUG 49055: make sure the value was not reset in the layer check just above.
                if (j < size - 1 && properties[j + 1] == CSSPropertyBackgroundRepeatY && value) {
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

            if (value && !value->isImplicitInitialValue()) {
                if (!layerRes.isNull())
                    layerRes += " ";
                if (useRepeatXShorthand) {
                    useRepeatXShorthand = false;
                    layerRes += getValueName(CSSValueRepeatX);
                } else if (useRepeatYShorthand) {
                    useRepeatYShorthand = false;
                    layerRes += getValueName(CSSValueRepeatY);
                } else if (useSingleWordShorthand) {
                    useSingleWordShorthand = false;
                    layerRes += value->cssText();
                } else
                    layerRes += value->cssText();
            }
        }

        if (!layerRes.isNull()) {
            if (!res.isNull())
                res += ", ";
            res += layerRes;
        }
    }

    return res;
}

String StylePropertySet::getShorthandValue(const int* properties, size_t size) const
{
    String res;
    for (size_t i = 0; i < size; ++i) {
        if (!isPropertyImplicit(properties[i])) {
            RefPtr<CSSValue> value = getPropertyCSSValue(properties[i]);
            // FIXME: provide default value if !value
            if (value) {
                if (!res.isNull())
                    res += " ";
                res += value->cssText();
            }
        }
    }
    return res;
}

// only returns a non-null value if all properties have the same, non-null value
String StylePropertySet::getCommonValue(const int* properties, size_t size) const
{
    String res;
    for (size_t i = 0; i < size; ++i) {
        RefPtr<CSSValue> value = getPropertyCSSValue(properties[i]);
        if (!value)
            return String();
        String text = value->cssText();
        if (text.isNull())
            return String();
        if (res.isNull())
            res = text;
        else if (res != text)
            return String();
    }
    return res;
}

PassRefPtr<CSSValue> StylePropertySet::getPropertyCSSValue(int propertyID) const
{
    const CSSProperty* property = findPropertyWithId(propertyID);
    return property ? property->value() : 0;
}

bool StylePropertySet::removeShorthandProperty(int propertyID)
{
    CSSPropertyLonghand longhand = longhandForProperty(propertyID);
    if (!longhand.length())
        return false;
    return removePropertiesInSet(longhand.properties(), longhand.length());
}

bool StylePropertySet::removeProperty(int propertyID, String* returnText)
{
    if (removeShorthandProperty(propertyID)) {
        // FIXME: Return an equivalent shorthand when possible.
        if (returnText)
            *returnText = "";
        return true;
    }

    CSSProperty* foundProperty = findPropertyWithId(propertyID);
    if (!foundProperty) {
        if (returnText)
            *returnText = "";
        return false;
    }

    if (returnText)
        *returnText = foundProperty->value()->cssText();

    // A more efficient removal strategy would involve marking entries as empty
    // and sweeping them when the vector grows too big.
    m_properties.remove(foundProperty - m_properties.data());
    
    return true;
}

bool StylePropertySet::propertyIsImportant(int propertyID) const
{
    const CSSProperty* property = findPropertyWithId(propertyID);
    if (property)
        return property->isImportant();

    CSSPropertyLonghand longhands = longhandForProperty(propertyID);
    if (!longhands.length())
        return false;

    for (unsigned i = 0; i < longhands.length(); ++i) {
        if (!propertyIsImportant(longhands.properties()[i]))
            return false;
    }
    return true;
}

int StylePropertySet::getPropertyShorthand(int propertyID) const
{
    const CSSProperty* property = findPropertyWithId(propertyID);
    return property ? property->shorthandID() : 0;
}

bool StylePropertySet::isPropertyImplicit(int propertyID) const
{
    const CSSProperty* property = findPropertyWithId(propertyID);
    return property ? property->isImplicit() : false;
}

bool StylePropertySet::setProperty(int propertyID, const String& value, bool important, CSSStyleSheet* contextStyleSheet)
{
    // Setting the value to an empty string just removes the property in both IE and Gecko.
    // Setting it to null seems to produce less consistent results, but we treat it just the same.
    if (value.isEmpty()) {
        removeProperty(propertyID);
        return true;
    }

    // When replacing an existing property value, this moves the property to the end of the list.
    // Firefox preserves the position, and MSIE moves the property to the beginning.
    return CSSParser::parseValue(this, propertyID, value, important, useStrictParsing(), contextStyleSheet);
}

void StylePropertySet::setProperty(int propertyID, PassRefPtr<CSSValue> prpValue, bool important)
{
    CSSPropertyLonghand longhand = longhandForProperty(propertyID);
    if (!longhand.length()) {
        setProperty(CSSProperty(propertyID, prpValue, important));
        return;
    }

    removePropertiesInSet(longhand.properties(), longhand.length());

    RefPtr<CSSValue> value = prpValue;
    for (unsigned i = 0; i < longhand.length(); ++i)
        m_properties.append(CSSProperty(longhand.properties()[i], value, important));
}

void StylePropertySet::setProperty(const CSSProperty& property, CSSProperty* slot)
{
    if (!removeShorthandProperty(property.id())) {
        CSSProperty* toReplace = slot ? slot : findPropertyWithId(property.id());
        if (toReplace) {
            *toReplace = property;
            return;
        }
    }
    m_properties.append(property);
}

bool StylePropertySet::setProperty(int propertyID, int identifier, bool important, CSSStyleSheet* contextStyleSheet)
{
    RefPtr<CSSPrimitiveValue> value;    
    if (Document* document = contextStyleSheet ? contextStyleSheet->findDocument() : 0)
        value = document->cssValuePool()->createIdentifierValue(identifier);
    else
        value = CSSPrimitiveValue::createIdentifier(identifier);

    setProperty(CSSProperty(propertyID, value.release(), important));
    return true;
}

void StylePropertySet::parseDeclaration(const String& styleDeclaration, CSSStyleSheet* contextStyleSheet)
{
    m_properties.clear();
    CSSParser parser(useStrictParsing());
    parser.parseDeclaration(this, styleDeclaration, 0, contextStyleSheet);
}

void StylePropertySet::addParsedProperties(const CSSProperty* properties, int numProperties)
{
    m_properties.reserveCapacity(numProperties);
    for (int i = 0; i < numProperties; ++i)
        addParsedProperty(properties[i]);
}

void StylePropertySet::addParsedProperty(const CSSProperty& property)
{
    // Only add properties that have no !important counterpart present
    if (!propertyIsImportant(property.id()) || property.isImportant()) {
        removeProperty(property.id());
        m_properties.append(property);
    }
}

String StylePropertySet::asText() const
{
    String result = "";

    const CSSProperty* positionXProp = 0;
    const CSSProperty* positionYProp = 0;
    const CSSProperty* repeatXProp = 0;
    const CSSProperty* repeatYProp = 0;

    unsigned size = m_properties.size();
    for (unsigned n = 0; n < size; ++n) {
        const CSSProperty& prop = m_properties[n];
        if (prop.id() == CSSPropertyBackgroundPositionX)
            positionXProp = &prop;
        else if (prop.id() == CSSPropertyBackgroundPositionY)
            positionYProp = &prop;
        else if (prop.id() == CSSPropertyBackgroundRepeatX)
            repeatXProp = &prop;
        else if (prop.id() == CSSPropertyBackgroundRepeatY)
            repeatYProp = &prop;
        else
            result += prop.cssText();
    }

    // FIXME: This is a not-so-nice way to turn x/y positions into single background-position in output.
    // It is required because background-position-x/y are non-standard properties and WebKit generated output
    // would not work in Firefox (<rdar://problem/5143183>)
    // It would be a better solution if background-position was CSS_PAIR.
    if (positionXProp && positionYProp && positionXProp->isImportant() == positionYProp->isImportant()) {
        String positionValue;
        const int properties[2] = { CSSPropertyBackgroundPositionX, CSSPropertyBackgroundPositionY };
        if (positionXProp->value()->isValueList() || positionYProp->value()->isValueList())
            positionValue = getLayeredShorthandValue(properties);
        else
            positionValue = positionXProp->value()->cssText() + " " + positionYProp->value()->cssText();
        result += "background-position: " + positionValue + (positionXProp->isImportant() ? " !important" : "") + "; ";
    } else {
        if (positionXProp)
            result += positionXProp->cssText();
        if (positionYProp)
            result += positionYProp->cssText();
    }

    // FIXME: We need to do the same for background-repeat.
    if (repeatXProp && repeatYProp && repeatXProp->isImportant() == repeatYProp->isImportant()) {
        String repeatValue;
        const int repeatProperties[2] = { CSSPropertyBackgroundRepeatX, CSSPropertyBackgroundRepeatY };
        if (repeatXProp->value()->isValueList() || repeatYProp->value()->isValueList())
            repeatValue = getLayeredShorthandValue(repeatProperties);
        else
            repeatValue = repeatXProp->value()->cssText() + " " + repeatYProp->value()->cssText();
        result += "background-repeat: " + repeatValue + (repeatXProp->isImportant() ? " !important" : "") + "; ";
    } else {
        if (repeatXProp)
            result += repeatXProp->cssText();
        if (repeatYProp)
            result += repeatYProp->cssText();
    }

    return result;
}

void StylePropertySet::merge(const StylePropertySet* other, bool argOverridesOnConflict)
{
    unsigned size = other->m_properties.size();
    for (unsigned n = 0; n < size; ++n) {
        const CSSProperty& toMerge = other->m_properties[n];
        CSSProperty* old = findPropertyWithId(toMerge.id());
        if (old) {
            if (!argOverridesOnConflict && old->value())
                continue;
            setProperty(toMerge, old);
        } else
            m_properties.append(toMerge);
    }
}

void StylePropertySet::addSubresourceStyleURLs(ListHashSet<KURL>& urls, CSSStyleSheet* contextStyleSheet)
{
    size_t size = m_properties.size();
    for (size_t i = 0; i < size; ++i)
        m_properties[i].value()->addSubresourceStyleURLs(urls, contextStyleSheet);
}

// This is the list of properties we want to copy in the copyBlockProperties() function.
// It is the list of CSS properties that apply specially to block-level elements.
static const int blockProperties[] = {
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
    CSSPropertyWebkitRegionBreakAfter,
    CSSPropertyWebkitRegionBreakBefore,
    CSSPropertyWebkitRegionBreakInside,
    CSSPropertyTextAlign,
    CSSPropertyTextIndent,
    CSSPropertyWidows
};

const unsigned numBlockProperties = WTF_ARRAY_LENGTH(blockProperties);

PassRefPtr<StylePropertySet> StylePropertySet::copyBlockProperties() const
{
    return copyPropertiesInSet(blockProperties, numBlockProperties);
}

void StylePropertySet::removeBlockProperties()
{
    removePropertiesInSet(blockProperties, numBlockProperties);
}

bool StylePropertySet::removePropertiesInSet(const int* set, unsigned length)
{
    if (m_properties.isEmpty())
        return false;

    // FIXME: This is always used with static sets and in that case constructing the hash repeatedly is pretty pointless.
    HashSet<int> toRemove;
    for (unsigned i = 0; i < length; ++i)
        toRemove.add(set[i]);

    Vector<CSSProperty, 4> newProperties;
    newProperties.reserveInitialCapacity(m_properties.size());

    unsigned size = m_properties.size();
    for (unsigned n = 0; n < size; ++n) {
        const CSSProperty& property = m_properties[n];
        // Not quite sure if the isImportant test is needed but it matches the existing behavior.
        if (!property.isImportant()) {
            if (toRemove.contains(property.id()))
                continue;
        }
        newProperties.append(property);
    }

    bool changed = newProperties.size() != m_properties.size();
    m_properties = newProperties;
    return changed;
}

const CSSProperty* StylePropertySet::findPropertyWithId(int propertyID) const
{
    for (int n = m_properties.size() - 1 ; n >= 0; --n) {
        if (propertyID == m_properties[n].m_id)
            return &m_properties[n];
    }
    return 0;
}

CSSProperty* StylePropertySet::findPropertyWithId(int propertyID)
{
    for (int n = m_properties.size() - 1 ; n >= 0; --n) {
        if (propertyID == m_properties[n].m_id)
            return &m_properties[n];
    }
    return 0;
}
    
bool StylePropertySet::propertyMatches(const CSSProperty* property) const
{
    RefPtr<CSSValue> value = getPropertyCSSValue(property->id());
    return value && value->cssText() == property->value()->cssText();
}
    
void StylePropertySet::removeEquivalentProperties(const StylePropertySet* style)
{
    Vector<int> propertiesToRemove;
    size_t size = m_properties.size();
    for (size_t i = 0; i < size; ++i) {
        const CSSProperty& property = m_properties[i];
        if (style->propertyMatches(&property))
            propertiesToRemove.append(property.id());
    }    
    // FIXME: This should use mass removal.
    for (unsigned i = 0; i < propertiesToRemove.size(); ++i)
        removeProperty(propertiesToRemove[i]);
}

void StylePropertySet::removeEquivalentProperties(const CSSStyleDeclaration* style)
{
    Vector<int> propertiesToRemove;
    size_t size = m_properties.size();
    for (size_t i = 0; i < size; ++i) {
        const CSSProperty& property = m_properties[i];
        if (style->cssPropertyMatches(&property))
            propertiesToRemove.append(property.id());
    }    
    // FIXME: This should use mass removal.
    for (unsigned i = 0; i < propertiesToRemove.size(); ++i)
        removeProperty(propertiesToRemove[i]);
}

PassRefPtr<StylePropertySet> StylePropertySet::copy() const
{
    return adoptRef(new StylePropertySet(m_properties));
}

PassRefPtr<StylePropertySet> StylePropertySet::copyPropertiesInSet(const int* set, unsigned length) const
{
    Vector<CSSProperty> list;
    list.reserveInitialCapacity(length);
    for (unsigned i = 0; i < length; ++i) {
        RefPtr<CSSValue> value = getPropertyCSSValue(set[i]);
        if (value)
            list.append(CSSProperty(set[i], value.release(), false));
    }
    return StylePropertySet::create(list);
}

CSSStyleDeclaration* StylePropertySet::ensureCSSStyleDeclaration() const
{
    if (m_hasCSSOMWrapper) {
        ASSERT(!static_cast<CSSStyleDeclaration*>(propertySetCSSOMWrapperMap().get(this))->parentRule());
        ASSERT(!propertySetCSSOMWrapperMap().get(this)->parentElement());
        return propertySetCSSOMWrapperMap().get(this);
    }
    m_hasCSSOMWrapper = true;
    PropertySetCSSStyleDeclaration* cssomWrapper = new PropertySetCSSStyleDeclaration(const_cast<StylePropertySet*>(this));
    propertySetCSSOMWrapperMap().add(this, adoptPtr(cssomWrapper));
    return cssomWrapper;
}

CSSStyleDeclaration* StylePropertySet::ensureRuleCSSStyleDeclaration(const CSSRule* parentRule) const
{
    if (m_hasCSSOMWrapper) {
        ASSERT(static_cast<CSSStyleDeclaration*>(propertySetCSSOMWrapperMap().get(this))->parentRule() == parentRule);
        return propertySetCSSOMWrapperMap().get(this);
    }
    m_hasCSSOMWrapper = true;
    PropertySetCSSStyleDeclaration* cssomWrapper = new RuleCSSStyleDeclaration(const_cast<StylePropertySet*>(this), const_cast<CSSRule*>(parentRule));
    propertySetCSSOMWrapperMap().add(this, adoptPtr(cssomWrapper));
    return cssomWrapper;
}

CSSStyleDeclaration* StylePropertySet::ensureInlineCSSStyleDeclaration(const StyledElement* parentElement) const
{
    if (m_hasCSSOMWrapper) {
        ASSERT(propertySetCSSOMWrapperMap().get(this)->parentElement() == parentElement);
        return propertySetCSSOMWrapperMap().get(this);
    }
    m_hasCSSOMWrapper = true;
    PropertySetCSSStyleDeclaration* cssomWrapper = new InlineCSSStyleDeclaration(const_cast<StylePropertySet*>(this), const_cast<StyledElement*>(parentElement));
    propertySetCSSOMWrapperMap().add(this, adoptPtr(cssomWrapper));
    return cssomWrapper;
}

void StylePropertySet::clearParentRule(CSSRule* rule)
{
    if (!m_hasCSSOMWrapper)
        return;
    ASSERT_UNUSED(rule, static_cast<CSSStyleDeclaration*>(propertySetCSSOMWrapperMap().get(this))->parentRule() == rule);
    propertySetCSSOMWrapperMap().get(this)->clearParentRule();
}

void StylePropertySet::clearParentElement(StyledElement* element)
{
    if (!m_hasCSSOMWrapper)
        return;
    ASSERT_UNUSED(element, propertySetCSSOMWrapperMap().get(this)->parentElement() == element);
    propertySetCSSOMWrapperMap().get(this)->clearParentElement();
}

class SameSizeAsStylePropertySet : public RefCounted<SameSizeAsStylePropertySet> {
    Vector<CSSProperty, 4> properties;
    unsigned bitfield;
};
COMPILE_ASSERT(sizeof(StylePropertySet) == sizeof(SameSizeAsStylePropertySet), style_property_set_should_stay_small);

} // namespace WebCore
