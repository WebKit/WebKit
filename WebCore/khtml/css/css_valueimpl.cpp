/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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

#include "css/css_valueimpl.h"

#include "dom/css_value.h"
#include "dom/css_stylesheet.h"
#include "dom/dom_exception.h"
#include "dom/dom_string.h"

#include "css/css_ruleimpl.h"
#include "css/css_stylesheetimpl.h"
#include "css/cssparser.h"
#include "css/cssproperties.h"
#include "css/cssvalues.h"
#include "css/cssstyleselector.h"

#include "xml/dom_stringimpl.h"
#include "xml/dom_docimpl.h"
#include "html/html_elementimpl.h"

#include "misc/loader.h"

#include "rendering/font.h"
#include "rendering/render_style.h"

#include <kdebug.h>
#include <qregexp.h>
#include <qpaintdevice.h>
#include <qpaintdevicemetrics.h>

// Hack for debugging purposes
extern DOM::DOMString getPropertyName(unsigned short id);

using khtml::FontDef;
using khtml::CSSStyleSelector;
using khtml::CachedImage;

namespace DOM {

// Defined in parser.y, but not in any header, so just declare it here.
int getPropertyID(const char *str, int len);

static int propertyID(const DOMString &s)
{
    char buffer[maxCSSPropertyNameLength];

    unsigned len = s.length();
    if (len > maxCSSPropertyNameLength)
        return 0;

    for (unsigned i = 0; i != len; ++i) {
        unsigned short c = s[i].unicode();
        if (c == 0 || c >= 0x7F)
            return 0; // illegal character
        buffer[i] = c;
    }

    return getPropertyID(buffer, len);
}

#if 0

// Too risky to quote all legal identifiers right now.
// Post-Tiger we should use this function or something like it.

// Return true if this string qualifies as an identifier (from the point of view of CSS syntax).
static bool isLegalIdentifier(const DOMString &string)
{
    int len = string.length();
    if (len == 0) {
        return false;
    }
    QChar *p = string.unicode();
    int i = 0;
    if (p[0] == '-') {
        ++i;
    }
    if (i == len) {
        return false;
    }
    ushort code = p[i].unicode();
    if (!(code >= 0x80 || code == '_' || isalpha(code))) {
        return false;
    }
    ++i;
    while (i != len) {
        code = p[i].unicode();
        if (!(code >= 0x80 || code == '-' || code == '_' || isalnum(code))) {
            return false;
        }
        ++i;
    }
    return true;
}

#endif

// Quotes the string if it needs quoting.
// We use single quotes for now beause markup.cpp uses double quotes.
static DOMString quoteStringIfNeeded(const DOMString &string)
{
    // For now, just do this for strings that start with "#" to fix Korean font names that start with "#".
    // Post-Tiger, we should isLegalIdentifier instead after working out all the ancillary issues.
    if (string[0] != '#') {
        return string;
    }

    // FIXME: Also need to transform control characters into \ sequences.
    QString s = string.qstring();
    s.replace('\\', "\\\\");
    s.replace('\'', "\\'");
    return '\'' + s + '\'';
}

CSSStyleDeclarationImpl::CSSStyleDeclarationImpl(CSSRuleImpl *parent)
    : StyleBaseImpl(parent)
{
}

bool CSSStyleDeclarationImpl::isStyleDeclaration()
{
    return true;
}

CSSValueImpl *CSSStyleDeclarationImpl::getPropertyCSSValue(const DOMString &propertyName)
{
    int propID = propertyID(propertyName);
    if (!propID)
        return 0;
    return getPropertyCSSValue(propID);
}

DOMString CSSStyleDeclarationImpl::getPropertyValue(const DOMString &propertyName)
{
    int propID = propertyID(propertyName);
    if (!propID)
        return DOMString();
    return getPropertyValue(propID);
}

DOMString CSSStyleDeclarationImpl::getPropertyPriority(const DOMString &propertyName)
{
    int propID = propertyID(propertyName);
    if (!propID)
        return DOMString();
    return getPropertyPriority(propID) ? "important" : "";
}

void CSSStyleDeclarationImpl::setProperty(const DOMString &propertyName, const DOMString &value, const DOMString &priority, int &exception)
{
    int propID = propertyID(propertyName);
    if (!propID) // set exception?
        return;
    bool important = priority.qstring().find("important", 0, false) != -1;
    setProperty(propID, value, important, exception);
}

DOMString CSSStyleDeclarationImpl::removeProperty(const DOMString &propertyName, int &exception)
{
    int propID = propertyID(propertyName);
    if (!propID)
        return DOMString();
    return removeProperty(propID, exception);
}

bool CSSStyleDeclarationImpl::isPropertyName(const DOMString &propertyName)
{
    return propertyID(propertyName);
}

// --------------------------------------------------------------------------------------

CSSMutableStyleDeclarationImpl::CSSMutableStyleDeclarationImpl()
    : m_node(0)
{
}

CSSMutableStyleDeclarationImpl::CSSMutableStyleDeclarationImpl(CSSRuleImpl *parent)
    : CSSStyleDeclarationImpl(parent), m_node(0)
{
}

CSSMutableStyleDeclarationImpl::CSSMutableStyleDeclarationImpl(CSSRuleImpl *parent, const QValueList<CSSProperty> &values)
    : CSSStyleDeclarationImpl(parent), m_values(values), m_node(0)
{
    // FIXME: This allows duplicate properties.
}

CSSMutableStyleDeclarationImpl::CSSMutableStyleDeclarationImpl(CSSRuleImpl *parent, const CSSProperty * const *properties, int numProperties)
    : CSSStyleDeclarationImpl(parent), m_node(0)
{
    for (int i = 0; i < numProperties; ++i)
        m_values.append(*properties[i]);
    // FIXME: This allows duplicate properties.
}

CSSMutableStyleDeclarationImpl& CSSMutableStyleDeclarationImpl::operator=(const CSSMutableStyleDeclarationImpl& o)
{
    // don't attach it to the same node, just leave the current m_node value
    m_values = o.m_values;
    return *this;
}

CSSMutableStyleDeclarationImpl::~CSSMutableStyleDeclarationImpl()
{
    // we don't use refcounting for m_node, to avoid cyclic references (see ElementImpl)
}

DOMString CSSMutableStyleDeclarationImpl::getPropertyValue( int propertyID ) const
{
    CSSValueImpl* value = getPropertyCSSValue( propertyID );
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
    //kdDebug() << k_funcinfo << "property not found:" << propertyID << endl;
    return DOMString();
}

DOMString CSSMutableStyleDeclarationImpl::get4Values( const int* properties ) const
{
    DOMString res;
    for ( int i = 0 ; i < 4 ; ++i ) {
        CSSValueImpl* value = getPropertyCSSValue( properties[i] );
        if ( !value ) { // apparently all 4 properties must be specified.
            return DOMString();
        }
        value->ref();
        if ( i > 0 )
            res += " ";
        res += value->cssText();
        value->deref();
    }
    return res;
}

DOMString CSSMutableStyleDeclarationImpl::getShortHandValue( const int* properties, int number ) const
{
    DOMString res;
    for ( int i = 0 ; i < number ; ++i ) {
        CSSValueImpl* value = getPropertyCSSValue( properties[i] );
        if ( value ) { // TODO provide default value if !value
            value->ref();
            if ( !res.isNull() )
                res += " ";
            res += value->cssText();
            value->deref();
        }
    }
    return res;
}

