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


#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "LayerTextureUpdaterCanvas.h"

#include "GraphicsContext.h"
#include "LayerPainterChromium.h"
#include "LayerTexture.h"
#include "TraceEvent.h"

namespace WebCore {

LayerTextureUpdaterCanvas::LayerTextureUpdaterCanvas(GraphicsContext3D* context, PassOwnPtr<LayerPainterChromium> painter)
    : LayerTextureUpdater(context)
    , m_painter(painter)
{
}

void LayerTextureUpdaterCanvas::paintContents(GraphicsContext& context, const IntRect& contentRect)
{
    context.translate(-contentRect.x(), -contentRect.y());
    {
        TRACE_EVENT("LayerTextureUpdaterCanvas::paint", this, 0);
        m_painter->paint(context, contentRect);
    }
    m_contentRect = contentRect;
}

LayerTextureUpdaterBitmap::LayerTextureUpdaterBitmap(GraphicsContext3D* context, PassOwnPtr<LayerPainterChromium> painter, bool useMapTexSubImage)
    : LayerTextureUpdaterCanvas(context, painter)
    , m_texSubImage(useMapTexSubImage)
{
}

void LayerTextureUpdaterBitmap::prepareToUpdate(const IntRect& contentRect, const IntSize& tileSize, int borderTexels)
{
    m_texSubImage.setSubImageSize(tileSize);

    m_canvas.resize(contentRect.size());
    // Assumption: if a tiler is using border texels, then it is because the
    // layer is likely to be filtered or transformed. Because of it might be
    // transformed, draw the text in grayscale instead of subpixel antialiasing.
    PlatformCanvas::Painter::TextOption textOption =
        borderTexels ? PlatformCanvas::Painter::GrayscaleText : PlatformCanvas::Painter::SubpixelText;
    PlatformCanvas::Painter canvasPainter(&m_canvas, textOption);
    paintContents(*canvasPainter.context(), contentRect);
}

void LayerTextureUpdaterBitmap::updateTextureRect(LayerTexture* texture, const IntRect& sourceRect, const IntRect& destRect)
{
    PlatformCanvas::AutoLocker locker(&m_canvas);

    texture->bindTexture();
    m_texSubImage.upload(locker.pixels(), contentRect(), sourceRect, destRect, context());
}

} // namespace WebCore
#endif // USE(ACCELERATED_COMPOSITING)

