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

#include <qregexp.h>
#include <qfontmetrics.h>
#include <q3paintdevicemetrics.h>

#include "RectImpl.h"
#include <kdom/css/CSSHelper.h>
#include <kdom/css/cssvalues.h>
#include "CounterImpl.h"
#include "RenderStyle.h"
#include "CDFInterface.h"
#include "RGBColorImpl.h"
#include "DOMStringImpl.h"
#include "CSSPrimitiveValueImpl.h"

using namespace KDOM;

CSSPrimitiveValueImpl::CSSPrimitiveValueImpl(CDFInterface *interface) : CSSValueImpl()
{
    m_type = CSS_UNKNOWN;
    m_interface = interface;
}

CSSPrimitiveValueImpl::CSSPrimitiveValueImpl(CDFInterface *interface, int ident) : CSSValueImpl()
{
    m_value.ident = ident;
    m_type = CSS_IDENT;
    m_interface = interface;
}

CSSPrimitiveValueImpl::CSSPrimitiveValueImpl(CDFInterface *interface, double num, UnitTypes type)
{
    m_value.num = num;
    m_type = type;
    m_interface = interface;
}

CSSPrimitiveValueImpl::CSSPrimitiveValueImpl(CDFInterface *interface, DOMStringImpl *str, UnitTypes type)
{
    m_value.string = str;
    if(m_value.string)
        m_value.string->ref();
        
    m_type = type;
    m_interface = interface;
}

CSSPrimitiveValueImpl::CSSPrimitiveValueImpl(CDFInterface *interface, CounterImpl *c)
{
    m_value.counter = c;
    if(m_value.counter)
        m_value.counter->ref();
        
    m_type = CSS_COUNTER;
    m_interface = interface;
}

CSSPrimitiveValueImpl::CSSPrimitiveValueImpl(CDFInterface *interface, RectImpl *r)
{
    m_value.rect = r;
    if(m_value.rect)
        m_value.rect->ref();

    m_type = CSS_RECT;
    m_interface = interface;
}

CSSPrimitiveValueImpl::CSSPrimitiveValueImpl(CDFInterface *interface, const QRgb &color)
{
    m_value.rgbcolor = color;
    m_type = CSS_RGBCOLOR;
    m_interface = interface;
}

CSSPrimitiveValueImpl::~CSSPrimitiveValueImpl()
{
    cleanup();
}

void CSSPrimitiveValueImpl::cleanup()
{
    switch(m_type)
    {
        case CSS_STRING:
        case CSS_URI:
        case CSS_ATTR:
        {
            if(m_value.string)
                m_value.string->deref();
                
            break;
        }
        case CSS_COUNTER:
        {
            m_value.counter->deref();
            break;
        }
        case CSS_RECT:
            m_value.rect->deref();
        default:
            break;
    }

    m_type = CSS_UNKNOWN;
}

unsigned short CSSPrimitiveValueImpl::primitiveType() const
{
    return m_type;
}

void CSSPrimitiveValueImpl::setFloatValue(unsigned short unitType, float floatValue)
{
    // TODO: check exception handling!

    cleanup();
    
    // ### check if property supports this type
    if(m_type > CSS_DIMENSION)
    {
        throw new DOMExceptionImpl(SYNTAX_ERR);
        return;
    }
    
    m_value.num = floatValue;
    m_type = unitType;
}

float CSSPrimitiveValueImpl::getFloatValue(unsigned short /*unitType*/)
{
    // TODO : add unit conversion
    return m_value.num;
}

void CSSPrimitiveValueImpl::setStringValue(unsigned short stringType, DOMStringImpl *stringValue)
{
    // TODO: check exception handling!

    cleanup();
    
    if(m_type < CSS_STRING || m_type > CSS_ATTR)
    {
        throw new DOMExceptionImpl(SYNTAX_ERR);
        return;
    }
    
    if(stringType != CSS_IDENT)
    {
        if(m_value.string)
            m_value.string->deref();
        
        m_value.string = stringValue;
        
        if(m_value.string)
            m_value.string->ref();
            
        m_type = stringType;
    }
    
    // ### parse ident
}

DOMStringImpl *CSSPrimitiveValueImpl::getDOMStringValue() const
{
    return (m_type < CSS_STRING || m_type > CSS_ATTR ||
            m_type == CSS_IDENT) ? // fix IDENT
            0 : m_value.string;
}

CounterImpl *CSSPrimitiveValueImpl::getCounterValue() const
{
    return  m_type != CSS_COUNTER ? 0 : m_value.counter;
}

RectImpl *CSSPrimitiveValueImpl::getRectValue() const
{
    return m_type != CSS_RECT ? 0 : m_value.rect;
}

QRgb CSSPrimitiveValueImpl::getQRGBColorValue() const
{
    return m_type != CSS_RGBCOLOR ? 0 : m_value.rgbcolor;
}

RGBColorImpl *CSSPrimitiveValueImpl::getRGBColorValue() const
{
    return new RGBColorImpl(m_interface, getQRGBColorValue());
}

int CSSPrimitiveValueImpl::computeLength(RenderStyle *style, Q3PaintDeviceMetrics *devMetrics)
{
    double result = computeLengthFloat(style, devMetrics);

    // This conversion is imprecise, often resulting in values of, e.g., 44.99998.  We
    // need to go ahead and round if we're really close to the next integer value.
    int intResult = (int)(result + (result < 0 ? -0.01 : +0.01));
    return intResult;
}

