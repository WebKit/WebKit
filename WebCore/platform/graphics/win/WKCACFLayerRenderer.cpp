/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "WKCACFLayerRenderer.h"

#include "WKCACFContextFlusher.h"
#include "WKCACFLayer.h"
#include "WebCoreInstanceHandle.h"
#include <CoreGraphics/CGSRegion.h>
#include <QuartzCore/CACFContext.h>
#include <QuartzCore/CARenderOGL.h>
#include <QuartzCoreInterface/QuartzCoreInterface.h>
#include <wtf/HashMap.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/StdLibExtras.h>
#include <d3d9.h>
#include <d3dx9.h>

#pragma comment(lib, "d3d9")
#pragma comment(lib, "d3dx9")
#ifdef DEBUG_ALL
#pragma comment(lib, "QuartzCore_debug")
#else
#pragma comment(lib, "QuartzCore")
#endif

static IDirect3D9* s_d3d = 0;
static IDirect3D9* d3d()
{
    if (s_d3d)
        return s_d3d;

    if (!LoadLibrary(TEXT("d3d9.dll")))
        return 0;

    s_d3d = Direct3DCreate9(D3D_SDK_VERSION);

    return s_d3d;
}

inline static CGRect winRectToCGRect(RECT rc)
{
    return CGRectMake(rc.left, rc.top, (rc.right - rc.left), (rc.bottom - rc.top));
}

inline static CGRect winRectToCGRect(RECT rc, RECT relativeToRect)
{
    return CGRectMake(rc.left, (relativeToRect.bottom-rc.bottom), (rc.right - rc.left), (rc.bottom - rc.top));
}

namespace WebCore {

// Subclass of WKCACFLayer to allow the root layer to have a back pointer to the layer renderer
// to fire off a draw
class WKCACFRootLayer : public WKCACFLayer {
public:
    WKCACFRootLayer(WKCACFLayerRenderer* renderer)
        : WKCACFLayer(WKCACFLayer::Layer)
    {
        m_renderer = renderer;
    }

    static PassRefPtr<WKCACFRootLayer> create(WKCACFLayerRenderer* renderer)
    {
        if (!WKCACFLayerRenderer::acceleratedCompositingAvailable())
            return 0;
        return adoptRef(new WKCACFRootLayer(renderer));
    }

    virtual void setNeedsRender() { m_renderer->renderSoon(); }

    // Overload this to avoid calling setNeedsDisplay on the layer, which would override the contents
    // we have placed on the root layer.
    virtual void setNeedsDisplay(const CGRect* dirtyRect) { setNeedsCommit(); }

private:
    WKCACFLayerRenderer* m_renderer;
};

typedef HashMap<CACFContextRef, WKCACFLayerRenderer*> ContextToWindowMap;

static ContextToWindowMap& windowsForContexts()
{
    DEFINE_STATIC_LOCAL(ContextToWindowMap, map, ());
    return map;
}

static D3DPRESENT_PARAMETERS initialPresentationParameters()
{
    D3DPRESENT_PARAMETERS parameters = {0};
    parameters.Windowed = TRUE;
    parameters.SwapEffect = D3DSWAPEFFECT_COPY;
    parameters.BackBufferCount = 1;
    parameters.BackBufferFormat = D3DFMT_A8R8G8B8;
    parameters.MultiSampleType = D3DMULTISAMPLE_NONE;

    return parameters;
}

// FIXME: <rdar://6507851> Share this code with CoreAnimation.
static bool hardwareCapabilitiesIndicateCoreAnimationSupport(const D3DCAPS9& caps)
{
    // CoreAnimation needs two or more texture units.
    if (caps.MaxTextureBlendStages < 2)
        return false;

    // CoreAnimation needs non-power-of-two textures.
    if ((caps.TextureCaps & D3DPTEXTURECAPS_POW2) && !(caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL))
        return false;

    // CoreAnimation needs vertex shader 2.0 or greater.
    if (D3DSHADER_VERSION_MAJOR(caps.VertexShaderVersion) < 2)
        return false;

    // CoreAnimation needs pixel shader 2.0 or greater.
    if (D3DSHADER_VERSION_MAJOR(caps.PixelShaderVersion) < 2)
        return false;

    return true;
}

bool WKCACFLayerRenderer::acceleratedCompositingAvailable()
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
    WNDCLASSEX wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc = DefWindowProc;
    wcex.hInstance = WebCore::instanceHandle();
    wcex.lpszClassName = L"CoreAnimationTesterWindowClass";
    ::RegisterClassEx(&wcex);
    HWND testWindow = ::CreateWindow(L"CoreAnimationTesterWindowClass", L"CoreAnimationTesterWindow", WS_POPUP, -500, -500, 0, 0, 0, 0, 0, 0);

