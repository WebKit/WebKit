/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Oliver Hunt <ojh16@student.canterbury.ac.nz>

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

#ifndef KSVG_SVGFELightElementImpl_H
#define KSVG_SVGFELightElementImpl_H
#if SVG_SUPPORT

#include "SVGElement.h"
#include "KCanvasFilters.h"

namespace WebCore
{
    class SVGAnimatedNumber;
    class SVGAnimatedNumberList;
    class SVGAnimatedEnumeration;
    
    class SVGFELightElement : public SVGElement
    {
    public:
        SVGFELightElement(const QualifiedName&, Document*);
        virtual ~SVGFELightElement();
        
        // 'SVGComponentTransferFunctionElement' functions
        SVGAnimatedNumber *azimuth() const;
        SVGAnimatedNumber *elevation() const;
        SVGAnimatedNumber *x() const;
        SVGAnimatedNumber *y() const;
        SVGAnimatedNumber *z() const;
        SVGAnimatedNumber *pointsAtX() const;
        SVGAnimatedNumber *pointsAtY() const;
        SVGAnimatedNumber *pointsAtZ() const;
        SVGAnimatedNumber *specularExponent() const;
        SVGAnimatedNumber *limitingConeAngle() const;
        
        virtual KCLightSource *lightSource() const = 0;
        
        // Derived from: 'Element'
        virtual void parseMappedAttribute(MappedAttribute *attr);        
    private:
        mutable RefPtr<SVGAnimatedNumber> m_azimuth;
        mutable RefPtr<SVGAnimatedNumber> m_elevation;
        mutable RefPtr<SVGAnimatedNumber> m_x;
        mutable RefPtr<SVGAnimatedNumber> m_y;
        mutable RefPtr<SVGAnimatedNumber> m_z;
        mutable RefPtr<SVGAnimatedNumber> m_pointsAtX;
        mutable RefPtr<SVGAnimatedNumber> m_pointsAtY;
        mutable RefPtr<SVGAnimatedNumber> m_pointsAtZ;
        mutable RefPtr<SVGAnimatedNumber> m_specularExponent;
        mutable RefPtr<SVGAnimatedNumber> m_limitingConeAngle;
        
    };
}

#endif // SVG_SUPPORT
#endif
