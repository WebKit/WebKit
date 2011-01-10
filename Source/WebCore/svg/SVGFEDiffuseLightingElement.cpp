/*
 * Copyright (C) 2005 Oliver Hunt <ojh16@student.canterbury.ac.nz>
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
#include "SVGFEDiffuseLightingElement.h"

#include "Attr.h"
#include "FEDiffuseLighting.h"
#include "FilterEffect.h"
#include "RenderStyle.h"
#include "SVGColor.h"
#include "SVGFELightElement.h"
#include "SVGFilterBuilder.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_STRING(SVGFEDiffuseLightingElement, SVGNames::inAttr, In1, in1)
DEFINE_ANIMATED_NUMBER(SVGFEDiffuseLightingElement, SVGNames::diffuseConstantAttr, DiffuseConstant, diffuseConstant)
DEFINE_ANIMATED_NUMBER(SVGFEDiffuseLightingElement, SVGNames::surfaceScaleAttr, SurfaceScale, surfaceScale)
DEFINE_ANIMATED_NUMBER_MULTIPLE_WRAPPERS(SVGFEDiffuseLightingElement, SVGNames::kernelUnitLengthAttr, kernelUnitLengthXIdentifier(), KernelUnitLengthX, kernelUnitLengthX)
DEFINE_ANIMATED_NUMBER_MULTIPLE_WRAPPERS(SVGFEDiffuseLightingElement, SVGNames::kernelUnitLengthAttr, kernelUnitLengthYIdentifier(), KernelUnitLengthY, kernelUnitLengthY)

inline SVGFEDiffuseLightingElement::SVGFEDiffuseLightingElement(const QualifiedName& tagName, Document* document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
    , m_diffuseConstant(1)
    , m_surfaceScale(1)
{
}

PassRefPtr<SVGFEDiffuseLightingElement> SVGFEDiffuseLightingElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGFEDiffuseLightingElement(tagName, document));
}

const AtomicString& SVGFEDiffuseLightingElement::kernelUnitLengthXIdentifier()
{
    DEFINE_STATIC_LOCAL(AtomicString, s_identifier, ("SVGKernelUnitLengthX"));
    return s_identifier;
}

const AtomicString& SVGFEDiffuseLightingElement::kernelUnitLengthYIdentifier()
{
    DEFINE_STATIC_LOCAL(AtomicString, s_identifier, ("SVGKernelUnitLengthY"));
    return s_identifier;
}

void SVGFEDiffuseLightingElement::parseMappedAttribute(Attribute* attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::inAttr)
        setIn1BaseValue(value);
    else if (attr->name() == SVGNames::surfaceScaleAttr)
        setSurfaceScaleBaseValue(value.toFloat());
    else if (attr->name() == SVGNames::diffuseConstantAttr)
        setDiffuseConstantBaseValue(value.toFloat());
    else if (attr->name() == SVGNames::kernelUnitLengthAttr) {
        float x, y;
        if (parseNumberOptionalNumber(value, x, y)) {
            setKernelUnitLengthXBaseValue(x);
            setKernelUnitLengthYBaseValue(y);
        }
    } else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

void SVGFEDiffuseLightingElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);

    if (attrName == SVGNames::inAttr
        || attrName == SVGNames::surfaceScaleAttr
        || attrName == SVGNames::diffuseConstantAttr
        || attrName == SVGNames::kernelUnitLengthAttr
        || attrName == SVGNames::lighting_colorAttr)
        invalidate();
}

void SVGFEDiffuseLightingElement::synchronizeProperty(const QualifiedName& attrName)
{
    SVGFilterPrimitiveStandardAttributes::synchronizeProperty(attrName);

    if (attrName == anyQName()) {
        synchronizeIn1();
        synchronizeSurfaceScale();
        synchronizeDiffuseConstant();
        synchronizeKernelUnitLengthX();
        synchronizeKernelUnitLengthY();
        return;
    }

    if (attrName == SVGNames::inAttr)
        synchronizeIn1();
    else if (attrName == SVGNames::surfaceScaleAttr)
        synchronizeSurfaceScale();
    else if (attrName == SVGNames::diffuseConstantAttr)
        synchronizeDiffuseConstant();
    else if (attrName == SVGNames::kernelUnitLengthAttr) {
        synchronizeKernelUnitLengthX();
        synchronizeKernelUnitLengthY();
    }
}

PassRefPtr<FilterEffect> SVGFEDiffuseLightingElement::build(SVGFilterBuilder* filterBuilder, Filter* filter)
{
    FilterEffect* input1 = filterBuilder->getEffectById(in1());
    
    if (!input1)
        return 0;
    
    RefPtr<RenderStyle> filterStyle = styleForRenderer();
    Color color = filterStyle->svgStyle()->lightingColor();
    
    RefPtr<FilterEffect> effect = FEDiffuseLighting::create(filter, color, surfaceScale(), diffuseConstant(), 
                                                                kernelUnitLengthX(), kernelUnitLengthY(), findLights());
    effect->inputEffects().append(input1);
    return effect.release();
}

PassRefPtr<LightSource> SVGFEDiffuseLightingElement::findLights() const
{
    for (Node* node = firstChild(); node; node = node->nextSibling()) {
        if (node->hasTagName(SVGNames::feDistantLightTag)
            || node->hasTagName(SVGNames::fePointLightTag)
            || node->hasTagName(SVGNames::feSpotLightTag)) {
            SVGFELightElement* lightNode = static_cast<SVGFELightElement*>(node); 
            return lightNode->lightSource();
        }
    }

    return 0;
}

}

#endif // ENABLE(SVG)
