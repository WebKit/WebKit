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
#include "Extensions3DChromium.h"
#include "FloatQuad.h"
#include "GeometryBinding.h"
#include "GraphicsContext3D.h"
#include "LayerChromium.h"
#include "LayerPainterChromium.h"
#include "LayerTexture.h"
#include "LayerTextureUpdaterCanvas.h"
#include "NotImplemented.h"
#include "TextStream.h"
#include "TextureManager.h"
#include "TreeSynchronizer.h"
#include "TraceEvent.h"
#include "WebGLLayerChromium.h"
#include "cc/CCLayerImpl.h"
#include "cc/CCLayerTreeHostImpl.h"
#include "cc/CCMainThreadTask.h"
#if USE(SKIA)
#include "Extensions3D.h"
#include "GrContext.h"
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#elif USE(CG)
#include <CoreGraphics/CGBitmapContext.h>
#endif
#include <wtf/CurrentTime.h>

namespace WebCore {

// FIXME: Make this limit adjustable and give it a useful value.

// Absolute maximum limit for texture allocations for this instance.
static size_t textureMemoryHighLimitBytes = 128 * 1024 * 1024;
// Preferred texture size limit. Can be exceeded if needed.
static size_t textureMemoryReclaimLimitBytes = 64 * 1024 * 1024;
// The maximum texture memory usage when asked to release textures.
static size_t textureMemoryLowLimitBytes = 3 * 1024 * 1024;

#ifndef NDEBUG
bool LayerRendererChromium::s_inPaintLayerContents = false;
#endif

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

// Returns true if the matrix has no rotation, skew or perspective components to it.
static bool isScaleOrTranslation(const TransformationMatrix& m)
{
    return !m.m12() && !m.m13() && !m.m14()
           && !m.m21() && !m.m23() && !m.m24()
           && !m.m31() && !m.m32() && !m.m43()
           && m.m44();

}

PassRefPtr<LayerRendererChromium> LayerRendererChromium::create(CCLayerTreeHostClient* client, PassOwnPtr<LayerPainterChromium> contentPaint, bool accelerateDrawing)
{
    RefPtr<GraphicsContext3D> context = client->createLayerTreeHostContext3D();
    if (!context)
        return 0;

    RefPtr<LayerRendererChromium> layerRenderer(adoptRef(new LayerRendererChromium(client, context, contentPaint, accelerateDrawing)));
    layerRenderer->init();
    if (!layerRenderer->hardwareCompositing())
        return 0;

    return layerRenderer.release();
}

LayerRendererChromium::LayerRendererChromium(CCLayerTreeHostClient* client,
                                             PassRefPtr<GraphicsContext3D> context,
                                             PassOwnPtr<LayerPainterChromium> contentPaint,
                                             bool accelerateDrawing)
    : CCLayerTreeHost(client)
    , m_viewportScrollPosition(IntPoint(-1, -1))
    , m_rootLayer(0)
    , m_accelerateDrawing(false)
    , m_currentRenderSurface(0)
    , m_offscreenFramebufferId(0)
    , m_compositeOffscreen(false)
    , m_context(context)
    , m_contextSupportsTextureFormatBGRA(false)
    , m_contextSupportsReadFormatBGRA(false)
    , m_animating(false)
    , m_defaultRenderSurface(0)
{
    WebCore::Extensions3D* extensions = m_context->getExtensions();
    m_contextSupportsMapSub = extensions->supports("GL_CHROMIUM_map_sub");
    if (m_contextSupportsMapSub)
        extensions->ensureEnabled("GL_CHROMIUM_map_sub");
    m_contextSupportsTextureFormatBGRA = extensions->supports("GL_EXT_texture_format_BGRA8888");
    if (m_contextSupportsTextureFormatBGRA)
        extensions->ensureEnabled("GL_EXT_texture_format_BGRA8888");
    m_contextSupportsReadFormatBGRA = extensions->supports("GL_EXT_read_format_bgra");
    if (m_contextSupportsReadFormatBGRA)
        extensions->ensureEnabled("GL_EXT_read_format_bgra");

#if USE(SKIA)
    m_accelerateDrawing = accelerateDrawing && skiaContext();
#endif
    m_hardwareCompositing = initializeSharedObjects();

#if USE(SKIA)
    if (m_accelerateDrawing)
        m_rootLayerTextureUpdater = LayerTextureUpdaterSkPicture::create(m_context.get(), contentPaint, skiaContext());
    else
#endif
        m_rootLayerTextureUpdater = LayerTextureUpdaterBitmap::create(m_context.get(), contentPaint, m_contextSupportsMapSub);

    m_rootLayerContentTiler = LayerTilerChromium::create(this, IntSize(256, 256), LayerTilerChromium::NoBorderTexels);
    ASSERT(m_rootLayerContentTiler);

    m_headsUpDisplay = CCHeadsUpDisplay::create(this);
}

LayerRendererChromium::~LayerRendererChromium()
{
    m_headsUpDisplay.clear(); // Explicitly destroy the HUD before the TextureManager dies.
    cleanupSharedObjects();
}

GraphicsContext3D* LayerRendererChromium::context()
{
    return m_context.get();
}

#if USE(SKIA)
GrContext* LayerRendererChromium::skiaContext()
{
    if (!m_skiaContext && m_contextSupportsTextureFormatBGRA && m_contextSupportsReadFormatBGRA) {
        m_context->makeContextCurrent();
        m_skiaContext = adoptPtr(GrContext::CreateGLShaderContext());
        // Limit the number of textures we hold in the bitmap->texture cache.
        static const int maxTextureCacheCount = 512;
        // Limit the bytes allocated toward textures in the bitmap->texture cache.
        static const size_t maxTextureCacheBytes = 50 * 1024 * 1024;
        m_skiaContext->setTextureCacheLimits(maxTextureCacheCount, maxTextureCacheBytes);
    }
    return m_skiaContext.get();
}
#endif

void LayerRendererChromium::debugGLCall(GraphicsContext3D* context, const char* command, const char* file, int line)
{
    unsigned long error = context->getError();
    if (error != GraphicsContext3D::NO_ERROR)
        LOG_ERROR("GL command failed: File: %s\n\tLine %d\n\tcommand: %s, error %x\n", file, line, command, static_cast<int>(error));
}

void LayerRendererChromium::invalidateRootLayerRect(const IntRect& dirtyRect)
{
    m_rootLayerContentTiler->invalidateRect(dirtyRect);
}

void LayerRendererChromium::releaseTextures()
{
    // Reduces texture memory usage to textureMemoryLowLimitBytes by deleting non root layer
    // textures.
    m_rootLayerContentTiler->protectTileTextures(m_viewportVisibleRect);
    m_contentsTextureManager->reduceMemoryToLimit(textureMemoryLowLimitBytes);
    m_contentsTextureManager->unprotectAllTextures();
    // Evict all RenderSurface textures.
    m_renderSurfaceTextureManager->unprotectAllTextures();
    m_renderSurfaceTextureManager->reduceMemoryToLimit(0);
}

void LayerRendererChromium::updateRootLayerContents()
{
    TRACE_EVENT("LayerRendererChromium::updateRootLayerContents", this, 0);
    m_rootLayerContentTiler->prepareToUpdate(m_viewportVisibleRect, m_rootLayerTextureUpdater.get());
}

void LayerRendererChromium::drawRootLayer()
{
    TransformationMatrix scroll;
    scroll.translate(-m_viewportVisibleRect.x(), -m_viewportVisibleRect.y());

    m_rootLayerContentTiler->draw(m_viewportVisibleRect, scroll, 1.0f);
}

void LayerRendererChromium::setViewport(const IntRect& visibleRect, const IntRect& contentRect, const IntPoint& scrollPosition)
{
    bool visibleRectChanged = m_viewportVisibleRect.size() != visibleRect.size();

    m_viewportVisibleRect = visibleRect;
    m_viewportContentRect = contentRect;
    m_viewportScrollPosition = scrollPosition;

    if (visibleRectChanged) {
        // Reset the current render surface to force an update of the viewport and
        // projection matrix next time useRenderSurface is called.
        m_currentRenderSurface = 0;
        m_rootLayerContentTiler->invalidateEntireLayer();
    }
    setNeedsCommitAndRedraw();
}

void LayerRendererChromium::updateLayers()
{
    if (m_viewportVisibleRect.isEmpty())
        return;

    // FIXME: use the frame begin time from the overall compositor scheduler.
    // This value is currently inaccessible because it is up in Chromium's
    // RenderWidget.
    m_headsUpDisplay->onFrameBegin(currentTime());
    ASSERT(m_hardwareCompositing);

    if (!m_rootLayer)
        return;

    updateRootLayerContents();

    // Recheck that we still have a root layer. This may become null if
    // compositing gets turned off during a paint operation.
    if (!m_rootLayer)
        return;

    {
        TRACE_EVENT("LayerRendererChromium::synchronizeTrees", this, 0);
        m_rootCCLayerImpl = TreeSynchronizer::synchronizeTrees(m_rootLayer.get(), m_rootCCLayerImpl.get());
    }

    m_computedRenderSurfaceLayerList = adoptPtr(new LayerList());
    updateLayers(*m_computedRenderSurfaceLayerList);
}

void LayerRendererChromium::drawLayers()
{
    ASSERT(m_hardwareCompositing);
    ASSERT(m_computedRenderSurfaceLayerList);
    // Before drawLayers:
    if (hardwareCompositing()) {
        // FIXME: The multithreaded compositor case will not work as long as
        // copyTexImage2D resolves to the parent texture, because the main
        // thread can execute WebGL calls on the child context at any time,
        // potentially clobbering the parent texture that is being renderered
        // by the compositor thread.
        ChildContextMap::iterator i = m_childContexts.begin();
        for (; i != m_childContexts.end(); ++i) {
            i->first->flush();
        }
    }

    m_renderSurfaceTextureManager->setMemoryLimitBytes(textureMemoryHighLimitBytes - m_contentsTextureManager->currentMemoryUseBytes());
    drawLayers(*m_computedRenderSurfaceLayerList);

    m_contentsTextureManager->unprotectAllTextures();
    m_contentsTextureManager->reduceMemoryToLimit(textureMemoryReclaimLimitBytes);

    if (textureMemoryReclaimLimitBytes > m_contentsTextureManager->currentMemoryUseBytes())
        m_renderSurfaceTextureManager->reduceMemoryToLimit(textureMemoryReclaimLimitBytes - m_contentsTextureManager->currentMemoryUseBytes());
    else
        m_renderSurfaceTextureManager->reduceMemoryToLimit(0);

    if (isCompositingOffscreen())
        copyOffscreenTextureToDisplay();
}

void LayerRendererChromium::updateLayers(LayerList& renderSurfaceLayerList)
{
    TRACE_EVENT("LayerRendererChromium::updateLayers", this, 0);
    CCLayerImpl* rootDrawLayer = m_rootLayer->ccLayerImpl();

    if (!rootDrawLayer->renderSurface())
        rootDrawLayer->createRenderSurface();
    ASSERT(rootDrawLayer->renderSurface());

    rootDrawLayer->renderSurface()->setContentRect(IntRect(IntPoint(0, 0), m_viewportVisibleRect.size()));

    IntRect rootScissorRect(m_viewportVisibleRect);
    // The scissorRect should not include the scroll offset.
    rootScissorRect.move(-m_viewportScrollPosition.x(), -m_viewportScrollPosition.y());
    rootDrawLayer->setScissorRect(rootScissorRect);

    m_defaultRenderSurface = rootDrawLayer->renderSurface();

    renderSurfaceLayerList.append(rootDrawLayer);

    TransformationMatrix identityMatrix;
    m_defaultRenderSurface->clearLayerList();
    // Unfortunately, updatePropertiesAndRenderSurfaces() currently both updates the layers and updates the draw state
    // (transforms, etc). It'd be nicer if operations on the presentation layers happened later, but the draw
    // transforms are needed by large layers to determine visibility. Tiling will fix this by eliminating the
    // concept of a large content layer.
    updatePropertiesAndRenderSurfaces(rootDrawLayer, identityMatrix, renderSurfaceLayerList, m_defaultRenderSurface->layerList());

#ifndef NDEBUG
    s_inPaintLayerContents = true;
#endif
    paintLayerContents(renderSurfaceLayerList);
#ifndef NDEBUG
    s_inPaintLayerContents = false;
#endif
    // Update compositor resources for root layer.
    {
        TRACE_EVENT("LayerRendererChromium::updateLayer::updateRoot", this, 0);
        m_rootLayerContentTiler->updateRect(m_rootLayerTextureUpdater.get());
    }

    m_contentsTextureManager->reduceMemoryToLimit(textureMemoryReclaimLimitBytes);
    updateCompositorResources(renderSurfaceLayerList);
}

static IntRect calculateVisibleRect(const IntRect& targetSurfaceRect, const IntRect& layerBoundRect, const TransformationMatrix& transform)
{
    // Is this layer fully contained within the target surface?
    IntRect layerInSurfaceSpace = transform.mapRect(layerBoundRect);
    if (targetSurfaceRect.contains(layerInSurfaceSpace))
        return layerBoundRect;

    // If the layer doesn't fill up the entire surface, then find the part of
    // the surface rect where the layer could be visible. This avoids trying to
    // project surface rect points that are behind the projection point.
    IntRect minimalSurfaceRect = targetSurfaceRect;
    minimalSurfaceRect.intersect(layerInSurfaceSpace);

    // Project the corners of the target surface rect into the layer space.
    // This bounding rectangle may be larger than it needs to be (being
    // axis-aligned), but is a reasonable filter on the space to consider.
    // Non-invertible transforms will create an empty rect here.
    const TransformationMatrix surfaceToLayer = transform.inverse();
    IntRect layerRect = surfaceToLayer.projectQuad(FloatQuad(FloatRect(minimalSurfaceRect))).enclosingBoundingBox();
    layerRect.intersect(layerBoundRect);
    return layerRect;
}

static IntRect calculateVisibleLayerRect(const IntRect& targetSurfaceRect, const IntSize& bounds, const IntSize& contentBounds, const TransformationMatrix& tilingTransform)
{
    if (targetSurfaceRect.isEmpty() || contentBounds.isEmpty())
        return targetSurfaceRect;

    const IntRect layerBoundRect = IntRect(IntPoint(), contentBounds);
    TransformationMatrix transform = tilingTransform;

    transform.scaleNonUniform(bounds.width() / static_cast<double>(contentBounds.width()),
                              bounds.height() / static_cast<double>(contentBounds.height()));
    transform.translate(-contentBounds.width() / 2.0, -contentBounds.height() / 2.0);

    return calculateVisibleRect(targetSurfaceRect, layerBoundRect, transform);
}

static void paintContentsIfDirty(LayerChromium* layer, const IntRect& visibleLayerRect)
{
    if (layer->drawsContent()) {
        layer->setVisibleLayerRect(visibleLayerRect);
        layer->paintContentsIfDirty();
    }
}

void LayerRendererChromium::paintLayerContents(const LayerList& renderSurfaceLayerList)
{
    for (int surfaceIndex = renderSurfaceLayerList.size() - 1; surfaceIndex >= 0 ; --surfaceIndex) {
        CCLayerImpl* renderSurfaceLayer = renderSurfaceLayerList[surfaceIndex].get();
        RenderSurfaceChromium* renderSurface = renderSurfaceLayer->renderSurface();
        ASSERT(renderSurface);

        // Make sure any renderSurfaceLayer is associated with this layerRenderer.
        // This is a defensive assignment in case the owner of this layer hasn't
        // set the layerRenderer on this layer already.
        renderSurfaceLayer->setLayerRenderer(this);

        // Render surfaces whose drawable area has zero width or height
        // will have no layers associated with them and should be skipped.
        if (!renderSurface->layerList().size())
            continue;

        if (!renderSurface->drawOpacity())
            continue;

        LayerList& layerList = renderSurface->layerList();
        ASSERT(layerList.size());
        for (unsigned layerIndex = 0; layerIndex < layerList.size(); ++layerIndex) {
            CCLayerImpl* ccLayerImpl = layerList[layerIndex].get();

            // Layers that start a new render surface will be painted when the render
            // surface's list is processed.
            if (ccLayerImpl->renderSurface() && ccLayerImpl->renderSurface() != renderSurface)
                continue;

            LayerChromium* layer = ccLayerImpl->owner();

            layer->setLayerRenderer(this);

            if (!layer->opacity())
                continue;

            if (layer->maskLayer())
                layer->maskLayer()->setLayerRenderer(this);
            if (layer->replicaLayer()) {
                layer->replicaLayer()->setLayerRenderer(this);
                if (layer->replicaLayer()->maskLayer())
                    layer->replicaLayer()->maskLayer()->setLayerRenderer(this);
            }

            if (layer->bounds().isEmpty())
                continue;

            IntRect targetSurfaceRect = ccLayerImpl->targetRenderSurface() ? ccLayerImpl->targetRenderSurface()->contentRect() : m_defaultRenderSurface->contentRect();
            if (layer->ccLayerImpl()->usesLayerScissor())
                targetSurfaceRect.intersect(layer->ccLayerImpl()->scissorRect());
            IntRect visibleLayerRect = calculateVisibleLayerRect(targetSurfaceRect, layer->bounds(), layer->contentBounds(), ccLayerImpl->drawTransform());

            paintContentsIfDirty(layer, visibleLayerRect);

            if (LayerChromium* maskLayer = layer->maskLayer()) {
                paintContentsIfDirty(maskLayer, IntRect(IntPoint(), maskLayer->contentBounds()));
            }

            if (LayerChromium* replicaLayer = layer->replicaLayer()) {
                paintContentsIfDirty(replicaLayer, visibleLayerRect);

                if (LayerChromium* replicaMaskLayer = replicaLayer->maskLayer()) {
                    paintContentsIfDirty(replicaMaskLayer, IntRect(IntPoint(), replicaMaskLayer->contentBounds()));
                }
            }
        }
    }
}

void LayerRendererChromium::drawLayers(const LayerList& renderSurfaceLayerList)
{
    if (m_viewportVisibleRect.isEmpty() || !m_rootLayer)
        return;

    TRACE_EVENT("LayerRendererChromium::drawLayers", this, 0);
    CCLayerImpl* rootDrawLayer = m_rootLayer->ccLayerImpl();
    makeContextCurrent();

    // The GL viewport covers the entire visible area, including the scrollbars.
    GLC(m_context.get(), m_context->viewport(0, 0, m_viewportVisibleRect.width(), m_viewportVisibleRect.height()));

    // Bind the common vertex attributes used for drawing all the layers.
    m_sharedGeometry->prepareForDraw();

    // FIXME: These calls can be made once, when the compositor context is initialized.
    GLC(m_context.get(), m_context->disable(GraphicsContext3D::DEPTH_TEST));
    GLC(m_context.get(), m_context->disable(GraphicsContext3D::CULL_FACE));

    // Blending disabled by default. Root layer alpha channel on Windows is incorrect when Skia uses ClearType.
    GLC(m_context.get(), m_context->disable(GraphicsContext3D::BLEND));

    useRenderSurface(m_defaultRenderSurface);

    // Clear to blue to make it easier to spot unrendered regions.
    m_context->clearColor(0, 0, 1, 1);
    m_context->colorMask(true, true, true, true);
    m_context->clear(GraphicsContext3D::COLOR_BUFFER_BIT);
    // Mask out writes to alpha channel: subpixel antialiasing via Skia results in invalid
    // zero alpha values on text glyphs. The root layer is always opaque.
    m_context->colorMask(true, true, true, false);

    drawRootLayer();

    // Re-enable color writes to layers, which may be partially transparent.
    m_context->colorMask(true, true, true, true);

    GLC(m_context.get(), m_context->enable(GraphicsContext3D::BLEND));
    GLC(m_context.get(), m_context->blendFunc(GraphicsContext3D::ONE, GraphicsContext3D::ONE_MINUS_SRC_ALPHA));
    GLC(m_context.get(), m_context->enable(GraphicsContext3D::SCISSOR_TEST));

    // Update the contents of the render surfaces. We traverse the array from
    // back to front to guarantee that nested render surfaces get rendered in the
    // correct order.
    for (int surfaceIndex = renderSurfaceLayerList.size() - 1; surfaceIndex >= 0 ; --surfaceIndex) {
        CCLayerImpl* renderSurfaceLayer = renderSurfaceLayerList[surfaceIndex].get();
        RenderSurfaceChromium* renderSurface = renderSurfaceLayer->renderSurface();
        ASSERT(renderSurface);

        renderSurface->setSkipsDraw(true);

        // Render surfaces whose drawable area has zero width or height
        // will have no layers associated with them and should be skipped.
        if (!renderSurface->layerList().size())
            continue;

        // Skip completely transparent render surfaces.
        if (!renderSurface->drawOpacity())
            continue;

        if (useRenderSurface(renderSurface)) {
            renderSurface->setSkipsDraw(false);

            if (renderSurfaceLayer != rootDrawLayer) {
                GLC(m_context.get(), m_context->disable(GraphicsContext3D::SCISSOR_TEST));
                GLC(m_context.get(), m_context->clearColor(0, 0, 0, 0));
                GLC(m_context.get(), m_context->clear(GraphicsContext3D::COLOR_BUFFER_BIT));
                GLC(m_context.get(), m_context->enable(GraphicsContext3D::SCISSOR_TEST));
            }

            LayerList& layerList = renderSurface->layerList();
            for (unsigned layerIndex = 0; layerIndex < layerList.size(); ++layerIndex)
                drawLayer(layerList[layerIndex].get(), renderSurface);
        }
    }

    if (m_headsUpDisplay->enabled()) {
        GLC(m_context.get(), m_context->enable(GraphicsContext3D::BLEND));
        GLC(m_context.get(), m_context->blendFunc(GraphicsContext3D::ONE, GraphicsContext3D::ONE_MINUS_SRC_ALPHA));
        GLC(m_context.get(), m_context->disable(GraphicsContext3D::SCISSOR_TEST));
        useRenderSurface(m_defaultRenderSurface);
        m_headsUpDisplay->draw();
    }

    GLC(m_context.get(), m_context->disable(GraphicsContext3D::SCISSOR_TEST));
    GLC(m_context.get(), m_context->disable(GraphicsContext3D::BLEND));
}

void LayerRendererChromium::finish()
{
    TRACE_EVENT("LayerRendererChromium::finish", this, 0);
    m_context->finish();
}

void LayerRendererChromium::present()
{
    TRACE_EVENT("LayerRendererChromium::present", this, 0);
    // We're done! Time to swapbuffers!

    // Note that currently this has the same effect as swapBuffers; we should
    // consider exposing a different entry point on GraphicsContext3D.
    m_context->prepareTexture();

    m_headsUpDisplay->onPresent();
}

void LayerRendererChromium::setRootLayer(PassRefPtr<LayerChromium> layer)
{
    m_rootLayer = layer;
    if (m_rootLayer)
        m_rootLayer->setLayerRenderer(this);
    m_rootLayerContentTiler->invalidateEntireLayer();
}

void LayerRendererChromium::setLayerRendererRecursive(LayerChromium* layer)
{
    const Vector<RefPtr<LayerChromium> >& children = layer->children();
    for (size_t i = 0; i < children.size(); ++i)
        setLayerRendererRecursive(children[i].get());

    if (layer->maskLayer())
        setLayerRendererRecursive(layer->maskLayer());
    if (layer->replicaLayer())
        setLayerRendererRecursive(layer->replicaLayer());

    layer->setLayerRenderer(this);
}

void LayerRendererChromium::transferRootLayer(LayerRendererChromium* other)
{
    other->setLayerRendererRecursive(m_rootLayer.get());
    other->m_rootLayer = m_rootLayer.release();
}

void LayerRendererChromium::getFramebufferPixels(void *pixels, const IntRect& rect)
{
    ASSERT(rect.maxX() <= m_viewportVisibleRect.width() && rect.maxY() <= m_viewportVisibleRect.height());

    if (!pixels)
        return;

    makeContextCurrent();

    GLC(m_context.get(), m_context->readPixels(rect.x(), rect.y(), rect.width(), rect.height(),
                                         GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, pixels));
}

// FIXME: This method should eventually be replaced by a proper texture manager.
unsigned LayerRendererChromium::createLayerTexture()
{
    unsigned textureId = 0;
    GLC(m_context.get(), textureId = m_context->createTexture());
    GLC(m_context.get(), m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, textureId));
    // Do basic linear filtering on resize.
    GLC(m_context.get(), m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR));
    GLC(m_context.get(), m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR));
    // NPOT textures in GL ES only work when the wrap mode is set to GraphicsContext3D::CLAMP_TO_EDGE.
    GLC(m_context.get(), m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE));
    GLC(m_context.get(), m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE));
    return textureId;
}

