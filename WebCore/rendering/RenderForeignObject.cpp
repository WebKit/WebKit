/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2009 Google, Inc.
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

#if ENABLE(SVG) && ENABLE(SVG_FOREIGN_OBJECT)
#include "RenderForeignObject.h"

#include "GraphicsContext.h"
#include "RenderView.h"
#include "SVGForeignObjectElement.h"
#include "SVGLength.h"
#include "SVGTransformList.h"

namespace WebCore {

RenderForeignObject::RenderForeignObject(SVGForeignObjectElement* node) 
    : RenderSVGBlock(node)
{
}

TransformationMatrix RenderForeignObject::translationForAttributes() const
{
    SVGForeignObjectElement* foreign = static_cast<SVGForeignObjectElement*>(node());
    return TransformationMatrix().translate(foreign->x().value(foreign), foreign->y().value(foreign));
}

void RenderForeignObject::paint(PaintInfo& paintInfo, int parentX, int parentY)
{
    // The SVG rendering tree should not be using parentX/parentY
    ASSERT(!parentX);
    ASSERT(!parentY);
    if (paintInfo.context->paintingDisabled())
        return;

    paintInfo.context->save();
    paintInfo.context->concatCTM(TransformationMatrix().translate(parentX, parentY));
    paintInfo.context->concatCTM(localToParentTransform());
    paintInfo.context->clip(clipRect(parentX, parentY));

    float opacity = style()->opacity();
    if (opacity < 1.0f)
        // FIXME: Possible optimization by clipping to bbox here, once relativeBBox is implemented & clip, mask and filter support added.
        paintInfo.context->beginTransparencyLayer(opacity);

    PaintInfo pi(paintInfo);
    pi.rect = absoluteTransform().inverse().mapRect(paintInfo.rect);
    RenderBlock::paint(pi, 0, 0);

    if (opacity < 1.0f)
        paintInfo.context->endTransparencyLayer();

    paintInfo.context->restore();
}

FloatRect RenderForeignObject::objectBoundingBox() const
{
    return borderBoxRect();
}

FloatRect RenderForeignObject::repaintRectInLocalCoordinates() const
{
    // HACK: to maintain historical LayoutTest results for now.
    // RenderForeignObject is a RenderBlock (not a RenderSVGModelObject) so this
    // should not affect repaint correctness.  But it should really be:
    // return borderBoxRect();
    return FloatRect();
}

void RenderForeignObject::computeRectForRepaint(RenderBoxModelObject* repaintContainer, IntRect& rect, bool fixed)
{
    rect = localToParentTransform().mapRect(rect);
    RenderBlock::computeRectForRepaint(repaintContainer, rect, fixed);
}

bool RenderForeignObject::calculateLocalTransform()
{
    TransformationMatrix oldTransform = m_localTransform;
    m_localTransform = static_cast<SVGForeignObjectElement*>(node())->animatedLocalTransform();
    return (oldTransform != m_localTransform);
}

TransformationMatrix RenderForeignObject::localToParentTransform() const
{
    // FIXME: This trasition is backwards!
    // It should be localTransform() * translationForAttributes()
    // but leaving it backwards for now for LayoutTest result compatibility
    // https://bugs.webkit.org/show_bug.cgi?id=25433
    return translationForAttributes() * localTransform();
}

void RenderForeignObject::layout()
{
    ASSERT(needsLayout());

    // Arbitrary affine transforms are incompatible with LayoutState.
    view()->disableLayoutState();

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());
    
    calculateLocalTransform();
    
    RenderBlock::layout();

    repainter.repaintAfterLayout();

    view()->enableLayoutState();
    setNeedsLayout(false);
}

bool RenderForeignObject::nodeAtFloatPoint(const HitTestRequest& request, HitTestResult& result, const FloatPoint& pointInParent, HitTestAction hitTestAction)
{
    FloatPoint localPoint = localToParentTransform().inverse().mapPoint(pointInParent);
    return RenderBlock::nodeAtPoint(request, result, static_cast<int>(localPoint.x()), static_cast<int>(localPoint.y()), 0, 0, hitTestAction);
}

bool RenderForeignObject::nodeAtPoint(const HitTestRequest&, HitTestResult&, int, int, int, int, HitTestAction)
{
    ASSERT_NOT_REACHED();
    return false;
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FOREIGN_OBJECT)
