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
#include "LayerTexture.h"
#include "NotImplemented.h"
#include "TextStream.h"
#include "TextureManager.h"
#include "TraceEvent.h"
#include "WebGLLayerChromium.h"
#include "cc/CCLayerImpl.h"
#if USE(SKIA)
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#elif USE(CG)
#include <CoreGraphics/CGBitmapContext.h>
#endif

namespace WebCore {

// FIXME: Make this limit adjustable and give it a useful value.
static size_t textureMemoryLimitBytes = 64 * 1024 * 1024;

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

bool LayerRendererChromium::compareLayerZ(const RefPtr<CCLayerImpl>& a, const RefPtr<CCLayerImpl>& b)
{
    return a->drawDepth() < b->drawDepth();
}

PassRefPtr<LayerRendererChromium> LayerRendererChromium::create(PassRefPtr<GraphicsContext3D> context, PassOwnPtr<TilePaintInterface> contentPaint)
{
    if (!context)
        return 0;

    RefPtr<LayerRendererChromium> layerRenderer(adoptRef(new LayerRendererChromium(context, contentPaint)));
    if (!layerRenderer->hardwareCompositing())
        return 0;

    return layerRenderer.release();
}

LayerRendererChromium::LayerRendererChromium(PassRefPtr<GraphicsContext3D> context,
                                             PassOwnPtr<TilePaintInterface> contentPaint)
    : m_viewportScrollPosition(IntPoint(-1, -1))
    , m_rootLayer(0)
    , m_rootLayerContentPaint(contentPaint)
    , m_currentShader(0)
    , m_currentRenderSurface(0)
    , m_offscreenFramebufferId(0)
    , m_compositeOffscreen(false)
    , m_context(context)
    , m_childContextsWereCopied(false)
    , m_contextSupportsLatch(false)
    , m_defaultRenderSurface(0)
{
    m_contextSupportsLatch = m_context->getExtensions()->supports("GL_CHROMIUM_latch");
    m_hardwareCompositing = initializeSharedObjects();
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

void LayerRendererChromium::debugGLCall(GraphicsContext3D* context, const char* command, const char* file, int line)
{
    unsigned long error = context->getError();
    if (error != GraphicsContext3D::NO_ERROR)
        LOG_ERROR("GL command failed: File: %s\n\tLine %d\n\tcommand: %s, error %x\n", file, line, command, static_cast<int>(error));
}

void LayerRendererChromium::useShader(unsigned programId)
{
    if (programId != m_currentShader) {
        GLC(m_context.get(), m_context->useProgram(programId));
        m_currentShader = programId;
    }
}

void LayerRendererChromium::invalidateRootLayerRect(const IntRect& dirtyRect)
{
    m_rootLayerContentTiler->invalidateRect(dirtyRect);
}

void LayerRendererChromium::updateRootLayerContents()
{
    TRACE_EVENT("LayerRendererChromium::updateRootLayerContents", this, 0);
    m_rootLayerContentTiler->update(*m_rootLayerContentPaint, m_viewportVisibleRect);
}

void LayerRendererChromium::drawRootLayer()
{
    TransformationMatrix scroll;
    scroll.translate(-m_viewportVisibleRect.x(), -m_viewportVisibleRect.y());

    m_rootLayerContentTiler->uploadCanvas();
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
}

void LayerRendererChromium::updateAndDrawLayers()
{
    ASSERT(m_hardwareCompositing);

    if (!m_rootLayer)
        return;

    updateRootLayerContents();

    // Recheck that we still have a root layer. This may become null if
    // compositing gets turned off during a paint operation.
    if (!m_rootLayer)
        return;

    LayerList renderSurfaceLayerList;

    updateLayers(renderSurfaceLayerList);

    // Before drawLayers:
    if (hardwareCompositing() && m_contextSupportsLatch) {
        // FIXME: The multithreaded compositor case will not work as long as
        // copyTexImage2D resolves to the parent texture, because the main
        // thread can execute WebGL calls on the child context at any time,
        // potentially clobbering the parent texture that is being renderered
        // by the compositor thread.
        if (m_childContextsWereCopied) {
            Extensions3DChromium* parentExt = static_cast<Extensions3DChromium*>(m_context->getExtensions());
            // For each child context:
            //   glWaitLatch(Offscreen->Compositor);
            ChildContextMap::iterator i = m_childContexts.begin();
            for (; i != m_childContexts.end(); ++i) {
                Extensions3DChromium* childExt = static_cast<Extensions3DChromium*>(i->first->getExtensions());
                GC3Duint latchId;
                childExt->getChildToParentLatchCHROMIUM(&latchId);
                parentExt->waitLatchCHROMIUM(latchId);
            }
        }
        // Reset to false to indicate that we have consumed the dirty child
        // contexts' parent textures. (This is only useful when the compositor
        // is multithreaded.)
        m_childContextsWereCopied = false;
    }

    drawLayers(renderSurfaceLayerList);

    m_textureManager->unprotectAllTextures();

    // After drawLayers:
    if (hardwareCompositing() && m_contextSupportsLatch) {
        Extensions3DChromium* parentExt = static_cast<Extensions3DChromium*>(m_context->getExtensions());
        // For each child context:
        //   glSetLatch(Compositor->Offscreen);
        ChildContextMap::iterator i = m_childContexts.begin();
        for (; i != m_childContexts.end(); ++i) {
            Extensions3DChromium* childExt = static_cast<Extensions3DChromium*>(i->first->getExtensions());
            GC3Duint latchId;
            childExt->getParentToChildLatchCHROMIUM(&latchId);
            parentExt->setLatchCHROMIUM(latchId);
        }
    }

    if (isCompositingOffscreen())
        copyOffscreenTextureToDisplay();
}

void LayerRendererChromium::updateLayers(LayerList& renderSurfaceLayerList)
{
    TRACE_EVENT("LayerRendererChromium::updateLayers", this, 0);
    m_rootLayer->createCCLayerImplIfNeeded();
    CCLayerImpl* rootDrawLayer = m_rootLayer->ccLayerImpl();

    if (!rootDrawLayer->renderSurface())
        rootDrawLayer->createRenderSurface();
    ASSERT(rootDrawLayer->renderSurface());

    rootDrawLayer->renderSurface()->m_contentRect = IntRect(IntPoint(0, 0), m_viewportVisibleRect.size());

    IntRect rootScissorRect(m_viewportVisibleRect);
    // The scissorRect should not include the scroll offset.
    rootScissorRect.move(-m_viewportScrollPosition.x(), -m_viewportScrollPosition.y());
    rootDrawLayer->setScissorRect(rootScissorRect);

    m_defaultRenderSurface = rootDrawLayer->renderSurface();

    renderSurfaceLayerList.append(rootDrawLayer);

    TransformationMatrix identityMatrix;
    m_defaultRenderSurface->m_layerList.clear();
    // Unfortunately, updatePropertiesAndRenderSurfaces() currently both updates the layers and updates the draw state
    // (transforms, etc). It'd be nicer if operations on the presentation layers happened later, but the draw
    // transforms are needed by large layers to determine visibility. Tiling will fix this by eliminating the
    // concept of a large content layer.
    updatePropertiesAndRenderSurfaces(m_rootLayer.get(), identityMatrix, renderSurfaceLayerList, m_defaultRenderSurface->m_layerList);

    paintLayerContents(renderSurfaceLayerList);

    // FIXME: Before updateCompositorResourcesRecursive, when the compositor runs in
    // its own thread, and when the copyTexImage2D bug is fixed, insert
    // a glWaitLatch(Compositor->Offscreen) on all child contexts here instead
    // of after updateCompositorResourcesRecursive.
    // Also uncomment the glSetLatch(Compositor->Offscreen) code in addChildContext.
//  if (hardwareCompositing() && m_contextSupportsLatch) {
//      // For each child context:
//      //   glWaitLatch(Compositor->Offscreen);
//      ChildContextMap::iterator i = m_childContexts.begin();
//      for (; i != m_childContexts.end(); ++i) {
//          Extensions3DChromium* ext = static_cast<Extensions3DChromium*>(i->first->getExtensions());
//          GC3Duint childToParentLatchId, parentToChildLatchId;
//          ext->getParentToChildLatchCHROMIUM(&parentToChildLatchId);
//          ext->waitLatchCHROMIUM(parentToChildLatchId);
//      }
//  }

    updateCompositorResourcesRecursive(m_rootLayer.get());

    // After updateCompositorResourcesRecursive, set/wait latches for all child
    // contexts. This will prevent the compositor from using any of the child
    // parent textures while WebGL commands are executing from javascript *and*
    // while the final parent texture is being blit'd. copyTexImage2D
    // uses the parent texture as a temporary resolve buffer, so that's why the
    // waitLatch is below, to block the compositor from using the parent texture
    // until the next WebGL SwapBuffers (or copyTextureToParentTexture for
    // Canvas2D).
    if (hardwareCompositing() && m_contextSupportsLatch) {
        m_childContextsWereCopied = true;
        // For each child context:
        //   glSetLatch(Offscreen->Compositor);
        //   glWaitLatch(Compositor->Offscreen);
        ChildContextMap::iterator i = m_childContexts.begin();
        for (; i != m_childContexts.end(); ++i) {
            Extensions3DChromium* ext = static_cast<Extensions3DChromium*>(i->first->getExtensions());
            GC3Duint childToParentLatchId, parentToChildLatchId;
            ext->getParentToChildLatchCHROMIUM(&parentToChildLatchId);
            ext->getChildToParentLatchCHROMIUM(&childToParentLatchId);
            ext->setLatchCHROMIUM(childToParentLatchId);
            ext->waitLatchCHROMIUM(parentToChildLatchId);
        }
    }
}

void LayerRendererChromium::paintLayerContents(const LayerList& renderSurfaceLayerList)
{
    for (int surfaceIndex = renderSurfaceLayerList.size() - 1; surfaceIndex >= 0 ; --surfaceIndex) {
        CCLayerImpl* renderSurfaceLayer = renderSurfaceLayerList[surfaceIndex].get();
        RenderSurfaceChromium* renderSurface = renderSurfaceLayer->renderSurface();
        ASSERT(renderSurface);

        // Render surfaces whose drawable area has zero width or height
        // will have no layers associated with them and should be skipped.
        if (!renderSurface->m_layerList.size())
            continue;

        LayerList& layerList = renderSurface->m_layerList;
        ASSERT(layerList.size());
        for (unsigned layerIndex = 0; layerIndex < layerList.size(); ++layerIndex) {
            CCLayerImpl* ccLayerImpl = layerList[layerIndex].get();

            // Layers that start a new render surface will be painted when the render
            // surface's list is processed.
            if (ccLayerImpl->renderSurface() && ccLayerImpl->renderSurface() != renderSurface)
                continue;

            LayerChromium* layer = ccLayerImpl->owner();
            if (layer->bounds().isEmpty())
                continue;

            const IntRect targetSurfaceRect = layer->ccLayerImpl()->scissorRect();

            if (layer->drawsContent())
                layer->paintContentsIfDirty(targetSurfaceRect);
            if (layer->maskLayer() && layer->maskLayer()->drawsContent())
                layer->maskLayer()->paintContentsIfDirty(targetSurfaceRect);
            if (layer->replicaLayer() && layer->replicaLayer()->drawsContent())
                layer->replicaLayer()->paintContentsIfDirty(targetSurfaceRect);
            if (layer->replicaLayer() && layer->replicaLayer()->maskLayer() && layer->replicaLayer()->maskLayer()->drawsContent())
                layer->replicaLayer()->maskLayer()->paintContentsIfDirty(targetSurfaceRect);
        }
    }
}

void LayerRendererChromium::drawLayers(const LayerList& renderSurfaceLayerList)
{
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
        ASSERT(renderSurfaceLayer->renderSurface());

        // Render surfaces whose drawable area has zero width or height
        // will have no layers associated with them and should be skipped.
        if (!renderSurfaceLayer->renderSurface()->m_layerList.size())
            continue;

        if (useRenderSurface(renderSurfaceLayer->renderSurface())) {
            if (renderSurfaceLayer != rootDrawLayer) {
                GLC(m_context.get(), m_context->disable(GraphicsContext3D::SCISSOR_TEST));
                GLC(m_context.get(), m_context->clearColor(0, 0, 0, 0));
                GLC(m_context.get(), m_context->clear(GraphicsContext3D::COLOR_BUFFER_BIT));
                GLC(m_context.get(), m_context->enable(GraphicsContext3D::SCISSOR_TEST));
            }

            LayerList& layerList = renderSurfaceLayer->renderSurface()->m_layerList;
            ASSERT(layerList.size());
            for (unsigned layerIndex = 0; layerIndex < layerList.size(); ++layerIndex)
                drawLayer(layerList[layerIndex].get(), renderSurfaceLayer->renderSurface());
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
void LayerRendererChromium::updatePropertiesAndRenderSurfaces(LayerChromium* layer, const TransformationMatrix& parentMatrix, LayerList& renderSurfaceLayerList, LayerList& layerList)
{
    // Make sure we have CCLayerImpls for this subtree.
    layer->createCCLayerImplIfNeeded();
    layer->setLayerRenderer(this);
    if (layer->maskLayer()) {
        layer->maskLayer()->createCCLayerImplIfNeeded();
        layer->maskLayer()->setLayerRenderer(this);
    }
    if (layer->replicaLayer()) {
        layer->replicaLayer()->createCCLayerImplIfNeeded();
        layer->replicaLayer()->setLayerRenderer(this);
    }
    if (layer->replicaLayer() && layer->replicaLayer()->maskLayer()) {
        layer->replicaLayer()->maskLayer()->createCCLayerImplIfNeeded();
        layer->replicaLayer()->maskLayer()->setLayerRenderer(this);
    }

    CCLayerImpl* drawLayer = layer->ccLayerImpl();
    // Currently we're calling pushPropertiesTo() twice - once here and once in updateCompositorResourcesRecursive().
    // We should only call pushPropertiesTo() in commit, but because we rely on the draw layer state to update
    // RenderSurfaces and we rely on RenderSurfaces being up to date in order to paint contents we have
    // to update the draw layers twice.
    // FIXME: Remove this call once layer updates no longer depend on render surfaces.
    layer->pushPropertiesTo(drawLayer);

    // Compute the new matrix transformation that will be applied to this layer and
    // all its sublayers. It's important to remember that the layer's position
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
    IntSize bounds = drawLayer->bounds();
    FloatPoint anchorPoint = drawLayer->anchorPoint();
    FloatPoint position = drawLayer->position();

    // Offset between anchor point and the center of the quad.
    float centerOffsetX = (0.5 - anchorPoint.x()) * bounds.width();
    float centerOffsetY = (0.5 - anchorPoint.y()) * bounds.height();

    TransformationMatrix layerLocalTransform;
    // LT = Tr[l]
    layerLocalTransform.translate3d(position.x(), position.y(), drawLayer->anchorPointZ());
    // LT = Tr[l] * M[l]
    layerLocalTransform.multiply(drawLayer->transform());
    // LT = Tr[l] * M[l] * Tr[c]
    layerLocalTransform.translate3d(centerOffsetX, centerOffsetY, -drawLayer->anchorPointZ());

    TransformationMatrix combinedTransform = parentMatrix;
    combinedTransform = combinedTransform.multiply(layerLocalTransform);

    FloatRect layerRect(-0.5 * drawLayer->bounds().width(), -0.5 * drawLayer->bounds().height(), drawLayer->bounds().width(), drawLayer->bounds().height());
    IntRect transformedLayerRect;

    // The layer and its descendants render on a new RenderSurface if any of
    // these conditions hold:
    // 1. The layer clips its descendants and its transform is not a simple translation.
    // 2. If the layer has opacity != 1 and does not have a preserves-3d transform style.
    // 3. The layer uses a mask
    // 4. The layer has a replica (used for reflections)
    // If a layer preserves-3d then we don't create a RenderSurface for it to avoid flattening
    // out its children. The opacity value of the children layers is multiplied by the opacity
    // of their parent.
    bool useSurfaceForClipping = drawLayer->masksToBounds() && !isScaleOrTranslation(combinedTransform);
    bool useSurfaceForOpacity = drawLayer->opacity() != 1 && !drawLayer->preserves3D();
    bool useSurfaceForMasking = drawLayer->maskLayer();
    bool useSurfaceForReflection = drawLayer->replicaLayer();
    if (useSurfaceForMasking || useSurfaceForReflection || ((useSurfaceForClipping || useSurfaceForOpacity) && drawLayer->descendantsDrawsContent())) {
        RenderSurfaceChromium* renderSurface = drawLayer->renderSurface();
        if (!renderSurface)
            renderSurface = drawLayer->createRenderSurface();

        // The origin of the new surface is the upper left corner of the layer.
        TransformationMatrix drawTransform;
        drawTransform.translate3d(0.5 * bounds.width(), 0.5 * bounds.height(), 0);
        drawLayer->setDrawTransform(drawTransform);

        transformedLayerRect = IntRect(0, 0, bounds.width(), bounds.height());

        // Layer's opacity will be applied when drawing the render surface.
        renderSurface->m_drawOpacity = drawLayer->opacity();
        if (drawLayer->superlayer() && drawLayer->superlayer()->preserves3D())
            renderSurface->m_drawOpacity *= drawLayer->superlayer()->drawOpacity();
        drawLayer->setDrawOpacity(1);

        TransformationMatrix layerOriginTransform = combinedTransform;
        layerOriginTransform.translate3d(-0.5 * bounds.width(), -0.5 * bounds.height(), 0);
        renderSurface->m_originTransform = layerOriginTransform;
        if (layerOriginTransform.isInvertible() && drawLayer->superlayer()) {
            TransformationMatrix parentToLayer = layerOriginTransform.inverse();

            drawLayer->setScissorRect(parentToLayer.mapRect(drawLayer->superlayer()->scissorRect()));
        } else
            drawLayer->setScissorRect(IntRect());

        // The render surface scissor rect is the scissor rect that needs to
        // be applied before drawing the render surface onto its containing
        // surface and is therefore expressed in the superlayer's coordinate system.
        renderSurface->m_scissorRect = drawLayer->superlayer() ? drawLayer->superlayer()->scissorRect() : drawLayer->scissorRect();

        renderSurface->m_layerList.clear();

        if (drawLayer->maskLayer()) {
            renderSurface->m_maskLayer = drawLayer->maskLayer();
            drawLayer->maskLayer()->setTargetRenderSurface(renderSurface);
        } else
            renderSurface->m_maskLayer = 0;

        if (drawLayer->replicaLayer() && drawLayer->replicaLayer()->maskLayer())
            drawLayer->replicaLayer()->maskLayer()->setTargetRenderSurface(renderSurface);

        renderSurfaceLayerList.append(drawLayer);
    } else {
        // DT = M[p] * LT
        drawLayer->setDrawTransform(combinedTransform);
        transformedLayerRect = enclosingIntRect(drawLayer->drawTransform().mapRect(layerRect));

        drawLayer->setDrawOpacity(drawLayer->opacity());

        if (drawLayer->superlayer()) {
            if (drawLayer->superlayer()->preserves3D())
               drawLayer->setDrawOpacity(drawLayer->drawOpacity() * drawLayer->superlayer()->drawOpacity());

            // Layers inherit the scissor rect from their superlayer.
            drawLayer->setScissorRect(drawLayer->superlayer()->scissorRect());

            drawLayer->setTargetRenderSurface(drawLayer->superlayer()->targetRenderSurface());
        }

        if (layer != m_rootLayer)
            drawLayer->clearRenderSurface();

        if (drawLayer->masksToBounds()) {
            IntRect scissor = drawLayer->scissorRect();
            scissor.intersect(transformedLayerRect);
            drawLayer->setScissorRect(scissor);
        }
    }

    if (drawLayer->renderSurface())
        drawLayer->setTargetRenderSurface(drawLayer->renderSurface());
    else {
        ASSERT(drawLayer->superlayer());
        drawLayer->setTargetRenderSurface(drawLayer->superlayer()->targetRenderSurface());
    }

    // drawableContentRect() is always stored in the coordinate system of the
    // RenderSurface the layer draws into.
    if (drawLayer->drawsContent())
        drawLayer->setDrawableContentRect(transformedLayerRect);
    else
        drawLayer->setDrawableContentRect(IntRect());

    TransformationMatrix sublayerMatrix = drawLayer->drawTransform();

    // Flatten to 2D if the layer doesn't preserve 3D.
    if (!drawLayer->preserves3D()) {
        sublayerMatrix.setM13(0);
        sublayerMatrix.setM23(0);
        sublayerMatrix.setM31(0);
        sublayerMatrix.setM32(0);
        sublayerMatrix.setM33(1);
        sublayerMatrix.setM34(0);
        sublayerMatrix.setM43(0);
    }

    // Apply the sublayer transform at the center of the layer.
    sublayerMatrix.multiply(drawLayer->sublayerTransform());

    // The origin of the sublayers is the top left corner of the layer, not the
    // center. The matrix passed down to the sublayers is therefore:
    // M[s] = M * Tr[-center]
    sublayerMatrix.translate3d(-bounds.width() * 0.5, -bounds.height() * 0.5, 0);

    // Compute the depth value of the center of the layer which will be used when
    // sorting the layers for the preserves-3d property.
    const TransformationMatrix& layerDrawMatrix = drawLayer->renderSurface() ? drawLayer->renderSurface()->m_drawTransform : drawLayer->drawTransform();
    if (drawLayer->superlayer()) {
        if (!drawLayer->superlayer()->preserves3D())
            drawLayer->setDrawDepth(drawLayer->superlayer()->drawDepth());
        else
            drawLayer->setDrawDepth(layerDrawMatrix.m43());
    } else
        drawLayer->setDrawDepth(0);

    LayerList& descendants = (drawLayer->renderSurface() ? drawLayer->renderSurface()->m_layerList : layerList);
    descendants.append(drawLayer);
    unsigned thisLayerIndex = descendants.size() - 1;

    const Vector<RefPtr<LayerChromium> >& sublayers = layer->getSublayers();
    for (size_t i = 0; i < sublayers.size(); ++i) {
        sublayers[i]->createCCLayerImplIfNeeded();
        CCLayerImpl* sublayer = sublayers[i]->ccLayerImpl();
        updatePropertiesAndRenderSurfaces(sublayers[i].get(), sublayerMatrix, renderSurfaceLayerList, descendants);

        if (sublayer->renderSurface()) {
            RenderSurfaceChromium* sublayerRenderSurface = sublayer->renderSurface();
            IntRect drawableContentRect = drawLayer->drawableContentRect();
            drawableContentRect.unite(enclosingIntRect(sublayerRenderSurface->drawableContentRect()));
            drawLayer->setDrawableContentRect(drawableContentRect);
            descendants.append(sublayer);
        } else {
            IntRect drawableContentRect = drawLayer->drawableContentRect();
            drawableContentRect.unite(sublayer->drawableContentRect());
            drawLayer->setDrawableContentRect(drawableContentRect);
        }
    }

    if (drawLayer->masksToBounds() || useSurfaceForMasking) {
        IntRect drawableContentRect = drawLayer->drawableContentRect();
        drawableContentRect.intersect(transformedLayerRect);
        drawLayer->setDrawableContentRect(drawableContentRect);
    }

    if (drawLayer->renderSurface() && layer != m_rootLayer) {
        RenderSurfaceChromium* renderSurface = drawLayer->renderSurface();
        renderSurface->m_contentRect = drawLayer->drawableContentRect();
        FloatPoint surfaceCenter = renderSurface->contentRectCenter();

        // Restrict the RenderSurface size to the portion that's visible.
        FloatSize centerOffsetDueToClipping;

        // Don't clip if the layer is reflected as the reflection shouldn't be
        // clipped.
        if (!drawLayer->replicaLayer()) {
            renderSurface->m_contentRect.intersect(drawLayer->scissorRect());
            FloatPoint clippedSurfaceCenter = renderSurface->contentRectCenter();
            centerOffsetDueToClipping = clippedSurfaceCenter - surfaceCenter;
        }

        // The RenderSurface backing texture cannot exceed the maximum supported
        // texture size.
        renderSurface->m_contentRect.setWidth(std::min(renderSurface->m_contentRect.width(), m_maxTextureSize));
        renderSurface->m_contentRect.setHeight(std::min(renderSurface->m_contentRect.height(), m_maxTextureSize));

        if (renderSurface->m_contentRect.isEmpty())
            renderSurface->m_layerList.clear();

        // Since the layer starts a new render surface we need to adjust its
        // scissor rect to be expressed in the new surface's coordinate system.
        drawLayer->setScissorRect(drawLayer->drawableContentRect());

        // Adjust the origin of the transform to be the center of the render surface.
        renderSurface->m_drawTransform = renderSurface->m_originTransform;
        renderSurface->m_drawTransform.translate3d(surfaceCenter.x() + centerOffsetDueToClipping.width(), surfaceCenter.y() + centerOffsetDueToClipping.height(), 0);

        // Compute the transformation matrix used to draw the replica of the render
        // surface.
        if (drawLayer->replicaLayer()) {
            renderSurface->m_replicaDrawTransform = renderSurface->m_originTransform;
            renderSurface->m_replicaDrawTransform.translate3d(drawLayer->replicaLayer()->position().x(), drawLayer->replicaLayer()->position().y(), 0);
            renderSurface->m_replicaDrawTransform.multiply(drawLayer->replicaLayer()->transform());
            renderSurface->m_replicaDrawTransform.translate3d(surfaceCenter.x() - anchorPoint.x() * bounds.width(), surfaceCenter.y() - anchorPoint.y() * bounds.height(), 0);
        }
    }

    // If preserves-3d then sort all the descendants by the Z coordinate of their
    // center. If the preserves-3d property is also set on the superlayer then
    // skip the sorting as the superlayer will sort all the descendants anyway.
    if (drawLayer->preserves3D() && (!drawLayer->superlayer() || !drawLayer->superlayer()->preserves3D()))
        std::stable_sort(&descendants.at(thisLayerIndex), descendants.end(), compareLayerZ);
}

void LayerRendererChromium::updateCompositorResourcesRecursive(LayerChromium* layer)
{
    const Vector<RefPtr<LayerChromium> >& sublayers = layer->getSublayers();
    for (size_t i = 0; i < sublayers.size(); ++i)
        updateCompositorResourcesRecursive(sublayers[i].get());

    if (layer->bounds().isEmpty())
        return;

    CCLayerImpl* drawLayer = layer->ccLayerImpl();

    if (drawLayer->drawsContent())
        drawLayer->updateCompositorResources();
    if (drawLayer->maskLayer() && drawLayer->maskLayer()->drawsContent())
        drawLayer->maskLayer()->updateCompositorResources();
    if (drawLayer->replicaLayer() && drawLayer->replicaLayer()->drawsContent())
        drawLayer->replicaLayer()->updateCompositorResources();
    if (drawLayer->replicaLayer() && drawLayer->replicaLayer()->maskLayer() && drawLayer->replicaLayer()->maskLayer()->drawsContent())
        drawLayer->replicaLayer()->maskLayer()->updateCompositorResources();

    layer->pushPropertiesTo(drawLayer);
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
    return m_compositeOffscreen ? m_rootLayer->ccLayerImpl()->renderSurface()->m_contentsTexture.get() : 0;
}

void LayerRendererChromium::copyOffscreenTextureToDisplay()
{
    if (m_compositeOffscreen) {
        makeContextCurrent();

        useRenderSurface(0);
        m_defaultRenderSurface->m_drawTransform.makeIdentity();
        m_defaultRenderSurface->m_drawTransform.translate3d(0.5 * m_defaultRenderSurface->m_contentRect.width(),
                                                            0.5 * m_defaultRenderSurface->m_contentRect.height(), 0);
        m_defaultRenderSurface->m_drawOpacity = 1;
        m_defaultRenderSurface->draw(m_defaultRenderSurface->m_contentRect);
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
            setDrawViewportRect(renderSurface->m_contentRect, true);
        else
            setDrawViewportRect(m_defaultRenderSurface->m_contentRect, true);
        return true;
    }

    GLC(m_context.get(), m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_offscreenFramebufferId));

    if (!renderSurface->prepareContentsTexture())
        return false;

    renderSurface->m_contentsTexture->framebufferTexture2D();

#if !defined ( NDEBUG )
    if (m_context->checkFramebufferStatus(GraphicsContext3D::FRAMEBUFFER) != GraphicsContext3D::FRAMEBUFFER_COMPLETE) {
        ASSERT_NOT_REACHED();
        return false;
    }
#endif

    setDrawViewportRect(renderSurface->m_contentRect, false);
    return true;
}

void LayerRendererChromium::drawLayer(CCLayerImpl* layer, RenderSurfaceChromium* targetSurface)
{
    if (layer->renderSurface() && layer->renderSurface() != targetSurface) {
        layer->renderSurface()->draw(layer->getDrawRect());
        return;
    }

    if (!layer->drawsContent())
        return;

    if (layer->bounds().isEmpty()) {
        layer->unreserveContentsTexture();
        return;
    }

    setScissorToRect(layer->scissorRect());

    // Check if the layer falls within the visible bounds of the page.
    IntRect layerRect = layer->getDrawRect();
    bool isLayerVisible = layer->scissorRect().intersects(layerRect);
    if (!isLayerVisible) {
        layer->unreserveContentsTexture();
        return;
    }

    // FIXME: Need to take into account the commulative render surface transforms all the way from
    //        the default render surface in order to determine visibility.
    TransformationMatrix combinedDrawMatrix = (layer->targetRenderSurface() ? layer->targetRenderSurface()->drawTransform().multiply(layer->drawTransform()) : layer->drawTransform());
    
    if (!layer->doubleSided()) {
        FloatRect layerRect(FloatPoint(0, 0), FloatSize(layer->bounds()));
        FloatQuad mappedLayer = combinedDrawMatrix.mapQuad(FloatQuad(layerRect));
        FloatSize horizontalDir = mappedLayer.p2() - mappedLayer.p1();
        FloatSize verticalDir = mappedLayer.p4() - mappedLayer.p1();
        FloatPoint3D xAxis(horizontalDir.width(), horizontalDir.height(), 0);
        FloatPoint3D yAxis(verticalDir.width(), verticalDir.height(), 0);
        FloatPoint3D zAxis = xAxis.cross(yAxis);
        if (zAxis.z() < 0) {
            layer->unreserveContentsTexture();
            return;
        }
    }

    layer->draw(layer->scissorRect());

    // Draw the debug border if there is one.
    layer->drawDebugBorder();
}

// Sets the scissor region to the given rectangle. The coordinate system for the
// scissorRect has its origin at the top left corner of the current visible rect.
void LayerRendererChromium::setScissorToRect(const IntRect& scissorRect)
{
    IntRect contentRect = (m_currentRenderSurface ? m_currentRenderSurface->m_contentRect : m_defaultRenderSurface->m_contentRect);

    // The scissor coordinates must be supplied in viewport space so we need to offset
    // by the relative position of the top left corner of the current render surface.
    int scissorX = scissorRect.x() - contentRect.x();
    // When rendering to the default render surface we're rendering upside down so the top
    // of the GL scissor is the bottom of our layer.
    // But, if rendering to offscreen texture, we reverse our sense of 'upside down'.
    int scissorY;
    if (m_currentRenderSurface == m_defaultRenderSurface && !m_compositeOffscreen)
        scissorY = m_currentRenderSurface->m_contentRect.height() - (scissorRect.maxY() - m_currentRenderSurface->m_contentRect.y());
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
    makeContextCurrent();

    // Get the max texture size supported by the system.
    m_maxTextureSize = 0;
    GLC(m_context.get(), m_context->getIntegerv(GraphicsContext3D::MAX_TEXTURE_SIZE, &m_maxTextureSize));

    // Create an FBO for doing offscreen rendering.
    GLC(m_context.get(), m_offscreenFramebufferId = m_context->createFramebuffer());

    m_sharedGeometry = adoptPtr(new GeometryBinding(m_context.get()));
    m_borderProgram = adoptPtr(new LayerChromium::BorderProgram(m_context.get()));
    m_headsUpDisplayProgram = adoptPtr(new CCHeadsUpDisplay::Program(m_context.get()));
    m_canvasLayerProgram = adoptPtr(new CCCanvasLayerImpl::Program(m_context.get()));
    m_videoLayerRGBAProgram = adoptPtr(new CCVideoLayerImpl::RGBAProgram(m_context.get()));
    m_videoLayerYUVProgram = adoptPtr(new CCVideoLayerImpl::YUVProgram(m_context.get()));
    m_pluginLayerProgram = adoptPtr(new CCPluginLayerImpl::Program(m_context.get()));
    m_renderSurfaceProgram = adoptPtr(new RenderSurfaceChromium::Program(m_context.get()));
    m_renderSurfaceMaskProgram = adoptPtr(new RenderSurfaceChromium::MaskProgram(m_context.get()));
    m_tilerProgram = adoptPtr(new LayerTilerChromium::Program(m_context.get()));

    if (!m_sharedGeometry->initialized() || !m_borderProgram->initialized()
        || !m_canvasLayerProgram->initialized()
        || !m_headsUpDisplayProgram->initialized()
        || !m_videoLayerRGBAProgram->initialized() || !m_videoLayerYUVProgram->initialized()
        || !m_pluginLayerProgram->initialized() || !m_renderSurfaceProgram->initialized()
        || !m_renderSurfaceMaskProgram->initialized() || !m_tilerProgram->initialized()) {
        LOG_ERROR("Compositor failed to initialize shaders. Falling back to software.");
        cleanupSharedObjects();
        return false;
    }

    m_textureManager = TextureManager::create(m_context.get(), textureMemoryLimitBytes, m_maxTextureSize);
    return true;
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
    if (m_offscreenFramebufferId)
        GLC(m_context.get(), m_context->deleteFramebuffer(m_offscreenFramebufferId));

    // Clear tilers before the texture manager, as they have references to textures.
    m_rootLayerContentTiler.clear();

    m_textureManager.clear();
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
    if (!ctx->getExtensions()->supports("GL_CHROMIUM_latch"))
        return;

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
    if (!ctx->getExtensions()->supports("GL_CHROMIUM_latch"))
        return;

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

void LayerRendererChromium::dumpRenderSurfaces(TextStream& ts, int indent, LayerChromium* layer) const
{
    if (layer->ccLayerImpl()->renderSurface())
        layer->ccLayerImpl()->renderSurface()->dumpSurface(ts, indent);

    for (size_t i = 0; i < layer->getSublayers().size(); ++i)
        dumpRenderSurfaces(ts, indent, layer->getSublayers()[i].get());
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
