/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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
 *
 */

#ifndef RenderSVGHiddenContainer_h
#define RenderSVGHiddenContainer_h

#if ENABLE(SVG)

#include "RenderSVGContainer.h"

namespace WebCore {
    
    class SVGStyledElement;
    
    // This class is for containers which are never drawn, but do need to support style
    // <defs>, <linearGradient>, <radialGradient> are all good examples
    class RenderSVGHiddenContainer : public RenderSVGContainer {
    public:
        RenderSVGHiddenContainer(SVGStyledElement*);
        virtual ~RenderSVGHiddenContainer();

        virtual bool isSVGContainer() const { return true; }
        virtual bool isSVGHiddenContainer() const { return true; }

        virtual const char* renderName() const { return "RenderSVGHiddenContainer"; }

        virtual bool requiresLayer() const { return false; }

        virtual void layout();
        virtual void paint(PaintInfo&, int parentX, int parentY);
        
        virtual IntRect clippedOverflowRectForRepaint(RenderBoxModelObject* repaintContainer);
        virtual void absoluteRects(Vector<IntRect>& rects, int tx, int ty);
        virtual void absoluteQuads(Vector<FloatQuad>&);

        // FIXME: This override only exists to match existing LayoutTest results.
        virtual TransformationMatrix absoluteTransform() const { return TransformationMatrix(); }

        virtual FloatRect objectBoundingBox() const;
        virtual FloatRect repaintRectInLocalCoordinates() const;

        virtual bool nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint& pointInParent, HitTestAction);
    };
}

#endif // ENABLE(SVG)
#endif // RenderSVGHiddenContainer_h
