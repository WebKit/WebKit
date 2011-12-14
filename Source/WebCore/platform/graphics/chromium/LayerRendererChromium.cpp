/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"

#if USE(ACCELERATED_COMPOSITING)
#include "LayerRendererChromium.h"

#include "Canvas2DLayerChromium.h"
#include "CanvasLayerTextureUpdater.h"
#include "Extensions3DChromium.h"
#include "FloatQuad.h"
#include "GeometryBinding.h"
#include "GraphicsContext3D.h"
#include "LayerChromium.h"
#include "LayerPainterChromium.h"
#include "ManagedTexture.h"
#include "NotImplemented.h"
#include "PlatformColor.h"
#include "RenderSurfaceChromium.h"
#include "TextStream.h"
#include "TextureManager.h"
#include "TraceEvent.h"
#include "TrackingTextureAllocator.h"
#include "TreeSynchronizer.h"
#include "WebGLLayerChromium.h"
#include "cc/CCCustomLayerDrawQuad.h"
#include "cc/CCDamageTracker.h"
#include "cc/CCDebugBorderDrawQuad.h"
#include "cc/CCRenderPass.h"
#include "cc/CCRenderSurfaceDrawQuad.h"
#include "cc/CCSolidColorDrawQuad.h"
#include "cc/CCTileDrawQuad.h"
#include "cc/CCLayerImpl.h"
#include "cc/CCLayerTreeHostCommon.h"
#include "cc/CCProxy.h"
#if USE(SKIA)
#include "Extensions3D.h"
#include "GrContext.h"
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#elif USE(CG)
#include <CoreGraphics/CGBitmapContext.h>
#endif
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>

using namespace std;

namespace WebCore {

namespace {

static TransformationMatrix orthoMatrix(float left, float right, float bottom, float top)
{
    float deltaX = right - left;
    float deltaY = top - bottom;
    TransformationMatrix ortho;
    if (!deltaX || !deltaY)
        return ortho;
    ortho.setM11(2.0f / deltaX);
    ortho.setM41(-(right + left) / deltaX);
    ortho.setM22(2.0f / deltaY);
    ortho.setM42(-(top + bottom) / deltaY);

    // Z component of vertices is always set to zero as we don't use the depth buffer
    // while drawing.
    ortho.setM33(0);

    return ortho;
}

static TransformationMatrix screenMatrix(int x, int y, int width, int height)
{
    TransformationMatrix screen;

    // Map to viewport.
    screen.translate3d(x, y, 0);
    screen.scale3d(width, height, 0);

    // Map x, y and z to unit square.
    screen.translate3d(0.5, 0.5, 0.5);
    screen.scale3d(0.5, 0.5, 0.5);

    return screen;
}

#if USE(SKIA)
bool contextSupportsAcceleratedPainting(GraphicsContext3D* context)
{
    WebCore::Extensions3D* extensions = context->getExtensions();
    if (extensions->supports("GL_EXT_texture_format_BGRA8888"))
        extensions->ensureEnabled("GL_EXT_texture_format_BGRA8888");
    else
        return false;

    if (extensions->supports("GL_EXT_read_format_bgra"))
        extensions->ensureEnabled("GL_EXT_read_format_bgra");
    else
        return false;

    if (!context->grContext())
        return false;

    return true;
}
#endif

} // anonymous namespace

class LayerRendererSwapBuffersCompleteCallbackAdapter : public Extensions3DChromium::SwapBuffersCompleteCallbackCHROMIUM {
public:
    static PassOwnPtr<LayerRendererSwapBuffersCompleteCallbackAdapter> create(LayerRendererChromium* layerRenderer)
    {
        return adoptPtr(new LayerRendererSwapBuffersCompleteCallbackAdapter(layerRenderer));
    }
    virtual ~LayerRendererSwapBuffersCompleteCallbackAdapter() { }

    virtual void onSwapBuffersComplete()
    {
        m_layerRenderer->onSwapBuffersComplete();
    }

private:
    explicit LayerRendererSwapBuffersCompleteCallbackAdapter(LayerRendererChromium* layerRenderer)
    {
        m_layerRenderer = layerRenderer;
    }

