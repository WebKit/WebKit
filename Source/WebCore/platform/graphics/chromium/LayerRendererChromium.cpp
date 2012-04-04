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
#include "TextureCopier.h"
#include "TextureManager.h"
#include "TraceEvent.h"
#include "TrackingTextureAllocator.h"
#include "TreeSynchronizer.h"
#include "WebGLLayerChromium.h"
#include "cc/CCDamageTracker.h"
#include "cc/CCDebugBorderDrawQuad.h"
#include "cc/CCLayerImpl.h"
#include "cc/CCLayerTreeHostCommon.h"
#include "cc/CCProxy.h"
#include "cc/CCRenderPass.h"
#include "cc/CCRenderSurfaceDrawQuad.h"
#include "cc/CCSolidColorDrawQuad.h"
#include "cc/CCTextureDrawQuad.h"
#include "cc/CCTileDrawQuad.h"
#include "cc/CCVideoDrawQuad.h"
#include "Extensions3D.h"
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
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

bool needsLionIOSurfaceReadbackWorkaround()
{
#if OS(DARWIN)
    static SInt32 systemVersion = 0;
    if (!systemVersion) {
        if (Gestalt(gestaltSystemVersion, &systemVersion) != noErr)
            return false;
    }

    return systemVersion >= 0x1070;
#else
    return false;
#endif
}

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

class LayerRendererGpuMemoryAllocationChangedCallbackAdapter : public Extensions3DChromium::GpuMemoryAllocationChangedCallbackCHROMIUM {
public:
    static PassOwnPtr<LayerRendererGpuMemoryAllocationChangedCallbackAdapter> create(LayerRendererChromium* layerRenderer)
    {
        return adoptPtr(new LayerRendererGpuMemoryAllocationChangedCallbackAdapter(layerRenderer));
    }
    virtual ~LayerRendererGpuMemoryAllocationChangedCallbackAdapter() { }

    virtual void onGpuMemoryAllocationChanged(Extensions3DChromium::GpuMemoryAllocationCHROMIUM allocation)
    {
        if (!allocation.suggestHaveBackbuffer)
            m_layerRenderer->discardFramebuffer();
        else
            m_layerRenderer->ensureFramebuffer();
    }

private:
    explicit LayerRendererGpuMemoryAllocationChangedCallbackAdapter(LayerRendererChromium* layerRenderer)
        : m_layerRenderer(layerRenderer)
    {
    }

    LayerRendererChromium* m_layerRenderer;
};


PassOwnPtr<LayerRendererChromium> LayerRendererChromium::create(LayerRendererChromiumClient* client, PassRefPtr<GraphicsContext3D> context)
{
    OwnPtr<LayerRendererChromium> layerRenderer(adoptPtr(new LayerRendererChromium(client, context)));
    if (!layerRenderer->initialize())
        return nullptr;

    return layerRenderer.release();
}

LayerRendererChromium::LayerRendererChromium(LayerRendererChromiumClient* client,
                                             PassRefPtr<GraphicsContext3D> context)
    : m_client(client)
    , m_currentRenderSurface(0)
    , m_offscreenFramebufferId(0)
    , m_context(context)
    , m_defaultRenderSurface(0)
    , m_sharedGeometryQuad(FloatRect(-0.5f, -0.5f, 1.0f, 1.0f))
    , m_isViewportChanged(false)
    , m_isFramebufferDiscarded(false)
{
}

class ContextLostCallbackAdapter : public GraphicsContext3D::ContextLostCallback {
public:
    static PassOwnPtr<ContextLostCallbackAdapter> create(LayerRendererChromiumClient* client)
    {
        return adoptPtr(new ContextLostCallbackAdapter(client));
    }

    virtual void onContextLost()
    {
        m_client->didLoseContext();
    }

private:
    explicit ContextLostCallbackAdapter(LayerRendererChromiumClient* client)
        : m_client(client) { }

    LayerRendererChromiumClient* m_client;
};

