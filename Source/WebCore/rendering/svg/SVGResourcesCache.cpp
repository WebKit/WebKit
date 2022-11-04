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

#include "ElementInlines.h"
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

static bool hasPaintResourceRequiringRemovalOnClientLayoutChange(RenderSVGResource* resource)
{
    return resource && resource->resourceType() == PatternResourceType;
}

static bool hasResourcesRequiringRemovalOnClientLayoutChange(SVGResources& resources)
{
    return resources.masker() || resources.filter() || hasPaintResourceRequiringRemovalOnClientLayoutChange(resources.fill()) || hasPaintResourceRequiringRemovalOnClientLayoutChange(resources.stroke());
}

void SVGResourcesCache::clientLayoutChanged(RenderElement& renderer)
{
    auto* resources = SVGResourcesCache::cachedResourcesForRenderer(renderer);
    if (!resources)
        return;

    // Invalidate the resources if either the RenderElement itself changed,
    // or we have filter resources, which could depend on the layout of children.
    if (renderer.selfNeedsLayout() && hasResourcesRequiringRemovalOnClientLayoutChange(*resources))
        resources->removeClientFromCache(renderer, false);
}

static inline bool rendererCanHaveResources(RenderObject& renderer)
{
    return renderer.node() && renderer.node()->isSVGElement() && !renderer.isSVGInlineText();
}

void SVGResourcesCache::clientStyleChanged(RenderElement& renderer, StyleDifference diff, const RenderStyle* oldStyle, const RenderStyle& newStyle)
{
    if (!renderer.parent())
        return;

    // For filter primitives, when diff is Repaint or RepaintIsText, the
    // SVGFE*Element will decide whether the modified CSS properties require a
    // relayout or repaint.
    //
    // Since diff can be Equal even if we have have a filter property change
    // (due to how RenderElement::adjustStyleDifference works), in general we
    // want to continue to the comparison of oldStyle and newStyle below, and
    // so we don't return early just when diff == StyleDifference::Equal. But
    // this isn't necessary for filter primitives, to which the filter property
    // doesn't apply, so we check for it here too.
    if (renderer.isSVGResourceFilterPrimitive() && (diff == StyleDifference::Equal || diff == StyleDifference::Repaint || diff == StyleDifference::RepaintIfText))
        return;

    auto& cache = resourcesCacheFromRenderer(renderer);

    auto hasStyleDifferencesAffectingResources = [&] {
        if (!rendererCanHaveResources(renderer))
            return false;

        if (!oldStyle)
            return true;

        if (!arePointingToEqualData(oldStyle->clipPath(), newStyle.clipPath()))
            return true;

        // RenderSVGResourceMarker only supports SVG <mask> references.
        if (!arePointingToEqualData(oldStyle->maskImage(), newStyle.maskImage()))
            return true;

        if (oldStyle->filter() != newStyle.filter())
            return true;

        // -apple-color-filter affects gradients.
        if (oldStyle->appleColorFilter() != newStyle.appleColorFilter())
            return true;

        auto& oldSVGStyle = oldStyle->svgStyle();
        auto& newSVGStyle = newStyle.svgStyle();

        if (oldSVGStyle.fillPaintUri() != newSVGStyle.fillPaintUri())
            return true;

        if (oldSVGStyle.strokePaintUri() != newSVGStyle.strokePaintUri())
            return true;

        return false;
    };

    if (hasStyleDifferencesAffectingResources()) {
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
            RELEASE_ASSERT(is<SVGElement>(clientElement));
            clientElement.document().accessSVGExtensions().addPendingResource(resource.element().getIdAttribute(), downcast<SVGElement>(clientElement));
        }
    }
}

SVGResourcesCache::SetStyleForScope::SetStyleForScope(RenderElement& renderer, const RenderStyle& scopedStyle, const RenderStyle& newStyle)
    : m_renderer(renderer)
    , m_scopedStyle(scopedStyle)
    , m_needsNewStyle(scopedStyle != newStyle && rendererCanHaveResources(renderer))
{
    setStyle(newStyle);
}

SVGResourcesCache::SetStyleForScope::~SetStyleForScope()
{
    setStyle(m_scopedStyle);
}

void SVGResourcesCache::SetStyleForScope::setStyle(const RenderStyle& style)
{
    if (!m_needsNewStyle)
        return;

    auto& cache = resourcesCacheFromRenderer(m_renderer);
    cache.removeResourcesFromRenderer(m_renderer);
    cache.addResourcesFromRenderer(m_renderer, style);
}

}
