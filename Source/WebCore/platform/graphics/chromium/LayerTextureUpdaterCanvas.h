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


#ifndef LayerTextureUpdaterCanvas_h
#define LayerTextureUpdaterCanvas_h

#if USE(ACCELERATED_COMPOSITING)

#include "GraphicsTypes3D.h"
#include "LayerTextureSubImage.h"
#include "LayerTextureUpdater.h"
#include "PlatformCanvas.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>

#if USE(SKIA)
#include "SkPicture.h"

class GrContext;
class SkCanvas;
class SkPicture;
#endif

namespace WebCore {

class GraphicsContext3D;
class LayerPainterChromium;

// A LayerTextureUpdater with an internal canvas.
class LayerTextureUpdaterCanvas : public LayerTextureUpdater {
protected:
    explicit LayerTextureUpdaterCanvas(PassOwnPtr<LayerPainterChromium>);

    void paintContents(GraphicsContext&, const IntRect& contentRect, float contentsScale);
    const IntRect& contentRect() const { return m_contentRect; }

private:

    IntRect m_contentRect;
    OwnPtr<LayerPainterChromium> m_painter;
};

// A LayerTextureUpdater with an internal bitmap canvas.
class LayerTextureUpdaterBitmap : public LayerTextureUpdaterCanvas {
public:
    static PassRefPtr<LayerTextureUpdaterBitmap> create(PassOwnPtr<LayerPainterChromium>, bool useMapTexSubImage);

    virtual Orientation orientation() { return LayerTextureUpdater::BottomUpOrientation; }
    virtual SampledTexelFormat sampledTexelFormat(GC3Denum textureFormat);
    virtual void prepareToUpdate(const IntRect& contentRect, const IntSize& tileSize, int borderTexels, float contentsScale);
    virtual void updateTextureRect(GraphicsContext3D*, TextureAllocator*, ManagedTexture*, const IntRect& sourceRect, const IntRect& destRect);

private:
    LayerTextureUpdaterBitmap(PassOwnPtr<LayerPainterChromium>, bool useMapTexSubImage);
    PlatformCanvas m_canvas;
    LayerTextureSubImage m_texSubImage;
};

#if USE(SKIA)
class LayerTextureUpdaterSkPicture : public LayerTextureUpdaterCanvas {
public:
    static PassRefPtr<LayerTextureUpdaterSkPicture> create(PassOwnPtr<LayerPainterChromium>);
    virtual ~LayerTextureUpdaterSkPicture();

    virtual Orientation orientation() { return LayerTextureUpdater::TopDownOrientation; }
    virtual SampledTexelFormat sampledTexelFormat(GC3Denum textureFormat);
    virtual void prepareToUpdate(const IntRect& contentRect, const IntSize& tileSize, int borderTexels, float contentsScale);
    virtual void updateTextureRect(GraphicsContext3D*, TextureAllocator*, ManagedTexture*, const IntRect& sourceRect, const IntRect& destRect);

private:
    explicit LayerTextureUpdaterSkPicture(PassOwnPtr<LayerPainterChromium>);
  
    SkPicture m_picture; // Recording canvas.
};
#endif // USE(SKIA)

} // namespace WebCore
#endif // USE(ACCELERATED_COMPOSITING)
#endif // LayerTextureUpdaterCanvas_h
