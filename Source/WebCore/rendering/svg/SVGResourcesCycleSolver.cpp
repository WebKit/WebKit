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
#include <wtf/Scope.h>

namespace WebCore {

bool SVGResourcesCycleSolver::resourceContainsCycles(RenderSVGResourceContainer& resource,
    WeakHashSet<RenderSVGResourceContainer>& activeResources, WeakHashSet<RenderSVGResourceContainer>& acyclicResources)
{
    if (acyclicResources.contains(resource))
        return false;

    activeResources.add(resource);
    auto activeResourceAcquisition = WTF::makeScopeExit([&]() {
        activeResources.remove(resource);
    });

    RenderObject* node = &resource;
    while (node) {
        if (node != &resource && node->isSVGResourceContainer()) {
            node = node->nextInPreOrderAfterChildren(&resource);
            continue;
        }
        if (is<RenderElement>(*node)) {
            if (auto* resources = SVGResourcesCache::cachedResourcesForRenderer(downcast<RenderElement>(*node))) {
                WeakHashSet<RenderSVGResourceContainer> resourceSet;
                resources->buildSetOfResources(resourceSet);

                for (auto& resource : resourceSet) {
                    if (activeResources.contains(resource) || resourceContainsCycles(resource, activeResources, acyclicResources))
                        return true;
                }
            }
        }

        node = node->nextInPreOrder(&resource);
    }

    acyclicResources.add(resource);
    return false;
}

void SVGResourcesCycleSolver::resolveCycles(RenderElement& renderer, SVGResources& resources)
{
    WeakHashSet<RenderSVGResourceContainer> localResources;
    resources.buildSetOfResources(localResources);

    WeakHashSet<RenderSVGResourceContainer> activeResources;
    WeakHashSet<RenderSVGResourceContainer> acyclicResources;

    if (is<RenderSVGResourceContainer>(renderer))
        activeResources.add(downcast<RenderSVGResourceContainer>(renderer));

    // The job of this function is to determine wheter any of the 'resources' associated with the given 'renderer'
    // references us (or whether any of its kids references us) -> that's a cycle, we need to find and break it.
    for (auto& resource : localResources) {
        if (activeResources.contains(resource) || resourceContainsCycles(resource, activeResources, acyclicResources))
            breakCycle(resource, resources);
    }
}

void SVGResourcesCycleSolver::breakCycle(RenderSVGResourceContainer& resourceLeadingToCycle, SVGResources& resources)
{
    if (&resourceLeadingToCycle == resources.linkedResource()) {
        resources.resetLinkedResource();
        return;
    }

    switch (resourceLeadingToCycle.resourceType()) {
    case MaskerResourceType:
        ASSERT(&resourceLeadingToCycle == resources.masker());
        resources.resetMasker();
        break;
    case MarkerResourceType:
        ASSERT(&resourceLeadingToCycle == resources.markerStart() || &resourceLeadingToCycle == resources.markerMid() || &resourceLeadingToCycle == resources.markerEnd());
        if (resources.markerStart() == &resourceLeadingToCycle)
            resources.resetMarkerStart();
        if (resources.markerMid() == &resourceLeadingToCycle)
            resources.resetMarkerMid();
        if (resources.markerEnd() == &resourceLeadingToCycle)
            resources.resetMarkerEnd();
        break;
    case PatternResourceType:
    case LinearGradientResourceType:
    case RadialGradientResourceType:
        ASSERT(&resourceLeadingToCycle == resources.fill() || &resourceLeadingToCycle == resources.stroke());
        if (resources.fill() == &resourceLeadingToCycle)
            resources.resetFill();
        if (resources.stroke() == &resourceLeadingToCycle)
            resources.resetStroke();
        break;
    case FilterResourceType:
        ASSERT(&resourceLeadingToCycle == resources.filter());
        resources.resetFilter();
        break;
    case ClipperResourceType:
        ASSERT(&resourceLeadingToCycle == resources.clipper());
        resources.resetClipper();
        break;
    case SolidColorResourceType:
        ASSERT_NOT_REACHED();
        break;
    }
}

}
