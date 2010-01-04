/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007 Rob Buis <buis@kde.org>
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
#include "RenderSVGViewportContainer.h"

#include "GraphicsContext.h"

#include "RenderView.h"
#include "SVGMarkerElement.h"
#include "SVGSVGElement.h"

namespace WebCore {

RenderSVGViewportContainer::RenderSVGViewportContainer(SVGStyledElement* node)
    : RenderSVGContainer(node)
{
}

void RenderSVGViewportContainer::paint(PaintInfo& paintInfo, int parentX, int parentY)
{
    // FIXME: The if statement here evaluates to false. isEmpty() is exactly the same
    // as what is on the right side, so it's basically !isEmpty && isEmpty. So this
    // function does nothing.

    // A value of zero disables rendering of the element. 
    if (!m_viewport.isEmpty() && (m_viewport.width() <= 0. || m_viewport.height() <= 0.))
        return;

    RenderSVGContainer::paint(paintInfo, parentX, parentY);
}

void RenderSVGViewportContainer::applyViewportClip(PaintInfo& paintInfo)
{
    if (style()->overflowX() != OVISIBLE)
        paintInfo.context->clip(enclosingIntRect(m_viewport)); // FIXME: Eventually we'll want float-precision clipping
}

void RenderSVGViewportContainer::calcViewport()
{
    SVGElement* svgelem = static_cast<SVGElement*>(node());
    if (svgelem->hasTagName(SVGNames::svgTag)) {
        SVGSVGElement* svg = static_cast<SVGSVGElement*>(node());

        if (!selfNeedsLayout() && !svg->hasRelativeValues())
            return;

        float x = svg->x().value(svg);
        float y = svg->y().value(svg);
        float w = svg->width().value(svg);
        float h = svg->height().value(svg);
        m_viewport = FloatRect(x, y, w, h);
    } else if (svgelem->hasTagName(SVGNames::markerTag)) {
        if (!selfNeedsLayout())
            return;

        SVGMarkerElement* svg = static_cast<SVGMarkerElement*>(node());
        float w = svg->markerWidth().value(svg);
        float h = svg->markerHeight().value(svg);
        m_viewport = FloatRect(0, 0, w, h);
    }
}

TransformationMatrix RenderSVGViewportContainer::viewportTransform() const
{
    if (node()->hasTagName(SVGNames::svgTag)) {
        SVGSVGElement* svg = static_cast<SVGSVGElement*>(node());
        return svg->viewBoxToViewTransform(m_viewport.width(), m_viewport.height());
    } else if (node()->hasTagName(SVGNames::markerTag)) {
        SVGMarkerElement* marker = static_cast<SVGMarkerElement*>(node());
        return marker->viewBoxToViewTransform(m_viewport.width(), m_viewport.height());
    }

    return TransformationMatrix();
}

TransformationMatrix RenderSVGViewportContainer::localToParentTransform() const
{
    TransformationMatrix viewportTranslation;
    viewportTranslation.translate(m_viewport.x(), m_viewport.y());
    return viewportTransform() * viewportTranslation;
    // If this class were ever given a localTransform(), then the above would read:
    // return viewportTransform() * localTransform() * viewportTranslation;
}

// FIXME: This method should be removed as soon as callers to RenderBox::absoluteTransform() can be removed.
TransformationMatrix RenderSVGViewportContainer::absoluteTransform() const
{
    // This would apply localTransform() twice if localTransform() were not the identity.
    return localToParentTransform() * RenderSVGContainer::absoluteTransform();
}

bool RenderSVGViewportContainer::pointIsInsideViewportClip(const FloatPoint& pointInParent)
{
    // Respect the viewport clip (which is in parent coords).  SVG does not support separate x/y overflow rules.
    if (style()->overflowX() == OHIDDEN) {
        ASSERT(style()->overflowY() == OHIDDEN);
        if (!m_viewport.contains(pointInParent))
            return false;
    }
    return true;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
