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
    RepaintIndicatorLayerClient(WebInspectorBackendClient& inspectorBackendClient, LocalFrame& frame)
        : m_inspectorBackendClient(inspectorBackendClient)
        , m_frame(frame)
    {
    }
    virtual ~RepaintIndicatorLayerClient() = default;
private:
    void notifyAnimationEnded(const GraphicsLayer* layer, const String&) override
    {
        RefPtr frame = m_frame.get();
        m_inspectorBackendClient.animationEndedForLayer(frame.get(), layer);
    }

    WebInspectorBackendClient& m_inspectorBackendClient;
    // The local root frame whose bucket owns the animating layers. Weak: the frame can be torn down
    // (cross-origin navigation) mid-fade, after which animationEndedForLayer() no-ops.
    WeakPtr<LocalFrame> m_frame;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebInspectorBackendClient);

WebInspectorBackendClient::WebInspectorBackendClient(WebPage* page)
    : m_page(page)
{
}

WebInspectorBackendClient::~WebInspectorBackendClient()
{
    RefPtr page = m_page.get();
    RefPtr corePage = page ? page->corePage() : nullptr;

    // Tear down every per-frame bucket: detach its layers, uninstall its overlay (the controller
    // holds the only Ref keeping it alive), and drop the controller's per-frame container. The page
    // may already be gone, in which case the controller torn down with it.
    for (auto entry : m_paintRectOverlays) {
        Ref frame = entry.key;
        auto& bucket = entry.value;
        for (auto& layer : bucket.layers)
            protect(layer)->removeFromParent();
        bucket.layers.clear();

        if (corePage) {
            if (RefPtr overlay = bucket.overlay)
                corePage->pageOverlayController().uninstallPageOverlay(*overlay, PageOverlay::FadeMode::DoNotFade);
            corePage->pageOverlayController().willDestroyRootFrameOverlayContainer(frame.get());
        }
    }
    m_paintRectOverlays.clear();
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
#if PLATFORM(GTK) || PLATFORM(WIN) || PLATFORM(PLAYSTATION) || PLATFORM(WPE)
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

#if PLATFORM(GTK) || PLATFORM(WIN) || PLATFORM(PLAYSTATION) || PLATFORM(WPE)
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

auto WebInspectorBackendClient::ensurePaintRectOverlayForFrame(LocalFrame& frame) -> PaintRectOverlayForFrame&
{
    RefPtr page = m_page.get();
    ASSERT(page && page->corePage());

    auto& bucket = m_paintRectOverlays.ensure(frame, [&] {
        PaintRectOverlayForFrame newBucket;
        Ref overlay = PageOverlay::create(*this, PageOverlay::OverlayType::Document);
        // Scope the overlay to this local root so the controller hosts it in that frame's own
        // compositing tree; sibling local roots in one process each flash in the right place.
        overlay->setAssociatedFrame(&frame);
        newBucket.overlay = overlay.copyRef();
        newBucket.layerClient = makeUnique<RepaintIndicatorLayerClient>(*this, frame);
        page->corePage()->pageOverlayController().installPageOverlay(overlay, PageOverlay::FadeMode::DoNotFade);
        return newBucket;
    }).iterator->value;

    return bucket;
}

void WebInspectorBackendClient::showPaintRect(const FloatRect& rect)
{
    // Frame-less entry point (single-process main-frame path): resolve this process's overlay-owning
    // frame and delegate to the frame-aware path.
    RefPtr page = m_page.get();
    if (!page || !page->corePage())
        return;

    RefPtr frame = page->corePage()->localMainOrRootFrame();
    if (!frame)
        return;

    showPaintRect(*frame, rect);
}

void WebInspectorBackendClient::showPaintRect(LocalFrame& frame, const FloatRect& rect)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    if (!page->corePage()->settings().acceleratedCompositingEnabled())
        return;

    auto& bucket = ensurePaintRectOverlayForFrame(frame);
    RefPtr paintRectOverlay = bucket.overlay;
    if (!paintRectOverlay)
        return;

    Ref paintLayer = GraphicsLayer::create(protect(page->drawingArea())->graphicsLayerFactory(), *bucket.layerClient);

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
    bucket.layers.add(WTF::move(paintLayer));

    Ref overlayRootLayer = paintRectOverlay->layer();
    overlayRootLayer->addChild(rawLayer.get());
}

unsigned WebInspectorBackendClient::paintRectCount() const
{
    unsigned count = 0;
    for (auto entry : m_paintRectOverlays)
        count += entry.value.layers.size();
    return count;
}

void WebInspectorBackendClient::animationEndedForLayer(LocalFrame* frame, const GraphicsLayer* layer)
{
    // The layer client that fired identifies the bucket directly. If the frame is gone (torn down
    // mid-fade), its bucket was already uninstalled by willDestroyFrameOverlays -- nothing to do.
    if (!frame)
        return;

    auto it = m_paintRectOverlays.find(*frame);
    if (it == m_paintRectOverlays.end())
        return;

    GraphicsLayer* nonConstLayer = const_cast<GraphicsLayer*>(layer);
    nonConstLayer->removeFromParent();
    it->value.layers.remove(*nonConstLayer);
}

void WebInspectorBackendClient::willDestroyFrameOverlays(WebCore::FrameIdentifier frameID)
{
    RefPtr page = m_page.get();
    RefPtr corePage = page ? page->corePage() : nullptr;

    // Buckets are keyed by local root frame; a detaching frame owns a bucket only if it is that root.
    // Find it by identity, uninstall its overlay, detach its layers, and drop the controller's
    // per-frame container so nothing is retained past the frame's lifetime.
    m_paintRectOverlays.removeIf([frameID, corePage](auto& entry) {
        Ref frame = entry.key;
        if (frame->frameID() != frameID)
            return false;

        auto& bucket = entry.value;
        for (auto& paintLayer : bucket.layers)
            paintLayer->removeFromParent();
        bucket.layers.clear();

        if (corePage) {
            if (RefPtr overlay = bucket.overlay) {
                Ref protectedOverlay = overlay.releaseNonNull();
                corePage->pageOverlayController().uninstallPageOverlay(protectedOverlay, PageOverlay::FadeMode::DoNotFade);
            }
            corePage->pageOverlayController().willDestroyRootFrameOverlayContainer(frame.get());
        }
        return true;
    });
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

void WebInspectorBackendClient::setShowPaintRects(bool show)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    // Without Site Isolation this is the single-process path: the whole frame tree lives here, the
    // real InspectorPageAgent already drives the flash via didPaint, and there are no other processes
    // to fan out to. Leave that path untouched.
    RefPtr corePage = page->corePage();
    if (!corePage || !corePage->settings().siteIsolationEnabled())
        return;

    // Under Site Isolation cross-origin subframes run in their own processes, which the frontend's
    // Page domain never reaches. Notify the UIProcess so its ProxyingPageAgent fans the toggle out to
    // every WebContent process (see WebInspectorBackend::setShowPaintRects).
    if (RefPtr inspector = page->inspector())
        inspector->showPaintRectsChanged(show);
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
