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

#include "Extensions3DChromium.h"
#include "FloatQuad.h"
#include "GraphicsContext.h"
#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "LayerTexture.h"
#include "LayerTextureUpdater.h"
#include "PlatformColor.h"
#include "TraceEvent.h"

using namespace std;

namespace WebCore {

typedef FloatPoint3D Edge;

PassOwnPtr<LayerTilerChromium> LayerTilerChromium::create(LayerRendererChromium* layerRenderer, const IntSize& tileSize, BorderTexelOption border)
{
    if (!layerRenderer || tileSize.isEmpty())
        return nullptr;

    return adoptPtr(new LayerTilerChromium(layerRenderer, tileSize, border));
}

LayerTilerChromium::LayerTilerChromium(LayerRendererChromium* layerRenderer, const IntSize& tileSize, BorderTexelOption border)
    : m_textureFormat(PlatformColor::bestTextureFormat(layerRenderer->context()))
    , m_skipsDraw(false)
    , m_textureOrientation(LayerTextureUpdater::InvalidOrientation)
    , m_sampledTexelFormat(LayerTextureUpdater::SampledTexelFormatInvalid)
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
    m_tilingData.setMaxTextureSize(max(size.width(), size.height()));
}

LayerTexture* LayerTilerChromium::getSingleTexture()
{
    if (m_tilingData.numTiles() != 1)
        return 0;

    Tile* tile = tileAt(0, 0);
    return tile ? tile->texture() : 0;
}

void LayerTilerChromium::reset()
{
    m_tiles.clear();
    m_unusedTiles.clear();
    m_tilingData.setTotalSize(0, 0);
    m_paintRect = IntRect();
    m_updateRect = IntRect();
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
        TextureManager* manager = layerRenderer()->contentsTextureManager();
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
    if (contentRect.isEmpty() || m_skipsDraw)
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

void LayerTilerChromium::protectTileTextures(const IntRect& contentRect)
{
    if (contentRect.isEmpty())
        return;

    int left, top, right, bottom;
    contentRectToTileIndices(contentRect, left, top, right, bottom);

    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            Tile* tile = tileAt(i, j);
            if (!tile || !tile->texture()->isValid(m_tileSize, GraphicsContext3D::RGBA))
                continue;

            tile->texture()->reserve(m_tileSize, GraphicsContext3D::RGBA);
        }
    }
}

void LayerTilerChromium::prepareToUpdate(const IntRect& contentRect, LayerTextureUpdater* textureUpdater)
{
    m_skipsDraw = false;

    if (contentRect.isEmpty()) {
        m_updateRect = IntRect();
        return;
    }

    // Invalidate old tiles that were previously used but aren't in use this
    // frame so that they can get reused for new tiles.
    invalidateTiles(contentRect);
    growLayerToContain(contentRect);

    if (!numTiles()) {
        m_updateRect = IntRect();
        return;
    }

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

            if (!tile->texture()->isValid(m_tileSize, m_textureFormat))
                tile->m_dirtyLayerRect = tileLayerRect(tile);

            if (!tile->texture()->reserve(m_tileSize, m_textureFormat)) {
                m_skipsDraw = true;
                reset();
                return;
            }

            dirtyLayerRect.unite(tile->m_dirtyLayerRect);
        }
    }

    // Due to borders, when the paint rect is extended to tile boundaries, it
    // may end up overlapping more tiles than the original content rect. Record
    // that original rect so we don't upload more tiles than necessary.
    m_updateRect = contentRect;

    m_paintRect = layerRectToContentRect(dirtyLayerRect);
    if (dirtyLayerRect.isEmpty())
        return;

    textureUpdater->prepareToUpdate(m_paintRect, m_tileSize, m_tilingData.borderTexels());
}

