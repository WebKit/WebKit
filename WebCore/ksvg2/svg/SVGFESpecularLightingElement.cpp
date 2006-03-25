/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
 */

#include "config.h"
#if SVG_SUPPORT
#include <DeprecatedStringList.h>

#include "Attr.h"

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGAnimatedColor.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedString.h"
#include "SVGFELightElement.h"
#include "SVGDOMImplementation.h"
#include "SVGFESpecularLightingElement.h"


using namespace WebCore;

SVGFESpecularLightingElement::SVGFESpecularLightingElement(const QualifiedName& tagName, Document *doc) : 
SVGFilterPrimitiveStandardAttributes(tagName, doc)
{
    m_filterEffect = 0;
}

SVGFESpecularLightingElement::~SVGFESpecularLightingElement()
{
    delete m_filterEffect;
}

SVGAnimatedString *SVGFESpecularLightingElement::in1() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedString>(m_in1, dummy);
}

SVGAnimatedNumber *SVGFESpecularLightingElement::specularConstant() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_specularConstant, dummy);
}

SVGAnimatedNumber *SVGFESpecularLightingElement::specularExponent() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_specularExponent, dummy);
}

SVGAnimatedNumber *SVGFESpecularLightingElement::surfaceScale() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_surfaceScale, dummy);
}

SVGAnimatedNumber *SVGFESpecularLightingElement::kernelUnitLengthX() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_kernelUnitLengthX, dummy);
}

SVGAnimatedNumber *SVGFESpecularLightingElement::kernelUnitLengthY() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_kernelUnitLengthY, dummy);
}

SVGAnimatedColor  *SVGFESpecularLightingElement::lightingColor() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedColor>(m_lightingColor, dummy);
}

void SVGFESpecularLightingElement::parseMappedAttribute(MappedAttribute *attr)
{    
    const String& value = attr->value();
    if (attr->name() == SVGNames::inAttr)
        in1()->setBaseVal(value.impl());
    else if (attr->name() == SVGNames::surfaceScaleAttr)
        surfaceScale()->setBaseVal(value.deprecatedString().toDouble());
    else if (attr->name() == SVGNames::specularConstantAttr)
        specularConstant()->setBaseVal(value.deprecatedString().toDouble());
    else if (attr->name() == SVGNames::specularExponentAttr)
        specularExponent()->setBaseVal(value.deprecatedString().toDouble());
    else if (attr->name() == SVGNames::kernelUnitLengthAttr) {
        DeprecatedStringList numbers = DeprecatedStringList::split(' ', value.deprecatedString());
        kernelUnitLengthX()->setBaseVal(numbers[0].toDouble());
        if (numbers.count() == 1)
            kernelUnitLengthY()->setBaseVal(numbers[0].toDouble());
        else
            kernelUnitLengthY()->setBaseVal(numbers[1].toDouble());
    } else if (attr->name() == SVGNames::lighting_colorAttr)
        lightingColor()->setBaseVal(new SVGColor(value.impl()));
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

KCanvasFESpecularLighting *SVGFESpecularLightingElement::filterEffect() const
{
    if (!m_filterEffect) 
        m_filterEffect = static_cast<KCanvasFESpecularLighting *>(renderingDevice()->createFilterEffect(FE_SPECULAR_LIGHTING));
    m_filterEffect->setIn(String(in1()->baseVal()).deprecatedString());
    setStandardAttributes(m_filterEffect);
    m_filterEffect->setSpecularConstant((specularConstant()->baseVal()));
    m_filterEffect->setSpecularExponent((specularExponent()->baseVal()));
    m_filterEffect->setSurfaceScale((surfaceScale()->baseVal()));
    m_filterEffect->setKernelUnitLengthX((kernelUnitLengthX()->baseVal()));
    m_filterEffect->setKernelUnitLengthY((kernelUnitLengthY()->baseVal()));
    m_filterEffect->setLightingColor(lightingColor()->baseVal()->color());
    updateLights();
    return m_filterEffect;
}

void SVGFESpecularLightingElement::updateLights() const
{
    if (!m_filterEffect)
        return;

    KCLightSource *light = 0;    
    for (Node *n = firstChild(); n; n = n->nextSibling()) {
        if (n->hasTagName(SVGNames::feDistantLightTag)||n->hasTagName(SVGNames::fePointLightTag)||n->hasTagName(SVGNames::feSpotLightTag)) {
            SVGFELightElement *lightNode = static_cast<SVGFELightElement *>(n); 
            light = lightNode->lightSource();
            break;
        }
    }
    m_filterEffect->setLightSource(light);
}
#endif // SVG_SUPPORT

