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
#include "SVGTransformListImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "SVGAnimatedLengthImpl.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGLinearGradientElementImpl.h"
#include "SVGAnimatedTransformListImpl.h"

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasMatrix.h>
#include <kcanvas/KCanvasRegistry.h>
#include <kcanvas/KCanvasResources.h>
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/device/KRenderingPaintServerGradient.h>

using namespace KSVG;

SVGLinearGradientElementImpl::SVGLinearGradientElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix) : SVGGradientElementImpl(doc, id, prefix)
{
    m_x1 = m_y1 = m_x2 = m_y2 = 0;
}

SVGLinearGradientElementImpl::~SVGLinearGradientElementImpl()
{
    if(m_x1)
        m_x1->deref();
    if(m_y1)
        m_y1->deref();
    if(m_x2)
        m_x2->deref();
    if(m_y2)
        m_y2->deref();
}

SVGAnimatedLengthImpl *SVGLinearGradientElementImpl::x1() const
{
    // Spec : If the attribute is not specified, the effect is as if a value of "0%" were specified.
    return lazy_create<SVGAnimatedLengthImpl>(m_x1, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGLinearGradientElementImpl::y1() const
{
    // Spec : If the attribute is not specified, the effect is as if a value of "0%" were specified.
    return lazy_create<SVGAnimatedLengthImpl>(m_y1, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLengthImpl *SVGLinearGradientElementImpl::x2() const
{
    // Spec : If the attribute is not specified, the effect is as if a value of "100%" were specified.
    if(!m_x2)
    {
         lazy_create<SVGAnimatedLengthImpl>(m_x2, this, LM_WIDTH, viewportElement());
        m_x2->baseVal()->setValue(1.0);
        return m_x2;
    }

    return m_x2;
}

SVGAnimatedLengthImpl *SVGLinearGradientElementImpl::y2() const
{
    // Spec : If the attribute is not specified, the effect is as if a value of "0%" were specified.
    return lazy_create<SVGAnimatedLengthImpl>(m_y2, this, LM_HEIGHT, viewportElement());
}

void SVGLinearGradientElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
    int id = (attr->id() & NodeImpl_IdLocalMask);
    KDOM::DOMStringImpl *value = attr->value();
    switch(id)
    {
        case ATTR_X1:
        {
            x1()->baseVal()->setValueAsString(value);
            break;
        }
        case ATTR_Y1:
        {
            y1()->baseVal()->setValueAsString(value);
            break;
        }
        case ATTR_X2:
        {
            x2()->baseVal()->setValueAsString(value);
            break;
        }
        case ATTR_Y2:
        {
            y2()->baseVal()->setValueAsString(value);
            break;
        }
        default:
            SVGGradientElementImpl::parseAttribute(attr);
    };
}

void SVGLinearGradientElementImpl::buildGradient(KRenderingPaintServerGradient *_grad, KCanvas *canvas) const
{
    QString ref = KDOM::DOMString(href()->baseVal()).string();
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

    KRenderingPaintServer *pserver = 0;
    if(!ref.isEmpty())
        pserver = canvas->registry()->getPaintServerById(ref.mid(1));

    if(pserver && (pserver->type() == PS_RADIAL_GRADIENT || pserver->type() == PS_LINEAR_GRADIENT))
    {
        bool isLinear = pserver->type() == PS_LINEAR_GRADIENT;
        KRenderingPaintServerGradient *gradient = static_cast<KRenderingPaintServerGradient *>(pserver);

        if(!hasAttribute(KDOM::DOMString("gradientUnits").handle()))
            bbox = gradient->boundingBoxMode();
            
        if(isLinear)
        {
            KRenderingPaintServerLinearGradient *linear = static_cast<KRenderingPaintServerLinearGradient *>(pserver);
            if(!hasAttribute(KDOM::DOMString("x1").handle()))
                _x1 = linear->gradientStart().x();
            else if(bbox)
                _x1 *= 100.;

            if(!hasAttribute(KDOM::DOMString("y1").handle()))
                _y1 = linear->gradientStart().y();
            else if(bbox)
                _y1 *= 100.;

            if(!hasAttribute(KDOM::DOMString("x2").handle()))
                _x2 = linear->gradientEnd().x();
            else if(bbox)
                _x2 *= 100.;

            if(!hasAttribute(KDOM::DOMString("y2").handle()))
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

        if(!hasAttribute(KDOM::DOMString("gradientTransform").handle()))
            mat = gradient->gradientTransform();

        // Inherit color stops if empty
        if(grad->gradientStops().count() == 0)
        {
            KCSortedGradientStopList::Iterator it(gradient->gradientStops());
            for(;it.current();++it)
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
            _x1 *= 100.0;
            _y1 *= 100.0;
            _x2 *= 100.0;
            _y2 *= 100.0;
        }
    }

    grad->setGradientTransform(mat);
    grad->setBoundingBoxMode(bbox);
    grad->setGradientStart(QPoint(qRound(_x1), qRound(_y1)));
    grad->setGradientEnd(QPoint(qRound(_x2), qRound(_y2)));
}

KCanvasItem *SVGLinearGradientElementImpl::createCanvasItem(KCanvas *canvas, KRenderingStyle *) const
{
    KRenderingPaintServer *temp = canvas->renderingDevice()->createPaintServer(KCPaintServerType(PS_LINEAR_GRADIENT));
    KRenderingPaintServerLinearGradient *pserver = static_cast<KRenderingPaintServerLinearGradient *>(temp);

    pserver->setListener(const_cast<SVGLinearGradientElementImpl *>(this));

    canvas->registry()->addPaintServerById(KDOM::DOMString(getId()).string(), pserver);
    return 0;
}

void SVGLinearGradientElementImpl::resourceNotification() const
{
    // We're referenced by a "client", build the gradient now...
    KRenderingPaintServer *pserver = canvas()->registry()->getPaintServerById(KDOM::DOMString(getId()).string());
    KRenderingPaintServerGradient *gradient = static_cast<KRenderingPaintServerGradient *>(pserver);
    buildGradient(gradient, canvas());
}

// vim:ts=4:noet
