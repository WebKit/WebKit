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
 */

#include "dom/css_value.h"
#include "dom/dom_exception.h"
#include "dom/dom_string.h"

#include "css/css_valueimpl.h"
#include "css/css_ruleimpl.h"
#include "css/css_stylesheetimpl.h"
#include "css/cssparser.h"
#include "css/cssvalues.h"

#include "xml/dom_stringimpl.h"
#include "xml/dom_docimpl.h"

#include "misc/loader.h"

#include "rendering/render_style.h"

#include <kdebug.h>
#include <qregexp.h>
#include <qpaintdevice.h>
#include <qpaintdevicemetrics.h>

// Hack for debugging purposes
extern DOM::DOMString getPropertyName(unsigned short id);

using namespace DOM;

CSSStyleDeclarationImpl::CSSStyleDeclarationImpl(CSSRuleImpl *parent)
    : StyleBaseImpl(parent)
{
    m_lstValues = 0;
    m_node = 0;
}

CSSStyleDeclarationImpl::CSSStyleDeclarationImpl(CSSRuleImpl *parent, QPtrList<CSSProperty> *lstValues)
    : StyleBaseImpl(parent)
{
    m_lstValues = lstValues;
    m_node = 0;
}

CSSStyleDeclarationImpl&  CSSStyleDeclarationImpl::operator= (const CSSStyleDeclarationImpl& o)
{
    // don't attach it to the same node, just leave the current m_node value
    delete m_lstValues;
    m_lstValues = 0;
    if (o.m_lstValues) {
        m_lstValues = new QPtrList<CSSProperty>;
        m_lstValues->setAutoDelete( true );

        QPtrListIterator<CSSProperty> lstValuesIt(*o.m_lstValues);
        for (lstValuesIt.toFirst(); lstValuesIt.current(); ++lstValuesIt)
            m_lstValues->append(new CSSProperty(*lstValuesIt.current()));
    }

    return *this;
}

CSSStyleDeclarationImpl::~CSSStyleDeclarationImpl()
{
    delete m_lstValues;
    // we don't use refcounting for m_node, to avoid cyclic references (see ElementImpl)
}

CSSValueImpl *CSSStyleDeclarationImpl::getPropertyCSSValue( int propertyID )
{
    if(!m_lstValues) return 0;

    QPtrListIterator<CSSProperty> lstValuesIt(*m_lstValues);
    CSSProperty *current;
    for ( lstValuesIt.toLast(); (current = lstValuesIt.current()); --lstValuesIt )
        if (current->m_id == propertyID && !current->nonCSSHint)
            return current->value();
    return 0;
}

DOMString CSSStyleDeclarationImpl::removeProperty( int propertyID, bool NonCSSHint )
{
    if(!m_lstValues) return DOMString();
    DOMString value;

    QPtrListIterator<CSSProperty> lstValuesIt(*m_lstValues);
     CSSProperty *current;
     for ( lstValuesIt.toLast(); (current = lstValuesIt.current()); --lstValuesIt )
         if (current->m_id == propertyID && NonCSSHint == current->nonCSSHint) {
             value = current->value()->cssText();
             m_lstValues->removeRef(current);
            setChanged();
	     break;
        }

    return value;
}

void CSSStyleDeclarationImpl::setChanged()
{
    if (m_node) {
        m_node->setChanged();
        return;
    }

    // ### quick&dirty hack for KDE 3.0... make this MUCH better! (Dirk)
    for (StyleBaseImpl* stylesheet = this; stylesheet; stylesheet = stylesheet->parent())
        if (stylesheet->isCSSStyleSheet()) {
            static_cast<CSSStyleSheetImpl*>(stylesheet)->doc()->updateStyleSelector();
            break;
        }
}

bool CSSStyleDeclarationImpl::getPropertyPriority( int propertyID )
{
    if ( m_lstValues) {
	QPtrListIterator<CSSProperty> lstValuesIt(*m_lstValues);
	CSSProperty *current;
	for ( lstValuesIt.toFirst(); (current = lstValuesIt.current()); ++lstValuesIt ) {
	    if( propertyID == current->m_id )
		return current->m_bImportant;
	}
    }
    return false;
}

void CSSStyleDeclarationImpl::setProperty(int id, const DOMString &value, bool important, bool nonCSSHint)
{
    if(!m_lstValues) {
	m_lstValues = new QPtrList<CSSProperty>;
	m_lstValues->setAutoDelete(true);
    }
    removeProperty(id, nonCSSHint );

    DOMString ppValue = preprocess(value.string(),true);
    bool success = parseValue(ppValue.unicode(), ppValue.unicode()+ppValue.length(), id, important, nonCSSHint, m_lstValues);

    if(!success)
	kdDebug( 6080 ) << "CSSStyleDeclarationImpl::setProperty invalid property: [" << getPropertyName(id).string()
					<< "] value: [" << value.string() << "]"<< endl;
    else
        setChanged();
}

void CSSStyleDeclarationImpl::setProperty(int id, int value, bool important, bool nonCSSHint)
{
    if(!m_lstValues) {
	m_lstValues = new QPtrList<CSSProperty>;
	m_lstValues->setAutoDelete(true);
    }
    removeProperty(id, nonCSSHint );

    CSSValueImpl * cssValue = new CSSPrimitiveValueImpl(value);
    setParsedValue(id, cssValue, important, nonCSSHint, m_lstValues);
    setChanged();
}

