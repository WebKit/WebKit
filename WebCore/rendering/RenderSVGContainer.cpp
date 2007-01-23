/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007 Rob Buis <buis@kde.org>

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

#ifdef SVG_SUPPORT
#include "RenderSVGContainer.h"

#include "SVGResourceClipper.h"
#include "SVGResourceFilter.h"
#include "SVGResourceMasker.h"
#include "SVGStyledElement.h"
#include "SVGURIReference.h"
#include "GraphicsContext.h"
#include "SVGLength.h"
#include "SVGMarkerElement.h"
#include "SVGSVGElement.h"

namespace WebCore {

RenderSVGContainer::RenderSVGContainer(SVGStyledElement *node)
    : RenderContainer(node)
    , m_drawsContents(true)
    , m_slice(false)
{
    setReplaced(true);
}

RenderSVGContainer::~RenderSVGContainer()
{
}

bool RenderSVGContainer::drawsContents() const
{
    return m_drawsContents;
}

void RenderSVGContainer::setDrawsContents(bool drawsContents)
{
    m_drawsContents = drawsContents;
}

AffineTransform RenderSVGContainer::localTransform() const
{
    return m_matrix;
}

void RenderSVGContainer::setLocalTransform(const AffineTransform& matrix)
{
    m_matrix = matrix;
}

bool RenderSVGContainer::canHaveChildren() const
{
    return true;
}
    
bool RenderSVGContainer::requiresLayer()
{
    // Only allow an <svg> element to generate a layer when it's positioned in a non-SVG context
    return (isPositioned() || isRelPositioned()) && (element()->parent() && !element()->parent()->isSVGElement());
}

short RenderSVGContainer::lineHeight(bool b, bool isRootLineBox) const
{
    return height() + marginTop() + marginBottom();
}

short RenderSVGContainer::baselinePosition(bool b, bool isRootLineBox) const
{
    return height() + marginTop() + marginBottom();
}

void RenderSVGContainer::calcMinMaxWidth()
{
    ASSERT(!minMaxKnown());
    m_minWidth = m_maxWidth = 0;
    setMinMaxKnown();
}

void RenderSVGContainer::layout()
{
    ASSERT(needsLayout());
    ASSERT(minMaxKnown());

    calcViewport();

    IntRect oldBounds;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (selfNeedsLayout() && checkForRepaint)
        oldBounds = m_absoluteBounds;

    RenderObject* child = firstChild();
    while (child) {
        if (!child->isRenderPath() || static_cast<RenderPath*>(child)->hasRelativeValues())
            child->setNeedsLayout(true);

        child->layoutIfNeeded();
        ASSERT(!child->needsLayout());
        child = child->nextSibling();
    }

    calcWidth();
    calcHeight();

    m_absoluteBounds = getAbsoluteRepaintRect();

    if (selfNeedsLayout() && checkForRepaint)
        repaintAfterLayoutIfNeeded(oldBounds, oldBounds);

    setNeedsLayout(false);
}

void RenderSVGContainer::paint(PaintInfo& paintInfo, int parentX, int parentY)
{
    if (paintInfo.context->paintingDisabled())
        return;

    // This should only exist for <svg> renderers
    if (hasBoxDecorations() && (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection)) 
        paintBoxDecorations(paintInfo, m_x + parentX, m_y + parentY);

    if ((paintInfo.phase == PaintPhaseOutline || paintInfo.phase == PaintPhaseSelfOutline) && style()->outlineWidth() && style()->visibility() == VISIBLE)
        paintOutline(paintInfo.context, parentX, parentY, width(), height(), style());
    
    if (paintInfo.phase != PaintPhaseForeground || !drawsContents())
        return;

    const SVGRenderStyle* svgStyle = style()->svgStyle();
    AtomicString filterId(SVGURIReference::getTarget(svgStyle->filter()));
 
    SVGResourceFilter* filter = getFilterById(document(), filterId);
    if (!firstChild() && !filter)
        return; // Spec: groups w/o children still may render filter content.
    
    paintInfo.context->save();

    if (!parent()->isSVGContainer()) {
        // Translate from parent offsets (html renderers) to a relative transform (svg renderers)
        IntPoint origin;
        origin.move(parentX, parentY);
        origin.move(m_x, m_y);
        origin.move(borderLeft(), borderTop());
        origin.move(paddingLeft(), paddingTop());
        if (origin.x() || origin.y())
            paintInfo.context->concatCTM(AffineTransform().translate(origin.x(), origin.y()));
        parentX = parentY = 0;
    } else {
        // Only the root <svg> element should need any translations using the HTML/CSS system
        // parentX, parentY are also non-zero for first-level kids of these
        // CSS-transformed <svg> root-elements (due to RenderBox::paint) for any other element
        // they should be 0.   m_x, m_y should always be 0 for non-root svg containers
        ASSERT(m_x == 0);
        ASSERT(m_y == 0);
    }

    if (!viewport().isEmpty()) {
        if (style()->overflowX() != OVISIBLE)
            paintInfo.context->clip(enclosingIntRect(viewport())); // FIXME: Eventually we'll want float-precision clipping

        paintInfo.context->concatCTM(AffineTransform().translate(viewport().x(), viewport().y()));
    }

    if (!localTransform().isIdentity())
        paintInfo.context->concatCTM(localTransform());

    FloatRect strokeBBox = relativeBBox(true);

    SVGElement* svgElement = static_cast<SVGElement*>(element());
    ASSERT(svgElement && svgElement->document() && svgElement->isStyled());

    SVGStyledElement* styledElement = static_cast<SVGStyledElement*>(svgElement);
 
    AtomicString clipperId(SVGURIReference::getTarget(svgStyle->clipPath()));
    AtomicString maskerId(SVGURIReference::getTarget(svgStyle->maskElement()));

    SVGResourceClipper* clipper = getClipperById(document(), clipperId);
    SVGResourceMasker* masker = getMaskerById(document(), maskerId);

    if (clipper) {
        clipper->addClient(styledElement);
        clipper->applyClip(paintInfo.context, strokeBBox);
    } else if (!clipperId.isEmpty())
        svgElement->document()->accessSVGExtensions()->addPendingResource(clipperId, styledElement);

    if (masker) {
        masker->addClient(styledElement);
        masker->applyMask(paintInfo.context, strokeBBox);
    } else if (!maskerId.isEmpty())
        svgElement->document()->accessSVGExtensions()->addPendingResource(maskerId, styledElement);

    float opacity = style()->opacity();
    if (opacity < 1.0f) {
        paintInfo.context->clip(enclosingIntRect(strokeBBox));
        paintInfo.context->beginTransparencyLayer(opacity);
    }

    if (filter)
        filter->prepareFilter(paintInfo.context, strokeBBox);
    else if (!filterId.isEmpty())
        svgElement->document()->accessSVGExtensions()->addPendingResource(filterId, styledElement);

    if (!viewBox().isEmpty())
        paintInfo.context->concatCTM(viewportTransform());

    RenderContainer::paint(paintInfo, 0, 0);

    if (filter)
        filter->applyFilter(paintInfo.context, strokeBBox);

    if (opacity < 1.0f)
        paintInfo.context->endTransparencyLayer();

    paintInfo.context->restore();
}

FloatRect RenderSVGContainer::viewport() const
{
    return m_viewport;
}

void RenderSVGContainer::calcViewport()
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
    } else if (svgelem->hasTagName(SVGNames::markerTag)) {
        if (!selfNeedsLayout())
            return;

        SVGMarkerElement* svg = static_cast<SVGMarkerElement*>(element());
        double w = svg->markerWidth().value();
        double h = svg->markerHeight().value();
        m_viewport = FloatRect(0, 0, w, h);
    }
}

