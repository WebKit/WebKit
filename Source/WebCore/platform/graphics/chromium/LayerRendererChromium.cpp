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

#include "CCDamageTracker.h"
#include "CCLayerQuad.h"
#include "CCMathUtil.h"
#include "CCProxy.h"
#include "CCRenderPass.h"
#include "CCRenderSurfaceFilters.h"
#include "CCScopedTexture.h"
#include "CCSettings.h"
#include "CCSingleThreadProxy.h"
#include "CCVideoLayerImpl.h"
#include "Extensions3D.h"
#include "FloatQuad.h"
#include "GeometryBinding.h"
#include "GrTexture.h"
#include "NotImplemented.h"
#include "PlatformColor.h"
#include "SkBitmap.h"
#include "SkColor.h"
#include "ThrottledTextureUploader.h"
#include "TraceEvent.h"
#include "UnthrottledTextureUploader.h"
#include <public/WebGraphicsContext3D.h>
#include <public/WebSharedGraphicsContext3D.h>
#include <public/WebVideoFrame.h>
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>
#include <wtf/text/StringHash.h>

using namespace std;
using WebKit::WebGraphicsContext3D;
using WebKit::WebGraphicsMemoryAllocation;
using WebKit::WebSharedGraphicsContext3D;
using WebKit::WebTransformationMatrix;

namespace WebCore {

namespace {

bool needsIOSurfaceReadbackWorkaround()
{
#if OS(DARWIN)
    return true;
#else
    return false;
#endif
}

} // anonymous namespace

PassOwnPtr<LayerRendererChromium> LayerRendererChromium::create(CCRendererClient* client, CCResourceProvider* resourceProvider, TextureUploaderOption textureUploaderSetting)
{
    OwnPtr<LayerRendererChromium> layerRenderer(adoptPtr(new LayerRendererChromium(client, resourceProvider, textureUploaderSetting)));
    if (!layerRenderer->initialize())
        return nullptr;

    return layerRenderer.release();
}

LayerRendererChromium::LayerRendererChromium(CCRendererClient* client,
                                             CCResourceProvider* resourceProvider,
                                             TextureUploaderOption textureUploaderSetting)
    : CCDirectRenderer(client, resourceProvider)
    , m_offscreenFramebufferId(0)
    , m_sharedGeometryQuad(FloatRect(-0.5f, -0.5f, 1.0f, 1.0f))
    , m_context(resourceProvider->graphicsContext3D())
    , m_isViewportChanged(false)
    , m_isFramebufferDiscarded(false)
    , m_isUsingBindUniform(false)
    , m_visible(true)
    , m_textureUploaderSetting(textureUploaderSetting)
{
    ASSERT(m_context);
}

bool LayerRendererChromium::initialize()
{
    if (!m_context->makeContextCurrent())
        return false;

    m_context->setContextLostCallback(this);
    m_context->pushGroupMarkerEXT("CompositorContext");

    WebKit::WebString extensionsWebString = m_context->getString(GraphicsContext3D::EXTENSIONS);
    String extensionsString(extensionsWebString.data(), extensionsWebString.length());
    Vector<String> extensionsList;
    extensionsString.split(' ', extensionsList);
    HashSet<String> extensions;
    for (size_t i = 0; i < extensionsList.size(); ++i)
        extensions.add(extensionsList[i]);

    if (settings().acceleratePainting && extensions.contains("GL_EXT_texture_format_BGRA8888")
                                      && extensions.contains("GL_EXT_read_format_bgra"))
        m_capabilities.usingAcceleratedPainting = true;
    else
        m_capabilities.usingAcceleratedPainting = false;


    m_capabilities.contextHasCachedFrontBuffer = extensions.contains("GL_CHROMIUM_front_buffer_cached");

    m_capabilities.usingPartialSwap = CCSettings::partialSwapEnabled() && extensions.contains("GL_CHROMIUM_post_sub_buffer");

    // Use the swapBuffers callback only with the threaded proxy.
    if (CCProxy::hasImplThread())
        m_capabilities.usingSwapCompleteCallback = extensions.contains("GL_CHROMIUM_swapbuffers_complete_callback");
    if (m_capabilities.usingSwapCompleteCallback) {
        m_context->setSwapBuffersCompleteCallbackCHROMIUM(this);
    }

    m_capabilities.usingSetVisibility = extensions.contains("GL_CHROMIUM_set_visibility");

    if (extensions.contains("GL_CHROMIUM_iosurface")) {
        ASSERT(extensions.contains("GL_ARB_texture_rectangle"));
    }

    m_capabilities.usingGpuMemoryManager = extensions.contains("GL_CHROMIUM_gpu_memory_manager");
    if (m_capabilities.usingGpuMemoryManager)
        m_context->setMemoryAllocationChangedCallbackCHROMIUM(this);

    m_capabilities.usingDiscardFramebuffer = extensions.contains("GL_CHROMIUM_discard_framebuffer");

    m_capabilities.usingEglImage = extensions.contains("GL_OES_EGL_image_external");

    GLC(m_context, m_context->getIntegerv(GraphicsContext3D::MAX_TEXTURE_SIZE, &m_capabilities.maxTextureSize));
    m_capabilities.bestTextureFormat = PlatformColor::bestTextureFormat(m_context, extensions.contains("GL_EXT_texture_format_BGRA8888"));

    m_isUsingBindUniform = extensions.contains("GL_CHROMIUM_bind_uniform_location");

    if (!initializeSharedObjects())
        return false;

    // Make sure the viewport and context gets initialized, even if it is to zero.
    viewportChanged();
    return true;
}

LayerRendererChromium::~LayerRendererChromium()
{
    ASSERT(CCProxy::isImplThread());
    m_context->setSwapBuffersCompleteCallbackCHROMIUM(0);
    m_context->setMemoryAllocationChangedCallbackCHROMIUM(0);
    m_context->setContextLostCallback(0);
    cleanupSharedObjects();
}

WebGraphicsContext3D* LayerRendererChromium::context()
{
    return m_context;
}

void LayerRendererChromium::debugGLCall(WebGraphicsContext3D* context, const char* command, const char* file, int line)
{
    unsigned long error = context->getError();
    if (error != GraphicsContext3D::NO_ERROR)
        LOG_ERROR("GL command failed: File: %s\n\tLine %d\n\tcommand: %s, error %x\n", file, line, command, static_cast<int>(error));
}

void LayerRendererChromium::setVisible(bool visible)
{
    if (m_visible == visible)
        return;
    m_visible = visible;

    // TODO: Replace setVisibilityCHROMIUM with an extension to explicitly manage front/backbuffers
    // crbug.com/116049
    if (m_capabilities.usingSetVisibility) {
        m_context->setVisibilityCHROMIUM(visible);
    }
}

void LayerRendererChromium::releaseRenderPassTextures()
{
    m_renderPassTextures.clear();
}

void LayerRendererChromium::viewportChanged()
{
    m_isViewportChanged = true;
}

void LayerRendererChromium::clearFramebuffer(DrawingFrame& frame)
{
    // On DEBUG builds, opaque render passes are cleared to blue to easily see regions that were not drawn on the screen.
    if (frame.currentRenderPass->hasTransparentBackground())
        GLC(m_context, m_context->clearColor(0, 0, 0, 0));
    else
        GLC(m_context, m_context->clearColor(0, 0, 1, 1));

#if defined(NDEBUG)
    if (frame.currentRenderPass->hasTransparentBackground())
#endif
        m_context->clear(GraphicsContext3D::COLOR_BUFFER_BIT);
}

void LayerRendererChromium::beginDrawingFrame(DrawingFrame& frame)
{
    // FIXME: Remove this once framebuffer is automatically recreated on first use
    ensureFramebuffer();

    if (viewportSize().isEmpty())
        return;

    TRACE_EVENT0("cc", "LayerRendererChromium::drawLayers");
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

    GLC(m_context, m_context->disable(GraphicsContext3D::DEPTH_TEST));
    GLC(m_context, m_context->disable(GraphicsContext3D::CULL_FACE));
    GLC(m_context, m_context->colorMask(true, true, true, true));
    GLC(m_context, m_context->enable(GraphicsContext3D::BLEND));
    GLC(m_context, m_context->blendFunc(GraphicsContext3D::ONE, GraphicsContext3D::ONE_MINUS_SRC_ALPHA));
}

void LayerRendererChromium::doNoOp()
{
    GLC(m_context, m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, 0));
    GLC(m_context, m_context->flush());
}

