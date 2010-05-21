/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *               2007 Rob Buis <buis@kde.org>
 *               2008 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
#include "RenderSVGResource.h"

#include "RenderSVGResourceContainer.h"
#include "RenderSVGResourceSolidColor.h"
#include "SVGURIReference.h"

namespace WebCore {

static inline void registerPendingResource(const AtomicString& id, const SVGPaint::SVGPaintType& paintType, const RenderObject* object)
{
    if (paintType != SVGPaint::SVG_PAINTTYPE_URI)
        return;

    SVGElement* svgElement = static_cast<SVGElement*>(object->node());
    ASSERT(svgElement);
    ASSERT(svgElement->isStyled());

    object->document()->accessSVGExtensions()->addPendingResource(id, static_cast<SVGStyledElement*>(svgElement));
}

inline void RenderSVGResource::adjustColorForPseudoRules(const RenderStyle* style, bool useFillPaint, Color& color)
{
    if (style->insideLink() != InsideVisitedLink)
        return;

    RenderStyle* visitedStyle = style->getCachedPseudoStyle(VISITED_LINK);
    SVGPaint* visitedPaint = useFillPaint ? visitedStyle->svgStyle()->fillPaint() : visitedStyle->svgStyle()->strokePaint();
    if (visitedPaint->paintType() == SVGPaint::SVG_PAINTTYPE_URI)
        return;
        
    Color visitedColor;
    if (visitedPaint->paintType() == SVGPaint::SVG_PAINTTYPE_CURRENTCOLOR)
        visitedColor = visitedStyle->color();
    else
        visitedColor = visitedPaint->color();

    if (visitedColor.isValid())
        color = Color(visitedColor.red(), visitedColor.green(), visitedColor.blue(), color.alpha());
}

// FIXME: This method and strokePaintingResource() should be refactored, to share even more code
RenderSVGResource* RenderSVGResource::fillPaintingResource(const RenderObject* object, const RenderStyle* style)
{
    ASSERT(object);
    ASSERT(style);

    const SVGRenderStyle* svgStyle = style->svgStyle();
    if (!svgStyle || !svgStyle->hasFill())
        return 0;

    SVGPaint* fillPaint = svgStyle->fillPaint();
    ASSERT(fillPaint);

    RenderSVGResource* fillPaintingResource = 0;

    SVGPaint::SVGPaintType paintType = fillPaint->paintType();
    if (paintType == SVGPaint::SVG_PAINTTYPE_URI
        || paintType == SVGPaint::SVG_PAINTTYPE_URI_RGBCOLOR) {
        AtomicString id(SVGURIReference::getTarget(fillPaint->uri()));
        fillPaintingResource = getRenderSVGResourceContainerById(object->document(), id);

        if (!fillPaintingResource)
            registerPendingResource(id, paintType, object);
    }

    if (paintType != SVGPaint::SVG_PAINTTYPE_URI && !fillPaintingResource) {
        RenderSVGResourceSolidColor* solidResource = sharedSolidPaintingResource();
        fillPaintingResource = solidResource;

        Color fillColor;
        if (fillPaint->paintType() == SVGPaint::SVG_PAINTTYPE_CURRENTCOLOR)
            fillColor = style->visitedDependentColor(CSSPropertyColor);
        else
            fillColor = fillPaint->color();

        adjustColorForPseudoRules(style, true /* useFillPaint */, fillColor);

        // FIXME: Ideally invalid colors would never get set on the RenderStyle and this could turn into an ASSERT
        if (fillColor.isValid())
            solidResource->setColor(fillColor);
        else
            fillPaintingResource = 0;
    }

    if (!fillPaintingResource) {
        // default value (black), see bug 11017
        RenderSVGResourceSolidColor* solidResource = sharedSolidPaintingResource();
        solidResource->setColor(Color::black);
        fillPaintingResource = solidResource;
    }

    return fillPaintingResource;
}

RenderSVGResource* RenderSVGResource::strokePaintingResource(const RenderObject* object, const RenderStyle* style)
{
    ASSERT(object);
    ASSERT(style);

    const SVGRenderStyle* svgStyle = style->svgStyle();
    if (!svgStyle || !svgStyle->hasStroke())
        return 0;

    SVGPaint* strokePaint = svgStyle->strokePaint();
    ASSERT(strokePaint);

    RenderSVGResource* strokePaintingResource = 0;
    FloatRect objectBoundingBox = object->objectBoundingBox();

    SVGPaint::SVGPaintType paintType = strokePaint->paintType();
    if (!objectBoundingBox.isEmpty()
        && (paintType == SVGPaint::SVG_PAINTTYPE_URI || paintType == SVGPaint::SVG_PAINTTYPE_URI_RGBCOLOR)) {
        AtomicString id(SVGURIReference::getTarget(strokePaint->uri()));
        strokePaintingResource = getRenderSVGResourceContainerById(object->document(), id);

        if (!strokePaintingResource)
            registerPendingResource(id, paintType, object);
    }

    if (paintType != SVGPaint::SVG_PAINTTYPE_URI && !strokePaintingResource) {
        RenderSVGResourceSolidColor* solidResource = sharedSolidPaintingResource();
        strokePaintingResource = solidResource;

        Color strokeColor;
        if (strokePaint->paintType() == SVGPaint::SVG_PAINTTYPE_CURRENTCOLOR)
            strokeColor = style->visitedDependentColor(CSSPropertyColor);
        else
            strokeColor = strokePaint->color();

        adjustColorForPseudoRules(style, false /* useFillPaint */, strokeColor);

        // FIXME: Ideally invalid colors would never get set on the RenderStyle and this could turn into an ASSERT
        if (strokeColor.isValid())
            solidResource->setColor(strokeColor);
        else
            strokePaintingResource = 0;
    }

    if (!strokePaintingResource) {
        // default value (black), see bug 11017
        RenderSVGResourceSolidColor* solidResource = sharedSolidPaintingResource();
        solidResource->setColor(Color::black);
        strokePaintingResource = solidResource;
    }

    return strokePaintingResource;
}

RenderSVGResourceSolidColor* RenderSVGResource::sharedSolidPaintingResource()
{
    static RenderSVGResourceSolidColor* s_sharedSolidPaintingResource = 0;
    if (!s_sharedSolidPaintingResource)
        s_sharedSolidPaintingResource = new RenderSVGResourceSolidColor;
    return s_sharedSolidPaintingResource;
}

void RenderSVGResource::markForLayoutAndResourceInvalidation(RenderObject* object)
{
    ASSERT(object);
    ASSERT(object->node());
    ASSERT(object->node()->isSVGElement());

    // Mark the renderer for layout
    object->setNeedsLayout(true);

    // Notify any resources in the ancestor chain, that we've been invalidated
    SVGElement* element = static_cast<SVGElement*>(object->node());
    if (!element->isStyled())
        return;

    static_cast<SVGStyledElement*>(element)->invalidateResourcesInAncestorChain();
}

}

#endif