void RenderSVGContainer::setViewBox(const FloatRect& viewBox)
{
    m_viewBox = viewBox;

    if (style())
        setNeedsLayout(true);
}

FloatRect RenderSVGContainer::viewBox() const
{
    return m_viewBox;
}

void RenderSVGContainer::setAlign(KCAlign align)
{
    m_align = align;
    if (style())
        setNeedsLayout(true);
}

KCAlign RenderSVGContainer::align() const
{
    return m_align;
}

AffineTransform RenderSVGContainer::viewportTransform() const
{
    // FIXME: The method name is confusing, since it does not
    // do viewport translating anymore. Look into this while
    //  fixing bug 12207.
    if (!viewBox().isEmpty()) {
        FloatRect viewportRect = viewport();
        if (!parent()->isSVGContainer())
            viewportRect = FloatRect(viewport().x(), viewport().y(), width(), height());

        return getAspectRatio(viewBox(), viewportRect);
    }

    return AffineTransform();
}

IntRect RenderSVGContainer::getAbsoluteRepaintRect()
{
    IntRect repaintRect;

    for (RenderObject* current = firstChild(); current != 0; current = current->nextSibling())
        repaintRect.unite(current->getAbsoluteRepaintRect());

    // Filters can expand the bounding box
    SVGResourceFilter* filter = getFilterById(document(), SVGURIReference::getTarget(style()->svgStyle()->filter()));
    if (filter)
        repaintRect.unite(enclosingIntRect(filter->filterBBoxForItemBBox(repaintRect)));

    return repaintRect;
}

