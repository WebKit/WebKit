/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)
#include "VideoLayerChromium.h"

#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "RenderLayerBacking.h"
#include "skia/ext/platform_canvas.h"

#if PLATFORM(SKIA)
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#endif

namespace WebCore {

PassRefPtr<VideoLayerChromium> VideoLayerChromium::create(GraphicsLayerChromium* owner,
                                                          VideoFrameProvider* provider)
{
    return adoptRef(new VideoLayerChromium(owner, provider));
}

VideoLayerChromium::VideoLayerChromium(GraphicsLayerChromium* owner, VideoFrameProvider* provider)
    : ContentLayerChromium(owner)
#if PLATFORM(SKIA)
    , m_canvas(0)
    , m_skiaContext(0)
#endif
    , m_graphicsContext(0)
    , m_provider(provider)
{
}

void VideoLayerChromium::updateContents()
{
    RenderLayerBacking* backing = static_cast<RenderLayerBacking*>(m_owner->client());
    if (!backing || backing->paintingGoesToWindow())
        return;

    ASSERT(drawsContent());

    IntRect dirtyRect(m_dirtyRect);
    IntSize requiredTextureSize;

#if PLATFORM(SKIA)
    requiredTextureSize = m_bounds;
    IntRect boundsRect(IntPoint(0, 0), m_bounds);

    // If the texture needs to be reallocated, then we must redraw the entire
    // contents of the layer.
    if (requiredTextureSize != m_allocatedTextureSize)
        dirtyRect = boundsRect;
    else {
        // Clip the dirtyRect to the size of the layer to avoid drawing outside
        // the bounds of the backing texture.
        dirtyRect.intersect(boundsRect);
    }

    if (!m_canvas.get()
        || dirtyRect.width() != m_canvas->getDevice()->width()
        || dirtyRect.height() != m_canvas->getDevice()->height()) {
        m_canvas = new skia::PlatformCanvas(dirtyRect.width(), dirtyRect.height(), true);
        m_skiaContext = new PlatformContextSkia(m_canvas.get());

        // This is needed to get text to show up correctly.
        // FIXME: Does this take us down a very slow text rendering path?
        m_skiaContext->setDrawingToImageBuffer(true);
        m_graphicsContext = new GraphicsContext(reinterpret_cast<PlatformGraphicsContext*>(m_skiaContext.get()));
    }

    // Bring the canvas into the coordinate system of the paint rect.
    m_canvas->translate(static_cast<SkScalar>(-dirtyRect.x()), static_cast<SkScalar>(-dirtyRect.y()));

    // FIXME: Remove this test when tiled layers are implemented.
    m_skipsDraw = false;
    if (!layerRenderer()->checkTextureSize(requiredTextureSize)) {
        m_skipsDraw = true;
        return;
    }

    unsigned textureId = m_contentsTexture;
    if (!textureId)
        textureId = layerRenderer()->createLayerTexture();

    // If the texture id or size changed since last time, then we need to tell GL
    // to re-allocate a texture.
    if (m_contentsTexture != textureId || requiredTextureSize != m_allocatedTextureSize)
        createTextureRect(requiredTextureSize, dirtyRect, textureId);
    else
        updateTextureRect(dirtyRect, textureId);
#else
    // FIXME: Implement non-skia path
    notImplemented();
#endif
}

void VideoLayerChromium::createTextureRect(const IntSize& requiredTextureSize, const IntRect& updateRect, unsigned textureId)
{
    // Paint into graphics context and get bitmap.
    m_owner->paintGraphicsLayerContents(*m_graphicsContext, updateRect);
    void* pixels = 0;
    IntSize bitmapSize = IntSize();
#if PLATFORM(SKIA)
    const SkBitmap& bitmap = m_canvas->getDevice()->accessBitmap(false);
    const SkBitmap* skiaBitmap = &bitmap;
    ASSERT(skiaBitmap);

    SkAutoLockPixels lock(*skiaBitmap);
    SkBitmap::Config skiaConfig = skiaBitmap->config();
    // FIXME: Do we need to support more image configurations?
    if (skiaConfig == SkBitmap::kARGB_8888_Config) {
        pixels = skiaBitmap->getPixels();
        bitmapSize = IntSize(skiaBitmap->width(), skiaBitmap->height());
    }
#else
    // FIXME: Implement non-skia path
    notImplemented();
#endif
    if (!pixels)
        return;

    GraphicsContext3D* context = layerRendererContext();
    context->bindTexture(GraphicsContext3D::TEXTURE_2D, textureId);
    ASSERT(bitmapSize == requiredTextureSize);
    context->texImage2D(GraphicsContext3D::TEXTURE_2D, 0, GraphicsContext3D::RGBA, requiredTextureSize.width(), requiredTextureSize.height(), 0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, pixels);

    m_contentsTexture = textureId;
    m_allocatedTextureSize = requiredTextureSize;

    updateCompleted();
}

void VideoLayerChromium::updateTextureRect(const IntRect& updateRect, unsigned textureId)
{
#if PLATFORM(SKIA)
    const SkBitmap& bitmap = m_canvas->getDevice()->accessBitmap(true);
    SkBitmap* skiaBitmap = const_cast<SkBitmap*>(&bitmap);
    ASSERT(skiaBitmap);

    SkAutoLockPixels lock(*skiaBitmap);
    SkBitmap::Config skiaConfig = skiaBitmap->config();

    if (skiaConfig == SkBitmap::kARGB_8888_Config) {
        GraphicsContext3D* context = layerRendererContext();
        context->bindTexture(GraphicsContext3D::TEXTURE_2D, textureId);
        ASSERT(context->supportsMapSubCHROMIUM());
        void* mem = context->mapTexSubImage2DCHROMIUM(GraphicsContext3D::TEXTURE_2D, 0, updateRect.x(), updateRect.y(), updateRect.width(), updateRect.height(), GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, GraphicsContext3D::WRITE_ONLY);
        skiaBitmap->setPixels(mem);
        m_owner->paintGraphicsLayerContents(*m_graphicsContext, updateRect);
        context->unmapTexSubImage2DCHROMIUM(mem);
    }

    updateCompleted();
#else
    // FIXME: Implement non-skia path
    notImplemented();
#endif
}

void VideoLayerChromium::updateCompleted()
{
    m_dirtyRect.setSize(FloatSize());
    m_contentsDirty = false;
}

}
#endif // USE(ACCELERATED_COMPOSITING)
