/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007, 2008, 2009 Rob Buis <buis@kde.org>
                  2007 Eric Seidel <eric@webkit.org>
                  2009 Google, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "RenderSVGRoot.h"

#include "GraphicsContext.h"
#include "RenderSVGContainer.h"
#include "RenderView.h"
#include "SVGLength.h"
#include "SVGRenderSupport.h"
#include "SVGSVGElement.h"
#include "SVGStyledElement.h"
#include "TransformState.h"

#if ENABLE(FILTERS)
#include "SVGResourceFilter.h"
#endif

using namespace std;

namespace WebCore {

RenderSVGRoot::RenderSVGRoot(SVGStyledElement* node)
    : RenderBox(node)
{
    setReplaced(true);
}

int RenderSVGRoot::lineHeight(bool, bool) const
{
    return height() + marginTop() + marginBottom();
}

int RenderSVGRoot::baselinePosition(bool, bool) const
{
    return height() + marginTop() + marginBottom();
}

void RenderSVGRoot::calcPrefWidths()
{
    ASSERT(prefWidthsDirty());

    int paddingAndBorders = paddingLeft() + paddingRight() + borderLeft() + borderRight();
    int width = calcReplacedWidth(false) + paddingAndBorders;

    if (style()->maxWidth().isFixed() && style()->maxWidth().value() != undefinedLength)
        width = min(width, style()->maxWidth().value() + (style()->boxSizing() == CONTENT_BOX ? paddingAndBorders : 0));

    if (style()->width().isPercent() || (style()->width().isAuto() && style()->height().isPercent())) {
        m_minPrefWidth = 0;
        m_maxPrefWidth = width;
    } else
        m_minPrefWidth = m_maxPrefWidth = width;

    setPrefWidthsDirty(false);
}

void RenderSVGRoot::layout()
{
    ASSERT(needsLayout());

    // Arbitrary affine transforms are incompatible with LayoutState.
    view()->disableLayoutState();

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout() && selfNeedsLayout());

    int oldWidth = width();
    calcWidth();

    int oldHeight = height();
    calcHeight();

    SVGSVGElement* svg = static_cast<SVGSVGElement*>(node());
    setWidth(static_cast<int>(width() * svg->currentScale()));
    setHeight(static_cast<int>(height() * svg->currentScale()));
    calcViewport();

    // RenderSVGRoot needs to take special care to propagate window size changes to the children,
    // if the outermost <svg> is using relative x/y/width/height values. Hence the additonal parameters.
    layoutChildren(this, selfNeedsLayout() || (svg->hasRelativeValues() && (width() != oldWidth || height() != oldHeight)));
    repainter.repaintAfterLayout();

    view()->enableLayoutState();
    setNeedsLayout(false);
}

bool RenderSVGRoot::selfWillPaint() const
{
#if ENABLE(FILTERS)
    const SVGRenderStyle* svgStyle = style()->svgStyle();
    SVGResourceFilter* filter = getFilterById(document(), svgStyle->filter(), this);
    if (filter)
        return true;
#endif
    return false;
}

void RenderSVGRoot::paint(PaintInfo& paintInfo, int parentX, int parentY)
{
    if (paintInfo.context->paintingDisabled())
        return;

    IntPoint parentOriginInContainer(parentX, parentY);
    IntPoint borderBoxOriginInContainer = parentOriginInContainer + IntSize(x(), y());

    if (hasBoxDecorations() && (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection)) 
        paintBoxDecorations(paintInfo, borderBoxOriginInContainer.x(), borderBoxOriginInContainer.y());

    // An empty viewport disables rendering.  FIXME: Should we still render filters?
    if (m_viewportSize.isEmpty())
        return;

    // Don't paint if we don't have kids, except if we have filters we should paint those.
    if (!firstChild() && !selfWillPaint())
        return;

    // Make a copy of the PaintInfo because applyTransformToPaintInfo will modify the damage rect.
    RenderObject::PaintInfo childPaintInfo(paintInfo);
    childPaintInfo.context->save();

    // Apply initial viewport clip - not affected by overflow handling
    childPaintInfo.context->clip(overflowClipRect(borderBoxOriginInContainer.x(), borderBoxOriginInContainer.y()));

    // Convert from container offsets (html renderers) to a relative transform (svg renderers).
    // Transform from our paint container's coordinate system to our local coords.
    applyTransformToPaintInfo(childPaintInfo, localToRepaintContainerTransform(parentOriginInContainer));

    SVGResourceFilter* filter = 0;
    FloatRect boundingBox = repaintRectInLocalCoordinates();

    bool continueRendering = true;
    if (childPaintInfo.phase == PaintPhaseForeground)
        continueRendering = prepareToRenderSVGContent(this, childPaintInfo, boundingBox, filter);

    if (continueRendering)
        RenderBox::paint(childPaintInfo, 0, 0);

    if (childPaintInfo.phase == PaintPhaseForeground)
        finishRenderSVGContent(this, childPaintInfo, filter, paintInfo.context);

    childPaintInfo.context->restore();

    if ((paintInfo.phase == PaintPhaseOutline || paintInfo.phase == PaintPhaseSelfOutline) && style()->outlineWidth() && style()->visibility() == VISIBLE)
        paintOutline(paintInfo.context, borderBoxOriginInContainer.x(), borderBoxOriginInContainer.y(), width(), height(), style());
}

