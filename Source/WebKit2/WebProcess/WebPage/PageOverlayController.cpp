/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "config.h"
#import "PageOverlayController.h"

#import "DrawingArea.h"
#import "PageOverlay.h"
#import "WebPage.h"
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/GraphicsLayer.h>
#import <WebCore/ScrollingCoordinator.h>
#import <WebCore/Settings.h>
#import <WebCore/TiledBacking.h>

using namespace WebCore;

namespace WebKit {

PageOverlayController::PageOverlayController(WebPage* webPage)
    : m_webPage(webPage)
{
}

void PageOverlayController::initialize()
{
    ASSERT(!m_rootLayer);

    m_rootLayer = GraphicsLayer::create(m_webPage->drawingArea()->graphicsLayerFactory(), this);
#ifndef NDEBUG
    m_rootLayer->setName("Page Overlay container");
#endif
}

void PageOverlayController::installPageOverlay(PassRefPtr<PageOverlay> pageOverlay, PageOverlay::FadeMode fadeMode)
{
    ASSERT(m_rootLayer);

    RefPtr<PageOverlay> overlay = pageOverlay;

    if (m_pageOverlays.contains(overlay))
        return;

    m_pageOverlays.append(overlay);
    overlay->setPage(m_webPage);

    if (fadeMode == PageOverlay::FadeMode::Fade)
        overlay->startFadeInAnimation();

    std::unique_ptr<GraphicsLayer> layer = GraphicsLayer::create(m_webPage->drawingArea()->graphicsLayerFactory(), this);
#ifndef NDEBUG
    layer->setName("Page Overlay content");
#endif

    updateSettingsForLayer(layer.get());
    m_rootLayer->addChild(layer.get());
    m_overlayGraphicsLayers.set(overlay.get(), std::move(layer));

#if ENABLE(ASYNC_SCROLLING)
    if (Page* page = m_webPage->corePage()) {
        if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
            scrollingCoordinator->setForceSynchronousScrollLayerPositionUpdates(true);
    }
#endif
}

void PageOverlayController::uninstallPageOverlay(PageOverlay* overlay, PageOverlay::FadeMode fadeMode)
{
    if (fadeMode == PageOverlay::FadeMode::Fade) {
        overlay->startFadeOutAnimation();
        return;
    }

    overlay->setPage(nullptr);

    m_overlayGraphicsLayers.take(overlay)->removeFromParent();

    size_t overlayIndex = m_pageOverlays.find(overlay);
    ASSERT(overlayIndex != notFound);
    m_pageOverlays.remove(overlayIndex);

    if (!m_pageOverlays.isEmpty())
        return;

#if ENABLE(ASYNC_SCROLLING)
    if (Page* page = m_webPage->corePage()) {
        if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
            scrollingCoordinator->setForceSynchronousScrollLayerPositionUpdates(false);
    }
#endif
}

void PageOverlayController::setPageOverlayNeedsDisplay(PageOverlay* overlay, const WebCore::IntRect& dirtyRect)
{
    GraphicsLayer* graphicsLayer = m_overlayGraphicsLayers.get(overlay);
    if (!graphicsLayer)
        return;

    if (!graphicsLayer->drawsContent()) {
        graphicsLayer->setDrawsContent(true);
        graphicsLayer->setSize(m_webPage->size());
    }
    graphicsLayer->setNeedsDisplayInRect(dirtyRect);
}

void PageOverlayController::setPageOverlayOpacity(PageOverlay* overlay, float opacity)
{
    GraphicsLayer* graphicsLayer = m_overlayGraphicsLayers.get(overlay);
    if (!graphicsLayer)
        return;

    graphicsLayer->setOpacity(opacity);
}

void PageOverlayController::clearPageOverlay(PageOverlay* overlay)
{
    GraphicsLayer* graphicsLayer = m_overlayGraphicsLayers.get(overlay);
    if (!graphicsLayer)
        return;

    graphicsLayer->setDrawsContent(false);
    graphicsLayer->setSize(IntSize());
}

void PageOverlayController::didChangeViewSize()
{
    for (auto& graphicsLayer : m_overlayGraphicsLayers.values()) {
        if (!graphicsLayer->drawsContent())
            continue;
        graphicsLayer->setSize(m_webPage->size());
        graphicsLayer->setNeedsDisplay();
    }
}

void PageOverlayController::didChangePreferences()
{
    for (auto& graphicsLayer : m_overlayGraphicsLayers.values())
        updateSettingsForLayer(graphicsLayer.get());
}

void PageOverlayController::didChangeDeviceScaleFactor()
{
    m_rootLayer->noteDeviceOrPageScaleFactorChangedIncludingDescendants();
    invalidateAllPageOverlayLayers();
}

void PageOverlayController::didChangeExposedRect()
{
    m_webPage->drawingArea()->scheduleCompositingLayerFlush();
}

void PageOverlayController::didScrollAnyFrame()
{
    invalidateAllPageOverlayLayers();
}

void PageOverlayController::invalidateAllPageOverlayLayers()
{
    for (auto& graphicsLayer : m_overlayGraphicsLayers.values())
        graphicsLayer->setNeedsDisplay();
}

void PageOverlayController::flushPageOverlayLayers(FloatRect visibleRect)
{
    m_rootLayer->flushCompositingState(visibleRect);
}

void PageOverlayController::updateSettingsForLayer(GraphicsLayer* layer)
{
    Settings& settings = m_webPage->corePage()->settings();
    layer->setAcceleratesDrawing(settings.acceleratedDrawingEnabled());
    layer->setShowDebugBorder(settings.showDebugBorders());
    layer->setShowRepaintCounter(settings.showRepaintCounter());
}

bool PageOverlayController::handleMouseEvent(const WebMouseEvent& mouseEvent)
{
    if (!m_pageOverlays.size())
        return false;

    for (auto it = m_pageOverlays.rbegin(), end = m_pageOverlays.rend(); it != end; ++it) {
        if ((*it)->mouseEvent(mouseEvent))
            return true;
    }

    return false;
}

WKTypeRef PageOverlayController::copyAccessibilityAttributeValue(WKStringRef attribute, WKTypeRef parameter)
{
    if (!m_pageOverlays.size())
        return nullptr;

    for (auto it = m_pageOverlays.rbegin(), end = m_pageOverlays.rend(); it != end; ++it) {
        if (WKTypeRef value = (*it)->copyAccessibilityAttributeValue(attribute, parameter))
            return value;
    }

    return nullptr;
}

WKArrayRef PageOverlayController::copyAccessibilityAttributesNames(bool parameterizedNames)
{
    if (!m_pageOverlays.size())
        return nullptr;

    for (auto it = m_pageOverlays.rbegin(), end = m_pageOverlays.rend(); it != end; ++it) {
        if (WKArrayRef value = (*it)->copyAccessibilityAttributeNames(parameterizedNames))
            return value;
    }

    return nullptr;
}

void PageOverlayController::paintContents(const WebCore::GraphicsLayer* graphicsLayer, WebCore::GraphicsContext& graphicsContext, WebCore::GraphicsLayerPaintingPhase, const WebCore::FloatRect& clipRect)
{
    for (auto it = m_overlayGraphicsLayers.begin(), end = m_overlayGraphicsLayers.end(); it != end; ++it) {
        if (it->value.get() != graphicsLayer)
            continue;

        GraphicsContextStateSaver stateSaver(graphicsContext);
        graphicsContext.clip(clipRect);
        it->key->drawRect(graphicsContext, enclosingIntRect(clipRect));

        return;
    }
}

float PageOverlayController::deviceScaleFactor() const
{
    return m_webPage->corePage()->deviceScaleFactor();
}

void PageOverlayController::notifyFlushRequired(const WebCore::GraphicsLayer*)
{
    m_webPage->drawingArea()->scheduleCompositingLayerFlush();
}

} // namespace WebKit
