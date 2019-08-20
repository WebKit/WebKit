/*
 * Copyright (C) 2014 Apple, Inc.
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

#include "AcceleratedCompositingContext.h"

#if USE(TEXTURE_MAPPER_GL)

#include "WebView.h"

#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsLayerTextureMapper.h>
#include <WebCore/Page.h>
#include <WebCore/Settings.h>
#include <WebCore/SystemInfo.h>
#include <WebCore/TemporaryOpenGLSetting.h>
#include <WebCore/TextureMapperGL.h>
#include <WebCore/TextureMapperLayer.h>

#if USE(OPENGL_ES)
#include <GLES2/gl2.h>
#else
#include <GL/gl.h>
#endif

using namespace WebCore;

AcceleratedCompositingContext::AcceleratedCompositingContext(WebView& webView)
    : m_webView(webView)
    , m_context(nullptr)
    , m_window(0)
    , m_layerFlushTimer(*this)
{
}

static IntSize getWebViewSize(WebView& webView)
{
    RECT r;
    webView.frameRect(&r);
    return IntSize(r.right - r.left, r.bottom - r.top);
}

void AcceleratedCompositingContext::initialize()
{
    if (m_rootLayer)
        return;

    IntSize pageSize = getWebViewSize(m_webView);

    m_window = m_webView.viewWindow();

    if (!m_window)
        return;

    m_rootLayer = GraphicsLayer::create(nullptr, *this);
    m_rootLayer->setDrawsContent(false);
    m_rootLayer->setSize(pageSize);

    applyDeviceScaleFactor();

    // The non-composited contents are a child of the root layer.
    m_nonCompositedContentLayer = GraphicsLayer::create(nullptr, *this);
    m_nonCompositedContentLayer->setDrawsContent(true);
    m_nonCompositedContentLayer->setContentsOpaque(!m_webView.transparent());
    m_nonCompositedContentLayer->setSize(pageSize);
    if (core(&m_webView)->settings().acceleratedDrawingEnabled())
        m_nonCompositedContentLayer->setAcceleratesDrawing(true);

#ifndef NDEBUG
    m_rootLayer->setName("Root layer");
    m_nonCompositedContentLayer->setName("Non-composited content");
#endif

    m_rootLayer->addChild(*m_nonCompositedContentLayer);
    m_nonCompositedContentLayer->setNeedsDisplay();

    // The creation of the TextureMapper needs an active OpenGL context.
    if (!m_context)
        m_context = GLContext::createContextForWindow(m_window);

    if (!m_context)
        return;

    m_context->makeContextCurrent();

    m_textureMapper = TextureMapperGL::create();
    downcast<GraphicsLayerTextureMapper>(*m_rootLayer).layer().setTextureMapper(m_textureMapper.get());

    scheduleLayerFlush();
}

AcceleratedCompositingContext::~AcceleratedCompositingContext()
{
    stopAnyPendingLayerFlush();
}

void AcceleratedCompositingContext::stopAnyPendingLayerFlush()
{
    m_layerFlushTimer.stop();
}

bool AcceleratedCompositingContext::enabled()
{
    return m_window && m_rootLayer && m_textureMapper;
}

bool AcceleratedCompositingContext::prepareForRendering()
{
    if (!enabled())
        return false;

    if (!m_context)
        return false;

    if (!m_context->makeContextCurrent())
        return false;

    return true;
}

bool AcceleratedCompositingContext::startedAnimation(WebCore::GraphicsLayer* layer)
{
    if (!layer)
        return false;

    return downcast<GraphicsLayerTextureMapper>(*layer).layer().descendantsOrSelfHaveRunningAnimations();
}

void AcceleratedCompositingContext::applyDeviceScaleFactor()
{
    if (!m_rootLayer)
        return;

    const FloatSize& size = m_rootLayer->size();

    TransformationMatrix m;
    m.scale(deviceScaleFactor());
    // Center view
    double tx = (size.width() - size.width() / deviceScaleFactor()) / 2.0;
    double ty = (size.height() - size.height() / deviceScaleFactor()) / 2.0;
    m.translate(tx, ty);
    m_rootLayer->setTransform(m);
}

void AcceleratedCompositingContext::compositeLayersToContext(CompositePurpose purpose)
{
    if (!prepareForRendering())
        return;

    RECT r;
    if (!::GetClientRect(m_window, &r))
        return;
    IntSize windowSize(r.right, r.bottom);
    glViewport(0, 0, windowSize.width(), windowSize.height());

    if (purpose == ForResize) {
        TemporaryOpenGLSetting scopedScissor(GL_SCISSOR_TEST, GL_FALSE);
        glClearColor(1, 1, 1, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    m_textureMapper->beginPainting();
    downcast<GraphicsLayerTextureMapper>(*m_rootLayer).layer().paint();
    m_fpsCounter.updateFPSAndDisplay(*m_textureMapper);
    m_textureMapper->endPainting();

    m_context->swapBuffers();
}

void AcceleratedCompositingContext::setRootCompositingLayer(GraphicsLayer* graphicsLayer)
{
    prepareForRendering();

    if (!graphicsLayer) {
        stopAnyPendingLayerFlush();
        m_rootLayer = nullptr;
        m_nonCompositedContentLayer = nullptr;
        m_textureMapper = nullptr;
        return;
    }

    // Add the accelerated layer tree hierarchy.
    initialize();
    if (!m_window)
        return;

    m_nonCompositedContentLayer->removeAllChildren();
    m_nonCompositedContentLayer->addChild(*graphicsLayer);

    stopAnyPendingLayerFlush();

    scheduleLayerFlush();
}

void AcceleratedCompositingContext::setNonCompositedContentsNeedDisplay(const IntRect& rect)
{
    if (!m_rootLayer)
        return;
    if (rect.isEmpty()) {
        m_rootLayer->setNeedsDisplay();
        return;
    }
    m_nonCompositedContentLayer->setNeedsDisplayInRect(rect);
    scheduleLayerFlush();
}

void AcceleratedCompositingContext::resizeRootLayer(const IntSize& newSize)
{
    if (!enabled())
        return;

    if (m_rootLayer->size() == newSize)
        return;

    m_rootLayer->setSize(newSize);

    applyDeviceScaleFactor();

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

    m_nonCompositedContentLayer->setNeedsDisplayInRect(IntRect(IntPoint(), newSize));
    compositeLayersToContext(ForResize);
    scheduleLayerFlush();
}

void AcceleratedCompositingContext::scrollNonCompositedContents(const IntRect& scrollRect, const IntSize& /* scrollOffset */)
{
    m_nonCompositedContentLayer->setNeedsDisplay();
    scheduleLayerFlush();
}

