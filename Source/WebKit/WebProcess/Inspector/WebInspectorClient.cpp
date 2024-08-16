/*
 * Copyright (C) 2010-2023 Apple Inc. All rights reserved.
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
#include "WebInspectorClient.h"

#include "DrawingArea.h"
#include "WebInspector.h"
#include "WebPage.h"
#include <WebCore/Animation.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/InspectorController.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/Page.h>
#include <WebCore/PageOverlayController.h>
#include <WebCore/Settings.h>
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(IOS_FAMILY)
#include <WebCore/InspectorOverlay.h>
#endif

namespace WebKit {
using namespace WebCore;

class RepaintIndicatorLayerClient final : public GraphicsLayerClient {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(RepaintIndicatorLayerClient);
public:
    RepaintIndicatorLayerClient(WebInspectorClient& inspectorClient)
        : m_inspectorClient(inspectorClient)
    {
    }
    virtual ~RepaintIndicatorLayerClient() { }
private:
    void notifyAnimationEnded(const GraphicsLayer* layer, const String&) override
    {
        m_inspectorClient.animationEndedForLayer(layer);
    }
    
    WebInspectorClient& m_inspectorClient;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebInspectorClient);

WebInspectorClient::WebInspectorClient(WebPage* page)
    : m_page(page)
    , m_highlightOverlay(nullptr)
{
}

WebInspectorClient::~WebInspectorClient()
{
    for (auto& layer : m_paintRectLayers)
        layer->removeFromParent();
    
    m_paintRectLayers.clear();

    if (m_paintRectOverlay) {
        RefPtr page = m_page.get();
        if (page && page->corePage())
            page->corePage()->pageOverlayController().uninstallPageOverlay(*m_paintRectOverlay, PageOverlay::FadeMode::Fade);
    }
}

void WebInspectorClient::inspectedPageDestroyed()
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    if (RefPtr inspector = page->inspector(WebPage::LazyCreationPolicy::UseExistingOnly))
        inspector->close();
}

void WebInspectorClient::frontendCountChanged(unsigned count)
{
    if (RefPtr page = m_page.get())
        page->inspectorFrontendCountChanged(count);
}

Inspector::FrontendChannel* WebInspectorClient::openLocalFrontend(InspectorController* controller)
{
    if (RefPtr page = m_page.get())
        page->inspector()->openLocalInspectorFrontend(controller->isUnderTest());
    return nullptr;
}

void WebInspectorClient::bringFrontendToFront()
{
    RefPtr page = m_page.get();
    if (page && page->inspector())
        page->inspector()->bringToFront();
}

void WebInspectorClient::didResizeMainFrame(LocalFrame*)
{
    RefPtr page = m_page.get();
    if (page && page->inspector())
        page->inspector()->updateDockingAvailability();
}

void WebInspectorClient::highlight()
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    if (!page->corePage()->settings().acceleratedCompositingEnabled()) {
#if PLATFORM(GTK) || PLATFORM(WIN) || PLATFORM(PLAYSTATION)
        // FIXME: It can be optimized by marking only highlighted rect dirty.
        // setNeedsDisplay always makes whole rect dirty, and could lead to poor performance.
        // https://bugs.webkit.org/show_bug.cgi?id=195933
        page->drawingArea()->setNeedsDisplay();
#endif
        return;
    }

#if !PLATFORM(IOS_FAMILY)
    if (!m_highlightOverlay) {
        auto highlightOverlay = PageOverlay::create(*this);
        m_highlightOverlay = highlightOverlay.ptr();
        page->corePage()->pageOverlayController().installPageOverlay(WTFMove(highlightOverlay), PageOverlay::FadeMode::Fade);
        m_highlightOverlay->setNeedsDisplay();
    } else {
        m_highlightOverlay->stopFadeOutAnimation();
        m_highlightOverlay->setNeedsDisplay();
    }
#else
    InspectorOverlay::Highlight highlight;
    page->corePage()->inspectorController().getHighlight(highlight, InspectorOverlay::CoordinateSystem::Document);
    page->showInspectorHighlight(highlight);
#endif
}

void WebInspectorClient::hideHighlight()
{
    RefPtr page = m_page.get();
    if (!page)
        return;

#if PLATFORM(GTK) || PLATFORM(WIN) || PLATFORM(PLAYSTATION)
    if (!page->corePage()->settings().acceleratedCompositingEnabled()) {
        // FIXME: It can be optimized by marking only highlighted rect dirty.
        // setNeedsDisplay always makes whole rect dirty, and could lead to poor performance.
        // https://bugs.webkit.org/show_bug.cgi?id=195933
        page->drawingArea()->setNeedsDisplay();
        return;
    }
#endif

#if !PLATFORM(IOS_FAMILY)
    if (m_highlightOverlay)
        page->corePage()->pageOverlayController().uninstallPageOverlay(*m_highlightOverlay, PageOverlay::FadeMode::Fade);
#else
    page->hideInspectorHighlight();
#endif
}

void WebInspectorClient::showPaintRect(const FloatRect& rect)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    if (!page->corePage()->settings().acceleratedCompositingEnabled())
        return;

    if (!m_paintRectOverlay) {
        m_paintRectOverlay = PageOverlay::create(*this, PageOverlay::OverlayType::Document);
        page->corePage()->pageOverlayController().installPageOverlay(*m_paintRectOverlay, PageOverlay::FadeMode::DoNotFade);
    }

    if (!m_paintIndicatorLayerClient)
        m_paintIndicatorLayerClient = makeUnique<RepaintIndicatorLayerClient>(*this);

    Ref paintLayer = GraphicsLayer::create(page->drawingArea()->graphicsLayerFactory(), *m_paintIndicatorLayerClient);
    
    paintLayer->setName(MAKE_STATIC_STRING_IMPL("paint rect"));
    paintLayer->setAnchorPoint(FloatPoint3D());
    paintLayer->setPosition(rect.location());
    paintLayer->setSize(rect.size());
    paintLayer->setBackgroundColor(Color::red.colorWithAlphaByte(51));

    KeyframeValueList fadeKeyframes(AnimatedProperty::Opacity);
    fadeKeyframes.insert(makeUnique<FloatAnimationValue>(0, 1));

    fadeKeyframes.insert(makeUnique<FloatAnimationValue>(0.25, 0));
    
    Ref opacityAnimation = Animation::create();
    opacityAnimation->setDuration(0.25);

    paintLayer->addAnimation(fadeKeyframes, FloatSize(), opacityAnimation.ptr(), "opacity"_s, 0);
    
    GraphicsLayer& rawLayer = paintLayer.get();
    m_paintRectLayers.add(WTFMove(paintLayer));

    GraphicsLayer& overlayRootLayer = m_paintRectOverlay->layer();
    overlayRootLayer.addChild(rawLayer);
}

void WebInspectorClient::animationEndedForLayer(const GraphicsLayer* layer)
{
    GraphicsLayer* nonConstLayer = const_cast<GraphicsLayer*>(layer);
    nonConstLayer->removeFromParent();
    m_paintRectLayers.remove(*nonConstLayer);
}

#if PLATFORM(IOS_FAMILY)
void WebInspectorClient::showInspectorIndication()
{
    if (RefPtr page = m_page.get())
        page->showInspectorIndication();
}

void WebInspectorClient::hideInspectorIndication()
{
    if (RefPtr page = m_page.get())
        page->hideInspectorIndication();
}

void WebInspectorClient::didSetSearchingForNode(bool enabled)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    if (enabled)
        page->enableInspectorNodeSearch();
    else
        page->disableInspectorNodeSearch();
}
#endif

void WebInspectorClient::elementSelectionChanged(bool active)
{
    RefPtr page = m_page.get();
    if (page && page->inspector())
        page->inspector()->elementSelectionChanged(active);
}

void WebInspectorClient::timelineRecordingChanged(bool active)
{
    RefPtr page = m_page.get();
    if (page && page->inspector())
        page->inspector()->timelineRecordingChanged(active);
}

void WebInspectorClient::setDeveloperPreferenceOverride(WebCore::InspectorClient::DeveloperPreference developerPreference, std::optional<bool> overrideValue)
{
    RefPtr page = m_page.get();
    if (page && page->inspector())
        page->inspector()->setDeveloperPreferenceOverride(developerPreference, overrideValue);
}

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)

bool WebInspectorClient::setEmulatedConditions(std::optional<int64_t>&& bytesPerSecondLimit)
{
    RefPtr page = m_page.get();
    if (page && page->inspector()) {
        page->inspector()->setEmulatedConditions(WTFMove(bytesPerSecondLimit));
        return true;
    }

    return false;
}

#endif // ENABLE(INSPECTOR_NETWORK_THROTTLING)

void WebInspectorClient::willMoveToPage(PageOverlay&, Page* page)
{
    if (page)
        return;

    // The page overlay is moving away from the web page, reset it.
    ASSERT(m_highlightOverlay);
    m_highlightOverlay = nullptr;
}

void WebInspectorClient::didMoveToPage(PageOverlay&, Page*)
{
}

void WebInspectorClient::drawRect(PageOverlay&, WebCore::GraphicsContext& context, const WebCore::IntRect& /*dirtyRect*/)
{
    if (RefPtr page = m_page.get())
        page->corePage()->inspectorController().drawHighlight(context);
}

bool WebInspectorClient::mouseEvent(PageOverlay&, const PlatformMouseEvent&)
{
    return false;
}

} // namespace WebKit
