/**
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
#include "css_valueimpl.h"
#include "css_value.h"
#include "css_ruleimpl.h"
#include "css_stylesheetimpl.h"
#include "cssparser.h"
#include "dom_exception.h"
#include "dom_string.h"
#include "dom_stringimpl.h"
#include "dom_nodeimpl.h"

#include "misc/loader.h"

#include <kdebug.h>

#include "cssvalues.h"

// Hack for debugging purposes
extern DOM::DOMString getPropertyName(unsigned short id);

using namespace DOM;

CSSStyleDeclarationImpl::CSSStyleDeclarationImpl(CSSRuleImpl *parent)
    : StyleBaseImpl(parent)
{
    m_lstValues = 0;
    m_node = 0;
}

CSSStyleDeclarationImpl::CSSStyleDeclarationImpl(CSSRuleImpl *parent, QList<CSSProperty> *lstValues)
    : StyleBaseImpl(parent)
{
    m_lstValues = lstValues;
    m_node = 0;
}

CSSStyleDeclarationImpl::~CSSStyleDeclarationImpl()
{
    delete m_lstValues;
    // we don't use refcounting for m_node, to avoid cyclic references (see ElementImpl)
}

DOMString CSSStyleDeclarationImpl::getPropertyValue( const DOMString &propertyName )
{
    CSSValueImpl *val = getPropertyCSSValue( propertyName );
    if ( !val )
	return 0;
    return val->cssText();
}

DOMString CSSStyleDeclarationImpl::getPropertyValue( int id )
{
    CSSValueImpl *val = getPropertyCSSValue( id );
    if ( !val )
	return 0;
    return val->cssText();
}

CSSValueImpl *CSSStyleDeclarationImpl::getPropertyCSSValue( const DOMString &propertyName )
{
    int id = getPropertyID(propertyName.string().ascii(), propertyName.length());
    return getPropertyCSSValue(id);
}

CSSValueImpl *CSSStyleDeclarationImpl::getPropertyCSSValue( int propertyID )
{
    if(!m_lstValues) return 0;

    unsigned int i = 0;
    while(i < m_lstValues->count())
    {
	if(propertyID == m_lstValues->at(i)->m_id) return m_lstValues->at(i)->value();
	i++;
    }
    return 0;
}


bool CSSStyleDeclarationImpl::removeProperty( int propertyID, bool onlyNonCSSHints )
{
    QListIterator<CSSProperty> lstValuesIt(*m_lstValues);
    lstValuesIt.toLast();
    while (lstValuesIt.current() && lstValuesIt.current()->m_id != propertyID)
        --lstValuesIt;
    if (lstValuesIt.current()) {
	if ( onlyNonCSSHints && !lstValuesIt.current()->nonCSSHint )
	    return false;
	m_lstValues->removeRef(lstValuesIt.current());
	return true;
	if (m_node)
	    m_node->setChanged(true);
    }
    return true;
}    
    

DOMString CSSStyleDeclarationImpl::removeProperty( const DOMString &propertyName )
{
    int id = getPropertyID(propertyName.string().lower().ascii(), propertyName.length());
    return removeProperty(id);
}

DOMString CSSStyleDeclarationImpl::removeProperty(int id)
{
    if(!m_lstValues) return 0;
    DOMString value;

    QListIterator<CSSProperty> lstValuesIt(*m_lstValues);
    lstValuesIt.toLast();
    while (lstValuesIt.current() && lstValuesIt.current()->m_id != id)
        --lstValuesIt;
    if (lstValuesIt.current()) {
	value = lstValuesIt.current()->value()->cssText();
        m_lstValues->removeRef(lstValuesIt.current());
	if (m_node)
	    m_node->setChanged(true);
    }
    return value;
}

DOMString CSSStyleDeclarationImpl::getPropertyPriority( const DOMString &propertyName )
{
    int id = getPropertyID(propertyName.string().ascii(), propertyName.length());
    if(getPropertyPriority(id))
	return DOMString("important");
    return DOMString();
}

bool CSSStyleDeclarationImpl::getPropertyPriority( int propertyID )
{
    if(!m_lstValues) return false;

    unsigned int i = 0;
    while(i < m_lstValues->count())
    {
	if(propertyID == m_lstValues->at(i)->m_id ) return m_lstValues->at(i)->m_bImportant;
	i++;
    }
    return false;
}

void CSSStyleDeclarationImpl::setProperty( const DOMString &propName, const DOMString &value, const DOMString &priority )
{
    int id = getPropertyID(propName.string().lower().ascii(), propName.length());
    if (!id) return;

    bool important = false;
    QString str = priority.string().lower();
    if(str.contains("important"))
	important = true;

    setProperty(id, value, important);
}

void CSSStyleDeclarationImpl::setProperty(const DOMString &propName, const DOMString &value, bool important, bool nonCSSHint)
{
    int id = getPropertyID(propName.string().lower().ascii(), propName.length());
    if (!id) return;
    setProperty(id, value, important, nonCSSHint);
}


void CSSStyleDeclarationImpl::setProperty(int id, const DOMString &value, bool important, bool nonCSSHint)
{
    if(!m_lstValues) {
	m_lstValues = new QList<CSSProperty>;
	m_lstValues->setAutoDelete(true);
    }
    if ( !removeProperty(id, nonCSSHint ) )
	return;
    int pos = m_lstValues->count();
    DOMString ppValue = preprocess(value.string(),true);
    parseValue(ppValue.unicode(), ppValue.unicode()+ppValue.length(), id, important, m_lstValues);

    if( nonCSSHint && pos < (int)m_lstValues->count() ) {
	CSSProperty *p = m_lstValues->at(pos);
	while ( p ) {
	    p->nonCSSHint = true;
	    p = m_lstValues->next();
	}
    } else if((unsigned) pos == m_lstValues->count() )
	{
	kdDebug( 6080 ) << "CSSStyleDeclarationImpl::setProperty invalid property: [" << getPropertyName(id).string()
					<< "] value: [" << value.string() << "]"<< endl;
	}
    if (m_node)
	m_node->setChanged(true);
}

void CSSStyleDeclarationImpl::setProperty(int id, int value, bool important, bool nonCSSHint)
{
    if(!m_lstValues) {
	m_lstValues = new QList<CSSProperty>;
	m_lstValues->setAutoDelete(true);
    }
    if ( !removeProperty(id, nonCSSHint ) )
	return;
    int pos = m_lstValues->count();
    CSSValueImpl * cssValue = new CSSPrimitiveValueImpl(value);
    setParsedValue(id, cssValue, important, m_lstValues);

    if( nonCSSHint && pos < (int)m_lstValues->count() ) {
	CSSProperty *p = m_lstValues->at(pos);
	while ( p ) {
	    p->nonCSSHint = true;
	    p = m_lstValues->next();
	}
    } else if((unsigned) pos == m_lstValues->count() )
	{
	kdDebug( 6080 ) << "CSSStyleDeclarationImpl::setProperty invalid property: [" << getPropertyName(id).string()
					<< "] value: [" << value << "]" << endl;
	}
    if (m_node)
	m_node->setChanged(true);
}

void CSSStyleDeclarationImpl::setProperty ( const DOMString &propertyString)
{
    DOMString ppPropertyString = preprocess(propertyString.string(),true);
    QList<CSSProperty> *props = parseProperties(ppPropertyString.unicode(),
						ppPropertyString.unicode()+ppPropertyString.length());
    if(!props || !props->count())
    {
	kdDebug( 6080 ) << "no properties returned!" << endl;
	return;
    }

    props->setAutoDelete(false);

    unsigned int i = 0;
    if(!m_lstValues) {
	m_lstValues = new QList<CSSProperty>;
	m_lstValues->setAutoDelete( true );
    }
    while(i < props->count())
    {
	//kdDebug( 6080 ) << "setting property" << endl;
	CSSProperty *prop = props->at(i);
	removeProperty(prop->m_id, false);
	m_lstValues->append(prop);
	i++;
    }
    delete props;
    if (m_node)
	m_node->setChanged(true);
}

void CSSStyleDeclarationImpl::setLengthProperty(int id, const DOMString &value,
						bool important, bool nonCSSHint)
{
    bool parseMode = strictParsing;
    strictParsing = false;
    setProperty( id, value, important, nonCSSHint);
    strictParsing = parseMode;
#if 0 // ### FIXME after 2.0
    if(!value.unicode() || value.length() == 0)
	return;

    if(!m_lstValues)
    {
	m_lstValues = new QList<CSSProperty>;
	m_lstValues->setAutoDelete(true);
    }

    CSSValueImpl *v = parseUnit(value.unicode(), value.unicode()+value.length(),
				INTEGER | PERCENT | LENGTH, );
    if(!v)
    {
	kdDebug( 6080 ) << "invalid length" << endl;
	return;
    }

    CSSPrimitiveValueImpl *p = static_cast<CSSPrimitiveValueImpl *>(v);
    if(p->primitiveType() == CSSPrimitiveValue::CSS_NUMBER)
    {
	// set the parsed number in pixels
	p->setPrimitiveType(CSSPrimitiveValue::CSS_PX);
    }
    CSSProperty *prop = new CSSProperty();
    prop->m_id = id;
    prop->setValue(v);
    prop->m_bImportant = important;
    prop->nonCSSHint = nonCSSHint;

    m_lstValues->append(prop);
#endif
}

unsigned long CSSStyleDeclarationImpl::length() const
{
    if(!m_lstValues) return 0;
    return m_lstValues->count();
}

DOMString CSSStyleDeclarationImpl::item( unsigned long /*index*/ )
{
    // ###
    //return m_lstValues->at(index);
    return 0;
}

