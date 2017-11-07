/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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
 */

#include "config.h"
#include "RenderSVGViewportContainer.h"

#include "GraphicsContext.h"
#include "RenderView.h"
#include "SVGSVGElement.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGViewportContainer);

RenderSVGViewportContainer::RenderSVGViewportContainer(SVGSVGElement& element, RenderStyle&& style)
    : RenderSVGContainer(element, WTFMove(style))
    , m_didTransformToRootUpdate(false)
    , m_isLayoutSizeChanged(false)
    , m_needsTransformUpdate(true)
{
}

SVGSVGElement& RenderSVGViewportContainer::svgSVGElement() const
{
    return downcast<SVGSVGElement>(RenderSVGContainer::element());
}

void RenderSVGViewportContainer::determineIfLayoutSizeChanged()
{
    m_isLayoutSizeChanged = svgSVGElement().hasRelativeLengths() && selfNeedsLayout();
}

void RenderSVGViewportContainer::applyViewportClip(PaintInfo& paintInfo)
{
    if (SVGRenderSupport::isOverflowHidden(*this))
        paintInfo.context().clip(m_viewport);
}

void RenderSVGViewportContainer::calcViewport()
{
    SVGSVGElement& element = svgSVGElement();
    SVGLengthContext lengthContext(&element);
    FloatRect newViewport(element.x().value(lengthContext), element.y().value(lengthContext), element.width().value(lengthContext), element.height().value(lengthContext));

    if (m_viewport == newViewport)
        return;

    m_viewport = newViewport;

    setNeedsBoundariesUpdate();
    setNeedsTransformUpdate();
}

bool RenderSVGViewportContainer::calculateLocalTransform() 
{
    m_didTransformToRootUpdate = m_needsTransformUpdate || SVGRenderSupport::transformToRootChanged(parent());
    if (!m_needsTransformUpdate)
        return false;
    
    m_localToParentTransform = AffineTransform::translation(m_viewport.x(), m_viewport.y()) * viewportTransform();
    m_needsTransformUpdate = false;
    return true;
}

AffineTransform RenderSVGViewportContainer::viewportTransform() const
{
    return svgSVGElement().viewBoxToViewTransform(m_viewport.width(), m_viewport.height());
}

bool RenderSVGViewportContainer::pointIsInsideViewportClip(const FloatPoint& pointInParent)
{
    // Respect the viewport clip (which is in parent coords)
    if (!SVGRenderSupport::isOverflowHidden(*this))
        return true;
    
    return m_viewport.contains(pointInParent);
}

void RenderSVGViewportContainer::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // An empty viewBox disables rendering.
    if (svgSVGElement().hasEmptyViewBox())
        return;

    RenderSVGContainer::paint(paintInfo, paintOffset);
}

}
