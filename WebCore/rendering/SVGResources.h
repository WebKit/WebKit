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

#ifndef SVGResources_h
#define SVGResources_h

#if ENABLE(SVG)
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class Document;
class RenderObject;
class RenderSVGResourceClipper;
class RenderSVGResourceContainer;
class RenderSVGResourceFilter;
class RenderSVGResourceMarker;
class RenderSVGResourceMasker;
class SVGRenderStyle;

// Holds a set of resources associated with a RenderObject
class SVGResources {
public:
    SVGResources();

    bool buildCachedResources(const RenderObject*, const SVGRenderStyle*);

    // Ordinary resources
    RenderSVGResourceClipper* clipper() const { return m_clipper; }
#if ENABLE(FILTERS)
    RenderSVGResourceFilter* filter() const { return m_filter; }
#endif
    RenderSVGResourceMarker* markerStart() const { return m_markerStart; }
    RenderSVGResourceMarker* markerMid() const { return m_markerMid; }
    RenderSVGResourceMarker* markerEnd() const { return m_markerEnd; }
    RenderSVGResourceMasker* masker() const { return m_masker; }

    // Paint servers
    RenderSVGResourceContainer* fill() const { return m_fill; }
    RenderSVGResourceContainer* stroke() const { return m_stroke; }

    void buildSetOfResources(HashSet<RenderSVGResourceContainer*>&);

    // Methods operating on all cached resources
    void invalidateClient(RenderObject*) const;
    void resourceDestroyed(RenderSVGResourceContainer*);

#ifndef NDEBUG
    void dump(const RenderObject*);
#endif

private:
    friend class SVGResourcesCycleSolver;

    // Only used by SVGResourcesCache cycle detection logic
    void resetClipper();
#if ENABLE(FILTERS)
    void resetFilter();
#endif
    void resetMarkerStart();
    void resetMarkerMid();
    void resetMarkerEnd();
    void resetMasker();
    void resetFill();
    void resetStroke();

private:
    // Ordinary resources
    RenderSVGResourceClipper* m_clipper;
#if ENABLE(FILTERS)
    RenderSVGResourceFilter* m_filter;
#endif
    RenderSVGResourceMarker* m_markerStart;
    RenderSVGResourceMarker* m_markerMid;
    RenderSVGResourceMarker* m_markerEnd;
    RenderSVGResourceMasker* m_masker;

    // Paint servers
    RenderSVGResourceContainer* m_fill;
    RenderSVGResourceContainer* m_stroke;
};

}

#endif
#endif