void AcceleratedCompositingContext::scheduleLayerFlush()
{
    if (!enabled())
        return;

    if (m_layerFlushTimer.isActive())
        return;

    m_layerFlushTimer.startOneShot(50_ms);
}

bool AcceleratedCompositingContext::flushPendingLayerChanges()
{
    FrameView* frameView = core(&m_webView)->mainFrame().view();
    m_rootLayer->flushCompositingStateForThisLayerOnly();
    m_nonCompositedContentLayer->flushCompositingStateForThisLayerOnly();
    if (!frameView->flushCompositingStateIncludingSubframes())
        return false;

    downcast<GraphicsLayerTextureMapper>(*m_rootLayer).updateBackingStoreIncludingSubLayers();
    return true;
}

bool AcceleratedCompositingContext::flushPendingLayerChangesSoon()
{
    scheduleLayerFlush();
    return true;
}

void AcceleratedCompositingContext::flushAndRenderLayers()
{
    if (!enabled())
        return;

    Frame& frame = core(&m_webView)->mainFrame();
    if (!frame.contentRenderer() || !frame.view())
        return;

    frame.view()->updateLayoutAndStyleIfNeededRecursive();

    if (!enabled())
        return;

    if (m_context && !m_context->makeContextCurrent())
        return;

    if (!flushPendingLayerChanges())
        return;

    compositeLayersToContext();
}

void AcceleratedCompositingContext::layerFlushTimerFired()
{
    flushAndRenderLayers();

    // In case an animation is running, we should flush again soon.
    if (startedAnimation(m_rootLayer.get()))
        scheduleLayerFlush();
}

void AcceleratedCompositingContext::paintContents(const GraphicsLayer*, GraphicsContext& context, OptionSet<GraphicsLayerPaintingPhase>, const FloatRect& rectToPaint, GraphicsLayerPaintBehavior)
{
    context.save();
    context.clip(rectToPaint);
    core(&m_webView)->mainFrame().view()->paint(context, enclosingIntRect(rectToPaint));
    context.restore();
}

float AcceleratedCompositingContext::deviceScaleFactor() const
{
    return m_webView.deviceScaleFactor();
}

#endif // USE(TEXTURE_MAPPER_GL)