void LayerRendererChromium::drawQuad(DrawingFrame& frame, const CCDrawQuad* quad)
{
    if (quad->needsBlending())
        GLC(m_context, m_context->enable(GraphicsContext3D::BLEND));
    else
        GLC(m_context, m_context->disable(GraphicsContext3D::BLEND));

    switch (quad->material()) {
    case CCDrawQuad::Invalid:
        ASSERT_NOT_REACHED();
        break;
    case CCDrawQuad::Checkerboard:
        drawCheckerboardQuad(frame, CCCheckerboardDrawQuad::materialCast(quad));
        break;
    case CCDrawQuad::DebugBorder:
        drawDebugBorderQuad(frame, CCDebugBorderDrawQuad::materialCast(quad));
        break;
    case CCDrawQuad::IOSurfaceContent:
        drawIOSurfaceQuad(frame, CCIOSurfaceDrawQuad::materialCast(quad));
        break;
    case CCDrawQuad::RenderPass:
        drawRenderPassQuad(frame, CCRenderPassDrawQuad::materialCast(quad));
        break;
    case CCDrawQuad::SolidColor:
        drawSolidColorQuad(frame, CCSolidColorDrawQuad::materialCast(quad));
        break;
    case CCDrawQuad::StreamVideoContent:
        drawStreamVideoQuad(frame, CCStreamVideoDrawQuad::materialCast(quad));
        break;
    case CCDrawQuad::TextureContent:
        drawTextureQuad(frame, CCTextureDrawQuad::materialCast(quad));
        break;
    case CCDrawQuad::TiledContent:
        drawTileQuad(frame, CCTileDrawQuad::materialCast(quad));
        break;
    case CCDrawQuad::YUVVideoContent:
        drawYUVVideoQuad(frame, CCYUVVideoDrawQuad::materialCast(quad));
        break;
    }
}

void LayerRendererChromium::drawCheckerboardQuad(const DrawingFrame& frame, const CCCheckerboardDrawQuad* quad)
{
    const TileCheckerboardProgram* program = tileCheckerboardProgram();
    ASSERT(program && program->initialized());
    GLC(context(), context()->useProgram(program->program()));

    IntRect tileRect = quad->quadRect();
    float texOffsetX = tileRect.x();
    float texOffsetY = tileRect.y();
    float texScaleX = tileRect.width();
    float texScaleY = tileRect.height();
    GLC(context(), context()->uniform4f(program->fragmentShader().texTransformLocation(), texOffsetX, texOffsetY, texScaleX, texScaleY));

    const int checkerboardWidth = 16;
    float frequency = 1.0 / checkerboardWidth;

    GLC(context(), context()->uniform1f(program->fragmentShader().frequencyLocation(), frequency));

    setShaderOpacity(quad->opacity(), program->fragmentShader().alphaLocation());
    drawQuadGeometry(frame, quad->quadTransform(), quad->quadRect(), program->vertexShader().matrixLocation());
}

void LayerRendererChromium::drawDebugBorderQuad(const DrawingFrame& frame, const CCDebugBorderDrawQuad* quad)
{
    static float glMatrix[16];
    const SolidColorProgram* program = solidColorProgram();
    ASSERT(program && program->initialized());
    GLC(context(), context()->useProgram(program->program()));

    // Use the full quadRect for debug quads to not move the edges based on partial swaps.
    const IntRect& layerRect = quad->quadRect();
    WebTransformationMatrix renderMatrix = quad->quadTransform();
    renderMatrix.translate(0.5 * layerRect.width() + layerRect.x(), 0.5 * layerRect.height() + layerRect.y());
    renderMatrix.scaleNonUniform(layerRect.width(), layerRect.height());
    LayerRendererChromium::toGLMatrix(&glMatrix[0], frame.projectionMatrix * renderMatrix);
    GLC(context(), context()->uniformMatrix4fv(program->vertexShader().matrixLocation(), 1, false, &glMatrix[0]));

    SkColor color = quad->color();
    float alpha = SkColorGetA(color) / 255.0;

    GLC(context(), context()->uniform4f(program->fragmentShader().colorLocation(), (SkColorGetR(color) / 255.0) * alpha, (SkColorGetG(color) / 255.0) * alpha, (SkColorGetB(color) / 255.0) * alpha, alpha));

    GLC(context(), context()->lineWidth(quad->width()));

    // The indices for the line are stored in the same array as the triangle indices.
    GLC(context(), context()->drawElements(GraphicsContext3D::LINE_LOOP, 4, GraphicsContext3D::UNSIGNED_SHORT, 6 * sizeof(unsigned short)));
}

static inline SkBitmap applyFilters(LayerRendererChromium* layerRenderer, const WebKit::WebFilterOperations& filters, CCScopedTexture* sourceTexture)
{
    if (filters.isEmpty())
        return SkBitmap();

    WebGraphicsContext3D* filterContext = CCProxy::hasImplThread() ? WebSharedGraphicsContext3D::compositorThreadContext() : WebSharedGraphicsContext3D::mainThreadContext();
    GrContext* filterGrContext = CCProxy::hasImplThread() ? WebSharedGraphicsContext3D::compositorThreadGrContext() : WebSharedGraphicsContext3D::mainThreadGrContext();

    if (!filterContext || !filterGrContext)
        return SkBitmap();

    layerRenderer->context()->flush();

    CCResourceProvider::ScopedWriteLockGL lock(layerRenderer->resourceProvider(), sourceTexture->id());
    SkBitmap source = CCRenderSurfaceFilters::apply(filters, lock.textureId(), sourceTexture->size(), filterContext, filterGrContext);
    return source;
}

PassOwnPtr<CCScopedTexture> LayerRendererChromium::drawBackgroundFilters(DrawingFrame& frame, const CCRenderPassDrawQuad* quad, const WebKit::WebFilterOperations& filters, const WebTransformationMatrix& contentsDeviceTransform)
{
    // This method draws a background filter, which applies a filter to any pixels behind the quad and seen through its background.
    // The algorithm works as follows:
    // 1. Compute a bounding box around the pixels that will be visible through the quad.
    // 2. Read the pixels in the bounding box into a buffer R.
    // 3. Apply the background filter to R, so that it is applied in the pixels' coordinate space.
    // 4. Apply the quad's inverse transform to map the pixels in R into the quad's content space. This implicitly
    // clips R by the content bounds of the quad since the destination texture has bounds matching the quad's content.
    // 5. Draw the background texture for the contents using the same transform as used to draw the contents itself. This is done
    // without blending to replace the current background pixels with the new filtered background.
    // 6. Draw the contents of the quad over drop of the new background with blending, as per usual. The filtered background
    // pixels will show through any non-opaque pixels in this draws.
    //
    // Pixel copies in this algorithm occur at steps 2, 3, 4, and 5.

    // FIXME: When this algorithm changes, update CCLayerTreeHost::prioritizeTextures() accordingly.

    if (filters.isEmpty())
        return nullptr;

    // FIXME: We only allow background filters on an opaque render surface because other surfaces may contain
    // translucent pixels, and the contents behind those translucent pixels wouldn't have the filter applied.
    if (frame.currentRenderPass->hasTransparentBackground())
        return nullptr;
    ASSERT(!frame.currentTexture);

    // FIXME: Do a single readback for both the surface and replica and cache the filtered results (once filter textures are not reused).
    IntRect deviceRect = enclosingIntRect(CCMathUtil::mapClippedRect(contentsDeviceTransform, sharedGeometryQuad().boundingBox()));

    int top, right, bottom, left;
    filters.getOutsets(top, right, bottom, left);
    deviceRect.move(-left, -top);
    deviceRect.expand(left + right, top + bottom);

    deviceRect.intersect(frame.currentRenderPass->outputRect());

    OwnPtr<CCScopedTexture> deviceBackgroundTexture = CCScopedTexture::create(m_resourceProvider);
    if (!getFramebufferTexture(deviceBackgroundTexture.get(), deviceRect))
        return nullptr;

    SkBitmap filteredDeviceBackground = applyFilters(this, filters, deviceBackgroundTexture.get());
    if (!filteredDeviceBackground.getTexture())
        return nullptr;

    GrTexture* texture = reinterpret_cast<GrTexture*>(filteredDeviceBackground.getTexture());
    int filteredDeviceBackgroundTextureId = texture->getTextureHandle();

    OwnPtr<CCScopedTexture> backgroundTexture = CCScopedTexture::create(m_resourceProvider);
    if (!backgroundTexture->allocate(CCRenderer::ImplPool, quad->quadRect().size(), GraphicsContext3D::RGBA, CCResourceProvider::TextureUsageFramebuffer))
        return nullptr;

    const CCRenderPass* targetRenderPass = frame.currentRenderPass;
    bool usingBackgroundTexture = useScopedTexture(frame, backgroundTexture.get(), quad->quadRect());

    if (usingBackgroundTexture) {
        // Copy the readback pixels from device to the background texture for the surface.
        WebTransformationMatrix deviceToFramebufferTransform;
        deviceToFramebufferTransform.translate(quad->quadRect().width() / 2.0, quad->quadRect().height() / 2.0);
        deviceToFramebufferTransform.scale3d(quad->quadRect().width(), quad->quadRect().height(), 1);
        deviceToFramebufferTransform.multiply(contentsDeviceTransform.inverse());
        copyTextureToFramebuffer(frame, filteredDeviceBackgroundTextureId, deviceRect, deviceToFramebufferTransform);
    }

    useRenderPass(frame, targetRenderPass);

    if (!usingBackgroundTexture)
        return nullptr;
    return backgroundTexture.release();
}

