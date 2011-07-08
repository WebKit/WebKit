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
#include "LayerPainterChromium.h"
#include "LayerRendererChromium.h"
#include "LayerTexture.h"
#include "LayerTextureUpdaterCanvas.h"
#include "LayerTilerChromium.h"
#include "PlatformBridge.h"
#include "RenderLayerBacking.h"
#include "TextStream.h"
#include <wtf/CurrentTime.h>

// Start tiling when the width and height of a layer are larger than this size.
static int maxUntiledSize = 510;

// When tiling is enabled, use tiles of this dimension squared.
static int defaultTileSize = 256;

using namespace std;

namespace WebCore {

class ContentLayerPainter : public LayerPainterChromium {
    WTF_MAKE_NONCOPYABLE(ContentLayerPainter);
public:
    static PassOwnPtr<ContentLayerPainter> create(GraphicsLayerChromium* owner)
    {
        return adoptPtr(new ContentLayerPainter(owner));
    }

    virtual void paint(GraphicsContext& context, const IntRect& contentRect)
    {
        double paintStart = currentTime();
        context.clearRect(contentRect);
        context.clip(contentRect);
        m_owner->paintGraphicsLayerContents(context, contentRect);
        double paintEnd = currentTime();
        double pixelsPerSec = (contentRect.width() * contentRect.height()) / (paintEnd - paintStart);
        PlatformBridge::histogramCustomCounts("Renderer4.AccelContentPaintDurationMS", (paintEnd - paintStart) * 1000, 0, 120, 30);
        PlatformBridge::histogramCustomCounts("Renderer4.AccelContentPaintMegapixPerSecond", pixelsPerSec / 1000000, 10, 210, 30);
    }
private:
    explicit ContentLayerPainter(GraphicsLayerChromium* owner)
        : m_owner(owner)
    {
    }

    GraphicsLayerChromium* m_owner;
};

PassRefPtr<ContentLayerChromium> ContentLayerChromium::create(GraphicsLayerChromium* owner)
{
    return adoptRef(new ContentLayerChromium(owner));
}

ContentLayerChromium::ContentLayerChromium(GraphicsLayerChromium* owner)
    : LayerChromium(owner)
    , m_tilingOption(ContentLayerChromium::AutoTile)
    , m_borderTexels(true)
{
}

ContentLayerChromium::~ContentLayerChromium()
{
    cleanupResources();
}

void ContentLayerChromium::paintContentsIfDirty()
{
    ASSERT(drawsContent());
    ASSERT(layerRenderer());

    updateLayerSize();

    const IntRect& layerRect = visibleLayerRect();
    if (layerRect.isEmpty())
        return;

    IntRect dirty = enclosingIntRect(m_dirtyRect);
    dirty.intersect(IntRect(IntPoint(), contentBounds()));
    m_tiler->invalidateRect(dirty);

    if (!drawsContent())
        return;

    m_tiler->prepareToUpdate(layerRect, m_textureUpdater.get());
    m_dirtyRect = FloatRect();
}

void ContentLayerChromium::cleanupResources()
{
    m_textureUpdater.clear();
    m_tiler.clear();
    LayerChromium::cleanupResources();
}

void ContentLayerChromium::setLayerRenderer(LayerRendererChromium* layerRenderer)
{
    LayerChromium::setLayerRenderer(layerRenderer);
    createTilerIfNeeded();
}

void ContentLayerChromium::createTextureUpdaterIfNeeded()
{
    if (m_textureUpdater)
        return;
#if USE(SKIA)
    if (layerRenderer()->accelerateDrawing()) {
        m_textureUpdater = LayerTextureUpdaterSkPicture::create(layerRendererContext(), ContentLayerPainter::create(m_owner), layerRenderer()->skiaContext());
        return;
    }
#endif
    m_textureUpdater = LayerTextureUpdaterBitmap::create(layerRendererContext(), ContentLayerPainter::create(m_owner), layerRenderer()->contextSupportsMapSub());
}

TransformationMatrix ContentLayerChromium::tilingTransform()
{
    TransformationMatrix transform = ccLayerImpl()->drawTransform();

    if (contentBounds().isEmpty())
        return transform;

    transform.scaleNonUniform(bounds().width() / static_cast<double>(contentBounds().width()),
                              bounds().height() / static_cast<double>(contentBounds().height()));

    // Tiler draws with a different origin from other layers.
    transform.translate(-contentBounds().width() / 2.0, -contentBounds().height() / 2.0);

    return transform;
}

IntSize ContentLayerChromium::contentBounds() const
{
    return bounds();
}

void ContentLayerChromium::updateLayerSize()
{
    if (!m_tiler)
        return;

    const IntSize tileSize(defaultTileSize, defaultTileSize);

    // Tile if both dimensions large, or any one dimension large and the other
    // extends into a second tile. This heuristic allows for long skinny layers
    // (e.g. scrollbars) that are Nx1 tiles to minimize wasted texture space.
    const bool anyDimensionLarge = contentBounds().width() > maxUntiledSize || contentBounds().height() > maxUntiledSize;
    const bool anyDimensionOneTile = contentBounds().width() <= defaultTileSize || contentBounds().height() <= defaultTileSize;
    const bool autoTiled = anyDimensionLarge && !anyDimensionOneTile;

    bool isTiled;
    if (m_tilingOption == AlwaysTile)
        isTiled = true;
    else if (m_tilingOption == NeverTile)
        isTiled = false;
    else
        isTiled = autoTiled;

    // Empty tile size tells the tiler to avoid tiling.
    IntSize requestedSize = isTiled ? tileSize : IntSize();
    const int maxSize = layerRenderer()->maxTextureSize();
    IntSize clampedSize = requestedSize.shrunkTo(IntSize(maxSize, maxSize));
    m_tiler->setTileSize(clampedSize);
}

void ContentLayerChromium::draw()
{
    const IntRect& layerRect = visibleLayerRect();
    if (!layerRect.isEmpty())
        m_tiler->draw(layerRect, tilingTransform(), ccLayerImpl()->drawOpacity(), m_textureUpdater.get());
}

bool ContentLayerChromium::drawsContent() const
{
    if (!m_owner || !m_owner->drawsContent())
        return false;

    if (!m_tiler)
        return true;

    if (m_tilingOption == NeverTile && m_tiler->numTiles() > 1)
        return false;

    return !m_tiler->skipsDraw();
}

void ContentLayerChromium::createTilerIfNeeded()
{
    if (m_tiler)
        return;

    createTextureUpdaterIfNeeded();

    m_tiler = LayerTilerChromium::create(
        layerRenderer(),
        IntSize(defaultTileSize, defaultTileSize),
        m_borderTexels ? LayerTilerChromium::HasBorderTexels :
        LayerTilerChromium::NoBorderTexels);
}

void ContentLayerChromium::updateCompositorResources()
{
    m_tiler->updateRect(m_textureUpdater.get());
}

void ContentLayerChromium::setTilingOption(TilingOption option)
{
    m_tilingOption = option;
    updateLayerSize();
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
    m_borderTexels = false;
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
    ts << "skipsDraw: " << (!m_tiler || m_tiler->skipsDraw()) << "\n";
}

}
#endif // USE(ACCELERATED_COMPOSITING)
