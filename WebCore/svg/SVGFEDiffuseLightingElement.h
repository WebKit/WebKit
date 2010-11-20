/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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

#ifndef SVGFEDiffuseLightingElement_h
#define SVGFEDiffuseLightingElement_h

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFELightElement.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore {

class FEDiffuseLighting;
class SVGColor;

class SVGFEDiffuseLightingElement : public SVGFilterPrimitiveStandardAttributes {
public:
    static PassRefPtr<SVGFEDiffuseLightingElement> create(const QualifiedName&, Document*);

private:
    SVGFEDiffuseLightingElement(const QualifiedName&, Document*);

    virtual void parseMappedAttribute(Attribute*);
    virtual void svgAttributeChanged(const QualifiedName&);
    virtual void synchronizeProperty(const QualifiedName&);
    virtual PassRefPtr<FilterEffect> build(SVGFilterBuilder*, Filter*);

    static const AtomicString& kernelUnitLengthXIdentifier();
    static const AtomicString& kernelUnitLengthYIdentifier();

    DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGFEDiffuseLightingElement, SVGNames::inAttr, String, In1, in1)
    DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGFEDiffuseLightingElement, SVGNames::diffuseConstantAttr, float, DiffuseConstant, diffuseConstant)
    DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGFEDiffuseLightingElement, SVGNames::surfaceScaleAttr, float, SurfaceScale, surfaceScale)
    DECLARE_ANIMATED_STATIC_PROPERTY_MULTIPLE_WRAPPERS_NEW(SVGFEDiffuseLightingElement, SVGNames::kernelUnitLengthAttr, kernelUnitLengthXIdentifier(), float, KernelUnitLengthX, kernelUnitLengthX)
    DECLARE_ANIMATED_STATIC_PROPERTY_MULTIPLE_WRAPPERS_NEW(SVGFEDiffuseLightingElement, SVGNames::kernelUnitLengthAttr, kernelUnitLengthYIdentifier(), float, KernelUnitLengthY, kernelUnitLengthY)

    PassRefPtr<LightSource> findLights() const;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
