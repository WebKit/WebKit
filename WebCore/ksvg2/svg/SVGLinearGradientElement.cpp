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
#include "SVGLinearGradientElement.h"

#include "SVGPaintServerLinearGradient.h"
#include "SVGHelper.h"
#include "SVGLength.h"
#include "SVGNames.h"
#include "SVGTransform.h"
#include "SVGTransformList.h"
#include "SVGUnitTypes.h"

namespace WebCore {

SVGLinearGradientElement::SVGLinearGradientElement(const QualifiedName& tagName, Document* doc)
    : SVGGradientElement(tagName, doc)
    , m_x1(new SVGLength(this, LM_WIDTH, viewportElement()))
    , m_y1(new SVGLength(this, LM_HEIGHT, viewportElement()))
    , m_x2(new SVGLength(this, LM_WIDTH, viewportElement()))
    , m_y2(new SVGLength(this, LM_HEIGHT, viewportElement()))
{
    // Spec : If the attribute is not specified, the effect is as if a value of "100%" were specified.
    m_x2->setValueAsString("100%");
}

SVGLinearGradientElement::~SVGLinearGradientElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGLinearGradientElement, SVGLength*, Length, length, X1, x1, SVGNames::x1Attr.localName(), m_x1.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGLinearGradientElement, SVGLength*, Length, length, Y1, y1, SVGNames::y1Attr.localName(), m_y1.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGLinearGradientElement, SVGLength*, Length, length, X2, x2, SVGNames::x2Attr.localName(), m_x2.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGLinearGradientElement, SVGLength*, Length, length, Y2, y2, SVGNames::y2Attr.localName(), m_y2.get())

void SVGLinearGradientElement::parseMappedAttribute(MappedAttribute* attr)
{
    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::x1Attr)
        x1BaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::y1Attr)
        y1BaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::x2Attr)
        x2BaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::y2Attr)
        y2BaseValue()->setValueAsString(value);
    else
        SVGGradientElement::parseMappedAttribute(attr);
}

void SVGLinearGradientElement::buildGradient(PassRefPtr<SVGPaintServerGradient> _grad) const
{
    rebuildStops(); // rebuild stops before possibly importing them from any referenced gradient.

    bool bbox = (gradientUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    
    x1()->setBboxRelative(bbox);
    y1()->setBboxRelative(bbox);
    x2()->setBboxRelative(bbox);
    y2()->setBboxRelative(bbox);
    
    float _x1 = x1()->value(), _y1 = y1()->value();
    float _x2 = x2()->value(), _y2 = y2()->value();

    RefPtr<SVGPaintServerLinearGradient> grad = WTF::static_pointer_cast<SVGPaintServerLinearGradient>(_grad);
    AffineTransform mat;
    if (gradientTransform()->numberOfItems() > 0)
        mat = gradientTransform()->consolidate()->matrix();

    DeprecatedString ref = href().deprecatedString();
    RefPtr<SVGPaintServer> pserver = getPaintServerById(document(), ref.mid(1));
    
    if (pserver && (pserver->type() == RadialGradientPaintServer || pserver->type() == LinearGradientPaintServer)) {
        bool isLinear = pserver->type() == LinearGradientPaintServer;
        SVGPaintServerGradient* gradient = static_cast<SVGPaintServerGradient*>(pserver.get());

        if (!hasAttribute(SVGNames::gradientUnitsAttr))
            bbox = gradient->boundingBoxMode();
            
        if (isLinear) {
            SVGPaintServerLinearGradient* linear = static_cast<SVGPaintServerLinearGradient*>(pserver.get());
            if (!hasAttribute(SVGNames::x1Attr))
                _x1 = linear->gradientStart().x();
            else if (bbox)
                _x1 *= 100.;

            if (!hasAttribute(SVGNames::y1Attr))
                _y1 = linear->gradientStart().y();
            else if (bbox)
                _y1 *= 100.;

            if (!hasAttribute(SVGNames::x2Attr))
                _x2 = linear->gradientEnd().x();
            else if (bbox)
                _x2 *= 100.;

            if (!hasAttribute(SVGNames::y2Attr))
                _y2 = linear->gradientEnd().y();
            else if (bbox)
                _y2 *= 100.;
        } else if (bbox) {
            _x1 *= 100.0;
            _y1 *= 100.0;
            _x2 *= 100.0;
            _y2 *= 100.0;
        }

        if (!hasAttribute(SVGNames::gradientTransformAttr))
            mat = gradient->gradientTransform();

        // Inherit color stops if empty
        if (grad->gradientStops().isEmpty())
            grad->setGradientStops(gradient);

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

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

