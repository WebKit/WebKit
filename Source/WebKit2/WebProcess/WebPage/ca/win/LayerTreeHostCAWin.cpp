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

static HWND dummyWindow;
static LPCWSTR dummyWindowClass = L"LayerTreeHostCAWindowClass";
static size_t validLayerTreeHostCount;

static void registerDummyWindowClass()
{
    static bool didRegister;
    if (didRegister)
        return;
    didRegister = true;

    WNDCLASSW wndClass = {0};
    wndClass.lpszClassName = dummyWindowClass;
    wndClass.lpfnWndProc = ::DefWindowProcW;
    wndClass.hInstance = instanceHandle();

    ::RegisterClassW(&wndClass);
}

// This window is never shown. It is only needed so that D3D can determine the display mode, etc.
static HWND createDummyWindow()
{
    registerDummyWindowClass();
    return ::CreateWindowW(dummyWindowClass, 0, WS_POPUP, 0, 0, 10, 10, 0, 0, instanceHandle(), 0);
}

bool LayerTreeHostCAWin::supportsAcceleratedCompositing()
{
    static bool initialized;
    static bool supportsAcceleratedCompositing;
    if (initialized)
        return supportsAcceleratedCompositing;
    initialized = true;

    ASSERT(!dummyWindow);
    dummyWindow = createDummyWindow();
    RetainPtr<WKCACFViewRef> view(AdoptCF, WKCACFViewCreate(kWKCACFViewDrawingDestinationImage));
    CGRect fakeBounds = CGRectMake(0, 0, 10, 10);
    WKCACFViewUpdate(view.get(), dummyWindow, &fakeBounds);

    supportsAcceleratedCompositing = WKCACFViewCanDraw(view.get());

    WKCACFViewUpdate(view.get(), 0, 0);
    ::DestroyWindow(dummyWindow);
    dummyWindow = 0;

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
    , m_nextDisplayTime(0)
{
}

LayerTreeHostCAWin::~LayerTreeHostCAWin()
{
}

void LayerTreeHostCAWin::platformInitialize(LayerTreeContext&)
{
    ++validLayerTreeHostCount;
    if (!dummyWindow)
        dummyWindow = createDummyWindow();

    m_view.adoptCF(WKCACFViewCreate(kWKCACFViewDrawingDestinationImage));
    WKCACFViewSetContextUserData(m_view.get(), static_cast<AbstractCACFLayerTreeHost*>(this));
    WKCACFViewSetLayer(m_view.get(), rootLayer()->platformLayer());
    WKCACFViewSetContextDidChangeCallback(m_view.get(), contextDidChangeCallback, this);

    CGRect bounds = m_webPage->bounds();
    WKCACFViewUpdate(m_view.get(), dummyWindow, &bounds);
}

void LayerTreeHostCAWin::invalidate()
{
    LayerChangesFlusher::shared().cancelPendingFlush(this);

    WKCACFViewUpdate(m_view.get(), 0, 0);
    WKCACFViewSetContextUserData(m_view.get(), 0);
    WKCACFViewSetLayer(m_view.get(), 0);
    WKCACFViewSetContextDidChangeCallback(m_view.get(), 0, 0);

    LayerTreeHostCA::invalidate();

    if (--validLayerTreeHostCount)
        return;
    ::DestroyWindow(dummyWindow);
    dummyWindow = 0;
}

void LayerTreeHostCAWin::scheduleLayerFlush()
{
    LayerChangesFlusher::shared().flushPendingLayerChangesSoon(this);
}

bool LayerTreeHostCAWin::participatesInDisplay()
{
    return true;
}

bool LayerTreeHostCAWin::needsDisplay()
{
    return timeUntilNextDisplay() <= 0;
}

double LayerTreeHostCAWin::timeUntilNextDisplay()
{
    return m_nextDisplayTime - currentTime();
}

static IntSize size(WKCACFImageRef image)
{
    return IntSize(WKCACFImageGetWidth(image), WKCACFImageGetHeight(image));
}

static PassRefPtr<ShareableBitmap> toShareableBitmap(WKCACFImageRef image)
{
    size_t fileMappingSize;
    HANDLE mapping = WKCACFImageCopyFileMapping(image, &fileMappingSize);
    if (!mapping)
        return 0;

    RefPtr<SharedMemory> sharedMemory = SharedMemory::adopt(mapping, fileMappingSize, SharedMemory::ReadWrite);
    if (!sharedMemory) {
        ::CloseHandle(mapping);
        return 0;
    }

    // WKCACFImage never has an alpha channel.
    return ShareableBitmap::create(size(image), 0, sharedMemory.release());
}

void LayerTreeHostCAWin::display(UpdateInfo& updateInfo)
{
    CGPoint imageOrigin;
    CFTimeInterval nextDrawTime;
    RetainPtr<WKCACFImageRef> image(AdoptCF, WKCACFViewCopyDrawnImage(m_view.get(), &imageOrigin, &nextDrawTime));
    m_nextDisplayTime = nextDrawTime - CACurrentMediaTime() + currentTime();
    if (!image)
        return;
    RefPtr<ShareableBitmap> bitmap = toShareableBitmap(image.get());
    if (!bitmap)
        return;
    if (!bitmap->createHandle(updateInfo.bitmapHandle))
        return;
    updateInfo.updateRectBounds = IntRect(IntPoint(imageOrigin.x, m_webPage->size().height() - imageOrigin.y - bitmap->size().height()), bitmap->size());
    updateInfo.updateRects.append(updateInfo.updateRectBounds);
}

void LayerTreeHostCAWin::sizeDidChange(const IntSize& newSize)
{
    LayerTreeHostCA::sizeDidChange(newSize);
    CGRect bounds = CGRectMake(0, 0, newSize.width(), newSize.height());
    WKCACFViewUpdate(m_view.get(), dummyWindow, &bounds);
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

    m_nextDisplayTime = 0;
    static_cast<DrawingAreaImpl*>(m_webPage->drawingArea())->setLayerHostNeedsDisplay();
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

} // namespace WebKit

#endif // HAVE(WKQCA)
