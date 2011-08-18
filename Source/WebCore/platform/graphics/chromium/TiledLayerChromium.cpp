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

#include "TiledLayerChromium.h"

#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "LayerTilerChromium.h"
#include "TextStream.h"
#include "cc/CCLayerImpl.h"
#include "cc/CCTiledLayerImpl.h"
#include <wtf/CurrentTime.h>

// Start tiling when the width and height of a layer are larger than this size.
static int maxUntiledSize = 512;

// When tiling is enabled, use tiles of this dimension squared.
static int defaultTileSize = 256;

using namespace std;

namespace WebCore {

TiledLayerChromium::TiledLayerChromium(GraphicsLayerChromium* owner)
    : LayerChromium(owner)
    , m_tilingOption(AutoTile)
{
}

TiledLayerChromium::~TiledLayerChromium()
{
}

PassRefPtr<CCLayerImpl> TiledLayerChromium::createCCLayerImpl()
{
    return CCTiledLayerImpl::create(id());
}

void TiledLayerChromium::cleanupResources()
{
    m_tiler.clear();
    LayerChromium::cleanupResources();
}

void TiledLayerChromium::setLayerRenderer(LayerRendererChromium* layerRenderer)
{
    LayerChromium::setLayerRenderer(layerRenderer);
    createTilerIfNeeded();
}

void TiledLayerChromium::updateTileSizeAndTilingOption()
{
    if (!m_tiler)
        return;

    const IntSize tileSize(min(defaultTileSize, contentBounds().width()), min(defaultTileSize, contentBounds().height()));

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

    IntSize requestedSize = isTiled ? tileSize : contentBounds();
    const int maxSize = layerRenderer()->maxTextureSize();
    IntSize clampedSize = requestedSize.shrunkTo(IntSize(maxSize, maxSize));
    m_tiler->setTileSize(clampedSize);
}

bool TiledLayerChromium::drawsContent() const
{
    if (!m_owner)
        return false;

    if (!m_tiler)
        return true;

    if (m_tilingOption == NeverTile && m_tiler->numTiles() > 1)
        return false;

    return !m_tiler->skipsDraw();
}

void TiledLayerChromium::createTilerIfNeeded()
{
    if (m_tiler)
        return;

    createTextureUpdaterIfNeeded();

    m_tiler = LayerTilerChromium::create(
        layerRenderer(),
        IntSize(defaultTileSize, defaultTileSize),
        isRootLayer() ? LayerTilerChromium::NoBorderTexels : LayerTilerChromium::HasBorderTexels);
}

void TiledLayerChromium::updateCompositorResources()
{
    m_tiler->updateRect(textureUpdater());
}

void TiledLayerChromium::setTilingOption(TilingOption tilingOption)
{
    m_tilingOption = tilingOption;
    updateTileSizeAndTilingOption();
}

void TiledLayerChromium::setIsMask(bool isMask)
{
    setTilingOption(isMask ? NeverTile : AutoTile);
}

TransformationMatrix TiledLayerChromium::tilingTransform() const
{
    TransformationMatrix transform = drawTransform();

    if (contentBounds().isEmpty())
        return transform;

    transform.scaleNonUniform(bounds().width() / static_cast<double>(contentBounds().width()),
                              bounds().height() / static_cast<double>(contentBounds().height()));

    // Tiler draws with a different origin from other layers.
    transform.translate(-contentBounds().width() / 2.0, -contentBounds().height() / 2.0);

    transform.translate(-scrollPosition().x(), -scrollPosition().y());

    return transform;
}

void TiledLayerChromium::pushPropertiesTo(CCLayerImpl* layer)
{
    LayerChromium::pushPropertiesTo(layer);

    CCTiledLayerImpl* tiledLayer = static_cast<CCTiledLayerImpl*>(layer);
    tiledLayer->setTilingTransform(tilingTransform());
    tiledLayer->setTiler(m_tiler.get());
}

static void writeIndent(TextStream& ts, int indent)
{
    for (int i = 0; i != indent; ++i)
        ts << "  ";
}

void TiledLayerChromium::dumpLayerProperties(TextStream& ts, int indent) const
{
    LayerChromium::dumpLayerProperties(ts, indent);
    writeIndent(ts, indent);
    ts << "skipsDraw: " << (!m_tiler || m_tiler->skipsDraw()) << "\n";
}

}
#endif // USE(ACCELERATED_COMPOSITING)
