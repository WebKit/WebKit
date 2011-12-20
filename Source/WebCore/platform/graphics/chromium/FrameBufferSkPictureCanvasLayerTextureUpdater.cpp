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

#include "FrameBufferSkPictureCanvasLayerTextureUpdater.h"

#include "LayerPainterChromium.h"
#include "SkCanvas.h"
#include "SkGpuDevice.h"

namespace WebCore {

static PassOwnPtr<SkCanvas> createAcceleratedCanvas(GraphicsContext3D* context,
                                                    TextureAllocator* allocator,
                                                    ManagedTexture* texture)
{
    // Allocate so that we have a valid texture id.
    texture->allocate(allocator);

    GrContext* grContext = context->grContext();
    IntSize canvasSize = texture->size();
    GrPlatformTextureDesc textureDesc;
    textureDesc.fFlags = kRenderTarget_GrPlatformTextureFlag;
    textureDesc.fWidth = canvasSize.width();
    textureDesc.fHeight = canvasSize.height();
    textureDesc.fConfig = kRGBA_8888_GrPixelConfig;
    textureDesc.fTextureHandle = texture->textureId();
    SkAutoTUnref<GrTexture> target(grContext->createPlatformTexture(textureDesc));
    SkAutoTUnref<SkDevice> device(new SkGpuDevice(grContext, target.get()));
    return adoptPtr(new SkCanvas(device.get()));
}

FrameBufferSkPictureCanvasLayerTextureUpdater::Texture::Texture(FrameBufferSkPictureCanvasLayerTextureUpdater* textureUpdater, PassOwnPtr<ManagedTexture> texture)
    : LayerTextureUpdater::Texture(texture)
    , m_textureUpdater(textureUpdater)
{
}

FrameBufferSkPictureCanvasLayerTextureUpdater::Texture::~Texture()
{
}

void FrameBufferSkPictureCanvasLayerTextureUpdater::Texture::updateRect(GraphicsContext3D* context, TextureAllocator* allocator, const IntRect& sourceRect, const IntRect& destRect)
{
    textureUpdater()->updateTextureRect(context, allocator, texture(), sourceRect, destRect);
}

PassRefPtr<FrameBufferSkPictureCanvasLayerTextureUpdater> FrameBufferSkPictureCanvasLayerTextureUpdater::create(PassOwnPtr<LayerPainterChromium> painter)
{
    return adoptRef(new FrameBufferSkPictureCanvasLayerTextureUpdater(painter));
}

FrameBufferSkPictureCanvasLayerTextureUpdater::FrameBufferSkPictureCanvasLayerTextureUpdater(PassOwnPtr<LayerPainterChromium> painter)
    : SkPictureCanvasLayerTextureUpdater(painter)
{
}

FrameBufferSkPictureCanvasLayerTextureUpdater::~FrameBufferSkPictureCanvasLayerTextureUpdater()
{
}

PassOwnPtr<LayerTextureUpdater::Texture> FrameBufferSkPictureCanvasLayerTextureUpdater::createTexture(TextureManager* manager)
{
    return adoptPtr(new Texture(this, ManagedTexture::create(manager)));
}

LayerTextureUpdater::SampledTexelFormat FrameBufferSkPictureCanvasLayerTextureUpdater::sampledTexelFormat(GC3Denum textureFormat)
{
    // Here we directly render to the texture, so the component order is always correct.
    return LayerTextureUpdater::SampledTexelFormatRGBA;
}

void FrameBufferSkPictureCanvasLayerTextureUpdater::updateTextureRect(GraphicsContext3D* context, TextureAllocator* allocator, ManagedTexture* texture, const IntRect& sourceRect, const IntRect& destRect)
{
    // Make sure SKIA uses the correct GL context.
    context->makeContextCurrent();
    // Notify SKIA to sync its internal GL state.
    context->grContext()->resetContext();

    // Create an accelerated canvas to draw on.
    OwnPtr<SkCanvas> canvas = createAcceleratedCanvas(context, allocator, texture);

    canvas->clipRect(SkRect(destRect));
    // The compositor expects the textures to be upside-down so it can flip
    // the final composited image. Ganesh renders the image upright so we
    // need to do a y-flip.
    canvas->translate(0.0, texture->size().height());
    canvas->scale(1.0, -1.0);
    // Translate the origin of contentRect to that of destRect.
    // Note that destRect is defined relative to sourceRect.
    canvas->translate(contentRect().x() - sourceRect.x() + destRect.x(),
                      contentRect().y() - sourceRect.y() + destRect.y());
    drawPicture(canvas.get());

    // Flush SKIA context so that all the rendered stuff appears on the texture.
    context->grContext()->flush();
}

} // namespace WebCore
#endif // USE(SKIA)
#endif // USE(ACCELERATED_COMPOSITING)
