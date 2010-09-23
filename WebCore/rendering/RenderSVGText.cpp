/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 *               2006 Alexander Kellett <lypanov@kde.org>
 *               2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 *               2007 Nikolas Zimmermann <zimmermann@kde.org>
 *               2008 Rob Buis <buis@kde.org>
 *               2009 Dirk Schulze <krit@webkit.org>
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
 *
 */

#include "config.h"

#if ENABLE(SVG)
#include "RenderSVGText.h"

#include "FloatConversion.h"
#include "FloatQuad.h"
#include "GraphicsContext.h"
#include "HitTestRequest.h"
#include "PointerEventsHitRules.h"
#include "RenderLayer.h"
#include "RenderSVGResource.h"
#include "RenderSVGRoot.h"
#include "SVGLengthList.h"
#include "SVGRenderSupport.h"
#include "SVGRootInlineBox.h"
#include "SVGTextElement.h"
#include "SVGTextLayoutBuilder.h"
#include "SVGTransformList.h"
#include "SVGURIReference.h"
#include "SimpleFontData.h"
#include "TransformState.h"

namespace WebCore {

RenderSVGText::RenderSVGText(SVGTextElement* node) 
    : RenderSVGBlock(node)
    , m_needsTransformUpdate(true)
{
}

IntRect RenderSVGText::clippedOverflowRectForRepaint(RenderBoxModelObject* repaintContainer)
{
    return SVGRenderSupport::clippedOverflowRectForRepaint(this, repaintContainer);
}

void RenderSVGText::computeRectForRepaint(RenderBoxModelObject* repaintContainer, IntRect& repaintRect, bool fixed)
{
    SVGRenderSupport::computeRectForRepaint(this, repaintContainer, repaintRect, fixed);
}

void RenderSVGText::mapLocalToContainer(RenderBoxModelObject* repaintContainer, bool fixed, bool useTransforms, TransformState& transformState) const
{
    SVGRenderSupport::mapLocalToContainer(this, repaintContainer, fixed, useTransforms, transformState);
}

void RenderSVGText::layout()
{
    ASSERT(needsLayout());
    LayoutRepainter repainter(*this, m_everHadLayout && checkForRepaintDuringLayout());

    bool updateCachedBoundariesInParents = false;
    if (m_needsTransformUpdate) {
        SVGTextElement* text = static_cast<SVGTextElement*>(node());
        m_localTransform = text->animatedLocalTransform();
        m_needsTransformUpdate = false;
        updateCachedBoundariesInParents = true;
    }

    SVGTextLayoutBuilder layoutBuilder;
    layoutBuilder.buildLayoutAttributesForTextSubtree(this);

    // Reduced version of RenderBlock::layoutBlock(), which only takes care of SVG text.
    // All if branches that could cause early exit in RenderBlocks layoutBlock() method are turned into assertions.
    ASSERT(!isInline());
    ASSERT(!layoutOnlyPositionedObjects());
    ASSERT(!scrollsOverflow());
    ASSERT(!hasControlClip());
    ASSERT(!hasColumns());
    ASSERT(!positionedObjects());
    ASSERT(!m_overflow);
    ASSERT(!isAnonymousBlock());

    if (!firstChild())
        setChildrenInline(true);

    // FIXME: We need to find a way to only layout the child boxes, if needed.
    FloatRect oldBoundaries = objectBoundingBox();
    ASSERT(childrenInline());
    forceLayoutInlineChildren();

    if (!updateCachedBoundariesInParents)
        updateCachedBoundariesInParents = oldBoundaries != objectBoundingBox();

    // Invalidate all resources of this client if our layout changed.
    if (m_everHadLayout && selfNeedsLayout())
        SVGResourcesCache::clientLayoutChanged(this);

    // If our bounds changed, notify the parents.
    if (updateCachedBoundariesInParents)
        RenderSVGBlock::setNeedsBoundariesUpdate();

    repainter.repaintAfterLayout();
    setNeedsLayout(false);
}

RootInlineBox* RenderSVGText::createRootInlineBox() 
{
    RootInlineBox* box = new (renderArena()) SVGRootInlineBox(this);
    box->setHasVirtualLogicalHeight();
    return box;
}

bool RenderSVGText::nodeAtFloatPoint(const HitTestRequest& request, HitTestResult& result, const FloatPoint& pointInParent, HitTestAction hitTestAction)
{
    PointerEventsHitRules hitRules(PointerEventsHitRules::SVG_TEXT_HITTESTING, request, style()->pointerEvents());
    bool isVisible = (style()->visibility() == VISIBLE);
    if (isVisible || !hitRules.requireVisible) {
        if ((hitRules.canHitStroke && (style()->svgStyle()->hasStroke() || !hitRules.requireStroke))
            || (hitRules.canHitFill && (style()->svgStyle()->hasFill() || !hitRules.requireFill))) {
            FloatPoint localPoint = localToParentTransform().inverse().mapPoint(pointInParent);

            if (!SVGRenderSupport::pointInClippingArea(this, localPoint))
                return false;       

            return RenderBlock::nodeAtPoint(request, result, (int)localPoint.x(), (int)localPoint.y(), 0, 0, hitTestAction);
        }
    }

    return false;
}

bool RenderSVGText::nodeAtPoint(const HitTestRequest&, HitTestResult&, int, int, int, int, HitTestAction)
{
    ASSERT_NOT_REACHED();
    return false;
}

void RenderSVGText::absoluteQuads(Vector<FloatQuad>& quads)
{
    quads.append(localToAbsoluteQuad(strokeBoundingBox()));
}

void RenderSVGText::paint(PaintInfo& paintInfo, int, int)
{
    if (paintInfo.context->paintingDisabled())
        return;

    if (paintInfo.phase != PaintPhaseForeground
     && paintInfo.phase != PaintPhaseSelfOutline
     && paintInfo.phase != PaintPhaseSelection)
         return;

    PaintInfo blockInfo(paintInfo);
    blockInfo.context->save();
    blockInfo.applyTransform(localToParentTransform());
    RenderBlock::paint(blockInfo, 0, 0);
    blockInfo.context->restore();
}

FloatRect RenderSVGText::strokeBoundingBox() const
{
    FloatRect strokeBoundaries = objectBoundingBox();
    const SVGRenderStyle* svgStyle = style()->svgStyle();
    if (!svgStyle->hasStroke())
        return strokeBoundaries;

    ASSERT(node());
    ASSERT(node()->isSVGElement());
    strokeBoundaries.inflate(svgStyle->strokeWidth().value(static_cast<SVGElement*>(node())));
    return strokeBoundaries;
}

FloatRect RenderSVGText::repaintRectInLocalCoordinates() const
{
    FloatRect repaintRect = strokeBoundingBox();
    SVGRenderSupport::intersectRepaintRectWithResources(this, repaintRect);

    if (const ShadowData* textShadow = style()->textShadow())
        textShadow->adjustRectForShadow(repaintRect);

    return repaintRect;
}

// Fix for <rdar://problem/8048875>. We should not render :first-line CSS Style
// in a SVG text element context.
RenderBlock* RenderSVGText::firstLineBlock() const
{
    return 0;
}

// Fix for <rdar://problem/8048875>. We should not render :first-letter CSS Style
// in a SVG text element context.
void RenderSVGText::updateFirstLetter()
{
}

}

#endif // ENABLE(SVG)
