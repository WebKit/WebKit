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

const CCLayerTilingData& CCLayerTilingData::operator=(const CCLayerTilingData& tiler)
{
    m_tileSize = tiler.m_tileSize;
    m_layerPosition = tiler.m_layerPosition;
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
    m_tilingData.setTotalSize(0, 0);
}

void CCLayerTilingData::contentRectToTileIndices(const IntRect& contentRect, int& left, int& top, int& right, int& bottom) const
{
    const IntRect layerRect = contentRectToLayerRect(contentRect);

    left = m_tilingData.tileXIndexFromSrcCoord(layerRect.x());
    top = m_tilingData.tileYIndexFromSrcCoord(layerRect.y());
    right = m_tilingData.tileXIndexFromSrcCoord(layerRect.maxX() - 1);
    bottom = m_tilingData.tileYIndexFromSrcCoord(layerRect.maxY() - 1);
}

IntRect CCLayerTilingData::contentRectToLayerRect(const IntRect& contentRect) const
{
    IntPoint pos(contentRect.x() - m_layerPosition.x(), contentRect.y() - m_layerPosition.y());
    IntRect layerRect(pos, contentRect.size());

    // Clip to the position.
    if (pos.x() < 0 || pos.y() < 0)
        layerRect = IntRect(IntPoint(0, 0), IntSize(contentRect.width() + pos.x(), contentRect.height() + pos.y()));
    return layerRect;
}

IntRect CCLayerTilingData::layerRectToContentRect(const IntRect& layerRect) const
{
    IntRect contentRect = layerRect;
    contentRect.move(m_layerPosition.x(), m_layerPosition.y());
    return contentRect;
}

IntRect CCLayerTilingData::tileContentRect(const Tile* tile) const
{
    IntRect contentRect = tileLayerRect(tile);
    contentRect.move(m_layerPosition.x(), m_layerPosition.y());
    return contentRect;
}

IntRect CCLayerTilingData::tileLayerRect(const Tile* tile) const
{
    const int index = m_tilingData.tileIndex(tile->i(), tile->j());
    IntRect layerRect = m_tilingData.tileBoundsWithBorder(index);
    layerRect.setSize(m_tileSize);
    return layerRect;
}

void CCLayerTilingData::setLayerPosition(const IntPoint& layerPosition)
{
    m_layerPosition = layerPosition;
}

void CCLayerTilingData::growLayerToContain(const IntRect& contentRect)
{
    // Grow the tile array to contain this content rect.
    IntRect layerRect = contentRectToLayerRect(contentRect);
    IntSize rectSize = IntSize(layerRect.maxX(), layerRect.maxY());

    IntSize oldLayerSize(m_tilingData.totalSizeX(), m_tilingData.totalSizeY());
    IntSize newSize = rectSize.expandedTo(oldLayerSize);
    m_tilingData.setTotalSize(newSize.width(), newSize.height());
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
