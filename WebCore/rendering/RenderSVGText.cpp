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
#include "PointerEventsHitRules.h"
#include "RenderLayer.h"
#include "RenderSVGRoot.h"
#include "SVGLengthList.h"
#include "SVGRenderSupport.h"
#include "SVGRootInlineBox.h"
#include "SVGTextElement.h"
#include "SVGTransformList.h"
#include "SVGURIReference.h"
#include "SimpleFontData.h"

namespace WebCore {

RenderSVGText::RenderSVGText(SVGTextElement* node) 
    : RenderSVGBlock(node)
    , m_needsTransformUpdate(true)
{
}

IntRect RenderSVGText::clippedOverflowRectForRepaint(RenderBoxModelObject* repaintContainer)
{
    return SVGRenderBase::clippedOverflowRectForRepaint(this, repaintContainer);
}

void RenderSVGText::computeRectForRepaint(RenderBoxModelObject* repaintContainer, IntRect& repaintRect, bool fixed)
{
    SVGRenderBase::computeRectForRepaint(this, repaintContainer, repaintRect, fixed);
}

void RenderSVGText::mapLocalToContainer(RenderBoxModelObject* repaintContainer, bool fixed , bool useTransforms, TransformState& transformState) const
{
    SVGRenderBase::mapLocalToContainer(this, repaintContainer, fixed, useTransforms, transformState);
}

void RenderSVGText::layout()
{
    ASSERT(needsLayout());
    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());

    // Best guess for a relative starting point
    SVGTextElement* text = static_cast<SVGTextElement*>(node());
    int xOffset = (int)(text->x()->getFirst().value(text));
    int yOffset = (int)(text->y()->getFirst().value(text));
    setLocation(xOffset, yOffset);

    if (m_needsTransformUpdate) {
        m_localTransform = text->animatedLocalTransform();
        m_needsTransformUpdate = false;
    }

    RenderBlock::layout();

    repainter.repaintAfterLayout();
    setNeedsLayout(false);
}

RootInlineBox* RenderSVGText::createRootInlineBox() 
{
    RootInlineBox* box = new (renderArena()) SVGRootInlineBox(this);
    box->setHasVirtualHeight();
    return box;
}

bool RenderSVGText::nodeAtFloatPoint(const HitTestRequest& request, HitTestResult& result, const FloatPoint& pointInParent, HitTestAction hitTestAction)
{
    PointerEventsHitRules hitRules(PointerEventsHitRules::SVG_TEXT_HITTESTING, style()->pointerEvents());
    bool isVisible = (style()->visibility() == VISIBLE);
    if (isVisible || !hitRules.requireVisible) {
        if ((hitRules.canHitStroke && (style()->svgStyle()->hasStroke() || !hitRules.requireStroke))
            || (hitRules.canHitFill && (style()->svgStyle()->hasFill() || !hitRules.requireFill))) {
            FloatPoint localPoint = localToParentTransform().inverse().mapPoint(pointInParent);
            return RenderBlock::nodeAtPoint(request, result, (int)localPoint.x(), (int)localPoint.y(), 0, 0, hitTestAction);
        }
    }

    return false;
}

void RenderSVGText::destroy()
{
    deregisterFromResources(this);
    RenderSVGBlock::destroy();
}

bool RenderSVGText::nodeAtPoint(const HitTestRequest&, HitTestResult&, int, int, int, int, HitTestAction)
{
    ASSERT_NOT_REACHED();
    return false;
}

void RenderSVGText::absoluteRects(Vector<IntRect>& rects, int, int)
{
    RenderSVGRoot* root = findSVGRootObject(parent());
    if (!root)
        return;
 
    // Don't use objectBoundingBox here, as it's unites the selection rects. Makes it hard
    // to spot errors, if there are any using WebInspector. Individually feed them into 'rects'.
    for (InlineFlowBox* flow = firstLineBox(); flow; flow = flow->nextLineBox()) {
        for (InlineBox* box = flow->firstChild(); box; box = box->nextOnLine()) {
            FloatRect boxRect(box->x(), box->y(), box->width(), box->height());
            // FIXME: crawling up the parent chain to map each rect is very inefficient
            // we should compute the absoluteTransform outside this loop first.
            rects.append(enclosingIntRect(localToAbsoluteQuad(boxRect).boundingBox()));
        }
    }
}

void RenderSVGText::absoluteQuads(Vector<FloatQuad>& quads)
{
    RenderSVGRoot* root = findSVGRootObject(parent());
    if (!root)
        return;
 
    // Don't use objectBoundingBox here, as it's unites the selection rects. Makes it hard
    // to spot errors, if there are any using WebInspector. Individually feed them into 'rects'.
    for (InlineFlowBox* flow = firstLineBox(); flow; flow = flow->nextLineBox()) {
        for (InlineBox* box = flow->firstChild(); box; box = box->nextOnLine()) {
            FloatRect boxRect(box->x(), box->y(), box->width(), box->height());
            // FIXME: crawling up the parent chain to map each quad is very inefficient
            // we should compute the absoluteTransform outside this loop first.
            quads.append(localToAbsoluteQuad(boxRect));
        }
    }
}

void RenderSVGText::paint(PaintInfo& paintInfo, int, int)
{
    if (paintInfo.context->paintingDisabled())
        return;

    PaintInfo pi(paintInfo);
    pi.context->save();
    applyTransformToPaintInfo(pi, localToParentTransform());
    RenderBlock::paint(pi, 0, 0);
    pi.context->restore();
}

FloatRect RenderSVGText::objectBoundingBox() const
{
    FloatRect boundingBox;

    for (InlineFlowBox* flow = firstLineBox(); flow; flow = flow->nextLineBox()) {
        for (InlineBox* box = flow->firstChild(); box; box = box->nextOnLine())
            boundingBox.unite(FloatRect(box->x(), box->y(), box->width(), box->height()));
    }

    boundingBox.move(x(), y());
    return boundingBox;
}

FloatRect RenderSVGText::strokeBoundingBox() const
{
    FloatRect strokeRect = objectBoundingBox();

    // SVG needs to include the strokeWidth(), not the textStrokeWidth().
    if (style()->svgStyle()->hasStroke()) {
        float strokeWidth = SVGRenderStyle::cssPrimitiveToLength(this, style()->svgStyle()->strokeWidth(), 1.0f);

#if ENABLE(SVG_FONTS)
        const Font& font = style()->font();
        if (font.primaryFont()->isSVGFont()) {
            float scale = font.unitsPerEm() > 0 ? font.size() / font.unitsPerEm() : 0.0f;

            if (scale != 0.0f)
                strokeWidth /= scale;
        }
#endif

        strokeRect.inflate(strokeWidth);
    }

    return strokeRect;
}

FloatRect RenderSVGText::repaintRectInLocalCoordinates() const
{
    FloatRect repaintRect = strokeBoundingBox();
    intersectRepaintRectWithResources(this, repaintRect);

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

// vim:ts=4:noet
