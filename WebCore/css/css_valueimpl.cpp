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
#include "css_valueimpl.h"

#include "Cache.h"
#include "CachedImage.h"
#include "DocLoader.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Font.h"
#include "HTMLElement.h"
#include "PlatformString.h"
#include "StringImpl.h"
#include "css_ruleimpl.h"
#include "css_stylesheetimpl.h"
#include "cssparser.h"
#include "CSSPropertyNames.h"
#include "cssstyleselector.h"
#include "CSSValueKeywords.h"
#include "render_style.h"
#include <qregexp.h>

#if SVG_SUPPORT
#include "ksvgcssproperties.h"
#endif

// Not in any header, so just declare it here for now.
WebCore::String getPropertyName(unsigned short id);

namespace WebCore {

// Defined in CSSGrammar.y, but not in any header, so just declare it here for now.
int getPropertyID(const char *str, int len);

static int propertyID(const String &s)
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

    int id = getPropertyID(buffer, len);
#if SVG_SUPPORT
    if (id == 0)
        id = SVG::getSVGCSSPropertyID(buffer, len);
#endif
    return id;
}


// Quotes the string if it needs quoting.
// We use single quotes for now beause markup.cpp uses double quotes.
static String quoteStringIfNeeded(const String &string)
{
    // For now, just do this for strings that start with "#" to fix Korean font names that start with "#".
    // Post-Tiger, we should isLegalIdentifier instead after working out all the ancillary issues.
    if (string[0] != '#')
        return string;

    // FIXME: Also need to transform control characters into \ sequences.
    DeprecatedString s = string.deprecatedString();
    s.replace('\\', "\\\\");
    s.replace('\'', "\\'");
    return '\'' + s + '\'';
}

CSSStyleDeclaration::CSSStyleDeclaration(CSSRule *parent)
    : StyleBase(parent)
{
}

bool CSSStyleDeclaration::isStyleDeclaration()
{
    return true;
}

PassRefPtr<CSSValue> CSSStyleDeclaration::getPropertyCSSValue(const String& propertyName)
{
    int propID = propertyID(propertyName);
    if (!propID)
        return 0;
    return getPropertyCSSValue(propID);
}

String CSSStyleDeclaration::getPropertyValue(const String &propertyName)
{
    int propID = propertyID(propertyName);
    if (!propID)
        return String();
    return getPropertyValue(propID);
}

String CSSStyleDeclaration::getPropertyPriority(const String &propertyName)
{
    int propID = propertyID(propertyName);
    if (!propID)
        return String();
    return getPropertyPriority(propID) ? "important" : "";
}

String CSSStyleDeclaration::getPropertyShorthand(const String &propertyName)
{
    int propID = propertyID(propertyName);
    if (!propID)
        return String();
    int shorthandID = getPropertyShorthand(propID);
    if (!shorthandID)
        return String();
    return getPropertyName(shorthandID);
}

bool CSSStyleDeclaration::isPropertyImplicit(const String &propertyName)
{
    int propID = propertyID(propertyName);
    if (!propID)
        return false;
    return isPropertyImplicit(propID);
}

void CSSStyleDeclaration::setProperty(const String& propertyName, const String& value, const String& priority, ExceptionCode& ec)
{
    int propID = propertyID(propertyName);
    if (!propID) // set exception?
        return;
    bool important = priority.find("important", 0, false) != -1;
    setProperty(propID, value, important, ec);
}

String CSSStyleDeclaration::removeProperty(const String& propertyName, ExceptionCode& ec)
{
    int propID = propertyID(propertyName);
    if (!propID)
        return String();
    return removeProperty(propID, ec);
}

bool CSSStyleDeclaration::isPropertyName(const String &propertyName)
{
    return propertyID(propertyName);
}

// --------------------------------------------------------------------------------------

CSSMutableStyleDeclaration::CSSMutableStyleDeclaration()
    : m_node(0)
{
}

CSSMutableStyleDeclaration::CSSMutableStyleDeclaration(CSSRule *parent)
    : CSSStyleDeclaration(parent), m_node(0)
{
}

CSSMutableStyleDeclaration::CSSMutableStyleDeclaration(CSSRule *parent, const DeprecatedValueList<CSSProperty> &values)
    : CSSStyleDeclaration(parent), m_values(values), m_node(0)
{
    // FIXME: This allows duplicate properties.
}