bool LayerRendererChromium::initialize()
{
    if (!m_context->makeContextCurrent())
        return false;

    m_context->setContextLostCallback(ContextLostCallbackAdapter::create(m_client));

    if (settings().acceleratePainting && contextSupportsAcceleratedPainting(m_context.get()))
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

    m_capabilities.usingGpuMemoryManager = extensions->supports("GL_CHROMIUM_gpu_memory_manager");
    if (m_capabilities.usingGpuMemoryManager) {
        extensions->ensureEnabled("GL_CHROMIUM_gpu_memory_manager");
        Extensions3DChromium* extensions3DChromium = static_cast<Extensions3DChromium*>(extensions);
        extensions3DChromium->setGpuMemoryAllocationChangedCallbackCHROMIUM(LayerRendererGpuMemoryAllocationChangedCallbackAdapter::create(this));
    }

    m_capabilities.usingDiscardFramebuffer = extensions->supports("GL_CHROMIUM_discard_framebuffer");
    if (m_capabilities.usingDiscardFramebuffer)
        extensions->ensureEnabled("GL_CHROMIUM_discard_framebuffer");

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
    extensions3DChromium->setGpuMemoryAllocationChangedCallbackCHROMIUM(nullptr);
    m_headsUpDisplay.clear(); // Explicitly destroy the HUD before the TextureManager dies.
    cleanupSharedObjects();
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

    // TODO: Replace setVisibilityCHROMIUM with an extension to explicitly manage front/backbuffers
    // crbug.com/116049
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

void LayerRendererChromium::clearRenderSurface(CCRenderSurface* renderSurface, CCRenderSurface* rootRenderSurface, const FloatRect& surfaceDamageRect)
{
    // Non-root layers should clear their entire contents to transparent. On DEBUG builds, the root layer
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

#if defined(NDEBUG)
    if (renderSurface != rootRenderSurface)
#endif
        m_context->clear(GraphicsContext3D::COLOR_BUFFER_BIT);

    GLC(m_context.get(), m_context->enable(GraphicsContext3D::SCISSOR_TEST));
}

void LayerRendererChromium::beginDrawingFrame()
{
    ASSERT(rootLayer());

    // FIXME: Remove this once framebuffer is automatically recreated on first use
    ensureFramebuffer();

    m_defaultRenderSurface = rootLayer()->renderSurface();
    ASSERT(m_defaultRenderSurface);

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

    clearRenderSurface(renderSurface, m_defaultRenderSurface, renderPass->surfaceDamageRect());

    const CCQuadList& quadList = renderPass->quadList();
    for (CCQuadList::constBackToFrontIterator it = quadList.backToFrontBegin(); it != quadList.backToFrontEnd(); ++it)
        drawQuad(it->get(), renderPass->surfaceDamageRect());
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
    case CCDrawQuad::TextureContent:
        drawTextureQuad(quad->toTextureDrawQuad());
        break;
    case CCDrawQuad::TiledContent:
        drawTileQuad(quad->toTileDrawQuad());
        break;
    case CCDrawQuad::VideoContent:
        drawVideoQuad(quad->toVideoDrawQuad());
        break;
    }
}

