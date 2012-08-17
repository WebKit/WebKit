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

#include "CCTiledLayerImpl.h"

#include "CCCheckerboardDrawQuad.h"
#include "CCDebugBorderDrawQuad.h"
#include "CCLayerTilingData.h"
#include "CCMathUtil.h"
#include "CCQuadSink.h"
#include "CCSolidColorDrawQuad.h"
#include "CCTileDrawQuad.h"
#include "FloatQuad.h"
#include "GraphicsContext3D.h"
#include "SkColor.h"
#include "TextStream.h"
#include <wtf/text/WTFString.h>

using namespace std;
using WebKit::WebTransformationMatrix;

namespace WebCore {

static const int debugTileBorderWidth = 1;
static const int debugTileBorderAlpha = 100;
static const int debugTileBorderColorRed = 80;
static const int debugTileBorderColorGreen = 200;
static const int debugTileBorderColorBlue = 200;
static const int debugTileBorderMissingTileColorRed = 255;
static const int debugTileBorderMissingTileColorGreen = 0;
static const int debugTileBorderMissingTileColorBlue = 0;

class DrawableTile : public CCLayerTilingData::Tile {
    WTF_MAKE_NONCOPYABLE(DrawableTile);
public:
    static PassOwnPtr<DrawableTile> create() { return adoptPtr(new DrawableTile()); }

    CCResourceProvider::ResourceId resourceId() const { return m_resourceId; }
    void setResourceId(CCResourceProvider::ResourceId resourceId) { m_resourceId = resourceId; }

private:
    DrawableTile() : m_resourceId(0) { }

    CCResourceProvider::ResourceId m_resourceId;
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

CCResourceProvider::ResourceId CCTiledLayerImpl::contentsResourceId() const
{
    // This function is only valid for single texture layers, e.g. masks.
    ASSERT(m_tiler);
    ASSERT(m_tiler->numTilesX() == 1);
    ASSERT(m_tiler->numTilesY() == 1);

    DrawableTile* tile = tileAt(0, 0);
    CCResourceProvider::ResourceId resourceId = tile ? tile->resourceId() : 0;
    ASSERT(resourceId);

    return resourceId;
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

bool CCTiledLayerImpl::hasTextureIdForTileAt(int i, int j) const
{
    return hasTileAt(i, j) && tileAt(i, j)->resourceId();
}

DrawableTile* CCTiledLayerImpl::tileAt(int i, int j) const
{
    return static_cast<DrawableTile*>(m_tiler->tileAt(i, j));
}

DrawableTile* CCTiledLayerImpl::createTile(int i, int j)
{
    OwnPtr<DrawableTile> tile(DrawableTile::create());
    DrawableTile* addedTile = tile.get();
    m_tiler->addTile(tile.release(), i, j);
    return addedTile;
}

void CCTiledLayerImpl::appendQuads(CCQuadSink& quadList, const CCSharedQuadState* sharedQuadState, bool& hadMissingTiles)
{
    const IntRect& contentRect = visibleContentRect();

    if (!m_tiler || m_tiler->hasEmptyBounds() || contentRect.isEmpty())
        return;

    int left, top, right, bottom;
    m_tiler->contentRectToTileIndices(contentRect, left, top, right, bottom);

    if (hasDebugBorders()) {
        for (int j = top; j <= bottom; ++j) {
            for (int i = left; i <= right; ++i) {
                DrawableTile* tile = tileAt(i, j);
                IntRect tileRect = m_tiler->tileBounds(i, j);
                SkColor borderColor;

                if (m_skipsDraw || !tile || !tile->resourceId())
                    borderColor = SkColorSetARGB(debugTileBorderAlpha, debugTileBorderMissingTileColorRed, debugTileBorderMissingTileColorGreen, debugTileBorderMissingTileColorBlue);
                else
                    borderColor = SkColorSetARGB(debugTileBorderAlpha, debugTileBorderColorRed, debugTileBorderColorGreen, debugTileBorderColorBlue);
                quadList.append(CCDebugBorderDrawQuad::create(sharedQuadState, tileRect, borderColor, debugTileBorderWidth));
            }
        }
    }

    if (m_skipsDraw)
        return;

    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            DrawableTile* tile = tileAt(i, j);
            IntRect tileRect = m_tiler->tileBounds(i, j);
            IntRect displayRect = tileRect;
            tileRect.intersect(contentRect);

            // Skip empty tiles.
            if (tileRect.isEmpty())
                continue;

            if (!tile || !tile->resourceId()) {
                if (drawCheckerboardForMissingTiles())
                    hadMissingTiles |= quadList.append(CCCheckerboardDrawQuad::create(sharedQuadState, tileRect));
                else
                    hadMissingTiles |= quadList.append(CCSolidColorDrawQuad::create(sharedQuadState, tileRect, backgroundColor()));
                continue;
            }

            IntRect tileOpaqueRect = tile->opaqueRect();
            tileOpaqueRect.intersect(contentRect);

            // Keep track of how the top left has moved, so the texture can be
            // offset the same amount.
            IntSize displayOffset = tileRect.minXMinYCorner() - displayRect.minXMinYCorner();
            IntPoint textureOffset = m_tiler->textureOffset(i, j) + displayOffset;
            float tileWidth = static_cast<float>(m_tiler->tileSize().width());
            float tileHeight = static_cast<float>(m_tiler->tileSize().height());
            IntSize textureSize(tileWidth, tileHeight);

            bool clipped = false;
            FloatQuad visibleContentInTargetQuad = CCMathUtil::mapQuad(drawTransform(), FloatQuad(visibleContentRect()), clipped);
            bool isAxisAlignedInTarget = !clipped && visibleContentInTargetQuad.isRectilinear();
            bool useAA = m_tiler->hasBorderTexels() && !isAxisAlignedInTarget;

            bool leftEdgeAA = !i && useAA;
            bool topEdgeAA = !j && useAA;
            bool rightEdgeAA = i == m_tiler->numTilesX() - 1 && useAA;
            bool bottomEdgeAA = j == m_tiler->numTilesY() - 1 && useAA;

            const GC3Dint textureFilter = m_tiler->hasBorderTexels() ? GraphicsContext3D::LINEAR : GraphicsContext3D::NEAREST;
            quadList.append(CCTileDrawQuad::create(sharedQuadState, tileRect, tileOpaqueRect, tile->resourceId(), textureOffset, textureSize, textureFilter, contentsSwizzled(), leftEdgeAA, topEdgeAA, rightEdgeAA, bottomEdgeAA));
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

void CCTiledLayerImpl::pushTileProperties(int i, int j, CCResourceProvider::ResourceId resourceId, const IntRect& opaqueRect)
{
    DrawableTile* tile = tileAt(i, j);
    if (!tile)
        tile = createTile(i, j);
    tile->setResourceId(resourceId);
    tile->setOpaqueRect(opaqueRect);
}

Region CCTiledLayerImpl::visibleContentOpaqueRegion() const
{
    if (m_skipsDraw)
        return Region();
    if (opaque())
        return visibleContentRect();
    return m_tiler->opaqueRegionInContentRect(visibleContentRect());
}

void CCTiledLayerImpl::didLoseContext()
{
    m_tiler->reset();
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
