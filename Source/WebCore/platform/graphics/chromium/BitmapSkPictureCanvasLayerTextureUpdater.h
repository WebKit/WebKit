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


#ifndef BitmapSkPictureCanvasLayerTextureUpdater_h
#define BitmapSkPictureCanvasLayerTextureUpdater_h

#if USE(ACCELERATED_COMPOSITING)
#if USE(SKIA)

#include "LayerTextureSubImage.h"
#include "PlatformColor.h"
#include "SkPictureCanvasLayerTextureUpdater.h"

namespace WebCore {

class BitmapSkPictureCanvasLayerTextureUpdater : public SkPictureCanvasLayerTextureUpdater {
public:
    class Texture : public CanvasLayerTextureUpdater::Texture {
    public:
        Texture(BitmapSkPictureCanvasLayerTextureUpdater*, PassOwnPtr<ManagedTexture>);

        virtual void prepareRect(const IntRect& sourceRect);
        virtual void updateRect(GraphicsContext3D*, TextureAllocator*, const IntRect& sourceRect, const IntRect& destRect);

    private:
        BitmapSkPictureCanvasLayerTextureUpdater* textureUpdater() { return m_textureUpdater; }

        OwnArrayPtr<uint8_t> m_pixelData;
        BitmapSkPictureCanvasLayerTextureUpdater* m_textureUpdater;
    };

    static PassRefPtr<BitmapSkPictureCanvasLayerTextureUpdater> create(PassOwnPtr<LayerPainterChromium>, bool useMapTexSubImage);
    virtual ~BitmapSkPictureCanvasLayerTextureUpdater();

    virtual PassOwnPtr<LayerTextureUpdater::Texture> createTexture(TextureManager*);
    virtual Orientation orientation() { return LayerTextureUpdater::BottomUpOrientation; }
    virtual SampledTexelFormat sampledTexelFormat(GC3Denum textureFormat);
    virtual void prepareToUpdate(const IntRect& contentRect, const IntSize& tileSize, int borderTexels, float contentsScale);
    void paintContentsRect(SkCanvas*, const IntRect& sourceRect);
    void updateTextureRect(GraphicsContext3D*, GC3Denum format, const IntRect& destRect, const uint8_t* pixels);

private:
    BitmapSkPictureCanvasLayerTextureUpdater(PassOwnPtr<LayerPainterChromium>, bool useMapTexSubImage);

    LayerTextureSubImage m_texSubImage;
};
} // namespace WebCore
#endif // USE(SKIA)
#endif // USE(ACCELERATED_COMPOSITING)
#endif // BitmapSkPictureCanvasLayerTextureUpdater_h