void LayerRendererChromium::drawDebugBorderQuad(const CCDebugBorderDrawQuad* quad)
{
    static float glMatrix[16];
    const LayerChromium::BorderProgram* program = borderProgram();
    ASSERT(program && program->initialized());
    GLC(context(), context()->useProgram(program->program()));

    const IntRect& layerRect = quad->quadRect();
    TransformationMatrix renderMatrix = quad->quadTransform();
    renderMatrix.translate(0.5 * layerRect.width() + layerRect.x(), 0.5 * layerRect.height() + layerRect.y());
    renderMatrix.scaleNonUniform(layerRect.width(), layerRect.height());
    LayerRendererChromium::toGLMatrix(&glMatrix[0], projectionMatrix() * renderMatrix);
    GLC(context(), context()->uniformMatrix4fv(program->vertexShader().matrixLocation(), 1, false, &glMatrix[0]));

    GLC(context(), context()->uniform4f(program->fragmentShader().colorLocation(), quad->color().red() / 255.0, quad->color().green() / 255.0, quad->color().blue() / 255.0, quad->color().alpha() / 255.0));

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

    GLC(context(), context()->uniform4f(solidColorProgram->fragmentShader().colorLocation(), color.red() / 255.0, color.green() / 255.0, color.blue() / 255.0, color.alpha() / 255.0));

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
    const IntRect& tileRect = quad->quadVisibleRect();

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

    FloatPoint textureOffset = quad->textureOffset() + clampOffset +
                               IntPoint(quad->quadVisibleRect().location() - quad->quadRect().location());

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
        GLC(context(), context()->uniform3fv(uniforms.edgeLocation, 8, edge));

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

        // Only apply anti-aliasing to edges not clipped during culling.
        if (quad->topEdgeAA() && tileRect.y() == quad->quadRect().y())
            topEdge = deviceLayerEdges.top();
        if (quad->leftEdgeAA() && tileRect.x() == quad->quadRect().x())
            leftEdge = deviceLayerEdges.left();
        if (quad->rightEdgeAA() && tileRect.maxX() == quad->quadRect().maxX())
            rightEdge = deviceLayerEdges.right();
        if (quad->bottomEdgeAA() && tileRect.maxY() == quad->quadRect().maxY())
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

void LayerRendererChromium::drawYUV(const CCVideoDrawQuad* quad)
{
    const CCVideoLayerImpl::YUVProgram* program = videoLayerYUVProgram();
    ASSERT(program && program->initialized());

    const CCVideoLayerImpl::Texture& yTexture = quad->textures()[VideoFrameChromium::yPlane];
    const CCVideoLayerImpl::Texture& uTexture = quad->textures()[VideoFrameChromium::uPlane];
    const CCVideoLayerImpl::Texture& vTexture = quad->textures()[VideoFrameChromium::vPlane];

    GLC(context(), context()->activeTexture(GraphicsContext3D::TEXTURE1));
    GLC(context(), context()->bindTexture(GraphicsContext3D::TEXTURE_2D, yTexture.m_texture->textureId()));
    GLC(context(), context()->activeTexture(GraphicsContext3D::TEXTURE2));
    GLC(context(), context()->bindTexture(GraphicsContext3D::TEXTURE_2D, uTexture.m_texture->textureId()));
    GLC(context(), context()->activeTexture(GraphicsContext3D::TEXTURE3));
    GLC(context(), context()->bindTexture(GraphicsContext3D::TEXTURE_2D, vTexture.m_texture->textureId()));

    GLC(context(), context()->useProgram(program->program()));

    float yWidthScaleFactor = static_cast<float>(yTexture.m_visibleSize.width()) / yTexture.m_texture->size().width();
    // Arbitrarily take the u sizes because u and v dimensions are identical.
    float uvWidthScaleFactor = static_cast<float>(uTexture.m_visibleSize.width()) / uTexture.m_texture->size().width();
    GLC(context(), context()->uniform1f(program->vertexShader().yWidthScaleFactorLocation(), yWidthScaleFactor));
    GLC(context(), context()->uniform1f(program->vertexShader().uvWidthScaleFactorLocation(), uvWidthScaleFactor));

    GLC(context(), context()->uniform1i(program->fragmentShader().yTextureLocation(), 1));
    GLC(context(), context()->uniform1i(program->fragmentShader().uTextureLocation(), 2));
    GLC(context(), context()->uniform1i(program->fragmentShader().vTextureLocation(), 3));

    GLC(context(), context()->uniformMatrix3fv(program->fragmentShader().ccMatrixLocation(), 1, 0, const_cast<float*>(CCVideoLayerImpl::yuv2RGB)));
    GLC(context(), context()->uniform3fv(program->fragmentShader().yuvAdjLocation(), 1, const_cast<float*>(CCVideoLayerImpl::yuvAdjust)));

    const IntSize& bounds = quad->quadRect().size();
    drawTexturedQuad(quad->layerTransform(), bounds.width(), bounds.height(), quad->opacity(), FloatQuad(),
                                    program->vertexShader().matrixLocation(),
                                    program->fragmentShader().alphaLocation(),
                                    -1);

    // Reset active texture back to texture 0.
    GLC(context(), context()->activeTexture(GraphicsContext3D::TEXTURE0));
}

template<class Program>
void LayerRendererChromium::drawSingleTextureVideoQuad(const CCVideoDrawQuad* quad, Program* program, float widthScaleFactor, Platform3DObject textureId, GC3Denum target)
{
    ASSERT(program && program->initialized());

    GLC(context(), context()->activeTexture(GraphicsContext3D::TEXTURE0));
    GLC(context(), context()->bindTexture(target, textureId));

    GLC(context(), context()->useProgram(program->program()));
    GLC(context(), context()->uniform4f(program->vertexShader().texTransformLocation(), 0, 0, widthScaleFactor, 1));
    GLC(context(), context()->uniform1i(program->fragmentShader().samplerLocation(), 0));

    const IntSize& bounds = quad->quadRect().size();
    drawTexturedQuad(quad->layerTransform(), bounds.width(), bounds.height(), quad->opacity(), sharedGeometryQuad(),
                                    program->vertexShader().matrixLocation(),
                                    program->fragmentShader().alphaLocation(),
                                    -1);
}

void LayerRendererChromium::drawRGBA(const CCVideoDrawQuad* quad)
{
    const CCVideoLayerImpl::RGBAProgram* program = videoLayerRGBAProgram();
    const CCVideoLayerImpl::Texture& texture = quad->textures()[VideoFrameChromium::rgbPlane];
    float widthScaleFactor = static_cast<float>(texture.m_visibleSize.width()) / texture.m_texture->size().width();
    drawSingleTextureVideoQuad(quad, program, widthScaleFactor, texture.m_texture->textureId(), GraphicsContext3D::TEXTURE_2D);
}

void LayerRendererChromium::drawNativeTexture2D(const CCVideoDrawQuad* quad)
{
    const CCVideoLayerImpl::NativeTextureProgram* program = videoLayerNativeTextureProgram();
    drawSingleTextureVideoQuad(quad, program, 1, quad->frame()->textureId(), GraphicsContext3D::TEXTURE_2D);
}

void LayerRendererChromium::drawStreamTexture(const CCVideoDrawQuad* quad)
{
    ASSERT(context()->getExtensions()->supports("GL_OES_EGL_image_external") && context()->getExtensions()->isEnabled("GL_OES_EGL_image_external"));

    const CCVideoLayerImpl::StreamTextureProgram* program = streamTextureLayerProgram();
    GLC(context(), context()->useProgram(program->program()));
    ASSERT(quad->matrix());
    GLC(context(), context()->uniformMatrix4fv(program->vertexShader().texMatrixLocation(), 1, false, const_cast<float*>(quad->matrix())));

    drawSingleTextureVideoQuad(quad, program, 1, quad->frame()->textureId(), Extensions3DChromium::GL_TEXTURE_EXTERNAL_OES);
}

bool LayerRendererChromium::copyFrameToTextures(const CCVideoDrawQuad* quad)
{
    const VideoFrameChromium* frame = quad->frame();

    for (unsigned plane = 0; plane < frame->planes(); ++plane) {
        ASSERT(quad->frame()->requiredTextureSize(plane) == quad->textures()[plane].m_texture->size());
        copyPlaneToTexture(quad, frame->data(plane), plane);
    }
    for (unsigned plane = frame->planes(); plane < CCVideoLayerImpl::MaxPlanes; ++plane) {
        CCVideoLayerImpl::Texture* texture = &quad->textures()[plane];
        texture->m_texture.clear();
        texture->m_visibleSize = IntSize();
    }
    return true;
}

void LayerRendererChromium::copyPlaneToTexture(const CCVideoDrawQuad* quad, const void* plane, int index)
{
    CCVideoLayerImpl::Texture& texture = quad->textures()[index];
    TextureAllocator* allocator = renderSurfaceTextureAllocator();
    texture.m_texture->bindTexture(context(), allocator);
    GC3Denum format = texture.m_texture->format();
    IntSize dimensions = texture.m_texture->size();

    void* mem = static_cast<Extensions3DChromium*>(context()->getExtensions())->mapTexSubImage2DCHROMIUM(GraphicsContext3D::TEXTURE_2D, 0, 0, 0, dimensions.width(), dimensions.height(), format, GraphicsContext3D::UNSIGNED_BYTE, Extensions3DChromium::WRITE_ONLY);
    if (mem) {
        memcpy(mem, plane, dimensions.width() * dimensions.height());
        GLC(context(), static_cast<Extensions3DChromium*>(context()->getExtensions())->unmapTexSubImage2DCHROMIUM(mem));
    } else {
        // If mapTexSubImage2DCHROMIUM fails, then do the slower texSubImage2D
        // upload. This does twice the copies as mapTexSubImage2DCHROMIUM, one
        // in the command buffer and another to the texture.
        GLC(context(), context()->texSubImage2D(GraphicsContext3D::TEXTURE_2D, 0, 0, 0, dimensions.width(), dimensions.height(), format, GraphicsContext3D::UNSIGNED_BYTE, plane));
    }
}

void LayerRendererChromium::drawVideoQuad(const CCVideoDrawQuad* quad)
{
    ASSERT(CCProxy::isImplThread());

    if (!quad->frame())
        return;

    if (!copyFrameToTextures(quad))
        return;

    switch (quad->format()) {
    case GraphicsContext3D::LUMINANCE:
        drawYUV(quad);
        break;
    case GraphicsContext3D::RGBA:
        drawRGBA(quad);
        break;
    case GraphicsContext3D::TEXTURE_2D:
        drawNativeTexture2D(quad);
        break;
    case Extensions3DChromium::GL_TEXTURE_EXTERNAL_OES:
        drawStreamTexture(quad);
        break;
    default:
        CRASH(); // Someone updated convertVFCFormatToGC3DFormat above but update this!
    }
}

struct TextureProgramBinding {
    template<class Program> void set(Program* program)
    {
        ASSERT(program && program->initialized());
        programId = program->program();
        samplerLocation = program->fragmentShader().samplerLocation();
        matrixLocation = program->vertexShader().matrixLocation();
        alphaLocation = program->fragmentShader().alphaLocation();
    }
    int programId;
    int samplerLocation;
    int matrixLocation;
    int alphaLocation;
};

struct TexStretchTextureProgramBinding : TextureProgramBinding {
    template<class Program> void set(Program* program)
    {
        TextureProgramBinding::set(program);
        offsetLocation = program->vertexShader().offsetLocation();
        scaleLocation = program->vertexShader().scaleLocation();
    }
    int offsetLocation;
    int scaleLocation;
};

struct TexTransformTextureProgramBinding : TextureProgramBinding {
    template<class Program> void set(Program* program)
    {
        TextureProgramBinding::set(program);
        texTransformLocation = program->vertexShader().texTransformLocation();
    }
    int texTransformLocation;
};

void LayerRendererChromium::drawTextureQuad(const CCTextureDrawQuad* quad)
{
    ASSERT(CCProxy::isImplThread());
    unsigned matrixLocation = 0;
    unsigned alphaLocation = 0;
    if (quad->ioSurfaceTextureId()) {
        TexTransformTextureProgramBinding binding;
        if (quad->flipped())
            binding.set(textureLayerTexRectProgramFlip());
        else
            binding.set(textureLayerTexRectProgram());

        GLC(context(), context()->activeTexture(GraphicsContext3D::TEXTURE0));

        GLC(context(), context()->useProgram(binding.programId));
        GLC(context(), context()->uniform1i(binding.samplerLocation, 0));
        GLC(context(), context()->uniform4f(binding.texTransformLocation, 0, 0, quad->ioSurfaceSize().width(), quad->ioSurfaceSize().height()));

        matrixLocation = binding.matrixLocation;
        alphaLocation = binding.alphaLocation;
    } else if (quad->flipped() && quad->uvRect() == FloatRect(0, 0, 1, 1)) {
        // A flipped quad with the default UV mapping is common enough to use a special shader.
        // Canvas 2d and WebGL layers use this path always and plugin/external texture layers use this by default.
        const CCTextureLayerImpl::ProgramFlip* program = textureLayerProgramFlip();
        GLC(context(), context()->useProgram(program->program()));
        GLC(context(), context()->uniform1i(program->fragmentShader().samplerLocation(), 0));
        matrixLocation = program->vertexShader().matrixLocation();
        alphaLocation = program->fragmentShader().alphaLocation();
    } else {
        TexStretchTextureProgramBinding binding;
        if (quad->flipped())
            binding.set(textureLayerProgramStretchFlip());
        else
            binding.set(textureLayerProgramStretch());
        GLC(context, context()->useProgram(binding.programId));
        GLC(context, context()->uniform1i(binding.samplerLocation, 0));
        GLC(context, context()->uniform2f(binding.offsetLocation, quad->uvRect().x(), quad->uvRect().y()));
        GLC(context, context()->uniform2f(binding.scaleLocation, quad->uvRect().width(), quad->uvRect().height()));

        matrixLocation = binding.matrixLocation;
        alphaLocation = binding.alphaLocation;
    }
    GLC(context(), context()->activeTexture(GraphicsContext3D::TEXTURE0));

    if (quad->ioSurfaceTextureId())
        GLC(context(), context()->bindTexture(Extensions3D::TEXTURE_RECTANGLE_ARB, quad->ioSurfaceTextureId()));
    else
        GLC(context(), context()->bindTexture(GraphicsContext3D::TEXTURE_2D, quad->textureId()));

    // FIXME: setting the texture parameters every time is redundant. Move this code somewhere
    // where it will only happen once per texture.
    GLC(context, context()->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR));
    GLC(context, context()->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR));
    GLC(context, context()->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE));
    GLC(context, context()->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE));

    if (quad->hasAlpha() && !quad->premultipliedAlpha())
        GLC(context(), context()->blendFunc(GraphicsContext3D::SRC_ALPHA, GraphicsContext3D::ONE_MINUS_SRC_ALPHA));

    const IntSize& bounds = quad->quadRect().size();

    drawTexturedQuad(quad->layerTransform(), bounds.width(), bounds.height(), quad->opacity(), sharedGeometryQuad(), matrixLocation, alphaLocation, -1);

    GLC(m_context.get(), m_context->blendFunc(GraphicsContext3D::ONE, GraphicsContext3D::ONE_MINUS_SRC_ALPHA));

    if (quad->ioSurfaceTextureId())
        GLC(context(), context()->bindTexture(Extensions3D::TEXTURE_RECTANGLE_ARB, 0));
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
    size_t preferredLimit = reclaimLimit > contentsMemoryUseBytes ? reclaimLimit - contentsMemoryUseBytes : 0;
    m_renderSurfaceTextureManager->setPreferredMemoryLimitBytes(preferredLimit);
    m_renderSurfaceTextureManager->reduceMemoryToLimit(preferredLimit);
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

    GLC(m_context, m_context->uniformMatrix4fv(matrixLocation, 1, false, &glMatrix[0]));

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
        GLC(m_context, m_context->uniform2fv(quadLocation, 4, point));
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

