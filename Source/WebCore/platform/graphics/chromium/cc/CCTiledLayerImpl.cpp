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
#include "cc/CCLayerQuad.h"
#include <wtf/text/WTFString.h>

using namespace std;

namespace WebCore {

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
{
}

CCTiledLayerImpl::~CCTiledLayerImpl()
{
}

const char* CCTiledLayerImpl::layerTypeAsString() const
{
    return "TiledLayer";
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
    CCLayerImpl::dumpLayerProperties(ts, indent);
    writeIndent(ts, indent);
    ts << "skipsDraw: " << (!m_tiler || m_skipsDraw) << "\n";
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

    if (m_skipsDraw || !m_tiler || m_tiler->isEmpty() || layerRect.isEmpty() || !layerRenderer())
        return;

    TransformationMatrix deviceMatrix = TransformationMatrix(layerRenderer()->windowMatrix() * layerRenderer()->projectionMatrix() * m_tilingTransform).to2dTransform();

    // Don't draw any tiles when device matrix is not invertible.
    if (!deviceMatrix.isInvertible())
        return;

    FloatQuad quad = deviceMatrix.mapQuad(FloatQuad(layerRect));
    CCLayerQuad layerQuad = CCLayerQuad(quad);

#if defined(OS_CHROMEOS)
    // FIXME: Disable anti-aliasing to workaround broken driver.
    bool useAA = false;
#else
    // Use anti-aliasing programs only when necessary.
    bool useAA = (m_tiler->hasBorderTexels() && (!quad.isRectilinear() || !quad.boundingBox().isExpressibleAsIntRect()));
#endif

    if (useAA)
        layerQuad.inflateAntiAliasingDistance();

    GraphicsContext3D* context = layerRenderer()->context();
    if (isRootLayer()) {
        context->colorMask(true, true, true, false);
        GLC(context, context->disable(GraphicsContext3D::BLEND));
    }

    switch (m_sampledTexelFormat) {
    case LayerTextureUpdater::SampledTexelFormatRGBA:
        if (useAA) {
            const ProgramAA* program = layerRenderer()->tilerProgramAA();
            drawTiles(layerRenderer(), layerRect, m_tilingTransform, deviceMatrix, layerQuad, drawOpacity(), program, program->fragmentShader().fragmentTexTransformLocation(), program->fragmentShader().edgeLocation());
        } else {
            const Program* program = layerRenderer()->tilerProgram();
            drawTiles(layerRenderer(), layerRect, m_tilingTransform, deviceMatrix, layerQuad, drawOpacity(), program, -1, -1);
        }
        break;
    case LayerTextureUpdater::SampledTexelFormatBGRA:
        if (useAA) {
            const ProgramSwizzleAA* program = layerRenderer()->tilerProgramSwizzleAA();
            drawTiles(layerRenderer(), layerRect, m_tilingTransform, deviceMatrix, layerQuad, drawOpacity(), program, program->fragmentShader().fragmentTexTransformLocation(), program->fragmentShader().edgeLocation());
        } else {
            const ProgramSwizzle* program = layerRenderer()->tilerProgramSwizzle();
            drawTiles(layerRenderer(), layerRect, m_tilingTransform, deviceMatrix, layerQuad, drawOpacity(), program, -1, -1);
        }
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

template <class T>
void CCTiledLayerImpl::drawTiles(LayerRendererChromium* layerRenderer, const IntRect& contentRect, const TransformationMatrix& globalTransform, const TransformationMatrix& deviceTransform, const CCLayerQuad& contentQuad, float opacity, const T* program, int fragmentTexTransformLocation, int edgeLocation)
{
    GraphicsContext3D* context = layerRenderer->context();
    GLC(context, context->useProgram(program->program()));
    GLC(context, context->uniform1i(program->fragmentShader().samplerLocation(), 0));
    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE0));

    TransformationMatrix quadTransform = deviceTransform.inverse();

    if (edgeLocation != -1) {
        float edge[12];
        contentQuad.toFloatArray(edge);
        GLC(context, context->uniform3fv(edgeLocation, edge, 4));
    }

    CCLayerQuad::Edge prevEdgeY = contentQuad.top();

    int left, top, right, bottom;
    m_tiler->contentRectToTileIndices(contentRect, left, top, right, bottom);
    IntRect layerRect = m_tiler->contentRectToLayerRect(contentRect);
    float sign = FloatQuad(contentRect).isCounterclockwise() ? -1 : 1;
    for (int j = top; j <= bottom; ++j) {
        CCLayerQuad::Edge prevEdgeX = contentQuad.left();

        CCLayerQuad::Edge edgeY = contentQuad.bottom();
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
            p1 = deviceTransform.mapPoint(p1);
            p2 = deviceTransform.mapPoint(p2);

            // Compute horizontal edge.
            edgeY = CCLayerQuad::Edge(p1, p2);
            edgeY.scale(sign);
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

            CCLayerQuad::Edge edgeX = contentQuad.right();
            if (i < (m_tiler->numTilesX() - 1)) {
                FloatPoint p1(tileRect.maxX(), tileRect.y());
                FloatPoint p2(tileRect.maxX(), tileRect.maxY());

                // Map points to device space.
                p1 = deviceTransform.mapPoint(p1);
                p2 = deviceTransform.mapPoint(p2);

                // Compute vertical edge.
                edgeX = CCLayerQuad::Edge(p1, p2);
                edgeX.scale(sign);
            }

            // Create device space quad.
            CCLayerQuad deviceQuad(prevEdgeX, prevEdgeY, edgeX, edgeY);

            // Map quad to layer space.
            FloatQuad quad = quadTransform.mapQuad(deviceQuad.floatQuad());

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

            LayerChromium::drawTexturedQuad(context, layerRenderer->projectionMatrix(), globalTransform,
                                            tileRect.width(), tileRect.height(), opacity, quad,
                                            program->vertexShader().matrixLocation(),
                                            program->fragmentShader().alphaLocation(),
                                            program->vertexShader().pointLocation());

            prevEdgeX = edgeX;
            // Reverse direction.
            prevEdgeX.scale(-1);
        }

        prevEdgeY = edgeY;
        // Reverse direction.
        prevEdgeY.scale(-1);
    }
}

}

#endif // USE(ACCELERATED_COMPOSITING)
