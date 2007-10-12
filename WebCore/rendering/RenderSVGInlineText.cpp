/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 *           (C) 2006 Apple Computer Inc.
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "AffineTransform.h"
#include "GraphicsContext.h"
#include "RenderBlock.h"
#include "RenderSVGRoot.h"
#include "SVGInlineTextBox.h"
#include "SVGRootInlineBox.h"

namespace WebCore {

RenderSVGInlineText::RenderSVGInlineText(Node* n, StringImpl* str) 
    : RenderText(n, str)
{
}

void RenderSVGInlineText::absoluteRects(Vector<IntRect>& rects, int, int, bool)
{
    rects.append(computeAbsoluteRectForRange(0, textLength()));
}

IntRect RenderSVGInlineText::selectionRect(bool)
{
    ASSERT(!needsLayout());

    IntRect rect;
    if (selectionState() == SelectionNone)
        return rect;

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
        return rect;

    return computeAbsoluteRectForRange(startPos, endPos);
}

IntRect RenderSVGInlineText::computeAbsoluteRectForRange(int startPos, int endPos)
{
    IntRect rect;

    RenderBlock* cb = containingBlock();
    if (!cb)
        return rect;

    RenderSVGRoot* root = findSVGRootObject(parent());
    if (!root)
        return rect;

    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
        rect.unite(box->selectionRect(0, 0, startPos, endPos));

    // Mimic RenderBox::computeAbsoluteRepaintRect() functionality. But only the subset needed for SVG and respecting SVG transformations.
    int x, y;
    cb->absolutePosition(x, y);

    // Remove HTML parent translation offsets here! These need to be retrieved from the RenderSVGRoot object.
    AffineTransform htmlParentCtm = root->RenderContainer::absoluteTransform();
    FloatRect fixedRect = FloatRect(x - htmlParentCtm.e(), y - htmlParentCtm.f(), rect.width(), rect.height());
    rect = enclosingIntRect(absoluteTransform().mapRect(fixedRect));

    // Work around rounding issues which may occour above!
    rect.inflate(1);
    return rect;
}

InlineTextBox* RenderSVGInlineText::createInlineTextBox()
{
    return new (renderArena()) SVGInlineTextBox(this);
}

IntRect RenderSVGInlineText::caretRect(int offset, EAffinity affinity, int* extraWidthToEndOfLine)
{
    // SVG doesn't have any editable content where a caret rect would be needed
    return IntRect();
}

VisiblePosition RenderSVGInlineText::positionForCoordinates(int x, int y)
{
    SVGInlineTextBox* textBox = static_cast<SVGInlineTextBox*>(firstTextBox());

    if (!textBox || textLength() == 0)
        return VisiblePosition(element(), 0, DOWNSTREAM);

    SVGRootInlineBox* rootBox = textBox->svgRootInlineBox();
    RenderObject* object = rootBox ? rootBox->object() : 0;

    if (!object)
        return VisiblePosition(element(), 0, DOWNSTREAM);

    int offset = 0;
    textBox->svgCharacterHitsPosition(x + object->xPos(), y + object->yPos(), offset);
    return VisiblePosition(element(), offset, DOWNSTREAM);
}

}

#endif // ENABLE(SVG)
