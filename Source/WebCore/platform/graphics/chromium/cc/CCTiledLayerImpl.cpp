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

#include "cc/CCTiledLayerImpl.h"

#include "LayerRendererChromium.h"
#include "cc/CCDebugBorderDrawQuad.h"
#include "cc/CCSolidColorDrawQuad.h"
#include "cc/CCTileDrawQuad.h"
#include <wtf/text/WTFString.h>

using namespace std;

namespace WebCore {

static const int debugTileBorderWidth = 1;
static const int debugTileBorderAlpha = 100;

class ManagedTexture;

class DrawableTile : public CCLayerTilingData::Tile {
    WTF_MAKE_NONCOPYABLE(DrawableTile);
public:
    DrawableTile() : m_textureId(0) { }

    Platform3DObject textureId() const { return m_textureId; }
    void setTextureId(Platform3DObject textureId) { m_textureId = textureId; }
private:
    Platform3DObject m_textureId;
};

CCTiledLayerImpl::CCTiledLayerImpl(int id)
    : CCLayerImpl(id)
    , m_skipsDraw(true)
    , m_contentsSwizzled(false)
{
}

CCTiledLayerImpl::~CCTiledLayerImpl()
{
}

void CCTiledLayerImpl::bindContentsTexture(LayerRendererChromium* layerRenderer)
{
    // This function is only valid for single texture layers, e.g. masks.
    ASSERT(m_tiler);
    ASSERT(m_tiler->numTiles() == 1);

    DrawableTile* tile = tileAt(0, 0);
    Platform3DObject textureId = tile ? tile->textureId() : 0;
    ASSERT(textureId);

    layerRenderer->context()->bindTexture(GraphicsContext3D::TEXTURE_2D, textureId);
}

void CCTiledLayerImpl::dumpLayerProperties(TextStream& ts, int indent) const
{
    CCLayerImpl::dumpLayerProperties(ts, indent);
    writeIndent(ts, indent);
    ts << "skipsDraw: " << (!m_tiler || m_skipsDraw) << "\n";
}

bool CCTiledLayerImpl::hasTileAt(int i, int j) const
{
    return m_tiler->tileAt(i, j);
}

DrawableTile* CCTiledLayerImpl::tileAt(int i, int j) const
{
    return static_cast<DrawableTile*>(m_tiler->tileAt(i, j));
}

DrawableTile* CCTiledLayerImpl::createTile(int i, int j)
{
    RefPtr<DrawableTile> tile = adoptRef(new DrawableTile());
    m_tiler->addTile(tile, i, j);
    return tile.get();
}

TransformationMatrix CCTiledLayerImpl::quadTransform() const
{
    TransformationMatrix transform = drawTransform();

    if (contentBounds().isEmpty())
        return transform;

    transform.scaleNonUniform(bounds().width() / static_cast<double>(contentBounds().width()),
                              bounds().height() / static_cast<double>(contentBounds().height()));

    // Tiler draws with a different origin from other layers.
    transform.translate(-contentBounds().width() / 2.0, -contentBounds().height() / 2.0);
    return transform;
}

void CCTiledLayerImpl::appendQuads(CCQuadList& quadList, const CCSharedQuadState* sharedQuadState)
{
    const IntRect& layerRect = visibleLayerRect();

    if (m_skipsDraw || !m_tiler || m_tiler->isEmpty() || layerRect.isEmpty())
        return;

    int left, top, right, bottom;
    m_tiler->layerRectToTileIndices(layerRect, left, top, right, bottom);
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            DrawableTile* tile = tileAt(i, j);
            IntRect tileRect = m_tiler->tileBounds(i, j);
            IntRect displayRect = tileRect;
            tileRect.intersect(layerRect);

            // Skip empty tiles.
            if (tileRect.isEmpty())
                continue;

            if (!tile || !tile->textureId()) {
                quadList.append(CCSolidColorDrawQuad::create(sharedQuadState, tileRect, backgroundColor()));
                continue;
            }

            // Keep track of how the top left has moved, so the texture can be
            // offset the same amount.
            IntSize displayOffset = tileRect.minXMinYCorner() - displayRect.minXMinYCorner();
            IntPoint textureOffset = m_tiler->textureOffset(i, j) + displayOffset;
            float tileWidth = static_cast<float>(m_tiler->tileSize().width());
            float tileHeight = static_cast<float>(m_tiler->tileSize().height());
            IntSize textureSize(tileWidth, tileHeight);

            bool useAA = m_tiler->hasBorderTexels() && !sharedQuadState->isLayerAxisAlignedIntRect();

            bool leftEdgeAA = !i && useAA;
            bool topEdgeAA = !j && useAA;
            bool rightEdgeAA = i == m_tiler->numTilesX() - 1 && useAA;
            bool bottomEdgeAA = j == m_tiler->numTilesY() - 1 && useAA;

            const GC3Dint textureFilter = m_tiler->hasBorderTexels() ? GraphicsContext3D::LINEAR : GraphicsContext3D::NEAREST;
            quadList.append(CCTileDrawQuad::create(sharedQuadState, tileRect, tile->textureId(), textureOffset, textureSize, textureFilter, contentsSwizzled(), leftEdgeAA, topEdgeAA, rightEdgeAA, bottomEdgeAA));

            if (hasDebugBorders()) {
                Color color(debugBorderColor().red(), debugBorderColor().green(), debugBorderColor().blue(), debugTileBorderAlpha);
                quadList.append(CCDebugBorderDrawQuad::create(sharedQuadState, tileRect, color, debugTileBorderWidth));
            }
        }
    }
}

void CCTiledLayerImpl::setTilingData(const CCLayerTilingData& tiler)
{
    if (m_tiler)
        m_tiler->reset();
    else
        m_tiler = CCLayerTilingData::create(tiler.tileSize(), tiler.hasBorderTexels() ? CCLayerTilingData::HasBorderTexels : CCLayerTilingData::NoBorderTexels);
    *m_tiler = tiler;
}

void CCTiledLayerImpl::syncTextureId(int i, int j, Platform3DObject textureId)
{
    DrawableTile* tile = tileAt(i, j);
    if (!tile)
        tile = createTile(i, j);
    tile->setTextureId(textureId);
}

}

#endif // USE(ACCELERATED_COMPOSITING)
