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

#ifndef KSVG_SVGColorImpl_H
#define KSVG_SVGColorImpl_H

#include <qcolor.h>

#include <kdom/core/DOMStringImpl.h>
#include <kdom/css/CSSValueImpl.h>

namespace KDOM
{
    class RGBColorImpl;
};

namespace KSVG
{
    class SVGColorImpl : public KDOM::CSSValueImpl
    {
    public:
        SVGColorImpl();
        SVGColorImpl(KDOM::DOMStringImpl *rgbColor);
        SVGColorImpl(unsigned short colorType);
        virtual ~SVGColorImpl();

        // 'SVGColor' functions
        unsigned short colorType() const;

        KDOM::RGBColorImpl *rgbColor() const;

        void setRGBColor(KDOM::DOMStringImpl *rgbColor);
        void setRGBColorICCColor(KDOM::DOMStringImpl *rgbColor, KDOM::DOMStringImpl *iccColor);
        void setColor(unsigned short colorType, KDOM::DOMStringImpl *rgbColor, KDOM::DOMStringImpl *iccColor);

        virtual KDOM::DOMString cssText() const;

        // Helpers
        const QColor &color() const;

    private:    
        QColor m_qColor;
        unsigned short m_colorType;
        KDOM::DOMStringImpl *m_rgbColor;
    };
};

#endif

// vim:ts=4:noet