bool LayerRendererChromium::swapBuffers(const IntRect& subBuffer)
{
    // FIXME: Remove this once gpu process supports ignoring swap buffers command while framebuffer is discarded.
    //        Alternatively (preferably?), protect all cc code so as not to attempt a swap after a framebuffer discard.
    if (m_isFramebufferDiscarded) {
        m_client->setFullRootLayerDamage();
        return false;
    }

    TRACE_EVENT("LayerRendererChromium::swapBuffers", this, 0);
    // We're done! Time to swapbuffers!

    if (m_capabilities.usingPartialSwap) {
        // If supported, we can save significant bandwidth by only swapping the damaged/scissored region (clamped to the viewport)
        IntRect clippedSubBuffer = subBuffer;
        clippedSubBuffer.intersect(IntRect(IntPoint::zero(), viewportSize()));
        Extensions3DChromium* extensions3DChromium = static_cast<Extensions3DChromium*>(m_context->getExtensions());
        int flippedYPosOfRectBottom = viewportHeight() - clippedSubBuffer.y() - clippedSubBuffer.height();
        extensions3DChromium->postSubBufferCHROMIUM(clippedSubBuffer.x(), flippedYPosOfRectBottom, clippedSubBuffer.width(), clippedSubBuffer.height());
    } else
        // Note that currently this has the same effect as swapBuffers; we should
        // consider exposing a different entry point on GraphicsContext3D.
        m_context->prepareTexture();

    m_headsUpDisplay->onSwapBuffers();
    return true;
}

