/*
 * Copyright (C) 2007, 2008 Rob Buis <buis@kde.org>
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 *           (C) 2007 Eric Seidel <eric@webkit.org>
 *           (C) 2009 Google, Inc.  All rights reserved.
 *           (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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
#include "SVGRenderSupport.h"

#include "FrameView.h"
#include "ImageBuffer.h"
#include "NodeRenderStyle.h"
#include "RenderLayer.h"
#include "RenderPath.h"
#include "RenderSVGContainer.h"
#include "RenderSVGResource.h"
#include "RenderSVGResourceClipper.h"
#include "RenderSVGResourceFilter.h"
#include "RenderSVGResourceMarker.h"
#include "RenderSVGResourceMasker.h"
#include "RenderSVGRoot.h"
#include "SVGResources.h"
#include "SVGStyledElement.h"
#include "TransformState.h"
#include <wtf/UnusedParam.h>

namespace WebCore {

IntRect SVGRenderSupport::clippedOverflowRectForRepaint(RenderObject* object, RenderBoxModelObject* repaintContainer)
{
    // Return early for any cases where we don't actually paint
    if (object->style()->visibility() != VISIBLE && !object->enclosingLayer()->hasVisibleContent())
        return IntRect();

    // Pass our local paint rect to computeRectForRepaint() which will
    // map to parent coords and recurse up the parent chain.
    IntRect repaintRect = enclosingIntRect(object->repaintRectInLocalCoordinates());
    object->computeRectForRepaint(repaintContainer, repaintRect);
    return repaintRect;
}

void SVGRenderSupport::computeRectForRepaint(RenderObject* object, RenderBoxModelObject* repaintContainer, IntRect& repaintRect, bool fixed)
{
    object->style()->svgStyle()->inflateForShadow(repaintRect);

    // Translate to coords in our parent renderer, and then call computeRectForRepaint on our parent
    repaintRect = object->localToParentTransform().mapRect(repaintRect);
    object->parent()->computeRectForRepaint(repaintContainer, repaintRect, fixed);
}

void SVGRenderSupport::mapLocalToContainer(const RenderObject* object, RenderBoxModelObject* repaintContainer, bool fixed , bool useTransforms, TransformState& transformState)
{
    ASSERT(!fixed); // We should have no fixed content in the SVG rendering tree.
    ASSERT(useTransforms); // Mapping a point through SVG w/o respecting transforms is useless.
    transformState.applyTransform(object->localToParentTransform());
    object->parent()->mapLocalToContainer(repaintContainer, fixed, useTransforms, transformState);
}

bool SVGRenderSupport::prepareToRenderSVGContent(RenderObject* object, PaintInfo& paintInfo)
{
    ASSERT(object);

    RenderStyle* style = object->style();
    ASSERT(style);

    const SVGRenderStyle* svgStyle = style->svgStyle();
    ASSERT(svgStyle);

    FloatRect repaintRect;

    // Setup transparency layers before setting up SVG resources!
    float opacity = style->opacity();
    if (opacity < 1) {
        repaintRect = object->repaintRectInLocalCoordinates();
        paintInfo.context->clip(repaintRect);
        paintInfo.context->beginTransparencyLayer(opacity);
    }

    if (const ShadowData* shadow = svgStyle->shadow()) {
        // Eventually compute repaint rect, if not done so far.
        if (opacity >= 1)
            repaintRect = object->repaintRectInLocalCoordinates();

        paintInfo.context->clip(repaintRect);
        paintInfo.context->setShadow(IntSize(shadow->x(), shadow->y()), shadow->blur(), shadow->color(), style->colorSpace());
        paintInfo.context->beginTransparencyLayer(1);
    }

    SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(object);
    if (!resources)
        return true;

    if (RenderSVGResourceMasker* masker = resources->masker()) {
        if (!masker->applyResource(object, style, paintInfo.context, ApplyToDefaultMode))
            return false;
    }

    if (RenderSVGResourceClipper* clipper = resources->clipper())
        clipper->applyResource(object, style, paintInfo.context, ApplyToDefaultMode);

#if ENABLE(FILTERS)
    if (RenderSVGResourceFilter* filter = resources->filter()) {
        if (!filter->applyResource(object, style, paintInfo.context, ApplyToDefaultMode))
            return false;
    }
#endif

    return true;
}

void SVGRenderSupport::finishRenderSVGContent(RenderObject* object, PaintInfo& paintInfo, GraphicsContext* savedContext)
{
#if !ENABLE(FILTERS)
    UNUSED_PARAM(savedContext);
#endif

    ASSERT(object);

    const RenderStyle* style = object->style();
    ASSERT(style);

    const SVGRenderStyle* svgStyle = style->svgStyle();
    ASSERT(svgStyle);

#if ENABLE(FILTERS)
    SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(object);
    if (resources) {
        if (RenderSVGResourceFilter* filter = resources->filter()) {
            filter->postApplyResource(object, paintInfo.context, ApplyToDefaultMode);
            paintInfo.context = savedContext;
        }
    }
#endif

    float opacity = style->opacity();    
    if (opacity < 1)
        paintInfo.context->endTransparencyLayer();

    // This needs to be done separately from opacity, because if both properties are set,
    // then the transparency layers are nested. 
    if (svgStyle->shadow())
        paintInfo.context->endTransparencyLayer();
}

void SVGRenderSupport::renderSubtreeToImage(ImageBuffer* image, RenderObject* item)
{
    ASSERT(item);
    ASSERT(image);
    ASSERT(image->context());

    PaintInfo info(image->context(), PaintInfo::infiniteRect(), PaintPhaseForeground, 0, 0, 0);

    // FIXME: isSVGContainer returns true for RenderSVGViewportContainer, so if this is ever
    // called with one of those, we will read from the wrong offset in an object due to a bad cast.
    RenderSVGContainer* svgContainer = 0;
    if (item && item->isSVGContainer())
        svgContainer = toRenderSVGContainer(item);

    bool drawsContents = svgContainer ? svgContainer->drawsContents() : false;
    if (svgContainer && !drawsContents)
        svgContainer->setDrawsContents(true);

    item->layoutIfNeeded();
    item->paint(info, 0, 0);

    if (svgContainer && !drawsContents)
        svgContainer->setDrawsContents(false);
}

FloatRect SVGRenderSupport::computeContainerBoundingBox(const RenderObject* container, ContainerBoundingBoxMode mode)
{
    FloatRect boundingBox;

    for (RenderObject* current = container->firstChild(); current; current = current->nextSibling()) {
        FloatRect childBoundingBox;

        switch (mode) {
        case ObjectBoundingBox:
            childBoundingBox = current->objectBoundingBox();
            break;
        case StrokeBoundingBox:
            childBoundingBox = current->strokeBoundingBox();
            break;
        case RepaintBoundingBox:
            childBoundingBox = current->repaintRectInLocalCoordinates();
            break;
        }

        boundingBox.unite(current->localToParentTransform().mapRect(childBoundingBox));
    }

    return boundingBox;
}

const RenderSVGRoot* SVGRenderSupport::findTreeRootObject(const RenderObject* start)
{
    while (start && !start->isSVGRoot())
        start = start->parent();

    ASSERT(start);
    ASSERT(start->isSVGRoot());
    return toRenderSVGRoot(start);
}

static inline void invalidateResourcesOfChildren(RenderObject* start)
{
    ASSERT(!start->needsLayout());
    if (SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(start))
        resources->removeClientFromCache(start, false);

    for (RenderObject* child = start->firstChild(); child; child = child->nextSibling())
        invalidateResourcesOfChildren(child);
}

void SVGRenderSupport::layoutChildren(RenderObject* start, bool selfNeedsLayout)
{
    bool layoutSizeChanged = findTreeRootObject(start)->isLayoutSizeChanged();
    HashSet<RenderObject*> notlayoutedObjects;

    for (RenderObject* child = start->firstChild(); child; child = child->nextSibling()) {
        bool needsLayout = selfNeedsLayout;

        if (layoutSizeChanged) {
            // When selfNeedsLayout is false and the layout size changed, we have to check whether this child uses relative lengths
            if (SVGElement* element = child->node()->isSVGElement() ? static_cast<SVGElement*>(child->node()) : 0) {
                if (element->isStyled() && static_cast<SVGStyledElement*>(element)->hasRelativeLengths()) {
                    // When the layout size changed and when using relative values tell the RenderPath to update its Path object
                    if (child->isRenderPath())
                        toRenderPath(child)->setNeedsPathUpdate();

                    needsLayout = true;
                }
            }
        }

        if (needsLayout) {
            child->setNeedsLayout(true, false);
            child->layout();
        } else {
            if (child->needsLayout())
                child->layout();
            else if (layoutSizeChanged)
                notlayoutedObjects.add(child);
        }

        ASSERT(!child->needsLayout());
    }

    if (!layoutSizeChanged) {
        ASSERT(notlayoutedObjects.isEmpty());
        return;
    }

    // If the layout size changed, invalidate all resources of all children that didn't go through the layout() code path.
    HashSet<RenderObject*>::iterator end = notlayoutedObjects.end();
    for (HashSet<RenderObject*>::iterator it = notlayoutedObjects.begin(); it != end; ++it)
        invalidateResourcesOfChildren(*it);
}

bool SVGRenderSupport::isOverflowHidden(const RenderObject* object)
{
    // SVG doesn't support independent x/y overflow
    ASSERT(object->style()->overflowX() == object->style()->overflowY());

    // OSCROLL is never set for SVG - see CSSStyleSelector::adjustRenderStyle
    ASSERT(object->style()->overflowX() != OSCROLL);

    // RenderSVGRoot should never query for overflow state - it should always clip itself to the initial viewport size.
    ASSERT(!object->isRoot());

    return object->style()->overflowX() == OHIDDEN;
}

void SVGRenderSupport::intersectRepaintRectWithResources(const RenderObject* object, FloatRect& repaintRect)
{
    ASSERT(object);

    RenderStyle* style = object->style();
    ASSERT(style);

    const SVGRenderStyle* svgStyle = style->svgStyle();
    ASSERT(svgStyle);

    RenderObject* renderer = const_cast<RenderObject*>(object);
    SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(renderer);
    if (!resources) {
        svgStyle->inflateForShadow(repaintRect);
        return;
    }

#if ENABLE(FILTERS)
    if (RenderSVGResourceFilter* filter = resources->filter())
        repaintRect = filter->resourceBoundingBox(renderer);
#endif

    if (RenderSVGResourceClipper* clipper = resources->clipper())
        repaintRect.intersect(clipper->resourceBoundingBox(renderer));

    if (RenderSVGResourceMasker* masker = resources->masker())
        repaintRect.intersect(masker->resourceBoundingBox(renderer));

    svgStyle->inflateForShadow(repaintRect);
}

bool SVGRenderSupport::pointInClippingArea(RenderObject* object, const FloatPoint& point)
{
    ASSERT(object);

    // We just take clippers into account to determine if a point is on the node. The Specification may
    // change later and we also need to check maskers.
    SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(object);
    if (!resources)
        return true;

    if (RenderSVGResourceClipper* clipper = resources->clipper())
        return clipper->hitTestClipContent(object->objectBoundingBox(), point);

    return true;
}

void SVGRenderSupport::applyStrokeStyleToContext(GraphicsContext* context, const RenderStyle* style, const RenderObject* object)
{
    ASSERT(context);
    ASSERT(style);
    ASSERT(object);
    ASSERT(object->node());
    ASSERT(object->node()->isSVGElement());

    const SVGRenderStyle* svgStyle = style->svgStyle();
    ASSERT(svgStyle);

    SVGElement* lengthContext = static_cast<SVGElement*>(object->node());
    context->setStrokeThickness(svgStyle->strokeWidth().value(lengthContext));
    context->setLineCap(svgStyle->capStyle());
    context->setLineJoin(svgStyle->joinStyle());
    if (svgStyle->joinStyle() == MiterJoin)
        context->setMiterLimit(svgStyle->strokeMiterLimit());

    const Vector<SVGLength>& dashes = svgStyle->strokeDashArray();
    if (dashes.isEmpty())
        context->setStrokeStyle(SolidStroke);
    else {
        DashArray dashArray;
        const Vector<SVGLength>::const_iterator end = dashes.end();
        for (Vector<SVGLength>::const_iterator it = dashes.begin(); it != end; ++it)
            dashArray.append((*it).value(lengthContext));

        context->setLineDash(dashArray, svgStyle->strokeDashOffset().value(lengthContext));
    }
}

const RenderObject* SVGRenderSupport::findTextRootObject(const RenderObject* start)
{
    while (start && !start->isSVGText())
        start = start->parent();
    ASSERT(start);
    ASSERT(start->isSVGText());

    return start;
}

}

#endif
