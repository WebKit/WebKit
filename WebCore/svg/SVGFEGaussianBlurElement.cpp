/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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
#include "SVGFEGaussianBlurElement.h"

#include "Attribute.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"

namespace WebCore {

char SVGStdDeviationXAttrIdentifier[] = "SVGStdDeviationXAttr";
char SVGStdDeviationYAttrIdentifier[] = "SVGStdDeviationYAttr";

inline SVGFEGaussianBlurElement::SVGFEGaussianBlurElement(const QualifiedName& tagName, Document* document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
{
}

PassRefPtr<SVGFEGaussianBlurElement> SVGFEGaussianBlurElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGFEGaussianBlurElement(tagName, document));
}

void SVGFEGaussianBlurElement::setStdDeviation(float, float)
{
    // FIXME: Needs an implementation.
}

void SVGFEGaussianBlurElement::parseMappedAttribute(Attribute* attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::stdDeviationAttr) {
        float x, y;
        if (parseNumberOptionalNumber(value, x, y)) {
            setStdDeviationXBaseValue(x);
            setStdDeviationYBaseValue(y);
        }
    } else if (attr->name() == SVGNames::inAttr)
        setIn1BaseValue(value);
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

void SVGFEGaussianBlurElement::synchronizeProperty(const QualifiedName& attrName)
{
    SVGFilterPrimitiveStandardAttributes::synchronizeProperty(attrName);

    if (attrName == anyQName()) {
        synchronizeStdDeviationX();
        synchronizeStdDeviationY();
        synchronizeIn1();
        return;
    }

    if (attrName == SVGNames::stdDeviationAttr) {
        synchronizeStdDeviationX();
        synchronizeStdDeviationY();
    } else if (attrName == SVGNames::inAttr)
        synchronizeIn1();
}

PassRefPtr<FilterEffect> SVGFEGaussianBlurElement::build(SVGFilterBuilder* filterBuilder)
{
    FilterEffect* input1 = filterBuilder->getEffectById(in1());

    if (!input1)
        return 0;

    RefPtr<FilterEffect> effect = FEGaussianBlur::create(stdDeviationX(), stdDeviationY());
    effect->inputEffects().append(input1);
    return effect.release();
}

}

#endif // ENABLE(SVG)
