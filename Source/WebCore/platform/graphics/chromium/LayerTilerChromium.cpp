/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "LayerTilerChromium.h"

#include "GraphicsContext.h"
#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "LayerTexture.h"
#include "TraceEvent.h"

#include <wtf/PassOwnArrayPtr.h>

using namespace std;

namespace WebCore {

PassOwnPtr<LayerTilerChromium> LayerTilerChromium::create(LayerRendererChromium* layerRenderer, const IntSize& tileSize, BorderTexelOption border)
{
    if (!layerRenderer || tileSize.isEmpty())
        return 0;

    return adoptPtr(new LayerTilerChromium(layerRenderer, tileSize, border));
}

LayerTilerChromium::LayerTilerChromium(LayerRendererChromium* layerRenderer, const IntSize& tileSize, BorderTexelOption border)
    : m_skipsDraw(false)
    , m_tilingData(max(tileSize.width(), tileSize.height()), 0, 0, border == HasBorderTexels)
    , m_layerRenderer(layerRenderer)
{
    setTileSize(tileSize);
}

LayerTilerChromium::~LayerTilerChromium()
{
    reset();
}

GraphicsContext3D* LayerTilerChromium::layerRendererContext() const
{
    ASSERT(layerRenderer());
    return layerRenderer()->context();
}

void LayerTilerChromium::setTileSize(const IntSize& size)
{
    if (m_tileSize == size)
        return;

    reset();

    m_tileSize = size;
    m_tilePixels = adoptArrayPtr(new uint8_t[m_tileSize.width() * m_tileSize.height() * 4]);
    m_tilingData.setMaxTextureSize(max(size.width(), size.height()));
}

void LayerTilerChromium::reset()
{
    m_tiles.clear();
    m_unusedTiles.clear();
    m_tilingData.setTotalSize(0, 0);
}

LayerTilerChromium::Tile* LayerTilerChromium::createTile(int i, int j)
{
    ASSERT(!tileAt(i, j));

    RefPtr<Tile> tile;
    if (m_unusedTiles.size() > 0) {
        tile = m_unusedTiles.last().release();
        m_unusedTiles.removeLast();
        ASSERT(tile->refCount() == 1);
    } else {
        GraphicsContext3D* context = layerRendererContext();
        TextureManager* manager = layerRenderer()->textureManager();
        tile = adoptRef(new Tile(LayerTexture::create(context, manager)));
    }
    m_tiles.add(make_pair(i, j), tile);

    tile->moveTo(i, j);
    tile->m_dirtyLayerRect = tileLayerRect(tile.get());

    return tile.get();
}

void LayerTilerChromium::invalidateTiles(const IntRect& contentRect)
{
    if (!m_tiles.size())
        return;

    Vector<TileMapKey> removeKeys;
    for (TileMap::iterator iter = m_tiles.begin(); iter != m_tiles.end(); ++iter) {
        Tile* tile = iter->second.get();
        IntRect tileRect = tileContentRect(tile);
        if (tileRect.intersects(contentRect))
            continue;
        removeKeys.append(iter->first);
    }

    for (size_t i = 0; i < removeKeys.size(); ++i)
        m_unusedTiles.append(m_tiles.take(removeKeys[i]));
}

void LayerTilerChromium::contentRectToTileIndices(const IntRect& contentRect, int& left, int& top, int& right, int& bottom) const
{
    const IntRect layerRect = contentRectToLayerRect(contentRect);

    left = m_tilingData.tileXIndexFromSrcCoord(layerRect.x());
    top = m_tilingData.tileYIndexFromSrcCoord(layerRect.y());
    right = m_tilingData.tileXIndexFromSrcCoord(layerRect.maxX() - 1);
    bottom = m_tilingData.tileYIndexFromSrcCoord(layerRect.maxY() - 1);
}

IntRect LayerTilerChromium::contentRectToLayerRect(const IntRect& contentRect) const
{
    IntPoint pos(contentRect.x() - m_layerPosition.x(), contentRect.y() - m_layerPosition.y());
    IntRect layerRect(pos, contentRect.size());

    // Clip to the position.
    if (pos.x() < 0 || pos.y() < 0)
        layerRect = IntRect(IntPoint(0, 0), IntSize(contentRect.width() + pos.x(), contentRect.height() + pos.y()));
    return layerRect;
}

IntRect LayerTilerChromium::layerRectToContentRect(const IntRect& layerRect) const
{
    IntRect contentRect = layerRect;
    contentRect.move(m_layerPosition.x(), m_layerPosition.y());
    return contentRect;
}

LayerTilerChromium::Tile* LayerTilerChromium::tileAt(int i, int j) const
{
    Tile* tile = m_tiles.get(make_pair(i, j)).get();
    ASSERT(!tile || tile->refCount() == 1);
    return tile;
}

IntRect LayerTilerChromium::tileContentRect(const Tile* tile) const
{
    IntRect contentRect = tileLayerRect(tile);
    contentRect.move(m_layerPosition.x(), m_layerPosition.y());
    return contentRect;
}

IntRect LayerTilerChromium::tileLayerRect(const Tile* tile) const
{
    const int index = m_tilingData.tileIndex(tile->i(), tile->j());
    IntRect layerRect = m_tilingData.tileBoundsWithBorder(index);
    layerRect.setSize(m_tileSize);
    return layerRect;
}

void LayerTilerChromium::invalidateRect(const IntRect& contentRect)
{
    if (contentRect.isEmpty())
        return;

    growLayerToContain(contentRect);

    // Dirty rects are always in layer space, as the layer could be repositioned
    // after invalidation.
    const IntRect layerRect = contentRectToLayerRect(contentRect);

    int left, top, right, bottom;
    contentRectToTileIndices(contentRect, left, top, right, bottom);
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            Tile* tile = tileAt(i, j);
            if (!tile)
                continue;
            IntRect bound = tileLayerRect(tile);
            bound.intersect(layerRect);
            tile->m_dirtyLayerRect.unite(bound);
        }
    }
}

