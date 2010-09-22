/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Oliver Hunt <oliver@nerget.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef SVGFESpecularLightingElement_h
#define SVGFESpecularLightingElement_h

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "FESpecularLighting.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore {

extern char SVGKernelUnitLengthXIdentifier[];
extern char SVGKernelUnitLengthYIdentifier[];

class SVGFESpecularLightingElement : public SVGFilterPrimitiveStandardAttributes {
public:
    static PassRefPtr<SVGFESpecularLightingElement> create(const QualifiedName&, Document*);

private:
    SVGFESpecularLightingElement(const QualifiedName&, Document*);
    
    virtual void parseMappedAttribute(Attribute*);
    virtual void synchronizeProperty(const QualifiedName&);
    virtual PassRefPtr<FilterEffect> build(SVGFilterBuilder*);

    DECLARE_ANIMATED_PROPERTY(SVGFESpecularLightingElement, SVGNames::inAttr, String, In1, in1)
    DECLARE_ANIMATED_PROPERTY(SVGFESpecularLightingElement, SVGNames::specularConstantAttr, float, SpecularConstant, specularConstant)
    DECLARE_ANIMATED_PROPERTY(SVGFESpecularLightingElement, SVGNames::specularExponentAttr, float, SpecularExponent, specularExponent)
    DECLARE_ANIMATED_PROPERTY(SVGFESpecularLightingElement, SVGNames::surfaceScaleAttr, float, SurfaceScale, surfaceScale)
    DECLARE_ANIMATED_PROPERTY_MULTIPLE_WRAPPERS(SVGFESpecularLightingElement, SVGNames::kernelUnitLengthAttr, SVGKernelUnitLengthXIdentifier, float, KernelUnitLengthX, kernelUnitLengthX)
    DECLARE_ANIMATED_PROPERTY_MULTIPLE_WRAPPERS(SVGFESpecularLightingElement, SVGNames::kernelUnitLengthAttr, SVGKernelUnitLengthYIdentifier, float, KernelUnitLengthY, kernelUnitLengthY)

    PassRefPtr<LightSource> findLights() const;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
