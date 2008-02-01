/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
{
}

CSSMutableStyleDeclaration::CSSMutableStyleDeclaration(CSSRule* parent)
    : CSSStyleDeclaration(parent)
    , m_node(0)
{
}

CSSMutableStyleDeclaration::CSSMutableStyleDeclaration(CSSRule* parent, const DeprecatedValueList<CSSProperty>& values)
    : CSSStyleDeclaration(parent)
    , m_values(values)
    , m_node(0)
{
    // FIXME: This allows duplicate properties.
}

CSSMutableStyleDeclaration::CSSMutableStyleDeclaration(CSSRule* parent, const CSSProperty* const * properties, int numProperties)
    : CSSStyleDeclaration(parent)
    , m_node(0)
{
    for (int i = 0; i < numProperties; ++i) {
        ASSERT(properties[i]);
        m_values.append(*properties[i]);
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
        case CSS_PROP_BACKGROUND_POSITION: {
            // FIXME: Is this correct? The code in cssparser.cpp is confusing
            const int properties[2] = { CSS_PROP_BACKGROUND_POSITION_X,
                                        CSS_PROP_BACKGROUND_POSITION_Y };
            return getLayeredShorthandValue(properties, 2);
        }
        case CSS_PROP_BACKGROUND: {
            const int properties[6] = { CSS_PROP_BACKGROUND_IMAGE, CSS_PROP_BACKGROUND_REPEAT,
                                        CSS_PROP_BACKGROUND_ATTACHMENT, CSS_PROP_BACKGROUND_POSITION_X,
                                        CSS_PROP_BACKGROUND_POSITION_Y, CSS_PROP_BACKGROUND_COLOR };
            return getLayeredShorthandValue(properties, 6);
        }
        case CSS_PROP_BORDER: {
            const int properties[3] = { CSS_PROP_BORDER_WIDTH, CSS_PROP_BORDER_STYLE,
                                        CSS_PROP_BORDER_COLOR };
            return getShorthandValue(properties, 3);
        }
        case CSS_PROP_BORDER_TOP: {
            const int properties[3] = { CSS_PROP_BORDER_TOP_WIDTH, CSS_PROP_BORDER_TOP_STYLE,
                                        CSS_PROP_BORDER_TOP_COLOR};
            return getShorthandValue(properties, 3);
        }
        case CSS_PROP_BORDER_RIGHT: {
            const int properties[3] = { CSS_PROP_BORDER_RIGHT_WIDTH, CSS_PROP_BORDER_RIGHT_STYLE,
                                        CSS_PROP_BORDER_RIGHT_COLOR};
            return getShorthandValue(properties, 3);
        }
        case CSS_PROP_BORDER_BOTTOM: {
            const int properties[3] = { CSS_PROP_BORDER_BOTTOM_WIDTH, CSS_PROP_BORDER_BOTTOM_STYLE,
                                        CSS_PROP_BORDER_BOTTOM_COLOR};
            return getShorthandValue(properties, 3);
        }
        case CSS_PROP_BORDER_LEFT: {
            const int properties[3] = { CSS_PROP_BORDER_LEFT_WIDTH, CSS_PROP_BORDER_LEFT_STYLE,
                                        CSS_PROP_BORDER_LEFT_COLOR};
            return getShorthandValue(properties, 3);
        }
        case CSS_PROP_OUTLINE: {
            const int properties[3] = { CSS_PROP_OUTLINE_WIDTH, CSS_PROP_OUTLINE_STYLE,
                                        CSS_PROP_OUTLINE_COLOR };
            return getShorthandValue(properties, 3);
        }
        case CSS_PROP_BORDER_COLOR: {
            const int properties[4] = { CSS_PROP_BORDER_TOP_COLOR, CSS_PROP_BORDER_RIGHT_COLOR,
                                        CSS_PROP_BORDER_BOTTOM_COLOR, CSS_PROP_BORDER_LEFT_COLOR };
            return get4Values(properties);
        }
        case CSS_PROP_BORDER_WIDTH: {
            const int properties[4] = { CSS_PROP_BORDER_TOP_WIDTH, CSS_PROP_BORDER_RIGHT_WIDTH,
                                        CSS_PROP_BORDER_BOTTOM_WIDTH, CSS_PROP_BORDER_LEFT_WIDTH };
            return get4Values(properties);
        }
        case CSS_PROP_BORDER_STYLE: {
            const int properties[4] = { CSS_PROP_BORDER_TOP_STYLE, CSS_PROP_BORDER_RIGHT_STYLE,
                                        CSS_PROP_BORDER_BOTTOM_STYLE, CSS_PROP_BORDER_LEFT_STYLE };
            return get4Values(properties);
        }
        case CSS_PROP_MARGIN: {
            const int properties[4] = { CSS_PROP_MARGIN_TOP, CSS_PROP_MARGIN_RIGHT,
                                        CSS_PROP_MARGIN_BOTTOM, CSS_PROP_MARGIN_LEFT };
            return get4Values(properties);
        }
        case CSS_PROP_PADDING: {
            const int properties[4] = { CSS_PROP_PADDING_TOP, CSS_PROP_PADDING_RIGHT,
                                        CSS_PROP_PADDING_BOTTOM, CSS_PROP_PADDING_LEFT };
            return get4Values(properties);
        }
        case CSS_PROP_LIST_STYLE: {
            const int properties[3] = { CSS_PROP_LIST_STYLE_TYPE, CSS_PROP_LIST_STYLE_POSITION,
                                        CSS_PROP_LIST_STYLE_IMAGE };
            return getShorthandValue(properties, 3);
        }
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
    unsigned i;
    unsigned j;

    // Begin by collecting the properties into an array.
    Vector< RefPtr<CSSValue> > values(number);
    unsigned numLayers = 0;
    
    for (i = 0; i < number; ++i) {
        values[i] = getPropertyCSSValue(properties[i]);
        if (values[i]) {
            if (values[i]->isValueList()) {
                CSSValueList* valueList = static_cast<CSSValueList*>(values[i].get());
                numLayers = max(valueList->length(), numLayers);
            } else
                numLayers = max(1U, numLayers);
        }
    }
    
    // Now stitch the properties together.  Implicit initial values are flagged as such and
    // can safely be omitted.
    for (i = 0; i < numLayers; i++) {
        String layerRes;
        for (j = 0; j < number; j++) {
            RefPtr<CSSValue> value;
            if (values[j]) {
                if (values[j]->isValueList())
                    value = static_cast<CSSValueList*>(values[j].get())->item(i);
                else {
                    value = values[j];
                    
                    // Color only belongs in the last layer.
                    if (properties[j] == CSS_PROP_BACKGROUND_COLOR) {
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
        { CSS_PROP_BORDER_TOP_COLOR, CSS_PROP_BORDER_TOP_STYLE, CSS_PROP_BORDER_TOP_WIDTH },
        { CSS_PROP_BORDER_RIGHT_COLOR, CSS_PROP_BORDER_RIGHT_STYLE, CSS_PROP_BORDER_RIGHT_WIDTH },
        { CSS_PROP_BORDER_BOTTOM_COLOR, CSS_PROP_BORDER_BOTTOM_STYLE, CSS_PROP_BORDER_BOTTOM_WIDTH },
        { CSS_PROP_BORDER_LEFT_COLOR, CSS_PROP_BORDER_LEFT_STYLE, CSS_PROP_BORDER_LEFT_WIDTH }
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSS_PROP_BORDER_TOP, borderProperties[0]);
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSS_PROP_BORDER_RIGHT, borderProperties[1]);
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSS_PROP_BORDER_BOTTOM, borderProperties[2]);
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSS_PROP_BORDER_LEFT, borderProperties[3]);

    shorthandMap.set(CSS_PROP_BORDER, PropertyLonghand(borderProperties[0], sizeof(borderProperties) / sizeof(borderProperties[0][0])));

    static const int borderColorProperties[] = {
        CSS_PROP_BORDER_TOP_COLOR,
        CSS_PROP_BORDER_RIGHT_COLOR,
        CSS_PROP_BORDER_BOTTOM_COLOR,
        CSS_PROP_BORDER_LEFT_COLOR
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSS_PROP_BORDER_COLOR, borderColorProperties);

    static const int borderStyleProperties[] = {
        CSS_PROP_BORDER_TOP_STYLE,
        CSS_PROP_BORDER_RIGHT_STYLE,
        CSS_PROP_BORDER_BOTTOM_STYLE,
        CSS_PROP_BORDER_LEFT_STYLE
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSS_PROP_BORDER_STYLE, borderStyleProperties);

    static const int borderWidthProperties[] = {
        CSS_PROP_BORDER_TOP_WIDTH,
        CSS_PROP_BORDER_RIGHT_WIDTH,
        CSS_PROP_BORDER_BOTTOM_WIDTH,
        CSS_PROP_BORDER_LEFT_WIDTH
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSS_PROP_BORDER_WIDTH, borderWidthProperties);

    static const int backgroundPositionProperties[] = { CSS_PROP_BACKGROUND_POSITION_X, CSS_PROP_BACKGROUND_POSITION_Y };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSS_PROP_BACKGROUND_POSITION, backgroundPositionProperties);

    static const int borderSpacingProperties[] = { CSS_PROP__WEBKIT_BORDER_HORIZONTAL_SPACING, CSS_PROP__WEBKIT_BORDER_VERTICAL_SPACING };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSS_PROP_BORDER_SPACING, borderSpacingProperties);

    static const int listStyleProperties[] = {
        CSS_PROP_LIST_STYLE_IMAGE,
        CSS_PROP_LIST_STYLE_POSITION,
        CSS_PROP_LIST_STYLE_TYPE
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSS_PROP_LIST_STYLE, listStyleProperties);

    static const int marginProperties[] = {
        CSS_PROP_MARGIN_TOP,
        CSS_PROP_MARGIN_RIGHT,
        CSS_PROP_MARGIN_BOTTOM,
        CSS_PROP_MARGIN_LEFT
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSS_PROP_MARGIN, marginProperties);

    static const int marginCollapseProperties[] = { CSS_PROP__WEBKIT_MARGIN_TOP_COLLAPSE, CSS_PROP__WEBKIT_MARGIN_BOTTOM_COLLAPSE };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSS_PROP__WEBKIT_MARGIN_COLLAPSE, marginCollapseProperties);

    static const int marqueeProperties[] = {
        CSS_PROP__WEBKIT_MARQUEE_DIRECTION,
        CSS_PROP__WEBKIT_MARQUEE_INCREMENT,
        CSS_PROP__WEBKIT_MARQUEE_REPETITION,
        CSS_PROP__WEBKIT_MARQUEE_STYLE,
        CSS_PROP__WEBKIT_MARQUEE_SPEED
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSS_PROP__WEBKIT_MARQUEE, marqueeProperties);

    static const int outlineProperties[] = {
        CSS_PROP_OUTLINE_COLOR,
        CSS_PROP_OUTLINE_OFFSET,
        CSS_PROP_OUTLINE_STYLE,
        CSS_PROP_OUTLINE_WIDTH
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSS_PROP_OUTLINE, outlineProperties);

    static const int paddingProperties[] = {
        CSS_PROP_PADDING_TOP,
        CSS_PROP_PADDING_RIGHT,
        CSS_PROP_PADDING_BOTTOM,
        CSS_PROP_PADDING_LEFT
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSS_PROP_PADDING, paddingProperties);

    static const int textStrokeProperties[] = { CSS_PROP__WEBKIT_TEXT_STROKE_COLOR, CSS_PROP__WEBKIT_TEXT_STROKE_WIDTH };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSS_PROP__WEBKIT_TEXT_STROKE, textStrokeProperties);

    static const int backgroundProperties[] = {
        CSS_PROP_BACKGROUND_ATTACHMENT,
        CSS_PROP_BACKGROUND_COLOR,
        CSS_PROP_BACKGROUND_IMAGE,
        CSS_PROP_BACKGROUND_POSITION_X,
        CSS_PROP_BACKGROUND_POSITION_Y,
        CSS_PROP_BACKGROUND_REPEAT,
        CSS_PROP__WEBKIT_BACKGROUND_SIZE
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSS_PROP_BACKGROUND, backgroundProperties);

    static const int columnsProperties[] = { CSS_PROP__WEBKIT_COLUMN_WIDTH, CSS_PROP__WEBKIT_COLUMN_COUNT };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSS_PROP__WEBKIT_COLUMNS, columnsProperties);

    static const int columnRuleProperties[] = {
        CSS_PROP__WEBKIT_COLUMN_RULE_COLOR,
        CSS_PROP__WEBKIT_COLUMN_RULE_STYLE,
        CSS_PROP__WEBKIT_COLUMN_RULE_WIDTH
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSS_PROP__WEBKIT_COLUMN_RULE, columnRuleProperties);

    static const int overflowProperties[] = { CSS_PROP_OVERFLOW_X, CSS_PROP_OVERFLOW_Y };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSS_PROP_OVERFLOW, overflowProperties);

    static const int borderRadiusProperties[] = {
        CSS_PROP__WEBKIT_BORDER_TOP_RIGHT_RADIUS,
        CSS_PROP__WEBKIT_BORDER_TOP_LEFT_RADIUS,
        CSS_PROP__WEBKIT_BORDER_BOTTOM_LEFT_RADIUS,
        CSS_PROP__WEBKIT_BORDER_BOTTOM_RIGHT_RADIUS
    };
    SET_SHORTHAND_MAP_ENTRY(shorthandMap, CSS_PROP__WEBKIT_BORDER_RADIUS, borderRadiusProperties);

    #undef SET_SHORTHAND_MAP_ENTRY
}