void LayerRendererChromium::onSwapBuffersComplete()
{
    m_client->onSwapBuffersComplete();
}

void LayerRendererChromium::discardFramebuffer()
{
    if (m_isFramebufferDiscarded)
        return;

    if (!m_capabilities.usingDiscardFramebuffer)
        return;

    Extensions3D* extensions = m_context->getExtensions();
    Extensions3DChromium* extensions3DChromium = static_cast<Extensions3DChromium*>(extensions);
    // FIXME: Update attachments argument to appropriate values once they are no longer ignored.
    extensions3DChromium->discardFramebufferEXT(GraphicsContext3D::TEXTURE_2D, 0, 0);
    m_isFramebufferDiscarded = true;

    // Damage tracker needs a full reset every time framebuffer is discarded.
    m_client->setFullRootLayerDamage();
}

void LayerRendererChromium::ensureFramebuffer()
{
    if (!m_isFramebufferDiscarded)
        return;

    if (!m_capabilities.usingDiscardFramebuffer)
        return;

    Extensions3D* extensions = m_context->getExtensions();
    Extensions3DChromium* extensions3DChromium = static_cast<Extensions3DChromium*>(extensions);
    extensions3DChromium->ensureFramebufferCHROMIUM();
    m_isFramebufferDiscarded = false;
}

