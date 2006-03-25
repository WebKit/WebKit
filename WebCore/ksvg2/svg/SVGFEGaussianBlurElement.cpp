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
#include <DeprecatedStringList.h>

#include "Attr.h"

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasResources.h>
#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/device/KRenderingPaintServerGradient.h>

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGFEGaussianBlurElement.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedString.h"
#include "SVGDOMImplementation.h"

using namespace WebCore;

SVGFEGaussianBlurElement::SVGFEGaussianBlurElement(const QualifiedName& tagName, Document *doc) : 
SVGFilterPrimitiveStandardAttributes(tagName, doc)
{
    m_filterEffect = 0;
}

SVGFEGaussianBlurElement::~SVGFEGaussianBlurElement()
{
    delete m_filterEffect;
}

SVGAnimatedString *SVGFEGaussianBlurElement::in1() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedString>(m_in1, dummy);
}

SVGAnimatedNumber *SVGFEGaussianBlurElement::stdDeviationX() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_stdDeviationX, dummy);
}

SVGAnimatedNumber *SVGFEGaussianBlurElement::stdDeviationY() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_stdDeviationY, dummy);
}

void SVGFEGaussianBlurElement::setStdDeviation(float stdDeviationX, float stdDeviationY)
{
}

void SVGFEGaussianBlurElement::parseMappedAttribute(MappedAttribute *attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::stdDeviationAttr) {
        DeprecatedStringList numbers = DeprecatedStringList::split(' ', value.deprecatedString());
        stdDeviationX()->setBaseVal(numbers[0].toDouble());
        if(numbers.count() == 1)
            stdDeviationY()->setBaseVal(numbers[0].toDouble());
        else
            stdDeviationY()->setBaseVal(numbers[1].toDouble());
    }
    else if (attr->name() == SVGNames::inAttr)
        in1()->setBaseVal(value.impl());
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

KCanvasFEGaussianBlur *SVGFEGaussianBlurElement::filterEffect() const
{
    if (!m_filterEffect)
        m_filterEffect = static_cast<KCanvasFEGaussianBlur *>(renderingDevice()->createFilterEffect(FE_GAUSSIAN_BLUR));
    if (!m_filterEffect)
        return 0;
    m_filterEffect->setIn(String(in1()->baseVal()).deprecatedString());
    setStandardAttributes(m_filterEffect);
    m_filterEffect->setStdDeviationX(stdDeviationX()->baseVal());
    m_filterEffect->setStdDeviationY(stdDeviationY()->baseVal());
    return m_filterEffect;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

