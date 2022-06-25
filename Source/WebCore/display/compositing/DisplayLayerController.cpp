/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "DisplayLayerController.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "Chrome.h"
#include "ChromeClient.h"
#include "DisplayCSSPainter.h"
#include "DisplayContainerBox.h"
#include "DisplayPaintingContext.h"
#include "DisplayTree.h"
#include "DisplayView.h"
#include "Frame.h"
#include "FrameView.h"
#include "Logging.h"
#include "Page.h"
#include "Settings.h"

#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Display {

LayerController::RootLayerClient::RootLayerClient(LayerController& layerController)
    : m_layerController(layerController)
{
}

void LayerController::RootLayerClient::notifyFlushRequired(const GraphicsLayer*)
{
    m_layerController.scheduleRenderingUpdate();
}

void LayerController::RootLayerClient::paintContents(const GraphicsLayer* layer, GraphicsContext& context, const FloatRect& dirtyRect, GraphicsLayerPaintBehavior)
{
    ASSERT_UNUSED(layer, layer == m_layerController.contentLayer());

    if (auto* displayTree = m_layerController.view().tree()) {
        PaintingContext paintingContext { context, deviceScaleFactor() };
        CSSPainter::paintTree(*displayTree, paintingContext, enclosingIntRect(dirtyRect));
    }
}

float LayerController::RootLayerClient::deviceScaleFactor() const
{
    return m_layerController.view().deviceScaleFactor();
}

LayerController::LayerController(View& view)
    : m_view(view)
    , m_rootLayerClient(*this)
{
}

LayerController::~LayerController() = default;

void LayerController::prepareForDisplay(Tree& displayTree)
{
    auto viewSize = displayTree.rootBox().absoluteBorderBoxRect().size();
    // FIXME: Do overflow etc.
    auto contentSize = viewSize;

    LOG_WITH_STREAM(FormattingContextLayout, stream << "LayerController::prepareForDisplay - viewSize " << viewSize << " contentSize " << contentSize);

    ensureRootLayer(viewSize, contentSize);
    
    // FIXME: For now, repaint the world on every update.
    m_contentLayer->setNeedsDisplay();
    
    auto& settings = m_view.frame().settings();
    bool showDebugBorders = settings.showDebugBorders();
    bool showRepaintCounter = settings.showRepaintCounter();
    bool acceleratedDrawingEnabled = settings.acceleratedDrawingEnabled();

    m_rootLayer->setShowDebugBorder(showDebugBorders);
    m_contentHostLayer->setShowDebugBorder(showDebugBorders);

    m_contentLayer->setShowDebugBorder(showDebugBorders);
    m_contentLayer->setShowRepaintCounter(showRepaintCounter);
    m_contentLayer->setAcceleratesDrawing(acceleratedDrawingEnabled);
}

void LayerController::setNeedsDisplay()
{
    m_contentLayer->setNeedsDisplay();
}

void LayerController::flushLayers()
{
    if (!m_rootLayer)
        return;

    FloatRect visibleRect = visibleRectForLayerFlushing();
    m_rootLayer->flushCompositingState(visibleRect);
}

FloatRect LayerController::visibleRectForLayerFlushing() const
{
    const auto& frameView = m_view.frameView();
    return frameView.visibleContentRect();
}

void LayerController::scheduleRenderingUpdate()
{
    auto page = m_view.page();
    if (!page)
        return;

    page->scheduleRenderingUpdate(RenderingUpdateStep::LayerFlush);
}

void LayerController::ensureRootLayer(FloatSize viewSize, FloatSize contentSize)
{
    if (m_rootLayer) {
        updateRootLayerGeometry(viewSize, contentSize);
        return;
    }

    setupRootLayerHierarchy();
    updateRootLayerGeometry(viewSize, contentSize);

    attachRootLayer();
}

void LayerController::setupRootLayerHierarchy()
{
    // FIXME: This layer hierarchy is just enough to get topContentInset working.
    m_rootLayer = GraphicsLayer::create(graphicsLayerFactory(), m_rootLayerClient);
    m_rootLayer->setName(MAKE_STATIC_STRING_IMPL("display root"));
    m_rootLayer->setPosition({ });
    m_rootLayer->setAnchorPoint({ });

    m_contentHostLayer = GraphicsLayer::create(graphicsLayerFactory(), m_rootLayerClient);
    m_contentHostLayer->setName(MAKE_STATIC_STRING_IMPL("content host"));
    m_contentHostLayer->setAnchorPoint({ });

    m_contentLayer = GraphicsLayer::create(graphicsLayerFactory(), m_rootLayerClient);
    m_contentLayer->setName(MAKE_STATIC_STRING_IMPL("content"));
    m_contentLayer->setPosition({ });
    m_contentLayer->setAnchorPoint({ });
    m_contentLayer->setDrawsContent(true);
    
    m_rootLayer->addChild(*m_contentHostLayer);
    m_contentHostLayer->addChild(*m_contentLayer);
}

void LayerController::updateRootLayerGeometry(FloatSize viewSize, FloatSize contentSize)
{
    m_rootLayer->setSize(viewSize);

    float topContentInset = m_view.frameView().topContentInset();
    m_contentHostLayer->setPosition({ 0, topContentInset });
    m_contentHostLayer->setSize(viewSize);

    m_contentLayer->setSize(contentSize);
}

void LayerController::attachRootLayer()
{
    auto page = m_view.page();
    if (!page)
        return;

    page->chrome().client().attachRootGraphicsLayer(m_view.frame(), rootGraphicsLayer());
}

void LayerController::detachRootLayer()
{
    auto page = m_view.page();
    if (!page)
        return;

    page->chrome().client().attachRootGraphicsLayer(m_view.frame(), nullptr);
}

void LayerController::setIsInWindow(bool isInWindow)
{
    if (isInWindow)
        attachRootLayer();
    else
        detachRootLayer();
}

GraphicsLayerFactory* LayerController::graphicsLayerFactory() const
{
    auto page = m_view.page();
    if (!page)
        return nullptr;

    return page->chrome().client().graphicsLayerFactory();
}

} // namespace Display
} // namespace WebCore

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
