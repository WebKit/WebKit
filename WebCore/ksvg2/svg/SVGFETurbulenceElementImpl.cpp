/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

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
#include <qstringlist.h>

#include <kdom/core/AttrImpl.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasResources.h>
#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/device/KRenderingPaintServerGradient.h>

#include "ksvg.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGFETurbulenceElementImpl.h"
#include "SVGAnimatedNumberImpl.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGAnimatedIntegerImpl.h"
#include "SVGDOMImplementationImpl.h"

using namespace KSVG;

SVGFETurbulenceElementImpl::SVGFETurbulenceElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc) : 
SVGFilterPrimitiveStandardAttributesImpl(tagName, doc)
{
    m_filterEffect = 0;
}

SVGFETurbulenceElementImpl::~SVGFETurbulenceElementImpl()
{
    delete m_filterEffect;
}

SVGAnimatedNumberImpl *SVGFETurbulenceElementImpl::baseFrequencyX() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_baseFrequencyX, dummy);
}

SVGAnimatedNumberImpl *SVGFETurbulenceElementImpl::baseFrequencyY() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_baseFrequencyY, dummy);
}

SVGAnimatedNumberImpl *SVGFETurbulenceElementImpl::seed() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_seed, dummy);
}

SVGAnimatedIntegerImpl *SVGFETurbulenceElementImpl::numOctaves() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedIntegerImpl>(m_numOctaves, dummy);
}

SVGAnimatedEnumerationImpl *SVGFETurbulenceElementImpl::stitchTiles() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedEnumerationImpl>(m_stitchTiles, dummy);
}

SVGAnimatedEnumerationImpl *SVGFETurbulenceElementImpl::type() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedEnumerationImpl>(m_type, dummy);
}

void SVGFETurbulenceElementImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    KDOM::DOMString value(attr->value());
    if (attr->name() == SVGNames::typeAttr)
    {
        if(value == "fractalNoise")
            type()->setBaseVal(SVG_TURBULENCE_TYPE_FRACTALNOISE);
        else if(value == "turbulence")
            type()->setBaseVal(SVG_TURBULENCE_TYPE_TURBULENCE);
    }
    else if (attr->name() == SVGNames::stitchTilesAttr)
    {
        if(value == "stitch")
            stitchTiles()->setBaseVal(SVG_STITCHTYPE_STITCH);
        else if(value == "nostitch")
            stitchTiles()->setBaseVal(SVG_STITCHTYPE_NOSTITCH);
    }
    else if (attr->name() == SVGNames::baseFrequencyAttr)
    {
        QStringList numbers = QStringList::split(' ', value.qstring());
        baseFrequencyX()->setBaseVal(numbers[0].toDouble());
        if(numbers.count() == 1)
            baseFrequencyY()->setBaseVal(numbers[0].toDouble());
        else
            baseFrequencyY()->setBaseVal(numbers[1].toDouble());
    }
    else if (attr->name() == SVGNames::seedAttr)
        seed()->setBaseVal(value.qstring().toDouble());
    else if (attr->name() == SVGNames::numOctavesAttr)
        numOctaves()->setBaseVal(value.qstring().toUInt());
    else
        SVGFilterPrimitiveStandardAttributesImpl::parseMappedAttribute(attr);
}

KCanvasFETurbulence *SVGFETurbulenceElementImpl::filterEffect() const
{
    if (!m_filterEffect)
        m_filterEffect = static_cast<KCanvasFETurbulence *>(QPainter::renderingDevice()->createFilterEffect(FE_TURBULENCE));
    if (!m_filterEffect)
        return 0;
    
    m_filterEffect->setType((KCTurbulanceType)(type()->baseVal() - 1));
    setStandardAttributes(m_filterEffect);
    m_filterEffect->setBaseFrequencyX(baseFrequencyX()->baseVal());
    m_filterEffect->setBaseFrequencyY(baseFrequencyY()->baseVal());
    m_filterEffect->setNumOctaves(numOctaves()->baseVal());
    m_filterEffect->setSeed(seed()->baseVal());
    m_filterEffect->setStitchTiles(stitchTiles()->baseVal() == SVG_STITCHTYPE_STITCH);
    return m_filterEffect;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