 CSSValueImpl *CSSMutableStyleDeclarationImpl::getPropertyCSSValue( int propertyID ) const
{
    QValueListConstIterator<CSSProperty> end;
    for (QValueListConstIterator<CSSProperty> it = m_values.fromLast(); it != end; --it)
        if (propertyID == (*it).m_id)
            return (*it).value();
    return 0;
}

DOMString CSSMutableStyleDeclarationImpl::removeProperty(int propertyID, bool notifyChanged, int &exceptionCode)
{
    if (m_node && !m_node->getDocument())
        return ""; // FIXME: This (not well-understood) situation happens on albertsons.com.  We don't really know how they managed to run a script on a node
                   // with no document pointer, but this sidesteps the crash.

    exceptionCode = 0;

    DOMString value;

    QValueListIterator<CSSProperty> end;
    for (QValueListIterator<CSSProperty> it = m_values.fromLast(); it != end; --it)
        if (propertyID == (*it).m_id) {
            value = (*it).value()->cssText();
            m_values.remove(it);
            if (notifyChanged)
                setChanged();
            break;
        }

    return value;
}

void CSSMutableStyleDeclarationImpl::clear()
{
    m_values.clear();
    setChanged();
}

void CSSMutableStyleDeclarationImpl::setChanged()
{
    if (m_node) {
        m_node->setChanged();
        // FIXME: Ideally, this should be factored better and there
        // should be a subclass of CSSMutableStyleDeclarationImpl just
        // for inline style declarations that handles this
        if (m_node->isHTMLElement() && this == static_cast<HTMLElementImpl *>(m_node)->inlineStyleDecl())
            static_cast<HTMLElementImpl *>(m_node)->invalidateStyleAttribute();
        return;
    }

    // ### quick&dirty hack for KDE 3.0... make this MUCH better! (Dirk)
    StyleBaseImpl *root = this;
    while (StyleBaseImpl *parent = root->parent())
        root = parent;
    if (root->isCSSStyleSheet())
        static_cast<CSSStyleSheetImpl*>(root)->doc()->updateStyleSelector();
}

bool CSSMutableStyleDeclarationImpl::getPropertyPriority(int propertyID) const
{
    QValueListConstIterator<CSSProperty> end;
    for (QValueListConstIterator<CSSProperty> it = m_values.begin(); it != end; ++it)
        if (propertyID == (*it).m_id)
            return (*it).m_bImportant;
    return false;
}

void CSSMutableStyleDeclarationImpl::setProperty(int propertyID, const DOMString &value, bool important, int &exceptionCode)
{
    setProperty(propertyID, value, important, true, exceptionCode);
}

DOMString CSSMutableStyleDeclarationImpl::removeProperty(int propertyID, int &exceptionCode)
{
    return removeProperty(propertyID, true, exceptionCode);
}

bool CSSMutableStyleDeclarationImpl::setProperty(int propertyID, const DOMString &value, bool important, bool notifyChanged, int &exceptionCode)
{
    if (m_node && !m_node->getDocument())
        return false; // FIXME: This (not well-understood) situation happens on albertsons.com.  We don't really know how they managed to run a script on a node
                      // with no document pointer, but this sidesteps the crash.
    exceptionCode = 0;

    removeProperty(propertyID);

    CSSParser parser(strictParsing);
    bool success = parser.parseValue(this, propertyID, value, important);
    if (!success) {
#if !APPLE_CHANGES
	kdDebug( 6080 ) << "CSSMutableStyleDeclarationImpl::setProperty invalid property: [" << getPropertyName(id).qstring()
			<< "] value: [" << value.qstring() << "]"<< endl;
#endif
        exceptionCode = CSSException::SYNTAX_ERR + CSSException::_EXCEPTION_OFFSET;
    } else if (notifyChanged)
        setChanged();
    return success;
}

bool CSSMutableStyleDeclarationImpl::setProperty(int propertyID, int value, bool important, bool notifyChanged)
{
    removeProperty(propertyID);
    m_values.append(CSSProperty(propertyID, new CSSPrimitiveValueImpl(value), important));
    if (notifyChanged)
        setChanged();
    return true;
}

void CSSMutableStyleDeclarationImpl::setStringProperty(int propertyId, const DOMString &value, CSSPrimitiveValue::UnitTypes type, bool important)
{
    removeProperty(propertyId);
    m_values.append(CSSProperty(propertyId, new CSSPrimitiveValueImpl(value, type), important));
    setChanged();
}

void CSSMutableStyleDeclarationImpl::setImageProperty(int propertyId, const DOMString &URL, bool important)
{
    removeProperty(propertyId);
    m_values.append(CSSProperty(propertyId, new CSSImageValueImpl(URL, this), important));
    setChanged();
}

void CSSMutableStyleDeclarationImpl::parseDeclaration(const DOMString &styleDeclaration)
{
    m_values.clear();
    CSSParser parser(strictParsing);
    parser.parseDeclaration(this, styleDeclaration);
    setChanged();
}

void CSSMutableStyleDeclarationImpl::addParsedProperties(const CSSProperty * const *properties, int numProperties)
{
    for (int i = 0; i < numProperties; ++i) {
        removeProperty(properties[i]->id(), false);
        m_values.append(*properties[i]);
    }
    // FIXME: This probably should have a call to setChanged() if something changed. We may also wish to add
    // a notifyChanged argument to this function to follow the model of other functions in this class.
}

void CSSMutableStyleDeclarationImpl::setLengthProperty(int id, const DOM::DOMString &value, bool important, bool _multiLength )
{
    bool parseMode = strictParsing;
    strictParsing = false;
    multiLength = _multiLength;
    setProperty( id, value, important);
    strictParsing = parseMode;
    multiLength = false;
}

unsigned CSSMutableStyleDeclarationImpl::length() const
{
    return m_values.count();
}

DOMString CSSMutableStyleDeclarationImpl::item(unsigned i) const
{
    if (i >= m_values.count())
       return DOMString();
    return getPropertyName(m_values[i].id());
}

CSSRuleImpl *CSSStyleDeclarationImpl::parentRule() const
{
    return (m_parent && m_parent->isRule() ) ?
	static_cast<CSSRuleImpl *>(m_parent) : 0;
}

DOM::DOMString CSSMutableStyleDeclarationImpl::cssText() const
{
    DOMString result = "";
    
    QValueListConstIterator<CSSProperty> end;
    for (QValueListConstIterator<CSSProperty> it = m_values.begin(); it != end; ++it)
        result += (*it).cssText();

    return result;
}

void CSSMutableStyleDeclarationImpl::setCssText(const DOM::DOMString& text, int &exceptionCode)
{
    exceptionCode = 0;
    m_values.clear();
    CSSParser parser(strictParsing);
    parser.parseDeclaration(this, text);
    // FIXME: Detect syntax errors and set exceptionCode.
    setChanged();
}

void CSSMutableStyleDeclarationImpl::merge(CSSMutableStyleDeclarationImpl *other, bool argOverridesOnConflict)
{
    QValueListConstIterator<CSSProperty> end;
    for (QValueListConstIterator<CSSProperty> it = other->valuesIterator(); it != end; ++it) {
        const CSSProperty &property = *it;
        CSSValueImpl *value = getPropertyCSSValue(property.id());
        if (value) {
            value->ref();
            value->deref();
            if (!argOverridesOnConflict)
                continue;
            removeProperty(property.id());
        }
        m_values.append(property);
    }
    // FIXME: This probably should have a call to setChanged() if something changed. We may also wish to add
    // a notifyChanged argument to this function to follow the model of other functions in this class.
}

void CSSStyleDeclarationImpl::diff(CSSMutableStyleDeclarationImpl *style) const
{
    if (!style)
        return;

    QValueList<int> properties;
    QValueListConstIterator<CSSProperty> end;
    for (QValueListConstIterator<CSSProperty> it(style->valuesIterator()); it != end; ++it) {
        const CSSProperty &property = *it;
        CSSValueImpl *value = getPropertyCSSValue(property.id());
        if (value) {
            value->ref();
            if (value->cssText() == property.value()->cssText()) {
                properties.append(property.id());
            }
            value->deref();
        }
    }
    
    for (QValueListIterator<int> it(properties.begin()); it != properties.end(); ++it)
        style->removeProperty(*it);
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
    CSS_PROP__KHTML_TEXT_DECORATIONS_IN_EFFECT,
    CSS_PROP_TEXT_INDENT,
    CSS_PROP__KHTML_TEXT_SIZE_ADJUST,
    CSS_PROP_TEXT_TRANSFORM,
    CSS_PROP_ORPHANS,
    CSS_PROP_WHITE_SPACE,
    CSS_PROP_WIDOWS,
    CSS_PROP_WORD_SPACING,
};

const unsigned numInheritableProperties = sizeof(inheritableProperties) / sizeof(inheritableProperties[0]);

// This is the list of properties we want to copy in the copyBlockProperties() function.
// It is the list of CSS properties that apply to specially to block-level elements.
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

CSSMutableStyleDeclarationImpl *CSSMutableStyleDeclarationImpl::copyBlockProperties() const
{
    return copyPropertiesInSet(blockProperties, numBlockProperties);
}

void CSSMutableStyleDeclarationImpl::removeBlockProperties()
{
    removePropertiesInSet(blockProperties, numBlockProperties);
}

void CSSMutableStyleDeclarationImpl::removeInheritableProperties()
{
    removePropertiesInSet(inheritableProperties, numInheritableProperties);
}

CSSMutableStyleDeclarationImpl *CSSStyleDeclarationImpl::copyPropertiesInSet(const int *set, unsigned length) const
{
    QValueList<CSSProperty> list;
    for (unsigned i = 0; i < length; i++) {
        CSSValueImpl *value = getPropertyCSSValue(set[i]);
        if (value)
            list.append(CSSProperty(set[i], value, false));
    }
    return new CSSMutableStyleDeclarationImpl(0, list);
}

void CSSMutableStyleDeclarationImpl::removePropertiesInSet(const int *set, unsigned length)
{
    bool changed = false;
    for (unsigned i = 0; i < length; i++) {
        CSSValueImpl *value = getPropertyCSSValue(set[i]);
        if (value) {
            m_values.remove(CSSProperty(set[i], value, false));
            changed = true;
        }
    }
    if (changed)
        setChanged();
}

CSSMutableStyleDeclarationImpl *CSSMutableStyleDeclarationImpl::makeMutable()
{
    return this;
}

CSSMutableStyleDeclarationImpl *CSSMutableStyleDeclarationImpl::copy() const
{
    return new CSSMutableStyleDeclarationImpl(0, m_values);
}

// --------------------------------------------------------------------------------------

unsigned short CSSInheritedValueImpl::cssValueType() const
{
    return CSSValue::CSS_INHERIT;
}

DOM::DOMString CSSInheritedValueImpl::cssText() const
{
    return DOMString("inherit");
}

unsigned short CSSInitialValueImpl::cssValueType() const
{ 
    return CSSValue::CSS_INITIAL; 
}

DOM::DOMString CSSInitialValueImpl::cssText() const
{
    return DOMString("initial");
}

// ----------------------------------------------------------------------------------------

CSSValueListImpl::CSSValueListImpl()
{
}

CSSValueListImpl::~CSSValueListImpl()
{
    CSSValueImpl *val = m_values.first();
    while( val ) {
	val->deref();
	val = m_values.next();
    }
}

unsigned short CSSValueListImpl::cssValueType() const
{
    return CSSValue::CSS_VALUE_LIST;
}

void CSSValueListImpl::append(CSSValueImpl *val)
{
    m_values.append(val);
    val->ref();
}

DOM::DOMString CSSValueListImpl::cssText() const
{
    DOMString result = "";

    for (QPtrListIterator<CSSValueImpl> iterator(m_values); iterator.current(); ++iterator) {
	if (result.length() != 0) {
	    result += ", ";
	}
	result += iterator.current()->cssText();
    }
    
    return result;
}

// -------------------------------------------------------------------------------------

CSSPrimitiveValueImpl::CSSPrimitiveValueImpl()
    : CSSValueImpl()
{
    m_type = 0;
}

CSSPrimitiveValueImpl::CSSPrimitiveValueImpl(int ident)
    : CSSValueImpl()
{
    m_value.ident = ident;
    m_type = CSSPrimitiveValue::CSS_IDENT;
}

CSSPrimitiveValueImpl::CSSPrimitiveValueImpl(double num, CSSPrimitiveValue::UnitTypes type)
{
    m_value.num = num;
    m_type = type;
}

CSSPrimitiveValueImpl::CSSPrimitiveValueImpl(const DOMString &str, CSSPrimitiveValue::UnitTypes type)
{
    m_value.string = str.impl();
    if(m_value.string) m_value.string->ref();
    m_type = type;
}

CSSPrimitiveValueImpl::CSSPrimitiveValueImpl(CounterImpl *c)
{
    m_value.counter = c;
    if (m_value.counter)
	m_value.counter->ref();
    m_type = CSSPrimitiveValue::CSS_COUNTER;
}

CSSPrimitiveValueImpl::CSSPrimitiveValueImpl( RectImpl *r)
{
    m_value.rect = r;
    if (m_value.rect)
	m_value.rect->ref();
    m_type = CSSPrimitiveValue::CSS_RECT;
}

CSSPrimitiveValueImpl::CSSPrimitiveValueImpl( DashboardRegionImpl *r)
{
    m_value.region = r;
    if (m_value.region)
	m_value.region->ref();
    m_type = CSSPrimitiveValue::CSS_DASHBOARD_REGION;
}

CSSPrimitiveValueImpl::CSSPrimitiveValueImpl(QRgb color)
{
    m_value.rgbcolor = color;
    m_type = CSSPrimitiveValue::CSS_RGBCOLOR;
}

CSSPrimitiveValueImpl::CSSPrimitiveValueImpl(PairImpl* p)
{
    m_value.pair = p;
    if (p)
	p->ref();
    m_type = CSSPrimitiveValue::CSS_PAIR;
}

CSSPrimitiveValueImpl::~CSSPrimitiveValueImpl()
{
    cleanup();
}

void CSSPrimitiveValueImpl::cleanup()
{
    switch(m_type) {
    case CSSPrimitiveValue::CSS_STRING:
    case CSSPrimitiveValue::CSS_URI:
    case CSSPrimitiveValue::CSS_ATTR:
        if(m_value.string) m_value.string->deref();
        break;
    case CSSPrimitiveValue::CSS_COUNTER:
        m_value.counter->deref();
        break;
    case CSSPrimitiveValue::CSS_RECT:
        m_value.rect->deref();
        break;
    case CSSPrimitiveValue::CSS_DASHBOARD_REGION:
        if (m_value.region)
            m_value.region->deref();
        break;
    default:
        break;
    }

    m_type = 0;
}

int CSSPrimitiveValueImpl::computeLength( khtml::RenderStyle *style, QPaintDeviceMetrics *devMetrics )
{
    double result = computeLengthFloat( style, devMetrics );
#if APPLE_CHANGES
    // This conversion is imprecise, often resulting in values of, e.g., 44.99998.  We
    // need to go ahead and round if we're really close to the next integer value.
    int intResult = (int)(result + (result < 0 ? -0.01 : +0.01));
#else
    int intResult = (int)result;
#endif
    return intResult;    
}

int CSSPrimitiveValueImpl::computeLength( khtml::RenderStyle *style, QPaintDeviceMetrics *devMetrics, 
                                          double multiplier )
{
    double result = multiplier * computeLengthFloat( style, devMetrics );
#if APPLE_CHANGES
    // This conversion is imprecise, often resulting in values of, e.g., 44.99998.  We
    // need to go ahead and round if we're really close to the next integer value.
    int intResult = (int)(result + (result < 0 ? -0.01 : +0.01));
#else
    int intResult = (int)result;
#endif
    return intResult;    
}

double CSSPrimitiveValueImpl::computeLengthFloat( khtml::RenderStyle *style, QPaintDeviceMetrics *devMetrics,
                                                  bool applyZoomFactor )
{
    unsigned short type = primitiveType();

    double dpiY = 72.; // fallback
    if ( devMetrics )
        dpiY = devMetrics->logicalDpiY();
    if ( !khtml::printpainter && dpiY < 96 )
        dpiY = 96.;

    double factor = 1.;
    switch(type)
    {
    case CSSPrimitiveValue::CSS_EMS:
        factor = applyZoomFactor ?
          style->htmlFont().getFontDef().computedSize :
          style->htmlFont().getFontDef().specifiedSize;
        break;
    case CSSPrimitiveValue::CSS_EXS:
        // FIXME: We have a bug right now where the zoom will be applied multiple times to EX units.
        // We really need to compute EX using fontMetrics for the original specifiedSize and not use
        // our actual constructed rendering font.
	{
        QFontMetrics fm = style->fontMetrics();
#if APPLE_CHANGES
        factor = fm.xHeight();
#else
        QRect b = fm.boundingRect('x');
        factor = b.height();
#endif
        break;
	}
    case CSSPrimitiveValue::CSS_PX:
        break;
    case CSSPrimitiveValue::CSS_CM:
	factor = dpiY/2.54; //72dpi/(2.54 cm/in)
        break;
    case CSSPrimitiveValue::CSS_MM:
	factor = dpiY/25.4;
        break;
    case CSSPrimitiveValue::CSS_IN:
            factor = dpiY;
        break;
    case CSSPrimitiveValue::CSS_PT:
            factor = dpiY/72.;
        break;
    case CSSPrimitiveValue::CSS_PC:
        // 1 pc == 12 pt
            factor = dpiY*12./72.;
        break;
    default:
        return -1;
    }

    return getFloatValue(type)*factor;
}

void CSSPrimitiveValueImpl::setFloatValue( unsigned short unitType, double floatValue, int &exceptioncode )
{
    exceptioncode = 0;
    cleanup();
    // ### check if property supports this type
    if(m_type > CSSPrimitiveValue::CSS_DIMENSION) {
	exceptioncode = CSSException::SYNTAX_ERR + CSSException::_EXCEPTION_OFFSET;
	return;
    }
    //if(m_type > CSSPrimitiveValue::CSS_DIMENSION) throw DOMException(DOMException::INVALID_ACCESS_ERR);
    m_value.num = floatValue;
    m_type = unitType;
}

void CSSPrimitiveValueImpl::setStringValue( unsigned short stringType, const DOMString &stringValue, int &exceptioncode )
{
    exceptioncode = 0;
    cleanup();
    //if(m_type < CSSPrimitiveValue::CSS_STRING) throw DOMException(DOMException::INVALID_ACCESS_ERR);
    //if(m_type > CSSPrimitiveValue::CSS_ATTR) throw DOMException(DOMException::INVALID_ACCESS_ERR);
    if(m_type < CSSPrimitiveValue::CSS_STRING || m_type >> CSSPrimitiveValue::CSS_ATTR) {
	exceptioncode = CSSException::SYNTAX_ERR + CSSException::_EXCEPTION_OFFSET;
	return;
    }
    if(stringType != CSSPrimitiveValue::CSS_IDENT)
    {
	m_value.string = stringValue.impl();
	m_value.string->ref();
	m_type = stringType;
    }
    // ### parse ident
}

DOMString CSSPrimitiveValueImpl::getStringValue() const
{
    switch (m_type) {
        case CSSPrimitiveValue::CSS_STRING:
        case CSSPrimitiveValue::CSS_ATTR:
        case CSSPrimitiveValue::CSS_URI:
            return m_value.string;
        case CSSPrimitiveValue::CSS_IDENT:
            return getValueName(m_value.ident);
        default:
            // FIXME: The CSS 2.1 spec says you should throw an exception here.
            break;
    }
    
    return DOMString();
}

unsigned short CSSPrimitiveValueImpl::cssValueType() const
{
    return CSSValue::CSS_PRIMITIVE_VALUE;
}

bool CSSPrimitiveValueImpl::parseString( const DOMString &/*string*/, bool )
{
    // ###
    return false;
}

int CSSPrimitiveValueImpl::getIdent()
{
    if(m_type != CSSPrimitiveValue::CSS_IDENT) return 0;
    return m_value.ident;
}

DOM::DOMString CSSPrimitiveValueImpl::cssText() const
{
    // ### return the original value instead of a generated one (e.g. color
    // name if it was specified) - check what spec says about this
    DOMString text;
    switch ( m_type ) {
	case CSSPrimitiveValue::CSS_UNKNOWN:
	    // ###
	    break;
	case CSSPrimitiveValue::CSS_NUMBER:
	    text = DOMString(QString::number( m_value.num ));
	    break;
	case CSSPrimitiveValue::CSS_PERCENTAGE:
	    text = DOMString(QString::number( m_value.num ) + "%");
	    break;
	case CSSPrimitiveValue::CSS_EMS:
	    text = DOMString(QString::number( m_value.num ) + "em");
	    break;
	case CSSPrimitiveValue::CSS_EXS:
	    text = DOMString(QString::number( m_value.num ) + "ex");
	    break;
	case CSSPrimitiveValue::CSS_PX:
	    text = DOMString(QString::number( m_value.num ) + "px");
	    break;
	case CSSPrimitiveValue::CSS_CM:
	    text = DOMString(QString::number( m_value.num ) + "cm");
	    break;
	case CSSPrimitiveValue::CSS_MM:
	    text = DOMString(QString::number( m_value.num ) + "mm");
	    break;
	case CSSPrimitiveValue::CSS_IN:
	    text = DOMString(QString::number( m_value.num ) + "in");
	    break;
	case CSSPrimitiveValue::CSS_PT:
	    text = DOMString(QString::number( m_value.num ) + "pt");
	    break;
	case CSSPrimitiveValue::CSS_PC:
	    text = DOMString(QString::number( m_value.num ) + "pc");
	    break;
	case CSSPrimitiveValue::CSS_DEG:
	    text = DOMString(QString::number( m_value.num ) + "deg");
	    break;
	case CSSPrimitiveValue::CSS_RAD:
	    text = DOMString(QString::number( m_value.num ) + "rad");
	    break;
	case CSSPrimitiveValue::CSS_GRAD:
	    text = DOMString(QString::number( m_value.num ) + "grad");
	    break;
	case CSSPrimitiveValue::CSS_MS:
	    text = DOMString(QString::number( m_value.num ) + "ms");
	    break;
	case CSSPrimitiveValue::CSS_S:
	    text = DOMString(QString::number( m_value.num ) + "s");
	    break;
	case CSSPrimitiveValue::CSS_HZ:
	    text = DOMString(QString::number( m_value.num ) + "hz");
	    break;
	case CSSPrimitiveValue::CSS_KHZ:
	    text = DOMString(QString::number( m_value.num ) + "khz");
	    break;
	case CSSPrimitiveValue::CSS_DIMENSION:
	    // ###
	    break;
	case CSSPrimitiveValue::CSS_STRING:
	    text = quoteStringIfNeeded(m_value.string);
	    break;
	case CSSPrimitiveValue::CSS_URI:
            text  = "url(";
	    text += DOMString( m_value.string );
            text += ")";
	    break;
	case CSSPrimitiveValue::CSS_IDENT:
	    text = getValueName(m_value.ident);
	    break;
	case CSSPrimitiveValue::CSS_ATTR:
	    // ###
	    break;
	case CSSPrimitiveValue::CSS_COUNTER:
	    // ###
	    break;
        case CSSPrimitiveValue::CSS_RECT: {
	    RectImpl* rectVal = getRectValue();
            text = "rect(";
            text += rectVal->top()->cssText() + " ";
            text += rectVal->right()->cssText() + " ";
            text += rectVal->bottom()->cssText() + " ";
            text += rectVal->left()->cssText() + ")";
	    break;
        }
	case CSSPrimitiveValue::CSS_RGBCOLOR: {
            QColor color(m_value.rgbcolor);
            if (qAlpha(m_value.rgbcolor) < 0xFF)
                text = "rgba(";
            else
                text = "rgb(";
            text += QString::number(color.red()) + ", ";
            text += QString::number(color.green()) + ", ";
            text += QString::number(color.blue());
            if (qAlpha(m_value.rgbcolor) < 0xFF)
                text += ", " + QString::number((float)qAlpha(m_value.rgbcolor) / 0xFF);
            text += ")";
            break;
        }
        case CSSPrimitiveValue::CSS_PAIR:
            text = m_value.pair->first()->cssText();
            text += " ";
            text += m_value.pair->second()->cssText();
            break;

#if APPLE_CHANGES        
        case CSSPrimitiveValue::CSS_DASHBOARD_REGION: {
            DashboardRegionImpl *region = getDashboardRegionValue();
            while (region) {
                text = "dashboard-region(";
                text += region->m_label;
                if (region->m_isCircle){
                    text += " circle ";
                }
                else if (region->m_isRectangle){
                    text += " rectangle ";
                }
                else
                    break;
                text += region->top()->cssText() + " ";
                text += region->right()->cssText() + " ";
                text += region->bottom()->cssText() + " ";
                text += region->left()->cssText();
                text += ")";
                region = region->m_next;
            }
            break;
        }
#endif
    }
    return text;
}

// -----------------------------------------------------------------

RectImpl::RectImpl()
{
    m_top = 0;
    m_right = 0;
    m_bottom = 0;
    m_left = 0;
}

RectImpl::~RectImpl()
{
    if (m_top) m_top->deref();
    if (m_right) m_right->deref();
    if (m_bottom) m_bottom->deref();
    if (m_left) m_left->deref();
}

void RectImpl::setTop( CSSPrimitiveValueImpl *top )
{
    if( top ) top->ref();
    if ( m_top ) m_top->deref();
    m_top = top;
}

void RectImpl::setRight( CSSPrimitiveValueImpl *right )
{
    if( right ) right->ref();
    if ( m_right ) m_right->deref();
    m_right = right;
}

void RectImpl::setBottom( CSSPrimitiveValueImpl *bottom )
{
    if( bottom ) bottom->ref();
    if ( m_bottom ) m_bottom->deref();
    m_bottom = bottom;
}

void RectImpl::setLeft( CSSPrimitiveValueImpl *left )
{
    if( left ) left->ref();
    if ( m_left ) m_left->deref();
    m_left = left;
}

// -----------------------------------------------------------------

PairImpl::PairImpl()
{
    m_first = m_second = 0;
}

PairImpl::~PairImpl()
{
    if (m_first) m_first->deref();
    if (m_second) m_second->deref();
}

void PairImpl::setFirst(CSSPrimitiveValueImpl *first)
{
    if (first) first->ref();
    if (m_first) m_first->deref();
    m_first = first;
}

void PairImpl::setSecond(CSSPrimitiveValueImpl *second)
{
    if (second) second->ref();
    if (m_second) m_second->deref();
    m_second = second;
}

// -----------------------------------------------------------------

CSSImageValueImpl::CSSImageValueImpl(const DOMString &url, StyleBaseImpl *style)
    : CSSPrimitiveValueImpl(url, CSSPrimitiveValue::CSS_URI), m_image(0), m_accessedImage(false)
{
}

CSSImageValueImpl::CSSImageValueImpl()
    : CSSPrimitiveValueImpl(CSS_VAL_NONE), m_image(0), m_accessedImage(true)
{
}

CSSImageValueImpl::~CSSImageValueImpl()
{
    if (m_image)
        m_image->deref(this);
}

CachedImage* CSSImageValueImpl::image(khtml::DocLoader* loader)
{
    if (!m_accessedImage) {
        m_accessedImage = true;

        if (loader)
            m_image = loader->requestImage(getStringValue());
        else
            m_image = khtml::Cache::requestImage(0, getStringValue());
        
        if (m_image)
            m_image->ref(this);
    }
    
    return m_image;
}

// ------------------------------------------------------------------------

CSSBorderImageValueImpl::CSSBorderImageValueImpl(CSSImageValueImpl* image, RectImpl* imageRect,
                                                 int horizontalRule, int verticalRule)
: m_image(image), m_imageSliceRect(imageRect),
  m_horizontalSizeRule(horizontalRule), m_verticalSizeRule(verticalRule)
{
    if (m_image)
        m_image->ref();
    if (m_imageSliceRect)
        m_imageSliceRect->ref();
}

CSSBorderImageValueImpl::~CSSBorderImageValueImpl()
{
    if (m_image)
        m_image->deref();
    if (m_imageSliceRect)
        m_imageSliceRect->deref();
}

DOMString CSSBorderImageValueImpl::cssText() const
{
    // Image first.
    DOMString text(m_image->cssText());
    text += " ";
    
    // Now the rect, but it isn't really a rect, so we dump manually
    text += m_imageSliceRect->top()->cssText();
    text += " ";
    text += m_imageSliceRect->right()->cssText();
    text += " ";
    text += m_imageSliceRect->bottom()->cssText();
    text += " ";
    text += m_imageSliceRect->left()->cssText();
    
    // Now the keywords.
    text += " ";
    text += CSSPrimitiveValueImpl(m_horizontalSizeRule).cssText();
    text += " ";
    text += CSSPrimitiveValueImpl(m_verticalSizeRule).cssText();

    return text;
}

// ------------------------------------------------------------------------

FontFamilyValueImpl::FontFamilyValueImpl( const QString &string)
: CSSPrimitiveValueImpl( DOMString(), CSSPrimitiveValue::CSS_STRING)
{
    static const QRegExp parenReg(" \\(.*\\)$");
    static const QRegExp braceReg(" \\[.*\\]$");

#if APPLE_CHANGES
    parsedFontName = string;
    // a language tag is often added in braces at the end. Remove it.
    parsedFontName.replace(parenReg, "");
    // remove [Xft] qualifiers
    parsedFontName.replace(braceReg, "");
#else
    const QString &available = KHTMLSettings::availableFamilies();

    QString face = string.lower();
    // a languge tag is often added in braces at the end. Remove it.
    face = face.replace(parenReg, "");
    // remove [Xft] qualifiers
    face = face.replace(braceReg, "");
    //kdDebug(0) << "searching for face '" << face << "'" << endl;

    int pos = available.find( face, 0, false );
    if( pos == -1 ) {
        QString str = face;
        int p = face.find(' ');
        // Arial Blk --> Arial
        // MS Sans Serif --> Sans Serif
        if ( p != -1 ) {
            if(p > 0 && (int)str.length() - p > p + 1)
                str = str.mid( p+1 );
            else
                str.truncate( p );
            pos = available.find( str, 0, false);
        }
    }

    if ( pos != -1 ) {
        int pos1 = available.findRev( ',', pos ) + 1;
        pos = available.find( ',', pos );
        if ( pos == -1 )
            pos = available.length();
        parsedFontName = available.mid( pos1, pos - pos1 );
    }
#endif // !APPLE_CHANGES
}

DOM::DOMString FontFamilyValueImpl::cssText() const
{
    return quoteStringIfNeeded(parsedFontName);
}

FontValueImpl::FontValueImpl()
    : style(0), variant(0), weight(0), size(0), lineHeight(0), family(0)
{
}

FontValueImpl::~FontValueImpl()
{
    delete style;
    delete variant;
    delete weight;
    delete size;
    delete lineHeight;
    delete family;
}

DOMString FontValueImpl::cssText() const
{
    // font variant weight size / line-height family 

    DOMString result("");

    if (style) {
	result += style->cssText();
    }
    if (variant) {
	if (result.length() > 0) {
	    result += " ";
	}
	result += variant->cssText();
    }
    if (weight) {
	if (result.length() > 0) {
	    result += " ";
	}
	result += weight->cssText();
    }
    if (size) {
	if (result.length() > 0) {
	    result += " ";
	}
	result += size->cssText();
    }
    if (lineHeight) {
	if (!size) {
	    result += " ";
	}
	result += "/";
	result += lineHeight->cssText();
    }
    if (family) {
	if (result.length() > 0) {
	    result += " ";
	}
	result += family->cssText();
    }

    return result;
}
    

// Used for text-shadow and box-shadow
ShadowValueImpl::ShadowValueImpl(CSSPrimitiveValueImpl* _x, CSSPrimitiveValueImpl* _y,
                                 CSSPrimitiveValueImpl* _blur, CSSPrimitiveValueImpl* _color)
:x(_x), y(_y), blur(_blur), color(_color)	
{}

ShadowValueImpl::~ShadowValueImpl()
{
    delete x;
    delete y;
    delete blur;
    delete color;
}

DOMString ShadowValueImpl::cssText() const
{
    DOMString text("");
    if (color) {
	text += color->cssText();
    }
    if (x) {
	if (text.length() > 0) {
	    text += " ";
	}
	text += x->cssText();
    }
    if (y) {
	if (text.length() > 0) {
	    text += " ";
	}
	text += y->cssText();
    }
    if (blur) {
	if (text.length() > 0) {
	    text += " ";
	}
	text += blur->cssText();
    }

    return text;
}

DOMString CSSProperty::cssText() const
{
    return getPropertyName(m_id) + DOMString(": ") + m_value->cssText() + (m_bImportant ? DOMString(" !important") : DOMString()) + DOMString("; ");
}

bool operator==(const CSSProperty &a, const CSSProperty &b)
{
    return a.m_id == b.m_id && a.m_bImportant == b.m_bImportant && a.m_value == b.m_value;
}

}
