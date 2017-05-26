/*
 * Copyright (C) 2015 Igalia S.L.
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
#include "DrawingAreaWPE.h"

#include "DrawingAreaProxyMessages.h"
#include "WebPage.h"
#include "WebPreferencesKeys.h"
#include "WebPreferencesStore.h"
#include <WebCore/DisplayRefreshMonitor.h>
#include <WebCore/MainFrame.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Settings.h>

using namespace WebCore;

namespace WebKit {

DrawingAreaWPE::DrawingAreaWPE(WebPage& webPage, const WebPageCreationParameters&)
    : DrawingArea(DrawingAreaTypeWPE, webPage)
{
    webPage.corePage()->settings().setForceCompositingMode(true);
    enterAcceleratedCompositingMode(0);
}

DrawingAreaWPE::~DrawingAreaWPE()
{
}

void DrawingAreaWPE::layerHostDidFlushLayers()
{
    m_webPage.send(Messages::DrawingAreaProxy::EnterAcceleratedCompositingMode(0, m_layerTreeHost->layerTreeContext()));
}

void DrawingAreaWPE::setNeedsDisplay()
{
    ASSERT(m_layerTreeHost);
    m_layerTreeHost->setNonCompositedContentsNeedDisplay();
}

void DrawingAreaWPE::setNeedsDisplayInRect(const IntRect& rect)
{
    ASSERT(m_layerTreeHost);
    m_layerTreeHost->setNonCompositedContentsNeedDisplayInRect(rect);
}

void DrawingAreaWPE::scroll(const IntRect& scrollRect, const IntSize&)
{
    ASSERT(m_layerTreeHost);
    m_layerTreeHost->scrollNonCompositedContents(scrollRect);
}

void DrawingAreaWPE::pageBackgroundTransparencyChanged()
{
    if (m_layerTreeHost)
        m_layerTreeHost->pageBackgroundTransparencyChanged();
}

void DrawingAreaWPE::forceRepaint()
{
    notImplemented();
}

bool DrawingAreaWPE::forceRepaintAsync(uint64_t callbackID)
{
    return m_layerTreeHost && m_layerTreeHost->forceRepaintAsync(callbackID);
}

void DrawingAreaWPE::setLayerTreeStateIsFrozen(bool frozen)
{
    m_layerTreeStateIsFrozen = frozen;
}

void DrawingAreaWPE::setPaintingEnabled(bool)
{
    notImplemented();
}

void DrawingAreaWPE::updatePreferences(const WebPreferencesStore& store)
{
    m_webPage.corePage()->settings().setForceCompositingMode(store.getBoolValueForKey(WebPreferencesKey::forceCompositingModeKey()));
}

void DrawingAreaWPE::mainFrameContentSizeChanged(const WebCore::IntSize& size)
{
    if (m_layerTreeHost)
        m_layerTreeHost->contentsSizeChanged(size);
}

GraphicsLayerFactory* DrawingAreaWPE::graphicsLayerFactory()
{
    if (m_layerTreeHost)
        return m_layerTreeHost->graphicsLayerFactory();
    return nullptr;
}

void DrawingAreaWPE::setRootCompositingLayer(GraphicsLayer* graphicsLayer)
{
    ASSERT(m_layerTreeHost);
    m_layerTreeHost->setRootCompositingLayer(graphicsLayer);
}

void DrawingAreaWPE::scheduleCompositingLayerFlush()
{
    if (m_layerTreeHost)
        m_layerTreeHost->scheduleLayerFlush();
}

void DrawingAreaWPE::scheduleCompositingLayerFlushImmediately()
{
    scheduleCompositingLayerFlush();
}

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
RefPtr<WebCore::DisplayRefreshMonitor> DrawingAreaWPE::createDisplayRefreshMonitor(PlatformDisplayID displayID)
{
    if (m_layerTreeHost)
        return m_layerTreeHost->createDisplayRefreshMonitor(displayID);

    return nullptr;
}
#endif

void DrawingAreaWPE::attachViewOverlayGraphicsLayer(WebCore::Frame* frame, WebCore::GraphicsLayer* viewOverlayRootLayer)
{
    if (!frame->isMainFrame())
        return;

    ASSERT(m_layerTreeHost);
    m_layerTreeHost->setViewOverlayRootLayer(viewOverlayRootLayer);
}

#if USE(COORDINATED_GRAPHICS_THREADED)
void DrawingAreaWPE::didChangeViewportAttributes(WebCore::ViewportAttributes&&)
{
}
#endif

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
void DrawingAreaWPE::deviceOrPageScaleFactorChanged()
{
}
#endif

void DrawingAreaWPE::updateBackingStoreState(uint64_t, bool, float deviceScaleFactor, const WebCore::IntSize& size, const WebCore::IntSize& scrollOffset)
{
    m_webPage.setDeviceScaleFactor(deviceScaleFactor);
    m_webPage.setSize(size);
    m_webPage.layoutIfNeeded();
    m_webPage.scrollMainFrameIfNotAtMaxScrollPosition(scrollOffset);

    ASSERT(m_layerTreeHost);
    m_layerTreeHost->sizeDidChange(m_webPage.size());
}

void DrawingAreaWPE::didUpdate()
{
}

void DrawingAreaWPE::enterAcceleratedCompositingMode(GraphicsLayer* graphicsLayer)
{
    ASSERT(!m_layerTreeHost);
    m_layerTreeHost = LayerTreeHost::create(m_webPage);
    m_layerTreeHost->setRootCompositingLayer(graphicsLayer);
    m_layerTreeHost->setShouldNotifyAfterNextScheduledLayerFlush(true);
    m_layerTreeHost->sizeDidChange(m_webPage.size());
}

#if USE(COORDINATED_GRAPHICS)
void DrawingAreaWPE::resetUpdateAtlasForTesting()
{
    ASSERT(m_layerTreeHost);
    m_layerTreeHost->clearUpdateAtlases();
}
#endif

} // namespace WebKit
