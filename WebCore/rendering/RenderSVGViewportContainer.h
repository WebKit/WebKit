/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007 Rob Buis <buis@kde.org>
                  2009 Google, Inc.
    Copyright (C) 2009 Apple Inc. All rights reserved.

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

#ifndef RenderSVGViewportContainer_h
#define RenderSVGViewportContainer_h

#if ENABLE(SVG)
#include "RenderSVGContainer.h"

namespace WebCore {

// This is used for non-root <svg> elements and <marker> elements, neither of which are SVGTransformable
// thus we inherit from RenderSVGContainer instead of RenderSVGTransformableContainer
class RenderSVGViewportContainer : public RenderSVGContainer {
public:
    RenderSVGViewportContainer(SVGStyledElement*);

    // Calculates marker boundaries, mapped to the target element's coordinate space
    FloatRect markerBoundaries(const AffineTransform& markerTransformation) const;

    // Generates a transformation matrix usable to render marker content. Handles scaling the marker content
    // acording to SVGs markerUnits="strokeWidth" concept, when a strokeWidth value != -1 is passed in.
    AffineTransform markerContentTransformation(const AffineTransform& contentTransformation, const FloatPoint& origin, float strokeWidth = -1) const;

private:
    virtual bool isSVGContainer() const { return true; }
    virtual const char* renderName() const { return "RenderSVGViewportContainer"; }

    AffineTransform viewportTransform() const;
    virtual const AffineTransform& localToParentTransform() const;

    virtual void calcViewport();

    virtual void applyViewportClip(PaintInfo&);
    virtual bool pointIsInsideViewportClip(const FloatPoint& pointInParent);

    FloatRect m_viewport;
    mutable AffineTransform m_localToParentTransform;
};
  
inline RenderSVGViewportContainer* toRenderSVGViewportContainer(RenderObject* object)
{
    ASSERT(!object || !strcmp(object->renderName(), "RenderSVGViewportContainer"));
    return static_cast<RenderSVGViewportContainer*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderSVGViewportContainer(const RenderSVGViewportContainer*);

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // RenderSVGViewportContainer_h
