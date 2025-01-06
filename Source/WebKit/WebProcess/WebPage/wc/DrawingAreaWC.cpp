/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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
#include "DrawingAreaWC.h"

#if USE(GRAPHICS_LAYER_WC)

#include "DrawingAreaProxyMessages.h"
#include "GraphicsLayerWC.h"
#include "ImageBufferShareableBitmapBackend.h"
#include "MessageSenderInlines.h"
#include "RemoteRenderingBackendProxy.h"
#include "RemoteWCLayerTreeHostProxy.h"
#include "UpdateInfo.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebPageCreationParameters.h"
#include "WebPageInlines.h"
#include "WebPreferencesKeys.h"
#include "WebProcess.h"
#include <WebCore/ImageBuffer.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/Page.h>
#include <wtf/text/MakeString.h>

namespace WebKit {
using namespace WebCore;

DrawingAreaWC::DrawingAreaWC(WebPage& webPage, const WebPageCreationParameters& parameters)
    : DrawingArea(DrawingAreaType::WC, parameters.drawingAreaIdentifier, webPage)
    , m_remoteWCLayerTreeHostProxy(makeUniqueWithoutRefCountedCheck<RemoteWCLayerTreeHostProxy>(webPage, parameters.usesOffscreenRendering))
    , m_layerFactory(*this)
    , m_updateRenderingTimer(*this, &DrawingAreaWC::updateRendering)
    , m_commitQueue(WorkQueue::create("DrawingAreaWC CommitQueue"_s))
{
}

DrawingAreaWC::~DrawingAreaWC()
{
    while (auto layer = m_liveGraphicsLayers.removeHead())
        layer->clearObserver();
}

GraphicsLayerFactory* DrawingAreaWC::graphicsLayerFactory()
{
    return &m_layerFactory;
}

void DrawingAreaWC::updateRootLayers()
{
    for (auto& rootLayer : m_rootLayers) {
        Vector<Ref<GraphicsLayer>> children;
        if (rootLayer.contentLayer) {
            children.append(*rootLayer.contentLayer);
            if (rootLayer.viewOverlayRootLayer)
                children.append(*rootLayer.viewOverlayRootLayer);
        }
        rootLayer.layer->setChildren(WTFMove(children));
    }
    triggerRenderingUpdate();
}

void DrawingAreaWC::setRootCompositingLayer(WebCore::Frame& frame, GraphicsLayer* rootGraphicsLayer)
{
    for (auto& rootLayer : m_rootLayers) {
        if (rootLayer.frameID == frame.frameID())
            rootLayer.contentLayer = rootGraphicsLayer;
    }
    if (rootGraphicsLayer)
        send(Messages::DrawingAreaProxy::EnterAcceleratedCompositingMode(m_backingStoreStateID, { }));
    updateRootLayers();
}

void DrawingAreaWC::addRootFrame(WebCore::FrameIdentifier frameID)
{
    auto layer = GraphicsLayer::create(graphicsLayerFactory(), this->m_rootLayerClient);
    // FIXME: This has an unnecessary string allocation. Adding a StringTypeAdapter for FrameIdentifier or ProcessQualified would remove that.
    layer->setName(makeString("drawing area root "_s, frameID.toString()));
    layer->setAnchorPoint({ });
    m_rootLayers.append(RootLayerInfo {
        WTFMove(layer),
        nullptr,
        nullptr,
        frameID
    });
}

void DrawingAreaWC::attachViewOverlayGraphicsLayer(WebCore::FrameIdentifier frameID, GraphicsLayer* layer)
{
    if (auto* layerInfo = rootLayerInfoWithFrameIdentifier(frameID)) {
        layerInfo->viewOverlayRootLayer = layer;
        updateRootLayers();
    }
}

void DrawingAreaWC::updatePreferences(const WebPreferencesStore& store)
{
    Settings& settings = m_webPage->corePage()->settings();
    settings.setAcceleratedCompositingForFixedPositionEnabled(settings.acceleratedCompositingEnabled());
    settings.setForceCompositingMode(store.getBoolValueForKey(WebPreferencesKey::siteIsolationEnabledKey()));
}

bool DrawingAreaWC::shouldUseTiledBackingForFrameView(const WebCore::LocalFrameView& frameView) const
{
    return frameView.frame().isRootFrame();
}

void DrawingAreaWC::setLayerTreeStateIsFrozen(bool isFrozen)
{
    if (m_isRenderingSuspended == isFrozen)
        return;
    m_isRenderingSuspended = isFrozen;
    if (!m_isRenderingSuspended && m_hasDeferredRenderingUpdate) {
        m_hasDeferredRenderingUpdate = false;
        triggerRenderingUpdate();
    }
}

void DrawingAreaWC::updateGeometryWC(uint64_t backingStoreStateID, IntSize viewSize, float deviceScaleFactor)
{
    m_backingStoreStateID = backingStoreStateID;
    m_webPage->setDeviceScaleFactor(deviceScaleFactor);
    m_webPage->setSize(viewSize);
}

void DrawingAreaWC::setNeedsDisplay()
{
    m_dirtyRegion = m_webPage->bounds();
    m_scrollRect = { };
    m_scrollOffset = { };
    triggerRenderingUpdate();
}

void DrawingAreaWC::setNeedsDisplayInRect(const IntRect& rect)
{
    if (isCompositingMode())
        return;
    IntRect dirtyRect = rect;
    dirtyRect.intersect(m_webPage->bounds());
    m_dirtyRegion.unite(dirtyRect);
    triggerRenderingUpdate();
}

void DrawingAreaWC::scroll(const IntRect& scrollRect, const IntSize& scrollDelta)
{
    if (isCompositingMode())
        return;

    if (scrollRect != m_scrollRect) {
        // Just repaint the entire current scroll rect, we'll scroll the new rect instead.
        setNeedsDisplayInRect(m_scrollRect);
        m_scrollRect = { };
        m_scrollOffset = { };
    }
    // Get the part of the dirty region that is in the scroll rect.
    Region dirtyRegionInScrollRect = intersect(scrollRect, m_dirtyRegion);
    if (!dirtyRegionInScrollRect.isEmpty()) {
        // There are parts of the dirty region that are inside the scroll rect.
        // We need to subtract them from the region, move them and re-add them.
        m_dirtyRegion.subtract(scrollRect);
        // Move the dirty parts.
        Region movedDirtyRegionInScrollRect = intersect(translate(dirtyRegionInScrollRect, scrollDelta), scrollRect);
        // And add them back.
        m_dirtyRegion.unite(movedDirtyRegionInScrollRect);
    }
    // Compute the scroll repaint region.
    Region scrollRepaintRegion = subtract(scrollRect, translate(scrollRect, scrollDelta));
    m_dirtyRegion.unite(scrollRepaintRegion);
    m_scrollRect = scrollRect;
    m_scrollOffset += scrollDelta;
    triggerRenderingUpdate();
}

void DrawingAreaWC::updateRenderingWithForcedRepaintAsync(WebPage&, CompletionHandler<void()>&& completionHandler)
{
    m_forceRepaintCompletionHandler = WTFMove(completionHandler);
    m_isForceRepaintCompletionHandlerDeferred = m_waitDidUpdate;
    setNeedsDisplay();
}

void DrawingAreaWC::triggerRenderingUpdate()
{
    if (m_isRenderingSuspended || m_waitDidUpdate) {
        m_hasDeferredRenderingUpdate = true;
        return;
    }
    if (m_updateRenderingTimer.isActive())
        return;
    m_updateRenderingTimer.startOneShot(0_s);
}

bool DrawingAreaWC::isCompositingMode()
{
    auto& mainWebFrame = m_webPage->mainWebFrame();
    if (!mainWebFrame.isRootFrame())
        return true;
    return rootLayerInfoWithFrameIdentifier(mainWebFrame.frameID())->contentLayer;
}

void DrawingAreaWC::updateRendering()
{
    if (m_isRenderingSuspended) {
        m_hasDeferredRenderingUpdate = true;
        return;
    }

    // This function is not reentrant, e.g. a rAF callback may force repaint.
    if (m_inUpdateRendering)
        return;
    SetForScope change(m_inUpdateRendering, true);

    ASSERT(!m_waitDidUpdate);
    m_waitDidUpdate = true;

    Ref webPage = m_webPage.get();
    webPage->updateRendering();
    webPage->flushPendingEditorStateUpdate();

    OptionSet<FinalizeRenderingUpdateFlags> flags;
    webPage->finalizeRenderingUpdate(flags);
    webPage->didUpdateRendering();

    willStartRenderingUpdateDisplay();

    if (isCompositingMode())
        sendUpdateAC();
    else
        sendUpdateNonAC();
}

void DrawingAreaWC::sendUpdateAC()
{
    bool didProcessMainFrame = false;
    for (auto it = m_rootLayers.begin(); it != m_rootLayers.end(); it++) {
        auto& rootLayer = *it;
        auto frame = WebProcess::singleton().webFrame(rootLayer.frameID);
        ASSERT(frame);
        m_updateInfo.remoteContextHostedIdentifier = frame->layerHostingContextIdentifier();
        m_updateInfo.rootLayer = rootLayer.layer->primaryLayerID();

        bool isLastFrame = (it + 1) == m_rootLayers.end();
        bool isMainFrame = frame->isMainFrame();
        didProcessMainFrame = didProcessMainFrame || isMainFrame;
        bool willCallDisplayDidRefresh = isMainFrame || (!didProcessMainFrame && isLastFrame);
        IntSize size;
        if (isMainFrame)
            size = m_webPage->size();
        else
            size = frame->size();
        FloatSize viewport { size };
        viewport.scale(m_webPage->deviceScaleFactor());
        m_updateInfo.viewport = WebCore::expandedIntSize(viewport);
        updateRootLayerDeviceScaleFactor(rootLayer.layer);
        rootLayer.layer->flushCompositingStateForThisLayerOnly();

        // Because our view-relative overlay root layer is not attached to the FrameView's GraphicsLayer tree, we need to flush it manually.
        FloatRect visibleRect({ }, size);
        if (rootLayer.viewOverlayRootLayer)
            rootLayer.viewOverlayRootLayer->flushCompositingState(visibleRect);

        auto fence = m_webPage->ensureRemoteRenderingBackendProxy().flushImageBuffers();

        m_commitQueue->dispatch([this, weakThis = WeakPtr(*this), stateID = m_backingStoreStateID, updateInfo = std::exchange(m_updateInfo, { }), fence = WTFMove(fence), willCallDisplayDidRefresh]() mutable {
            fence();
            RunLoop::main().dispatch([this, weakThis = WTFMove(weakThis), stateID, updateInfo = WTFMove(updateInfo), willCallDisplayDidRefresh]() mutable {
                if (!weakThis)
                    return;
                m_remoteWCLayerTreeHostProxy->update(WTFMove(updateInfo), [this, weakThis = WTFMove(weakThis), stateID, willCallDisplayDidRefresh](std::optional<UpdateInfo> updateInfo) {
                    if (!weakThis)
                        return;
                    if (updateInfo && stateID == m_backingStoreStateID) {
                        ASSERT(willCallDisplayDidRefresh);
                        send(Messages::DrawingAreaProxy::Update(m_backingStoreStateID, WTFMove(*updateInfo)));
                        return;
                    }
                    if (willCallDisplayDidRefresh)
                        displayDidRefresh();
                });
            });
        });
    }
}

void DrawingAreaWC::updateRootLayerDeviceScaleFactor(WebCore::GraphicsLayer& layer)
{
    float deviceScaleFactor = m_webPage->deviceScaleFactor();
    TransformationMatrix m;
    m.scale(deviceScaleFactor);
    layer.setTransform(m);
}

static bool shouldPaintBoundsRect(const IntRect& bounds, const Vector<IntRect, 1>& rects)
{
    constexpr size_t rectThreshold = 10;
    constexpr double wastedSpaceThreshold = 0.75;

    if (rects.size() <= 1 || rects.size() > rectThreshold)
        return true;
    // Attempt to guess whether or not we should use the region bounds rect or the individual rects.
    // We do this by computing the percentage of "wasted space" in the bounds. If that wasted space
    // is too large, then we will do individual rect painting instead.
    unsigned boundsArea = bounds.width() * bounds.height();
    unsigned rectsArea = 0;
    for (size_t i = 0; i < rects.size(); ++i)
        rectsArea += rects[i].width() * rects[i].height();

    double wastedSpace = 1 - (static_cast<double>(rectsArea) / boundsArea);
    return wastedSpace <= wastedSpaceThreshold;
}

void DrawingAreaWC::sendUpdateNonAC()
{
    if (m_dirtyRegion.isEmpty()) {
        displayDidRefresh();
        return;
    }
    IntRect bounds = m_dirtyRegion.bounds();
    Ref webPage = m_webPage.get();
    ASSERT(webPage->bounds().contains(bounds));
    IntSize bitmapSize = bounds.size();
    float deviceScaleFactor = webPage->corePage()->deviceScaleFactor();
    auto image = createImageBuffer(bitmapSize, deviceScaleFactor);
    auto rects = m_dirtyRegion.rects();
    if (shouldPaintBoundsRect(bounds, rects)) {
        rects.clear();
        rects.append(bounds);
    }
    m_dirtyRegion = { };

    UpdateInfo updateInfo;
    updateInfo.viewSize = webPage->size();
    updateInfo.deviceScaleFactor = deviceScaleFactor;
    updateInfo.updateRectBounds = bounds;
    updateInfo.updateRects = rects;
    updateInfo.scrollRect = m_scrollRect;
    updateInfo.scrollOffset = m_scrollOffset;
    m_scrollRect = { };
    m_scrollOffset = { };

    auto& graphicsContext = image->context();
    graphicsContext.translate(-bounds.x(), -bounds.y());
    for (const auto& rect : rects)
        webPage->drawRect(graphicsContext, rect);
    image->flushDrawingContextAsync();

    auto fence = m_webPage->ensureRemoteRenderingBackendProxy().flushImageBuffers();

    m_commitQueue->dispatch([this, weakThis = WeakPtr(*this), stateID = m_backingStoreStateID, updateInfo = WTFMove(updateInfo), image = WTFMove(image), fence = WTFMove(fence)]() mutable {
        fence();
        RunLoop::main().dispatch([this, weakThis = WTFMove(weakThis), stateID, updateInfo = WTFMove(updateInfo), image = WTFMove(image)]() mutable {
            if (!weakThis)
                return;
            if (stateID != m_backingStoreStateID) {
                displayDidRefresh();
                return;
            }

            auto* sharing = image->toBackendSharing();
            if (is<ImageBufferBackendHandleSharing>(sharing)) {
                if (auto handle = downcast<ImageBufferBackendHandleSharing>(*sharing).createBackendHandle())
                    updateInfo.bitmapHandle = std::get<ShareableBitmap::Handle>(WTFMove(*handle));
            }

            send(Messages::DrawingAreaProxy::Update(stateID, WTFMove(updateInfo)));
        });
    });
}

void DrawingAreaWC::graphicsLayerAdded(GraphicsLayerWC& layer)
{
    m_liveGraphicsLayers.append(&layer);
    m_updateInfo.addedLayers.append(*layer.primaryLayerID());
}

void DrawingAreaWC::graphicsLayerRemoved(GraphicsLayerWC& layer)
{
    m_liveGraphicsLayers.remove(&layer);
    m_updateInfo.removedLayers.append(*layer.primaryLayerID());
}

void DrawingAreaWC::commitLayerUpdateInfo(WCLayerUpdateInfo&& info)
{
    m_updateInfo.changedLayers.append(WTFMove(info));
}

RefPtr<ImageBuffer> DrawingAreaWC::createImageBuffer(FloatSize size, float deviceScaleFactor)
{
    if (WebProcess::singleton().shouldUseRemoteRenderingFor(RenderingPurpose::DOM))
        return Ref { m_webPage.get() }->ensureRemoteRenderingBackendProxy().createImageBuffer(size, RenderingMode::Unaccelerated, RenderingPurpose::DOM, deviceScaleFactor, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8);
    return ImageBuffer::create<ImageBufferShareableBitmapBackend>(size, deviceScaleFactor, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8, RenderingPurpose::DOM, { });
}

void DrawingAreaWC::displayDidRefresh()
{
    m_waitDidUpdate = false;
    didCompleteRenderingUpdateDisplay();
    didCompleteRenderingFrame();
    if (m_forceRepaintCompletionHandler) {
        if (m_isForceRepaintCompletionHandlerDeferred)
            m_isForceRepaintCompletionHandlerDeferred = false;
        else
            m_forceRepaintCompletionHandler();
    }
    if (m_hasDeferredRenderingUpdate) {
        m_hasDeferredRenderingUpdate = false;
        triggerRenderingUpdate();
    }
}

DrawingAreaWC::RootLayerInfo* DrawingAreaWC::rootLayerInfoWithFrameIdentifier(WebCore::FrameIdentifier frameID)
{
    auto index = m_rootLayers.findIf([&] (const auto& layer) {
        return layer.frameID == frameID;
    });
    if (index == WTF::notFound) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    return &m_rootLayers[index];
}

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_WC)
