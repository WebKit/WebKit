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

#include "cc/CCRenderSurface.h"

#include "GeometryBinding.h"
#include "GrTexture.h"
#include "GraphicsContext3D.h"
#include "LayerChromium.h"
#include "LayerRendererChromium.h"
#include "ManagedTexture.h"
#include "SharedGraphicsContext3D.h"
#include "TextStream.h"
#include "cc/CCDamageTracker.h"
#include "cc/CCLayerImpl.h"
#include "cc/CCProxy.h"
#include "cc/CCRenderSurfaceFilters.h"
#include "cc/CCSharedQuadState.h"
#include <wtf/text/CString.h>

namespace WebCore {

CCRenderSurface::CCRenderSurface(CCLayerImpl* owningLayer)
    : m_owningLayer(owningLayer)
    , m_maskLayer(0)
    , m_skipsDraw(false)
    , m_surfacePropertyChanged(false)
    , m_drawOpacity(1)
    , m_drawOpacityIsAnimating(false)
    , m_targetSurfaceTransformsAreAnimating(false)
    , m_screenSpaceTransformsAreAnimating(false)
    , m_nearestAncestorThatMovesPixels(0)
    , m_targetRenderSurfaceLayerIndexHistory(0)
    , m_currentLayerIndexHistory(0)
{
    m_damageTracker = CCDamageTracker::create();
}

CCRenderSurface::~CCRenderSurface()
{
}

FloatRect CCRenderSurface::drawableContentRect() const
{
    FloatRect localContentRect(-0.5 * m_contentRect.width(), -0.5 * m_contentRect.height(),
                               m_contentRect.width(), m_contentRect.height());
    FloatRect drawableContentRect = m_drawTransform.mapRect(localContentRect);
    if (hasReplica())
        drawableContentRect.unite(m_replicaDrawTransform.mapRect(localContentRect));

    return drawableContentRect;
}

bool CCRenderSurface::prepareContentsTexture(LayerRendererChromium* layerRenderer)
{
    IntSize requiredSize(m_contentRect.size());
    TextureManager* textureManager = layerRenderer->renderSurfaceTextureManager();

    if (!m_contentsTexture)
        m_contentsTexture = ManagedTexture::create(textureManager);

    if (m_contentsTexture->isReserved())
        return true;

    if (!m_contentsTexture->reserve(requiredSize, GraphicsContext3D::RGBA)) {
        m_skipsDraw = true;
        return false;
    }

    m_skipsDraw = false;
    return true;
}

void CCRenderSurface::releaseContentsTexture()
{
    if (m_skipsDraw || !m_contentsTexture)
        return;
    m_contentsTexture->unreserve();
}

bool CCRenderSurface::prepareBackgroundTexture(LayerRendererChromium* layerRenderer)
{
    IntSize requiredSize(m_contentRect.size());
    TextureManager* textureManager = layerRenderer->renderSurfaceTextureManager();

    if (!m_backgroundTexture)
        m_backgroundTexture = ManagedTexture::create(textureManager);

    if (m_backgroundTexture->isReserved())
        return true;

    if (!m_backgroundTexture->reserve(requiredSize, GraphicsContext3D::RGBA))
        return false;

    return true;
}

void CCRenderSurface::releaseBackgroundTexture()
{
    if (!m_backgroundTexture)
        return;
    m_backgroundTexture->unreserve();
}

TransformationMatrix CCRenderSurface::computeDeviceTransform(LayerRendererChromium* layerRenderer, const TransformationMatrix& drawTransform) const
{
    TransformationMatrix renderTransform = drawTransform;
    // Apply a scaling factor to size the quad from 1x1 to its intended size.
    renderTransform.scale3d(m_contentRect.width(), m_contentRect.height(), 1);
    TransformationMatrix deviceTransform = TransformationMatrix(layerRenderer->windowMatrix() * layerRenderer->projectionMatrix() * renderTransform).to2dTransform();
    return deviceTransform;
}

IntRect CCRenderSurface::computeDeviceBoundingBox(LayerRendererChromium* layerRenderer, const TransformationMatrix& drawTransform) const
{
    TransformationMatrix contentsDeviceTransform = computeDeviceTransform(layerRenderer, drawTransform);

    // Can only draw surface if device matrix is invertible.
    if (!contentsDeviceTransform.isInvertible())
        return IntRect();

    FloatQuad deviceQuad = contentsDeviceTransform.mapQuad(layerRenderer->sharedGeometryQuad());
    return enclosingIntRect(deviceQuad.boundingBox());
}

IntRect CCRenderSurface::computeReadbackDeviceBoundingBox(LayerRendererChromium* layerRenderer, const TransformationMatrix& drawTransform) const
{
    IntRect deviceRect = computeDeviceBoundingBox(layerRenderer, drawTransform);

    if (m_backgroundFilters.isEmpty())
        return deviceRect;

    int top, right, bottom, left;
    m_backgroundFilters.getOutsets(top, right, bottom, left);
    deviceRect.move(-left, -top);
    deviceRect.expand(left + right, top + bottom);

    return deviceRect;
}

IntRect CCRenderSurface::readbackDeviceContentRect(LayerRendererChromium* layerRenderer, const TransformationMatrix& drawTransform) const
{
    return computeReadbackDeviceBoundingBox(layerRenderer, drawTransform);
}

void CCRenderSurface::copyTextureToFramebuffer(LayerRendererChromium* layerRenderer, int textureId, const IntSize& bounds, const TransformationMatrix& drawMatrix)
{
    const LayerRendererChromium::RenderSurfaceProgram* program = layerRenderer->renderSurfaceProgram();

    GLC(layerRenderer->context(), layerRenderer->context()->activeTexture(GraphicsContext3D::TEXTURE0));
    GLC(layerRenderer->context(), layerRenderer->context()->bindTexture(GraphicsContext3D::TEXTURE_2D, textureId));
    GLC(layerRenderer->context(), layerRenderer->context()->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR));
    GLC(layerRenderer->context(), layerRenderer->context()->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR));

    GLC(layerRenderer->context(), layerRenderer->context()->useProgram(program->program()));
    GLC(layerRenderer->context(), layerRenderer->context()->uniform1i(program->fragmentShader().samplerLocation(), 0));
    layerRenderer->drawTexturedQuad(drawMatrix, bounds.width(), bounds.height(), 1, layerRenderer->sharedGeometryQuad(),
                                    program->vertexShader().matrixLocation(),
                                    program->fragmentShader().alphaLocation(),
                                    -1);
}