void LayerTilerChromium::invalidateEntireLayer()
{
    for (TileMap::iterator iter = m_tiles.begin(); iter != m_tiles.end(); ++iter) {
        ASSERT(iter->second->refCount() == 1);
        m_unusedTiles.append(iter->second.release());
    }
    m_tiles.clear();

    m_tilingData.setTotalSize(0, 0);
}

void LayerTilerChromium::update(TilePaintInterface& painter, const IntRect& contentRect)
{
    if (m_skipsDraw)
        return;

    // Invalidate old tiles that were previously used but aren't in use this
    // frame so that they can get reused for new tiles.
    invalidateTiles(contentRect);
    growLayerToContain(contentRect);

    // Create tiles as needed, expanding a dirty rect to contain all
    // the dirty regions currently being drawn.
    IntRect dirtyLayerRect;
    int left, top, right, bottom;
    contentRectToTileIndices(contentRect, left, top, right, bottom);
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            Tile* tile = tileAt(i, j);
            if (!tile)
                tile = createTile(i, j);
            if (!tile->texture()->isValid(m_tileSize, GraphicsContext3D::RGBA))
                tile->m_dirtyLayerRect = tileLayerRect(tile);
            dirtyLayerRect.unite(tile->m_dirtyLayerRect);
        }
    }

    if (dirtyLayerRect.isEmpty())
        return;

    const IntRect paintRect = layerRectToContentRect(dirtyLayerRect);

    m_canvas.resize(paintRect.size());
    PlatformCanvas::Painter canvasPainter(&m_canvas);
    canvasPainter.context()->translate(-paintRect.x(), -paintRect.y());
    {
        TRACE_EVENT("LayerTilerChromium::update::paint", this, 0);
        painter.paint(*canvasPainter.context(), paintRect);
    }

    PlatformCanvas::AutoLocker locker(&m_canvas);
    {
        TRACE_EVENT("LayerTilerChromium::updateFromPixels", this, 0);
        updateFromPixels(paintRect, locker.pixels());
    }
}

void LayerTilerChromium::updateFromPixels(const IntRect& paintRect, const uint8_t* paintPixels)
{
    // Painting could cause compositing to get turned off, which may cause the tiler to become invalidated mid-update.
    if (!m_tiles.size())
        return;

    GraphicsContext3D* context = layerRendererContext();

    int left, top, right, bottom;
    contentRectToTileIndices(paintRect, left, top, right, bottom);
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            Tile* tile = tileAt(i, j);
            if (!tile)
                CRASH();
            if (!tile->dirty())
                continue;

            // Calculate page-space rectangle to copy from.
            IntRect sourceRect = tileContentRect(tile);
            const IntPoint anchor = sourceRect.location();
            sourceRect.intersect(layerRectToContentRect(tile->m_dirtyLayerRect));
            if (sourceRect.isEmpty())
                continue;

            if (!tile->texture()->reserve(m_tileSize, GraphicsContext3D::RGBA)) {
                m_skipsDraw = true;
                reset();
                return;
            }

            // Calculate tile-space rectangle to upload into.
            IntRect destRect(IntPoint(sourceRect.x() - anchor.x(), sourceRect.y() - anchor.y()), sourceRect.size());
            if (destRect.x() < 0)
                CRASH();
            if (destRect.y() < 0)
                CRASH();

            // Offset from paint rectangle to this tile's dirty rectangle.
            IntPoint paintOffset(sourceRect.x() - paintRect.x(), sourceRect.y() - paintRect.y());
            if (paintOffset.x() < 0)
                CRASH();
            if (paintOffset.y() < 0)
                CRASH();
            if (paintOffset.x() + destRect.width() > paintRect.width())
                CRASH();
            if (paintOffset.y() + destRect.height() > paintRect.height())
                CRASH();

            const uint8_t* pixelSource;
            if (paintRect.width() == sourceRect.width() && !paintOffset.x())
                pixelSource = &paintPixels[4 * paintOffset.y() * paintRect.width()];
            else {
                // Strides not equal, so do a row-by-row memcpy from the
                // paint results into a temp buffer for uploading.
                for (int row = 0; row < destRect.height(); ++row)
                    memcpy(&m_tilePixels[destRect.width() * 4 * row],
                           &paintPixels[4 * (paintOffset.x() + (paintOffset.y() + row) * paintRect.width())],
                           destRect.width() * 4);

                pixelSource = &m_tilePixels[0];
            }

            tile->texture()->bindTexture();
            GLC(context, context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::NEAREST));
            GLC(context, context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::NEAREST));

            GLC(context, context->texSubImage2D(GraphicsContext3D::TEXTURE_2D, 0, destRect.x(), destRect.y(), destRect.width(), destRect.height(), GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, pixelSource));

            tile->clearDirty();
        }
    }
}

