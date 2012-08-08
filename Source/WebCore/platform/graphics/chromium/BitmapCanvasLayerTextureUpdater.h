/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef BitmapCanvasLayerTextureUpdater_h
#define BitmapCanvasLayerTextureUpdater_h

#if USE(ACCELERATED_COMPOSITING)

#include "CanvasLayerTextureUpdater.h"

class SkCanvas;

namespace WebCore {

class LayerPainterChromium;

// This class rasterizes the contentRect into a skia bitmap canvas. It then updates
// textures by copying from the canvas into the texture, using MapSubImage if
// possible.
class BitmapCanvasLayerTextureUpdater : public CanvasLayerTextureUpdater {
public:
    class Texture : public LayerTextureUpdater::Texture {
    public:
        Texture(BitmapCanvasLayerTextureUpdater*, PassOwnPtr<CCPrioritizedTexture>);
        virtual ~Texture();

        virtual void updateRect(CCResourceProvider*, const IntRect& sourceRect, const IntRect& destRect) OVERRIDE;

    private:
        BitmapCanvasLayerTextureUpdater* textureUpdater() { return m_textureUpdater; }

        BitmapCanvasLayerTextureUpdater* m_textureUpdater;
    };

    static PassRefPtr<BitmapCanvasLayerTextureUpdater> create(PassOwnPtr<LayerPainterChromium>);
    virtual ~BitmapCanvasLayerTextureUpdater();

    virtual PassOwnPtr<LayerTextureUpdater::Texture> createTexture(CCPrioritizedTextureManager*) OVERRIDE;
    virtual SampledTexelFormat sampledTexelFormat(GC3Denum textureFormat) OVERRIDE;
    virtual void prepareToUpdate(const IntRect& contentRect, const IntSize& tileSize, float contentsWidthScale, float contentsHeightScale, IntRect& resultingOpaqueRect, CCRenderingStats&) OVERRIDE;
    void updateTextureRect(CCResourceProvider*, CCPrioritizedTexture*, const IntRect& sourceRect, const IntRect& destRect);

    virtual void setOpaque(bool) OVERRIDE;

private:
    explicit BitmapCanvasLayerTextureUpdater(PassOwnPtr<LayerPainterChromium>);

    OwnPtr<SkCanvas> m_canvas;
    IntSize m_canvasSize;
    bool m_opaque;
};

} // namespace WebCore
#endif // USE(ACCELERATED_COMPOSITING)
#endif // BitmapCanvasLayerTextureUpdater_h
