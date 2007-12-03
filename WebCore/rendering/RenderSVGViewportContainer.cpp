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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "RenderSVGViewportContainer.h"

#include "GraphicsContext.h"

#include "RenderView.h"
#include "SVGMarkerElement.h"
#include "SVGSVGElement.h"

namespace WebCore {

RenderSVGViewportContainer::RenderSVGViewportContainer(SVGStyledElement* node)
    : RenderSVGContainer(node)
{
    setReplaced(true);
}

RenderSVGViewportContainer::~RenderSVGViewportContainer()
{
}

void RenderSVGViewportContainer::layout()
{
    ASSERT(needsLayout());
    
    calcViewport();
    
    // Arbitrary affine transforms are incompatible with LayoutState.
    view()->disableLayoutState();
    
    IntRect oldBounds = m_absoluteBounds;
    IntRect oldOutlineBox;
    bool checkForRepaint = checkForRepaintDuringLayout() && selfNeedsLayout();
    if (checkForRepaint)
        oldOutlineBox = absoluteOutlineBox();

    calcBounds();    
    
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (selfNeedsLayout())
            child->setNeedsLayout(true);
        
        child->layoutIfNeeded();
        ASSERT(!child->needsLayout());
    }
    
    if (checkForRepaint)
        repaintAfterLayoutIfNeeded(oldBounds, oldOutlineBox);
    
    view()->enableLayoutState();
    setNeedsLayout(false);
}

void RenderSVGViewportContainer::paint(PaintInfo& paintInfo, int parentX, int parentY)
{
    // A value of zero disables rendering of the element. 
    if (!viewport().isEmpty() && (viewport().width() <= 0. || viewport().height() <= 0.))
        return;

    RenderSVGContainer::paint(paintInfo, parentX, parentY);
}

void RenderSVGViewportContainer::applyContentTransforms(PaintInfo& paintInfo)
{
    if (!viewport().isEmpty()) {
        if (style()->overflowX() != OVISIBLE)
            paintInfo.context->clip(enclosingIntRect(viewport())); // FIXME: Eventually we'll want float-precision clipping
        
        paintInfo.context->concatCTM(AffineTransform().translate(viewport().x(), viewport().y()));
    }

    RenderSVGContainer::applyContentTransforms(paintInfo);
}

void RenderSVGViewportContainer::applyAdditionalTransforms(PaintInfo& paintInfo)
{
    paintInfo.context->concatCTM(viewportTransform());
    RenderSVGContainer::applyAdditionalTransforms(paintInfo);
}

FloatRect RenderSVGViewportContainer::viewport() const
{
    return m_viewport;
}

void RenderSVGViewportContainer::calcViewport()
{
    SVGElement* svgelem = static_cast<SVGElement*>(element());
    if (svgelem->hasTagName(SVGNames::svgTag)) {
        SVGSVGElement* svg = static_cast<SVGSVGElement*>(element());

        if (!selfNeedsLayout() && !svg->hasRelativeValues())
            return;

        float x = svg->x().value();
        float y = svg->y().value();
        float w = svg->width().value();
        float h = svg->height().value();
        m_viewport = FloatRect(x, y, w, h);
    } else if (svgelem->hasTagName(SVGNames::markerTag)) {
        if (!selfNeedsLayout())
            return;

        SVGMarkerElement* svg = static_cast<SVGMarkerElement*>(element());
        float w = svg->markerWidth().value();
        float h = svg->markerHeight().value();
        m_viewport = FloatRect(0, 0, w, h);
    }
}

AffineTransform RenderSVGViewportContainer::viewportTransform() const
{
    if (element()->hasTagName(SVGNames::svgTag)) {
        SVGSVGElement* svg = static_cast<SVGSVGElement*>(element());
        return svg->viewBoxToViewTransform(viewport().width(), viewport().height());
    } else if (element()->hasTagName(SVGNames::markerTag)) {
        SVGMarkerElement* marker = static_cast<SVGMarkerElement*>(element());
        return marker->viewBoxToViewTransform(viewport().width(), viewport().height());
    }
 
     return AffineTransform();
}

AffineTransform RenderSVGViewportContainer::absoluteTransform() const
{
    AffineTransform ctm = RenderObject::absoluteTransform();
    ctm.translate(viewport().x(), viewport().y());
    return viewportTransform() * ctm;
}

bool RenderSVGViewportContainer::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int _x, int _y, int _tx, int _ty, HitTestAction hitTestAction)
{
    if (!viewport().isEmpty()
        && style()->overflowX() == OHIDDEN
        && style()->overflowY() == OHIDDEN) {
        // Check if we need to do anything at all.
        IntRect overflowBox = overflowRect(false);
        overflowBox.move(_tx, _ty);
        AffineTransform ctm = RenderObject::absoluteTransform();
        ctm.translate(viewport().x(), viewport().y());
        double localX, localY;
        ctm.inverse().map(_x - _tx, _y - _ty, &localX, &localY);
        if (!overflowBox.contains((int)localX, (int)localY))
            return false;
    }

    int sx = 0;
    int sy = 0;

    // Respect parent translation offset for non-outermost <svg> elements.
    // Outermost <svg> element is handled by RenderSVGRoot.
    if (element()->hasTagName(SVGNames::svgTag)) {
        sx = _tx;
        sy = _ty;
    }

    for (RenderObject* child = lastChild(); child; child = child->previousSibling()) {
        if (child->nodeAtPoint(request, result, _x - sx, _y - sy, _tx, _ty, hitTestAction)) {
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
