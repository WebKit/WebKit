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

#ifndef SVGAnimatedTemplate_H
#define SVGAnimatedTemplate_H

#ifdef SVG_SUPPORT

#include "Shared.h"

namespace WebCore {
    class SVGAngle;
    class SVGLength;
    class SVGLengthList;
    class SVGNumberList;
    class SVGPreserveAspectRatio;
    class SVGTransformList;
    class String;
    class FloatRect;

    template<typename BareType>
    class SVGAnimatedTemplate : public Shared<SVGAnimatedTemplate<BareType> >
    {
    public:
        virtual ~SVGAnimatedTemplate() { }

        virtual BareType baseVal() const = 0;
        virtual void setBaseVal(BareType newBaseVal) = 0;

        virtual BareType animVal() const = 0;
        virtual void setAnimVal(BareType newAnimVal) = 0;
    };

    // Common type definitions, to ease IDL generation...
    typedef SVGAnimatedTemplate<SVGAngle*> SVGAnimatedAngle;
    typedef SVGAnimatedTemplate<bool> SVGAnimatedBoolean;
    typedef SVGAnimatedTemplate<int> SVGAnimatedEnumeration;
    typedef SVGAnimatedTemplate<long> SVGAnimatedInteger;
    typedef SVGAnimatedTemplate<SVGLength> SVGAnimatedLength;
    typedef SVGAnimatedTemplate<SVGLengthList*> SVGAnimatedLengthList;
    typedef SVGAnimatedTemplate<double> SVGAnimatedNumber;
    typedef SVGAnimatedTemplate<SVGNumberList*> SVGAnimatedNumberList; 
    typedef SVGAnimatedTemplate<SVGPreserveAspectRatio*> SVGAnimatedPreserveAspectRatio;
    typedef SVGAnimatedTemplate<FloatRect> SVGAnimatedRect;
    typedef SVGAnimatedTemplate<String> SVGAnimatedString;
    typedef SVGAnimatedTemplate<SVGTransformList*> SVGAnimatedTransformList; 
}

#endif // SVG_SUPPORT
#endif // SVGAnimatedTemplate_H

// vim:ts=4:noet
