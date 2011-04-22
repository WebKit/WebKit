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

#include "ContentLayerChromium.h"

#include "cc/CCLayerImpl.h"
#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "LayerTexture.h"
#include "RenderLayerBacking.h"
#include "TextStream.h"

// Maximum size the width or height of this layer can be before enabling tiling
// when m_tilingOption == AutoTile.
static int maxUntiledSize = 512;
// When tiling is enabled, use tiles of this dimension squared.
static int defaultTileSize = 256;

using namespace std;

namespace WebCore {

PassRefPtr<ContentLayerChromium> ContentLayerChromium::create(GraphicsLayerChromium* owner)
{
    return adoptRef(new ContentLayerChromium(owner));
}

ContentLayerChromium::ContentLayerChromium(GraphicsLayerChromium* owner)
    : LayerChromium(owner)
    , m_tilingOption(ContentLayerChromium::AutoTile)
{
}

ContentLayerChromium::~ContentLayerChromium()
{
    m_tiler.clear();
    LayerChromium::cleanupResources();
}

class ContentLayerPainter : public TilePaintInterface {
public:
    explicit ContentLayerPainter(GraphicsLayerChromium* owner)
        : m_owner(owner)
    {
    }

    virtual void paint(GraphicsContext& context, const IntRect& contentRect)
    {
        context.save();
        context.clearRect(contentRect);
        context.clip(contentRect);
        m_owner->paintGraphicsLayerContents(context, contentRect);
        context.restore();
    }
private:
    GraphicsLayerChromium* m_owner;
};

void ContentLayerChromium::paintContentsIfDirty(const IntRect& targetSurfaceRect)
{
    ASSERT(drawsContent());
    ASSERT(layerRenderer());

    createTilerIfNeeded();

    ContentLayerPainter painter(m_owner);
    updateLayerSize(layerBounds().size());

    IntRect layerRect = visibleLayerRect(targetSurfaceRect);
    if (layerRect.isEmpty())
        return;

    IntRect dirty = enclosingIntRect(m_dirtyRect);
    dirty.intersect(layerBounds());
    m_tiler->invalidateRect(dirty);

    m_tiler->update(painter, layerRect);
    m_dirtyRect = FloatRect();
}

void ContentLayerChromium::setLayerRenderer(LayerRendererChromium* layerRenderer)
{
    LayerChromium::setLayerRenderer(layerRenderer);
    createTilerIfNeeded();
    m_tiler->setLayerRenderer(layerRenderer);
}

TransformationMatrix ContentLayerChromium::tilingTransform()
{
    TransformationMatrix transform = ccLayerImpl()->drawTransform();
    // Tiler draws from the upper left corner. The draw transform
    // specifies the middle of the layer.
    IntSize size = bounds();
    transform.translate(-size.width() / 2.0, -size.height() / 2.0);

    return transform;
}

IntRect ContentLayerChromium::visibleLayerRect(const IntRect& targetSurfaceRect)
{
    if (targetSurfaceRect.isEmpty())
        return targetSurfaceRect;

    const IntRect layerBoundRect = layerBounds();
    const TransformationMatrix transform = tilingTransform();

    // Is this layer fully contained within the target surface?
    IntRect layerInSurfaceSpace = transform.mapRect(layerBoundRect);
    if (targetSurfaceRect.contains(layerInSurfaceSpace))
        return layerBoundRect;

    // If the layer doesn't fill up the entire surface, then find the part of
    // the surface rect where the layer could be visible. This avoids trying to
    // project surface rect points that are behind the projection point.
    IntRect minimalSurfaceRect = targetSurfaceRect;
    minimalSurfaceRect.intersect(layerInSurfaceSpace);

    // Project the corners of the target surface rect into the layer space.
    // This bounding rectangle may be larger than it needs to be (being
    // axis-aligned), but is a reasonable filter on the space to consider.
    // Non-invertible transforms will create an empty rect here.
    const TransformationMatrix surfaceToLayer = transform.inverse();
    IntRect layerRect = surfaceToLayer.projectQuad(FloatQuad(FloatRect(minimalSurfaceRect))).enclosingBoundingBox();
    layerRect.intersect(layerBoundRect);
    return layerRect;
}

IntRect ContentLayerChromium::layerBounds() const
{
    return IntRect(IntPoint(0, 0), bounds());
}

void ContentLayerChromium::updateLayerSize(const IntSize& layerSize)
{
    if (!m_tiler)
        return;

    const IntSize tileSize(min(defaultTileSize, layerSize.width()), min(defaultTileSize, layerSize.height()));
    const bool autoTiled = layerSize.width() > maxUntiledSize || layerSize.height() > maxUntiledSize;

    bool isTiled;
    if (m_tilingOption == AlwaysTile)
        isTiled = true;
    else if (m_tilingOption == NeverTile)
        isTiled = false;
    else
        isTiled = autoTiled;

    m_tiler->setTileSize(isTiled ? tileSize : layerSize);
}

void ContentLayerChromium::draw(const IntRect& targetSurfaceRect)
{
    const TransformationMatrix transform = tilingTransform();
    IntRect layerRect = visibleLayerRect(targetSurfaceRect);
    if (!layerRect.isEmpty())
        m_tiler->draw(layerRect, transform, ccLayerImpl()->drawOpacity());
}

void ContentLayerChromium::createTilerIfNeeded()
{
    if (m_tiler)
        return;
    m_tiler = LayerTilerChromium::create(layerRenderer(), IntSize(defaultTileSize, defaultTileSize), LayerTilerChromium::HasBorderTexels);
}

void ContentLayerChromium::updateCompositorResources()
{
    m_tiler->uploadCanvas();
}

void ContentLayerChromium::setTilingOption(TilingOption option)
{
    m_tilingOption = option;
    updateLayerSize(bounds());
}

void ContentLayerChromium::bindContentsTexture()
{
    // This function is only valid for single texture layers, e.g. masks.
    ASSERT(m_tilingOption == NeverTile);
    ASSERT(m_tiler);

    LayerTexture* texture = m_tiler->getSingleTexture();
    ASSERT(texture);

    texture->bindTexture();
}

void ContentLayerChromium::setIsMask(bool isMask)
{
    setTilingOption(isMask ? NeverTile : AutoTile);
}

static void writeIndent(TextStream& ts, int indent)
{
    for (int i = 0; i != indent; ++i)
        ts << "  ";
}

void ContentLayerChromium::dumpLayerProperties(TextStream& ts, int indent) const
{
    LayerChromium::dumpLayerProperties(ts, indent);
    writeIndent(ts, indent);
    ts << "skipsDraw: " << m_tiler->skipsDraw() << "\n";
}

}
#endif // USE(ACCELERATED_COMPOSITING)