void LayerRendererChromium::deleteLayerTexture(unsigned textureId)
{
    if (!textureId)
        return;

    GLC(m_context.get(), m_context->deleteTexture(textureId));
}

// Returns true if any part of the layer falls within the visibleRect
bool LayerRendererChromium::isLayerVisible(LayerChromium* layer, const TransformationMatrix& matrix, const IntRect& visibleRect)
{
    // Form the matrix used by the shader to map the corners of the layer's
    // bounds into clip space.
    TransformationMatrix renderMatrix = matrix;
    renderMatrix.scale3d(layer->bounds().width(), layer->bounds().height(), 1);
    renderMatrix = m_projectionMatrix * renderMatrix;

    FloatRect layerRect(-0.5, -0.5, 1, 1);
    FloatRect mappedRect = renderMatrix.mapRect(layerRect);

    // The layer is visible if it intersects any part of a rectangle whose origin
    // is at (-1, -1) and size is 2x2.
    return mappedRect.intersects(FloatRect(-1, -1, 2, 2));
}

// Recursively walks the layer tree starting at the given node and computes all the
// necessary transformations, scissor rectangles, render surfaces, etc.
void LayerRendererChromium::updatePropertiesAndRenderSurfaces(CCLayerImpl* layer, const TransformationMatrix& parentMatrix, LayerList& renderSurfaceLayerList, LayerList& layerList)
{
    // Compute the new matrix transformation that will be applied to this layer and
    // all its children. It's important to remember that the layer's position
    // is the position of the layer's anchor point. Also, the coordinate system used
    // assumes that the origin is at the lower left even though the coordinates the browser
    // gives us for the layers are for the upper left corner. The Y flip happens via
    // the orthographic projection applied at render time.
    // The transformation chain for the layer is (using the Matrix x Vector order):
    // M = M[p] * Tr[l] * M[l] * Tr[c]
    // Where M[p] is the parent matrix passed down to the function
    //       Tr[l] is the translation matrix locating the layer's anchor point
    //       Tr[c] is the translation offset between the anchor point and the center of the layer
    //       M[l] is the layer's matrix (applied at the anchor point)
    // This transform creates a coordinate system whose origin is the center of the layer.
    // Note that the final matrix used by the shader for the layer is P * M * S . This final product
    // is computed in drawTexturedQuad().
    // Where: P is the projection matrix
    //        M is the layer's matrix computed above
    //        S is the scale adjustment (to scale up to the layer size)
    IntSize bounds = layer->bounds();
    FloatPoint anchorPoint = layer->anchorPoint();
    FloatPoint position = layer->position();

    // Offset between anchor point and the center of the quad.
    float centerOffsetX = (0.5 - anchorPoint.x()) * bounds.width();
    float centerOffsetY = (0.5 - anchorPoint.y()) * bounds.height();

    TransformationMatrix layerLocalTransform;
    // LT = Tr[l]
    layerLocalTransform.translate3d(position.x(), position.y(), layer->anchorPointZ());
    // LT = Tr[l] * M[l]
    layerLocalTransform.multiply(layer->transform());
    // LT = Tr[l] * M[l] * Tr[c]
    layerLocalTransform.translate3d(centerOffsetX, centerOffsetY, -layer->anchorPointZ());

    TransformationMatrix combinedTransform = parentMatrix;
    combinedTransform = combinedTransform.multiply(layerLocalTransform);

    FloatRect layerRect(-0.5 * layer->bounds().width(), -0.5 * layer->bounds().height(), layer->bounds().width(), layer->bounds().height());
    IntRect transformedLayerRect;

    // FIXME: This seems like the wrong place to set this
    layer->setUsesLayerScissor(false);

    // The layer and its descendants render on a new RenderSurface if any of
    // these conditions hold:
    // 1. The layer clips its descendants and its transform is not a simple translation.
    // 2. If the layer has opacity != 1 and does not have a preserves-3d transform style.
    // 3. The layer uses a mask
    // 4. The layer has a replica (used for reflections)
    // 5. The layer doesn't preserve-3d but is the child of a layer which does.
    // If a layer preserves-3d then we don't create a RenderSurface for it to avoid flattening
    // out its children. The opacity value of the children layers is multiplied by the opacity
    // of their parent.
    bool useSurfaceForClipping = layer->masksToBounds() && !isScaleOrTranslation(combinedTransform);
    bool useSurfaceForOpacity = layer->opacity() != 1 && !layer->preserves3D();
    bool useSurfaceForMasking = layer->maskLayer();
    bool useSurfaceForReflection = layer->replicaLayer();
    bool useSurfaceForFlatDescendants = layer->parent() && layer->parent()->preserves3D() && !layer->preserves3D() && layer->descendantsDrawsContent();
    if (useSurfaceForMasking || useSurfaceForReflection || useSurfaceForFlatDescendants || ((useSurfaceForClipping || useSurfaceForOpacity) && layer->descendantsDrawsContent())) {
        RenderSurfaceChromium* renderSurface = layer->renderSurface();
        if (!renderSurface)
            renderSurface = layer->createRenderSurface();

        // The origin of the new surface is the upper left corner of the layer.
        TransformationMatrix drawTransform;
        drawTransform.translate3d(0.5 * bounds.width(), 0.5 * bounds.height(), 0);
        layer->setDrawTransform(drawTransform);

        transformedLayerRect = IntRect(0, 0, bounds.width(), bounds.height());

        // Layer's opacity will be applied when drawing the render surface.
        float drawOpacity = layer->opacity();
        if (layer->parent() && layer->parent()->preserves3D())
            drawOpacity *= layer->parent()->drawOpacity();
        renderSurface->setDrawOpacity(drawOpacity);
        layer->setDrawOpacity(1);

        TransformationMatrix layerOriginTransform = combinedTransform;
        layerOriginTransform.translate3d(-0.5 * bounds.width(), -0.5 * bounds.height(), 0);
        renderSurface->setOriginTransform(layerOriginTransform);

        // The render surface scissor rect is the scissor rect that needs to
        // be applied before drawing the render surface onto its containing
        // surface and is therefore expressed in the parent's coordinate system.
        renderSurface->setScissorRect(layer->parent() ? layer->parent()->scissorRect() : layer->scissorRect());

        renderSurface->clearLayerList();

        if (layer->maskLayer()) {
            renderSurface->setMaskLayer(layer->maskLayer());
            layer->maskLayer()->setTargetRenderSurface(renderSurface);
        } else
            renderSurface->setMaskLayer(0);

        if (layer->replicaLayer() && layer->replicaLayer()->maskLayer())
            layer->replicaLayer()->maskLayer()->setTargetRenderSurface(renderSurface);

        renderSurfaceLayerList.append(layer);
    } else {
        // DT = M[p] * LT
        layer->setDrawTransform(combinedTransform);
        transformedLayerRect = enclosingIntRect(layer->drawTransform().mapRect(layerRect));

        layer->setDrawOpacity(layer->opacity());

        if (layer->parent()) {
            if (layer->parent()->preserves3D())
               layer->setDrawOpacity(layer->drawOpacity() * layer->parent()->drawOpacity());

            // Layers inherit the scissor rect from their parent.
            layer->setScissorRect(layer->parent()->scissorRect());
            if (layer->parent()->usesLayerScissor())
                layer->setUsesLayerScissor(true);

            layer->setTargetRenderSurface(layer->parent()->targetRenderSurface());
        }

        if (layer != m_rootCCLayerImpl.get())
            layer->clearRenderSurface();

        if (layer->masksToBounds()) {
            IntRect scissor = transformedLayerRect;
            if (!layer->scissorRect().isEmpty())
                scissor.intersect(layer->scissorRect());
            layer->setScissorRect(scissor);
            layer->setUsesLayerScissor(true);
        }
    }

    if (layer->renderSurface())
        layer->setTargetRenderSurface(layer->renderSurface());
    else {
        ASSERT(layer->parent());
        layer->setTargetRenderSurface(layer->parent()->targetRenderSurface());
    }

    // drawableContentRect() is always stored in the coordinate system of the
    // RenderSurface the layer draws into.
    if (layer->drawsContent()) {
        IntRect drawableContentRect = transformedLayerRect;
        if (layer->usesLayerScissor())
            drawableContentRect.intersect(layer->scissorRect());
        layer->setDrawableContentRect(drawableContentRect);
    } else
        layer->setDrawableContentRect(IntRect());

    TransformationMatrix sublayerMatrix = layer->drawTransform();

    // Flatten to 2D if the layer doesn't preserve 3D.
    if (!layer->preserves3D()) {
        sublayerMatrix.setM13(0);
        sublayerMatrix.setM23(0);
        sublayerMatrix.setM31(0);
        sublayerMatrix.setM32(0);
        sublayerMatrix.setM33(1);
        sublayerMatrix.setM34(0);
        sublayerMatrix.setM43(0);
    }

    // Apply the sublayer transform at the center of the layer.
    sublayerMatrix.multiply(layer->sublayerTransform());

    // The origin of the children is the top left corner of the layer, not the
    // center. The matrix passed down to the children is therefore:
    // M[s] = M * Tr[-center]
    sublayerMatrix.translate3d(-bounds.width() * 0.5, -bounds.height() * 0.5, 0);

    LayerList& descendants = (layer->renderSurface() ? layer->renderSurface()->layerList() : layerList);
    descendants.append(layer);

    unsigned thisLayerIndex = descendants.size() - 1;

    for (size_t i = 0; i < layer->children().size(); ++i) {
        CCLayerImpl* child = layer->children()[i].get();
        updatePropertiesAndRenderSurfaces(child, sublayerMatrix, renderSurfaceLayerList, descendants);

        if (child->renderSurface()) {
            RenderSurfaceChromium* childRenderSurface = child->renderSurface();
            IntRect drawableContentRect = layer->drawableContentRect();
            drawableContentRect.unite(enclosingIntRect(childRenderSurface->drawableContentRect()));
            layer->setDrawableContentRect(drawableContentRect);
            descendants.append(child);
        } else {
            IntRect drawableContentRect = layer->drawableContentRect();
            drawableContentRect.unite(child->drawableContentRect());
            layer->setDrawableContentRect(drawableContentRect);
        }
    }

    if (layer->masksToBounds() || useSurfaceForMasking) {
        IntRect drawableContentRect = layer->drawableContentRect();
        drawableContentRect.intersect(transformedLayerRect);
        layer->setDrawableContentRect(drawableContentRect);
    }

    if (layer->renderSurface() && layer != m_rootCCLayerImpl.get()) {
        RenderSurfaceChromium* renderSurface = layer->renderSurface();
        IntRect renderSurfaceContentRect = layer->drawableContentRect();
        FloatPoint surfaceCenter = FloatRect(renderSurfaceContentRect).center();

        // Restrict the RenderSurface size to the portion that's visible.
        FloatSize centerOffsetDueToClipping;

        // Don't clip if the layer is reflected as the reflection shouldn't be
        // clipped.
        if (!layer->replicaLayer()) {
            if (!renderSurface->scissorRect().isEmpty() && !renderSurfaceContentRect.isEmpty()) {
                IntRect surfaceScissorRect = calculateVisibleRect(renderSurface->scissorRect(), renderSurfaceContentRect, renderSurface->originTransform());
                renderSurfaceContentRect.intersect(surfaceScissorRect);
            }
            FloatPoint clippedSurfaceCenter = FloatRect(renderSurfaceContentRect).center();
            centerOffsetDueToClipping = clippedSurfaceCenter - surfaceCenter;
        }

        // The RenderSurface backing texture cannot exceed the maximum supported
        // texture size.
        renderSurfaceContentRect.setWidth(std::min(renderSurfaceContentRect.width(), m_maxTextureSize));
        renderSurfaceContentRect.setHeight(std::min(renderSurfaceContentRect.height(), m_maxTextureSize));

        if (renderSurfaceContentRect.isEmpty())
            renderSurface->clearLayerList();
        renderSurface->setContentRect(renderSurfaceContentRect);

        // Since the layer starts a new render surface we need to adjust its
        // scissor rect to be expressed in the new surface's coordinate system.
        layer->setScissorRect(layer->drawableContentRect());

        // Adjust the origin of the transform to be the center of the render surface.
        TransformationMatrix drawTransform = renderSurface->originTransform();
        drawTransform.translate3d(surfaceCenter.x() + centerOffsetDueToClipping.width(), surfaceCenter.y() + centerOffsetDueToClipping.height(), 0);
        renderSurface->setDrawTransform(drawTransform);

        // Compute the transformation matrix used to draw the replica of the render
        // surface.
        if (layer->replicaLayer()) {
            TransformationMatrix replicaDrawTransform = renderSurface->originTransform();
            replicaDrawTransform.translate3d(layer->replicaLayer()->position().x(), layer->replicaLayer()->position().y(), 0);
            replicaDrawTransform.multiply(layer->replicaLayer()->transform());
            replicaDrawTransform.translate3d(surfaceCenter.x() - anchorPoint.x() * bounds.width(), surfaceCenter.y() - anchorPoint.y() * bounds.height(), 0);
            renderSurface->setReplicaDrawTransform(replicaDrawTransform);
        }
    }

    // If preserves-3d then sort all the descendants in 3D so that they can be 
    // drawn from back to front. If the preserves-3d property is also set on the parent then
    // skip the sorting as the parent will sort all the descendants anyway.
    if (layer->preserves3D() && (!layer->parent() || !layer->parent()->preserves3D()))
        m_layerSorter.sort(&descendants.at(thisLayerIndex), descendants.end());
}