CSSRuleImpl *CSSStyleDeclarationImpl::parentRule() const
{
    if( !m_parent ) return 0;
    if( m_parent->isRule() ) return static_cast<CSSRuleImpl *>(m_parent);
    return 0;
}

DOM::DOMString CSSStyleDeclarationImpl::cssText() const
{
    return DOM::DOMString();
    // ###
}

void CSSStyleDeclarationImpl::setCssText(DOM::DOMString /*str*/)
{
    // ###
}

bool CSSStyleDeclarationImpl::parseString( const DOMString &/*string*/, bool )
{
    return false;
    // ###
}


// --------------------------------------------------------------------------------------

CSSValueImpl::CSSValueImpl()
    : StyleBaseImpl()
{
}

CSSValueImpl::~CSSValueImpl()
{
}

DOM::DOMString CSSValueImpl::cssText() const
{
    return DOM::DOMString();
}

void CSSValueImpl::setCssText(DOM::DOMString /*str*/)
{
    // ###
}

DOM::DOMString CSSInheritedValueImpl::cssText() const
{
    return DOMString("inherited");
}
// ----------------------------------------------------------------------------------------

CSSValueListImpl::CSSValueListImpl()
    : CSSValueImpl()
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

unsigned short CSSValueListImpl::valueType() const
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
    // ###
    return DOM::DOMString();
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

