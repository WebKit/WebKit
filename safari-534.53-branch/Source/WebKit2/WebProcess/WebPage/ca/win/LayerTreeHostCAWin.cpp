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
#include "LayerTreeHostCAWin.h"

#if HAVE(WKQCA)

#include "DrawingAreaImpl.h"
#include "ShareableBitmap.h"
#include "UpdateInfo.h"
#include "WKCACFViewWindow.h"
#include "WebPage.h"
#include <WebCore/GraphicsLayerCA.h>
#include <WebCore/LayerChangesFlusher.h>
#include <WebCore/PlatformCALayer.h>
#include <WebCore/WebCoreInstanceHandle.h>
#include <WebKitQuartzCoreAdditions/WKCACFImage.h>
#include <WebKitQuartzCoreAdditions/WKCACFView.h>
#include <wtf/CurrentTime.h>
#include <wtf/Threading.h>

#ifdef DEBUG_ALL
#pragma comment(lib, "WebKitQuartzCoreAdditions_debug")
#else
#pragma comment(lib, "WebKitQuartzCoreAdditions")
#endif

using namespace WebCore;

namespace WebKit {

bool LayerTreeHostCAWin::supportsAcceleratedCompositing()
{
    static bool initialized;
    static bool supportsAcceleratedCompositing;
    if (initialized)
        return supportsAcceleratedCompositing;
    initialized = true;

    RetainPtr<WKCACFViewRef> view(AdoptCF, WKCACFViewCreate(kWKCACFViewDrawingDestinationWindow));
    WKCACFViewWindow dummyWindow(view.get(), 0, 0);
    CGRect fakeBounds = CGRectMake(0, 0, 10, 10);
    WKCACFViewUpdate(view.get(), dummyWindow.window(), &fakeBounds);

    supportsAcceleratedCompositing = WKCACFViewCanDraw(view.get());

    WKCACFViewUpdate(view.get(), 0, 0);

    return supportsAcceleratedCompositing;
}

PassRefPtr<LayerTreeHostCAWin> LayerTreeHostCAWin::create(WebPage* webPage)
{
    RefPtr<LayerTreeHostCAWin> host = adoptRef(new LayerTreeHostCAWin(webPage));
    host->initialize();
    return host.release();
}

LayerTreeHostCAWin::LayerTreeHostCAWin(WebPage* webPage)
    : LayerTreeHostCA(webPage)
    , m_isFlushingLayerChanges(false)
{
}

LayerTreeHostCAWin::~LayerTreeHostCAWin()
{
}

void LayerTreeHostCAWin::platformInitialize(LayerTreeContext& context)
{
    m_view.adoptCF(WKCACFViewCreate(kWKCACFViewDrawingDestinationWindow));
    WKCACFViewSetContextUserData(m_view.get(), static_cast<AbstractCACFLayerTreeHost*>(this));
    WKCACFViewSetLayer(m_view.get(), rootLayer()->platformLayer());
    WKCACFViewSetContextDidChangeCallback(m_view.get(), contextDidChangeCallback, this);

    // Passing WS_DISABLED makes the window invisible to mouse events, which lets WKView's normal
    // event handling mechanism work even when this window is obscuring the entire WKView HWND.
    // Note that m_webPage->nativeWindow() is owned by the UI process, so this creates a cross-
    // process window hierarchy (and thus implicitly attaches the input queues of the UI and web
    // processes' main threads).
    m_window = adoptPtr(new WKCACFViewWindow(m_view.get(), m_webPage->nativeWindow(), WS_DISABLED));

    CGRect bounds = m_webPage->bounds();
    WKCACFViewUpdate(m_view.get(), m_window->window(), &bounds);

    context.window = m_window->window();
}

void LayerTreeHostCAWin::invalidate()
{
    LayerChangesFlusher::shared().cancelPendingFlush(this);

    WKCACFViewSetContextUserData(m_view.get(), 0);
    WKCACFViewSetLayer(m_view.get(), 0);
    WKCACFViewSetContextDidChangeCallback(m_view.get(), 0, 0);

    // The UI process will destroy m_window's HWND when it gets the message to switch out of
    // accelerated compositing mode. We don't want to destroy the HWND before then or we will get a
    // flash of white before the UI process has a chance to display the non-composited content.
    // Since the HWND needs to outlive us, we leak m_window here and tell it to clean itself up
    // when its HWND is destroyed.
    WKCACFViewWindow* window = m_window.leakPtr();
    window->setDeletesSelfWhenWindowDestroyed(true);

    LayerTreeHostCA::invalidate();
}

void LayerTreeHostCAWin::scheduleLayerFlush()
{
    if (!m_layerFlushSchedulingEnabled)
        return;

    LayerChangesFlusher::shared().flushPendingLayerChangesSoon(this);
}

void LayerTreeHostCAWin::setLayerFlushSchedulingEnabled(bool layerFlushingEnabled)
{
    if (m_layerFlushSchedulingEnabled == layerFlushingEnabled)
        return;

    m_layerFlushSchedulingEnabled = layerFlushingEnabled;

    if (m_layerFlushSchedulingEnabled)
        return;

    LayerChangesFlusher::shared().cancelPendingFlush(this);
}

void LayerTreeHostCAWin::scheduleChildWindowGeometryUpdate(const WindowGeometry& geometry)
{
    m_geometriesUpdater.addPendingUpdate(geometry);
}

void LayerTreeHostCAWin::sizeDidChange(const IntSize& newSize)
{
    LayerTreeHostCA::sizeDidChange(newSize);
    CGRect bounds = CGRectMake(0, 0, newSize.width(), newSize.height());
    WKCACFViewUpdate(m_view.get(), m_window->window(), &bounds);
    WKCACFViewFlushContext(m_view.get());
}

void LayerTreeHostCAWin::forceRepaint()
{
    LayerTreeHostCA::forceRepaint();
    WKCACFViewFlushContext(m_view.get());
}

void LayerTreeHostCAWin::contextDidChangeCallback(WKCACFViewRef view, void* info)
{
    // This should only be called on a background thread when no changes have actually 
    // been committed to the context, eg. when a video frame has been added to an image
    // queue, so return without triggering animations etc.
    if (!isMainThread())
        return;
    
    LayerTreeHostCAWin* host = static_cast<LayerTreeHostCAWin*>(info);
    ASSERT_ARG(view, view == host->m_view);
    host->contextDidChange();
}

void LayerTreeHostCAWin::contextDidChange()
{
    // Send currentTime to the pending animations. This function is called by CACF in a callback
    // which occurs after the drawInContext calls. So currentTime is very close to the time
    // the animations actually start
    double currentTime = WTF::currentTime();

    HashSet<RefPtr<PlatformCALayer> >::iterator end = m_pendingAnimatedLayers.end();
    for (HashSet<RefPtr<PlatformCALayer> >::iterator it = m_pendingAnimatedLayers.begin(); it != end; ++it)
        (*it)->animationStarted(currentTime);

    m_pendingAnimatedLayers.clear();

    // Update child window geometries now so that they stay mostly in sync with the accelerated content.
    // FIXME: We should really be doing this when the changes we just flushed appear on screen. <http://webkit.org/b/61867>
    // We also bring the child windows (i.e., plugins) to the top of the z-order to ensure they are above m_window.
    // FIXME: We could do this just once per window when it is first shown. Maybe that would be more efficient?
    m_geometriesUpdater.updateGeometries(BringToTop);
}

PlatformCALayer* LayerTreeHostCAWin::rootLayer() const
{
    return static_cast<GraphicsLayerCA*>(LayerTreeHostCA::rootLayer())->platformCALayer();
}

void LayerTreeHostCAWin::addPendingAnimatedLayer(PassRefPtr<PlatformCALayer> layer)
{
    m_pendingAnimatedLayers.add(layer);
}

void LayerTreeHostCAWin::layerTreeDidChange()
{
    if (m_isFlushingLayerChanges) {
        // The layer tree is changing as a result of flushing GraphicsLayer changes to their
        // underlying PlatformCALayers. We'll flush those changes to the context as part of that
        // process, so there's no need to schedule another flush here.
        return;
    }

    // The layer tree is changing as a result of someone modifying a PlatformCALayer that doesn't
    // have a corresponding GraphicsLayer. Schedule a flush since we won't schedule one through the
    // normal GraphicsLayer mechanisms.
    LayerChangesFlusher::shared().flushPendingLayerChangesSoon(this);
}

void LayerTreeHostCAWin::flushPendingLayerChangesNow()
{
    RefPtr<LayerTreeHostCA> protector(this);

    m_isFlushingLayerChanges = true;

    // Flush changes stored up in GraphicsLayers to their underlying PlatformCALayers, if
    // requested.
    performScheduledLayerFlush();

    // Flush changes stored up in PlatformCALayers to the context so they will be rendered.
    WKCACFViewFlushContext(m_view.get());

    m_isFlushingLayerChanges = false;
}

void LayerTreeHostCAWin::setRootCompositingLayer(GraphicsLayer* graphicsLayer)
{
    // Don't flush any changes when we don't have a root layer. This will prevent flashes of white
    // when switching out of compositing mode.
    setLayerFlushSchedulingEnabled(graphicsLayer);

    // Resubmit all existing animations. CACF does not remember running animations
    // When the layer tree is removed and then added back to the hierarchy
    if (graphicsLayer)
        static_cast<GraphicsLayerCA*>(graphicsLayer)->platformCALayer()->ensureAnimationsSubmitted();

    LayerTreeHostCA::setRootCompositingLayer(graphicsLayer);
}

} // namespace WebKit

#endif // HAVE(WKQCA)