void LayerRendererChromium::updateCompositorResources(const LayerList& renderSurfaceLayerList)
{
    for (int surfaceIndex = renderSurfaceLayerList.size() - 1; surfaceIndex >= 0 ; --surfaceIndex) {
        CCLayerImpl* renderSurfaceLayer = renderSurfaceLayerList[surfaceIndex].get();
        RenderSurfaceChromium* renderSurface = renderSurfaceLayer->renderSurface();
        ASSERT(renderSurface);

        if (!renderSurface->layerList().size() || !renderSurface->drawOpacity())
            continue;

        const LayerList& layerList = renderSurface->layerList();
        ASSERT(layerList.size());
        for (unsigned layerIndex = 0; layerIndex < layerList.size(); ++layerIndex) {
            CCLayerImpl* ccLayerImpl = layerList[layerIndex].get();
            if (ccLayerImpl->renderSurface() && ccLayerImpl->renderSurface() != renderSurface)
                continue;

            updateCompositorResources(ccLayerImpl);
        }
    }
}

void LayerRendererChromium::updateCompositorResources(CCLayerImpl* ccLayerImpl)
{
    LayerChromium* layer = ccLayerImpl->owner();

    if (layer->bounds().isEmpty())
        return;

    if (!layer->opacity())
        return;

    if (layer->maskLayer())
        updateCompositorResources(ccLayerImpl->maskLayer());
    if (layer->replicaLayer())
        updateCompositorResources(ccLayerImpl->replicaLayer());

    if (layer->drawsContent())
        layer->updateCompositorResources();

    layer->pushPropertiesTo(ccLayerImpl);
}

