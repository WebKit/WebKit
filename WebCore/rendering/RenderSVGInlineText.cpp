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
#include "SVGInlineTextBox.h"
#include "SVGRootInlineBox.h"

namespace WebCore {

RenderSVGInlineText::RenderSVGInlineText(Node* n, StringImpl* str) 
    : RenderText(n, str)
{
}

void RenderSVGInlineText::absoluteRects(Vector<IntRect>& rects, int, int, bool)
{
    InlineTextBox* firstBox = firstTextBox();

    SVGRootInlineBox* rootBox = firstBox ? static_cast<SVGInlineTextBox*>(firstBox)->svgRootInlineBox() : 0;
    RenderObject* object = rootBox ? rootBox->object() : 0;

    if (!object)
        return;

    int xRef = xPos() + object->xPos();
    int yRef = yPos() + object->yPos();

    for (InlineTextBox* curr = firstBox; curr; curr = curr->nextTextBox()) {
        FloatRect rect(xRef - curr->xPos(), yRef - curr->yPos(), curr->width(), curr->height());
        rects.append(enclosingIntRect(absoluteTransform().mapRect(rect)));
    }
}

IntRect RenderSVGInlineText::selectionRect(bool clipToVisibleContent)
{
    IntRect rect = RenderText::selectionRect(clipToVisibleContent);
    rect = parent()->absoluteTransform().mapRect(rect);
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
