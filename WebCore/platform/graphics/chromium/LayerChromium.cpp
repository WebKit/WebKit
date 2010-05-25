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

#include "LayerChromium.h"

#include "LayerRendererChromium.h"
#if PLATFORM(SKIA)
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#endif
#include "RenderLayerBacking.h"
#include "skia/ext/platform_canvas.h"

#include <GLES2/gl2.h>


namespace WebCore {

using namespace std;

PassRefPtr<LayerChromium> LayerChromium::create(LayerType type, GraphicsLayerChromium* owner)
{
    return adoptRef(new LayerChromium(type, owner));
}

LayerChromium::LayerChromium(LayerType type, GraphicsLayerChromium* owner)
    : m_needsDisplayOnBoundsChange(false)
    , m_owner(owner)
    , m_layerType(type)
    , m_superlayer(0)
    , m_layerRenderer(0)
    , m_borderWidth(0)
    , m_allocatedTextureId(0)
    , m_borderColor(0, 0, 0, 0)
    , m_backgroundColor(0, 0, 0, 0)
    , m_anchorPoint(0.5, 0.5)
    , m_anchorPointZ(0)
    , m_clearsContext(false)
    , m_doubleSided(true)
    , m_edgeAntialiasingMask(0)
    , m_hidden(false)
    , m_masksToBounds(false)
    , m_contentsGravity(Bottom)
    , m_opacity(1.0)
    , m_opaque(true)
    , m_zPosition(0.0)
    , m_geometryFlipped(false)
    , m_contentsDirty(false)
    , m_contents(0)
{
}

LayerChromium::~LayerChromium()
{
    // Our superlayer should be holding a reference to us so there should be no
    // way for us to be destroyed while we still have a superlayer.
    ASSERT(!superlayer());

    // Remove the superlayer reference from all sublayers.
    removeAllSublayers();

    // Notify the renderer to clean up the texture associated with the layer.
    if (m_layerRenderer)
        m_layerRenderer->freeLayerTexture(this);
}

void LayerChromium::setLayerRenderer(LayerRendererChromium* renderer)
{
    // It's not expected that layers will ever switch renderers.
    ASSERT(!renderer || !m_layerRenderer || renderer == m_layerRenderer);

    m_layerRenderer = renderer;
}

void LayerChromium::updateTextureContents(unsigned int textureId)
{
    RenderLayerBacking* backing = static_cast<RenderLayerBacking*>(m_owner->client());

    if (backing && !backing->paintingGoesToWindow()) {
        void* pixels = 0;
        IntRect dirtyRect(m_dirtyRect);
        IntSize requiredTextureSize;
        IntSize bitmapSize;
#if PLATFORM(SKIA)
        const SkBitmap* skiaBitmap = 0;
        OwnPtr<skia::PlatformCanvas> canvas;
        OwnPtr<PlatformContextSkia> skiaContext;
        OwnPtr<GraphicsContext> graphicsContext;
        if (drawsContent()) { // Layer contents must be drawn into a canvas.
            canvas.set(new skia::PlatformCanvas(dirtyRect.width(), dirtyRect.height(), false));
            skiaContext.set(new PlatformContextSkia(canvas.get()));

            // This is needed to get text to show up correctly. Without it,
            // GDI renders with zero alpha and the text becomes invisible.
            // Unfortunately, setting this to true disables cleartype.
            // FIXME: Does this take us down a very slow text rendering path?
            skiaContext->setDrawingToImageBuffer(true);

            graphicsContext.set(new GraphicsContext(reinterpret_cast<PlatformGraphicsContext*>(skiaContext.get())));

            // Bring the canvas into the coordinate system of the paint rect.
            canvas->translate(static_cast<SkScalar>(-dirtyRect.x()), static_cast<SkScalar>(-dirtyRect.y()));

            m_owner->paintGraphicsLayerContents(*graphicsContext, dirtyRect);
            const SkBitmap& bitmap = canvas->getDevice()->accessBitmap(false);
            skiaBitmap = &bitmap;
            requiredTextureSize = IntSize(max(m_bounds.width(), dirtyRect.width()),
                                          max(m_bounds.height(), dirtyRect.height()));
        } else { // Layer is a container.
            // The layer contains an Image.
            NativeImageSkia* skiaImage = static_cast<NativeImageSkia*>(contents());
            skiaBitmap = skiaImage;
            requiredTextureSize = IntSize(skiaBitmap->width(), skiaBitmap->height());
        }

        ASSERT(skiaBitmap);
        SkAutoLockPixels lock(*skiaBitmap);
        SkBitmap::Config skiaConfig = skiaBitmap->config();
        // FIXME: do we need to support more image configurations?
        if (skiaConfig == SkBitmap::kARGB_8888_Config) {
            pixels = skiaBitmap->getPixels();
            bitmapSize = IntSize(skiaBitmap->width(), skiaBitmap->height());
        }
#else
#error "Need to implement for your platform."
#endif
        if (pixels) {
            glBindTexture(GL_TEXTURE_2D, textureId);
            // If the texture id or size changed since last time then we need to tell GL
            // to re-allocate a texture.
            if (m_allocatedTextureId != textureId || requiredTextureSize != m_allocatedTextureSize) {
                ASSERT(bitmapSize == requiredTextureSize);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, requiredTextureSize.width(), requiredTextureSize.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

                m_allocatedTextureId = textureId;
                m_allocatedTextureSize = requiredTextureSize;
            } else {
                ASSERT(dirtyRect.width() <= m_allocatedTextureSize.width() && dirtyRect.height() <= m_allocatedTextureSize.height());
                ASSERT(dirtyRect.width() == bitmapSize.width() && dirtyRect.height() == bitmapSize.height());
                glTexSubImage2D(GL_TEXTURE_2D, 0, dirtyRect.x(), dirtyRect.y(), dirtyRect.width(), dirtyRect.height(), GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            }
        }
    }

    m_contentsDirty = false;
    m_dirtyRect.setSize(FloatSize());
}

void LayerChromium::setContents(NativeImagePtr contents)
{
    // Check if the image has changed.
    if (m_contents == contents)
        return;
    m_contents = contents;
    setNeedsDisplay();
}

void LayerChromium::setNeedsCommit()
{
    // Call notifySyncRequired(), which in this implementation plumbs through to
    // call setRootLayerNeedsDisplay() on the WebView, which will cause LayerRendererChromium
    // to render a frame.
    if (m_owner)
        m_owner->notifySyncRequired();
}

void LayerChromium::addSublayer(PassRefPtr<LayerChromium> sublayer)
{
    insertSublayer(sublayer, numSublayers());
}

void LayerChromium::insertSublayer(PassRefPtr<LayerChromium> sublayer, size_t index)
{
    index = min(index, m_sublayers.size());
    sublayer->removeFromSuperlayer();
    sublayer->setSuperlayer(this);
    m_sublayers.insert(index, sublayer);
    setNeedsCommit();
}

void LayerChromium::removeFromSuperlayer()
{
    if (m_superlayer)
        m_superlayer->removeSublayer(this);
}

void LayerChromium::removeSublayer(LayerChromium* sublayer)
{
    int foundIndex = indexOfSublayer(sublayer);
    if (foundIndex == -1)
        return;

    sublayer->setSuperlayer(0);
    m_sublayers.remove(foundIndex);
    setNeedsCommit();
}

void LayerChromium::replaceSublayer(LayerChromium* reference, PassRefPtr<LayerChromium> newLayer)
{
    ASSERT_ARG(reference, reference);
    ASSERT_ARG(reference, reference->superlayer() == this);

    if (reference == newLayer)
        return;

    int referenceIndex = indexOfSublayer(reference);
    if (referenceIndex == -1) {
        ASSERT_NOT_REACHED();
        return;
    }

    reference->removeFromSuperlayer();

    if (newLayer) {
        newLayer->removeFromSuperlayer();
        insertSublayer(newLayer, referenceIndex);
    }
}

int LayerChromium::indexOfSublayer(const LayerChromium* reference)
{
    for (size_t i = 0; i < m_sublayers.size(); i++) {
        if (m_sublayers[i] == reference)
            return i;
    }
    return -1;
}

void LayerChromium::setBounds(const IntSize& size)
{
    if (m_bounds == size)
        return;

    m_bounds = size;
    m_backingStoreSize = size;

    setNeedsCommit();
}

void LayerChromium::setFrame(const FloatRect& rect)
{
    if (rect == m_frame)
      return;

    m_frame = rect;
    setNeedsCommit();
}

const LayerChromium* LayerChromium::rootLayer() const
{
    const LayerChromium* layer = this;
    for (LayerChromium* superlayer = layer->superlayer(); superlayer; layer = superlayer, superlayer = superlayer->superlayer()) { }
    return layer;
}

void LayerChromium::removeAllSublayers()
{
    while (m_sublayers.size()) {
        LayerChromium* layer = m_sublayers[0].get();
        ASSERT(layer->superlayer());
        layer->removeFromSuperlayer();
    }
    setNeedsCommit();
}

void LayerChromium::setSublayers(const Vector<RefPtr<LayerChromium> >& sublayers)
{
    if (sublayers == m_sublayers)
        return;

    removeAllSublayers();
    size_t listSize = sublayers.size();
    for (size_t i = 0; i < listSize; i++)
        addSublayer(sublayers[i]);
}

LayerChromium* LayerChromium::superlayer() const
{
    return m_superlayer;
}

void LayerChromium::setNeedsDisplay(const FloatRect& dirtyRect)
{
    // Simply mark the contents as dirty. The actual redraw will
    // happen when it's time to do the compositing.
    m_contentsDirty = true;

    m_dirtyRect.unite(dirtyRect);

    setNeedsCommit();
}

void LayerChromium::setNeedsDisplay()
{
    m_dirtyRect.setSize(m_bounds);
    m_contentsDirty = true;
}

}

#endif // USE(ACCELERATED_COMPOSITING)