void LayerRendererChromium::setCompositeOffscreen(bool compositeOffscreen)
{
    if (m_compositeOffscreen == compositeOffscreen)
       return;

    m_compositeOffscreen = compositeOffscreen;

    if (!m_compositeOffscreen && m_rootLayer)
        m_rootLayer->ccLayerImpl()->clearRenderSurface();
}

LayerTexture* LayerRendererChromium::getOffscreenLayerTexture()
{
    return m_compositeOffscreen ? m_rootLayer->ccLayerImpl()->renderSurface()->contentsTexture() : 0;
}

void LayerRendererChromium::copyOffscreenTextureToDisplay()
{
    if (m_compositeOffscreen) {
        makeContextCurrent();

        useRenderSurface(0);
        TransformationMatrix drawTransform;
        drawTransform.translate3d(0.5 * m_defaultRenderSurface->contentRect().width(), 0.5 * m_defaultRenderSurface->contentRect().height(), 0);
        m_defaultRenderSurface->setDrawTransform(drawTransform);
        m_defaultRenderSurface->setDrawOpacity(1);
        m_defaultRenderSurface->draw(m_defaultRenderSurface->contentRect());
    }
}

bool LayerRendererChromium::useRenderSurface(RenderSurfaceChromium* renderSurface)
{
    if (m_currentRenderSurface == renderSurface)
        return true;

    m_currentRenderSurface = renderSurface;

    if ((renderSurface == m_defaultRenderSurface && !m_compositeOffscreen) || (!renderSurface && m_compositeOffscreen)) {
        GLC(m_context.get(), m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, 0));
        if (renderSurface)
            setDrawViewportRect(renderSurface->contentRect(), true);
        else
            setDrawViewportRect(m_defaultRenderSurface->contentRect(), true);
        return true;
    }

    GLC(m_context.get(), m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_offscreenFramebufferId));

    if (!renderSurface->prepareContentsTexture())
        return false;

    renderSurface->contentsTexture()->framebufferTexture2D();

