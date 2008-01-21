/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 *               2006 Alexander Kellett <lypanov@kde.org>
 *               2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 *               2007 Nikolas Zimmermann <zimmermann@kde.org>
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
#include "GraphicsContext.h"
#include "PointerEventsHitRules.h"
#include "RenderSVGRoot.h"
#include "SimpleFontData.h"
#include "SVGLengthList.h"
#include "SVGResourceFilter.h"
#include "SVGRootInlineBox.h"
#include "SVGTextElement.h"
#include "SVGTransformList.h"
#include "SVGURIReference.h"

namespace WebCore {

RenderSVGText::RenderSVGText(SVGTextElement* node) 
    : RenderSVGBlock(node)
{
}

IntRect RenderSVGText::absoluteClippedOverflowRect()
{
    FloatRect repaintRect = absoluteTransform().mapRect(relativeBBox(true));

#if ENABLE(SVG_FILTERS)
    // Filters can expand the bounding box
    SVGResourceFilter* filter = getFilterById(document(), SVGURIReference::getTarget(style()->svgStyle()->filter()));
    if (filter)
        repaintRect.unite(filter->filterBBoxForItemBBox(repaintRect));
#endif

    if (!repaintRect.isEmpty())
        repaintRect.inflate(1); // inflate 1 pixel for antialiasing

    return enclosingIntRect(repaintRect);
}

bool RenderSVGText::requiresLayer()
{
    return false;
}

bool RenderSVGText::calculateLocalTransform()
{
    AffineTransform oldTransform = m_localTransform;
    m_localTransform = static_cast<SVGTextElement*>(element())->animatedLocalTransform();
    return (oldTransform != m_localTransform);
}

void RenderSVGText::layout()
{
    ASSERT(needsLayout());
    
    // FIXME: This is a hack to avoid the RenderBlock::layout() partial repainting code which is not (yet) SVG aware
    setNeedsLayout(true);

    IntRect oldBounds;
    IntRect oldOutlineBox;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (checkForRepaint) {
        oldBounds = m_absoluteBounds;
        oldOutlineBox = absoluteOutlineBox();
    }

    // Best guess for a relative starting point
    SVGTextElement* text = static_cast<SVGTextElement*>(element());
    int xOffset = (int)(text->x()->getFirst().value());
    int yOffset = (int)(text->y()->getFirst().value());
    setPos(xOffset, yOffset);
    
    calculateLocalTransform();

    RenderBlock::layout();

    m_absoluteBounds = absoluteClippedOverflowRect();

    bool repainted = false;
    if (checkForRepaint)
        repainted = repaintAfterLayoutIfNeeded(oldBounds, oldOutlineBox);
    
    setNeedsLayout(false);
}

InlineBox* RenderSVGText::createInlineBox(bool makePlaceHolderBox, bool isRootLineBox, bool isOnlyRun)
{
    ASSERT(!isInlineFlow());
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

void RenderSVGText::absoluteRects(Vector<IntRect>& rects, int, int, bool)
{
    RenderSVGRoot* root = findSVGRootObject(parent());
    if (!root)
        return;

    int x, y;
    absolutePosition(x, y);

    AffineTransform htmlParentCtm = root->RenderContainer::absoluteTransform();
 
    // Don't use relativeBBox here, as it's unites the selection rects. Makes it hard
    // to spot errors, if there are any using WebInspector. Individually feed them into 'rects'.
    for (InlineRunBox* runBox = firstLineBox(); runBox; runBox = runBox->nextLineBox()) {
        ASSERT(runBox->isInlineFlowBox());

        InlineFlowBox* flowBox = static_cast<InlineFlowBox*>(runBox);
        for (InlineBox* box = flowBox->firstChild(); box; box = box->nextOnLine()) {
            FloatRect boxRect(box->xPos(), box->yPos(), box->width(), box->height());
            boxRect.move(narrowPrecisionToFloat(x - htmlParentCtm.e()), narrowPrecisionToFloat(y - htmlParentCtm.f()));
            rects.append(enclosingIntRect(absoluteTransform().mapRect(boxRect)));
        }
    }
}

void RenderSVGText::paint(PaintInfo& paintInfo, int, int)
{   
    RenderObject::PaintInfo pi(paintInfo);
    pi.rect = absoluteTransform().inverse().mapRect(pi.rect);
    RenderBlock::paint(pi, 0, 0);
}

FloatRect RenderSVGText::relativeBBox(bool includeStroke) const
{
    FloatRect repaintRect;

    for (InlineRunBox* runBox = firstLineBox(); runBox; runBox = runBox->nextLineBox()) {
        ASSERT(runBox->isInlineFlowBox());

        InlineFlowBox* flowBox = static_cast<InlineFlowBox*>(runBox);
        for (InlineBox* box = flowBox->firstChild(); box; box = box->nextOnLine())
            repaintRect.unite(FloatRect(box->xPos(), box->yPos(), box->width(), box->height()));
    }

    // SVG needs to include the strokeWidth(), not the textStrokeWidth().
    if (includeStroke && style()->svgStyle()->hasStroke()) {
        float strokeWidth = SVGRenderStyle::cssPrimitiveToLength(this, style()->svgStyle()->strokeWidth(), 0.0f);

#if ENABLE(SVG_FONTS)
        const Font& font = style()->font();
        if (font.primaryFont()->isSVGFont()) {
            float scale = font.unitsPerEm() > 0 ? font.size() / font.unitsPerEm() : 0.0f;

            if (scale != 0.0f)
                strokeWidth /= scale;
        }
#endif

        repaintRect.inflate(strokeWidth);
    }

    repaintRect.move(xPos(), yPos());
    return repaintRect;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