    LayerRendererChromium* m_layerRenderer;
};


PassOwnPtr<LayerRendererChromium> LayerRendererChromium::create(CCLayerTreeHostImpl* owner, PassRefPtr<GraphicsContext3D> context)
{
#if USE(SKIA)
    if (owner->settings().acceleratePainting && !contextSupportsAcceleratedPainting(context.get()))
        return nullptr;
#endif
    OwnPtr<LayerRendererChromium> layerRenderer(adoptPtr(new LayerRendererChromium(owner, context)));
    if (!layerRenderer->initialize())
        return nullptr;

    return layerRenderer.release();
}

LayerRendererChromium::LayerRendererChromium(CCLayerTreeHostImpl* owner,
                                             PassRefPtr<GraphicsContext3D> context)
    : m_owner(owner)
    , m_currentRenderSurface(0)
    , m_offscreenFramebufferId(0)
    , m_context(context)
    , m_defaultRenderSurface(0)
    , m_sharedGeometryQuad(FloatRect(-0.5f, -0.5f, 1.0f, 1.0f))
    , m_isViewportChanged(false)
{
}

bool LayerRendererChromium::initialize()
{
    if (!m_context->makeContextCurrent())
        return false;

    if (settings().acceleratePainting)
        m_capabilities.usingAcceleratedPainting = true;

    WebCore::Extensions3D* extensions = m_context->getExtensions();
    m_capabilities.contextHasCachedFrontBuffer = extensions->supports("GL_CHROMIUM_front_buffer_cached");
    if (m_capabilities.contextHasCachedFrontBuffer)
        extensions->ensureEnabled("GL_CHROMIUM_front_buffer_cached");

    m_capabilities.usingPartialSwap = settings().partialSwapEnabled && extensions->supports("GL_CHROMIUM_post_sub_buffer");
    if (m_capabilities.usingPartialSwap)
        extensions->ensureEnabled("GL_CHROMIUM_post_sub_buffer");

    m_capabilities.usingMapSub = extensions->supports("GL_CHROMIUM_map_sub");
    if (m_capabilities.usingMapSub)
        extensions->ensureEnabled("GL_CHROMIUM_map_sub");

    // Use the swapBuffers callback only with the threaded proxy.
    if (CCProxy::hasImplThread())
        m_capabilities.usingSwapCompleteCallback = extensions->supports("GL_CHROMIUM_swapbuffers_complete_callback");
    if (m_capabilities.usingSwapCompleteCallback) {
        extensions->ensureEnabled("GL_CHROMIUM_swapbuffers_complete_callback");
        Extensions3DChromium* extensions3DChromium = static_cast<Extensions3DChromium*>(extensions);
        extensions3DChromium->setSwapBuffersCompleteCallbackCHROMIUM(LayerRendererSwapBuffersCompleteCallbackAdapter::create(this));
    }

    if (extensions->supports("GL_EXT_texture_format_BGRA8888"))
        extensions->ensureEnabled("GL_EXT_texture_format_BGRA8888");

    m_capabilities.usingSetVisibility = extensions->supports("GL_CHROMIUM_set_visibility");
    if (m_capabilities.usingSetVisibility)
        extensions->ensureEnabled("GL_CHROMIUM_set_visibility");

    if (extensions->supports("GL_CHROMIUM_iosurface")) {
        ASSERT(extensions->supports("GL_ARB_texture_rectangle"));
        extensions->ensureEnabled("GL_ARB_texture_rectangle");
        extensions->ensureEnabled("GL_CHROMIUM_iosurface");
    }

    m_capabilities.usingTextureUsageHint = extensions->supports("GL_ANGLE_texture_usage");
    if (m_capabilities.usingTextureUsageHint)
        extensions->ensureEnabled("GL_ANGLE_texture_usage");

    m_capabilities.usingTextureStorageExtension = extensions->supports("GL_EXT_texture_storage");
    if (m_capabilities.usingTextureStorageExtension)
        extensions->ensureEnabled("GL_EXT_texture_storage");

    GLC(m_context.get(), m_context->getIntegerv(GraphicsContext3D::MAX_TEXTURE_SIZE, &m_capabilities.maxTextureSize));
    m_capabilities.bestTextureFormat = PlatformColor::bestTextureFormat(m_context.get());

    if (!initializeSharedObjects())
        return false;

    m_headsUpDisplay = CCHeadsUpDisplay::create(this);

    // Make sure the viewport and context gets initialized, even if it is to zero.
    viewportChanged();
    return true;
}

LayerRendererChromium::~LayerRendererChromium()
{
    ASSERT(CCProxy::isImplThread());
    Extensions3DChromium* extensions3DChromium = static_cast<Extensions3DChromium*>(m_context->getExtensions());
    extensions3DChromium->setSwapBuffersCompleteCallbackCHROMIUM(nullptr);
    m_headsUpDisplay.clear(); // Explicitly destroy the HUD before the TextureManager dies.
    cleanupSharedObjects();
}

void LayerRendererChromium::clearRenderSurfacesOnCCLayerImplRecursive(CCLayerImpl* layer)
{
    for (size_t i = 0; i < layer->children().size(); ++i)
        clearRenderSurfacesOnCCLayerImplRecursive(layer->children()[i].get());
    layer->clearRenderSurface();
}

void LayerRendererChromium::close()
{
    if (rootLayer())
        clearRenderSurfacesOnCCLayerImplRecursive(rootLayer());
}

GraphicsContext3D* LayerRendererChromium::context()
{
    return m_context.get();
}

void LayerRendererChromium::debugGLCall(GraphicsContext3D* context, const char* command, const char* file, int line)
{
    unsigned long error = context->getError();
    if (error != GraphicsContext3D::NO_ERROR)
        LOG_ERROR("GL command failed: File: %s\n\tLine %d\n\tcommand: %s, error %x\n", file, line, command, static_cast<int>(error));
}

void LayerRendererChromium::setVisible(bool visible)
{
    if (!visible)
        releaseRenderSurfaceTextures();
    if (m_capabilities.usingSetVisibility) {
        Extensions3DChromium* extensions3DChromium = static_cast<Extensions3DChromium*>(m_context->getExtensions());
        extensions3DChromium->setVisibilityCHROMIUM(visible);
    }
}

void LayerRendererChromium::releaseRenderSurfaceTextures()
{
    if (m_renderSurfaceTextureManager)
        m_renderSurfaceTextureManager->evictAndDeleteAllTextures(m_renderSurfaceTextureAllocator.get());
}

void LayerRendererChromium::viewportChanged()
{
    m_isViewportChanged = true;

    // Reset the current render surface to force an update of the viewport and
    // projection matrix next time useRenderSurface is called.
    m_currentRenderSurface = 0;
}

void LayerRendererChromium::clearSurfaceForDebug(CCRenderSurface* renderSurface, CCRenderSurface* rootRenderSurface, const FloatRect& surfaceDamageRect)
{
    // Non-root layers should clear their entire contents to transparent. The root layer
    // is cleared to blue to easily see regions that were not drawn on the screen. If we
    // are using partial swap / scissor optimization, then the surface should only
    // clear the damaged region, so that we don't accidentally clear un-changed portions
    // of the screen.

    if (renderSurface != rootRenderSurface)
        GLC(m_context.get(), m_context->clearColor(0, 0, 0, 0));
    else
        GLC(m_context.get(), m_context->clearColor(0, 0, 1, 1));

    if (m_capabilities.usingPartialSwap)
        setScissorToRect(enclosingIntRect(surfaceDamageRect));
    else
        GLC(m_context.get(), m_context->disable(GraphicsContext3D::SCISSOR_TEST));

    m_context->clear(GraphicsContext3D::COLOR_BUFFER_BIT);
    GLC(m_context.get(), m_context->enable(GraphicsContext3D::SCISSOR_TEST));
}

void LayerRendererChromium::beginDrawingFrame()
{
    m_defaultRenderSurface = rootLayer()->renderSurface();

    // FIXME: use the frame begin time from the overall compositor scheduler.
    // This value is currently inaccessible because it is up in Chromium's
    // RenderWidget.
    m_headsUpDisplay->onFrameBegin(currentTime());

    size_t contentsMemoryUseBytes = m_contentsTextureAllocator->currentMemoryUseBytes();
    size_t maxLimit = TextureManager::highLimitBytes(viewportSize());
    m_renderSurfaceTextureManager->setMaxMemoryLimitBytes(maxLimit - contentsMemoryUseBytes);

    if (viewportSize().isEmpty() || !rootLayer())
        return;

    TRACE_EVENT("LayerRendererChromium::drawLayers", this, 0);
    if (m_isViewportChanged) {
        // Only reshape when we know we are going to draw. Otherwise, the reshape
        // can leave the window at the wrong size if we never draw and the proper
        // viewport size is never set.
        m_isViewportChanged = false;
        m_context->reshape(viewportWidth(), viewportHeight());
    }

    makeContextCurrent();
    // The GL viewport covers the entire visible area, including the scrollbars.
    GLC(m_context.get(), m_context->viewport(0, 0, viewportWidth(), viewportHeight()));
    m_windowMatrix = screenMatrix(0, 0, viewportWidth(), viewportHeight());

    // Bind the common vertex attributes used for drawing all the layers.
    m_sharedGeometry->prepareForDraw();

    GLC(m_context.get(), m_context->disable(GraphicsContext3D::DEPTH_TEST));
    GLC(m_context.get(), m_context->disable(GraphicsContext3D::CULL_FACE));
    GLC(m_context.get(), m_context->colorMask(true, true, true, true));
    GLC(m_context.get(), m_context->enable(GraphicsContext3D::BLEND));
    GLC(m_context.get(), m_context->blendFunc(GraphicsContext3D::ONE, GraphicsContext3D::ONE_MINUS_SRC_ALPHA));
}

void LayerRendererChromium::drawRenderPass(const CCRenderPass* renderPass)
{
    CCRenderSurface* renderSurface = renderPass->targetSurface();
    if (!useRenderSurface(renderSurface))
        return;

    // FIXME: eventually we should place this under a debug flag.
    clearSurfaceForDebug(renderSurface, m_defaultRenderSurface, renderPass->surfaceDamageRect());

    const CCQuadList& quadList = renderPass->quadList();
    for (size_t i = 0; i < quadList.size(); ++i)
        drawQuad(quadList[i].get(), renderPass->surfaceDamageRect());
}

void LayerRendererChromium::drawQuad(const CCDrawQuad* quad, const FloatRect& surfaceDamageRect)
{
    IntRect scissorRect;
    if (m_capabilities.usingPartialSwap) {
        FloatRect clipAndDamageRect = surfaceDamageRect;
        if (!quad->clipRect().isEmpty())
            clipAndDamageRect.intersect(quad->clipRect());
        scissorRect = enclosingIntRect(clipAndDamageRect);
        if (scissorRect.isEmpty())
            return;
    } else
        scissorRect = quad->clipRect();

    if (scissorRect.isEmpty())
        GLC(m_context.get(), m_context->disable(GraphicsContext3D::SCISSOR_TEST));
    else
        setScissorToRect(scissorRect);

    if (quad->needsBlending())
        GLC(m_context.get(), m_context->enable(GraphicsContext3D::BLEND));
    else
        GLC(m_context.get(), m_context->disable(GraphicsContext3D::BLEND));

    switch (quad->material()) {
    case CCDrawQuad::Invalid:
        ASSERT_NOT_REACHED();
        break;
    case CCDrawQuad::DebugBorder:
        drawDebugBorderQuad(quad->toDebugBorderDrawQuad());
        break;
    case CCDrawQuad::RenderSurface:
        drawRenderSurfaceQuad(quad->toRenderSurfaceDrawQuad());
        break;
    case CCDrawQuad::SolidColor:
        drawSolidColorQuad(quad->toSolidColorDrawQuad());
        break;
    case CCDrawQuad::TiledContent:
        drawTileQuad(quad->toTileDrawQuad());
        break;
    case CCDrawQuad::CustomLayer:
        drawCustomLayerQuad(quad->toCustomLayerDrawQuad());
        break;
    }
}

void LayerRendererChromium::drawDebugBorderQuad(const CCDebugBorderDrawQuad* quad)
{
    static float glMatrix[16];
    const LayerChromium::BorderProgram* program = borderProgram();
    ASSERT(program && program->initialized());
    GLC(context(), context()->useProgram(program->program()));

    TransformationMatrix renderMatrix = quad->layerTransform();
    const IntRect& layerRect = quad->quadRect();
    renderMatrix.scaleNonUniform(layerRect.width(), layerRect.height());
    LayerRendererChromium::toGLMatrix(&glMatrix[0], projectionMatrix() * renderMatrix);
    GLC(context(), context()->uniformMatrix4fv(program->vertexShader().matrixLocation(), false, &glMatrix[0], 1));

    GLC(context(), context()->uniform4f(program->fragmentShader().colorLocation(), quad->color().red() / 255.0, quad->color().green() / 255.0, quad->color().blue() / 255.0, 1));

    GLC(context(), context()->lineWidth(quad->width()));

    // The indices for the line are stored in the same array as the triangle indices.
    GLC(context(), context()->drawElements(GraphicsContext3D::LINE_LOOP, 4, GraphicsContext3D::UNSIGNED_SHORT, 6 * sizeof(unsigned short)));
}

void LayerRendererChromium::drawRenderSurfaceQuad(const CCRenderSurfaceDrawQuad* quad)
{
    CCLayerImpl* layer = quad->layer();
    layer->renderSurface()->draw(this, quad->surfaceDamageRect());
    layer->renderSurface()->releaseContentsTexture();
}

void LayerRendererChromium::drawSolidColorQuad(const CCSolidColorDrawQuad* quad)
{
    const LayerChromium::BorderProgram* solidColorProgram = borderProgram();
    GLC(context(), context()->useProgram(solidColorProgram->program()));

    IntRect tileRect = quad->quadRect();

    TransformationMatrix tileTransform = quad->quadTransform();
    tileTransform.translate(tileRect.x() + tileRect.width() / 2.0, tileRect.y() + tileRect.height() / 2.0);

    const Color& color = quad->color();

    GLC(context(), context()->uniform4f(solidColorProgram->fragmentShader().colorLocation(), color.red(), color.green(), color.blue(), color.alpha()));

    float opacity = quad->opacity();
    drawTexturedQuad(tileTransform,
                     tileRect.width(), tileRect.height(), opacity, FloatQuad(),
                     solidColorProgram->vertexShader().matrixLocation(),
                     -1, -1);
}

struct TileProgramUniforms {
    unsigned program;
    unsigned samplerLocation;
    unsigned vertexTexTransformLocation;
    unsigned fragmentTexTransformLocation;
    unsigned edgeLocation;
    unsigned matrixLocation;
    unsigned alphaLocation;
    unsigned pointLocation;
};

template<class T>
static void tileUniformLocation(T program, TileProgramUniforms& uniforms)
{
    uniforms.program = program->program();
    uniforms.vertexTexTransformLocation = program->vertexShader().vertexTexTransformLocation();
    uniforms.matrixLocation = program->vertexShader().matrixLocation();
    uniforms.pointLocation = program->vertexShader().pointLocation();

    uniforms.samplerLocation = program->fragmentShader().samplerLocation();
    uniforms.alphaLocation = program->fragmentShader().alphaLocation();
    uniforms.fragmentTexTransformLocation = program->fragmentShader().fragmentTexTransformLocation();
    uniforms.edgeLocation = program->fragmentShader().edgeLocation();
}

static void findTileProgramUniforms(LayerRendererChromium* layerRenderer, const CCTileDrawQuad* quad, TileProgramUniforms& uniforms)
{
    if (quad->isAntialiased()) {
        if (quad->swizzleContents()) {
            const CCTiledLayerImpl::ProgramSwizzleAA* program = layerRenderer->tilerProgramSwizzleAA();
            tileUniformLocation(program, uniforms);
        } else {
            const CCTiledLayerImpl::ProgramAA* program = layerRenderer->tilerProgramAA();
            tileUniformLocation(program, uniforms);
        }
    } else {
        if (quad->needsBlending()) {
            if (quad->swizzleContents()) {
                const CCTiledLayerImpl::ProgramSwizzle* program = layerRenderer->tilerProgramSwizzle();
                tileUniformLocation(program, uniforms);
            } else {
                const CCTiledLayerImpl::Program* program = layerRenderer->tilerProgram();
                tileUniformLocation(program, uniforms);
            }
        } else {
            if (quad->swizzleContents()) {
                const CCTiledLayerImpl::ProgramSwizzleOpaque* program = layerRenderer->tilerProgramSwizzleOpaque();
                tileUniformLocation(program, uniforms);
            } else {
                const CCTiledLayerImpl::ProgramOpaque* program = layerRenderer->tilerProgramOpaque();
                tileUniformLocation(program, uniforms);
            }
        }
    }
}

void LayerRendererChromium::drawTileQuad(const CCTileDrawQuad* quad)
{
    const IntRect& tileRect = quad->quadRect();

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

    FloatPoint textureOffset = quad->textureOffset() + clampOffset;

    // Map clamping rectangle to unit square.
    float vertexTexTranslateX = -clampRect.x() / clampRect.width();
    float vertexTexTranslateY = -clampRect.y() / clampRect.height();
    float vertexTexScaleX = tileRect.width() / clampRect.width();
    float vertexTexScaleY = tileRect.height() / clampRect.height();

    // Map to normalized texture coordinates.
    const IntSize& textureSize = quad->textureSize();
    float fragmentTexTranslateX = textureOffset.x() / textureSize.width();
    float fragmentTexTranslateY = textureOffset.y() / textureSize.height();
    float fragmentTexScaleX = clampRect.width() / textureSize.width();
    float fragmentTexScaleY = clampRect.height() / textureSize.height();

    TileProgramUniforms uniforms;
    findTileProgramUniforms(this, quad, uniforms);

    GLC(context(), context()->useProgram(uniforms.program));
    GLC(context(), context()->uniform1i(uniforms.samplerLocation, 0));
    GLC(context(), context()->activeTexture(GraphicsContext3D::TEXTURE0));
    GLC(context(), context()->bindTexture(GraphicsContext3D::TEXTURE_2D, quad->textureId()));
    GLC(context(), context()->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, quad->textureFilter()));
    GLC(context(), context()->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, quad->textureFilter()));

    FloatQuad localQuad;
    if (quad->isAntialiased()) {
        TransformationMatrix deviceTransform = TransformationMatrix(windowMatrix() * projectionMatrix() * quad->quadTransform()).to2dTransform();
        if (!deviceTransform.isInvertible())
            return;

        FloatQuad deviceLayerQuad = deviceTransform.mapQuad(FloatQuad(quad->layerRect()));

        CCLayerQuad deviceLayerBounds = CCLayerQuad(FloatQuad(deviceLayerQuad.boundingBox()));
        deviceLayerBounds.inflateAntiAliasingDistance();

        CCLayerQuad deviceLayerEdges = CCLayerQuad(deviceLayerQuad);
        deviceLayerEdges.inflateAntiAliasingDistance();

        float edge[24];
        deviceLayerEdges.toFloatArray(edge);
        deviceLayerBounds.toFloatArray(&edge[12]);
        GLC(context(), context()->uniform3fv(uniforms.edgeLocation, edge, 8));

        GLC(context(), context()->uniform4f(uniforms.vertexTexTransformLocation, vertexTexTranslateX, vertexTexTranslateY, vertexTexScaleX, vertexTexScaleY));
        GLC(context(), context()->uniform4f(uniforms.fragmentTexTransformLocation, fragmentTexTranslateX, fragmentTexTranslateY, fragmentTexScaleX, fragmentTexScaleY));

        FloatPoint bottomRight(tileRect.maxX(), tileRect.maxY());
        FloatPoint bottomLeft(tileRect.x(), tileRect.maxY());
        FloatPoint topLeft(tileRect.x(), tileRect.y());
        FloatPoint topRight(tileRect.maxX(), tileRect.y());

        // Map points to device space.
        bottomRight = deviceTransform.mapPoint(bottomRight);
        bottomLeft = deviceTransform.mapPoint(bottomLeft);
        topLeft = deviceTransform.mapPoint(topLeft);
        topRight = deviceTransform.mapPoint(topRight);

        CCLayerQuad::Edge bottomEdge(bottomRight, bottomLeft);
        CCLayerQuad::Edge leftEdge(bottomLeft, topLeft);
        CCLayerQuad::Edge topEdge(topLeft, topRight);
        CCLayerQuad::Edge rightEdge(topRight, bottomRight);

        if (quad->topEdgeAA())
            topEdge = deviceLayerEdges.top();
        if (quad->leftEdgeAA())
            leftEdge = deviceLayerEdges.left();
        if (quad->rightEdgeAA())
            rightEdge = deviceLayerEdges.right();
        if (quad->bottomEdgeAA())
            bottomEdge = deviceLayerEdges.bottom();

        float sign = FloatQuad(tileRect).isCounterclockwise() ? -1 : 1;
        bottomEdge.scale(sign);
        leftEdge.scale(sign);
        topEdge.scale(sign);
        rightEdge.scale(sign);

        // Create device space quad.
        CCLayerQuad deviceQuad(leftEdge, topEdge, rightEdge, bottomEdge);

        // Map quad to layer space.
        TransformationMatrix inverseDeviceTransform = deviceTransform.inverse();
        localQuad = inverseDeviceTransform.mapQuad(deviceQuad.floatQuad());
    } else {
        // Move fragment shader transform to vertex shader. We can do this while
        // still producing correct results as fragmentTexTransformLocation
        // should always be non-negative when tiles are transformed in a way
        // that could result in sampling outside the layer.
        vertexTexScaleX *= fragmentTexScaleX;
        vertexTexScaleY *= fragmentTexScaleY;
        vertexTexTranslateX *= fragmentTexScaleX;
        vertexTexTranslateY *= fragmentTexScaleY;
        vertexTexTranslateX += fragmentTexTranslateX;
        vertexTexTranslateY += fragmentTexTranslateY;

        GLC(context(), context()->uniform4f(uniforms.vertexTexTransformLocation, vertexTexTranslateX, vertexTexTranslateY, vertexTexScaleX, vertexTexScaleY));

        localQuad = FloatRect(tileRect);
    }

    // Normalize to tileRect.
    localQuad.scale(1.0f / tileRect.width(), 1.0f / tileRect.height());

    drawTexturedQuad(quad->quadTransform(), tileRect.width(), tileRect.height(), quad->opacity(), localQuad, uniforms.matrixLocation, uniforms.alphaLocation, uniforms.pointLocation);
}

