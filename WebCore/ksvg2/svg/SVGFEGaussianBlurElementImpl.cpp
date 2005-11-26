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
#include <qstringlist.h>

#include <kdom/core/AttrImpl.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasResources.h>
#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/device/KRenderingPaintServerGradient.h>

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGFEGaussianBlurElementImpl.h"
#include "SVGAnimatedNumberImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "SVGDOMImplementationImpl.h"

using namespace KSVG;

SVGFEGaussianBlurElementImpl::SVGFEGaussianBlurElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc) : 
SVGFilterPrimitiveStandardAttributesImpl(tagName, doc)
{
    m_filterEffect = 0;
}

SVGFEGaussianBlurElementImpl::~SVGFEGaussianBlurElementImpl()
{
}

SVGAnimatedStringImpl *SVGFEGaussianBlurElementImpl::in1() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedStringImpl>(m_in1, dummy);
}

SVGAnimatedNumberImpl *SVGFEGaussianBlurElementImpl::stdDeviationX() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_stdDeviationX, dummy);
}

SVGAnimatedNumberImpl *SVGFEGaussianBlurElementImpl::stdDeviationY() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_stdDeviationY, dummy);
}

void SVGFEGaussianBlurElementImpl::setStdDeviation(float stdDeviationX, float stdDeviationY)
{
}

void SVGFEGaussianBlurElementImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    KDOM::DOMString value(attr->value());
    if (attr->name() == SVGNames::stdDeviationAttr) {
        QStringList numbers = QStringList::split(' ', value.qstring());
        stdDeviationX()->setBaseVal(numbers[0].toDouble());
        if(numbers.count() == 1)
            stdDeviationY()->setBaseVal(numbers[0].toDouble());
        else
            stdDeviationY()->setBaseVal(numbers[1].toDouble());
    }
    else if (attr->name() == SVGNames::inAttr)
        in1()->setBaseVal(value.impl());
    else
        SVGFilterPrimitiveStandardAttributesImpl::parseMappedAttribute(attr);
}

KCanvasFEGaussianBlur *SVGFEGaussianBlurElementImpl::filterEffect() const
{
    if (!m_filterEffect)
        m_filterEffect = static_cast<KCanvasFEGaussianBlur *>(canvas()->renderingDevice()->createFilterEffect(FE_GAUSSIAN_BLUR));
    if (!m_filterEffect)
        return 0;
    m_filterEffect->setIn(KDOM::DOMString(in1()->baseVal()).qstring());
    setStandardAttributes(m_filterEffect);
    m_filterEffect->setStdDeviationX(stdDeviationX()->baseVal());
    m_filterEffect->setStdDeviationY(stdDeviationY()->baseVal());
    return m_filterEffect;
}

// vim:ts=4:noet
