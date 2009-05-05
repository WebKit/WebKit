/*
    Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
    Copyright (C) 2006 Apple Computer, Inc.
    Copyright (C) 2007 Rob Buis <buis@kde.org>
    Copyright (C) 2009 Google, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef RenderSVGImage_h
#define RenderSVGImage_h

#if ENABLE(SVG)

#include "TransformationMatrix.h"
#include "FloatRect.h"
#include "RenderImage.h"
#include "SVGRenderSupport.h"

namespace WebCore {

    class SVGImageElement;
    class SVGPreserveAspectRatio;

    class RenderSVGImage : public RenderImage, SVGRenderBase {
    public:
        RenderSVGImage(SVGImageElement*);
        virtual ~RenderSVGImage();

        virtual const char* renderName() const { return "RenderSVGImage"; }
        virtual bool isSVGImage() const { return true; }

        virtual TransformationMatrix localToParentTransform() const { return m_localTransform; }

        virtual FloatRect objectBoundingBox() const;
        virtual FloatRect repaintRectInLocalCoordinates() const;

        virtual IntRect clippedOverflowRectForRepaint(RenderBoxModelObject* repaintContainer);
        virtual void computeRectForRepaint(RenderBoxModelObject* repaintContainer, IntRect&, bool fixed = false);

        virtual void mapLocalToContainer(RenderBoxModelObject* repaintContainer, bool useTransforms, bool fixed, TransformState&) const;

        virtual void absoluteRects(Vector<IntRect>&, int tx, int ty);
        virtual void absoluteQuads(Vector<FloatQuad>&);
        virtual void addFocusRingRects(GraphicsContext*, int tx, int ty);

        virtual void imageChanged(WrappedImagePtr, const IntRect* = 0);
        void adjustRectsForAspectRatio(FloatRect& destRect, FloatRect& srcRect, SVGPreserveAspectRatio*);
        
        virtual void layout();
        virtual void paint(PaintInfo&, int parentX, int parentY);

        bool requiresLayer() const { return false; }

        virtual bool nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint& pointInParent, HitTestAction);
        virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int _x, int _y, int _tx, int _ty, HitTestAction);

    private:
        virtual TransformationMatrix localTransform() const { return m_localTransform; }

        TransformationMatrix m_localTransform;
        FloatRect m_localBounds;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // RenderSVGImage_h

// vim:ts=4:noet
