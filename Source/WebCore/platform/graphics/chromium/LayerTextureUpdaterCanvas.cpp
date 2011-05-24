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

#if USE(SKIA)
#include "GrContext.h"
#include "PlatformContextSkia.h"
#include "SkCanvas.h"
#include "SkGpuDevice.h"
#include "SkGpuDeviceFactory.h"
#endif // USE(SKIA)

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

#if USE(SKIA)
LayerTextureUpdaterSkPicture::LayerTextureUpdaterSkPicture(GraphicsContext3D* context, PassOwnPtr<LayerPainterChromium> painter, GrContext* skiaContext)
    : LayerTextureUpdaterCanvas(context, painter)
    , m_skiaContext(skiaContext)
    , m_createFrameBuffer(false)
    , m_fbo(0)
    , m_stencilBuffer(0)
{
}

LayerTextureUpdaterSkPicture::~LayerTextureUpdaterSkPicture()
{
    deleteFrameBuffer();
}

void LayerTextureUpdaterSkPicture::prepareToUpdate(const IntRect& contentRect, const IntSize& tileSize, int borderTexels)
{
    // Need to recreate FBO if tile-size changed.
    // Note that we cannot create the framebuffer here because this function does not run in compositor thread
    // and hence does not have access to compositor context.
    if (m_bufferSize != tileSize) {
        m_createFrameBuffer = true;
        m_bufferSize = tileSize;
    }

    SkCanvas* canvas = m_picture.beginRecording(contentRect.width(), contentRect.height());
    PlatformContextSkia platformContext(canvas);
    GraphicsContext graphicsContext(&platformContext);
    paintContents(graphicsContext, contentRect);
    m_picture.endRecording();
}

void LayerTextureUpdaterSkPicture::updateTextureRect(LayerTexture* texture, const IntRect& sourceRect, const IntRect& destRect)
{
    if (m_createFrameBuffer) {
        deleteFrameBuffer();
        createFrameBuffer();
        m_createFrameBuffer = false;
    }
    if (!m_fbo)
        return;

    // Bind texture.
    context()->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_fbo);
    texture->framebufferTexture2D();
    ASSERT(context()->checkFramebufferStatus(GraphicsContext3D::FRAMEBUFFER) == GraphicsContext3D::FRAMEBUFFER_COMPLETE);

    context()->viewport(0, 0, m_bufferSize.width(), m_bufferSize.height());
    clearFrameBuffer();

    // Notify SKIA to sync its internal GL state.
    m_skiaContext->resetContext();
    // Offset from source rectangle to this destination rectangle.
    IntPoint offset(sourceRect.x() - contentRect().x(), sourceRect.y() - contentRect().y());
    m_canvas->save();
    m_canvas->translate(-offset.x(), -offset.y());
    m_canvas->drawPicture(m_picture);
    m_canvas->restore();
    // Flush SKIA context so that all the rendered stuff appears on the texture.
    m_skiaContext->flush(GrContext::kForceCurrentRenderTarget_FlushBit);

    // Unbind texture.
    context()->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, 0, 0);
    context()->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, 0);
}

void LayerTextureUpdaterSkPicture::deleteFrameBuffer()
{
    m_canvas.clear();

    if (m_stencilBuffer)
        context()->deleteRenderbuffer(m_stencilBuffer);
    if (m_fbo)
        context()->deleteFramebuffer(m_fbo);
}

bool LayerTextureUpdaterSkPicture::createFrameBuffer()
{
    ASSERT(!m_fbo);
    ASSERT(!m_bufferSize.isEmpty());

    // SKIA needs stencil buffer for path rendering.
    // Make sure it is available.
    GraphicsContext3D::Attributes contextAttribs = context()->getContextAttributes();
    if (!contextAttribs.stencil)
        return false;

    // Create and bind a frame-buffer-object.
    m_fbo = context()->createFramebuffer();
    if (!m_fbo)
        return false;
    context()->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_fbo);

    // We just need to create a stencil buffer for FBO.
    // The color buffer (texture) will be provided by tiles.
    // SKIA does not need depth buffer.
    m_stencilBuffer = context()->createRenderbuffer();
    if (!m_stencilBuffer) {
        context()->deleteFramebuffer(m_fbo);
        m_fbo = 0;
        return false;
    }
    context()->bindRenderbuffer(GraphicsContext3D::RENDERBUFFER, m_stencilBuffer);
    context()->renderbufferStorage(GraphicsContext3D::RENDERBUFFER, GraphicsContext3D::STENCIL_INDEX8, m_bufferSize.width(), m_bufferSize.height());
    context()->framebufferRenderbuffer(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::STENCIL_ATTACHMENT, GraphicsContext3D::RENDERBUFFER, m_stencilBuffer);

    // Create a skia gpu canvas.
    GrPlatformSurfaceDesc targetDesc;
    targetDesc.reset();
    targetDesc.fSurfaceType = kRenderTarget_GrPlatformSurfaceType;
    targetDesc.fRenderTargetFlags = kNone_GrPlatformRenderTargetFlagBit;
    targetDesc.fWidth = m_bufferSize.width();
    targetDesc.fHeight = m_bufferSize.height();
    targetDesc.fConfig = kRGBA_8888_GrPixelConfig;
    targetDesc.fStencilBits = 8;
    targetDesc.fPlatformRenderTarget = m_fbo;
    SkAutoTUnref<GrRenderTarget> target(static_cast<GrRenderTarget*>(m_skiaContext->createPlatformSurface(targetDesc)));
    SkAutoTUnref<SkDeviceFactory> factory(new SkGpuDeviceFactory(m_skiaContext, target.get()));
    SkAutoTUnref<SkDevice> device(factory.get()->newDevice(0, SkBitmap::kARGB_8888_Config, m_bufferSize.width(), m_bufferSize.height(), false, false));        
    m_canvas = adoptPtr(new SkCanvas(factory.get()));
    m_canvas->setDevice(device.get());

    context()->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, 0);
    return true;
}

void LayerTextureUpdaterSkPicture::clearFrameBuffer()
{
#ifndef NDEBUG
    // Clear to green to make it easier to spot unrendered regions.
    context()->clearColor(0, 1, 0, 1);
    context()->clear(GraphicsContext3D::COLOR_BUFFER_BIT | GraphicsContext3D::STENCIL_BUFFER_BIT);
#endif
}
#endif // SKIA

} // namespace WebCore
#endif // USE(ACCELERATED_COMPOSITING)

