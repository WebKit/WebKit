/**
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 *           (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
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

#ifndef SVGRenderBase_h
#define SVGRenderBase_h

#if ENABLE(SVG)
#include "RenderObject.h"

namespace WebCore {

    class SVGResourceFilter;
    class ImageBuffer;

    // SVGRendererBase is an abstract base class which all SVG renderers inherit
    // from in order to share SVG renderer code.
    // FIXME: This code can all move into RenderSVGModelObject once
    // all SVG renderers inherit from RenderSVGModelObject.
    class SVGRenderBase {
    public:
        // FIXME: These are only public for SVGRootInlineBox.
        // It's unclear if these should be exposed or not.  SVGRootInlineBox may
        // pass the wrong RenderObject* and boundingBox to these functions.
        static void prepareToRenderSVGContent(RenderObject*, RenderObject::PaintInfo&, const FloatRect& boundingBox, SVGResourceFilter*&, SVGResourceFilter* rootFilter = 0);
        static void finishRenderSVGContent(RenderObject*, RenderObject::PaintInfo&, SVGResourceFilter*&, GraphicsContext* savedContext);

    protected:
        static IntRect clippedOverflowRectForRepaint(RenderObject*, RenderBoxModelObject* repaintContainer);
        static void computeRectForRepaint(RenderObject*, RenderBoxModelObject* repaintContainer, IntRect&, bool fixed);

        static void mapLocalToContainer(const RenderObject*, RenderBoxModelObject* repaintContainer, bool useTransforms, bool fixed, TransformState&);

        // Used to share the "walk all the children" logic between objectBoundingBox
        // and repaintRectInLocalCoordinates in RenderSVGRoot and RenderSVGContainer
        static FloatRect computeContainerBoundingBox(const RenderObject* container, bool includeAllPaintedContent);

        // returns the filter bounding box (or the empty rect if no filter) in local coordinates
        static FloatRect filterBoundingBoxForRenderer(const RenderObject*);
    };

    // FIXME: This should move to RenderObject or PaintInfo
    // Used for transforming the GraphicsContext and damage rect before passing PaintInfo to child renderers.
    void applyTransformToPaintInfo(RenderObject::PaintInfo&, const TransformationMatrix& localToChildTransform);

    // This offers a way to render parts of a WebKit rendering tree into a ImageBuffer.
    void renderSubtreeToImage(ImageBuffer*, RenderObject*);

    void clampImageBufferSizeToViewport(FrameView*, IntSize& imageBufferSize);
} // namespace WebCore

#endif // ENABLE(SVG)

#endif // SVGRenderBase_h