String CSSMutableStyleDeclaration::removeProperty(int propertyID, bool notifyChanged, bool returnText, ExceptionCode& ec)
{
    ec = 0;

    static HashMap<int, PropertyLonghand> shorthandMap;
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
            m_values.remove(it);
            if (notifyChanged)
                setChanged();
            break;
        }
    }

    return value;
}

void CSSMutableStyleDeclaration::clear()
{
    m_values.clear();
    setChanged();
}

void CSSMutableStyleDeclaration::setChanged(StyleChangeType changeType)
{
    if (m_node) {
        m_node->setChanged(changeType);
        // FIXME: Ideally, this should be factored better and there
        // should be a subclass of CSSMutableStyleDeclaration just
        // for inline style declarations that handles this
        if (m_node->isStyledElement() && this == static_cast<StyledElement*>(m_node)->inlineStyleDecl())
            static_cast<StyledElement*>(m_node)->invalidateStyleAttribute();
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
        setChanged(InlineStyleChange);
    ASSERT(!ec);
    return success;
}

bool CSSMutableStyleDeclaration::setProperty(int propertyID, int value, bool important, bool notifyChanged)
{
    removeProperty(propertyID);
    m_values.append(CSSProperty(propertyID, new CSSPrimitiveValue(value), important));
    if (notifyChanged)
        setChanged();
    return true;
}

void CSSMutableStyleDeclaration::setStringProperty(int propertyId, const String &value, CSSPrimitiveValue::UnitTypes type, bool important)
{
    removeProperty(propertyId);
    m_values.append(CSSProperty(propertyId, new CSSPrimitiveValue(value, type), important));
    setChanged();
}

void CSSMutableStyleDeclaration::setImageProperty(int propertyId, const String& url, bool important)
{
    removeProperty(propertyId);
    m_values.append(CSSProperty(propertyId, new CSSImageValue(url, this), important));
    setChanged();
}

void CSSMutableStyleDeclaration::parseDeclaration(const String& styleDeclaration)
{
    m_values.clear();
    CSSParser parser(useStrictParsing());
    parser.parseDeclaration(this, styleDeclaration);
    setChanged();
}

void CSSMutableStyleDeclaration::addParsedProperties(const CSSProperty * const * properties, int numProperties)
{
    for (int i = 0; i < numProperties; ++i) {
        // Only add properties that have no !important counterpart present
        if (!getPropertyPriority(properties[i]->id()) || properties[i]->isImportant()) {
            removeProperty(properties[i]->id(), false);
            ASSERT(properties[i]);
            m_values.append(*properties[i]);
        }
    }
    // FIXME: This probably should have a call to setChanged() if something changed. We may also wish to add
    // a notifyChanged argument to this function to follow the model of other functions in this class.
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
        if (prop.id() == CSS_PROP_BACKGROUND_POSITION_X)
            positionXProp = &prop;
        else if (prop.id() == CSS_PROP_BACKGROUND_POSITION_Y)
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
        const int properties[2] = { CSS_PROP_BACKGROUND_POSITION_X, CSS_PROP_BACKGROUND_POSITION_Y };
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
    CSS_PROP_ORPHANS,
    CSS_PROP_OVERFLOW, // This can be also be applied to replaced elements
    CSS_PROP__WEBKIT_COLUMN_COUNT,
    CSS_PROP__WEBKIT_COLUMN_GAP,
    CSS_PROP__WEBKIT_COLUMN_RULE_COLOR,
    CSS_PROP__WEBKIT_COLUMN_RULE_STYLE,
    CSS_PROP__WEBKIT_COLUMN_RULE_WIDTH,
    CSS_PROP__WEBKIT_COLUMN_BREAK_BEFORE,
    CSS_PROP__WEBKIT_COLUMN_BREAK_AFTER,
    CSS_PROP__WEBKIT_COLUMN_BREAK_INSIDE,
    CSS_PROP__WEBKIT_COLUMN_WIDTH,
    CSS_PROP_PAGE_BREAK_AFTER,
    CSS_PROP_PAGE_BREAK_BEFORE,
    CSS_PROP_PAGE_BREAK_INSIDE,
    CSS_PROP_TEXT_ALIGN,
    CSS_PROP_TEXT_INDENT,
    CSS_PROP_WIDOWS
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
    return new CSSMutableStyleDeclaration(0, m_values);
}

} // namespace WebCore
