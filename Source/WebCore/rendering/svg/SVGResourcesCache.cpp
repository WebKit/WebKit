/*
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
#include "SVGResourcesCache.h"

#if ENABLE(SVG)
#include "HTMLNames.h"
#include "RenderSVGResourceContainer.h"
#include "SVGResources.h"
#include "SVGResourcesCycleSolver.h"

namespace WebCore {

SVGResourcesCache::SVGResourcesCache()
{
}

SVGResourcesCache::~SVGResourcesCache()
{
}

void SVGResourcesCache::addResourcesFromRenderer(RenderElement& renderer, const RenderStyle& style)
{
    ASSERT(!m_cache.contains(&renderer));

    const SVGRenderStyle& svgStyle = style.svgStyle();

    // Build a list of all resources associated with the passed RenderObject
    OwnPtr<SVGResources> newResources = adoptPtr(new SVGResources);
    if (!newResources->buildCachedResources(renderer, svgStyle))
        return;

    // Put object in cache.
    SVGResources& resources = *m_cache.add(&renderer, newResources.release()).iterator->value;

    // Run cycle-detection _afterwards_, so self-references can be caught as well.
    SVGResourcesCycleSolver solver(renderer, resources);
    solver.resolveCycles();

    // Walk resources and register the render object at each resources.
    HashSet<RenderSVGResourceContainer*> resourceSet;
    resources.buildSetOfResources(resourceSet);

    for (auto it = resourceSet.begin(), end = resourceSet.end(); it != end; ++it)
        (*it)->addClient(&renderer);
}

void SVGResourcesCache::removeResourcesFromRenderer(RenderElement& renderer)
{
    OwnPtr<SVGResources> resources = m_cache.take(&renderer);
    if (!resources)
        return;

    // Walk resources and register the render object at each resources.
    HashSet<RenderSVGResourceContainer*> resourceSet;
    resources->buildSetOfResources(resourceSet);

    for (auto it = resourceSet.begin(), end = resourceSet.end(); it != end; ++it)
        (*it)->removeClient(&renderer);
}

static inline SVGResourcesCache& resourcesCacheFromRenderer(const RenderObject& renderer)
{
    SVGDocumentExtensions* extensions = renderer.document().accessSVGExtensions();
    ASSERT(extensions);
    return extensions->resourcesCache();
}

SVGResources* SVGResourcesCache::cachedResourcesForRenderObject(const RenderObject& renderer)
{
    return resourcesCacheFromRenderer(renderer).m_cache.get(&renderer);
}

void SVGResourcesCache::clientLayoutChanged(RenderElement& renderer)
{
    SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(renderer);
    if (!resources)
        return;

    // Invalidate the resources if either the RenderElement itself changed,
    // or we have filter resources, which could depend on the layout of children.
    if (renderer.selfNeedsLayout())
        resources->removeClientFromCache(renderer);
}

static inline bool rendererCanHaveResources(RenderObject& renderer)
{
    return renderer.node() && renderer.node()->isSVGElement() && !renderer.isSVGInlineText();
}

void SVGResourcesCache::clientStyleChanged(RenderElement& renderer, StyleDifference diff, const RenderStyle& newStyle)
{
    if (diff == StyleDifferenceEqual || !renderer.parent())
        return;

    // In this case the proper SVGFE*Element will decide whether the modified CSS properties require a relayout or repaint.
    if (renderer.isSVGResourceFilterPrimitive() && (diff == StyleDifferenceRepaint || diff == StyleDifferenceRepaintIfTextOrBorderOrOutline))
        return;

    // Dynamic changes of CSS properties like 'clip-path' may require us to recompute the associated resources for a renderer.
    // FIXME: Avoid passing in a useless StyleDifference, but instead compare oldStyle/newStyle to see which resources changed
    // to be able to selectively rebuild individual resources, instead of all of them.
    if (rendererCanHaveResources(renderer)) {
        auto& cache = resourcesCacheFromRenderer(renderer);
        cache.removeResourcesFromRenderer(renderer);
        cache.addResourcesFromRenderer(renderer, newStyle);
    }

    RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer, false);

    if (renderer.element() && !renderer.element()->isSVGElement())
        renderer.element()->setNeedsStyleRecalc(SyntheticStyleChange);
}

void SVGResourcesCache::clientWasAddedToTree(RenderObject& renderer)
{
    if (renderer.isAnonymous())
        return;

    RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer, false);

    if (!rendererCanHaveResources(renderer))
        return;
    RenderElement& elementRenderer = toRenderElement(renderer);
    resourcesCacheFromRenderer(elementRenderer).addResourcesFromRenderer(elementRenderer, elementRenderer.style());
}

void SVGResourcesCache::clientWillBeRemovedFromTree(RenderObject& renderer)
{
    if (renderer.isAnonymous())
        return;

    RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer, false);

    if (!rendererCanHaveResources(renderer))
        return;
    RenderElement& elementRenderer = toRenderElement(renderer);
    resourcesCacheFromRenderer(elementRenderer).removeResourcesFromRenderer(elementRenderer);
}

void SVGResourcesCache::clientDestroyed(RenderElement& renderer)
{
    SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(renderer);
    if (resources)
        resources->removeClientFromCache(renderer);

    resourcesCacheFromRenderer(renderer).removeResourcesFromRenderer(renderer);
}

void SVGResourcesCache::resourceDestroyed(RenderSVGResourceContainer& resource)
{
    auto& cache = resourcesCacheFromRenderer(resource);

    // The resource itself may have clients, that need to be notified.
    cache.removeResourcesFromRenderer(resource);

    for (auto& it : cache.m_cache) {
        it.value->resourceDestroyed(resource);

        // Mark users of destroyed resources as pending resolution based on the id of the old resource.
        Element& resourceElement = resource.element();
        Element* clientElement = toElement(it.key->node());
        SVGDocumentExtensions* extensions = clientElement->document().accessSVGExtensions();

        extensions->addPendingResource(resourceElement.getIdAttribute(), clientElement);
    }
}

}

#endif