void LayerRendererChromium::getFramebufferPixels(void *pixels, const IntRect& rect)
{
    ASSERT(rect.maxX() <= viewportWidth() && rect.maxY() <= viewportHeight());

    if (!pixels)
        return;

    makeContextCurrent();

    bool doWorkaround = needsLionIOSurfaceReadbackWorkaround();

    Platform3DObject temporaryTexture = NullPlatform3DObject;
    Platform3DObject temporaryFBO = NullPlatform3DObject;
    GraphicsContext3D* context = m_context.get();

    if (doWorkaround) {
        // On Mac OS X 10.7, calling glReadPixels against an FBO whose color attachment is an
        // IOSurface-backed texture causes corruption of future glReadPixels calls, even those on
        // different OpenGL contexts. It is believed that this is the root cause of top crasher
        // http://crbug.com/99393. <rdar://problem/10949687>

        temporaryTexture = context->createTexture();
        GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, temporaryTexture));
        GLC(context, context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR));
        GLC(context, context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR));
        GLC(context, context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE));
        GLC(context, context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE));
        // Copy the contents of the current (IOSurface-backed) framebuffer into a temporary texture.
        GLC(context, context->copyTexImage2D(GraphicsContext3D::TEXTURE_2D, 0, GraphicsContext3D::RGBA, 0, 0, rect.maxX(), rect.maxY(), 0));
        temporaryFBO = context->createFramebuffer();
        // Attach this texture to an FBO, and perform the readback from that FBO.
        GLC(context, context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, temporaryFBO));
        GLC(context, context->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, temporaryTexture, 0));

        ASSERT(context->checkFramebufferStatus(GraphicsContext3D::FRAMEBUFFER) == GraphicsContext3D::FRAMEBUFFER_COMPLETE);
    }

    GLC(context, context->readPixels(rect.x(), rect.y(), rect.width(), rect.height(),
                                     GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, pixels));

    if (doWorkaround) {
        // Clean up.
        GLC(context, context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, 0));
        GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, 0));
        GLC(context, context->deleteFramebuffer(temporaryFBO));
        GLC(context, context->deleteTexture(temporaryTexture));
    }
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
    m_textureCopier = AcceleratedTextureCopier::create(m_context.get());
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

