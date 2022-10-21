/*
 * Copyright (C) 2006 Apple Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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
#include "RenderSVGBlock.h"

#include "RenderSVGBlockInlines.h"
#include "RenderSVGResource.h"
#include "RenderView.h"
#include "SVGGraphicsElement.h"
#include "SVGRenderSupport.h"
#include "SVGResourcesCache.h"
#include "StyleInheritedData.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGBlock);

RenderSVGBlock::RenderSVGBlock(SVGGraphicsElement& element, RenderStyle&& style)
    : RenderBlockFlow(element, WTFMove(style))
{
}

void RenderSVGBlock::updateFromStyle()
{
    RenderBlockFlow::updateFromStyle();

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (document().settings().layerBasedSVGEngineEnabled()) {
        updateHasSVGTransformFlags(graphicsElement());
        return;
    }
#endif

    // RenderSVGlock, used by Render(SVGText|ForeignObject), is not allowed to call setHasNonVisibleOverflow(true).
    // RenderBlock assumes a layer to be present when the overflow clip functionality is requested. Both
    // Render(SVGText|ForeignObject) return 'false' on 'requiresLayer'. Fine for RenderSVGText.
    //
    // If we want to support overflow rules for <foreignObject> we can choose between two solutions:
    // a) make LegacyRenderSVGForeignObject require layers and SVG layer aware
    // b) refactor overflow logic out of RenderLayer (as suggested by dhyatt), which is a large task
    //
    // Until this is resolved, disable overflow support. Opera/FF don't support it as well at the moment (Feb 2010).
    //
    // Note: This does NOT affect overflow handling on outer/inner <svg> elements - this is handled
    // manually by LegacyRenderSVGRoot - which owns the documents enclosing root layer and thus works fine.
    setHasNonVisibleOverflow(false);
}

void RenderSVGBlock::absoluteRects(Vector<IntRect>& rects, const LayoutPoint& accumulatedOffset) const
{
#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (document().settings().layerBasedSVGEngineEnabled()) {
        rects.append(snappedIntRect(LayoutRect(accumulatedOffset + location(), size())));
        return;
    }
#else
    UNUSED_PARAM(rects);
    UNUSED_PARAM(accumulatedOffset);
#endif

    // This code path should never be taken for SVG, as we're assuming useTransforms=true everywhere, absoluteQuads should be used.
    ASSERT_NOT_REACHED();
}

void RenderSVGBlock::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    quads.append(localToAbsoluteQuad(strokeBoundingBox(), UseTransforms, wasFixed));
}

void RenderSVGBlock::willBeDestroyed()
{
    SVGResourcesCache::clientDestroyed(*this);
    RenderBlockFlow::willBeDestroyed();
}

void RenderSVGBlock::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (diff == StyleDifference::Layout && !document().settings().layerBasedSVGEngineEnabled())
#else
    if (diff == StyleDifference::Layout)
#endif
        setNeedsBoundariesUpdate();
    RenderBlockFlow::styleDidChange(diff, oldStyle);
    SVGResourcesCache::clientStyleChanged(*this, diff, oldStyle, style());
}

#if ENABLE(LAYER_BASED_SVG_ENGINE)
FloatRect RenderSVGBlock::referenceBoxRect(CSSBoxType boxType) const
{
    if (document().settings().layerBasedSVGEngineEnabled()) {
        // Skip RenderBox::referenceBoxRect() implementation (generic CSS, not SVG), if LBSE is enabled.
        return RenderElement::referenceBoxRect(boxType);
    }

    return RenderBlockFlow::referenceBoxRect(boxType);
}
#endif

void RenderSVGBlock::computeOverflow(LayoutUnit oldClientAfterEdge, bool recomputeFloats)
{
    RenderBlockFlow::computeOverflow(oldClientAfterEdge, recomputeFloats);

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (document().settings().layerBasedSVGEngineEnabled())
        return;
#endif

    const auto* textShadow = style().textShadow();
    if (!textShadow)
        return;

    LayoutRect borderRect = borderBoxRect();
    textShadow->adjustRectForShadow(borderRect);
    addVisualOverflow(snappedIntRect(borderRect));
}

LayoutRect RenderSVGBlock::clippedOverflowRect(const RenderLayerModelObject* repaintContainer, VisibleRectContext context) const
{
#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (document().settings().layerBasedSVGEngineEnabled()) {
        if (isInsideEntirelyHiddenLayer())
            return { };

        ASSERT(!view().frameView().layoutContext().isPaintOffsetCacheEnabled());
        return computeRect(visualOverflowRect(), repaintContainer, context);
    }
#else
    UNUSED_PARAM(context);
#endif

    return SVGRenderSupport::clippedOverflowRectForRepaint(*this, repaintContainer);
}

std::optional<LayoutRect> RenderSVGBlock::computeVisibleRectInContainer(const LayoutRect& rect, const RenderLayerModelObject* container, VisibleRectContext context) const
{
#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (document().settings().layerBasedSVGEngineEnabled())
        return computeVisibleRectInSVGContainer(rect, container, context);
#endif

    std::optional<FloatRect> adjustedRect = computeFloatVisibleRectInContainer(rect, container, context);
    if (adjustedRect)
        return enclosingLayoutRect(*adjustedRect);
    return std::nullopt;
}

std::optional<FloatRect> RenderSVGBlock::computeFloatVisibleRectInContainer(const FloatRect& rect, const RenderLayerModelObject* container, VisibleRectContext context) const
{
#if ENABLE(LAYER_BASED_SVG_ENGINE)
    ASSERT(!document().settings().layerBasedSVGEngineEnabled());
#endif
    return SVGRenderSupport::computeFloatVisibleRectInContainer(*this, rect, container, context);
}

void RenderSVGBlock::mapLocalToContainer(const RenderLayerModelObject* ancestorContainer, TransformState& transformState, OptionSet<MapCoordinatesMode> mode, bool* wasFixed) const
{
#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (document().settings().layerBasedSVGEngineEnabled()) {
        RenderBlock::mapLocalToContainer(ancestorContainer, transformState, mode, wasFixed);
        return;
    }
#else
    UNUSED_PARAM(mode);
#endif
    SVGRenderSupport::mapLocalToContainer(*this, ancestorContainer, transformState, wasFixed);
}

const RenderObject* RenderSVGBlock::pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap& geometryMap) const
{
#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (document().settings().layerBasedSVGEngineEnabled())
        return RenderBlock::pushMappingToContainer(ancestorToStopAt, geometryMap);
#endif

    return SVGRenderSupport::pushMappingToContainer(*this, ancestorToStopAt, geometryMap);
}

#if ENABLE(LAYER_BASED_SVG_ENGINE)
LayoutSize RenderSVGBlock::offsetFromContainer(RenderElement& container, const LayoutPoint&, bool*) const
{
    ASSERT_UNUSED(container, &container == this->container());
    ASSERT(!isInFlowPositioned());
    ASSERT(!isAbsolutelyPositioned());
    ASSERT(!isInline());
    return LayoutSize();
}
#endif

}