void LayerRendererChromium::drawCustomLayerQuad(const CCCustomLayerDrawQuad* quad)
{
    CCLayerImpl* layer = quad->layer();
    layer->draw(this);
}

void LayerRendererChromium::finishDrawingFrame()
{
    if (m_headsUpDisplay->enabled()) {
        GLC(m_context.get(), m_context->enable(GraphicsContext3D::BLEND));
        GLC(m_context.get(), m_context->blendFunc(GraphicsContext3D::ONE, GraphicsContext3D::ONE_MINUS_SRC_ALPHA));
        GLC(m_context.get(), m_context->disable(GraphicsContext3D::SCISSOR_TEST));
        useRenderSurface(m_defaultRenderSurface);
        m_headsUpDisplay->draw();
    }

    GLC(m_context.get(), m_context->disable(GraphicsContext3D::SCISSOR_TEST));
    GLC(m_context.get(), m_context->disable(GraphicsContext3D::BLEND));

    size_t contentsMemoryUseBytes = m_contentsTextureAllocator->currentMemoryUseBytes();
    size_t reclaimLimit = TextureManager::reclaimLimitBytes(viewportSize());
    if (reclaimLimit > contentsMemoryUseBytes)
        m_renderSurfaceTextureManager->setPreferredMemoryLimitBytes(reclaimLimit - contentsMemoryUseBytes);
    else
        m_renderSurfaceTextureManager->setPreferredMemoryLimitBytes(0);

    m_renderSurfaceTextureManager->deleteEvictedTextures(m_renderSurfaceTextureAllocator.get());

    if (settings().compositeOffscreen)
        copyOffscreenTextureToDisplay();
}

