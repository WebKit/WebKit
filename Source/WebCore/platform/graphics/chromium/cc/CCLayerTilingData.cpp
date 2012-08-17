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

#include "CCLayerTilingData.h"

using namespace std;

namespace WebCore {

PassOwnPtr<CCLayerTilingData> CCLayerTilingData::create(const IntSize& tileSize, BorderTexelOption border)
{
    return adoptPtr(new CCLayerTilingData(tileSize, border));
}

CCLayerTilingData::CCLayerTilingData(const IntSize& tileSize, BorderTexelOption border)
    : m_tilingData(tileSize, IntSize(), border == HasBorderTexels)
{
    setTileSize(tileSize);
}

void CCLayerTilingData::setTileSize(const IntSize& size)
{
    if (tileSize() == size)
        return;

    reset();

    m_tilingData.setMaxTextureSize(size);
}

const IntSize& CCLayerTilingData::tileSize() const
{
    return m_tilingData.maxTextureSize();
}

void CCLayerTilingData::setBorderTexelOption(BorderTexelOption borderTexelOption)
{
    bool borderTexels = borderTexelOption == HasBorderTexels;
    if (hasBorderTexels() == borderTexels)
        return;

    reset();
    m_tilingData.setHasBorderTexels(borderTexels);
}

const CCLayerTilingData& CCLayerTilingData::operator=(const CCLayerTilingData& tiler)
{
    m_tilingData = tiler.m_tilingData;

    return *this;
}

void CCLayerTilingData::addTile(PassOwnPtr<Tile> tile, int i, int j)
{
    ASSERT(!tileAt(i, j));
    tile->moveTo(i, j);
    m_tiles.add(make_pair(i, j), tile);
}

PassOwnPtr<CCLayerTilingData::Tile> CCLayerTilingData::takeTile(int i, int j)
{
    return m_tiles.take(make_pair(i, j));
}

CCLayerTilingData::Tile* CCLayerTilingData::tileAt(int i, int j) const
{
    return m_tiles.get(make_pair(i, j));
}

void CCLayerTilingData::reset()
{
    m_tiles.clear();
}

void CCLayerTilingData::contentRectToTileIndices(const IntRect& contentRect, int& left, int& top, int& right, int& bottom) const
{
    // An empty rect doesn't result in an empty set of tiles, so don't pass an empty rect.
    // FIXME: Possibly we should fill a vector of tiles instead,
    //        since the normal use of this function is to enumerate some tiles.
    ASSERT(!contentRect.isEmpty());

    left = m_tilingData.tileXIndexFromSrcCoord(contentRect.x());
    top = m_tilingData.tileYIndexFromSrcCoord(contentRect.y());
    right = m_tilingData.tileXIndexFromSrcCoord(contentRect.maxX() - 1);
    bottom = m_tilingData.tileYIndexFromSrcCoord(contentRect.maxY() - 1);
}

IntRect CCLayerTilingData::tileRect(const Tile* tile) const
{
    IntRect tileRect = m_tilingData.tileBoundsWithBorder(tile->i(), tile->j());
    tileRect.setSize(tileSize());
    return tileRect;
}

Region CCLayerTilingData::opaqueRegionInContentRect(const IntRect& contentRect) const
{
    if (contentRect.isEmpty())
        return Region();

    Region opaqueRegion;
    int left, top, right, bottom;
    contentRectToTileIndices(contentRect, left, top, right, bottom);
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            Tile* tile = tileAt(i, j);
            if (!tile)
                continue;

            IntRect tileOpaqueRect = intersection(contentRect, tile->opaqueRect());
            opaqueRegion.unite(tileOpaqueRect);
        }
    }
    return opaqueRegion;
}

void CCLayerTilingData::setBounds(const IntSize& size)
{
    m_tilingData.setTotalSize(size);
    if (size.isEmpty()) {
        m_tiles.clear();
        return;
    }

    // Any tiles completely outside our new bounds are invalid and should be dropped.
    int left, top, right, bottom;
    contentRectToTileIndices(IntRect(IntPoint(), size), left, top, right, bottom);
    Vector<TileMapKey> invalidTileKeys;
    for (TileMap::const_iterator it = m_tiles.begin(); it != m_tiles.end(); ++it) {
        if (it->first.first > right || it->first.second > bottom)
            invalidTileKeys.append(it->first);
    }
    for (size_t i = 0; i < invalidTileKeys.size(); ++i)
        m_tiles.remove(invalidTileKeys[i]);
}

IntSize CCLayerTilingData::bounds() const
{
    return m_tilingData.totalSize();
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
