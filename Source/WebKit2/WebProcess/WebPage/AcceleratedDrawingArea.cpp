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
#include <WebCore/MainFrame.h>
#include <WebCore/Page.h>
#include <WebCore/PageOverlayController.h>
#include <WebCore/Settings.h>

using namespace WebCore;

namespace WebKit {

AcceleratedDrawingArea::~AcceleratedDrawingArea()
{
    if (m_layerTreeHost)
        m_layerTreeHost->invalidate();
}

AcceleratedDrawingArea::AcceleratedDrawingArea(WebPage& webPage, const WebPageCreationParameters& parameters)
#if USE(COORDINATED_GRAPHICS_MULTIPROCESS)
    : DrawingArea(DrawingAreaTypeCoordinated, webPage)
#else
    : DrawingArea(DrawingAreaTypeImpl, webPage)
#endif
    , m_exitCompositingTimer(RunLoop::main(), this, &AcceleratedDrawingArea::exitAcceleratedCompositingMode)
{
    if (!m_webPage.isVisible())
        suspendPainting();
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

bool AcceleratedDrawingArea::forceRepaintAsync(uint64_t callbackID)
{
    return m_layerTreeHost && m_layerTreeHost->forceRepaintAsync(callbackID);
}

void AcceleratedDrawingArea::setPaintingEnabled(bool paintingEnabled)
{
    m_isPaintingEnabled = paintingEnabled;
}

void AcceleratedDrawingArea::updatePreferences(const WebPreferencesStore& store)
{
    m_webPage.corePage()->settings().setForceCompositingMode(store.getBoolValueForKey(WebPreferencesKey::forceCompositingModeKey()));
    if (!m_layerTreeHost)
        enterAcceleratedCompositingMode(nullptr);
}

void AcceleratedDrawingArea::mainFrameContentSizeChanged(const IntSize& size)
{
    if (m_webPage.useFixedLayout() && m_layerTreeHost)
        m_layerTreeHost->sizeDidChange(size);
    m_webPage.mainFrame()->pageOverlayController().didChangeDocumentSize();
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
        m_webPage.send(Messages::DrawingAreaProxy::EnterAcceleratedCompositingMode(m_backingStoreStateID, m_layerTreeHost->layerTreeContext()));
        m_compositingAccordingToProxyMessages = true;
    }
}

GraphicsLayerFactory* AcceleratedDrawingArea::graphicsLayerFactory()
{
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

void AcceleratedDrawingArea::scheduleCompositingLayerFlushImmediately()
{
    scheduleCompositingLayerFlush();
}

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
        m_webPage.scrollMainFrameIfNotAtMaxScrollPosition(scrollOffset);

#if USE(COORDINATED_GRAPHICS_MULTIPROCESS)
        // Coordinated Graphics sets the size of the root layer to contents size.
        if (!m_webPage.useFixedLayout())
            m_layerTreeHost->sizeDidChange(m_webPage.size());
#else
        if (m_layerTreeHost)
            m_layerTreeHost->sizeDidChange(m_webPage.size());
#endif
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

    m_webPage.send(Messages::DrawingAreaProxy::DidUpdateBackingStoreState(m_backingStoreStateID, updateInfo, layerTreeContext));
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
    m_exitCompositingTimer.stop();
    m_wantsToExitAcceleratedCompositingMode = false;

    ASSERT(!m_layerTreeHost);
    m_layerTreeHost = LayerTreeHost::create(m_webPage);
#if USE(TEXTURE_MAPPER_GL) && PLATFORM(GTK) && PLATFORM(X11) && !USE(REDIRECTED_XCOMPOSITE_WINDOW)
    if (m_nativeSurfaceHandleForCompositing)
        m_layerTreeHost->setNativeSurfaceHandleForCompositing(m_nativeSurfaceHandleForCompositing);
#endif
    if (!m_inUpdateBackingStoreState)
        m_layerTreeHost->setShouldNotifyAfterNextScheduledLayerFlush(true);
    if (m_isPaintingSuspended)
        m_layerTreeHost->pauseRendering();

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

    m_exitCompositingTimer.startOneShot(0);
}

#if USE(COORDINATED_GRAPHICS_MULTIPROCESS)
void AcceleratedDrawingArea::didReceiveCoordinatedLayerTreeHostMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    m_layerTreeHost->didReceiveCoordinatedLayerTreeHostMessage(connection, decoder);
}
#endif

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

void AcceleratedDrawingArea::viewStateDidChange(ViewState::Flags changed, bool, const Vector<uint64_t>&)
{
    if (changed & ViewState::IsVisible) {
        if (m_webPage.isVisible())
            resumePainting();
        else
            suspendPainting();
    }
}

void AcceleratedDrawingArea::attachViewOverlayGraphicsLayer(Frame* frame, GraphicsLayer* viewOverlayRootLayer)
{
    if (!frame->isMainFrame())
        return;

    if (m_layerTreeHost)
        m_layerTreeHost->setViewOverlayRootLayer(viewOverlayRootLayer);
}

} // namespace WebKit