#if !defined ( NDEBUG )
    if (m_context->checkFramebufferStatus(GraphicsContext3D::FRAMEBUFFER) != GraphicsContext3D::FRAMEBUFFER_COMPLETE) {
        ASSERT_NOT_REACHED();
        return false;
    }
#endif

    setDrawViewportRect(renderSurface->contentRect(), false);
    return true;
}

void LayerRendererChromium::drawLayer(CCLayerImpl* layer, RenderSurfaceChromium* targetSurface)
{
    if (layer->renderSurface() && layer->renderSurface() != targetSurface) {
        layer->renderSurface()->draw(layer->getDrawRect());
        layer->renderSurface()->releaseContentsTexture();
        return;
    }

    if (!layer->drawsContent())
        return;

    if (!layer->opacity())
        return;

    if (layer->bounds().isEmpty())
        return;

    IntRect targetSurfaceRect = layer->targetRenderSurface() ? layer->targetRenderSurface()->contentRect() : m_defaultRenderSurface->contentRect();
    if (layer->usesLayerScissor()) {
        IntRect scissorRect = layer->scissorRect();
        targetSurfaceRect.intersect(scissorRect);
        if (targetSurfaceRect.isEmpty())
            return;
        setScissorToRect(scissorRect);
    } else
        GLC(m_context.get(), m_context->disable(GraphicsContext3D::SCISSOR_TEST));


    if (!layer->doubleSided()) {
        // FIXME: Need to take into account the cumulative render surface transforms all the way from
        //        the default render surface in order to determine visibility.
        TransformationMatrix combinedDrawMatrix;
        if (layer->targetRenderSurface()) {
            combinedDrawMatrix = layer->targetRenderSurface()->drawTransform();
            combinedDrawMatrix.multiply(layer->drawTransform());
        } else
            combinedDrawMatrix = layer->drawTransform();

        FloatRect layerRect(FloatPoint(0, 0), FloatSize(layer->bounds()));
        FloatQuad mappedLayer = combinedDrawMatrix.mapQuad(FloatQuad(layerRect));
        FloatSize horizontalDir = mappedLayer.p2() - mappedLayer.p1();
        FloatSize verticalDir = mappedLayer.p4() - mappedLayer.p1();
        FloatPoint3D xAxis(horizontalDir.width(), horizontalDir.height(), 0);
        FloatPoint3D yAxis(verticalDir.width(), verticalDir.height(), 0);
        FloatPoint3D zAxis = xAxis.cross(yAxis);
        if (zAxis.z() < 0)
            return;
    }

    layer->draw();

    // Draw the debug border if there is one.
    layer->drawDebugBorder();
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
    if (m_currentRenderSurface == m_defaultRenderSurface && !m_compositeOffscreen)
        scissorY = m_currentRenderSurface->contentRect().height() - (scissorRect.maxY() - m_currentRenderSurface->contentRect().y());
    else
        scissorY = scissorRect.y() - contentRect.y();
    GLC(m_context.get(), m_context->scissor(scissorX, scissorY, scissorRect.width(), scissorRect.height()));
}

