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
#include "SVGResourcesCycleSolver.h"

#include "Logging.h"
#include "RenderAncestorIterator.h"
#include "RenderSVGResourceClipper.h"
#include "RenderSVGResourceFilter.h"
#include "RenderSVGResourceMarker.h"
#include "RenderSVGResourceMasker.h"
#include "SVGGradientElement.h"
#include "SVGPatternElement.h"
#include "SVGResources.h"
#include "SVGResourcesCache.h"

// Set to truthy value to debug the resource cache.
#define DEBUG_CYCLE_DETECTION 0

#if DEBUG_CYCLE_DETECTION
#define LOG_DEBUG_CYCLE(...) LOG(SVG, __VA_ARGS__)
#else
#define LOG_DEBUG_CYCLE(...) ((void)0)
#endif

namespace WebCore {

SVGResourcesCycleSolver::SVGResourcesCycleSolver(RenderElement& renderer, SVGResources& resources)
    : m_renderer(renderer)
    , m_resources(resources)
{
}

SVGResourcesCycleSolver::~SVGResourcesCycleSolver() = default;

bool SVGResourcesCycleSolver::resourceContainsCycles(RenderElement& renderer) const
{
    LOG_DEBUG_CYCLE("\n(%p) Check for cycles\n", &renderer);

    // First operate on the resources of the given renderer.
    // <marker id="a"> <path marker-start="url(#b)"/> ...
    // <marker id="b" marker-start="url(#a)"/>
    if (auto* resources = SVGResourcesCache::cachedResourcesForRenderer(renderer)) {
        HashSet<RenderSVGResourceContainer*> resourceSet;
        resources->buildSetOfResources(resourceSet);

        LOG_DEBUG_CYCLE("(%p) Examine our cached resources\n", &renderer);

        // Walk all resources and check whether they reference any resource contained in the resources set.
        for (auto* resource : resourceSet) {
            LOG_DEBUG_CYCLE("(%p) Check %p\n", &renderer, resource);
            if (m_allResources.contains(resource))
                return true;

            // Now check if the resources themselves contain cycles.
            if (resourceContainsCycles(*resource))
                return true;
        }
    }

    LOG_DEBUG_CYCLE("(%p) Now the children renderers\n", &renderer);

    // Then operate on the child resources of the given renderer.
    // <marker id="a"> <path marker-start="url(#b)"/> ...
    // <marker id="b"> <path marker-start="url(#a)"/> ...
    for (auto& child : childrenOfType<RenderElement>(renderer)) {

        LOG_DEBUG_CYCLE("(%p) Checking child %p\n", &renderer, &child);

        if (auto* childResources = SVGResourcesCache::cachedResourcesForRenderer(child)) {

            LOG_DEBUG_CYCLE("(%p) Child %p had cached resources. Check them.\n", &renderer, &child);

            // A child of the given 'resource' contains resources.
            HashSet<RenderSVGResourceContainer*> childResourceSet;
            childResources->buildSetOfResources(childResourceSet);

            // Walk all child resources and check whether they reference any resource contained in the resources set.
            for (auto* resource : childResourceSet) {
                LOG_DEBUG_CYCLE("(%p) Child %p had resource %p\n", &renderer, &child, resource);
                if (m_allResources.contains(resource))
                    return true;
            }
        }

        LOG_DEBUG_CYCLE("(%p) Recurse into child %p\n", &renderer, &child);

        // Walk children recursively, stop immediately if we found a cycle
        if (resourceContainsCycles(child))
            return true;

        LOG_DEBUG_CYCLE("\n(%p) Child %p was ok\n", &renderer, &child);
    }

    LOG_DEBUG_CYCLE("\n(%p) No cycles found\n", &renderer);

    return false;
}

void SVGResourcesCycleSolver::resolveCycles()
{
    ASSERT(m_allResources.isEmpty());

#if DEBUG_CYCLE_DETECTION
    LOG_DEBUG_CYCLE("\nBefore cycle detection:\n");
    m_resources.dump(&m_renderer);
#endif

    // Stash all resources into a HashSet for the ease of traversing.
    HashSet<RenderSVGResourceContainer*> localResources;
    m_resources.buildSetOfResources(localResources);
    ASSERT(!localResources.isEmpty());

    // Add all parent resource containers to the HashSet.
    HashSet<RenderSVGResourceContainer*> ancestorResources;
    for (auto& resource : ancestorsOfType<RenderSVGResourceContainer>(m_renderer))
        ancestorResources.add(&resource);

#if DEBUG_CYCLE_DETECTION
    LOG_DEBUG_CYCLE("\nDetecting whether any resources references any of following objects:\n");
    {
        LOG_DEBUG_CYCLE("Local resources:\n");
        for (RenderObject* resource : localResources)
            LOG_DEBUG_CYCLE("|> %s : %p (node %p)\n", resource->renderName(), resource, resource->node());

        fprintf(stderr, "Parent resources:\n");
        for (RenderObject* resource : ancestorResources)
            LOG_DEBUG_CYCLE("|> %s : %p (node %p)\n", resource->renderName(), resource, resource->node());
    }
#endif

    // Build combined set of local and parent resources.
    m_allResources = localResources;
    for (auto* resource : ancestorResources)
        m_allResources.add(resource);

    // If we're a resource, add ourselves to the HashSet.
    if (is<RenderSVGResourceContainer>(m_renderer))
        m_allResources.add(&downcast<RenderSVGResourceContainer>(m_renderer));

    ASSERT(!m_allResources.isEmpty());

#if DEBUG_CYCLE_DETECTION
    LOG_DEBUG_CYCLE("\nAll resources:\n");
    for (auto* resource : m_allResources)
        LOG_DEBUG_CYCLE("- %p\n", resource);
#endif

    // The job of this function is to determine wheter any of the 'resources' associated with the given 'renderer'
    // references us (or whether any of its kids references us) -> that's a cycle, we need to find and break it.
    for (auto* resource : localResources) {
        if (ancestorResources.contains(resource) || resourceContainsCycles(*resource)) {
            LOG_DEBUG_CYCLE("\n**** Detected a cycle (see the last test in the output above) ****\n");
            breakCycle(*resource);
        }
    }

#if DEBUG_CYCLE_DETECTION
    LOG_DEBUG_CYCLE("\nAfter cycle detection:\n");
    m_resources.dump(&m_renderer);
#endif

    m_allResources.clear();
}

void SVGResourcesCycleSolver::breakCycle(RenderSVGResourceContainer& resourceLeadingToCycle)
{
    if (&resourceLeadingToCycle == m_resources.linkedResource()) {
        m_resources.resetLinkedResource();
        return;
    }

    switch (resourceLeadingToCycle.resourceType()) {
    case MaskerResourceType:
        ASSERT(&resourceLeadingToCycle == m_resources.masker());
        m_resources.resetMasker();
        break;
    case MarkerResourceType:
        ASSERT(&resourceLeadingToCycle == m_resources.markerStart() || &resourceLeadingToCycle == m_resources.markerMid() || &resourceLeadingToCycle == m_resources.markerEnd());
        if (m_resources.markerStart() == &resourceLeadingToCycle)
            m_resources.resetMarkerStart();
        if (m_resources.markerMid() == &resourceLeadingToCycle)
            m_resources.resetMarkerMid();
        if (m_resources.markerEnd() == &resourceLeadingToCycle)
            m_resources.resetMarkerEnd();
        break;
    case PatternResourceType:
    case LinearGradientResourceType:
    case RadialGradientResourceType:
        ASSERT(&resourceLeadingToCycle == m_resources.fill() || &resourceLeadingToCycle == m_resources.stroke());
        if (m_resources.fill() == &resourceLeadingToCycle)
            m_resources.resetFill();
        if (m_resources.stroke() == &resourceLeadingToCycle)
            m_resources.resetStroke();
        break;
    case FilterResourceType:
        ASSERT(&resourceLeadingToCycle == m_resources.filter());
        m_resources.resetFilter();
        break;
    case ClipperResourceType:
        ASSERT(&resourceLeadingToCycle == m_resources.clipper());
        m_resources.resetClipper();
        break;
    case SolidColorResourceType:
        ASSERT_NOT_REACHED();
        break;
    }
}

}
