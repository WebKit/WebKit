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
#include "SVGResources.h"

#if ENABLE(SVG)
#include "RenderSVGResourceClipper.h"
#include "RenderSVGResourceFilter.h"
#include "RenderSVGResourceMarker.h"
#include "RenderSVGResourceMasker.h"
#include "SVGPaint.h"
#include "SVGRenderStyle.h"
#include "SVGURIReference.h"

namespace WebCore {

SVGResources::SVGResources()
    : m_clipper(0)
#if ENABLE(FILTERS)
    , m_filter(0)
#endif
    , m_markerStart(0)
    , m_markerMid(0)
    , m_markerEnd(0)
    , m_masker(0)
    , m_fill(0)
    , m_stroke(0)
{
}

static inline RenderSVGResourceContainer* paintingResourceFromSVGPaint(Document* document, SVGPaint* paint, AtomicString& id, bool& hasPendingResource)
{
    ASSERT(paint);

    SVGPaint::SVGPaintType paintType = paint->paintType();
    if (paintType != SVGPaint::SVG_PAINTTYPE_URI && paintType != SVGPaint::SVG_PAINTTYPE_URI_RGBCOLOR)
        return 0;

    id = SVGURIReference::getTarget(paint->uri());
    if (RenderSVGResourceContainer* container = getRenderSVGResourceContainerById(document, id))
        return container;

    hasPendingResource = true;
    return 0;
}

static inline void registerPendingResource(SVGDocumentExtensions* extensions, const AtomicString& id, Node* node)
{
    ASSERT(node);
    if (!node->isSVGElement())
        return;

    SVGElement* svgElement = static_cast<SVGElement*>(node);
    if (!svgElement->isStyled())
        return;

    extensions->addPendingResource(id, static_cast<SVGStyledElement*>(svgElement));
}

bool SVGResources::buildCachedResources(const RenderObject* object, const SVGRenderStyle* style)
{
    ASSERT(object);
    ASSERT(style);

    Node* node = object->node();
    ASSERT(node);

    Document* document = object->document();
    ASSERT(document);

    SVGDocumentExtensions* extensions = document->accessSVGExtensions();
    ASSERT(extensions);

    bool foundResources = false;
    if (style->hasClipper()) {
        AtomicString id(style->clipperResource());
        m_clipper = getRenderSVGResourceById<RenderSVGResourceClipper>(document, id);
        if (m_clipper)
            foundResources = true;
        else
            registerPendingResource(extensions, id, node);
    }

    if (style->hasMasker()) {
        AtomicString id(style->maskerResource());
        m_masker = getRenderSVGResourceById<RenderSVGResourceMasker>(document, id);
        if (m_masker)
            foundResources = true;
        else
            registerPendingResource(extensions, id, node);
    }

#if ENABLE(FILTERS)
    if (style->hasFilter()) {
        AtomicString id(style->filterResource());
        m_filter = getRenderSVGResourceById<RenderSVGResourceFilter>(document, id);
        if (m_filter)
            foundResources = true;
        else
            registerPendingResource(extensions, id, node);
    }
#endif

    if (style->hasMarkers()) {
        AtomicString markerStartId(style->markerStartResource());
        m_markerStart = getRenderSVGResourceById<RenderSVGResourceMarker>(document, markerStartId);
        if (m_markerStart)
            foundResources = true;
        else
            registerPendingResource(extensions, markerStartId, node);

        AtomicString markerMidId(style->markerMidResource());
        m_markerMid = getRenderSVGResourceById<RenderSVGResourceMarker>(document, markerMidId);
        if (m_markerMid)
            foundResources = true;
        else
            registerPendingResource(extensions, markerMidId, node);

        AtomicString markerEndId(style->markerEndResource());
        m_markerEnd = getRenderSVGResourceById<RenderSVGResourceMarker>(document, markerEndId);
        if (m_markerEnd)
            foundResources = true;
        else
            registerPendingResource(extensions, markerEndId, node);
    }

    if (style->hasFill()) {
        bool hasPendingResource = false;
        AtomicString id;
        m_fill = paintingResourceFromSVGPaint(document, style->fillPaint(), id, hasPendingResource);
        if (m_fill)
            foundResources = true;
        else if (hasPendingResource)
            registerPendingResource(extensions, id, node);
    }

    if (style->hasStroke()) {
        bool hasPendingResource = false;
        AtomicString id;
        m_stroke = paintingResourceFromSVGPaint(document, style->strokePaint(), id, hasPendingResource);
        if (m_stroke)
            foundResources = true;
        else if (hasPendingResource)
            registerPendingResource(extensions, id, node);
    }

    return foundResources;
}

void SVGResources::invalidateClient(RenderObject* object) const
{
    // Ordinary resources
    if (m_clipper)
        m_clipper->invalidateClient(object);
#if ENABLE(FILTERS)
    if (m_filter)
        m_filter->invalidateClient(object);
#endif
    if (m_masker)
        m_masker->invalidateClient(object);
    if (m_markerStart)
        m_markerStart->invalidateClient(object);
    if (m_markerMid)
        m_markerMid->invalidateClient(object);
    if (m_markerEnd)
        m_markerEnd->invalidateClient(object);

    // Paint servers
    if (m_fill)
        m_fill->invalidateClient(object);
    if (m_stroke)
        m_stroke->invalidateClient(object);
}

void SVGResources::resourceDestroyed(RenderSVGResourceContainer* resource)
{
    ASSERT(resource);

    switch (resource->resourceType()) {
    case MaskerResourceType:
        if (m_masker == resource) {
            m_masker->invalidateClients();
            m_masker = 0;
        }
        break;
    case MarkerResourceType:
        if (m_markerStart == resource) {
            m_markerStart->invalidateClients();
            m_markerStart = 0;
        }

        if (m_markerMid == resource) {
            m_markerMid->invalidateClients();
            m_markerMid = 0;
        }

        if (m_markerEnd == resource) {
            m_markerEnd->invalidateClients();
            m_markerEnd = 0;
        }
        break;
    case PatternResourceType:
    case LinearGradientResourceType:
    case RadialGradientResourceType:
        if (m_fill == resource) {
            m_fill->invalidateClients();
            m_fill = 0;
        }

        if (m_stroke == resource) {
            m_stroke->invalidateClients();
            m_stroke = 0;
        }
        break;
#if ENABLE(FILTERS)
    case FilterResourceType:
        if (m_filter == resource) {
            m_filter->invalidateClients();
            m_filter = 0;
        }
        break;
#endif
    case ClipperResourceType:
        if (m_clipper == resource) {
            m_clipper->invalidateClients();
            m_clipper = 0;
        }
        break;
    case SolidColorResourceType:
        ASSERT_NOT_REACHED();
    }
}

void SVGResources::buildSetOfResources(HashSet<RenderSVGResourceContainer*>& set)
{
    // Ordinary resources
    if (m_clipper)
        set.add(m_clipper);
#if ENABLE(FILTERS)
    if (m_filter)
        set.add(m_filter);
#endif
    if (m_markerStart)
        set.add(m_markerStart);
    if (m_markerMid)
        set.add(m_markerMid);
    if (m_markerEnd)
        set.add(m_markerEnd);
    if (m_masker)
        set.add(m_masker);

    // Paint servers 
    if (m_fill)
        set.add(m_fill);
    if (m_stroke)
        set.add(m_stroke);
}

void SVGResources::resetClipper()
{
    ASSERT(m_clipper);
    m_clipper = 0;
}

#if ENABLE(FILTERS)
void SVGResources::resetFilter()
{
    ASSERT(m_filter);
    m_filter = 0;
}
#endif

void SVGResources::resetMarkerStart()
{
    ASSERT(m_markerStart);
    m_markerStart = 0;
}

void SVGResources::resetMarkerMid()
{
    ASSERT(m_markerMid);
    m_markerMid = 0;
}

void SVGResources::resetMarkerEnd()
{
    ASSERT(m_markerEnd);
    m_markerEnd = 0;
}

void SVGResources::resetMasker()
{
    ASSERT(m_masker);
    m_masker = 0;
}

void SVGResources::resetFill()
{
    ASSERT(m_fill);
    m_fill = 0;
}

void SVGResources::resetStroke()
{
    ASSERT(m_stroke);
    m_stroke = 0;
}

#ifndef NDEBUG
void SVGResources::dump(const RenderObject* object)
{
    ASSERT(object);
    ASSERT(object->node());

    fprintf(stderr, "-> this=%p, SVGResources(renderer=%p, node=%p)\n", this, object, object->node());
    fprintf(stderr, " | DOM Tree:\n");
    object->node()->showTreeForThis();

    fprintf(stderr, "\n | List of resources:\n");
    if (m_clipper)
        fprintf(stderr, " |-> Clipper    : %p (node=%p)\n", m_clipper, m_clipper->node());
#if ENABLE(FILTERS)
    if (m_filter)
        fprintf(stderr, " |-> Filter     : %p (node=%p)\n", m_filter, m_filter->node());
#endif
    if (m_markerStart)
        fprintf(stderr, " |-> MarkerStart: %p (node=%p)\n", m_markerStart, m_markerStart->node());
    if (m_markerMid)
        fprintf(stderr, " |-> MarkerMid  : %p (node=%p)\n", m_markerMid, m_markerMid->node());
    if (m_markerEnd)
        fprintf(stderr, " |-> MarkerEnd  : %p (node=%p)\n", m_markerEnd, m_markerEnd->node());
    if (m_masker)
        fprintf(stderr, " |-> Masker     : %p (node=%p)\n", m_masker, m_masker->node());
    if (m_fill)
        fprintf(stderr, " |-> Fill       : %p (node=%p)\n", m_fill, m_fill->node());
    if (m_stroke)
        fprintf(stderr, " |-> Stroke     : %p (node=%p)\n", m_stroke, m_stroke->node());
}
#endif

}

#endif
