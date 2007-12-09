/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>
                  2005 Oliver Hunt <oliver@nerget.com>

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
 */

#ifndef SVGFESpecularLightingElement_h
#define SVGFESpecularLightingElement_h

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGFESpecularLighting.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore
{
    class SVGColor;
    
    class SVGFESpecularLightingElement : public SVGFilterPrimitiveStandardAttributes
    {
    public:
        SVGFESpecularLightingElement(const QualifiedName&, Document*);
        virtual ~SVGFESpecularLightingElement();
        
        virtual void parseMappedAttribute(MappedAttribute*);
        virtual SVGFESpecularLighting* filterEffect(SVGResourceFilter*) const;

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGFESpecularLightingElement, String, String, In1, in1)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFESpecularLightingElement, float, float, SpecularConstant, specularConstant)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFESpecularLightingElement, float, float, SpecularExponent, specularExponent)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFESpecularLightingElement, float, float, SurfaceScale, surfaceScale)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFESpecularLightingElement, float, float, KernelUnitLengthX, kernelUnitLengthX)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFESpecularLightingElement, float, float, KernelUnitLengthY, kernelUnitLengthY)

        mutable SVGFESpecularLighting* m_filterEffect;
        
        void updateLights() const;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
