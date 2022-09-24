/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>
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
#include "SVGTextElement.h"

#include "RenderSVGResource.h"
#include "RenderSVGText.h"
#include "SVGElementInlines.h"
#include "SVGNames.h"
#include "SVGRenderStyle.h"
#include "SVGTSpanElement.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGTextElement);

inline SVGTextElement::SVGTextElement(const QualifiedName& tagName, Document& document)
    : SVGTextPositioningElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    ASSERT(hasTagName(SVGNames::textTag));
}

Ref<SVGTextElement> SVGTextElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGTextElement(tagName, document));
}

RenderPtr<RenderElement> SVGTextElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSVGText>(*this, WTFMove(style));
}

bool SVGTextElement::childShouldCreateRenderer(const Node& child) const
{
    if (child.isTextNode()
        || child.hasTagName(SVGNames::aTag)
        || child.hasTagName(SVGNames::altGlyphTag)
        || child.hasTagName(SVGNames::textPathTag)
        || child.hasTagName(SVGNames::trefTag)
        || child.hasTagName(SVGNames::tspanTag))
        return true;

    return false;
}

}
