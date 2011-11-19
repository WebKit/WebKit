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

#include "Extensions3D.h"
#include "GraphicsContext.h"
#include "LayerPainterChromium.h"
#include "ManagedTexture.h"
#include "PlatformColor.h"
#include "TraceEvent.h"

#if USE(SKIA)
#include "GrContext.h"
#include "PlatformContextSkia.h"
#include "SkCanvas.h"
#include "SkGpuDevice.h"
#endif // USE(SKIA)

namespace WebCore {

#if USE(SKIA)
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
#endif // USE(SKIA)

LayerTextureUpdaterCanvas::LayerTextureUpdaterCanvas(PassOwnPtr<LayerPainterChromium> painter)
    : m_painter(painter)
{
}

void LayerTextureUpdaterCanvas::paintContents(GraphicsContext& context, const IntRect& contentRect, float contentsScale)
{
    context.translate(-contentRect.x(), -contentRect.y());
    {
        TRACE_EVENT("LayerTextureUpdaterCanvas::paint", this, 0);

        IntRect scaledContentRect = contentRect;

        if (contentsScale != 1.0) {
            context.save();
            context.scale(FloatSize(contentsScale, contentsScale));

            FloatRect rect = contentRect;
            rect.scale(1 / contentsScale);
            scaledContentRect = enclosingIntRect(rect);
        }

        m_painter->paint(context, scaledContentRect);

        if (contentsScale != 1.0)
            context.restore();
    }
    m_contentRect = contentRect;
}

PassRefPtr<LayerTextureUpdaterBitmap> LayerTextureUpdaterBitmap::create(PassOwnPtr<LayerPainterChromium> painter, bool useMapTexSubImage)
{
    return adoptRef(new LayerTextureUpdaterBitmap(painter, useMapTexSubImage));
}

LayerTextureUpdaterBitmap::LayerTextureUpdaterBitmap(PassOwnPtr<LayerPainterChromium> painter, bool useMapTexSubImage)
    : LayerTextureUpdaterCanvas(painter)
    , m_texSubImage(useMapTexSubImage)
{
}

LayerTextureUpdater::SampledTexelFormat LayerTextureUpdaterBitmap::sampledTexelFormat(GC3Denum textureFormat)
{
    // The component order may be bgra if we uploaded bgra pixels to rgba textures.
    return PlatformColor::sameComponentOrder(textureFormat) ?
            LayerTextureUpdater::SampledTexelFormatRGBA : LayerTextureUpdater::SampledTexelFormatBGRA;
}

void LayerTextureUpdaterBitmap::prepareToUpdate(const IntRect& contentRect, const IntSize& tileSize, int borderTexels, float contentsScale)
{
    m_texSubImage.setSubImageSize(tileSize);

    m_canvas.resize(contentRect.size());
    // Assumption: if a tiler is using border texels, then it is because the
    // layer is likely to be filtered or transformed. Because of it might be
    // transformed, draw the text in grayscale instead of subpixel antialiasing.
    PlatformCanvas::Painter::TextOption textOption =
        borderTexels ? PlatformCanvas::Painter::GrayscaleText : PlatformCanvas::Painter::SubpixelText;
    PlatformCanvas::Painter canvasPainter(&m_canvas, textOption);
    paintContents(*canvasPainter.context(), contentRect, contentsScale);
}

void LayerTextureUpdaterBitmap::updateTextureRect(GraphicsContext3D* context, TextureAllocator* allocator, ManagedTexture* texture, const IntRect& sourceRect, const IntRect& destRect)
{
    PlatformCanvas::AutoLocker locker(&m_canvas);

    texture->bindTexture(context, allocator);
    m_texSubImage.upload(locker.pixels(), contentRect(), sourceRect, destRect, texture->format(), context);
}

#if USE(SKIA)
PassRefPtr<LayerTextureUpdaterSkPicture> LayerTextureUpdaterSkPicture::create(PassOwnPtr<LayerPainterChromium> painter)
{
    return adoptRef(new LayerTextureUpdaterSkPicture(painter));
}

LayerTextureUpdaterSkPicture::LayerTextureUpdaterSkPicture(PassOwnPtr<LayerPainterChromium> painter)
    : LayerTextureUpdaterCanvas(painter)
{
}

LayerTextureUpdaterSkPicture::~LayerTextureUpdaterSkPicture()
{
}

LayerTextureUpdater::SampledTexelFormat LayerTextureUpdaterSkPicture::sampledTexelFormat(GC3Denum textureFormat)
{
    // Here we directly render to the texture, so the component order is always correct.
    return LayerTextureUpdater::SampledTexelFormatRGBA;
}

void LayerTextureUpdaterSkPicture::prepareToUpdate(const IntRect& contentRect, const IntSize& tileSize, int borderTexels, float contentsScale)
{
    SkCanvas* canvas = m_picture.beginRecording(contentRect.width(), contentRect.height());
    PlatformContextSkia platformContext(canvas);
    GraphicsContext graphicsContext(&platformContext);
    paintContents(graphicsContext, contentRect, contentsScale);
    m_picture.endRecording();
}

void LayerTextureUpdaterSkPicture::updateTextureRect(GraphicsContext3D* context, TextureAllocator* allocator, ManagedTexture* texture, const IntRect& sourceRect, const IntRect& destRect)
{
    // Make sure SKIA uses the correct GL context.
    context->makeContextCurrent();
    // Notify SKIA to sync its internal GL state.
    context->grContext()->resetContext();

    // Create an accelerated canvas to draw on.
    FrameBuffer buffer;
    SkCanvas* canvas = buffer.initialize(context, allocator, texture);

    canvas->clipRect(SkRect(destRect));
    // Translate the origin of contentRect to that of destRect.
    // Note that destRect is defined relative to sourceRect.
    canvas->translate(contentRect().x() - sourceRect.x() + destRect.x(),
                      contentRect().y() - sourceRect.y() + destRect.y());
    canvas->drawPicture(m_picture);

    // Flush SKIA context so that all the rendered stuff appears on the texture.
    context->grContext()->flush();
}
#endif // USE(SKIA)

} // namespace WebCore
#endif // USE(ACCELERATED_COMPOSITING)
