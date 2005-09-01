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

#include <kdom/core/AttrImpl.h>

#include "ksvg.h"
#include "svgattrs.h"
#include "SVGHelper.h"
#include "SVGMatrixImpl.h"
#include "SVGDocumentImpl.h"
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
#include <kcanvas/KCanvasRegistry.h>
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/device/KRenderingPaintServerGradient.h>

using namespace KSVG;

SVGRadialGradientElementImpl::SVGRadialGradientElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix) : SVGGradientElementImpl(doc, id, prefix)
{
    m_cx = m_cy = m_fx = m_fy = m_r = 0;
}

SVGRadialGradientElementImpl::~SVGRadialGradientElementImpl()
{
    if(m_cx)
        m_cx->deref();
    if(m_cy)
        m_cy->deref();
    if(m_fx)
        m_fx->deref();
    if(m_fy)
        m_fy->deref();
    if(m_r)
        m_r->deref();
}

SVGAnimatedLengthImpl *SVGRadialGradientElementImpl::cx() const
{
    // Spec: If the attribute is not specified, the effect is as if a value of "50%" were specified.
    if(!m_cx)
    {
        lazy_create<SVGAnimatedLengthImpl>(m_cx, this, LM_WIDTH, viewportElement());
        m_cx->baseVal()->setValue(0.5);
    }

    return m_cx;
}

SVGAnimatedLengthImpl *SVGRadialGradientElementImpl::cy() const
{
    // Spec: If the attribute is not specified, the effect is as if a value of "50%" were specified.
    if(!m_cy)
    {
        lazy_create<SVGAnimatedLengthImpl>(m_cy, this, LM_HEIGHT, viewportElement());
        m_cy->baseVal()->setValue(0.5);
    }

    return m_cy;
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
    if(!m_r)
    {
        lazy_create<SVGAnimatedLengthImpl>(m_r, this, LM_OTHER, viewportElement());
        m_r->baseVal()->setValue(0.5);
    }

    return m_r;
}

void SVGRadialGradientElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
    int id = (attr->id() & NodeImpl_IdLocalMask);
    KDOM::DOMStringImpl *value = attr->value();
    switch(id)
    {
        case ATTR_CX:
        {
            cx()->baseVal()->setValueAsString(value);
            break;
        }
        case ATTR_CY:
        {
            cy()->baseVal()->setValueAsString(value);
            break;
        }
        case ATTR_R:
        {
            r()->baseVal()->setValueAsString(value);
            break;
        }
        case ATTR_FX:
        {
            fx()->baseVal()->setValueAsString(value);
            break;
        }
        case ATTR_FY:
        {
            fy()->baseVal()->setValueAsString(value);
            break;
        }
        default:
            SVGGradientElementImpl::parseAttribute(attr);
    };
}

void SVGRadialGradientElementImpl::buildGradient(KRenderingPaintServerGradient *_grad, KCanvas *canvas) const
{
    QString ref = KDOM::DOMString(href()->baseVal()).string();

    bool bbox = (gradientUnits()->baseVal() == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    bool fxSet = hasAttribute(KDOM::DOMString("fx").handle());
    bool fySet = hasAttribute(KDOM::DOMString("fy").handle());
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

    KRenderingPaintServer *pserver = 0;
    if(!ref.isEmpty())
        pserver = canvas->registry()->getPaintServerById(ref.mid(1));

    if(pserver && (pserver->type() == PS_RADIAL_GRADIENT || pserver->type() == PS_LINEAR_GRADIENT))
    {
        bool isRadial = pserver->type() == PS_RADIAL_GRADIENT;
        KRenderingPaintServerGradient *gradient = static_cast<KRenderingPaintServerGradient *>(pserver);

        if(!hasAttribute(KDOM::DOMString("gradientUnits").handle()))
            bbox = gradient->boundingBoxMode();

        if(isRadial)
        {
            KRenderingPaintServerRadialGradient *radial = static_cast<KRenderingPaintServerRadialGradient *>(pserver);
            if(!hasAttribute(KDOM::DOMString("cx").handle()))
                _cx = radial->gradientCenter().x();
            else if(bbox)
                _cx *= 100.;
            if(!hasAttribute(KDOM::DOMString("cy").handle()))
                _cy = radial->gradientCenter().y();
            else if(bbox)
                _cy *= 100.;

            if(!hasAttribute(KDOM::DOMString("fx").handle()))
                _fx = radial->gradientFocal().x();
            else if(bbox)
                _fx *= 100.;
            if(!hasAttribute(KDOM::DOMString("fy").handle()))
                _fy = radial->gradientFocal().y();
            else if(bbox)
                _fy *= 100.;

            if(!hasAttribute(KDOM::DOMString("r").handle()))
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

        if(!hasAttribute(KDOM::DOMString("gradientTransform").handle()))
            mat = gradient->gradientTransform();

        // Inherit color stops if empty
        if(grad->gradientStops().count() == 0)
        {
            KCSortedGradientStopList::Iterator it(gradient->gradientStops());
            for(; it.current(); ++it)
                grad->gradientStops().addStop(it.current()->offset, it.current()->color);
        }

        if(!hasAttribute(KDOM::DOMString("spreadMethod").handle()))
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
    grad->setGradientCenter(QPoint(qRound(_cx), qRound(_cy)));
    grad->setGradientFocal(QPoint(qRound(_fx), qRound(_fy)));
    grad->setGradientRadius(_r);
}

KCanvasItem *SVGRadialGradientElementImpl::createCanvasItem(KCanvas *canvas, KRenderingStyle *) const
{
    KRenderingPaintServer *temp = canvas->renderingDevice()->createPaintServer(KCPaintServerType(PS_RADIAL_GRADIENT));
    KRenderingPaintServerRadialGradient *pserver = static_cast<KRenderingPaintServerRadialGradient *>(temp);

    pserver->setListener(const_cast<SVGRadialGradientElementImpl *>(this));

    canvas->registry()->addPaintServerById(KDOM::DOMString(getId()).string(), pserver);
    return 0;
}

void SVGRadialGradientElementImpl::resourceNotification() const
{
    // We're referenced by a "client", build the gradient now...
    KRenderingPaintServer *pserver = canvas()->registry()->getPaintServerById(KDOM::DOMString(getId()).string());
    KRenderingPaintServerGradient *gradient = static_cast<KRenderingPaintServerGradient *>(pserver);
    buildGradient(gradient, canvas());
}

// vim:ts=4:noet
