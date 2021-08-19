/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
#include "RenderSVGInline.h"

#include "RenderSVGInlineText.h"
#include "RenderSVGResource.h"
#include "RenderSVGText.h"
#include "SVGInlineFlowBox.h"
#include "SVGResourcesCache.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGInline);
    
RenderSVGInline::RenderSVGInline(SVGGraphicsElement& element, RenderStyle&& style)
    : RenderInline(element, WTFMove(style))
{
    setAlwaysCreateLineBoxes();
}

std::unique_ptr<LegacyInlineFlowBox> RenderSVGInline::createInlineFlowBox()
{
    auto box = makeUnique<SVGInlineFlowBox>(*this);
    box->setHasVirtualLogicalHeight();
    return box;
}

FloatRect RenderSVGInline::objectBoundingBox() const
{
    if (auto* textAncestor = RenderSVGText::locateRenderSVGTextAncestor(*this))
        return textAncestor->objectBoundingBox();

    return FloatRect();
}

FloatRect RenderSVGInline::strokeBoundingBox() const
{
    if (auto* textAncestor = RenderSVGText::locateRenderSVGTextAncestor(*this))
        return textAncestor->strokeBoundingBox();

    return FloatRect();
}

FloatRect RenderSVGInline::repaintRectInLocalCoordinates() const
{
    if (auto* textAncestor = RenderSVGText::locateRenderSVGTextAncestor(*this))
        return textAncestor->repaintRectInLocalCoordinates();

    return FloatRect();
}

LayoutRect RenderSVGInline::clippedOverflowRect(const RenderLayerModelObject* repaintContainer, VisibleRectContext) const
{
    return SVGRenderSupport::clippedOverflowRectForRepaint(*this, repaintContainer);
}

std::optional<FloatRect> RenderSVGInline::computeFloatVisibleRectInContainer(const FloatRect& rect, const RenderLayerModelObject* container, VisibleRectContext context) const
{
    return SVGRenderSupport::computeFloatVisibleRectInContainer(*this, rect, container, context);
}

void RenderSVGInline::mapLocalToContainer(const RenderLayerModelObject* ancestorContainer, TransformState& transformState, OptionSet<MapCoordinatesMode>, bool* wasFixed) const
{
    SVGRenderSupport::mapLocalToContainer(*this, ancestorContainer, transformState, wasFixed);
}

const RenderObject* RenderSVGInline::pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap& geometryMap) const
{
    return SVGRenderSupport::pushMappingToContainer(*this, ancestorToStopAt, geometryMap);
}

void RenderSVGInline::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    auto* textAncestor = RenderSVGText::locateRenderSVGTextAncestor(*this);
    if (!textAncestor)
        return;

    FloatRect textBoundingBox = textAncestor->strokeBoundingBox();
    for (auto* box = firstLineBox(); box; box = box->nextLineBox())
        quads.append(localToAbsoluteQuad(FloatRect(textBoundingBox.x() + box->x(), textBoundingBox.y() + box->y(), box->logicalWidth(), box->logicalHeight()), UseTransforms, wasFixed));
}

void RenderSVGInline::willBeDestroyed()
{
    SVGResourcesCache::clientDestroyed(*this);
    RenderInline::willBeDestroyed();
}

void RenderSVGInline::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    if (diff == StyleDifference::Layout)
        setNeedsBoundariesUpdate();
    RenderInline::styleDidChange(diff, oldStyle);
    SVGResourcesCache::clientStyleChanged(*this, diff, style());
}

void RenderSVGInline::updateFromStyle()
{
    RenderInline::updateFromStyle();

    // SVG text layout code expects us to be an inline-level element.
    setInline(true);
}

}
