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
#include "SVGResources.h"

#include "FilterOperation.h"
#include "LegacyRenderSVGResourceClipperInlines.h"
#include "LegacyRenderSVGResourceFilterInlines.h"
#include "LegacyRenderSVGResourceMarkerInlines.h"
#include "LegacyRenderSVGResourceMaskerInlines.h"
#include "LegacyRenderSVGRoot.h"
#include "PathOperation.h"
#include "SVGElementTypeHelpers.h"
#include "SVGFilterElement.h"
#include "SVGGradientElement.h"
#include "SVGMarkerElement.h"
#include "SVGNames.h"
#include "SVGPatternElement.h"
#include "SVGRenderStyle.h"
#include "SVGURIReference.h"
#include "StyleCachedImage.h"
#include <wtf/RobinHoodHashSet.h>

#if ENABLE(TREE_DEBUGGING)
#include <stdio.h>
#endif

namespace WebCore {

SVGResources::SVGResources()
{
}

static MemoryCompactLookupOnlyRobinHoodHashSet<AtomString> tagSet(std::span<decltype(SVGNames::aTag)* const> tags)
{
    MemoryCompactLookupOnlyRobinHoodHashSet<AtomString> set;
    set.reserveInitialCapacity(tags.size());
    for (auto& tag : tags)
        set.add(tag->get().localName());
    return set;
}

static const MemoryCompactLookupOnlyRobinHoodHashSet<AtomString>& clipperFilterMaskerTags()
{
    static constexpr std::array tags {
        // "container elements": http://www.w3.org/TR/SVG11/intro.html#TermContainerElement
        // "graphics elements" : http://www.w3.org/TR/SVG11/intro.html#TermGraphicsElement
        &SVGNames::aTag,
        &SVGNames::circleTag,
        &SVGNames::ellipseTag,
        &SVGNames::glyphTag,
        &SVGNames::gTag,
        &SVGNames::imageTag,
        &SVGNames::lineTag,
        &SVGNames::markerTag,
        &SVGNames::maskTag,
        &SVGNames::missing_glyphTag,
        &SVGNames::pathTag,
        &SVGNames::polygonTag,
        &SVGNames::polylineTag,
        &SVGNames::rectTag,
        &SVGNames::svgTag,
        &SVGNames::switchTag,
        &SVGNames::textTag,
        &SVGNames::useTag,

        // Not listed in the definitions is the clipPath element, the SVG spec says though:
        // The "clipPath" element or any of its children can specify property "clip-path".
        // So we have to add clipPathTag here, otherwhise clip-path on clipPath will fail.
        // (Already mailed SVG WG, waiting for a solution)
        &SVGNames::clipPathTag,

        // Not listed in the definitions are the text content elements, though filter/clipper/masker on tspan/text/.. is allowed.
        // (Already mailed SVG WG, waiting for a solution)
        &SVGNames::altGlyphTag,
        &SVGNames::textPathTag,
        &SVGNames::trefTag,
        &SVGNames::tspanTag,

        // Not listed in the definitions is the foreignObject element, but clip-path
        // is a supported attribute.
        &SVGNames::foreignObjectTag,

        // Elements that we ignore, as it doesn't make any sense.
        // defs, pattern (FIXME: Mail SVG WG about these)
        // symbol (is converted to a svg element, when referenced by use, we can safely ignore it.)
    };
    static NeverDestroyed set = tagSet(tags);
    return set;
}

static const MemoryCompactLookupOnlyRobinHoodHashSet<AtomString>& markerTags()
{
    static constexpr std::array tags {
        &SVGNames::lineTag,
        &SVGNames::pathTag,
        &SVGNames::polygonTag,
        &SVGNames::polylineTag,
    };
    static NeverDestroyed set = tagSet(tags);
    return set;
}

static const MemoryCompactLookupOnlyRobinHoodHashSet<AtomString>& fillAndStrokeTags()
{
    static constexpr std::array tags {
        &SVGNames::altGlyphTag,
        &SVGNames::circleTag,
        &SVGNames::ellipseTag,
        &SVGNames::lineTag,
        &SVGNames::pathTag,
        &SVGNames::polygonTag,
        &SVGNames::polylineTag,
        &SVGNames::rectTag,
        &SVGNames::textTag,
        &SVGNames::textPathTag,
        &SVGNames::trefTag,
        &SVGNames::tspanTag,
    };
    static NeverDestroyed set = tagSet(tags);
    return set;
}

static const MemoryCompactLookupOnlyRobinHoodHashSet<AtomString>& chainableResourceTags()
{
    static constexpr std::array tags {
        &SVGNames::linearGradientTag,
        &SVGNames::filterTag,
        &SVGNames::patternTag,
        &SVGNames::radialGradientTag,
    };
    static NeverDestroyed set = tagSet(tags);
    return set;
}

static inline String targetReferenceFromResource(SVGElement& element)
{
    String target;
    if (auto* svgPattern = dynamicDowncast<SVGPatternElement>(element))
        target = svgPattern->href();
    else if (auto* svgGradient = dynamicDowncast<SVGGradientElement>(element))
        target = svgGradient->href();
    else if (auto* svgFilter = dynamicDowncast<SVGFilterElement>(element))
        target = svgFilter->href();
    else
        ASSERT_NOT_REACHED();

    return SVGURIReference::fragmentIdentifierFromIRIString(target, element.document());
}

static inline bool isChainableResource(const SVGElement& element, const SVGElement& linkedResource)
{
    if (is<SVGPatternElement>(element))
        return is<SVGPatternElement>(linkedResource);

    if (is<SVGGradientElement>(element))
        return is<SVGGradientElement>(linkedResource);
    
    if (is<SVGFilterElement>(element))
        return is<SVGFilterElement>(linkedResource);

    ASSERT_NOT_REACHED();
    return false;
}

static inline LegacyRenderSVGResourceContainer* paintingResourceFromSVGPaint(TreeScope& treeScope, const SVGPaintType& paintType, const String& paintUri, AtomString& id, bool& hasPendingResource)
{
    if (paintType != SVGPaintType::URI && paintType != SVGPaintType::URIRGBColor && paintType != SVGPaintType::URICurrentColor)
        return nullptr;

    id = SVGURIReference::fragmentIdentifierFromIRIString(paintUri, treeScope.documentScope());
    LegacyRenderSVGResourceContainer* container = getRenderSVGResourceContainerById(treeScope, id);
    if (!container) {
        hasPendingResource = true;
        return nullptr;
    }

    RenderSVGResourceType resourceType = container->resourceType();
    if (resourceType != PatternResourceType && resourceType != LinearGradientResourceType && resourceType != RadialGradientResourceType)
        return nullptr;

    return container;
}

std::unique_ptr<SVGResources> SVGResources::buildCachedResources(const RenderElement& renderer, const RenderStyle& style)
{
    ASSERT(renderer.element());
    if (!renderer.element())
        return nullptr;

    auto& element = downcast<SVGElement>(*renderer.element());

    auto& treeScope = element.treeScopeForSVGReferences();
    Document& document = treeScope.documentScope();

    const AtomString& tagName = element.localName();
    if (tagName.isNull())
        return nullptr;

    const SVGRenderStyle& svgStyle = style.svgStyle();

    auto ensureResources = [](std::unique_ptr<SVGResources>& resources) -> SVGResources& {
        if (!resources)
            resources = makeUnique<SVGResources>();
        return *resources;
    };

    std::unique_ptr<SVGResources> foundResources;
    if (clipperFilterMaskerTags().contains(tagName)) {
        if (auto* clipPath = dynamicDowncast<ReferencePathOperation>(style.clipPath())) {
            // FIXME: -webkit-clip-path should support external resources
            // https://bugs.webkit.org/show_bug.cgi?id=127032
            AtomString id(clipPath->fragment());
            if (auto* clipper = getRenderSVGResourceById<LegacyRenderSVGResourceClipper>(treeScope, id))
                ensureResources(foundResources).setClipper(clipper);
            else
                treeScope.addPendingSVGResource(id, element);
        }

        if (style.hasFilter()) {
            const FilterOperations& filterOperations = style.filter();
            if (filterOperations.size() == 1) {
                const FilterOperation& filterOperation = *filterOperations.at(0);
                if (auto* referenceFilterOperation = dynamicDowncast<ReferenceFilterOperation>(filterOperation)) {
                    AtomString id = SVGURIReference::fragmentIdentifierFromIRIString(referenceFilterOperation->url(), element.document());
                    if (auto* filter = getRenderSVGResourceById<LegacyRenderSVGResourceFilter>(treeScope, id))
                        ensureResources(foundResources).setFilter(filter);
                    else
                        treeScope.addPendingSVGResource(id, element);
                }
            }
        }

        if (style.hasPositionedMask()) {
            // FIXME: We should support all the values in the CSS mask property, but for now just use the first mask-image if it's a reference.
            auto* maskImage = style.maskImage();
            auto reresolvedURL = maskImage ? maskImage->reresolvedURL(document) : URL();

            if (!reresolvedURL.isEmpty()) {
                auto resourceID = SVGURIReference::fragmentIdentifierFromIRIString(reresolvedURL.string(), document);
                if (auto* masker = getRenderSVGResourceById<LegacyRenderSVGResourceMasker>(treeScope, resourceID))
                    ensureResources(foundResources).setMasker(masker);
                else
                    treeScope.addPendingSVGResource(resourceID, element);
            }
        }
    }

    if (markerTags().contains(tagName) && svgStyle.hasMarkers()) {
        auto buildCachedMarkerResource = [&](const String& markerResource, bool (SVGResources::*setMarker)(LegacyRenderSVGResourceMarker*)) {
            auto markerId = SVGURIReference::fragmentIdentifierFromIRIString(markerResource, document);
            if (auto* marker = getRenderSVGResourceById<LegacyRenderSVGResourceMarker>(treeScope, markerId))
                (ensureResources(foundResources).*setMarker)(marker);
            else
                treeScope.addPendingSVGResource(markerId, element);
        };
        buildCachedMarkerResource(svgStyle.markerStartResource(), &SVGResources::setMarkerStart);
        buildCachedMarkerResource(svgStyle.markerMidResource(), &SVGResources::setMarkerMid);
        buildCachedMarkerResource(svgStyle.markerEndResource(), &SVGResources::setMarkerEnd);
    }

    if (fillAndStrokeTags().contains(tagName)) {
        if (svgStyle.hasFill()) {
            bool hasPendingResource = false;
            AtomString id;
            if (auto* fill = paintingResourceFromSVGPaint(treeScope, svgStyle.fillPaintType(), svgStyle.fillPaintUri(), id, hasPendingResource))
                ensureResources(foundResources).setFill(fill);
            else if (hasPendingResource)
                treeScope.addPendingSVGResource(id, element);
        }

        if (svgStyle.hasStroke()) {
            bool hasPendingResource = false;
            AtomString id;
            if (auto* stroke = paintingResourceFromSVGPaint(treeScope, svgStyle.strokePaintType(), svgStyle.strokePaintUri(), id, hasPendingResource))
                ensureResources(foundResources).setStroke(stroke);
            else if (hasPendingResource)
                treeScope.addPendingSVGResource(id, element);
        }
    }

    if (chainableResourceTags().contains(tagName)) {
        AtomString id(targetReferenceFromResource(element));
        auto* linkedResource = getRenderSVGResourceContainerById(document, id);
        if (!linkedResource)
            treeScope.addPendingSVGResource(id, element);
        else if (isChainableResource(element, linkedResource->element()))
            ensureResources(foundResources).setLinkedResource(linkedResource);
    }

    return foundResources;
}

void SVGResources::layoutDifferentRootIfNeeded(const RenderElement& resourcesClient)
{
    const LegacyRenderSVGRoot* clientRoot = nullptr;

    auto layoutDifferentRootIfNeeded = [&](LegacyRenderSVGResourceContainer* container) {
        if (!container)
            return;

        auto* root = SVGRenderSupport::findTreeRootObject(*container);
        if (root->isInLayout())
            return;

        if (!clientRoot)
            clientRoot = SVGRenderSupport::findTreeRootObject(resourcesClient);

        if (clientRoot == root)
            return;

        container->layoutIfNeeded();
    };

    if (m_clipperFilterMaskerData) {
        layoutDifferentRootIfNeeded(m_clipperFilterMaskerData->clipper.get());
        layoutDifferentRootIfNeeded(m_clipperFilterMaskerData->masker.get());
        layoutDifferentRootIfNeeded(m_clipperFilterMaskerData->filter.get());
    }

    if (m_markerData) {
        layoutDifferentRootIfNeeded(m_markerData->markerStart.get());
        layoutDifferentRootIfNeeded(m_markerData->markerMid.get());
        layoutDifferentRootIfNeeded(m_markerData->markerEnd.get());
    }
}

bool SVGResources::markerReverseStart() const
{
    return m_markerData
        && m_markerData->markerStart
        && m_markerData->markerStart->markerElement().orientType() == SVGMarkerOrientAutoStartReverse;
}

void SVGResources::removeClientFromCache(RenderElement& renderer, bool markForInvalidation) const
{
    if (isEmpty())
        return;

    if (m_linkedResource) {
        ASSERT(!m_clipperFilterMaskerData);
        ASSERT(!m_markerData);
        ASSERT(!m_fillStrokeData);
        m_linkedResource->removeClientFromCache(renderer, markForInvalidation);
        return;
    }

    if (m_clipperFilterMaskerData) {
        if (auto* clipper = m_clipperFilterMaskerData->clipper.get())
            clipper->removeClientFromCache(renderer, markForInvalidation);
        if (auto* filter = m_clipperFilterMaskerData->filter.get())
            filter->removeClientFromCache(renderer, markForInvalidation);
        if (auto* masker = m_clipperFilterMaskerData->masker.get())
            masker->removeClientFromCache(renderer, markForInvalidation);
    }

    if (m_markerData) {
        if (auto* markerStart = m_markerData->markerStart.get())
            markerStart->removeClientFromCache(renderer, markForInvalidation);
        if (auto* markerMid = m_markerData->markerMid.get())
            markerMid->removeClientFromCache(renderer, markForInvalidation);
        if (auto* markerEnd = m_markerData->markerEnd.get())
            markerEnd->removeClientFromCache(renderer, markForInvalidation);
    }

    if (m_fillStrokeData) {
        if (auto* fill = m_fillStrokeData->fill.get())
            fill->removeClientFromCache(renderer, markForInvalidation);
        if (auto* stroke = m_fillStrokeData->stroke.get())
            stroke->removeClientFromCache(renderer, markForInvalidation);
    }
}

bool SVGResources::resourceDestroyed(LegacyRenderSVGResourceContainer& resource)
{
    if (isEmpty())
        return false;

    if (m_linkedResource == &resource) {
        ASSERT(!m_clipperFilterMaskerData);
        ASSERT(!m_markerData);
        ASSERT(!m_fillStrokeData);
        m_linkedResource->removeAllClientsFromCache();
        m_linkedResource = nullptr;
        return true;
    }

    bool foundResources = false;
    switch (resource.resourceType()) {
    case MaskerResourceType:
        if (!m_clipperFilterMaskerData)
            break;
        if (m_clipperFilterMaskerData->masker == &resource) {
            resource.removeAllClientsFromCache();
            m_clipperFilterMaskerData->masker = nullptr;
            foundResources = true;
        }
        break;
    case MarkerResourceType:
        if (!m_markerData)
            break;
        if (m_markerData->markerStart == &resource) {
            resource.removeAllClientsFromCache();
            m_markerData->markerStart = nullptr;
            foundResources = true;
        }
        if (m_markerData->markerMid == &resource) {
            resource.removeAllClientsFromCache();
            m_markerData->markerMid = nullptr;
            foundResources = true;
        }
        if (m_markerData->markerEnd == &resource) {
            resource.removeAllClientsFromCache();
            m_markerData->markerEnd = nullptr;
            foundResources = true;
        }
        break;
    case PatternResourceType:
    case LinearGradientResourceType:
    case RadialGradientResourceType:
        if (!m_fillStrokeData)
            break;
        if (m_fillStrokeData->fill == &resource) {
            resource.removeAllClientsFromCache();
            m_fillStrokeData->fill = nullptr;
            foundResources = true;
        }
        if (m_fillStrokeData->stroke == &resource) {
            resource.removeAllClientsFromCache();
            m_fillStrokeData->stroke = nullptr;
            foundResources = true;
        }
        break;
    case FilterResourceType:
        if (!m_clipperFilterMaskerData)
            break;
        if (m_clipperFilterMaskerData->filter == &resource) {
            resource.removeAllClientsFromCache();
            m_clipperFilterMaskerData->filter = nullptr;
            foundResources = true;
        }
        break;
    case ClipperResourceType:
        if (!m_clipperFilterMaskerData)
            break; 
        if (m_clipperFilterMaskerData->clipper == &resource) {
            resource.removeAllClientsFromCache();
            m_clipperFilterMaskerData->clipper = nullptr;
            foundResources = true;
        }
        break;
    case SolidColorResourceType:
        ASSERT_NOT_REACHED();
    }
    return foundResources;
}

void SVGResources::buildSetOfResources(SingleThreadWeakHashSet<LegacyRenderSVGResourceContainer>& set)
{
    if (isEmpty())
        return;

    if (auto* linkedResource = m_linkedResource.get()) {
        ASSERT(!m_clipperFilterMaskerData);
        ASSERT(!m_markerData);
        ASSERT(!m_fillStrokeData);
        set.add(*linkedResource);
        return;
    }

    if (m_clipperFilterMaskerData) {
        if (auto* clipper = m_clipperFilterMaskerData->clipper.get())
            set.add(*clipper);
        if (auto* filter = m_clipperFilterMaskerData->filter.get())
            set.add(*filter);
        if (auto* masker = m_clipperFilterMaskerData->masker.get())
            set.add(*masker);
    }

    if (m_markerData) {
        if (auto* markerStart = m_markerData->markerStart.get())
            set.add(*markerStart);
        if (auto* markerMid = m_markerData->markerMid.get())
            set.add(*markerMid);
        if (auto* markerEnd = m_markerData->markerEnd.get())
            set.add(*markerEnd);
    }

    if (m_fillStrokeData) {
        if (auto* fill = m_fillStrokeData->fill.get())
            set.add(*fill);
        if (auto* stroke = m_fillStrokeData->stroke.get())
            set.add(*stroke);
    }
}

bool SVGResources::setClipper(LegacyRenderSVGResourceClipper* clipper)
{
    ASSERT(clipper);
    ASSERT(clipper->resourceType() == ClipperResourceType);

    if (!m_clipperFilterMaskerData)
        m_clipperFilterMaskerData = makeUnique<ClipperFilterMaskerData>();

    m_clipperFilterMaskerData->clipper = clipper;
    return true;
}

void SVGResources::resetClipper()
{
    ASSERT(m_clipperFilterMaskerData);
    ASSERT(m_clipperFilterMaskerData->clipper);
    m_clipperFilterMaskerData->clipper = nullptr;
}

bool SVGResources::setFilter(LegacyRenderSVGResourceFilter* filter)
{
    ASSERT(filter);
    ASSERT(filter->resourceType() == FilterResourceType);

    if (!m_clipperFilterMaskerData)
        m_clipperFilterMaskerData = makeUnique<ClipperFilterMaskerData>();

    m_clipperFilterMaskerData->filter = filter;
    return true;
}

void SVGResources::resetFilter()
{
    ASSERT(m_clipperFilterMaskerData);
    ASSERT(m_clipperFilterMaskerData->filter);
    m_clipperFilterMaskerData->filter = nullptr;
}

bool SVGResources::setMarkerStart(LegacyRenderSVGResourceMarker* markerStart)
{
    ASSERT(markerStart);
    ASSERT(markerStart->resourceType() == MarkerResourceType);

    if (!m_markerData)
        m_markerData = makeUnique<MarkerData>();

    m_markerData->markerStart = markerStart;
    return true;
}

void SVGResources::resetMarkerStart()
{
    ASSERT(m_markerData);
    ASSERT(m_markerData->markerStart);
    m_markerData->markerStart = nullptr;
}

bool SVGResources::setMarkerMid(LegacyRenderSVGResourceMarker* markerMid)
{
    ASSERT(markerMid);
    ASSERT(markerMid->resourceType() == MarkerResourceType);

    if (!m_markerData)
        m_markerData = makeUnique<MarkerData>();

    m_markerData->markerMid = markerMid;
    return true;
}

void SVGResources::resetMarkerMid()
{
    ASSERT(m_markerData);
    ASSERT(m_markerData->markerMid);
    m_markerData->markerMid = nullptr;
}

bool SVGResources::setMarkerEnd(LegacyRenderSVGResourceMarker* markerEnd)
{
    ASSERT(markerEnd);
    ASSERT(markerEnd->resourceType() == MarkerResourceType);

    if (!m_markerData)
        m_markerData = makeUnique<MarkerData>();

    m_markerData->markerEnd = markerEnd;
    return true;
}

void SVGResources::resetMarkerEnd()
{
    ASSERT(m_markerData);
    ASSERT(m_markerData->markerEnd);
    m_markerData->markerEnd = nullptr;
}

bool SVGResources::setMasker(LegacyRenderSVGResourceMasker* masker)
{
    ASSERT(masker);
    ASSERT(masker->resourceType() == MaskerResourceType);

    if (!m_clipperFilterMaskerData)
        m_clipperFilterMaskerData = makeUnique<ClipperFilterMaskerData>();

    m_clipperFilterMaskerData->masker = masker;
    return true;
}

void SVGResources::resetMasker()
{
    ASSERT(m_clipperFilterMaskerData);
    ASSERT(m_clipperFilterMaskerData->masker);
    m_clipperFilterMaskerData->masker = nullptr;
}

bool SVGResources::setFill(LegacyRenderSVGResourceContainer* fill)
{
    ASSERT(fill);
    ASSERT(fill->resourceType() == PatternResourceType
           || fill->resourceType() == LinearGradientResourceType
           || fill->resourceType() == RadialGradientResourceType);

    if (!m_fillStrokeData)
        m_fillStrokeData = makeUnique<FillStrokeData>();

    m_fillStrokeData->fill = fill;
    return true;
}

void SVGResources::resetFill()
{
    ASSERT(m_fillStrokeData);
    ASSERT(m_fillStrokeData->fill);
    m_fillStrokeData->fill = nullptr;
}

bool SVGResources::setStroke(LegacyRenderSVGResourceContainer* stroke)
{
    ASSERT(stroke);
    ASSERT(stroke->resourceType() == PatternResourceType
           || stroke->resourceType() == LinearGradientResourceType
           || stroke->resourceType() == RadialGradientResourceType);

    if (!m_fillStrokeData)
        m_fillStrokeData = makeUnique<FillStrokeData>();

    m_fillStrokeData->stroke = stroke;
    return true;
}

void SVGResources::resetStroke()
{
    ASSERT(m_fillStrokeData);
    ASSERT(m_fillStrokeData->stroke);
    m_fillStrokeData->stroke = nullptr;
}

bool SVGResources::setLinkedResource(LegacyRenderSVGResourceContainer* linkedResource)
{
    ASSERT(linkedResource);
    m_linkedResource = linkedResource;
    return true;
}

void SVGResources::resetLinkedResource()
{
    ASSERT(m_linkedResource);
    m_linkedResource = nullptr;
}

#if ENABLE(TREE_DEBUGGING)
void SVGResources::dump(const RenderObject* object)
{
    ASSERT(object);
    ASSERT(object->node());

    fprintf(stderr, "-> this=%p, SVGResources(renderer=%p, node=%p)\n", this, object, object->node());
    fprintf(stderr, " | DOM Tree:\n");
    object->node()->showTreeForThis();

    fprintf(stderr, "\n | List of resources:\n");
    if (m_clipperFilterMaskerData) {
        if (auto* clipper = m_clipperFilterMaskerData->clipper.get())
            fprintf(stderr, " |-> Clipper    : %p (node=%p)\n", clipper, &clipper->clipPathElement());
        if (auto* filter = m_clipperFilterMaskerData->filter.get())
            fprintf(stderr, " |-> Filter     : %p (node=%p)\n", filter, &filter->filterElement());
        if (auto* masker = m_clipperFilterMaskerData->masker.get())
            fprintf(stderr, " |-> Masker     : %p (node=%p)\n", masker, &masker->maskElement());
    }

    if (m_markerData) {
        if (auto* markerStart = m_markerData->markerStart.get())
            fprintf(stderr, " |-> MarkerStart: %p (node=%p)\n", markerStart, &markerStart->markerElement());
        if (auto* markerMid = m_markerData->markerMid.get())
            fprintf(stderr, " |-> MarkerMid  : %p (node=%p)\n", markerMid, &markerMid->markerElement());
        if (auto* markerEnd = m_markerData->markerEnd.get())
            fprintf(stderr, " |-> MarkerEnd  : %p (node=%p)\n", markerEnd, &markerEnd->markerElement());
    }

    if (m_fillStrokeData) {
        if (auto* fill = m_fillStrokeData->fill.get())
            fprintf(stderr, " |-> Fill       : %p (node=%p)\n", fill, &fill->element());
        if (auto* stroke = m_fillStrokeData->stroke.get())
            fprintf(stderr, " |-> Stroke     : %p (node=%p)\n", stroke, &stroke->element());
    }

    if (m_linkedResource)
        fprintf(stderr, " |-> xlink:href : %p (node=%p)\n", m_linkedResource.get(), &m_linkedResource->element());
}
#endif

}