CSSPrimitiveValueImpl::CSSPrimitiveValueImpl(float num, CSSPrimitiveValue::UnitTypes type)
{
    m_value.num = num;
    m_type = type;
}

CSSPrimitiveValueImpl::CSSPrimitiveValueImpl(const DOMString &str, CSSPrimitiveValue::UnitTypes type)
{
    m_value.string = str.implementation();
    if(m_value.string) m_value.string->ref();
    m_type = type;
}

CSSPrimitiveValueImpl::CSSPrimitiveValueImpl(const Counter &c)
{
    m_value.counter = c.handle();
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

CSSPrimitiveValueImpl::CSSPrimitiveValueImpl(const RGBColor &rgb)
{
    m_value.rgbcolor = new RGBColor(rgb);
    m_type = CSSPrimitiveValue::CSS_RGBCOLOR;
}

CSSPrimitiveValueImpl::CSSPrimitiveValueImpl(const QColor &color)
{
    m_value.rgbcolor = new RGBColor(color);
    m_type = CSSPrimitiveValue::CSS_RGBCOLOR;
}

CSSPrimitiveValueImpl::~CSSPrimitiveValueImpl()
{
    cleanup();
}

void CSSPrimitiveValueImpl::cleanup()
{
    if(m_type == CSSPrimitiveValue::CSS_RGBCOLOR)
	delete m_value.rgbcolor;
    else if(m_type < CSSPrimitiveValue::CSS_STRING || m_type == CSSPrimitiveValue::CSS_IDENT)
    { }
    else if(m_type < CSSPrimitiveValue::CSS_COUNTER)
	if(m_value.string) m_value.string->deref();
    else if(m_type == CSSPrimitiveValue::CSS_COUNTER)
	m_value.counter->deref();
    else if(m_type == CSSPrimitiveValue::CSS_RECT)
	m_value.rect->deref();
    m_type = 0;
}

unsigned short CSSPrimitiveValueImpl::primitiveType() const
{
    return m_type;
}

void CSSPrimitiveValueImpl::setFloatValue( unsigned short unitType, float floatValue, int &exceptioncode )
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

float CSSPrimitiveValueImpl::getFloatValue( unsigned short /*unitType*/)
{
    return m_value.num;
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
	m_value.string = stringValue.implementation();
	m_value.string->ref();
	m_type = stringType;
    }
    // ### parse ident
}

