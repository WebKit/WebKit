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

namespace {

class FrameBuffer {
public:
    FrameBuffer();
    ~FrameBuffer();

    SkCanvas* initialize(GraphicsContext3D*, TextureAllocator*, ManagedTexture*);

private:
    GraphicsContext3D* m_context;
    Platform3DObject m_fbo;
    OwnPtr<SkCanvas> m_canvas;
};

FrameBuffer::FrameBuffer()
    : m_context(0)
    , m_fbo(0)
{
}

FrameBuffer::~FrameBuffer()
{
    m_canvas.clear();

    if (m_fbo)
        m_context->deleteFramebuffer(m_fbo);
}

SkCanvas* FrameBuffer::initialize(GraphicsContext3D* context, TextureAllocator* allocator, ManagedTexture* texture)
{
    m_context = context;
    m_fbo = m_context->createFramebuffer();
    m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_fbo);
    texture->framebufferTexture2D(m_context, allocator);

    // Create a skia gpu canvas.
    GrContext* grContext = m_context->grContext();
    IntSize canvasSize = texture->size();
    GrPlatformSurfaceDesc targetDesc;
    targetDesc.reset();
    targetDesc.fSurfaceType = kTextureRenderTarget_GrPlatformSurfaceType;
    targetDesc.fRenderTargetFlags = kNone_GrPlatformRenderTargetFlagBit;
    targetDesc.fWidth = canvasSize.width();
    targetDesc.fHeight = canvasSize.height();
    targetDesc.fConfig = kRGBA_8888_GrPixelConfig;
    targetDesc.fStencilBits = 8;
    targetDesc.fPlatformTexture = texture->textureId();
    targetDesc.fPlatformRenderTarget = m_fbo;
    SkAutoTUnref<GrTexture> target(static_cast<GrTexture*>(grContext->createPlatformSurface(targetDesc)));
    SkAutoTUnref<SkDevice> device(new SkGpuDevice(grContext, target.get()));
    m_canvas = adoptPtr(new SkCanvas(device.get()));

    return m_canvas.get();
}

} // namespace

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
    FrameBuffer buffer;
    SkCanvas* canvas = buffer.initialize(context, allocator, texture);

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
    drawPicture(canvas);

    // Flush SKIA context so that all the rendered stuff appears on the texture.
    context->grContext()->flush();
}

} // namespace WebCore
#endif // USE(SKIA)
#endif // USE(ACCELERATED_COMPOSITING)