void CCRenderSurface::copyDeviceToBackgroundTexture(LayerRendererChromium* layerRenderer, int deviceBackgroundTextureId, const IntRect& deviceTextureRect, const TransformationMatrix& deviceTransform) const
{
    ASSERT(!m_backgroundFilters.isEmpty());

    TransformationMatrix deviceToSurfaceTransform;
    deviceToSurfaceTransform.translate(m_contentRect.width() / 2.0, m_contentRect.height() / 2.0);
    deviceToSurfaceTransform.scale3d(m_contentRect.width(), m_contentRect.height(), 1);
    deviceToSurfaceTransform *= deviceTransform.inverse();
    deviceToSurfaceTransform.translate(deviceTextureRect.width() / 2.0, deviceTextureRect.height() / 2.0);
    deviceToSurfaceTransform.translate(deviceTextureRect.x(), deviceTextureRect.y());

    copyTextureToFramebuffer(layerRenderer, deviceBackgroundTextureId, deviceTextureRect.size(), deviceToSurfaceTransform);
}

inline static int getSkBitmapTextureId(const SkBitmap& bitmap, int fallback)
{
    if (!bitmap.getTexture())
        return fallback;
    GrTexture* texture = reinterpret_cast<GrTexture*>(bitmap.getTexture());
    return texture->getTextureHandle();
}

