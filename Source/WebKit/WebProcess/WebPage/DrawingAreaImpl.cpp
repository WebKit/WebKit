/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "DrawingAreaImpl.h"

#include "DrawingAreaProxyMessages.h"
#include "LayerTreeHost.h"
#include "ShareableBitmap.h"
#include "UpdateInfo.h"
#include "WebPage.h"
#include "WebPageCreationParameters.h"
#include "WebPreferencesKeys.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/Page.h>
#include <WebCore/Settings.h>

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace WebKit {
using namespace WebCore;

DrawingAreaImpl::~DrawingAreaImpl()
{
}

DrawingAreaImpl::DrawingAreaImpl(WebPage& webPage, const WebPageCreationParameters& parameters)
    : AcceleratedDrawingArea(webPage, parameters)
    , m_displayTimer(RunLoop::main(), this, &DrawingAreaImpl::displayTimerFired)
{
#if USE(GLIB_EVENT_LOOP)
    m_displayTimer.setPriority(RunLoopSourcePriority::NonAcceleratedDrawingTimer);
#endif
}

void DrawingAreaImpl::setNeedsDisplay()
{
    if (m_layerTreeHost) {
        ASSERT(m_dirtyRegion.isEmpty());
        AcceleratedDrawingArea::setNeedsDisplay();
        return;
    }

    setNeedsDisplayInRect(m_webPage.bounds());
}

void DrawingAreaImpl::setNeedsDisplayInRect(const IntRect& rect)
{
    if (m_layerTreeHost) {
        ASSERT(m_dirtyRegion.isEmpty());
        AcceleratedDrawingArea::setNeedsDisplayInRect(rect);
        return;
    }

    if (!m_isPaintingEnabled)
        return;

    IntRect dirtyRect = rect;
    dirtyRect.intersect(m_webPage.bounds());
    if (dirtyRect.isEmpty())
        return;

    m_dirtyRegion.unite(dirtyRect);
    scheduleDisplay();
}

void DrawingAreaImpl::scroll(const IntRect& scrollRect, const IntSize& scrollDelta)
{
    if (m_layerTreeHost) {
        ASSERT(m_scrollRect.isEmpty());
        ASSERT(m_scrollOffset.isEmpty());
        ASSERT(m_dirtyRegion.isEmpty());
        AcceleratedDrawingArea::scroll(scrollRect, scrollDelta);
        return;
    }

    if (!m_isPaintingEnabled)
        return;

    if (scrollRect.isEmpty())
        return;

    if (m_previousLayerTreeHost)
        m_previousLayerTreeHost->scrollNonCompositedContents(scrollRect);

    if (!m_scrollRect.isEmpty() && scrollRect != m_scrollRect) {
        unsigned scrollArea = scrollRect.width() * scrollRect.height();
        unsigned currentScrollArea = m_scrollRect.width() * m_scrollRect.height();

        if (currentScrollArea >= scrollArea) {
            // The rect being scrolled is at least as large as the rect we'd like to scroll.
            // Go ahead and just invalidate the scroll rect.
            setNeedsDisplayInRect(scrollRect);
            return;
        }

        // Just repaint the entire current scroll rect, we'll scroll the new rect instead.
        setNeedsDisplayInRect(m_scrollRect);
        m_scrollRect = IntRect();
        m_scrollOffset = IntSize();
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
    scheduleDisplay();

    m_scrollRect = scrollRect;
    m_scrollOffset += scrollDelta;
}

void DrawingAreaImpl::forceRepaint()
{
    if (m_inUpdateBackingStoreState) {
        m_forceRepaintAfterBackingStoreStateUpdate = true;
        return;
    }
    m_forceRepaintAfterBackingStoreStateUpdate = false;

    if (m_layerTreeHost) {
        AcceleratedDrawingArea::forceRepaint();
        return;
    }

    m_isWaitingForDidUpdate = false;
    if (m_isPaintingEnabled) {
        m_dirtyRegion = m_webPage.bounds();
        display();
    }
}

void DrawingAreaImpl::updatePreferences(const WebPreferencesStore& store)
{
    Settings& settings = m_webPage.corePage()->settings();
    settings.setForceCompositingMode(store.getBoolValueForKey(WebPreferencesKey::forceCompositingModeKey()));

#if USE(COORDINATED_GRAPHICS_THREADED)
    // Fixed position elements need to be composited and create stacking contexts
    // in order to be scrolled by the ScrollingCoordinator.
    settings.setAcceleratedCompositingForFixedPositionEnabled(settings.acceleratedCompositingEnabled());
#endif

    m_alwaysUseCompositing = settings.acceleratedCompositingEnabled() && settings.forceCompositingMode();
    if (m_alwaysUseCompositing && !m_layerTreeHost)
        enterAcceleratedCompositingMode(nullptr);
}

void DrawingAreaImpl::setRootCompositingLayer(GraphicsLayer* graphicsLayer)
{
    if (m_layerTreeHost) {
        AcceleratedDrawingArea::setRootCompositingLayer(graphicsLayer);

        if (!graphicsLayer && !m_alwaysUseCompositing) {
            // We'll exit accelerated compositing mode on a timer, to avoid re-entering
            // compositing code via display() and layout.
            // If we're leaving compositing mode because of a setSize, it is safe to
            // exit accelerated compositing mode right away.
            if (m_inUpdateBackingStoreState)
                exitAcceleratedCompositingMode();
            else
                exitAcceleratedCompositingModeSoon();
        }
        return;
    }

    if (!graphicsLayer)
        return;

    // We're actually entering accelerated compositing mode.
    enterAcceleratedCompositingMode(graphicsLayer);
}

void DrawingAreaImpl::updateBackingStoreState(uint64_t stateID, bool respondImmediately, float deviceScaleFactor, const WebCore::IntSize& size, const WebCore::IntSize& scrollOffset)
{
    if (stateID != m_backingStoreStateID && !m_layerTreeHost)
        m_dirtyRegion = IntRect(IntPoint(), size);

    AcceleratedDrawingArea::updateBackingStoreState(stateID, respondImmediately, deviceScaleFactor, size, scrollOffset);

    if (m_forceRepaintAfterBackingStoreStateUpdate)
        forceRepaint();
}

void DrawingAreaImpl::didUpdateBackingStoreState()
{
    // The UI process has updated to a new backing store state. Any Update messages we sent before
    // this point will be ignored. We wait to set this to false until after updating the page's
    // size so that any displays triggered by the relayout will be ignored. If we're supposed to
    // respond to the UpdateBackingStoreState message immediately, we'll do a display anyway in
    // sendDidUpdateBackingStoreState; otherwise we shouldn't do one right now.
    m_isWaitingForDidUpdate = false;
}

void DrawingAreaImpl::sendDidUpdateBackingStoreState()
{
    ASSERT(!m_isWaitingForDidUpdate);
    ASSERT(m_shouldSendDidUpdateBackingStoreState);

    if (!m_isPaintingSuspended && !m_layerTreeHost) {
        UpdateInfo updateInfo;
        display(updateInfo);
        if (!m_layerTreeHost) {
            m_shouldSendDidUpdateBackingStoreState = false;

            LayerTreeContext layerTreeContext;
            send(Messages::DrawingAreaProxy::DidUpdateBackingStoreState(m_backingStoreStateID, updateInfo, layerTreeContext));
            m_compositingAccordingToProxyMessages = false;
            return;
        }
    }

    AcceleratedDrawingArea::sendDidUpdateBackingStoreState();
}

void DrawingAreaImpl::didUpdate()
{
    // We might get didUpdate messages from the UI process even after we've
    // entered accelerated compositing mode. Ignore them.
    if (m_layerTreeHost)
        return;

    m_isWaitingForDidUpdate = false;

    // Display if needed. We call displayTimerFired here since it will throttle updates to 60fps.
    displayTimerFired();
}

void DrawingAreaImpl::suspendPainting()
{
    AcceleratedDrawingArea::suspendPainting();
    m_displayTimer.stop();
}

void DrawingAreaImpl::enterAcceleratedCompositingMode(GraphicsLayer* graphicsLayer)
{
    AcceleratedDrawingArea::enterAcceleratedCompositingMode(graphicsLayer);

    // Non-composited content will now be handled exclusively by the layer tree host.
    m_dirtyRegion = Region();
    m_scrollRect = IntRect();
    m_scrollOffset = IntSize();
    m_displayTimer.stop();
    m_isWaitingForDidUpdate = false;
}

void DrawingAreaImpl::exitAcceleratedCompositingMode()
{
    if (m_alwaysUseCompositing)
        return;

    AcceleratedDrawingArea::exitAcceleratedCompositingModeNow();
    m_dirtyRegion = m_webPage.bounds();

    if (m_inUpdateBackingStoreState)
        return;

    if (m_shouldSendDidUpdateBackingStoreState) {
        sendDidUpdateBackingStoreState();
        return;
    }

    UpdateInfo updateInfo;
    if (m_isPaintingSuspended) {
        updateInfo.viewSize = m_webPage.size();
        updateInfo.deviceScaleFactor = m_webPage.corePage()->deviceScaleFactor();
    } else
        display(updateInfo);

    // Send along a complete update of the page so we can paint the contents right after we exit the
    // accelerated compositing mode, eliminiating flicker.
    if (m_compositingAccordingToProxyMessages) {
        send(Messages::DrawingAreaProxy::ExitAcceleratedCompositingMode(m_backingStoreStateID, updateInfo));
        m_compositingAccordingToProxyMessages = false;
    } else {
        // If we left accelerated compositing mode before we sent an EnterAcceleratedCompositingMode message to the
        // UI process, we still need to let it know about the new contents, so send an Update message.
        send(Messages::DrawingAreaProxy::Update(m_backingStoreStateID, updateInfo));
    }
}

void DrawingAreaImpl::scheduleDisplay()
{
    ASSERT(!m_layerTreeHost);

    if (m_isWaitingForDidUpdate)
        return;

    if (m_isPaintingSuspended)
        return;

    if (m_dirtyRegion.isEmpty())
        return;

    if (m_displayTimer.isActive())
        return;

    m_displayTimer.startOneShot(0_s);
}

void DrawingAreaImpl::displayTimerFired()
{
    display();
}

void DrawingAreaImpl::display()
{
    ASSERT(!m_layerTreeHost);
    ASSERT(!m_isWaitingForDidUpdate);
    ASSERT(!m_inUpdateBackingStoreState);

    if (m_isPaintingSuspended)
        return;

    if (m_dirtyRegion.isEmpty())
        return;

    if (m_shouldSendDidUpdateBackingStoreState) {
        sendDidUpdateBackingStoreState();
        return;
    }

    UpdateInfo updateInfo;
    display(updateInfo);

    if (m_layerTreeHost) {
        // The call to update caused layout which turned on accelerated compositing.
        // Don't send an Update message in this case.
        return;
    }

    send(Messages::DrawingAreaProxy::Update(m_backingStoreStateID, updateInfo));
    m_isWaitingForDidUpdate = true;
}

static bool shouldPaintBoundsRect(const IntRect& bounds, const Vector<IntRect>& rects)
{
    const size_t rectThreshold = 10;
    const double wastedSpaceThreshold = 0.75;

    if (rects.size() <= 1 || rects.size() > rectThreshold)
        return true;

    // Attempt to guess whether or not we should use the region bounds rect or the individual rects.
    // We do this by computing the percentage of "wasted space" in the bounds.  If that wasted space
    // is too large, then we will do individual rect painting instead.
    unsigned boundsArea = bounds.width() * bounds.height();
    unsigned rectsArea = 0;
    for (size_t i = 0; i < rects.size(); ++i)
        rectsArea += rects[i].width() * rects[i].height();

    double wastedSpace = 1 - (static_cast<double>(rectsArea) / boundsArea);

    return wastedSpace <= wastedSpaceThreshold;
}

void DrawingAreaImpl::display(UpdateInfo& updateInfo)
{
    ASSERT(!m_isPaintingSuspended);
    ASSERT(!m_layerTreeHost);
    ASSERT(!m_webPage.size().isEmpty());

    m_webPage.layoutIfNeeded();
    m_webPage.flushPendingEditorStateUpdate();

    // The layout may have put the page into accelerated compositing mode. If the LayerTreeHost is
    // in charge of displaying, we have nothing more to do.
    if (m_layerTreeHost)
        return;

    m_webPage.willDisplayPage();
    updateInfo.viewSize = m_webPage.size();
    updateInfo.deviceScaleFactor = m_webPage.corePage()->deviceScaleFactor();

    IntRect bounds = m_dirtyRegion.bounds();
    ASSERT(m_webPage.bounds().contains(bounds));

    IntSize bitmapSize = bounds.size();
    float deviceScaleFactor = m_webPage.corePage()->deviceScaleFactor();
    bitmapSize.scale(deviceScaleFactor);
    auto bitmap = ShareableBitmap::createShareable(bitmapSize, { });
    if (!bitmap)
        return;

    if (!bitmap->createHandle(updateInfo.bitmapHandle))
        return;

    Vector<IntRect> rects = m_dirtyRegion.rects();
    if (shouldPaintBoundsRect(bounds, rects)) {
        rects.clear();
        rects.append(bounds);
    }

    updateInfo.scrollRect = m_scrollRect;
    updateInfo.scrollOffset = m_scrollOffset;

    m_dirtyRegion = Region();
    m_scrollRect = IntRect();
    m_scrollOffset = IntSize();

    auto graphicsContext = bitmap->createGraphicsContext();
    graphicsContext->applyDeviceScaleFactor(deviceScaleFactor);
    
    updateInfo.updateRectBounds = bounds;

    graphicsContext->translate(-bounds.x(), -bounds.y());

    for (const auto& rect : rects) {
        m_webPage.drawRect(*graphicsContext, rect);
        updateInfo.updateRects.append(rect);
    }

    // Layout can trigger more calls to setNeedsDisplay and we don't want to process them
    // until the UI process has painted the update, so we stop the timer here.
    m_displayTimer.stop();
}

} // namespace WebKit
