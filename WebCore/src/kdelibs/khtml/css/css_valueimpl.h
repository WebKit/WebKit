/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
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
 *
 * $Id$
 */
#ifndef _CSS_css_valueimpl_h_
#define _CSS_css_valueimpl_h_

#include "dom/css_value.h"
#include "dom/dom_string.h"
#include "css/cssparser.h"
#include "misc/loader_client.h"

#include <qintdict.h>

namespace khtml {
    class CachedImage;
};

namespace DOM {

class CSSRuleImpl;
class CSSValueImpl;
class NodeImpl;
class CounterImpl;


class CSSStyleDeclarationImpl : public StyleBaseImpl
{
public:
    CSSStyleDeclarationImpl(CSSRuleImpl *parentRule);
    CSSStyleDeclarationImpl(CSSRuleImpl *parentRule, QPtrList<CSSProperty> *lstValues);

    virtual ~CSSStyleDeclarationImpl();

    unsigned long length() const;
    CSSRuleImpl *parentRule() const;
    DOM::DOMString removeProperty( int propertyID, bool NonCSSHints = false );
    void setProperty ( int propertyId, const DOM::DOMString &value, bool important = false, bool nonCSSHint = false);
    void setProperty ( int propertyId, int value, bool important = false, bool nonCSSHint = false);
    // this treats integers as pixels!
    // needed for conversion of html attributes
    void setLengthProperty(int id, const DOM::DOMString &value, bool important, bool nonCSSHint = true);

    // add a whole, unparsed property
    void setProperty ( const DOMString &propertyString);
    DOM::DOMString item ( unsigned long index );

    DOM::DOMString cssText() const;
    void setCssText(DOM::DOMString str);

    virtual bool isStyleDeclaration() { return true; }

    virtual bool parseString( const DOMString &string, bool = false );

    CSSValueImpl *getPropertyCSSValue( int propertyID );
    bool getPropertyPriority( int propertyID );

    QPtrList<CSSProperty> *values() { return m_lstValues; }
    void setNode(NodeImpl *_node) { m_node = _node; }

protected:
    QPtrList<CSSProperty> *m_lstValues;
    NodeImpl *m_node;
};

class CSSValueImpl : public StyleBaseImpl
{
public:
    CSSValueImpl();

    virtual ~CSSValueImpl();

    virtual unsigned short cssValueType() const = 0;

    virtual DOM::DOMString cssText() const;
    void setCssText(DOM::DOMString str);

    virtual bool isValue() { return true; }
};

class CSSInheritedValueImpl : public CSSValueImpl
{
public:
    CSSInheritedValueImpl() : CSSValueImpl() {}
    virtual ~CSSInheritedValueImpl() {}

    virtual unsigned short cssValueType() const { return CSSValue::CSS_INHERIT; }
    virtual DOM::DOMString cssText() const;
};


class CSSValueListImpl : public CSSValueImpl
{
public:
    CSSValueListImpl();

    virtual ~CSSValueListImpl();

    unsigned long length() const { return m_values.count(); }
    CSSValueImpl *item ( unsigned long index ) { return m_values.at(index); }

    virtual bool isValueList() { return true; }

    virtual unsigned short cssValueType() const;

    void append(CSSValueImpl *val);
    virtual DOM::DOMString cssText() const;

protected:
    QPtrList<CSSValueImpl> m_values;
};


class Counter;
class RGBColor;
class Rect;

class CSSPrimitiveValueImpl : public CSSValueImpl
{
public:
    CSSPrimitiveValueImpl();
    CSSPrimitiveValueImpl(int ident);
    CSSPrimitiveValueImpl(float num, CSSPrimitiveValue::UnitTypes type);
    CSSPrimitiveValueImpl(const DOMString &str, CSSPrimitiveValue::UnitTypes type);
    CSSPrimitiveValueImpl(const Counter &c);
    CSSPrimitiveValueImpl( RectImpl *r);
    CSSPrimitiveValueImpl(const RGBColor &rgb);
    CSSPrimitiveValueImpl(const QColor &color);

    virtual ~CSSPrimitiveValueImpl();

    void cleanup();

    unsigned short primitiveType() const {
	    return m_type;
    }

