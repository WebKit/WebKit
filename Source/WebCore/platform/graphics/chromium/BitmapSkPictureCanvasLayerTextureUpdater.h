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
#include "SkBitmap.h"
#include "SkPictureCanvasLayerTextureUpdater.h"

namespace WebCore {

// This class records the contentRect into an SkPicture, then software rasterizes
// the SkPicture into bitmaps for each tile. This implements CCSettings::perTilePainting.
class BitmapSkPictureCanvasLayerTextureUpdater : public SkPictureCanvasLayerTextureUpdater {
public:
    class Texture : public CanvasLayerTextureUpdater::Texture {
    public:
        Texture(BitmapSkPictureCanvasLayerTextureUpdater*, PassOwnPtr<CCPrioritizedTexture>);

        virtual void prepareRect(const IntRect& sourceRect, CCRenderingStats&) OVERRIDE;
        virtual void updateRect(CCResourceProvider*, const IntRect& sourceRect, const IntSize& destOffset) OVERRIDE;

    private:
        BitmapSkPictureCanvasLayerTextureUpdater* textureUpdater() { return m_textureUpdater; }

        SkBitmap m_bitmap;
        BitmapSkPictureCanvasLayerTextureUpdater* m_textureUpdater;
    };

    static PassRefPtr<BitmapSkPictureCanvasLayerTextureUpdater> create(PassOwnPtr<LayerPainterChromium>);
    virtual ~BitmapSkPictureCanvasLayerTextureUpdater();

    virtual PassOwnPtr<LayerTextureUpdater::Texture> createTexture(CCPrioritizedTextureManager*) OVERRIDE;
    virtual SampledTexelFormat sampledTexelFormat(GC3Denum textureFormat) OVERRIDE;
    void paintContentsRect(SkCanvas*, const IntRect& sourceRect, CCRenderingStats&);

private:
    explicit BitmapSkPictureCanvasLayerTextureUpdater(PassOwnPtr<LayerPainterChromium>);
};
} // namespace WebCore
#endif // USE(ACCELERATED_COMPOSITING)
#endif // BitmapSkPictureCanvasLayerTextureUpdater_h