bool LayerRendererChromium::makeContextCurrent()
{
    m_context->makeContextCurrent();
    return true;
}

// Checks whether a given size is within the maximum allowed texture size range.
bool LayerRendererChromium::checkTextureSize(const IntSize& textureSize)
{
    if (textureSize.width() > m_maxTextureSize || textureSize.height() > m_maxTextureSize)
        return false;
    return true;
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
}



void LayerRendererChromium::resizeOnscreenContent(const IntSize& size)
{
    if (m_context)
        m_context->reshape(size.width(), size.height());
}

bool LayerRendererChromium::initializeSharedObjects()
{
    TRACE_EVENT("LayerRendererChromium::initializeSharedObjects", this, 0);
    makeContextCurrent();

    // Get the max texture size supported by the system.
    m_maxTextureSize = 0;
    GLC(m_context.get(), m_context->getIntegerv(GraphicsContext3D::MAX_TEXTURE_SIZE, &m_maxTextureSize));

    // Create an FBO for doing offscreen rendering.
    GLC(m_context.get(), m_offscreenFramebufferId = m_context->createFramebuffer());

    // We will always need these programs to render, so create the programs eagerly so that the shader compilation can
    // start while we do other work. Other programs are created lazily on first access.
    m_sharedGeometry = adoptPtr(new GeometryBinding(m_context.get()));
    m_renderSurfaceProgram = adoptPtr(new RenderSurfaceChromium::Program(m_context.get()));
    m_tilerProgram = adoptPtr(new LayerTilerChromium::Program(m_context.get()));

    GLC(m_context.get(), m_context->flush());

    m_contentsTextureManager = TextureManager::create(m_context.get(), textureMemoryHighLimitBytes, m_maxTextureSize);
    m_renderSurfaceTextureManager = TextureManager::create(m_context.get(), textureMemoryHighLimitBytes, m_maxTextureSize);
    return true;
}

