/*
 * Copyright (C) 2009, 2013, 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "CACFLayerTreeHost.h"

#if USE(CA)

#include "CACFLayerTreeHostClient.h"
#include "DebugPageOverlays.h"
#include "DefWndProcWindowClass.h"
#include "Frame.h"
#include "FrameView.h"
#include "LayerChangesFlusher.h"
#include "Logging.h"
#include "PlatformCALayerWin.h"
#include "PlatformLayer.h"
#include "Settings.h"
#include "TiledBacking.h"
#include "WKCACFViewLayerTreeHost.h"
#include "WebCoreInstanceHandle.h"
#include <QuartzCore/CABase.h>
#include <limits.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UniqueArray.h>
#include <wtf/win/GDIObject.h>

#ifdef DEBUG_ALL
#pragma comment(lib, "QuartzCore_debug")
#else
#pragma comment(lib, "QuartzCore")
#endif

inline static CGRect winRectToCGRect(RECT rc)
{
    return CGRectMake(rc.left, rc.top, (rc.right - rc.left), (rc.bottom - rc.top));
}

inline static CGRect winRectToCGRect(RECT rc, RECT relativeToRect)
{
    return CGRectMake(rc.left, (relativeToRect.bottom-rc.bottom), (rc.right - rc.left), (rc.bottom - rc.top));
}

namespace WebCore {

bool CACFLayerTreeHost::acceleratedCompositingAvailable()
{
    static bool available;
    static bool tested;

    if (tested)
        return available;

    tested = true;

    // Initialize available to true since this function will be called from a 
    // propagation within createRenderer(). We want to be able to return true 
    // when that happens so that the test can continue.
    available = true;
    
    HMODULE library = LoadLibrary(TEXT("d3d9.dll"));
    if (!library) {
        available = false;
        return available;
    }

    FreeLibrary(library);
#ifdef DEBUG_ALL
    library = LoadLibrary(TEXT("QuartzCore_debug.dll"));
#else
    library = LoadLibrary(TEXT("QuartzCore.dll"));
#endif
    if (!library) {
        available = false;
        return available;
    }

    FreeLibrary(library);

    // Make a dummy HWND.
    HWND testWindow = ::CreateWindow(defWndProcWindowClassName(), L"CoreAnimationTesterWindow", WS_POPUP, -500, -500, 20, 20, 0, 0, 0, 0);

    if (!testWindow) {
        available = false;
        return available;
    }

    auto host = CACFLayerTreeHost::create();

    if (!host) {
        available = false;
        return available;
    }

    host->setWindow(testWindow);
    available = host->createRenderer();
    host->setWindow(0);
    ::DestroyWindow(testWindow);

    return available;
}

RefPtr<CACFLayerTreeHost> CACFLayerTreeHost::create()
{
    if (!acceleratedCompositingAvailable())
        return nullptr;
    auto host = WKCACFViewLayerTreeHost::create();
    if (!host) {
        LOG_ERROR("Failed to create layer tree host for accelerated compositing.");
        return nullptr;
    }
    host->initialize();
    return host;
}

CACFLayerTreeHost::CACFLayerTreeHost()
    : m_rootLayer(PlatformCALayerWin::create(PlatformCALayer::LayerTypeRootLayer, nullptr))
{
}

void CACFLayerTreeHost::initialize()
{
    // Point the CACFContext to this
    initializeContext(this, m_rootLayer.get());

    // Under the root layer, we have a clipping layer to clip the content,
    // that contains a scroll layer that we use for scrolling the content.
    // The root layer is the size of the client area of the window.
    // The clipping layer is the size of the WebView client area (window less the scrollbars).
    // The scroll layer is the size of the root child layer.
    // Resizing the window will change the bounds of the rootLayer and the clip layer and will not
    // cause any repositioning.
    // Scrolling will affect only the position of the scroll layer without affecting the bounds.

    m_rootLayer->setName(MAKE_STATIC_STRING_IMPL("CACFLayerTreeHost rootLayer"));
    m_rootLayer->setAnchorPoint(FloatPoint3D(0, 0, 0));
    m_rootLayer->setGeometryFlipped(true);

#ifndef NDEBUG
    auto debugColor = adoptCF(CGColorCreateGenericRGB(1, 0, 0, 0.8));
    m_rootLayer->setBackgroundColor(roundAndClampToSRGBALossy(debugColor.get()));
#endif
}

CACFLayerTreeHost::~CACFLayerTreeHost()
{
    ASSERT_WITH_MESSAGE(m_state != WindowSet, "Must call setWindow(0) before destroying CACFLayerTreeHost");
}

void CACFLayerTreeHost::setWindow(HWND window)
{
    if (window == m_window)
        return;

#if ASSERT_ENABLED
    switch (m_state) {
    case WindowNotSet:
        ASSERT_ARG(window, window);
        ASSERT(!m_window);
        m_state = WindowSet;
        break;
    case WindowSet:
        ASSERT_ARG(window, !window);
        ASSERT(m_window);
        m_state = WindowCleared;
        break;
    case WindowCleared:
        ASSERT_NOT_REACHED();
        break;
    }
#endif

    if (m_window)
        destroyRenderer();

    m_window = window;
}

void CACFLayerTreeHost::setPage(Page* page)
{
    m_page = page;
}

PlatformCALayer* CACFLayerTreeHost::rootLayer() const
{
    return m_rootLayer.get();
}

void CACFLayerTreeHost::addPendingAnimatedLayer(PlatformCALayer& layer)
{
    m_pendingAnimatedLayers.add(&layer);
}

void CACFLayerTreeHost::setRootChildLayer(PlatformCALayer* layer)
{
    m_rootLayer->removeAllSublayers();
    m_rootChildLayer = layer;
    if (m_rootChildLayer)
        m_rootLayer->appendSublayer(*m_rootChildLayer);
    updateDebugInfoLayer(m_page->settings().showTiledScrollingIndicator());
}
   
void CACFLayerTreeHost::layerTreeDidChange()
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
    LayerChangesFlusher::singleton().flushPendingLayerChangesSoon(this);
}

void CACFLayerTreeHost::destroyRenderer()
{
    m_rootLayer = nullptr;
    m_rootChildLayer = nullptr;
    LayerChangesFlusher::singleton().cancelPendingFlush(this);
}

static void getDirtyRects(HWND window, Vector<CGRect>& outRects)
{
    ASSERT_ARG(outRects, outRects.isEmpty());

    RECT clientRect;
    if (!GetClientRect(window, &clientRect))
        return;

    auto region = adoptGDIObject(::CreateRectRgn(0, 0, 0, 0));
    int regionType = GetUpdateRgn(window, region.get(), false);
    if (regionType != COMPLEXREGION) {
        RECT dirtyRect;
        if (GetUpdateRect(window, &dirtyRect, false))
            outRects.append(winRectToCGRect(dirtyRect, clientRect));
        return;
    }

    DWORD dataSize = ::GetRegionData(region.get(), 0, 0);
    auto regionDataBuffer = makeUniqueArray<unsigned char>(dataSize);
    RGNDATA* regionData = reinterpret_cast<RGNDATA*>(regionDataBuffer.get());
    if (!::GetRegionData(region.get(), dataSize, regionData))
        return;

    outRects.resize(regionData->rdh.nCount);

    RECT* rect = reinterpret_cast<RECT*>(regionData->Buffer);
    for (size_t i = 0; i < outRects.size(); ++i, ++rect)
        outRects[i] = winRectToCGRect(*rect, clientRect);
}

void CACFLayerTreeHost::paint(HDC dc)
{
    Vector<CGRect> dirtyRects;
    getDirtyRects(m_window, dirtyRects);
    render(dirtyRects, dc);
}

void CACFLayerTreeHost::flushPendingGraphicsLayerChangesSoon()
{
    m_shouldFlushPendingGraphicsLayerChanges = true;
    LayerChangesFlusher::singleton().flushPendingLayerChangesSoon(this);
}

void CACFLayerTreeHost::setShouldInvertColors(bool)
{
}

void CACFLayerTreeHost::flushPendingLayerChangesNow()
{
    // Calling out to the client could cause our last reference to go away.
    RefPtr<CACFLayerTreeHost> protectedThis(this);

    updateDebugInfoLayer(m_page->settings().showTiledScrollingIndicator());

    m_isFlushingLayerChanges = true;

    // Flush changes stored up in GraphicsLayers to their underlying PlatformCALayers, if
    // requested.
    if (m_client && m_shouldFlushPendingGraphicsLayerChanges) {
        m_shouldFlushPendingGraphicsLayerChanges = false;
        m_client->flushPendingGraphicsLayerChanges();
    }

    // Flush changes stored up in PlatformCALayers to the context so they will be rendered.
    flushContext();

    m_isFlushingLayerChanges = false;
}

void CACFLayerTreeHost::contextDidChange()
{
    // All pending animations will have been started with the flush. Fire the animationStarted calls.
    notifyAnimationsStarted();
}

void CACFLayerTreeHost::notifyAnimationsStarted()
{
    // Send currentTime to the pending animations. This function is called by CACF in a callback
    // which occurs after the drawInContext calls. So currentTime is very close to the time
    // the animations actually start
    MonotonicTime currentTime = MonotonicTime::now();

    HashSet<RefPtr<PlatformCALayer> >::iterator end = m_pendingAnimatedLayers.end();
    for (HashSet<RefPtr<PlatformCALayer> >::iterator it = m_pendingAnimatedLayers.begin(); it != end; ++it)
        (*it)->animationStarted(String(), currentTime);

    m_pendingAnimatedLayers.clear();
}

CGRect CACFLayerTreeHost::bounds() const
{
    RECT clientRect;
    GetClientRect(m_window, &clientRect);

    return winRectToCGRect(clientRect);
}

String CACFLayerTreeHost::layerTreeAsString() const
{
    if (!m_rootLayer)
        return emptyString();

    return m_rootLayer->layerTreeAsString();
}

TiledBacking* CACFLayerTreeHost::mainFrameTiledBacking() const
{
    if (!m_page)
        return nullptr;

    FrameView* frameView = m_page->mainFrame().view();
    if (!frameView)
        return nullptr;
    
    return frameView->tiledBacking();
}

void CACFLayerTreeHost::updateDebugInfoLayer(bool showLayer)
{
    if (showLayer) {
        if (!m_debugInfoLayer) {
            if (TiledBacking* tiledBacking = mainFrameTiledBacking())
                m_debugInfoLayer = tiledBacking->tiledScrollingIndicatorLayer();
        }

        if (m_debugInfoLayer) {
#ifndef NDEBUG
            m_debugInfoLayer->setName(MAKE_STATIC_STRING_IMPL("Debug Info"));
#endif
            m_rootLayer->appendSublayer(*m_debugInfoLayer);
        }
    } else if (m_debugInfoLayer) {
        m_debugInfoLayer->removeFromSuperlayer();
        m_debugInfoLayer = nullptr;
    }
}

}

#endif
