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
#include "PlatformImageBufferShareableBackend.h"
#include "RemoteRenderingBackendProxy.h"
#include "RemoteWCLayerTreeHostProxy.h"
#include "UpdateInfo.h"
#include "WebFrame.h"
#include "WebPageCreationParameters.h"
#include "WebProcess.h"
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/ImageBuffer.h>

namespace WebKit {
using namespace WebCore;

DrawingAreaWC::DrawingAreaWC(WebPage& webPage, const WebPageCreationParameters& parameters)
    : DrawingArea(DrawingAreaType::WC, parameters.drawingAreaIdentifier, webPage)
    , m_remoteWCLayerTreeHostProxy(makeUnique<RemoteWCLayerTreeHostProxy>(webPage, parameters.usesOffscreenRendering))
    , m_layerFactory(*this)
    , m_rootLayer(GraphicsLayer::create(graphicsLayerFactory(), this->m_rootLayerClient))
    , m_updateRenderingTimer(*this, &DrawingAreaWC::updateRendering)
    , m_commitQueue(WorkQueue::create("DrawingAreaWC CommitQueue"_s))
{
    m_rootLayer->setName(MAKE_STATIC_STRING_IMPL("drawing area root"));
    m_rootLayer->setSize(m_webPage.size());
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
    Vector<Ref<GraphicsLayer>> children;
    if (m_contentLayer) {
        children.append(*m_contentLayer);
        if (m_viewOverlayRootLayer)
            children.append(*m_viewOverlayRootLayer);
    }
    m_rootLayer->setChildren(WTFMove(children));
    triggerRenderingUpdate();
}

void DrawingAreaWC::setRootCompositingLayer(GraphicsLayer* rootLayer)
{
    m_contentLayer = rootLayer;
    if (rootLayer)
        send(Messages::DrawingAreaProxy::EnterAcceleratedCompositingMode(m_backingStoreStateID, { }));
    updateRootLayers();
}

void DrawingAreaWC::attachViewOverlayGraphicsLayer(GraphicsLayer* layer)
{
    m_viewOverlayRootLayer = layer;
    updateRootLayers();
}

void DrawingAreaWC::updatePreferences(const WebPreferencesStore&)
{
    Settings& settings = m_webPage.corePage()->settings();
    settings.setAcceleratedCompositingForFixedPositionEnabled(settings.acceleratedCompositingEnabled());
}

bool DrawingAreaWC::shouldUseTiledBackingForFrameView(const WebCore::FrameView& frameView) const
{
    auto* localFrame = dynamicDowncast<WebCore::LocalFrame>(frameView.frame());
    return localFrame && localFrame->isMainFrame();
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

void DrawingAreaWC::updateGeometry(uint64_t backingStoreStateID, IntSize viewSize)
{
    m_backingStoreStateID = backingStoreStateID;
    m_webPage.setSize(viewSize);
    m_rootLayer->setSize(m_webPage.size());
}

void DrawingAreaWC::setNeedsDisplay()
{
    m_dirtyRegion = m_webPage.bounds();
    m_scrollRect = { };
    m_scrollOffset = { };
    triggerRenderingUpdate();
}

void DrawingAreaWC::setNeedsDisplayInRect(const IntRect& rect)
{
    if (isCompositingMode())
        return;
    IntRect dirtyRect = rect;
    dirtyRect.intersect(m_webPage.bounds());
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

void DrawingAreaWC::forceRepaintAsync(WebPage&, CompletionHandler<void()>&& completionHandler)
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

static void flushLayerImageBuffers(WCUpateInfo& info)
{
    for (auto& layerInfo : info.changedLayers) {
        if (layerInfo.changes & WCLayerChange::Background) {
            for (auto& tileUpdate : layerInfo.tileUpdate) {
                if (auto image = tileUpdate.backingStore.imageBuffer()) {
                    if (auto flusher = image->createFlusher())
                        flusher->flush();
                }
            }
        }
    }
}

bool DrawingAreaWC::isCompositingMode()
{
    return m_contentLayer;
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

    m_webPage.updateRendering();
    m_webPage.flushPendingEditorStateUpdate();

    OptionSet<FinalizeRenderingUpdateFlags> flags;
    m_webPage.finalizeRenderingUpdate(flags);
    m_webPage.didUpdateRendering();

    if (isCompositingMode())
        sendUpdateAC();
    else
        sendUpdateNonAC();
}

void DrawingAreaWC::sendUpdateAC()
{
    m_rootLayer->flushCompositingStateForThisLayerOnly();

    // Because our view-relative overlay root layer is not attached to the FrameView's GraphicsLayer tree, we need to flush it manually.
    if (m_viewOverlayRootLayer) {
        FloatRect visibleRect({ }, m_webPage.size());
        m_viewOverlayRootLayer->flushCompositingState(visibleRect);
    }

    m_updateInfo.rootLayer = m_rootLayer->primaryLayerID();

    m_commitQueue->dispatch([this, weakThis = WeakPtr(*this), stateID = m_backingStoreStateID, updateInfo = std::exchange(m_updateInfo, { })]() mutable {
        flushLayerImageBuffers(updateInfo);
        RunLoop::main().dispatch([this, weakThis = WTFMove(weakThis), stateID, updateInfo = WTFMove(updateInfo)]() mutable {
            if (!weakThis)
                return;
            m_remoteWCLayerTreeHostProxy->update(WTFMove(updateInfo), [this, weakThis = WTFMove(weakThis), stateID](std::optional<UpdateInfo> updateInfo) {
                if (!weakThis)
                    return;
                if (updateInfo && stateID == m_backingStoreStateID) {
                    send(Messages::DrawingAreaProxy::Update(m_backingStoreStateID, WTFMove(*updateInfo)));
                    return;
                }
                displayDidRefresh();
            });
        });
    });
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
    ASSERT(m_webPage.bounds().contains(bounds));
    IntSize bitmapSize = bounds.size();
    float deviceScaleFactor = m_webPage.corePage()->deviceScaleFactor();
    bitmapSize.scale(deviceScaleFactor);

    auto image = createImageBuffer(bitmapSize);
    auto rects = m_dirtyRegion.rects();
    if (shouldPaintBoundsRect(bounds, rects)) {
        rects.clear();
        rects.append(bounds);
    }
    m_dirtyRegion = { };

    UpdateInfo updateInfo;
    updateInfo.viewSize = m_webPage.size();
    updateInfo.deviceScaleFactor = m_webPage.corePage()->deviceScaleFactor();
    updateInfo.updateRectBounds = bounds;
    updateInfo.updateRects = rects;
    updateInfo.scrollRect = m_scrollRect;
    updateInfo.scrollOffset = m_scrollOffset;
    m_scrollRect = { };
    m_scrollOffset = { };

    auto& graphicsContext = image->context();
    graphicsContext.applyDeviceScaleFactor(deviceScaleFactor);
    graphicsContext.translate(-bounds.x(), -bounds.y());
    for (const auto& rect : rects)
        m_webPage.drawRect(image->context(), rect);
    image->flushDrawingContextAsync();

    m_commitQueue->dispatch([this, weakThis = WeakPtr(*this), stateID = m_backingStoreStateID, updateInfo = WTFMove(updateInfo), image = WTFMove(image)]() mutable {
        if (auto flusher = image->createFlusher())
            flusher->flush();
        RunLoop::main().dispatch([this, weakThis = WTFMove(weakThis), stateID, updateInfo = WTFMove(updateInfo), image = WTFMove(image)]() mutable {
            if (!weakThis)
                return;
            if (stateID != m_backingStoreStateID) {
                displayDidRefresh();
                return;
            }

            ImageBufferBackendHandle handle;
            if (auto* backend = image->ensureBackendCreated()) {
                auto* sharing = backend->toBackendSharing();
                if (is<ImageBufferBackendHandleSharing>(sharing))
                    handle = downcast<ImageBufferBackendHandleSharing>(*sharing).createBackendHandle();
            }
            updateInfo.bitmapHandle = std::get<ShareableBitmapHandle>(WTFMove(handle));
            send(Messages::DrawingAreaProxy::Update(stateID, WTFMove(updateInfo)));
        });
    });
}

void DrawingAreaWC::graphicsLayerAdded(GraphicsLayerWC& layer)
{
    m_liveGraphicsLayers.append(&layer);
    m_updateInfo.addedLayers.append(layer.primaryLayerID());
}

void DrawingAreaWC::graphicsLayerRemoved(GraphicsLayerWC& layer)
{
    m_liveGraphicsLayers.remove(&layer);
    m_updateInfo.removedLayers.append(layer.primaryLayerID());
}

void DrawingAreaWC::commitLayerUpateInfo(WCLayerUpateInfo&& info)
{
    m_updateInfo.changedLayers.append(WTFMove(info));
}

RefPtr<ImageBuffer> DrawingAreaWC::createImageBuffer(FloatSize size)
{
    if (WebProcess::singleton().shouldUseRemoteRenderingFor(RenderingPurpose::DOM))
        return m_webPage.ensureRemoteRenderingBackendProxy().createImageBuffer(size, RenderingMode::Unaccelerated, RenderingPurpose::DOM, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    return ImageBuffer::create<UnacceleratedImageBufferShareableBackend>(size, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8, RenderingPurpose::DOM, nullptr);
}

void DrawingAreaWC::displayDidRefresh()
{
    m_waitDidUpdate = false;
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

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_WC)
