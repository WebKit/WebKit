/*
    Copyright (C) Research In Motion Limited 2010. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "SVGResourcesCycleSolver.h"

// Set to a value > 0, to debug the resource cache.
#define DEBUG_CYCLE_DETECTION 0

#if ENABLE(SVG)
#include "RenderSVGResourceClipper.h"
#include "RenderSVGResourceFilter.h"
#include "RenderSVGResourceMarker.h"
#include "RenderSVGResourceMasker.h"
#include "SVGFilterElement.h"
#include "SVGGradientElement.h"
#include "SVGPatternElement.h"
#include "SVGResources.h"
#include "SVGResourcesCache.h"

namespace WebCore {

SVGResourcesCycleSolver::SVGResourcesCycleSolver(RenderObject* renderer, SVGResources* resources)
    : m_renderer(renderer)
    , m_resources(resources)
{
    ASSERT(m_renderer);
    ASSERT(m_resources);
}

SVGResourcesCycleSolver::~SVGResourcesCycleSolver()
{
}

bool SVGResourcesCycleSolver::resourceContainsCycles(RenderObject* renderer) const
{
    ASSERT(renderer);

    // First operate on the resources of the given renderer.
    // <marker id="a"> <path marker-start="url(#b)"/> ...
    // <marker id="b" marker-start="url(#a)"/>
    if (SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(renderer)) {
        HashSet<RenderSVGResourceContainer*> resourceSet;
        resources->buildSetOfResources(resourceSet);

        // Walk all resources and check wheter they reference any resource contained in the resources set.
        HashSet<RenderSVGResourceContainer*>::iterator end = resourceSet.end();
        for (HashSet<RenderSVGResourceContainer*>::iterator it = resourceSet.begin(); it != end; ++it) {
            if (m_allResources.contains(*it))
                return true;
        }
    }

    // Then operate on the child resources of the given renderer.
    // <marker id="a"> <path marker-start="url(#b)"/> ...
    // <marker id="b"> <path marker-start="url(#a)"/> ...
    for (RenderObject* child = renderer->firstChild(); child; child = child->nextSibling()) {
        SVGResources* childResources = SVGResourcesCache::cachedResourcesForRenderObject(child);
        if (!childResources)
            continue;
        
        // A child of the given 'resource' contains resources. 
        HashSet<RenderSVGResourceContainer*> childSet;
        childResources->buildSetOfResources(childSet);

        // Walk all child resources and check wheter they reference any resource contained in the resources set.
        HashSet<RenderSVGResourceContainer*>::iterator end = childSet.end();
        for (HashSet<RenderSVGResourceContainer*>::iterator it = childSet.begin(); it != end; ++it) {
            if (m_allResources.contains(*it))
                return true;
        }

        // Walk children recursively, stop immediately if we found a cycle
        if (resourceContainsCycles(child))
            return true;
    }

    return false;
}

static inline String targetReferenceFromResource(SVGElement* element, bool& isValid)
{
    String target;
    if (element->hasTagName(SVGNames::patternTag))
        target = static_cast<SVGPatternElement*>(element)->href();
    else if (element->hasTagName(SVGNames::linearGradientTag) || element->hasTagName(SVGNames::radialGradientTag))
        target = static_cast<SVGGradientElement*>(element)->href();
    else if (element->hasTagName(SVGNames::filterTag))
        target = static_cast<SVGFilterElement*>(element)->href();
    else {
        isValid = false;
        return target;
    }

    return SVGURIReference::getTarget(target);
}

static inline void setFollowLinkForChainableResource(SVGElement* element, bool followLink)
{
    if (element->hasTagName(SVGNames::patternTag))
        static_cast<SVGPatternElement*>(element)->setFollowLink(followLink);
    else if (element->hasTagName(SVGNames::linearGradientTag) || element->hasTagName(SVGNames::radialGradientTag))
        static_cast<SVGGradientElement*>(element)->setFollowLink(followLink);
    else if (element->hasTagName(SVGNames::filterTag))
        static_cast<SVGFilterElement*>(element)->setFollowLink(followLink);
}

bool SVGResourcesCycleSolver::chainableResourceContainsCycles(RenderSVGResourceContainer* container) const
{
    ASSERT(container);
    ASSERT(container->node());
    ASSERT(container->node()->isSVGElement());

    // Chainable resources cycle detection is performed in the DOM tree.
    SVGElement* element = static_cast<SVGElement*>(container->node());
    ASSERT(element);

    HashSet<SVGElement*> processedObjects;

    bool isValid = true;
    String target = targetReferenceFromResource(element, isValid);
    ASSERT(isValid);

    SVGElement* previousElement = element;
    while (!target.isEmpty()) {
        Node* targetNode = element->document()->getElementById(target);
        if (!targetNode || !targetNode->isSVGElement())
            break;

        // Catch cylic chaining, otherwhise we'll run into an infinite loop here.
        // <pattern id="foo" xlink:href="#bar"/> <pattern id="bar xlink:href="#foo"/>
        SVGElement* targetElement = static_cast<SVGElement*>(targetNode);

        bool followLink = true;
        if (processedObjects.contains(targetElement) || targetElement == element)
            followLink = false;

        setFollowLinkForChainableResource(previousElement, followLink);
        if (!followLink)
            return false;

        previousElement = targetElement;
        processedObjects.add(targetElement);
        target = targetReferenceFromResource(targetElement, isValid);
        if (!isValid)
            break;
    }

    // Couldn't find any direct cycle in the xlink:href chain, maybe there's an indirect one.
    // <pattern id="foo" xlink:href="#bar"/> <pattern id="bar"> <rect fill="url(#foo)"...
    HashSet<SVGElement*>::iterator end = processedObjects.end();
    for (HashSet<SVGElement*>::iterator it = processedObjects.begin(); it != end; ++it) {
        RenderObject* renderer = (*it)->renderer();
        if (!renderer)
            continue;
        ASSERT(renderer->isSVGResourceContainer());
        if (m_allResources.contains(renderer->toRenderSVGResourceContainer()))
            return true;
    }

    return false;
}

void SVGResourcesCycleSolver::resolveCycles()
{
    ASSERT(m_allResources.isEmpty());

#if DEBUG_CYCLE_DETECTION > 0
    fprintf(stderr, "\nBefore cycle detection:\n");
    m_resources->dump(m_renderer);
#endif

    // Stash all resources into a HashSet for the ease of traversing.
    HashSet<RenderSVGResourceContainer*> localResources;
    m_resources->buildSetOfResources(localResources);
    ASSERT(!localResources.isEmpty());

    // Add all parent resource containers to the HashSet.
    HashSet<RenderSVGResourceContainer*> parentResources;
    RenderObject* parent = m_renderer->parent();
    while (parent) {
        if (parent->isSVGResourceContainer())
            parentResources.add(parent->toRenderSVGResourceContainer());
        parent = parent->parent();
    }

#if DEBUG_CYCLE_DETECTION > 0
    fprintf(stderr, "\nDetecting wheter any resources references any of following objects:\n");
    {
        fprintf(stderr, "Local resources:\n");
        HashSet<RenderSVGResourceContainer*>::iterator end = localResources.end();
        for (HashSet<RenderSVGResourceContainer*>::iterator it = localResources.begin(); it != end; ++it)
            fprintf(stderr, "|> %s: object=%p (node=%p)\n", (*it)->renderName(), *it, (*it)->node());

        fprintf(stderr, "Parent resources:\n");
        end = parentResources.end();
        for (HashSet<RenderSVGResourceContainer*>::iterator it = parentResources.begin(); it != end; ++it)
            fprintf(stderr, "|> %s: object=%p (node=%p)\n", (*it)->renderName(), *it, (*it)->node());
    }
#endif

    // Build combined set of local and parent resources.
    m_allResources = localResources;
    HashSet<RenderSVGResourceContainer*>::iterator end = parentResources.end();
    for (HashSet<RenderSVGResourceContainer*>::iterator it = parentResources.begin(); it != end; ++it)
        m_allResources.add(*it);

    ASSERT(!m_allResources.isEmpty());

    // The job of this function is to determine wheter any of the 'resources' associated with the given 'renderer'
    // references us (or wheter any of its kids references us) -> that's a cycle, we need to find and break it.
    end = localResources.end();
    for (HashSet<RenderSVGResourceContainer*>::iterator it = localResources.begin(); it != end; ++it) {
        RenderSVGResourceContainer* resource = *it;

        // Special handling for resources that can be chained using xlink:href - need to detect cycles as well!
        switch (resource->resourceType()) {
        case PatternResourceType:
        case LinearGradientResourceType:
        case RadialGradientResourceType:
        case FilterResourceType:
            if (chainableResourceContainsCycles(resource)) {
                breakCycle(resource);
                continue;
            }
            break;
        default:
            break;
        }

        if (parentResources.contains(resource) || resourceContainsCycles(resource))
            breakCycle(resource);
    }

#if DEBUG_CYCLE_DETECTION > 0
    fprintf(stderr, "\nAfter cycle detection:\n");
    m_resources->dump(m_renderer);
#endif

    m_allResources.clear();
}

void SVGResourcesCycleSolver::breakCycle(RenderSVGResourceContainer* resourceLeadingToCycle)
{
    ASSERT(resourceLeadingToCycle);
    switch (resourceLeadingToCycle->resourceType()) {
    case MaskerResourceType:
        ASSERT(resourceLeadingToCycle == m_resources->masker());
        m_resources->resetMasker();
        break;
    case MarkerResourceType:
        ASSERT(resourceLeadingToCycle == m_resources->markerStart() || resourceLeadingToCycle == m_resources->markerMid() || resourceLeadingToCycle == m_resources->markerEnd());
        if (m_resources->markerStart() == resourceLeadingToCycle)
            m_resources->resetMarkerStart();
        if (m_resources->markerMid() == resourceLeadingToCycle)
            m_resources->resetMarkerMid();
        if (m_resources->markerEnd() == resourceLeadingToCycle)
            m_resources->resetMarkerEnd();
        break;
    case PatternResourceType:
    case LinearGradientResourceType:
    case RadialGradientResourceType:
        ASSERT(resourceLeadingToCycle == m_resources->fill() || resourceLeadingToCycle == m_resources->stroke());
        if (m_resources->fill() == resourceLeadingToCycle)
            m_resources->resetFill();
        if (m_resources->stroke() == resourceLeadingToCycle)
            m_resources->resetStroke();
        break;
    case FilterResourceType:
        ASSERT(resourceLeadingToCycle == m_resources->filter());
        m_resources->resetFilter();
        break;
    case ClipperResourceType:
        ASSERT(resourceLeadingToCycle == m_resources->clipper());
        m_resources->resetClipper();
        break;
    case SolidColorResourceType:
        ASSERT_NOT_REACHED();
        break;
    }
}

}

#endif
