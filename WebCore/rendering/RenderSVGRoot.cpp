/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007 Rob Buis <buis@kde.org>
                  2007 Eric Seidel <eric@webkit.org>

    This file is part of the KDE project

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "RenderSVGRoot.h"

#include "GraphicsContext.h"
#include "RenderView.h"
#include "SVGLength.h"
#include "SVGRenderSupport.h"
#include "SVGResourceClipper.h"
#include "SVGResourceFilter.h"
#include "SVGResourceMasker.h"
#include "SVGSVGElement.h"
#include "SVGStyledElement.h"
#include "SVGURIReference.h"

namespace WebCore {

RenderSVGRoot::RenderSVGRoot(SVGStyledElement* node)
    : RenderContainer(node)
    , m_slice(false)
{
    setReplaced(true);
}

RenderSVGRoot::~RenderSVGRoot()
{
}

AffineTransform RenderSVGRoot::localTransform() const
{
    return m_matrix;
}

void RenderSVGRoot::setLocalTransform(const AffineTransform& matrix)
{
    m_matrix = matrix;
}

bool RenderSVGRoot::requiresLayer()
{
    // Only allow an <svg> element to generate a layer when it's positioned in a non-SVG context
    return false;
}

short RenderSVGRoot::lineHeight(bool b, bool isRootLineBox) const
{
    return height() + marginTop() + marginBottom();
}

short RenderSVGRoot::baselinePosition(bool b, bool isRootLineBox) const
{
    return height() + marginTop() + marginBottom();
}

void RenderSVGRoot::layout()
{
    ASSERT(needsLayout());

    calcViewport();

    // Arbitrary affine transforms are incompatible with LayoutState.
    view()->disableLayoutState();

    IntRect oldBounds;
    IntRect oldOutlineBox;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (selfNeedsLayout() && checkForRepaint) {
        oldBounds = m_absoluteBounds;
        oldOutlineBox = absoluteOutlineBox();
    }

    RenderObject* child = firstChild();
    while (child) {
        // FIXME: This check is bogus, see http://bugs.webkit.org/show_bug.cgi?id=14003
        if (!child->isRenderPath() || static_cast<RenderPath*>(child)->hasRelativeValues())
            child->setNeedsLayout(true);

        child->layoutIfNeeded();
        ASSERT(!child->needsLayout());
        child = child->nextSibling();
    }

    calcWidth();
    calcHeight();

    m_absoluteBounds = absoluteClippedOverflowRect();
    SVGSVGElement* svg = static_cast<SVGSVGElement*>(element());
    m_width = m_width * svg->currentScale();
    m_height = m_height * svg->currentScale();

    if (selfNeedsLayout() && checkForRepaint)
        repaintAfterLayoutIfNeeded(oldBounds, oldOutlineBox);

    view()->enableLayoutState();
    setNeedsLayout(false);
}

void RenderSVGRoot::applyContentTransforms(PaintInfo& paintInfo, int& parentX, int& parentY)
{
    // Translate from parent offsets (html renderers) to a relative transform (svg renderers)
    IntPoint origin;
    origin.move(parentX, parentY);
    origin.move(m_x, m_y);
    origin.move(borderLeft(), borderTop());
    origin.move(paddingLeft(), paddingTop());
    if (origin.x() || origin.y()) {
        paintInfo.context->concatCTM(AffineTransform().translate(origin.x(), origin.y()));
        paintInfo.rect.move(-origin.x(), -origin.y());
    }
    parentX = parentY = 0;
    SVGSVGElement* svg = static_cast<SVGSVGElement*>(element());
    paintInfo.context->concatCTM(AffineTransform().scale(svg->currentScale()));
    
    if (!viewport().isEmpty()) {
        if (style()->overflowX() != OVISIBLE)
            paintInfo.context->clip(enclosingIntRect(viewport())); // FIXME: Eventually we'll want float-precision clipping
        
        paintInfo.context->concatCTM(AffineTransform().translate(viewport().x(), viewport().y()));
    }
    if (!localTransform().isIdentity())
        paintInfo.context->concatCTM(localTransform());
    paintInfo.context->concatCTM(AffineTransform().translate(svg->currentTranslate().x(), svg->currentTranslate().y()));
}

void RenderSVGRoot::paint(PaintInfo& paintInfo, int parentX, int parentY)
{
    if (paintInfo.context->paintingDisabled())
        return;

    // A value of zero disables rendering of the element. 
    if (viewport().width() <= 0. || viewport().height() <= 0.)
        return;

    // This should only exist for <svg> renderers
    if (hasBoxDecorations() && (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection)) 
        paintBoxDecorations(paintInfo, m_x + parentX, m_y + parentY);

    if (!firstChild()) {
#if ENABLE(SVG_EXPERIMENTAL_FEATURES)
        // Spec: groups w/o children still may render filter content.
        const SVGRenderStyle* svgStyle = style()->svgStyle();
        AtomicString filterId(SVGURIReference::getTarget(svgStyle->filter()));
        SVGResourceFilter* filter = getFilterById(document(), filterId);
        if (!filter)
#endif
            return;
    }
    
    paintInfo.context->save();
    
    applyContentTransforms(paintInfo, parentX, parentY);

#if ENABLE(SVG_EXPERIMENTAL_FEATURES)
    SVGResourceFilter* filter = 0;
#else
    void* filter = 0;
#endif
    FloatRect boundingBox = relativeBBox(true);
    float opacity = style()->opacity();
    if (paintInfo.phase == PaintPhaseForeground) {
        prepareToRenderSVGContent(this, paintInfo, boundingBox, filter);
        
        if (opacity < 1.0f) {
            paintInfo.context->clip(enclosingIntRect(boundingBox));
            paintInfo.context->beginTransparencyLayer(opacity);
        }
    }

    if (!viewBox().isEmpty())
        paintInfo.context->concatCTM(viewportTransform());

    RenderContainer::paint(paintInfo, 0, 0);

    if (paintInfo.phase == PaintPhaseForeground) {
#if ENABLE(SVG_EXPERIMENTAL_FEATURES)
        if (filter)
            filter->applyFilter(paintInfo.context, boundingBox);
#endif

        if (opacity < 1.0f)
            paintInfo.context->endTransparencyLayer();
    }

    paintInfo.context->restore();
    
    if ((paintInfo.phase == PaintPhaseOutline || paintInfo.phase == PaintPhaseSelfOutline) && style()->outlineWidth() && style()->visibility() == VISIBLE)
        paintOutline(paintInfo.context, m_absoluteBounds.x(), m_absoluteBounds.y(), m_absoluteBounds.width(), m_absoluteBounds.height(), style());
}

FloatRect RenderSVGRoot::viewport() const
{
    return m_viewport;
}

void RenderSVGRoot::calcViewport()
{
    SVGElement* svgelem = static_cast<SVGElement*>(element());
    if (svgelem->hasTagName(SVGNames::svgTag)) {
        SVGSVGElement* svg = static_cast<SVGSVGElement*>(element());

        if (!selfNeedsLayout() && !svg->hasRelativeValues())
            return;

        double x = svg->x().value();
        double y = svg->y().value();
        double w = svg->width().value();
        double h = svg->height().value();
        m_viewport = FloatRect(x, y, w, h);
    }
}

void RenderSVGRoot::setViewBox(const FloatRect& viewBox)
{
    m_viewBox = viewBox;

    if (style())
        setNeedsLayout(true);
}

FloatRect RenderSVGRoot::viewBox() const
{
    return m_viewBox;
}

void RenderSVGRoot::setAlign(SVGPreserveAspectRatio::SVGPreserveAspectRatioType align)
{
    m_align = align;
    if (style())
        setNeedsLayout(true);
}

SVGPreserveAspectRatio::SVGPreserveAspectRatioType RenderSVGRoot::align() const
{
    return m_align;
}

AffineTransform RenderSVGRoot::viewportTransform() const
{
    // FIXME: The method name is confusing, since it does not
    // do viewport translating anymore. Look into this while
    //  fixing bug 12207.
    if (!viewBox().isEmpty()) {
        FloatRect viewportRect = viewport();
        viewportRect = FloatRect(viewport().x(), viewport().y(), width(), height());

        return getAspectRatio(viewBox(), viewportRect);
    }

    return AffineTransform();
}

IntRect RenderSVGRoot::absoluteClippedOverflowRect()
{
    IntRect repaintRect;

    for (RenderObject* current = firstChild(); current != 0; current = current->nextSibling())
        repaintRect.unite(current->absoluteClippedOverflowRect());

#if ENABLE(SVG_EXPERIMENTAL_FEATURES)
    // Filters can expand the bounding box
    SVGResourceFilter* filter = getFilterById(document(), SVGURIReference::getTarget(style()->svgStyle()->filter()));
    if (filter)
        repaintRect.unite(enclosingIntRect(filter->filterBBoxForItemBBox(repaintRect)));
#endif

    return repaintRect;
}

void RenderSVGRoot::addFocusRingRects(GraphicsContext* graphicsContext, int tx, int ty)
{
    graphicsContext->addFocusRingRect(m_absoluteBounds);
}

void RenderSVGRoot::absoluteRects(Vector<IntRect>& rects, int, int)
{
    rects.append(absoluteClippedOverflowRect());
}

AffineTransform RenderSVGRoot::absoluteTransform() const
{
    AffineTransform ctm = RenderContainer::absoluteTransform();
    ctm.translate(m_x, m_y);
    SVGSVGElement* svg = static_cast<SVGSVGElement*>(element());
    ctm.scale(svg->currentScale());
    ctm.translate(svg->currentTranslate().x(), svg->currentTranslate().y());
    ctm.translate(viewport().x(), viewport().y());
    return viewportTransform() * ctm;
}

bool RenderSVGRoot::fillContains(const FloatPoint& p) const
{
    RenderObject* current = firstChild();
    while (current != 0) {
        if (current->isRenderPath() && static_cast<RenderPath*>(current)->fillContains(p))
            return true;

        current = current->nextSibling();
    }

    return false;
}

bool RenderSVGRoot::strokeContains(const FloatPoint& p) const
{
    RenderObject* current = firstChild();
    while (current != 0) {
        if (current->isRenderPath() && static_cast<RenderPath*>(current)->strokeContains(p))
            return true;

        current = current->nextSibling();
    }

    return false;
}

FloatRect RenderSVGRoot::relativeBBox(bool includeStroke) const
{
    FloatRect rect;
    
    RenderObject* current = firstChild();
    for (; current != 0; current = current->nextSibling()) {
        FloatRect childBBox = current->relativeBBox(includeStroke);
        FloatRect mappedBBox = current->localTransform().mapRect(childBBox);
        rect.unite(mappedBBox);
    }

    return rect;
}

void RenderSVGRoot::setSlice(bool slice)
{
    m_slice = slice;

    if (style())
        setNeedsLayout(true);
}

bool RenderSVGRoot::slice() const
{
    return m_slice;
}

AffineTransform RenderSVGRoot::getAspectRatio(const FloatRect& logical, const FloatRect& physical) const
{
    AffineTransform temp;

    float logicX = logical.x();
    float logicY = logical.y();
    float logicWidth = logical.width();
    float logicHeight = logical.height();
    float physWidth = physical.width();
    float physHeight = physical.height();

    float vpar = logicWidth / logicHeight;
    float svgar = physWidth / physHeight;

    if (align() == SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE) {
        temp.scale(physWidth / logicWidth, physHeight / logicHeight);
        temp.translate(-logicX, -logicY);
    } else if ((vpar < svgar && !slice()) || (vpar >= svgar && slice())) {
        temp.scale(physHeight / logicHeight, physHeight / logicHeight);

        if (align() == SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMIN || align() == SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMID || align() == SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMAX)
            temp.translate(-logicX, -logicY);
        else if (align() == SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMIN || align() == SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID || align() == SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMAX)
            temp.translate(-logicX - (logicWidth - physWidth * logicHeight / physHeight) / 2, -logicY);
        else
            temp.translate(-logicX - (logicWidth - physWidth * logicHeight / physHeight), -logicY);
    } else {
        temp.scale(physWidth / logicWidth, physWidth / logicWidth);

        if (align() == SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMIN || align() == SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMIN || align() == SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMIN)
            temp.translate(-logicX, -logicY);
        else if (align() == SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMID || align() == SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID || align() == SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMID)
            temp.translate(-logicX, -logicY - (logicHeight - physHeight * logicWidth / physWidth) / 2);
        else
            temp.translate(-logicX, -logicY - (logicHeight - physHeight * logicWidth / physWidth));
    }

    return temp;
}

bool RenderSVGRoot::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int _x, int _y, int _tx, int _ty, HitTestAction hitTestAction)
{
    AffineTransform ctm = RenderContainer::absoluteTransform();
    if (!viewport().isEmpty()
        && style()->overflowX() == OHIDDEN
        && style()->overflowY() == OHIDDEN) {
        int tx = m_x - _tx;
        int ty = m_y - _ty;

        // Check if we need to do anything at all.
        IntRect overflowBox = overflowRect(false);
        overflowBox.move(tx, ty);
        ctm.translate(viewport().x(), viewport().y());
        double localX, localY;
        ctm.inverse().map(_x - _tx, _y - _ty, &localX, &localY);
        if (!overflowBox.contains((int)localX, (int)localY))
            return false;
    }

    int sx = (_tx - ctm.e()); // scroll offset
    int sy = (_ty - ctm.f()); // scroll offset
    for (RenderObject* child = lastChild(); child; child = child->previousSibling()) {
        if (child->nodeAtPoint(request, result, _x - sx, _y - sy, 0, 0, hitTestAction)) {
            updateHitTestResult(result, IntPoint(_x - _tx, _y - _ty));
            return true;
        }
    }
    
    // Spec: Only graphical elements can be targeted by the mouse, period.
    // 16.4: "If there are no graphics elements whose relevant graphics content is under the pointer (i.e., there is no target element), the event is not dispatched."
    return false;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
