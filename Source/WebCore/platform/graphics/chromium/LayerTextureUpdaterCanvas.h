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

#include "LayerTextureSubImage.h"
#include "LayerTextureUpdater.h"
#include "PlatformCanvas.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class GraphicsContext3D;
class LayerPainterChromium;

// A LayerTextureUpdater with an internal canvas.
class LayerTextureUpdaterCanvas : public LayerTextureUpdater {
public:
    LayerTextureUpdaterCanvas(GraphicsContext3D*, PassOwnPtr<LayerPainterChromium>);
    virtual ~LayerTextureUpdaterCanvas() { }

protected:
    void paintContents(GraphicsContext&, const IntRect& contentRect);
    const IntRect& contentRect() const { return m_contentRect; }

private:
    IntRect m_contentRect;
    OwnPtr<LayerPainterChromium> m_painter;
};

// A LayerTextureUpdater with an internal bitmap canvas.
class LayerTextureUpdaterBitmap : public LayerTextureUpdaterCanvas {
public:
    LayerTextureUpdaterBitmap(GraphicsContext3D*, PassOwnPtr<LayerPainterChromium>, bool useMapTexSubImage);
    virtual ~LayerTextureUpdaterBitmap() { }

    virtual Orientation orientation() { return LayerTextureUpdater::BottomUpOrientation; }
    virtual void prepareToUpdate(const IntRect& contentRect, const IntSize& tileSize, int borderTexels);
    virtual void updateTextureRect(LayerTexture*, const IntRect& sourceRect, const IntRect& destRect);

private:
    PlatformCanvas m_canvas;
    LayerTextureSubImage m_texSubImage;
};

} // namespace WebCore
#endif // USE(ACCELERATED_COMPOSITING)
#endif // LayerTextureUpdaterCanvas_h

