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

#ifndef KSVG_SVGFETurbulenceElement_H
#define KSVG_SVGFETurbulenceElement_H

#include <ksvg2/dom/SVGElement.h>
#include <ksvg2/dom/SVGFilterPrimitiveStandardAttributes.h>

namespace KSVG
{
    class SVGAnimatedInteger;
    class SVGAnimatedEnumeration;
    class SVGAnimatedNumber;
    class SVGFETurbulenceElementImpl;

    class SVGFETurbulenceElement :  public SVGElement,
                                      public SVGFilterPrimitiveStandardAttributes
    {
    public:
        SVGFETurbulenceElement();
        explicit SVGFETurbulenceElement(SVGFETurbulenceElementImpl *i);
        SVGFETurbulenceElement(const SVGFETurbulenceElement &other);
        SVGFETurbulenceElement(const KDOM::Node &other);
        virtual ~SVGFETurbulenceElement();

        // Operators
        SVGFETurbulenceElement &operator=(const SVGFETurbulenceElement &other);
        SVGFETurbulenceElement &operator=(const KDOM::Node &other);

        // 'SVGFETurbulencelement' functions
        SVGAnimatedNumber baseFrequencyX() const;
        SVGAnimatedNumber baseFrequencyY() const;
        SVGAnimatedInteger numOctaves() const;
        SVGAnimatedNumber seed() const;
        SVGAnimatedEnumeration stitchTiles() const;
        SVGAnimatedEnumeration type() const;

        // Internal
        KSVG_INTERNAL(SVGFETurbulenceElement)

    public: // EcmaScript section
        KDOM_GET
        KDOM_FORWARDPUT

        KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
    };
};

#endif

// vim:ts=4:noet
