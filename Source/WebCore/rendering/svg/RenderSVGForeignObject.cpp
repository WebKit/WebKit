/*
 * Copyright (C) 2006 Apple Inc.
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved. 
 * Copyright (C) 2020, 2021, 2022 Igalia S.L.
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
#include "RenderSVGForeignObject.h"

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "LayoutRepainter.h"
#include "RenderLayer.h"
#include "RenderObject.h"
#include "RenderSVGBlockInlines.h"
#include "RenderSVGResource.h"
#include "RenderView.h"
#include "SVGElementTypeHelpers.h"
#include "SVGForeignObjectElement.h"
#include "SVGRenderSupport.h"
#include "SVGRenderingContext.h"
#include "SVGResourcesCache.h"
#include "TransformState.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGForeignObject);

RenderSVGForeignObject::RenderSVGForeignObject(SVGForeignObjectElement& element, RenderStyle&& style)
    : RenderSVGBlock(element, WTFMove(style))
{
}

RenderSVGForeignObject::~RenderSVGForeignObject() = default;

SVGForeignObjectElement& RenderSVGForeignObject::foreignObjectElement() const
{
    return downcast<SVGForeignObjectElement>(RenderSVGBlock::graphicsElement());
}

void RenderSVGForeignObject::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!shouldPaintSVGRenderer(paintInfo))
        return;

    if (paintInfo.phase == PaintPhase::ClippingMask) {
        // FIXME: [LBSE] Upstream clipping support
        // SVGRenderSupport::paintSVGClippingMask(*this, paintInfo);
        return;
    }

    auto adjustedPaintOffset = paintOffset + location();
    if (paintInfo.phase == PaintPhase::Mask) {
        // FIXME: [LBSE] Upstream masking support
        // SVGRenderSupport::paintSVGMask(*this, paintInfo, adjustedPaintOffset);
        return;
    }

    GraphicsContextStateSaver stateSaver(paintInfo.context());

    auto coordinateSystemOriginTranslation = adjustedPaintOffset - flooredLayoutPoint(objectBoundingBox().location());
    paintInfo.context().translate(coordinateSystemOriginTranslation.width(), coordinateSystemOriginTranslation.height());

    RenderSVGBlock::paint(paintInfo, paintOffset);
}

void RenderSVGForeignObject::updateLogicalWidth()
{
    setWidth(enclosingLayoutRect(m_viewport).width());
}

RenderBox::LogicalExtentComputedValues RenderSVGForeignObject::computeLogicalHeight(LayoutUnit, LayoutUnit logicalTop) const
{
    return { enclosingLayoutRect(m_viewport).height(), logicalTop, ComputedMarginValues() };
}

void RenderSVGForeignObject::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());

    auto& useForeignObjectElement = foreignObjectElement();
    SVGLengthContext lengthContext(&useForeignObjectElement);

    // Cache viewport boundaries
    auto x = useForeignObjectElement.x().value(lengthContext);
    auto y = useForeignObjectElement.y().value(lengthContext);
    auto width = useForeignObjectElement.width().value(lengthContext);
    auto height = useForeignObjectElement.height().value(lengthContext);
    m_viewport = { 0, 0, width, height };

    m_supplementalLayerTransform.makeIdentity();
    m_supplementalLayerTransform.translate(x, y);

    bool layoutChanged = everHadLayout() && selfNeedsLayout();
    RenderSVGBlock::layout();
    ASSERT(!needsLayout());

    setLocation(LayoutPoint());
    updateLayerTransform();

    // Invalidate all resources of this client if our layout changed.
    if (layoutChanged)
        SVGResourcesCache::clientLayoutChanged(*this);

    repainter.repaintAfterLayout();
}

LayoutRect RenderSVGForeignObject::overflowClipRect(const LayoutPoint& location, RenderFragmentContainer*, OverlayScrollbarSizeRelevancy, PaintPhase) const
{
    auto clipRect = enclosingLayoutRect(m_viewport);
    clipRect.moveBy(location);
    return clipRect;
}

void RenderSVGForeignObject::updateFromStyle()
{
    RenderSVGBlock::updateFromStyle();

    if (SVGRenderSupport::isOverflowHidden(*this))
        setHasNonVisibleOverflow();
}

void RenderSVGForeignObject::applyTransform(TransformationMatrix& transform, const RenderStyle& style, const FloatRect& boundingBox, OptionSet<RenderStyle::TransformOperationOption> options) const
{
    applySVGTransform(transform, foreignObjectElement(), style, boundingBox, std::nullopt, m_supplementalLayerTransform.isIdentity() ? std::nullopt : std::make_optional(m_supplementalLayerTransform), options);
}

}

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
