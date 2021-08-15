/**
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2012 Zoltan Herczeg <zherczeg@webkit.org>.
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

#include "ImageBuffer.h"
#include "PaintInfo.h"

namespace WebCore {

class AffineTransform;
class FloatRect;
class RenderElement;
class RenderObject;
class RenderSVGResourceFilter;

// SVGRenderingContext 
class SVGRenderingContext {
public:
    enum NeedsGraphicsContextSave {
        SaveGraphicsContext,
        DontSaveGraphicsContext,
    };

    // Does not start rendering.
    SVGRenderingContext()
    {
    }

    SVGRenderingContext(RenderElement& object, PaintInfo& paintinfo, NeedsGraphicsContextSave needsGraphicsContextSave = DontSaveGraphicsContext)
    {
        prepareToRenderSVGContent(object, paintinfo, needsGraphicsContextSave);
    }

    // Automatically finishes context rendering.
    ~SVGRenderingContext();

    // Used by all SVG renderers who apply clip/filter/etc. resources to the renderer content.
    void prepareToRenderSVGContent(RenderElement&, PaintInfo&, NeedsGraphicsContextSave = DontSaveGraphicsContext);
    bool isRenderingPrepared() const { return m_renderingFlags & RenderingPrepared; }

    static RefPtr<ImageBuffer> createImageBuffer(const FloatRect& targetRect, const AffineTransform& absoluteTransform, const DestinationColorSpace&, RenderingMode, const GraphicsContext* = nullptr);
    static RefPtr<ImageBuffer> createImageBuffer(const FloatRect& targetRect, const FloatRect& clampedRect, const DestinationColorSpace&, RenderingMode, const GraphicsContext* = nullptr);

    static void renderSubtreeToContext(GraphicsContext&, RenderElement&, const AffineTransform&);
    static void clipToImageBuffer(GraphicsContext&, const AffineTransform& absoluteTransform, const FloatRect& targetRect, RefPtr<ImageBuffer>&, bool safeToClear);

    static float calculateScreenFontSizeScalingFactor(const RenderObject&);
    static AffineTransform calculateTransformationToOutermostCoordinateSystem(const RenderObject&);
    static void clear2DRotation(AffineTransform&);

    static IntRect calculateImageBufferRect(const FloatRect& targetRect, const AffineTransform& absoluteTransform)
    {
        return enclosingIntRect(absoluteTransform.mapRect(targetRect));
    }

    // Support for the buffered-rendering hint.
    bool bufferForeground(RefPtr<ImageBuffer>&);

private:
    // To properly revert partially successful initializtions in the destructor, we record all successful steps.
    enum RenderingFlags {
        RenderingPrepared = 1,
        RestoreGraphicsContext = 1 << 1,
        EndOpacityLayer = 1 << 2,
        EndFilterLayer = 1 << 3,
        PrepareToRenderSVGContentWasCalled = 1 << 4
    };

    // List of those flags which require actions during the destructor.
    static constexpr int ActionsNeeded = RestoreGraphicsContext | EndOpacityLayer | EndFilterLayer;

    RenderElement* m_renderer { nullptr };
    PaintInfo* m_paintInfo { nullptr };
    GraphicsContext* m_savedContext  { nullptr };
    RenderSVGResourceFilter* m_filter  { nullptr };
    LayoutRect m_savedPaintRect;
    int m_renderingFlags { 0 };
};

} // namespace WebCore