void LayerRendererChromium::toGLMatrix(float* flattened, const TransformationMatrix& m)
{
    flattened[0] = m.m11();
    flattened[1] = m.m12();
    flattened[2] = m.m13();
    flattened[3] = m.m14();
    flattened[4] = m.m21();
    flattened[5] = m.m22();
    flattened[6] = m.m23();
    flattened[7] = m.m24();
    flattened[8] = m.m31();
    flattened[9] = m.m32();
    flattened[10] = m.m33();
    flattened[11] = m.m34();
    flattened[12] = m.m41();
    flattened[13] = m.m42();
    flattened[14] = m.m43();
    flattened[15] = m.m44();
}

void LayerRendererChromium::drawTexturedQuad(const TransformationMatrix& drawMatrix,
                                             float width, float height, float opacity, const FloatQuad& quad,
                                             int matrixLocation, int alphaLocation, int quadLocation)
{
    static float glMatrix[16];

    TransformationMatrix renderMatrix = drawMatrix;

    // Apply a scaling factor to size the quad from 1x1 to its intended size.
    renderMatrix.scale3d(width, height, 1);

    // Apply the projection matrix before sending the transform over to the shader.
    toGLMatrix(&glMatrix[0], m_projectionMatrix * renderMatrix);

    GLC(m_context, m_context->uniformMatrix4fv(matrixLocation, false, &glMatrix[0], 1));

    if (quadLocation != -1) {
        float point[8];
        point[0] = quad.p1().x();
        point[1] = quad.p1().y();
        point[2] = quad.p2().x();
        point[3] = quad.p2().y();
        point[4] = quad.p3().x();
        point[5] = quad.p3().y();
        point[6] = quad.p4().x();
        point[7] = quad.p4().y();
        GLC(m_context, m_context->uniform2fv(quadLocation, point, 4));
    }

    if (alphaLocation != -1)
        GLC(m_context, m_context->uniform1f(alphaLocation, opacity));

    GLC(m_context, m_context->drawElements(GraphicsContext3D::TRIANGLES, 6, GraphicsContext3D::UNSIGNED_SHORT, 0));
}

