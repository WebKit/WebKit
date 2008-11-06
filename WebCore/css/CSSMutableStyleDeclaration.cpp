/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "CSSMutableStyleDeclaration.h"

#include "CSSImageValue.h"
#include "CSSParser.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSStyleSheet.h"
#include "CSSValueList.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "StyledElement.h"

using namespace std;

namespace WebCore {

CSSMutableStyleDeclaration::CSSMutableStyleDeclaration()
    : m_node(0)
    , m_variableDependentValueCount(0)
{
}

CSSMutableStyleDeclaration::CSSMutableStyleDeclaration(CSSRule* parent)
    : CSSStyleDeclaration(parent)
    , m_node(0)
    , m_variableDependentValueCount(0)
{
}

CSSMutableStyleDeclaration::CSSMutableStyleDeclaration(CSSRule* parent, const DeprecatedValueList<CSSProperty>& values, unsigned variableDependentValueCount)
    : CSSStyleDeclaration(parent)
    , m_values(values)
    , m_node(0)
    , m_variableDependentValueCount(variableDependentValueCount)
{
    // FIXME: This allows duplicate properties.
}

CSSMutableStyleDeclaration::CSSMutableStyleDeclaration(CSSRule* parent, const CSSProperty* const * properties, int numProperties)
    : CSSStyleDeclaration(parent)
    , m_node(0)
    , m_variableDependentValueCount(0)
{
    for (int i = 0; i < numProperties; ++i) {
        ASSERT(properties[i]);
        m_values.append(*properties[i]);
        if (properties[i]->value()->isVariableDependentValue())
            m_variableDependentValueCount++;
    }
    // FIXME: This allows duplicate properties.
}

CSSMutableStyleDeclaration& CSSMutableStyleDeclaration::operator=(const CSSMutableStyleDeclaration& other)
{
    // don't attach it to the same node, just leave the current m_node value
    m_values = other.m_values;
    return *this;
}

String CSSMutableStyleDeclaration::getPropertyValue(int propertyID) const
{
    RefPtr<CSSValue> value = getPropertyCSSValue(propertyID);
    if (value)
        return value->cssText();

    // Shorthand and 4-values properties
    switch (propertyID) {
        case CSSPropertyBackgroundPosition: {
            // FIXME: Is this correct? The code in cssparser.cpp is confusing
            const int properties[2] = { CSSPropertyBackgroundPositionX,
                                        CSSPropertyBackgroundPositionY };
            return getLayeredShorthandValue(properties, 2);
        }
        case CSSPropertyBackground: {
            const int properties[7] = { CSSPropertyBackgroundImage, CSSPropertyBackgroundRepeat, 
                                        CSSPropertyBackgroundAttachment, CSSPropertyBackgroundPosition, CSSPropertyWebkitBackgroundClip,
                                        CSSPropertyWebkitBackgroundOrigin, CSSPropertyBackgroundColor };
            return getLayeredShorthandValue(properties, 7);
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
            const int nrprops = sizeof(properties) / sizeof(properties[0]);
            for (int i = 0; i < nrprops; ++i) {
                String value = getCommonValue(properties[i], 4);
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
            return getShorthandValue(properties, 3);
        }
        case CSSPropertyBorderRight: {
            const int properties[3] = { CSSPropertyBorderRightWidth, CSSPropertyBorderRightStyle,
                                        CSSPropertyBorderRightColor};
            return getShorthandValue(properties, 3);
        }
        case CSSPropertyBorderBottom: {
            const int properties[3] = { CSSPropertyBorderBottomWidth, CSSPropertyBorderBottomStyle,
                                        CSSPropertyBorderBottomColor};
            return getShorthandValue(properties, 3);
        }
        case CSSPropertyBorderLeft: {
            const int properties[3] = { CSSPropertyBorderLeftWidth, CSSPropertyBorderLeftStyle,
                                        CSSPropertyBorderLeftColor};
            return getShorthandValue(properties, 3);
        }
        case CSSPropertyOutline: {
            const int properties[3] = { CSSPropertyOutlineWidth, CSSPropertyOutlineStyle,
                                        CSSPropertyOutlineColor };
            return getShorthandValue(properties, 3);
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
        case CSSPropertyMargin: {
            const int properties[4] = { CSSPropertyMarginTop, CSSPropertyMarginRight,
                                        CSSPropertyMarginBottom, CSSPropertyMarginLeft };
            return get4Values(properties);
        }
        case CSSPropertyOverflow: {
            const int properties[2] = { CSSPropertyOverflowX, CSSPropertyOverflowY };
            return getCommonValue(properties, 2);
        }
        case CSSPropertyPadding: {
            const int properties[4] = { CSSPropertyPaddingTop, CSSPropertyPaddingRight,
                                        CSSPropertyPaddingBottom, CSSPropertyPaddingLeft };
            return get4Values(properties);
        }
        case CSSPropertyListStyle: {
            const int properties[3] = { CSSPropertyListStyleType, CSSPropertyListStylePosition,
                                        CSSPropertyListStyleImage };
            return getShorthandValue(properties, 3);
        }
        case CSSPropertyWebkitMaskPosition: {
            // FIXME: Is this correct? The code in cssparser.cpp is confusing
            const int properties[2] = { CSSPropertyWebkitMaskPositionX,
                                        CSSPropertyWebkitMaskPositionY };
            return getLayeredShorthandValue(properties, 2);
        }
        case CSSPropertyWebkitMask: {
            const int properties[] = { CSSPropertyWebkitMaskImage, CSSPropertyWebkitMaskRepeat, 
                                       CSSPropertyWebkitMaskAttachment, CSSPropertyWebkitMaskPosition, CSSPropertyWebkitMaskClip,
                                       CSSPropertyWebkitMaskOrigin };
            return getLayeredShorthandValue(properties, 6);
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

String CSSMutableStyleDeclaration::get4Values( const int* properties ) const
{
    String res;
    for (int i = 0; i < 4; ++i) {
        if (!isPropertyImplicit(properties[i])) {
            RefPtr<CSSValue> value = getPropertyCSSValue(properties[i]);

            // apparently all 4 properties must be specified.
            if (!value)
                return String();

            if (!res.isNull())
                res += " ";
            res += value->cssText();
        }
    }
    return res;
}

String CSSMutableStyleDeclaration::getLayeredShorthandValue(const int* properties, unsigned number) const
{
    String res;

    // Begin by collecting the properties into an array.
    Vector< RefPtr<CSSValue> > values(number);
    size_t numLayers = 0;
    
    for (size_t i = 0; i < number; ++i) {
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
        for (size_t j = 0; j < number; j++) {
            RefPtr<CSSValue> value;
            if (values[j]) {
                if (values[j]->isValueList())
                    value = static_cast<CSSValueList*>(values[j].get())->itemWithoutBoundsCheck(i);
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
            
            if (value && !value->isImplicitInitialValue()) {
                if (!layerRes.isNull())
                    layerRes += " ";
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

String CSSMutableStyleDeclaration::getShorthandValue(const int* properties, int number) const
{
    String res;
    for (int i = 0; i < number; ++i) {
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
String CSSMutableStyleDeclaration::getCommonValue(const int* properties, int number) const
{
    String res;
    for (int i = 0; i < number; ++i) {
        if (!isPropertyImplicit(properties[i])) {
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
    }
    return res;
}

PassRefPtr<CSSValue> CSSMutableStyleDeclaration::getPropertyCSSValue(int propertyID) const
{
    DeprecatedValueListConstIterator<CSSProperty> end;
    for (DeprecatedValueListConstIterator<CSSProperty> it = m_values.fromLast(); it != end; --it) {
        if (propertyID == (*it).m_id)
            return (*it).value();
    }
    return 0;
}

struct PropertyLonghand {
    PropertyLonghand()
        : m_properties(0)
        , m_length(0)
    {
    }

    PropertyLonghand(const int* firstProperty, unsigned numProperties)
        : m_properties(firstProperty)
        , m_length(numProperties)
    {
    }

    const int* properties() const { return m_properties; }
    unsigned length() const { return m_length; }

private:
    const int* m_properties;
    unsigned m_length;
};

static void initShorthandMap(HashMap<int, PropertyLonghand>& shorthandMap)
{
    #define SET_SHORTHAND_MAP_ENTRY(map, propID, array) \
        map.set(propID, PropertyLonghand(array, sizeof(array) / sizeof(array[0])))

    // FIXME: The 'font' property has "shorthand nature" but is not parsed as a shorthand.

    // Do not change the order of the following four shorthands, and keep them together.
    static const int borderProperties[4][3] = {
        { CSSPropertyBorderTopColor, CSSPropertyBorderTopStyle, CSSPropertyBorderTopWidth },
        { CSSPropertyBorderRightColor, CSSPropertyBorderRightStyle, CSSPropertyBorderRightWidth },
        { CSSPropertyBorderBottomColor, CSSPropertyBorderBottomStyle, CSSPropertyBorderBottomWidth },
        { CSSPropertyBorderLeftColor, CSSPropertyBorderLeftStyle, CSSPropertyBorderLeftWidth }
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSSPropertyBorderTop, borderProperties[0]);
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSSPropertyBorderRight, borderProperties[1]);
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSSPropertyBorderBottom, borderProperties[2]);
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSSPropertyBorderLeft, borderProperties[3]);

    shorthandMap.set(CSSPropertyBorder, PropertyLonghand(borderProperties[0], sizeof(borderProperties) / sizeof(borderProperties[0][0])));

    static const int borderColorProperties[] = {
        CSSPropertyBorderTopColor,
        CSSPropertyBorderRightColor,
        CSSPropertyBorderBottomColor,
        CSSPropertyBorderLeftColor
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSSPropertyBorderColor, borderColorProperties);

    static const int borderStyleProperties[] = {
        CSSPropertyBorderTopStyle,
        CSSPropertyBorderRightStyle,
        CSSPropertyBorderBottomStyle,
        CSSPropertyBorderLeftStyle
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSSPropertyBorderStyle, borderStyleProperties);

    static const int borderWidthProperties[] = {
        CSSPropertyBorderTopWidth,
        CSSPropertyBorderRightWidth,
        CSSPropertyBorderBottomWidth,
        CSSPropertyBorderLeftWidth
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSSPropertyBorderWidth, borderWidthProperties);

    static const int backgroundPositionProperties[] = { CSSPropertyBackgroundPositionX, CSSPropertyBackgroundPositionY };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSSPropertyBackgroundPosition, backgroundPositionProperties);

    static const int borderSpacingProperties[] = { CSSPropertyWebkitBorderHorizontalSpacing, CSSPropertyWebkitBorderVerticalSpacing };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSSPropertyBorderSpacing, borderSpacingProperties);

    static const int listStyleProperties[] = {
        CSSPropertyListStyleImage,
        CSSPropertyListStylePosition,
        CSSPropertyListStyleType
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSSPropertyListStyle, listStyleProperties);

    static const int marginProperties[] = {
        CSSPropertyMarginTop,
        CSSPropertyMarginRight,
        CSSPropertyMarginBottom,
        CSSPropertyMarginLeft
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSSPropertyMargin, marginProperties);

    static const int marginCollapseProperties[] = { CSSPropertyWebkitMarginTopCollapse, CSSPropertyWebkitMarginBottomCollapse };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSSPropertyWebkitMarginCollapse, marginCollapseProperties);

    static const int marqueeProperties[] = {
        CSSPropertyWebkitMarqueeDirection,
        CSSPropertyWebkitMarqueeIncrement,
        CSSPropertyWebkitMarqueeRepetition,
        CSSPropertyWebkitMarqueeStyle,
        CSSPropertyWebkitMarqueeSpeed
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSSPropertyWebkitMarquee, marqueeProperties);

    static const int outlineProperties[] = {
        CSSPropertyOutlineColor,
        CSSPropertyOutlineOffset,
        CSSPropertyOutlineStyle,
        CSSPropertyOutlineWidth
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSSPropertyOutline, outlineProperties);

    static const int paddingProperties[] = {
        CSSPropertyPaddingTop,
        CSSPropertyPaddingRight,
        CSSPropertyPaddingBottom,
        CSSPropertyPaddingLeft
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSSPropertyPadding, paddingProperties);

    static const int textStrokeProperties[] = { CSSPropertyWebkitTextStrokeColor, CSSPropertyWebkitTextStrokeWidth };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSSPropertyWebkitTextStroke, textStrokeProperties);

    static const int backgroundProperties[] = {
        CSSPropertyBackgroundAttachment,
        CSSPropertyWebkitBackgroundClip,
        CSSPropertyBackgroundColor,
        CSSPropertyBackgroundImage,
        CSSPropertyWebkitBackgroundOrigin,
        CSSPropertyBackgroundPositionX,
        CSSPropertyBackgroundPositionY,
        CSSPropertyBackgroundRepeat,
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSSPropertyBackground, backgroundProperties);

    static const int columnsProperties[] = { CSSPropertyWebkitColumnWidth, CSSPropertyWebkitColumnCount };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSSPropertyWebkitColumns, columnsProperties);

    static const int columnRuleProperties[] = {
        CSSPropertyWebkitColumnRuleColor,
        CSSPropertyWebkitColumnRuleStyle,
        CSSPropertyWebkitColumnRuleWidth
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSSPropertyWebkitColumnRule, columnRuleProperties);

    static const int overflowProperties[] = { CSSPropertyOverflowX, CSSPropertyOverflowY };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSSPropertyOverflow, overflowProperties);

    static const int borderRadiusProperties[] = {
        CSSPropertyWebkitBorderTopRightRadius,
        CSSPropertyWebkitBorderTopLeftRadius,
        CSSPropertyWebkitBorderBottomLeftRadius,
        CSSPropertyWebkitBorderBottomRightRadius
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSSPropertyWebkitBorderRadius, borderRadiusProperties);

    static const int maskPositionProperties[] = { CSSPropertyWebkitMaskPositionX, CSSPropertyWebkitMaskPositionY };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSSPropertyWebkitMaskPosition, maskPositionProperties);

    static const int maskProperties[] = {
        CSSPropertyWebkitMaskAttachment,
        CSSPropertyWebkitMaskClip,
        CSSPropertyWebkitMaskImage,
        CSSPropertyWebkitMaskOrigin,
        CSSPropertyWebkitMaskPositionX,
        CSSPropertyWebkitMaskPositionY,
        CSSPropertyWebkitMaskRepeat,
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSSPropertyWebkitMask, maskProperties);
    
    #undef SET_SHORTHAND_MAP_ENTRY
}

String CSSMutableStyleDeclaration::removeProperty(int propertyID, bool notifyChanged, bool returnText, ExceptionCode& ec)
{
    ec = 0;

    static HashMap<int, PropertyLonghand>& shorthandMap = *new HashMap<int, PropertyLonghand>;
    if (shorthandMap.isEmpty())
        initShorthandMap(shorthandMap);

    PropertyLonghand longhand = shorthandMap.get(propertyID);
    if (longhand.length()) {
        removePropertiesInSet(longhand.properties(), longhand.length(), notifyChanged);
        // FIXME: Return an equivalent shorthand when possible.
        return String();
    }

    String value;

    DeprecatedValueListIterator<CSSProperty> end;
    for (DeprecatedValueListIterator<CSSProperty> it = m_values.fromLast(); it != end; --it) {
        if (propertyID == (*it).m_id) {
            if (returnText)
                value = (*it).value()->cssText();
            if ((*it).value()->isVariableDependentValue())
                m_variableDependentValueCount--;
            m_values.remove(it);
            if (notifyChanged)
                setChanged();
            break;
        }
    }

    return value;
}

void CSSMutableStyleDeclaration::setChanged()
{
    if (m_node) {
        // FIXME: Ideally, this should be factored better and there
        // should be a subclass of CSSMutableStyleDeclaration just
        // for inline style declarations that handles this
        bool isInlineStyleDeclaration = m_node->isStyledElement() && this == static_cast<StyledElement*>(m_node)->inlineStyleDecl();
        if (isInlineStyleDeclaration) {
            m_node->setChanged(InlineStyleChange);
            static_cast<StyledElement*>(m_node)->invalidateStyleAttribute();
        } else
            m_node->setChanged(FullStyleChange);
        return;
    }

    // FIXME: quick&dirty hack for KDE 3.0... make this MUCH better! (Dirk)
    StyleBase* root = this;
    while (StyleBase* parent = root->parent())
        root = parent;
    if (root->isCSSStyleSheet())
        static_cast<CSSStyleSheet*>(root)->doc()->updateStyleSelector();
}

bool CSSMutableStyleDeclaration::getPropertyPriority(int propertyID) const
{
    DeprecatedValueListConstIterator<CSSProperty> end;
    for (DeprecatedValueListConstIterator<CSSProperty> it = m_values.begin(); it != end; ++it) {
        if (propertyID == (*it).id())
            return (*it).isImportant();
    }
    return false;
}

int CSSMutableStyleDeclaration::getPropertyShorthand(int propertyID) const
{
    DeprecatedValueListConstIterator<CSSProperty> end;
    for (DeprecatedValueListConstIterator<CSSProperty> it = m_values.begin(); it != end; ++it) {
        if (propertyID == (*it).id())
            return (*it).shorthandID();
    }
    return false;
}

bool CSSMutableStyleDeclaration::isPropertyImplicit(int propertyID) const
{
    DeprecatedValueListConstIterator<CSSProperty> end;
    for (DeprecatedValueListConstIterator<CSSProperty> it = m_values.begin(); it != end; ++it) {
        if (propertyID == (*it).id())
            return (*it).isImplicit();
    }
    return false;
}

void CSSMutableStyleDeclaration::setProperty(int propertyID, const String& value, bool important, ExceptionCode& ec)
{
    setProperty(propertyID, value, important, true, ec);
}

String CSSMutableStyleDeclaration::removeProperty(int propertyID, ExceptionCode& ec)
{
    return removeProperty(propertyID, true, true, ec);
}

bool CSSMutableStyleDeclaration::setProperty(int propertyID, const String& value, bool important, bool notifyChanged, ExceptionCode& ec)
{
    ec = 0;

    // Setting the value to an empty string just removes the property in both IE and Gecko.
    // Setting it to null seems to produce less consistent results, but we treat it just the same.
    if (value.isEmpty()) {
        removeProperty(propertyID, notifyChanged, false, ec);
        return ec == 0;
    }

    // When replacing an existing property value, this moves the property to the end of the list.
    // Firefox preserves the position, and MSIE moves the property to the beginning.
    CSSParser parser(useStrictParsing());
    bool success = parser.parseValue(this, propertyID, value, important);
    if (!success) {
        // CSS DOM requires raising SYNTAX_ERR here, but this is too dangerous for compatibility,
        // see <http://bugs.webkit.org/show_bug.cgi?id=7296>.
    } else if (notifyChanged)
        setChanged();
    ASSERT(!ec);
    return success;
}

bool CSSMutableStyleDeclaration::setProperty(int propertyID, int value, bool important, bool notifyChanged)
{
    removeProperty(propertyID);
    m_values.append(CSSProperty(propertyID, CSSPrimitiveValue::createIdentifier(value), important));
    if (notifyChanged)
        setChanged();
    return true;
}

void CSSMutableStyleDeclaration::setStringProperty(int propertyId, const String &value, CSSPrimitiveValue::UnitTypes type, bool important)
{
    removeProperty(propertyId);
    m_values.append(CSSProperty(propertyId, CSSPrimitiveValue::create(value, type), important));
    setChanged();
}

void CSSMutableStyleDeclaration::setImageProperty(int propertyId, const String& url, bool important)
{
    removeProperty(propertyId);
    m_values.append(CSSProperty(propertyId, CSSImageValue::create(url), important));
    setChanged();
}

void CSSMutableStyleDeclaration::parseDeclaration(const String& styleDeclaration)
{
    m_values.clear();
    CSSParser parser(useStrictParsing());
    parser.parseDeclaration(this, styleDeclaration);
    setChanged();
}

void CSSMutableStyleDeclaration::addParsedProperties(const CSSProperty* const* properties, int numProperties)
{
    for (int i = 0; i < numProperties; ++i) {
        // Only add properties that have no !important counterpart present
        if (!getPropertyPriority(properties[i]->id()) || properties[i]->isImportant()) {
            removeProperty(properties[i]->id(), false);
            ASSERT(properties[i]);
            m_values.append(*properties[i]);
            if (properties[i]->value()->isVariableDependentValue())
                m_variableDependentValueCount++;
        }
    }
    // FIXME: This probably should have a call to setChanged() if something changed. We may also wish to add
    // a notifyChanged argument to this function to follow the model of other functions in this class.
}

void CSSMutableStyleDeclaration::addParsedProperty(const CSSProperty& property)
{
    removeProperty(property.id(), false);
    m_values.append(property);
}

void CSSMutableStyleDeclaration::setLengthProperty(int propertyId, const String& value, bool important, bool /*multiLength*/)
{
    bool parseMode = useStrictParsing();
    setStrictParsing(false);
    setProperty(propertyId, value, important);
    setStrictParsing(parseMode);
}

unsigned CSSMutableStyleDeclaration::length() const
{
    return m_values.count();
}

String CSSMutableStyleDeclaration::item(unsigned i) const
{
    if (i >= m_values.count())
       return String();
    return getPropertyName(static_cast<CSSPropertyID>(m_values[i].id()));
}

String CSSMutableStyleDeclaration::cssText() const
{
    String result = "";
    
    const CSSProperty* positionXProp = 0;
    const CSSProperty* positionYProp = 0;

    DeprecatedValueListConstIterator<CSSProperty> end;
    for (DeprecatedValueListConstIterator<CSSProperty> it = m_values.begin(); it != end; ++it) {
        const CSSProperty& prop = *it;
        if (prop.id() == CSSPropertyBackgroundPositionX)
            positionXProp = &prop;
        else if (prop.id() == CSSPropertyBackgroundPositionY)
            positionYProp = &prop;
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
            positionValue = getLayeredShorthandValue(properties, 2);
        else
            positionValue = positionXProp->value()->cssText() + " " + positionYProp->value()->cssText();
        result += "background-position: " + positionValue + (positionXProp->isImportant() ? " !important" : "") + "; ";
    } else {
        if (positionXProp) 
            result += positionXProp->cssText();
        if (positionYProp)
            result += positionYProp->cssText();
    }

    return result;
}

void CSSMutableStyleDeclaration::setCssText(const String& text, ExceptionCode& ec)
{
    ec = 0;
    m_values.clear();
    CSSParser parser(useStrictParsing());
    parser.parseDeclaration(this, text);
    // FIXME: Detect syntax errors and set ec.
    setChanged();
}

void CSSMutableStyleDeclaration::merge(CSSMutableStyleDeclaration* other, bool argOverridesOnConflict)
{
    DeprecatedValueListConstIterator<CSSProperty> end;
    for (DeprecatedValueListConstIterator<CSSProperty> it = other->valuesIterator(); it != end; ++it) {
        const CSSProperty& property = *it;
        RefPtr<CSSValue> value = getPropertyCSSValue(property.id());
        if (value) {
            if (!argOverridesOnConflict)
                continue;
            removeProperty(property.id());
        }
        m_values.append(property);
    }
    // FIXME: This probably should have a call to setChanged() if something changed. We may also wish to add
    // a notifyChanged argument to this function to follow the model of other functions in this class.
}

// This is the list of properties we want to copy in the copyBlockProperties() function.
// It is the list of CSS properties that apply specially to block-level elements.
static const int blockProperties[] = {
    CSSPropertyOrphans,
    CSSPropertyOverflow, // This can be also be applied to replaced elements
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
    CSSPropertyTextAlign,
    CSSPropertyTextIndent,
    CSSPropertyWidows
};

const unsigned numBlockProperties = sizeof(blockProperties) / sizeof(blockProperties[0]);

PassRefPtr<CSSMutableStyleDeclaration> CSSMutableStyleDeclaration::copyBlockProperties() const
{
    return copyPropertiesInSet(blockProperties, numBlockProperties);
}

void CSSMutableStyleDeclaration::removeBlockProperties()
{
    removePropertiesInSet(blockProperties, numBlockProperties);
}

void CSSMutableStyleDeclaration::removePropertiesInSet(const int* set, unsigned length, bool notifyChanged)
{
    bool changed = false;
    for (unsigned i = 0; i < length; i++) {
        RefPtr<CSSValue> value = getPropertyCSSValue(set[i]);
        if (value) {
            m_values.remove(CSSProperty(set[i], value.release(), false));
            changed = true;
        }
    }
    if (changed && notifyChanged)
        setChanged();
}

PassRefPtr<CSSMutableStyleDeclaration> CSSMutableStyleDeclaration::makeMutable()
{
    return this;
}

PassRefPtr<CSSMutableStyleDeclaration> CSSMutableStyleDeclaration::copy() const
{
    return adoptRef(new CSSMutableStyleDeclaration(0, m_values, m_variableDependentValueCount));
}

} // namespace WebCore
