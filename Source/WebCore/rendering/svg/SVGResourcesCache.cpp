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

#include "RenderSVGResourceContainer.h"
#include "SVGResources.h"
#include "SVGResourcesCycleSolver.h"

namespace WebCore {

SVGResourcesCache::SVGResourcesCache() = default;

SVGResourcesCache::~SVGResourcesCache() = default;

void SVGResourcesCache::addResourcesFromRenderer(RenderElement& renderer, const RenderStyle& style)
{
    ASSERT(!m_cache.contains(&renderer));

    // Build a list of all resources associated with the passed RenderObject
    auto newResources = makeUnique<SVGResources>();
    if (!newResources->buildCachedResources(renderer, style))
        return;

    // Put object in cache.
    SVGResources& resources = *m_cache.add(&renderer, WTFMove(newResources)).iterator->value;

    // Run cycle-detection _afterwards_, so self-references can be caught as well.
    SVGResourcesCycleSolver solver(renderer, resources);
    solver.resolveCycles();

    // Walk resources and register the render object at each resources.
    HashSet<RenderSVGResourceContainer*> resourceSet;
    resources.buildSetOfResources(resourceSet);

    for (auto* resourceContainer : resourceSet)
        resourceContainer->addClient(renderer);
}

void SVGResourcesCache::removeResourcesFromRenderer(RenderElement& renderer)
{
    std::unique_ptr<SVGResources> resources = m_cache.take(&renderer);
    if (!resources)
        return;

    // Walk resources and register the render object at each resources.
    HashSet<RenderSVGResourceContainer*> resourceSet;
    resources->buildSetOfResources(resourceSet);

    for (auto* resourceContainer : resourceSet)
        resourceContainer->removeClient(renderer);
}

static inline SVGResourcesCache& resourcesCacheFromRenderer(const RenderElement& renderer)
{
    return renderer.document().accessSVGExtensions().resourcesCache();
}

SVGResources* SVGResourcesCache::cachedResourcesForRenderer(const RenderElement& renderer)
{
    return resourcesCacheFromRenderer(renderer).m_cache.get(&renderer);
}

void SVGResourcesCache::clientLayoutChanged(RenderElement& renderer)
{
    auto* resources = SVGResourcesCache::cachedResourcesForRenderer(renderer);
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
    if (diff == StyleDifference::Equal || !renderer.parent())
        return;

    // In this case the proper SVGFE*Element will decide whether the modified CSS properties require a relayout or repaint.
    if (renderer.isSVGResourceFilterPrimitive() && (diff == StyleDifference::Repaint || diff == StyleDifference::RepaintIfTextOrBorderOrOutline))
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
        renderer.element()->invalidateStyle();
}

void SVGResourcesCache::clientWasAddedToTree(RenderObject& renderer)
{
    if (renderer.isAnonymous())
        return;

    RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer, false);

    if (!rendererCanHaveResources(renderer))
        return;
    RenderElement& elementRenderer = downcast<RenderElement>(renderer);
    resourcesCacheFromRenderer(elementRenderer).addResourcesFromRenderer(elementRenderer, elementRenderer.style());
}

void SVGResourcesCache::clientWillBeRemovedFromTree(RenderObject& renderer)
{
    if (renderer.isAnonymous())
        return;

    RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer, false);

    if (!rendererCanHaveResources(renderer))
        return;
    RenderElement& elementRenderer = downcast<RenderElement>(renderer);
    resourcesCacheFromRenderer(elementRenderer).removeResourcesFromRenderer(elementRenderer);
}

void SVGResourcesCache::clientDestroyed(RenderElement& renderer)
{
    if (auto* resources = SVGResourcesCache::cachedResourcesForRenderer(renderer))
        resources->removeClientFromCache(renderer);

    resourcesCacheFromRenderer(renderer).removeResourcesFromRenderer(renderer);
}

void SVGResourcesCache::resourceDestroyed(RenderSVGResourceContainer& resource)
{
    auto& cache = resourcesCacheFromRenderer(resource);

    // The resource itself may have clients, that need to be notified.
    cache.removeResourcesFromRenderer(resource);

    for (auto& it : cache.m_cache) {
        if (it.value->resourceDestroyed(resource)) {
            // Mark users of destroyed resources as pending resolution based on the id of the old resource.
            auto& clientElement = *it.key->element();
            clientElement.document().accessSVGExtensions().addPendingResource(resource.element().getIdAttribute(), clientElement);
        }
    }
}

}