void LayerRendererChromium::drawRenderPassQuad(DrawingFrame& frame, const CCRenderPassDrawQuad* quad)
{
    CachedTexture* contentsTexture = m_renderPassTextures.get(quad->renderPassId());
    if (!contentsTexture || !contentsTexture->id())
        return;

    const CCRenderPass* renderPass = frame.renderPassesById->get(quad->renderPassId());
    ASSERT(renderPass);
    if (!renderPass)
        return;

    WebTransformationMatrix renderMatrix = quad->quadTransform();
    renderMatrix.translate(0.5 * quad->quadRect().width() + quad->quadRect().x(), 0.5 * quad->quadRect().height() + quad->quadRect().y());
    WebTransformationMatrix deviceMatrix = renderMatrix;
    deviceMatrix.scaleNonUniform(quad->quadRect().width(), quad->quadRect().height());
    WebTransformationMatrix contentsDeviceTransform = WebTransformationMatrix(frame.windowMatrix * frame.projectionMatrix * deviceMatrix).to2dTransform();

    // Can only draw surface if device matrix is invertible.
    if (!contentsDeviceTransform.isInvertible())
        return;

    OwnPtr<CCScopedTexture> backgroundTexture = drawBackgroundFilters(frame, quad, renderPass->backgroundFilters(), contentsDeviceTransform);

    // FIXME: Cache this value so that we don't have to do it for both the surface and its replica.
    // Apply filters to the contents texture.
    SkBitmap filterBitmap = applyFilters(this, renderPass->filters(), contentsTexture);
    OwnPtr<CCResourceProvider::ScopedReadLockGL> contentsResourceLock;
    unsigned contentsTextureId = 0;
    if (filterBitmap.getTexture()) {
        GrTexture* texture = reinterpret_cast<GrTexture*>(filterBitmap.getTexture());
        contentsTextureId = texture->getTextureHandle();
    } else {
        contentsResourceLock = adoptPtr(new CCResourceProvider::ScopedReadLockGL(m_resourceProvider, contentsTexture->id()));
        contentsTextureId = contentsResourceLock->textureId();
    }

    // Draw the background texture if there is one.
    if (backgroundTexture) {
        ASSERT(backgroundTexture->size() == quad->quadRect().size());
        CCResourceProvider::ScopedReadLockGL lock(m_resourceProvider, backgroundTexture->id());
        copyTextureToFramebuffer(frame, lock.textureId(), quad->quadRect(), quad->quadTransform());
    }

    bool clipped = false;
    FloatQuad deviceQuad = CCMathUtil::mapQuad(contentsDeviceTransform, sharedGeometryQuad(), clipped);
    ASSERT(!clipped);
    CCLayerQuad deviceLayerBounds = CCLayerQuad(FloatQuad(deviceQuad.boundingBox()));
    CCLayerQuad deviceLayerEdges = CCLayerQuad(deviceQuad);

    // Use anti-aliasing programs only when necessary.
    bool useAA = (!deviceQuad.isRectilinear() || !deviceQuad.boundingBox().isExpressibleAsIntRect());
    if (useAA) {
        deviceLayerBounds.inflateAntiAliasingDistance();
        deviceLayerEdges.inflateAntiAliasingDistance();
    }

    OwnPtr<CCResourceProvider::ScopedReadLockGL> maskResourceLock;
    unsigned maskTextureId = 0;
    if (quad->maskResourceId()) {
        maskResourceLock = adoptPtr(new CCResourceProvider::ScopedReadLockGL(m_resourceProvider, quad->maskResourceId()));
        maskTextureId = maskResourceLock->textureId();
    }

    // FIXME: use the backgroundTexture and blend the background in with this draw instead of having a separate copy of the background texture.

    GLC(context(), context()->activeTexture(GraphicsContext3D::TEXTURE0));
    context()->bindTexture(GraphicsContext3D::TEXTURE_2D, contentsTextureId);

    int shaderQuadLocation = -1;
    int shaderEdgeLocation = -1;
    int shaderMaskSamplerLocation = -1;
    int shaderMaskTexCoordScaleLocation = -1;
    int shaderMaskTexCoordOffsetLocation = -1;
    int shaderMatrixLocation = -1;
    int shaderAlphaLocation = -1;
    if (useAA && maskTextureId) {
        const RenderPassMaskProgramAA* program = renderPassMaskProgramAA();
        GLC(context(), context()->useProgram(program->program()));
        GLC(context(), context()->uniform1i(program->fragmentShader().samplerLocation(), 0));

        shaderQuadLocation = program->vertexShader().pointLocation();
        shaderEdgeLocation = program->fragmentShader().edgeLocation();
        shaderMaskSamplerLocation = program->fragmentShader().maskSamplerLocation();
        shaderMaskTexCoordScaleLocation = program->fragmentShader().maskTexCoordScaleLocation();
        shaderMaskTexCoordOffsetLocation = program->fragmentShader().maskTexCoordOffsetLocation();
        shaderMatrixLocation = program->vertexShader().matrixLocation();
        shaderAlphaLocation = program->fragmentShader().alphaLocation();
    } else if (!useAA && maskTextureId) {
        const RenderPassMaskProgram* program = renderPassMaskProgram();
        GLC(context(), context()->useProgram(program->program()));
        GLC(context(), context()->uniform1i(program->fragmentShader().samplerLocation(), 0));

        shaderMaskSamplerLocation = program->fragmentShader().maskSamplerLocation();
        shaderMaskTexCoordScaleLocation = program->fragmentShader().maskTexCoordScaleLocation();
        shaderMaskTexCoordOffsetLocation = program->fragmentShader().maskTexCoordOffsetLocation();
        shaderMatrixLocation = program->vertexShader().matrixLocation();
        shaderAlphaLocation = program->fragmentShader().alphaLocation();
    } else if (useAA && !maskTextureId) {
        const RenderPassProgramAA* program = renderPassProgramAA();
        GLC(context(), context()->useProgram(program->program()));
        GLC(context(), context()->uniform1i(program->fragmentShader().samplerLocation(), 0));

        shaderQuadLocation = program->vertexShader().pointLocation();
        shaderEdgeLocation = program->fragmentShader().edgeLocation();
        shaderMatrixLocation = program->vertexShader().matrixLocation();
        shaderAlphaLocation = program->fragmentShader().alphaLocation();
    } else {
        const RenderPassProgram* program = renderPassProgram();
        GLC(context(), context()->useProgram(program->program()));
        GLC(context(), context()->uniform1i(program->fragmentShader().samplerLocation(), 0));

        shaderMatrixLocation = program->vertexShader().matrixLocation();
        shaderAlphaLocation = program->fragmentShader().alphaLocation();
    }

    if (shaderMaskSamplerLocation != -1) {
        ASSERT(shaderMaskTexCoordScaleLocation != 1);
        ASSERT(shaderMaskTexCoordOffsetLocation != 1);
        GLC(context(), context()->activeTexture(GraphicsContext3D::TEXTURE1));
        GLC(context(), context()->uniform1i(shaderMaskSamplerLocation, 1));
        GLC(context(), context()->uniform2f(shaderMaskTexCoordScaleLocation, quad->maskTexCoordScaleX(), quad->maskTexCoordScaleY()));
        GLC(context(), context()->uniform2f(shaderMaskTexCoordOffsetLocation, quad->maskTexCoordOffsetX(), quad->maskTexCoordOffsetY()));
        context()->bindTexture(GraphicsContext3D::TEXTURE_2D, maskTextureId);
        GLC(context(), context()->activeTexture(GraphicsContext3D::TEXTURE0));
    }

    if (shaderEdgeLocation != -1) {
        float edge[24];
        deviceLayerEdges.toFloatArray(edge);
        deviceLayerBounds.toFloatArray(&edge[12]);
        GLC(context(), context()->uniform3fv(shaderEdgeLocation, 8, edge));
    }

    // Map device space quad to surface space. contentsDeviceTransform has no 3d component since it was generated with to2dTransform() so we don't need to project.
    FloatQuad surfaceQuad = CCMathUtil::mapQuad(contentsDeviceTransform.inverse(), deviceLayerEdges.floatQuad(), clipped);
    ASSERT(!clipped);

    setShaderOpacity(quad->opacity(), shaderAlphaLocation);
    setShaderFloatQuad(surfaceQuad, shaderQuadLocation);
    drawQuadGeometry(frame, quad->quadTransform(), quad->quadRect(), shaderMatrixLocation);
}

