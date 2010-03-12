/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006 Apple Inc. All rights reserved.
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
#include "RenderSVGInline.h"

#include "FloatQuad.h"
#include "RenderBlock.h"
#include "SVGInlineFlowBox.h"
#include "SVGInlineTextBox.h"
#include "SVGRootInlineBox.h"

namespace WebCore {
    
RenderSVGInline::RenderSVGInline(Node* n)
    : RenderInline(n)
{
}

InlineFlowBox* RenderSVGInline::createInlineFlowBox()
{
    InlineFlowBox* box = new (renderArena()) SVGInlineFlowBox(this);
    box->setHasVirtualHeight();
    return box;
}

void RenderSVGInline::absoluteRects(Vector<IntRect>& rects, int, int)
{
    InlineFlowBox* firstBox = firstLineBox();

    RootInlineBox* rootBox = firstBox ? firstBox->root() : 0;
    RenderBox* object = rootBox ? rootBox->block() : 0;

    if (!object)
        return;

    int xRef = object->x();
    int yRef = object->y();

    for (InlineFlowBox* curr = firstBox; curr; curr = curr->nextLineBox()) {
        FloatRect rect(xRef + curr->x(), yRef + curr->y(), curr->width(), curr->height());
        rects.append(enclosingIntRect(localToAbsoluteQuad(rect).boundingBox()));
    }
}

FloatRect RenderSVGInline::objectBoundingBox() const
{
    if (const RenderObject* object = findTextRootObject(this))
        return object->objectBoundingBox();

    return FloatRect();
}

FloatRect RenderSVGInline::strokeBoundingBox() const
{
    const RenderObject* object = findTextRootObject(this);
    ASSERT(object);

    const SVGRenderBase* renderer = object->toSVGRenderBase();
    if (!renderer)
        return FloatRect();

    return renderer->strokeBoundingBox();
}

FloatRect RenderSVGInline::repaintRectInLocalCoordinates() const
{
    if (const RenderObject* object = findTextRootObject(this))
        return object->repaintRectInLocalCoordinates();

    return FloatRect();
}

void RenderSVGInline::absoluteQuads(Vector<FloatQuad>& quads)
{
    InlineFlowBox* firstBox = firstLineBox();

    RootInlineBox* rootBox = firstBox ? firstBox->root() : 0;
    RenderBox* object = rootBox ? rootBox->block() : 0;

    if (!object)
        return;

    int xRef = object->x();
    int yRef = object->y();

    for (InlineFlowBox* curr = firstBox; curr; curr = curr->nextLineBox()) {
        FloatRect rect(xRef + curr->x(), yRef + curr->y(), curr->width(), curr->height());
        quads.append(localToAbsoluteQuad(rect));
    }
}

}

#endif // ENABLE(SVG)
