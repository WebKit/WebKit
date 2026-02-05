/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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
#include "WebInspectorBackendClient.h"

#include "DrawingArea.h"
#include "WebInspectorBackend.h"
#include "WebPage.h"
#include <WebCore/GraphicsLayer.h>
#include <WebCore/GraphicsLayerAnimation.h>
#include <WebCore/GraphicsLayerFactory.h>
#include <WebCore/GraphicsLayerFloatAnimationValue.h>
#include <WebCore/GraphicsLayerKeyframeValueList.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/Page.h>
#include <WebCore/PageInspectorController.h>
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
    RepaintIndicatorLayerClient(WebInspectorBackendClient& inspectorBackendClient)
        : m_inspectorBackendClient(inspectorBackendClient)
    {
    }
    virtual ~RepaintIndicatorLayerClient() = default;
private:
    void notifyAnimationEnded(const GraphicsLayer* layer, const String&) override
    {
        m_inspectorBackendClient.animationEndedForLayer(layer);
    }

    WebInspectorBackendClient& m_inspectorBackendClient;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebInspectorBackendClient);

WebInspectorBackendClient::WebInspectorBackendClient(WebPage* page)
    : m_page(page)
{
}

WebInspectorBackendClient::~WebInspectorBackendClient()
{
    for (auto& layer : m_paintRectLayers)
        layer->removeFromParent();

    m_paintRectLayers.clear();

    if (RefPtr paintRectOverlay = m_paintRectOverlay) {
        RefPtr page = m_page.get();
        if (page && page->corePage())
            page->corePage()->pageOverlayController().uninstallPageOverlay(*paintRectOverlay, PageOverlay::FadeMode::Fade);
    }
}

void WebInspectorBackendClient::inspectedPageDestroyed()
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    if (RefPtr inspector = page->inspector(WebPage::LazyCreationPolicy::UseExistingOnly))
        inspector->close();
}

void WebInspectorBackendClient::frontendCountChanged(unsigned count)
{
    if (RefPtr page = m_page.get())
        page->inspectorFrontendCountChanged(count);
}

Inspector::FrontendChannel* WebInspectorBackendClient::openLocalFrontend(PageInspectorController* controller)
{
    if (RefPtr page = m_page.get())
        protect(page->inspector())->openLocalInspectorFrontend();
    return nullptr;
}

void WebInspectorBackendClient::bringFrontendToFront()
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    if (RefPtr inspector = page->inspector())
        inspector->bringToFront();
}

void WebInspectorBackendClient::didResizeMainFrame(LocalFrame*)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    if (RefPtr inspector = page->inspector())
        inspector->updateDockingAvailability();
}

void WebInspectorBackendClient::highlight()
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
    if (RefPtr highlightOverlay = m_highlightOverlay.get()) {
        highlightOverlay->stopFadeOutAnimation();
        highlightOverlay->setNeedsDisplay();
    } else {
        Ref newHighlightOverlay = PageOverlay::create(*this);
        m_highlightOverlay = newHighlightOverlay.ptr();
        page->corePage()->pageOverlayController().installPageOverlay(newHighlightOverlay.copyRef(), PageOverlay::FadeMode::Fade);
        newHighlightOverlay->setNeedsDisplay();
    }
#else
    InspectorOverlay::Highlight highlight;
    page->corePage()->inspectorController().getHighlight(highlight, InspectorOverlay::CoordinateSystem::Document);
    page->showInspectorHighlight(highlight);
#endif
}

void WebInspectorBackendClient::hideHighlight()
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
    if (RefPtr highlightOverlay = m_highlightOverlay.get())
        page->corePage()->pageOverlayController().uninstallPageOverlay(*highlightOverlay, PageOverlay::FadeMode::Fade);
#else
    page->hideInspectorHighlight();
#endif
}

