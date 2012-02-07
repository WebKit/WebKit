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
#if USE(SKIA)

#include "BitmapSkPictureCanvasLayerTextureUpdater.h"

#include "LayerPainterChromium.h"
#include "SkCanvas.h"
#include "SkDevice.h"

namespace WebCore {

BitmapSkPictureCanvasLayerTextureUpdater::Texture::Texture(BitmapSkPictureCanvasLayerTextureUpdater* textureUpdater, PassOwnPtr<ManagedTexture> texture)
    : CanvasLayerTextureUpdater::Texture(texture)
    , m_textureUpdater(textureUpdater)
{
}

void BitmapSkPictureCanvasLayerTextureUpdater::Texture::prepareRect(const IntRect& sourceRect)
{
    m_device = adoptPtr(new SkDevice(SkBitmap::kARGB_8888_Config, sourceRect.width(), sourceRect.height()));
    OwnPtr<SkCanvas> canvas = adoptPtr(new SkCanvas(m_device.get()));
    textureUpdater()->paintContentsRect(canvas.get(), sourceRect);
}

void BitmapSkPictureCanvasLayerTextureUpdater::Texture::updateRect(GraphicsContext3D* context, TextureAllocator* allocator, const IntRect& sourceRect, const IntRect& destRect)
{
    texture()->bindTexture(context, allocator);

    ASSERT(m_device);
    const SkBitmap* bitmap = &m_device->accessBitmap(false);
    bitmap->lockPixels();
    textureUpdater()->updateTextureRect(context, texture()->format(), destRect, static_cast<uint8_t*>(bitmap->getPixels()));
    bitmap->unlockPixels();
    m_device.clear();
}

PassRefPtr<BitmapSkPictureCanvasLayerTextureUpdater> BitmapSkPictureCanvasLayerTextureUpdater::create(PassOwnPtr<LayerPainterChromium> painter, bool useMapTexSubImage)
{
    return adoptRef(new BitmapSkPictureCanvasLayerTextureUpdater(painter, useMapTexSubImage));
}

BitmapSkPictureCanvasLayerTextureUpdater::BitmapSkPictureCanvasLayerTextureUpdater(PassOwnPtr<LayerPainterChromium> painter, bool useMapTexSubImage)
    : SkPictureCanvasLayerTextureUpdater(painter)
    , m_texSubImage(useMapTexSubImage)
{
}

BitmapSkPictureCanvasLayerTextureUpdater::~BitmapSkPictureCanvasLayerTextureUpdater()
{
}

PassOwnPtr<LayerTextureUpdater::Texture> BitmapSkPictureCanvasLayerTextureUpdater::createTexture(TextureManager* manager)
{
    return adoptPtr(new Texture(this, ManagedTexture::create(manager)));
}

LayerTextureUpdater::SampledTexelFormat BitmapSkPictureCanvasLayerTextureUpdater::sampledTexelFormat(GC3Denum textureFormat)
{
    // The component order may be bgra if we uploaded bgra pixels to rgba textures.
    return PlatformColor::sameComponentOrder(textureFormat) ?
            LayerTextureUpdater::SampledTexelFormatRGBA : LayerTextureUpdater::SampledTexelFormatBGRA;
}

void BitmapSkPictureCanvasLayerTextureUpdater::prepareToUpdate(const IntRect& contentRect, const IntSize& tileSize, int borderTexels, float contentsScale, IntRect* resultingOpaqueRect)
{
    m_texSubImage.setSubImageSize(tileSize);
    SkPictureCanvasLayerTextureUpdater::prepareToUpdate(contentRect, tileSize, borderTexels, contentsScale, resultingOpaqueRect);
}

void BitmapSkPictureCanvasLayerTextureUpdater::paintContentsRect(SkCanvas* canvas, const IntRect& sourceRect)
{
    // Translate the origin of contentRect to that of sourceRect.
    canvas->translate(contentRect().x() - sourceRect.x(),
                      contentRect().y() - sourceRect.y());
    drawPicture(canvas);
}

void BitmapSkPictureCanvasLayerTextureUpdater::updateTextureRect(GraphicsContext3D* context, GC3Denum format, const IntRect& destRect, const uint8_t* pixels)
{
    m_texSubImage.upload(pixels, destRect, destRect, destRect, format, context);
}

} // namespace WebCore
#endif // USE(SKIA)
#endif // USE(ACCELERATED_COMPOSITING)
