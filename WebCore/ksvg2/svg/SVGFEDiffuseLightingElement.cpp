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
     the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.
 */

#include "config.h"
#if SVG_SUPPORT
#include <DeprecatedStringList.h>

#include "Attr.h"

#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "ksvg.h"
#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGAnimatedColor.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedString.h"
#include "SVGFELightElement.h"
#include "SVGDOMImplementation.h"
#include "SVGFEDiffuseLightingElement.h"

namespace WebCore {

SVGFEDiffuseLightingElement::SVGFEDiffuseLightingElement(const QualifiedName& tagName, Document *doc) : 
SVGFilterPrimitiveStandardAttributes(tagName, doc)
{
    m_filterEffect = 0;
}

SVGFEDiffuseLightingElement::~SVGFEDiffuseLightingElement()
{
    delete m_filterEffect;
}

SVGAnimatedString *SVGFEDiffuseLightingElement::in1() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedString>(m_in1, dummy);
}

SVGAnimatedNumber *SVGFEDiffuseLightingElement::diffuseConstant() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_diffuseConstant, dummy);
}

SVGAnimatedNumber *SVGFEDiffuseLightingElement::surfaceScale() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_surfaceScale, dummy);
}

SVGAnimatedNumber *SVGFEDiffuseLightingElement::kernelUnitLengthX() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_kernelUnitLengthX, dummy);
}

SVGAnimatedNumber *SVGFEDiffuseLightingElement::kernelUnitLengthY() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_kernelUnitLengthY, dummy);
}

SVGAnimatedColor *SVGFEDiffuseLightingElement::lightingColor() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedColor>(m_lightingColor, dummy);
}

void SVGFEDiffuseLightingElement::parseMappedAttribute(MappedAttribute *attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::inAttr)
        in1()->setBaseVal(value.impl());
    else if (attr->name() == SVGNames::surfaceScaleAttr)
        surfaceScale()->setBaseVal(value.deprecatedString().toDouble());
    else if (attr->name() == SVGNames::diffuseConstantAttr)
        diffuseConstant()->setBaseVal(value.toInt());
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

KCanvasFEDiffuseLighting *SVGFEDiffuseLightingElement::filterEffect() const
{
    if (!m_filterEffect) 
        m_filterEffect = static_cast<KCanvasFEDiffuseLighting *>(renderingDevice()->createFilterEffect(FE_DIFFUSE_LIGHTING));
    m_filterEffect->setIn(String(in1()->baseVal()).deprecatedString());
    setStandardAttributes(m_filterEffect);
    m_filterEffect->setDiffuseConstant((diffuseConstant()->baseVal()));
    m_filterEffect->setSurfaceScale((surfaceScale()->baseVal()));
    m_filterEffect->setKernelUnitLengthX((kernelUnitLengthX()->baseVal()));
    m_filterEffect->setKernelUnitLengthY((kernelUnitLengthY()->baseVal()));
    m_filterEffect->setLightingColor(lightingColor()->baseVal()->color());
    updateLights();
    return m_filterEffect;
}

void SVGFEDiffuseLightingElement::updateLights() const
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

}

#endif // SVG_SUPPORT

