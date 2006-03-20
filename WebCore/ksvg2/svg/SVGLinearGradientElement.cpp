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
#include <kdom/core/Attr.h>

#include "ksvg.h"
#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGMatrix.h"
#include "SVGTransform.h"
#include "SVGTransformList.h"
#include "SVGAnimatedString.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGLinearGradientElement.h"
#include "SVGAnimatedTransformList.h"

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasMatrix.h>
#include <kcanvas/KCanvasResources.h>
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/device/KRenderingPaintServerGradient.h>

using namespace WebCore;

SVGLinearGradientElement::SVGLinearGradientElement(const QualifiedName& tagName, Document *doc) : SVGGradientElement(tagName, doc)
{
}

SVGLinearGradientElement::~SVGLinearGradientElement()
{
}

SVGAnimatedLength *SVGLinearGradientElement::x1() const
{
    // Spec : If the attribute is not specified, the effect is as if a value of "0%" were specified.
    return lazy_create<SVGAnimatedLength>(m_x1, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLength *SVGLinearGradientElement::y1() const
{
    // Spec : If the attribute is not specified, the effect is as if a value of "0%" were specified.
    return lazy_create<SVGAnimatedLength>(m_y1, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLength *SVGLinearGradientElement::x2() const
{
    // Spec : If the attribute is not specified, the effect is as if a value of "100%" were specified.
    if (!m_x2) {
        lazy_create<SVGAnimatedLength>(m_x2, this, LM_WIDTH, viewportElement());
        m_x2->baseVal()->setValue(1.0);
    }

    return m_x2.get();
}

SVGAnimatedLength *SVGLinearGradientElement::y2() const
{
    // Spec : If the attribute is not specified, the effect is as if a value of "0%" were specified.
    return lazy_create<SVGAnimatedLength>(m_y2, this, LM_HEIGHT, viewportElement());
}

void SVGLinearGradientElement::parseMappedAttribute(MappedAttribute *attr)
{
    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::x1Attr)
        x1()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::y1Attr)
        y1()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::x2Attr)
        x2()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::y2Attr)
        y2()->baseVal()->setValueAsString(value.impl());
    else
        SVGGradientElement::parseMappedAttribute(attr);
}

void SVGLinearGradientElement::buildGradient(KRenderingPaintServerGradient *_grad) const
{
    rebuildStops(); // rebuild stops before possibly importing them from any referenced gradient.

    bool bbox = (gradientUnits()->baseVal() == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    
    x1()->baseVal()->setBboxRelative(bbox);
    y1()->baseVal()->setBboxRelative(bbox);
    x2()->baseVal()->setBboxRelative(bbox);
    y2()->baseVal()->setBboxRelative(bbox);
    
    float _x1 = x1()->baseVal()->value(), _y1 = y1()->baseVal()->value();
    float _x2 = x2()->baseVal()->value(), _y2 = y2()->baseVal()->value();

    KRenderingPaintServerLinearGradient *grad = static_cast<KRenderingPaintServerLinearGradient *>(_grad);
    KCanvasMatrix mat;
    if(gradientTransform()->baseVal()->numberOfItems() > 0)
        mat = KCanvasMatrix(gradientTransform()->baseVal()->consolidate()->matrix()->qmatrix());

    DeprecatedString ref = String(href()->baseVal()).deprecatedString();
    KRenderingPaintServer *pserver = getPaintServerById(getDocument(), ref.mid(1));
    
    if(pserver && (pserver->type() == PS_RADIAL_GRADIENT || pserver->type() == PS_LINEAR_GRADIENT))
    {
        bool isLinear = pserver->type() == PS_LINEAR_GRADIENT;
        KRenderingPaintServerGradient *gradient = static_cast<KRenderingPaintServerGradient *>(pserver);

        if(!hasAttribute(SVGNames::gradientUnitsAttr))
            bbox = gradient->boundingBoxMode();
            
        if(isLinear)
        {
            KRenderingPaintServerLinearGradient *linear = static_cast<KRenderingPaintServerLinearGradient *>(pserver);
            if(!hasAttribute(SVGNames::x1Attr))
                _x1 = linear->gradientStart().x();
            else if(bbox)
                _x1 *= 100.;

            if(!hasAttribute(SVGNames::y1Attr))
                _y1 = linear->gradientStart().y();
            else if(bbox)
                _y1 *= 100.;

            if(!hasAttribute(SVGNames::x2Attr))
                _x2 = linear->gradientEnd().x();
            else if(bbox)
                _x2 *= 100.;

            if(!hasAttribute(SVGNames::y2Attr))
                _y2 = linear->gradientEnd().y();
            else if(bbox)
                _y2 *= 100.;
        }
        else if(bbox)
        {
            _x1 *= 100.0;
            _y1 *= 100.0;
            _x2 *= 100.0;
            _y2 *= 100.0;
        }

        if(!hasAttribute(SVGNames::gradientTransformAttr))
            mat = gradient->gradientTransform();

        // Inherit color stops if empty
        if (grad->gradientStops().isEmpty())
            grad->setGradientStops(gradient);

        if(!hasAttribute(SVGNames::spreadMethodAttr))
            grad->setGradientSpreadMethod(gradient->spreadMethod());
    }
    else
    {
        if(spreadMethod()->baseVal() == SVG_SPREADMETHOD_REFLECT)
            grad->setGradientSpreadMethod(SPREADMETHOD_REFLECT);
        else if(spreadMethod()->baseVal() == SVG_SPREADMETHOD_REPEAT)
            grad->setGradientSpreadMethod(SPREADMETHOD_REPEAT);
        else
            grad->setGradientSpreadMethod(SPREADMETHOD_PAD);

        if(bbox)
        {
            _x1 *= 100.0;
            _y1 *= 100.0;
            _x2 *= 100.0;
            _y2 *= 100.0;
        }
    }

    grad->setGradientTransform(mat);
    grad->setBoundingBoxMode(bbox);
    grad->setGradientStart(FloatPoint(_x1, _y1));
    grad->setGradientEnd(FloatPoint(_x2, _y2));
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