void LayerRendererChromium::finish()
{
    TRACE_EVENT("LayerRendererChromium::finish", this, 0);
    m_context->finish();
}

void LayerRendererChromium::swapBuffers()
{
    TRACE_EVENT("LayerRendererChromium::swapBuffers", this, 0);
    // We're done! Time to swapbuffers!

    if (m_capabilities.usingPartialSwap) {
        // If supported, we can save significant bandwidth by only swapping the damaged/scissored region (clamped to the viewport)
        IntRect subBuffer = enclosingIntRect(m_rootDamageRect);
        subBuffer.intersect(IntRect(IntPoint::zero(), viewportSize()));
        Extensions3DChromium* extensions3DChromium = static_cast<Extensions3DChromium*>(m_context->getExtensions());
        int flippedYPosOfRectBottom = viewportHeight() - subBuffer.y() - subBuffer.height();
        extensions3DChromium->postSubBufferCHROMIUM(subBuffer.x(), flippedYPosOfRectBottom, subBuffer.width(), subBuffer.height());
    } else
        // Note that currently this has the same effect as swapBuffers; we should
        // consider exposing a different entry point on GraphicsContext3D.
        m_context->prepareTexture();

    m_headsUpDisplay->onSwapBuffers();
}

void LayerRendererChromium::onSwapBuffersComplete()
{
    m_owner->onSwapBuffersComplete();
}

void LayerRendererChromium::getFramebufferPixels(void *pixels, const IntRect& rect)
{
    ASSERT(rect.maxX() <= viewportWidth() && rect.maxY() <= viewportHeight());

    if (!pixels)
        return;

    makeContextCurrent();

    GLC(m_context.get(), m_context->readPixels(rect.x(), rect.y(), rect.width(), rect.height(),
                                         GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, pixels));
}

ManagedTexture* LayerRendererChromium::getOffscreenLayerTexture()
{
    return settings().compositeOffscreen && rootLayer() ? rootLayer()->renderSurface()->contentsTexture() : 0;
}

void LayerRendererChromium::copyOffscreenTextureToDisplay()
{
    if (settings().compositeOffscreen) {
        makeContextCurrent();

        useRenderSurface(0);
        TransformationMatrix drawTransform;
        drawTransform.translate3d(0.5 * m_defaultRenderSurface->contentRect().width(), 0.5 * m_defaultRenderSurface->contentRect().height(), 0);
        m_defaultRenderSurface->setDrawTransform(drawTransform);
        m_defaultRenderSurface->setDrawOpacity(1);
        m_defaultRenderSurface->draw(this, m_defaultRenderSurface->contentRect());
    }
}

