/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "GraphicsContext.h"
#include "LayoutRect.h"
#include "RegionContext.h"
#include "RenderLayerModelObject.h"
#include "RenderStyleConstants.h"
#include "StylePathOperationWrappers.h"

namespace WebCore {

class LegacyRenderSVGResourceClipper;
class RenderSVGResourceClipper;

// Applies a clip-path to a renderer and keeps it active for the scope's lifetime. Handles CSS
// basic-shape and box clip-paths and SVG clipper resources, so it works for both CSS boxes and SVG.
// Used by the layer paint path (RenderLayer::setupClipPath) and the SVG non-layer path
// (SVGNonLayerClippingAndMaskingScope).
//
// needsMaskClipping() is true when the clip can't be a path and the caller must paint the clipper as a
// destination-in mask, using the layer's transparency layer on the layer path and its own otherwise.
class ClipPathPaintScope {
    WTF_MAKE_NONCOPYABLE(ClipPathPaintScope);
public:
    // Where the CTM sits when the clip is applied, which sets how the clip origin is computed:
    //   LayerPaint    - CTM is at the renderer (the layer transform already placed it).
    //   NonLayerPaint - CTM is at the renderer's container, so the clip origin also adds the
    //                   renderer's currentSVGLayoutLocation. Used by the SVG non-layer paint path.
    enum class CoordinateMode : bool {
        LayerPaint,
        NonLayerPaint
    };

    ClipPathPaintScope(GraphicsContext&, RegionContext*, RenderLayerModelObject&, const LayoutSize& offsetFromRoot, const LayoutSize& subpixelOffset, const LayoutRect& clippedContentBounds, bool isCollectingEventRegion, CoordinateMode);

    bool needsMaskClipping() const { return m_needsMaskClipping; }

    static std::pair<Path, WindRule> computeClipPath(const RenderLayerModelObject&, const LayoutSize& offsetFromRoot, const LayoutRect& rootRelativeBoundsForNonBoxes);
    static FloatRect referenceBoxRectForClipPath(const RenderLayerModelObject&, CSSBoxType, const LayoutSize& offsetFromRoot, const LayoutRect& rootRelativeBounds);

private:
    // Applies one kind of clip-path. The constructor picks which based on the renderer's clip-path.
    void applyBasicShapeOrBoxClip(GraphicsContext&, RenderLayerModelObject&, const LayoutSize& offsetFromRoot, const LayoutSize& subpixelOffset, const LayoutRect& clippedContentBounds, bool isCollectingEventRegion, CoordinateMode);
    void applySVGResourceClip(GraphicsContext&, RenderLayerModelObject&, RenderSVGResourceClipper&, const LayoutSize& offsetFromRoot, const LayoutSize& subpixelOffset, const LayoutRect& clippedContentBounds, CoordinateMode);
    void applyLegacySVGResourceClip(GraphicsContext&, RenderLayerModelObject&, LegacyRenderSVGResourceClipper&, const LayoutSize& offsetFromRoot, const LayoutRect& clippedContentBounds);

    GraphicsContextStateSaver m_clipSaver;
    RegionContextStateSaver m_regionSaver;
    bool m_needsMaskClipping : 1 { false };
};

} // namespace WebCore
