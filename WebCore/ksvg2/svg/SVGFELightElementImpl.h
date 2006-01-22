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

#include "SVGElementImpl.h"
#include "KCanvasFilters.h"

namespace KSVG
{
    class SVGAnimatedNumberImpl;
    class SVGAnimatedNumberListImpl;
    class SVGAnimatedEnumerationImpl;
    
    class SVGFELightElementImpl : public SVGElementImpl
    {
    public:
        SVGFELightElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc);
        virtual ~SVGFELightElementImpl();
        
        // 'SVGComponentTransferFunctionElement' functions
        SVGAnimatedNumberImpl *azimuth() const;
        SVGAnimatedNumberImpl *elevation() const;
        SVGAnimatedNumberImpl *x() const;
        SVGAnimatedNumberImpl *y() const;
        SVGAnimatedNumberImpl *z() const;
        SVGAnimatedNumberImpl *pointsAtX() const;
        SVGAnimatedNumberImpl *pointsAtY() const;
        SVGAnimatedNumberImpl *pointsAtZ() const;
        SVGAnimatedNumberImpl *specularExponent() const;
        SVGAnimatedNumberImpl *limitingConeAngle() const;
        
        virtual KCLightSource *lightSource() const = 0;
        
        // Derived from: 'ElementImpl'
        virtual void parseMappedAttribute(KDOM::MappedAttributeImpl *attr);        
    private:
        mutable RefPtr<SVGAnimatedNumberImpl> m_azimuth;
        mutable RefPtr<SVGAnimatedNumberImpl> m_elevation;
        mutable RefPtr<SVGAnimatedNumberImpl> m_x;
        mutable RefPtr<SVGAnimatedNumberImpl> m_y;
        mutable RefPtr<SVGAnimatedNumberImpl> m_z;
        mutable RefPtr<SVGAnimatedNumberImpl> m_pointsAtX;
        mutable RefPtr<SVGAnimatedNumberImpl> m_pointsAtY;
        mutable RefPtr<SVGAnimatedNumberImpl> m_pointsAtZ;
        mutable RefPtr<SVGAnimatedNumberImpl> m_specularExponent;
        mutable RefPtr<SVGAnimatedNumberImpl> m_limitingConeAngle;
        
    };
}

#endif // SVG_SUPPORT
#endif
