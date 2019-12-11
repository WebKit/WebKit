/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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
#include "SVGSetElement.h"

#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGSetElement);
    
inline SVGSetElement::SVGSetElement(const QualifiedName& tagName, Document& document)
    : SVGAnimateElementBase(tagName, document)
{
    setAnimationMode(AnimationMode::To);
    ASSERT(hasTagName(SVGNames::setTag));
}

Ref<SVGSetElement> SVGSetElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGSetElement(tagName, document));
}

void SVGSetElement::updateAnimationMode()
{
    // No-op, as <set> has a constant animation mode of AnimationMode::To.
    // See: http://www.w3.org/TR/SVG/single-page.html#animate-SetElement
}

}
