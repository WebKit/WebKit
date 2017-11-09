/*
* Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "CAView.h"

#include "CVDisplayLink.h"
#include "Image.h"
#include "WebKitQuartzCoreAdditionsInternal.h"
#include <QuartzCore/CACFContext.h>
#include <QuartzCore/CACFLayer.h>
#include <QuartzCore/CARenderOGL.h>
#include <d3d9.h>
#include <wtf/HashSet.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/win/GDIObject.h>

using namespace std;

namespace WKQCA {

class CAView::Handle : public ThreadSafeRefCounted<Handle> {
public:
    static RefPtr<Handle> create(CAView* view) { return adoptRef(new Handle(view)); }
    ~Handle() { ASSERT(!m_view); }

    Mutex& mutex() { return m_mutex; }
    CAView* view() const { ASSERT_WITH_MESSAGE(!const_cast<Mutex&>(m_mutex).tryLock(), "CAView::Handle's mutex must be held when calling this function"); return m_view; }
    void clear() { ASSERT_WITH_MESSAGE(!m_mutex.tryLock(), "CAView::Handle's mutex must be held when calling this function"); m_view = 0; }

private:
    Handle(CAView* view)
    : m_view(view) { }

    Mutex m_mutex;
    CAView* m_view;
};

static Mutex& globalStateMutex()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Mutex, mutex, ());
    return mutex;
}

static HWND messageWindow;
static const wchar_t messageWindowClassName[] = L"CAViewMessageWindow";

static HashSet<RefPtr<CAView::Handle> >& views()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(HashSet<RefPtr<CAView::Handle> >, views, ());
    return views;
}

static HashSet<RefPtr<CAView::Handle> >& viewsNeedingUpdate()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(HashSet<RefPtr<CAView::Handle> >, views, ());
    return views;
}

static void registerMessageWindowClass()
{
    static bool didRegister;
    if (didRegister)
        return;
    didRegister = true;

    WNDCLASSW cls = {0};
    cls.hInstance = dllInstance();
    cls.lpfnWndProc = ::DefWindowProcW;
    cls.lpszClassName = messageWindowClassName;
    ::RegisterClassW(&cls);
}

static HWND createMessageWindow()
{
    registerMessageWindowClass();
    return ::CreateWindowW(messageWindowClassName, 0, 0, 0, 0, 0, 0, HWND_MESSAGE, 0, 0, 0);
}

void CAView::releaseAllD3DResources()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Mutex, mutex, ());

    if (!mutex.tryLock()) {
        // Another thread is currently releasing 3D resources.
        // Since it will also release resources for the view calling this method, we can just return early.
        return;
    }

    Vector<RefPtr<Handle>> viewsToRelease;

    {
        MutexLocker lock(globalStateMutex());
        viewsToRelease = copyToVector(views());
    }

    for (size_t i = 0; i < viewsToRelease.size(); ++i) {
        const RefPtr<Handle>& handle = viewsToRelease[i];
        MutexLocker lock(handle->mutex());
        CAView* view = handle->view();
        if (!view)
            continue;
        MutexLocker viewLock(view->m_mutex);
        view->m_swapChain = nullptr;
        view->m_d3dPostProcessingContext = nullptr;
    }

    mutex.unlock();
}

inline CAView::CAView(DrawingDestination destination)
    : m_destination(destination)
    , m_handle(Handle::create(this))
    , m_context(adoptCF(CACFContextCreate(0)))
    , m_bounds(CGRectZero)
{
    {
        MutexLocker lock(globalStateMutex());
        views().add(m_handle);
    }

    CARenderNotificationAddObserver(kCARenderContextDidChange, CACFContextGetRenderContext(m_context.get()), contextDidChangeCallback, this);
}

CAView::~CAView()
{
    CARenderNotificationRemoveObserver(kCARenderContextDidChange, CACFContextGetRenderContext(m_context.get()), contextDidChangeCallback, this);

    m_layer = nullptr;

    {
        MutexLocker lock(m_mutex);
        m_context = nullptr;
    }

    // Avoid stopping the display link while we hold either m_mutex or m_displayLinkMutex, as doing
    // so will wait for displayLinkReachedCAMediaTime to return and that function can take those
    // same mutexes.
    RefPtr<CVDisplayLink> linkToStop;

    {
        MutexLocker lock(m_displayLinkMutex);
        linkToStop = WTFMove(m_displayLink);
    }

    if (linkToStop)
        linkToStop->stop();

    update(nullptr, CGRectZero);

    {
        MutexLocker lock(m_handle->mutex());
        m_handle->clear();
    }

    MutexLocker lock(globalStateMutex());

    views().remove(m_handle);
    if (!views().isEmpty())
        return;

    ::DestroyWindow(messageWindow);
    messageWindow = nullptr;
}

RefPtr<CAView> CAView::create(DrawingDestination destination)
{
    return adoptRef(new CAView(destination));
}

void CAView::setContextDidChangeCallback(ContextDidChangeCallbackFunction function, void* info)
{
    m_contextDidChangeCallback.function = function;
    m_contextDidChangeCallback.info = info;
}

void CAView::setLayer(CACFLayerRef layer)
{
    m_layer = layer;

    if (!m_layer)
        return;

    CACFLayerSetFrame(m_layer.get(), m_bounds);

    MutexLocker lock(m_mutex);
    CACFContextSetLayer(m_context.get(), m_layer.get());
}

void CAView::update(CWindow window, const CGRect& bounds)
{
    {
        MutexLocker lock(globalStateMutex());

        // Ensure our message window is created on the thread that called CAView::update.
        if (!messageWindow)
            messageWindow = createMessageWindow();

        // CAView::update may only be called from a single thread, and that thread must be the same as
        // the window's thread (due to limitations of the D3D API).
        ASSERT(::GetCurrentThreadId() == CWindow(messageWindow).GetWindowThreadID());
        ASSERT(!window || ::GetCurrentThreadId() == window.GetWindowThreadID());

        viewsNeedingUpdate().remove(m_handle);
    }

    bool boundsChanged;

    {
        MutexLocker lock(m_mutex);

        boundsChanged = !CGRectEqualToRect(m_bounds, bounds);

        if (m_window == window && !boundsChanged && !m_flushing && m_swapChain)
            return;

        m_window = window;
        m_bounds = bounds;

        // Invalidate the post-processing context since the window's bounds
        // have changed. If needed, the post-processing context will be
        // recreated with the correct bounds during the next rendering pass.
        m_d3dPostProcessingContext = nullptr;

        if (!m_window) {
            m_swapChain = nullptr;
            // If we don't have a window, we can't draw, so there's no point in having the display
            // link fire.
            scheduleNextDraw(numeric_limits<CFTimeInterval>::infinity());
        } else {
            // FIXME: We might be able to get better resizing performance by allocating swap chains in
            // multiples of some size (say, 256x256) and only reallocating when our window size passes into
            // the next size threshold.
            m_swapChain = CAD3DRenderer::shared().swapChain(m_window, m_bounds.size);

            if (boundsChanged) {
                // Our layer's rendered appearance won't be updated until our context is next flushed.
                // We shouldn't draw until that happens.
                m_drawingProhibited = true;
                // There's no point in allowing the display link to fire until drawing becomes
                // allowed again (at which time we'll restart the display link).
                scheduleNextDraw(numeric_limits<CFTimeInterval>::infinity());
            } else if (m_destination == DrawingDestinationImage) {
                // It is the caller's responsibility to ask us to draw sometime later.
                scheduleNextDraw(numeric_limits<CFTimeInterval>::infinity());
            } else {
                // We should draw into the new window and/or swap chain as soon as possible.
                scheduleNextDraw(0);
            }
        }
    }

    if (m_layer)
        CACFLayerSetFrame(m_layer.get(), m_bounds);

    if (m_window)
        invalidateRects(&m_bounds, 1);

    m_flushing = false;
}

void CAView::invalidateRects(const CGRect* rects, size_t count)
{
    CARenderContext* renderContext = static_cast<CARenderContext*>(CACFContextGetRenderContext(m_context.get()));
    CARenderContextLock(renderContext);
    for (size_t i = 0; i < count; ++i)
        CARenderContextInvalidateRect(renderContext, &rects[i]);
    CARenderContextUnlock(renderContext);
}

void CAView::drawToWindow()
{
    MutexLocker lock(m_mutex);
    drawToWindowInternal();
}

void CAView::drawToWindowInternal()
{
    ASSERT_WITH_MESSAGE(!m_mutex.tryLock(), "m_mutex must be held when calling this function");
    CFTimeInterval nextDrawTime;
    bool unusedWillUpdateSoon;
    if (willDraw(unusedWillUpdateSoon))
        didDraw(CAD3DRenderer::shared().renderAndPresent(m_bounds, m_swapChain, m_d3dPostProcessingContext.get(), m_context.get(), nextDrawTime), unusedWillUpdateSoon);
    else
        nextDrawTime = numeric_limits<CFTimeInterval>::infinity();
    scheduleNextDraw(nextDrawTime);
}

RefPtr<Image> CAView::drawToImage(CGPoint& imageOrigin, CFTimeInterval& nextDrawTime)
{
    ASSERT(m_destination == DrawingDestinationImage);

    imageOrigin = CGPointZero;
    nextDrawTime = numeric_limits<CFTimeInterval>::infinity();

    MutexLocker lock(m_mutex);

    RefPtr<Image> image;
    bool willUpdateSoon;
    if (willDraw(willUpdateSoon))
        didDraw(CAD3DRenderer::shared().renderToImage(m_bounds, m_swapChain, m_d3dPostProcessingContext.get(), m_context.get(), image, imageOrigin, nextDrawTime), willUpdateSoon);

    if (willUpdateSoon) {
        // We weren't able to draw, and have scheduled an update in the near future. Our caller
        // will have to call us again in order to get anything to be drawn. Ideally we'd set
        // nextDrawTime to just after we update ourselves, but we don't know exactly when it will
        // occur, so we just tell the caller to call us again as soon as they can.
        nextDrawTime = 0;
    }
    return image;
}

bool CAView::willDraw(bool& willUpdateSoon)
{
    ASSERT_WITH_MESSAGE(!m_mutex.tryLock(), "m_mutex must be held when calling this function");

    willUpdateSoon = false;

    if (!m_window)
        return false;

    if (m_drawingProhibited)
        return false;

    if (!m_swapChain) {
        // We've lost our swap chain. This probably means the Direct3D device became lost.
        updateSoon();
        willUpdateSoon = true;
        // We'll schedule a new draw once we've updated. Until then, there's no point in trying to draw.
        return false;
    }

    if (m_shouldInvertColors && !m_d3dPostProcessingContext)
        m_d3dPostProcessingContext = CAD3DRenderer::shared().createD3DPostProcessingContext(m_swapChain, m_bounds.size);
    else if (!m_shouldInvertColors)
        m_d3dPostProcessingContext = nullptr;

    return true;
}

void CAView::didDraw(CAD3DRenderer::RenderResult result, bool& willUpdateSoon)
{
    willUpdateSoon = false;

    switch (result) {
    case CAD3DRenderer::DeviceBecameLost:
        // MSDN says we have to release any existing D3D resources before we try to recover a lost
        // device. That includes swap chains. See
        // <http://msdn.microsoft.com/en-us/library/bb174425(v=VS.85).aspx>.
        releaseAllD3DResources();
        updateSoon();
        willUpdateSoon = true;
        return;

    case CAD3DRenderer::DeviceIsLost:
        updateSoon();
        willUpdateSoon = true;
        return;

    case CAD3DRenderer::OtherFailure:
    case CAD3DRenderer::NoRenderingRequired:
    case CAD3DRenderer::Success:
        return;
    }

    ASSERT_NOT_REACHED();
}

void CAView::drawIntoDC(HDC dc)
{
    auto memoryDC = adoptGDIObject(::CreateCompatibleDC(dc));

    // Create a bitmap and select it into our memoryDC.

    BITMAPINFO info = {0};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = CGRectGetWidth(m_bounds);
    info.bmiHeader.biHeight = CGRectGetHeight(m_bounds);
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;

    void* bits = nullptr;
    auto bitmap = adoptGDIObject(::CreateDIBSection(memoryDC.get(), &info, DIB_RGB_COLORS, &bits, 0, 0));
    if (!bitmap)
        return;

    HBITMAP oldBitmap = static_cast<HBITMAP>(::SelectObject(memoryDC.get(), bitmap.get()));

    BITMAP bmp;
    ::GetObject(bitmap.get(), sizeof(bmp), &bmp);

    // Create a software renderer.
    CARenderOGLContext* renderer = CARenderOGLNew(&kCARenderSoftwareCallbacks, 0, kCARenderFlippedGeometry);

    // Tell it to render into our bitmap.
    if (!CARenderSoftwareSetDestination(renderer, bmp.bmBits, bmp.bmWidthBytes, bmp.bmBitsPixel, 0, 0, 0, 0, CGRectGetWidth(m_bounds), CGRectGetHeight(m_bounds))) {
        ::SelectObject(memoryDC.get(), oldBitmap);
        CARenderOGLDestroy(renderer);
        return;
    }

    // Prepare for rendering.
    char space[4096];
    CARenderUpdate* u = CARenderUpdateBegin(space, sizeof(space), CACurrentMediaTime(), 0, 0, &m_bounds);
    if (!u) {
        ::SelectObject(memoryDC.get(), oldBitmap);
        CARenderOGLDestroy(renderer);
        return;
    }

    {
        MutexLocker lock(m_mutex);

        CARenderContext* renderContext = static_cast<CARenderContext*>(CACFContextGetRenderContext(m_context.get()));
        CARenderContextLock(renderContext);
        CARenderUpdateAddContext(u, renderContext);
        CARenderContextUnlock(renderContext);

        // We always want to render our entire contents.
        CARenderUpdateAddRect(u, &m_bounds);

        CARenderOGLRender(renderer, u);

        CARenderUpdateFinish(u);
    }

    // Copy the rendered image into the destination DC.
    ::BitBlt(dc, 0, 0, CGRectGetWidth(m_bounds), CGRectGetHeight(m_bounds), memoryDC.get(), 0, 0, SRCCOPY);

    // Clean up.
    ::SelectObject(memoryDC.get(), oldBitmap);
    CARenderOGLDestroy(renderer);
}

void CAView::setShouldInvertColors(bool shouldInvertColors)
{
    MutexLocker lock(m_mutex);
    m_shouldInvertColors = shouldInvertColors;
}

void CAView::scheduleNextDraw(CFTimeInterval mediaTime)
{
    MutexLocker lock(m_displayLinkMutex);

    if (!m_context)
        return;

    m_nextDrawTime = mediaTime;

    // We use !< here to ensure that we bail out when mediaTime is NaN.
    // (Comparisons with NaN always yield false.)
    if (!(m_nextDrawTime < numeric_limits<CFTimeInterval>::infinity())) {
        if (m_displayLink)
            m_displayLink->setPaused(true);
        return;
    }

    ASSERT(m_destination == DrawingDestinationWindow);

    if (!m_displayLink) {
        m_displayLink = CVDisplayLink::create(this);
        m_displayLink->start();
    } else
        m_displayLink->setPaused(false);
}

void CAView::displayLinkReachedCAMediaTime(CVDisplayLink* displayLink, CFTimeInterval time)
{
    ASSERT(m_destination == DrawingDestinationWindow);

    {
        MutexLocker lock(m_displayLinkMutex);
        if (!m_displayLink)
            return;
        ASSERT_UNUSED(displayLink, displayLink == m_displayLink);

        if (time < m_nextDrawTime)
            return;
    }

    MutexLocker lock(m_mutex);
    drawToWindowInternal();
}

void CAView::contextDidChangeCallback(void* object, void* info, void*)
{
    ASSERT_ARG(info, info);
    ASSERT_ARG(object, object);

    CAView* view = static_cast<CAView*>(info);
    ASSERT_UNUSED(object, CACFContextGetRenderContext(view->context()) == object);
    view->contextDidChange();
}

void CAView::contextDidChange()
{
    {
        MutexLocker lock(m_mutex);

        // Our layer's rendered appearance once again matches our bounds, so it's safe to draw.
        m_drawingProhibited = false;
        m_flushing = true;

        if (m_destination == DrawingDestinationWindow)
            scheduleNextDraw(0);
    }

    if (m_contextDidChangeCallback.function)
        m_contextDidChangeCallback.function(this, m_contextDidChangeCallback.info);
}

void CAView::updateSoon()
{
    {
        MutexLocker lock(globalStateMutex());
        viewsNeedingUpdate().add(m_handle);
    }
    // It doesn't matter what timer ID we pass here, as long as it's nonzero.
    ASSERT(messageWindow);
    ::SetTimer(messageWindow, 1, 0, updateViewsNow);
}

void CAView::updateViewsNow(HWND window, UINT, UINT_PTR timerID, DWORD)
{
    ASSERT_ARG(window, window == messageWindow);
    ::KillTimer(window, timerID);

    Vector<RefPtr<Handle>> viewsToUpdate;

    {
        MutexLocker lock(globalStateMutex());
        viewsToUpdate = copyToVector(viewsNeedingUpdate());
        viewsNeedingUpdate().clear();
    }

    for (size_t i = 0; i < viewsToUpdate.size(); ++i) {
        const RefPtr<Handle>& handle = viewsToUpdate[i];
        MutexLocker lock(handle->mutex());
        CAView* view = handle->view();
        if (!view)
            continue;
        MutexLocker viewLock(view->m_mutex);
        view->update(view->m_window, view->m_bounds);
    }
}

IDirect3DDevice9* CAView::d3dDevice9()
{
    // Hold the mutex while we return the shared d3d device. The caller is responsible for retaining
    // the device before returning to ensure that it is not released.
    MutexLocker lock(m_mutex);

    return CAD3DRenderer::shared().d3dDevice9();
}

} // namespace WKQCA
