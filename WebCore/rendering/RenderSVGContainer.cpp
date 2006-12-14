/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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
#include "GraphicsContext.h"
#include "SVGLength.h"
#include "SVGMarkerElement.h"
#include "SVGSVGElement.h"
#include "SVGStyledTransformableElement.h"

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
    return (isPositioned() || isRelPositioned()) && (parent() && parent()->element() && !parent()->element()->isSVGElement());
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

    if (selfNeedsLayout())
        calcViewport();

    IntRect oldBounds;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (selfNeedsLayout() && checkForRepaint)
        oldBounds = m_absoluteBounds;

    RenderObject* child = firstChild();
    while (child) {
        if (!child->isRenderPath() || static_cast<RenderPath*>(child)->hasPercentageValues())
            child->setNeedsLayout(true);
        child = child->nextSibling();
    }
    RenderContainer::layout();
    calcWidth();
    calcHeight();

    m_absoluteBounds = getAbsoluteRepaintRect();

    if (selfNeedsLayout() && checkForRepaint)
        repaintAfterLayoutIfNeeded(oldBounds, oldBounds);
}

void RenderSVGContainer::paint(PaintInfo& paintInfo, int parentX, int parentY)
{
    if (paintInfo.context->paintingDisabled())
        return;

    // No one should be transforming us via these.
    //ASSERT(m_x == 0);
    //ASSERT(m_y == 0);

    if (shouldPaintBackgroundOrBorder() && (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection)) 
        paintBoxDecorations(paintInfo, parentX, parentY);

    if ((paintInfo.phase == PaintPhaseOutline || paintInfo.phase == PaintPhaseSelfOutline) && style()->outlineWidth() && style()->visibility() == VISIBLE)
        paintOutline(paintInfo.context, parentX, parentY, width(), height(), style());
    
    if (paintInfo.phase != PaintPhaseForeground || !drawsContents() || style()->visibility() == HIDDEN)
        return;
    
    SVGResourceFilter* filter = getFilterById(document(), style()->svgStyle()->filter().substring(1));
    if (!firstChild() && !filter)
        return; // Spec: groups w/o children still may render filter content.
    
    paintInfo.context->save();

    if (parentX != 0 || parentY != 0) {
        // Translate from parent offsets (html renderers) to a relative transform (svg renderers)
        paintInfo.context->concatCTM(AffineTransform().translate(parentX, parentY));
        parentX = parentY = 0;
    }
    
    if (!viewport().isEmpty()) {
        if (style()->overflowX() != OVISIBLE)
            paintInfo.context->clip(enclosingIntRect(viewport())); // FIXME: Eventually we'll want float-precision clipping

        paintInfo.context->concatCTM(AffineTransform().translate(viewport().x(), viewport().y()));
    }

    if (!localTransform().isIdentity())
        paintInfo.context->concatCTM(localTransform());
    
    FloatRect strokeBBox = relativeBBox(true);
    
    if (SVGResourceClipper* clipper = getClipperById(document(), style()->svgStyle()->clipPath().substring(1)))
        clipper->applyClip(paintInfo.context, strokeBBox);

    if (SVGResourceMasker* masker = getMaskerById(document(), style()->svgStyle()->maskElement().substring(1)))
        masker->applyMask(paintInfo.context, strokeBBox);

    float opacity = style()->opacity();
    if (opacity < 1.0f) {
        paintInfo.context->clip(enclosingIntRect(strokeBBox));
        paintInfo.context->beginTransparencyLayer(opacity);
    }

    if (filter)
        filter->prepareFilter(paintInfo.context, strokeBBox);

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
        double x = svg->x()->value();
        double y = svg->y()->value();
        double w = svg->width()->value();
        double h = svg->height()->value();
        m_viewport = FloatRect(x, y, w, h);
    } else if (svgelem->hasTagName(SVGNames::markerTag)) {
        SVGMarkerElement* svg = static_cast<SVGMarkerElement*>(element());
            double w = svg->markerWidth()->value();
        double h = svg->markerHeight()->value();
        m_viewport = FloatRect(0, 0, w, h);
    }
}

void RenderSVGContainer::setViewBox(const FloatRect& viewBox)
{
    m_viewBox = viewBox;
}

FloatRect RenderSVGContainer::viewBox() const
{
    return m_viewBox;
}

void RenderSVGContainer::setAlign(KCAlign align)
{
    m_align = align;
}

KCAlign RenderSVGContainer::align() const
{
    return m_align;
}

AffineTransform RenderSVGContainer::viewportTransform() const
{
    if (!viewBox().isEmpty()) {
        FloatRect viewportRect = viewport();
        if (!parent()->isKCanvasContainer())
            viewportRect = FloatRect(viewport().x(), viewport().y(), width(), height());
        return getAspectRatio(viewBox(), viewportRect);
    }
    return AffineTransform().translate(viewport().x(), viewport().y());
}

IntRect RenderSVGContainer::getAbsoluteRepaintRect()
{
    IntRect repaintRect;
    
    for (RenderObject *current = firstChild(); current != 0; current = current->nextSibling())
        repaintRect.unite(current->getAbsoluteRepaintRect());
    
    // Filters can expand the bounding box
    SVGResourceFilter *filter = getFilterById(document(), style()->svgStyle()->filter().substring(1));
    if (filter)
        repaintRect.unite(enclosingIntRect(filter->filterBBoxForItemBBox(repaintRect)));

    // FIXME: what about transform?

    return repaintRect;
}

void RenderSVGContainer::computeAbsoluteRepaintRect(IntRect& r, bool f)
{
    AffineTransform transform = localTransform();
    r = transform.mapRect(r);
    
    // FIXME: consider filter

    RenderContainer::computeAbsoluteRepaintRect(r, f);
}

AffineTransform RenderSVGContainer::absoluteTransform() const
{
    return viewportTransform() * RenderContainer::absoluteTransform();
}

bool RenderSVGContainer::fillContains(const FloatPoint& p) const
{
    RenderObject *current = firstChild();
    while (current != 0) {
        if (current->isRenderPath() && static_cast<RenderPath*>(current)->fillContains(p))
            return true;
        current = current->nextSibling();
    }

    return false;
}

bool RenderSVGContainer::strokeContains(const FloatPoint& p) const
{
    RenderObject *current = firstChild();
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
    
    RenderObject *current = firstChild();
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
        AffineTransform totalTransform = absoluteTransform();
        double localX, localY;
        totalTransform.inverse().map(_x + _tx, _y + _ty, &localX, &localY);
        if (!overflowBox.contains((int)localX, (int)localY))
            return false;
    }

    return RenderContainer::nodeAtPoint(request, result, _x, _y, _tx, _ty, hitTestAction);
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