void WebInspectorBackendClient::showPaintRect(const FloatRect& rect)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    if (!page->corePage()->settings().acceleratedCompositingEnabled())
        return;

    RefPtr paintRectOverlay = m_paintRectOverlay;
    if (!paintRectOverlay) {
        m_paintRectOverlay = PageOverlay::create(*this, PageOverlay::OverlayType::Document);
        paintRectOverlay = m_paintRectOverlay.copyRef();
        page->corePage()->pageOverlayController().installPageOverlay(*paintRectOverlay, PageOverlay::FadeMode::DoNotFade);
    }

    if (!m_paintIndicatorLayerClient)
        m_paintIndicatorLayerClient = makeUnique<RepaintIndicatorLayerClient>(*this);

    Ref paintLayer = GraphicsLayer::create(protect(page->drawingArea())->graphicsLayerFactory(), *m_paintIndicatorLayerClient);

    paintLayer->setName(MAKE_STATIC_STRING_IMPL("paint rect"));
    paintLayer->setAnchorPoint(FloatPoint3D());
    paintLayer->setPosition(rect.location());
    paintLayer->setSize(rect.size());
    paintLayer->setBackgroundColor(Color::red.colorWithAlphaByte(51));

    GraphicsLayerKeyframeValueList fadeKeyframes(AnimatedProperty::Opacity);
    fadeKeyframes.insert(makeUnique<GraphicsLayerFloatAnimationValue>(0, 1));

    fadeKeyframes.insert(makeUnique<GraphicsLayerFloatAnimationValue>(0.25, 0));

    Ref opacityAnimation = GraphicsLayerAnimation::create();
    opacityAnimation->setDuration(0.25);

    paintLayer->addAnimation(fadeKeyframes, opacityAnimation.ptr(), "opacity"_s, 0);

    Ref rawLayer = paintLayer.get();
    m_paintRectLayers.add(WTF::move(paintLayer));

    Ref overlayRootLayer = paintRectOverlay->layer();
    overlayRootLayer->addChild(rawLayer.get());
}

void WebInspectorBackendClient::animationEndedForLayer(const GraphicsLayer* layer)
{
    GraphicsLayer* nonConstLayer = const_cast<GraphicsLayer*>(layer);
    nonConstLayer->removeFromParent();
    m_paintRectLayers.remove(*nonConstLayer);
}

#if PLATFORM(IOS_FAMILY)
void WebInspectorBackendClient::showInspectorIndication()
{
    if (RefPtr page = m_page.get())
        page->showInspectorIndication();
}

void WebInspectorBackendClient::hideInspectorIndication()
{
    if (RefPtr page = m_page.get())
        page->hideInspectorIndication();
}

void WebInspectorBackendClient::didSetSearchingForNode(bool enabled)
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

void WebInspectorBackendClient::elementSelectionChanged(bool active)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    if (RefPtr inspector = page->inspector())
        inspector->elementSelectionChanged(active);
}

void WebInspectorBackendClient::timelineRecordingChanged(bool active)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    if (RefPtr inspector = page->inspector())
        inspector->timelineRecordingChanged(active);
}

void WebInspectorBackendClient::setDeveloperPreferenceOverride(WebCore::InspectorBackendClient::DeveloperPreference developerPreference, std::optional<bool> overrideValue)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    if (RefPtr inspector = page->inspector())
        inspector->setDeveloperPreferenceOverride(developerPreference, overrideValue);
}

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)

bool WebInspectorBackendClient::setEmulatedConditions(std::optional<int64_t>&& bytesPerSecondLimit)
{
    RefPtr page = m_page.get();
    if (page && page->inspector()) {
        page->inspector()->setEmulatedConditions(WTF::move(bytesPerSecondLimit));
        return true;
    }

    return false;
}

#endif // ENABLE(INSPECTOR_NETWORK_THROTTLING)

void WebInspectorBackendClient::willMoveToPage(PageOverlay&, Page* page)
{
    if (page)
        return;

    // The page overlay is moving away from the web page, reset it.
    ASSERT(m_highlightOverlay);
    m_highlightOverlay = nullptr;
}

void WebInspectorBackendClient::didMoveToPage(PageOverlay&, Page*)
{
}

void WebInspectorBackendClient::drawRect(PageOverlay&, WebCore::GraphicsContext& context, const WebCore::IntRect& /*dirtyRect*/)
{
    if (RefPtr page = m_page.get())
        page->corePage()->inspectorController().drawHighlight(context);
}

bool WebInspectorBackendClient::mouseEvent(PageOverlay&, const PlatformMouseEvent&)
{
    return false;
}

} // namespace WebKit