    // use with care!!!
    void setPrimitiveType(unsigned short type) { m_type = type; }
    void setFloatValue ( unsigned short unitType, float floatValue, int &exceptioncode );
    float getFloatValue ( unsigned short/* unitType */) const {
	return m_value.num;
    }

    void setStringValue ( unsigned short stringType, const DOM::DOMString &stringValue, int &exceptioncode );
    DOM::DOMStringImpl *getStringValue () const {
	return ( ( m_type < CSSPrimitiveValue::CSS_STRING ||
		   m_type > CSSPrimitiveValue::CSS_ATTR ||
		   m_type == CSSPrimitiveValue::CSS_IDENT ) ? // fix IDENT
		 0 : m_value.string );
    }
    CounterImpl *getCounterValue () const {
        return ( m_type != CSSPrimitiveValue::CSS_COUNTER ? 0 : m_value.counter );
    }

    RectImpl *getRectValue () const {
	return ( m_type != CSSPrimitiveValue::CSS_RECT ? 0 : m_value.rect );
    }

    RGBColor *getRGBColorValue () const {
	return ( m_type != CSSPrimitiveValue::CSS_RGBCOLOR ? 0 : m_value.rgbcolor );
    }

    virtual bool isPrimitiveValue() const { return true; }
    virtual unsigned short cssValueType() const;

    int getIdent();

    virtual bool parseString( const DOMString &string, bool = false);
    virtual DOM::DOMString cssText() const;

protected:
    int m_type;
    union {
	int ident;
	float num;
	DOM::DOMStringImpl *string;
	CounterImpl *counter;
	RectImpl *rect;
	RGBColor *rgbcolor;
    } m_value;
};

class CounterImpl : public DomShared {
public:
    CounterImpl() { }
    DOMString identifier() const { return m_identifier; }
    DOMString listStyle() const { return m_listStyle; }
    DOMString separator() const { return m_separator; }

    DOMString m_identifier;
    DOMString m_listStyle;
    DOMString m_separator;
};

class RectImpl : public DomShared {
public:
    RectImpl();
    ~RectImpl();

    CSSPrimitiveValueImpl *top() { return m_top; }
    CSSPrimitiveValueImpl *right() { return m_right; }
    CSSPrimitiveValueImpl *bottom() { return m_bottom; }
    CSSPrimitiveValueImpl *left() { return m_left; }

    void setTop( CSSPrimitiveValueImpl *top );
    void setRight( CSSPrimitiveValueImpl *right );
    void setBottom( CSSPrimitiveValueImpl *bottom );
    void setLeft( CSSPrimitiveValueImpl *left );
protected:
    CSSPrimitiveValueImpl *m_top;
    CSSPrimitiveValueImpl *m_right;
    CSSPrimitiveValueImpl *m_bottom;
    CSSPrimitiveValueImpl *m_left;
};

class CSSImageValueImpl : public CSSPrimitiveValueImpl, public khtml::CachedObjectClient
{
public:
    CSSImageValueImpl(const DOMString &url, StyleBaseImpl *style);
    CSSImageValueImpl();
    virtual ~CSSImageValueImpl();

    khtml::CachedImage *image() { return m_image; }
protected:
    khtml::CachedImage *m_image;
};

class FontFamilyValueImpl : public CSSPrimitiveValueImpl
{
public:
    FontFamilyValueImpl( const QString &string);
    const QString &fontName() const { return parsedFontName; }
protected:
    QString parsedFontName;
};

// ------------------------------------------------------------------------------

// another helper class
class CSSProperty
{
public:
    CSSProperty()
    {
	m_id = -1;
	m_value = 0;
	m_bImportant = false;
	nonCSSHint = false;
    }
    ~CSSProperty() {
	if(m_value) m_value->deref();
    }

    void setValue(CSSValueImpl *val) {
	if ( val != m_value ) {
	    if(m_value) m_value->deref();
	    m_value = val;
	    if(m_value) m_value->ref();
	}
    }

    CSSValueImpl *value() { return m_value; }

    // make sure the following fits in 4 bytes.
    int  m_id 		: 29;
    bool m_bImportant 	: 1;
    bool nonCSSHint 	: 1;
protected:
    CSSValueImpl *m_value;
};


}; // namespace

#endif