void LayerTilerChromium::updateRect(LayerTextureUpdater* textureUpdater)
{
    // Painting could cause compositing to get turned off, which may cause the tiler to become invalidated mid-update.
    if (!m_tilingData.totalSizeX() || !m_tilingData.totalSizeY() || m_updateRect.isEmpty() || !numTiles() || m_skipsDraw)
        return;

    GraphicsContext3D* context = layerRendererContext();
    m_textureOrientation = textureUpdater->orientation();
    m_sampledTexelFormat = textureUpdater->sampledTexelFormat(m_textureFormat);

    int left, top, right, bottom;
    contentRectToTileIndices(m_updateRect, left, top, right, bottom);
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            Tile* tile = tileAt(i, j);
            if (!tile)
                tile = createTile(i, j);
            else if (!tile->dirty())
                continue;

            // Calculate page-space rectangle to copy from.
            IntRect sourceRect = tileContentRect(tile);
            const IntPoint anchor = sourceRect.location();
            sourceRect.intersect(layerRectToContentRect(tile->m_dirtyLayerRect));
            // Paint rect not guaranteed to line up on tile boundaries, so
            // make sure that sourceRect doesn't extend outside of it.
            sourceRect.intersect(m_paintRect);
            if (sourceRect.isEmpty())
                continue;

            ASSERT(tile->texture()->isReserved());

            // Calculate tile-space rectangle to upload into.
            IntRect destRect(IntPoint(sourceRect.x() - anchor.x(), sourceRect.y() - anchor.y()), sourceRect.size());
            if (destRect.x() < 0)
                CRASH();
            if (destRect.y() < 0)
                CRASH();

            // Offset from paint rectangle to this tile's dirty rectangle.
            IntPoint paintOffset(sourceRect.x() - m_paintRect.x(), sourceRect.y() - m_paintRect.y());
            if (paintOffset.x() < 0)
                CRASH();
            if (paintOffset.y() < 0)
                CRASH();
            if (paintOffset.x() + destRect.width() > m_paintRect.width())
                CRASH();
            if (paintOffset.y() + destRect.height() > m_paintRect.height())
                CRASH();

            tile->texture()->bindTexture();
            const GC3Dint filter = m_tilingData.borderTexels() ? GraphicsContext3D::LINEAR : GraphicsContext3D::NEAREST;
            GLC(context, context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, filter));
            GLC(context, context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, filter));
            GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, 0));

            textureUpdater->updateTextureRect(tile->texture(), sourceRect, destRect);
            tile->clearDirty();
        }
    }
}

void LayerTilerChromium::setLayerPosition(const IntPoint& layerPosition)
{
    m_layerPosition = layerPosition;
}

