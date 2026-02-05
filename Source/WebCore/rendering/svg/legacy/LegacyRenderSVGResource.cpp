/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2023-2026 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
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
#include "LegacyRenderSVGResource.h"

#include "ContainerNodeInlines.h"
#include "LegacyRenderSVGResourceClipper.h"
#include "LegacyRenderSVGResourceFilter.h"
#include "LegacyRenderSVGResourceMasker.h"
#include "LegacyRenderSVGResourceSolidColor.h"
#include "LegacyRenderSVGRoot.h"
#include "LegacyRenderSVGShape.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "RenderElementInlines.h"
#include "RenderObjectInlines.h"
#include "RenderSVGRoot.h"
#include "RenderSVGShape.h"
#include "RenderView.h"
#include "SVGResourceElementClient.h"
#include "SVGResources.h"
#include "SVGResourcesCache.h"
#include "SVGURIReference.h"
#include "Settings.h"
#include "StyleComputedStyle+InitialInlines.h"

namespace WebCore {

static inline LegacyRenderSVGResource* requestPaintingResource(RenderSVGResourceMode mode, RenderElement& renderer, const RenderStyle& style, Color& fallbackColor)
{
    bool applyToFill = mode == RenderSVGResourceMode::ApplyToFill;

    // When rendering the mask for a LegacyRenderSVGResourceClipper, always use the initial fill paint server.
    if (renderer.view().frameView().paintBehavior().contains(PaintBehavior::RenderingSVGClipOrMask)) {
        // Ignore stroke.
        if (!applyToFill)
            return nullptr;
        
        // But always use the initial fill paint server.
        LegacyRenderSVGResourceSolidColor* colorResource = LegacyRenderSVGResource::sharedSolidPaintingResource();
        colorResource->setColor(Style::ComputedStyle::initialFill().colorDisregardingType().resolvedColor());
        return colorResource;
    }

    const auto& paint = applyToFill ? style.fill() : style.stroke();

    // If we have no fill/stroke, return nullptr.
    if (paint.isNone())
        return nullptr;

    Style::ColorResolver colorResolver { style };

    Color color;
    if (auto paintColor = paint.tryAnyColor())
        color = colorResolver.colorResolvingCurrentColor(*paintColor);

    if (style.insideLink() == InsideLink::InsideVisited) {
        // FIXME: This code doesn't support the uri component of the visited link paint, https://bugs.webkit.org/show_bug.cgi?id=70006
        auto& visitedPaint = applyToFill ? style.visitedLinkFill() : style.visitedLinkStroke();

        if (auto visitedPaintColor = visitedPaint.tryColor()) {
            if (visitedPaintColor->isCurrentColor())
                color = style.visitedLinkColor();
            else if (auto visitedColor = colorResolver.colorResolvingCurrentColor(*visitedPaintColor); visitedColor.isValid())
                color = visitedColor.colorWithAlpha(color.alphaAsFloat());
        }
    }

    // If the primary resource is just a color, return immediately.
    auto* colorResource = LegacyRenderSVGResource::sharedSolidPaintingResource();
    if (paint.isColor()) {
        colorResource->setColor(color);
        return colorResource;
    }

    // FIXME: [LBSE] Add support for non-solid color resources in LBSE (gradient/pattern).
    SVGResources* resources = nullptr;
    if (!renderer.document().settings().layerBasedSVGEngineEnabled())
        resources = SVGResourcesCache::cachedResourcesForRenderer(renderer);

    LegacyRenderSVGResource* uriResource = nullptr;
    if (resources)
        uriResource = mode == RenderSVGResourceMode::ApplyToFill ? resources->fill() : resources->stroke();

    // If the requested resource is not available, return the color resource or 'none'.
    if (!uriResource) {
        // The fallback is 'none'. (SVG2 say 'none' is implied when no fallback is specified.)
        if (paint.isURLNone())
            return nullptr;

        colorResource->setColor(color);
        return colorResource;
    }

    // The paint server resource exists, though it may be invalid (pattern with width/height=0). Pass the fallback color to our caller
    // so it can use the solid color painting resource, if applyResource() on the URI resource failed.
    fallbackColor = color;
    return uriResource;
}

void LegacyRenderSVGResource::removeAllClientsFromCacheAndMarkForInvalidation(bool markForInvalidation)
{
    SingleThreadWeakHashSet<RenderObject> visitedRenderers;
    removeAllClientsFromCacheAndMarkForInvalidationIfNeeded(markForInvalidation, &visitedRenderers);
}

LegacyRenderSVGResource* LegacyRenderSVGResource::fillPaintingResource(RenderElement& renderer, const RenderStyle& style, Color& fallbackColor)
{
    return requestPaintingResource(RenderSVGResourceMode::ApplyToFill, renderer, style, fallbackColor);
}

LegacyRenderSVGResource* LegacyRenderSVGResource::strokePaintingResource(RenderElement& renderer, const RenderStyle& style, Color& fallbackColor)
{
    return requestPaintingResource(RenderSVGResourceMode::ApplyToStroke, renderer, style, fallbackColor);
}

LegacyRenderSVGResourceSolidColor* LegacyRenderSVGResource::sharedSolidPaintingResource()
{
    static LegacyRenderSVGResourceSolidColor* s_sharedSolidPaintingResource = 0;
    if (!s_sharedSolidPaintingResource)
        s_sharedSolidPaintingResource = new LegacyRenderSVGResourceSolidColor;
    return s_sharedSolidPaintingResource;
}

static void removeFromCacheAndInvalidateDependencies(RenderElement& renderer, bool needsLayout, SingleThreadWeakHashSet<RenderObject>* visitedRenderers)
{
    if (auto* resources = SVGResourcesCache::cachedResourcesForRenderer(renderer)) {
        if (LegacyRenderSVGResourceFilter* filter = resources->filter())
            filter->removeClientFromCacheAndMarkForInvalidation(renderer);

        if (LegacyRenderSVGResourceMasker* masker = resources->masker())
            masker->removeClientFromCacheAndMarkForInvalidation(renderer);

        if (LegacyRenderSVGResourceClipper* clipper = resources->clipper())
            clipper->removeClientFromCacheAndMarkForInvalidation(renderer);
    }

    auto svgElement = dynamicDowncast<SVGElement>(renderer.protectedElement());
    if (!svgElement)
        return;

    for (auto& element : svgElement->referencingElements()) {
        if (auto* renderer = element->renderer()) {
            // We allow cycles in SVGDocumentExtensions reference sets in order to avoid expensive
            // reference graph adjustments on changes, so we need to break possible cycles here.
            static NeverDestroyed<WeakHashSet<SVGElement, WeakPtrImplWithEventTargetData>> invalidatingDependencies;
            if (!invalidatingDependencies.get().add(element.get()).isNewEntry) [[unlikely]] {
                // Reference cycle: we are in process of invalidating this dependant.
                continue;
            }
            LegacyRenderSVGResource::markForLayoutAndParentResourceInvalidationIfNeeded(*renderer, needsLayout, visitedRenderers);
            invalidatingDependencies.get().remove(element.get());
        }
    }

    for (auto& cssClient : svgElement->referencingCSSClients()) {
        if (!cssClient)
            continue;
        cssClient->resourceChanged(*svgElement);
    }
}

void LegacyRenderSVGResource::markForLayoutAndParentResourceInvalidation(RenderObject& object, bool needsLayout)
{
    SingleThreadWeakHashSet<RenderObject> visitedRenderers;
    markForLayoutAndParentResourceInvalidationIfNeeded(object, needsLayout, &visitedRenderers);
}

void LegacyRenderSVGResource::markForLayoutAndParentResourceInvalidationIfNeeded(RenderObject& object, bool needsLayout, SingleThreadWeakHashSet<RenderObject>* visitedRenderers)
{
    ASSERT(object.node());
    if (object.document().settings().layerBasedSVGEngineEnabled()) {
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }

    if (visitedRenderers) {
        auto addResult = visitedRenderers->add(object);
        if (!addResult.isNewEntry)
            return;
    }

    if (needsLayout && !object.renderTreeBeingDestroyed()) {
        // If we are inside the layout of an LegacyRenderSVGRoot, do not cross the SVG boundary to
        // invalidate the ancestor renderer because it may have finished its layout already.
        if (CheckedPtr svgRoot = dynamicDowncast<LegacyRenderSVGRoot>(object); svgRoot && svgRoot->isInLayout())
            svgRoot->setNeedsLayout(MarkOnlyThis);
        else {
            if (CheckedPtr element = dynamicDowncast<RenderElement>(object)) {
                auto svgRoot = SVGRenderSupport::findTreeRootObject(*element);
                if (!svgRoot || !svgRoot->isInLayout())
                    element->setNeedsLayout(MarkContainingBlockChain);
                else {
                    // We just want to re-layout the ancestors up to the RenderSVGRoot.
                    element->setNeedsLayout(MarkOnlyThis);
                    for (auto current = element->parent(); current != svgRoot; current = current->parent())
                        current->setNeedsLayout(MarkOnlyThis);
                    svgRoot->setNeedsLayout(MarkOnlyThis);
                }
            } else
                object.setNeedsLayout(MarkOnlyThis);
        }
    }

    if (CheckedPtr element = dynamicDowncast<RenderElement>(object))
        removeFromCacheAndInvalidateDependencies(*element, needsLayout, visitedRenderers);

    // Invalidate resources in ancestor chain, if needed.
    auto current = object.parent();
    while (current) {
        removeFromCacheAndInvalidateDependencies(*current, needsLayout, visitedRenderers);

        if (CheckedPtr container = dynamicDowncast<LegacyRenderSVGResourceContainer>(*current)) {
            // This will process the rest of the ancestors.
            bool markForInvalidation = true;
            container->removeAllClientsFromCacheAndMarkForInvalidationIfNeeded(markForInvalidation, visitedRenderers);
            break;
        }

        current = current->parent();
    }
}

void LegacyRenderSVGResource::fillAndStrokePathOrShape(GraphicsContext& context, OptionSet<RenderSVGResourceMode> resourceMode, const Path* path, const RenderElement* shape)
{
    if (shape) {
        ASSERT(shape->isRenderOrLegacyRenderSVGShape());

        if (resourceMode.contains(RenderSVGResourceMode::ApplyToFill)) {
            if (CheckedPtr svgShape = dynamicDowncast<LegacyRenderSVGShape>(shape))
                svgShape->fillShape(context);
            else if (CheckedPtr svgShape = dynamicDowncast<RenderSVGShape>(shape))
                svgShape->fillShape(context);
        }

        if (resourceMode.contains(RenderSVGResourceMode::ApplyToStroke)) {
            if (CheckedPtr svgShape = dynamicDowncast<LegacyRenderSVGShape>(shape))
                svgShape->strokeShape(context);
            else if (CheckedPtr svgShape = dynamicDowncast<RenderSVGShape>(shape))
                svgShape->strokeShape(context);
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
