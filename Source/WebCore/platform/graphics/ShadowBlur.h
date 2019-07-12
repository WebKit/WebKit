/*
 * Copyright (C) 2011 Apple Inc.
 * Copyright (C) 2010 Sencha, Inc.
 * Copyright (C) 2010 Igalia S.L.
 * All rights reserved.
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

#include "Color.h"
#include "FloatRect.h"
#include "FloatRoundedRect.h"
#include <wtf/Function.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class AffineTransform;
class GraphicsContext;
struct GraphicsContextState;
class ImageBuffer;

class ShadowBlur {
    WTF_MAKE_NONCOPYABLE(ShadowBlur);
public:
    enum ShadowType {
        NoShadow,
        SolidShadow,
        BlurShadow
    };

    ShadowBlur();
    ShadowBlur(const FloatSize& radius, const FloatSize& offset, const Color&, bool shadowsIgnoreTransforms = false);
    ShadowBlur(const GraphicsContextState&);

    void setShadowValues(const FloatSize&, const FloatSize& , const Color&, bool ignoreTransforms = false);

    void setShadowsIgnoreTransforms(bool ignoreTransforms) { m_shadowsIgnoreTransforms = ignoreTransforms; }
    bool shadowsIgnoreTransforms() const { return m_shadowsIgnoreTransforms; }

    void drawRectShadow(GraphicsContext&, const FloatRoundedRect&);
    void drawInsetShadow(GraphicsContext&, const FloatRect&, const FloatRoundedRect& holeRect);

    using DrawBufferCallback = WTF::Function<void(ImageBuffer&, const FloatPoint&, const FloatSize&)>;
    using DrawImageCallback = WTF::Function<void(ImageBuffer&, const FloatRect&, const FloatRect&)>;
    using FillRectCallback = WTF::Function<void(const FloatRect&, const Color&)>;
    using FillRectWithHoleCallback = WTF::Function<void(const FloatRect&, const FloatRect&, const Color&)>;
    using DrawShadowCallback = WTF::Function<void(GraphicsContext&)>;

    // DrawBufferCallback is for drawing shadow without tiling.
    // DrawImageCallback and FillRectCallback is for drawing shadow with tiling.
    void drawRectShadow(const AffineTransform&, const IntRect& clipBounds, const FloatRoundedRect& shadowedRect, const DrawBufferCallback&, const DrawImageCallback&, const FillRectCallback&);
    void drawInsetShadow(const AffineTransform&, const IntRect& clipBounds, const FloatRect& fullRect, const FloatRoundedRect& holeRect, const DrawBufferCallback&, const DrawImageCallback&, const FillRectWithHoleCallback&);
    void drawShadowLayer(const AffineTransform&, const IntRect& clipBounds, const FloatRect& layerArea, const DrawShadowCallback&, const DrawBufferCallback&);

    void blurLayerImage(unsigned char*, const IntSize&, int stride);

    void clear();

    ShadowType type() const { return m_type; }

private:
    void updateShadowBlurValues();

    void drawShadowBuffer(GraphicsContext&, ImageBuffer&, const FloatPoint&, const FloatSize&);

    void adjustBlurRadius(const AffineTransform&);

    enum ShadowDirection {
        OuterShadow,
        InnerShadow
    };

    struct LayerImageProperties {
        FloatSize shadowedResultSize; // Size of the result of shadowing which is same as shadowedRect + blurred edges.
        FloatPoint layerOrigin; // Top-left corner of the (possibly clipped) bounding rect to draw the shadow to.
        FloatSize layerSize; // Size of layerImage pixels that need blurring.
        FloatSize layerContextTranslation; // Translation to apply to layerContext for the shadow to be correctly clipped.
    };

    Optional<ShadowBlur::LayerImageProperties> calculateLayerBoundingRect(const AffineTransform&, const FloatRect& layerArea, const IntRect& clipRect);
    IntSize templateSize(const IntSize& blurredEdgeSize, const FloatRoundedRect::Radii&) const;

    void blurShadowBuffer(ImageBuffer& layerImage, const IntSize& templateSize);
    void blurAndColorShadowBuffer(ImageBuffer& layerImage, const IntSize& templateSize);

    void drawInsetShadowWithoutTiling(const AffineTransform&, const FloatRect& fullRect, const FloatRoundedRect& holeRect, const LayerImageProperties&, const DrawBufferCallback&);
    void drawInsetShadowWithTiling(const AffineTransform&, const FloatRect& fullRect, const FloatRoundedRect& holeRect, const IntSize& shadowTemplateSize, const IntSize& blurredEdgeSize, const DrawImageCallback&, const FillRectWithHoleCallback&);

    void drawRectShadowWithoutTiling(const AffineTransform&, const FloatRoundedRect& shadowedRect, const LayerImageProperties&, const DrawBufferCallback&);
    void drawRectShadowWithTiling(const AffineTransform&, const FloatRoundedRect& shadowedRect, const IntSize& shadowTemplateSize, const IntSize& blurredEdgeSize, const DrawImageCallback&, const FillRectCallback&, const LayerImageProperties&);

    void drawLayerPiecesAndFillCenter(ImageBuffer& layerImage, const FloatRect& shadowBounds, const FloatRoundedRect::Radii&, const IntSize& roundedRadius, const IntSize& templateSize, const DrawImageCallback&, const FillRectCallback&);
    void drawLayerPieces(ImageBuffer& layerImage, const FloatRect& shadowBounds, const FloatRoundedRect::Radii&, const IntSize& roundedRadius, const IntSize& templateSize, const DrawImageCallback&);

    IntSize blurredEdgeSize() const;

    ShadowType m_type { NoShadow };

    Color m_color;
    FloatSize m_blurRadius;
    FloatSize m_offset;

    bool m_shadowsIgnoreTransforms { false };
};

} // namespace WebCore
