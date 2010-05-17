/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 *           (C) 2006 Apple Computer Inc.
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 *           (C) 2008 Rob Buis <buis@kde.org>
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
#include "RenderSVGInlineText.h"

#include "FloatConversion.h"
#include "FloatQuad.h"
#include "RenderBlock.h"
#include "RenderSVGRoot.h"
#include "SVGInlineTextBox.h"
#include "SVGRootInlineBox.h"
#include "VisiblePosition.h"

namespace WebCore {

static inline bool isChildOfHiddenContainer(RenderObject* start)
{
    while (start) {
        if (start->isSVGHiddenContainer())
            return true;

        start = start->parent();
    }

    return false;
}

RenderSVGInlineText::RenderSVGInlineText(Node* n, PassRefPtr<StringImpl> str) 
    : RenderText(n, str)
{
}


void RenderSVGInlineText::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    // Skip RenderText's possible layout scheduling on style change
    RenderObject::styleDidChange(diff, oldStyle);
    
    // FIXME: SVG text is apparently always transformed?
    if (RefPtr<StringImpl> textToTransform = originalText())
        setText(textToTransform.release(), true);
}

void RenderSVGInlineText::absoluteRects(Vector<IntRect>& rects, int, int)
{
    rects.append(computeRepaintRectForRange(0, 0, textLength()));
}

void RenderSVGInlineText::absoluteQuads(Vector<FloatQuad>& quads)
{
    quads.append(computeRepaintQuadForRange(0, 0, textLength()));
}

IntRect RenderSVGInlineText::selectionRectForRepaint(RenderBoxModelObject* repaintContainer, bool /*clipToVisibleContent*/)
{
    ASSERT(!needsLayout());

    if (selectionState() == SelectionNone)
        return IntRect();

    // Early exit if we're ie. a <text> within a <defs> section.
    if (isChildOfHiddenContainer(this))
        return IntRect();

    // Now calculate startPos and endPos for painting selection.
    // We include a selection while endPos > 0
    int startPos, endPos;
    if (selectionState() == SelectionInside) {
        // We are fully selected.
        startPos = 0;
        endPos = textLength();
    } else {
        selectionStartEnd(startPos, endPos);
        if (selectionState() == SelectionStart)
            endPos = textLength();
        else if (selectionState() == SelectionEnd)
            startPos = 0;
    }

    if (startPos == endPos)
        return IntRect();

    return computeRepaintRectForRange(repaintContainer, startPos, endPos);
}

IntRect RenderSVGInlineText::computeRepaintRectForRange(RenderBoxModelObject* repaintContainer, int startPos, int endPos)
{
    FloatQuad repaintQuad = computeRepaintQuadForRange(repaintContainer, startPos, endPos);
    return enclosingIntRect(repaintQuad.boundingBox());
}

FloatQuad RenderSVGInlineText::computeRepaintQuadForRange(RenderBoxModelObject* repaintContainer, int startPos, int endPos)
{
    RenderBlock* cb = containingBlock();
    if (!cb || !cb->container())
        return FloatQuad();

    RenderSVGRoot* root = findSVGRootObject(parent());
    if (!root)
        return FloatQuad();

    IntRect rect;
    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
        rect.unite(box->selectionRect(0, 0, startPos, endPos));

    return localToContainerQuad(FloatQuad(rect), repaintContainer);
}

InlineTextBox* RenderSVGInlineText::createTextBox()
{
    InlineTextBox* box = new (renderArena()) SVGInlineTextBox(this);
    box->setHasVirtualHeight();
    return box;
}

IntRect RenderSVGInlineText::localCaretRect(InlineBox*, int, int*)
{
    // SVG doesn't have any editable content where a caret rect would be needed.
    // FIXME: That's not sufficient. The localCaretRect function is also used for selection.
    return IntRect();
}

VisiblePosition RenderSVGInlineText::positionForPoint(const IntPoint& point)
{
    SVGInlineTextBox* textBox = static_cast<SVGInlineTextBox*>(firstTextBox());

    if (!textBox || textLength() == 0)
        return createVisiblePosition(0, DOWNSTREAM);

    SVGRootInlineBox* rootBox = textBox->svgRootInlineBox();
    RenderBlock* object = rootBox ? rootBox->block() : 0;

    if (!object)
        return createVisiblePosition(0, DOWNSTREAM);

    int closestOffsetInBox = 0;

    // FIXME: This approach is wrong.  The correct code would first find the
    // closest SVGInlineTextBox to the point, and *then* ask only that inline box
    // what the closest text offset to that point is.  This code instead walks
    // through all boxes in order, so when you click "near" a box, you'll actually
    // end up returning the nearest offset in the last box, even if the
    // nearest offset to your click is contained in another box.
    for (SVGInlineTextBox* box = textBox; box; box = static_cast<SVGInlineTextBox*>(box->nextTextBox())) {
        if (box->svgCharacterHitsPosition(point.x() + object->x(), point.y() + object->y(), closestOffsetInBox)) {
            // If we're not at the end/start of the box, stop looking for other selected boxes.
            if (box->direction() == LTR) {
                if (closestOffsetInBox <= (int) box->end() + 1)
                    break;
            } else {
                if (closestOffsetInBox > (int) box->start())
                    break;
            }
        }
    }

    return createVisiblePosition(closestOffsetInBox, DOWNSTREAM);
}

void RenderSVGInlineText::destroy()
{
    if (!documentBeingDestroyed()) {
        setNeedsLayoutAndPrefWidthsRecalc();
        repaint();
    }
    RenderText::destroy();
}

}

#endif // ENABLE(SVG)
