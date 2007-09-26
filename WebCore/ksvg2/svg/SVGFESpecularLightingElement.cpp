/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>
                  2005 Oliver Hunt <ojh16@student.canterbury.ac.nz>

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
#include "SVGFESpecularLightingElement.h"

#include "SVGColor.h"
#include "SVGNames.h"
#include "SVGFELightElement.h"
#include "SVGParserUtilities.h"
#include "SVGResourceFilter.h"

namespace WebCore {

SVGFESpecularLightingElement::SVGFESpecularLightingElement(const QualifiedName& tagName, Document* doc)
    : SVGFilterPrimitiveStandardAttributes(tagName, doc)
    , m_specularConstant(0.0)
    , m_specularExponent(0.0)
    , m_surfaceScale(0.0)
    , m_lightingColor(new SVGColor)
    , m_kernelUnitLengthX(0.0)
    , m_kernelUnitLengthY(0.0)
    , m_filterEffect(0)
{
}

SVGFESpecularLightingElement::~SVGFESpecularLightingElement()
{
    delete m_filterEffect;
}

ANIMATED_PROPERTY_DEFINITIONS(SVGFESpecularLightingElement, String, String, string, In1, in1, SVGNames::inAttr.localName(), m_in1)
ANIMATED_PROPERTY_DEFINITIONS(SVGFESpecularLightingElement, double, Number, number, SpecularConstant, specularConstant, SVGNames::specularConstantAttr.localName(), m_specularConstant)
ANIMATED_PROPERTY_DEFINITIONS(SVGFESpecularLightingElement, double, Number, number, SpecularExponent, specularExponent, SVGNames::specularExponentAttr.localName(), m_specularExponent)
ANIMATED_PROPERTY_DEFINITIONS(SVGFESpecularLightingElement, double, Number, number, SurfaceScale, surfaceScale, SVGNames::surfaceScaleAttr.localName(), m_surfaceScale)
ANIMATED_PROPERTY_DEFINITIONS(SVGFESpecularLightingElement, double, Number, number, KernelUnitLengthX, kernelUnitLengthX, "kernelUnitLengthX", m_kernelUnitLengthX)
ANIMATED_PROPERTY_DEFINITIONS(SVGFESpecularLightingElement, double, Number, number, KernelUnitLengthY, kernelUnitLengthY, "kernelUnitLengthY", m_kernelUnitLengthY)
ANIMATED_PROPERTY_DEFINITIONS(SVGFESpecularLightingElement, SVGColor*, Color, color, LightingColor, lightingColor, SVGNames::lighting_colorAttr.localName(), m_lightingColor.get())

void SVGFESpecularLightingElement::parseMappedAttribute(MappedAttribute* attr)
{    
    const String& value = attr->value();
    if (attr->name() == SVGNames::inAttr)
        setIn1BaseValue(value);
    else if (attr->name() == SVGNames::surfaceScaleAttr)
        setSurfaceScaleBaseValue(value.toDouble());
    else if (attr->name() == SVGNames::specularConstantAttr)
        setSpecularConstantBaseValue(value.toDouble());
    else if (attr->name() == SVGNames::specularExponentAttr)
        setSpecularExponentBaseValue(value.toDouble());
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

SVGFESpecularLighting* SVGFESpecularLightingElement::filterEffect() const
{
    if (!m_filterEffect) 
        m_filterEffect = static_cast<SVGFESpecularLighting*>(SVGResourceFilter::createFilterEffect(FE_SPECULAR_LIGHTING));
    if (!m_filterEffect)
        return 0;
    m_filterEffect->setIn(in1());
    setStandardAttributes(m_filterEffect);
    m_filterEffect->setSpecularConstant((specularConstant()));
    m_filterEffect->setSpecularExponent((specularExponent()));
    m_filterEffect->setSurfaceScale((surfaceScale()));
    m_filterEffect->setKernelUnitLengthX((kernelUnitLengthX()));
    m_filterEffect->setKernelUnitLengthY((kernelUnitLengthY()));
    m_filterEffect->setLightingColor(lightingColor()->color());
    updateLights();
    return m_filterEffect;
}

void SVGFESpecularLightingElement::updateLights() const
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
