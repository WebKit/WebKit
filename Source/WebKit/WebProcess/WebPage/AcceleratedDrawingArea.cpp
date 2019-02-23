/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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
#include "AcceleratedDrawingArea.h"

#include "DrawingAreaProxyMessages.h"
#include "LayerTreeHost.h"
#include "UpdateInfo.h"
#include "WebPage.h"
#include "WebPageCreationParameters.h"
#include "WebPreferencesKeys.h"
#include <WebCore/Frame.h>
#include <WebCore/Page.h>
#include <WebCore/PageOverlayController.h>
#include <WebCore/Settings.h>

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace WebKit {
using namespace WebCore;

AcceleratedDrawingArea::~AcceleratedDrawingArea()
{
    discardPreviousLayerTreeHost();
    if (m_layerTreeHost)
        m_layerTreeHost->invalidate();
}

AcceleratedDrawingArea::AcceleratedDrawingArea(WebPage& webPage, const WebPageCreationParameters& parameters)
    : DrawingArea(DrawingAreaTypeImpl, parameters.drawingAreaIdentifier, webPage)
    , m_exitCompositingTimer(RunLoop::main(), this, &AcceleratedDrawingArea::exitAcceleratedCompositingMode)
    , m_discardPreviousLayerTreeHostTimer(RunLoop::main(), this, &AcceleratedDrawingArea::discardPreviousLayerTreeHost)
{
#if USE(GLIB_EVENT_LOOP)
    m_discardPreviousLayerTreeHostTimer.setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
#endif
    if (!m_webPage.isVisible())
        suspendPainting();

    m_webPage.corePage()->setDeviceScaleFactor(parameters.deviceScaleFactor);
}

void AcceleratedDrawingArea::setNeedsDisplay()
{
    if (!m_isPaintingEnabled)
        return;

    if (m_layerTreeHost)
        m_layerTreeHost->setNonCompositedContentsNeedDisplay();
}

void AcceleratedDrawingArea::setNeedsDisplayInRect(const IntRect& rect)
{
    if (!m_isPaintingEnabled)
        return;

    if (m_layerTreeHost)
        m_layerTreeHost->setNonCompositedContentsNeedDisplayInRect(rect);
}

void AcceleratedDrawingArea::scroll(const IntRect& scrollRect, const IntSize& scrollDelta)
{
    if (!m_isPaintingEnabled)
        return;

    if (m_layerTreeHost)
        m_layerTreeHost->scrollNonCompositedContents(scrollRect);
}

void AcceleratedDrawingArea::pageBackgroundTransparencyChanged()
{
    if (m_layerTreeHost)
        m_layerTreeHost->pageBackgroundTransparencyChanged();
    else if (m_previousLayerTreeHost)
        m_previousLayerTreeHost->pageBackgroundTransparencyChanged();
}

void AcceleratedDrawingArea::setLayerTreeStateIsFrozen(bool isFrozen)
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

void AcceleratedDrawingArea::forceRepaint()
{
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

bool AcceleratedDrawingArea::forceRepaintAsync(CallbackID callbackID)
{
    return m_layerTreeHost && m_layerTreeHost->forceRepaintAsync(callbackID);
}

void AcceleratedDrawingArea::setPaintingEnabled(bool paintingEnabled)
{
    m_isPaintingEnabled = paintingEnabled;
}

void AcceleratedDrawingArea::updatePreferences(const WebPreferencesStore& store)
{
    Settings& settings = m_webPage.corePage()->settings();
    bool forceCompositiongMode = store.getBoolValueForKey(WebPreferencesKey::forceCompositingModeKey());
    settings.setForceCompositingMode(forceCompositiongMode);
    settings.setAcceleratedCompositingForFixedPositionEnabled(forceCompositiongMode);
    if (!m_layerTreeHost)
        enterAcceleratedCompositingMode(nullptr);
}

void AcceleratedDrawingArea::mainFrameContentSizeChanged(const IntSize& size)
{
#if USE(COORDINATED_GRAPHICS_THREADED)
    if (m_layerTreeHost)
        m_layerTreeHost->contentsSizeChanged(size);
    else if (m_previousLayerTreeHost)
        m_previousLayerTreeHost->contentsSizeChanged(size);
#else
    UNUSED_PARAM(size);
#endif
}

void AcceleratedDrawingArea::layerHostDidFlushLayers()
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

GraphicsLayerFactory* AcceleratedDrawingArea::graphicsLayerFactory()
{
    if (!m_layerTreeHost)
        enterAcceleratedCompositingMode(nullptr);
    return m_layerTreeHost ? m_layerTreeHost->graphicsLayerFactory() : nullptr;
}

void AcceleratedDrawingArea::setRootCompositingLayer(GraphicsLayer* graphicsLayer)
{
    ASSERT(m_layerTreeHost);

    // FIXME: Instead of using nested if statements, we should keep a compositing state
    // enum in the AcceleratedDrawingArea object and have a changeAcceleratedCompositingState function
    // that takes the new state.

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
}

void AcceleratedDrawingArea::scheduleCompositingLayerFlush()
{
    if (m_layerTreeHost)
        m_layerTreeHost->scheduleLayerFlush();
}

void AcceleratedDrawingArea::scheduleInitialDeferredPaint()
{
}

void AcceleratedDrawingArea::scheduleCompositingLayerFlushImmediately()
{
    scheduleCompositingLayerFlush();
}

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
RefPtr<WebCore::DisplayRefreshMonitor> AcceleratedDrawingArea::createDisplayRefreshMonitor(WebCore::PlatformDisplayID displayID)
{
    if (!m_layerTreeHost || m_wantsToExitAcceleratedCompositingMode || exitAcceleratedCompositingModePending())
        return nullptr;
    return m_layerTreeHost->createDisplayRefreshMonitor(displayID);
}
#endif

void AcceleratedDrawingArea::updateBackingStoreState(uint64_t stateID, bool respondImmediately, float deviceScaleFactor, const IntSize& size, const IntSize& scrollOffset)
{
    ASSERT(!m_inUpdateBackingStoreState);
    m_inUpdateBackingStoreState = true;

    ASSERT_ARG(stateID, stateID >= m_backingStoreStateID);
    if (stateID != m_backingStoreStateID) {
        m_backingStoreStateID = stateID;
        m_shouldSendDidUpdateBackingStoreState = true;

        m_webPage.setDeviceScaleFactor(deviceScaleFactor);
        m_webPage.setSize(size);
        m_webPage.layoutIfNeeded();
        m_webPage.flushPendingEditorStateUpdate();
        m_webPage.scrollMainFrameIfNotAtMaxScrollPosition(scrollOffset);
        m_webPage.willDisplayPage();

        if (m_layerTreeHost)
            m_layerTreeHost->sizeDidChange(m_webPage.size());
        else if (m_previousLayerTreeHost)
            m_previousLayerTreeHost->sizeDidChange(m_webPage.size());
    } else {
        ASSERT(size == m_webPage.size());
        if (!m_shouldSendDidUpdateBackingStoreState) {
            // We've already sent a DidUpdateBackingStoreState message for this state. We have nothing more to do.
            m_inUpdateBackingStoreState = false;
            return;
        }
    }

    didUpdateBackingStoreState();

    if (respondImmediately) {
        // Make sure to resume painting if we're supposed to respond immediately, otherwise we'll just
        // send back an empty UpdateInfo struct.
        if (m_isPaintingSuspended)
            resumePainting();

        sendDidUpdateBackingStoreState();
    }

    m_inUpdateBackingStoreState = false;
}

void AcceleratedDrawingArea::sendDidUpdateBackingStoreState()
{
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

void AcceleratedDrawingArea::suspendPainting()
{
    ASSERT(!m_isPaintingSuspended);

    if (m_layerTreeHost)
        m_layerTreeHost->pauseRendering();

    m_isPaintingSuspended = true;

    m_webPage.corePage()->suspendScriptedAnimations();
}

void AcceleratedDrawingArea::resumePainting()
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

void AcceleratedDrawingArea::enterAcceleratedCompositingMode(GraphicsLayer* graphicsLayer)
{
    m_discardPreviousLayerTreeHostTimer.stop();

    m_exitCompositingTimer.stop();
    m_wantsToExitAcceleratedCompositingMode = false;

    ASSERT(!m_layerTreeHost);
    if (m_previousLayerTreeHost) {
#if USE(COORDINATED_GRAPHICS)
        m_layerTreeHost = WTFMove(m_previousLayerTreeHost);
        m_layerTreeHost->setIsDiscardable(false);
        if (!m_isPaintingSuspended)
            m_layerTreeHost->resumeRendering();
        if (!m_layerTreeStateIsFrozen)
            m_layerTreeHost->setLayerFlushSchedulingEnabled(true);
#else
        ASSERT_NOT_REACHED();
#endif
    } else {
        m_layerTreeHost = LayerTreeHost::create(m_webPage);

        if (!m_layerTreeHost)
            return;

        if (m_isPaintingSuspended)
            m_layerTreeHost->pauseRendering();
    }

#if USE(TEXTURE_MAPPER_GL) && PLATFORM(GTK) && PLATFORM(X11) && !USE(REDIRECTED_XCOMPOSITE_WINDOW)
    if (m_nativeSurfaceHandleForCompositing)
        m_layerTreeHost->setNativeSurfaceHandleForCompositing(m_nativeSurfaceHandleForCompositing);
#endif
    if (!m_inUpdateBackingStoreState)
        m_layerTreeHost->setShouldNotifyAfterNextScheduledLayerFlush(true);

    m_layerTreeHost->setRootCompositingLayer(graphicsLayer);
}

void AcceleratedDrawingArea::exitAcceleratedCompositingModeSoon()
{
    if (m_layerTreeStateIsFrozen) {
        m_wantsToExitAcceleratedCompositingMode = true;
        return;
    }

    if (exitAcceleratedCompositingModePending())
        return;

    m_exitCompositingTimer.startOneShot(0_s);
}

void AcceleratedDrawingArea::exitAcceleratedCompositingModeNow()
{
    ASSERT(!m_layerTreeStateIsFrozen);

    m_exitCompositingTimer.stop();
    m_wantsToExitAcceleratedCompositingMode = false;

#if USE(COORDINATED_GRAPHICS)
    ASSERT(m_layerTreeHost);
    m_previousLayerTreeHost = WTFMove(m_layerTreeHost);
    m_previousLayerTreeHost->setIsDiscardable(true);
    m_previousLayerTreeHost->pauseRendering();
    m_previousLayerTreeHost->setLayerFlushSchedulingEnabled(false);
    m_discardPreviousLayerTreeHostTimer.startOneShot(5_s);
#else
    m_layerTreeHost = nullptr;
#endif
}

void AcceleratedDrawingArea::discardPreviousLayerTreeHost()
{
    m_discardPreviousLayerTreeHostTimer.stop();
    if (!m_previousLayerTreeHost)
        return;

    m_previousLayerTreeHost->invalidate();
    m_previousLayerTreeHost = nullptr;
}

#if USE(TEXTURE_MAPPER_GL) && PLATFORM(GTK) && PLATFORM(X11) && !USE(REDIRECTED_XCOMPOSITE_WINDOW)
void AcceleratedDrawingArea::setNativeSurfaceHandleForCompositing(uint64_t handle)
{
    m_nativeSurfaceHandleForCompositing = handle;
    if (m_layerTreeHost) {
        m_webPage.corePage()->settings().setAcceleratedCompositingEnabled(true);
        m_layerTreeHost->setNativeSurfaceHandleForCompositing(handle);
    }
}

void AcceleratedDrawingArea::destroyNativeSurfaceHandleForCompositing(bool& handled)
{
    handled = true;
    setNativeSurfaceHandleForCompositing(0);
}
#endif

#if USE(COORDINATED_GRAPHICS_THREADED)
void AcceleratedDrawingArea::didChangeViewportAttributes(ViewportAttributes&& attrs)
{
    if (m_layerTreeHost)
        m_layerTreeHost->didChangeViewportAttributes(WTFMove(attrs));
    else if (m_previousLayerTreeHost)
        m_previousLayerTreeHost->didChangeViewportAttributes(WTFMove(attrs));
}
#endif

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
void AcceleratedDrawingArea::deviceOrPageScaleFactorChanged()
{
    if (m_layerTreeHost)
        m_layerTreeHost->deviceOrPageScaleFactorChanged();
    else if (m_previousLayerTreeHost)
        m_previousLayerTreeHost->deviceOrPageScaleFactorChanged();
}
#endif

void AcceleratedDrawingArea::activityStateDidChange(OptionSet<ActivityState::Flag> changed, ActivityStateChangeID, const Vector<CallbackID>&)
{
    if (changed & ActivityState::IsVisible) {
        if (m_webPage.isVisible())
            resumePainting();
        else
            suspendPainting();
    }
}

void AcceleratedDrawingArea::attachViewOverlayGraphicsLayer(GraphicsLayer* viewOverlayRootLayer)
{
    if (m_layerTreeHost)
        m_layerTreeHost->setViewOverlayRootLayer(viewOverlayRootLayer);
    else if (m_previousLayerTreeHost)
        m_previousLayerTreeHost->setViewOverlayRootLayer(viewOverlayRootLayer);
}

} // namespace WebKit