void CSSStyleDeclarationImpl::setProperty ( const DOMString &propertyString)
{
    DOMString ppPropertyString = preprocess(propertyString.string(),true);
    QPtrList<CSSProperty> *props = parseProperties(ppPropertyString.unicode(),
						ppPropertyString.unicode()+ppPropertyString.length());
    if(!props || !props->count())
    {
	kdDebug( 6080 ) << "no properties returned!" << endl;
	return;
    }

    props->setAutoDelete(false);

    if(!m_lstValues) {
	m_lstValues = new QPtrList<CSSProperty>;
	m_lstValues->setAutoDelete( true );
    }

    CSSProperty *prop = props->first();
    while( prop ) {
	removeProperty(prop->m_id, false);
	m_lstValues->append(prop);
 	prop = props->next();
    }

    delete props;
    setChanged();
}

void CSSStyleDeclarationImpl::setLengthProperty(int id, const DOM::DOMString &value, bool important, bool nonCSSHint)
{
    bool parseMode = strictParsing;
    strictParsing = false;
    setProperty( id, value, important, nonCSSHint);
    strictParsing = parseMode;
}

unsigned long CSSStyleDeclarationImpl::length() const
{
    return m_lstValues ? m_lstValues->count() : 0;
}

DOMString CSSStyleDeclarationImpl::item( unsigned long /*index*/ )
{
    // ###
    //return m_lstValues->at(index);
    return DOMString();
}

CSSRuleImpl *CSSStyleDeclarationImpl::parentRule() const
{
    return (m_parent && m_parent->isRule() ) ?
	static_cast<CSSRuleImpl *>(m_parent) : 0;
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
	{ if(m_value.string) m_value.string->deref(); }
    else if(m_type == CSSPrimitiveValue::CSS_COUNTER)
	m_value.counter->deref();
    else if(m_type == CSSPrimitiveValue::CSS_RECT)
	m_value.rect->deref();
    m_type = 0;
}

int CSSPrimitiveValueImpl::computeLength( khtml::RenderStyle *style, QPaintDeviceMetrics *devMetrics )
{
    return ( int ) computeLengthFloat( style, devMetrics );
}

float CSSPrimitiveValueImpl::computeLengthFloat( khtml::RenderStyle *style, QPaintDeviceMetrics *devMetrics)
{
    unsigned short type = primitiveType();

    float dpiY = 72.; // fallback
    if ( devMetrics )
        dpiY = devMetrics->logicalDpiY();
    if ( !khtml::printpainter && dpiY < 72 )
        dpiY = 72.;

    float factor = 1.;
    switch(type)
    {
        case CSSPrimitiveValue::CSS_EMS:
            factor = style->font().pixelSize();
            break;
        case CSSPrimitiveValue::CSS_EXS:
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

#ifdef APPLE_CHANGES
// Compute point equivalent size for each unit type.  OS X fonts are all specified in 
// device independent point size, so don't apply DPI corrections.
float CSSPrimitiveValueImpl::computePointFloat( khtml::RenderStyle *style, QPaintDeviceMetrics *devMetrics)
{
    unsigned short type = primitiveType();

    float dpiY = 72.; // fallback
    if ( devMetrics )
        dpiY = devMetrics->logicalDpiY();
    if ( !khtml::printpainter && dpiY < 72 )
        dpiY = 72.;

    float factor = 1.;
    switch(type)
    {
        case CSSPrimitiveValue::CSS_EMS:
            factor = style->font().pixelSize();
            break;
        case CSSPrimitiveValue::CSS_EXS:
        {
            QFontMetrics fm = style->fontMetrics();
            factor = fm.xHeight();
            break;
        }
        case CSSPrimitiveValue::CSS_PX:
            factor = 72./dpiY;
            break;
        case CSSPrimitiveValue::CSS_CM:
            factor = 72./2.54; //(2.54 cm/in)
            break;
        case CSSPrimitiveValue::CSS_MM:
            factor = 72./25.4; //(25.4 cm/in)
            break;
        case CSSPrimitiveValue::CSS_IN:
            factor = 72.;
            break;
        case CSSPrimitiveValue::CSS_PT:
            break;
        case CSSPrimitiveValue::CSS_PC:
        // 1 pc == 12 pt
            factor = 12.;
            break;
        default:
            return -1;
    }
    return getFloatValue(type)*factor;
}
#endif

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

CSSImageValueImpl::CSSImageValueImpl(const DOMString &url, StyleBaseImpl *style)
    : CSSPrimitiveValueImpl(url, CSSPrimitiveValue::CSS_URI)
{
    khtml::DocLoader *docLoader = 0;
    StyleBaseImpl *root = style;
    while (root->parent())
	root = root->parent();
    if (root->isCSSStyleSheet())
	docLoader = static_cast<CSSStyleSheetImpl*>(root)->docLoader();

    if (docLoader)
	m_image = docLoader->requestImage(url);
    else
	m_image = khtml::Cache::requestImage(0, url);

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

// ------------------------------------------------------------------------

FontFamilyValueImpl::FontFamilyValueImpl( const QString &string)
    : CSSPrimitiveValueImpl( DOMString(string), CSSPrimitiveValue::CSS_STRING)
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
    if(face == "serif" ||
       face == "sans-serif" ||
       face == "cursive" ||
       face == "fantasy" ||
       face == "monospace" ||
       face == "konq_default") {
	parsedFontName = face;
    } else {
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
    }
#endif // !APPLE_CHANGES
}