void CCRenderSurface::setScissorRect(LayerRendererChromium* layerRenderer, const FloatRect& surfaceDamageRect) const
{
    if (m_owningLayer->parent() && m_owningLayer->parent()->usesLayerClipping() && layerRenderer->capabilities().usingPartialSwap) {
        FloatRect clipAndDamageRect = m_clipRect;
        clipAndDamageRect.intersect(surfaceDamageRect);
        layerRenderer->setScissorToRect(enclosingIntRect(clipAndDamageRect));
    } else if (layerRenderer->capabilities().usingPartialSwap)
        layerRenderer->setScissorToRect(enclosingIntRect(surfaceDamageRect));
    else if (m_owningLayer->parent() && m_owningLayer->parent()->usesLayerClipping())
        layerRenderer->setScissorToRect(m_clipRect);
    else
        GLC(layerRenderer->context(), layerRenderer->context()->disable(GraphicsContext3D::SCISSOR_TEST));
}

void CCRenderSurface::drawContents(LayerRendererChromium* layerRenderer)
{
    if (m_skipsDraw || !m_contentsTexture)
        return;

    // FIXME: Cache this value so that we don't have to do it for both the surface and its replica.
    // Apply filters to the contents texture.
    SkBitmap filterBitmap = applyFilters(layerRenderer, m_filters, m_contentsTexture.get());

    int contentsTextureId = getSkBitmapTextureId(filterBitmap, m_contentsTexture->textureId());
    drawLayer(layerRenderer, m_maskLayer, m_drawTransform, contentsTextureId);
}

void CCRenderSurface::drawReplica(LayerRendererChromium* layerRenderer)
{
    ASSERT(hasReplica());
    if (!hasReplica() || m_skipsDraw || !m_contentsTexture)
        return;

    // Apply filters to the contents texture.
    SkBitmap filterBitmap = applyFilters(layerRenderer, m_filters, m_contentsTexture.get());

    // FIXME: By using the same RenderSurface for both the content and its reflection,
    // it's currently not possible to apply a separate mask to the reflection layer
    // or correctly handle opacity in reflections (opacity must be applied after drawing
    // both the layer and its reflection). The solution is to introduce yet another RenderSurface
    // to draw the layer and its reflection in. For now we only apply a separate reflection
    // mask if the contents don't have a mask of their own.
    CCLayerImpl* replicaMaskLayer = m_maskLayer;
    if (!m_maskLayer && m_owningLayer->replicaLayer())
        replicaMaskLayer = m_owningLayer->replicaLayer()->maskLayer();

    int contentsTextureId = getSkBitmapTextureId(filterBitmap, m_contentsTexture->textureId());
    drawLayer(layerRenderer, replicaMaskLayer, m_replicaDrawTransform, contentsTextureId);
}