const CCTextureLayerImpl::ProgramFlip* LayerRendererChromium::textureLayerProgramFlip()
{
    if (!m_textureLayerProgramFlip)
        m_textureLayerProgramFlip = adoptPtr(new CCTextureLayerImpl::ProgramFlip(m_context.get()));
    if (!m_textureLayerProgramFlip->initialized()) {
        TRACE_EVENT("LayerRendererChromium::textureLayerProgram::initialize", this, 0);
        m_textureLayerProgramFlip->initialize(m_context.get());
    }
    return m_textureLayerProgramFlip.get();
}

const CCTextureLayerImpl::ProgramStretch* LayerRendererChromium::textureLayerProgramStretch()
{
    if (!m_textureLayerProgramStretch)
        m_textureLayerProgramStretch = adoptPtr(new CCTextureLayerImpl::ProgramStretch(m_context.get()));
    if (!m_textureLayerProgramStretch->initialized()) {
        TRACE_EVENT("LayerRendererChromium::textureLayerProgram::initialize", this, 0);
        m_textureLayerProgramStretch->initialize(m_context.get());
    }
    return m_textureLayerProgramStretch.get();
}

const CCTextureLayerImpl::ProgramStretchFlip* LayerRendererChromium::textureLayerProgramStretchFlip()
{
    if (!m_textureLayerProgramStretchFlip)
        m_textureLayerProgramStretchFlip = adoptPtr(new CCTextureLayerImpl::ProgramStretchFlip(m_context.get()));
    if (!m_textureLayerProgramStretchFlip->initialized()) {
        TRACE_EVENT("LayerRendererChromium::textureLayerProgramStretchFlip::initialize", this, 0);
        m_textureLayerProgramStretchFlip->initialize(m_context.get());
    }
    return m_textureLayerProgramStretchFlip.get();
}

const CCTextureLayerImpl::TexRectProgram* LayerRendererChromium::textureLayerTexRectProgram()
{
    if (!m_textureLayerTexRectProgram)
        m_textureLayerTexRectProgram = adoptPtr(new CCTextureLayerImpl::TexRectProgram(m_context.get()));
    if (!m_textureLayerTexRectProgram->initialized()) {
        TRACE_EVENT("LayerRendererChromium::textureLayerTexRectProgram::initialize", this, 0);
        m_textureLayerTexRectProgram->initialize(m_context.get());
    }
    return m_textureLayerTexRectProgram.get();
}

