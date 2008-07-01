/*
     Copyright (C) 2005 Oliver Hunt <ojh16@student.canterbury.ac.nz>
     
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
     the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
     Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGFESpotLightElement.h"
#include "SVGSpotLightSource.h"

namespace WebCore {

SVGFESpotLightElement::SVGFESpotLightElement(const QualifiedName& tagName, Document* doc)
    : SVGFELightElement(tagName, doc)
{
}

SVGFESpotLightElement::~SVGFESpotLightElement()
{
}

LightSource* SVGFESpotLightElement::lightSource() const
{
    FloatPoint3D pos(x(), y(), z());

    // convert lookAt to a direction
    FloatPoint3D direction(pointsAtX() - pos.x(), 
                           pointsAtY() - pos.y(), 
                           pointsAtZ() - pos.z());

    direction.normalize();
    return new SpotLightSource(pos, direction, specularExponent(), limitingConeAngle());
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