void LayerRendererChromium::drawSolidColorQuad(const DrawingFrame& frame, const CCSolidColorDrawQuad* quad)
{
    const SolidColorProgram* program = solidColorProgram();
    GLC(context(), context()->useProgram(program->program()));

    SkColor color = quad->color();
    float opacity = quad->opacity();
    float alpha = (SkColorGetA(color) / 255.0) * opacity;

    GLC(context(), context()->uniform4f(program->fragmentShader().colorLocation(), (SkColorGetR(color) / 255.0) * alpha, (SkColorGetG(color) / 255.0) * alpha, (SkColorGetB(color) / 255.0) * alpha, alpha));

    drawQuadGeometry(frame, quad->quadTransform(), quad->quadRect(), program->vertexShader().matrixLocation());
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

void LayerRendererChromium::drawTileQuad(const DrawingFrame& frame, const CCTileDrawQuad* quad)
{
    IntRect tileRect = quad->quadVisibleRect();

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
                               IntPoint(tileRect.location() - quad->quadRect().location());

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


    FloatQuad localQuad;
    WebTransformationMatrix deviceTransform = WebTransformationMatrix(frame.windowMatrix * frame.projectionMatrix * quad->quadTransform()).to2dTransform();
    if (!deviceTransform.isInvertible())
        return;

    bool clipped = false;
    FloatQuad deviceLayerQuad = CCMathUtil::mapQuad(deviceTransform, FloatQuad(quad->visibleContentRect()), clipped);
    ASSERT(!clipped);

    TileProgramUniforms uniforms;
    // For now, we simply skip anti-aliasing with the quad is clipped. This only happens
    // on perspective transformed layers that go partially behind the camera.
    if (quad->isAntialiased() && !clipped) {
        if (quad->swizzleContents())
            tileUniformLocation(tileProgramSwizzleAA(), uniforms);
        else
            tileUniformLocation(tileProgramAA(), uniforms);
    } else {
        if (quad->needsBlending()) {
            if (quad->swizzleContents())
                tileUniformLocation(tileProgramSwizzle(), uniforms);
            else
                tileUniformLocation(tileProgram(), uniforms);
        } else {
            if (quad->swizzleContents())
                tileUniformLocation(tileProgramSwizzleOpaque(), uniforms);
            else
                tileUniformLocation(tileProgramOpaque(), uniforms);
        }
    }

    GLC(context(), context()->useProgram(uniforms.program));
    GLC(context(), context()->uniform1i(uniforms.samplerLocation, 0));
    GLC(context(), context()->activeTexture(GraphicsContext3D::TEXTURE0));
    CCResourceProvider::ScopedReadLockGL quadResourceLock(m_resourceProvider, quad->resourceId());
    GLC(context(), context()->bindTexture(GraphicsContext3D::TEXTURE_2D, quadResourceLock.textureId()));
    GLC(context(), context()->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, quad->textureFilter()));
    GLC(context(), context()->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, quad->textureFilter()));

    bool useAA = !clipped && quad->isAntialiased();
    if (useAA) {
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
        bottomRight = CCMathUtil::mapPoint(deviceTransform, bottomRight, clipped);
        ASSERT(!clipped);
        bottomLeft = CCMathUtil::mapPoint(deviceTransform, bottomLeft, clipped);
        ASSERT(!clipped);
        topLeft = CCMathUtil::mapPoint(deviceTransform, topLeft, clipped);
        ASSERT(!clipped);
        topRight = CCMathUtil::mapPoint(deviceTransform, topRight, clipped);
        ASSERT(!clipped);

        CCLayerQuad::Edge bottomEdge(bottomRight, bottomLeft);
        CCLayerQuad::Edge leftEdge(bottomLeft, topLeft);
        CCLayerQuad::Edge topEdge(topLeft, topRight);
        CCLayerQuad::Edge rightEdge(topRight, bottomRight);

        // Only apply anti-aliasing to edges not clipped by culling or scissoring.
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

        // Map device space quad to local space. contentsDeviceTransform has no 3d component since it was generated with to2dTransform() so we don't need to project.
        WebTransformationMatrix inverseDeviceTransform = deviceTransform.inverse();
        localQuad = CCMathUtil::mapQuad(inverseDeviceTransform, deviceQuad.floatQuad(), clipped);

        // We should not ASSERT(!clipped) here, because anti-aliasing inflation may cause deviceQuad to become
        // clipped.  To our knowledge this scenario does not need to be handled differently than the unclipped case.
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

    setShaderOpacity(quad->opacity(), uniforms.alphaLocation);
    setShaderFloatQuad(localQuad, uniforms.pointLocation);

    // The tile quad shader behaves differently compared to all other shaders.
    // The transform and vertex data are used to figure out the extents that the
    // un-antialiased quad should have and which vertex this is and the float
    // quad passed in via uniform is the actual geometry that gets used to draw
    // it. This is why this centered rect is used and not the original quadRect.
    FloatRect centeredRect(FloatPoint(-0.5 * tileRect.width(), -0.5 * tileRect.height()), tileRect.size());
    drawQuadGeometry(frame, quad->quadTransform(), centeredRect, uniforms.matrixLocation);
}

