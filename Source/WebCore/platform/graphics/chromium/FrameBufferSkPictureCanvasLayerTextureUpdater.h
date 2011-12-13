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


#ifndef FrameBufferSkPictureCanvasLayerTextureUpdater_h
#define FrameBufferSkPictureCanvasLayerTextureUpdater_h

#if USE(ACCELERATED_COMPOSITING)
#if USE(SKIA)

#include "SkPictureCanvasLayerTextureUpdater.h"

namespace WebCore {

class FrameBufferSkPictureCanvasLayerTextureUpdater : public SkPictureCanvasLayerTextureUpdater {
public:
    class Texture : public LayerTextureUpdater::Texture {
    public:
        Texture(FrameBufferSkPictureCanvasLayerTextureUpdater*, PassOwnPtr<ManagedTexture>);
        virtual ~Texture();

        virtual void updateRect(GraphicsContext3D*, TextureAllocator*, const IntRect& sourceRect, const IntRect& destRect);

    private:
        FrameBufferSkPictureCanvasLayerTextureUpdater* textureUpdater() { return m_textureUpdater; }

        FrameBufferSkPictureCanvasLayerTextureUpdater* m_textureUpdater;
    };

    static PassRefPtr<FrameBufferSkPictureCanvasLayerTextureUpdater> create(PassOwnPtr<LayerPainterChromium>);
    virtual ~FrameBufferSkPictureCanvasLayerTextureUpdater();

    virtual PassOwnPtr<LayerTextureUpdater::Texture> createTexture(TextureManager*);
    virtual SampledTexelFormat sampledTexelFormat(GC3Denum textureFormat);
    void updateTextureRect(GraphicsContext3D*, TextureAllocator*, ManagedTexture*, const IntRect& sourceRect, const IntRect& destRect);

private:
    explicit FrameBufferSkPictureCanvasLayerTextureUpdater(PassOwnPtr<LayerPainterChromium>);
};
} // namespace WebCore
#endif // USE(SKIA)
#endif // USE(ACCELERATED_COMPOSITING)
#endif // FrameBufferSkPictureCanvasLayerTextureUpdater_h
