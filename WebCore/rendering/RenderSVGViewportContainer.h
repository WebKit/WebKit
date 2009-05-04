/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007 Rob Buis <buis@kde.org>
                  2009 Google, Inc.

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
    ~RenderSVGViewportContainer();

    virtual bool isSVGContainer() const { return true; }
    virtual const char* renderName() const { return "RenderSVGViewportContainer"; }

    virtual void paint(PaintInfo&, int parentX, int parentY);

    virtual TransformationMatrix localToParentTransform() const;

    // FIXME: This override should be removed once callers of RenderBox::absoluteTransform() can be removed.
    virtual TransformationMatrix absoluteTransform() const;

    FloatRect viewport() const;

    // FIXME: This is only public for SVGResourceMarker::draw, likely the callsite should be changed.
    TransformationMatrix viewportTransform() const;

private:
    virtual void calcViewport();

    virtual void applyViewportClip(PaintInfo&);
    virtual bool pointIsInsideViewportClip(const FloatPoint& pointInParent);

    FloatRect m_viewport;
};
  
} // namespace WebCore

#endif // ENABLE(SVG)
#endif // RenderSVGViewportContainer_h

// vim:ts=4:noet
