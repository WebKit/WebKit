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
#include <kdom/core/AttrImpl.h>

#include "ksvg.h"
#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGMatrixImpl.h"
#include "SVGTransformImpl.h"
#include "SVGStopElementImpl.h"
#include "SVGTransformListImpl.h"
#include "SVGAnimatedLengthImpl.h"
#include "SVGAnimatedNumberImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGRadialGradientElementImpl.h"
#include "SVGAnimatedTransformListImpl.h"

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasMatrix.h>
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/device/KRenderingPaintServerGradient.h>

using namespace KSVG;

SVGRadialGradientElementImpl::SVGRadialGradientElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc) : SVGGradientElementImpl(tagName, doc)
{
}

SVGRadialGradientElementImpl::~SVGRadialGradientElementImpl()
{
}

SVGAnimatedLengthImpl *SVGRadialGradientElementImpl::cx() const
{
    // Spec: If the attribute is not specified, the effect is as if a value of "50%" were specified.
    if (!m_cx) {
        lazy_create<SVGAnimatedLengthImpl>(m_cx, this, LM_WIDTH, viewportElement());
        m_cx->baseVal()->setValue(0.5);
    }

    return m_cx.get();
}

SVGAnimatedLengthImpl *SVGRadialGradientElementImpl::cy() const
{
    // Spec: If the attribute is not specified, the effect is as if a value of "50%" were specified.
    if (!m_cy) {
        lazy_create<SVGAnimatedLengthImpl>(m_cy, this, LM_HEIGHT, viewportElement());
        m_cy->baseVal()->setValue(0.5);
    }

    return m_cy.get();
}

SVGAnimatedLengthImpl *SVGRadialGradientElementImpl::fx() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_fx, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGRadialGradientElementImpl::fy() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_fy, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLengthImpl *SVGRadialGradientElementImpl::r() const
{
    // Spec: If the attribute is not specified, the effect is as if a value of "50%" were specified.
    if (!m_r) {
        lazy_create<SVGAnimatedLengthImpl>(m_r, this, LM_OTHER, viewportElement());
        m_r->baseVal()->setValue(0.5);
    }

    return m_r.get();
}

void SVGRadialGradientElementImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    const KDOM::AtomicString& value = attr->value();
    if (attr->name() == SVGNames::cxAttr)
        cx()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::cyAttr)
        cy()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::rAttr)
        r()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::fxAttr)
        fx()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::fyAttr)
        fy()->baseVal()->setValueAsString(value.impl());
    else
        SVGGradientElementImpl::parseMappedAttribute(attr);
}

void SVGRadialGradientElementImpl::buildGradient(KRenderingPaintServerGradient *_grad) const
{
    rebuildStops(); // rebuild stops before possibly importing them from any referenced gradient.

    bool bbox = (gradientUnits()->baseVal() == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    bool fxSet = hasAttribute(SVGNames::fxAttr);
    bool fySet = hasAttribute(SVGNames::fyAttr);
    cx()->baseVal()->setBboxRelative(bbox);
    cy()->baseVal()->setBboxRelative(bbox);
    r()->baseVal()->setBboxRelative(bbox);
    fx()->baseVal()->setBboxRelative(bbox);
    fy()->baseVal()->setBboxRelative(bbox);
    float _cx = cx()->baseVal()->value(), _cy = cy()->baseVal()->value();
    float _r = r()->baseVal()->value();
    float _fx = fxSet ? fx()->baseVal()->value() : _cx;
    float _fy = fySet ? fy()->baseVal()->value() : _cy;

    KRenderingPaintServerRadialGradient *grad = static_cast<KRenderingPaintServerRadialGradient *>(_grad);
    KCanvasMatrix mat;
    if(gradientTransform()->baseVal()->numberOfItems() > 0)
        mat = KCanvasMatrix(gradientTransform()->baseVal()->consolidate()->matrix()->qmatrix());

    QString ref = KDOM::DOMString(href()->baseVal()).qstring();
    KRenderingPaintServer *pserver = getPaintServerById(getDocument(), ref.mid(1));

    if(pserver && (pserver->type() == PS_RADIAL_GRADIENT || pserver->type() == PS_LINEAR_GRADIENT))
    {
        bool isRadial = pserver->type() == PS_RADIAL_GRADIENT;
        KRenderingPaintServerGradient *gradient = static_cast<KRenderingPaintServerGradient *>(pserver);

        if(!hasAttribute(SVGNames::gradientUnitsAttr))
            bbox = gradient->boundingBoxMode();

        if(isRadial)
        {
            KRenderingPaintServerRadialGradient *radial = static_cast<KRenderingPaintServerRadialGradient *>(pserver);
            if(!hasAttribute(SVGNames::cxAttr))
                _cx = radial->gradientCenter().x();
            else if(bbox)
                _cx *= 100.;
            if(!hasAttribute(SVGNames::cyAttr))
                _cy = radial->gradientCenter().y();
            else if(bbox)
                _cy *= 100.;

            if(!fxSet)
                _fx = radial->gradientFocal().x();
            else if(bbox)
                _fx *= 100.;
            if(!fySet)
                _fy = radial->gradientFocal().y();
            else if(bbox)
                _fy *= 100.;

            if(!hasAttribute(SVGNames::rAttr))
                _r = radial->gradientRadius();
            else if(bbox)
                _r *= 100.;
        }
        else if(bbox)
        {
            _cx *= 100.0;
            _cy *= 100.0;
            _fx *= 100.0;
            _fy *= 100.0;
            _r *= 100.0;
        }

        if (!hasAttribute(SVGNames::gradientTransformAttr))
            mat = gradient->gradientTransform();

        // Inherit color stops if empty
        if (grad->gradientStops().isEmpty())
            grad->setGradientStops(gradient);

        if (!hasAttribute(SVGNames::spreadMethodAttr))
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
            _cx *= 100.0;
            _cy *= 100.0;
            _fx *= 100.0;
            _fy *= 100.0;
            _r *= 100.0;
        }
    }

    grad->setGradientTransform(mat);
    grad->setBoundingBoxMode(bbox);
    grad->setGradientCenter(FloatPoint(_cx, _cy));
    grad->setGradientFocal(FloatPoint(_fx, _fy));
    grad->setGradientRadius(_r);
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