DOMStringImpl *CSSPrimitiveValueImpl::getStringValue(  )
{
    if(m_type < CSSPrimitiveValue::CSS_STRING) return 0;
    if(m_type > CSSPrimitiveValue::CSS_ATTR) return 0;
    if(m_type == CSSPrimitiveValue::CSS_IDENT)
    {
	// ###
	return 0;
    }
    return m_value.string;
}

CounterImpl *CSSPrimitiveValueImpl::getCounterValue(  )
{
    if(m_type != CSSPrimitiveValue::CSS_COUNTER) return 0;
    return m_value.counter;
}

RectImpl *CSSPrimitiveValueImpl::getRectValue(  )
{
    if(m_type != CSSPrimitiveValue::CSS_RECT) return 0;
    return m_value.rect;
}

RGBColor *CSSPrimitiveValueImpl::getRGBColorValue(  )
{
    if(m_type != CSSPrimitiveValue::CSS_RGBCOLOR) return 0;
    return m_value.rgbcolor;
}

unsigned short CSSPrimitiveValueImpl::valueType() const
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
	    text = DOMString(QString::number( (int)m_value.num ));
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
	    // ###
	    break;
	case CSSPrimitiveValue::CSS_URI:
	    text = DOMString( m_value.string );
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
	case CSSPrimitiveValue::CSS_RECT:
	    // ###
	    break;
	case CSSPrimitiveValue::CSS_RGBCOLOR:
	    text = m_value.rgbcolor->color().name();
	    break;
	default:
	    break;
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

CSSImageValueImpl::CSSImageValueImpl(const DOMString &url, const DOMString &baseurl, StyleBaseImpl *style)
    : CSSPrimitiveValueImpl(url, CSSPrimitiveValue::CSS_URI)
{
    khtml::DocLoader *docLoader = 0;
    StyleBaseImpl *root = style;
    while (root->parent())
	root = root->parent();
    if (root->isCSSStyleSheet())
	docLoader = static_cast<CSSStyleSheetImpl*>(root)->docLoader();

    if (docLoader)
	m_image = docLoader->requestImage(url, baseurl);
    else
	m_image = khtml::Cache::requestImage(0, url, baseurl);

    if(m_image) m_image->ref(this);
}

CSSImageValueImpl::CSSImageValueImpl()
    : CSSPrimitiveValueImpl(CSS_VAL_NONE)
{
    m_image = 0;
}

CSSImageValueImpl::~CSSImageValueImpl()
{
    if(m_image) m_image->deref(this);
}