void RenderSVGRoot::calcViewport()
{
    SVGSVGElement* svg = static_cast<SVGSVGElement*>(node());

    if (!selfNeedsLayout() && !svg->hasRelativeValues())
        return;

    if (!svg->hasSetContainerSize()) {
        // In the normal case of <svg> being stand-alone or in a CSSBoxModel object we use
        // RenderBox::width()/height() (which pulls data from RenderStyle)
        m_viewportSize = FloatSize(width(), height());
    } else {
        // In the SVGImage case grab the SVGLength values off of SVGSVGElement and use
        // the special relativeWidthValue accessors which respect the specified containerSize
        SVGLength width = svg->width();
        SVGLength height = svg->height();
        float viewportWidth = (width.unitType() == LengthTypePercentage) ? svg->relativeWidthValue() : width.value(svg);
        float viewportHeight = (height.unitType() == LengthTypePercentage) ? svg->relativeHeightValue() : height.value(svg);
        m_viewportSize = FloatSize(viewportWidth, viewportHeight);
    }
}

// RenderBox methods will expect coordinates w/o any transforms in coordinates
// relative to our borderBox origin.  This method gives us exactly that.
AffineTransform RenderSVGRoot::localToBorderBoxTransform() const
{
    IntSize borderAndPadding = borderOriginToContentBox();
    SVGSVGElement* svg = static_cast<SVGSVGElement*>(node());
    float scale = svg->currentScale();
    AffineTransform ctm(scale, 0, 0, scale, borderAndPadding.width(), borderAndPadding.height());
    ctm.translate(svg->currentTranslate().x(), svg->currentTranslate().y());
    return svg->viewBoxToViewTransform(width(), height()) * ctm;
}

IntSize RenderSVGRoot::parentOriginToBorderBox() const
{
    return IntSize(x(), y());
}

IntSize RenderSVGRoot::borderOriginToContentBox() const
{
    return IntSize(borderLeft() + paddingLeft(), borderTop() + paddingTop());
}

AffineTransform RenderSVGRoot::localToRepaintContainerTransform(const IntPoint& parentOriginInContainer) const
{
    AffineTransform parentToContainer(localToParentTransform());
    return parentToContainer.translateRight(parentOriginInContainer.x(), parentOriginInContainer.y());
}

const AffineTransform& RenderSVGRoot::localToParentTransform() const
{
    IntSize parentToBorderBoxOffset = parentOriginToBorderBox();

    AffineTransform borderBoxOriginToParentOrigin(localToBorderBoxTransform());
    borderBoxOriginToParentOrigin.translateRight(parentToBorderBoxOffset.width(), parentToBorderBoxOffset.height());

    m_localToParentTransform = borderBoxOriginToParentOrigin;
    return m_localToParentTransform;
}

FloatRect RenderSVGRoot::objectBoundingBox() const
{
    return computeContainerBoundingBox(this, false);
}

FloatRect RenderSVGRoot::repaintRectInLocalCoordinates() const
{
    // FIXME: This does not include the border but it should!
    FloatRect repaintRect = computeContainerBoundingBox(this, true);
    style()->svgStyle()->inflateForShadow(repaintRect);
    return repaintRect;
}

AffineTransform RenderSVGRoot::localTransform() const
{
    return AffineTransform();
}

void RenderSVGRoot::computeRectForRepaint(RenderBoxModelObject* repaintContainer, IntRect& repaintRect, bool fixed)
{
    // Apply our local transforms (except for x/y translation), then our shadow, 
    // and then call RenderBox's method to handle all the normal CSS Box model bits
    repaintRect = localToBorderBoxTransform().mapRect(repaintRect);

    // Apply initial viewport clip - not affected by overflow settings    
    repaintRect.intersect(enclosingIntRect(FloatRect(FloatPoint(), m_viewportSize)));

    style()->svgStyle()->inflateForShadow(repaintRect);
    RenderBox::computeRectForRepaint(repaintContainer, repaintRect, fixed);
}

void RenderSVGRoot::mapLocalToContainer(RenderBoxModelObject* repaintContainer, bool fixed , bool useTransforms, TransformState& transformState) const
{
    ASSERT(!fixed); // We should have no fixed content in the SVG rendering tree.
    ASSERT(useTransforms); // mapping a point through SVG w/o respecting trasnforms is useless.

    // Transform to our border box and let RenderBox transform the rest of the way.
    transformState.applyTransform(localToBorderBoxTransform());
    RenderBox::mapLocalToContainer(repaintContainer, fixed, useTransforms, transformState);
}

bool RenderSVGRoot::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int _x, int _y, int _tx, int _ty, HitTestAction hitTestAction)
{
    IntPoint pointInContainer(_x, _y);
    IntSize containerToParentOffset(_tx, _ty);

    IntPoint pointInParent = pointInContainer - containerToParentOffset;
    IntPoint pointInBorderBox = pointInParent - parentOriginToBorderBox();

    // Note: For now, we're ignoring hits to border and padding for <svg>
    IntPoint pointInContentBox = pointInBorderBox - borderOriginToContentBox();
    if (!contentBoxRect().contains(pointInContentBox))
        return false;

    IntPoint localPoint = localToParentTransform().inverse().mapPoint(pointInParent);

    for (RenderObject* child = lastChild(); child; child = child->previousSibling()) {
        if (child->nodeAtFloatPoint(request, result, localPoint, hitTestAction)) {
            // FIXME: CSS/HTML assumes the local point is relative to the border box, right?
            updateHitTestResult(result, pointInBorderBox);
            return true;
        }
    }

    // Spec: Only graphical elements can be targeted by the mouse, so we don't check self here.
    // 16.4: "If there are no graphics elements whose relevant graphics content is under the pointer (i.e., there is no target element), the event is not dispatched."
    return false;
}

}

#endif // ENABLE(SVG)