void LayerTilerChromium::draw(const IntRect& contentRect, const TransformationMatrix& globalTransform, float opacity)
{
    if (m_skipsDraw || !m_tiles.size() || contentRect.isEmpty())
        return;

    // Use anti-aliasing programs when border texels are preset and transform
    // is not an integer translation.
    bool useAA = (m_tilingData.borderTexels() && !globalTransform.isIntegerTranslation());

    switch (m_sampledTexelFormat) {
    case LayerTextureUpdater::SampledTexelFormatRGBA:
        if (useAA)
            drawTiles(contentRect, globalTransform, opacity, layerRenderer()->tilerProgramAA(), layerRenderer()->tilerProgramAA()->fragmentShader().fragmentTexTransformLocation(), layerRenderer()->tilerProgramAA()->fragmentShader().edgeLocation());
        else
            drawTiles(contentRect, globalTransform, opacity, layerRenderer()->tilerProgram(), -1, -1);
        break;
    case LayerTextureUpdater::SampledTexelFormatBGRA:
        if (useAA)
            drawTiles(contentRect, globalTransform, opacity, layerRenderer()->tilerProgramSwizzleAA(), layerRenderer()->tilerProgramSwizzleAA()->fragmentShader().fragmentTexTransformLocation(), layerRenderer()->tilerProgramSwizzleAA()->fragmentShader().edgeLocation());
        else
            drawTiles(contentRect, globalTransform, opacity, layerRenderer()->tilerProgramSwizzle(), -1, -1);
        break;
    default:
        ASSERT_NOT_REACHED();
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

static bool isCCW(const FloatQuad& quad)
{
    FloatPoint v1 = FloatPoint(quad.p2().x() - quad.p1().x(),
                               quad.p2().y() - quad.p1().y());
    FloatPoint v2 = FloatPoint(quad.p3().x() - quad.p2().x(),
                               quad.p3().y() - quad.p2().y());
    return (v1.x() * v2.y() - v1.y() * v2.x()) < 0;
}

static Edge computeEdge(const FloatPoint& p, const FloatPoint& q, float sign)
{
    ASSERT(p != q);

    FloatPoint tangent(p.y() - q.y(), q.x() - p.x());
    float scale = sign / tangent.length();
    float cross2 = p.x() * q.y() - q.x() * p.y();

    return Edge(tangent.x() * scale,
                tangent.y() * scale,
                cross2 * scale);
}

static FloatPoint intersect(const Edge& a, const Edge& b)
{
    return FloatPoint(
        (a.y() * b.z() - b.y() * a.z()) / (a.x() * b.y() - b.x() * a.y()),
        (a.x() * b.z() - b.x() * a.z()) / (b.x() * a.y() - a.x() * b.y()));
}

template <class T>
void LayerTilerChromium::drawTiles(const IntRect& contentRect, const TransformationMatrix& globalTransform, float opacity, const T* program, int fragmentTexTransformLocation, int edgeLocation)
{
    TransformationMatrix matrix(layerRenderer()->windowMatrix() * layerRenderer()->projectionMatrix() * globalTransform);

    // We don't care about Z component.
    TransformationMatrix matrixXYW =
        TransformationMatrix(matrix.m11(), matrix.m12(), 0, matrix.m14(),
                             matrix.m21(), matrix.m22(), 0, matrix.m24(),
                             matrix.m31(), matrix.m32(), 1, matrix.m34(),
                             matrix.m41(), matrix.m42(), 0, matrix.m44());

    // Don't draw any tiles when matrix is not invertible.
    if (!matrixXYW.isInvertible())
        return;

    TransformationMatrix inverse = matrixXYW.inverse();

    GraphicsContext3D* context = layerRendererContext();
    GLC(context, context->useProgram(program->program()));
    GLC(context, context->uniform1i(program->fragmentShader().samplerLocation(), 0));
    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE0));

    // Map content rectangle to device space.
    FloatQuad deviceQuad = matrix.mapQuad(FloatQuad(contentRect));

    // Counter-clockwise?
    float sign = isCCW(deviceQuad) ? -1 : 1;

    // Compute outer edges.
    Edge leftEdge = computeEdge(deviceQuad.p4(), deviceQuad.p1(), sign);
    Edge rightEdge = computeEdge(deviceQuad.p2(), deviceQuad.p3(), sign);
    Edge topEdge = computeEdge(deviceQuad.p1(), deviceQuad.p2(), sign);
    Edge bottomEdge = computeEdge(deviceQuad.p3(), deviceQuad.p4(), sign);

    if (edgeLocation != -1) {
        // Move outer edges to ensure that all partially covered pixels are
        // processed.
        leftEdge.move(0, 0, 0.5f);
        rightEdge.move(0, 0, 0.5f);
        topEdge.move(0, 0, 0.5f);
        bottomEdge.move(0, 0, 0.5f);

        float edge[12];
        edge[0] = leftEdge.x();
        edge[1] = leftEdge.y();
        edge[2] = leftEdge.z();
        edge[3] = topEdge.x();
        edge[4] = topEdge.y();
        edge[5] = topEdge.z();
        edge[6] = rightEdge.x();
        edge[7] = rightEdge.y();
        edge[8] = rightEdge.z();
        edge[9] = bottomEdge.x();
        edge[10] = bottomEdge.y();
        edge[11] = bottomEdge.z();
        GLC(context, context->uniform3fv(edgeLocation, edge, 4));
    }

    Edge prevEdgeY = topEdge;

    int left, top, right, bottom;
    contentRectToTileIndices(contentRect, left, top, right, bottom);
    IntRect layerRect = contentRectToLayerRect(contentRect);
    for (int j = top; j <= bottom; ++j) {
        Edge prevEdgeX = leftEdge;

        Edge edgeY = bottomEdge;
        if (j < (m_tilingData.numTilesY() - 1)) {
            IntRect tileRect = unionRect(m_tilingData.tileBounds(m_tilingData.tileIndex(0, j)), m_tilingData.tileBounds(m_tilingData.tileIndex(m_tilingData.numTilesX() - 1, j)));
            tileRect.intersect(layerRect);

            // Skip empty rows.
            if (tileRect.isEmpty())
                continue;

            tileRect.move(m_layerPosition.x(), m_layerPosition.y());

            FloatPoint p1(tileRect.maxX(), tileRect.maxY());
            FloatPoint p2(tileRect.x(), tileRect.maxY());

            // Map points to device space.
            p1 = matrix.mapPoint(p1);
            p2 = matrix.mapPoint(p2);

            // Compute horizontal edge.
            edgeY = computeEdge(p1, p2, sign);
        }

        for (int i = left; i <= right; ++i) {
            Tile* tile = tileAt(i, j);
            if (!tile)
                continue;

            ASSERT(tile->texture()->isReserved());

            tile->texture()->bindTexture();

            // Don't use tileContentRect here, as that contains the full
            // rect with border texels which shouldn't be drawn.
            IntRect tileRect = m_tilingData.tileBounds(m_tilingData.tileIndex(tile->i(), tile->j()));
            IntRect displayRect = tileRect;
            tileRect.intersect(layerRect);

            // Keep track of how the top left has moved, so the texture can be
            // offset the same amount.
            IntSize displayOffset = tileRect.minXMinYCorner() - displayRect.minXMinYCorner();

            // Skip empty tiles.
            if (tileRect.isEmpty())
                continue;

            tileRect.move(m_layerPosition.x(), m_layerPosition.y());

            FloatRect clampRect(tileRect);
            // Clamp texture coordinates to avoid sampling outside the layer
            // by deflating the tile region half a texel or half a texel
            // minus epsilon for one pixel layers. The resulting clamp region
            // is mapped to the unit square by the vertex shader and mapped
            // back to normalized texture coordinates by the fragment shader
            // after being clamped to 0-1 range.
            const float epsilon = 1 / 1024.0f;
            float clampX = min(0.5, clampRect.width() / 2.0 - epsilon);
            float clampY = min(0.5, clampRect.height() / 2.0 - epsilon);
            clampRect.inflateX(-clampX);
            clampRect.inflateY(-clampY);
            FloatSize clampOffset = clampRect.minXMinYCorner() - FloatRect(tileRect).minXMinYCorner();

            FloatPoint texOffset = m_tilingData.textureOffset(tile->i(), tile->j()) + clampOffset + FloatSize(displayOffset);
            float tileWidth = static_cast<float>(m_tileSize.width());
            float tileHeight = static_cast<float>(m_tileSize.height());

            // Map clamping rectangle to unit square.
            float vertexTexTranslateX = -clampRect.x() / clampRect.width();
            float vertexTexTranslateY = -clampRect.y() / clampRect.height();
            float vertexTexScaleX = tileRect.width() / clampRect.width();
            float vertexTexScaleY = tileRect.height() / clampRect.height();

            // Map to normalized texture coordinates.
            float fragmentTexTranslateX = texOffset.x() / tileWidth;
            float fragmentTexTranslateY = texOffset.y() / tileHeight;
            float fragmentTexScaleX = clampRect.width() / tileWidth;
            float fragmentTexScaleY = clampRect.height() / tileHeight;

            // OpenGL coordinate system is bottom-up.
            // If tile texture is top-down, we need to flip the texture coordinates.
            if (m_textureOrientation == LayerTextureUpdater::TopDownOrientation) {
                fragmentTexTranslateY = 1.0 - fragmentTexTranslateY;
                fragmentTexScaleY *= -1.0;
            }

            Edge edgeX = rightEdge;
            if (i < (m_tilingData.numTilesX() - 1)) {
                FloatPoint p1(tileRect.maxX(), tileRect.y());
                FloatPoint p2(tileRect.maxX(), tileRect.maxY());

                // Map points to device space.
                p1 = matrix.mapPoint(p1);
                p2 = matrix.mapPoint(p2);

                // Compute vertical edge.
                edgeX = computeEdge(p1, p2, sign);
            }

            // Create device space quad.
            FloatQuad deviceQuad(intersect(edgeY, prevEdgeX),
                                 intersect(prevEdgeX, prevEdgeY),
                                 intersect(prevEdgeY, edgeX),
                                 intersect(edgeX, edgeY));

            // Map quad to layer space.
            FloatQuad quad = inverse.mapQuad(deviceQuad);

            // Normalize to tileRect.
            quad.scale(1.0f / tileRect.width(), 1.0f / tileRect.height());

            if (fragmentTexTransformLocation == -1) {
                // Move fragment shader transform to vertex shader. We can do
                // this while still producing correct results as
                // fragmentTexTransformLocation should always be non-negative
                // when tiles are transformed in a way that could result in
                // sampling outside the layer.
                vertexTexScaleX *= fragmentTexScaleX;
                vertexTexScaleY *= fragmentTexScaleY;
                vertexTexTranslateX *= fragmentTexScaleX;
                vertexTexTranslateY *= fragmentTexScaleY;
                vertexTexTranslateX += fragmentTexTranslateX;
                vertexTexTranslateY += fragmentTexTranslateY;
            } else
                GLC(context, context->uniform4f(fragmentTexTransformLocation, fragmentTexTranslateX, fragmentTexTranslateY, fragmentTexScaleX, fragmentTexScaleY));

            GLC(context, context->uniform4f(program->vertexShader().vertexTexTransformLocation(), vertexTexTranslateX, vertexTexTranslateY, vertexTexScaleX, vertexTexScaleY));

            float point[8];
            point[0] = quad.p1().x();
            point[1] = quad.p1().y();
            point[2] = quad.p2().x();
            point[3] = quad.p2().y();
            point[4] = quad.p3().x();
            point[5] = quad.p3().y();
            point[6] = quad.p4().x();
            point[7] = quad.p4().y();
            GLC(context, context->uniform2fv(program->vertexShader().pointLocation(), point, 4));

            LayerChromium::drawTexturedQuad(context, layerRenderer()->projectionMatrix(), globalTransform,
                                            tileRect.width(), tileRect.height(), opacity,
                                            program->vertexShader().matrixLocation(),
                                            program->fragmentShader().alphaLocation());

            prevEdgeX = edgeX;
            // Reverse direction.
            prevEdgeX.scale(-1, -1, -1);
        }

        prevEdgeY = edgeY;
        // Reverse direction.
        prevEdgeY.scale(-1, -1, -1);
    }
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
