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

#include "LegacyRenderSVGResource.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderSVGInlineInlines.h"
#include "RenderSVGInlineText.h"
#include "RenderSVGText.h"
#include "SVGGraphicsElement.h"
#include "SVGInlineFlowBox.h"
#include "SVGRenderSupport.h"
#include "SVGResourcesCache.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderSVGInline);
    
RenderSVGInline::RenderSVGInline(Type type, SVGGraphicsElement& element, RenderStyle&& style)
    : RenderInline(type, element, WTFMove(style))
{
    ASSERT(isRenderSVGInline());
}

RenderSVGInline::~RenderSVGInline() = default;

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

FloatRect RenderSVGInline::repaintRectInLocalCoordinates(RepaintRectCalculation repaintRectCalculation) const
{
    if (auto* textAncestor = RenderSVGText::locateRenderSVGTextAncestor(*this))
        return textAncestor->repaintRectInLocalCoordinates(repaintRectCalculation);

    return FloatRect();
}

LayoutRect RenderSVGInline::clippedOverflowRect(const RenderLayerModelObject* repaintContainer, VisibleRectContext context) const
{
    if (document().settings().layerBasedSVGEngineEnabled())
        return RenderInline::clippedOverflowRect(repaintContainer, context);
    return SVGRenderSupport::clippedOverflowRectForRepaint(*this, repaintContainer, context);
}

auto RenderSVGInline::rectsForRepaintingAfterLayout(const RenderLayerModelObject* repaintContainer, RepaintOutlineBounds repaintOutlineBounds) const -> RepaintRects
{
    if (document().settings().layerBasedSVGEngineEnabled())
        return RenderInline::rectsForRepaintingAfterLayout(repaintContainer, repaintOutlineBounds);

    auto rects = RepaintRects { SVGRenderSupport::clippedOverflowRectForRepaint(*this, repaintContainer, visibleRectContextForRepaint()) };
    if (repaintOutlineBounds == RepaintOutlineBounds::Yes)
        rects.outlineBoundsRect = outlineBoundsForRepaint(repaintContainer);

    return rects;
}

std::optional<FloatRect> RenderSVGInline::computeFloatVisibleRectInContainer(const FloatRect& rect, const RenderLayerModelObject* container, VisibleRectContext context) const
{
    if (document().settings().layerBasedSVGEngineEnabled()) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
    return SVGRenderSupport::computeFloatVisibleRectInContainer(*this, rect, container, context);
}

void RenderSVGInline::mapLocalToContainer(const RenderLayerModelObject* ancestorContainer, TransformState& transformState, OptionSet<MapCoordinatesMode> mode, bool* wasFixed) const
{
    if (document().settings().layerBasedSVGEngineEnabled()) {
        RenderInline::mapLocalToContainer(ancestorContainer, transformState, mode, wasFixed);
        return;
    }
    SVGRenderSupport::mapLocalToContainer(*this, ancestorContainer, transformState, wasFixed);
}

const RenderObject* RenderSVGInline::pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap& geometryMap) const
{
    if (document().settings().layerBasedSVGEngineEnabled())
        return RenderInline::pushMappingToContainer(ancestorToStopAt, geometryMap);
    return SVGRenderSupport::pushMappingToContainer(*this, ancestorToStopAt, geometryMap);
}

void RenderSVGInline::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    if (document().settings().layerBasedSVGEngineEnabled()) {
        RenderInline::absoluteQuads(quads, wasFixed);
        return;
    }

    auto* textAncestor = RenderSVGText::locateRenderSVGTextAncestor(*this);
    if (!textAncestor)
        return;

    FloatRect textBoundingBox = textAncestor->strokeBoundingBox();
    for (auto* box = firstLegacyInlineBox(); box; box = box->nextLineBox())
        quads.append(localToAbsoluteQuad(FloatRect(textBoundingBox.x() + box->x(), textBoundingBox.y() + box->y(), box->logicalWidth(), box->logicalHeight()), UseTransforms, wasFixed));
}

void RenderSVGInline::willBeDestroyed()
{
    if (document().settings().layerBasedSVGEngineEnabled()) {
        RenderInline::willBeDestroyed();
        return;
    }

    SVGResourcesCache::clientDestroyed(*this);
    RenderInline::willBeDestroyed();
}

void RenderSVGInline::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    if (document().settings().layerBasedSVGEngineEnabled()) {
        RenderInline::styleDidChange(diff, oldStyle);
        return;
    }

    if (diff == StyleDifference::Layout)
        invalidateCachedBoundaries();
    RenderInline::styleDidChange(diff, oldStyle);
    SVGResourcesCache::clientStyleChanged(*this, diff, oldStyle, style());
}

bool RenderSVGInline::needsHasSVGTransformFlags() const
{
    return graphicsElement().hasTransformRelatedAttributes();
}

void RenderSVGInline::updateFromStyle()
{
    RenderInline::updateFromStyle();

    if (document().settings().layerBasedSVGEngineEnabled())
        updateHasSVGTransformFlags();

    // SVG text layout code expects us to be an inline-level element.
    setInline(true);
}

}
