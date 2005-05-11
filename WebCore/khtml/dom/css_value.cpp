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
 */

#include "dom/css_rule.h"
#include "dom/dom_exception.h"

#include "css/css_valueimpl.h"

namespace DOM {

CSSStyleDeclaration::CSSStyleDeclaration()
{
    impl = 0;
}

CSSStyleDeclaration::CSSStyleDeclaration(const CSSStyleDeclaration &other)
{
    impl = other.impl;
    if(impl) impl->ref();
}

CSSStyleDeclaration::CSSStyleDeclaration(CSSStyleDeclarationImpl *i)
{
    impl = i;
    if(impl) impl->ref();
}

CSSStyleDeclaration &CSSStyleDeclaration::operator = (const CSSStyleDeclaration &other)
{
    if ( impl != other.impl ) {
    if(impl) impl->deref();
    impl = other.impl;
    if(impl) impl->ref();
    }
    return *this;
}

CSSStyleDeclaration::~CSSStyleDeclaration()
{
    if(impl) impl->deref();
}

DOMString CSSStyleDeclaration::cssText() const
{
    if(!impl) return DOMString();
    return impl->cssText();
}

static void throwException(int exceptioncode)
{
    if (exceptioncode >= CSSException::_EXCEPTION_OFFSET)
	throw CSSException(exceptioncode - CSSException::_EXCEPTION_OFFSET);
    if (exceptioncode)
	throw DOMException(exceptioncode);
}

void CSSStyleDeclaration::setCssText( const DOMString &value )
{
    if(!impl) return;
    int exceptionCode = 0;
    impl->setCssText(value, exceptionCode);
    throwException(exceptionCode);
}

DOMString CSSStyleDeclaration::getPropertyValue( const DOMString &propertyName )
{
    if(!impl) return DOMString();
    return impl->getPropertyValue(propertyName);
}

CSSValue CSSStyleDeclaration::getPropertyCSSValue( const DOMString &propertyName )
{
    if(!impl) return 0;
    return impl->getPropertyCSSValue(propertyName);
}

DOMString CSSStyleDeclaration::removeProperty( const DOMString &property )
{
    if(!impl) return DOMString();
    int exceptionCode = 0;
    DOMString result = impl->removeProperty( property, exceptionCode );
    throwException(exceptionCode);
    return result;
}

DOMString CSSStyleDeclaration::getPropertyPriority( const DOMString &propertyName )
{
    if(!impl) return DOMString();
    return impl->getPropertyPriority(propertyName);
}

void CSSStyleDeclaration::setProperty( const DOMString &propName, const DOMString &value, const DOMString &priority )
{
    if(!impl) return;
    int exceptionCode;
    impl->setProperty( propName, value, priority, exceptionCode );
    throwException(exceptionCode);
}

unsigned long CSSStyleDeclaration::length() const
{
    if(!impl) return 0;
    return impl->length();
}

DOMString CSSStyleDeclaration::item( unsigned long index )
{
    if(!impl) return DOMString();
    return impl->item( index );
}

CSSRule CSSStyleDeclaration::parentRule() const
{
    if(!impl) return 0;
    return impl->parentRule();
}

CSSStyleDeclarationImpl *CSSStyleDeclaration::handle() const
{
    return impl;
}

bool CSSStyleDeclaration::isNull() const
{
    return (impl == 0);
}

// ----------------------------------------------------------

CSSValue::CSSValue()
{
    impl = 0;
}

CSSValue::CSSValue(const CSSValue &other)
{
    impl = other.impl;
    if(impl) impl->ref();
}

CSSValue::CSSValue(CSSValueImpl *i)
{
    impl = i;
    if(impl) impl->ref();
}

CSSValue &CSSValue::operator = (const CSSValue &other)
{
    if ( impl != other.impl ) {
    if(impl) impl->deref();
    impl = other.impl;
    if(impl) impl->ref();
    }
    return *this;
}

CSSValue::~CSSValue()
{
    if(impl) impl->deref();
}

DOMString CSSValue::cssText() const
{
    if(!impl) return DOMString();
    return ((CSSValueImpl *)impl)->cssText();
}

void CSSValue::setCssText(const DOMString &)
{
    // ### not implemented
}

unsigned short CSSValue::cssValueType() const
{
    if(!impl) return 0;
    return ((CSSValueImpl *)impl)->cssValueType();
}

bool CSSValue::isCSSValueList() const
{
    if(!impl) return false;
    return ((CSSValueImpl *)impl)->isValueList();
}

bool CSSValue::isCSSPrimitiveValue() const
{
    if(!impl) return false;
    return ((CSSValueImpl *)impl)->isPrimitiveValue();
}

CSSValueImpl *CSSValue::handle() const
{
    return impl;
}

bool CSSValue::isNull() const
{
    return (impl == 0);
}

// ----------------------------------------------------------

CSSValueList::CSSValueList() : CSSValue()
{
}

CSSValueList::CSSValueList(const CSSValueList &other) : CSSValue(other)
{
}

CSSValueList::CSSValueList(const CSSValue &other)
{
   impl = 0;
   operator=(other);
}

CSSValueList::CSSValueList(CSSValueListImpl *impl) : CSSValue(impl)
{
}

CSSValueList &CSSValueList::operator = (const CSSValueList &other)
{
    if ( impl != other.impl ) {
    if (impl) impl->deref();
    impl = other.handle();
    if (impl) impl->ref();
    }
    return *this;
}

CSSValueList &CSSValueList::operator = (const CSSValue &other)
{
    CSSValueImpl *ohandle = other.handle() ;
    if ( impl != ohandle ) {
    if (impl) impl->deref();
    if (!other.isNull() && !other.isCSSValueList()) {
	impl = 0;
	} else {
	    impl = ohandle;
    if (impl) impl->ref();
	}
    }
    return *this;
}

CSSValueList::~CSSValueList()
{
}

unsigned long CSSValueList::length() const
{
    if(!impl) return 0;
    return ((CSSValueListImpl *)impl)->length();
}

CSSValue CSSValueList::item( unsigned long index )
{
    if(!impl) return 0;
    return ((CSSValueListImpl *)impl)->item( index );
}

// ----------------------------------------------------------

CSSPrimitiveValue::CSSPrimitiveValue() : CSSValue()
{
}

CSSPrimitiveValue::CSSPrimitiveValue(const CSSPrimitiveValue &other) : CSSValue(other)
{
}

CSSPrimitiveValue::CSSPrimitiveValue(const CSSValue &other) : CSSValue(other)
{
    impl = 0;
    operator=(other);
}

CSSPrimitiveValue::CSSPrimitiveValue(CSSPrimitiveValueImpl *impl) : CSSValue(impl)
{
}

CSSPrimitiveValue &CSSPrimitiveValue::operator = (const CSSPrimitiveValue &other)
{
    if ( impl != other.impl ) {
    if (impl) impl->deref();
    impl = other.handle();
    if (impl) impl->ref();
    }
    return *this;
}

CSSPrimitiveValue &CSSPrimitiveValue::operator = (const CSSValue &other)
{
    CSSValueImpl *ohandle = other.handle();
    if ( impl != ohandle ) {
    if (impl) impl->deref();
    if (!other.isNull() && !other.isCSSPrimitiveValue()) {
	impl = 0;
	} else {
	    impl = ohandle;
    if (impl) impl->ref();
	}
    }
    return *this;
}

CSSPrimitiveValue::~CSSPrimitiveValue()
{
}

unsigned short CSSPrimitiveValue::primitiveType() const
{
    if(!impl) return 0;
    return ((CSSPrimitiveValueImpl *)impl)->primitiveType();
}

void CSSPrimitiveValue::setFloatValue( unsigned short unitType, float floatValue )
{
    if(!impl) return;
    int exceptioncode = 0;
    ((CSSPrimitiveValueImpl *)impl)->setFloatValue( unitType, floatValue, exceptioncode );
    throwException(exceptioncode);
}

float CSSPrimitiveValue::getFloatValue( unsigned short unitType )
{
    if(!impl) return 0;
    // ### add unit conversion
    if(primitiveType() != unitType)
	throw CSSException(CSSException::SYNTAX_ERR);
    return ((CSSPrimitiveValueImpl *)impl)->getFloatValue( unitType );
}

void CSSPrimitiveValue::setStringValue( unsigned short stringType, const DOMString &stringValue )
{
    int exceptioncode = 0;
    if(impl)
        ((CSSPrimitiveValueImpl *)impl)->setStringValue( stringType, stringValue, exceptioncode );
    throwException(exceptioncode);

}

DOMString CSSPrimitiveValue::getStringValue(  )
{
    if(!impl) return DOMString();
    return ((CSSPrimitiveValueImpl *)impl)->getStringValue(  );
}

Counter CSSPrimitiveValue::getCounterValue(  )
{
    if(!impl) return Counter();
    return ((CSSPrimitiveValueImpl *)impl)->getCounterValue(  );
}

Rect CSSPrimitiveValue::getRectValue(  )
{
    if(!impl) return Rect();
    return ((CSSPrimitiveValueImpl *)impl)->getRectValue(  );
}

RGBColor CSSPrimitiveValue::getRGBColorValue(  )
{
    // ###
    return RGBColor();
    //if(!impl) return RGBColor();
    //return ((CSSPrimitiveValueImpl *)impl)->getRGBColorValue(  );
}

// -------------------------------------------------------------------

Counter::Counter()
{
}

Counter::Counter(const Counter &/*other*/)
{
    impl = 0;
}

Counter &Counter::operator = (const Counter &other)
{
    if ( impl != other.impl ) {
    if (impl) impl->deref();
    impl = other.impl;
    if (impl) impl->ref();
    }
    return *this;
}

Counter::Counter(CounterImpl *i)
{
    impl = i;
    if (impl) impl->ref();
}

Counter::~Counter()
{
    if (impl) impl->deref();
}

DOMString Counter::identifier() const
{
  if (!impl) return DOMString();
  return impl->identifier();
}

DOMString Counter::listStyle() const
{
  if (!impl) return DOMString();
  return impl->listStyle();
}

DOMString Counter::separator() const
{
  if (!impl) return DOMString();
  return impl->separator();
}

CounterImpl *Counter::handle() const
{
    return impl;
}

bool Counter::isNull() const
{
    return (impl == 0);
}

// --------------------------------------------------------------------

RGBColor::RGBColor()
{
}

RGBColor::RGBColor(const RGBColor &other)
{
    m_color = other.m_color;
}

RGBColor::RGBColor(const QColor &color)
{
    m_color = color;
}

RGBColor &RGBColor::operator = (const RGBColor &other)
{
    m_color = other.m_color;
    return *this;
}

RGBColor::~RGBColor()
{
}

CSSPrimitiveValue RGBColor::red() const
{
    return new CSSPrimitiveValueImpl((float)m_color.red(), CSSPrimitiveValue::CSS_DIMENSION);
}

CSSPrimitiveValue RGBColor::green() const
{
    return new CSSPrimitiveValueImpl((float)m_color.green(), CSSPrimitiveValue::CSS_DIMENSION);
}

CSSPrimitiveValue RGBColor::blue() const
{
    return new CSSPrimitiveValueImpl((float)m_color.blue(), CSSPrimitiveValue::CSS_DIMENSION);
}


// ---------------------------------------------------------------------

Rect::Rect()
{
    impl = 0;
}

Rect::Rect(const Rect &other)
{
    impl = other.impl;
    if (impl) impl->ref();
}

Rect::Rect(RectImpl *i)
{
    impl = i;
    if (impl) impl->ref();
}

Rect &Rect::operator = (const Rect &other)
{
    if ( impl != other.impl ) {
    if (impl) impl->deref();
    impl = other.impl;
    if (impl) impl->ref();
    }
    return *this;
}

Rect::~Rect()
{
    if (impl) impl->deref();
}

CSSPrimitiveValue Rect::top() const
{
    if (!impl) return 0;
    return impl->top();
}

CSSPrimitiveValue Rect::right() const
{
    if (!impl) return 0;
    return impl->right();
}

CSSPrimitiveValue Rect::bottom() const
{
    if (!impl) return 0;
    return impl->bottom();
}

CSSPrimitiveValue Rect::left() const
{
    if (!impl) return 0;
    return impl->left();
}

RectImpl *Rect::handle() const
{
    return impl;
}

bool Rect::isNull() const
{
    return (impl == 0);
}

} // namespace DOM
