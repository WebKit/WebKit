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

#include "SVGInlineFlowBox.h"

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

FloatRect RenderSVGInline::objectBoundingBox() const
{
    if (const RenderObject* object = findTextRootObject(this))
        return object->objectBoundingBox();

    return FloatRect();
}

FloatRect RenderSVGInline::strokeBoundingBox() const
{
    if (const RenderObject* object = findTextRootObject(this))
        return object->strokeBoundingBox();

    return FloatRect();
}

FloatRect RenderSVGInline::repaintRectInLocalCoordinates() const
{
    if (const RenderObject* object = findTextRootObject(this))
        return object->repaintRectInLocalCoordinates();

    return FloatRect();
}

IntRect RenderSVGInline::clippedOverflowRectForRepaint(RenderBoxModelObject* repaintContainer)
{
    return SVGRenderBase::clippedOverflowRectForRepaint(this, repaintContainer);
}

void RenderSVGInline::computeRectForRepaint(RenderBoxModelObject* repaintContainer, IntRect& repaintRect, bool fixed)
{
    SVGRenderBase::computeRectForRepaint(this, repaintContainer, repaintRect, fixed);
}

void RenderSVGInline::mapLocalToContainer(RenderBoxModelObject* repaintContainer, bool useTransforms, bool fixed, TransformState& transformState) const
{
    SVGRenderBase::mapLocalToContainer(this, repaintContainer, useTransforms, fixed, transformState);
}

}

#endif // ENABLE(SVG)