const LayerChromium::BorderProgram* LayerRendererChromium::borderProgram()
{
    if (!m_borderProgram)
        m_borderProgram = adoptPtr(new LayerChromium::BorderProgram(m_context.get()));
    if (!m_borderProgram->initialized()) {
        TRACE_EVENT("LayerRendererChromium::borderProgram::initialize", this, 0);
        m_borderProgram->initialize();
    }
    return m_borderProgram.get();
}

const CCHeadsUpDisplay::Program* LayerRendererChromium::headsUpDisplayProgram()
{
    if (!m_headsUpDisplayProgram)
        m_headsUpDisplayProgram = adoptPtr(new CCHeadsUpDisplay::Program(m_context.get()));
    if (!m_headsUpDisplayProgram->initialized()) {
        TRACE_EVENT("LayerRendererChromium::borderProgram::initialize", this, 0);
        m_headsUpDisplayProgram->initialize();
    }
    return m_headsUpDisplayProgram.get();
}

const RenderSurfaceChromium::Program* LayerRendererChromium::renderSurfaceProgram()
{
    ASSERT(m_renderSurfaceProgram);
    if (!m_renderSurfaceProgram->initialized()) {
        TRACE_EVENT("LayerRendererChromium::borderProgram::initialize", this, 0);
        m_renderSurfaceProgram->initialize();
    }
    return m_renderSurfaceProgram.get();
}

const RenderSurfaceChromium::MaskProgram* LayerRendererChromium::renderSurfaceMaskProgram()
{
    if (!m_renderSurfaceMaskProgram)
        m_renderSurfaceMaskProgram = adoptPtr(new RenderSurfaceChromium::MaskProgram(m_context.get()));
    if (!m_renderSurfaceMaskProgram->initialized()) {
        TRACE_EVENT("LayerRendererChromium::borderProgram::initialize", this, 0);
        m_renderSurfaceMaskProgram->initialize();
    }
    return m_renderSurfaceMaskProgram.get();
}

const LayerTilerChromium::Program* LayerRendererChromium::tilerProgram()
{
    ASSERT(m_tilerProgram);
    if (!m_tilerProgram->initialized()) {
        TRACE_EVENT("LayerRendererChromium::tilerProgram::initialize", this, 0);
        m_tilerProgram->initialize();
    }
    return m_tilerProgram.get();
}

const LayerTilerChromium::ProgramSwizzle* LayerRendererChromium::tilerProgramSwizzle()
{
    if (!m_tilerProgramSwizzle)
        m_tilerProgramSwizzle = adoptPtr(new LayerTilerChromium::ProgramSwizzle(m_context.get()));
    if (!m_tilerProgramSwizzle->initialized()) {
        TRACE_EVENT("LayerRendererChromium::tilerProgramSwizzle::initialize", this, 0);
        m_tilerProgramSwizzle->initialize();
    }
    return m_tilerProgramSwizzle.get();
}

const CCCanvasLayerImpl::Program* LayerRendererChromium::canvasLayerProgram()
{
    if (!m_canvasLayerProgram)
        m_canvasLayerProgram = adoptPtr(new CCCanvasLayerImpl::Program(m_context.get()));
    if (!m_canvasLayerProgram->initialized()) {
        TRACE_EVENT("LayerRendererChromium::borderProgram::initialize", this, 0);
        m_canvasLayerProgram->initialize();
    }
    return m_canvasLayerProgram.get();
}

const CCPluginLayerImpl::Program* LayerRendererChromium::pluginLayerProgram()
{
    if (!m_pluginLayerProgram)
        m_pluginLayerProgram = adoptPtr(new CCPluginLayerImpl::Program(m_context.get()));
    if (!m_pluginLayerProgram->initialized()) {
        TRACE_EVENT("LayerRendererChromium::borderProgram::initialize", this, 0);
        m_pluginLayerProgram->initialize();
    }
    return m_pluginLayerProgram.get();
}

