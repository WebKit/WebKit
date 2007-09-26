/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>
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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
 */

#ifndef SVGFESpecularLightingElement_h
#define SVGFESpecularLightingElement_h
#if ENABLE(SVG) && ENABLE(SVG_EXPERIMENTAL_FEATURES)

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
        
        // 'SVGFEDiffuseLightingElement' functions
        // Derived from: 'Element'
        virtual void parseMappedAttribute(MappedAttribute* attr);
        
        virtual SVGFESpecularLighting* filterEffect() const;

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGFESpecularLightingElement, String, String, In1, in1)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFESpecularLightingElement, double, double, SpecularConstant, specularConstant)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFESpecularLightingElement, double, double, SpecularExponent, specularExponent)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFESpecularLightingElement, double, double, SurfaceScale, surfaceScale)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFESpecularLightingElement, SVGColor*, RefPtr<SVGColor>, LightingColor, lightingColor)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFESpecularLightingElement, double, double, KernelUnitLengthX, kernelUnitLengthX)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFESpecularLightingElement, double, double, KernelUnitLengthY, kernelUnitLengthY)
        //need other properties here...
        mutable SVGFESpecularLighting* m_filterEffect;
        
        //light management
        void updateLights() const;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
