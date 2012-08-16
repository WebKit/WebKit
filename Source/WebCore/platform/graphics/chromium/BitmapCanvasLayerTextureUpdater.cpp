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

#include "BitmapCanvasLayerTextureUpdater.h"

#include "LayerPainterChromium.h"
#include "PlatformColor.h"
#include "skia/ext/platform_canvas.h"

namespace WebCore {

BitmapCanvasLayerTextureUpdater::Texture::Texture(BitmapCanvasLayerTextureUpdater* textureUpdater, PassOwnPtr<CCPrioritizedTexture> texture)
    : LayerTextureUpdater::Texture(texture)
    , m_textureUpdater(textureUpdater)
{
}

BitmapCanvasLayerTextureUpdater::Texture::~Texture()
{
}

void BitmapCanvasLayerTextureUpdater::Texture::updateRect(CCResourceProvider* resourceProvider, const IntRect& sourceRect, const IntSize& destOffset)
{
    textureUpdater()->updateTextureRect(resourceProvider, texture(), sourceRect, destOffset);
}

PassRefPtr<BitmapCanvasLayerTextureUpdater> BitmapCanvasLayerTextureUpdater::create(PassOwnPtr<LayerPainterChromium> painter)
{
    return adoptRef(new BitmapCanvasLayerTextureUpdater(painter));
}

BitmapCanvasLayerTextureUpdater::BitmapCanvasLayerTextureUpdater(PassOwnPtr<LayerPainterChromium> painter)
    : CanvasLayerTextureUpdater(painter)
    , m_opaque(false)
{
}

BitmapCanvasLayerTextureUpdater::~BitmapCanvasLayerTextureUpdater()
{
}

PassOwnPtr<LayerTextureUpdater::Texture> BitmapCanvasLayerTextureUpdater::createTexture(CCPrioritizedTextureManager* manager)
{
    return adoptPtr(new Texture(this, CCPrioritizedTexture::create(manager)));
}

LayerTextureUpdater::SampledTexelFormat BitmapCanvasLayerTextureUpdater::sampledTexelFormat(GC3Denum textureFormat)
{
    // The component order may be bgra if we uploaded bgra pixels to rgba textures.
    return PlatformColor::sameComponentOrder(textureFormat) ?
            LayerTextureUpdater::SampledTexelFormatRGBA : LayerTextureUpdater::SampledTexelFormatBGRA;
}

void BitmapCanvasLayerTextureUpdater::prepareToUpdate(const IntRect& contentRect, const IntSize& tileSize, float contentsWidthScale, float contentsHeightScale, IntRect& resultingOpaqueRect, CCRenderingStats& stats)
{
    if (m_canvasSize != contentRect.size()) {
        m_canvasSize = contentRect.size();
        m_canvas = adoptPtr(skia::CreateBitmapCanvas(m_canvasSize.width(), m_canvasSize.height(), m_opaque));
    }

    paintContents(m_canvas.get(), contentRect, contentsWidthScale, contentsHeightScale, resultingOpaqueRect, stats);
}

void BitmapCanvasLayerTextureUpdater::updateTextureRect(CCResourceProvider* resourceProvider, CCPrioritizedTexture* texture, const IntRect& sourceRect, const IntSize& destOffset)
{
    const SkBitmap& bitmap = m_canvas->getDevice()->accessBitmap(false);
    bitmap.lockPixels();

    texture->upload(resourceProvider, static_cast<const uint8_t*>(bitmap.getPixels()), contentRect(), sourceRect, destOffset);
    bitmap.unlockPixels();
}

void BitmapCanvasLayerTextureUpdater::setOpaque(bool opaque)
{
    if (opaque != m_opaque) {
        m_canvas.clear();
        m_canvasSize = IntSize();
    }
    m_opaque = opaque;
}

} // namespace WebCore
#endif // USE(ACCELERATED_COMPOSITING)