const CCVideoLayerImpl::RGBAProgram* LayerRendererChromium::videoLayerRGBAProgram()
{
    if (!m_videoLayerRGBAProgram)
        m_videoLayerRGBAProgram = adoptPtr(new CCVideoLayerImpl::RGBAProgram(m_context.get()));
    if (!m_videoLayerRGBAProgram->initialized()) {
        TRACE_EVENT("LayerRendererChromium::borderProgram::initialize", this, 0);
        m_videoLayerRGBAProgram->initialize();
    }
    return m_videoLayerRGBAProgram.get();
}

const CCVideoLayerImpl::YUVProgram* LayerRendererChromium::videoLayerYUVProgram()
{
    if (!m_videoLayerYUVProgram)
        m_videoLayerYUVProgram = adoptPtr(new CCVideoLayerImpl::YUVProgram(m_context.get()));
    if (!m_videoLayerYUVProgram->initialized()) {
        TRACE_EVENT("LayerRendererChromium::borderProgram::initialize", this, 0);
        m_videoLayerYUVProgram->initialize();
    }
    return m_videoLayerYUVProgram.get();
}


void LayerRendererChromium::cleanupSharedObjects()
{
    makeContextCurrent();

    m_sharedGeometry.clear();
    m_borderProgram.clear();
    m_canvasLayerProgram.clear();
    m_headsUpDisplayProgram.clear();
    m_videoLayerRGBAProgram.clear();
    m_videoLayerYUVProgram.clear();
    m_pluginLayerProgram.clear();
    m_renderSurfaceProgram.clear();
    m_renderSurfaceMaskProgram.clear();
    m_tilerProgram.clear();
    m_tilerProgramSwizzle.clear();
    if (m_offscreenFramebufferId)
        GLC(m_context.get(), m_context->deleteFramebuffer(m_offscreenFramebufferId));

    // Clear tilers before the texture manager, as they have references to textures.
    m_rootLayerContentTiler.clear();

    m_contentsTextureManager.clear();
    m_renderSurfaceTextureManager.clear();
}

String LayerRendererChromium::layerTreeAsText() const
{
    TextStream ts;
    if (m_rootLayer.get()) {
        ts << m_rootLayer->layerTreeAsText();
        ts << "RenderSurfaces:\n";
        dumpRenderSurfaces(ts, 1, m_rootLayer.get());
    }
    return ts.release();
}

void LayerRendererChromium::addChildContext(GraphicsContext3D* ctx)
{
    // This is a ref-counting map, because some contexts are shared by multiple
    // layers (specifically, Canvas2DLayerChromium).

    // Insert the ctx with a count of 1, or return the existing iterator.
    std::pair<ChildContextMap::iterator, bool> insert_result = m_childContexts.add(ctx, 1);
    if (!insert_result.second) {
        // Already present in map, so increment.
        ++insert_result.first->second;
    } else {
// FIXME(jbates): when compositor is multithreaded and copyTexImage2D bug is fixed,
// uncomment this block:
//      // This is a new child context - set the parentToChild latch so that it
//      // can continue past its first wait latch.
//      Extensions3DChromium* ext = static_cast<Extensions3DChromium*>(ctx->getExtensions());
//      GC3Duint latchId;
//      ext->getParentToChildLatchCHROMIUM(&latchId);
//      ext->setLatchCHROMIUM(0, latchId);
    }
}

void LayerRendererChromium::removeChildContext(GraphicsContext3D* ctx)
{
    ChildContextMap::iterator i = m_childContexts.find(ctx);
    if (i != m_childContexts.end()) {
        if (--i->second <= 0) {
            // Count reached zero, so remove from map.
            m_childContexts.remove(i);
        }
    } else {
        // error
        ASSERT(0 && "m_childContexts map has mismatched add/remove calls");
    }
}

bool LayerRendererChromium::isCompositorContextLost()
{
    return (m_context.get()->getExtensions()->getGraphicsResetStatusARB() != GraphicsContext3D::NO_ERROR);
}

void LayerRendererChromium::dumpRenderSurfaces(TextStream& ts, int indent, LayerChromium* layer) const
{
    if (layer->ccLayerImpl()->renderSurface())
        layer->ccLayerImpl()->renderSurface()->dumpSurface(ts, indent);

    for (size_t i = 0; i < layer->children().size(); ++i)
        dumpRenderSurfaces(ts, indent, layer->children()[i].get());
}

class LayerRendererChromiumImpl : public CCLayerTreeHostImpl {
public:
    static PassOwnPtr<LayerRendererChromiumImpl> create(CCLayerTreeHostImplClient* client, LayerRendererChromium* layerRenderer)
    {
        return adoptPtr(new LayerRendererChromiumImpl(client, layerRenderer));
    }

    virtual void drawLayersAndPresent()
    {
        CCCompletionEvent completion;
        bool contextLost;
        CCMainThread::postTask(createMainThreadTask(this, &LayerRendererChromiumImpl::drawLayersOnMainThread, AllowCrossThreadAccess(&completion), AllowCrossThreadAccess(&contextLost)));
        completion.wait();

        // FIXME: Send the "UpdateRect" message up to the RenderWidget [or moveplugin equivalents...]

        // FIXME: handle context lost
        if (contextLost)
            FATAL("LayerRendererChromiumImpl does not handle context lost yet.");
    }

private:
    LayerRendererChromiumImpl(CCLayerTreeHostImplClient* client, LayerRendererChromium* layerRenderer)
        : CCLayerTreeHostImpl(client)
        , m_layerRenderer(layerRenderer) { }

    void drawLayersOnMainThread(CCCompletionEvent* completion, bool* contextLost)
    {
        ASSERT(isMainThread());

        if (m_layerRenderer->rootLayer()) {
            m_layerRenderer->drawLayers();
            m_layerRenderer->present();

            GraphicsContext3D* context = m_layerRenderer->context();
            *contextLost = context->getExtensions()->getGraphicsResetStatusARB() != GraphicsContext3D::NO_ERROR;
        } else
            *contextLost = false;
        completion->signal();
    }

    LayerRendererChromium* m_layerRenderer;
};

class LayerRendererChromiumImplProxy : public CCLayerTreeHostImplProxy {
public:
    static PassOwnPtr<LayerRendererChromiumImplProxy> create(LayerRendererChromium* layerRenderer)
    {
        return adoptPtr(new LayerRendererChromiumImplProxy(layerRenderer));
    }

    virtual PassOwnPtr<CCLayerTreeHostImpl> createLayerTreeHostImpl()
    {
        return LayerRendererChromiumImpl::create(this, static_cast<LayerRendererChromium*>(host()));
    }

private:
    LayerRendererChromiumImplProxy(LayerRendererChromium* layerRenderer)
        : CCLayerTreeHostImplProxy(layerRenderer) { }
};

PassOwnPtr<CCLayerTreeHostImplProxy> LayerRendererChromium::createLayerTreeHostImplProxy()
{
    OwnPtr<CCLayerTreeHostImplProxy> proxy = LayerRendererChromiumImplProxy::create(this);
    proxy->start();
    return proxy.release();
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
