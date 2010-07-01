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
#include "RenderSVGContainer.h"
#include "RenderSVGResource.h"
#include "RenderSVGResourceClipper.h"
#include "RenderSVGResourceFilter.h"
#include "RenderSVGResourceMarker.h"
#include "RenderSVGResourceMasker.h"
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
    SVGElement* svgElement = static_cast<SVGElement*>(object->node());
    ASSERT(svgElement && svgElement->document() && svgElement->isStyled());

    SVGStyledElement* styledElement = static_cast<SVGStyledElement*>(svgElement);
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

    Document* document = object->document();

    if (svgStyle->hasMasker()) {
        AtomicString maskerId(svgStyle->maskerResource());
        if (RenderSVGResourceMasker* masker = getRenderSVGResourceById<RenderSVGResourceMasker>(document, maskerId)) {
            if (!masker->applyResource(object, style, paintInfo.context, ApplyToDefaultMode))
                return false;
        } else
            document->accessSVGExtensions()->addPendingResource(maskerId, styledElement);
    }

    if (svgStyle->hasClipper()) {
        AtomicString clipperId(svgStyle->clipperResource());
        if (RenderSVGResourceClipper* clipper = getRenderSVGResourceById<RenderSVGResourceClipper>(document, clipperId))
            clipper->applyResource(object, style, paintInfo.context, ApplyToDefaultMode);
        else
            document->accessSVGExtensions()->addPendingResource(clipperId, styledElement);
    }

#if ENABLE(FILTERS)
    if (svgStyle->hasFilter()) {
        AtomicString filterId(svgStyle->filterResource());
        if (RenderSVGResourceFilter* filter = getRenderSVGResourceById<RenderSVGResourceFilter>(document, filterId)) { 
            if (!filter->applyResource(object, style, paintInfo.context, ApplyToDefaultMode))
                return false;
        } else
            document->accessSVGExtensions()->addPendingResource(filterId, styledElement);
    }
#endif

    return true;
}

void SVGRenderSupport::finishRenderSVGContent(RenderObject* object, PaintInfo& paintInfo, GraphicsContext* savedContext)
{
#if !ENABLE(FILTERS)
    UNUSED_PARAM(filter);
    UNUSED_PARAM(savedContext);
#endif

    ASSERT(object);

    const RenderStyle* style = object->style();
    ASSERT(style);

    const SVGRenderStyle* svgStyle = style->svgStyle();
    ASSERT(svgStyle);

#if ENABLE(FILTERS)
    if (svgStyle->hasFilter()) {
        AtomicString filterId(svgStyle->filterResource());
        if (RenderSVGResourceFilter* filter = getRenderSVGResourceById<RenderSVGResourceFilter>(object->document(), filterId)) { 
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

    // FIXME: This sets the rect to the viewable area of the current frame. This
    // is used to support text drawings to the ImageBuffer. See bug 30399.
    IntRect rect;
    FrameView* frameView = item->document()->view();
    if (frameView)
        rect = IntRect(0, 0, frameView->visibleWidth(), frameView->visibleHeight());
    PaintInfo info(image->context(), rect, PaintPhaseForeground, 0, 0, 0);

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

void SVGRenderSupport::layoutChildren(RenderObject* start, bool selfNeedsLayout)
{
    for (RenderObject* child = start->firstChild(); child; child = child->nextSibling()) {
        // Only force our kids to layout if we're being asked to relayout as a result of a parent changing
        // FIXME: We should be able to skip relayout of non-relative kids when only bounds size has changed
        // that's a possible future optimization using LayoutState
        // http://bugs.webkit.org/show_bug.cgi?id=15391
        bool needsLayout = selfNeedsLayout;
        if (!needsLayout) {
            if (SVGElement* element = child->node()->isSVGElement() ? static_cast<SVGElement*>(child->node()) : 0) {
                if (element->isStyled())
                    needsLayout = static_cast<SVGStyledElement*>(element)->hasRelativeValues();
            }
        }

        if (needsLayout)
            child->setNeedsLayout(true, false);

        child->layoutIfNeeded();
        ASSERT(!child->needsLayout());
    }
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
    ASSERT(object->style());
    const SVGRenderStyle* svgStyle = object->style()->svgStyle();
    if (!svgStyle)
        return;
        
    RenderObject* renderer = const_cast<RenderObject*>(object);
#if ENABLE(FILTERS)
    if (svgStyle->hasFilter()) {
        if (RenderSVGResourceFilter* filter = getRenderSVGResourceById<RenderSVGResourceFilter>(object->document(), svgStyle->filterResource()))
            repaintRect = filter->resourceBoundingBox(renderer);
    }
#endif

    if (svgStyle->hasClipper()) {
        if (RenderSVGResourceClipper* clipper = getRenderSVGResourceById<RenderSVGResourceClipper>(object->document(), svgStyle->clipperResource()))
            repaintRect.intersect(clipper->resourceBoundingBox(renderer));
    }
    
    if (svgStyle->hasMasker()) {
        if (RenderSVGResourceMasker* masker = getRenderSVGResourceById<RenderSVGResourceMasker>(object->document(), svgStyle->maskerResource()))
            repaintRect.intersect(masker->resourceBoundingBox(renderer));
    }
    
    svgStyle->inflateForShadow(repaintRect);
}

bool SVGRenderSupport::pointInClippingArea(const RenderObject* object, const FloatPoint& point)
{
    ASSERT(object);
    ASSERT(object->style());

    Document* document = object->document();
    ASSERT(document);

    const SVGRenderStyle* svgStyle = object->style()->svgStyle();
    ASSERT(svgStyle);

    // We just take clippers into account to determine if a point is on the node. The Specification may
    // change later and we also need to check maskers.
    if (svgStyle->hasClipper()) {
        if (RenderSVGResourceClipper* clipper = getRenderSVGResourceById<RenderSVGResourceClipper>(document, svgStyle->clipperResource()))
            return clipper->hitTestClipContent(object->objectBoundingBox(), point);
    }

    return true;
}

DashArray SVGRenderSupport::dashArrayFromRenderingStyle(const RenderStyle* style, RenderStyle* rootStyle)
{
    DashArray array;
    
    CSSValueList* dashes = style->svgStyle()->strokeDashArray();
    if (!dashes)
        return array;

    CSSPrimitiveValue* dash = 0;
    unsigned long len = dashes->length();
    for (unsigned long i = 0; i < len; ++i) {
        dash = static_cast<CSSPrimitiveValue*>(dashes->itemWithoutBoundsCheck(i));
        if (!dash)
            continue;

        array.append(dash->computeLengthFloat(const_cast<RenderStyle*>(style), rootStyle));
    }

    return array;
}

void SVGRenderSupport::applyStrokeStyleToContext(GraphicsContext* context, const RenderStyle* style, const RenderObject* object)
{
    context->setStrokeThickness(SVGRenderStyle::cssPrimitiveToLength(object, style->svgStyle()->strokeWidth(), 1.0f));
    context->setLineCap(style->svgStyle()->capStyle());
    context->setLineJoin(style->svgStyle()->joinStyle());
    if (style->svgStyle()->joinStyle() == MiterJoin)
        context->setMiterLimit(style->svgStyle()->strokeMiterLimit());

    const DashArray& dashes = dashArrayFromRenderingStyle(object->style(), object->document()->documentElement()->renderStyle());
    float dashOffset = SVGRenderStyle::cssPrimitiveToLength(object, style->svgStyle()->strokeDashOffset(), 0.0f);
    if (dashes.isEmpty())
        context->setStrokeStyle(SolidStroke);
    else
        context->setLineDash(dashes, dashOffset);
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