void CCRenderSurface::drawLayer(LayerRendererChromium* layerRenderer, CCLayerImpl* maskLayer, const TransformationMatrix& drawTransform, int contentsTextureId)
{
    TransformationMatrix deviceMatrix = computeDeviceTransform(layerRenderer, drawTransform);

    // Can only draw surface if device matrix is invertible.
    if (!deviceMatrix.isInvertible())
        return;

    // Draw the background texture if there is one.
    if (m_backgroundTexture && m_backgroundTexture->isReserved())
        copyTextureToFramebuffer(layerRenderer, m_backgroundTexture->textureId(), m_contentRect.size(), drawTransform);

    FloatQuad quad = deviceMatrix.mapQuad(layerRenderer->sharedGeometryQuad());
    CCLayerQuad deviceRect = CCLayerQuad(FloatQuad(quad.boundingBox()));
    CCLayerQuad layerQuad = CCLayerQuad(quad);

    // Use anti-aliasing programs only when necessary.
    bool useAA = (!quad.isRectilinear() || !quad.boundingBox().isExpressibleAsIntRect());

    if (useAA) {
        deviceRect.inflateAntiAliasingDistance();
        layerQuad.inflateAntiAliasingDistance();
    }

    bool useMask = false;
    if (maskLayer && maskLayer->drawsContent())
        if (!maskLayer->bounds().isEmpty())
            useMask = true;

    // FIXME: pass in backgroundTextureId and blend the background in with this draw instead of having a separate drawBackground() pass.

    if (useMask) {
        if (useAA) {
            const LayerRendererChromium::RenderSurfaceMaskProgramAA* program = layerRenderer->renderSurfaceMaskProgramAA();
            drawSurface(layerRenderer, maskLayer, drawTransform, deviceMatrix, deviceRect, layerQuad, contentsTextureId, program, program->fragmentShader().maskSamplerLocation(), program->vertexShader().pointLocation(), program->fragmentShader().edgeLocation());
        } else {
            const LayerRendererChromium::RenderSurfaceMaskProgram* program = layerRenderer->renderSurfaceMaskProgram();
            drawSurface(layerRenderer, maskLayer, drawTransform, deviceMatrix, deviceRect, layerQuad, contentsTextureId, program, program->fragmentShader().maskSamplerLocation(), -1, -1);
        }
    } else {
        if (useAA) {
            const LayerRendererChromium::RenderSurfaceProgramAA* program = layerRenderer->renderSurfaceProgramAA();
            drawSurface(layerRenderer, maskLayer, drawTransform, deviceMatrix, deviceRect, layerQuad, contentsTextureId, program, -1, program->vertexShader().pointLocation(), program->fragmentShader().edgeLocation());
        } else {
            const LayerRendererChromium::RenderSurfaceProgram* program = layerRenderer->renderSurfaceProgram();
            drawSurface(layerRenderer, maskLayer, drawTransform, deviceMatrix, deviceRect, layerQuad, contentsTextureId, program, -1, -1, -1);
        }
    }
}

template <class T>
void CCRenderSurface::drawSurface(LayerRendererChromium* layerRenderer, CCLayerImpl* maskLayer, const TransformationMatrix& drawTransform, const TransformationMatrix& deviceTransform, const CCLayerQuad& deviceRect, const CCLayerQuad& layerQuad, int contentsTextureId, const T* program, int shaderMaskSamplerLocation, int shaderQuadLocation, int shaderEdgeLocation)
{
    GraphicsContext3D* context3D = layerRenderer->context();

    context3D->makeContextCurrent();
    GLC(context3D, context3D->useProgram(program->program()));

    GLC(context3D, context3D->activeTexture(GraphicsContext3D::TEXTURE0));
    GLC(context3D, context3D->uniform1i(program->fragmentShader().samplerLocation(), 0));
    context3D->bindTexture(GraphicsContext3D::TEXTURE_2D, contentsTextureId);

    if (shaderMaskSamplerLocation != -1) {
        GLC(context3D, context3D->activeTexture(GraphicsContext3D::TEXTURE1));
        GLC(context3D, context3D->uniform1i(shaderMaskSamplerLocation, 1));
        maskLayer->bindContentsTexture(layerRenderer);
        GLC(context3D, context3D->activeTexture(GraphicsContext3D::TEXTURE0));
    }

    if (shaderEdgeLocation != -1) {
        float edge[24];
        layerQuad.toFloatArray(edge);
        deviceRect.toFloatArray(&edge[12]);
        GLC(context3D, context3D->uniform3fv(shaderEdgeLocation, 8, edge));
    }

    // Map device space quad to layer space.
    FloatQuad quad = deviceTransform.inverse().mapQuad(layerQuad.floatQuad());

    layerRenderer->drawTexturedQuad(drawTransform, m_contentRect.width(), m_contentRect.height(), m_drawOpacity, quad,
                                    program->vertexShader().matrixLocation(), program->fragmentShader().alphaLocation(), shaderQuadLocation);
}

SkBitmap CCRenderSurface::applyFilters(LayerRendererChromium* layerRenderer, const FilterOperations& filters, ManagedTexture* sourceTexture)
{
    if (filters.isEmpty())
        return SkBitmap();

    RefPtr<GraphicsContext3D> filterContext = CCProxy::hasImplThread() ? SharedGraphicsContext3D::getForImplThread() : SharedGraphicsContext3D::get();
    if (!filterContext)
        return SkBitmap();

    layerRenderer->context()->flush();

    return CCRenderSurfaceFilters::apply(filters, sourceTexture->textureId(), sourceTexture->size(), filterContext.get());
}

