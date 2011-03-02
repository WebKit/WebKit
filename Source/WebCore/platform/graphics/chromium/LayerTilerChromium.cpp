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
    m_lastUpdateLayerRect = IntRect();
}

LayerTilerChromium::Tile* LayerTilerChromium::createTile(int i, int j)
{
    const int index = tileIndex(i, j);
    ASSERT(!m_tiles[index]);

    if (m_unusedTiles.size() > 0) {
        m_tiles[index] = m_unusedTiles.last().release();
        m_unusedTiles.removeLast();
    } else {
        GraphicsContext3D* context = layerRendererContext();
        TextureManager* manager = layerRenderer()->textureManager();
        OwnPtr<Tile> tile = adoptPtr(new Tile(LayerTexture::create(context, manager)));
        m_tiles[index] = tile.release();
    }

    m_tiles[index]->m_dirtyLayerRect = tileLayerRect(i, j);
    return m_tiles[index].get();
}

void LayerTilerChromium::invalidateTiles(const IntRect& oldLayerRect, const IntRect& newLayerRect)
{
    if (!m_tiles.size())
        return;

    IntRect oldContentRect = layerRectToContentRect(oldLayerRect);
    int oldLeft, oldTop, oldRight, oldBottom;
    contentRectToTileIndices(oldContentRect, oldLeft, oldTop, oldRight, oldBottom);

    IntRect newContentRect = layerRectToContentRect(newLayerRect);
    int newLeft, newTop, newRight, newBottom;
    contentRectToTileIndices(newContentRect, newLeft, newTop, newRight, newBottom);

    // Iterating through just the old tile indices is an optimization to avoid
    // iterating through the entire m_tiles array.
    for (int j = oldTop; j <= oldBottom; ++j) {
        for (int i = oldLeft; i <= oldRight; ++i) {
            if (i >= newLeft && i <= newRight && j >= newTop && j <= newBottom)
                continue;

            const int index = tileIndex(i, j);
            if (m_tiles[index])
                m_unusedTiles.append(m_tiles[index].release());
        }
    }
}

void LayerTilerChromium::contentRectToTileIndices(const IntRect& contentRect, int &left, int &top, int &right, int &bottom) const
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

int LayerTilerChromium::tileIndex(int i, int j) const
{
    return m_tilingData.tileIndex(i, j);
}

IntRect LayerTilerChromium::tileContentRect(int i, int j) const
{
    IntRect contentRect = tileLayerRect(i, j);
    contentRect.move(m_layerPosition.x(), m_layerPosition.y());
    return contentRect;
}

IntRect LayerTilerChromium::tileLayerRect(int i, int j) const
{
    const int index = m_tilingData.tileIndex(i, j);
    IntRect layerRect = m_tilingData.tileBoundsWithBorder(index);
    layerRect.setSize(m_tileSize);
    return layerRect;
}

IntSize LayerTilerChromium::layerSize() const
{
    return IntSize(m_tilingData.totalSizeX(), m_tilingData.totalSizeY());
}

IntSize LayerTilerChromium::layerTileSize() const
{
    return IntSize(m_tilingData.numTilesX(), m_tilingData.numTilesY());
}

void LayerTilerChromium::invalidateRect(const IntRect& contentRect)
{
    if (contentRect.isEmpty())
        return;

    growLayerToContain(contentRect);

    // Dirty rects are always in layer space, as the layer could be repositioned
    // after invalidation.
    IntRect layerRect = contentRectToLayerRect(contentRect);

    int left, top, right, bottom;
    contentRectToTileIndices(contentRect, left, top, right, bottom);
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            Tile* tile = m_tiles[tileIndex(i, j)].get();
            if (!tile)
                continue;
            IntRect bound = tileLayerRect(i, j);
            bound.intersect(layerRect);
            tile->m_dirtyLayerRect.unite(bound);
        }
    }
}