bool LayerRendererChromium::useRenderSurface(CCRenderSurface* renderSurface)
{
    if (m_currentRenderSurface == renderSurface)
        return true;

    m_currentRenderSurface = renderSurface;

    if ((renderSurface == m_defaultRenderSurface && !settings().compositeOffscreen) || (!renderSurface && settings().compositeOffscreen)) {
        GLC(m_context.get(), m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, 0));
        if (renderSurface)
            setDrawViewportRect(renderSurface->contentRect(), true);
        else
            setDrawViewportRect(m_defaultRenderSurface->contentRect(), true);
        return true;
    }

    GLC(m_context.get(), m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_offscreenFramebufferId));

    if (!renderSurface->prepareContentsTexture(this))
        return false;

    renderSurface->contentsTexture()->framebufferTexture2D(m_context.get(), m_renderSurfaceTextureAllocator.get());

#if !defined ( NDEBUG )
    if (m_context->checkFramebufferStatus(GraphicsContext3D::FRAMEBUFFER) != GraphicsContext3D::FRAMEBUFFER_COMPLETE) {
        ASSERT_NOT_REACHED();
        return false;
    }
#endif

    setDrawViewportRect(renderSurface->contentRect(), false);

    return true;
}

// Sets the scissor region to the given rectangle. The coordinate system for the
// scissorRect has its origin at the top left corner of the current visible rect.
void LayerRendererChromium::setScissorToRect(const IntRect& scissorRect)
{
    IntRect contentRect = (m_currentRenderSurface ? m_currentRenderSurface->contentRect() : m_defaultRenderSurface->contentRect());

    GLC(m_context.get(), m_context->enable(GraphicsContext3D::SCISSOR_TEST));

    // The scissor coordinates must be supplied in viewport space so we need to offset
    // by the relative position of the top left corner of the current render surface.
    int scissorX = scissorRect.x() - contentRect.x();
    // When rendering to the default render surface we're rendering upside down so the top
    // of the GL scissor is the bottom of our layer.
    // But, if rendering to offscreen texture, we reverse our sense of 'upside down'.
    int scissorY;
    if (m_currentRenderSurface == m_defaultRenderSurface && !settings().compositeOffscreen)
        scissorY = m_currentRenderSurface->contentRect().height() - (scissorRect.maxY() - m_currentRenderSurface->contentRect().y());
    else
        scissorY = scissorRect.y() - contentRect.y();
    GLC(m_context.get(), m_context->scissor(scissorX, scissorY, scissorRect.width(), scissorRect.height()));
}

bool LayerRendererChromium::makeContextCurrent()
{
    return m_context->makeContextCurrent();
}

// Sets the coordinate range of content that ends being drawn onto the target render surface.
// The target render surface is assumed to have an origin at 0, 0 and the width and height of
// of the drawRect.
void LayerRendererChromium::setDrawViewportRect(const IntRect& drawRect, bool flipY)
{
    if (flipY)
        m_projectionMatrix = orthoMatrix(drawRect.x(), drawRect.maxX(), drawRect.maxY(), drawRect.y());
    else
        m_projectionMatrix = orthoMatrix(drawRect.x(), drawRect.maxX(), drawRect.y(), drawRect.maxY());
    GLC(m_context.get(), m_context->viewport(0, 0, drawRect.width(), drawRect.height()));
    m_windowMatrix = screenMatrix(0, 0, drawRect.width(), drawRect.height());
}


bool LayerRendererChromium::initializeSharedObjects()
{
    TRACE_EVENT("LayerRendererChromium::initializeSharedObjects", this, 0);
    makeContextCurrent();

    // Create an FBO for doing offscreen rendering.
    GLC(m_context.get(), m_offscreenFramebufferId = m_context->createFramebuffer());

    // We will always need these programs to render, so create the programs eagerly so that the shader compilation can
    // start while we do other work. Other programs are created lazily on first access.
    m_sharedGeometry = adoptPtr(new GeometryBinding(m_context.get()));
    m_renderSurfaceProgram = adoptPtr(new CCRenderSurface::Program(m_context.get()));
    m_tilerProgram = adoptPtr(new CCTiledLayerImpl::Program(m_context.get()));
    m_tilerProgramOpaque = adoptPtr(new CCTiledLayerImpl::ProgramOpaque(m_context.get()));

    GLC(m_context.get(), m_context->flush());

    m_renderSurfaceTextureManager = TextureManager::create(TextureManager::highLimitBytes(viewportSize()),
                                                           TextureManager::reclaimLimitBytes(viewportSize()),
                                                           m_capabilities.maxTextureSize);
    m_contentsTextureAllocator = TrackingTextureAllocator::create(m_context.get());
    m_renderSurfaceTextureAllocator = TrackingTextureAllocator::create(m_context.get());
    if (m_capabilities.usingTextureUsageHint)
        m_renderSurfaceTextureAllocator->setTextureUsageHint(TrackingTextureAllocator::FramebufferAttachment);
    if (m_capabilities.usingTextureStorageExtension) {
        m_contentsTextureAllocator->setUseTextureStorageExt(true);
        m_renderSurfaceTextureAllocator->setUseTextureStorageExt(true);
    }

    return true;
}

const LayerChromium::BorderProgram* LayerRendererChromium::borderProgram()
{
    if (!m_borderProgram)
        m_borderProgram = adoptPtr(new LayerChromium::BorderProgram(m_context.get()));
    if (!m_borderProgram->initialized()) {
        TRACE_EVENT("LayerRendererChromium::borderProgram::initialize", this, 0);
        m_borderProgram->initialize(m_context.get());
    }
    return m_borderProgram.get();
}

const CCHeadsUpDisplay::Program* LayerRendererChromium::headsUpDisplayProgram()
{
    if (!m_headsUpDisplayProgram)
        m_headsUpDisplayProgram = adoptPtr(new CCHeadsUpDisplay::Program(m_context.get()));
    if (!m_headsUpDisplayProgram->initialized()) {
        TRACE_EVENT("LayerRendererChromium::headsUpDisplayProgram::initialize", this, 0);
        m_headsUpDisplayProgram->initialize(m_context.get());
    }
    return m_headsUpDisplayProgram.get();
}

const CCRenderSurface::Program* LayerRendererChromium::renderSurfaceProgram()
{
    ASSERT(m_renderSurfaceProgram);
    if (!m_renderSurfaceProgram->initialized()) {
        TRACE_EVENT("LayerRendererChromium::renderSurfaceProgram::initialize", this, 0);
        m_renderSurfaceProgram->initialize(m_context.get());
    }
    return m_renderSurfaceProgram.get();
}

