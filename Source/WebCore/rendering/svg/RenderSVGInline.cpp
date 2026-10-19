/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006-2024 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "InlineIteratorInlineBox.h"
#include "LegacyRenderSVGResource.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderObjectInlines.h"
#include "RenderSVGInlineInlines.h"
#include "RenderSVGInlineText.h"
#include "RenderSVGText.h"
#include "SVGGraphicsElement.h"
#include "SVGRenderSupport.h"
#include "SVGResourcesCache.h"
#include "Settings.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderSVGInline);
    
RenderSVGInline::RenderSVGInline(Type type, SVGGraphicsElement& element, Style::ComputedStyle&& style)
    : RenderBoxModelObject(type, element, WTF::move(style))
{
    ASSERT(isRenderSVGInline());
}

RenderSVGInline::RenderSVGInline(Type type, Document& document, Style::ComputedStyle&& style)
    : RenderBoxModelObject(type, document, WTF::move(style))
{
    ASSERT(isRenderSVGInline());
}

RenderSVGInline::~RenderSVGInline() = default;

bool RenderSVGInline::isChildAllowed(const RenderObject& child, const Style::ComputedStyle& style) const
{
    auto isEmptySVGInlineText = [](const RenderObject* object) {
        const auto svgInlineText = dynamicDowncast<RenderSVGInlineText>(object);
        return svgInlineText && svgInlineText->hasEmptyText();
    };

    if (isEmptySVGInlineText(&child))
        return false;

    return RenderElement::isChildAllowed(child, style);
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

FloatRect RenderSVGInline::decoratedBoundingBox() const
{
    if (auto* textAncestor = RenderSVGText::locateRenderSVGTextAncestor(*this))
        return textAncestor->decoratedBoundingBox();

    return { };
}

LayoutRect RenderSVGInline::clippedOverflowRect(const RenderLayerModelObject* repaintContainer, const VisibleRectContext& context) const
{
    if (document().settings().layerBasedSVGEngineEnabled())
        return RenderBoxModelObject::clippedOverflowRect(repaintContainer, context);
    return SVGRenderSupport::clippedOverflowRectForRepaint(*this, repaintContainer, context);
}

auto RenderSVGInline::rectsForRepaintingAfterLayout(const RenderLayerModelObject* repaintContainer, RepaintOutlineBounds repaintOutlineBounds) const -> RepaintRects
{
    if (document().settings().layerBasedSVGEngineEnabled())
        return RenderBoxModelObject::rectsForRepaintingAfterLayout(repaintContainer, repaintOutlineBounds);

    auto rects = RepaintRects { SVGRenderSupport::clippedOverflowRectForRepaint(*this, repaintContainer, visibleRectContextForRepaint()) };
    if (repaintOutlineBounds == RepaintOutlineBounds::Yes)
        rects.outlineBoundsRect = outlineBoundsForRepaint(repaintContainer);

    return rects;
}

std::optional<FloatRect> RenderSVGInline::computeFloatVisibleRectInContainer(const FloatRect& rect, const RenderLayerModelObject* container, const VisibleRectContext& context, VisibleRectState state) const
{
    if (document().settings().layerBasedSVGEngineEnabled()) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
    return SVGRenderSupport::computeFloatVisibleRectInContainer(*this, rect, container, context, state);
}

void RenderSVGInline::mapLocalToContainer(const RenderLayerModelObject* ancestorContainer, TransformState& transformState, OptionSet<MapCoordinatesMode> mode, bool* wasFixed) const
{
    if (document().settings().layerBasedSVGEngineEnabled()) {
        RenderBoxModelObject::mapLocalToContainer(ancestorContainer, transformState, mode, wasFixed);
        return;
    }
    SVGRenderSupport::mapLocalToContainer(*this, ancestorContainer, transformState, wasFixed);
}

const RenderElement* RenderSVGInline::pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap& geometryMap) const
{
    if (document().settings().layerBasedSVGEngineEnabled())
        return RenderBoxModelObject::pushMappingToContainer(ancestorToStopAt, geometryMap);
    return SVGRenderSupport::pushMappingToContainer(*this, ancestorToStopAt, geometryMap);
}

void RenderSVGInline::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    if (document().settings().layerBasedSVGEngineEnabled()) {
        RenderBoxModelObject::absoluteQuads(quads, wasFixed);
        return;
    }

    auto* textAncestor = RenderSVGText::locateRenderSVGTextAncestor(*this);
    if (!textAncestor)
        return;

    FloatRect textBoundingBox = textAncestor->strokeBoundingBox();
    for (auto& box : InlineIterator::inlineBoxesFor(*this)) {
        auto boxRect = box.visualRectIgnoringBlockDirection();
        boxRect.moveBy(textBoundingBox.location());
        quads.append(localToAbsoluteQuad(boxRect, MapCoordinatesMode::UseTransforms, wasFixed));
    }
}

#if PLATFORM(IOS_FAMILY)
void RenderSVGInline::absoluteQuadsForSelection(Vector<FloatQuad>& quads) const
{
    // Unlike absoluteQuads(), selection geometry is built from the line box rects even with the legacy SVG engine.
    RenderBoxModelObject::absoluteQuads(quads, nullptr);
}
#endif

void RenderSVGInline::willBeDestroyed()
{
    if (!document().settings().layerBasedSVGEngineEnabled())
        SVGResourcesCache::clientDestroyed(*this);

    RenderBoxModelObject::willBeDestroyed();
}

void RenderSVGInline::styleDidChange(Style::Difference diff, const Style::ComputedStyle* oldStyle)
{
    if (document().settings().layerBasedSVGEngineEnabled()) {
        RenderBoxModelObject::styleDidChange(diff, oldStyle);
        return;
    }

    if (diff == Style::DifferenceResult::Layout)
        invalidateCachedBoundaries();
    RenderBoxModelObject::styleDidChange(diff, oldStyle);
    if (!isAnonymous())
        SVGResourcesCache::clientStyleChanged(*this, diff, oldStyle, style());
}

bool RenderSVGInline::needsHasSVGTransformFlags() const
{
    if (isAnonymous())
        return false;
    return protect(graphicsElement())->hasTransformRelatedAttributes();
}

void RenderSVGInline::updateFromStyle()
{
    RenderBoxModelObject::updateFromStyle();

    if (document().settings().layerBasedSVGEngineEnabled()) {
        updateHasSVGTransformFlags();

        if (style().vectorEffect() == VectorEffect::NonScalingStroke)
            setMayHaveNonScalingStrokeInSubtreeIncludingAncestors();
    }

    // SVG text layout code expects us to be an inline-level element.
    setInline(true);
}

}
