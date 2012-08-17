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

#include "BitmapSkPictureCanvasLayerTextureUpdater.h"

#include "CCRenderingStats.h"
#include "LayerPainterChromium.h"
#include "PlatformColor.h"
#include "SkCanvas.h"
#include "SkDevice.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

BitmapSkPictureCanvasLayerTextureUpdater::Texture::Texture(BitmapSkPictureCanvasLayerTextureUpdater* textureUpdater, PassOwnPtr<CCPrioritizedTexture> texture)
    : CanvasLayerTextureUpdater::Texture(texture)
    , m_textureUpdater(textureUpdater)
{
}

void BitmapSkPictureCanvasLayerTextureUpdater::Texture::prepareRect(const IntRect& sourceRect, CCRenderingStats& stats)
{
    m_bitmap.setConfig(SkBitmap::kARGB_8888_Config, sourceRect.width(), sourceRect.height());
    m_bitmap.allocPixels();
    m_bitmap.setIsOpaque(m_textureUpdater->layerIsOpaque());
    SkDevice device(m_bitmap);
    SkCanvas canvas(&device);
    double paintBeginTime = monotonicallyIncreasingTime();
    textureUpdater()->paintContentsRect(&canvas, sourceRect, stats);
    stats.totalPaintTimeInSeconds += monotonicallyIncreasingTime() - paintBeginTime;
}

void BitmapSkPictureCanvasLayerTextureUpdater::Texture::updateRect(CCResourceProvider* resourceProvider, const IntRect& sourceRect, const IntSize& destOffset)
{
    m_bitmap.lockPixels();
    texture()->upload(resourceProvider, static_cast<uint8_t*>(m_bitmap.getPixels()), sourceRect, sourceRect, destOffset);
    m_bitmap.unlockPixels();
    m_bitmap.reset();
}

PassRefPtr<BitmapSkPictureCanvasLayerTextureUpdater> BitmapSkPictureCanvasLayerTextureUpdater::create(PassOwnPtr<LayerPainterChromium> painter)
{
    return adoptRef(new BitmapSkPictureCanvasLayerTextureUpdater(painter));
}

BitmapSkPictureCanvasLayerTextureUpdater::BitmapSkPictureCanvasLayerTextureUpdater(PassOwnPtr<LayerPainterChromium> painter)
    : SkPictureCanvasLayerTextureUpdater(painter)
{
}

BitmapSkPictureCanvasLayerTextureUpdater::~BitmapSkPictureCanvasLayerTextureUpdater()
{
}

PassOwnPtr<LayerTextureUpdater::Texture> BitmapSkPictureCanvasLayerTextureUpdater::createTexture(CCPrioritizedTextureManager* manager)
{
    return adoptPtr(new Texture(this, CCPrioritizedTexture::create(manager)));
}

LayerTextureUpdater::SampledTexelFormat BitmapSkPictureCanvasLayerTextureUpdater::sampledTexelFormat(GC3Denum textureFormat)
{
    // The component order may be bgra if we uploaded bgra pixels to rgba textures.
    return PlatformColor::sameComponentOrder(textureFormat) ?
            LayerTextureUpdater::SampledTexelFormatRGBA : LayerTextureUpdater::SampledTexelFormatBGRA;
}

void BitmapSkPictureCanvasLayerTextureUpdater::paintContentsRect(SkCanvas* canvas, const IntRect& sourceRect, CCRenderingStats& stats)
{
    // Translate the origin of contentRect to that of sourceRect.
    canvas->translate(contentRect().x() - sourceRect.x(),
                      contentRect().y() - sourceRect.y());
    double rasterizeBeginTime = monotonicallyIncreasingTime();
    drawPicture(canvas);
    stats.totalRasterizeTimeInSeconds += monotonicallyIncreasingTime() - rasterizeBeginTime;
}

} // namespace WebCore
#endif // USE(ACCELERATED_COMPOSITING)