void LayerTilerChromium::setLayerPosition(const IntPoint& layerPosition)
{
    m_layerPosition = layerPosition;
}

void LayerTilerChromium::draw(const IntRect& contentRect)
{
    if (m_skipsDraw || !m_tiles.size())
        return;

    GraphicsContext3D* context = layerRendererContext();
    const LayerTilerChromium::Program* program = layerRenderer()->tilerProgram();
    layerRenderer()->useShader(program->program());
    GLC(context, context->uniform1i(program->fragmentShader().samplerLocation(), 0));

    int left, top, right, bottom;
    contentRectToTileIndices(contentRect, left, top, right, bottom);
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            Tile* tile = tileAt(i, j);
            ASSERT(tile);

            tile->texture()->bindTexture();

            TransformationMatrix tileMatrix;

            // Don't use tileContentRect here, as that contains the full
            // rect with border texels which shouldn't be drawn.
            IntRect tileRect = m_tilingData.tileBounds(m_tilingData.tileIndex(tile->i(), tile->j()));
            tileRect.move(m_layerPosition.x(), m_layerPosition.y());
            tileMatrix.translate3d(tileRect.x() - contentRect.x() + tileRect.width() / 2.0, tileRect.y() - contentRect.y() + tileRect.height() / 2.0, 0);

            IntPoint texOffset = m_tilingData.textureOffset(tile->i(), tile->j());
            float tileWidth = static_cast<float>(m_tileSize.width());
            float tileHeight = static_cast<float>(m_tileSize.height());
            float texTranslateX = texOffset.x() / tileWidth;
            float texTranslateY = texOffset.y() / tileHeight;
            float texScaleX = tileRect.width() / tileWidth;
            float texScaleY = tileRect.height() / tileHeight;

            drawTexturedQuad(context, layerRenderer()->projectionMatrix(), tileMatrix, tileRect.width(), tileRect.height(), 1, texTranslateX, texTranslateY, texScaleX, texScaleY, program);

            tile->texture()->unreserve();
        }
    }
}

void LayerTilerChromium::growLayerToContain(const IntRect& contentRect)
{
    // Grow the tile array to contain this content rect.
    IntRect layerRect = contentRectToLayerRect(contentRect);
    IntSize rectSize = IntSize(layerRect.maxX(), layerRect.maxY());

    IntSize oldLayerSize(m_tilingData.totalSizeX(), m_tilingData.totalSizeY());
    IntSize newSize = rectSize.expandedTo(oldLayerSize);
    m_tilingData.setTotalSize(newSize.width(), newSize.height());
}

void LayerTilerChromium::drawTexturedQuad(GraphicsContext3D* context, const TransformationMatrix& projectionMatrix, const TransformationMatrix& drawMatrix,
                                     float width, float height, float opacity,
                                     float texTranslateX, float texTranslateY,
                                     float texScaleX, float texScaleY,
                                     const LayerTilerChromium::Program* program)
{
    static float glMatrix[16];

    TransformationMatrix renderMatrix = drawMatrix;

    // Apply a scaling factor to size the quad from 1x1 to its intended size.
    renderMatrix.scale3d(width, height, 1);

    // Apply the projection matrix before sending the transform over to the shader.
    LayerChromium::toGLMatrix(&glMatrix[0], projectionMatrix * renderMatrix);

    GLC(context, context->uniformMatrix4fv(program->vertexShader().matrixLocation(), false, &glMatrix[0], 1));

    GLC(context, context->uniform1f(program->fragmentShader().alphaLocation(), opacity));

    GLC(context, context->uniform4f(program->vertexShader().texTransformLocation(),
        texTranslateX, texTranslateY, texScaleX, texScaleY));

    GLC(context, context->drawElements(GraphicsContext3D::TRIANGLES, 6, GraphicsContext3D::UNSIGNED_SHORT, 0));
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
