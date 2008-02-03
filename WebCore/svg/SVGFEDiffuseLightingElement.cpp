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

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGFEDiffuseLightingElement.h"

#include "Attr.h"
#include "RenderObject.h"
#include "SVGColor.h"
#include "SVGFELightElement.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "SVGRenderStyle.h"
#include "SVGFEDiffuseLighting.h"
#include "SVGResourceFilter.h"

namespace WebCore {

SVGFEDiffuseLightingElement::SVGFEDiffuseLightingElement(const QualifiedName& tagName, Document* doc)
    : SVGFilterPrimitiveStandardAttributes(tagName, doc)
    , m_diffuseConstant(1.0f)
    , m_surfaceScale(1.0f)
    , m_kernelUnitLengthX(0.0f)
    , m_kernelUnitLengthY(0.0f)
    , m_filterEffect(0)
{
}

SVGFEDiffuseLightingElement::~SVGFEDiffuseLightingElement()
{
    delete m_filterEffect;
}

ANIMATED_PROPERTY_DEFINITIONS(SVGFEDiffuseLightingElement, String, String, string, In1, in1, SVGNames::inAttr, m_in1)
ANIMATED_PROPERTY_DEFINITIONS(SVGFEDiffuseLightingElement, float, Number, number, DiffuseConstant, diffuseConstant, SVGNames::diffuseConstantAttr, m_diffuseConstant)
ANIMATED_PROPERTY_DEFINITIONS(SVGFEDiffuseLightingElement, float, Number, number, SurfaceScale, surfaceScale, SVGNames::surfaceScaleAttr, m_surfaceScale)
ANIMATED_PROPERTY_DEFINITIONS(SVGFEDiffuseLightingElement, float, Number, number, KernelUnitLengthX, kernelUnitLengthX, "kernelUnitLengthX", m_kernelUnitLengthX)
ANIMATED_PROPERTY_DEFINITIONS(SVGFEDiffuseLightingElement, float, Number, number, KernelUnitLengthY, kernelUnitLengthY, "kernelUnitLengthY", m_kernelUnitLengthY)

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

SVGFilterEffect* SVGFEDiffuseLightingElement::filterEffect(SVGResourceFilter* filter) const
{
    if (!m_filterEffect) 
        m_filterEffect = new SVGFEDiffuseLighting(filter);

    m_filterEffect->setIn(in1());
    m_filterEffect->setDiffuseConstant(diffuseConstant());
    m_filterEffect->setSurfaceScale(surfaceScale());
    m_filterEffect->setKernelUnitLengthX(kernelUnitLengthX());
    m_filterEffect->setKernelUnitLengthY(kernelUnitLengthY());

    SVGFEDiffuseLightingElement* nonConstThis = const_cast<SVGFEDiffuseLightingElement*>(this);

    RenderStyle* parentStyle = nonConstThis->styleForRenderer(parent()->renderer());
    RenderStyle* filterStyle = nonConstThis->resolveStyle(parentStyle);
    
    m_filterEffect->setLightingColor(filterStyle->svgStyle()->lightingColor());
    setStandardAttributes(m_filterEffect);
 
    parentStyle->deref(document()->renderArena());
    filterStyle->deref(document()->renderArena());
    
    updateLights();
    return m_filterEffect;
}

void SVGFEDiffuseLightingElement::updateLights() const
{
    if (!m_filterEffect)
        return;
    
    SVGLightSource* light = 0;
    for (Node* n = firstChild(); n; n = n->nextSibling()) {
        if (n->hasTagName(SVGNames::feDistantLightTag) ||
            n->hasTagName(SVGNames::fePointLightTag) ||
            n->hasTagName(SVGNames::feSpotLightTag)) {
            SVGFELightElement* lightNode = static_cast<SVGFELightElement*>(n); 
            light = lightNode->lightSource();
            break;
        }
    }

    m_filterEffect->setLightSource(light);
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
