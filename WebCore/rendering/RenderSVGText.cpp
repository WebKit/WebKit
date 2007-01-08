/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 *               2006 Alexander Kellett <lypanov@kde.org>
 *               2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#ifdef SVG_SUPPORT
#include "RenderSVGText.h"

#include "GraphicsContext.h"
#include "KCanvasRenderingStyle.h"
#include "PointerEventsHitRules.h"
#include "SVGLength.h"
#include "SVGLengthList.h"
#include "SVGTextElement.h"
#include "SVGRootInlineBox.h"

#include <wtf/OwnPtr.h>

namespace WebCore {

RenderSVGText::RenderSVGText(SVGTextElement* node) 
    : RenderSVGBlock(node)
{
}

void RenderSVGText::computeAbsoluteRepaintRect(IntRect& r, bool f)
{
    AffineTransform transform = localTransform();
    r = transform.mapRect(r);
    RenderContainer::computeAbsoluteRepaintRect(r, f);
    r = transform.inverse().mapRect(r);
}

bool RenderSVGText::requiresLayer()
{
    return false;
}

void RenderSVGText::layout()
{
    ASSERT(needsLayout());
    ASSERT(minMaxKnown());

    IntRect oldBounds;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (checkForRepaint)
        oldBounds = m_absoluteBounds;
    SVGTextElement* text = static_cast<SVGTextElement*>(element());
    //FIXME:  need to allow floating point positions
    int xOffset = (int)(text->x()->getFirst().value());
    int yOffset = (int)(text->y()->getFirst().value());
    setPos(xOffset, yOffset);
    RenderBlock::layout();
    
    m_absoluteBounds = getAbsoluteRepaintRect();

    bool repainted = false;
    if (checkForRepaint)
        repainted = repaintAfterLayoutIfNeeded(oldBounds, oldBounds);
    
    setNeedsLayout(false);
}

InlineBox* RenderSVGText::createInlineBox(bool makePlaceHolderBox, bool isRootLineBox, bool isOnlyRun)
{
    assert(!isInlineFlow());
    InlineFlowBox* flowBox = new (renderArena()) SVGRootInlineBox(this);
    
    if (!m_firstLineBox)
        m_firstLineBox = m_lastLineBox = flowBox;
    else {
        m_lastLineBox->setNextLineBox(flowBox);
        flowBox->setPreviousLineBox(m_lastLineBox);
        m_lastLineBox = flowBox;
    }
    
    return flowBox;
}

bool RenderSVGText::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int _x, int _y, int _tx, int _ty, HitTestAction hitTestAction)
{
    PointerEventsHitRules hitRules(PointerEventsHitRules::SVG_TEXT_HITTESTING, style()->svgStyle()->pointerEvents());
    bool isVisible = (style()->visibility() == VISIBLE);
    if (isVisible || !hitRules.requireVisible) {
        if ((hitRules.canHitStroke && (style()->svgStyle()->hasStroke() || !hitRules.requireStroke))
            || (hitRules.canHitFill && (style()->svgStyle()->hasFill() || !hitRules.requireFill))) {
            AffineTransform totalTransform = absoluteTransform();
            double localX, localY;
            totalTransform.inverse().map(_x, _y, &localX, &localY);
            FloatPoint hitPoint(_x, _y);
            return RenderBlock::nodeAtPoint(request, result, (int)localX, (int)localY, _tx, _ty, hitTestAction);
        }
    }
    return false;
}

void RenderSVGText::absoluteRects(Vector<IntRect>& rects, int tx, int ty)
{
    InlineBox* box = firstLineBox();
    if (box) {
        AffineTransform boxTransform = box->object()->absoluteTransform();
        FloatRect boundsRect = FloatRect(xPos() + box->xPos(), yPos() + box->yPos(), box->width(), box->height());
        rects.append(enclosingIntRect(boxTransform.mapRect(boundsRect)));
    }
}

void RenderSVGText::paint(PaintInfo& paintInfo, int tx, int ty)
{   
    RenderObject::PaintInfo pi(paintInfo);
    pi.rect = absoluteTransform().inverse().mapRect(pi.rect);
    RenderBlock::paint(pi, tx, ty);
}

FloatRect RenderSVGText::relativeBBox(bool includeStroke) const
{
    FloatRect boundsRect;
    InlineBox* box = firstLineBox();
    if (box) {
        boundsRect = FloatRect(xPos() + box->xPos(), yPos() + box->yPos(), box->width(), box->height());
        boundsRect = localTransform().mapRect(boundsRect);
    }
    return boundsRect;
}

}

#endif // SVG_SUPPORT