const CCRenderSurface::ProgramAA* LayerRendererChromium::renderSurfaceProgramAA()
{
    if (!m_renderSurfaceProgramAA)
        m_renderSurfaceProgramAA = adoptPtr(new CCRenderSurface::ProgramAA(m_context.get()));
    if (!m_renderSurfaceProgramAA->initialized()) {
        TRACE_EVENT("LayerRendererChromium::renderSurfaceProgramAA::initialize", this, 0);
        m_renderSurfaceProgramAA->initialize(m_context.get());
    }
    return m_renderSurfaceProgramAA.get();
}

const CCRenderSurface::MaskProgram* LayerRendererChromium::renderSurfaceMaskProgram()
{
    if (!m_renderSurfaceMaskProgram)
        m_renderSurfaceMaskProgram = adoptPtr(new CCRenderSurface::MaskProgram(m_context.get()));
    if (!m_renderSurfaceMaskProgram->initialized()) {
        TRACE_EVENT("LayerRendererChromium::renderSurfaceMaskProgram::initialize", this, 0);
        m_renderSurfaceMaskProgram->initialize(m_context.get());
    }
    return m_renderSurfaceMaskProgram.get();
}

const CCRenderSurface::MaskProgramAA* LayerRendererChromium::renderSurfaceMaskProgramAA()
{
    if (!m_renderSurfaceMaskProgramAA)
        m_renderSurfaceMaskProgramAA = adoptPtr(new CCRenderSurface::MaskProgramAA(m_context.get()));
    if (!m_renderSurfaceMaskProgramAA->initialized()) {
        TRACE_EVENT("LayerRendererChromium::renderSurfaceMaskProgramAA::initialize", this, 0);
        m_renderSurfaceMaskProgramAA->initialize(m_context.get());
    }
    return m_renderSurfaceMaskProgramAA.get();
}

const CCTiledLayerImpl::Program* LayerRendererChromium::tilerProgram()
{
    ASSERT(m_tilerProgram);
    if (!m_tilerProgram->initialized()) {
        TRACE_EVENT("LayerRendererChromium::tilerProgram::initialize", this, 0);
        m_tilerProgram->initialize(m_context.get());
    }
    return m_tilerProgram.get();
}

const CCTiledLayerImpl::ProgramOpaque* LayerRendererChromium::tilerProgramOpaque()
{
    ASSERT(m_tilerProgramOpaque);
    if (!m_tilerProgramOpaque->initialized()) {
        TRACE_EVENT("LayerRendererChromium::tilerProgramOpaque::initialize", this, 0);
        m_tilerProgramOpaque->initialize(m_context.get());
    }
    return m_tilerProgramOpaque.get();
}

const CCTiledLayerImpl::ProgramAA* LayerRendererChromium::tilerProgramAA()
{
    if (!m_tilerProgramAA)
        m_tilerProgramAA = adoptPtr(new CCTiledLayerImpl::ProgramAA(m_context.get()));
    if (!m_tilerProgramAA->initialized()) {
        TRACE_EVENT("LayerRendererChromium::tilerProgramAA::initialize", this, 0);
        m_tilerProgramAA->initialize(m_context.get());
    }
    return m_tilerProgramAA.get();
}

const CCTiledLayerImpl::ProgramSwizzle* LayerRendererChromium::tilerProgramSwizzle()
{
    if (!m_tilerProgramSwizzle)
        m_tilerProgramSwizzle = adoptPtr(new CCTiledLayerImpl::ProgramSwizzle(m_context.get()));
    if (!m_tilerProgramSwizzle->initialized()) {
        TRACE_EVENT("LayerRendererChromium::tilerProgramSwizzle::initialize", this, 0);
        m_tilerProgramSwizzle->initialize(m_context.get());
    }
    return m_tilerProgramSwizzle.get();
}

const CCTiledLayerImpl::ProgramSwizzleOpaque* LayerRendererChromium::tilerProgramSwizzleOpaque()
{
    if (!m_tilerProgramSwizzleOpaque)
        m_tilerProgramSwizzleOpaque = adoptPtr(new CCTiledLayerImpl::ProgramSwizzleOpaque(m_context.get()));
    if (!m_tilerProgramSwizzleOpaque->initialized()) {
        TRACE_EVENT("LayerRendererChromium::tilerProgramSwizzleOpaque::initialize", this, 0);
        m_tilerProgramSwizzleOpaque->initialize(m_context.get());
    }
    return m_tilerProgramSwizzleOpaque.get();
}

const CCTiledLayerImpl::ProgramSwizzleAA* LayerRendererChromium::tilerProgramSwizzleAA()
{
    if (!m_tilerProgramSwizzleAA)
        m_tilerProgramSwizzleAA = adoptPtr(new CCTiledLayerImpl::ProgramSwizzleAA(m_context.get()));
    if (!m_tilerProgramSwizzleAA->initialized()) {
        TRACE_EVENT("LayerRendererChromium::tilerProgramSwizzleAA::initialize", this, 0);
        m_tilerProgramSwizzleAA->initialize(m_context.get());
    }
    return m_tilerProgramSwizzleAA.get();
}

const CCCanvasLayerImpl::Program* LayerRendererChromium::canvasLayerProgram()
{
    if (!m_canvasLayerProgram)
        m_canvasLayerProgram = adoptPtr(new CCCanvasLayerImpl::Program(m_context.get()));
    if (!m_canvasLayerProgram->initialized()) {
        TRACE_EVENT("LayerRendererChromium::canvasLayerProgram::initialize", this, 0);
        m_canvasLayerProgram->initialize(m_context.get());
    }
    return m_canvasLayerProgram.get();
}

const CCPluginLayerImpl::Program* LayerRendererChromium::pluginLayerProgram()
{
    if (!m_pluginLayerProgram)
        m_pluginLayerProgram = adoptPtr(new CCPluginLayerImpl::Program(m_context.get()));
    if (!m_pluginLayerProgram->initialized()) {
        TRACE_EVENT("LayerRendererChromium::pluginLayerProgram::initialize", this, 0);
        m_pluginLayerProgram->initialize(m_context.get());
    }
    return m_pluginLayerProgram.get();
}

const CCPluginLayerImpl::ProgramFlip* LayerRendererChromium::pluginLayerProgramFlip()
{
    if (!m_pluginLayerProgramFlip)
        m_pluginLayerProgramFlip = adoptPtr(new CCPluginLayerImpl::ProgramFlip(m_context.get()));
    if (!m_pluginLayerProgramFlip->initialized()) {
        TRACE_EVENT("LayerRendererChromium::pluginLayerProgramFlip::initialize", this, 0);
        m_pluginLayerProgramFlip->initialize(m_context.get());
    }
    return m_pluginLayerProgramFlip.get();
}

