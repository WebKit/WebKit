/*
 * Copyright (C) 2014 Apple, Inc.
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include "LayerTreeHostTextureMapper.h"

#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)

#include "WebPage.h"
#include <GLES2/gl2.h>
#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsLayerTextureMapper.h>
#include <WebCore/Page.h>
#include <WebCore/Settings.h>
#include <WebCore/TemporaryOpenGLSetting.h>
#include <WebCore/TextureMapperGL.h>
#include <WebCore/TextureMapperLayer.h>

namespace WebKit {
using namespace WebCore;

bool LayerTreeHost::prepareForRendering()
{
    if (!enabled())
        return false;

    if (!m_context)
        return false;

    if (!m_context->makeContextCurrent())
        return false;

    return true;
}

void LayerTreeHost::compositeLayersToContext()
{
    if (!prepareForRendering())
        return;

    IntSize windowSize = expandedIntSize(m_rootLayer->size());
    glViewport(0, 0, windowSize.width(), windowSize.height());

    m_textureMapper->beginPainting();
    downcast<GraphicsLayerTextureMapper>(*m_rootLayer).layer().paint();
    m_fpsCounter.updateFPSAndDisplay(*m_textureMapper);
    m_textureMapper->endPainting();

    m_context->swapBuffers();
}

bool LayerTreeHost::flushPendingLayerChanges()
{
    FrameView* frameView = m_webPage.corePage()->mainFrame().view();
    m_rootLayer->flushCompositingStateForThisLayerOnly();
    if (!frameView->flushCompositingStateIncludingSubframes())
        return false;

    if (m_overlayCompositingLayer)
        m_overlayCompositingLayer->flushCompositingState(FloatRect(FloatPoint(), m_rootLayer->size()));

    downcast<GraphicsLayerTextureMapper>(*m_rootLayer).updateBackingStoreIncludingSubLayers();
    return true;
}

void LayerTreeHost::layerFlushTimerFired()
{
    flushAndRenderLayers();

    if (!enabled())
        return;

    // In case an animation is running, we should flush again soon.
    if (downcast<GraphicsLayerTextureMapper>(m_rootLayer.get())->layer().descendantsOrSelfHaveRunningAnimations())
        m_webPage.corePage()->scheduleTimedRenderingUpdate();
}

LayerTreeHost::LayerTreeHost(WebPage& webPage)
    : m_webPage(webPage)
    , m_layerFlushTimer(*this, &LayerTreeHost::layerFlushTimerFired)
{
    m_rootLayer = GraphicsLayer::create(nullptr, *this);
    m_rootLayer->setDrawsContent(true);
    m_rootLayer->setContentsOpaque(true);
    m_rootLayer->setSize(m_webPage.size());
    m_rootLayer->setNeedsDisplay();
#ifndef NDEBUG
    m_rootLayer->setName("Root layer");
#endif
    applyDeviceScaleFactor();

    // The creation of the TextureMapper needs an active OpenGL context.
    m_context = GLContext::createContextForWindow(window());

    if (!m_context)
        return;

    m_context->makeContextCurrent();

    m_textureMapper = TextureMapperGL::create();
    downcast<GraphicsLayerTextureMapper>(*m_rootLayer).layer().setTextureMapper(m_textureMapper.get());
}

void LayerTreeHost::setLayerFlushSchedulingEnabled(bool)
{
}

void LayerTreeHost::setShouldNotifyAfterNextScheduledLayerFlush(bool)
{
}

void LayerTreeHost::scheduleLayerFlush()
{
    if (!enabled())
        return;

    if (m_layerFlushTimer.isActive())
        return;

    m_layerFlushTimer.startOneShot(0_s);
}

void LayerTreeHost::cancelPendingLayerFlush()
{
    m_layerFlushTimer.stop();
}

void LayerTreeHost::setRootCompositingLayer(WebCore::GraphicsLayer* graphicsLayer)
{
    if (m_rootCompositingLayer == graphicsLayer)
        return;

    m_rootCompositingLayer = graphicsLayer;
    m_rootLayer->removeAllChildren();
    if (m_overlayCompositingLayer)
        m_rootLayer->addChild(*m_overlayCompositingLayer);
    if (m_rootCompositingLayer)
        m_rootLayer->addChild(*m_rootCompositingLayer);
}

void LayerTreeHost::setViewOverlayRootLayer(WebCore::GraphicsLayer* graphicsLayer)
{
    if (m_overlayCompositingLayer == graphicsLayer)
        return;

    m_overlayCompositingLayer = graphicsLayer;
    m_rootLayer->removeAllChildren();
    if (m_overlayCompositingLayer)
        m_rootLayer->addChild(*m_overlayCompositingLayer);
    if (m_rootCompositingLayer)
        m_rootLayer->addChild(*m_rootCompositingLayer);
}

void LayerTreeHost::setNonCompositedContentsNeedDisplay(const WebCore::IntRect& rect)
{
    if (!enabled())
        return;
    m_rootLayer->setNeedsDisplayInRect(rect);
    scheduleLayerFlush();
}

void LayerTreeHost::scrollNonCompositedContents(const WebCore::IntRect& scrollRect)
{
    setNonCompositedContentsNeedDisplay(scrollRect);
}

void LayerTreeHost::flushAndRenderLayers()
{
    if (!enabled())
        return;

    m_webPage.corePage()->updateRendering();

    if (!flushPendingLayerChanges())
        return;

    compositeLayersToContext();
}

void LayerTreeHost::forceRepaint()
{
    flushAndRenderLayers();
}

bool LayerTreeHost::forceRepaintAsync(CallbackID)
{
    return false;
}

void LayerTreeHost::sizeDidChange(const WebCore::IntSize& newSize)
{
    if (!enabled())
        return;

    if (m_rootLayer->size() == newSize)
        return;
    m_rootLayer->setSize(newSize);
    applyDeviceScaleFactor();

    scheduleLayerFlush();
}

void LayerTreeHost::pauseRendering()
{
}

void LayerTreeHost::resumeRendering()
{
}

WebCore::GraphicsLayerFactory* LayerTreeHost::graphicsLayerFactory()
{
    return nullptr;
}

void LayerTreeHost::contentsSizeChanged(const WebCore::IntSize&)
{
}

void LayerTreeHost::didChangeViewportAttributes(WebCore::ViewportAttributes&&)
{
}

void LayerTreeHost::setIsDiscardable(bool)
{
}

void LayerTreeHost::deviceOrPageScaleFactorChanged()
{
}

RefPtr<WebCore::DisplayRefreshMonitor> LayerTreeHost::createDisplayRefreshMonitor(WebCore::PlatformDisplayID)
{
    return nullptr;
}

HWND LayerTreeHost::window()
{
    return reinterpret_cast<HWND>(m_webPage.nativeWindowHandle());
}

bool LayerTreeHost::enabled()
{
    return window() && m_rootCompositingLayer;
}

void LayerTreeHost::paintContents(const GraphicsLayer*, GraphicsContext& context, const FloatRect& rectToPaint, GraphicsLayerPaintBehavior)
{
    context.save();
    context.clip(rectToPaint);
    m_webPage.corePage()->mainFrame().view()->paint(context, enclosingIntRect(rectToPaint));
    context.restore();
}

float LayerTreeHost::deviceScaleFactor() const
{
    return m_webPage.corePage()->deviceScaleFactor();
}

void LayerTreeHost::applyDeviceScaleFactor()
{
    const FloatSize& size = m_rootLayer->size();

    TransformationMatrix m;
    m.scale(deviceScaleFactor());
    // Center view
    double tx = (size.width() - size.width() / deviceScaleFactor()) / 2.0;
    double ty = (size.height() - size.height() / deviceScaleFactor()) / 2.0;
    m.translate(tx, ty);
    m_rootLayer->setTransform(m);
}

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
