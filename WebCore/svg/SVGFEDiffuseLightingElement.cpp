/*
     Copyright (C) 2005 Oliver Hunt <ojh16@student.canterbury.ac.nz>
     
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

#include "config.h"

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFEDiffuseLightingElement.h"

#include "Attr.h"
#include "MappedAttribute.h"
#include "RenderObject.h"
#include "SVGColor.h"
#include "SVGFEDiffuseLighting.h"
#include "SVGFELightElement.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "SVGRenderStyle.h"
#include "SVGResourceFilter.h"

namespace WebCore {

char SVGKernelUnitLengthXIdentifier[] = "SVGKernelUnitLengthX";
char SVGKernelUnitLengthYIdentifier[] = "SVGKernelUnitLengthY";

SVGFEDiffuseLightingElement::SVGFEDiffuseLightingElement(const QualifiedName& tagName, Document* doc)
    : SVGFilterPrimitiveStandardAttributes(tagName, doc)
    , m_in1(this, SVGNames::inAttr)
    , m_diffuseConstant(this, SVGNames::diffuseConstantAttr, 1.0f)
    , m_surfaceScale(this, SVGNames::surfaceScaleAttr, 1.0f)
    , m_kernelUnitLengthX(this, SVGNames::kernelUnitLengthAttr)
    , m_kernelUnitLengthY(this, SVGNames::kernelUnitLengthAttr)
{
}

SVGFEDiffuseLightingElement::~SVGFEDiffuseLightingElement()
{
}

void SVGFEDiffuseLightingElement::parseMappedAttribute(MappedAttribute *attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::inAttr)
        setIn1BaseValue(value);
    else if (attr->name() == SVGNames::surfaceScaleAttr)
        setSurfaceScaleBaseValue(value.toFloat());
    else if (attr->name() == SVGNames::diffuseConstantAttr)
        setDiffuseConstantBaseValue(value.toInt());
    else if (attr->name() == SVGNames::kernelUnitLengthAttr) {
        float x, y;
        if (parseNumberOptionalNumber(value, x, y)) {
            setKernelUnitLengthXBaseValue(x);
            setKernelUnitLengthYBaseValue(y);
        }
    } else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

bool SVGFEDiffuseLightingElement::build(SVGResourceFilter* filterResource)
{
    FilterEffect* input1 = filterResource->builder()->getEffectById(in1());
    
    if(!input1)
        return false;
    
    RefPtr<RenderStyle> filterStyle = styleForRenderer();
    Color color = filterStyle->svgStyle()->lightingColor();
    
    RefPtr<FilterEffect> effect = FEDiffuseLighting::create(input1, color, surfaceScale(), diffuseConstant(), 
                                            kernelUnitLengthX(), kernelUnitLengthY(), findLights());
    filterResource->addFilterEffect(this, effect.release());
    
    return true;
}

LightSource* SVGFEDiffuseLightingElement::findLights() const
{
    LightSource* light = 0;
    for (Node* n = firstChild(); n; n = n->nextSibling()) {
        if (n->hasTagName(SVGNames::feDistantLightTag) ||
            n->hasTagName(SVGNames::fePointLightTag) ||
            n->hasTagName(SVGNames::feSpotLightTag)) {
            SVGFELightElement* lightNode = static_cast<SVGFELightElement*>(n); 
            light = lightNode->lightSource();
            break;
        }
    }

    return light;
}

}

#endif // ENABLE(SVG)
