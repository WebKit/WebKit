/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
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
 */

#include "config.h"
#include "RenderSVGResource.h"

#include "Frame.h"
#include "FrameView.h"
#include "LegacyRenderSVGRoot.h"
#include "LegacyRenderSVGShape.h"
#include "RenderSVGResourceClipper.h"
#include "RenderSVGResourceFilter.h"
#include "RenderSVGResourceMasker.h"
#include "RenderSVGResourceSolidColor.h"
#include "RenderSVGRoot.h"
#include "RenderSVGShape.h"
#include "RenderView.h"
#include "SVGResourceElementClient.h"
#include "SVGResources.h"
#include "SVGResourcesCache.h"
#include "SVGURIReference.h"

namespace WebCore {

static inline bool inheritColorFromParentStyleIfNeeded(RenderElement& object, bool applyToFill, Color& color)
{
    if (color.isValid())
        return true;
    if (!object.parent())
        return false;
    const SVGRenderStyle& parentSVGStyle = object.parent()->style().svgStyle();
    color = object.style().colorResolvingCurrentColor(applyToFill ? parentSVGStyle.fillPaintColor() : parentSVGStyle.strokePaintColor());
    return true;
}

static inline RenderSVGResource* requestPaintingResource(RenderSVGResourceMode mode, RenderElement& renderer, const RenderStyle& style, Color& fallbackColor)
{
    const SVGRenderStyle& svgStyle = style.svgStyle();

    bool isRenderingMask = renderer.view().frameView().paintBehavior().contains(PaintBehavior::RenderingSVGMask);

    // If we have no fill/stroke, return nullptr.
    if (mode == RenderSVGResourceMode::ApplyToFill) {
        // When rendering the mask for a RenderSVGResourceClipper, always use the initial fill paint server, and ignore stroke.
        if (isRenderingMask) {
            RenderSVGResourceSolidColor* colorResource = RenderSVGResource::sharedSolidPaintingResource();
            colorResource->setColor(SVGRenderStyle::initialFillPaintColor().absoluteColor());
            return colorResource;
        }

        if (!svgStyle.hasFill())
            return nullptr;
    } else {
        if (!svgStyle.hasStroke() || isRenderingMask)
            return nullptr;
    }

    bool applyToFill = mode == RenderSVGResourceMode::ApplyToFill;
    SVGPaintType paintType = applyToFill ? svgStyle.fillPaintType() : svgStyle.strokePaintType();
    if (paintType == SVGPaintType::None)
        return nullptr;

    Color color;
    switch (paintType) {
    case SVGPaintType::CurrentColor:
    case SVGPaintType::RGBColor:
    case SVGPaintType::URICurrentColor:
    case SVGPaintType::URIRGBColor:
        color = style.colorResolvingCurrentColor(applyToFill ? svgStyle.fillPaintColor() : svgStyle.strokePaintColor());
        break;
    default:
        break;
    }

    if (style.insideLink() == InsideLink::InsideVisited) {
        // FIXME: This code doesn't support the uri component of the visited link paint, https://bugs.webkit.org/show_bug.cgi?id=70006
        SVGPaintType visitedPaintType = applyToFill ? svgStyle.visitedLinkFillPaintType() : svgStyle.visitedLinkStrokePaintType();

        // For SVGPaintType::CurrentColor, 'color' already contains the 'visitedColor'.
        if (visitedPaintType < SVGPaintType::URINone && visitedPaintType != SVGPaintType::CurrentColor) {
            const Color& visitedColor = style.colorResolvingCurrentColor(applyToFill ? svgStyle.visitedLinkFillPaintColor() : svgStyle.visitedLinkStrokePaintColor());
            if (visitedColor.isValid())
                color = visitedColor.colorWithAlpha(color.alphaAsFloat());
        }
    }

    // If the primary resource is just a color, return immediately.
    RenderSVGResourceSolidColor* colorResource = RenderSVGResource::sharedSolidPaintingResource();
    if (paintType < SVGPaintType::URINone) {
        if (!inheritColorFromParentStyleIfNeeded(renderer, applyToFill, color))
            return nullptr;

        colorResource->setColor(color);
        return colorResource;
    }

    // If no resources are associated with the given renderer, return the color resource.
    auto* resources = SVGResourcesCache::cachedResourcesForRenderer(renderer);
    if (!resources) {
        if (paintType == SVGPaintType::URINone || !inheritColorFromParentStyleIfNeeded(renderer, applyToFill, color))
            return nullptr;

        colorResource->setColor(color);
        return colorResource;
    }

    // If the requested resource is not available, return the color resource.
    RenderSVGResource* uriResource = mode == RenderSVGResourceMode::ApplyToFill ? resources->fill() : resources->stroke();
    if (!uriResource) {
        if (!inheritColorFromParentStyleIfNeeded(renderer, applyToFill, color))
            return nullptr;

        colorResource->setColor(color);
        return colorResource;
    }

    // The paint server resource exists, though it may be invalid (pattern with width/height=0). Pass the fallback color to our caller
    // so it can use the solid color painting resource, if applyResource() on the URI resource failed.
    fallbackColor = color;
    return uriResource;
}

RenderSVGResource* RenderSVGResource::fillPaintingResource(RenderElement& renderer, const RenderStyle& style, Color& fallbackColor)
{
    return requestPaintingResource(RenderSVGResourceMode::ApplyToFill, renderer, style, fallbackColor);
}

RenderSVGResource* RenderSVGResource::strokePaintingResource(RenderElement& renderer, const RenderStyle& style, Color& fallbackColor)
{
    return requestPaintingResource(RenderSVGResourceMode::ApplyToStroke, renderer, style, fallbackColor);
}

RenderSVGResourceSolidColor* RenderSVGResource::sharedSolidPaintingResource()
{
    static RenderSVGResourceSolidColor* s_sharedSolidPaintingResource = 0;
    if (!s_sharedSolidPaintingResource)
        s_sharedSolidPaintingResource = new RenderSVGResourceSolidColor;
    return s_sharedSolidPaintingResource;
}

static void removeFromCacheAndInvalidateDependencies(RenderElement& renderer, bool needsLayout)
{
    if (auto* resources = SVGResourcesCache::cachedResourcesForRenderer(renderer)) {
        if (RenderSVGResourceFilter* filter = resources->filter())
            filter->removeClientFromCache(renderer);

        if (RenderSVGResourceMasker* masker = resources->masker())
            masker->removeClientFromCache(renderer);

        if (RenderSVGResourceClipper* clipper = resources->clipper())
            clipper->removeClientFromCache(renderer);
    }

    if (!is<SVGElement>(renderer.element()))
        return;

    Ref svgElement = downcast<SVGElement>(*renderer.element());

    for (auto& element : svgElement->referencingElements()) {
        if (auto* renderer = element->renderer()) {
            // We allow cycles in SVGDocumentExtensions reference sets in order to avoid expensive
            // reference graph adjustments on changes, so we need to break possible cycles here.
            static NeverDestroyed<WeakHashSet<SVGElement, WeakPtrImplWithEventTargetData>> invalidatingDependencies;
            if (UNLIKELY(!invalidatingDependencies.get().add(element.get()).isNewEntry)) {
                // Reference cycle: we are in process of invalidating this dependant.
                continue;
            }
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer, needsLayout);
            invalidatingDependencies.get().remove(element.get());
        }
    }