void RenderSVGContainer::absoluteRects(Vector<IntRect>& rects, int, int)
{
    rects.append(getAbsoluteRepaintRect());
}

AffineTransform RenderSVGContainer::absoluteTransform() const
{
    AffineTransform ctm = RenderContainer::absoluteTransform();
    ctm.translate(viewport().x(), viewport().y());
    return viewportTransform() * ctm;
}

bool RenderSVGContainer::fillContains(const FloatPoint& p) const
{
    RenderObject* current = firstChild();
    while (current != 0) {
        if (current->isRenderPath() && static_cast<RenderPath*>(current)->fillContains(p))
            return true;

        current = current->nextSibling();
    }

    return false;
}

bool RenderSVGContainer::strokeContains(const FloatPoint& p) const
{
    RenderObject* current = firstChild();
    while (current != 0) {
        if (current->isRenderPath() && static_cast<RenderPath*>(current)->strokeContains(p))
            return true;

        current = current->nextSibling();
    }

    return false;
}

FloatRect RenderSVGContainer::relativeBBox(bool includeStroke) const
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

void RenderSVGContainer::setSlice(bool slice)
{
    m_slice = slice;

    if (style())
        setNeedsLayout(true);
}

bool RenderSVGContainer::slice() const
{
    return m_slice;
}

AffineTransform RenderSVGContainer::getAspectRatio(const FloatRect& logical, const FloatRect& physical) const
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

    if (align() == ALIGN_NONE) {
        temp.scale(physWidth / logicWidth, physHeight / logicHeight);
        temp.translate(-logicX, -logicY);
    } else if ((vpar < svgar && !slice()) || (vpar >= svgar && slice())) {
        temp.scale(physHeight / logicHeight, physHeight / logicHeight);

        if (align() == ALIGN_XMINYMIN || align() == ALIGN_XMINYMID || align() == ALIGN_XMINYMAX)
            temp.translate(-logicX, -logicY);
        else if (align() == ALIGN_XMIDYMIN || align() == ALIGN_XMIDYMID || align() == ALIGN_XMIDYMAX)
            temp.translate(-logicX - (logicWidth - physWidth * logicHeight / physHeight) / 2, -logicY);
        else
            temp.translate(-logicX - (logicWidth - physWidth * logicHeight / physHeight), -logicY);
    } else {
        temp.scale(physWidth / logicWidth, physWidth / logicWidth);

        if (align() == ALIGN_XMINYMIN || align() == ALIGN_XMIDYMIN || align() == ALIGN_XMAXYMIN)
            temp.translate(-logicX, -logicY);
        else if (align() == ALIGN_XMINYMID || align() == ALIGN_XMIDYMID || align() == ALIGN_XMAXYMID)
            temp.translate(-logicX, -logicY - (logicHeight - physHeight * logicWidth / physWidth) / 2);
        else
            temp.translate(-logicX, -logicY - (logicHeight - physHeight * logicWidth / physWidth));
    }

    return temp;
}

bool RenderSVGContainer::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int _x, int _y, int _tx, int _ty, HitTestAction hitTestAction)
{
    SVGElement* svgelem = static_cast<SVGElement*>(element());
    if (svgelem->hasTagName(SVGNames::svgTag)) {
        int tx = _tx + m_x;
        int ty = _ty + m_y;

        // Check if we need to do anything at all.
        IntRect overflowBox = overflowRect(false);
        overflowBox.move(tx, ty);
        AffineTransform ctm = RenderContainer::absoluteTransform();
        ctm.translate(viewport().x(), viewport().y());
        double localX, localY;
        ctm.inverse().map(_x + _tx, _y + _ty, &localX, &localY);
        if (!overflowBox.contains((int)localX, (int)localY))
            return false;
    }

    for (RenderObject* child = lastChild(); child; child = child->previousSibling()) {
        if (child->nodeAtPoint(request, result, _x, _y, _tx, _ty, hitTestAction)) {
            updateHitTestResult(result, IntPoint(_x - _tx, _y - _ty));
            return true;
        }
    }
    
    // Spec: Only graphical elements can be targeted by the mouse, period.
    // 16.4: "If there are no graphics elements whose relevant graphics content is under the pointer (i.e., there is no target element), the event is not dispatched."
    return false;
}

}

#endif // SVG_SUPPORT

// vim:ts=4:noet