void LayerTilerChromium::invalidateEntireLayer()
{
    for (size_t i = 0; i < m_tiles.size(); ++i) {
        if (m_tiles[i])
            m_unusedTiles.append(m_tiles[i].release());
    }
    m_tiles.clear();

    m_tilingData.setTotalSize(0, 0);
    m_lastUpdateLayerRect = IntRect();
}

void LayerTilerChromium::update(TilePaintInterface& painter, const IntRect& contentRect)
{
    if (m_skipsDraw)
        return;

    // Invalidate old tiles that were previously used but aren't in use this
    // frame so that they can get reused for new tiles.
    IntRect layerRect = contentRectToLayerRect(contentRect);
    invalidateTiles(m_lastUpdateLayerRect, layerRect);
    m_lastUpdateLayerRect = layerRect;

    growLayerToContain(contentRect);

    // Create tiles as needed, expanding a dirty rect to contain all
    // the dirty regions currently being drawn.
    IntRect dirtyLayerRect;
    int left, top, right, bottom;
    contentRectToTileIndices(contentRect, left, top, right, bottom);
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            Tile* tile = m_tiles[tileIndex(i, j)].get();
            if (!tile)
                tile = createTile(i, j);
            if (!tile->texture()->isValid(m_tileSize, GraphicsContext3D::RGBA))
                tile->m_dirtyLayerRect = tileLayerRect(i, j);
            dirtyLayerRect.unite(tile->m_dirtyLayerRect);
        }
    }

    if (dirtyLayerRect.isEmpty())
        return;

    const IntRect paintRect = layerRectToContentRect(dirtyLayerRect);

    m_canvas.resize(paintRect.size());
    PlatformCanvas::Painter canvasPainter(&m_canvas);
    canvasPainter.context()->translate(-paintRect.x(), -paintRect.y());
    painter.paint(*canvasPainter.context(), paintRect);

    PlatformCanvas::AutoLocker locker(&m_canvas);
    updateFromPixels(paintRect, locker.pixels());
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
            Tile* tile = m_tiles[tileIndex(i, j)].get();
            if (!tile)
                CRASH();
            if (!tile->dirty())
                continue;

            // Calculate page-space rectangle to copy from.
            IntRect sourceRect = tileContentRect(i, j);
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
            const int index = tileIndex(i, j);
            Tile* tile = m_tiles[index].get();
            ASSERT(tile);

            tile->texture()->bindTexture();

            TransformationMatrix tileMatrix;

            // Don't use tileContentRect here, as that contains the full
            // rect with border texels which shouldn't be drawn.
            IntRect tileRect = m_tilingData.tileBounds(index);
            tileRect.move(m_layerPosition.x(), m_layerPosition.y());
            tileMatrix.translate3d(tileRect.x() - contentRect.x() + tileRect.width() / 2.0, tileRect.y() - contentRect.y() + tileRect.height() / 2.0, 0);

            IntPoint texOffset = m_tilingData.textureOffset(i, j);
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

void LayerTilerChromium::resizeLayer(const IntSize& size)
{
    if (layerSize() == size)
        return;

    const IntSize oldTileSize = layerTileSize();
    m_tilingData.setTotalSize(size.width(), size.height());
    const IntSize newTileSize = layerTileSize();

    if (oldTileSize == newTileSize)
        return;

    if (newTileSize.height() && (newTileSize.width() > INT_MAX / newTileSize.height()))
        CRASH();

    Vector<OwnPtr<Tile> > newTiles;
    newTiles.resize(newTileSize.width() * newTileSize.height());
    for (int j = 0; j < oldTileSize.height(); ++j)
        for (int i = 0; i < oldTileSize.width(); ++i)
            newTiles[i + j * newTileSize.width()].swap(m_tiles[i + j * oldTileSize.width()]);
    m_tiles.swap(newTiles);
}

void LayerTilerChromium::growLayerToContain(const IntRect& contentRect)
{
    // Grow the tile array to contain this content rect.
    IntRect layerRect = contentRectToLayerRect(contentRect);
    IntSize rectSize = IntSize(layerRect.maxX(), layerRect.maxY());

    IntSize newSize = rectSize.expandedTo(layerSize());
    resizeLayer(newSize);
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
