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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "config.h"
#include "CSSMutableStyleDeclaration.h"

#include "CSSImageValue.h"
#include "cssparser.h"
#include "CSSPropertyNames.h"
#include "CSSProperty.h"
#include "CSSStyleSheet.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Node.h"
#include "StyledElement.h"

// Not in any header, so just declare it here for now.
WebCore::String getPropertyName(unsigned short id);

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

CSSMutableStyleDeclaration::CSSMutableStyleDeclaration(CSSRule* parent, const CSSProperty * const *properties, int numProperties)
    : CSSStyleDeclaration(parent)
    , m_node(0)
{
    for (int i = 0; i < numProperties; ++i)
        m_values.append(*properties[i]);
    // FIXME: This allows duplicate properties.
}

CSSMutableStyleDeclaration& CSSMutableStyleDeclaration::operator=(const CSSMutableStyleDeclaration& o)
{
    // don't attach it to the same node, just leave the current m_node value
    m_values = o.m_values;
    return *this;
}

String CSSMutableStyleDeclaration::getPropertyValue(int propertyID) const
{
    RefPtr<CSSValue> value = getPropertyCSSValue(propertyID);
    if (value)
        return value->cssText();

    // Shorthand and 4-values properties
    switch ( propertyID ) {
    case CSS_PROP_BACKGROUND_POSITION:
    {
        // ## Is this correct? The code in cssparser.cpp is confusing
        const int properties[2] = { CSS_PROP_BACKGROUND_POSITION_X,
                                    CSS_PROP_BACKGROUND_POSITION_Y };
        return getShortHandValue( properties, 2 );
    }
    case CSS_PROP_BACKGROUND:
    {
        const int properties[5] = { CSS_PROP_BACKGROUND_IMAGE, CSS_PROP_BACKGROUND_REPEAT,
                                    CSS_PROP_BACKGROUND_ATTACHMENT, CSS_PROP_BACKGROUND_POSITION,
                                    CSS_PROP_BACKGROUND_COLOR };
        return getShortHandValue( properties, 5 );
    }
    case CSS_PROP_BORDER:
    {
        const int properties[3] = { CSS_PROP_BORDER_WIDTH, CSS_PROP_BORDER_STYLE,
                                    CSS_PROP_BORDER_COLOR };
        return getShortHandValue( properties, 3 );
    }
    case CSS_PROP_BORDER_TOP:
    {
        const int properties[3] = { CSS_PROP_BORDER_TOP_WIDTH, CSS_PROP_BORDER_TOP_STYLE,
                                    CSS_PROP_BORDER_TOP_COLOR};
        return getShortHandValue( properties, 3 );
    }
    case CSS_PROP_BORDER_RIGHT:
    {
        const int properties[3] = { CSS_PROP_BORDER_RIGHT_WIDTH, CSS_PROP_BORDER_RIGHT_STYLE,
                                    CSS_PROP_BORDER_RIGHT_COLOR};
        return getShortHandValue( properties, 3 );
    }
    case CSS_PROP_BORDER_BOTTOM:
    {
        const int properties[3] = { CSS_PROP_BORDER_BOTTOM_WIDTH, CSS_PROP_BORDER_BOTTOM_STYLE,
                                    CSS_PROP_BORDER_BOTTOM_COLOR};
        return getShortHandValue( properties, 3 );
    }
    case CSS_PROP_BORDER_LEFT:
    {
        const int properties[3] = { CSS_PROP_BORDER_LEFT_WIDTH, CSS_PROP_BORDER_LEFT_STYLE,
                                    CSS_PROP_BORDER_LEFT_COLOR};
        return getShortHandValue( properties, 3 );
    }
    case CSS_PROP_OUTLINE:
    {
        const int properties[3] = { CSS_PROP_OUTLINE_WIDTH, CSS_PROP_OUTLINE_STYLE,
                                    CSS_PROP_OUTLINE_COLOR };
        return getShortHandValue( properties, 3 );
    }
    case CSS_PROP_BORDER_COLOR:
    {
        const int properties[4] = { CSS_PROP_BORDER_TOP_COLOR, CSS_PROP_BORDER_RIGHT_COLOR,
                                    CSS_PROP_BORDER_BOTTOM_COLOR, CSS_PROP_BORDER_LEFT_COLOR };
        return get4Values( properties );
    }
    case CSS_PROP_BORDER_WIDTH:
    {
        const int properties[4] = { CSS_PROP_BORDER_TOP_WIDTH, CSS_PROP_BORDER_RIGHT_WIDTH,
                                    CSS_PROP_BORDER_BOTTOM_WIDTH, CSS_PROP_BORDER_LEFT_WIDTH };
        return get4Values( properties );
    }
    case CSS_PROP_BORDER_STYLE:
    {
        const int properties[4] = { CSS_PROP_BORDER_TOP_STYLE, CSS_PROP_BORDER_RIGHT_STYLE,
                                    CSS_PROP_BORDER_BOTTOM_STYLE, CSS_PROP_BORDER_LEFT_STYLE };
        return get4Values( properties );
    }
    case CSS_PROP_MARGIN:
    {
        const int properties[4] = { CSS_PROP_MARGIN_TOP, CSS_PROP_MARGIN_RIGHT,
                                    CSS_PROP_MARGIN_BOTTOM, CSS_PROP_MARGIN_LEFT };
        return get4Values( properties );
    }
    case CSS_PROP_PADDING:
    {
        const int properties[4] = { CSS_PROP_PADDING_TOP, CSS_PROP_PADDING_RIGHT,
                                    CSS_PROP_PADDING_BOTTOM, CSS_PROP_PADDING_LEFT };
        return get4Values( properties );
    }
    case CSS_PROP_LIST_STYLE:
    {
        const int properties[3] = { CSS_PROP_LIST_STYLE_TYPE, CSS_PROP_LIST_STYLE_POSITION,
                                    CSS_PROP_LIST_STYLE_IMAGE };
        return getShortHandValue( properties, 3 );
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
            if (!value) // apparently all 4 properties must be specified.
                return String();
            if (!res.isNull())
                res += " ";
            res += value->cssText();
        }
    }
    return res;
}