    if (!testWindow) {
        available = false;
        return available;
    }

    OwnPtr<WKCACFLayerRenderer> testLayerRenderer = WKCACFLayerRenderer::create();
    testLayerRenderer->setHostWindow(testWindow);
    available = testLayerRenderer->createRenderer();
    ::DestroyWindow(testWindow);

    return available;
}

void WKCACFLayerRenderer::didFlushContext(CACFContextRef context)
{
    WKCACFLayerRenderer* window = windowsForContexts().get(context);
    if (!window)
        return;

    window->renderSoon();
}

PassOwnPtr<WKCACFLayerRenderer> WKCACFLayerRenderer::create()
{
    if (!acceleratedCompositingAvailable())
        return 0;
    return new WKCACFLayerRenderer;
}

WKCACFLayerRenderer::WKCACFLayerRenderer()
    : m_triedToCreateD3DRenderer(false)
    , m_renderContext(0)
    , m_renderer(0)
    , m_hostWindow(0)
    , m_renderTimer(this, &WKCACFLayerRenderer::renderTimerFired)
    , m_scrollPosition(0, 0)
    , m_scrollSize(1, 1)
    , m_backingStoreDirty(false)
{
#ifndef NDEBUG
    char* printTreeFlag = getenv("CA_PRINT_TREE");
    m_printTree = printTreeFlag && atoi(printTreeFlag);
#endif
}

WKCACFLayerRenderer::~WKCACFLayerRenderer()
{
    destroyRenderer();
}

WKCACFLayer* WKCACFLayerRenderer::rootLayer() const
{
    return m_rootLayer.get();
}

void WKCACFLayerRenderer::setScrollFrame(const IntPoint& position, const IntSize& size)
{
    m_scrollSize = size;
    m_scrollPosition = position;

    updateScrollFrame();
}

void WKCACFLayerRenderer::updateScrollFrame()
{
    CGRect frameBounds = bounds();
    m_clipLayer->setBounds(CGRectMake(0, 0, m_scrollSize.width(), m_scrollSize.height()));
    m_clipLayer->setPosition(CGPointMake(0, frameBounds.size.height));
    if (m_rootChildLayer) {
        CGRect rootBounds = m_rootChildLayer->bounds();
        m_scrollLayer->setBounds(rootBounds);
    }
    m_scrollLayer->setPosition(CGPointMake(-m_scrollPosition.x(), m_scrollPosition.y() + m_scrollSize.height()));
}

void WKCACFLayerRenderer::setRootContents(CGImageRef image)
{
    ASSERT(m_rootLayer);
    m_rootLayer->setContents(image);
    renderSoon();
}

void WKCACFLayerRenderer::setRootChildLayer(WKCACFLayer* layer)
{
    if (!m_scrollLayer)
        return;

    m_scrollLayer->removeAllSublayers();
    m_rootChildLayer = layer;
    if (layer) {
        m_scrollLayer->addSublayer(layer);
        // Adjust the scroll frame accordingly
        updateScrollFrame();
    }
}
   
void WKCACFLayerRenderer::setNeedsDisplay()
{
    ASSERT(m_rootLayer);
    m_rootLayer->setNeedsDisplay(0);
    renderSoon();
}