double CSSPrimitiveValueImpl::computeLengthFloat(RenderStyle *style, Q3PaintDeviceMetrics *devMetrics)
{
    unsigned short type = primitiveType();

    double dpiY = 72.; // fallback
    if(devMetrics)
        dpiY = devMetrics->logicalDpiY();

/* FIXME
    if(!khtml::printpainter && dpiY < 96)
        dpiY = 96.;
*/

    double factor = 1.;
    switch(type)
    {
        case CSS_EMS:
        {
            factor = style->font().pixelSize();
            break;
        }
        case CSS_EXS:
        {
            QFontMetrics fm = style->fontMetrics();

            QRect b = fm.boundingRect('x');
            factor = b.height();
            break;
        }
        case CSS_PX:
            break;
        case CSS_CM:
        {
            factor = dpiY / 2.54; // 72dpi / (2.54 cm/in)
            break;
        }
        case CSS_MM:
        {
            factor = dpiY / 25.4;
            break;
        }
        case CSS_IN:
        {
            factor = dpiY;
            break;
        }
        case CSS_PT:
        {
            factor = dpiY / 72.;
            break;
        }
        case CSS_PC:
        {
            factor = dpiY * 12. / 72.; // 1pc == 12 pt
            break;
        }
        default:
            return -1;
    }

    return getFloatValue(type) * factor;
}

void CSSPrimitiveValueImpl::setCssText(DOMStringImpl *)
{
}

int CSSPrimitiveValueImpl::getIdent() const
{
    if(m_type != CSS_IDENT)
        return 0;

    return m_value.ident;
}

DOMStringImpl *CSSPrimitiveValueImpl::cssText() const
{
    // ### return the original value instead of a generated one (e.g. color
    // name if it was specified) - check what spec says about this
    DOMStringImpl *text = CSSHelper::unitTypeToString(m_type);
    if(text)
        text->ref();

    if(text && !text->isEmpty())
    {
        QString str = text->string();
        text->deref();

        text = new DOMStringImpl(QString::number(m_value.num) + text->string());
    }
    else
    {
        switch(m_type)
        {
        case CSS_UNKNOWN:
            // ###
            break;
        case CSS_NUMBER:
            KDOM_SAFE_SET(text, new DOMStringImpl(QString::number((int) m_value.num)));
            break;
        case CSS_DIMENSION:
            // ###
            KDOM_SAFE_SET(text, new DOMStringImpl(QString::number(m_value.num)));
            break;
        case CSS_STRING:
            // ###
            break;
        case CSS_URI:
            KDOM_SAFE_SET(text, new DOMStringImpl(QString::fromLatin1("url(") +
                                                  DOMString(m_value.string).string() +
                                                  QString::fromLatin1(")")));
            break;
        case CSS_IDENT:
            KDOM_SAFE_SET(text, new DOMStringImpl((m_interface ? m_interface->getValueName(m_value.ident) : "")));
            break;
        case CSS_ATTR:
            // ###
            break;
        case CSS_COUNTER:
            // ###
            break;
        case CSS_RECT:
        {
            RectImpl *rectVal = getRectValue();
            KDOM_SAFE_SET(text, new DOMStringImpl(QString::fromLatin1("rect(") +
                                                  DOMString(rectVal->top()->cssText()).string() +
                                                  QString::fromLatin1(" ") +
                                                  DOMString(rectVal->right()->cssText()).string() +
                                                  QString::fromLatin1(" ") +
                                                  DOMString(rectVal->bottom()->cssText()).string() +
                                                  QString::fromLatin1(" ") +
                                                  DOMString(rectVal->left()->cssText()).string() +
                                                  QString::fromLatin1(")")));
            break;
        }
        case CSS_RGBCOLOR:
            KDOM_SAFE_SET(text, new DOMStringImpl(QColor(m_value.rgbcolor).name()));
            break;
        default:
            break;
        }
    }

    return text;
}

unsigned short CSSPrimitiveValueImpl::cssValueType() const
{
    return CSS_PRIMITIVE_VALUE;
}

FontFamilyValueImpl::FontFamilyValueImpl(CDFInterface *interface, const QString &string) : CSSPrimitiveValueImpl(interface, DOMString(string).handle(), CSS_STRING)
{
    static const QRegExp parenReg(QString::fromLatin1(" \\(.*\\)$"));
    static const QRegExp braceReg(QString::fromLatin1(" \\[.*\\]$"));

    parsedFontName = string;
    // a language tag is often added in braces at the end. Remove it.
    parsedFontName.replace(parenReg, QString::null);
    // remove [Xft] qualifiers
    parsedFontName.replace(braceReg, QString::null);

#if 0
#ifndef APPLE_CHANGES
    const QString &available = KHTMLSettings::availableFamilies();

    parsedFontName = parsedFontName.lower();
    // kdDebug(0) << "searching for face '" << parsedFontName << "'" << endl;

    int pos = available.find( ',' + parsedFontName + ',', 0, false );
    if(pos == -1)
    {
        // many pages add extra MSs to make sure it's windows only ;(
        if(parsedFontName.startsWith("ms "))
            parsedFontName = parsedFontName.mid( 3 );
        if(parsedFontName.endsWith(" ms"))
            parsedFontName.truncate(parsedFontName.length() - 3);
        pos = available.find(",ms " + parsedFontName + ',', 0, false);
        if(pos == -1)
            pos = available.find(',' + parsedFontName + " ms,", 0, false);
    }

    if(pos != -1)
    {
       ++pos;
       int p = available.find(',', pos);
       assert( p != -1 ); // available is supposed to start and end with ,
       parsedFontName = available.mid( pos, p - pos);
       // kdDebug(0) << "going for '" << parsedFontName << "'" << endl;
    }
    else
        parsedFontName = QString::null;

#endif // !APPLE_CHANGES
#endif
}
// vim:ts=4:noet