CSSMutableStyleDeclaration::CSSMutableStyleDeclaration(CSSRule *parent, const CSSProperty * const *properties, int numProperties)
    : CSSStyleDeclaration(parent), m_node(0)
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
    //kdDebug() << k_funcinfo << "property not found:" << propertyID << endl;
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
        if (m_node->isStyledElement() && this == static_cast<StyledElement *>(m_node)->inlineStyleDecl())
            static_cast<StyledElement *>(m_node)->invalidateStyleAttribute();
        return;
    }

    // ### quick&dirty hack for KDE 3.0... make this MUCH better! (Dirk)
    StyleBase *root = this;
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

CSSRule *CSSStyleDeclaration::parentRule() const
{
    return (parent() && parent()->isRule()) ? static_cast<CSSRule *>(parent()) : 0;
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

void CSSStyleDeclaration::diff(CSSMutableStyleDeclaration *style) const
{
    if (!style)
        return;

    DeprecatedValueList<int> properties;
    DeprecatedValueListConstIterator<CSSProperty> end;
    for (DeprecatedValueListConstIterator<CSSProperty> it(style->valuesIterator()); it != end; ++it) {
        const CSSProperty &property = *it;
        RefPtr<CSSValue> value = getPropertyCSSValue(property.id());
        if (value && (value->cssText() == property.value()->cssText()))
            properties.append(property.id());
    }
    
    for (DeprecatedValueListIterator<int> it(properties.begin()); it != properties.end(); ++it)
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

PassRefPtr<CSSMutableStyleDeclaration> CSSStyleDeclaration::copyPropertiesInSet(const int *set, unsigned length) const
{
    DeprecatedValueList<CSSProperty> list;
    for (unsigned i = 0; i < length; i++) {
        RefPtr<CSSValue> value = getPropertyCSSValue(set[i]);
        if (value)
            list.append(CSSProperty(set[i], value.release(), false));
    }
    return new CSSMutableStyleDeclaration(0, list);
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

// --------------------------------------------------------------------------------------

unsigned short CSSInheritedValue::cssValueType() const
{
    return CSS_INHERIT;
}

String CSSInheritedValue::cssText() const
{
    return "inherit";
}

unsigned short CSSInitialValue::cssValueType() const
{ 
    return CSS_INITIAL; 
}

String CSSInitialValue::cssText() const
{
    return "initial";
}

// ----------------------------------------------------------------------------------------

CSSValueList::~CSSValueList()
{
    for (CSSValue *val = m_values.first(); val; val = m_values.next())
        val->deref();
}

unsigned short CSSValueList::cssValueType() const
{
    return CSS_VALUE_LIST;
}

void CSSValueList::append(PassRefPtr<CSSValue> val)
{
    m_values.append(val.release());
}

String CSSValueList::cssText() const
{
    String result = "";

    for (DeprecatedPtrListIterator<CSSValue> iterator(m_values); iterator.current(); ++iterator) {
        if (!result.isEmpty())
            result += ", ";
        result += iterator.current()->cssText();
    }
    
    return result;
}

// -------------------------------------------------------------------------------------

CSSPrimitiveValue::CSSPrimitiveValue()
{
    m_type = 0;
}

CSSPrimitiveValue::CSSPrimitiveValue(int ident)
{
    m_value.ident = ident;
    m_type = CSS_IDENT;
}

CSSPrimitiveValue::CSSPrimitiveValue(double num, UnitTypes type)
{
    m_value.num = num;
    m_type = type;
}

CSSPrimitiveValue::CSSPrimitiveValue(const String& str, UnitTypes type)
{
    if ((m_value.string = str.impl()))
        m_value.string->ref();
    m_type = type;
}

CSSPrimitiveValue::CSSPrimitiveValue(PassRefPtr<Counter> c)
{
    m_value.counter = c.release();
    m_type = CSS_COUNTER;
}

CSSPrimitiveValue::CSSPrimitiveValue(PassRefPtr<RectImpl> r)
{
    m_value.rect = r.release();
    m_type = CSS_RECT;
}

#if __APPLE__
CSSPrimitiveValue::CSSPrimitiveValue(PassRefPtr<DashboardRegion> r)
{
    m_value.region = r.release();
    m_type = CSS_DASHBOARD_REGION;
}
#endif

CSSPrimitiveValue::CSSPrimitiveValue(RGBA32 color)
{
    m_value.rgbcolor = color;
    m_type = CSS_RGBCOLOR;
}

CSSPrimitiveValue::CSSPrimitiveValue(PassRefPtr<Pair> p)
{
    m_value.pair = p.release();
    m_type = CSS_PAIR;
}

CSSPrimitiveValue::~CSSPrimitiveValue()
{
    cleanup();
}

void CSSPrimitiveValue::cleanup()
{
    switch(m_type) {
    case CSS_STRING:
    case CSS_URI:
    case CSS_ATTR:
        if (m_value.string)
            m_value.string->deref();
        break;
    case CSS_COUNTER:
        m_value.counter->deref();
        break;
    case CSS_RECT:
        m_value.rect->deref();
        break;
#if __APPLE__
    case CSS_DASHBOARD_REGION:
        if (m_value.region)
            m_value.region->deref();
        break;
#endif
    default:
        break;
    }

    m_type = 0;
}

int CSSPrimitiveValue::computeLength(RenderStyle *style)
{
    double result = computeLengthFloat(style);
    // This conversion is imprecise, often resulting in values of, e.g., 44.99998.  We
    // need to go ahead and round if we're really close to the next integer value.
    int intResult = (int)(result + (result < 0 ? -0.01 : +0.01));
    return intResult;    
}

int CSSPrimitiveValue::computeLength(RenderStyle *style, double multiplier)
{
    double result = multiplier * computeLengthFloat(style);
    // This conversion is imprecise, often resulting in values of, e.g., 44.99998.  We
    // need to go ahead and round if we're really close to the next integer value.
    int intResult = (int)(result + (result < 0 ? -0.01 : +0.01));
    return intResult;    
}

double CSSPrimitiveValue::computeLengthFloat(RenderStyle *style, bool applyZoomFactor)
{
    unsigned short type = primitiveType();

    // We always assume 96 CSS pixels in a CSS inch. This is the cold hard truth of the Web.
    // At high DPI, we may scale a CSS pixel, but the ratio of the CSS pixel to the so-called
    // "absolute" CSS length units like inch and pt is always fixed and never changes.
    double cssPixelsPerInch = 96.;

    double factor = 1.;
    switch(type) {
    case CSS_EMS:
        factor = applyZoomFactor ?
          style->fontDescription().computedSize() :
          style->fontDescription().specifiedSize();
        break;
    case CSS_EXS: {
        // FIXME: We have a bug right now where the zoom will be applied multiple times to EX units.
        // We really need to compute EX using fontMetrics for the original specifiedSize and not use
        // our actual constructed rendering font.
        factor = style->font().xHeight();
        break;
    }
    case CSS_PX:
        break;
    case CSS_CM:
        factor = cssPixelsPerInch/2.54; // (2.54 cm/in)
        break;
    case CSS_MM:
        factor = cssPixelsPerInch/25.4;
        break;
    case CSS_IN:
        factor = cssPixelsPerInch;
        break;
    case CSS_PT:
        factor = cssPixelsPerInch/72.;
        break;
    case CSS_PC:
        // 1 pc == 12 pt
        factor = cssPixelsPerInch*12./72.;
        break;
    default:
        return -1;
    }

    return getFloatValue(type)*factor;
}

void CSSPrimitiveValue::setFloatValue( unsigned short unitType, double floatValue, ExceptionCode& ec)
{
    ec = 0;
    cleanup();
    // ### check if property supports this type
    if (m_type > CSS_DIMENSION) {
        ec = SYNTAX_ERR;
        return;
    }
    //if(m_type > CSS_DIMENSION) throw DOMException(INVALID_ACCESS_ERR);
    m_value.num = floatValue;
    m_type = unitType;
}

void CSSPrimitiveValue::setStringValue( unsigned short stringType, const String &stringValue, ExceptionCode& ec)
{
    ec = 0;
    cleanup();
    //if(m_type < CSS_STRING) throw DOMException(INVALID_ACCESS_ERR);
    //if(m_type > CSS_ATTR) throw DOMException(INVALID_ACCESS_ERR);
    if (m_type < CSS_STRING || m_type > CSS_ATTR) {
        ec = SYNTAX_ERR;
        return;
    }
    if (stringType != CSS_IDENT) {
        m_value.string = stringValue.impl();
        m_value.string->ref();
        m_type = stringType;
    }
    // ### parse ident
}

String CSSPrimitiveValue::getStringValue() const
{
    switch (m_type) {
        case CSS_STRING:
        case CSS_ATTR:
        case CSS_URI:
            return m_value.string;
        case CSS_IDENT:
            return getValueName(m_value.ident);
        default:
            // FIXME: The CSS 2.1 spec says you should throw an exception here.
            break;
    }
    
    return String();
}

unsigned short CSSPrimitiveValue::cssValueType() const
{
    return CSS_PRIMITIVE_VALUE;
}

bool CSSPrimitiveValue::parseString( const String &/*string*/, bool )
{
    // ###
    return false;
}

int CSSPrimitiveValue::getIdent()
{
    if(m_type != CSS_IDENT) return 0;
    return m_value.ident;
}

String CSSPrimitiveValue::cssText() const
{
    // ### return the original value instead of a generated one (e.g. color
    // name if it was specified) - check what spec says about this
    String text;
    switch ( m_type ) {
        case CSS_UNKNOWN:
            // ###
            break;
        case CSS_NUMBER:
            text = DeprecatedString::number(m_value.num);
            break;
        case CSS_PERCENTAGE:
            text = DeprecatedString::number(m_value.num) + "%";
            break;
        case CSS_EMS:
            text = DeprecatedString::number(m_value.num) + "em";
            break;
        case CSS_EXS:
            text = DeprecatedString::number(m_value.num) + "ex";
            break;
        case CSS_PX:
            text = DeprecatedString::number(m_value.num) + "px";
            break;
        case CSS_CM:
            text = DeprecatedString::number(m_value.num) + "cm";
            break;
        case CSS_MM:
            text = DeprecatedString::number(m_value.num) + "mm";
            break;
        case CSS_IN:
            text = DeprecatedString::number(m_value.num) + "in";
            break;
        case CSS_PT:
            text = DeprecatedString::number(m_value.num) + "pt";
            break;
        case CSS_PC:
            text = DeprecatedString::number(m_value.num) + "pc";
            break;
        case CSS_DEG:
            text = DeprecatedString::number(m_value.num) + "deg";
            break;
        case CSS_RAD:
            text = DeprecatedString::number(m_value.num) + "rad";
            break;
        case CSS_GRAD:
            text = DeprecatedString::number(m_value.num) + "grad";
            break;
        case CSS_MS:
            text = DeprecatedString::number(m_value.num) + "ms";
            break;
        case CSS_S:
            text = DeprecatedString::number(m_value.num) + "s";
            break;
        case CSS_HZ:
            text = DeprecatedString::number(m_value.num) + "hz";
            break;
        case CSS_KHZ:
            text = DeprecatedString::number(m_value.num) + "khz";
            break;
        case CSS_DIMENSION:
            // ###
            break;
        case CSS_STRING:
            text = quoteStringIfNeeded(m_value.string);
            break;
        case CSS_URI:
            text = "url(" + String(m_value.string) + ")";
            break;
        case CSS_IDENT:
            text = getValueName(m_value.ident);
            break;
        case CSS_ATTR:
            // ###
            break;
        case CSS_COUNTER:
            // ###
            break;
        case CSS_RECT: {
            RectImpl* rectVal = getRectValue();
            text = "rect(";
            text += rectVal->top()->cssText() + " ";
            text += rectVal->right()->cssText() + " ";
            text += rectVal->bottom()->cssText() + " ";
            text += rectVal->left()->cssText() + ")";
            break;
        }
        case CSS_RGBCOLOR: {
            Color color(m_value.rgbcolor);
            if (color.alpha() < 0xFF)
                text = "rgba(";
            else
                text = "rgb(";
            text += DeprecatedString::number(color.red()) + ", ";
            text += DeprecatedString::number(color.green()) + ", ";
            text += DeprecatedString::number(color.blue());
            if (color.alpha() < 0xFF)
                text += ", " + DeprecatedString::number((float)color.alpha() / 0xFF);
            text += ")";
            break;
        }
        case CSS_PAIR:
            text = m_value.pair->first()->cssText();
            text += " ";
            text += m_value.pair->second()->cssText();
            break;
#if __APPLE__
        case CSS_DASHBOARD_REGION:
            for (DashboardRegion* region = getDashboardRegionValue(); region; region = region->m_next.get()) {
                text = "dashboard-region(";
                text += region->m_label;
                if (region->m_isCircle)
                    text += " circle ";
                else if (region->m_isRectangle)
                    text += " rectangle ";
                else
                    break;
                text += region->top()->cssText() + " ";
                text += region->right()->cssText() + " ";
                text += region->bottom()->cssText() + " ";
                text += region->left()->cssText();
                text += ")";
            }
            break;
#endif
    }
    return text;
}

// -----------------------------------------------------------------

RectImpl::~RectImpl()
{
}

// -----------------------------------------------------------------

Pair::~Pair()
{
}

// -----------------------------------------------------------------

CSSImageValue::CSSImageValue(const String& url, StyleBase *style)
    : CSSPrimitiveValue(url, CSS_URI), m_image(0), m_accessedImage(false)
{
}

CSSImageValue::CSSImageValue()
    : CSSPrimitiveValue(CSS_VAL_NONE), m_image(0), m_accessedImage(true)
{
}

CSSImageValue::~CSSImageValue()
{
    if (m_image)
        m_image->deref(this);
}

CachedImage* CSSImageValue::image(DocLoader* loader)
{
    if (!m_accessedImage) {
        m_accessedImage = true;

        if (loader)
            m_image = loader->requestImage(getStringValue());
        else
            m_image = Cache::requestImage(0, getStringValue());
        
        if (m_image)
            m_image->ref(this);
    }
    
    return m_image;
}

// ------------------------------------------------------------------------

CSSBorderImageValue::CSSBorderImageValue(PassRefPtr<CSSImageValue> image,
    PassRefPtr<RectImpl> imageRect, int horizontalRule, int verticalRule)
    : m_image(image), m_imageSliceRect(imageRect)
    , m_horizontalSizeRule(horizontalRule), m_verticalSizeRule(verticalRule)
{
}

String CSSBorderImageValue::cssText() const
{
    // Image first.
    String text(m_image->cssText());
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
    text += CSSPrimitiveValue(m_horizontalSizeRule).cssText();
    text += " ";
    text += CSSPrimitiveValue(m_verticalSizeRule).cssText();

    return text;
}

// ------------------------------------------------------------------------

FontFamilyValue::FontFamilyValue(const DeprecatedString& string)
    : CSSPrimitiveValue(String(), CSS_STRING)
{
    static const RegularExpression parenReg(" \\(.*\\)$");
    static const RegularExpression braceReg(" \\[.*\\]$");

    parsedFontName = string;
    // a language tag is often added in braces at the end. Remove it.
    parsedFontName.replace(parenReg, "");
    // remove [Xft] qualifiers
    parsedFontName.replace(braceReg, "");
}

String FontFamilyValue::cssText() const
{
    return quoteStringIfNeeded(parsedFontName);
}

String FontValue::cssText() const
{
    // font variant weight size / line-height family 

    String result("");

    if (style) {
        result += style->cssText();
    }
    if (variant) {
        if (!result.isEmpty())
            result += " ";
        result += variant->cssText();
    }
    if (weight) {
        if (!result.isEmpty())
            result += " ";
        result += weight->cssText();
    }
    if (size) {
        if (!result.isEmpty())
            result += " ";
        result += size->cssText();
    }
    if (lineHeight) {
        if (!size)
            result += " ";
        result += "/";
        result += lineHeight->cssText();
    }
    if (family) {
        if (!result.isEmpty())
            result += " ";
        result += family->cssText();
    }

    return result;
}
    

// Used for text-shadow and box-shadow
ShadowValue::ShadowValue(PassRefPtr<CSSPrimitiveValue> _x,
    PassRefPtr<CSSPrimitiveValue> _y,
    PassRefPtr<CSSPrimitiveValue> _blur,
    PassRefPtr<CSSPrimitiveValue> _color)
    : x(_x), y(_y), blur(_blur), color(_color)
{
}

String ShadowValue::cssText() const
{
    String text("");

    if (color)
        text += color->cssText();
    if (x) {
        if (!text.isEmpty())
            text += " ";
        text += x->cssText();
    }
    if (y) {
        if (!text.isEmpty())
            text += " ";
        text += y->cssText();
    }
    if (blur) {
        if (!text.isEmpty())
            text += " ";
        text += blur->cssText();
    }

    return text;
}

String CSSProperty::cssText() const
{
    return getPropertyName(id()) + ": " + m_value->cssText()
        + (isImportant() ? " !important" : "") + "; ";
}

bool operator==(const CSSProperty &a, const CSSProperty &b)
{
    return a.m_id == b.m_id && a.m_important == b.m_important && a.m_value == b.m_value;
}

}
