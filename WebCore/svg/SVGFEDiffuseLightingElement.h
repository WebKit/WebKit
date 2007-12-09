/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
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

#ifndef SVGFEDiffuseLightingElement_h
#define SVGFEDiffuseLightingElement_h

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore {
    class SVGFEDiffuseLighting;
    class SVGColor;

    class SVGFEDiffuseLightingElement : public SVGFilterPrimitiveStandardAttributes
    {
    public:
        SVGFEDiffuseLightingElement(const QualifiedName&, Document*);
        virtual ~SVGFEDiffuseLightingElement();

        virtual void parseMappedAttribute(MappedAttribute*);
        virtual SVGFilterEffect* filterEffect(SVGResourceFilter*) const;

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGFEDiffuseLightingElement, String, String, In1, in1)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFEDiffuseLightingElement, float, float, DiffuseConstant, diffuseConstant)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFEDiffuseLightingElement, float, float, SurfaceScale, surfaceScale)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFEDiffuseLightingElement, float, float, KernelUnitLengthX, kernelUnitLengthX)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFEDiffuseLightingElement, float, float, KernelUnitLengthY, kernelUnitLengthY)

        mutable SVGFEDiffuseLighting* m_filterEffect;
        
        void updateLights() const;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