void LayerRendererChromium::drawYUVVideoQuad(const DrawingFrame& frame, const CCYUVVideoDrawQuad* quad)
{
    const VideoYUVProgram* program = videoYUVProgram();
    ASSERT(program && program->initialized());

    const CCVideoLayerImpl::FramePlane& yPlane = quad->yPlane();
    const CCVideoLayerImpl::FramePlane& uPlane = quad->uPlane();
    const CCVideoLayerImpl::FramePlane& vPlane = quad->vPlane();

    CCResourceProvider::ScopedReadLockGL yPlaneLock(m_resourceProvider, yPlane.resourceId);
    CCResourceProvider::ScopedReadLockGL uPlaneLock(m_resourceProvider, uPlane.resourceId);
    CCResourceProvider::ScopedReadLockGL vPlaneLock(m_resourceProvider, vPlane.resourceId);
    GLC(context(), context()->activeTexture(GraphicsContext3D::TEXTURE1));
    GLC(context(), context()->bindTexture(GraphicsContext3D::TEXTURE_2D, yPlaneLock.textureId()));
    GLC(context(), context()->activeTexture(GraphicsContext3D::TEXTURE2));
    GLC(context(), context()->bindTexture(GraphicsContext3D::TEXTURE_2D, uPlaneLock.textureId()));
    GLC(context(), context()->activeTexture(GraphicsContext3D::TEXTURE3));
    GLC(context(), context()->bindTexture(GraphicsContext3D::TEXTURE_2D, vPlaneLock.textureId()));

    GLC(context(), context()->useProgram(program->program()));

    float yWidthScaleFactor = static_cast<float>(yPlane.visibleSize.width()) / yPlane.size.width();
    // Arbitrarily take the u sizes because u and v dimensions are identical.
    float uvWidthScaleFactor = static_cast<float>(uPlane.visibleSize.width()) / uPlane.size.width();
    GLC(context(), context()->uniform1f(program->vertexShader().yWidthScaleFactorLocation(), yWidthScaleFactor));
    GLC(context(), context()->uniform1f(program->vertexShader().uvWidthScaleFactorLocation(), uvWidthScaleFactor));

    GLC(context(), context()->uniform1i(program->fragmentShader().yTextureLocation(), 1));
    GLC(context(), context()->uniform1i(program->fragmentShader().uTextureLocation(), 2));
    GLC(context(), context()->uniform1i(program->fragmentShader().vTextureLocation(), 3));

    // These values are magic numbers that are used in the transformation from YUV to RGB color values.
    // They are taken from the following webpage: http://www.fourcc.org/fccyvrgb.php
    float yuv2RGB[9] = {
        1.164f, 1.164f, 1.164f,
        0.f, -.391f, 2.018f,
        1.596f, -.813f, 0.f,
    };
    GLC(context(), context()->uniformMatrix3fv(program->fragmentShader().ccMatrixLocation(), 1, 0, yuv2RGB));

    // These values map to 16, 128, and 128 respectively, and are computed
    // as a fraction over 256 (e.g. 16 / 256 = 0.0625).
    // They are used in the YUV to RGBA conversion formula:
    //   Y - 16   : Gives 16 values of head and footroom for overshooting
    //   U - 128  : Turns unsigned U into signed U [-128,127]
    //   V - 128  : Turns unsigned V into signed V [-128,127]
    float yuvAdjust[3] = {
        -0.0625f,
        -0.5f,
        -0.5f,
    };
    GLC(context(), context()->uniform3fv(program->fragmentShader().yuvAdjLocation(), 1, yuvAdjust));

    setShaderOpacity(quad->opacity(), program->fragmentShader().alphaLocation());
    drawQuadGeometry(frame, quad->quadTransform(), quad->quadRect(), program->vertexShader().matrixLocation());

    // Reset active texture back to texture 0.
    GLC(context(), context()->activeTexture(GraphicsContext3D::TEXTURE0));
}

void LayerRendererChromium::drawStreamVideoQuad(const DrawingFrame& frame, const CCStreamVideoDrawQuad* quad)
{
    static float glMatrix[16];

    ASSERT(m_capabilities.usingEglImage);

    const VideoStreamTextureProgram* program = videoStreamTextureProgram();
    GLC(context(), context()->useProgram(program->program()));

    toGLMatrix(&glMatrix[0], quad->matrix());
    GLC(context(), context()->uniformMatrix4fv(program->vertexShader().texMatrixLocation(), 1, false, glMatrix));

    GLC(context(), context()->activeTexture(GraphicsContext3D::TEXTURE0));
    GLC(context(), context()->bindTexture(Extensions3DChromium::GL_TEXTURE_EXTERNAL_OES, quad->textureId()));

    GLC(context(), context()->uniform1i(program->fragmentShader().samplerLocation(), 0));

    setShaderOpacity(quad->opacity(), program->fragmentShader().alphaLocation());
    drawQuadGeometry(frame, quad->quadTransform(), quad->quadRect(), program->vertexShader().matrixLocation());
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

struct TexTransformTextureProgramBinding : TextureProgramBinding {
    template<class Program> void set(Program* program)
    {
        TextureProgramBinding::set(program);
        texTransformLocation = program->vertexShader().texTransformLocation();
    }
    int texTransformLocation;
};

void LayerRendererChromium::drawTextureQuad(const DrawingFrame& frame, const CCTextureDrawQuad* quad)
{
    ASSERT(CCProxy::isImplThread());

    TexTransformTextureProgramBinding binding;
    if (quad->flipped())
        binding.set(textureProgramFlip());
    else
        binding.set(textureProgram());
    GLC(context(), context()->useProgram(binding.programId));
    GLC(context(), context()->uniform1i(binding.samplerLocation, 0));
    const FloatRect& uvRect = quad->uvRect();
    GLC(context(), context()->uniform4f(binding.texTransformLocation, uvRect.x(), uvRect.y(), uvRect.width(), uvRect.height()));

    GLC(context(), context()->activeTexture(GraphicsContext3D::TEXTURE0));
    CCResourceProvider::ScopedReadLockGL quadResourceLock(m_resourceProvider, quad->resourceId());
    GLC(context(), context()->bindTexture(GraphicsContext3D::TEXTURE_2D, quadResourceLock.textureId()));

    // FIXME: setting the texture parameters every time is redundant. Move this code somewhere
    // where it will only happen once per texture.
    GLC(context(), context()->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR));
    GLC(context(), context()->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR));
    GLC(context(), context()->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE));
    GLC(context(), context()->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE));

    if (!quad->premultipliedAlpha()) {
        // As it turns out, the premultiplied alpha blending function (ONE, ONE_MINUS_SRC_ALPHA)
        // will never cause the alpha channel to be set to anything less than 1.0 if it is
        // initialized to that value! Therefore, premultipliedAlpha being false is the first
        // situation we can generally see an alpha channel less than 1.0 coming out of the
        // compositor. This is causing platform differences in some layout tests (see
        // https://bugs.webkit.org/show_bug.cgi?id=82412), so in this situation, use a separate
        // blend function for the alpha channel to avoid modifying it. Don't use colorMask for this
        // as it has performance implications on some platforms.
        GLC(context(), context()->blendFuncSeparate(GraphicsContext3D::SRC_ALPHA, GraphicsContext3D::ONE_MINUS_SRC_ALPHA, GraphicsContext3D::ZERO, GraphicsContext3D::ONE));
    }

    setShaderOpacity(quad->opacity(), binding.alphaLocation);
    drawQuadGeometry(frame, quad->quadTransform(), quad->quadRect(), binding.matrixLocation);

    if (!quad->premultipliedAlpha())
        GLC(m_context, m_context->blendFunc(GraphicsContext3D::ONE, GraphicsContext3D::ONE_MINUS_SRC_ALPHA));
}

void LayerRendererChromium::drawIOSurfaceQuad(const DrawingFrame& frame, const CCIOSurfaceDrawQuad* quad)
{
    ASSERT(CCProxy::isImplThread());
    TexTransformTextureProgramBinding binding;
    binding.set(textureIOSurfaceProgram());

    GLC(context(), context()->useProgram(binding.programId));
    GLC(context(), context()->uniform1i(binding.samplerLocation, 0));
    if (quad->orientation() == CCIOSurfaceDrawQuad::Flipped)
        GLC(context(), context()->uniform4f(binding.texTransformLocation, 0, quad->ioSurfaceSize().height(), quad->ioSurfaceSize().width(), quad->ioSurfaceSize().height() * -1.0));
    else
        GLC(context(), context()->uniform4f(binding.texTransformLocation, 0, 0, quad->ioSurfaceSize().width(), quad->ioSurfaceSize().height()));

    GLC(context(), context()->activeTexture(GraphicsContext3D::TEXTURE0));
    GLC(context(), context()->bindTexture(Extensions3D::TEXTURE_RECTANGLE_ARB, quad->ioSurfaceTextureId()));

    setShaderOpacity(quad->opacity(), binding.alphaLocation);
    drawQuadGeometry(frame, quad->quadTransform(), quad->quadRect(), binding.matrixLocation);

    GLC(context(), context()->bindTexture(Extensions3D::TEXTURE_RECTANGLE_ARB, 0));
}

