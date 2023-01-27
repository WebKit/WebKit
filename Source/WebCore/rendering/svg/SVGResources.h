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

#pragma once

#include "RenderSVGResourceClipper.h"
#include "RenderSVGResourceFilter.h"
#include "RenderSVGResourceMarker.h"
#include "RenderSVGResourceMasker.h"
#include <memory>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

class Document;
class RenderElement;
class RenderObject;
class RenderStyle;
class RenderSVGResourceContainer;
class LegacyRenderSVGRoot;
class SVGRenderStyle;

// Holds a set of resources associated with a RenderObject
class SVGResources {
    WTF_MAKE_NONCOPYABLE(SVGResources); WTF_MAKE_FAST_ALLOCATED;
public:
    SVGResources();

    bool buildCachedResources(const RenderElement&, const RenderStyle&);
    void layoutDifferentRootIfNeeded(const LegacyRenderSVGRoot*);

    // Ordinary resources
    RenderSVGResourceClipper* clipper() const { return m_clipperFilterMaskerData ? m_clipperFilterMaskerData->clipper.get() : nullptr; }
    RenderSVGResourceMarker* markerStart() const { return m_markerData ? m_markerData->markerStart.get() : nullptr; }
    RenderSVGResourceMarker* markerMid() const { return m_markerData ? m_markerData->markerMid.get() : nullptr; }
    RenderSVGResourceMarker* markerEnd() const { return m_markerData ? m_markerData->markerEnd.get() : nullptr; }
    bool markerReverseStart() const;
    RenderSVGResourceMasker* masker() const { return m_clipperFilterMaskerData ? m_clipperFilterMaskerData->masker.get() : nullptr; }
    RenderSVGResourceFilter* filter() const { return m_clipperFilterMaskerData ? m_clipperFilterMaskerData->filter.get() : nullptr; }

    // Paint servers
    RenderSVGResourceContainer* fill() const { return m_fillStrokeData ? m_fillStrokeData->fill.get() : nullptr; }
    RenderSVGResourceContainer* stroke() const { return m_fillStrokeData ? m_fillStrokeData->stroke.get() : nullptr; }

    // Chainable resources - linked through xlink:href
    RenderSVGResourceContainer* linkedResource() const { return m_linkedResource.get(); }

    void buildSetOfResources(WeakHashSet<RenderSVGResourceContainer>&);

    // Methods operating on all cached resources
    void removeClientFromCache(RenderElement&, bool markForInvalidation = true) const;
    // Returns true if the resource-to-be-destroyed is one of our resources.
    bool resourceDestroyed(RenderSVGResourceContainer&);

#if ENABLE(TREE_DEBUGGING)
    void dump(const RenderObject*);
#endif

private:
    friend class SVGResourcesCycleSolver;

    // Only used by SVGResourcesCache cycle detection logic
    void resetClipper();
    void resetFilter();
    void resetMarkerStart();
    void resetMarkerMid();
    void resetMarkerEnd();
    void resetMasker();
    void resetFill();
    void resetStroke();
    void resetLinkedResource();

private:
    bool setClipper(RenderSVGResourceClipper*);
    bool setFilter(RenderSVGResourceFilter*);
    bool setMarkerStart(RenderSVGResourceMarker*);
    bool setMarkerMid(RenderSVGResourceMarker*);
    bool setMarkerEnd(RenderSVGResourceMarker*);
    bool setMasker(RenderSVGResourceMasker*);
    bool setFill(RenderSVGResourceContainer*);
    bool setStroke(RenderSVGResourceContainer*);
    bool setLinkedResource(RenderSVGResourceContainer*);
    
    bool isEmpty() const { return !m_clipperFilterMaskerData && !m_markerData && !m_fillStrokeData && !m_linkedResource; }

    // From SVG 1.1 2nd Edition
    // clipper: 'container elements' and 'graphics elements'
    // filter:  'container elements' and 'graphics elements'
    // masker:  'container elements' and 'graphics elements'
    // -> a, circle, defs, ellipse, glyph, g, image, line, marker, mask, missing-glyph, path, pattern, polygon, polyline, rect, svg, switch, symbol, text, use
    struct ClipperFilterMaskerData {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        ClipperFilterMaskerData() = default;
        WeakPtr<RenderSVGResourceClipper> clipper;
        WeakPtr<RenderSVGResourceFilter> filter;
        WeakPtr<RenderSVGResourceMasker> masker;
    };

    // From SVG 1.1 2nd Edition
    // marker: line, path, polygon, polyline
    struct MarkerData {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        MarkerData() = default;
        WeakPtr<RenderSVGResourceMarker> markerStart;
        WeakPtr<RenderSVGResourceMarker> markerMid;
        WeakPtr<RenderSVGResourceMarker> markerEnd;
    };

    // From SVG 1.1 2nd Edition
    // fill:       'shapes' and 'text content elements'
    // stroke:     'shapes' and 'text content elements'
    // -> altGlyph, circle, ellipse, line, path, polygon, polyline, rect, text, textPath, tref, tspan
    struct FillStrokeData {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        FillStrokeData() = default;
        WeakPtr<RenderSVGResourceContainer> fill;
        WeakPtr<RenderSVGResourceContainer> stroke;
    };

    std::unique_ptr<ClipperFilterMaskerData> m_clipperFilterMaskerData;
    std::unique_ptr<MarkerData> m_markerData;
    std::unique_ptr<FillStrokeData> m_fillStrokeData;
    WeakPtr<RenderSVGResourceContainer> m_linkedResource;
};

} // namespace WebCore
