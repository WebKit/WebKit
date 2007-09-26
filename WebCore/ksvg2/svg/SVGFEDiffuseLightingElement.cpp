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

#if ENABLE(SVG) && ENABLE(SVG_EXPERIMENTAL_FEATURES)
#include "SVGFEDiffuseLightingElement.h"

#include "Attr.h"
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
    , m_diffuseConstant(0.0)
    , m_surfaceScale(0.0)
    , m_lightingColor(new SVGColor())
    , m_kernelUnitLengthX(0.0)
    , m_kernelUnitLengthY(0.0)
    , m_filterEffect(0)
{
}

SVGFEDiffuseLightingElement::~SVGFEDiffuseLightingElement()
{
    delete m_filterEffect;
}

ANIMATED_PROPERTY_DEFINITIONS(SVGFEDiffuseLightingElement, String, String, string, In1, in1, SVGNames::inAttr.localName(), m_in1)
ANIMATED_PROPERTY_DEFINITIONS(SVGFEDiffuseLightingElement, double, Number, number, DiffuseConstant, diffuseConstant, SVGNames::diffuseConstantAttr.localName(), m_diffuseConstant)
ANIMATED_PROPERTY_DEFINITIONS(SVGFEDiffuseLightingElement, double, Number, number, SurfaceScale, surfaceScale, SVGNames::surfaceScaleAttr.localName(), m_surfaceScale)
ANIMATED_PROPERTY_DEFINITIONS(SVGFEDiffuseLightingElement, double, Number, number, KernelUnitLengthX, kernelUnitLengthX, "kernelUnitLengthX", m_kernelUnitLengthX)
ANIMATED_PROPERTY_DEFINITIONS(SVGFEDiffuseLightingElement, double, Number, number, KernelUnitLengthY, kernelUnitLengthY, "kernelUnitLengthY", m_kernelUnitLengthY)
ANIMATED_PROPERTY_DEFINITIONS(SVGFEDiffuseLightingElement, SVGColor*, Color, color, LightingColor, lightingColor, SVGNames::lighting_colorAttr.localName(), m_lightingColor.get())

void SVGFEDiffuseLightingElement::parseMappedAttribute(MappedAttribute *attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::inAttr)
        setIn1BaseValue(value);
    else if (attr->name() == SVGNames::surfaceScaleAttr)
        setSurfaceScaleBaseValue(value.toDouble());
    else if (attr->name() == SVGNames::diffuseConstantAttr)
        setDiffuseConstantBaseValue(value.toInt());
    else if (attr->name() == SVGNames::kernelUnitLengthAttr) {
        double x, y;
        if (parseNumberOptionalNumber(value, x, y)) {
            setKernelUnitLengthXBaseValue(x);
            setKernelUnitLengthYBaseValue(y);
        }
    } else if (attr->name() == SVGNames::lighting_colorAttr)
        setLightingColorBaseValue(new SVGColor(value));
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

SVGFilterEffect* SVGFEDiffuseLightingElement::filterEffect() const
{
    if (!m_filterEffect) 
        m_filterEffect = static_cast<SVGFEDiffuseLighting*>(SVGResourceFilter::createFilterEffect(FE_DIFFUSE_LIGHTING));
    if (!m_filterEffect)
        return 0;

    m_filterEffect->setIn(in1());
    setStandardAttributes(m_filterEffect);
    m_filterEffect->setDiffuseConstant(diffuseConstant());
    m_filterEffect->setSurfaceScale(surfaceScale());
    m_filterEffect->setKernelUnitLengthX(kernelUnitLengthX());
    m_filterEffect->setKernelUnitLengthY(kernelUnitLengthY());
    m_filterEffect->setLightingColor(lightingColor()->color());
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