const CCPluginLayerImpl::TexRectProgram* LayerRendererChromium::pluginLayerTexRectProgram()
{
    if (!m_pluginLayerTexRectProgram)
        m_pluginLayerTexRectProgram = adoptPtr(new CCPluginLayerImpl::TexRectProgram(m_context.get()));
    if (!m_pluginLayerTexRectProgram->initialized()) {
        TRACE_EVENT("LayerRendererChromium::pluginLayerTexRectProgram::initialize", this, 0);
        m_pluginLayerTexRectProgram->initialize(m_context.get());
    }
    return m_pluginLayerTexRectProgram.get();
}

const CCPluginLayerImpl::TexRectProgramFlip* LayerRendererChromium::pluginLayerTexRectProgramFlip()
{
    if (!m_pluginLayerTexRectProgramFlip)
        m_pluginLayerTexRectProgramFlip = adoptPtr(new CCPluginLayerImpl::TexRectProgramFlip(m_context.get()));
    if (!m_pluginLayerTexRectProgramFlip->initialized()) {
        TRACE_EVENT("LayerRendererChromium::pluginLayerTexRectProgramFlip::initialize", this, 0);
        m_pluginLayerTexRectProgramFlip->initialize(m_context.get());
    }
    return m_pluginLayerTexRectProgramFlip.get();
}

const CCVideoLayerImpl::RGBAProgram* LayerRendererChromium::videoLayerRGBAProgram()
{
    if (!m_videoLayerRGBAProgram)
        m_videoLayerRGBAProgram = adoptPtr(new CCVideoLayerImpl::RGBAProgram(m_context.get()));
    if (!m_videoLayerRGBAProgram->initialized()) {
        TRACE_EVENT("LayerRendererChromium::videoLayerRGBAProgram::initialize", this, 0);
        m_videoLayerRGBAProgram->initialize(m_context.get());
    }
    return m_videoLayerRGBAProgram.get();
}

const CCVideoLayerImpl::YUVProgram* LayerRendererChromium::videoLayerYUVProgram()
{
    if (!m_videoLayerYUVProgram)
        m_videoLayerYUVProgram = adoptPtr(new CCVideoLayerImpl::YUVProgram(m_context.get()));
    if (!m_videoLayerYUVProgram->initialized()) {
        TRACE_EVENT("LayerRendererChromium::videoLayerYUVProgram::initialize", this, 0);
        m_videoLayerYUVProgram->initialize(m_context.get());
    }
    return m_videoLayerYUVProgram.get();
}

const CCVideoLayerImpl::NativeTextureProgram* LayerRendererChromium::videoLayerNativeTextureProgram()
{
    if (!m_videoLayerNativeTextureProgram)
        m_videoLayerNativeTextureProgram = adoptPtr(new CCVideoLayerImpl::NativeTextureProgram(m_context.get()));
    if (!m_videoLayerNativeTextureProgram->initialized()) {
        TRACE_EVENT("LayerRendererChromium::videoLayerNativeTextureProgram::initialize", this, 0);
        m_videoLayerNativeTextureProgram->initialize(m_context.get());
    }
    return m_videoLayerNativeTextureProgram.get();
}


void LayerRendererChromium::cleanupSharedObjects()
{
    makeContextCurrent();

    m_sharedGeometry.clear();

    if (m_borderProgram)
        m_borderProgram->cleanup(m_context.get());
    if (m_headsUpDisplayProgram)
        m_headsUpDisplayProgram->cleanup(m_context.get());
    if (m_tilerProgram)
        m_tilerProgram->cleanup(m_context.get());
    if (m_tilerProgramOpaque)
        m_tilerProgramOpaque->cleanup(m_context.get());
    if (m_tilerProgramAA)
        m_tilerProgramAA->cleanup(m_context.get());
    if (m_tilerProgramSwizzle)
        m_tilerProgramSwizzle->cleanup(m_context.get());
    if (m_tilerProgramSwizzleOpaque)
        m_tilerProgramSwizzleOpaque->cleanup(m_context.get());
    if (m_tilerProgramSwizzleAA)
        m_tilerProgramSwizzleAA->cleanup(m_context.get());
    if (m_canvasLayerProgram)
        m_canvasLayerProgram->cleanup(m_context.get());
    if (m_pluginLayerProgram)
        m_pluginLayerProgram->cleanup(m_context.get());
    if (m_pluginLayerProgramFlip)
        m_pluginLayerProgramFlip->cleanup(m_context.get());
    if (m_renderSurfaceMaskProgram)
        m_renderSurfaceMaskProgram->cleanup(m_context.get());
    if (m_renderSurfaceMaskProgramAA)
        m_renderSurfaceMaskProgramAA->cleanup(m_context.get());
    if (m_renderSurfaceProgram)
        m_renderSurfaceProgram->cleanup(m_context.get());
    if (m_renderSurfaceProgramAA)
        m_renderSurfaceProgramAA->cleanup(m_context.get());
    if (m_videoLayerRGBAProgram)
        m_videoLayerRGBAProgram->cleanup(m_context.get());
    if (m_videoLayerYUVProgram)
        m_videoLayerYUVProgram->cleanup(m_context.get());

    m_borderProgram.clear();
    m_headsUpDisplayProgram.clear();
    m_tilerProgram.clear();
    m_tilerProgramOpaque.clear();
    m_tilerProgramAA.clear();
    m_tilerProgramSwizzle.clear();
    m_tilerProgramSwizzleOpaque.clear();
    m_tilerProgramSwizzleAA.clear();
    m_canvasLayerProgram.clear();
    m_pluginLayerProgram.clear();
    m_pluginLayerProgramFlip.clear();
    m_renderSurfaceMaskProgram.clear();
    m_renderSurfaceMaskProgramAA.clear();
    m_renderSurfaceProgram.clear();
    m_renderSurfaceProgramAA.clear();
    m_videoLayerRGBAProgram.clear();
    m_videoLayerYUVProgram.clear();
    if (m_offscreenFramebufferId)
        GLC(m_context.get(), m_context->deleteFramebuffer(m_offscreenFramebufferId));

    releaseRenderSurfaceTextures();
}

String LayerRendererChromium::layerTreeAsText() const
{
    TextStream ts;
    if (rootLayer()) {
        ts << rootLayer()->layerTreeAsText();
        ts << "RenderSurfaces:\n";
        dumpRenderSurfaces(ts, 1, rootLayer());
    }
    return ts.release();
}

void LayerRendererChromium::dumpRenderSurfaces(TextStream& ts, int indent, const CCLayerImpl* layer) const
{
    if (layer->renderSurface())
        layer->renderSurface()->dumpSurface(ts, indent);

    for (size_t i = 0; i < layer->children().size(); ++i)
        dumpRenderSurfaces(ts, indent, layer->children()[i].get());
}

bool LayerRendererChromium::isContextLost()
{
    return (m_context.get()->getExtensions()->getGraphicsResetStatusARB() != GraphicsContext3D::NO_ERROR);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