String CSSMutableStyleDeclaration::getShortHandValue( const int* properties, int number ) const
{
    String res;
    for (int i = 0; i < number; ++i) {
        if (!isPropertyImplicit(properties[i])) {
            RefPtr<CSSValue> value = getPropertyCSSValue(properties[i]);
            if (value) { // TODO provide default value if !value
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
    for (DeprecatedValueListConstIterator<CSSProperty> it = m_values.fromLast(); it != end; --it)
        if (propertyID == (*it).m_id)
            return (*it).value();
    return 0;
}

String CSSMutableStyleDeclaration::removeProperty(int propertyID, bool notifyChanged, ExceptionCode& ec)
{
    ec = 0;

    String value;

    DeprecatedValueListIterator<CSSProperty> end;
    for (DeprecatedValueListIterator<CSSProperty> it = m_values.fromLast(); it != end; --it)
        if (propertyID == (*it).m_id) {
            value = (*it).value()->cssText();
            m_values.remove(it);
            if (notifyChanged)
                setChanged();
            break;
        }

    return value;
}

void CSSMutableStyleDeclaration::clear()
{
    m_values.clear();
    setChanged();
}

void CSSMutableStyleDeclaration::setChanged()
{
    if (m_node) {
        m_node->setChanged();
        // FIXME: Ideally, this should be factored better and there
        // should be a subclass of CSSMutableStyleDeclaration just
        // for inline style declarations that handles this
        if (m_node->isStyledElement() && this == static_cast<StyledElement*>(m_node)->inlineStyleDecl())
            static_cast<StyledElement*>(m_node)->invalidateStyleAttribute();
        return;
    }

    // ### quick&dirty hack for KDE 3.0... make this MUCH better! (Dirk)
    StyleBase* root = this;
    while (StyleBase *parent = root->parent())
        root = parent;
    if (root->isCSSStyleSheet())
        static_cast<CSSStyleSheet*>(root)->doc()->updateStyleSelector();
}

bool CSSMutableStyleDeclaration::getPropertyPriority(int propertyID) const
{
    DeprecatedValueListConstIterator<CSSProperty> end;
    for (DeprecatedValueListConstIterator<CSSProperty> it = m_values.begin(); it != end; ++it)
        if (propertyID == (*it).id())
            return (*it).isImportant();
    return false;
}

int CSSMutableStyleDeclaration::getPropertyShorthand(int propertyID) const
{
    DeprecatedValueListConstIterator<CSSProperty> end;
    for (DeprecatedValueListConstIterator<CSSProperty> it = m_values.begin(); it != end; ++it)
        if (propertyID == (*it).id())
            return (*it).shorthandID();
    return false;
}

bool CSSMutableStyleDeclaration::isPropertyImplicit(int propertyID) const
{
    DeprecatedValueListConstIterator<CSSProperty> end;
    for (DeprecatedValueListConstIterator<CSSProperty> it = m_values.begin(); it != end; ++it)
        if (propertyID == (*it).id())
            return (*it).isImplicit();
    return false;
}

void CSSMutableStyleDeclaration::setProperty(int propertyID, const String &value, bool important, ExceptionCode& ec)
{
    setProperty(propertyID, value, important, true, ec);
}

String CSSMutableStyleDeclaration::removeProperty(int propertyID, ExceptionCode& ec)
{
    return removeProperty(propertyID, true, ec);
}

bool CSSMutableStyleDeclaration::setProperty(int propertyID, const String &value, bool important, bool notifyChanged, ExceptionCode& ec)
{
    ec = 0;

    removeProperty(propertyID);

    CSSParser parser(useStrictParsing());
    bool success = parser.parseValue(this, propertyID, value, important);
    if (!success) {
        ec = SYNTAX_ERR;
    } else if (notifyChanged)
        setChanged();
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

void CSSMutableStyleDeclaration::setImageProperty(int propertyId, const String &URL, bool important)
{
    removeProperty(propertyId);
    m_values.append(CSSProperty(propertyId, new CSSImageValue(URL, this), important));
    setChanged();
}

void CSSMutableStyleDeclaration::parseDeclaration(const String &styleDeclaration)
{
    m_values.clear();
    CSSParser parser(useStrictParsing());
    parser.parseDeclaration(this, styleDeclaration);
    setChanged();
}

void CSSMutableStyleDeclaration::addParsedProperties(const CSSProperty * const *properties, int numProperties)
{
    for (int i = 0; i < numProperties; ++i) {
        removeProperty(properties[i]->id(), false);
        m_values.append(*properties[i]);
    }
    // FIXME: This probably should have a call to setChanged() if something changed. We may also wish to add
    // a notifyChanged argument to this function to follow the model of other functions in this class.
}

void CSSMutableStyleDeclaration::setLengthProperty(int id, const String &value, bool important, bool /* multiLength*/)
{
    bool parseMode = useStrictParsing();
    setStrictParsing(false);
    setProperty(id, value, important);
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
    return getPropertyName(m_values[i].id());
}

String CSSMutableStyleDeclaration::cssText() const
{
    String result = "";
    
    DeprecatedValueListConstIterator<CSSProperty> end;
    for (DeprecatedValueListConstIterator<CSSProperty> it = m_values.begin(); it != end; ++it)
        result += (*it).cssText();

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

void CSSMutableStyleDeclaration::merge(CSSMutableStyleDeclaration *other, bool argOverridesOnConflict)
{
    DeprecatedValueListConstIterator<CSSProperty> end;
    for (DeprecatedValueListConstIterator<CSSProperty> it = other->valuesIterator(); it != end; ++it) {
        const CSSProperty &property = *it;
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

// This is the list of properties we want to copy in the copyInheritableProperties() function.
// It is the intersection of the list of inherited CSS properties and the
// properties for which we have a computed implementation in this file.
const int inheritableProperties[] = {
    CSS_PROP_BORDER_COLLAPSE,
    CSS_PROP_BORDER_SPACING,
    CSS_PROP_COLOR,
    CSS_PROP_FONT_FAMILY,
    CSS_PROP_FONT_SIZE,
    CSS_PROP_FONT_STYLE,
    CSS_PROP_FONT_VARIANT,
    CSS_PROP_FONT_WEIGHT,
    CSS_PROP_LETTER_SPACING,
    CSS_PROP_LINE_HEIGHT,
    CSS_PROP_TEXT_ALIGN,
    CSS_PROP__WEBKIT_TEXT_DECORATIONS_IN_EFFECT,
    CSS_PROP_TEXT_INDENT,
    CSS_PROP__WEBKIT_TEXT_SIZE_ADJUST,
    CSS_PROP_TEXT_TRANSFORM,
    CSS_PROP_ORPHANS,
    CSS_PROP_WHITE_SPACE,
    CSS_PROP_WIDOWS,
    CSS_PROP_WORD_SPACING,
};

const unsigned numInheritableProperties = sizeof(inheritableProperties) / sizeof(inheritableProperties[0]);

// This is the list of properties we want to copy in the copyBlockProperties() function.
// It is the list of CSS properties that apply specially to block-level elements.
static const int blockProperties[] = {
    CSS_PROP_ORPHANS,
    CSS_PROP_OVERFLOW, // This can be also be applied to replaced elements
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

void CSSMutableStyleDeclaration::removeInheritableProperties()
{
    removePropertiesInSet(inheritableProperties, numInheritableProperties);
}

void CSSMutableStyleDeclaration::removePropertiesInSet(const int *set, unsigned length)
{
    bool changed = false;
    for (unsigned i = 0; i < length; i++) {
        RefPtr<CSSValue> value = getPropertyCSSValue(set[i]);
        if (value) {
            m_values.remove(CSSProperty(set[i], value.release(), false));
            changed = true;
        }
    }
    if (changed)
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

}
