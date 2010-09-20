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

#include "config.h"

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFEConvolveMatrixElement.h"

#include "Attr.h"
#include "FloatPoint.h"
#include "FloatSize.h"
#include "IntPoint.h"
#include "IntSize.h"
#include "SVGNames.h"
#include "SVGNumberList.h"
#include "SVGParserUtilities.h"

#include <math.h>

namespace WebCore {

char SVGKernelUnitLengthXAttrIdentifier[] = "SVGKernelUnitLengthXAttr";
char SVGKernelUnitLengthYAttrIdentifier[] = "SVGKernelUnitLengthYAttr";

inline SVGFEConvolveMatrixElement::SVGFEConvolveMatrixElement(const QualifiedName& tagName, Document* document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
    , m_kernelMatrix(SVGNumberList::create(SVGNames::kernelMatrixAttr))
    , m_edgeMode(EDGEMODE_DUPLICATE)
{
}

PassRefPtr<SVGFEConvolveMatrixElement> SVGFEConvolveMatrixElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGFEConvolveMatrixElement(tagName, document));
}

void SVGFEConvolveMatrixElement::parseMappedAttribute(Attribute* attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::inAttr)
        setIn1BaseValue(value);
    else if (attr->name() == SVGNames::orderAttr) {
        float x, y;
        if (parseNumberOptionalNumber(value, x, y)) {
            setOrderXBaseValue(x);
            setOrderYBaseValue(y);
        }
    } else if (attr->name() == SVGNames::edgeModeAttr) {
        if (value == "duplicate")
            setEdgeModeBaseValue(EDGEMODE_DUPLICATE);
        else if (value == "wrap")
            setEdgeModeBaseValue(EDGEMODE_WRAP);
        else if (value == "none")
            setEdgeModeBaseValue(EDGEMODE_NONE);
    } else if (attr->name() == SVGNames::kernelMatrixAttr)
        kernelMatrixBaseValue()->parse(value);
    else if (attr->name() == SVGNames::divisorAttr)
        setDivisorBaseValue(value.toFloat());
    else if (attr->name() == SVGNames::biasAttr)
        setBiasBaseValue(value.toFloat());
    else if (attr->name() == SVGNames::targetXAttr)
        setTargetXBaseValue(value.toUIntStrict());
    else if (attr->name() == SVGNames::targetYAttr)
        setTargetYBaseValue(value.toUIntStrict());
    else if (attr->name() == SVGNames::kernelUnitLengthAttr) {
        float x, y;
        if (parseNumberOptionalNumber(value, x, y)) {
            setKernelUnitLengthXBaseValue(x);
            setKernelUnitLengthYBaseValue(y);
        }
    } else if (attr->name() == SVGNames::preserveAlphaAttr) {
        if (value == "true")
            setPreserveAlphaBaseValue(true);
        else if (value == "false")
            setPreserveAlphaBaseValue(false);
    } else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

void SVGFEConvolveMatrixElement::setOrder(float, float)
{
    // FIXME: Needs an implementation.
}

void SVGFEConvolveMatrixElement::setKernelUnitLength(float, float)
{
    // FIXME: Needs an implementation.
}

PassRefPtr<FilterEffect> SVGFEConvolveMatrixElement::build(SVGFilterBuilder* filterBuilder)
{
    FilterEffect* input1 = filterBuilder->getEffectById(in1());

    if (!input1)
        return 0;

    Vector<float> kernelMatrixValues;
    SVGNumberList* numbers = kernelMatrix();

    ExceptionCode ec = 0;
    int numberOfItems = numbers->numberOfItems();
    for (int i = 0; i < numberOfItems; ++i)
        kernelMatrixValues.append(numbers->getItem(i, ec));

    int orderXValue = orderX();
    int orderYValue = orderY();
    if (!hasAttribute(SVGNames::orderAttr)) {
        orderXValue = 3;
        orderYValue = 3;
    }
    // The spec says this is a requirement, and should bail out if fails
    if (orderXValue * orderYValue != numberOfItems)
        return 0;

    int targetXValue = targetX();
    int targetYValue = targetY();
    if (hasAttribute(SVGNames::targetXAttr) && (targetXValue < 0 || targetXValue >= orderXValue))
        return 0;
    // The spec says the default value is: targetX = floor ( orderX / 2 ))
    if (!hasAttribute(SVGNames::targetXAttr))
        targetXValue = static_cast<int>(floorf(orderXValue / 2));
    if (hasAttribute(SVGNames::targetYAttr) && (targetYValue < 0 || targetYValue >= orderYValue))
        return 0;
    // The spec says the default value is: targetY = floor ( orderY / 2 ))
    if (!hasAttribute(SVGNames::targetYAttr))
        targetYValue = static_cast<int>(floorf(orderYValue / 2));

    float divisorValue = divisor();
    if (hasAttribute(SVGNames::divisorAttr) && !divisorValue)
        return 0;
    if (!hasAttribute(SVGNames::divisorAttr)) {
        for (int i = 0; i < numberOfItems; ++i)
            divisorValue += kernelMatrixValues[i];
        if (!divisorValue)
            divisorValue = 1;
    }

    RefPtr<FilterEffect> effect = FEConvolveMatrix::create(
                    IntSize(orderXValue, orderYValue), divisorValue,
                    bias(), IntPoint(targetXValue, targetYValue), static_cast<EdgeModeType>(edgeMode()),
                    FloatPoint(kernelUnitLengthX(), kernelUnitLengthX()), preserveAlpha(), kernelMatrixValues);
    effect->inputEffects().append(input1);
    return effect.release();
}

} // namespace WebCore

#endif // ENABLE(SVG)
