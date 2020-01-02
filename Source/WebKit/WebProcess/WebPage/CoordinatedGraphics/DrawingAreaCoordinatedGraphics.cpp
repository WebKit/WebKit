/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2019 Igalia S.L.
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
#include "DrawingAreaCoordinatedGraphics.h"

#include "DrawingAreaProxyMessages.h"
#include "LayerTreeHost.h"
#include "ShareableBitmap.h"
#include "UpdateInfo.h"
#include "WebPage.h"
#include "WebPageCreationParameters.h"
#include "WebPreferencesKeys.h"
#include <WebCore/Frame.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/Page.h>
#include <WebCore/PageOverlayController.h>
#include <WebCore/Settings.h>

#if USE(DIRECT2D)
#include <WebCore/GraphicsContextImplDirect2D.h>
#include <WebCore/PlatformContextDirect2D.h>
#include <d2d1.h>
#include <d3d11_1.h>
#endif


#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace WebKit {
using namespace WebCore;

DrawingAreaCoordinatedGraphics::DrawingAreaCoordinatedGraphics(WebPage& webPage, const WebPageCreationParameters& parameters)
    : DrawingArea(DrawingAreaTypeCoordinatedGraphics, parameters.drawingAreaIdentifier, webPage)
    , m_exitCompositingTimer(RunLoop::main(), this, &DrawingAreaCoordinatedGraphics::exitAcceleratedCompositingMode)
    , m_discardPreviousLayerTreeHostTimer(RunLoop::main(), this, &DrawingAreaCoordinatedGraphics::discardPreviousLayerTreeHost)
    , m_supportsAsyncScrolling(parameters.store.getBoolValueForKey(WebPreferencesKey::threadedScrollingEnabledKey()))
    , m_displayTimer(RunLoop::main(), this, &DrawingAreaCoordinatedGraphics::displayTimerFired)
{
#if USE(GLIB_EVENT_LOOP)
    m_discardPreviousLayerTreeHostTimer.setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
#if !PLATFORM(WPE)
    m_displayTimer.setPriority(RunLoopSourcePriority::NonAcceleratedDrawingTimer);
#endif
#endif

#if ENABLE(DEVELOPER_MODE)
    if (m_supportsAsyncScrolling) {
        auto* disableAsyncScrolling = getenv("WEBKIT_DISABLE_ASYNC_SCROLLING");
        if (disableAsyncScrolling && strcmp(disableAsyncScrolling, "0"))
            m_supportsAsyncScrolling = false;
    }
#endif
}

DrawingAreaCoordinatedGraphics::~DrawingAreaCoordinatedGraphics() = default;

void DrawingAreaCoordinatedGraphics::setNeedsDisplay()
{
    if (m_layerTreeHost) {
        ASSERT(m_dirtyRegion.isEmpty());
        return;
    }

    setNeedsDisplayInRect(m_webPage.bounds());
}