String CCRenderSurface::name() const
{
    return String::format("RenderSurface(id=%i,owner=%s)", m_owningLayer->id(), m_owningLayer->debugName().utf8().data());
}

static void writeIndent(TextStream& ts, int indent)
{
    for (int i = 0; i != indent; ++i)
        ts << "  ";
}

void CCRenderSurface::dumpSurface(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << name() << "\n";

    writeIndent(ts, indent+1);
    ts << "contentRect: (" << m_contentRect.x() << ", " << m_contentRect.y() << ", " << m_contentRect.width() << ", " << m_contentRect.height() << "\n";

    writeIndent(ts, indent+1);
    ts << "drawTransform: ";
    ts << m_drawTransform.m11() << ", " << m_drawTransform.m12() << ", " << m_drawTransform.m13() << ", " << m_drawTransform.m14() << "  //  ";
    ts << m_drawTransform.m21() << ", " << m_drawTransform.m22() << ", " << m_drawTransform.m23() << ", " << m_drawTransform.m24() << "  //  ";
    ts << m_drawTransform.m31() << ", " << m_drawTransform.m32() << ", " << m_drawTransform.m33() << ", " << m_drawTransform.m34() << "  //  ";
    ts << m_drawTransform.m41() << ", " << m_drawTransform.m42() << ", " << m_drawTransform.m43() << ", " << m_drawTransform.m44() << "\n";

    writeIndent(ts, indent+1);
    ts << "damageRect is pos(" << m_damageTracker->currentDamageRect().x() << "," << m_damageTracker->currentDamageRect().y() << "), ";
    ts << "size(" << m_damageTracker->currentDamageRect().width() << "," << m_damageTracker->currentDamageRect().height() << ")\n";
}

int CCRenderSurface::owningLayerId() const
{
    return m_owningLayer ? m_owningLayer->id() : 0;
}

bool CCRenderSurface::hasReplica() const
{
    return m_owningLayer->replicaLayer();
}

bool CCRenderSurface::hasMask() const
{
    return m_maskLayer;
}

bool CCRenderSurface::replicaHasMask() const
{
    return hasReplica() && (m_maskLayer || m_owningLayer->replicaLayer()->maskLayer());
}

void CCRenderSurface::setClipRect(const IntRect& clipRect)
{
    if (m_clipRect == clipRect)
        return;

    m_surfacePropertyChanged = true;
    m_clipRect = clipRect;
}

void CCRenderSurface::setContentRect(const IntRect& contentRect)
{
    if (m_contentRect == contentRect)
        return;

    m_surfacePropertyChanged = true;
    m_contentRect = contentRect;
}

bool CCRenderSurface::surfacePropertyChanged() const
{
    // Surface property changes are tracked as follows:
    //
    // - m_surfacePropertyChanged is flagged when the clipRect or contentRect change. As
    //   of now, these are the only two properties that can be affected by descendant layers.
    //
    // - all other property changes come from the owning layer (or some ancestor layer
    //   that propagates its change to the owning layer).
    //
    ASSERT(m_owningLayer);
    return m_surfacePropertyChanged || m_owningLayer->layerPropertyChanged();
}

bool CCRenderSurface::surfacePropertyChangedOnlyFromDescendant() const
{
    return m_surfacePropertyChanged && !m_owningLayer->layerPropertyChanged();
}

PassOwnPtr<CCSharedQuadState> CCRenderSurface::createSharedQuadState() const
{
    bool isOpaque = false;
    return CCSharedQuadState::create(originTransform(), drawTransform(), contentRect(), clipRect(), drawOpacity(), isOpaque);
}

PassOwnPtr<CCSharedQuadState> CCRenderSurface::createReplicaSharedQuadState() const
{
    bool isOpaque = false;
    return CCSharedQuadState::create(replicaOriginTransform(), replicaDrawTransform(), contentRect(), clipRect(), drawOpacity(), isOpaque);
}

}
#endif // USE(ACCELERATED_COMPOSITING)
