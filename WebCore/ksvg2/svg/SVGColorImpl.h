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
#if SVG_SUPPORT

#include "Color.h"

#include "StringImpl.h"
#include "css_valueimpl.h"

namespace WebCore
{
    class RGBColorImpl;
};

namespace WebCore
{
    class SVGColorImpl : public CSSValueImpl
    {
    public:
        SVGColorImpl();
        SVGColorImpl(DOMStringImpl *rgbColor);
        SVGColorImpl(unsigned short colorType);
        virtual ~SVGColorImpl();

        // 'SVGColor' functions
        unsigned short colorType() const;

        RGBColorImpl *rgbColor() const;

        void setRGBColor(DOMStringImpl *rgbColor);
        void setRGBColorICCColor(DOMStringImpl *rgbColor, DOMStringImpl *iccColor);
        void setColor(unsigned short colorType, DOMStringImpl *rgbColor, DOMStringImpl *iccColor);

        virtual DOMString cssText() const;

        // Helpers
        const Color &color() const;

    private:    
        Color m_qColor;
        unsigned short m_colorType;
        DOMString m_rgbColor;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
