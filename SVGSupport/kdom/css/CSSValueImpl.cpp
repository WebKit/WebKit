/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "kdomcss.h"
#include "CSSValueImpl.h"
#include "CSSValueListImpl.h"
#include "CSSPrimitiveValueImpl.h"

using namespace KDOM;

CSSValueImpl::CSSValueImpl() : StyleBaseImpl()
{
}

CSSValueImpl::~CSSValueImpl()
{
}

void CSSValueImpl::setCssText(DOMStringImpl *)
{
}

unsigned short CSSValueImpl::cssValueType() const
{
    return CSS_CUSTOM;
}

unsigned short CSSInheritedValueImpl::cssValueType() const
{
    return CSS_INHERIT;
}

DOMStringImpl *CSSInheritedValueImpl::cssText() const
{
    return new DOMStringImpl("inherit");
}

unsigned short CSSInitialValueImpl::cssValueType() const
{
    return CSS_INITIAL;
}

DOMStringImpl *CSSInitialValueImpl::cssText() const
{
    return new DOMStringImpl("initial");
}

FontValueImpl::FontValueImpl() : style(0), variant(0), weight(0), size(0), lineHeight(0), family(0)
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

DOMStringImpl *FontValueImpl::cssText() const
{
    // font variant weight size / line-height family
    DOMStringImpl *result = new DOMStringImpl();

    if(style)
        result->append(style->cssText());

    if(variant)
    {
        if(result->length() > 0)
            result->append(" ");

        result->append(variant->cssText());
    }

    if(weight)
    {
        if(result->length() > 0)
            result->append(" ");

        result->append(weight->cssText());
    }

    if(size)
    {
        if(result->length() > 0)
            result->append(" ");

        result->append(size->cssText());
    }

    if(lineHeight)
    {
        if(!size)
            result->append(" ");

        result->append("/");
        result->append(lineHeight->cssText());
    }

    if(family)
    {
        if(result->length() > 0) 
            result->append(" ");

        result->append(family->cssText());
    }

    return result;
}

unsigned short FontValueImpl::cssValueType() const
{
    return CSS_CUSTOM;
}

QuotesValueImpl::QuotesValueImpl() : levels(0)
{
}

QuotesValueImpl::~QuotesValueImpl()
{
}

unsigned short QuotesValueImpl::cssValueType() const
{
    return CSS_CUSTOM;
}

DOMStringImpl *QuotesValueImpl::cssText() const
{
    return new DOMStringImpl(QString::fromLatin1("\"") +
                             data.join(QString::fromLatin1("\" \"")) +
                             QString::fromLatin1("\""));
}

void QuotesValueImpl::addLevel(const QString& open, const QString& close)
{
    data.append(open);
    data.append(close);
    levels++;
}

QString QuotesValueImpl::openQuote(int level) const
{
    if(levels == 0)
        return QString::fromLatin1("");
    
    level--; // increments are calculated before openQuote is called
    
    //     kdDebug( 6080 ) << "Open quote level:" << level << endl;
    if(level < 0)
        level = 0;
    else
    {
        if((unsigned int)level >= levels)
            level = levels - 1;
    }

    return data[level * 2];
}

QString QuotesValueImpl::closeQuote(int level) const
{
    if(levels == 0)
        return QString::fromLatin1("");
        
    //     kdDebug( 6080 ) << "Close quote level:" << level << endl;

    if(level < 0)
        level = 0;
    else
    {
        if((unsigned int)level >= levels)
            level = levels - 1;
    }

    return data[level * 2 + 1];
}

// Used for text-shadow and box-shadow
ShadowValueImpl::ShadowValueImpl(CSSPrimitiveValueImpl *_x, CSSPrimitiveValueImpl *_y,
                                 CSSPrimitiveValueImpl *_blur, CSSPrimitiveValueImpl *_color)
{
    x = _x;
    y = _y;
    blur = _blur;
    color = _color;
}

ShadowValueImpl::~ShadowValueImpl()
{
    delete x;
    delete y;
    delete blur;
    delete color;
}

unsigned short ShadowValueImpl::cssValueType() const
{
    return CSS_CUSTOM;
}

DOMStringImpl *ShadowValueImpl::cssText() const
{
    DOMString text("");
    if(color)
        text += DOMString(color->cssText());
    
    if(x)
    {
        if(text.length() > 0)
            text += " ";
        
        text += DOMString(x->cssText());
    }
    
    if(y)
    {
        if(text.length() > 0)
            text += " ";
        
        text += DOMString(y->cssText());
    }
    
    if(blur)
    {
        if(text.length() > 0)
            text += " ";
        
        text += DOMString(blur->cssText());
    }

    return text.handle()->copy();
}

CounterActImpl::CounterActImpl(DOMStringImpl *c, short v)
{
    m_counter = c;
    if(m_counter)
        m_counter->ref();

    m_value = v;
}

CounterActImpl::~CounterActImpl()
{
    if(m_counter)
        m_counter->deref();
}

unsigned short CounterActImpl::cssValueType() const
{
    return CSS_CUSTOM;
}

DOMStringImpl *CounterActImpl::cssText() const
{
    DOMString text(m_counter);
    text += DOMString(QString::number(m_value));

    return text.handle()->copy();
}

DOMStringImpl *CounterActImpl::counter() const
{
    return m_counter;
}

short CounterActImpl::value() const
{
    return m_value;
}

void CounterActImpl::setValue(const short v)
{
    m_value = v;
}

// vim:ts=4:noet