void LayerRendererChromium::finishDrawingFrame(DrawingFrame& frame)
{
    m_currentFramebufferLock.clear();
    m_swapBufferRect.unite(enclosingIntRect(frame.rootDamageRect));

    GLC(m_context, m_context->disable(GraphicsContext3D::SCISSOR_TEST));
    GLC(m_context, m_context->disable(GraphicsContext3D::BLEND));
}

void LayerRendererChromium::toGLMatrix(float* flattened, const WebTransformationMatrix& m)
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

void LayerRendererChromium::setShaderFloatQuad(const FloatQuad& quad, int quadLocation)
{
    if (quadLocation == -1)
        return;

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

void LayerRendererChromium::setShaderOpacity(float opacity, int alphaLocation)
{
    if (alphaLocation != -1)
        GLC(m_context, m_context->uniform1f(alphaLocation, opacity));
}

void LayerRendererChromium::drawQuadGeometry(const DrawingFrame& frame, const WebKit::WebTransformationMatrix& drawTransform, const FloatRect& quadRect, int matrixLocation)
{
    WebTransformationMatrix quadRectMatrix;
    quadRectTransform(&quadRectMatrix, drawTransform, quadRect);
    static float glMatrix[16];
    toGLMatrix(&glMatrix[0], frame.projectionMatrix * quadRectMatrix);
    GLC(m_context, m_context->uniformMatrix4fv(matrixLocation, 1, false, &glMatrix[0]));

    GLC(m_context, m_context->drawElements(GraphicsContext3D::TRIANGLES, 6, GraphicsContext3D::UNSIGNED_SHORT, 0));
}

void LayerRendererChromium::copyTextureToFramebuffer(const DrawingFrame& frame, int textureId, const IntRect& rect, const WebTransformationMatrix& drawMatrix)
{
    const RenderPassProgram* program = renderPassProgram();

    GLC(context(), context()->activeTexture(GraphicsContext3D::TEXTURE0));
    GLC(context(), context()->bindTexture(GraphicsContext3D::TEXTURE_2D, textureId));
    GLC(context(), context()->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR));
    GLC(context(), context()->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR));
    GLC(context(), context()->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE));
    GLC(context(), context()->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE));

    GLC(context(), context()->useProgram(program->program()));
    GLC(context(), context()->uniform1i(program->fragmentShader().samplerLocation(), 0));
    setShaderOpacity(1, program->fragmentShader().alphaLocation());
    drawQuadGeometry(frame, drawMatrix, rect, program->vertexShader().matrixLocation());
}

void LayerRendererChromium::finish()
{
    TRACE_EVENT0("cc", "LayerRendererChromium::finish");
    m_context->finish();
}

bool LayerRendererChromium::swapBuffers()
{
    ASSERT(m_visible);
    ASSERT(!m_isFramebufferDiscarded);

    TRACE_EVENT0("cc", "LayerRendererChromium::swapBuffers");
    // We're done! Time to swapbuffers!

    if (m_capabilities.usingPartialSwap) {
        // If supported, we can save significant bandwidth by only swapping the damaged/scissored region (clamped to the viewport)
        m_swapBufferRect.intersect(IntRect(IntPoint(), viewportSize()));
        int flippedYPosOfRectBottom = viewportHeight() - m_swapBufferRect.y() - m_swapBufferRect.height();
        m_context->postSubBufferCHROMIUM(m_swapBufferRect.x(), flippedYPosOfRectBottom, m_swapBufferRect.width(), m_swapBufferRect.height());
    } else {
        // Note that currently this has the same effect as swapBuffers; we should
        // consider exposing a different entry point on WebGraphicsContext3D.
        m_context->prepareTexture();
    }

    m_swapBufferRect = IntRect();

    return true;
}

void LayerRendererChromium::onSwapBuffersComplete()
{
    m_client->onSwapBuffersComplete();
}

void LayerRendererChromium::onMemoryAllocationChanged(WebGraphicsMemoryAllocation allocation)
{
    // FIXME: This is called on the main thread in single threaded mode, but we expect it on the impl thread.
    if (!CCProxy::hasImplThread()) {
      ASSERT(CCProxy::isMainThread());
      DebugScopedSetImplThread impl;
      onMemoryAllocationChangedOnImplThread(allocation);
    } else {
      ASSERT(CCProxy::isImplThread());
      onMemoryAllocationChangedOnImplThread(allocation);
    }
}

void LayerRendererChromium::onMemoryAllocationChangedOnImplThread(WebKit::WebGraphicsMemoryAllocation allocation)
{
    if (m_visible && !allocation.gpuResourceSizeInBytes)
        return;

    if (!allocation.suggestHaveBackbuffer && !m_visible)
        discardFramebuffer();

    if (!allocation.gpuResourceSizeInBytes) {
        releaseRenderPassTextures();
        m_client->releaseContentsTextures();
        GLC(m_context, m_context->flush());
    } else
        m_client->setMemoryAllocationLimitBytes(allocation.gpuResourceSizeInBytes);
}

void LayerRendererChromium::discardFramebuffer()
{
    if (m_isFramebufferDiscarded)
        return;

    if (!m_capabilities.usingDiscardFramebuffer)
        return;

    // FIXME: Update attachments argument to appropriate values once they are no longer ignored.
    m_context->discardFramebufferEXT(GraphicsContext3D::TEXTURE_2D, 0, 0);
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

    m_context->ensureFramebufferCHROMIUM();
    m_isFramebufferDiscarded = false;
}

void LayerRendererChromium::onContextLost()
{
    m_client->didLoseContext();
}


void LayerRendererChromium::getFramebufferPixels(void *pixels, const IntRect& rect)
{
    ASSERT(rect.maxX() <= viewportWidth() && rect.maxY() <= viewportHeight());

    if (!pixels)
        return;

    makeContextCurrent();

    bool doWorkaround = needsIOSurfaceReadbackWorkaround();

    Platform3DObject temporaryTexture = 0;
    Platform3DObject temporaryFBO = 0;

    if (doWorkaround) {
        // On Mac OS X, calling glReadPixels against an FBO whose color attachment is an
        // IOSurface-backed texture causes corruption of future glReadPixels calls, even those on
        // different OpenGL contexts. It is believed that this is the root cause of top crasher
        // http://crbug.com/99393. <rdar://problem/10949687>

        temporaryTexture = m_context->createTexture();
        GLC(m_context, m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, temporaryTexture));
        GLC(m_context, m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR));
        GLC(m_context, m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR));
        GLC(m_context, m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE));
        GLC(m_context, m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE));
        // Copy the contents of the current (IOSurface-backed) framebuffer into a temporary texture.
        GLC(m_context, m_context->copyTexImage2D(GraphicsContext3D::TEXTURE_2D, 0, GraphicsContext3D::RGBA, 0, 0, rect.maxX(), rect.maxY(), 0));
        temporaryFBO = m_context->createFramebuffer();
        // Attach this texture to an FBO, and perform the readback from that FBO.
        GLC(m_context, m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, temporaryFBO));
        GLC(m_context, m_context->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, temporaryTexture, 0));

        ASSERT(m_context->checkFramebufferStatus(GraphicsContext3D::FRAMEBUFFER) == GraphicsContext3D::FRAMEBUFFER_COMPLETE);
    }

    GLC(m_context, m_context->readPixels(rect.x(), rect.y(), rect.width(), rect.height(),
                                     GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, pixels));

    if (doWorkaround) {
        // Clean up.
        GLC(m_context, m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, 0));
        GLC(m_context, m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, 0));
        GLC(m_context, m_context->deleteFramebuffer(temporaryFBO));
        GLC(m_context, m_context->deleteTexture(temporaryTexture));
    }

    if (!m_visible) {
        TRACE_EVENT0("cc", "LayerRendererChromium::getFramebufferPixels dropping resources after readback");
        discardFramebuffer();
        releaseRenderPassTextures();
        m_client->releaseContentsTextures();
        GLC(m_context, m_context->flush());
    }
}

