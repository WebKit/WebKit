/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LayerTreeHostGtk.h"

#if USE(TEXTURE_MAPPER_GL)

#include "AcceleratedSurface.h"
#include "DrawingAreaImpl.h"
#include "TextureMapperGL.h"
#include "WebPage.h"
#include "WebProcess.h"

#if USE(OPENGL_ES_2)
#include <GLES2/gl2.h>
#else
#include <GL/gl.h>
#endif

#include <WebCore/FrameView.h>
#include <WebCore/GLContext.h>
#include <WebCore/GraphicsLayerTextureMapper.h>
#include <WebCore/MainFrame.h>
#include <WebCore/Page.h>
#include <WebCore/Settings.h>
#include <gdk/gdk.h>
#include <wtf/CurrentTime.h>

using namespace WebCore;

namespace WebKit {

LayerTreeHostGtk::RenderFrameScheduler::RenderFrameScheduler(std::function<bool()> renderer)
    : m_renderer(WTFMove(renderer))
    , m_timer(RunLoop::main(), this, &LayerTreeHostGtk::RenderFrameScheduler::renderFrame)
{
    // We use a RunLoop timer because otherwise GTK+ event handling during dragging can starve WebCore timers, which have a lower priority.
    // Use a higher priority than WebCore timers.
    m_timer.setPriority(GDK_PRIORITY_REDRAW - 1);
}

LayerTreeHostGtk::RenderFrameScheduler::~RenderFrameScheduler()
{
}

void LayerTreeHostGtk::RenderFrameScheduler::start()
{
    if (m_timer.isActive())
        return;
    m_fireTime = 0;
    nextFrame();
}

void LayerTreeHostGtk::RenderFrameScheduler::stop()
{
    m_timer.stop();
}

static inline bool shouldSkipNextFrameBecauseOfContinousImmediateFlushes(double current, double lastImmediateFlushTime)
{
    // 100ms is about a perceptable delay in UI, so when scheduling layer flushes immediately for more than 100ms,
    // we skip the next frame to ensure pending timers have a change to be fired.
    static const double maxDurationOfImmediateFlushes = 0.100;
    if (!lastImmediateFlushTime)
        return false;
    return lastImmediateFlushTime + maxDurationOfImmediateFlushes < current;
}

void LayerTreeHostGtk::RenderFrameScheduler::nextFrame()
{
    static const double targetFramerate = 1 / 60.0;
    // When rendering layers takes more time than the target delay (0.016), we end up scheduling layer flushes
    // immediately. Since the layer flush timer has a higher priority than WebCore timers, these are never
    // fired while we keep scheduling layer flushes immediately.
    double current = monotonicallyIncreasingTime();
    double timeToNextFlush = std::max(targetFramerate - (current - m_fireTime), 0.0);
    if (timeToNextFlush)
        m_lastImmediateFlushTime = 0;
    else if (!m_lastImmediateFlushTime)
        m_lastImmediateFlushTime = current;

    if (shouldSkipNextFrameBecauseOfContinousImmediateFlushes(current, m_lastImmediateFlushTime)) {
        timeToNextFlush = targetFramerate;
        m_lastImmediateFlushTime = 0;
    }

    m_timer.startOneShot(timeToNextFlush);
}

void LayerTreeHostGtk::RenderFrameScheduler::renderFrame()
{
    m_fireTime = monotonicallyIncreasingTime();
    if (!m_renderer() || m_timer.isActive())
        return;
    nextFrame();
}

Ref<LayerTreeHostGtk> LayerTreeHostGtk::create(WebPage& webPage)
{
    return adoptRef(*new LayerTreeHostGtk(webPage));
}

LayerTreeHostGtk::LayerTreeHostGtk(WebPage& webPage)
    : LayerTreeHost(webPage)
    , m_surface(AcceleratedSurface::create(webPage))
    , m_renderFrameScheduler(std::bind(&LayerTreeHostGtk::renderFrame, this))
{
    m_rootLayer = GraphicsLayer::create(graphicsLayerFactory(), *this);
    m_rootLayer->setDrawsContent(false);
    m_rootLayer->setSize(m_webPage.size());

    m_scaleMatrix.makeIdentity();
    m_scaleMatrix.scale(m_webPage.deviceScaleFactor() * m_webPage.pageScaleFactor());
    downcast<GraphicsLayerTextureMapper>(*m_rootLayer).layer().setAnchorPoint(FloatPoint3D());
    downcast<GraphicsLayerTextureMapper>(*m_rootLayer).layer().setTransform(m_scaleMatrix);

    // The non-composited contents are a child of the root layer.
    m_nonCompositedContentLayer = GraphicsLayer::create(graphicsLayerFactory(), *this);
    m_nonCompositedContentLayer->setDrawsContent(true);
    m_nonCompositedContentLayer->setContentsOpaque(m_webPage.drawsBackground());
    m_nonCompositedContentLayer->setSize(m_webPage.size());
    if (m_webPage.corePage()->settings().acceleratedDrawingEnabled())
        m_nonCompositedContentLayer->setAcceleratesDrawing(true);

#ifndef NDEBUG
    m_rootLayer->setName("LayerTreeHost root layer");
    m_nonCompositedContentLayer->setName("LayerTreeHost non-composited content");
#endif

    m_rootLayer->addChild(m_nonCompositedContentLayer.get());
    m_nonCompositedContentLayer->setNeedsDisplay();

    if (m_surface) {
        createTextureMapper();
        m_layerTreeContext.contextID = m_surface->surfaceID();
    }
}

bool LayerTreeHostGtk::makeContextCurrent()
{
    uint64_t nativeHandle = m_surface ? m_surface->window() : m_layerTreeContext.contextID;
    if (!nativeHandle) {
        m_context = nullptr;
        return false;
    }

    if (m_context)
        return m_context->makeContextCurrent();

    m_context = GLContext::createContextForWindow(reinterpret_cast<GLNativeWindowType>(nativeHandle), &PlatformDisplay::sharedDisplayForCompositing());
    if (!m_context)
        return false;

    if (!m_context->makeContextCurrent())
        return false;

    // Do not do frame sync when rendering offscreen in the web process to ensure that SwapBuffers never blocks.
    // Rendering to the actual screen will happen later anyway since the UI process schedules a redraw for every update,
    // the compositor will take care of syncing to vblank.
    if (m_surface)
        m_context->swapInterval(0);

    return true;
}

LayerTreeHostGtk::~LayerTreeHostGtk()
{
    ASSERT(!m_rootLayer);
    cancelPendingLayerFlush();
}

void LayerTreeHostGtk::setRootCompositingLayer(GraphicsLayer* graphicsLayer)
{
    m_nonCompositedContentLayer->removeAllChildren();

    // Add the accelerated layer tree hierarchy.
    if (graphicsLayer)
        m_nonCompositedContentLayer->addChild(graphicsLayer);

    scheduleLayerFlush();
}

void LayerTreeHostGtk::invalidate()
{
    // This can trigger destruction of GL objects so let's make sure that
    // we have the right active context
    if (m_context)
        m_context->makeContextCurrent();

    cancelPendingLayerFlush();
    m_rootLayer = nullptr;
    m_nonCompositedContentLayer = nullptr;
    m_textureMapper = nullptr;

    m_context = nullptr;
    LayerTreeHost::invalidate();

    m_surface = nullptr;
}

void LayerTreeHostGtk::setNonCompositedContentsNeedDisplay()
{
    m_nonCompositedContentLayer->setNeedsDisplay();
    scheduleLayerFlush();
}

void LayerTreeHostGtk::setNonCompositedContentsNeedDisplayInRect(const IntRect& rect)
{
    m_nonCompositedContentLayer->setNeedsDisplayInRect(rect);
    scheduleLayerFlush();
}

void LayerTreeHostGtk::scrollNonCompositedContents(const IntRect& scrollRect)
{
    setNonCompositedContentsNeedDisplayInRect(scrollRect);
}

void LayerTreeHostGtk::sizeDidChange(const IntSize& newSize)
{
    if (m_rootLayer->size() == newSize)
        return;
    m_rootLayer->setSize(newSize);

    // If the newSize exposes new areas of the non-composited content a setNeedsDisplay is needed
    // for those newly exposed areas.
    FloatSize oldSize = m_nonCompositedContentLayer->size();
    m_nonCompositedContentLayer->setSize(newSize);

    if (newSize.width() > oldSize.width()) {
        float height = std::min(static_cast<float>(newSize.height()), oldSize.height());
        m_nonCompositedContentLayer->setNeedsDisplayInRect(FloatRect(oldSize.width(), 0, newSize.width() - oldSize.width(), height));
    }

    if (newSize.height() > oldSize.height())
        m_nonCompositedContentLayer->setNeedsDisplayInRect(FloatRect(0, oldSize.height(), newSize.width(), newSize.height() - oldSize.height()));
    m_nonCompositedContentLayer->setNeedsDisplay();

    if (m_surface && m_surface->resize(newSize))
        m_layerTreeContext.contextID = m_surface->surfaceID();

    compositeLayersToContext(ForResize);
}

void LayerTreeHostGtk::deviceOrPageScaleFactorChanged()
{
    if (m_surface && m_surface->resize(m_webPage.size()))
        m_layerTreeContext.contextID = m_surface->surfaceID();

    // Other layers learn of the scale factor change via WebPage::setDeviceScaleFactor.
    m_nonCompositedContentLayer->deviceOrPageScaleFactorChanged();

    m_scaleMatrix.makeIdentity();
    m_scaleMatrix.scale(m_webPage.deviceScaleFactor() * m_webPage.pageScaleFactor());
    downcast<GraphicsLayerTextureMapper>(*m_rootLayer).layer().setTransform(m_scaleMatrix);
}

void LayerTreeHostGtk::forceRepaint()
{
    scheduleLayerFlush();
}

void LayerTreeHostGtk::paintContents(const GraphicsLayer* graphicsLayer, GraphicsContext& graphicsContext, GraphicsLayerPaintingPhase, const FloatRect& clipRect)
{
    if (graphicsLayer == m_nonCompositedContentLayer.get())
        m_webPage.drawRect(graphicsContext, enclosingIntRect(clipRect));
}

float LayerTreeHostGtk::deviceScaleFactor() const
{
    return m_webPage.deviceScaleFactor();
}

float LayerTreeHostGtk::pageScaleFactor() const
{
    return m_webPage.pageScaleFactor();
}

bool LayerTreeHostGtk::renderFrame()
{
    flushAndRenderLayers();
    return downcast<GraphicsLayerTextureMapper>(*m_rootLayer).layer().descendantsOrSelfHaveRunningAnimations();
}

bool LayerTreeHostGtk::flushPendingLayerChanges()
{
    m_rootLayer->flushCompositingStateForThisLayerOnly();
    m_nonCompositedContentLayer->flushCompositingStateForThisLayerOnly();

    if (!m_webPage.corePage()->mainFrame().view()->flushCompositingStateIncludingSubframes())
        return false;

    if (m_viewOverlayRootLayer)
        m_viewOverlayRootLayer->flushCompositingState(FloatRect(FloatPoint(), m_rootLayer->size()));

    downcast<GraphicsLayerTextureMapper>(*m_rootLayer).updateBackingStoreIncludingSubLayers();
    return true;
}

void LayerTreeHostGtk::compositeLayersToContext(CompositePurpose purpose)
{
    if (!makeContextCurrent())
        return;

    // The window size may be out of sync with the page size at this point, and getting
    // the viewport parameters incorrect, means that the content will be misplaced. Thus
    // we set the viewport parameters directly from the window size.
    IntSize contextSize = m_context->defaultFrameBufferSize();
    glViewport(0, 0, contextSize.width(), contextSize.height());
    if (purpose == ForResize || !m_webPage.drawsBackground()) {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    ASSERT(m_textureMapper);

    TextureMapper::PaintFlags paintFlags = 0;

    if (m_surface && m_surface->shouldPaintMirrored())
        paintFlags |= TextureMapper::PaintingMirrored;

    m_textureMapper->beginPainting(paintFlags);
    downcast<GraphicsLayerTextureMapper>(*m_rootLayer).layer().paint();
    m_textureMapper->endPainting();

    m_context->swapBuffers();
}

void LayerTreeHostGtk::flushAndRenderLayers()
{
    {
        RefPtr<LayerTreeHostGtk> protect(this);
        m_webPage.layoutIfNeeded();

        if (!m_isValid)
            return;
    }

    if (!makeContextCurrent())
        return;

    if (!flushPendingLayerChanges())
        return;

    // Our model is very simple. We always composite and render the tree immediately after updating it.
    compositeLayersToContext();

    if (m_notifyAfterScheduledLayerFlush) {
        // Let the drawing area know that we've done a flush of the layer changes.
        m_webPage.drawingArea()->layerHostDidFlushLayers();
        m_notifyAfterScheduledLayerFlush = false;
    }
}

void LayerTreeHostGtk::scheduleLayerFlush()
{
    if (!m_layerFlushSchedulingEnabled || !m_textureMapper)
        return;

    m_renderFrameScheduler.start();
}

void LayerTreeHostGtk::pageBackgroundTransparencyChanged()
{
    m_nonCompositedContentLayer->setContentsOpaque(m_webPage.drawsBackground());
}

void LayerTreeHostGtk::cancelPendingLayerFlush()
{
    m_renderFrameScheduler.stop();
}

void LayerTreeHostGtk::setViewOverlayRootLayer(GraphicsLayer* viewOverlayRootLayer)
{
    LayerTreeHost::setViewOverlayRootLayer(viewOverlayRootLayer);
    if (m_viewOverlayRootLayer)
        m_rootLayer->addChild(m_viewOverlayRootLayer);
}

void LayerTreeHostGtk::createTextureMapper()
{
    // The creation of the TextureMapper needs an active OpenGL context.
    if (!makeContextCurrent())
        return;

    ASSERT(m_isValid);
    ASSERT(!m_textureMapper);
    m_textureMapper = TextureMapper::create();
    static_cast<TextureMapperGL*>(m_textureMapper.get())->setEnableEdgeDistanceAntialiasing(true);
    downcast<GraphicsLayerTextureMapper>(*m_rootLayer).layer().setTextureMapper(m_textureMapper.get());
}

#if PLATFORM(X11) && !USE(REDIRECTED_XCOMPOSITE_WINDOW)
void LayerTreeHostGtk::setNativeSurfaceHandleForCompositing(uint64_t handle)
{
    cancelPendingLayerFlush();
    m_layerTreeContext.contextID = handle;

    createTextureMapper();
    scheduleLayerFlush();
}
#endif

} // namespace WebKit

#endif
