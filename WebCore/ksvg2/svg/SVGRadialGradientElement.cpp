/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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
#ifdef SVG_SUPPORT
#include "SVGRadialGradientElement.h"

#include "SVGPaintServerRadialGradient.h"
#include "SVGHelper.h"
#include "SVGLength.h"
#include "SVGMatrix.h"
#include "SVGNames.h"
#include "SVGStopElement.h"
#include "SVGTransform.h"
#include "SVGTransformList.h"
#include "SVGUnitTypes.h"

namespace WebCore {

SVGRadialGradientElement::SVGRadialGradientElement(const QualifiedName& tagName, Document* doc)
    : SVGGradientElement(tagName, doc)
    , m_cx(new SVGLength(this, LM_WIDTH, viewportElement()))
    , m_cy(new SVGLength(this, LM_HEIGHT, viewportElement()))
    , m_r(new SVGLength(this, LM_OTHER, viewportElement()))
    , m_fx(new SVGLength(this, LM_WIDTH, viewportElement()))
    , m_fy(new SVGLength(this, LM_HEIGHT, viewportElement()))
{
    // Spec: If the attribute is not specified, the effect is as if a value of "50%" were specified.
    m_cx->setValueAsString("50%");
    m_cy->setValueAsString("50%");
    m_r->setValueAsString("50%");
}

SVGRadialGradientElement::~SVGRadialGradientElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGRadialGradientElement, SVGLength*, Length, length, Cx, cx, SVGNames::cxAttr.localName(), m_cx.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGRadialGradientElement, SVGLength*, Length, length, Cy, cy, SVGNames::cyAttr.localName(), m_cy.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGRadialGradientElement, SVGLength*, Length, length, Fx, fx, SVGNames::fxAttr.localName(), m_fx.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGRadialGradientElement, SVGLength*, Length, length, Fy, fy, SVGNames::fyAttr.localName(), m_fy.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGRadialGradientElement, SVGLength*, Length, length, R, r, SVGNames::rAttr.localName(), m_r.get())

void SVGRadialGradientElement::parseMappedAttribute(MappedAttribute* attr)
{
    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::cxAttr)
        cxBaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::cyAttr)
        cyBaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::rAttr)
        rBaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::fxAttr)
        fxBaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::fyAttr)
        fyBaseValue()->setValueAsString(value);
    else
        SVGGradientElement::parseMappedAttribute(attr);
}

void SVGRadialGradientElement::buildGradient(PassRefPtr<SVGPaintServerGradient> _grad) const
{
    rebuildStops(); // rebuild stops before possibly importing them from any referenced gradient.

    bool bbox = (gradientUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    bool fxSet = hasAttribute(SVGNames::fxAttr);
    bool fySet = hasAttribute(SVGNames::fyAttr);
    cx()->setBboxRelative(bbox);
    cy()->setBboxRelative(bbox);
    r()->setBboxRelative(bbox);
    fx()->setBboxRelative(bbox);
    fy()->setBboxRelative(bbox);
    float _cx = cx()->value(), _cy = cy()->value();
    float _r = r()->value();
    float _fx = fxSet ? fx()->value() : _cx;
    float _fy = fySet ? fy()->value() : _cy;

    RefPtr<SVGPaintServerRadialGradient> grad = WTF::static_pointer_cast<SVGPaintServerRadialGradient>(_grad);
    AffineTransform mat;
    if (gradientTransform()->numberOfItems() > 0)
        mat = gradientTransform()->consolidate()->matrix()->matrix();

    DeprecatedString ref = href().deprecatedString();
    RefPtr<SVGPaintServer> pserver = getPaintServerById(document(), ref.mid(1));

    if (pserver && (pserver->type() == PS_RADIAL_GRADIENT || pserver->type() == PS_LINEAR_GRADIENT)) {
        bool isRadial = pserver->type() == PS_RADIAL_GRADIENT;
        RefPtr<SVGPaintServerGradient> gradient = WTF::static_pointer_cast<SVGPaintServerGradient>(pserver);

        if (!hasAttribute(SVGNames::gradientUnitsAttr))
            bbox = gradient->boundingBoxMode();

        if (isRadial) {
            RefPtr<SVGPaintServerRadialGradient> radial = WTF::static_pointer_cast<SVGPaintServerRadialGradient>(pserver);
            if (!hasAttribute(SVGNames::cxAttr))
                _cx = radial->gradientCenter().x();
            else if (bbox)
                _cx *= 100.;
            if (!hasAttribute(SVGNames::cyAttr))
                _cy = radial->gradientCenter().y();
            else if (bbox)
                _cy *= 100.;

            if (!fxSet)
                _fx = radial->gradientFocal().x();
            else if (bbox)
                _fx *= 100.;
            if (!fySet)
                _fy = radial->gradientFocal().y();
            else if (bbox)
                _fy *= 100.;

            if (!hasAttribute(SVGNames::rAttr))
                _r = radial->gradientRadius();
            else if (bbox)
                _r *= 100.;
        } else if (bbox) {
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
            grad->setGradientStops(gradient.get());

        if (!hasAttribute(SVGNames::spreadMethodAttr))
            grad->setGradientSpreadMethod(gradient->spreadMethod());
    } else {
        if (spreadMethod() == SVG_SPREADMETHOD_REFLECT)
            grad->setGradientSpreadMethod(SPREADMETHOD_REFLECT);
        else if (spreadMethod() == SVG_SPREADMETHOD_REPEAT)
            grad->setGradientSpreadMethod(SPREADMETHOD_REPEAT);
        else
            grad->setGradientSpreadMethod(SPREADMETHOD_PAD);

        if (bbox) {
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

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