void DrawingAreaCoordinatedGraphics::setNeedsDisplayInRect(const IntRect& rect)
{
    if (m_layerTreeHost) {
        ASSERT(m_dirtyRegion.isEmpty());
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

void DrawingAreaCoordinatedGraphics::scroll(const IntRect& scrollRect, const IntSize& scrollDelta)
{
    if (!m_isPaintingEnabled)
        return;

    if (m_layerTreeHost) {
        ASSERT(m_scrollRect.isEmpty());
        ASSERT(m_scrollOffset.isEmpty());
        ASSERT(m_dirtyRegion.isEmpty());
        m_layerTreeHost->scrollNonCompositedContents(scrollRect);
        return;
    }

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

void DrawingAreaCoordinatedGraphics::forceRepaint()
{
    if (m_inUpdateBackingStoreState) {
        m_forceRepaintAfterBackingStoreStateUpdate = true;
        return;
    }
    m_forceRepaintAfterBackingStoreStateUpdate = false;

    if (!m_layerTreeHost) {
        m_isWaitingForDidUpdate = false;
        if (m_isPaintingEnabled) {
            m_dirtyRegion = m_webPage.bounds();
            display();
        }
        return;
    }

    if (m_layerTreeStateIsFrozen)
        return;

    setNeedsDisplay();
    m_webPage.layoutIfNeeded();
    if (!m_layerTreeHost)
        return;

    // FIXME: We need to do the same work as the layerHostDidFlushLayers function here,
    // but clearly it doesn't make sense to call the function with that name.
    // Consider refactoring and renaming it.
    if (m_compositingAccordingToProxyMessages)
        m_layerTreeHost->forceRepaint();
    else {
        // Call setShouldNotifyAfterNextScheduledLayerFlush(false) here to
        // prevent layerHostDidFlushLayers() from being called a second time.
        m_layerTreeHost->setShouldNotifyAfterNextScheduledLayerFlush(false);
        layerHostDidFlushLayers();
    }
}

bool DrawingAreaCoordinatedGraphics::forceRepaintAsync(CallbackID callbackID)
{
    if (m_layerTreeStateIsFrozen)
        return false;

    return m_layerTreeHost && m_layerTreeHost->forceRepaintAsync(callbackID);
}

void DrawingAreaCoordinatedGraphics::setLayerTreeStateIsFrozen(bool isFrozen)
{
    if (m_layerTreeStateIsFrozen == isFrozen)
        return;

    m_layerTreeStateIsFrozen = isFrozen;

    if (m_layerTreeHost)
        m_layerTreeHost->setLayerFlushSchedulingEnabled(!isFrozen);

    if (isFrozen)
        m_exitCompositingTimer.stop();
    else if (m_wantsToExitAcceleratedCompositingMode)
        exitAcceleratedCompositingModeSoon();
}

void DrawingAreaCoordinatedGraphics::updatePreferences(const WebPreferencesStore& store)
{
    Settings& settings = m_webPage.corePage()->settings();
#if PLATFORM(WAYLAND) && USE(WPE_RENDERER)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::Wayland
        && &PlatformDisplay::sharedDisplayForCompositing() == &PlatformDisplay::sharedDisplay()) {
        // We failed to create the shared display for compositing, disable accelerated compositing.
        settings.setAcceleratedCompositingEnabled(false);
    }
#endif
    settings.setForceCompositingMode(store.getBoolValueForKey(WebPreferencesKey::forceCompositingModeKey()));
    // Fixed position elements need to be composited and create stacking contexts
    // in order to be scrolled by the ScrollingCoordinator.
    settings.setAcceleratedCompositingForFixedPositionEnabled(settings.acceleratedCompositingEnabled());

    m_alwaysUseCompositing = settings.acceleratedCompositingEnabled() && settings.forceCompositingMode();

    // If async scrolling is disabled, we have to force-disable async frame and overflow scrolling
    // to keep the non-async scrolling on those elements working.
    if (!m_supportsAsyncScrolling) {
        settings.setAsyncFrameScrollingEnabled(false);
        settings.setAsyncOverflowScrollingEnabled(false);
    }
}

void DrawingAreaCoordinatedGraphics::enablePainting()
{
    m_isPaintingEnabled = true;

    if (m_alwaysUseCompositing && !m_layerTreeHost)
        enterAcceleratedCompositingMode(nullptr);
}

void DrawingAreaCoordinatedGraphics::mainFrameContentSizeChanged(const IntSize& size)
{
    if (m_layerTreeHost)
        m_layerTreeHost->contentsSizeChanged(size);
    else if (m_previousLayerTreeHost)
        m_previousLayerTreeHost->contentsSizeChanged(size);
}

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
void DrawingAreaCoordinatedGraphics::deviceOrPageScaleFactorChanged()
{
    if (m_layerTreeHost)
        m_layerTreeHost->deviceOrPageScaleFactorChanged();
    else if (m_previousLayerTreeHost)
        m_previousLayerTreeHost->deviceOrPageScaleFactorChanged();
}

void DrawingAreaCoordinatedGraphics::didChangeViewportAttributes(ViewportAttributes&& attrs)
{
    if (m_layerTreeHost)
        m_layerTreeHost->didChangeViewportAttributes(WTFMove(attrs));
    else if (m_previousLayerTreeHost)
        m_previousLayerTreeHost->didChangeViewportAttributes(WTFMove(attrs));
}
#endif

bool DrawingAreaCoordinatedGraphics::supportsAsyncScrolling() const
{
    return m_supportsAsyncScrolling;
}

GraphicsLayerFactory* DrawingAreaCoordinatedGraphics::graphicsLayerFactory()
{
    if (!m_layerTreeHost)
        enterAcceleratedCompositingMode(nullptr);
    return m_layerTreeHost ? m_layerTreeHost->graphicsLayerFactory() : nullptr;
}

void DrawingAreaCoordinatedGraphics::setRootCompositingLayer(GraphicsLayer* graphicsLayer)
{
    if (m_layerTreeHost) {
        if (graphicsLayer) {
            // We're already in accelerated compositing mode, but the root compositing layer changed.
            m_exitCompositingTimer.stop();
            m_wantsToExitAcceleratedCompositingMode = false;

            // If we haven't sent the EnterAcceleratedCompositingMode message, make sure that the
            // layer tree host calls us back after the next layer flush so we can send it then.
            if (!m_compositingAccordingToProxyMessages)
                m_layerTreeHost->setShouldNotifyAfterNextScheduledLayerFlush(true);
        }
        m_layerTreeHost->setRootCompositingLayer(graphicsLayer);

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

void DrawingAreaCoordinatedGraphics::scheduleCompositingLayerFlush()
{
    if (m_layerTreeHost)
        m_layerTreeHost->scheduleLayerFlush();
    else
        setNeedsDisplay();
}

void DrawingAreaCoordinatedGraphics::layerHostDidFlushLayers()
{
    ASSERT(m_layerTreeHost);
    m_layerTreeHost->forceRepaint();

    if (m_shouldSendDidUpdateBackingStoreState && !exitAcceleratedCompositingModePending()) {
        sendDidUpdateBackingStoreState();
        return;
    }

    ASSERT(!m_compositingAccordingToProxyMessages);
    if (!exitAcceleratedCompositingModePending()) {
        send(Messages::DrawingAreaProxy::EnterAcceleratedCompositingMode(m_backingStoreStateID, m_layerTreeHost->layerTreeContext()));
        m_compositingAccordingToProxyMessages = true;
    }
}

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
RefPtr<DisplayRefreshMonitor> DrawingAreaCoordinatedGraphics::createDisplayRefreshMonitor(PlatformDisplayID displayID)
{
    if (!m_layerTreeHost || m_wantsToExitAcceleratedCompositingMode || exitAcceleratedCompositingModePending())
        return nullptr;
    return m_layerTreeHost->createDisplayRefreshMonitor(displayID);
}
#endif

void DrawingAreaCoordinatedGraphics::activityStateDidChange(OptionSet<ActivityState::Flag> changed, ActivityStateChangeID, const Vector<CallbackID>&)
{
    if (changed & ActivityState::IsVisible) {
        if (m_webPage.isVisible())
            resumePainting();
        else
            suspendPainting();
    }
}

void DrawingAreaCoordinatedGraphics::attachViewOverlayGraphicsLayer(GraphicsLayer* viewOverlayRootLayer)
{
    if (m_layerTreeHost)
        m_layerTreeHost->setViewOverlayRootLayer(viewOverlayRootLayer);
    else if (m_previousLayerTreeHost)
        m_previousLayerTreeHost->setViewOverlayRootLayer(viewOverlayRootLayer);
}

void DrawingAreaCoordinatedGraphics::updateBackingStoreState(uint64_t stateID, bool respondImmediately, float deviceScaleFactor, const IntSize& size, const IntSize& scrollOffset)
{
    if (stateID != m_backingStoreStateID && !m_layerTreeHost)
        m_dirtyRegion = IntRect(IntPoint(), size);

    ASSERT(!m_inUpdateBackingStoreState);
    m_inUpdateBackingStoreState = true;

    ASSERT_ARG(stateID, stateID >= m_backingStoreStateID);
    if (stateID != m_backingStoreStateID) {
        m_backingStoreStateID = stateID;
        m_shouldSendDidUpdateBackingStoreState = true;

        m_webPage.setDeviceScaleFactor(deviceScaleFactor);
        m_webPage.setSize(size);
        m_webPage.updateRendering();
        m_webPage.flushPendingEditorStateUpdate();
        m_webPage.scrollMainFrameIfNotAtMaxScrollPosition(scrollOffset);

        if (m_layerTreeHost)
            m_layerTreeHost->sizeDidChange(m_webPage.size());
        else if (m_previousLayerTreeHost)
            m_previousLayerTreeHost->sizeDidChange(m_webPage.size());
    } else {
        ASSERT(size == m_webPage.size());
        if (!m_shouldSendDidUpdateBackingStoreState) {
            // We've already sent a DidUpdateBackingStoreState message for this state. We have nothing more to do.
            m_inUpdateBackingStoreState = false;
            if (m_forceRepaintAfterBackingStoreStateUpdate)
                forceRepaint();
            return;
        }
    }

    // The UI process has updated to a new backing store state. Any Update messages we sent before
    // this point will be ignored. We wait to set this to false until after updating the page's
    // size so that any displays triggered by the relayout will be ignored. If we're supposed to
    // respond to the UpdateBackingStoreState message immediately, we'll do a display anyway in
    // sendDidUpdateBackingStoreState; otherwise we shouldn't do one right now.
    m_isWaitingForDidUpdate = false;

    if (respondImmediately) {
        // Make sure to resume painting if we're supposed to respond immediately, otherwise we'll just
        // send back an empty UpdateInfo struct.
        bool wasSuspended = m_isPaintingSuspended;
        if (m_isPaintingSuspended)
            resumePainting();

        sendDidUpdateBackingStoreState();
        if (wasSuspended)
            suspendPainting();
    }

    m_inUpdateBackingStoreState = false;

    if (m_forceRepaintAfterBackingStoreStateUpdate)
        forceRepaint();
}

void DrawingAreaCoordinatedGraphics::didUpdate()
{
    // We might get didUpdate messages from the UI process even after we've
    // entered accelerated compositing mode. Ignore them.
    if (m_layerTreeHost)
        return;

    m_isWaitingForDidUpdate = false;

    // Display if needed. We call displayTimerFired here since it will throttle updates to 60fps.
    displayTimerFired();
}

void DrawingAreaCoordinatedGraphics::sendDidUpdateBackingStoreState()
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

    ASSERT(m_shouldSendDidUpdateBackingStoreState);
    m_shouldSendDidUpdateBackingStoreState = false;

    UpdateInfo updateInfo;
    updateInfo.viewSize = m_webPage.size();
    updateInfo.deviceScaleFactor = m_webPage.corePage()->deviceScaleFactor();

    LayerTreeContext layerTreeContext;
    if (m_layerTreeHost) {
        layerTreeContext = m_layerTreeHost->layerTreeContext();

        // We don't want the layer tree host to notify after the next scheduled
        // layer flush because that might end up sending an EnterAcceleratedCompositingMode
        // message back to the UI process, but the updated layer tree context
        // will be sent back in the DidUpdateBackingStoreState message.
        m_layerTreeHost->setShouldNotifyAfterNextScheduledLayerFlush(false);
        m_layerTreeHost->forceRepaint();
    }

    send(Messages::DrawingAreaProxy::DidUpdateBackingStoreState(m_backingStoreStateID, updateInfo, layerTreeContext));
    m_compositingAccordingToProxyMessages = !layerTreeContext.isEmpty();
}

void DrawingAreaCoordinatedGraphics::exitAcceleratedCompositingModeSoon()
{
    if (m_layerTreeStateIsFrozen) {
        m_wantsToExitAcceleratedCompositingMode = true;
        return;
    }

    if (exitAcceleratedCompositingModePending())
        return;

    m_exitCompositingTimer.startOneShot(0_s);
}

void DrawingAreaCoordinatedGraphics::discardPreviousLayerTreeHost()
{
    m_discardPreviousLayerTreeHostTimer.stop();
    m_previousLayerTreeHost = nullptr;
}

void DrawingAreaCoordinatedGraphics::suspendPainting()
{
    ASSERT(!m_isPaintingSuspended);

    if (m_layerTreeHost)
        m_layerTreeHost->pauseRendering();
    else
        m_displayTimer.stop();

    m_isPaintingSuspended = true;

    m_webPage.corePage()->suspendScriptedAnimations();
}

void DrawingAreaCoordinatedGraphics::resumePainting()
{
    if (!m_isPaintingSuspended) {
        // FIXME: We can get a call to resumePainting when painting is not suspended.
        // This happens when sending a synchronous message to create a new page. See <rdar://problem/8976531>.
        return;
    }

    if (m_layerTreeHost)
        m_layerTreeHost->resumeRendering();

    m_isPaintingSuspended = false;

    // FIXME: We shouldn't always repaint everything here.
    setNeedsDisplay();

    m_webPage.corePage()->resumeScriptedAnimations();
}

void DrawingAreaCoordinatedGraphics::enterAcceleratedCompositingMode(GraphicsLayer* graphicsLayer)
{
#if PLATFORM(GTK)
    if (!m_alwaysUseCompositing) {
        m_webPage.corePage()->settings().setForceCompositingMode(true);
        m_alwaysUseCompositing = true;
    }
#endif
    m_discardPreviousLayerTreeHostTimer.stop();

    m_exitCompositingTimer.stop();
    m_wantsToExitAcceleratedCompositingMode = false;

    auto changeWindowScreen = [&] {
        // In order to ensure that we get a unique DisplayRefreshMonitor per-DrawingArea (necessary because ThreadedDisplayRefreshMonitor
        // is driven by the ThreadedCompositor of the drawing area), give each page a unique DisplayID derived from WebPage's unique ID.
        m_webPage.windowScreenDidChange(m_layerTreeHost->displayID());
    };

    ASSERT(!m_layerTreeHost);
    if (m_previousLayerTreeHost) {
        m_layerTreeHost = WTFMove(m_previousLayerTreeHost);
        changeWindowScreen();
        m_layerTreeHost->setIsDiscardable(false);
        m_layerTreeHost->resumeRendering();
        if (!m_layerTreeStateIsFrozen)
            m_layerTreeHost->setLayerFlushSchedulingEnabled(true);
    } else {
#if USE(COORDINATED_GRAPHICS)
        m_layerTreeHost = makeUnique<LayerTreeHost>(m_webPage);
        changeWindowScreen();
#else
        m_layerTreeHost = nullptr;
        return;
#endif
        if (m_isPaintingSuspended)
            m_layerTreeHost->pauseRendering();
    }

    if (!m_inUpdateBackingStoreState)
        m_layerTreeHost->setShouldNotifyAfterNextScheduledLayerFlush(true);

    m_layerTreeHost->setRootCompositingLayer(graphicsLayer);

    // Non-composited content will now be handled exclusively by the layer tree host.
    m_dirtyRegion = Region();
    m_scrollRect = IntRect();
    m_scrollOffset = IntSize();
    m_displayTimer.stop();
    m_isWaitingForDidUpdate = false;
}

void DrawingAreaCoordinatedGraphics::exitAcceleratedCompositingMode()
{
    if (m_alwaysUseCompositing)
        return;

    ASSERT(!m_layerTreeStateIsFrozen);

    m_exitCompositingTimer.stop();
    m_wantsToExitAcceleratedCompositingMode = false;

    ASSERT(m_layerTreeHost);
    m_previousLayerTreeHost = WTFMove(m_layerTreeHost);
    m_previousLayerTreeHost->setIsDiscardable(true);
    m_previousLayerTreeHost->pauseRendering();
    m_previousLayerTreeHost->setLayerFlushSchedulingEnabled(false);
    m_discardPreviousLayerTreeHostTimer.startOneShot(5_s);

    // Always use the primary display ID (0) when not in accelerated compositing mode.
    m_webPage.windowScreenDidChange(0);

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

void DrawingAreaCoordinatedGraphics::scheduleDisplay()
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

void DrawingAreaCoordinatedGraphics::displayTimerFired()
{
    display();
}

void DrawingAreaCoordinatedGraphics::display()
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

static bool shouldPaintBoundsRect(const IntRect& bounds, const Vector<IntRect, 1>& rects)
{
    const size_t rectThreshold = 10;
    const double wastedSpaceThreshold = 0.75;

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

void DrawingAreaCoordinatedGraphics::display(UpdateInfo& updateInfo)
{
    ASSERT(!m_isPaintingSuspended);
    ASSERT(!m_layerTreeHost);
    ASSERT(!m_webPage.size().isEmpty());

    m_webPage.updateRendering();
    m_webPage.flushPendingEditorStateUpdate();

    // The layout may have put the page into accelerated compositing mode. If the LayerTreeHost is
    // in charge of displaying, we have nothing more to do.
    if (m_layerTreeHost)
        return;

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

    auto rects = m_dirtyRegion.rects();
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
    if (graphicsContext) {
        graphicsContext->applyDeviceScaleFactor(deviceScaleFactor);
        graphicsContext->translate(-bounds.x(), -bounds.y());
    }

    updateInfo.updateRectBounds = bounds;

    for (const auto& rect : rects) {
        if (graphicsContext)
            m_webPage.drawRect(*graphicsContext, rect);
        updateInfo.updateRects.append(rect);
    }

#if USE(DIRECT2D)
    bitmap->leakSharedResource(); // It will be destroyed in the UIProcess.
#endif

    // Layout can trigger more calls to setNeedsDisplay and we don't want to process them
    // until the UI process has painted the update, so we stop the timer here.
    m_displayTimer.stop();
}

} // namespace WebKit