bool LayerRendererChromium::getFramebufferTexture(CCScopedTexture* texture, const IntRect& deviceRect)
{
    ASSERT(!texture->id() || (texture->size() == deviceRect.size() && texture->format() == GraphicsContext3D::RGB));

    if (!texture->id() && !texture->allocate(CCRenderer::ImplPool, deviceRect.size(), GraphicsContext3D::RGB, CCResourceProvider::TextureUsageAny))
        return false;

    CCResourceProvider::ScopedWriteLockGL lock(m_resourceProvider, texture->id());
    GLC(m_context, m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, lock.textureId()));
    GLC(m_context, m_context->copyTexImage2D(GraphicsContext3D::TEXTURE_2D, 0, texture->format(),
                                             deviceRect.x(), deviceRect.y(), deviceRect.width(), deviceRect.height(), 0));
    return true;
}

bool LayerRendererChromium::useScopedTexture(DrawingFrame& frame, const CCScopedTexture* texture, const IntRect& viewportRect)
{
    ASSERT(texture->id());
    frame.currentRenderPass = 0;
    frame.currentTexture = texture;

    return bindFramebufferToTexture(frame, texture, viewportRect);
}

void LayerRendererChromium::bindFramebufferToOutputSurface(DrawingFrame& frame)
{
    m_currentFramebufferLock.clear();
    GLC(m_context, m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, 0));
}

bool LayerRendererChromium::bindFramebufferToTexture(DrawingFrame& frame, const CCScopedTexture* texture, const IntRect& framebufferRect)
{
    ASSERT(texture->id());

    GLC(m_context, m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_offscreenFramebufferId));
    m_currentFramebufferLock = adoptPtr(new CCResourceProvider::ScopedWriteLockGL(m_resourceProvider, texture->id()));
    unsigned textureId = m_currentFramebufferLock->textureId();
    GLC(m_context, m_context->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, textureId, 0));

#if !defined ( NDEBUG )
    if (m_context->checkFramebufferStatus(GraphicsContext3D::FRAMEBUFFER) != GraphicsContext3D::FRAMEBUFFER_COMPLETE) {
        ASSERT_NOT_REACHED();
        return false;
    }
#endif

    initializeMatrices(frame, framebufferRect, false);
    setDrawViewportSize(framebufferRect.size());

    return true;
}

void LayerRendererChromium::enableScissorTestRect(const IntRect& scissorRect)
{
    GLC(m_context, m_context->enable(GraphicsContext3D::SCISSOR_TEST));
    GLC(m_context, m_context->scissor(scissorRect.x(), scissorRect.y(), scissorRect.width(), scissorRect.height()));
}

void LayerRendererChromium::disableScissorTest()
{
    GLC(m_context, m_context->disable(GraphicsContext3D::SCISSOR_TEST));
}

void LayerRendererChromium::setDrawViewportSize(const IntSize& viewportSize)
{
    GLC(m_context, m_context->viewport(0, 0, viewportSize.width(), viewportSize.height()));
}

bool LayerRendererChromium::makeContextCurrent()
{
    return m_context->makeContextCurrent();
}

bool LayerRendererChromium::initializeSharedObjects()
{
    TRACE_EVENT0("cc", "LayerRendererChromium::initializeSharedObjects");
    makeContextCurrent();

    // Create an FBO for doing offscreen rendering.
    GLC(m_context, m_offscreenFramebufferId = m_context->createFramebuffer());

    // We will always need these programs to render, so create the programs eagerly so that the shader compilation can
    // start while we do other work. Other programs are created lazily on first access.
    m_sharedGeometry = adoptPtr(new GeometryBinding(m_context, quadVertexRect()));
    m_renderPassProgram = adoptPtr(new RenderPassProgram(m_context));
    m_tileProgram = adoptPtr(new TileProgram(m_context));
    m_tileProgramOpaque = adoptPtr(new TileProgramOpaque(m_context));

    GLC(m_context, m_context->flush());

    m_textureCopier = AcceleratedTextureCopier::create(m_context, m_isUsingBindUniform);
    if (m_textureUploaderSetting == ThrottledUploader)
        m_textureUploader = ThrottledTextureUploader::create(m_context);
    else
        m_textureUploader = UnthrottledTextureUploader::create();

    return true;
}

const LayerRendererChromium::TileCheckerboardProgram* LayerRendererChromium::tileCheckerboardProgram()
{
    if (!m_tileCheckerboardProgram)
        m_tileCheckerboardProgram = adoptPtr(new TileCheckerboardProgram(m_context));
    if (!m_tileCheckerboardProgram->initialized()) {
        TRACE_EVENT0("cc", "LayerRendererChromium::checkerboardProgram::initalize");
        m_tileCheckerboardProgram->initialize(m_context, m_isUsingBindUniform);
    }
    return m_tileCheckerboardProgram.get();
}

const LayerRendererChromium::SolidColorProgram* LayerRendererChromium::solidColorProgram()
{
    if (!m_solidColorProgram)
        m_solidColorProgram = adoptPtr(new SolidColorProgram(m_context));
    if (!m_solidColorProgram->initialized()) {
        TRACE_EVENT0("cc", "LayerRendererChromium::solidColorProgram::initialize");
        m_solidColorProgram->initialize(m_context, m_isUsingBindUniform);
    }
    return m_solidColorProgram.get();
}

const LayerRendererChromium::RenderPassProgram* LayerRendererChromium::renderPassProgram()
{
    ASSERT(m_renderPassProgram);
    if (!m_renderPassProgram->initialized()) {
        TRACE_EVENT0("cc", "LayerRendererChromium::renderPassProgram::initialize");
        m_renderPassProgram->initialize(m_context, m_isUsingBindUniform);
    }
    return m_renderPassProgram.get();
}

const LayerRendererChromium::RenderPassProgramAA* LayerRendererChromium::renderPassProgramAA()
{
    if (!m_renderPassProgramAA)
        m_renderPassProgramAA = adoptPtr(new RenderPassProgramAA(m_context));
    if (!m_renderPassProgramAA->initialized()) {
        TRACE_EVENT0("cc", "LayerRendererChromium::renderPassProgramAA::initialize");
        m_renderPassProgramAA->initialize(m_context, m_isUsingBindUniform);
    }
    return m_renderPassProgramAA.get();
}

const LayerRendererChromium::RenderPassMaskProgram* LayerRendererChromium::renderPassMaskProgram()
{
    if (!m_renderPassMaskProgram)
        m_renderPassMaskProgram = adoptPtr(new RenderPassMaskProgram(m_context));
    if (!m_renderPassMaskProgram->initialized()) {
        TRACE_EVENT0("cc", "LayerRendererChromium::renderPassMaskProgram::initialize");
        m_renderPassMaskProgram->initialize(m_context, m_isUsingBindUniform);
    }
    return m_renderPassMaskProgram.get();
}

const LayerRendererChromium::RenderPassMaskProgramAA* LayerRendererChromium::renderPassMaskProgramAA()
{
    if (!m_renderPassMaskProgramAA)
        m_renderPassMaskProgramAA = adoptPtr(new RenderPassMaskProgramAA(m_context));
    if (!m_renderPassMaskProgramAA->initialized()) {
        TRACE_EVENT0("cc", "LayerRendererChromium::renderPassMaskProgramAA::initialize");
        m_renderPassMaskProgramAA->initialize(m_context, m_isUsingBindUniform);
    }
    return m_renderPassMaskProgramAA.get();
}

