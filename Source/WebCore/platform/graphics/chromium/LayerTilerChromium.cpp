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

#if PLATFORM(SKIA)
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#elif PLATFORM(CG)
#include <CoreGraphics/CGBitmapContext.h>
#endif

#include <wtf/PassOwnArrayPtr.h>

namespace WebCore {

PassOwnPtr<LayerTilerChromium> LayerTilerChromium::create(LayerRendererChromium* layerRenderer, const IntSize& tileSize)
{
    if (!layerRenderer || tileSize.isEmpty())
        return 0;

    return adoptPtr(new LayerTilerChromium(layerRenderer, tileSize));
}

LayerTilerChromium::LayerTilerChromium(LayerRendererChromium* layerRenderer, const IntSize& tileSize)
    : m_skipsDraw(false)
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
}

void LayerTilerChromium::reset()
{
    m_tiles.clear();
    m_unusedTiles.clear();

    m_layerSize = IntSize();
    m_layerTileSize = IntSize();
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

    left = layerRect.x() / m_tileSize.width();
    top = layerRect.y() / m_tileSize.height();
    right = (layerRect.maxX() - 1) / m_tileSize.width();
    bottom = (layerRect.maxY() - 1) / m_tileSize.height();
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
    ASSERT(i >= 0 && j >= 0 && i < m_layerTileSize.width() && j < m_layerTileSize.height());
    return i + j * m_layerTileSize.width();
}

IntRect LayerTilerChromium::tileContentRect(int i, int j) const
{
    IntPoint anchor(m_layerPosition.x() + i * m_tileSize.width(), m_layerPosition.y() + j * m_tileSize.height());
    IntRect tile(anchor, m_tileSize);
    return tile;
}

IntRect LayerTilerChromium::tileLayerRect(int i, int j) const
{
    IntPoint anchor(i * m_tileSize.width(), j * m_tileSize.height());
    IntRect tile(anchor, m_tileSize);
    return tile;
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

    m_layerSize = IntSize();
    m_layerTileSize = IntSize();
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
    GraphicsContext3D* context = layerRendererContext();
#if PLATFORM(SKIA)
    OwnPtr<skia::PlatformCanvas> canvas(new skia::PlatformCanvas(paintRect.width(), paintRect.height(), false));
    OwnPtr<PlatformContextSkia> skiaContext(new PlatformContextSkia(canvas.get()));
    OwnPtr<GraphicsContext> graphicsContext(new GraphicsContext(reinterpret_cast<PlatformGraphicsContext*>(skiaContext.get())));

    // Bring the canvas into the coordinate system of the paint rect.
    canvas->translate(static_cast<SkScalar>(-paintRect.x()), static_cast<SkScalar>(-paintRect.y()));

    painter.paint(*graphicsContext, paintRect);

    // Get the contents of the updated rect.
    const SkBitmap& bitmap = canvas->getDevice()->accessBitmap(false);
    ASSERT(bitmap.width() == paintRect.width() && bitmap.height() == paintRect.height());
    if (bitmap.width() != paintRect.width() || bitmap.height() != paintRect.height())
        CRASH();
    uint8_t* paintPixels = static_cast<uint8_t*>(bitmap.getPixels());
    if (!paintPixels)
        CRASH();
#elif PLATFORM(CG)
    Vector<uint8_t> canvasPixels;
    int rowBytes = 4 * paintRect.width();
    canvasPixels.resize(rowBytes * paintRect.height());
    memset(canvasPixels.data(), 0, canvasPixels.size());
    RetainPtr<CGColorSpaceRef> colorSpace(AdoptCF, CGColorSpaceCreateDeviceRGB());
    RetainPtr<CGContextRef> m_cgContext;
    m_cgContext.adoptCF(CGBitmapContextCreate(canvasPixels.data(),
                                                       paintRect.width(), paintRect.height(), 8, rowBytes,
                                                       colorSpace.get(),
                                                       kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host));
    CGContextTranslateCTM(m_cgContext.get(), 0, paintRect.height());
    CGContextScaleCTM(m_cgContext.get(), 1, -1);
    OwnPtr<GraphicsContext> m_graphicsContext(new GraphicsContext(m_cgContext.get()));

    // Bring the CoreGraphics context into the coordinate system of the paint rect.
    CGContextTranslateCTM(m_cgContext.get(), -paintRect.x(), -paintRect.y());
    painter.paint(*m_graphicsContext, paintRect);

    // Get the contents of the updated rect.
    ASSERT(static_cast<int>(CGBitmapContextGetWidth(m_cgContext.get())) == paintRect.width() && static_cast<int>(CGBitmapContextGetHeight(m_cgContext.get())) == paintRect.height());
    uint8_t* paintPixels = static_cast<uint8_t*>(canvasPixels.data());
#else
#error "Need to implement for your platform."
#endif

    // Painting could cause compositing to get turned off, which may cause the tiler to become invalidated mid-update.
    if (!m_tiles.size())
        return;

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

            uint8_t* pixelSource;
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

    // We reuse the shader program used by ContentLayerChromium.
    GraphicsContext3D* context = layerRendererContext();
    const ContentLayerChromium::SharedValues* contentLayerValues = layerRenderer()->contentLayerSharedValues();
    layerRenderer()->useShader(contentLayerValues->contentShaderProgram());
    GLC(context, context->uniform1i(contentLayerValues->shaderSamplerLocation(), 0));

    int left, top, right, bottom;
    contentRectToTileIndices(contentRect, left, top, right, bottom);
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            Tile* tile = m_tiles[tileIndex(i, j)].get();
            ASSERT(tile);

            tile->texture()->bindTexture();

            TransformationMatrix tileMatrix;
            IntRect tileRect = tileContentRect(i, j);
            tileMatrix.translate3d(tileRect.x() - contentRect.x() + tileRect.width() / 2.0, tileRect.y() - contentRect.y() + tileRect.height() / 2.0, 0);

            LayerChromium::drawTexturedQuad(context, layerRenderer()->projectionMatrix(), tileMatrix, m_tileSize.width(), m_tileSize.height(), 1, contentLayerValues->shaderMatrixLocation(), contentLayerValues->shaderAlphaLocation());

            tile->texture()->unreserve();
        }
    }
}

void LayerTilerChromium::resizeLayer(const IntSize& size)
{
    if (m_layerSize == size)
        return;

    int width = (size.width() + m_tileSize.width() - 1) / m_tileSize.width();
    int height = (size.height() + m_tileSize.height() - 1) / m_tileSize.height();

    if (height && (width > INT_MAX / height))
        CRASH();

    Vector<OwnPtr<Tile> > newTiles;
    newTiles.resize(width * height);
    for (int j = 0; j < m_layerTileSize.height(); ++j)
        for (int i = 0; i < m_layerTileSize.width(); ++i)
            newTiles[i + j * width].swap(m_tiles[i + j * m_layerTileSize.width()]);

    m_tiles.swap(newTiles);
    m_layerSize = size;
    m_layerTileSize = IntSize(width, height);
}

void LayerTilerChromium::growLayerToContain(const IntRect& contentRect)
{
    // Grow the tile array to contain this content rect.
    IntRect layerRect = contentRectToLayerRect(contentRect);
    IntSize layerSize = IntSize(layerRect.maxX(), layerRect.maxY());

    IntSize newSize = layerSize.expandedTo(m_layerSize);
    resizeLayer(newSize);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