const CCTextureLayerImpl::TexRectProgramFlip* LayerRendererChromium::textureLayerTexRectProgramFlip()
{
    if (!m_textureLayerTexRectProgramFlip)
        m_textureLayerTexRectProgramFlip = adoptPtr(new CCTextureLayerImpl::TexRectProgramFlip(m_context.get()));
    if (!m_textureLayerTexRectProgramFlip->initialized()) {
        TRACE_EVENT("LayerRendererChromium::textureLayerTexRectProgramFlip::initialize", this, 0);
        m_textureLayerTexRectProgramFlip->initialize(m_context.get());
    }
    return m_textureLayerTexRectProgramFlip.get();
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

const CCVideoLayerImpl::StreamTextureProgram* LayerRendererChromium::streamTextureLayerProgram()
{
    if (!m_streamTextureLayerProgram)
        m_streamTextureLayerProgram = adoptPtr(new CCVideoLayerImpl::StreamTextureProgram(m_context.get()));
    if (!m_streamTextureLayerProgram->initialized()) {
        TRACE_EVENT("LayerRendererChromium::streamTextureLayerProgram::initialize", this, 0);
        m_streamTextureLayerProgram->initialize(m_context.get());
    }
    return m_streamTextureLayerProgram.get();
}

void LayerRendererChromium::cleanupSharedObjects()
{
    makeContextCurrent();

    m_sharedGeometry.clear();

    if (m_borderProgram)
        m_borderProgram->cleanup(m_context.get());
    if (m_headsUpDisplayProgram)
        m_headsUpDisplayProgram->cleanup(m_context.get());
    if (m_textureLayerProgramFlip)
        m_textureLayerProgramFlip->cleanup(m_context.get());
    if (m_textureLayerProgramStretch)
        m_textureLayerProgramStretch->cleanup(m_context.get());
    if (m_textureLayerProgramStretchFlip)
        m_textureLayerProgramStretchFlip->cleanup(m_context.get());
    if (m_textureLayerTexRectProgram)
        m_textureLayerTexRectProgram->cleanup(m_context.get());
    if (m_textureLayerTexRectProgramFlip)
        m_textureLayerTexRectProgramFlip->cleanup(m_context.get());
    if (m_tilerProgram)
        m_tilerProgram->cleanup(m_context.get());
    if (m_tilerProgramOpaque)
        m_tilerProgramOpaque->cleanup(m_context.get());
    if (m_tilerProgramSwizzle)
        m_tilerProgramSwizzle->cleanup(m_context.get());
    if (m_tilerProgramSwizzleOpaque)
        m_tilerProgramSwizzleOpaque->cleanup(m_context.get());
    if (m_tilerProgramAA)
        m_tilerProgramAA->cleanup(m_context.get());
    if (m_tilerProgramSwizzleAA)
        m_tilerProgramSwizzleAA->cleanup(m_context.get());
    if (m_renderSurfaceMaskProgram)
        m_renderSurfaceMaskProgram->cleanup(m_context.get());
    if (m_renderSurfaceProgram)
        m_renderSurfaceProgram->cleanup(m_context.get());
    if (m_renderSurfaceMaskProgramAA)
        m_renderSurfaceMaskProgramAA->cleanup(m_context.get());
    if (m_renderSurfaceProgramAA)
        m_renderSurfaceProgramAA->cleanup(m_context.get());
    if (m_videoLayerRGBAProgram)
        m_videoLayerRGBAProgram->cleanup(m_context.get());
    if (m_videoLayerYUVProgram)
        m_videoLayerYUVProgram->cleanup(m_context.get());
    if (m_videoLayerNativeTextureProgram)
        m_videoLayerNativeTextureProgram->cleanup(m_context.get());
    if (m_streamTextureLayerProgram)
        m_streamTextureLayerProgram->cleanup(m_context.get());

    m_borderProgram.clear();
    m_headsUpDisplayProgram.clear();
    m_textureLayerProgramFlip.clear();
    m_textureLayerProgramStretch.clear();
    m_textureLayerProgramStretchFlip.clear();
    m_textureLayerTexRectProgram.clear();
    m_textureLayerTexRectProgramFlip.clear();
    m_tilerProgram.clear();
    m_tilerProgramOpaque.clear();
    m_tilerProgramSwizzle.clear();
    m_tilerProgramSwizzleOpaque.clear();
    m_tilerProgramAA.clear();
    m_tilerProgramSwizzleAA.clear();
    m_renderSurfaceMaskProgram.clear();
    m_renderSurfaceProgram.clear();
    m_renderSurfaceMaskProgramAA.clear();
    m_renderSurfaceProgramAA.clear();
    m_videoLayerRGBAProgram.clear();
    m_videoLayerYUVProgram.clear();
    m_videoLayerNativeTextureProgram.clear();
    m_streamTextureLayerProgram.clear();
    if (m_offscreenFramebufferId)
        GLC(m_context.get(), m_context->deleteFramebuffer(m_offscreenFramebufferId));
    m_textureCopier.clear();

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