    for (auto& cssClient : svgElement->referencingCSSClients()) {
        if (!cssClient)
            continue;
        cssClient->resourceChanged(svgElement.get());
    }
}

void RenderSVGResource::markForLayoutAndParentResourceInvalidation(RenderObject& object, bool needsLayout)
{
    ASSERT(object.node());

    if (needsLayout && !object.renderTreeBeingDestroyed()) {
        // If we are inside the layout of an LegacyRenderSVGRoot, do not cross the SVG boundary to
        // invalidate the ancestor renderer because it may have finished its layout already.
        if (is<LegacyRenderSVGRoot>(object) && downcast<LegacyRenderSVGRoot>(object).isInLayout())
            object.setNeedsLayout(MarkOnlyThis);
#if ENABLE(LAYER_BASED_SVG_ENGINE)
        else if (is<RenderSVGRoot>(object) && downcast<RenderSVGRoot>(object).isInLayout())
            object.setNeedsLayout(MarkOnlyThis);
#endif
        else {
            if (!is<RenderElement>(object))
                object.setNeedsLayout(MarkOnlyThis);
            else {
                auto svgRoot = SVGRenderSupport::findTreeRootObject(downcast<RenderElement>(object));
                if (!svgRoot || !svgRoot->isInLayout())
                    object.setNeedsLayout(MarkContainingBlockChain);
                else {
                    // We just want to re-layout the ancestors up to the RenderSVGRoot.
                    object.setNeedsLayout(MarkOnlyThis);
                    for (auto current = object.parent(); current != svgRoot; current = current->parent())
                        current->setNeedsLayout(MarkOnlyThis);
                    svgRoot->setNeedsLayout(MarkOnlyThis);
                }
            }
        }
    }

    if (is<RenderElement>(object))
        removeFromCacheAndInvalidateDependencies(downcast<RenderElement>(object), needsLayout);

    // Invalidate resources in ancestor chain, if needed.
    auto current = object.parent();
    while (current) {
        removeFromCacheAndInvalidateDependencies(*current, needsLayout);

        if (is<RenderSVGResourceContainer>(*current)) {
            // This will process the rest of the ancestors.
            downcast<RenderSVGResourceContainer>(*current).removeAllClientsFromCache();
            break;
        }

        current = current->parent();
    }
}

void RenderSVGResource::fillAndStrokePathOrShape(GraphicsContext& context, OptionSet<RenderSVGResourceMode> resourceMode, const Path* path, const RenderElement* shape) const
{
    if (shape) {
        ASSERT(shape->isSVGShapeOrLegacySVGShape());

        if (resourceMode.contains(RenderSVGResourceMode::ApplyToFill)) {
            if (is<LegacyRenderSVGShape>(shape))
                downcast<LegacyRenderSVGShape>(shape)->fillShape(context);
#if ENABLE(LAYER_BASED_SVG_ENGINE)
            else if (is<RenderSVGShape>(shape))
                downcast<RenderSVGShape>(shape)->fillShape(context);
#endif
        }

        if (resourceMode.contains(RenderSVGResourceMode::ApplyToStroke)) {
            if (is<LegacyRenderSVGShape>(shape))
                downcast<LegacyRenderSVGShape>(shape)->strokeShape(context);
#if ENABLE(LAYER_BASED_SVG_ENGINE)
            else if (is<RenderSVGShape>(shape))
                downcast<RenderSVGShape>(shape)->strokeShape(context);
#endif
        }

        return;
    }

    if (!path)
        return;

    if (resourceMode.contains(RenderSVGResourceMode::ApplyToFill))
        context.fillPath(*path);
    if (resourceMode.contains(RenderSVGResourceMode::ApplyToStroke))
        context.strokePath(*path);
}

}
