/*
 * Copyright (C) 2005 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SVGFESpotLightElement.h"

#include "SVGNames.h"
#include "SpotLightSource.h"

namespace WebCore {

inline SVGFESpotLightElement::SVGFESpotLightElement(const QualifiedName& tagName, Document& document)
    : SVGFELightElement(tagName, document)
{
    ASSERT(hasTagName(SVGNames::feSpotLightTag));
}

Ref<SVGFESpotLightElement> SVGFESpotLightElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFESpotLightElement(tagName, document));
}

Ref<LightSource> SVGFESpotLightElement::lightSource() const
{
    return SpotLightSource::create({ x(), y(), z() }, { pointsAtX(), pointsAtY(), pointsAtZ() }, specularExponent(), limitingConeAngle());
}

} // namespace WebCore
