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

#ifndef KDOM_CSSPrimitiveValueImpl_H
#define KDOM_CSSPrimitiveValueImpl_H

#include <kdom/kdom.h>
#include <kdom/css/kdomcss.h>
#include <kdom/css/CSSValueImpl.h>

#include <qcolor.h>
class QPaintDeviceMetrics;

namespace KDOM
{
    class RenderStyle;
    class RectImpl;
    class CounterImpl;
    class RGBColorImpl;
    class CSSPrimitiveValueImpl : public CSSValueImpl
    {
    public:
        CSSPrimitiveValueImpl(CDFInterface *interface);
        CSSPrimitiveValueImpl(CDFInterface *interface, int ident);
        CSSPrimitiveValueImpl(CDFInterface *interface, double num, UnitTypes type);
        CSSPrimitiveValueImpl(CDFInterface *interface, DOMStringImpl *str, UnitTypes type);
        CSSPrimitiveValueImpl(CDFInterface *interface, CounterImpl *c);
        CSSPrimitiveValueImpl(CDFInterface *interface, RectImpl *r);
        CSSPrimitiveValueImpl(CDFInterface *interface, const QRgb &color);
        virtual ~CSSPrimitiveValueImpl();

        void cleanup();

        // 'CSSPrimitiveValue' functions
        unsigned short primitiveType() const;
        void setFloatValue(unsigned short unitType, float floatValue);
        float getFloatValue(unsigned short unitType);
        void setStringValue(unsigned short stringType, DOMStringImpl *stringValue);
        DOMStringImpl *getDOMStringValue() const;
        CounterImpl *getCounterValue() const;
        RectImpl *getRectValue() const;

        QRgb getQRGBColorValue() const;
        RGBColorImpl *getRGBColorValue() const;

        /*
         * computes a length in pixels out of the given CSSValue. Need the RenderStyle to get
         * the fontinfo in case val is defined in em or ex.
         *
         * The metrics have to be a bit different for screen and printer output.
         * For screen output we assume 1 inch == 72 px, for printer we assume 300 dpi
         *
         * this is screen/printer dependent, so we probably need a config option for this,
         * and some tool to calibrate.
         */
        int computeLength(RenderStyle *style, QPaintDeviceMetrics *devMetrics);
        double computeLengthFloat(RenderStyle *style, QPaintDeviceMetrics *devMetrics);

        // use with care!
        double floatValue(unsigned short /* unitType */) const { return m_value.num; }

        virtual bool isQuirkValue() const { return false; }

        // 'CSSValue' functions
        virtual void setCssText(DOMStringImpl *cssText);
        virtual DOMStringImpl *cssText() const;
        virtual unsigned short cssValueType() const;

        virtual bool isPrimitiveValue() const { return true; }

        int getIdent() const;            

    protected:
        int m_type;
        union
        {
            int ident;
            double num;
            DOMStringImpl *string;
            CounterImpl *counter;
            RectImpl *rect;
            QRgb rgbcolor;
        } m_value;

        friend class CSSPrimitiveValue;
        CDFInterface *m_interface;
    };

    // This value is used to handle quirky margins in reflow roots (body, td, and th) like WinIE.
    // The basic idea is that a stylesheet can use the value __qem (for quirky em) instead of em
    // in a stylesheet.  When the quirky value is used, if you're in quirks mode, the margin will
    // collapse away inside a table cell.
    class CSSQuirkPrimitiveValueImpl : public CSSPrimitiveValueImpl
    {
    public:
        CSSQuirkPrimitiveValueImpl(CDFInterface *interface, double num, UnitTypes type)
        : CSSPrimitiveValueImpl(interface, num, type) { }
        virtual ~CSSQuirkPrimitiveValueImpl() { }

        virtual bool isQuirkValue() const { return true; }
    };

    class FontFamilyValueImpl : public CSSPrimitiveValueImpl
    {
    public:
        FontFamilyValueImpl(CDFInterface *interface, const QString &string);
        const QString &fontName() const { return parsedFontName; }
        int genericFamilyType() const { return _genericFamilyType; }

    protected:
        QString parsedFontName;

    private:
        int _genericFamilyType;
    };
};

#endif

// vim:ts=4:noet
