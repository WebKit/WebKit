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

#include "FloatQuad.h"
#include "LayerRendererChromium.h"
#include "cc/CCLayerTreeHostImplProxy.h"
#include <wtf/text/WTFString.h>

using namespace std;

namespace WebCore {

typedef FloatPoint3D Edge;
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
{
}

CCTiledLayerImpl::~CCTiledLayerImpl()
{
}

void CCTiledLayerImpl::bindContentsTexture()
{
    // This function is only valid for single texture layers, e.g. masks.
    ASSERT(m_tiler);
    ASSERT(m_tiler->numTiles() == 1);

    DrawableTile* tile = tileAt(0, 0);
    Platform3DObject textureId = tile ? tile->textureId() : 0;
    ASSERT(textureId);

    layerRenderer()->context()->bindTexture(GraphicsContext3D::TEXTURE_2D, textureId);
}

void CCTiledLayerImpl::dumpLayerProperties(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    CCLayerImpl::dumpLayerProperties(ts, indent);
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

void CCTiledLayerImpl::draw()
{
    const IntRect& layerRect = visibleLayerRect();

    if (m_skipsDraw || m_tiler->isEmpty() || layerRect.isEmpty() || !layerRenderer())
        return;

#if defined(OS_CHROMEOS)
    // FIXME: Disable anti-aliasing to workaround broken driver.
    bool useAA = false;
#else
    // Use anti-aliasing programs when border texels are preset and transform
    // is not an integer translation.
    bool useAA = (m_tiler->hasBorderTexels() && !m_tilingTransform.isIntegerTranslation());
#endif

    GraphicsContext3D* context = layerRenderer()->context();
    if (isRootLayer()) {
        context->colorMask(true, true, true, false);
        GLC(context, context->disable(GraphicsContext3D::BLEND));
    }

    switch (m_sampledTexelFormat) {
    case LayerTextureUpdater::SampledTexelFormatRGBA:
        if (useAA)
            drawTiles(layerRenderer(), layerRect, m_tilingTransform, drawOpacity(), layerRenderer()->tilerProgramAA(), layerRenderer()->tilerProgramAA()->fragmentShader().fragmentTexTransformLocation(), layerRenderer()->tilerProgramAA()->fragmentShader().edgeLocation());
        else
            drawTiles(layerRenderer(), layerRect, m_tilingTransform, drawOpacity(), layerRenderer()->tilerProgram(), -1, -1);
        break;
    case LayerTextureUpdater::SampledTexelFormatBGRA:
        if (useAA)
            drawTiles(layerRenderer(), layerRect, m_tilingTransform, drawOpacity(), layerRenderer()->tilerProgramSwizzleAA(), layerRenderer()->tilerProgramSwizzleAA()->fragmentShader().fragmentTexTransformLocation(), layerRenderer()->tilerProgramSwizzleAA()->fragmentShader().edgeLocation());
        else
            drawTiles(layerRenderer(), layerRect, m_tilingTransform, drawOpacity(), layerRenderer()->tilerProgramSwizzle(), -1, -1);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    if (isRootLayer()) {
        context->colorMask(true, true, true, true);
        GLC(context, context->enable(GraphicsContext3D::BLEND));
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
void CCTiledLayerImpl::drawTiles(LayerRendererChromium* layerRenderer, const IntRect& contentRect, const TransformationMatrix& globalTransform, float opacity, const T* program, int fragmentTexTransformLocation, int edgeLocation)
{
    TransformationMatrix matrix(layerRenderer->windowMatrix() * layerRenderer->projectionMatrix() * globalTransform);

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

    GraphicsContext3D* context = layerRenderer->context();
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
    m_tiler->contentRectToTileIndices(contentRect, left, top, right, bottom);
    IntRect layerRect = m_tiler->contentRectToLayerRect(contentRect);
    for (int j = top; j <= bottom; ++j) {
        Edge prevEdgeX = leftEdge;

        Edge edgeY = bottomEdge;
        if (j < (m_tiler->numTilesY() - 1)) {
            IntRect tileRect = unionRect(m_tiler->tileBounds(0, j), m_tiler->tileBounds(m_tiler->numTilesX() - 1, j));
            tileRect.intersect(layerRect);

            // Skip empty rows.
            if (tileRect.isEmpty())
                continue;

            tileRect = m_tiler->layerRectToContentRect(tileRect);

            FloatPoint p1(tileRect.maxX(), tileRect.maxY());
            FloatPoint p2(tileRect.x(), tileRect.maxY());

            // Map points to device space.
            p1 = matrix.mapPoint(p1);
            p2 = matrix.mapPoint(p2);

            // Compute horizontal edge.
            edgeY = computeEdge(p1, p2, sign);
        }

        for (int i = left; i <= right; ++i) {
            DrawableTile* tile = tileAt(i, j);
            if (!tile || !tile->textureId())
                continue;

            context->bindTexture(GraphicsContext3D::TEXTURE_2D, tile->textureId());

            // Don't use tileContentRect here, as that contains the full
            // rect with border texels which shouldn't be drawn.
            IntRect tileRect = m_tiler->tileBounds(tile->i(), tile->j());
            IntRect displayRect = tileRect;
            tileRect.intersect(layerRect);

            // Keep track of how the top left has moved, so the texture can be
            // offset the same amount.
            IntSize displayOffset = tileRect.minXMinYCorner() - displayRect.minXMinYCorner();

            // Skip empty tiles.
            if (tileRect.isEmpty())
                continue;

            tileRect = m_tiler->layerRectToContentRect(tileRect);

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

            FloatPoint texOffset = m_tiler->textureOffset(tile->i(), tile->j()) + clampOffset + FloatSize(displayOffset);
            float tileWidth = static_cast<float>(m_tiler->tileSize().width());
            float tileHeight = static_cast<float>(m_tiler->tileSize().height());

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
            if (i < (m_tiler->numTilesX() - 1)) {
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

            LayerChromium::drawTexturedQuad(context, layerRenderer->projectionMatrix(), globalTransform,
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

}

#endif // USE(ACCELERATED_COMPOSITING)
