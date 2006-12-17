/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
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
#include "SVGNames.h"
#include "SVGStopElement.h"
#include "SVGTransform.h"
#include "SVGTransformList.h"
#include "SVGUnitTypes.h"

namespace WebCore {

SVGRadialGradientElement::SVGRadialGradientElement(const QualifiedName& tagName, Document* doc)
    : SVGGradientElement(tagName, doc)
    , m_cx(this, LengthModeWidth)
    , m_cy(this, LengthModeHeight)
    , m_r(this, LengthModeOther)
    , m_fx(this, LengthModeWidth)
    , m_fy(this, LengthModeHeight)
{
    // Spec: If the attribute is not specified, the effect is as if a value of "50%" were specified.
    setCxBaseValue(SVGLength(this, LengthModeWidth, "50%"));
    setCyBaseValue(SVGLength(this, LengthModeHeight, "50%"));
    setRBaseValue(SVGLength(this, LengthModeOther, "50%"));
}

SVGRadialGradientElement::~SVGRadialGradientElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGRadialGradientElement, SVGLength, Length, length, Cx, cx, SVGNames::cxAttr.localName(), m_cx)
ANIMATED_PROPERTY_DEFINITIONS(SVGRadialGradientElement, SVGLength, Length, length, Cy, cy, SVGNames::cyAttr.localName(), m_cy)
ANIMATED_PROPERTY_DEFINITIONS(SVGRadialGradientElement, SVGLength, Length, length, Fx, fx, SVGNames::fxAttr.localName(), m_fx)
ANIMATED_PROPERTY_DEFINITIONS(SVGRadialGradientElement, SVGLength, Length, length, Fy, fy, SVGNames::fyAttr.localName(), m_fy)
ANIMATED_PROPERTY_DEFINITIONS(SVGRadialGradientElement, SVGLength, Length, length, R, r, SVGNames::rAttr.localName(), m_r)

void SVGRadialGradientElement::parseMappedAttribute(MappedAttribute* attr)
{
    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::cxAttr)
        setCxBaseValue(SVGLength(this, LengthModeWidth, value));
    else if (attr->name() == SVGNames::cyAttr)
        setCyBaseValue(SVGLength(this, LengthModeHeight, value));
    else if (attr->name() == SVGNames::rAttr)
        setRBaseValue(SVGLength(this, LengthModeOther, value));
    else if (attr->name() == SVGNames::fxAttr)
        setFxBaseValue(SVGLength(this, LengthModeWidth, value));
    else if (attr->name() == SVGNames::fyAttr)
        setFyBaseValue(SVGLength(this, LengthModeHeight, value));
    else
        SVGGradientElement::parseMappedAttribute(attr);
}

void SVGRadialGradientElement::buildGradient(PassRefPtr<SVGPaintServerGradient> _grad) const
{
    rebuildStops(); // rebuild stops before possibly importing them from any referenced gradient.

    bool bbox = (gradientUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    bool fxSet = hasAttribute(SVGNames::fxAttr);
    bool fySet = hasAttribute(SVGNames::fyAttr);
 
    float _cx, _cy, _r, _fx, _fy;

    if (bbox) {
        _cx = cx().valueInSpecifiedUnits();
        if (SVGLength::isFraction(cx()))
            _cx *= 100.0;

        _cy = cy().valueInSpecifiedUnits();
        if (SVGLength::isFraction(cy()))
            _cy *= 100.0;

        _r = r().valueInSpecifiedUnits();
        if (SVGLength::isFraction(r()))
            _r *= 100.0;

        if (fxSet) {
            _fx = fx().valueInSpecifiedUnits();
            if (SVGLength::isFraction(fx()))
                _fx *= 100.0;
        } else
            _fx = _cx;
        
        if (fySet) {
            _fy = fy().valueInSpecifiedUnits();
            if (SVGLength::isFraction(fy()))
                _fy *= 100.0;
        } else
            _fy = _cy;
    } else {
        _cx = cx().value();
        _cy = cy().value();
        _r = r().value();
        _fx = fxSet ? fx().value() : _cx;
        _fy = fySet ? fy().value() : _cy;
    }

    RefPtr<SVGPaintServerRadialGradient> grad = WTF::static_pointer_cast<SVGPaintServerRadialGradient>(_grad);
    AffineTransform mat;
    if (gradientTransform()->numberOfItems() > 0)
        mat = gradientTransform()->consolidate()->matrix();

    DeprecatedString ref = href().deprecatedString();
    RefPtr<SVGPaintServer> pserver = getPaintServerById(document(), ref.mid(1));

    if (pserver && (pserver->type() == RadialGradientPaintServer || pserver->type() == LinearGradientPaintServer)) {
        bool isRadial = pserver->type() == RadialGradientPaintServer;
        RefPtr<SVGPaintServerGradient> gradient = WTF::static_pointer_cast<SVGPaintServerGradient>(pserver);

        if (!hasAttribute(SVGNames::gradientUnitsAttr))
            bbox = gradient->boundingBoxMode();

        if (isRadial) {
            RefPtr<SVGPaintServerRadialGradient> radial = WTF::static_pointer_cast<SVGPaintServerRadialGradient>(pserver);
            if (!hasAttribute(SVGNames::cxAttr))
                _cx = radial->gradientCenter().x();

            if (!hasAttribute(SVGNames::cyAttr))
                _cy = radial->gradientCenter().y();

            if (!fxSet)
                _fx = radial->gradientFocal().x();

            if (!fySet)
                _fy = radial->gradientFocal().y();

            if (!hasAttribute(SVGNames::rAttr))
                _r = radial->gradientRadius();
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
    }

    grad->setGradientTransform(mat);
    grad->setBoundingBoxMode(bbox);
    grad->setGradientCenter(FloatPoint(_cx, _cy));
    grad->setGradientFocal(FloatPoint(_fx, _fy));
    grad->setGradientRadius(_r);
}

}

#endif // SVG_SUPPORT

// vim:ts=4:noet
