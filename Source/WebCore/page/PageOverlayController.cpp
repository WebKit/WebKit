/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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
#include "PageOverlayController.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "Page.h"
#include "PageOverlay.h"
#include "ScrollingCoordinator.h"
#include "Settings.h"
#include "TiledBacking.h"

// FIXME: Someone needs to call didChangeSettings() if we want dynamic updates of layer border/repaint counter settings.

namespace WebCore {

PageOverlayController::PageOverlayController(Page& page)
    :  m_page(page)
{
}

PageOverlayController::~PageOverlayController() = default;

void PageOverlayController::createRootLayersIfNeeded()
{
    if (m_initialized)
        return;

    m_initialized = true;

    ASSERT(!m_documentOverlayRootLayer);
    ASSERT(!m_viewOverlayRootLayer);

    m_documentOverlayRootLayer = GraphicsLayer::create(m_page.chrome().client().graphicsLayerFactory(), *this);
    m_viewOverlayRootLayer = GraphicsLayer::create(m_page.chrome().client().graphicsLayerFactory(), *this);
    m_documentOverlayRootLayer->setName("Document overlay Container");
    m_viewOverlayRootLayer->setName("View overlay container");
}

void PageOverlayController::installedPageOverlaysChanged()
{
    if (hasViewOverlays())
        attachViewOverlayLayers();
    else
        detachViewOverlayLayers();

    if (auto* frameView = m_page.mainFrame().view())
        frameView->setNeedsCompositingConfigurationUpdate();

    updateForceSynchronousScrollLayerPositionUpdates();
}

bool PageOverlayController::hasDocumentOverlays() const
{
    for (const auto& overlay : m_pageOverlays) {
        if (overlay->overlayType() == PageOverlay::OverlayType::Document)
            return true;
    }
    return false;
}

bool PageOverlayController::hasViewOverlays() const
{
    for (const auto& overlay : m_pageOverlays) {
        if (overlay->overlayType() == PageOverlay::OverlayType::View)
            return true;
    }
    return false;
}

void PageOverlayController::attachViewOverlayLayers()
{
    if (hasViewOverlays())
        m_page.chrome().client().attachViewOverlayGraphicsLayer(&layerWithViewOverlays());
}

void PageOverlayController::detachViewOverlayLayers()
{
    m_page.chrome().client().attachViewOverlayGraphicsLayer(nullptr);
}

GraphicsLayer* PageOverlayController::documentOverlayRootLayer() const
{
    return m_documentOverlayRootLayer.get();
}

GraphicsLayer* PageOverlayController::viewOverlayRootLayer() const
{
    return m_viewOverlayRootLayer.get();
}

static void updateOverlayGeometry(PageOverlay& overlay, GraphicsLayer& graphicsLayer)
{
    IntRect overlayFrame = overlay.frame();

    if (overlayFrame.location() == graphicsLayer.position() && overlayFrame.size() == graphicsLayer.size())
        return;

    graphicsLayer.setPosition(overlayFrame.location());
    graphicsLayer.setSize(overlayFrame.size());
}

GraphicsLayer& PageOverlayController::layerWithDocumentOverlays()
{
    createRootLayersIfNeeded();

    bool inWindow = m_page.isInWindow();

    for (auto& overlayAndLayer : m_overlayGraphicsLayers) {
        PageOverlay& overlay = *overlayAndLayer.key;
        if (overlay.overlayType() != PageOverlay::OverlayType::Document)
            continue;

        auto& layer = overlayAndLayer.value;
        GraphicsLayer::traverse(layer.get(), [inWindow](GraphicsLayer& layer) {
            layer.setIsInWindow(inWindow);
        });
        updateOverlayGeometry(overlay, layer.get());
        
        if (!layer->parent())
            m_documentOverlayRootLayer->addChild(layer.copyRef());
    }

    return *m_documentOverlayRootLayer;
}

GraphicsLayer& PageOverlayController::layerWithViewOverlays()
{
    createRootLayersIfNeeded();

    bool inWindow = m_page.isInWindow();

    for (auto& overlayAndLayer : m_overlayGraphicsLayers) {
        PageOverlay& overlay = *overlayAndLayer.key;
        if (overlay.overlayType() != PageOverlay::OverlayType::View)
            continue;

        auto& layer = overlayAndLayer.value;
        GraphicsLayer::traverse(layer.get(), [inWindow](GraphicsLayer& layer) {
            layer.setIsInWindow(inWindow);
        });
        updateOverlayGeometry(overlay, layer.get());
        
        if (!layer->parent())
            m_viewOverlayRootLayer->addChild(layer.copyRef());
    }

    return *m_viewOverlayRootLayer;
}

void PageOverlayController::installPageOverlay(PageOverlay& overlay, PageOverlay::FadeMode fadeMode)
{
    createRootLayersIfNeeded();

    if (m_pageOverlays.contains(&overlay))
        return;

    m_pageOverlays.append(&overlay);

    auto layer = GraphicsLayer::create(m_page.chrome().client().graphicsLayerFactory(), *this);
    layer->setAnchorPoint({ });
    layer->setBackgroundColor(overlay.backgroundColor());
    layer->setName("Overlay content");

    updateSettingsForLayer(layer.get());

    switch (overlay.overlayType()) {
    case PageOverlay::OverlayType::View:
        m_viewOverlayRootLayer->addChild(layer.get());
        break;
    case PageOverlay::OverlayType::Document:
        m_documentOverlayRootLayer->addChild(layer.get());
        break;
    }

    auto& rawLayer = layer.get();
    m_overlayGraphicsLayers.set(&overlay, WTFMove(layer));

    overlay.setPage(&m_page);

    if (FrameView* frameView = m_page.mainFrame().view())
        frameView->enterCompositingMode();

    updateOverlayGeometry(overlay, rawLayer);

    if (fadeMode == PageOverlay::FadeMode::Fade)
        overlay.startFadeInAnimation();

    installedPageOverlaysChanged();
}

void PageOverlayController::uninstallPageOverlay(PageOverlay& overlay, PageOverlay::FadeMode fadeMode)
{
    if (fadeMode == PageOverlay::FadeMode::Fade) {
        overlay.startFadeOutAnimation();
        return;
    }

    overlay.setPage(nullptr);

    if (auto optionalLayer = m_overlayGraphicsLayers.take(&overlay))
        optionalLayer.value()->removeFromParent();

    bool removed = m_pageOverlays.removeFirst(&overlay);
    ASSERT_UNUSED(removed, removed);

    installedPageOverlaysChanged();
}

void PageOverlayController::updateForceSynchronousScrollLayerPositionUpdates()
{
#if ENABLE(ASYNC_SCROLLING)
    bool forceSynchronousScrollLayerPositionUpdates = false;

    for (auto& overlay : m_pageOverlays) {
        if (overlay->needsSynchronousScrolling())
            forceSynchronousScrollLayerPositionUpdates = true;
    }

    if (ScrollingCoordinator* scrollingCoordinator = m_page.scrollingCoordinator())
        scrollingCoordinator->setForceSynchronousScrollLayerPositionUpdates(forceSynchronousScrollLayerPositionUpdates);
#endif
}

void PageOverlayController::setPageOverlayNeedsDisplay(PageOverlay& overlay, const WebCore::IntRect& dirtyRect)
{
    ASSERT(m_pageOverlays.contains(&overlay));
    auto* graphicsLayer = m_overlayGraphicsLayers.get(&overlay);

    if (!graphicsLayer->drawsContent()) {
        graphicsLayer->setDrawsContent(true);
        updateOverlayGeometry(overlay, *graphicsLayer);
    }

    graphicsLayer->setNeedsDisplayInRect(dirtyRect);
}

void PageOverlayController::setPageOverlayOpacity(PageOverlay& overlay, float opacity)
{
    ASSERT(m_pageOverlays.contains(&overlay));
    m_overlayGraphicsLayers.get(&overlay)->setOpacity(opacity);
}

void PageOverlayController::clearPageOverlay(PageOverlay& overlay)
{
    ASSERT(m_pageOverlays.contains(&overlay));
    m_overlayGraphicsLayers.get(&overlay)->setDrawsContent(false);
}

GraphicsLayer& PageOverlayController::layerForOverlay(PageOverlay& overlay) const
{
    ASSERT(m_pageOverlays.contains(&overlay));
    return *m_overlayGraphicsLayers.get(&overlay);
}

void PageOverlayController::didChangeViewSize()
{
    for (auto& overlayAndLayer : m_overlayGraphicsLayers) {
        if (overlayAndLayer.key->overlayType() == PageOverlay::OverlayType::View)
            updateOverlayGeometry(*overlayAndLayer.key, overlayAndLayer.value.get());
    }
}

void PageOverlayController::didChangeDocumentSize()
{
    for (auto& overlayAndLayer : m_overlayGraphicsLayers) {
        if (overlayAndLayer.key->overlayType() == PageOverlay::OverlayType::Document)
            updateOverlayGeometry(*overlayAndLayer.key, overlayAndLayer.value.get());
    }
}

void PageOverlayController::didChangeSettings()
{
    // FIXME: We should apply these settings to all overlay sublayers recursively.
    for (auto& graphicsLayer : m_overlayGraphicsLayers.values())
        updateSettingsForLayer(graphicsLayer.get());
}

void PageOverlayController::didChangeDeviceScaleFactor()
{
    if (!m_initialized)
        return;

    m_documentOverlayRootLayer->noteDeviceOrPageScaleFactorChangedIncludingDescendants();
    m_viewOverlayRootLayer->noteDeviceOrPageScaleFactorChangedIncludingDescendants();

    for (auto& graphicsLayer : m_overlayGraphicsLayers.values())
        graphicsLayer->setNeedsDisplay();
}

void PageOverlayController::didChangeViewExposedRect()
{
    m_page.renderingUpdateScheduler().scheduleRenderingUpdate();
}

void PageOverlayController::didScrollFrame(Frame& frame)
{
    for (auto& overlayAndLayer : m_overlayGraphicsLayers) {
        if (overlayAndLayer.key->overlayType() == PageOverlay::OverlayType::View || !frame.isMainFrame())
            overlayAndLayer.value->setNeedsDisplay();
        overlayAndLayer.key->didScrollFrame(frame);
    }
}

void PageOverlayController::updateSettingsForLayer(GraphicsLayer& layer)
{
    Settings& settings = m_page.settings();
    layer.setAcceleratesDrawing(settings.acceleratedDrawingEnabled());
    layer.setShowDebugBorder(settings.showDebugBorders());
    layer.setShowRepaintCounter(settings.showRepaintCounter());
}

bool PageOverlayController::handleMouseEvent(const PlatformMouseEvent& mouseEvent)
{
    if (m_pageOverlays.isEmpty())
        return false;

    for (auto it = m_pageOverlays.rbegin(), end = m_pageOverlays.rend(); it != end; ++it) {
        if ((*it)->mouseEvent(mouseEvent))
            return true;
    }

    return false;
}

bool PageOverlayController::copyAccessibilityAttributeStringValueForPoint(String attribute, FloatPoint parameter, String& value)
{
    if (m_pageOverlays.isEmpty())
        return false;

    for (auto it = m_pageOverlays.rbegin(), end = m_pageOverlays.rend(); it != end; ++it) {
        if ((*it)->copyAccessibilityAttributeStringValueForPoint(attribute, parameter, value))
            return true;
    }

    return false;
}

bool PageOverlayController::copyAccessibilityAttributeBoolValueForPoint(String attribute, FloatPoint parameter, bool& value)
{
    if (m_pageOverlays.isEmpty())
        return false;

    for (auto it = m_pageOverlays.rbegin(), end = m_pageOverlays.rend(); it != end; ++it) {
        if ((*it)->copyAccessibilityAttributeBoolValueForPoint(attribute, parameter, value))
            return true;
    }
    
    return false;
}

Vector<String> PageOverlayController::copyAccessibilityAttributesNames(bool parameterizedNames)
{
    if (m_pageOverlays.isEmpty())
        return { };

    for (auto it = m_pageOverlays.rbegin(), end = m_pageOverlays.rend(); it != end; ++it) {
        Vector<String> names = (*it)->copyAccessibilityAttributeNames(parameterizedNames);
        if (!names.isEmpty())
            return names;
    }

    return { };
}

void PageOverlayController::paintContents(const WebCore::GraphicsLayer* graphicsLayer, WebCore::GraphicsContext& graphicsContext, WebCore::GraphicsLayerPaintingPhase, const WebCore::FloatRect& clipRect, GraphicsLayerPaintBehavior)
{
    for (auto& overlayAndGraphicsLayer : m_overlayGraphicsLayers) {
        if (overlayAndGraphicsLayer.value.ptr() != graphicsLayer)
            continue;

        GraphicsContextStateSaver stateSaver(graphicsContext);
        graphicsContext.clip(clipRect);
        overlayAndGraphicsLayer.key->drawRect(graphicsContext, enclosingIntRect(clipRect));

        return;
    }
}

float PageOverlayController::deviceScaleFactor() const
{
    return m_page.deviceScaleFactor();
}

void PageOverlayController::notifyFlushRequired(const WebCore::GraphicsLayer*)
{
    m_page.renderingUpdateScheduler().scheduleRenderingUpdate();
}

void PageOverlayController::didChangeOverlayFrame(PageOverlay& overlay)
{
    ASSERT(m_pageOverlays.contains(&overlay));
    if (auto* layer = m_overlayGraphicsLayers.get(&overlay))
        updateOverlayGeometry(overlay, *layer);
}

void PageOverlayController::didChangeOverlayBackgroundColor(PageOverlay& overlay)
{
    ASSERT(m_pageOverlays.contains(&overlay));
    if (auto* layer = m_overlayGraphicsLayers.get(&overlay))
        layer->setBackgroundColor(overlay.backgroundColor());
}

bool PageOverlayController::shouldSkipLayerInDump(const GraphicsLayer*, LayerTreeAsTextBehavior behavior) const
{
    return !(behavior & LayerTreeAsTextIncludePageOverlayLayers);
}

void PageOverlayController::tiledBackingUsageChanged(const GraphicsLayer* graphicsLayer, bool usingTiledBacking)
{
    if (usingTiledBacking)
        graphicsLayer->tiledBacking()->setIsInWindow(m_page.isInWindow());
}

} // namespace WebKit