bool WKCACFLayerRenderer::createRenderer()
{
    if (m_triedToCreateD3DRenderer)
        return m_d3dDevice;

    m_triedToCreateD3DRenderer = true;
    D3DPRESENT_PARAMETERS parameters = initialPresentationParameters();

    if (!d3d() || !::IsWindow(m_hostWindow))
        return false;

    // D3D doesn't like to make back buffers for 0 size windows. We skirt this problem if we make the
    // passed backbuffer width and height non-zero. The window will necessarily get set to a non-zero
    // size eventually, and then the backbuffer size will get reset.
    RECT rect;
    GetClientRect(m_hostWindow, &rect);

    if (rect.left-rect.right == 0 || rect.bottom-rect.top == 0) {
        parameters.BackBufferWidth = 1;
        parameters.BackBufferHeight = 1;
    }

    COMPtr<IDirect3DDevice9> device;
    if (FAILED(d3d()->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hostWindow, D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &parameters, &device)))
        return false;

    // Now that we've created the IDirect3DDevice9 based on the capabilities we
    // got from the IDirect3D9 global object, we requery the device for its
    // actual capabilities. The capabilities returned by the device can
    // sometimes be more complete, for example when using software vertex
    // processing.
    D3DCAPS9 deviceCaps;
    if (FAILED(device->GetDeviceCaps(&deviceCaps)))
        return false;

    if (!hardwareCapabilitiesIndicateCoreAnimationSupport(deviceCaps))
        return false;

    m_d3dDevice = device;

    D3DXMATRIXA16 projection;
    D3DXMatrixOrthoOffCenterRH(&projection, rect.left, rect.right, rect.top, rect.bottom, -1.0f, 1.0f);

    m_d3dDevice->SetTransform(D3DTS_PROJECTION, &projection);

    m_context.adoptCF(CACFContextCreate(0));
    windowsForContexts().set(m_context.get(), this);

    m_renderContext = static_cast<CARenderContext*>(CACFContextGetRenderContext(m_context.get()));
    m_renderer = CARenderOGLNew(wkqcCARenderOGLCallbacks(wkqckCARenderDX9Callbacks), m_d3dDevice.get(), 0);

    // Create the root hierarchy.
    // Under the root layer, we have a clipping layer to clip the content,
    // that contains a scroll layer that we use for scrolling the content.
    // The root layer is the size of the client area of the window.
    // The clipping layer is the size of the WebView client area (window less the scrollbars).
    // The scroll layer is the size of the root child layer.
    // Resizing the window will change the bounds of the rootLayer and the clip layer and will not
    // cause any repositioning.
    // Scrolling will affect only the position of the scroll layer without affecting the bounds.

    m_rootLayer = WKCACFRootLayer::create(this);
    m_rootLayer->setName("WKCACFLayerRenderer rootLayer");

    m_clipLayer = WKCACFLayer::create(WKCACFLayer::Layer);
    m_clipLayer->setName("WKCACFLayerRenderer clipLayer");
    
    m_scrollLayer = WKCACFLayer::create(WKCACFLayer::Layer);
    m_scrollLayer->setName("WKCACFLayerRenderer scrollLayer");

    m_rootLayer->addSublayer(m_clipLayer);
    m_clipLayer->addSublayer(m_scrollLayer);
    m_clipLayer->setMasksToBounds(true);
    m_scrollLayer->setAnchorPoint(CGPointMake(0, 1));
    m_clipLayer->setAnchorPoint(CGPointMake(0, 1));

#ifndef NDEBUG
    CGColorRef debugColor = createCGColor(Color(255, 0, 0, 204));
    m_rootLayer->setBackgroundColor(debugColor);
    CGColorRelease(debugColor);
#endif

    if (IsWindow(m_hostWindow))
        m_rootLayer->setFrame(bounds());

    if (m_context)
        m_rootLayer->becomeRootLayerForContext(m_context.get());

    return true;
}

void WKCACFLayerRenderer::destroyRenderer()
{
    if (m_context) {
        windowsForContexts().remove(m_context.get());
        WKCACFContextFlusher::shared().removeContext(m_context.get());
    }

    if (m_renderer)
        CARenderOGLDestroy(m_renderer);
    m_renderer = 0;
    m_d3dDevice = 0;
    if (s_d3d)
        s_d3d->Release();

    s_d3d = 0;
    m_rootLayer = 0;
    m_clipLayer = 0;
    m_scrollLayer = 0;
    m_rootChildLayer = 0;

    m_triedToCreateD3DRenderer = false;
}

void WKCACFLayerRenderer::resize()
{
    if (!m_d3dDevice)
        return;

    resetDevice();

    if (m_rootLayer) {
        m_rootLayer->setFrame(bounds());
        WKCACFContextFlusher::shared().flushAllContexts();
        updateScrollFrame();
    }
}

static void getDirtyRects(HWND window, Vector<CGRect>& outRects)
{
    ASSERT_ARG(outRects, outRects.isEmpty());

    RECT clientRect;
    if (!GetClientRect(window, &clientRect))
        return;

    HRGN region = CreateRectRgn(0, 0, 0, 0);
    int regionType = GetUpdateRgn(window, region, false);
    if (regionType != COMPLEXREGION) {
        RECT dirtyRect;
        if (GetUpdateRect(window, &dirtyRect, false))
            outRects.append(winRectToCGRect(dirtyRect, clientRect));
        return;
    }

    DWORD dataSize = GetRegionData(region, 0, 0);
    OwnArrayPtr<unsigned char> regionDataBuffer(new unsigned char[dataSize]);
    RGNDATA* regionData = reinterpret_cast<RGNDATA*>(regionDataBuffer.get());
    if (!GetRegionData(region, dataSize, regionData))
        return;

    outRects.resize(regionData->rdh.nCount);

    RECT* rect = reinterpret_cast<RECT*>(regionData->Buffer);
    for (size_t i = 0; i < outRects.size(); ++i, ++rect)
        outRects[i] = winRectToCGRect(*rect, clientRect);

    DeleteObject(region);
}

