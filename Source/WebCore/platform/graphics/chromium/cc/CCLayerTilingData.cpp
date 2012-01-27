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

#include "cc/CCLayerTilingData.h"

using namespace std;

namespace WebCore {

PassOwnPtr<CCLayerTilingData> CCLayerTilingData::create(const IntSize& tileSize, BorderTexelOption border)
{
    return adoptPtr(new CCLayerTilingData(tileSize, border));
}

CCLayerTilingData::CCLayerTilingData(const IntSize& tileSize, BorderTexelOption border)
    : m_tilingData(max(tileSize.width(), tileSize.height()), 0, 0, border == HasBorderTexels)
{
    setTileSize(tileSize);
}

void CCLayerTilingData::setTileSize(const IntSize& size)
{
    if (m_tileSize == size)
        return;

    reset();

    m_tileSize = size;
    m_tilingData.setMaxTextureSize(max(size.width(), size.height()));
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
    m_tileSize = tiler.m_tileSize;
    m_tilingData = tiler.m_tilingData;

    return *this;
}

void CCLayerTilingData::addTile(PassRefPtr<Tile> tile, int i, int j)
{
    ASSERT(!tileAt(i, j));
    tile->moveTo(i, j);
    m_tiles.add(make_pair(i, j), tile);
}

PassRefPtr<CCLayerTilingData::Tile> CCLayerTilingData::takeTile(int i, int j)
{
    return m_tiles.take(make_pair(i, j));
}

CCLayerTilingData::Tile* CCLayerTilingData::tileAt(int i, int j) const
{
    Tile* tile = m_tiles.get(make_pair(i, j)).get();
    ASSERT(!tile || tile->refCount() == 1);
    return tile;
}

void CCLayerTilingData::reset()
{
    m_tiles.clear();
}

void CCLayerTilingData::layerRectToTileIndices(const IntRect& layerRect, int& left, int& top, int& right, int& bottom) const
{
    left = m_tilingData.tileXIndexFromSrcCoord(layerRect.x());
    top = m_tilingData.tileYIndexFromSrcCoord(layerRect.y());
    right = m_tilingData.tileXIndexFromSrcCoord(layerRect.maxX() - 1);
    bottom = m_tilingData.tileYIndexFromSrcCoord(layerRect.maxY() - 1);
}

IntRect CCLayerTilingData::tileRect(const Tile* tile) const
{
    const int index = m_tilingData.tileIndex(tile->i(), tile->j());
    IntRect tileRect = m_tilingData.tileBoundsWithBorder(index);
    tileRect.setSize(m_tileSize);
    return tileRect;
}

void CCLayerTilingData::setBounds(const IntSize& size)
{
    m_tilingData.setTotalSize(size.width(), size.height());
}

IntSize CCLayerTilingData::bounds() const
{
    return IntSize(m_tilingData.totalSizeX(), m_tilingData.totalSizeY());
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