const LayerRendererChromium::TileProgram* LayerRendererChromium::tileProgram()
{
    ASSERT(m_tileProgram);
    if (!m_tileProgram->initialized()) {
        TRACE_EVENT0("cc", "LayerRendererChromium::tileProgram::initialize");
        m_tileProgram->initialize(m_context, m_isUsingBindUniform);
    }
    return m_tileProgram.get();
}

const LayerRendererChromium::TileProgramOpaque* LayerRendererChromium::tileProgramOpaque()
{
    ASSERT(m_tileProgramOpaque);
    if (!m_tileProgramOpaque->initialized()) {
        TRACE_EVENT0("cc", "LayerRendererChromium::tileProgramOpaque::initialize");
        m_tileProgramOpaque->initialize(m_context, m_isUsingBindUniform);
    }
    return m_tileProgramOpaque.get();
}

const LayerRendererChromium::TileProgramAA* LayerRendererChromium::tileProgramAA()
{
    if (!m_tileProgramAA)
        m_tileProgramAA = adoptPtr(new TileProgramAA(m_context));
    if (!m_tileProgramAA->initialized()) {
        TRACE_EVENT0("cc", "LayerRendererChromium::tileProgramAA::initialize");
        m_tileProgramAA->initialize(m_context, m_isUsingBindUniform);
    }
    return m_tileProgramAA.get();
}

const LayerRendererChromium::TileProgramSwizzle* LayerRendererChromium::tileProgramSwizzle()
{
    if (!m_tileProgramSwizzle)
        m_tileProgramSwizzle = adoptPtr(new TileProgramSwizzle(m_context));
    if (!m_tileProgramSwizzle->initialized()) {
        TRACE_EVENT0("cc", "LayerRendererChromium::tileProgramSwizzle::initialize");
        m_tileProgramSwizzle->initialize(m_context, m_isUsingBindUniform);
    }
    return m_tileProgramSwizzle.get();
}

const LayerRendererChromium::TileProgramSwizzleOpaque* LayerRendererChromium::tileProgramSwizzleOpaque()
{
    if (!m_tileProgramSwizzleOpaque)
        m_tileProgramSwizzleOpaque = adoptPtr(new TileProgramSwizzleOpaque(m_context));
    if (!m_tileProgramSwizzleOpaque->initialized()) {
        TRACE_EVENT0("cc", "LayerRendererChromium::tileProgramSwizzleOpaque::initialize");
        m_tileProgramSwizzleOpaque->initialize(m_context, m_isUsingBindUniform);
    }
    return m_tileProgramSwizzleOpaque.get();
}

const LayerRendererChromium::TileProgramSwizzleAA* LayerRendererChromium::tileProgramSwizzleAA()
{
    if (!m_tileProgramSwizzleAA)
        m_tileProgramSwizzleAA = adoptPtr(new TileProgramSwizzleAA(m_context));
    if (!m_tileProgramSwizzleAA->initialized()) {
        TRACE_EVENT0("cc", "LayerRendererChromium::tileProgramSwizzleAA::initialize");
        m_tileProgramSwizzleAA->initialize(m_context, m_isUsingBindUniform);
    }
    return m_tileProgramSwizzleAA.get();
}

const LayerRendererChromium::TextureProgram* LayerRendererChromium::textureProgram()
{
    if (!m_textureProgram)
        m_textureProgram = adoptPtr(new TextureProgram(m_context));
    if (!m_textureProgram->initialized()) {
        TRACE_EVENT0("cc", "LayerRendererChromium::textureProgram::initialize");
        m_textureProgram->initialize(m_context, m_isUsingBindUniform);
    }
    return m_textureProgram.get();
}

const LayerRendererChromium::TextureProgramFlip* LayerRendererChromium::textureProgramFlip()
{
    if (!m_textureProgramFlip)
        m_textureProgramFlip = adoptPtr(new TextureProgramFlip(m_context));
    if (!m_textureProgramFlip->initialized()) {
        TRACE_EVENT0("cc", "LayerRendererChromium::textureProgramFlip::initialize");
        m_textureProgramFlip->initialize(m_context, m_isUsingBindUniform);
    }
    return m_textureProgramFlip.get();
}

const LayerRendererChromium::TextureIOSurfaceProgram* LayerRendererChromium::textureIOSurfaceProgram()
{
    if (!m_textureIOSurfaceProgram)
        m_textureIOSurfaceProgram = adoptPtr(new TextureIOSurfaceProgram(m_context));
    if (!m_textureIOSurfaceProgram->initialized()) {
        TRACE_EVENT0("cc", "LayerRendererChromium::textureIOSurfaceProgram::initialize");
        m_textureIOSurfaceProgram->initialize(m_context, m_isUsingBindUniform);
    }
    return m_textureIOSurfaceProgram.get();
}

const LayerRendererChromium::VideoYUVProgram* LayerRendererChromium::videoYUVProgram()
{
    if (!m_videoYUVProgram)
        m_videoYUVProgram = adoptPtr(new VideoYUVProgram(m_context));
    if (!m_videoYUVProgram->initialized()) {
        TRACE_EVENT0("cc", "LayerRendererChromium::videoYUVProgram::initialize");
        m_videoYUVProgram->initialize(m_context, m_isUsingBindUniform);
    }
    return m_videoYUVProgram.get();
}

const LayerRendererChromium::VideoStreamTextureProgram* LayerRendererChromium::videoStreamTextureProgram()
{
    if (!m_videoStreamTextureProgram)
        m_videoStreamTextureProgram = adoptPtr(new VideoStreamTextureProgram(m_context));
    if (!m_videoStreamTextureProgram->initialized()) {
        TRACE_EVENT0("cc", "LayerRendererChromium::streamTextureProgram::initialize");
        m_videoStreamTextureProgram->initialize(m_context, m_isUsingBindUniform);
    }
    return m_videoStreamTextureProgram.get();
}

void LayerRendererChromium::cleanupSharedObjects()
{
    makeContextCurrent();

    m_sharedGeometry.clear();

    if (m_tileProgram)
        m_tileProgram->cleanup(m_context);
    if (m_tileProgramOpaque)
        m_tileProgramOpaque->cleanup(m_context);
    if (m_tileProgramSwizzle)
        m_tileProgramSwizzle->cleanup(m_context);
    if (m_tileProgramSwizzleOpaque)
        m_tileProgramSwizzleOpaque->cleanup(m_context);
    if (m_tileProgramAA)
        m_tileProgramAA->cleanup(m_context);
    if (m_tileProgramSwizzleAA)
        m_tileProgramSwizzleAA->cleanup(m_context);
    if (m_tileCheckerboardProgram)
        m_tileCheckerboardProgram->cleanup(m_context);

    if (m_renderPassMaskProgram)
        m_renderPassMaskProgram->cleanup(m_context);
    if (m_renderPassProgram)
        m_renderPassProgram->cleanup(m_context);
    if (m_renderPassMaskProgramAA)
        m_renderPassMaskProgramAA->cleanup(m_context);
    if (m_renderPassProgramAA)
        m_renderPassProgramAA->cleanup(m_context);

    if (m_textureProgram)
        m_textureProgram->cleanup(m_context);
    if (m_textureProgramFlip)
        m_textureProgramFlip->cleanup(m_context);
    if (m_textureIOSurfaceProgram)
        m_textureIOSurfaceProgram->cleanup(m_context);

    if (m_videoYUVProgram)
        m_videoYUVProgram->cleanup(m_context);
    if (m_videoStreamTextureProgram)
        m_videoStreamTextureProgram->cleanup(m_context);

    if (m_solidColorProgram)
        m_solidColorProgram->cleanup(m_context);

    if (m_offscreenFramebufferId)
        GLC(m_context, m_context->deleteFramebuffer(m_offscreenFramebufferId));

    m_textureCopier.clear();
    m_textureUploader.clear();

    releaseRenderPassTextures();
}

bool LayerRendererChromium::isContextLost()
{
    return (m_context->getGraphicsResetStatusARB() != GraphicsContext3D::NO_ERROR);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
