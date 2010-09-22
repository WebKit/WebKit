/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#ifndef SVGFEConvolveMatrixElement_h
#define SVGFEConvolveMatrixElement_h

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "FEConvolveMatrix.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SVGNumberList.h"

namespace WebCore {

class SVGFEConvolveMatrixElement : public SVGFilterPrimitiveStandardAttributes {
public:
    static PassRefPtr<SVGFEConvolveMatrixElement> create(const QualifiedName&, Document*);

    void setOrder(float orderX, float orderY);
    void setKernelUnitLength(float kernelUnitLengthX, float kernelUnitLengthY);

private:
    SVGFEConvolveMatrixElement(const QualifiedName&, Document*);

    virtual void parseMappedAttribute(Attribute*);
    virtual PassRefPtr<FilterEffect> build(SVGFilterBuilder*);

    DECLARE_ANIMATED_PROPERTY(SVGFEConvolveMatrixElement, SVGNames::inAttr, String, In1, in1)
    DECLARE_ANIMATED_PROPERTY(SVGFEConvolveMatrixElement, SVGNames::orderXAttr, long, OrderX, orderX)
    DECLARE_ANIMATED_PROPERTY(SVGFEConvolveMatrixElement, SVGNames::orderYAttr, long, OrderY, orderY)
    DECLARE_ANIMATED_PROPERTY(SVGFEConvolveMatrixElement, SVGNames::kernelMatrixAttr, SVGNumberList*, KernelMatrix, kernelMatrix)
    DECLARE_ANIMATED_PROPERTY(SVGFEConvolveMatrixElement, SVGNames::divisorAttr, float, Divisor, divisor)
    DECLARE_ANIMATED_PROPERTY(SVGFEConvolveMatrixElement, SVGNames::biasAttr, float, Bias, bias)
    DECLARE_ANIMATED_PROPERTY(SVGFEConvolveMatrixElement, SVGNames::targetXAttr, long, TargetX, targetX)
    DECLARE_ANIMATED_PROPERTY(SVGFEConvolveMatrixElement, SVGNames::targetYAttr, long, TargetY, targetY)
    DECLARE_ANIMATED_PROPERTY(SVGFEConvolveMatrixElement, SVGNames::operatorAttr, int, EdgeMode, edgeMode)
    DECLARE_ANIMATED_PROPERTY_MULTIPLE_WRAPPERS(SVGFEConvolveMatrixElement, SVGNames::kernelUnitLengthAttr, SVGKernelUnitLengthXIdentifier, float, KernelUnitLengthX, kernelUnitLengthX)
    DECLARE_ANIMATED_PROPERTY_MULTIPLE_WRAPPERS(SVGFEConvolveMatrixElement, SVGNames::kernelUnitLengthAttr, SVGKernelUnitLengthYIdentifier, float, KernelUnitLengthY, kernelUnitLengthY)
    DECLARE_ANIMATED_PROPERTY(SVGFEConvolveMatrixElement, SVGNames::preserveAlphaAttr, bool, PreserveAlpha, preserveAlpha)
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