void WKCACFLayerRenderer::renderTimerFired(Timer<WKCACFLayerRenderer>*)
{
    paint();
}

void WKCACFLayerRenderer::paint()
{
    if (!m_d3dDevice)
        return;

    if (m_backingStoreDirty) {
        // If the backing store is still dirty when we are about to draw the
        // composited content, we need to force the window to paint into the
        // backing store. The paint will only paint the dirty region that
        // if being tracked in WebView.
        UpdateWindow(m_hostWindow);
        return;
    }

    Vector<CGRect> dirtyRects;
    getDirtyRects(m_hostWindow, dirtyRects);
    render(dirtyRects);
}

void WKCACFLayerRenderer::render(const Vector<CGRect>& dirtyRects)
{
    ASSERT(m_d3dDevice);

    // Flush the root layer to the render tree.
    WKCACFContextFlusher::shared().flushAllContexts();

    CGRect bounds = this->bounds();

    CFTimeInterval t = CACurrentMediaTime();

    // Give the renderer some space to use. This needs to be valid until the
    // CARenderUpdateFinish() call below.
    char space[4096];
    CARenderUpdate* u = CARenderUpdateBegin(space, sizeof(space), t, 0, 0, &bounds);
    if (!u)
        return;

    CARenderContextLock(m_renderContext);
    CARenderUpdateAddContext(u, m_renderContext);
    CARenderContextUnlock(m_renderContext);

    for (size_t i = 0; i < dirtyRects.size(); ++i)
        CARenderUpdateAddRect(u, &dirtyRects[i]);

    HRESULT err = S_OK;
    do {
        CGSRegionObj rgn = CARenderUpdateCopyRegion(u);

        if (!rgn)
            break;

        // FIXME: don't need to clear dirty region if layer tree is opaque.

        Vector<D3DRECT, 64> rects;
        CGSRegionEnumeratorObj e = CGSRegionEnumerator(rgn);
        for (const CGRect* r = CGSNextRect(e); r; r = CGSNextRect(e)) {
            D3DRECT rect;
            rect.x1 = r->origin.x;
            rect.x2 = rect.x1 + r->size.width;
            rect.y1 = bounds.origin.y + bounds.size.height - (r->origin.y + r->size.height);
            rect.y2 = rect.y1 + r->size.height;

            rects.append(rect);
        }
        CGSReleaseRegionEnumerator(e);
        CGSReleaseRegion(rgn);

        if (rects.isEmpty())
            break;

        m_d3dDevice->Clear(rects.size(), rects.data(), D3DCLEAR_TARGET, 0, 1.0f, 0);

        m_d3dDevice->BeginScene();
        CARenderOGLRender(m_renderer, u);
        m_d3dDevice->EndScene();

        err = m_d3dDevice->Present(0, 0, 0, 0);

        if (err == D3DERR_DEVICELOST) {
            // Lost device situation.
            CARenderOGLPurge(m_renderer);
            resetDevice();
            CARenderUpdateAddRect(u, &bounds);
        }
    } while (err == D3DERR_DEVICELOST);

    CARenderUpdateFinish(u);

#ifndef NDEBUG
    if (m_printTree)
        m_rootLayer->printTree();
#endif
}

void WKCACFLayerRenderer::renderSoon()
{
    if (!m_renderTimer.isActive())
        m_renderTimer.startOneShot(0);
}

CGRect WKCACFLayerRenderer::bounds() const
{
    RECT clientRect;
    GetClientRect(m_hostWindow, &clientRect);

    return winRectToCGRect(clientRect);
}

void WKCACFLayerRenderer::initD3DGeometry()
{
    ASSERT(m_d3dDevice);

    CGRect bounds = this->bounds();

    float x0 = bounds.origin.x;
    float y0 = bounds.origin.y;
    float x1 = x0 + bounds.size.width;
    float y1 = y0 + bounds.size.height;

    D3DXMATRIXA16 projection;
    D3DXMatrixOrthoOffCenterRH(&projection, x0, x1, y0, y1, -1.0f, 1.0f);

    m_d3dDevice->SetTransform(D3DTS_PROJECTION, &projection);
}

void WKCACFLayerRenderer::resetDevice()
{
    ASSERT(m_d3dDevice);

    D3DPRESENT_PARAMETERS parameters = initialPresentationParameters();
    m_d3dDevice->Reset(&parameters);
    initD3DGeometry();
}

}

#endif // USE(ACCELERATED_COMPOSITING)
