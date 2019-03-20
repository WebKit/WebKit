/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007, 2008 Rob Buis <buis@kde.org>
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
#include "SVGFEFloodElement.h"

#include "FEFlood.h"
#include "RenderStyle.h"
#include "SVGNames.h"
#include "SVGRenderStyle.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFEFloodElement);

inline SVGFEFloodElement::SVGFEFloodElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
{
    ASSERT(hasTagName(SVGNames::feFloodTag));
}

Ref<SVGFEFloodElement> SVGFEFloodElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEFloodElement(tagName, document));
}

bool SVGFEFloodElement::setFilterEffectAttribute(FilterEffect* effect, const QualifiedName& attrName)
{
    RenderObject* renderer = this->renderer();
    ASSERT(renderer);
    const RenderStyle& style = renderer->style();
    FEFlood* flood = static_cast<FEFlood*>(effect);

    if (attrName == SVGNames::flood_colorAttr)
        return flood->setFloodColor(style.svgStyle().floodColor());
    if (attrName == SVGNames::flood_opacityAttr)
        return flood->setFloodOpacity(style.svgStyle().floodOpacity());

    ASSERT_NOT_REACHED();
    return false;
}

RefPtr<FilterEffect> SVGFEFloodElement::build(SVGFilterBuilder*, Filter& filter) const
{
    RenderObject* renderer = this->renderer();
    if (!renderer)
        return nullptr;
    
    const SVGRenderStyle& svgStyle = renderer->style().svgStyle();

    Color color = renderer->style().colorByApplyingColorFilter(svgStyle.floodColor());
    float opacity = svgStyle.floodOpacity();

    return FEFlood::create(filter, color, opacity);
}

}
