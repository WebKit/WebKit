/*
 * Copyright (c) 2009, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(SVG)
#include "RenderSVGModelObject.h"

#include "GraphicsContext.h"
#include "RenderLayer.h"
#include "RenderView.h"
#include "SVGStyledElement.h"
#include "TransformState.h"

#if ENABLE(SVG_FILTERS)
#include "SVGResourceFilter.h"
#endif

namespace WebCore {

RenderSVGModelObject::RenderSVGModelObject(SVGStyledElement* node)
    : RenderObject(node)
{
}

IntRect RenderSVGModelObject::clippedOverflowRectForRepaint(RenderBoxModelObject* repaintContainer)
{
    // Return early for any cases where we don't actually paint
    if (style()->visibility() != VISIBLE && !enclosingLayer()->hasVisibleContent())
        return IntRect();

    // Pass our local paint rect to computeRectForRepaint() which will
    // map to parent coords and recurse up the parent chain.
    IntRect repaintRect = enclosingIntRect(repaintRectInLocalCoordinates());
    computeRectForRepaint(repaintContainer, repaintRect);
    return repaintRect;
}

void RenderSVGModelObject::computeRectForRepaint(RenderBoxModelObject* repaintContainer, IntRect& repaintRect, bool fixed)
{
    // Translate to coords in our parent renderer, and then call computeRectForRepaint on our parent
    repaintRect = localToParentTransform().mapRect(repaintRect);
    parent()->computeRectForRepaint(repaintContainer, repaintRect, fixed);
}

void RenderSVGModelObject::mapLocalToContainer(RenderBoxModelObject* repaintContainer, bool fixed , bool useTransforms, TransformState& transformState) const
{
    ASSERT(!fixed); // We should have no fixed content in the SVG rendering tree.

    // FIXME: If we don't respect useTransforms we break SVG text rendering.
    // Seems RenderSVGInlineText has some own broken translation hacks which depend useTransforms=false
    // This should instead be ASSERT(useTransforms) once we fix RenderSVGInlineText
    if (useTransforms)
        transformState.applyTransform(localToParentTransform());

    parent()->mapLocalToContainer(repaintContainer, fixed, useTransforms, transformState);
}

// Copied from RenderBox, this method likely requires further refactoring to work easily for both SVG and CSS Box Model content.
IntRect RenderSVGModelObject::outlineBoundsForRepaint(RenderBoxModelObject* repaintContainer) const
{
    IntRect box = enclosingIntRect(repaintRectInLocalCoordinates());
    adjustRectForOutlineAndShadow(box);

    FloatQuad containerRelativeQuad = localToContainerQuad(FloatRect(box), repaintContainer);
    return containerRelativeQuad.enclosingBoundingBox();
}

void RenderSVGModelObject::absoluteRects(Vector<IntRect>& rects, int, int, bool)
{
    rects.append(absoluteClippedOverflowRect());
}

void RenderSVGModelObject::absoluteQuads(Vector<FloatQuad>& quads, bool)
{
    quads.append(absoluteClippedOverflowRect());
}

bool RenderSVGModelObject::nodeAtPoint(const HitTestRequest&, HitTestResult&, int, int, int, int, HitTestAction)
{
    ASSERT_NOT_REACHED();
    return false;
}

} // namespace WebCore

#endif // ENABLE(SVG)
