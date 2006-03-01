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
#include <QStringList.h>

#include <kdom/core/AttrImpl.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGAnimatedColorImpl.h"
#include "SVGAnimatedNumberImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "SVGFELightElementImpl.h"
#include "SVGDOMImplementationImpl.h"
#include "SVGFESpecularLightingElementImpl.h"


using namespace WebCore;

SVGFESpecularLightingElementImpl::SVGFESpecularLightingElementImpl(const QualifiedName& tagName, DocumentImpl *doc) : 
SVGFilterPrimitiveStandardAttributesImpl(tagName, doc)
{
    m_filterEffect = 0;
}

SVGFESpecularLightingElementImpl::~SVGFESpecularLightingElementImpl()
{
    delete m_filterEffect;
}

SVGAnimatedStringImpl *SVGFESpecularLightingElementImpl::in1() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedStringImpl>(m_in1, dummy);
}

SVGAnimatedNumberImpl *SVGFESpecularLightingElementImpl::specularConstant() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_specularConstant, dummy);
}

SVGAnimatedNumberImpl *SVGFESpecularLightingElementImpl::specularExponent() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_specularExponent, dummy);
}

SVGAnimatedNumberImpl *SVGFESpecularLightingElementImpl::surfaceScale() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_surfaceScale, dummy);
}

SVGAnimatedNumberImpl *SVGFESpecularLightingElementImpl::kernelUnitLengthX() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_kernelUnitLengthX, dummy);
}

SVGAnimatedNumberImpl *SVGFESpecularLightingElementImpl::kernelUnitLengthY() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_kernelUnitLengthY, dummy);
}

SVGAnimatedColorImpl  *SVGFESpecularLightingElementImpl::lightingColor() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedColorImpl>(m_lightingColor, dummy);
}

void SVGFESpecularLightingElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{    
    DOMString value(attr->value());
    if (attr->name() == SVGNames::inAttr)
        in1()->setBaseVal(value.impl());
    else if (attr->name() == SVGNames::surfaceScaleAttr)
        surfaceScale()->setBaseVal(value.qstring().toDouble());
    else if (attr->name() == SVGNames::specularConstantAttr)
        specularConstant()->setBaseVal(value.qstring().toDouble());
    else if (attr->name() == SVGNames::specularExponentAttr)
        specularExponent()->setBaseVal(value.qstring().toDouble());
    else if (attr->name() == SVGNames::kernelUnitLengthAttr) {
        QStringList numbers = QStringList::split(' ', value.qstring());
        kernelUnitLengthX()->setBaseVal(numbers[0].toDouble());
        if (numbers.count() == 1)
            kernelUnitLengthY()->setBaseVal(numbers[0].toDouble());
        else
            kernelUnitLengthY()->setBaseVal(numbers[1].toDouble());
    } else if (attr->name() == SVGNames::lighting_colorAttr)
        lightingColor()->setBaseVal(new SVGColorImpl(value.impl()));
    else
        SVGFilterPrimitiveStandardAttributesImpl::parseMappedAttribute(attr);
}

KCanvasFESpecularLighting *SVGFESpecularLightingElementImpl::filterEffect() const
{
    if (!m_filterEffect) 
        m_filterEffect = static_cast<KCanvasFESpecularLighting *>(renderingDevice()->createFilterEffect(FE_SPECULAR_LIGHTING));
    m_filterEffect->setIn(DOMString(in1()->baseVal()).qstring());
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

void SVGFESpecularLightingElementImpl::updateLights() const
{
    if (!m_filterEffect)
        return;

    KCLightSource *light = 0;    
    for (NodeImpl *n = firstChild(); n; n = n->nextSibling()) {
        if (n->hasTagName(SVGNames::feDistantLightTag)||n->hasTagName(SVGNames::fePointLightTag)||n->hasTagName(SVGNames::feSpotLightTag)) {
            SVGFELightElementImpl *lightNode = static_cast<SVGFELightElementImpl *>(n); 
            light = lightNode->lightSource();
            break;
        }
    }
    m_filterEffect->setLightSource(light);
}
#endif // SVG_SUPPORT

