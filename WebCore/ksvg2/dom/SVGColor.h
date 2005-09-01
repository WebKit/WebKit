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

#ifndef KSVG_SVGColor_H
#define KSVG_SVGColor_H

#include <kdom/DOMString.h>
#include <kdom/css/CSSValue.h>
#include <ksvg2/ecma/SVGLookup.h>

class KCanvasItem;

namespace KDOM
{
    class RGBColor;
};

namespace KSVG
{
    class SVGColorImpl;
    class SVGColor : public KDOM::CSSValue
    {
    public:
        SVGColor();
        explicit SVGColor(SVGColorImpl *i);
        SVGColor(const SVGColor &other);
        SVGColor(const KDOM::CSSValue &other);
        virtual ~SVGColor();

        // Operators
        SVGColor &operator=(const SVGColor &other);
        SVGColor &operator=(const KDOM::CSSValue &other);

        // 'SVGColor' functions
        unsigned short colorType() const;

        KDOM::RGBColor rgbColor() const;

        void setRGBColor(const KDOM::DOMString &rgbColor);
        void setRGBColorICCColor(const KDOM::DOMString &rgbColor, const KDOM::DOMString &iccColor);
        void setColor(unsigned short colorType, const KDOM::DOMString &rgbColor, const KDOM::DOMString &iccColor);

        // Internal
        KSVG_INTERNAL(SVGColor)

    public: // EcmaScript section
        KDOM_GET
        KDOM_FORWARDPUT

        KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
    };
};

KSVG_DEFINE_PROTOTYPE(SVGColorProto)
KSVG_IMPLEMENT_PROTOFUNC(SVGColorProtoFunc, SVGColor)

#endif

// vim:ts=4:noet
