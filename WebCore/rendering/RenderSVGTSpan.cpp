/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 *           (C) 2006 Apple Computer Inc.
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
#include "RenderSVGTSpan.h"

#include "FloatQuad.h"
#include "RenderBlock.h"
#include "SVGInlineTextBox.h"
#include "SVGRootInlineBox.h"

namespace WebCore {

RenderSVGTSpan::RenderSVGTSpan(Node* n)
    : RenderSVGInline(n)
{
}

void RenderSVGTSpan::absoluteRects(Vector<IntRect>& rects, int, int, bool)
{
    InlineRunBox* firstBox = firstLineBox();

    SVGRootInlineBox* rootBox = firstBox ? static_cast<SVGInlineTextBox*>(firstBox)->svgRootInlineBox() : 0;
    RenderBox* object = rootBox ? rootBox->block() : 0;

    if (!object)
        return;

    int xRef = object->x() + x();
    int yRef = object->y() + y();
 
    for (InlineRunBox* curr = firstBox; curr; curr = curr->nextLineBox()) {
        FloatRect rect(xRef + curr->xPos(), yRef + curr->yPos(), curr->width(), curr->height());
        // FIXME: broken with CSS transforms
        rects.append(enclosingIntRect(absoluteTransform().mapRect(rect)));
    }
}

void RenderSVGTSpan::absoluteQuads(Vector<FloatQuad>& quads, bool)
{
    InlineRunBox* firstBox = firstLineBox();

    SVGRootInlineBox* rootBox = firstBox ? static_cast<SVGInlineTextBox*>(firstBox)->svgRootInlineBox() : 0;
    RenderBox* object = rootBox ? rootBox->block() : 0;

    if (!object)
        return;

    int xRef = object->x() + x();
    int yRef = object->y() + y();
 
    for (InlineRunBox* curr = firstBox; curr; curr = curr->nextLineBox()) {
        FloatRect rect(xRef + curr->xPos(), yRef + curr->yPos(), curr->width(), curr->height());
        // FIXME: broken with CSS transforms
        quads.append(absoluteTransform().mapRect(rect));
    }
}

}

#endif // ENABLE(SVG)
